using System.IO;
using System.Numerics;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using SmoImporter.Core;
using SmoViewer.Core;

namespace SmoImporter.Gui;

public partial class MainWindow
{
    private enum WizardStep { Models, Preparation, Size, Rig, PostRig, Prepared }

    private sealed record InputStepState(string? Source, string? Donor, WizardStep Step, ReplacementTransform Transform,
        bool SizeVisited, bool AlignmentApplied, bool ManualAdaptPlacement,
        PortingModeUiChoice Mode, ImportedTexture[] ExternalTextures, NativeDonorTextureEdit[] NativeTextures,
        string? ObjMaterialPath, IReadOnlyDictionary<int, ImportedTexture> ManualTextures, WizardRigState? Rig,
        WizardProtectionState? Protection);

    private readonly List<InputStepState> _inputHistory =
        [new(null, null, WizardStep.Models, ReplacementTransform.Identity, false, false, false,
            PortingModeUiChoice.Auto, [], [], null, NoTextureAssignments, null, null)];
    private int _inputHistoryIndex;
    private WizardStep _wizardStep;
    private bool _preparationStep => _wizardStep == WizardStep.Preparation;
    private bool _sizeStep => _wizardStep == WizardStep.Size;
    private bool _rigStep => _wizardStep == WizardStep.Rig;
    private bool _postRigStep => _wizardStep == WizardStep.PostRig;
    private bool _preparedStep => _wizardStep == WizardStep.Prepared;
    private bool WizardShowsRigPreview => _rigStep;
    private bool WizardShowsSourceComparison => _sizeStep || WizardShowsRigPreview;
    private bool WizardShowsComparison => WizardShowsSourceComparison || _postRigStep || _preparedStep;
    private bool _inputActionRunning;
    private bool _replayingInputStep;
    private string? _inputError;
    private string? _staticSaveNotice;
    private bool _wizardSizeVisited;
    private bool _wizardAlignmentApplied;

    private bool WizardBaseBusy => _inputActionRunning || _replacementLoadRunning ||
        _generatedSkinningCalculationRunning || _protectedRegionPreviewRunning ||
        _saveRunning || _nativeValidationRunning;
    private bool WizardBusy => WizardBaseBusy || WizardProtectionIsRunning || WizardResultIsPreparing;

    private bool HasInputSource => _sourceScene?.Meshes.Any(mesh =>
        mesh.Positions.Length > 0 && mesh.TriangleIndices.Length >= 3) == true;
    private bool HasInputDonor => _baseReplacementScene?.Meshes.Any(mesh =>
        mesh.Positions.Length > 0 && mesh.TriangleIndices.Length >= 3) == true;

    private bool HasWizardTargetSkeleton => _sourceScene?.Skins.Count > 0 ||
        _document?.Objects.Any(entry => entry.TypeHash == SmoClassIds.Skin) == true;

    private bool CanEnterWizardRig => HasInputSource && HasInputDonor && HasWizardTargetSkeleton;

    private InputStepState CaptureInputStep() => new(_sourcePath, _replacementPath, _wizardStep, _wizardTransform,
        _wizardSizeVisited, _wizardAlignmentApplied, ManualAdaptWeights,
        SelectedPortingModeChoice, _externalTextureHistory, _nativeDonorTextureEdits,
        _objMaterialOverridePath, _manualTextureAssignments, CaptureWizardRigState(), CaptureWizardProtectionState());

    private void ResetInputHistory()
    {
        _inputHistory.Clear();
        _inputHistory.Add(CaptureInputStep());
        _inputHistoryIndex = 0;
    }

    private void RecordInputStep()
    {
        InputStepState current = CaptureInputStep();
        if (current == _inputHistory[_inputHistoryIndex])
            return;

        int next = _inputHistoryIndex + 1;
        _inputHistory.RemoveRange(next, _inputHistory.Count - next);
        _inputHistory.Add(current);
        _inputHistoryIndex = next;
        _staticSaveNotice = null;
    }

    private void WizardBack_Click(object sender, RoutedEventArgs e)
    {
        if (_wizardStep != WizardStep.Models && !WizardBaseBusy)
        {
            FlushWizardProtectionHistory();
            MoveToInputStep(_wizardStep switch
            {
                WizardStep.Prepared => !HasWizardTargetSkeleton ? WizardStep.Size : WizardRigSupportsHumanoid ? WizardStep.PostRig : WizardStep.Rig,
                WizardStep.PostRig => WizardStep.Rig,
                WizardStep.Rig => WizardStep.Size,
                WizardStep.Size => WizardStep.Preparation,
                _ => WizardStep.Models
            }, record: true);
        }
    }

    private void WizardNext_Click(object sender, RoutedEventArgs e)
    {
        if (WizardBusy || !HasInputSource || !HasInputDonor)
            return;
        if (_wizardStep == WizardStep.Models)
            MoveToInputStep(WizardStep.Preparation, record: true);
        else if (_preparationStep)
            MoveToInputStep(WizardStep.Size, record: true);
        else if (_rigStep)
            AdvanceWizardRig();
        else if (_postRigStep)
            AdvanceWizardProtection();
        else if (_preparedStep)
            return;
        else if (_sizeStep && CanEnterWizardRig && !HasUnappliedWizardTransform)
            MoveToInputStep(WizardStep.Rig, record: true);
        else if (_sizeStep && IsStaticReplacementPair && GetWizardSizeSaveBlockMessage() is null)
            MoveToInputStep(WizardStep.Prepared, record: true);
    }

    private void AdvanceWizardRig()
    {
        if (GetWizardRigNextBlockMessage() is not null)
            return;

        _inputError = null;
        if (!WizardRigPoseAccepted || WizardRigHasDraft)
        {
            if (!TryApplyWizardRigPose())
            {
                RefreshWizardState();
                return;
            }
            InvalidateWizardProtectionPose();
        }
        MoveToInputStep(WizardRigSupportsHumanoid ? WizardStep.PostRig : WizardStep.Prepared, record: true);
    }

    private void AdvanceWizardProtection()
    {
        if (GetWizardProtectionNextBlockMessage() is not null)
            return;
        if (UsesGeneratedWeightsPortingMode && !TryCommitWizardProtection())
        {
            RefreshWizardState();
            return;
        }
        MoveToInputStep(WizardStep.Prepared, record: true);
    }

    private async void WizardUndo_Click(object sender, RoutedEventArgs e) =>
        await ReplayInputStepAsync(-1);

    private async void WizardRedo_Click(object sender, RoutedEventArgs e) =>
        await ReplayInputStepAsync(1);

    private void MoveToInputStep(WizardStep step, bool record)
    {
        EndWizardProtectionRectangleSelection();
        bool enteringSize = step == WizardStep.Size && !_sizeStep;
        bool enteringProtection = step == WizardStep.PostRig && !_postRigStep;
        bool enteringComparison = (step is WizardStep.Size or WizardStep.Rig or WizardStep.PostRig or WizardStep.Prepared) &&
            !WizardShowsComparison;
        bool leavingSizeDuringReplay = _replayingInputStep && _sizeStep && step == WizardStep.Preparation;
        if (_rigStep && step != WizardStep.Rig)
            DiscardWizardRigDraft();
        if ((_postRigStep || _preparedStep) && step != _wizardStep)
            CancelWizardProtectionPreview();
        _wizardStep = step;
        if (record)
        {
            _inputError = null;
            if (enteringSize)
                ApplyInitialWizardFit();
            if (_rigStep && UsesGeneratedWeightsPortingMode)
                _wizardAlignmentApplied = true;
            RecordInputStep();
        }

        ModelsStep.Visibility = step == WizardStep.Models ? Visibility.Visible : Visibility.Collapsed;
        PreparationStep.Visibility = _preparationStep ? Visibility.Visible : Visibility.Collapsed;
        SizeStep.Visibility = _sizeStep ? Visibility.Visible : Visibility.Collapsed;
        RigStep.Visibility = _rigStep ? Visibility.Visible : Visibility.Collapsed;
        PostRigStep.Visibility = _postRigStep ? Visibility.Visible : Visibility.Collapsed;
        PreparedStep.Visibility = _preparedStep ? Visibility.Visible : Visibility.Collapsed;
        if (step != WizardStep.Models)
            ReleaseSourceInputPreview();
        if (!_preparationStep)
            ClearTextureFocus();
        if (WizardShowsComparison || leavingSizeDuringReplay)
            SyncWizardAlignmentForMode();
        RefreshWizardState();
        if (enteringComparison || enteringProtection)
            FrameWizardFittedModel();
        if (_postRigStep && !_replayingInputStep)
            EnsureWizardProtectionPreview();
    }

    private void ApplyInitialWizardFit()
    {
        if (_wizardSizeVisited)
            return;

        _wizardSizeVisited = true;
        // Startup presets and explicit edits already describe the intended placement.
        if (_wizardAlignmentApplied || _wizardTransform != ReplacementTransform.Identity ||
            !WizardCanAdjustScale || !TryGetWizardEntryFit(out ReplacementTransform transform))
            return;

        _wizardTransform = transform;
        _wizardAlignmentApplied = UsesGeneratedWeightsPortingMode;
    }

    private async Task ReplayInputStepAsync(int offset)
    {
        if (WizardBaseBusy)
            return;
        FlushWizardProtectionHistory();
        int index = _inputHistoryIndex + offset;
        if (index < 0 || index >= _inputHistory.Count)
            return;

        InputStepState previous = CaptureInputStep();
        InputStepState destination = _inputHistory[index];
        _staticSaveNotice = null;
        CancelWizardProtectionPreview();
        _inputActionRunning = true;
        _replayingInputStep = true;
        _inputError = null;
        DiscardWizardRigDraft();
        ControlsScrollViewer.IsEnabled = false;
        RefreshWizardState();
        try
        {
            await RestoreInputFilesAsync(destination);
            if (destination.Step != WizardStep.Models && (!HasInputSource || !HasInputDonor))
                throw new InvalidDataException("Для этого шага нужны обе модели.");
            bool requiresRig = destination.Step is WizardStep.Rig or WizardStep.PostRig ||
                destination.Step == WizardStep.Prepared && !IsStaticReplacementPair;
            if (requiresRig && !CanEnterWizardRig)
                throw new InvalidDataException("Для этого шага нужен скелет исходной модели.");
            if ((destination.Step == WizardStep.PostRig || destination.Step == WizardStep.Prepared && requiresRig) &&
                !WizardRigPoseAccepted)
                throw new InvalidDataException("Не удалось восстановить применённую позу.");
            if (destination.Step == WizardStep.PostRig && !WizardRigSupportsHumanoid)
                throw new InvalidDataException("Для защиты нужна гуманоидная схема.");

            _inputHistoryIndex = index;
            MoveToInputStep(destination.Step, record: false);
            RefreshState();
        }
        catch (Exception exception) when (!_isClosing)
        {
            SessionLog.Error("INPUT_HISTORY", exception);
            await RecoverInputFilesAsync(previous);
            _inputError = DescribeUserError(exception, "состояние из истории");
        }
        catch (Exception) when (_isClosing) { }
        finally
        {
            _inputActionRunning = false;
            _replayingInputStep = false;
            if (!_isClosing)
            {
                ControlsScrollViewer.IsEnabled = !WizardBaseBusy;
                RefreshWizardState();
                if (_postRigStep || _preparedStep)
                    EnsureWizardProtectionPreview();
            }
        }
    }

    private async Task RestoreInputFilesAsync(InputStepState state)
    {
        bool sourceChanged = !SameInputFile(_sourcePath, state.Source);
        bool donorChanged = !SameInputFile(_replacementPath, state.Donor);
        if ((sourceChanged && state.Source is not null && !File.Exists(state.Source)) ||
            (donorChanged && state.Donor is not null && !File.Exists(state.Donor)))
        {
            throw new FileNotFoundException("A file from the input history is no longer available.");
        }

        if (sourceChanged)
        {
            if (state.Source is null)
                ClearSourceInput();
            else if (!await LoadSourceAsync(state.Source))
                throw new InvalidDataException(_inputError ?? "Не удалось восстановить исходный SMO.");
        }

        if (donorChanged)
        {
            if (state.Donor is null)
                ClearDonorInput();
            else
                await LoadExternalReplacementAsync(state.Donor);
        }
        _wizardTransform = state.Transform;
        _wizardSizeVisited = state.SizeVisited;
        _wizardAlignmentApplied = state.AlignmentApplied;
        SetWizardManualPlacement(state.ManualAdaptPlacement);
        if (_isClosing)
            return;
        if (state.Donor is null)
        {
            RestoreWizardRigState(null);
            RestoreWizardProtectionState(null);
            return;
        }

        if (ReplacementIsSmo)
            await RestoreNativeDonorTexturesAsync(state.NativeTextures);
        else if (ReplacementIsObj && !SameInputFile(_objMaterialOverridePath, state.ObjMaterialPath))
            await RestoreObjMaterialAsync(state.ObjMaterialPath, state.ExternalTextures, state.ManualTextures);
        else if (!_externalTextureHistory.SequenceEqual(state.ExternalTextures) ||
                 !ReferenceEquals(_manualTextureAssignments, state.ManualTextures))
        {
            ApplyTextureAssignments(state.ExternalTextures, state.ManualTextures);
            _externalTextureHistory = state.ExternalTextures;
        }
        if (SelectedPortingModeChoice != state.Mode)
        {
            SetPortingModeChoice(state.Mode);
            ApplyPortingModeSelection();
        }
        if (state.Rig is not null)
            SyncWizardAlignmentForMode();
        RestoreWizardRigState(state.Rig);
        RestoreWizardProtectionState(state.Protection);
    }

    private async Task RecoverInputFilesAsync(InputStepState previous)
    {
        try
        {
            await RestoreInputFilesAsync(previous);
        }
        catch (Exception exception)
        {
            SessionLog.Error("INPUT_RECOVERY", exception);
            if (!_isClosing && _wizardStep != WizardStep.Models && (!HasInputSource || !HasInputDonor))
                MoveToInputStep(WizardStep.Models, record: false);
            ResetInputHistory();
        }
    }

    private static bool SameInputFile(string? left, string? right) =>
        string.Equals(left, right, StringComparison.OrdinalIgnoreCase);

    private bool CanDropInputFile(Border surface) =>
        !_isClosing && !WizardBusy && !WizardShowsComparison && (!_preparationStep || surface == DonorPreviewSurface);

    private string? GetInputDropFile(Border surface, DragEventArgs e)
    {
        if (!CanDropInputFile(surface) || (e.AllowedEffects & DragDropEffects.Copy) == 0 ||
            !e.Data.GetDataPresent(DataFormats.FileDrop, autoConvert: false) ||
            e.Data.GetData(DataFormats.FileDrop, autoConvert: false) is not string[] { Length: 1 } files ||
            files[0] is not { Length: > 0 } path || !Path.IsPathFullyQualified(path))
            return null;

        string extension = Path.GetExtension(path).ToLowerInvariant();
        return extension == ".smo" ||
            (surface == DonorPreviewSurface && extension is ".fbx" or ".glb" or ".obj")
            ? path : null;
    }

    private void InputPreview_DragOver(object sender, DragEventArgs e)
    {
        var surface = (Border)sender;
        bool accepted = GetInputDropFile(surface, e) is not null;
        e.Effects = accepted ? DragDropEffects.Copy : DragDropEffects.None;
        if (accepted)
            surface.BorderBrush = Brushes.NavajoWhite;
        else
            surface.ClearValue(Border.BorderBrushProperty);
        e.Handled = true;
    }

    private void InputPreview_DragLeave(object sender, DragEventArgs e) =>
        ((Border)sender).ClearValue(Border.BorderBrushProperty);

    private async void InputPreview_Drop(object sender, DragEventArgs e)
    {
        var surface = (Border)sender;
        surface.ClearValue(Border.BorderBrushProperty);
        e.Handled = true;
        string? path = GetInputDropFile(surface, e);
        e.Effects = path is null ? DragDropEffects.None : DragDropEffects.Copy;
        if (path is null)
        {
            if (CanDropInputFile(surface) && e.Data.GetDataPresent(DataFormats.FileDrop, autoConvert: false))
            {
                _inputError = surface == SourcePreviewSurface
                    ? "Перетащите один файл SMO."
                    : "Перетащите один файл SMO, FBX, GLB или OBJ.";
                RefreshWizardState();
            }
            return;
        }

        if (surface == SourcePreviewSurface)
            await LoadSelectedSourceAsync(path);
        else
            await LoadSelectedDonorAsync(path);
    }

    private async Task LoadSelectedSourceAsync(string path)
    {
        InputStepState previous = CaptureInputStep();
        _inputActionRunning = true;
        _inputError = null;
        RefreshWizardState();
        try
        {
            bool loaded = await LoadSourceAsync(path);
            if (_isClosing) return;
            if (loaded)
            {
                if (!SameInputFile(path, previous.Source)) ResetWizardTransform();
            }
            else
            {
                string? error = _inputError;
                await RecoverInputFilesAsync(previous);
                _inputError = error;
            }
        }
        finally
        {
            _inputActionRunning = false;
            if (!_isClosing)
            {
                RecordInputStep();
                RefreshWizardState();
            }
        }
    }

    private async Task LoadSelectedDonorAsync(string path)
    {
        InputStepState previous = CaptureInputStep();
        _inputActionRunning = true;
        _inputError = null;
        RefreshWizardState();
        try
        {
            if (Path.GetExtension(path).Equals(".fbx", StringComparison.OrdinalIgnoreCase) &&
                !EnsureBlenderForFbx())
                return;

            await LoadExternalReplacementAsync(path);
            if (!SameInputFile(previous.Donor, _replacementPath))
                ResetWizardTransform();
            _framePreviewOnRefresh = true;
            RefreshState();
        }
        catch (Exception exception) when (!_isClosing)
        {
            await RecoverInputFilesAsync(previous);
            RefreshState();
            ReportInputError(exception, exception is OperationCanceledException
                ? "Загрузка модели отменена."
                : DescribeUserError(exception, "Модель донора", path));
        }
        catch (Exception) when (_isClosing) { }
        finally
        {
            _inputActionRunning = false;
            if (!_isClosing)
            {
                RecordInputStep();
                RefreshWizardState();
            }
        }
    }

    private void ClearSourceInput()
    {
        _sourcePath = null;
        _document = null;
        _sourceScene = null;
        _targetFittingGpuPreview?.Clear();
        _plan = null;
        _glbSkinTransferPlan = null;
        _nativeSmoVisualPlan = null;
        _staticModelReplacementPlan = null;
        ResetRigFittingState();
        InvalidateAdaptedPortingPreparation();
        InvalidateGeneratedSkinningPreparation();
        SourcePathText.Text = "Не выбран";
        SourceSummaryText.Text = "—";
        UpdatePortingModeRecommendation();
        RefreshState();
    }

    private void ClearDonorInput()
    {
        BeginReplacementLoad();
        _portingModeRecommendation = null;
        SetPortingModeChoice(PortingModeUiChoice.Auto);
        ReplacementPathText.Text = "Не выбрана";
        ReplacementSummaryText.Text = "—";
        UpdatePortingModeRecommendation();
        RefreshTextureList();
        RefreshState();
    }

    private void ReportInputError(Exception exception, string message)
    {
        SessionLog.Error("INPUT_STEP", exception);
        _inputError = message;
        RefreshWizardState();
    }

    private bool WizardCanAdjustScale => !ReplacementIsSmo || NativeStaticFallbackReady ||
        !IsStaticReplacementPair || _nativeSmoVisualPlan?.CanScaleStatic == true;

    private bool WizardCanAdjustPlacement => NativeStaticFallbackReady ||
        !ReplacementIsSmo && (UsesStaticModelPortingMode || UsesGeneratedWeightsPortingMode ||
                             UsesAdaptDonorWeightsPortingMode && ManualAdaptWeights);

    private void SetWizardManualPlacement(bool manual)
    {
        if (ManualAdaptWeights == manual)
            return;
        SetManualAdaptWeights(manual);
        _manualAlignmentEditorDirty = false;
        _plan = null;
        _glbSkinTransferPlan = null;
        InvalidateAdaptedPortingPreparation();
    }

    private void WizardManualPlacement_Click(object sender, RoutedEventArgs e)
    {
        if (!_sizeStep || WizardBusy || !UsesAdaptDonorWeightsPortingMode)
            return;

        SetWizardManualPlacement(!ManualAdaptWeights);
        ResetUnsupportedWizardPlacement();
        InvalidateWizardRigAlignment();
        InvalidateWizardProtectionPose();
        SyncWizardAlignmentForMode();
        _inputError = null;
        RecordInputStep();
        RefreshWizardState();
    }

    private void ResetWizardTransform()
    {
        _wizardTransform = ReplacementTransform.Identity;
        _wizardSizeVisited = false;
        _wizardAlignmentApplied = false;
        ResetWizardRigState();
        ResetWizardProtectionState();
    }

    private void ResetUnsupportedWizardPlacement()
    {
        if (!WizardCanAdjustPlacement && (_wizardTransform.RotationDegrees != Vector3.Zero ||
                                          _wizardTransform.Translation != Vector3.Zero))
            _wizardTransform = new ReplacementTransform(_wizardTransform.Scale, Vector3.Zero, Vector3.Zero);
    }

    private void ApplyWizardAlignmentForMode()
    {
        if (UsesGeneratedWeightsPortingMode)
            _wizardAlignmentApplied = true;
        SyncWizardAlignmentForMode();
    }

    private void SyncWizardAlignmentForMode()
    {
        if (UsesGeneratedWeightsPortingMode && HasInputSource && HasInputDonor &&
            (_generatedDonorAlignment != _wizardTransform || _generatedAlignmentAccepted != _wizardAlignmentApplied))
        {
            if (_wizardAlignmentApplied)
            {
                CommitGeneratedDonorAlignment(_wizardTransform, "Подгонка применена.", recalculate: false);
                _generatedAlignmentAccepted = true;
            }
            else
            {
                SetGeneratedDonorAlignmentState(_wizardTransform, updateEditor: true);
                InvalidateGeneratedSkinningPreparation();
            }
        }
        else if (UsesAdaptDonorWeightsPortingMode && ManualAdaptWeights && _manualDonorAlignment != _wizardTransform)
        {
            _manualDonorAlignment = _wizardTransform;
            WriteTransformEditor(_wizardTransform);
            InvalidateAdaptedPortingPreparation();
        }
    }

    private string? GetWizardSizeSaveBlockMessage()
    {
        if (HasUnappliedWizardTransform)
            return "Примените изменения подгонки.";
        if (GetStaticSaveBlockMessage() is { } block)
            return block;
        if (!WizardCanAdjustPlacement && (_wizardTransform.RotationDegrees != Vector3.Zero ||
                                          _wizardTransform.Translation != Vector3.Zero))
            return "Эта пара SMO поддерживает только изменение масштаба.";
        if (_wizardTransform.Scale != 1f && !WizardCanAdjustScale)
            return "Запись нового размера для этой пары SMO пока недоступна.";
        return null;
    }

    private void RefreshWizardState()
    {
        if (WizardNextButton is null)
            return;

        bool busy = WizardBusy;
        bool hasSource = HasInputSource;
        bool hasDonor = HasInputDonor;
        bool models = _wizardStep == WizardStep.Models;
        bool hasRig = HasWizardTargetSkeleton;
        SourceSelectionButton.IsEnabled = !busy;
        DonorSelectionButton.IsEnabled = !busy && !WizardShowsComparison;
        DonorSelectionButton.Visibility = WizardShowsComparison ? Visibility.Hidden : Visibility.Visible;
        DonorPreviewHeading.Text = _preparedStep ? "Готовая модель" : WizardShowsRigPreview ? "Скелет и модели" : _sizeStep ? "Сравнение моделей" : "Модель-донор";
        DonorPreviewHint.ToolTip = _preparedStep ? "Собранный SMO с текстурами. Анимации и видимость не меняют экспортируемый файл." :
            _rigStep ? "Скелет принадлежит исходной модели. Щёлкните по кости, чтобы выделить её." :
            WizardShowsSourceComparison ? "Серая модель — исходная, зелёная — донор после подгонки." :
            "Это модель, которая появится в игре вместо исходной.";
        SourceSelectionText.Text = InputFileLabel(_sourcePath, hasSource, "Файл ещё не выбран");
        DonorSelectionText.Text = _replacementLoadRunning ? "Загружается…" :
            InputFileLabel(_replacementPath, hasDonor, "Файл ещё не выбран");
        SourceSelectionText.ToolTip = _sourcePath;
        DonorSelectionText.ToolTip = _replacementPath;
        ModelsStepLabel.Foreground = models ? Brushes.NavajoWhite : Brushes.LightSlateGray;
        PreparationStepLabel.Foreground = _preparationStep ? Brushes.NavajoWhite : Brushes.LightSlateGray;
        SizeStepLabel.Foreground = _sizeStep ? Brushes.NavajoWhite : Brushes.LightSlateGray;
        RigStepLabel.Foreground = _rigStep ? Brushes.NavajoWhite : hasRig ? Brushes.LightSlateGray : Brushes.DimGray;
        RigStepHint.Foreground = hasRig ? Brushes.LightSlateGray : Brushes.DimGray;
        RigStepHint.ToolTip = hasRig ? "Подгоните скелет исходной модели под донора." :
            hasSource ? "У исходной модели нет скелета. Этот шаг пропускается." :
            "Этот шаг нужен для исходной модели со скелетом.";
        RigStepLabel.ToolTip = RigStepHint.ToolTip;
        PostRigStepLabel.Foreground = _postRigStep ? Brushes.NavajoWhite : WizardRigSupportsHumanoid ? Brushes.LightSlateGray : Brushes.DimGray;
        PostRigStepLabel.ToolTip = WizardRigSupportsHumanoid ? "Настройте защитные области и отдельные детали." :
            "Шаг применяется к моделям с гуманоидным скелетом.";
        PreparedStepLabel.Foreground = _preparedStep ? Brushes.NavajoWhite : hasSource ? Brushes.LightSlateGray : Brushes.DimGray;
        WizardBackButton.IsEnabled = !models && !WizardBaseBusy;
        WizardUndoButton.IsEnabled = _inputHistoryIndex > 0 && !WizardBaseBusy;
        WizardRedoButton.IsEnabled = _inputHistoryIndex < _inputHistory.Count - 1 && !WizardBaseBusy;
        if (models)
            RefreshSourceInputPreview();
        RefreshWizardRigPresentation();
        RefreshWizardProtectionPresentation();
        RefreshDonorInputPreview();
        RefreshWizardProtectionPreview();
        RefreshWizardSize();
        RefreshWizardComparisonOverlays();
        EnsureWizardResult();
        RefreshWizardResultPreview();
        RefreshWizardResultControls();
        bool staticSave = _sizeStep && IsStaticReplacementPair;
        bool openRig = _sizeStep && CanEnterWizardRig;
        string? sizeBlock = !_sizeStep ? null : staticSave ? GetWizardSizeSaveBlockMessage() :
            openRig ? HasUnappliedWizardTransform ? "Примените изменения подгонки." : null :
                "Для этой пары пока доступен только предпросмотр размера.";
        string? rigBlock = _rigStep ? GetWizardRigNextBlockMessage() : null;
        string? protectionBlock = _postRigStep ? GetWizardProtectionNextBlockMessage() : null;
        string? preparedBlock = _preparedStep ? GetWizardPreparedSaveBlockMessage() : null;
        WizardNextButton.IsEnabled = !_preparedStep && !WizardBusy && hasSource && hasDonor && sizeBlock is null && rigBlock is null &&
            protectionBlock is null && preparedBlock is null;
        string? nextHint = !models ? null : !hasSource && !hasDonor
            ? "Выберите исходный SMO и модель донора."
            : !hasSource ? "Выберите исходный SMO."
            : !hasDonor ? "Выберите модель донора."
            : null;
        WizardNextButton.ToolTip = busy ? "Дождитесь завершения текущей операции." :
            _preparedStep ? "Последний шаг. Используйте кнопки экспорта и проверки загрузки." :
            _postRigStep ? protectionBlock ?? "Применить защиту и детали и перейти дальше." :
            _rigStep ? rigBlock ?? "Применить текущую позу и перейти к следующему шагу." :
            _sizeStep ? sizeBlock ?? (openRig ? "Перейти к подгонке скелета." : "Перейти к готовой модели.") :
            _preparationStep ? "Перейти к подгонке размера." :
            nextHint ?? "Перейти к настройке импорта.";
        string? message = _inputError ?? (_preparedStep ? _staticSaveNotice ?? preparedBlock :
            _postRigStep ? null : WizardShowsRigPreview ? _wizardRigPreviewIssue : _sizeStep ?
            _staticSaveNotice ?? sizeBlock ?? (staticSave ? "«Далее» откроет готовую модель." : null) :
            models && busy ? "Подождите, идёт загрузка." : nextHint);
        WizardMessageText.Text = message ?? string.Empty;
        WizardMessageText.Foreground = _inputError is null ? Brushes.LightSlateGray : Brushes.Salmon;
        WizardMessageText.Visibility = message is null || _inputError is not null ? Visibility.Collapsed : Visibility.Visible;
        RefreshObjMaterialControls();
        UpdateNativeDonorTextureActions();
        RefreshWizardNotices();
        RefreshPreviewGuidance();
    }

    private static string InputFileLabel(string? path, bool ready, string emptyLabel) =>
        path is null ? emptyLabel : ready ? Path.GetFileName(path) : "В файле нет геометрии";

}
