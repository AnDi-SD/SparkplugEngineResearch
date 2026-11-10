using System.IO;
using System.Windows;
using System.Windows.Controls;
using Microsoft.Win32;
using SmoImporter.Core;
using SmoViewer.Core;

namespace SmoImporter.Gui;

public partial class MainWindow
{
    private sealed record NativeDonorTextureEdit(int ObjectIndex, ImportedTexture Image, bool Automatic = false);

    private sealed record NativeDonorTextureItem(NativeDonorTextureDraft.TextureSlot Slot, bool Edited)
    {
        public bool CanReplace => Slot.CanReplace;
        public string Display => $"{Slot.Name}  ·  {Slot.Width} × {Slot.Height}" +
            (Edited ? "  ·  изменена" : Slot.CanReplace ? "" : "  ·  только просмотр");
    }

    private sealed record NativeReplacementAnalysis(SmoNativeVisualGraphPlan NativePlan,
        ImportedScene? StaticScene = null, SmoLevelModelGraphReplacementAnalysis? StaticPlan = null);

    private SmoDocument? _nativeDonorDocument;
    private ImportedScene? _staticNativeReplacementScene;
    private SmoDocument? _editedNativeDonorDocument;
    private IReadOnlyList<SmoModelReader.NativePreviewMaterial>? _editedNativeDonorMaterials;
    private IReadOnlyList<NativeDonorTextureDraft.TextureSlot> _nativeDonorTextureSlots = [];
    private NativeDonorTextureEdit[] _nativeDonorTextureEdits = [];
    private ImportedTexture[] _externalTextureHistory = [];
    private IReadOnlyList<NativeDonorTextureDraft.TextureSlot>? _listedNativeTextureSlots;
    private NativeDonorTextureEdit[]? _listedNativeTextureEdits;

    private SmoDocument CurrentNativeDonorDocument => _editedNativeDonorDocument ?? _nativeDonorDocument ??
        throw new InvalidOperationException("Выберите SMO-донора.");

    private void ClearDonorTextureDraft()
    {
        _nativeDonorDocument = null;
        _staticNativeReplacementScene = null;
        _editedNativeDonorDocument = null;
        _editedNativeDonorMaterials = null;
        _nativeDonorTextureSlots = [];
        _nativeDonorTextureEdits = [];
        _externalTextureHistory = [];
        _manualTextureAssignments = NoTextureAssignments;
        _objMaterialOverridePath = null;
        ClearTextureFocus();
    }

    private bool IsStaticReplacementPair => AllReplacementMeshesAreUnskinned && HasInputSource &&
        _document is { } target && !target.Objects.Any(entry => entry.TypeHash == SmoClassIds.Skin);

    private bool NativeStaticFallbackReady => ReplacementIsSmo && _staticNativeReplacementScene is not null &&
        _staticModelReplacementPlan?.CanReplace == true;

    private static NativeReplacementAnalysis AnalyzeNativeReplacement(SmoDocument target,
        SmoDocument donor, bool staticPair)
    {
        SmoNativeVisualGraphPlan plan = SmoNativeVisualGraphReplacer.Analyze(target, donor);
        if (!staticPair || (target.Header.PlatformMask & 2) == 0 ||
            !plan.HasMeshPlatformMismatch)
            return new NativeReplacementAnalysis(plan);

        try
        {
            ImportedScene scene = SmoModelReader.Read(donor);
            SmoLevelModelGraphReplacementAnalysis rebuilt = SmoLevelModelGraphReplacer.Analyze(target,
                GetFirstMeshObjectIndex(target), scene, SmoLevelModelGraphWriteOptions.StandaloneStatic);
            if (rebuilt.CanReplace)
                return new NativeReplacementAnalysis(plan, scene, rebuilt);
            SessionLog.Info("STATIC_SMO_FALLBACK", string.Join(" | ", rebuilt.Messages));
        }
        catch (Exception exception) when (exception is InvalidDataException or InvalidOperationException or
            ArgumentException or NotSupportedException or OverflowException)
        {
            SessionLog.Error("STATIC_SMO_FALLBACK", exception);
        }
        return new NativeReplacementAnalysis(plan);
    }

    private void ApplyNativeStaticFallback(NativeReplacementAnalysis? analysis)
    {
        _staticNativeReplacementScene = analysis?.StaticScene;
        _staticModelReplacementPlan = analysis?.StaticPlan;
    }

    private string? GetStaticSaveBlockMessage()
    {
        if (!ReplacementIsSmo && !UsesStaticModelPortingMode)
            return "Выберите статическую замену.";
        if (!CanRunSelectedPortingPipeline)
        {
            var details = ReplacementIsSmo ? _nativeSmoVisualPlan?.Errors : _staticModelReplacementPlan?.Messages;
            if (details?.Any(message => message.Contains("TEXTURE_DESTINATION_PLATFORM", StringComparison.Ordinal)) == true)
                return "В этот вариант SMO пока нельзя записать текстуры внешней модели.";
            if (ReplacementIsSmo && _nativeSmoVisualPlan?.HasMeshPlatformMismatch == true)
                return "У исходного SMO и донора разные форматы геометрии.";
            return "Эта пара моделей несовместима. " +
                (details is null ? "Выберите другой режим или модель." : string.Join(" | ", details));
        }
        return null;
    }

    private void UpdatePreparationOverview()
    {
        if (PreparationModelTypeText is null || _baseReplacementScene is not { } donor)
            return;

        int skinned = donor.Meshes.Count(mesh => mesh.Skinning is not null);
        string modelType = skinned == 0 ? "без костей" :
            skinned == donor.Meshes.Count ? "с костями" : "часть деталей с костями";
        string format = Path.GetExtension(_replacementPath ?? "").TrimStart('.').ToUpperInvariant();
        PreparationModelTypeText.Text = $"{format} · {modelType}";

        bool native = ReplacementIsSmo;
        bool staticPair = IsStaticReplacementPair;
        PortingModePanel.Visibility = native && !staticPair ? Visibility.Collapsed : Visibility.Visible;
        PreparationModeHintText.Visibility = native && !staticPair ? Visibility.Visible : Visibility.Collapsed;
        if (native && !staticPair)
        {
            PreparationModeHintText.Text = _nativeSmoVisualPlan?.CanReplace == true
                ? "Прямой перенос SMO. Совместимость подтверждена."
                : "Прямой перенос SMO недоступен для этой пары.";
            return;
        }

        bool targetHasBones = _sourceScene?.Meshes.Any(mesh => mesh.SkinObjectIndex is not null) == true;
        bool donorHasWeights = AllReplacementMeshesAreSkinned;
        SetPreparationModeAvailable(1, !native && targetHasBones && donorHasWeights);
        SetPreparationModeAvailable(2, !native && targetHasBones && donorHasWeights);
        SetPreparationModeAvailable(3, !native && targetHasBones);
        SetPreparationModeAvailable(4, staticPair);
        if (staticPair)
        {
            PortingModeRecommendationText.Text = GetStaticSaveBlockMessage() ?? (NativeStaticFallbackReady
                ? "Статическая замена с преобразованием формата."
                : native ? "Прямой перенос статической модели." : "Рекомендуем статическую замену.");
            return;
        }

        PortingModeRecommendationText.Text = _portingModeRecommendation?.Mode switch
        {
            ModelPortingMode.PreparedGameSkeleton => "Рекомендуем готовый игровой скелет.",
            ModelPortingMode.AdaptDonorWeights => "Рекомендуем адаптацию скелета донора.",
            ModelPortingMode.GenerateWeights when targetHasBones => "Рекомендуем создать веса для игрового скелета.",
            _ => "Для этой пары нет подходящего способа."
        };
    }

    private void SetPreparationModeAvailable(int index, bool available)
    {
        var choice = (ListBoxItem)PortingModeChoices.Items[index];
        choice.IsEnabled = available;
        choice.Visibility = available || choice.IsSelected ? Visibility.Visible : Visibility.Collapsed;
    }

    private void RefreshNativeDonorTextureEditor()
    {
        if (NativeDonorTextureEditor is null)
            return;
        NativeDonorTextureEditor.Visibility = ReplacementIsSmo ? Visibility.Visible : Visibility.Collapsed;
        if (!ReferenceEquals(_listedNativeTextureSlots, _nativeDonorTextureSlots) ||
            !ReferenceEquals(_listedNativeTextureEdits, _nativeDonorTextureEdits))
        {
            int? selected = _textureFocus?.NativeIndex;
            var edited = _nativeDonorTextureEdits.Select(item => item.ObjectIndex).ToHashSet();
            var items = _nativeDonorTextureSlots.Select(slot =>
                new NativeDonorTextureItem(slot, edited.Contains(slot.ObjectIndex))).ToArray();
            _listedNativeTextureSlots = _nativeDonorTextureSlots;
            _listedNativeTextureEdits = _nativeDonorTextureEdits;
            _syncingTextureSelection = true;
            try
            {
                NativeDonorTextureList.ItemsSource = items;
                NativeDonorTextureList.SelectedItem = items.FirstOrDefault(item => item.Slot.ObjectIndex == selected);
            }
            finally
            {
                _syncingTextureSelection = false;
            }
            NativeDonorTextureList.Visibility = items.Length == 0 ? Visibility.Collapsed : Visibility.Visible;
            if (selected is not null && NativeDonorTextureList.SelectedItem is null)
                _textureFocus = null;
        }
        UpdateNativeDonorTextureActions();
    }

    private void NativeDonorTexture_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        UpdateNativeDonorTextureActions();
        FocusNativeTexture();
    }

    private void UpdateNativeDonorTextureActions()
    {
        if (ResetNativeDonorTextureButton is null || NativeDonorTextureStatusText is null)
            return;
        var item = NativeDonorTextureList.SelectedItem as NativeDonorTextureItem;
        bool editable = _preparationStep && !WizardBusy && ReplacementIsSmo && item?.Slot.CanReplace == true;
        ResetNativeDonorTextureButton.IsEnabled = editable && item!.Edited;
        string? hint = item switch
        {
            null when _nativeDonorTextureSlots.Count == 0 => "У модели нет доступных текстур.",
            { Slot.CanReplace: false } => "Эта текстура доступна для просмотра.",
            _ => null
        };
        NativeDonorTextureStatusText.Text = hint ?? string.Empty;
        NativeDonorTextureStatusText.Visibility = hint is null ? Visibility.Collapsed : Visibility.Visible;
    }

    private async void ResetNativeDonorTexture_Click(object sender, RoutedEventArgs e)
    {
        if (!WizardBusy && _preparationStep && ReplacementIsSmo &&
            NativeDonorTextureList.SelectedItem is NativeDonorTextureItem { Edited: true } item)
            await ChangeNativeDonorTextureAsync(item.Slot.ObjectIndex, null);
    }

    private Task ChangeNativeDonorTextureAsync(int objectIndex, string? imagePath) =>
        RunTextureEditAsync(async () =>
        {
            var edits = _nativeDonorTextureEdits.Where(edit => edit.ObjectIndex != objectIndex).ToList();
            if (imagePath is not null)
            {
                ImportedTexture image = await Task.Run(() => ImportedTextureFileReader.Read(imagePath));
                edits.Add(new NativeDonorTextureEdit(objectIndex, image));
            }
            if (!_isClosing)
                await RestoreNativeDonorTexturesAsync(edits.ToArray());
        }, "Не удалось заменить текстуру. Проверьте изображение.");

    private async Task RestoreNativeDonorTexturesAsync(NativeDonorTextureEdit[] edits)
    {
        if (_nativeDonorTextureEdits.SequenceEqual(edits))
            return;

        SmoDocument original = _nativeDonorDocument ?? throw new InvalidOperationException("Выберите SMO-донора.");
        SmoDocument? target = _document;
        bool staticPair = IsStaticReplacementPair;
        IReadOnlyList<SmoModelReader.NativePreviewMaterial> originalMaterials = _nativeDonorPreviewMaterials;
        var result = await Task.Run(() =>
        {
            SmoDocument document = original;
            foreach (var edit in edits)
                document = NativeDonorTextureDraft.Apply(document, edit.ObjectIndex, edit.Image);
            var materials = edits.Length == 0 ? null : ReloadNativePreviewMaterials(document, originalMaterials);
            var analysis = target is null ? null : AnalyzeNativeReplacement(target, document, staticPair);
            return (Document: edits.Length == 0 ? null : document, Materials: materials, Analysis: analysis);
        });
        if (_isClosing)
            return;

        _editedNativeDonorDocument = result.Document;
        _editedNativeDonorMaterials = result.Materials;
        _nativeDonorTextureEdits = edits;
        _nativeSmoVisualPlan = result.Analysis?.NativePlan;
        _glbSkinTransferPlan = _nativeSmoVisualPlan is null ? null : ToSkinnedCompatibilityPlan(_nativeSmoVisualPlan);
        ApplyNativeStaticFallback(result.Analysis);
        _plan = null;
        NativeValidationResultBorder.Visibility = Visibility.Collapsed;
        PlanSummaryText.Text = "Текстуры обновлены.";
    }

    private static IReadOnlyList<SmoModelReader.NativePreviewMaterial> ReloadNativePreviewMaterials(
        SmoDocument document, IReadOnlyList<SmoModelReader.NativePreviewMaterial> previous)
    {
        SmoLoadedResources loaded = SmoLoadedResources.Get(document);
        if (loaded.LoadIssue is not null || loaded.SceneIssue is not null)
            throw new InvalidDataException(loaded.LoadIssue ?? loaded.SceneIssue);
        var materials = loaded.Models.Values.Select(model => model.Material).OfType<SmoLoadedMaterial>()
            .DistinctBy(material => material.ObjectIndex).ToDictionary(material => material.ObjectIndex);
        return previous.Select(item =>
        {
            if (item.Material is not { } material)
                return item;
            return new SmoModelReader.NativePreviewMaterial(materials[material.ObjectIndex],
                loaded.PreviewMaterialDraws.GetValueOrDefault(material.ObjectIndex));
        }).ToArray();
    }

}
