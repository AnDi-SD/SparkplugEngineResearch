using System.Collections.Immutable;
using System.IO;
using SmoImporter.Core;
using SmoViewer.Core;

namespace SmoImporter.Gui;

public partial class MainWindow
{
    private enum WizardProtectionAttachment { Head, Wings }

    private sealed record WizardProtectionChoice(WizardProtectionAttachment Attachment, string GeometryFingerprint);

    private sealed record WizardProtectionState(
        ImmutableDictionary<GeneratedSkinningSeparationPlaneKind, GeneratedSkinningSeparationPlaneAdjustment> Planes,
        ImmutableDictionary<GeneratedSkinningSemanticRegion, GeneratedSkinningRegionAdjustment> Hands,
        ImmutableDictionary<int, WizardProtectionChoice> Components,
        float WingHeightOffsetPercent = 0,
        float WingRotationDegrees = 0)
    {
        public static WizardProtectionState Automatic { get; } = new(
            ImmutableDictionary<GeneratedSkinningSeparationPlaneKind, GeneratedSkinningSeparationPlaneAdjustment>.Empty,
            ImmutableDictionary<GeneratedSkinningSemanticRegion, GeneratedSkinningRegionAdjustment>.Empty,
            ImmutableDictionary<int, WizardProtectionChoice>.Empty);
    }

    private sealed record WizardProtectionInputs(
        SmoDocument Target, ImportedScene Donor, ImportedScene MaterialScene, long RigRevision);

    private sealed record WizardProtectionRequest(WizardProtectionInputs Inputs, WizardProtectionState State);

    private sealed record WizardProtectionResult(
        WizardProtectionRequest Request,
        ImportedScene InputScene,
        GeneratedSkinningCalculationResult Automatic,
        GeneratedSkinningCalculationResult Current,
        GeneratedSkinningComponentOverrides? Components,
        GeneratedSkinningRegionOverrides? Regions,
        SmoWingFitResult? Wings,
        SmoWingAttachmentDraft? WingAttachments);

    private WizardProtectionState _wizardProtectionState = WizardProtectionState.Automatic;
    private WizardProtectionRequest? _wizardProtectionRequest;
    private WizardProtectionResult? _wizardProtectionResult;
    private CancellationTokenSource? _wizardProtectionCancellation;
    private Task? _wizardProtectionOperation;
    private string? _wizardProtectionIssue;
    private double _wizardProtectionProgressFraction;
    private string? _wizardProtectionProgressStage;
    private bool _wizardProtectionStopping;
    private bool _wizardProtectionHistoryPending;
    private WizardProtectionRequest? _wizardProtectionCommittedRequest;
    private SmoWingAttachmentDraft? _wizardProtectionWingAttachments;

    private bool WizardProtectionModeAvailable => WizardRigSupportsHumanoid && UsesGeneratedWeightsPortingMode;
    private bool WizardProtectionIsRunning => _wizardProtectionCancellation is not null;
    private string? WizardProtectionIssue => _wizardProtectionIssue;
    private WizardProtectionResult? VisibleWizardProtectionResult =>
        !_preparedStep || IsWizardProtectionResultCurrent() ? _wizardProtectionResult : null;
    private GeneratedSkinningPreparationResult? WizardProtectionPreparation => VisibleWizardProtectionResult?.Current.Preparation;
    private GeneratedSkinningRegionAnalysis? WizardProtectionRegions => WizardProtectionPreparation?.Analysis.SemanticRegions;
    private SmoWingFitResult? WizardProtectionWingFit => VisibleWizardProtectionResult?.Wings;
    private SmoWingAttachmentDraft? WizardProtectionWingAttachments => VisibleWizardProtectionResult?.WingAttachments;
    private ImportedScene? WizardProtectionFittingScene => WizardProtectionWingFit?.Scene ?? WizardProtectionPreparation?.FittingPreviewScene;
    private bool WizardProtectionCanWrite => IsWizardProtectionResultCurrent() &&
        _wizardProtectionResult is { Current.Plan.CanReplace: true };
    private bool WizardProtectionCommitIsCurrent => IsWizardProtectionResultCurrent() &&
        _wizardProtectionResult is { } result && _wizardProtectionCommittedRequest == result.Request &&
        ReferenceEquals(_generatedSkinningPreparation, result.Current.Preparation) &&
        ReferenceEquals(_wizardProtectionWingAttachments, result.WingAttachments) &&
        _generatedSkinningPreparationRevision == _rigFittingRevision && result.Current.Plan.CanReplace;

    private string? WizardProtectionWingStatus
    {
        get
        {
            if (!_wizardProtectionState.Components.Values.Any(choice => choice.Attachment == WizardProtectionAttachment.Wings))
                return null;
            if (!IsWizardProtectionResultCurrent() || WizardProtectionWingAttachments is not { } attachments)
                return null;
            return attachments.Left.Reference.NodeObjectIndex is not null && attachments.Right.Reference.NodeObjectIndex is not null
                ? "Крылья подогнаны по исходной модели."
                : "Крылья подогнаны по образцу BloomX.";
        }
    }

    private WizardProtectionState? CaptureWizardProtectionState() =>
        _wizardProtectionState == WizardProtectionState.Automatic ? null : _wizardProtectionState;

    private void RestoreWizardProtectionState(WizardProtectionState? state)
    {
        state ??= WizardProtectionState.Automatic;
        if (_wizardProtectionState == state)
            return;

        CancelWizardProtectionPreview();
        if (_wizardProtectionResult is { } result && ReferenceEquals(_generatedSkinningPreparation, result.Current.Preparation))
            ClearWizardProtectionCommit();
        _wizardProtectionState = state;
        _wizardProtectionRequest = null;
        _wizardProtectionIssue = null;
        _wizardProtectionHistoryPending = false;
    }

    private void ResetWizardProtectionState()
    {
        ClearWizardProtectionCommit();
        RestoreWizardProtectionState(null);
        CancelWizardProtectionPreview();
        _wizardProtectionRequest = null;
        _wizardProtectionResult = null;
        _wizardProtectionIssue = null;
        _wizardProtectionHistoryPending = false;
        ResetWizardProtectionVisibility();
    }

    private void InvalidateWizardProtectionPose()
    {
        ClearWizardProtectionCommit();
        RestoreWizardProtectionState(WizardProtectionState.Automatic with
        {
            Components = _wizardProtectionState.Components,
            WingHeightOffsetPercent = _wizardProtectionState.WingHeightOffsetPercent,
            WingRotationDegrees = _wizardProtectionState.WingRotationDegrees
        });
        CancelWizardProtectionPreview();
        _wizardProtectionResult = null;
        _wizardProtectionRequest = null;
    }

    private GeneratedSkinningSeparationPlaneAdjustment GetWizardProtectionPlaneAdjustment(GeneratedSkinningSeparationPlaneKind kind) =>
        _wizardProtectionState.Planes.GetValueOrDefault(kind) ?? GeneratedSkinningSeparationPlaneAdjustment.Automatic(kind);

    private GeneratedSkinningRegionAdjustment GetWizardProtectionHandAdjustment(GeneratedSkinningSemanticRegion region) =>
        _wizardProtectionState.Hands.GetValueOrDefault(region) ?? GeneratedSkinningRegionAdjustment.Automatic(region);

    private WizardProtectionAttachment? GetWizardProtectionComponentAttachment(int componentIndex) =>
        _wizardProtectionState.Components.GetValueOrDefault(componentIndex)?.Attachment;

    private bool CanEditWizardProtection => _postRigStep && WizardProtectionModeAvailable &&
        WizardProtectionPreparation is not null && !WizardBaseBusy;

    private bool TrySetWizardProtectionPlane(GeneratedSkinningSeparationPlaneKind kind, GeneratedSkinningSeparationPlaneAdjustment adjustment)
    {
        if (!CanEditWizardProtection || adjustment.Kind != kind || kind == GeneratedSkinningSeparationPlaneKind.Back ||
            WizardProtectionRegions?.SeparationPlanes.FirstOrDefault(plane => plane.Kind == kind) is not { IsAvailable: true } plane ||
            !InRange(adjustment.Offset, plane.Limits.MinimumOffset, plane.Limits.MaximumOffset) ||
            !InRange(adjustment.AngleDegrees, plane.Limits.MinimumAngleDegrees, plane.Limits.MaximumAngleDegrees))
            return false;

        var planes = adjustment == GeneratedSkinningSeparationPlaneAdjustment.Automatic(kind)
            ? _wizardProtectionState.Planes.Remove(kind)
            : _wizardProtectionState.Planes.SetItem(kind, adjustment);
        return ChangeWizardProtectionState(_wizardProtectionState with { Planes = planes });
    }

    private bool TrySetWizardProtectionHand(GeneratedSkinningSemanticRegion region, GeneratedSkinningRegionAdjustment adjustment, bool mirror)
    {
        if (!CanEditWizardProtection || region == GeneratedSkinningSemanticRegion.Head || adjustment.Region != region)
            return false;

        var hands = _wizardProtectionState.Hands;
        foreach (GeneratedSkinningSemanticRegion side in mirror
            ? new[] { GeneratedSkinningSemanticRegion.LeftHand, GeneratedSkinningSemanticRegion.RightHand }
            : [region])
        {
            if (WizardProtectionRegions?.Regions.FirstOrDefault(hand => hand.Region == side) is not { AutomaticVolume: not null } hand)
                return false;

            var sideAdjustment = adjustment with { Region = side };
            var limits = hand.AdjustmentLimits;
            if (!InRange(sideAdjustment.AxialOffset, limits.MinimumAxialOffset, limits.MaximumAxialOffset) ||
                !InRange(sideAdjustment.AxialScale, limits.MinimumAxialScale, limits.MaximumAxialScale) ||
                !InRange(sideAdjustment.RadialScale, limits.MinimumRadialScale, limits.MaximumRadialScale))
                return false;

            hands = sideAdjustment == GeneratedSkinningRegionAdjustment.Automatic(side)
                ? hands.Remove(side)
                : hands.SetItem(side, sideAdjustment);
        }
        return ChangeWizardProtectionState(_wizardProtectionState with { Hands = hands });
    }

    private bool TrySetWizardProtectionComponents(IReadOnlyCollection<int> componentIndices, WizardProtectionAttachment? target)
    {
        if (!CanEditWizardProtection || componentIndices.Count == 0 || WizardProtectionPreparation?.Analysis is not { } analysis)
            return false;

        var components = _wizardProtectionState.Components;
        foreach (int index in componentIndices)
        {
            if (!analysis.Components.Any(component => component.ComponentIndex == index))
                return false;

            components = target is { } attachment
                ? components.SetItem(index, new WizardProtectionChoice(attachment, analysis.DonorGeometryFingerprint))
                : components.Remove(index);
        }
        return ChangeWizardProtectionState(_wizardProtectionState with { Components = components });
    }

    private bool TrySetWizardProtectionWingHeight(float percent)
    {
        if (!CanEditWizardProtection || !InRange(percent, -25, 25) ||
            !_wizardProtectionState.Components.Values.Any(choice => choice.Attachment == WizardProtectionAttachment.Wings))
            return false;

        return ChangeWizardProtectionState(_wizardProtectionState with { WingHeightOffsetPercent = percent });
    }

    private bool TrySetWizardProtectionWingRotation(float degrees)
    {
        if (!CanEditWizardProtection || !InRange(degrees, -90, 90) ||
            !_wizardProtectionState.Components.Values.Any(choice => choice.Attachment == WizardProtectionAttachment.Wings))
            return false;

        return ChangeWizardProtectionState(_wizardProtectionState with { WingRotationDegrees = degrees });
    }

    private static bool InRange(float value, float minimum, float maximum) =>
        float.IsFinite(value) && value >= minimum && value <= maximum;

    private bool ChangeWizardProtectionState(WizardProtectionState state)
    {
        if (state == _wizardProtectionState)
            return false;

        _wizardProtectionState = state;
        _inputError = null;
        _wizardProtectionHistoryPending = true;
        ClearWizardProtectionCommit();
        EnsureWizardProtectionPreview();
        return true;
    }

    private void FlushWizardProtectionHistory()
    {
        if (!_wizardProtectionHistoryPending)
            return;

        _wizardProtectionHistoryPending = false;
        RecordInputStep();
    }

    private WizardProtectionInputs? GetWizardProtectionInputs() =>
        WizardProtectionModeAvailable && WizardRigPoseAccepted &&
        _document is { } target && _baseReplacementScene is { } donor && _replacementScene is { } materialScene
            ? new WizardProtectionInputs(target, donor, materialScene, _rigFittingRevision)
            : null;

    private bool IsWizardProtectionResultCurrent() => _wizardProtectionResult is { } result &&
        result.Request.State == _wizardProtectionState && result.Request.Inputs == GetWizardProtectionInputs();

    private void EnsureWizardProtectionPreview()
    {
        if (_isClosing || !(_postRigStep || _preparedStep) || _replayingInputStep || GetWizardProtectionInputs() is not { } inputs ||
            WizardRigPose is not { } pose || _generatedDonorAlignment is not { } alignment)
            return;

        var request = new WizardProtectionRequest(inputs, _wizardProtectionState);
        if (_wizardProtectionRequest == request)
        {
            if (_preparedStep && _wizardProtectionResult is { } accepted && IsWizardProtectionResultCurrent() &&
                _wizardProtectionIssue is null && !WizardProtectionIsRunning)
                PublishWizardProtectionCommit(accepted);
            return;
        }

        CancelWizardProtectionPreview();
        _wizardProtectionRequest = request;
        _wizardProtectionIssue = null;
        if (_wizardProtectionResult?.Request.Inputs != inputs)
            _wizardProtectionResult = null;

        var cancellation = new CancellationTokenSource();
        _wizardProtectionCancellation = cancellation;
        _wizardProtectionProgressFraction = 0;
        _wizardProtectionProgressStage = _wizardProtectionOperation is { IsCompleted: false }
            ? "Ожидание завершения предыдущего расчёта…" : "Подготовка данных для расчёта…";
        _wizardProtectionStopping = false;
        _wizardProtectionOperation = RunWizardProtectionPreviewAsync(request, pose, alignment, _wizardProtectionResult,
            _wizardProtectionOperation, cancellation, _wizardProtectionHistoryPending);
        RefreshWizardState();
    }

    private void CancelWizardProtectionPreview()
    {
        if (_wizardProtectionCancellation is not { } cancellation)
            return;

        _wizardProtectionCancellation = null;
        _wizardProtectionRequest = null;
        cancellation.Cancel();
    }

    private async Task RunWizardProtectionPreviewAsync(WizardProtectionRequest request,
        TargetRigFittingPoseSnapshot pose, ReplacementTransform alignment, WizardProtectionResult? previousResult,
        Task? previousOperation, CancellationTokenSource cancellation, bool recordEdit)
    {
        CancellationToken token = cancellation.Token;
        CancellationTokenSource? calculationCancellation = null;
        CancellationTokenRegistration cancellationNotice = default;
        try
        {
            await Task.Yield();
            if (recordEdit)
                await Task.Delay(220, token);
            if (previousOperation is not null)
                await previousOperation;
            token.ThrowIfCancellationRequested();
            FlushWizardProtectionHistory();

            calculationCancellation = CancellationTokenSource.CreateLinkedTokenSource(token);
            CancellationToken calculationToken = calculationCancellation.Token;
            calculationCancellation.CancelAfter(MaximumGeneratedSkinningCalculationTime);
            cancellationNotice = calculationToken.Register(() =>
            {
                bool deadlineExpired = !token.IsCancellationRequested;
                Dispatcher.BeginInvoke(new Action(() =>
                {
                    if (!IsWizardProtectionRequestCurrent(request, cancellation)) return;
                    _wizardProtectionStopping = true;
                    _wizardProtectionProgressStage = deadlineExpired
                        ? "Достигнут лимит расчёта 90 секунд. Завершаем текущий блок…"
                        : "Запрошена отмена. Завершаем текущий блок расчёта…";
                    RefreshWizardProtectionProgress();
                }));
            });

            ImportedScene inputScene = previousResult?.InputScene ?? ResolveGeneratedSkinningInputScene();
            SkinnedRenderableMaterialProfile profile = ResolveSkinnedMaterialProfile(inputScene);
            int lastPercent = -1;
            string? lastStage = null;
            var progress = new Progress<GeneratedSkinningProgress>(update =>
            {
                if (!IsWizardProtectionRequestCurrent(request, cancellation) || calculationToken.IsCancellationRequested) return;
                _wizardProtectionProgressFraction = double.IsFinite(update.Fraction) ? Math.Clamp(update.Fraction, 0, 1) : 0;
                _wizardProtectionProgressStage = update.Stage;
                RefreshWizardProtectionProgress();
                int percent = (int)Math.Round(_wizardProtectionProgressFraction * 100);
                if (percent == lastPercent && string.Equals(lastStage, update.Stage, StringComparison.Ordinal)) return;
                lastPercent = percent;
                lastStage = update.Stage;
                SessionLog.Info("WIZARD_PROTECTION_PROGRESS", $"percent={percent}; stage={update.Stage}");
            });
            WizardProtectionResult result = previousResult?.Request == request ? previousResult :
                await Task.Run(() => CalculateWizardProtection(request, inputScene, pose,
                    alignment, profile, previousResult?.Automatic, calculationToken, progress), calculationToken);
            calculationToken.ThrowIfCancellationRequested();
            if (!IsWizardProtectionRequestCurrent(request, cancellation))
                return;

            _wizardProtectionResult = result;
            if (!result.Current.Plan.CanReplace)
                _wizardProtectionIssue = "Не удалось подготовить модель к записи. " + string.Join(" | ", result.Current.Plan.Messages);
            else if (_preparedStep)
                PublishWizardProtectionCommit(result);
            foreach (string message in result.Current.Plan.Messages)
                SessionLog.Info("WIZARD_PROTECTION_PLAN", message);
        }
        catch (OperationCanceledException) when (token.IsCancellationRequested || calculationCancellation?.IsCancellationRequested == true)
        {
            if (IsWizardProtectionRequestCurrent(request, cancellation))
            {
                bool timedOut = !token.IsCancellationRequested;
                string preserved = previousResult is null ? " Настройки сохранены." : " Предыдущий предпросмотр сохранён.";
                _wizardProtectionIssue = timedOut
                    ? "Расчёт защиты остановлен по лимиту 90 секунд. Уменьшите модель или повторите расчёт." + preserved
                    : "Расчёт защиты отменён. Можно изменить настройки или повторить расчёт." + preserved;
                SessionLog.Warning("WIZARD_PROTECTION", timedOut ? "Calculation deadline reached." : "Calculation cancelled by the user.");
            }
        }
        catch (Exception exception)
        {
            if (!IsWizardProtectionRequestCurrent(request, cancellation))
                return;

            SessionLog.Error("WIZARD_PROTECTION", exception);
            _wizardProtectionIssue = exception is WizardProtectionException ? exception.Message :
                DescribeUserError(exception, "Не удалось рассчитать защиту");
        }
        finally
        {
            cancellationNotice.Dispose();
            calculationCancellation?.Dispose();
            if (ReferenceEquals(_wizardProtectionCancellation, cancellation))
            {
                _wizardProtectionCancellation = null;
                if (!_isClosing)
                    RefreshWizardState();
            }
            cancellation.Dispose();
        }
    }

    private bool IsWizardProtectionRequestCurrent(WizardProtectionRequest request, CancellationTokenSource cancellation) =>
        !_isClosing && (_postRigStep || _preparedStep) && ReferenceEquals(_wizardProtectionCancellation, cancellation) &&
        request == _wizardProtectionRequest && request.Inputs == GetWizardProtectionInputs();

    private sealed class WizardProtectionException(string message, Exception? inner = null) : Exception(message, inner);

    private static WizardProtectionResult CalculateWizardProtection(WizardProtectionRequest request, ImportedScene inputScene,
        TargetRigFittingPoseSnapshot pose, ReplacementTransform alignment, SkinnedRenderableMaterialProfile profile,
        GeneratedSkinningCalculationResult? automatic, CancellationToken token, IProgress<GeneratedSkinningProgress> progress)
    {
        SmoDocument target = request.Inputs.Target;
        automatic ??= ComputeGeneratedSkinning(target, inputScene, pose, alignment, pose.Definition, null, null,
            profile, token, new MappedGeneratedSkinningProgress(progress, 0, 0.55));
        progress.Report(new GeneratedSkinningProgress(0.55, "Применение настроек защиты и деталей"));
        GeneratedSkinningAnalysis analysis = automatic.Preparation.Analysis;
        WizardProtectionState state = request.State;
        var head = new List<GeneratedSkinningComponentOverride>();
        var wings = new List<GeneratedSkinningComponentInfo>();
        foreach (var (index, choice) in state.Components)
        {
            if (choice.GeometryFingerprint != analysis.DonorGeometryFingerprint ||
                analysis.Components.FirstOrDefault(component => component.ComponentIndex == index) is not { } component)
                throw new WizardProtectionException("Детали модели изменились. Сбросьте назначение и выберите их заново.");

            if (choice.Attachment == WizardProtectionAttachment.Head)
                head.Add(new GeneratedSkinningComponentOverride(index, GeneratedSkinningComponentAttachmentTarget.Head, component.VerticesByMesh));
            else
                wings.Add(component);
        }

        GeneratedSkinningComponentOverrides? components = head.Count == 0 ? null : new GeneratedSkinningComponentOverrides(
            head, analysis.DonorComponentCount, analysis.TargetRigFingerprint, analysis.DonorGeometryFingerprint);
        GeneratedSkinningRegionOverrides? regions = state.Planes.Count == 0 && state.Hands.Count == 0 ? null :
            analysis.SemanticRegions.CreateOverrides(state.Hands.Values.ToArray(), state.Planes.Values.ToArray());
        GeneratedSkinningCalculationResult current = automatic;
        if (components is not null || regions is not null)
        {
            GeneratedSkinningPreparationResult preparation = PrepareGeneratedSkinningForTarget(target, inputScene, pose,
                alignment, automatic.BodySelection, components, regions, token,
                new MappedGeneratedSkinningProgress(progress, 0.55, 0.25));
            var resolved = preparation.Analysis.SemanticRegions;
            if (resolved.Regions.Any(region => state.Hands.TryGetValue(region.Region, out var adjustment) && adjustment.Enabled && !region.IsApplied))
                throw new WizardProtectionException("Не удалось применить защиту кисти. Поправьте объём или верните автоматическую настройку.");

            progress.Report(new GeneratedSkinningProgress(0.82, "Проверка плана модели после защиты"));
            GlbSkinTransferPlan plan = AnalyzeGlbSkinTransferCore(target, preparation.PreparedScene, profile, token);
            plan = GeneratedSkinningQualityGuard.Apply(preparation.Analysis, plan);
            current = new GeneratedSkinningCalculationResult(alignment, automatic.BodySelection, preparation, plan);
        }

        SmoWingFitResult? wingFit = null;
        SmoWingAttachmentDraft? wingAttachments = null;
        if (wings.Count > 0)
        {
            token.ThrowIfCancellationRequested();
            progress.Report(new GeneratedSkinningProgress(0.90, "Подгонка крыльев и проверка их привязки"));
            try
            {
                wingFit = SmoWingFitter.Fit(target, pose, current.Preparation.FittingPreviewScene, wings,
                    state.WingHeightOffsetPercent, state.WingRotationDegrees);
                wingAttachments = SmoWingAttachmentDraft.Create(wingFit, pose, wings, token);
                GlbSkinTransferPlan plan = AnalyzeGlbSkinTransferCore(target, current.Preparation.PreparedScene,
                    profile, token, wingAttachments);
                plan = GeneratedSkinningQualityGuard.Apply(current.Preparation.Analysis, plan);
                current = current with { Plan = plan };
            }
            catch (Exception exception) when (exception is InvalidDataException or NotSupportedException)
            {
                throw new WizardProtectionException("Не удалось подогнать крылья. Выберите отдельные детали крыльев.", exception);
            }
        }

        token.ThrowIfCancellationRequested();
        progress.Report(new GeneratedSkinningProgress(1, "Защита и детали подготовлены"));
        return new WizardProtectionResult(request, inputScene, automatic, current, components, regions, wingFit, wingAttachments);
    }

    private string? GetWizardProtectionNextBlockMessage() => !WizardProtectionModeAvailable ? null :
        WizardProtectionIsRunning ? "Подождите, рассчитывается защита." :
        WizardProtectionIssue ?? (IsWizardProtectionResultCurrent() ? null : "Подготовка защиты ещё не завершена.");

    private bool TryCommitWizardProtection()
    {
        if (GetWizardProtectionNextBlockMessage() is not null || _wizardProtectionResult is not { } result)
            return false;

        FlushWizardProtectionHistory();
        PublishWizardProtectionCommit(result);
        return true;
    }

    private void PublishWizardProtectionCommit(WizardProtectionResult result)
    {
        if (WizardProtectionCommitIsCurrent)
            return;

        ClearWizardProtectionCommit();
        if (WizardProtectionCanWrite)
        {
            _generatedBodySelection = result.Current.BodySelection;
            _generatedSkinningPreparation = result.Current.Preparation;
            _generatedSkinningPreparationRevision = _rigFittingRevision;
            _generatedSkinningEffectiveScene = result.InputScene;
            _generatedSkinningComponentOverrides = result.Components;
            _generatedSkinningRegionOverrides = result.Regions;
            _glbSkinTransferPlan = result.Current.Plan;
            _wizardProtectionCommittedRequest = result.Request;
            _wizardProtectionWingAttachments = result.WingAttachments;
        }
    }

    private void ClearWizardProtectionCommit()
    {
        bool ownsSkinTransferPlan = _glbSkinTransferPlan is not null &&
            ReferenceEquals(_glbSkinTransferPlan, _wizardProtectionResult?.Current.Plan);
        _wizardProtectionCommittedRequest = null;
        _wizardProtectionWingAttachments = null;
        if (_generatedSkinningPreparation is not null)
            InvalidateGeneratedSkinningPreparation();
        _generatedSkinningComponentOverrides = null;
        _generatedSkinningRegionOverrides = null;
        if (UsesGeneratedWeightsPortingMode || ownsSkinTransferPlan)
            _glbSkinTransferPlan = null;
        _plan = null;
    }

    private string? GetWizardPreparedSaveBlockMessage()
    {
        if (!_preparedStep || _document is null || _replacementScene is null)
            return "Подготовьте обе модели и примените позу скелета.";
        if (WizardBusy)
            return "Подождите, идёт подготовка модели.";
        if (IsStaticReplacementPair)
            return GetWizardSizeSaveBlockMessage();
        if (!WizardRigPoseAccepted)
            return "Примените позу скелета.";
        if (WizardProtectionIssue is { } issue)
            return issue;
        if (RigFittingEditorHasPendingChanges)
            return "Завершите подгонку модели.";
        if (UsesGeneratedWeightsPortingMode)
            return WizardProtectionCommitIsCurrent ? null : "Сначала примите настройки защиты.";
        if (!CanRunSelectedPortingPipeline)
            return GetPortingModeBlockMessage() ?? "Эту модель пока нельзя записать выбранным способом.";
        if (ReplacementIsSmo)
            return _nativeSmoVisualPlan?.CanReplace == true ? null : "Не удалось подготовить замену модели.";
        if (UsesAdaptDonorWeightsPortingMode && !AdaptedPortingPreparationIsCurrent)
            return _adaptedPortingPreparationIssue ?? "Адаптация весов ещё не завершена.";
        return _glbSkinTransferPlan?.CanReplace == true ? null : "Не удалось подготовить модель к записи.";
    }
}
