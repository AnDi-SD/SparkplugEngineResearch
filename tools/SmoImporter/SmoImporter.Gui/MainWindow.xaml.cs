using System.Globalization;
using System.IO;
using System.Numerics;
using System.ComponentModel;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Media.Media3D;
using Microsoft.Win32;
using SmoExporter.Core;
using SmoImporter.Core;
using SmoViewer.Core;
using Quaternion = System.Numerics.Quaternion;
using WpfEllipse = System.Windows.Shapes.Ellipse;
using WpfLine = System.Windows.Shapes.Line;

namespace SmoImporter.Gui;

public partial class MainWindow : Window
{
    private const double OrbitSensitivity = 0.008;
    private const double DragZoomSensitivity = 0.012;
    private const double MinimumCameraDistance = 0.01;
    private const double MaximumCameraDistance = 1_000_000;
    private const double PitchLimit = Math.PI / 2 - 0.001;

    private readonly ImporterNativeValidator _nativeValidator = new();
    private CancellationTokenSource? _nativeValidationCancellation;
    private bool _nativeValidationRunning;
    private bool _settingNativeExecutablePath;
    private bool _settingPreserveOriginalTextures;
    private bool _settingPortingMode;
    private bool _settingGeneratedSkinningConfirmation;
    private bool _settingManualAdaptWeights;
    private bool _settingRigFittingControls;
    private bool _settingBodyPoseControls;
    private bool _settingGeneratedAttachmentSelection;
    private bool _settingModelTransform;
    private bool _isClosing;

    private string? _sourcePath;
    private SmoDocument? _document;
    private SmoExportScene? _sourceScene;
    private string? _replacementPath;
    private ImportedScene? _baseReplacementScene;
    private ImportedScene? _replacementScene;
    private RigidGlbTextureBundle? _replacementRigidTextureBundle;
    private ImportedTextureCatalogResult? _textureCatalogResult;
    private readonly List<ImportedTexture> _externalTextures = [];
    private readonly HashSet<int> _opaqueOverlaySourceMeshKeys = [];
    private SmoRigidMultiMaterialPackAnalysis? _rigidMultiMaterialAnalysis;
    private MeshSplitPlan? _plan;
    private SmoDocument? _replacementSmoDocument;
    private SmoExportScene? _replacementSmoScene;
    private SmoToSmoReplacementPlan? _smoReplacementPlan;
    private GlbSkinTransferPlan? _glbSkinTransferPlan;
    private SkinnedModelPortingPreparation? _adaptedPortingPreparation;
    private string? _adaptedPortingPreparationIssue;
    private ImportedScene? _generatedSkinningBaseScene;
    private ImportedScene? _generatedSkinningEffectiveScene;
    private ImportedTextureCatalogResult? _generatedSkinningTextureCatalog;
    private GeneratedSkinningPreparationResult? _generatedSkinningPreparation;
    private GeneratedSkinningComponentOverrides? _generatedSkinningComponentOverrides;
    private TargetRigBodySelection? _generatedBodySelection;
    private string? _generatedSkinningPreparationIssue;
    private TargetRigDefinition? _targetRigDefinition;
    private TargetRigFittingPose? _targetRigFittingPose;
    private Vector3[]? _rigLocalEulerDegrees;
    private Vector3 _rigRootEulerDegrees;
    private ReplacementTransform _manualDonorAlignment = ReplacementTransform.Identity;
    private ReplacementTransform? _generatedDonorAlignment;
    private ReplacementTransform? _generatedDonorAlignmentDraft;
    private long _rigFittingRevision;
    private long _adaptedPortingPreparationRevision = -1;
    private long _generatedSkinningPreparationRevision = -1;
    private long _generatedPreparedSceneViewedRevision = -1;
    private bool _rigPoseEditorDirty;
    private bool _bodyPoseEditorDirty;
    private bool _manualAlignmentEditorDirty;
    private bool _generatedAlignmentEditorDirty;
    private BodyPoseControlValues _committedBodyPoseControls;
    private BodyPoseControlValues _draftBodyPoseControls;
    private TargetRigFittingPoseSnapshot? _bodyPoseDraftSnapshot;
    private TargetRigAutomaticPoseFitResult? _bodyPoseAutoFitResult;
    private string? _bodyPoseAutoFitDetails;
    private string? _rigFittingIssue;
    private ModelPortingModeRecommendation? _portingModeRecommendation;
    private string? _blenderPath;
    private string? _multiTextureDirectory;
    private string? _rigidTextureBindingIssue;
    private string? _geometryOnlyFallbackIssue;
    private readonly List<Point3D> _previewBounds = new();
    private readonly Dictionary<GeometryModel3D, int> _generatedAttachmentMeshByModel = new();
    private readonly Dictionary<int, Dictionary<int, int>> _generatedAttachmentComponentByMeshVertex = new();
    private readonly Dictionary<int, Point3D> _generatedAttachmentPreviewCenters = new();
    private readonly HashSet<int> _selectedGeneratedAttachmentComponents = [];
    private readonly List<UIElement> _generatedAttachmentOverlayElements = new();
    private readonly List<UIElement> _rigOverlayElements = new();
    private readonly List<RigJointScreenPoint> _rigJointScreenPoints = new();
    private Vector3[]? _rigOverlayJointPositions;
    private Point3D? _selectedBonePosition;
    private Point _lastMousePosition;
    private Point3D _cameraTarget;
    private double _cameraYaw;
    private double _cameraPitch;
    private double _cameraDistance = 10;
    private CameraNavigationMode _cameraNavigationMode;
    private bool _framePreviewOnRefresh = true;
    private bool _showFinalTexturedPreview;
    private bool _explicitGeneratedReviewRequested;
    private ImportedScene? _finalTexturedPreviewScene;
    private Matrix4x4 _finalTexturedPreviewTransform = Matrix4x4.Identity;

    public MainWindow()
    {
        InitializeComponent();
        if (!string.IsNullOrWhiteSpace(_nativeValidator.SavedExecutablePath))
            SetGameExecutablePath(_nativeValidator.SavedExecutablePath);
        Loaded += MainWindow_Loaded;
        Closing += MainWindow_Closing;
        string? commandLineSource = Environment.GetCommandLineArgs().Skip(1)
            .FirstOrDefault(argument =>
                argument.EndsWith(".smo", StringComparison.OrdinalIgnoreCase) && File.Exists(argument));
        if (commandLineSource is not null)
            LoadSource(commandLineSource);
    }

    private bool PreserveOriginalTextures =>
        PreserveOriginalTexturesCheckBox.IsChecked == true;

    private PortingModeUiChoice SelectedPortingModeChoice =>
        PortingModeCombo?.SelectedIndex switch
        {
            1 => PortingModeUiChoice.PreparedModel,
            2 => PortingModeUiChoice.AdaptSkeleton,
            3 => PortingModeUiChoice.GenerateWeights,
            4 => PortingModeUiChoice.LegacyRigid,
            _ => PortingModeUiChoice.Auto
        };

    private ModelPortingMode? EffectivePortingMode =>
        SelectedPortingModeChoice switch
        {
            PortingModeUiChoice.PreparedModel =>
                ModelPortingMode.PreparedGameSkeleton,
            PortingModeUiChoice.AdaptSkeleton =>
                ModelPortingMode.AdaptDonorWeights,
            PortingModeUiChoice.GenerateWeights =>
                ModelPortingMode.GenerateWeights,
            PortingModeUiChoice.Auto => _portingModeRecommendation?.Mode,
            _ => null
        };

    private bool HasExternalReplacement =>
        _replacementSmoDocument is null && _replacementScene is not null;

    private bool UsesPreparedModelPortingMode =>
        HasExternalReplacement &&
        SelectedPortingModeChoice != PortingModeUiChoice.LegacyRigid &&
        EffectivePortingMode == ModelPortingMode.PreparedGameSkeleton;

    private bool UsesLegacyRigidPortingMode =>
        HasExternalReplacement &&
        SelectedPortingModeChoice == PortingModeUiChoice.LegacyRigid;

    private bool UsesAdaptDonorWeightsPortingMode =>
        HasExternalReplacement &&
        SelectedPortingModeChoice != PortingModeUiChoice.LegacyRigid &&
        EffectivePortingMode == ModelPortingMode.AdaptDonorWeights;

    private bool UsesGeneratedWeightsPortingMode =>
        HasExternalReplacement &&
        SelectedPortingModeChoice != PortingModeUiChoice.LegacyRigid &&
        EffectivePortingMode == ModelPortingMode.GenerateWeights;

    private bool AllReplacementMeshesAreSkinned =>
        _replacementScene is { Meshes.Count: > 0 } &&
        _replacementScene.Meshes.All(mesh => mesh.Skinning is not null);

    private bool AllReplacementMeshesAreUnskinned =>
        _replacementScene is { Meshes.Count: > 0 } &&
        _replacementScene.Meshes.All(mesh => mesh.Skinning is null);

    private bool ReplacementMeshesMixSkinning =>
        _replacementScene is { Meshes.Count: > 0 } &&
        _replacementScene.Meshes.Any(mesh => mesh.Skinning is not null) &&
        _replacementScene.Meshes.Any(mesh => mesh.Skinning is null);

    private bool GeneratedSkinningIsConfirmed =>
        GeneratedSkinningConfirmationCheckBox?.IsChecked == true;

    private bool ManualAdaptWeights =>
        ManualAdaptWeightsCheckBox?.IsChecked == true;

    private bool AdaptedPortingPreparationIsCurrent =>
        _adaptedPortingPreparation is not null &&
        _adaptedPortingPreparationRevision == _rigFittingRevision;

    private bool GeneratedSkinningPreparationIsCurrent =>
        _generatedSkinningPreparation is not null &&
        _generatedSkinningPreparationRevision == _rigFittingRevision;

    private bool GeneratedPreparedSceneViewedForCurrentRevision =>
        GeneratedSkinningPreparationIsCurrent &&
        _generatedPreparedSceneViewedRevision == _rigFittingRevision;

    private bool GeneratedSkinningIsReady =>
        GeneratedSkinningPreparationIsCurrent &&
        GeneratedPreparedSceneViewedForCurrentRevision &&
        GeneratedSkinningIsConfirmed;

    private bool CanSelectGeneratedAttachments =>
        UsesGeneratedWeightsPortingMode &&
        !IsJointPoseEditorMode &&
        GeneratedSkinningPreparationIsCurrent &&
        !_nativeValidationRunning;

    private bool CanShowFinalTexturedPreview =>
        !_nativeValidationRunning &&
        !PreserveOriginalTextures &&
        !UseRigidMultiTextureMode &&
        !RigFittingEditorHasPendingChanges &&
        _document is not null &&
        _replacementScene is { Textures.Count: > 0 } &&
        (UsesLegacyRigidPortingMode ||
         (UsesPreparedModelPortingMode && _glbSkinTransferPlan?.CanReplace == true) ||
         (UsesAdaptDonorWeightsPortingMode && AdaptedPortingPreparationIsCurrent &&
          _glbSkinTransferPlan?.CanReplace == true) ||
         (UsesGeneratedWeightsPortingMode && GeneratedSkinningPreparationIsCurrent &&
          _glbSkinTransferPlan?.CanReplace == true));

    private bool RigFittingEditorHasPendingChanges =>
        (UsesGeneratedWeightsPortingMode ||
         (UsesAdaptDonorWeightsPortingMode && ManualAdaptWeights)) &&
        (_rigPoseEditorDirty || _bodyPoseEditorDirty ||
         (UsesGeneratedWeightsPortingMode &&
          _generatedAlignmentEditorDirty) ||
         (UsesAdaptDonorWeightsPortingMode && ManualAdaptWeights &&
          _manualAlignmentEditorDirty));

    private bool CanRunSelectedPortingPipeline =>
        UsesLegacyRigidPortingMode ||
        ((UsesPreparedModelPortingMode || UsesAdaptDonorWeightsPortingMode) &&
         AllReplacementMeshesAreSkinned) ||
        (UsesGeneratedWeightsPortingMode &&
         _baseReplacementScene is { Meshes.Count: > 0 } &&
         !string.IsNullOrWhiteSpace(_replacementPath) &&
         GeneratedSkinningPreparationIsCurrent);

    private bool UseRigidMultiTextureMode =>
        _replacementSmoDocument is null &&
        _replacementRigidTextureBundle is not null &&
        UsesLegacyRigidPortingMode &&
        !PreserveOriginalTextures;

    private SkinnedTextureTransferMode SelectedSkinnedTextureTransferMode =>
        PreserveOriginalTextures
            ? SkinnedTextureTransferMode.PreserveTarget
            : SkinnedTextureTransferMode.ImportDonor;

    private ImportedScene? SkinnedMaterialOverrideCatalogScene
    {
        get
        {
            if (_generatedSkinningEffectiveScene is not null)
                return _generatedSkinningEffectiveScene;
            if (_baseReplacementScene is not { } baseScene ||
                baseScene.Meshes.Any(mesh => mesh.Skinning is not null))
            {
                return null;
            }
            return _textureCatalogResult?.EffectiveScene ?? baseScene;
        }
    }

    private bool CanEditSkinnedMaterialOverrides =>
        !_nativeValidationRunning &&
        UsesGeneratedWeightsPortingMode &&
        SelectedSkinnedTextureTransferMode ==
            SkinnedTextureTransferMode.ImportDonor &&
        SkinnedMaterialOverrideCatalogScene is { Meshes.Count: > 0 };

    private void SetPreserveOriginalTextures(bool value)
    {
        _settingPreserveOriginalTextures = true;
        try
        {
            PreserveOriginalTexturesCheckBox.IsChecked = value;
        }
        finally
        {
            _settingPreserveOriginalTextures = false;
        }
    }

    private void SetPortingModeChoice(PortingModeUiChoice choice)
    {
        _settingPortingMode = true;
        try
        {
            PortingModeCombo.SelectedIndex = (int)choice;
        }
        finally
        {
            _settingPortingMode = false;
        }
    }

    private void SetGeneratedSkinningConfirmation(bool value)
    {
        _settingGeneratedSkinningConfirmation = true;
        try
        {
            GeneratedSkinningConfirmationCheckBox.IsChecked = value;
        }
        finally
        {
            _settingGeneratedSkinningConfirmation = false;
        }
    }

    private void UpdateGeneratedSkinningConfirmationAvailability()
    {
        bool canConfirm = UsesGeneratedWeightsPortingMode &&
            GeneratedSkinningPreparationIsCurrent &&
            GeneratedPreparedSceneViewedForCurrentRevision &&
            _generatedSkinningPreparation?.Analysis.RequiresConfirmation == true &&
            !RigFittingEditorHasPendingChanges &&
            !_nativeValidationRunning;
        GeneratedSkinningConfirmationCheckBox.IsEnabled = canConfirm;
        GeneratedSkinningConfirmationHintText.Text =
            !GeneratedSkinningPreparationIsCurrent
                ? "Сначала должна успешно завершиться подготовка режима 3."
                : RigFittingEditorHasPendingChanges
                    ? "Сначала примените введённые значения размера или подгонки."
                : !GeneratedPreparedSceneViewedForCurrentRevision
                    ? "Нажмите «Показать итог и разрешить подтверждение»: окно просмотра само переключится на точный PreparedScene."
                    : GeneratedSkinningIsConfirmed
                        ? "Итоговая модель текущей ревизии проверена и подтверждена."
                        : "Итоговая модель текущей ревизии показана. Проверьте её и поставьте галочку.";
    }

    private string? GetGeneratedSkinningTransferBlocker()
    {
        if (!UsesGeneratedWeightsPortingMode ||
            !GeneratedSkinningPreparationIsCurrent ||
            _glbSkinTransferPlan?.CanReplace != false)
        {
            return null;
        }

        return _glbSkinTransferPlan.Messages
            .FirstOrDefault(message => !string.IsNullOrWhiteSpace(message)) ??
            "Подготовленная модель несовместима со структурой целевого SMO.";
    }

    private static bool IsMaterialGroupTransferBlocker(string message) =>
        message.Contains("material groups", StringComparison.OrdinalIgnoreCase) ||
        message.Contains("texture groups", StringComparison.OrdinalIgnoreCase);

    private static bool IsOpaqueAlphaSplitBlocker(string message) =>
        message.Contains("mix fully opaque and transparent", StringComparison.OrdinalIgnoreCase) ||
        message.Contains("mixes source texture groups with transparency", StringComparison.OrdinalIgnoreCase) ||
        message.Contains("separate opaque and alpha", StringComparison.OrdinalIgnoreCase) ||
        message.Contains("conventional alpha", StringComparison.OrdinalIgnoreCase);

    private static string PresentGeneratedSkinningTransferBlocker(string message)
    {
        string result = message
            .Replace("GLB material groups", "Группы материалов донора")
            .Replace("target texture groups", "группы текстур шаблона");
        if (IsOpaqueAlphaSplitBlocker(message))
        {
            result += " Непрозрачную геометрию и геометрию с alpha нельзя " +
                      "объединять в один material/spSkin: подготовьте для них " +
                      "отдельные native-ветви.";
        }
        else if (IsMaterialGroupTransferBlocker(message))
        {
            result += " Уменьшите число материалов донора либо включите " +
                      "«Оставить текстуры исходного SMO» ниже.";
        }
        return result;
    }

    private void UpdateGeneratedSkinningPrimaryStatus()
    {
        if (GeneratedSkinningPrimaryStatusText is null)
            return;

        string text;
        Color color;
        string? transferBlocker = GetGeneratedSkinningTransferBlocker();
        if (!UsesGeneratedWeightsPortingMode)
        {
            text = string.Empty;
            color = Color.FromRgb(82, 96, 109);
        }
        else if (RigFittingEditorHasPendingChanges)
        {
            text = "Есть неприменённые изменения. Примените их, прежде чем проверять итоговую модель.";
            color = Color.FromRgb(180, 83, 9);
        }
        else if (!GeneratedSkinningPreparationIsCurrent)
        {
            text = _generatedSkinningPreparationIssue is null
                ? "Подготовка автоматических весов ещё не завершена."
                : "Подготовка заблокирована: " + _generatedSkinningPreparationIssue;
            color = Color.FromRgb(185, 28, 28);
        }
        else if (transferBlocker is not null)
        {
            text = "Создание SMO пока заблокировано: " +
                   PresentGeneratedSkinningTransferBlocker(transferBlocker);
            color = Color.FromRgb(185, 28, 28);
        }
        else if (!GeneratedPreparedSceneViewedForCurrentRevision)
        {
            text = "Веса готовы. Следующий шаг — показать точную итоговую модель кнопкой ниже.";
            color = Color.FromRgb(180, 83, 9);
        }
        else if (!GeneratedSkinningIsConfirmed)
        {
            text = "Итоговая модель показана. Проверьте её и подтвердите результат.";
            color = Color.FromRgb(180, 83, 9);
        }
        else if (_plan is null)
        {
            text = "Результат подтверждён. Теперь можно построить palettes и проверить план.";
            color = Color.FromRgb(3, 105, 161);
        }
        else
        {
            text = "Подготовка завершена: план проверен, можно создавать SMO.";
            color = Color.FromRgb(22, 101, 52);
        }

        GeneratedSkinningPrimaryStatusText.Text = text;
        GeneratedSkinningPrimaryStatusText.Foreground = new SolidColorBrush(color);
    }

    private void SetManualAdaptWeights(bool value)
    {
        _settingManualAdaptWeights = true;
        try
        {
            ManualAdaptWeightsCheckBox.IsChecked = value;
        }
        finally
        {
            _settingManualAdaptWeights = false;
        }
    }

    private void InvalidateAdaptedPortingPreparation()
    {
        ClearFinalTexturedPreview();
        _adaptedPortingPreparation = null;
        _adaptedPortingPreparationIssue = null;
        _adaptedPortingPreparationRevision = -1;
    }

    private void InvalidateGeneratedSkinningPreparation(
        bool clearGeometryBase = false)
    {
        ClearFinalTexturedPreview();
        _generatedSkinningPreparation = null;
        _generatedSkinningPreparationIssue = null;
        _generatedSkinningPreparationRevision = -1;
        _generatedPreparedSceneViewedRevision = -1;
        _generatedSkinningEffectiveScene = null;
        _generatedSkinningTextureCatalog = null;
        if (clearGeometryBase)
            _generatedSkinningBaseScene = null;
        SetGeneratedSkinningConfirmation(false);
        GeneratedSkinningConfirmationCheckBox.IsEnabled = false;
    }

    private void ResetSkinnedMaterialOverrides()
    {
        _opaqueOverlaySourceMeshKeys.Clear();
    }

    private SkinnedRenderableMaterialProfile ResolveSkinnedMaterialProfile(
        ImportedScene donor,
        SkinnedTextureTransferMode textureMode)
    {
        if (!UsesGeneratedWeightsPortingMode ||
            textureMode != SkinnedTextureTransferMode.ImportDonor ||
            _opaqueOverlaySourceMeshKeys.Count == 0)
        {
            return SkinnedRenderableMaterialProfile.Default;
        }

        ValidateMaterialMeshKeys(donor, _opaqueOverlaySourceMeshKeys);
        return CreateSkinnedMaterialProfile(
            donor,
            _opaqueOverlaySourceMeshKeys);
    }

    private void ValidateMaterialMeshKeys(
        ImportedScene donor,
        IEnumerable<int> sourceMeshKeys)
    {
        ImportedScene? catalog = SkinnedMaterialOverrideCatalogScene;
        int[] keys = sourceMeshKeys.Distinct().Order().ToArray();
        foreach (int sourceMeshKey in keys)
        {
            if ((uint)sourceMeshKey >= (uint)donor.Meshes.Count)
            {
                throw new InvalidOperationException(
                    $"Сохранённый индекс меша {sourceMeshKey} отсутствует в текущем доноре.");
            }
        }

        if (catalog is null || ReferenceEquals(catalog, donor))
            return;
        if (catalog.Meshes.Count != donor.Meshes.Count)
        {
            throw new InvalidOperationException(
                "PreparedScene изменила количество мешей; сохранённые режимы " +
                "материалов нельзя переносить по индексам.");
        }

        foreach (int sourceMeshKey in keys)
        {
            if ((uint)sourceMeshKey >= (uint)catalog.Meshes.Count)
            {
                throw new InvalidOperationException(
                    $"Сохранённый индекс меша {sourceMeshKey} отсутствует в текущем доноре.");
            }

            ImportedMesh source = catalog.Meshes[sourceMeshKey];
            ImportedMesh prepared = donor.Meshes[sourceMeshKey];
            bool sameMaterial = source.MaterialIndex == prepared.MaterialIndex &&
                (source.MaterialIndex < 0 ||
                 ((uint)source.MaterialIndex < (uint)catalog.Materials.Count &&
                  (uint)source.MaterialIndex < (uint)donor.Materials.Count &&
                  catalog.Materials[source.MaterialIndex] ==
                      donor.Materials[source.MaterialIndex]));
            bool sameIdentity =
                string.Equals(source.Name, prepared.Name, StringComparison.Ordinal) &&
                sameMaterial &&
                source.Positions.Length == prepared.Positions.Length &&
                source.TriangleIndices.AsSpan().SequenceEqual(
                    prepared.TriangleIndices) &&
                source.TextureCoordinates.AsSpan().SequenceEqual(
                    prepared.TextureCoordinates);
            if (!sameIdentity)
            {
                throw new InvalidOperationException(
                    $"PreparedScene не сохранила identity меша [{sourceMeshKey}] " +
                    $"\"{source.Name}\". Выберите режим материала заново.");
            }
        }
    }

    private static SkinnedRenderableMaterialProfile CreateSkinnedMaterialProfile(
        ImportedScene donor,
        IEnumerable<int> opaqueOverlaySourceMeshKeys) =>
        new(
            donor,
            opaqueOverlaySourceMeshKeys
                .Distinct()
                .Order()
                .Select(sourceMeshKey =>
                    new SkinnedRenderableMaterialOverride(
                        sourceMeshKey,
                        SkinnedRenderableMaterialMode.OpaqueOverlay)));

    private void ClearFinalTexturedPreview()
    {
        _explicitGeneratedReviewRequested = false;
        _showFinalTexturedPreview = false;
        _finalTexturedPreviewScene = null;
        _finalTexturedPreviewTransform = Matrix4x4.Identity;
    }

    private void ResetRigFittingState(bool resetManualMode = true)
    {
        _generatedSkinningComponentOverrides = null;
        _generatedBodySelection = null;
        ClearFinalTexturedPreview();
        _selectedGeneratedAttachmentComponents.Clear();
        _generatedAttachmentComponentByMeshVertex.Clear();
        _generatedAttachmentMeshByModel.Clear();
        _generatedAttachmentPreviewCenters.Clear();
        ClearGeneratedAttachmentScreenOverlay();
        if (GeneratedAttachmentList is not null)
        {
            _settingGeneratedAttachmentSelection = true;
            try
            {
                GeneratedAttachmentList.ItemsSource = null;
            }
            finally
            {
                _settingGeneratedAttachmentSelection = false;
            }
            GeneratedAttachmentSummaryText.Text =
                "Отдельные детали ещё не рассчитаны.";
            GeneratedAttachmentStatusText.Text = string.Empty;
        }
        _targetRigDefinition = null;
        _targetRigFittingPose = null;
        _rigLocalEulerDegrees = null;
        _rigRootEulerDegrees = Vector3.Zero;
        _manualDonorAlignment = ReplacementTransform.Identity;
        _generatedDonorAlignment = null;
        _generatedDonorAlignmentDraft = null;
        _rigFittingIssue = null;
        _rigPoseEditorDirty = false;
        _bodyPoseEditorDirty = false;
        _manualAlignmentEditorDirty = false;
        _generatedAlignmentEditorDirty = false;
        _committedBodyPoseControls = default;
        _draftBodyPoseControls = default;
        _bodyPoseDraftSnapshot = null;
        _bodyPoseAutoFitResult = null;
        _bodyPoseAutoFitDetails = null;
        _rigFittingRevision++;
        if (resetManualMode)
        {
            SetManualAdaptWeights(false);
            _settingModelTransform = true;
            try
            {
                ScaleBox.Text = "1";
                RotXBox.Text = RotYBox.Text = RotZBox.Text = "0";
                MoveXBox.Text = MoveYBox.Text = MoveZBox.Text = "0";
            }
            finally
            {
                _settingModelTransform = false;
            }
        }

        if (RigFittingJointCombo is not null)
        {
            _settingRigFittingControls = true;
            try
            {
                RigFittingJointCombo.ItemsSource = null;
                RigLocalRotXSlider.Value = 0;
                RigLocalRotYSlider.Value = 0;
                RigLocalRotZSlider.Value = 0;
                RigRootRotXBox.Text = RigRootRotYBox.Text = RigRootRotZBox.Text = "0";
                RigRootMoveXBox.Text = RigRootMoveYBox.Text = RigRootMoveZBox.Text = "0";
                RigFittingStatusText.Text = "Начальная поза — каноническая bind pose игрового скелета.";
            }
            finally
            {
                _settingRigFittingControls = false;
            }
        }

        if (BodyArmRaiseSlider is not null)
        {
            WriteBodyPoseControls(default);
            BodyPoseStatusText.ToolTip = null;
        }
    }

    private void UpdatePortingModeRecommendation()
    {
        if (!HasExternalReplacement)
        {
            _portingModeRecommendation = null;
            return;
        }

        if (_document is not null)
        {
            _portingModeRecommendation = ModelPortingModeAnalyzer.Recommend(
                _document,
                _replacementScene!);
            return;
        }

        bool hasSkinning = _replacementScene!.HasSkinning;
        _portingModeRecommendation = new ModelPortingModeRecommendation(
            hasSkinning
                ? ModelPortingMode.AdaptDonorWeights
                : ModelPortingMode.GenerateWeights,
            hasSkinning
                ? "Select the target SMO to verify the donor skeleton."
                : "The donor has no usable skin weights.");
    }

    private void UpdatePortingModePresentation()
    {
        if (PortingModePanel is null)
            return;

        PortingModePanel.Visibility = HasExternalReplacement
            ? Visibility.Visible
            : Visibility.Collapsed;
        if (!HasExternalReplacement)
            return;

        PortingModeCombo.IsEnabled = !_nativeValidationRunning;
        ModelPortingMode recommended = _portingModeRecommendation?.Mode ??
            (_replacementScene!.HasSkinning
                ? ModelPortingMode.AdaptDonorWeights
                : ModelPortingMode.GenerateWeights);
        PortingModeRecommendationText.Text =
            $"Рекомендация: {GetPortingModeDisplayName(recommended)}. " +
            GetPortingModeRecommendationReason(recommended);

        PortingModeDescriptionText.Text = SelectedPortingModeChoice switch
        {
            PortingModeUiChoice.Auto =>
                $"Автовыбор использует рекомендацию «{GetPortingModeDisplayName(recommended)}». " +
                GetPortingModeAvailabilityText(recommended),
            PortingModeUiChoice.PreparedModel =>
                "Модель считается подготовленной: её кости строго сопоставляются с игровыми, " +
                "а её bind pose можно согласовать с исходным SMO переключателем ниже.",
            PortingModeUiChoice.AdaptSkeleton =>
                "Используются веса модели, а активные кости строго сопоставляются с игровыми " +
                "по имени или проверенному humanoid-алиасу. Неизвестные кости не угадываются.",
            PortingModeUiChoice.GenerateWeights =>
                "Сначала модель масштабируется и центрируется; автоматический результат " +
                "можно поправить вручную. Только после применения положения веса " +
                "рассчитываются по игровому скелету. Модель должна быть вертикальной, " +
                "Y-up, не зеркальной и смотреть в ту же сторону, что оригинал.",
            PortingModeUiChoice.LegacyRigid =>
                "Прежний режим для статических объектов: вся геометрия привязывается к одной " +
                "выбранной palette bone. Автоматические веса не создаются.",
            _ => string.Empty
        };
        if (!string.IsNullOrWhiteSpace(_geometryOnlyFallbackIssue))
        {
            PortingModeDescriptionText.Text +=
                "\nRig донора не удалось прочитать, поэтому его кости и веса " +
                "игнорируются, а модель загружена только как геометрия. " +
                "Автовыбор рекомендует режим 3. Причина: " +
                _geometryOnlyFallbackIssue;
        }

        string? blocked = GetPortingModeBlockMessage();
        if (blocked is not null)
        {
            PlanSummaryText.Text = blocked;
            ReplacementModeText.Text = EffectivePortingMode is ModelPortingMode mode
                ? $"{GetPortingModeDisplayName(mode)} — недоступен"
                : "Выбранный режим недоступен";
            CompatibilityText.Text =
                "Модель и её текстуры загружены, но геометрия не передаётся в старый " +
                "rigid-путь автоматически. " + blocked;
            SplitModeText.Text = blocked;
            ReplacementModePanel.Background = new SolidColorBrush(
                Color.FromRgb(255, 243, 205));
        }
        else if (UsesLegacyRigidPortingMode &&
                 _replacementRigidTextureBundle is null)
        {
            ReplacementModeText.Text = "Дополнительный режим rigid-объекта";
            CompatibilityText.Text =
                "Вся геометрия будет привязана к одной выбранной palette bone. " +
                "Skin weights модели не используются и не создаются.";
            SplitModeText.Text =
                "Вся модель помещается в один основной body-slot; остальные slots " +
                "получают невидимый вырожденный triangle.";
            ReplacementModePanel.Background = new SolidColorBrush(
                Color.FromRgb(255, 243, 205));
            BoneMappingTree.Items.Clear();
            BoneMappingPanel.Visibility = Visibility.Collapsed;
        }
    }

    private static string GetPortingModeDisplayName(ModelPortingMode mode) => mode switch
    {
        ModelPortingMode.PreparedGameSkeleton => "1. Подготовленная модель",
        ModelPortingMode.AdaptDonorWeights => "2. Адаптировать существующий скелет",
        ModelPortingMode.GenerateWeights => "3. Создать веса с нуля",
        _ => mode.ToString()
    };

    private string GetPortingModeRecommendationReason(ModelPortingMode mode)
    {
        if (_document is null)
            return "После выбора исходного SMO рекомендация будет уточнена.";
        return mode switch
        {
            ModelPortingMode.PreparedGameSkeleton =>
                "Активные кости имеют проверенное сопоставление с игровым скелетом.",
            ModelPortingMode.AdaptDonorWeights =>
                "Веса присутствуют, но строгая привязка к игровому скелету не прошла.",
            ModelPortingMode.GenerateWeights =>
                "У модели нет пригодных skin weights.",
            _ => string.Empty
        };
    }

    private static string GetPortingModeAvailabilityText(ModelPortingMode mode) => mode switch
    {
        ModelPortingMode.PreparedGameSkeleton =>
            "Строгий подготовленный импорт доступен.",
        ModelPortingMode.AdaptDonorWeights =>
            "Доступна строгая адаптация donor weights; результат будет показан до сохранения.",
        ModelPortingMode.GenerateWeights =>
            "Автоматические веса и alignment будут сразу показаны; перед планом нужно подтверждение.",
        _ => string.Empty
    };

    private string? GetPortingModeBlockMessage()
    {
        if (ReplacementMeshesMixSkinning &&
            (UsesPreparedModelPortingMode ||
             UsesAdaptDonorWeightsPortingMode))
        {
            return "Модель смешивает skinned и unskinned meshes. Автоматически удалять, " +
                   "перепривязывать или игнорировать часть геометрии небезопасно. " +
                   "Подготовьте все meshes одним способом либо импортируйте их отдельно.";
        }
        if (UsesPreparedModelPortingMode && _replacementScene?.HasSkinning != true)
        {
            return "Подготовленный режим требует модель с пригодными skin weights. " +
                   "Выберите режим 3 или дополнительный rigid-режим.";
        }
        if (UsesAdaptDonorWeightsPortingMode &&
            _replacementScene?.HasSkinning != true)
        {
            return "Адаптация требует исходные skin weights. " +
                   "Для модели без весов выберите режим 3 или rigid-режим.";
        }
        if (UsesGeneratedWeightsPortingMode &&
            _baseReplacementScene is not { Meshes.Count: > 0 })
        {
            return "Создание весов требует непустую модель-донор.";
        }
        if (UsesGeneratedWeightsPortingMode &&
            string.IsNullOrWhiteSpace(_replacementPath))
            return "Для создания весов не сохранён путь к модели-донору.";
        if (UsesGeneratedWeightsPortingMode &&
            !GeneratedSkinningPreparationIsCurrent &&
            !string.IsNullOrWhiteSpace(_generatedSkinningPreparationIssue))
            return _generatedSkinningPreparationIssue;
        return null;
    }

    private void PortingMode_Changed(object sender, SelectionChangedEventArgs e)
    {
        if (_settingPortingMode || !IsLoaded)
            return;

        if (_rigPoseEditorDirty)
        {
            _rigPoseEditorDirty = false;
            LoadRigFittingEditorValues();
        }
        if (_bodyPoseEditorDirty)
        {
            _bodyPoseEditorDirty = false;
            _bodyPoseDraftSnapshot = null;
            _draftBodyPoseControls = _committedBodyPoseControls;
            WriteBodyPoseControls(_committedBodyPoseControls);
            BodyPoseStatusText.Text =
                "Неприменённая групповая поза отброшена при смене режима.";
        }
        if (!(UsesAdaptDonorWeightsPortingMode && ManualAdaptWeights) &&
            _manualAlignmentEditorDirty)
        {
            _manualAlignmentEditorDirty = false;
            WriteTransformEditor(_manualDonorAlignment);
        }
        if (UsesGeneratedWeightsPortingMode &&
            _generatedAlignmentEditorDirty &&
            _generatedDonorAlignmentDraft is null &&
            _generatedDonorAlignment is not null)
        {
            // Invalid text cannot be restored after another mode owned the shared
            // editor. Return to the last committed transaction instead of keeping
            // a hidden dirty state that would require a no-op Apply.
            _generatedAlignmentEditorDirty = false;
        }
        ReplacementTransform? generatedAlignment = _generatedAlignmentEditorDirty
            ? _generatedDonorAlignmentDraft ?? _generatedDonorAlignment
            : _generatedDonorAlignment;
        if (UsesGeneratedWeightsPortingMode && generatedAlignment is not null)
        {
            WriteTransformEditor(generatedAlignment);
        }
        else if (UsesGeneratedWeightsPortingMode)
        {
            WriteTransformEditor(ReplacementTransform.Identity);
        }
        else if (UsesAdaptDonorWeightsPortingMode && ManualAdaptWeights)
        {
            WriteTransformEditor(_manualDonorAlignment);
        }

        _plan = null;
        _rigidMultiMaterialAnalysis = null;
        InvalidateAdaptedPortingPreparation();
        InvalidateGeneratedSkinningPreparation();
        PlanSummaryText.Text = "План ещё не построен.";

        if (UsesPreparedModelPortingMode &&
            _document is not null && _replacementScene?.HasSkinning == true)
        {
            UpdateGlbSkinTransferPlan();
        }
        else if (UsesAdaptDonorWeightsPortingMode)
        {
            UpdateAdaptedPortingPreparation();
        }
        else if (UsesGeneratedWeightsPortingMode)
        {
            UpdateGeneratedSkinningPreparation();
        }
        else if (UsesLegacyRigidPortingMode &&
                 _replacementRigidTextureBundle is not null)
        {
            UpdateRigidTextureModeDescription();
        }

        RefreshTextureList();
        RefreshState();
        string? blocked = GetPortingModeBlockMessage();
        if (blocked is not null)
            StatusText.Text = blocked;
        else if (!UsesAdaptDonorWeightsPortingMode &&
                 !UsesGeneratedWeightsPortingMode)
        {
            StatusText.Text = UsesLegacyRigidPortingMode
                ? "Выбран дополнительный rigid-режим. Постройте план заново."
                : "Режим портирования изменён. Постройте план заново.";
        }
    }

    private void SelectSource_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new OpenFileDialog { Filter = "Sparkplug model (*.smo)|*.smo", CheckFileExists = true };
        if (dialog.ShowDialog(this) != true) return;
        LoadSource(dialog.FileName);
    }

    private void LoadSource(string path)
    {
        try
        {
            string sourcePath = Path.GetFullPath(path);
            SmoDocument document = SmoDocument.Load(sourcePath);
            SmoExportScene sourceScene = SmoSceneBuilder.Build(document);
            BoneItem[] boneItems = SmoWholeModelReplacer.GetRigidBoneChoices(document)
                .Select(bone => new BoneItem(
                    bone.Slot, bone.ObjectId, $"[{bone.Slot}] {bone.Name}"))
                .ToArray();
            BoneItem? preferredHead = _replacementRigidTextureBundle is null
                ? null
                : FindPreferredHeadBone(boneItems);

            _sourcePath = sourcePath;
            _document = document;
            _sourceScene = sourceScene;
            _plan = null;
            _rigidMultiMaterialAnalysis = null;
            ResetRigFittingState();
            InvalidateAdaptedPortingPreparation();
            InvalidateGeneratedSkinningPreparation();
            SourcePathText.Text = _sourcePath;
            SourceSummaryText.Text = $"{_sourceScene.Meshes.Count} mesh slots; {_sourceScene.Meshes.Sum(mesh => mesh.Positions.Length):N0} vertices; {_sourceScene.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3):N0} triangles.";
            BoneCombo.ItemsSource = boneItems;
            BoneCombo.SelectedIndex = BoneCombo.Items.Count > 0 ? 0 : -1;
            if (preferredHead is not null)
                BoneCombo.SelectedItem = preferredHead;
            UpdatePortingModeRecommendation();
            if (_replacementSmoDocument is not null)
                UpdateSmoReplacementPlan();
            else if (UsesPreparedModelPortingMode &&
                     _replacementScene?.HasSkinning == true)
                UpdateGlbSkinTransferPlan();
            else if (UsesAdaptDonorWeightsPortingMode &&
                     _replacementScene?.HasSkinning == true)
                UpdateAdaptedPortingPreparation();
            else if (UsesGeneratedWeightsPortingMode)
                UpdateGeneratedSkinningPreparation();
            else if (UseRigidMultiTextureMode)
            {
                ApplyAutoFit();
                PlanSummaryText.Text = "Проверка multi-texture структуры ещё не выполнена.";
                StatusText.Text = "Целевой SMO изменён. Повторите проверку multi-texture структуры.";
            }
            else if (UsesLegacyRigidPortingMode &&
                     _replacementRigidTextureBundle is not null &&
                     PreserveOriginalTextures)
            {
                ApplyAutoFit();
                PlanSummaryText.Text = "Диагностический план с текстурами исходного SMO ещё не построен.";
                StatusText.Text = "Целевой SMO изменён. Повторите проверку диагностического плана.";
            }
            else
                StatusText.Text = "Шаблон SMO загружен.";
            _framePreviewOnRefresh = true;
            RefreshState();
            if (GetPortingModeBlockMessage() is string blocked)
                StatusText.Text = blocked;
            if (IsLoaded &&
                (ResolveGameExecutablePath() is null ||
                 string.IsNullOrWhiteSpace(
                     _nativeValidator.SavedExecutablePath)))
                _ = LocateGameExecutableAsync();
        }
        catch (Exception exception) { ShowError(exception); }
    }

    private void SelectReplacement_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new OpenFileDialog
        {
            Filter = "Модель замены (*.smo;*.fbx;*.glb;*.obj)|*.smo;*.fbx;*.glb;*.obj|SMO (*.smo)|*.smo|FBX (*.fbx)|*.fbx|GLB (*.glb)|*.glb|OBJ (*.obj)|*.obj",
            CheckFileExists = true
        };
        if (dialog.ShowDialog(this) != true) return;
        try
        {
            if (Path.GetExtension(dialog.FileName).Equals(
                    ".smo", StringComparison.OrdinalIgnoreCase))
                LoadSmoReplacement(dialog.FileName);
            else
            {
                if (Path.GetExtension(dialog.FileName).Equals(
                        ".fbx", StringComparison.OrdinalIgnoreCase) &&
                    !EnsureBlenderForFbx())
                    return;
                LoadExternalReplacement(dialog.FileName);
            }
            _framePreviewOnRefresh = true;
            RefreshState();
        }
        catch (Exception exception) { ShowError(exception); }
    }

    private void LoadSmoReplacement(string path)
    {
        string fullPath = Path.GetFullPath(path);
        SmoDocument donor = SmoDocument.Load(fullPath);
        SmoExportScene donorScene = SmoSceneBuilder.Build(donor);
        SetPreserveOriginalTextures(false);
        SetPortingModeChoice(PortingModeUiChoice.Auto);
        _portingModeRecommendation = null;

        _replacementPath = fullPath;
        _baseReplacementScene = null;
        _replacementSmoDocument = donor;
        _replacementSmoScene = donorScene;
        _replacementScene = null;
        _replacementRigidTextureBundle = null;
        _textureCatalogResult = null;
        _externalTextures.Clear();
        ResetSkinnedMaterialOverrides();
        _multiTextureDirectory = null;
        _rigidTextureBindingIssue = null;
        _geometryOnlyFallbackIssue = null;
        _rigidMultiMaterialAnalysis = null;
        ResetRigFittingState();
        InvalidateAdaptedPortingPreparation();
        InvalidateGeneratedSkinningPreparation(clearGeometryBase: true);
        _plan = null;
        ReplacementPathText.Text = fullPath;
        int textures = donor.Objects.Count(entry =>
            entry.TypeHash == SmoClassIds.TextureData);
        ReplacementSummaryText.Text =
            $"{donorScene.Meshes.Count} готовых mesh slots; " +
            $"{donorScene.Meshes.Sum(mesh => mesh.Positions.Length):N0} vertices; " +
            $"{donorScene.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3):N0} triangles; " +
            $"{textures} textures.";
        PlanSummaryText.Text =
            "Service/skeleton graph target сохраняется. Donor meshes, materials и " +
            "textures добавляются отдельными visual branches со своими palettes; " +
            "старые target meshes становятся невидимыми anchors.";
        ReplacementModeText.Text = "Режим SMO → SMO";
        RefreshTextureList();
        UpdateSmoReplacementPlan();
    }

    private void LoadExternalReplacement(string path)
    {
        string fullPath = Path.GetFullPath(path);
        string extension = Path.GetExtension(fullPath);
        bool canReadGeometryOnly =
            extension.Equals(".glb", StringComparison.OrdinalIgnoreCase) ||
            extension.Equals(".fbx", StringComparison.OrdinalIgnoreCase);
        ImportedScene sourceScene;
        string? geometryOnlyFallbackIssue = null;
        try
        {
            sourceScene = ImportedModelReader.Read(fullPath, _blenderPath);
        }
        catch (InvalidDataException exception) when (canReadGeometryOnly)
        {
            sourceScene = ImportedModelReader.ReadGeometryOnly(
                fullPath,
                _blenderPath);
            geometryOnlyFallbackIssue = exception.Message;
        }
        ImportedTextureCatalogResult catalog =
            ImportedTextureCatalog.ResolveExternalOverrides(sourceScene, []);
        RigidGlbTextureBundle? rigidTextureBundle = null;
        string? textureDirectory = null;
        string? textureBindingIssue = null;
        ImportedScene effectiveScene = catalog.EffectiveScene;
        bool sourceIsFbx = Path.GetExtension(fullPath).Equals(
            ".fbx", StringComparison.OrdinalIgnoreCase);
        if (rigidTextureBundle is null && (!sourceScene.HasSkinning || sourceIsFbx) &&
            RigidGlbTextureBundleReader.HasCandidateTextureFiles(fullPath))
        {
            try
            {
                rigidTextureBundle = sourceIsFbx
                        ? RigidGlbTextureBundleReader.ReadModel(
                            fullPath, blenderPath: _blenderPath)
                        : RigidGlbTextureBundleReader.Bind(fullPath, sourceScene);
                effectiveScene = rigidTextureBundle.Scene;
                textureDirectory = rigidTextureBundle.TextureDirectory;
            }
            catch (Exception exception) when (
                exception is InvalidDataException or
                RigidTextureBundleContainsSkinnedMeshesException)
            {
                // A stray matN file next to an otherwise valid model must not hijack
                // ordinary loading. Explicit "Папка…" keeps the strict diagnostics.
            }
        }
        if (rigidTextureBundle is null)
        {
            try
            {
                RigidGlbTextureBundleReader.TryBindSceneTextures(
                    fullPath,
                    catalog.EffectiveScene,
                    out rigidTextureBundle);
            }
            catch (InvalidDataException exception)
            {
                // Keep the donor available for the diagnostic mode that preserves
                // every target TextureData. Normal texture import remains blocked
                // later by the stored binding issue.
                textureBindingIssue = exception.Message;
            }
        }

        _externalTextures.Clear();
        ApplyExternalReplacementState(
            fullPath,
            sourceScene,
            effectiveScene,
            rigidTextureBundle,
            catalog,
            textureDirectory,
            resetTransform: true,
            textureBindingIssue: textureBindingIssue,
            geometryOnlyFallbackIssue: geometryOnlyFallbackIssue);
    }

    private void ApplyTextureOverrides(IReadOnlyList<ImportedTexture> externalTextures)
    {
        if (_replacementPath is null || _baseReplacementScene is null)
            throw new InvalidOperationException("Сначала выберите модель-донор.");

        ImportedTextureCatalogResult catalog =
            ImportedTextureCatalog.ResolveExternalOverrides(
                _baseReplacementScene,
                externalTextures);
        RigidGlbTextureBundle? rigidTextureBundle = null;
        string? textureBindingIssue = null;
        try
        {
            RigidGlbTextureBundleReader.TryBindSceneTextures(
                _replacementPath,
                catalog.EffectiveScene,
                out rigidTextureBundle);
        }
        catch (InvalidDataException exception)
        {
            // Commit the partial catalog so more files can be added or removed.
            // Normal import remains blocked until the mapping is complete, while
            // the diagnostic texture-preserving mode can still inspect geometry.
            textureBindingIssue = exception.Message;
        }

        _externalTextures.Clear();
        _externalTextures.AddRange(externalTextures);
        ApplyExternalReplacementState(
            _replacementPath,
            _baseReplacementScene,
            catalog.EffectiveScene,
            rigidTextureBundle,
            catalog,
            textureDirectory: null,
            resetTransform: false,
            textureBindingIssue: textureBindingIssue);
    }

    private void ApplyExternalReplacementState(
        string fullPath,
        ImportedScene sourceScene,
        ImportedScene effectiveScene,
        RigidGlbTextureBundle? rigidTextureBundle,
        ImportedTextureCatalogResult? catalog,
        string? textureDirectory,
        bool resetTransform,
        string? textureBindingIssue = null,
        string? geometryOnlyFallbackIssue = null)
    {
        if (resetTransform)
        {
            ResetSkinnedMaterialOverrides();
            SetPreserveOriginalTextures(false);
            SetPortingModeChoice(PortingModeUiChoice.Auto);
            _portingModeRecommendation = null;
            _geometryOnlyFallbackIssue = geometryOnlyFallbackIssue;
            ResetRigFittingState();
        }

        BoneItem? preferredHead = rigidTextureBundle is not null && _document is not null
            ? FindPreferredHeadBone(BoneCombo.Items.OfType<BoneItem>())
            : null;
        IEnumerable<Vector3> replacementFitPositions = rigidTextureBundle is null
            ? effectiveScene.Meshes.SelectMany(mesh => mesh.Positions)
            : rigidTextureBundle.MaterialGroups
                .SelectMany(group => group.Meshes)
                .SelectMany(mesh => mesh.Positions);
        ReplacementTransform? automaticFit = resetTransform && rigidTextureBundle is not null &&
            _sourceScene is not null
                ? ReplacementTransformFitter.FitByHeightAndCenter(
                    _sourceScene.Meshes.SelectMany(mesh => mesh.Positions),
                    replacementFitPositions)
                : null;
        bool convertedFbx = Path.GetExtension(fullPath).Equals(
            ".fbx", StringComparison.OrdinalIgnoreCase);

        _replacementPath = fullPath;
        _baseReplacementScene = sourceScene;
        _replacementScene = effectiveScene;
        _replacementRigidTextureBundle = rigidTextureBundle;
        _textureCatalogResult = catalog;
        _multiTextureDirectory = textureDirectory;
        _rigidTextureBindingIssue = textureBindingIssue;
        _rigidMultiMaterialAnalysis = null;
        _replacementSmoDocument = null;
        _replacementSmoScene = null;
        _smoReplacementPlan = null;
        _glbSkinTransferPlan = null;
        InvalidateAdaptedPortingPreparation();
        InvalidateGeneratedSkinningPreparation(
            clearGeometryBase: resetTransform);
        _plan = null;
        UpdatePortingModeRecommendation();
        PlanSummaryText.Text = "План ещё не построен.";
        ReplacementPathText.Text = fullPath;
        int jointCount = effectiveScene.Meshes.Select(mesh => mesh.Skinning?.Skeleton)
            .FirstOrDefault(skeleton => skeleton is not null)?.JointNames.Count ?? 0;
        ReplacementSummaryText.Text = rigidTextureBundle is null
            ? $"{effectiveScene.Meshes.Count} source meshes; " +
              $"{effectiveScene.Meshes.Sum(mesh => mesh.Positions.Length):N0} vertices; " +
              $"{effectiveScene.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3):N0} triangles" +
              (effectiveScene.HasSkinning ? $"; {jointCount} skin joints." : ".") +
              (string.IsNullOrWhiteSpace(_geometryOnlyFallbackIssue)
                  ? string.Empty
                  : "\nRig файла проигнорирован; загружена только геометрия.")
            : $"{effectiveScene.Meshes.Count} meshes; " +
              $"{effectiveScene.Meshes.Sum(mesh => mesh.Positions.Length):N0} vertices; " +
              $"{effectiveScene.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3):N0} triangles; " +
              $"{rigidTextureBundle.MaterialGroups.Count} materials; " +
              $"{rigidTextureBundle.MaterialGroups.Sum(group => group.Frames.Count)} textures." +
              (string.IsNullOrWhiteSpace(_geometryOnlyFallbackIssue)
                  ? string.Empty
                  : "\nRig файла проигнорирован; загружена только геометрия.");
        RefreshTextureList();
        AppendRigidTextureBindingStatus();

        if (rigidTextureBundle is null && !effectiveScene.HasSkinning && BoneCombo.Items.Count > 0)
            BoneCombo.SelectedIndex = 0;
        if (rigidTextureBundle is not null)
        {
            string ignoredMeshWarning = rigidTextureBundle.IgnoredMeshes.Count == 0
                ? string.Empty
                : "\nИгнорируются служебные meshes: " +
                  string.Join(", ", rigidTextureBundle.IgnoredMeshes) + ".";
            string ignoredTextureWarning = rigidTextureBundle.IgnoredTextureFiles.Count == 0
                ? string.Empty
                : "\nНе используются текстуры: " +
                  string.Join(", ", rigidTextureBundle.IgnoredTextureFiles) + ".";
            string modelKind = Path.GetExtension(fullPath).TrimStart('.').ToUpperInvariant();
            ReplacementModeText.Text = $"Multi-texture rigid {modelKind} → SMO";
            CompatibilityText.Text =
                "Каждая текстурная группа сохраняется отдельной material/mesh-веткой; " +
                "вся модель rigid-привязана к Head." +
                ignoredMeshWarning + ignoredTextureWarning;
            ReplacementModePanel.Background = new SolidColorBrush(
                Color.FromRgb(220, 252, 231));
            BoneMappingTree.Items.Clear();
            BoneMappingPanel.Visibility = Visibility.Collapsed;
            SplitModeText.Text = textureDirectory is null
                ? "Геометрия не объединяется: встроенные и добавленные изображения " +
                  "остаются отдельными material-группами."
                : "Геометрия не объединяется: material-группы и PNG остаются раздельными; " +
                  "дополнительные matN.frame PNG используются как последовательности кадров.";
            PlanButton.Content = "Проверить multi-texture структуру";
            StatusText.Text =
                "Набор текстур загружен. Проверьте multi-texture структуру.";
            if (preferredHead is not null)
                BoneCombo.SelectedItem = preferredHead;
            if (automaticFit is not null)
                ApplyTransform(automaticFit);
            if (UsesGeneratedWeightsPortingMode)
                UpdateGeneratedSkinningPreparation();
        }
        else if (effectiveScene.HasSkinning)
        {
            RebaseBindPoseCheckBox.IsChecked = true;
            if (resetTransform)
                ApplyTransform(ReplacementTransform.Identity);
            ReplacementModeText.Text = convertedFbx
                ? "Экспериментальный режим Skinned FBX → GLB → SMO"
                : "Экспериментальный режим Skinned GLB → SMO";
            SplitModeText.Text =
                "Triangles автоматически распределяются по существующим 16-bone palettes target; " +
                "object graph и IDs исходного SMO сохраняются.";
            PlanButton.Content = "Проверить кости и построить palettes";
            if (UsesPreparedModelPortingMode)
                UpdateGlbSkinTransferPlan();
            else if (UsesAdaptDonorWeightsPortingMode)
                UpdateAdaptedPortingPreparation();
            else if (UsesGeneratedWeightsPortingMode)
                UpdateGeneratedSkinningPreparation();
        }
        else
        {
            ReplacementModeText.Text = "Экспериментальный режим OBJ/FBX/GLB → SMO";
            CompatibilityText.Text =
                "Требуются подгонка, выбор rigid bone и проверка плана нарезки.";
            ReplacementModePanel.Background = new SolidColorBrush(
                Color.FromRgb(255, 243, 205));
            BoneMappingTree.Items.Clear();
            BoneMappingPanel.Visibility = Visibility.Collapsed;
            SplitModeText.Text =
                "Вся модель помещается в один основной body-slot; остальные slots получают " +
                "невидимый вырожденный triangle.";
            PlanButton.Content = "Построить план и проверить";
            StatusText.Text = "Модель замены загружена. Постройте план нарезки.";
            if (UsesGeneratedWeightsPortingMode)
                UpdateGeneratedSkinningPreparation();
        }
        if (GetPortingModeBlockMessage() is string blocked)
            StatusText.Text = blocked;
    }

    private void UpdateRigidTextureModeDescription()
    {
        if (_replacementRigidTextureBundle is null || _replacementPath is null)
            return;

        string modelKind = Path.GetExtension(_replacementPath)
            .TrimStart('.')
            .ToUpperInvariant();
        if (PreserveOriginalTextures)
        {
            ReplacementModeText.Text =
                $"Диагностический rigid {modelKind} → SMO с исходной текстурой";
            CompatibilityText.Text =
                "TextureData исходного SMO останутся побайтно неизменными. " +
                "Material-группы донора объединяются и используют основной " +
                "material-slot исходной модели.";
            SplitModeText.Text =
                "Вся геометрия помещается в один основной body-slot; действует " +
                "лимит 65 535 уникальных вершин.";
            ReplacementModePanel.Background = new SolidColorBrush(
                Color.FromRgb(255, 243, 205));
            StatusText.Text =
                "Текстуры донора отключены. Постройте диагностический план геометрии.";
            return;
        }

        string ignoredMeshWarning = _replacementRigidTextureBundle.IgnoredMeshes.Count == 0
            ? string.Empty
            : "\nИгнорируются служебные meshes: " +
              string.Join(", ", _replacementRigidTextureBundle.IgnoredMeshes) + ".";
        string ignoredTextureWarning =
            _replacementRigidTextureBundle.IgnoredTextureFiles.Count == 0
                ? string.Empty
                : "\nНе используются текстуры: " +
                  string.Join(", ", _replacementRigidTextureBundle.IgnoredTextureFiles) + ".";
        ReplacementModeText.Text = $"Multi-texture rigid {modelKind} → SMO";
        CompatibilityText.Text =
            "Каждая текстурная группа сохраняется отдельной material/mesh-веткой; " +
            "вся модель rigid-привязана к Head." +
            ignoredMeshWarning + ignoredTextureWarning;
        SplitModeText.Text = _multiTextureDirectory is null
            ? "Геометрия не объединяется: встроенные и добавленные изображения " +
              "остаются отдельными material-группами."
            : "Геометрия не объединяется: material-группы и PNG остаются раздельными; " +
              "дополнительные matN.frame PNG используются как последовательности кадров.";
        ReplacementModePanel.Background = new SolidColorBrush(
            Color.FromRgb(220, 252, 231));
        StatusText.Text =
            "Текстуры донора включены. Проверьте multi-texture структуру.";
    }

    private void AppendRigidTextureBindingStatus()
    {
        if (string.IsNullOrWhiteSpace(_rigidTextureBindingIssue))
            return;
        TextureSummaryText.Text += PreserveOriginalTextures
            ? " Ошибка сопоставления не мешает диагностическому режиму: " +
              _rigidTextureBindingIssue
            : " Обычный импорт заблокирован: " + _rigidTextureBindingIssue;
    }

    private bool EnsureBlenderForFbx()
    {
        _blenderPath = FbxExporter.FindBlenderExecutable(_blenderPath);
        if (_blenderPath is not null)
            return true;
        var dialog = new OpenFileDialog
        {
            Title = "Для импорта FBX укажите blender.exe",
            Filter = "Blender (blender.exe)|blender.exe|Исполняемые файлы (*.exe)|*.exe",
            CheckFileExists = true
        };
        if (dialog.ShowDialog(this) != true)
            return false;
        _blenderPath = FbxExporter.ResolveBlenderExecutable(dialog.FileName);
        if (_blenderPath is not null)
            return true;
        MessageBox.Show(this,
            "Выбранный файл не является blender.exe.",
            "Blender не найден", MessageBoxButton.OK, MessageBoxImage.Warning);
        return false;
    }

    private void UpdateGlbSkinTransferPlan()
    {
        _plan = null;
        if (_document is null || _replacementScene?.HasSkinning != true)
        {
            _glbSkinTransferPlan = null;
            CompatibilityText.Text = "Сначала выберите целевой SMO.";
            StatusText.Text = "Skinned GLB загружен; требуется целевой SMO.";
            return;
        }
        _glbSkinTransferPlan = AnalyzeGlbSkinTransfer(
            _document,
            _replacementScene,
            SelectedSkinnedTextureTransferMode);
        PopulateGlbBoneMappingTree(_glbSkinTransferPlan);
        string details = _glbSkinTransferPlan.Messages.Count == 0
            ? string.Empty
            : "\n" + string.Join("\n", _glbSkinTransferPlan.Messages.Select(
                message => "• " + message));
        CompatibilityText.Text =
            $"Joints: {_glbSkinTransferPlan.JointCount}; active weights: " +
            $"{_glbSkinTransferPlan.ActiveJointCount}; exact: " +
            $"{_glbSkinTransferPlan.MatchedBoneNames.Count}; remap: " +
            $"{_glbSkinTransferPlan.RemappedBones.Count}; material groups: " +
            $"{_glbSkinTransferPlan.MaterialGroupCount}; bind-pose differences: " +
            $"{_glbSkinTransferPlan.DifferentBindPoseJointCount}." + details;
        if (_glbSkinTransferPlan.CanReplace)
        {
            ReplacementModePanel.Background = new SolidColorBrush(
                Color.FromRgb(255, 243, 205));
            StatusText.Text =
                _glbSkinTransferPlan.DifferentBindPoseJointCount > 0
                    ? "Skinned GLB совместим. Оставьте одноразовое согласование bind pose включённым; donor-узлы и donor inverse-bind в SMO не переносятся."
                    : "Skinned GLB совместим. Bind pose уже совпадает; проверьте дерево костей и palettes.";
        }
        else
        {
            ReplacementModePanel.Background = new SolidColorBrush(
                Color.FromRgb(254, 226, 226));
            StatusText.Text =
                "Skinned GLB заблокирован: проверьте подробные сообщения плана.";
        }
    }

    private bool EnsureRigFittingState()
    {
        if (_targetRigDefinition is not null &&
            _targetRigFittingPose is not null &&
            _rigLocalEulerDegrees is not null)
            return true;
        if (!string.IsNullOrWhiteSpace(_rigFittingIssue))
        {
            RigFittingStatusText.Text =
                "Редактор позы недоступен: " + _rigFittingIssue;
            return false;
        }
        if (_document is null || !HasExternalReplacement)
        {
            _rigFittingIssue =
                "Для редактора позы нужны текущие target SMO и модель-донор.";
            RigFittingStatusText.Text = _rigFittingIssue;
            return false;
        }

        try
        {
            _targetRigDefinition = TargetRigDefinition.FromSmoDocument(_document);
            _targetRigFittingPose = _targetRigDefinition.CreateFittingPose();
            _rigLocalEulerDegrees = new Vector3[_targetRigDefinition.Joints.Count];
            _rigRootEulerDegrees = Vector3.Zero;
            _rigFittingIssue = null;
            TargetRigJointItem[] editableJoints = _targetRigDefinition.Joints
                .Where(joint => joint.IsDeformJoint)
                .Select(joint => new TargetRigJointItem(
                    joint.JointIndex,
                    $"[{joint.JointIndex}] {joint.Name}"))
                .ToArray();
            _settingRigFittingControls = true;
            try
            {
                RigFittingJointCombo.ItemsSource = editableJoints;
                RigFittingJointCombo.SelectedIndex = editableJoints.Length > 0 ? 0 : -1;
            }
            finally
            {
                _settingRigFittingControls = false;
            }
            LoadRigFittingEditorValues();
            RigFittingStatusText.Text =
                $"Начальная поза каноническая; deform-костей: {editableJoints.Length}, " +
                $"всего FK-узлов с service ancestors: {_targetRigDefinition.Joints.Count}.";
            BodyPoseStatusText.Text =
                "Нажмите автоподгонку для начальной позы или настройте симметричные " +
                "части тела ползунками.";
            return true;
        }
        catch (Exception exception) when (exception is InvalidDataException or
                                          InvalidOperationException or
                                          OverflowException or
                                          ArgumentException)
        {
            _targetRigDefinition = null;
            _targetRigFittingPose = null;
            _rigLocalEulerDegrees = null;
            _rigFittingIssue = exception.Message;
            RigFittingStatusText.Text =
                "Редактор позы недоступен: " + exception.Message;
            return false;
        }
    }

    private TargetRigFittingPoseSnapshot CaptureRigFittingPose(
        bool localRotationsOnly = false)
    {
        if (!EnsureRigFittingState() || _targetRigFittingPose is null)
            throw new InvalidOperationException(
                _rigFittingIssue ?? "Поза подгонки не создана.");
        if (localRotationsOnly)
        {
            TargetRigFittingPose localOnlyPose =
                _targetRigFittingPose.Definition.CreateFittingPose();
            for (int jointIndex = 0;
                 jointIndex < _targetRigFittingPose.LocalRotationDeltas.Count;
                 jointIndex++)
            {
                localOnlyPose.SetLocalRotationDelta(
                    jointIndex,
                    _targetRigFittingPose.LocalRotationDeltas[jointIndex]);
            }
            return localOnlyPose.Capture();
        }
        return _targetRigFittingPose.Capture();
    }

    private TargetRigFittingPoseSnapshot CaptureDisplayedRigFittingPose(
        bool localRotationsOnly = false)
    {
        TargetRigFittingPoseSnapshot snapshot =
            _bodyPoseEditorDirty && _bodyPoseDraftSnapshot is not null
                ? _bodyPoseDraftSnapshot
                : CaptureRigFittingPose(localRotationsOnly: false);
        if (!localRotationsOnly ||
            (snapshot.RootRotation == Quaternion.Identity &&
             snapshot.RootTranslation == Vector3.Zero))
        {
            return snapshot;
        }

        TargetRigFittingPose localOnly = snapshot.Definition.CreateFittingPose();
        for (int jointIndex = 0;
             jointIndex < snapshot.LocalRotationDeltas.Count;
             jointIndex++)
        {
            localOnly.SetLocalRotationDelta(
                jointIndex,
                snapshot.LocalRotationDeltas[jointIndex]);
        }
        return localOnly.Capture();
    }

    private void UpdateAdaptedPortingPreparation()
    {
        _plan = null;
        _glbSkinTransferPlan = null;
        InvalidateAdaptedPortingPreparation();
        ReplacementModeText.Text = "2. Адаптация donor weights к игровому скелету";
        SplitModeText.Text =
            "Сначала веса и bind pose переводятся на игровые кости, затем triangles " +
            "распределяются по существующим 16-bone palettes без изменения target graph.";
        if (ManualAdaptWeights)
        {
            UpdateManualAdaptedPortingPreparation();
            return;
        }
        if (_document is null || _replacementScene?.HasSkinning != true)
        {
            _adaptedPortingPreparationIssue =
                "Для адаптации нужны целевой SMO и модель с исходными skin weights.";
            CompatibilityText.Text = _adaptedPortingPreparationIssue;
            return;
        }

        SkinnedModelPortingAnalysis analysis;
        try
        {
            analysis = SkinnedModelPortingPreparer.AnalyzeAdaptDonorWeights(
                _document, _replacementScene);
        }
        catch (Exception exception) when (exception is InvalidDataException or
                                          InvalidOperationException or
                                          OverflowException or
                                          ArgumentException)
        {
            _adaptedPortingPreparationIssue = exception.Message;
            CompatibilityText.Text =
                "Анализ адаптации не завершён: " + exception.Message;
            ReplacementModePanel.Background = new SolidColorBrush(
                Color.FromRgb(254, 226, 226));
            StatusText.Text = "Автоматическая адаптация весов не завершена.";
            return;
        }
        PopulateAdaptedBoneMappingTree(analysis);
        string details = analysis.Messages.Count == 0
            ? string.Empty
            : "\n" + string.Join("\n", analysis.Messages.Select(message => "• " + message));
        CompatibilityText.Text =
            $"Donor joints: {analysis.ActiveDonorJointCount}; mapped: " +
            $"{analysis.JointMappings.Count}; target deform joints: " +
            $"{analysis.TargetDeformJointCount}; donor skeletons: " +
            $"{analysis.DonorSkeletonCount}." + details;
        if (!analysis.CanPrepare)
        {
            _adaptedPortingPreparationIssue = analysis.Errors.Count == 0
                ? "Адаптация весов признана несовместимой."
                : string.Join(" | ", analysis.Errors);
            ReplacementModePanel.Background = new SolidColorBrush(
                Color.FromRgb(254, 226, 226));
            StatusText.Text =
                "Автоматическая адаптация весов заблокирована: исправьте сопоставление костей.";
            return;
        }

        try
        {
            _adaptedPortingPreparation =
                SkinnedModelPortingPreparer.PrepareAdaptDonorWeights(
                    _document, _replacementScene);
            _adaptedPortingPreparationRevision = _rigFittingRevision;
            _glbSkinTransferPlan = AnalyzeGlbSkinTransfer(
                _document,
                _adaptedPortingPreparation.PreparedScene,
                SelectedSkinnedTextureTransferMode);
            AppendGlbSkinTransferPlanMessages(_glbSkinTransferPlan);
            ReplacementModePanel.Background = new SolidColorBrush(
                _glbSkinTransferPlan.CanReplace
                    ? Color.FromRgb(255, 243, 205)
                    : Color.FromRgb(254, 226, 226));
            StatusText.Text = _glbSkinTransferPlan.CanReplace
                ? "Веса донора адаптированы к игровым костям. Проверьте результат в окне и постройте план."
                : "Веса донора адаптированы, но skinned-план несовместим. " +
                  "Проверьте подробные сообщения плана.";
        }
        catch (Exception exception) when (exception is InvalidDataException or
                                          InvalidOperationException or
                                          OverflowException or
                                          ArgumentException)
        {
            _adaptedPortingPreparation = null;
            _adaptedPortingPreparationRevision = -1;
            _adaptedPortingPreparationIssue = exception.Message;
            CompatibilityText.Text += "\n• Подготовка не завершена: " + exception.Message;
            ReplacementModePanel.Background = new SolidColorBrush(
                Color.FromRgb(254, 226, 226));
            StatusText.Text = "Автоматическая адаптация весов не завершена.";
        }
    }

    private void UpdateManualAdaptedPortingPreparation()
    {
        if (_document is null || _replacementScene?.HasSkinning != true)
        {
            _adaptedPortingPreparationIssue =
                "Ручная адаптация требует target SMO и donor skin weights.";
            CompatibilityText.Text = _adaptedPortingPreparationIssue;
            return;
        }
        if (RigFittingEditorHasPendingChanges)
        {
            _adaptedPortingPreparationIssue =
                "Есть неприменённые значения позы или donor alignment. Нажмите «Применить».";
            CompatibilityText.Text = _adaptedPortingPreparationIssue;
            return;
        }

        try
        {
            TargetRigFittingPoseSnapshot snapshot = CaptureRigFittingPose();
            _adaptedPortingPreparation =
                SkinnedModelPortingPreparer.PrepareAdaptDonorWeights(
                    _document,
                    _replacementScene,
                    snapshot,
                    _manualDonorAlignment);
            _adaptedPortingPreparationRevision = _rigFittingRevision;
            SkinnedModelPortingAnalysis analysis =
                _adaptedPortingPreparation.Analysis;
            PopulateAdaptedBoneMappingTree(analysis);
            _glbSkinTransferPlan = AnalyzeGlbSkinTransfer(
                _document,
                _adaptedPortingPreparation.PreparedScene,
                SelectedSkinnedTextureTransferMode);
            string details = analysis.Messages.Count == 0
                ? string.Empty
                : "\n" + string.Join(
                    "\n",
                    analysis.Messages.Select(message => "• " + message));
            CompatibilityText.Text =
                $"Weights-only fitting; donor joints: {analysis.ActiveDonorJointCount}; " +
                $"mapped: {analysis.JointMappings.Count}; target deform joints: " +
                $"{analysis.TargetDeformJointCount}; revision {_rigFittingRevision}." +
                details;
            AppendGlbSkinTransferPlanMessages(_glbSkinTransferPlan);
            ReplacementModePanel.Background = new SolidColorBrush(
                _glbSkinTransferPlan.CanReplace
                    ? Color.FromRgb(255, 243, 205)
                    : Color.FromRgb(254, 226, 226));
            StatusText.Text = _glbSkinTransferPlan.CanReplace
                ? "Ручная weights-only подгонка подготовлена. Donor bind geometry не использовалась; проверьте fitting preview."
                : "Ручная weights-only подгонка построена, но skinned-план несовместим. " +
                  "Проверьте подробные сообщения плана.";
        }
        catch (Exception exception) when (exception is InvalidDataException or
                                          InvalidOperationException or
                                          OverflowException or
                                          ArgumentException or
                                          NotSupportedException)
        {
            _adaptedPortingPreparation = null;
            _adaptedPortingPreparationRevision = -1;
            _adaptedPortingPreparationIssue = exception.Message;
            CompatibilityText.Text =
                "Ручная weights-only подгонка не подготовлена: " + exception.Message;
            ReplacementModePanel.Background = new SolidColorBrush(
                Color.FromRgb(254, 226, 226));
            StatusText.Text = CompatibilityText.Text;
        }
    }

    private void PopulateAdaptedBoneMappingTree(
        SkinnedModelPortingAnalysis analysis)
    {
        BoneMappingTree.Items.Clear();
        BoneMappingPanel.Visibility = Visibility.Visible;

        var mappings = new TreeViewItem
        {
            Header = $"Сопоставлены активные кости: {analysis.JointMappings.Count}",
            IsExpanded = analysis.Errors.Count > 0
        };
        foreach (SkinnedModelPortingJointMapping mapping in analysis.JointMappings)
        {
            mappings.Items.Add(new TreeViewItem
            {
                Header = $"{mapping.DonorJointName} → {mapping.TargetJointName} ({mapping.MatchKind})"
            });
        }
        BoneMappingTree.Items.Add(mappings);

        var unused = new TreeViewItem
        {
            Header = $"Неактивные donor joints: {analysis.UnusedDonorJoints.Count}",
            IsExpanded = false
        };
        foreach (string name in analysis.UnusedDonorJoints)
            unused.Items.Add(new TreeViewItem { Header = name });
        if (analysis.UnusedDonorJoints.Count == 0)
            unused.Items.Add(new TreeViewItem { Header = "Нет" });
        BoneMappingTree.Items.Add(unused);

        var unweighted = new TreeViewItem
        {
            Header = $"Игровые кости без donor weights: {analysis.TargetJointsWithoutWeights.Count}",
            IsExpanded = false
        };
        foreach (string name in analysis.TargetJointsWithoutWeights)
            unweighted.Items.Add(new TreeViewItem { Header = name });
        if (analysis.TargetJointsWithoutWeights.Count == 0)
            unweighted.Items.Add(new TreeViewItem { Header = "Нет" });
        BoneMappingTree.Items.Add(unweighted);

        if (analysis.Errors.Count > 0)
        {
            var errors = new TreeViewItem
            {
                Header = $"Ошибки: {analysis.Errors.Count}",
                Foreground = new SolidColorBrush(Color.FromRgb(185, 28, 28)),
                IsExpanded = true
            };
            foreach (string error in analysis.Errors)
                errors.Items.Add(new TreeViewItem { Header = error });
            BoneMappingTree.Items.Add(errors);
        }
    }

    private void UpdateGeneratedSkinningPreparation()
    {
        _plan = null;
        _glbSkinTransferPlan = null;
        InvalidateGeneratedSkinningPreparation();
        ReplacementModeText.Text = "3. Автоматическое создание весов";
        SplitModeText.Text =
            "Geometry выравнивается по основному телу target, получает игровые веса " +
            "и затем распределяется по существующим 16-bone palettes без изменения target graph.";
        BoneMappingTree.Items.Clear();
        BoneMappingPanel.Visibility = Visibility.Collapsed;

        if (_document is null)
        {
            SetGeneratedSkinningFailure(
                "Сначала выберите целевой SMO для alignment и расчёта весов.",
                isError: false);
            return;
        }
        if (_replacementScene is null)
        {
            SetGeneratedSkinningFailure("Сначала выберите модель-донор.", isError: false);
            return;
        }
        try
        {
            ImportedScene inputScene = ResolveGeneratedSkinningInputScene();
            TargetRigFittingPoseSnapshot fittingPose = CaptureRigFittingPose(
                localRotationsOnly: true);
            bool discoverAutomaticAlignment = _generatedDonorAlignment is null;
            if (discoverAutomaticAlignment)
            {
                // Keep a usable coarse alignment even when conservative topology
                // analysis fails before mode 3 can produce its robust alignment.
                // This lets the user correct height/position against the target
                // in the raw preview without weakening any Core safety check.
                SetGeneratedDonorAlignmentState(
                    ComputeCoarseGeneratedDonorAlignment(inputScene),
                    updateEditor: true);
                try
                {
                    GeneratedSkinningPreparationResult alignmentDiscovery =
                        GeneratedSkinningPreparer.Prepare(
                        _document,
                        inputScene,
                        fittingPose);
                    GeneratedSkinningAlignment automaticAlignment =
                        alignmentDiscovery.Analysis.Alignment;
                    SetGeneratedDonorAlignmentState(
                        new ReplacementTransform(
                            automaticAlignment.Scale,
                            Vector3.Zero,
                            automaticAlignment.Translation),
                        updateEditor: true);
                    // The legacy preparation above is used only to discover a
                    // robust alignment. Body-surface selection is a separate
                    // prerequisite for manual fitting: otherwise a successful
                    // single-surface preparation would make the pose controls
                    // depend on pressing the automatic pose fitter first.
                    TargetRigBodySelection bodySelection =
                        ResolveGeneratedBodySelection(
                            inputScene,
                            _generatedDonorAlignment!);
                    _generatedSkinningPreparation = PrepareGeneratedSkinning(
                        inputScene,
                        fittingPose,
                        _generatedDonorAlignment!,
                        bodySelection,
                        _generatedSkinningComponentOverrides);
                }
                catch (Exception automaticException) when (
                    automaticException is InvalidDataException or
                                          InvalidOperationException or
                                          ArgumentException or
                                          NotSupportedException)
                {
                    // A fragmented character can be perfectly usable even when
                    // the legacy single-dominant-surface heuristic rejects it.
                    // Select the humanoid lower/upper surfaces independently,
                    // but keep the canonical/manual pose instead of accepting
                    // any automatically optimized joint rotations.
                    TargetRigBodySelection bodySelection =
                        ResolveGeneratedBodySelection(
                            inputScene,
                            _generatedDonorAlignment!);
                    _generatedSkinningPreparation = PrepareGeneratedSkinning(
                        inputScene,
                        fittingPose,
                        _generatedDonorAlignment!,
                        bodySelection,
                        _generatedSkinningComponentOverrides);
                }
            }
            else
            {
                TargetRigBodySelection bodySelection =
                    ResolveGeneratedBodySelection(
                        inputScene,
                        _generatedDonorAlignment!);
                _generatedSkinningPreparation = PrepareGeneratedSkinning(
                    inputScene,
                    fittingPose,
                    _generatedDonorAlignment!,
                    bodySelection,
                    _generatedSkinningComponentOverrides);
            }
            _generatedSkinningPreparationRevision = _rigFittingRevision;
            _glbSkinTransferPlan = AnalyzeGlbSkinTransfer(
                _document,
                _generatedSkinningPreparation.PreparedScene,
                SelectedSkinnedTextureTransferMode);
            PopulateGeneratedSkinningDiagnostics(
                _generatedSkinningPreparation.Analysis);
            UpdateGeneratedSkinningCompatibilityPresentation();
            StatusText.Text = _glbSkinTransferPlan.CanReplace
                ? "Автоматические веса построены и показаны. Проверьте позу, предупреждения " +
                  "и attachments, затем подтвердите результат."
                : "Веса построены и показаны, но итоговый skinned-план пока несовместим. " +
                  "Проверьте сообщения и текстуры.";
        }
        catch (Exception exception) when (exception is InvalidDataException or
                                          InvalidOperationException or
                                          OverflowException or
                                          ArgumentException or
                                          NotSupportedException)
        {
            SetGeneratedSkinningFailure(exception.Message);
        }
        finally
        {
            RefreshTextureList();
        }
    }

    private TargetRigBodySelection ResolveGeneratedBodySelection(
        ImportedScene inputScene,
        ReplacementTransform alignment)
    {
        if (_targetRigDefinition is null && !EnsureRigFittingState())
        {
            throw new InvalidOperationException(
                _rigFittingIssue ?? "Игровой скелет недоступен.");
        }

        TargetRigBodySelection? automatic =
            _bodyPoseAutoFitResult?.BodySelection;
        if (automatic is not null &&
            Equals(automatic.DonorAlignment, alignment))
        {
            _generatedBodySelection = automatic;
            return automatic;
        }
        if (_generatedBodySelection is not null &&
            Equals(_generatedBodySelection.DonorAlignment, alignment))
        {
            return _generatedBodySelection;
        }

        _generatedBodySelection = TargetRigAutomaticPoseFitter.SelectBody(
            _targetRigDefinition!,
            inputScene,
            alignment);
        return _generatedBodySelection;
    }

    private void UpdateGeneratedSkinningCompatibilityPresentation()
    {
        if (_generatedSkinningPreparation is null || _glbSkinTransferPlan is null)
            return;

        string planDetails = _glbSkinTransferPlan.Messages.Count == 0
            ? string.Empty
            : "\n" + string.Join(
                "\n",
                _glbSkinTransferPlan.Messages.Select(message => "• " + message));
        CompatibilityText.Text =
            $"Generated weights: {_generatedSkinningPreparation.Analysis.PreparedVertexCount:N0} vertices; " +
            $"target deform joints: {_generatedSkinningPreparation.Analysis.TargetDeformJointCount}; " +
            $"rigid attachments: {_generatedSkinningPreparation.Analysis.Attachments.Count}." +
            planDetails;
        ReplacementModePanel.Background = new SolidColorBrush(
            _glbSkinTransferPlan.CanReplace
                ? Color.FromRgb(255, 243, 205)
                : Color.FromRgb(254, 226, 226));
    }

    private GeneratedSkinningPreparationResult PrepareGeneratedSkinning(
        ImportedScene inputScene,
        TargetRigFittingPoseSnapshot fittingPose,
        ReplacementTransform alignment,
        TargetRigBodySelection? bodySelection,
        GeneratedSkinningComponentOverrides? componentOverrides)
    {
        if (_document is null)
            throw new InvalidOperationException("Сначала выберите исходный SMO.");

        bool bodySelectionIsCurrent = bodySelection is not null &&
            Equals(bodySelection.DonorAlignment, alignment);
        if (componentOverrides is not null)
        {
            if (bodySelectionIsCurrent)
            {
                HashSet<int> explicitlyRigidComponents = componentOverrides
                    .Components
                    .Select(component => component.ComponentIndex)
                    .ToHashSet();
                TargetRigSelectedBodyComponent[] filteredBody = bodySelection!
                    .Components
                    .Where(component => !explicitlyRigidComponents.Contains(
                        component.ComponentIndex))
                    .ToArray();
                if (filteredBody.Length != bodySelection.Components.Count)
                {
                    // An explicit rigid pin is the user's final decision. Keep
                    // it out of a later automatic body selection instead of
                    // allowing a pose/alignment rerun to silently override it.
                    bodySelection = bodySelection with
                    {
                        Components = Array.AsReadOnly(filteredBody),
                        ExcludedComponentCount =
                            bodySelection.TotalComponentCount - filteredBody.Length
                    };
                }
            }
            return bodySelectionIsCurrent
                ? GeneratedSkinningPreparer.Prepare(
                    _document,
                    inputScene,
                    fittingPose,
                    alignment,
                    bodySelection!,
                    componentOverrides)
                : GeneratedSkinningPreparer.Prepare(
                    _document,
                    inputScene,
                    fittingPose,
                    alignment,
                    componentOverrides);
        }

        return bodySelectionIsCurrent
            ? GeneratedSkinningPreparer.Prepare(
                _document,
                inputScene,
                fittingPose,
                alignment,
                bodySelection!)
            : GeneratedSkinningPreparer.Prepare(
                _document,
                inputScene,
                fittingPose,
                alignment);
    }

    private ImportedScene ResolveGeneratedSkinningInputScene()
    {
        if (_baseReplacementScene is null)
            throw new InvalidOperationException("Модель-донор не загружена.");
        if (_baseReplacementScene.Meshes.Count == 0)
            throw new InvalidDataException("Модель-донор не содержит geometry.");

        if (_baseReplacementScene.Meshes.All(mesh => mesh.Skinning is null))
        {
            // Mode 3 must start from the complete imported scene. The legacy
            // rigid matN bundle may intentionally omit meshes and therefore is
            // never a safe source for generated weights.
            _generatedSkinningBaseScene = _baseReplacementScene;
            _generatedSkinningTextureCatalog =
                ImportedTextureCatalog.ResolveExternalOverrides(
                    _generatedSkinningBaseScene,
                    _externalTextures);
            _generatedSkinningEffectiveScene =
                _generatedSkinningTextureCatalog.EffectiveScene;
            return _generatedSkinningEffectiveScene;
        }
        if (string.IsNullOrWhiteSpace(_replacementPath))
            throw new InvalidOperationException("Путь к модели-донору не сохранён.");

        _generatedSkinningBaseScene ??= ImportedModelReader.ReadGeometryOnly(
            _replacementPath,
            _blenderPath);
        if (_generatedSkinningBaseScene.Meshes.Count == 0 ||
            _generatedSkinningBaseScene.Meshes.Any(mesh => mesh.Skinning is not null))
        {
            throw new InvalidDataException(
                "Geometry-only reader не вернул полностью unskinned geometry.");
        }

        _generatedSkinningTextureCatalog =
            ImportedTextureCatalog.ResolveExternalOverrides(
                _generatedSkinningBaseScene,
                _externalTextures);
        _generatedSkinningEffectiveScene =
            _generatedSkinningTextureCatalog.EffectiveScene;
        return _generatedSkinningEffectiveScene;
    }

    private ReplacementTransform ComputeCoarseGeneratedDonorAlignment(
        ImportedScene inputScene)
    {
        if (_sourceScene is null)
        {
            throw new InvalidOperationException(
                "Для подгонки размера и положения сначала выберите целевой SMO.");
        }

        return ReplacementTransformFitter.FitByHeightAndCenter(
            _sourceScene.Meshes.SelectMany(mesh => mesh.Positions),
            inputScene.Meshes.SelectMany(mesh => mesh.Positions));
    }

    private void SetGeneratedDonorAlignmentState(
        ReplacementTransform alignment,
        bool updateEditor)
    {
        ValidateGeneratedDonorAlignment(alignment);
        _generatedDonorAlignment = alignment;
        _generatedDonorAlignmentDraft = alignment;
        _generatedAlignmentEditorDirty = false;
        if (updateEditor && UsesGeneratedWeightsPortingMode)
            WriteTransformEditor(alignment);
        UpdateGeneratedAlignmentText(alignment);
    }

    private void UpdateGeneratedAlignmentText(ReplacementTransform? alignment)
    {
        GeneratedSkinningAlignmentText.Text = alignment is null
            ? "Alignment: —"
            : $"Alignment: scale {alignment.Scale:G6}; move " +
              $"({alignment.Translation.X:G6}, {alignment.Translation.Y:G6}, " +
              $"{alignment.Translation.Z:G6})";
    }

    private void PopulateGeneratedSkinningDiagnostics(
        GeneratedSkinningAnalysis analysis)
    {
        UpdateGeneratedAlignmentText(_generatedDonorAlignment);
        int attachmentBoneCount = analysis.Attachments
            .Select(attachment => attachment.TargetBoneName)
            .Distinct(StringComparer.Ordinal)
            .Count();
        GeneratedSkinningSummaryText.Text =
            $"Main body target: {analysis.TargetMainComponentVertexCount:N0} vertices / " +
            $"{analysis.TargetMainComponentTriangleCount:N0} triangles; donor: " +
            $"{analysis.DonorMainComponentVertexCount:N0} / " +
            $"{analysis.DonorMainComponentTriangleCount:N0}. Prepared: " +
            $"{analysis.PreparedVertexCount:N0} vertices, " +
            $"{analysis.TargetDeformJointCount} deform bones. Отдельные детали: " +
            $"{analysis.Attachments.Count}; назначено костей: {attachmentBoneCount}.";
        string[] distinctWarnings = analysis.Warnings
            .Where(warning => !IsGeneratedAttachmentEchoWarning(warning))
            .Distinct(StringComparer.Ordinal)
            .ToArray();
        GeneratedSkinningWarningsText.Text = distinctWarnings.Length == 0
            ? "Нет."
            : string.Join("\n", distinctWarnings.Select(warning => "• " + warning));
        GeneratedSkinningAttachmentsText.Text = analysis.Attachments.Count == 0
            ? "Нет: отдельных rigid-компонентов не найдено."
            : string.Join(
                "\n",
                analysis.Attachments.Select(attachment =>
                    $"• component {attachment.ComponentIndex}: " +
                    $"{string.Join(", ", attachment.MeshNames)} → " +
                    $"{attachment.TargetBoneName} " +
                    $"({attachment.VertexCount:N0} vertices, " +
                    $"{attachment.TriangleCount:N0} triangles, " +
                    $"distance {attachment.DistanceToBone:G5})"));
        GeneratedSkinningDetailsExpander.Header =
            $"Технические детали: предупреждений {distinctWarnings.Length}, " +
            $"rigid attachments {analysis.Attachments.Count}";
        PopulateGeneratedAttachmentEditor(analysis);
        UpdateGeneratedSkinningConfirmationAvailability();
        UpdateGeneratedSkinningPrimaryStatus();
    }

    private void PopulateGeneratedAttachmentEditor(
        GeneratedSkinningAnalysis analysis)
    {
        GeneratedAttachmentListItem[] items = analysis.Attachments
            .OrderBy(attachment => attachment.ComponentIndex)
            .Select(attachment => new GeneratedAttachmentListItem(attachment))
            .ToArray();
        HashSet<int> available = items
            .Select(item => item.ComponentIndex)
            .ToHashSet();
        _selectedGeneratedAttachmentComponents.RemoveWhere(
            componentIndex => !available.Contains(componentIndex));

        _generatedAttachmentComponentByMeshVertex.Clear();
        foreach (GeneratedSkinningAttachment attachment in analysis.Attachments)
        {
            foreach (TargetRigBodyVertexMembership membership in attachment.VerticesByMesh)
            {
                if (!_generatedAttachmentComponentByMeshVertex.TryGetValue(
                        membership.MeshIndex,
                        out Dictionary<int, int>? componentsByVertex))
                {
                    componentsByVertex = [];
                    _generatedAttachmentComponentByMeshVertex.Add(
                        membership.MeshIndex,
                        componentsByVertex);
                }
                foreach (int vertexIndex in membership.VertexIndices)
                    componentsByVertex[vertexIndex] = attachment.ComponentIndex;
            }
        }

        _settingGeneratedAttachmentSelection = true;
        try
        {
            GeneratedAttachmentList.ItemsSource = items;
            foreach (GeneratedAttachmentListItem item in items)
            {
                if (_selectedGeneratedAttachmentComponents.Contains(
                        item.ComponentIndex))
                {
                    GeneratedAttachmentList.SelectedItems.Add(item);
                }
            }
        }
        finally
        {
            _settingGeneratedAttachmentSelection = false;
        }

        int manualCount = analysis.Attachments.Count(attachment =>
            attachment.ManualAssignment is not null);
        GeneratedAttachmentSummaryText.Text = analysis.Attachments.Count == 0
            ? "Отдельных жёстких деталей не найдено."
            : $"Деталей: {analysis.Attachments.Count}; вручную закреплено: " +
              $"{manualCount}. Можно выбирать несколько строк или щёлкать по модели.";
        UpdateGeneratedAttachmentEditorAvailability();
    }

    private void UpdateGeneratedAttachmentEditorAvailability()
    {
        if (GeneratedAttachmentEditorPanel is null)
            return;

        bool selectionAvailable = UsesGeneratedWeightsPortingMode &&
            !IsJointPoseEditorMode &&
            GeneratedSkinningPreparationIsCurrent &&
            !_nativeValidationRunning;
        bool assignmentAvailable = selectionAvailable &&
            !RigFittingEditorHasPendingChanges;
        GeneratedAttachmentEditorPanel.Opacity = selectionAvailable ? 1 : 0.65;
        GeneratedAttachmentList.IsEnabled = selectionAvailable;
        HideGeneratedMainBodyCheckBox.IsEnabled = selectionAvailable;

        GeneratedAttachmentListItem[] selected = GeneratedAttachmentList
            .SelectedItems
            .OfType<GeneratedAttachmentListItem>()
            .ToArray();
        SelectSameMesh.IsEnabled = selectionAvailable && selected.Length > 0;
        PinUpperBack.IsEnabled = assignmentAvailable && selected.Length > 0;
        PinHead.IsEnabled = assignmentAvailable && selected.Length > 0;
        ResetAutomatic.IsEnabled = assignmentAvailable && selected.Any(item =>
            item.Attachment.ManualAssignment is not null);

        if (!GeneratedSkinningPreparationIsCurrent)
        {
            GeneratedAttachmentStatusText.Text =
                "Сначала дождитесь успешного расчёта автоматических весов.";
        }
        else if (IsJointPoseEditorMode)
        {
            GeneratedAttachmentStatusText.Text =
                "Переключитесь в режим «Человек», чтобы выбирать жёсткие детали.";
        }
        else if (RigFittingEditorHasPendingChanges)
        {
            GeneratedAttachmentStatusText.Text =
                "Сначала примените текущие изменения позы или положения модели.";
        }
        else if (selected.Length == 0)
        {
            GeneratedAttachmentStatusText.Text =
                "Выберите детали в списке или щёлкните по ним в окне просмотра.";
        }
        else
        {
            GeneratedAttachmentStatusText.Text =
                $"Выбрано: {selected.Length}. " +
                string.Join(
                    ", ",
                    selected.Take(8).Select(item => $"#{item.ComponentIndex}")) +
                (selected.Length > 8 ? "…" : string.Empty);
        }
    }

    private void GeneratedAttachmentList_SelectionChanged(
        object sender,
        SelectionChangedEventArgs e)
    {
        if (_settingGeneratedAttachmentSelection)
            return;

        _selectedGeneratedAttachmentComponents.Clear();
        foreach (GeneratedAttachmentListItem item in GeneratedAttachmentList
                     .SelectedItems
                     .OfType<GeneratedAttachmentListItem>())
        {
            _selectedGeneratedAttachmentComponents.Add(item.ComponentIndex);
        }
        UpdateGeneratedAttachmentEditorAvailability();
        RefreshPreview();
    }

    private void SelectSameMesh_Click(object sender, RoutedEventArgs e)
    {
        GeneratedAttachmentListItem[] selected = GeneratedAttachmentList
            .SelectedItems
            .OfType<GeneratedAttachmentListItem>()
            .ToArray();
        if (selected.Length == 0)
            return;

        HashSet<int> meshIndices = selected
            .SelectMany(item => item.Attachment.MeshIndices)
            .ToHashSet();
        _settingGeneratedAttachmentSelection = true;
        try
        {
            GeneratedAttachmentList.SelectedItems.Clear();
            foreach (GeneratedAttachmentListItem item in GeneratedAttachmentList.Items)
            {
                if (item.Attachment.MeshIndices.Any(meshIndices.Contains))
                    GeneratedAttachmentList.SelectedItems.Add(item);
            }
        }
        finally
        {
            _settingGeneratedAttachmentSelection = false;
        }
        SynchronizeGeneratedAttachmentSelectionFromList();
    }

    private void PinUpperBack_Click(object sender, RoutedEventArgs e) =>
        ApplyGeneratedAttachmentAssignment(
            GeneratedSkinningComponentAttachmentTarget.UpperBack);

    private void PinHead_Click(object sender, RoutedEventArgs e) =>
        ApplyGeneratedAttachmentAssignment(
            GeneratedSkinningComponentAttachmentTarget.Head);

    private void ResetAutomatic_Click(object sender, RoutedEventArgs e) =>
        ApplyGeneratedAttachmentAssignment(target: null);

    private void SynchronizeGeneratedAttachmentSelectionFromList()
    {
        _selectedGeneratedAttachmentComponents.Clear();
        foreach (GeneratedAttachmentListItem item in GeneratedAttachmentList
                     .SelectedItems
                     .OfType<GeneratedAttachmentListItem>())
        {
            _selectedGeneratedAttachmentComponents.Add(item.ComponentIndex);
        }
        UpdateGeneratedAttachmentEditorAvailability();
        RefreshPreview();
    }

    private void ApplyGeneratedAttachmentAssignment(
        GeneratedSkinningComponentAttachmentTarget? target)
    {
        if (_nativeValidationRunning ||
            !UsesGeneratedWeightsPortingMode ||
            !GeneratedSkinningPreparationIsCurrent ||
            RigFittingEditorHasPendingChanges ||
            _document is null ||
            _generatedDonorAlignment is null)
        {
            UpdateGeneratedAttachmentEditorAvailability();
            return;
        }

        GeneratedAttachmentListItem[] selected = GeneratedAttachmentList
            .SelectedItems
            .OfType<GeneratedAttachmentListItem>()
            .ToArray();
        if (selected.Length == 0)
            return;

        GeneratedSkinningPreparationResult previousPreparation =
            _generatedSkinningPreparation!;
        GeneratedSkinningComponentOverrides? previousOverrides =
            _generatedSkinningComponentOverrides;
        GlbSkinTransferPlan? previousTransferPlan = _glbSkinTransferPlan;
        MeshSplitPlan? previousPlan = _plan;
        string? previousIssue = _generatedSkinningPreparationIssue;
        long previousPreparationRevision = _generatedSkinningPreparationRevision;
        long previousViewedRevision = _generatedPreparedSceneViewedRevision;
        bool previousConfirmation = GeneratedSkinningIsConfirmed;
        try
        {
            var targetsByComponent = previousOverrides?.Components
                .ToDictionary(component => component.ComponentIndex,
                    component => component.Target) ?? [];
            foreach (GeneratedAttachmentListItem item in selected)
            {
                if (target is GeneratedSkinningComponentAttachmentTarget value)
                    targetsByComponent[item.ComponentIndex] = value;
                else
                    targetsByComponent.Remove(item.ComponentIndex);
            }

            GeneratedSkinningAnalysis currentAnalysis =
                previousPreparation.Analysis;
            Dictionary<int, GeneratedSkinningAttachment> attachmentsByComponent =
                currentAnalysis.Attachments.ToDictionary(
                    attachment => attachment.ComponentIndex);
            GeneratedSkinningComponentOverrides? candidateOverrides = null;
            if (targetsByComponent.Count > 0)
            {
                GeneratedSkinningComponentOverride[] components =
                    targetsByComponent
                        .OrderBy(pair => pair.Key)
                        .Select(pair =>
                        {
                            if (!attachmentsByComponent.TryGetValue(
                                    pair.Key,
                                    out GeneratedSkinningAttachment? attachment))
                            {
                                throw new InvalidOperationException(
                                    $"Компонент #{pair.Key} больше не существует " +
                                    "в текущей геометрии донора.");
                            }
                            return new GeneratedSkinningComponentOverride(
                                pair.Key,
                                pair.Value,
                                attachment.VerticesByMesh);
                        })
                        .ToArray();
                candidateOverrides = new GeneratedSkinningComponentOverrides(
                    components,
                    currentAnalysis.DonorComponentCount,
                    currentAnalysis.TargetRigFingerprint,
                    currentAnalysis.DonorGeometryFingerprint);
            }

            ImportedScene inputScene = ResolveGeneratedSkinningInputScene();
            TargetRigFittingPoseSnapshot fittingPose = CaptureRigFittingPose(
                localRotationsOnly: true);
            GeneratedSkinningPreparationResult candidatePreparation =
                PrepareGeneratedSkinning(
                    inputScene,
                    fittingPose,
                    _generatedDonorAlignment,
                    ResolveGeneratedBodySelection(
                        inputScene,
                        _generatedDonorAlignment),
                    candidateOverrides);
            GlbSkinTransferPlan candidateTransferPlan = AnalyzeGlbSkinTransfer(
                _document,
                candidatePreparation.PreparedScene,
                SelectedSkinnedTextureTransferMode);

            // Commit only after both Core preparation and the exact writer plan
            // have completed. A rejected/stale identity leaves the visible and
            // writable scene unchanged.
            ClearFinalTexturedPreview();
            _generatedSkinningComponentOverrides = candidateOverrides;
            _generatedSkinningPreparation = candidatePreparation;
            _generatedSkinningPreparationIssue = null;
            _generatedSkinningPreparationRevision = _rigFittingRevision;
            _generatedPreparedSceneViewedRevision = -1;
            _glbSkinTransferPlan = candidateTransferPlan;
            _plan = null;
            SetGeneratedSkinningConfirmation(false);
            PopulateGeneratedSkinningDiagnostics(candidatePreparation.Analysis);
            UpdateGeneratedSkinningCompatibilityPresentation();
            RefreshTextureList();
            RefreshState();

            string assignment = target switch
            {
                GeneratedSkinningComponentAttachmentTarget.UpperBack =>
                    "к верхней части спины",
                GeneratedSkinningComponentAttachmentTarget.Head => "к голове",
                _ => "в автоматический режим"
            };
            StatusText.Text =
                $"Деталей {assignment}: {selected.Length}. Веса и план пересчитаны.";
        }
        catch (Exception exception) when (exception is InvalidDataException or
                                          InvalidOperationException or
                                          OverflowException or
                                          ArgumentException or
                                          NotSupportedException)
        {
            _generatedSkinningPreparation = previousPreparation;
            _generatedSkinningComponentOverrides = previousOverrides;
            _glbSkinTransferPlan = previousTransferPlan;
            _plan = previousPlan;
            _generatedSkinningPreparationIssue = previousIssue;
            _generatedSkinningPreparationRevision = previousPreparationRevision;
            _generatedPreparedSceneViewedRevision = previousViewedRevision;
            SetGeneratedSkinningConfirmation(previousConfirmation);
            GeneratedAttachmentStatusText.Text =
                "Назначение не применено: " + exception.Message;
            StatusText.Text = GeneratedAttachmentStatusText.Text;
        }
    }

    private static bool IsGeneratedAttachmentEchoWarning(string warning) =>
        warning.StartsWith("Detached component ", StringComparison.Ordinal) &&
        warning.Contains(" was kept rigid on ", StringComparison.Ordinal);

    private void SetGeneratedSkinningFailure(string message, bool isError = true)
    {
        _generatedSkinningPreparation = null;
        _generatedSkinningPreparationRevision = -1;
        _generatedPreparedSceneViewedRevision = -1;
        _glbSkinTransferPlan = null;
        _generatedSkinningPreparationIssue = message;
        SetGeneratedSkinningConfirmation(false);
        UpdateGeneratedAlignmentText(_generatedDonorAlignment);
        GeneratedSkinningSummaryText.Text = "Подготовленная сцена не создана.";
        GeneratedSkinningWarningsText.Text = "• " + message;
        GeneratedSkinningAttachmentsText.Text = "Не построены.";
        GeneratedSkinningDetailsExpander.Header =
            "Технические детали: подготовка не завершена";
        UpdateGeneratedAttachmentEditorAvailability();
        UpdateGeneratedSkinningConfirmationAvailability();
        UpdateGeneratedSkinningPrimaryStatus();
        CompatibilityText.Text = _generatedDonorAlignment is null
            ? message
            : message +
              "\nРазмер и положение модели сохранены. Исправьте их вручную при " +
              "необходимости; raw preview остаётся доступен, но plan/save заблокированы.";
        ReplacementModePanel.Background = new SolidColorBrush(
            isError ? Color.FromRgb(254, 226, 226) : Color.FromRgb(255, 243, 205));
        StatusText.Text = message;
    }

    private GlbSkinTransferPlan AnalyzeGlbSkinTransfer(
        SmoDocument target,
        ImportedScene donor,
        SkinnedTextureTransferMode textureMode,
        SkinnedRenderableMaterialProfile? materialProfile = null)
    {
        materialProfile ??= ResolveSkinnedMaterialProfile(donor, textureMode);
        GlbSkinTransferPlan plan = SmoSkinnedGlbReplacer.Analyze(
            target,
            donor,
            textureMode,
            materialProfile);
        if (textureMode == SkinnedTextureTransferMode.PreserveTarget)
            return plan;

        string[] unresolvedMaterials = GetUnresolvedTextureMaterialDescriptions(donor);
        if (unresolvedMaterials.Length == 0)
            return plan;

        return plan with
        {
            Compatibility = SmoSkeletonCompatibility.Incompatible,
            Messages = plan.Messages.Concat(new[]
            {
                "Не найдены изображения для material references: " +
                string.Join(", ", unresolvedMaterials) + ". Добавьте файлы текстур."
            }).ToArray()
        };
    }

    private void AppendGlbSkinTransferPlanMessages(GlbSkinTransferPlan plan)
    {
        if (plan.Messages.Count == 0)
            return;
        if (!string.IsNullOrWhiteSpace(CompatibilityText.Text))
            CompatibilityText.Text += "\n";
        CompatibilityText.Text += string.Join(
            "\n",
            plan.Messages.Select(message => "• " + message));
    }

    private static string[] GetUnresolvedTextureMaterialDescriptions(
        ImportedScene scene) =>
        scene.Meshes
            .Where(mesh => mesh.MaterialIndex >= 0 &&
                mesh.MaterialIndex < scene.Materials.Count)
            .Select(mesh => scene.Materials[mesh.MaterialIndex])
            .Where(material => material.BaseColorTextureIndex < 0 &&
                !string.IsNullOrWhiteSpace(material.BaseColorTextureName))
            .Select(material => $"{material.Name} → {material.BaseColorTextureName}")
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .ToArray();

    private void PopulateGlbBoneMappingTree(GlbSkinTransferPlan plan)
    {
        BoneMappingTree.Items.Clear();
        BoneMappingPanel.Visibility = Visibility.Visible;
        var matched = new TreeViewItem
        {
            Header = $"Точное совпадение: {plan.MatchedBoneNames.Count}"
        };
        foreach (string name in plan.MatchedBoneNames)
            matched.Items.Add(new TreeViewItem { Header = name });
        BoneMappingTree.Items.Add(matched);

        var remapped = new TreeViewItem
        {
            Header = $"Перенаправлены: {plan.RemappedBones.Count}",
            Foreground = new SolidColorBrush(Color.FromRgb(180, 83, 9)),
            IsExpanded = plan.RemappedBones.Count > 0
        };
        foreach (GlbBoneRemap mapping in plan.RemappedBones)
            remapped.Items.Add(new TreeViewItem
            {
                Header = $"{mapping.DonorBoneName} → {mapping.TargetBoneName} — {mapping.Reason}"
            });
        if (plan.RemappedBones.Count == 0)
            remapped.Items.Add(new TreeViewItem { Header = "Нет" });
        BoneMappingTree.Items.Add(remapped);

        var unused = new TreeViewItem
        {
            Header = $"GLB joints без weights: {plan.UnusedGlbJoints.Count}"
        };
        foreach (string name in plan.UnusedGlbJoints)
            unused.Items.Add(new TreeViewItem { Header = name });
        BoneMappingTree.Items.Add(unused);

        var targetUnused = new TreeViewItem
        {
            Header = $"Target bones без новых weights: {plan.TargetBonesWithoutWeights.Count}"
        };
        foreach (string name in plan.TargetBonesWithoutWeights)
            targetUnused.Items.Add(new TreeViewItem { Header = name });
        BoneMappingTree.Items.Add(targetUnused);
    }

    private void UpdateSmoReplacementPlan()
    {
        if (_document is null || _replacementSmoDocument is null)
        {
            _smoReplacementPlan = null;
            CompatibilityText.Text = "Сначала выберите целевой SMO.";
            StatusText.Text = "SMO-донор загружен; требуется целевой SMO.";
            return;
        }

        _smoReplacementPlan = SmoToSmoReplacer.Analyze(
            _document, _replacementSmoDocument);
        PopulateBoneMappingTree(_smoReplacementPlan);
        string summary =
            $"Скелет: {_smoReplacementPlan.TargetBoneCount} → " +
            $"{_smoReplacementPlan.DonorBoneCount} костей; meshes: " +
            $"{_smoReplacementPlan.TargetMeshCount} → {_smoReplacementPlan.DonorMeshCount}; " +
            $"textures: {_smoReplacementPlan.TargetTextureCount} → " +
            $"{_smoReplacementPlan.DonorTextureCount}.";
        string details = _smoReplacementPlan.Messages.Count == 0
            ? string.Empty
            : "\n" + string.Join("\n", _smoReplacementPlan.Messages.Select(
                message => "• " + message));
        CompatibilityText.Text = summary + details;

        switch (_smoReplacementPlan.Compatibility)
        {
            case SmoSkeletonCompatibility.Exact:
                ReplacementModePanel.Background = new SolidColorBrush(
                    Color.FromRgb(220, 252, 231));
                StatusText.Text =
                    "Скелет SMO-донора совпадает. Можно упаковать его visual branches в target-контейнер.";
                break;
            case SmoSkeletonCompatibility.CompatibleWithWarnings:
                ReplacementModePanel.Background = new SolidColorBrush(
                    Color.FromRgb(255, 243, 205));
                StatusText.Text =
                    "SMO-донор совместим с предупреждениями; проверьте список изменений.";
                break;
            default:
                ReplacementModePanel.Background = new SolidColorBrush(
                    Color.FromRgb(254, 226, 226));
                StatusText.Text =
                    "Подмена заблокирована: скелеты SMO несовместимы.";
                break;
        }
    }

    private void PopulateBoneMappingTree(SmoToSmoReplacementPlan plan)
    {
        BoneMappingTree.Items.Clear();
        BoneMappingPanel.Visibility = Visibility.Visible;

        var matched = new TreeViewItem
        {
            Header = $"Совпали по имени: {plan.MatchedBoneNames.Count}",
            IsExpanded = false
        };
        foreach (string name in plan.MatchedBoneNames)
            matched.Items.Add(new TreeViewItem { Header = name });
        BoneMappingTree.Items.Add(matched);

        var ignored = new TreeViewItem
        {
            Header = $"Дополнительные кости донора: {plan.IgnoredDonorBones.Count}",
            Foreground = new SolidColorBrush(Color.FromRgb(180, 83, 9)),
            IsExpanded = plan.IgnoredDonorBones.Count > 0
        };
        foreach (SmoIgnoredDonorBone mapping in plan.IgnoredDonorBones)
        {
            ignored.Items.Add(new TreeViewItem
            {
                Header = $"{mapping.DonorBoneName} → {mapping.TargetBoneName} — отдельная локальная анимация теряется"
            });
        }
        if (plan.IgnoredDonorBones.Count == 0)
            ignored.Items.Add(new TreeViewItem { Header = "Нет" });
        BoneMappingTree.Items.Add(ignored);

        var unbound = new TreeViewItem
        {
            Header = $"Нет весов у донора: {plan.UnboundTargetBones.Count}",
            Foreground = new SolidColorBrush(Color.FromRgb(185, 28, 28)),
            IsExpanded = plan.UnboundTargetBones.Count > 0
        };
        foreach (string name in plan.UnboundTargetBones)
        {
            unbound.Items.Add(new TreeViewItem
            {
                Header = $"{name} — останется без новой привязки"
            });
        }
        if (plan.UnboundTargetBones.Count == 0)
            unbound.Items.Add(new TreeViewItem { Header = "Нет" });
        BoneMappingTree.Items.Add(unbound);

        var hierarchy = new TreeViewItem
        {
            Header = $"Обойдены helper/control nodes: {plan.HierarchyAdaptations.Count}",
            Foreground = new SolidColorBrush(Color.FromRgb(3, 105, 161)),
            IsExpanded = plan.HierarchyAdaptations.Count > 0
        };
        foreach (SmoHierarchyAdaptation adaptation in plan.HierarchyAdaptations)
        {
            var bone = new TreeViewItem { Header = adaptation.BoneName };
            bone.Items.Add(new TreeViewItem
            {
                Header = $"Target: {adaptation.TargetPath}"
            });
            bone.Items.Add(new TreeViewItem
            {
                Header = $"Donor: {adaptation.DonorPath}"
            });
            hierarchy.Items.Add(bone);
        }
        if (plan.HierarchyAdaptations.Count == 0)
            hierarchy.Items.Add(new TreeViewItem { Header = "Нет" });
        BoneMappingTree.Items.Add(hierarchy);
    }

    private void Plan_Click(object sender, RoutedEventArgs e)
    {
        if (_document is null || _replacementScene is null) return;
        if (RigFittingEditorHasPendingChanges)
        {
            _plan = null;
            const string pendingMessage =
                "Сначала нажмите «Применить» в редакторе подгонки.";
            PlanSummaryText.Text = pendingMessage;
            StatusText.Text = pendingMessage;
            RefreshState();
            return;
        }
        if (!CanRunSelectedPortingPipeline)
        {
            _plan = null;
            _rigidMultiMaterialAnalysis = null;
            string message = GetPortingModeBlockMessage() ??
                "Выбранный режим нельзя применить к этой модели.";
            PlanSummaryText.Text = message;
            StatusText.Text = message;
            RefreshState();
            return;
        }
        if (UsesGeneratedWeightsPortingMode && !GeneratedSkinningIsReady)
        {
            _plan = null;
            string message = !GeneratedSkinningPreparationIsCurrent
                ? _generatedSkinningPreparationIssue ??
                  "Автоматические веса ещё не подготовлены."
                : "Сначала проверьте предпросмотр и явно подтвердите созданные веса.";
            PlanSummaryText.Text = message;
            StatusText.Text = message;
            RefreshState();
            return;
        }
        if (UsesAdaptDonorWeightsPortingMode &&
            !AdaptedPortingPreparationIsCurrent)
        {
            _plan = null;
            string message = _adaptedPortingPreparationIssue ??
                "Адаптация весов ещё не подготовлена.";
            PlanSummaryText.Text = message;
            StatusText.Text = message;
            RefreshState();
            return;
        }
        try
        {
            bool preserveOriginalTextures = PreserveOriginalTextures;
            bool multiTextureMode = UseRigidMultiTextureMode;
            if (!preserveOriginalTextures &&
                !string.IsNullOrWhiteSpace(_rigidTextureBindingIssue))
            {
                throw new InvalidOperationException(
                    "Текстуры модели нельзя безопасно сопоставить: " +
                    _rigidTextureBindingIssue +
                    " Добавьте недостающие изображения либо включите " +
                    "«Оставить текстуры исходного SMO».");
            }
            if (!preserveOriginalTextures &&
                (_replacementRigidTextureBundle is null ||
                 UsesGeneratedWeightsPortingMode))
            {
                ImportedScene textureSourceScene = UsesGeneratedWeightsPortingMode
                    ? _generatedSkinningEffectiveScene ??
                      (GeneratedSkinningPreparationIsCurrent
                          ? _generatedSkinningPreparation!.PreparedScene
                          : null) ??
                      _replacementScene
                    : _replacementScene;
                string[] unresolvedTextures =
                    GetUnresolvedTextureMaterialDescriptions(textureSourceScene);
                if (unresolvedTextures.Length > 0)
                {
                    throw new InvalidOperationException(
                        "Добавьте изображения для material references: " +
                        string.Join(", ", unresolvedTextures) + ".");
                }
            }
            if (multiTextureMode)
            {
                _plan = null;
                _rigidMultiMaterialAnalysis = SmoRigidMultiMaterialPacker.Analyze(
                    _document, _replacementRigidTextureBundle!);
                string details = string.Join(
                    "\n",
                    _rigidMultiMaterialAnalysis.Messages.Select(message => "• " + message));
                if (!_rigidMultiMaterialAnalysis.CanPack)
                {
                    PlanSummaryText.Text = "Проверка multi-texture структуры не пройдена." +
                        (details.Length == 0 ? string.Empty : "\n" + details);
                    StatusText.Text =
                        "Multi-texture SMO создать нельзя: исправьте указанную несовместимость.";
                    RefreshState();
                    return;
                }

                BoneItem? head = BoneCombo.Items.OfType<BoneItem>()
                    .FirstOrDefault(item => item.Slot == _rigidMultiMaterialAnalysis.RigidBoneSlot);
                if (head is not null)
                    BoneCombo.SelectedItem = head;
                PlanSummaryText.Text =
                    $"{_rigidMultiMaterialAnalysis.MaterialGroupCount} material branches; " +
                    $"{_rigidMultiMaterialAnalysis.MeshCount} meshes; " +
                    $"{_rigidMultiMaterialAnalysis.TextureCount} texture frames; " +
                    $"{_rigidMultiMaterialAnalysis.SequenceCount} sequences; " +
                    $"rigid palette slot {_rigidMultiMaterialAnalysis.RigidBoneSlot} → " +
                    $"{_rigidMultiMaterialAnalysis.RigidBoneName}." +
                    (details.Length == 0 ? string.Empty : "\n" + details);
                StatusText.Text =
                    "Multi-texture структура проверена writer-ом. Можно создать новый SMO.";
                RefreshState();
                return;
            }
            if (UsesPreparedModelPortingMode ||
                UsesAdaptDonorWeightsPortingMode ||
                UsesGeneratedWeightsPortingMode)
            {
                ImportedScene skinnedScene;
                SkinnedGeometryTransferMode geometryMode;
                if (UsesGeneratedWeightsPortingMode)
                {
                    skinnedScene = GeneratedSkinningPreparationIsCurrent
                        ? _generatedSkinningPreparation!.PreparedScene
                        :
                        throw new InvalidOperationException(
                            _generatedSkinningPreparationIssue ??
                            "Автоматические веса не подготовлены.");
                    geometryMode = SkinnedGeometryTransferMode.PreservePreparedGeometry;
                }
                else if (UsesAdaptDonorWeightsPortingMode)
                {
                    skinnedScene = AdaptedPortingPreparationIsCurrent
                        ? _adaptedPortingPreparation!.PreparedScene
                        :
                        throw new InvalidOperationException(
                            _adaptedPortingPreparationIssue ??
                            "Адаптация весов не подготовлена.");
                    geometryMode = SkinnedGeometryTransferMode.PreservePreparedGeometry;
                }
                else
                {
                    skinnedScene = _replacementScene;
                    geometryMode = RebaseBindPoseCheckBox.IsChecked == true
                        ? SkinnedGeometryTransferMode.RetargetToGameBindPose
                        : SkinnedGeometryTransferMode.PreservePreparedGeometry;
                }
                _glbSkinTransferPlan = AnalyzeGlbSkinTransfer(
                    _document,
                    skinnedScene,
                    SelectedSkinnedTextureTransferMode);
                if (UsesAdaptDonorWeightsPortingMode)
                    PopulateAdaptedBoneMappingTree(
                        _adaptedPortingPreparation!.Analysis);
                else if (UsesPreparedModelPortingMode)
                    PopulateGlbBoneMappingTree(_glbSkinTransferPlan);
                if (!_glbSkinTransferPlan.CanReplace)
                    throw new InvalidOperationException(
                        "Skinned GLB несовместим:\n" +
                        string.Join("\n", _glbSkinTransferPlan.Messages.Select(
                            message => "• " + message)));
                _plan = MeshSplitter.Split(skinnedScene);
                PlanSummaryText.Text =
                    $"{skinnedScene.Meshes.Sum(mesh => mesh.Positions.Length):N0} vertices, " +
                    $"{skinnedScene.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3):N0} triangles; " +
                    $"{_glbSkinTransferPlan.ActiveJointCount} active joints → " +
                    $"{_glbSkinTransferPlan.MaterialGroupCount} material groups. " +
                    (UsesAdaptDonorWeightsPortingMode
                        ? "Donor weights адаптированы; geometry уже находится в bind pose игры. "
                        : UsesGeneratedWeightsPortingMode
                            ? "Geometry выровнена, веса созданы и явно подтверждены; prepared geometry используется без повторной трансформации. "
                        : string.Empty) +
                    "16-bone palettes построены без изменения target graph." +
                    (preserveOriginalTextures
                        ? " Все TextureData исходного SMO останутся без изменений."
                        : string.Empty);
                StatusText.Text =
                    $"Skinned-план проверен ({geometryMode}). Можно создать экспериментальный SMO.";
                RefreshState();
                return;
            }
            int slots = _document.Objects.Count(entry => entry.TypeHash == SmoClassIds.MeshData);
            ImportedMesh combined = ImportedMeshCombiner.Combine(_replacementScene);
            _plan = MeshSplitter.Split(_replacementScene);
            if (_plan.Chunks.Count != 1)
                throw new InvalidOperationException("Модель превышает 65 535 уникальных вершин и пока требует умного пространственного разбиения.");
            PlanSummaryText.Text = $"{combined.Positions.Length:N0} vertices и {combined.TriangleIndices.Length / 3:N0} triangles → " +
                $"1 цельный rigid body-slot; ещё {slots - 1} slots получат невидимый degenerate triangle." +
                (preserveOriginalTextures
                    ? " Текстура исходного body-slot останется без изменений."
                    : string.Empty);
            StatusText.Text = "План проверен. Можно создать экспериментальный SMO.";
            RefreshState();
        }
        catch (Exception exception)
        {
            _plan = null;
            _rigidMultiMaterialAnalysis = null;
            RefreshState();
            ShowError(exception);
        }
    }

    private async void Save_Click(object sender, RoutedEventArgs e)
    {
        if (_nativeValidationRunning || _document is null || _sourcePath is null)
            return;
        bool smoMode = _replacementSmoDocument is not null;
        bool preserveOriginalTextures = !smoMode && PreserveOriginalTextures;
        bool multiTextureMode = !smoMode && UseRigidMultiTextureMode;
        bool skinnedGlbMode = !smoMode &&
            (((UsesPreparedModelPortingMode || UsesAdaptDonorWeightsPortingMode) &&
              AllReplacementMeshesAreSkinned) ||
             (UsesGeneratedWeightsPortingMode &&
              GeneratedSkinningPreparationIsCurrent));
        if (smoMode && _smoReplacementPlan?.CanReplace != true) return;
        if (!smoMode && !CanRunSelectedPortingPipeline)
        {
            StatusText.Text = GetPortingModeBlockMessage() ??
                "Выбранный режим нельзя применить к этой модели.";
            return;
        }
        if (!smoMode && RigFittingEditorHasPendingChanges)
        {
            StatusText.Text =
                "Сначала нажмите «Применить» в редакторе подгонки.";
            return;
        }
        if (!smoMode && UsesGeneratedWeightsPortingMode &&
            !GeneratedSkinningIsReady)
        {
            StatusText.Text = !GeneratedSkinningPreparationIsCurrent
                ? _generatedSkinningPreparationIssue ??
                  "Автоматические веса ещё не подготовлены."
                : "Сначала проверьте предпросмотр и явно подтвердите созданные веса.";
            return;
        }
        if (!smoMode && UsesAdaptDonorWeightsPortingMode &&
            !AdaptedPortingPreparationIsCurrent)
        {
            StatusText.Text = _adaptedPortingPreparationIssue ??
                "Адаптация весов ещё не подготовлена.";
            return;
        }
        if (multiTextureMode && _rigidMultiMaterialAnalysis?.CanPack != true) return;
        if (!smoMode && !multiTextureMode &&
            (_replacementScene is null || _plan is null)) return;
        string donorStem = _replacementPath is null
            ? "replacement"
            : Path.GetFileNameWithoutExtension(_replacementPath);
        var dialog = new SaveFileDialog
        {
            Filter = "Sparkplug model (*.smo)|*.smo",
            FileName = smoMode
                ? Path.GetFileNameWithoutExtension(_sourcePath) + "_from_" + donorStem + ".smo"
                : multiTextureMode
                    ? Path.GetFileNameWithoutExtension(_sourcePath) + "_multitexture_" + donorStem + ".smo"
                : skinnedGlbMode
                    ? Path.GetFileNameWithoutExtension(_sourcePath) + "_skinned_" + donorStem + ".smo"
                    : Path.GetFileNameWithoutExtension(_sourcePath) + "_whole_replaced.smo",
            InitialDirectory = Path.GetDirectoryName(_sourcePath)
        };
        if (dialog.ShowDialog(this) != true) return;
        NativeValidationResultBorder.Visibility = Visibility.Collapsed;
        try
        {
            string fullOutputPath = Path.GetFullPath(dialog.FileName);
            EnsureSeparateOutput(fullOutputPath, _sourcePath, "исходный SMO");
            EnsureSeparateOutput(fullOutputPath, _replacementPath, "файл-донор");
            string outputPath;
            if (smoMode)
            {
                SmoToSmoReplacementResult smoResult = SmoToSmoReplacer.Replace(
                    _document, _replacementSmoDocument!, fullOutputPath);
                outputPath = smoResult.OutputPath;
            }
            else if (multiTextureMode)
            {
                SmoRigidMultiMaterialPackResult packed =
                    SmoRigidMultiMaterialPacker.Pack(
                        _document,
                        _replacementRigidTextureBundle!,
                        ReadTransform(),
                        fullOutputPath);
                outputPath = packed.OutputPath;
            }
            else if (skinnedGlbMode)
            {
                ImportedScene skinnedScene;
                SkinnedGeometryTransferMode geometryMode;
                if (UsesGeneratedWeightsPortingMode)
                {
                    skinnedScene = GeneratedSkinningPreparationIsCurrent
                        ? _generatedSkinningPreparation!.PreparedScene
                        :
                        throw new InvalidOperationException(
                            _generatedSkinningPreparationIssue ??
                            "Автоматические веса не подготовлены.");
                    geometryMode = SkinnedGeometryTransferMode.PreservePreparedGeometry;
                }
                else if (UsesAdaptDonorWeightsPortingMode)
                {
                    skinnedScene = AdaptedPortingPreparationIsCurrent
                        ? _adaptedPortingPreparation!.PreparedScene
                        :
                        throw new InvalidOperationException(
                            _adaptedPortingPreparationIssue ??
                            "Адаптация весов не подготовлена.");
                    geometryMode = SkinnedGeometryTransferMode.PreservePreparedGeometry;
                }
                else
                {
                    skinnedScene = _replacementScene!;
                    geometryMode = RebaseBindPoseCheckBox.IsChecked == true
                        ? SkinnedGeometryTransferMode.RetargetToGameBindPose
                        : SkinnedGeometryTransferMode.PreservePreparedGeometry;
                }
                SkinnedRenderableMaterialProfile materialProfile =
                    ResolveSkinnedMaterialProfile(
                        skinnedScene,
                        SelectedSkinnedTextureTransferMode);
                ImportedTexture? legacyTexture = preserveOriginalTextures
                    ? null
                    : ResolveLegacySkinnedTextureOverride(
                        skinnedScene,
                        UsesGeneratedWeightsPortingMode
                            ? _generatedSkinningTextureCatalog
                            : _textureCatalogResult);
                if (materialProfile.HasExplicitOverrides)
                    legacyTexture = null;
                GlbSkinTransferResult skinResult = SmoSkinnedGlbReplacer.Replace(
                    _document,
                    skinnedScene,
                    ReplacementTransform.Identity,
                    fullOutputPath,
                    geometryMode,
                    texture: legacyTexture,
                    textureMode: SelectedSkinnedTextureTransferMode,
                    materialProfile: materialProfile);
                outputPath = skinResult.OutputPath;
            }
            else
            {
                WholeModelReplacementResult result = SmoWholeModelReplacer.Replace(
                    _document, _replacementScene!, ReadTransform(), fullOutputPath,
                    BoneCombo.SelectedItem is BoneItem bone ? bone.Slot : 0,
                    texturePath: null,
                    embeddedTexture: preserveOriginalTextures
                        ? null
                        : ResolveRigidSingleTexture());
                outputPath = result.OutputPath;
            }

            await ValidateSavedModelAsync(outputPath, _sourcePath);
        }
        catch (Exception exception) { ShowError(exception); }
    }

    private static void EnsureSeparateOutput(
        string outputPath,
        string? inputPath,
        string inputDescription)
    {
        if (inputPath is not null && string.Equals(
                Path.GetFullPath(inputPath),
                outputPath,
                StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException(
                $"Результат нельзя записать поверх {inputDescription}; выберите новый файл.");
        }
    }

    private async void MainWindow_Loaded(object sender, RoutedEventArgs e) =>
        await LocateGameExecutableAsync();

    private void MainWindow_Closing(object? sender, CancelEventArgs e)
    {
        _isClosing = true;
        try
        {
            _nativeValidationCancellation?.Cancel();
        }
        catch (ObjectDisposedException)
        {
        }
    }

    private void ChooseGameExecutable_Click(object sender, RoutedEventArgs e)
    {
        if (_nativeValidationRunning)
            return;

        var dialog = new OpenFileDialog
        {
            Filter = "Winx Club (*.exe)|*.exe",
            CheckFileExists = true,
            FileName = ResolveGameExecutablePath() ?? "WinxClub.exe"
        };
        if (dialog.ShowDialog(this) != true)
            return;

        SetGameExecutablePath(dialog.FileName);
        _nativeValidator.SaveManualExecutablePath(dialog.FileName);
    }

    private void GameExecutablePathBox_TextChanged(
        object sender,
        TextChangedEventArgs e)
    {
        if (_settingNativeExecutablePath || _nativeValidationRunning ||
            NativeValidationResultBorder is null)
        {
            return;
        }

        NativeValidationResultBorder.Visibility = Visibility.Collapsed;
    }

    private void GameExecutablePathBox_LostKeyboardFocus(
        object sender,
        KeyboardFocusChangedEventArgs e)
    {
        string? executablePath = ResolveGameExecutablePath();
        if (executablePath is null)
            return;

        SetGameExecutablePath(executablePath);
        _nativeValidator.SaveManualExecutablePath(executablePath);
    }

    private async Task LocateGameExecutableAsync()
    {
        if (_nativeValidationRunning || _isClosing)
            return;

        string? executableBeforeSearch = ResolveGameExecutablePath();
        try
        {
            string? located = await _nativeValidator.LocateExecutableAsync(
                _sourcePath);
            string? executableAfterSearch = ResolveGameExecutablePath();
            bool pathWasChangedWhileSearching =
                executableAfterSearch is not null &&
                !string.Equals(
                    executableAfterSearch,
                    executableBeforeSearch,
                    StringComparison.OrdinalIgnoreCase);
            if (!_isClosing && located is not null && !pathWasChangedWhileSearching)
                SetGameExecutablePath(located);
        }
        catch (OperationCanceledException)
        {
        }
        catch (Exception exception) when (
            exception is IOException or UnauthorizedAccessException or
                System.Security.SecurityException)
        {
            // Autodiscovery is optional; a path can still be entered manually.
        }
    }

    private async Task ValidateSavedModelAsync(
        string outputPath,
        string sourcePath)
    {
        string? executablePath = ResolveGameExecutablePath();
        if (executablePath is null)
        {
            await LocateGameExecutableAsync();
            executablePath = ResolveGameExecutablePath();
        }

        if (executablePath is null)
        {
            SetNativeValidationResult(
                ImporterNativeVerdict.Indeterminate,
                "Проверка не выполнена — выберите WinxClub.exe.");
            StatusText.Text = $"SMO сохранён: {outputPath}";
            return;
        }

        SetGameExecutablePath(executablePath);
        _nativeValidator.SaveManualExecutablePath(executablePath);
        _nativeValidationRunning = true;
        ControlsScrollViewer.IsEnabled = false;
        SaveButton.IsEnabled = false;
        SetNativeValidationProgress("Проверяем модель в игре…");
        StatusText.Text = "SMO сохранён. Идёт автоматическая проверка…";

        CancellationTokenSource runCancellation = new();
        _nativeValidationCancellation = runCancellation;
        try
        {
            ImporterNativeValidationResult result =
                await _nativeValidator.ValidateAsync(
                    executablePath,
                    outputPath,
                    InferLogicalGameAssetPath(sourcePath),
                    runCancellation.Token);
            if (!_isClosing)
            {
                SetNativeValidationResult(result.Verdict, result.Message);
                StatusText.Text = $"SMO сохранён: {outputPath}";
            }
        }
        catch (OperationCanceledException)
        {
            if (!_isClosing)
            {
                SetNativeValidationResult(
                    ImporterNativeVerdict.Indeterminate,
                    "Проверка отменена.");
                StatusText.Text = $"SMO сохранён: {outputPath}";
            }
        }
        catch (Exception)
        {
            if (!_isClosing)
            {
                SetNativeValidationResult(
                    ImporterNativeVerdict.Indeterminate,
                    "Проверка не выполнена — WinxClub.exe не удалось запустить.");
                StatusText.Text = $"SMO сохранён: {outputPath}";
            }
        }
        finally
        {
            runCancellation.Dispose();
            if (ReferenceEquals(_nativeValidationCancellation, runCancellation))
                _nativeValidationCancellation = null;
            _nativeValidationRunning = false;
            if (!_isClosing)
            {
                ControlsScrollViewer.IsEnabled = true;
                RefreshState();
            }
        }
    }

    private void SetGameExecutablePath(string path)
    {
        string normalized;
        try
        {
            normalized = Path.GetFullPath(path.Trim().Trim('"'));
        }
        catch (Exception exception) when (
            exception is ArgumentException or NotSupportedException or
                PathTooLongException)
        {
            return;
        }

        _settingNativeExecutablePath = true;
        try
        {
            GameExecutablePathBox.Text = normalized;
            GameExecutablePathBox.CaretIndex = GameExecutablePathBox.Text.Length;
        }
        finally
        {
            _settingNativeExecutablePath = false;
        }
    }

    private string? ResolveGameExecutablePath()
    {
        string value = GameExecutablePathBox.Text.Trim().Trim('"');
        if (string.IsNullOrWhiteSpace(value))
            return null;

        try
        {
            string fullPath = Path.GetFullPath(value);
            return File.Exists(fullPath) &&
                Path.GetExtension(fullPath).Equals(
                    ".exe", StringComparison.OrdinalIgnoreCase)
                ? fullPath
                : null;
        }
        catch (Exception exception) when (
            exception is ArgumentException or NotSupportedException or
                PathTooLongException)
        {
            return null;
        }
    }

    private void SetNativeValidationProgress(string message)
    {
        NativeValidationResultBorder.Background = new SolidColorBrush(
            Color.FromRgb(224, 242, 254));
        NativeValidationResultBorder.BorderBrush = new SolidColorBrush(
            Color.FromRgb(14, 165, 233));
        NativeValidationResultBorder.BorderThickness = new Thickness(1);
        NativeValidationResultText.Foreground = new SolidColorBrush(
            Color.FromRgb(3, 105, 161));
        NativeValidationResultText.Text = message;
        NativeValidationResultBorder.Visibility = Visibility.Visible;
    }

    private void SetNativeValidationResult(
        ImporterNativeVerdict verdict,
        string message)
    {
        (Color background, Color border, Color text) = verdict switch
        {
            ImporterNativeVerdict.Suitable => (
                Color.FromRgb(220, 252, 231),
                Color.FromRgb(34, 197, 94),
                Color.FromRgb(21, 128, 61)),
            ImporterNativeVerdict.Unsuitable => (
                Color.FromRgb(254, 226, 226),
                Color.FromRgb(239, 68, 68),
                Color.FromRgb(185, 28, 28)),
            _ => (
                Color.FromRgb(254, 243, 199),
                Color.FromRgb(245, 158, 11),
                Color.FromRgb(146, 64, 14))
        };
        NativeValidationResultBorder.Background = new SolidColorBrush(background);
        NativeValidationResultBorder.BorderBrush = new SolidColorBrush(border);
        NativeValidationResultBorder.BorderThickness = new Thickness(1);
        NativeValidationResultText.Foreground = new SolidColorBrush(text);
        NativeValidationResultText.Text = message;
        NativeValidationResultBorder.Visibility = Visibility.Visible;
        NativeValidationResultBorder.BringIntoView();
    }

    private static string InferLogicalGameAssetPath(string sourcePath)
    {
        string fullPath = Path.GetFullPath(sourcePath);
        string separator = Path.DirectorySeparatorChar.ToString();
        string mediaMarker = $"{separator}Media{separator}";
        int mediaIndex = fullPath.LastIndexOf(
            mediaMarker, StringComparison.OrdinalIgnoreCase);
        return mediaIndex >= 0
            ? fullPath[(mediaIndex + mediaMarker.Length)..].Replace('/', '\\')
            : Path.GetFileName(fullPath);
    }

    private void RefreshTextureList()
    {
        var items = new List<TextureResourceItem>();
        if (_replacementSmoDocument is not null)
        {
            int textureCount = _replacementSmoDocument.Objects.Count(entry =>
                entry.TypeHash == SmoClassIds.TextureData);
            TextureList.ItemsSource = items;
            TextureSummaryText.Text =
                $"SMO-донор содержит {textureCount} texture slots; они переносятся автоматически.";
            UpdateRemoveTexturesAvailability();
            UpdateMaterialOverrideAvailability();
            return;
        }
        bool generatedWeightsMode = UsesGeneratedWeightsPortingMode;
        ImportedScene? textureScene = generatedWeightsMode
            ? _generatedSkinningEffectiveScene ??
              _textureCatalogResult?.EffectiveScene ??
              _baseReplacementScene
            : _replacementScene;
        ImportedTextureCatalogResult? textureCatalog = generatedWeightsMode
            ? _generatedSkinningTextureCatalog ?? _textureCatalogResult
            : _textureCatalogResult;
        if (textureScene is null)
        {
            TextureList.ItemsSource = items;
            TextureSummaryText.Text =
                "Доступные текстуры появятся после выбора модели.";
            UpdateRemoveTexturesAvailability();
            UpdateMaterialOverrideAvailability();
            return;
        }

        if (_multiTextureDirectory is not null &&
            _replacementRigidTextureBundle is not null &&
            !generatedWeightsMode)
        {
            foreach (RigidMaterialGroup group in
                     _replacementRigidTextureBundle.MaterialGroups)
            {
                foreach (RigidTextureFrame frame in group.Frames)
                {
                    items.Add(new TextureResourceItem(
                        $"{group.Name} · кадр {frame.FrameNumber}",
                        $"{frame.Texture.Width}×{frame.Texture.Height} · " +
                        $"{Path.GetFileName(frame.SourcePath)}",
                        ExternalPath: null,
                        CanRemove: true,
                        RemovesFolder: true));
                }
            }
            TextureList.ItemsSource = items;
            TextureSummaryText.Text =
                $"Папка: {_multiTextureDirectory}. Материалов: " +
                $"{_replacementRigidTextureBundle.MaterialGroups.Count}; кадров: " +
                $"{items.Count}." + (_externalTextures.Count == 0
                    ? string.Empty
                    : $" Ранее добавленных файлов временно не используется: " +
                      $"{_externalTextures.Count}.");
            UpdateRemoveTexturesAvailability();
            UpdateMaterialOverrideAvailability();
            return;
        }

        HashSet<string> manuallyAddedPaths = _externalTextures
            .Select(texture => texture.SourcePath)
            .Where(path => !string.IsNullOrWhiteSpace(path))
            .Select(path => Path.GetFullPath(path!))
            .ToHashSet(StringComparer.OrdinalIgnoreCase);
        HashSet<int> referencedTextureIndices = GetReferencedTextureIndices(
            textureScene);
        for (int textureIndex = 0;
             textureIndex < textureScene.Textures.Count;
             textureIndex++)
        {
            ImportedTexture texture = textureScene.Textures[textureIndex];
            string? fullSourcePath = string.IsNullOrWhiteSpace(texture.SourcePath)
                ? null
                : Path.GetFullPath(texture.SourcePath);
            bool external = fullSourcePath is not null;
            string[] materialNames = textureScene.Materials
                .Where(material => material.BaseColorTextureIndex == textureIndex)
                .Select(material => material.Name)
                .Distinct(StringComparer.OrdinalIgnoreCase)
                .ToArray();
            string binding = materialNames.Length == 0
                ? "не привязана к материалам"
                : "материалы: " + string.Join(", ", materialNames);
            string displayName = string.IsNullOrWhiteSpace(texture.Name)
                ? $"texture_{textureIndex}"
                : texture.Name;
            int[] sourceMeshKeys = textureScene.Meshes
                .Select((mesh, sourceMeshKey) => (mesh, sourceMeshKey))
                .Where(item => item.mesh.MaterialIndex >= 0 &&
                    item.mesh.MaterialIndex < textureScene.Materials.Count &&
                    textureScene.Materials[item.mesh.MaterialIndex]
                        .BaseColorTextureIndex == textureIndex)
                .Select(item => item.sourceMeshKey)
                .Distinct()
                .Order()
                .ToArray();
            items.Add(new TextureResourceItem(
                $"{(external ? "Внешняя" : "Встроенная")} · {displayName}",
                $"{texture.Width}×{texture.Height} · {texture.MimeType} · {binding}",
                fullSourcePath,
                fullSourcePath is not null && manuallyAddedPaths.Contains(fullSourcePath),
                SourceMeshKeys: Array.AsReadOnly(sourceMeshKeys),
                MaterialModeStatus: GetMaterialModeStatus(sourceMeshKeys)));
        }

        IGrouping<string, ImportedMaterial>[] unresolvedGroups = textureScene.Meshes
            .Where(mesh => mesh.MaterialIndex >= 0 &&
                mesh.MaterialIndex < textureScene.Materials.Count)
            .Select(mesh => textureScene.Materials[mesh.MaterialIndex])
             .Where(material => material.BaseColorTextureIndex < 0 &&
                 !string.IsNullOrWhiteSpace(material.BaseColorTextureName))
             .GroupBy(
                 material => NormalizeTextureGroupReference(
                     material.BaseColorTextureName!),
                 StringComparer.OrdinalIgnoreCase)
             .ToArray();
        foreach (IGrouping<string, ImportedMaterial> group in unresolvedGroups)
        {
            string[] materialNames = group
                .Select(material => material.Name)
                .Distinct(StringComparer.OrdinalIgnoreCase)
                .ToArray();
            int[] sourceMeshKeys = textureScene.Meshes
                .Select((mesh, sourceMeshKey) => (mesh, sourceMeshKey))
                .Where(item => item.mesh.MaterialIndex >= 0 &&
                    item.mesh.MaterialIndex < textureScene.Materials.Count)
                 .Where(item => string.Equals(
                     NormalizeTextureGroupReference(
                         textureScene.Materials[item.mesh.MaterialIndex]
                             .BaseColorTextureName!),
                     group.Key,
                     StringComparison.OrdinalIgnoreCase))
                .Select(item => item.sourceMeshKey)
                .Distinct()
                .Order()
                .ToArray();
            items.Add(new TextureResourceItem(
                $"Ожидается · {Path.GetFileName(group.Key)}",
                "Изображение ещё не добавлено · материалы: " +
                string.Join(", ", materialNames),
                ExternalPath: null,
                CanRemove: false,
                SourceMeshKeys: Array.AsReadOnly(sourceMeshKeys),
                MaterialModeStatus: GetMaterialModeStatus(sourceMeshKeys)));
        }

        foreach (ImportedTexture texture in
                 textureCatalog?.UnusedExternalTextures ?? [])
        {
            string? fullSourcePath = string.IsNullOrWhiteSpace(texture.SourcePath)
                ? null
                : Path.GetFullPath(texture.SourcePath);
            items.Add(new TextureResourceItem(
                $"Не привязана · {Path.GetFileName(fullSourcePath ?? texture.Name)}",
                $"{texture.Width}×{texture.Height} · {texture.MimeType} · " +
                "файл не используется ни одним материалом модели",
                fullSourcePath,
                fullSourcePath is not null && manuallyAddedPaths.Contains(fullSourcePath)));
        }

        TextureList.ItemsSource = items;
        int unusedCount = textureCatalog?.UnusedExternalTextures.Count ?? 0;
        int opaqueOverlayGroupCount = items.Count(item =>
            item.SourceMeshKeys is { Count: > 0 } keys &&
            keys.All(_opaqueOverlaySourceMeshKeys.Contains));
        TextureSummaryText.Text =
            $"Доступно: {items.Count}; привязано к материалам: " +
            $"{referencedTextureIndices.Count}; добавлено извне: {_externalTextures.Count}; " +
            $"ожидается файлов: {unresolvedGroups.Length}; не привязано: {unusedCount}; " +
            $"непрозрачных накладок: {opaqueOverlayGroupCount}." +
            (generatedWeightsMode && _replacementRigidTextureBundle is not null
                ? " Rigid matN bundle здесь проигнорирован; показаны ресурсы полной исходной geometry."
                : string.Empty);
        UpdateRemoveTexturesAvailability();
        UpdateMaterialOverrideAvailability();
    }

    private static string NormalizeTextureGroupReference(string value)
    {
        string trimmed = value.Trim();
        try
        {
            string fileName = Path.GetFileName(trimmed);
            return string.IsNullOrWhiteSpace(fileName) ? trimmed : fileName;
        }
        catch (ArgumentException)
        {
            return trimmed;
        }
    }

    private string GetMaterialModeStatus(IReadOnlyCollection<int> sourceMeshKeys)
    {
        if (sourceMeshKeys.Count == 0)
            return string.Empty;

        int opaqueCount = sourceMeshKeys.Count(
            _opaqueOverlaySourceMeshKeys.Contains);
        return opaqueCount switch
        {
            0 => "Режим материала: Авто",
            _ when opaqueCount == sourceMeshKeys.Count =>
                "Режим материала: Непрозрачная накладка",
            _ => "Режим материала: смешанный — выберите «Авто» или " +
                 "«Непрозрачная накладка» для всей группы"
        };
    }

    private int[] GetSelectedTextureMaterialMeshKeys() =>
        TextureList.SelectedItems
            .OfType<TextureResourceItem>()
            .Where(item => item.SourceMeshKeys is { Count: > 0 })
            .SelectMany(item => item.SourceMeshKeys!)
            .Distinct()
            .Order()
            .ToArray();

    private void UpdateMaterialOverrideAvailability()
    {
        if (MaterialOverridePanel is null || SetOpaqueOverlayButton is null ||
            ResetMaterialModeButton is null ||
            MaterialOverrideStatusText is null || TextureList is null)
        {
            return;
        }

        int[] selectedMeshKeys = GetSelectedTextureMaterialMeshKeys();
        bool canEdit = CanEditSkinnedMaterialOverrides;
        MaterialOverridePanel.IsEnabled = canEdit;
        MaterialOverridePanel.Opacity = canEdit ? 1 : 0.55;
        SetOpaqueOverlayButton.IsEnabled = canEdit &&
            selectedMeshKeys.Any(key =>
                !_opaqueOverlaySourceMeshKeys.Contains(key));
        ResetMaterialModeButton.IsEnabled = canEdit &&
            selectedMeshKeys.Any(_opaqueOverlaySourceMeshKeys.Contains);

        MaterialOverrideStatusText.Text = !UsesGeneratedWeightsPortingMode
            ? "Доступно только в режиме 3 «Создать веса с нуля»."
            : SelectedSkinnedTextureTransferMode !=
              SkinnedTextureTransferMode.ImportDonor
                ? "Включите импорт текстур донора. С текстурами исходного SMO этот режим неприменим."
            : SkinnedMaterialOverrideCatalogScene is null
                ? "Точный каталог geometry-only ещё не подготовлен. Завершите подготовку режима 3."
            : selectedMeshKeys.Length == 0
                ? "Выберите одну или несколько привязанных текстурных групп в списке выше."
                : $"Выбрано элементов: {selectedMeshKeys.Length}; " +
                  $"непрозрачных накладок среди них: " +
                  $"{selectedMeshKeys.Count(_opaqueOverlaySourceMeshKeys.Contains)}.";
    }

    private void SetOpaqueOverlay_Click(object sender, RoutedEventArgs e) =>
        ApplySelectedSkinnedMaterialMode(opaqueOverlay: true);

    private void ResetMaterialMode_Click(object sender, RoutedEventArgs e) =>
        ApplySelectedSkinnedMaterialMode(opaqueOverlay: false);

    private void ApplySelectedSkinnedMaterialMode(bool opaqueOverlay)
    {
        ImportedScene? catalogScene = SkinnedMaterialOverrideCatalogScene;
        if (!CanEditSkinnedMaterialOverrides || catalogScene is null)
        {
            UpdateMaterialOverrideAvailability();
            return;
        }

        TextureResourceItem[] selectedItems = TextureList.SelectedItems
            .OfType<TextureResourceItem>()
            .Where(item => item.SourceMeshKeys is { Count: > 0 })
            .ToArray();
        int[] selectedMeshKeys = selectedItems
            .SelectMany(item => item.SourceMeshKeys!)
            .Distinct()
            .Order()
            .ToArray();
        if (selectedMeshKeys.Length == 0)
        {
            UpdateMaterialOverrideAvailability();
            return;
        }

        var candidateKeys = new HashSet<int>(_opaqueOverlaySourceMeshKeys);
        bool changed = false;
        foreach (int sourceMeshKey in selectedMeshKeys)
        {
            changed |= opaqueOverlay
                ? candidateKeys.Add(sourceMeshKey)
                : candidateKeys.Remove(sourceMeshKey);
        }
        if (!changed)
        {
            StatusText.Text = opaqueOverlay
                ? "Выбранные группы уже имеют режим «Непрозрачная накладка»."
                : "Выбранные группы уже используют режим «Авто».";
            UpdateMaterialOverrideAvailability();
            return;
        }

        try
        {
            bool hasCurrentPreparedScene =
                GeneratedSkinningPreparationIsCurrent;
            ImportedScene profileSource = hasCurrentPreparedScene
                ? _generatedSkinningPreparation!.PreparedScene
                : catalogScene;
            ValidateMaterialMeshKeys(profileSource, candidateKeys);
            SkinnedRenderableMaterialProfile candidateProfile =
                CreateSkinnedMaterialProfile(profileSource, candidateKeys);
            GlbSkinTransferPlan? candidatePlan = null;
            if (_document is not null)
            {
                GlbSkinTransferPlan validationPlan = AnalyzeGlbSkinTransfer(
                    _document,
                    profileSource,
                    SkinnedTextureTransferMode.ImportDonor,
                    candidateProfile);
                string? materialProfileIssue = validationPlan.Messages
                    .FirstOrDefault(message => message.StartsWith(
                        "Renderable material profile is invalid:",
                        StringComparison.Ordinal));
                if (materialProfileIssue is not null)
                {
                    throw new InvalidOperationException(
                        "Выбранные группы нельзя применить как непрозрачные накладки: " +
                        materialProfileIssue[
                            "Renderable material profile is invalid:".Length..].Trim());
                }
                if (hasCurrentPreparedScene)
                    candidatePlan = validationPlan;
            }

            // Publish the selection only after Core has analyzed the exact
            // PreparedScene when it exists. Before preparation, retain only
            // stable source mesh keys; the first preparation performs the same
            // strict validation before plan/save can become available.
            _opaqueOverlaySourceMeshKeys.Clear();
            _opaqueOverlaySourceMeshKeys.UnionWith(candidateKeys);
            ClearFinalTexturedPreview();
            _plan = null;
            _glbSkinTransferPlan = candidatePlan;
            _generatedPreparedSceneViewedRevision = -1;
            SetGeneratedSkinningConfirmation(false);
            PlanSummaryText.Text = hasCurrentPreparedScene
                ? "Режим материалов изменён. Снова покажите окончательный результат, " +
                  "подтвердите его и постройте план."
                : "Режим материалов сохранён и будет проверен при подготовке режима 3.";
            if (candidatePlan is not null)
                UpdateGeneratedSkinningCompatibilityPresentation();
            RefreshTextureList();
            RefreshState();
            StatusText.Text = !hasCurrentPreparedScene
                ? $"Режим сохранён для групп: {selectedItems.Length}. Он будет строго " +
                  "проверен при создании PreparedScene; plan/save пока недоступны."
                : opaqueOverlay
                    ? $"Групп назначено как «Непрозрачная накладка»: " +
                      $"{selectedItems.Length}. Прозрачность их исходных изображений " +
                      "будет убрана только в этой ветви SMO."
                    : $"Для групп восстановлен режим «Авто»: {selectedItems.Length}.";
        }
        catch (Exception exception) when (exception is InvalidDataException or
                                          InvalidOperationException or
                                          OverflowException or
                                          ArgumentException or
                                          NotSupportedException)
        {
            StatusText.Text = "Режим материала не изменён: " + exception.Message;
            MessageBox.Show(
                this,
                StatusText.Text,
                "Непрозрачная накладка",
                MessageBoxButton.OK,
                MessageBoxImage.Warning);
            UpdateMaterialOverrideAvailability();
        }
    }

    private void UpdateRemoveTexturesAvailability()
    {
        if (RemoveTexturesButton is null || TextureList is null)
            return;
        RemoveTexturesButton.IsEnabled = !PreserveOriginalTextures &&
            TextureList.SelectedItems
            .OfType<TextureResourceItem>()
            .Any(item => item.CanRemove);
    }

    private static HashSet<int> GetReferencedTextureIndices(ImportedScene scene) =>
        scene.Meshes
            .Where(mesh => mesh.MaterialIndex >= 0 &&
                mesh.MaterialIndex < scene.Materials.Count)
            .Select(mesh => scene.Materials[mesh.MaterialIndex].BaseColorTextureIndex)
            .Where(index => index >= 0 && index < scene.Textures.Count)
            .ToHashSet();

    private ImportedTexture? ResolveRigidSingleTexture()
    {
        if (_replacementScene is null)
            return null;
        int[] referenced = GetReferencedTextureIndices(_replacementScene).ToArray();
        if (referenced.Length == 1)
            return _replacementScene.Textures[referenced[0]];
        if (referenced.Length > 1)
            return null;
        if (_textureCatalogResult?.UnusedExternalTextures.Count == 1)
            return _textureCatalogResult.UnusedExternalTextures[0];
        return _replacementScene.Textures.Count == 1
            ? _replacementScene.Textures[0]
            : null;
    }

    private ImportedTexture? ResolveLegacySkinnedTextureOverride(
        ImportedScene? scene = null,
        ImportedTextureCatalogResult? textureCatalog = null)
    {
        scene ??= _replacementScene;
        textureCatalog ??= _textureCatalogResult;
        if (scene is null || GetReferencedTextureIndices(scene).Count > 0)
            return null;
        return textureCatalog?.UnusedExternalTextures.Count == 1
            ? textureCatalog.UnusedExternalTextures[0]
            : null;
    }

    private void SelectTextureFolder_Click(object sender, RoutedEventArgs e)
    {
        if (UsesGeneratedWeightsPortingMode)
        {
            StatusText.Text =
                "Папка matN в режиме 3 не применяется: используйте «Добавить файлы…» для однозначно сопоставимых текстур.";
            return;
        }
        if (_nativeValidationRunning || PreserveOriginalTextures ||
            _replacementPath is null || _baseReplacementScene is null ||
            _replacementSmoDocument is not null)
            return;

        var dialog = new OpenFolderDialog
        {
            Title = "Выберите папку с текстурами (matN-последовательности поддерживаются)",
            Multiselect = false
        };
        string? initialDirectory = _multiTextureDirectory ??
            Path.GetDirectoryName(_replacementPath);
        if (initialDirectory is not null && Directory.Exists(initialDirectory))
            dialog.InitialDirectory = initialDirectory;
        if (dialog.ShowDialog(this) != true)
            return;

        try
        {
            string fullTextureDirectory = Path.GetFullPath(dialog.FolderName);
            bool replacementIsFbx = Path.GetExtension(_replacementPath).Equals(
                ".fbx", StringComparison.OrdinalIgnoreCase);
            bool strictMatBundle =
                (replacementIsFbx || !_baseReplacementScene.HasSkinning) &&
                RigidGlbTextureBundleReader.HasCandidateTextureFiles(
                    _replacementPath, fullTextureDirectory);
            if (strictMatBundle)
            {
                try
                {
                    // FBX needs the dedicated rigid conversion: the ordinary reader
                    // may intentionally retain only its skinned meshes.
                    RigidGlbTextureBundle bundle = replacementIsFbx
                            ? RigidGlbTextureBundleReader.ReadModel(
                                _replacementPath,
                                fullTextureDirectory,
                                _blenderPath)
                            : RigidGlbTextureBundleReader.Bind(
                                _replacementPath,
                                _baseReplacementScene,
                                fullTextureDirectory);
                    ApplyExternalReplacementState(
                        _replacementPath,
                        _baseReplacementScene,
                        bundle.Scene,
                        bundle,
                        catalog: null,
                        textureDirectory: fullTextureDirectory,
                        resetTransform: false);
                    StatusText.Text =
                        "Папка matN подключена как набор материалов и кадров. " +
                        "Ранее добавленные файлы сохранены и вернутся после отключения папки. " +
                        "Проверьте multi-texture структуру заново.";
                }
                catch (RigidTextureBundleContainsSkinnedMeshesException)
                    when (replacementIsFbx && _baseReplacementScene.HasSkinning)
                {
                    int addedCount = ApplyTextureFilesFromFolder(fullTextureDirectory);
                    SetTextureFolderImportStatus(addedCount);
                }
            }
            else
            {
                int addedCount = ApplyTextureFilesFromFolder(fullTextureDirectory);
                SetTextureFolderImportStatus(addedCount);
            }
            _framePreviewOnRefresh = true;
            RefreshState();
        }
        catch (Exception exception)
        {
            ShowError(exception);
        }
    }

    private int ApplyTextureFilesFromFolder(string fullTextureDirectory)
    {
        string[] files = Directory.EnumerateFiles(
                fullTextureDirectory, "*", SearchOption.TopDirectoryOnly)
            .Where(IsSupportedExternalTexturePath)
            .OrderBy(Path.GetFileName, StringComparer.OrdinalIgnoreCase)
            .ThenBy(path => path, StringComparer.Ordinal)
            .ToArray();
        if (files.Length == 0)
        {
            throw new InvalidDataException(
                "В выбранной папке нет поддерживаемых изображений.");
        }
        (ImportedTexture[] textures, int addedCount) =
            MergeExternalTextureFiles(files);
        ApplyTextureOverrides(textures);
        return addedCount;
    }

    private void SetTextureFolderImportStatus(int addedCount)
    {
        int unresolved = GetUnresolvedTextureMaterialDescriptions(
            _replacementScene!).Length;
        StatusText.Text =
            $"Из папки добавлено текстур: {addedCount}. " +
            $"Ожидается файлов: {unresolved}. " +
            $"Не привязано к материалам: " +
            $"{_textureCatalogResult?.UnusedExternalTextures.Count ?? 0}.";
    }

    private void AddTextureFiles_Click(object sender, RoutedEventArgs e)
    {
        if (_nativeValidationRunning || PreserveOriginalTextures ||
            _replacementPath is null || _baseReplacementScene is null ||
            _replacementSmoDocument is not null)
            return;
        var dialog = new OpenFileDialog
        {
            Filter = "Изображения (*.png;*.jpg;*.jpeg;*.bmp;*.tga)|*.png;*.jpg;*.jpeg;*.bmp;*.tga|PNG (*.png)|*.png|JPEG (*.jpg;*.jpeg)|*.jpg;*.jpeg|BMP (*.bmp)|*.bmp|TGA (*.tga)|*.tga",
            CheckFileExists = true,
            Multiselect = true
        };
        if (dialog.ShowDialog(this) != true)
            return;

        try
        {
            (ImportedTexture[] textures, int addedCount) =
                MergeExternalTextureFiles(dialog.FileNames);
            ApplyTextureOverrides(textures);
            _framePreviewOnRefresh = true;
            RefreshState();
            int unused = _textureCatalogResult?.UnusedExternalTextures.Count ?? 0;
            int unresolved = GetUnresolvedTextureMaterialDescriptions(
                _replacementScene!).Length;
            StatusText.Text = addedCount == 0
                ? "Выбранные текстуры уже находятся в списке."
                : $"Добавлено текстур: {addedCount}. " +
                  (unresolved > 0
                      ? $"Ожидается ещё файлов: {unresolved}."
                      : unused == 0
                      ? "Все сопоставлены материалам модели."
                      : $"Не привязано к материалам: {unused}.");
        }
        catch (Exception exception)
        {
            ShowError(exception);
        }
    }

    private (ImportedTexture[] Textures, int AddedCount) MergeExternalTextureFiles(
        IEnumerable<string> paths)
    {
        var candidate = new List<ImportedTexture>(_externalTextures);
        var knownPaths = candidate
            .Select(texture => texture.SourcePath)
            .Where(path => !string.IsNullOrWhiteSpace(path))
            .Select(path => Path.GetFullPath(path!))
            .ToHashSet(StringComparer.OrdinalIgnoreCase);
        ImportedTexture[] added = paths
            .Select(Path.GetFullPath)
            .Where(path => knownPaths.Add(path))
            .Select(ImportedTextureFileReader.Read)
            .ToArray();
        candidate.AddRange(added);
        return (candidate.ToArray(), added.Length);
    }

    private static bool IsSupportedExternalTexturePath(string path) =>
        Path.GetExtension(path).ToLowerInvariant() is
            ".png" or ".jpg" or ".jpeg" or ".bmp" or ".tga";

    private void RemoveTextures_Click(object sender, RoutedEventArgs e)
    {
        if (_nativeValidationRunning || PreserveOriginalTextures)
            return;

        TextureResourceItem[] selected = TextureList.SelectedItems
            .OfType<TextureResourceItem>()
            .Where(item => item.CanRemove)
            .ToArray();
        if (selected.Any(item => item.RemovesFolder))
        {
            try
            {
                ApplyTextureOverrides(_externalTextures.ToArray());
                RefreshState();
                StatusText.Text = "Папка matN отключена; восстановлены текстуры модели.";
            }
            catch (Exception exception)
            {
                ShowError(exception);
            }
            return;
        }

        string[] paths = selected
            .Where(item => item.CanRemove && !string.IsNullOrWhiteSpace(item.ExternalPath))
            .Select(item => Path.GetFullPath(item.ExternalPath!))
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .ToArray();
        if (paths.Length == 0)
            return;

        try
        {
            HashSet<string> removed = paths.ToHashSet(StringComparer.OrdinalIgnoreCase);
            ImportedTexture[] remaining = _externalTextures
                .Where(texture => texture.SourcePath is null ||
                    !removed.Contains(Path.GetFullPath(texture.SourcePath)))
                .ToArray();
            ApplyTextureOverrides(remaining);
            RefreshState();
            StatusText.Text = $"Удалено внешних текстур: {paths.Length}.";
        }
        catch (Exception exception)
        {
            ShowError(exception);
        }
    }

    private void TextureList_SelectionChanged(
        object sender,
        SelectionChangedEventArgs e)
    {
        UpdateRemoveTexturesAvailability();
        UpdateMaterialOverrideAvailability();
    }

    private void PreserveOriginalTextures_Changed(
        object sender,
        RoutedEventArgs e)
    {
        if (_settingPreserveOriginalTextures || _nativeValidationRunning)
            return;

        _plan = null;
        _rigidMultiMaterialAnalysis = null;
        _glbSkinTransferPlan = null;
        InvalidateAdaptedPortingPreparation();
        InvalidateGeneratedSkinningPreparation();
        PlanSummaryText.Text = PreserveOriginalTextures
            ? "Диагностический план с текстурами исходного SMO ещё не построен."
            : "План ещё не построен.";

        if (UsesAdaptDonorWeightsPortingMode &&
            _replacementScene?.HasSkinning == true)
            UpdateAdaptedPortingPreparation();
        else if (UsesGeneratedWeightsPortingMode)
            UpdateGeneratedSkinningPreparation();
        else if (UsesPreparedModelPortingMode &&
            _replacementScene?.HasSkinning == true)
            UpdateGlbSkinTransferPlan();
        else if (UsesLegacyRigidPortingMode &&
                 _replacementRigidTextureBundle is not null)
            UpdateRigidTextureModeDescription();
        else if (_replacementScene is not null)
            StatusText.Text = PreserveOriginalTextures
                ? "Текстуры донора отключены. Постройте диагностический план геометрии."
                : "Текстуры донора включены. Постройте план импорта.";

        if (!string.IsNullOrWhiteSpace(_rigidTextureBindingIssue))
        {
            RefreshTextureList();
            AppendRigidTextureBindingStatus();
        }

        RefreshState();
    }

    private void GeneratedSkinningConfirmation_Changed(
        object sender,
        RoutedEventArgs e)
    {
        if (_settingGeneratedSkinningConfirmation || !IsLoaded ||
            !UsesGeneratedWeightsPortingMode)
            return;

        if (GeneratedSkinningIsConfirmed &&
            (!GeneratedSkinningPreparationIsCurrent ||
             !GeneratedPreparedSceneViewedForCurrentRevision))
        {
            SetGeneratedSkinningConfirmation(false);
            StatusText.Text =
                "Подтверждение доступно только после просмотра точного PreparedScene текущей ревизии.";
            return;
        }

        _plan = null;
        PlanSummaryText.Text = "План ещё не построен.";
        RefreshState();
        string? transferBlocker = GetGeneratedSkinningTransferBlocker();
        StatusText.Text = !GeneratedSkinningIsConfirmed
            ? "Подтверждение режима 3 снято; построение плана и сохранение отключены."
            : transferBlocker is not null
                ? "Итоговая модель подтверждена, но создание SMO всё ещё заблокировано: " +
                  PresentGeneratedSkinningTransferBlocker(transferBlocker)
                : "Результат режима 3 подтверждён. Теперь можно построить план palettes.";
    }

    private void ShowGeneratedPreparedScene_Click(
        object sender,
        RoutedEventArgs e)
    {
        ClearFinalTexturedPreview();
        if (_nativeValidationRunning || !UsesGeneratedWeightsPortingMode)
            return;
        if (RigFittingEditorHasPendingChanges)
        {
            StatusText.Text =
                "Сначала примените введённые значения размера или подгонки.";
            RefreshState();
            return;
        }
        if (!GeneratedSkinningPreparationIsCurrent)
        {
            StatusText.Text = _generatedSkinningPreparationIssue ??
                "Автоматические веса ещё не подготовлены.";
            RefreshState();
            return;
        }

        // This is the explicit review transition. The old workflow required the
        // user to discover that the fitting-pose checkbox had to be cleared.
        // Keep that view switch internal and render the exact writer scene now.
        if (ShowFittingPoseCheckBox.IsChecked == true)
            ShowFittingPoseCheckBox.IsChecked = false;

        _explicitGeneratedReviewRequested = true;
        try
        {
            RefreshPreview();
        }
        finally
        {
            _explicitGeneratedReviewRequested = false;
        }

        RefreshState();
        StatusText.Text = GeneratedPreparedSceneViewedForCurrentRevision
            ? "Показана точная итоговая модель текущей ревизии. Проверьте её и подтвердите результат слева."
            : "Не удалось отметить итоговую модель как просмотренную; повторите показ после применения всех изменений.";
    }

    private void ShowFinalTexturedPreview_Click(
        object sender,
        RoutedEventArgs e)
    {
        if (!CanShowFinalTexturedPreview)
        {
            StatusText.Text = PreserveOriginalTextures
                ? "Для текстурированного итогового просмотра включите импорт текстур модели-донора."
                : RigFittingEditorHasPendingChanges
                    ? "Сначала примените изменения размера или позы."
                    : "Окончательный текстурированный результат пока не подготовлен.";
            RefreshState();
            return;
        }

        try
        {
            ImportedScene scene = BuildFinalTexturedPreviewScene(
                out Matrix4x4 transform);
            ValidateFinalTexturedPreviewScene(scene);

            // These controls are authoring views. The final preview always
            // shows the complete canonical writer input without gray target,
            // skeleton or attachment isolation.
            if (HideGeneratedMainBodyCheckBox.IsChecked == true)
                HideGeneratedMainBodyCheckBox.IsChecked = false;
            if (ShowFittingPoseCheckBox.IsChecked == true)
                ShowFittingPoseCheckBox.IsChecked = false;

            _finalTexturedPreviewScene = scene;
            _finalTexturedPreviewTransform = transform;
            _showFinalTexturedPreview = true;
            _explicitGeneratedReviewRequested = true;
            try
            {
                RefreshState();
            }
            finally
            {
                _explicitGeneratedReviewRequested = false;
            }
            StatusText.Text =
                $"Показан окончательный результат с текстурами: " +
                $"{scene.Meshes.Count} mesh, {scene.Textures.Count} texture resources.";
        }
        catch (Exception exception) when (exception is InvalidDataException or
                                          InvalidOperationException or
                                          OverflowException or
                                          ArgumentException or
                                          NotSupportedException)
        {
            ClearFinalTexturedPreview();
            StatusText.Text =
                "Не удалось построить окончательный текстурированный просмотр: " +
                exception.Message;
            RefreshState();
        }
    }

    private ImportedScene BuildFinalTexturedPreviewScene(
        out Matrix4x4 transform)
    {
        if (_document is null || _replacementScene is null)
            throw new InvalidOperationException(
                "Сначала выберите целевой SMO и модель-донор.");

        transform = Matrix4x4.Identity;
        if (UsesLegacyRigidPortingMode)
        {
            transform = ReadTransform().Matrix;
            return _replacementScene;
        }

        ImportedScene writerInput;
        SkinnedGeometryTransferMode geometryMode;
        if (UsesGeneratedWeightsPortingMode)
        {
            if (!GeneratedSkinningPreparationIsCurrent)
                throw new InvalidOperationException(
                    "Автоматически созданные веса ещё не пересчитаны для текущей позы.");
            writerInput = _generatedSkinningPreparation!.PreparedScene;
            geometryMode = SkinnedGeometryTransferMode.PreservePreparedGeometry;
        }
        else if (UsesAdaptDonorWeightsPortingMode)
        {
            if (!AdaptedPortingPreparationIsCurrent)
                throw new InvalidOperationException(
                    "Адаптированные веса ещё не пересчитаны для текущей позы.");
            writerInput = _adaptedPortingPreparation!.PreparedScene;
            geometryMode = SkinnedGeometryTransferMode.PreservePreparedGeometry;
        }
        else if (UsesPreparedModelPortingMode)
        {
            writerInput = _replacementScene;
            geometryMode = RebaseBindPoseCheckBox.IsChecked == true
                ? SkinnedGeometryTransferMode.RetargetToGameBindPose
                : SkinnedGeometryTransferMode.PreservePreparedGeometry;
        }
        else
        {
            throw new InvalidOperationException(
                "Выбранный режим не имеет текстурированного итогового просмотра.");
        }

        // This follows the same BuildGlbPlan path as Replace, including the
        // verified multi-material atlas and its UV remap. The returned scene is
        // therefore the visual writer input, not merely the raw donor.
        return SmoSkinnedGlbReplacer.PrepareGeometryPreview(
            _document,
            writerInput,
            ReplacementTransform.Identity,
            geometryMode,
            SkinnedTextureTransferMode.ImportDonor,
            ResolveSkinnedMaterialProfile(
                writerInput,
                SkinnedTextureTransferMode.ImportDonor));
    }

    private static void ValidateFinalTexturedPreviewScene(ImportedScene scene)
    {
        for (int meshIndex = 0; meshIndex < scene.Meshes.Count; meshIndex++)
        {
            ImportedMesh mesh = scene.Meshes[meshIndex];
            if (mesh.TriangleIndices.Length == 0)
                continue;
            if (mesh.TextureCoordinates.Length != mesh.Positions.Length)
                throw new InvalidDataException(
                    $"Mesh [{meshIndex}] \"{mesh.Name}\" не имеет полного набора UV.");
            if ((uint)mesh.MaterialIndex >= (uint)scene.Materials.Count)
                throw new InvalidDataException(
                    $"Mesh [{meshIndex}] \"{mesh.Name}\" не ссылается на материал.");
            ImportedMaterial material = scene.Materials[mesh.MaterialIndex];
            if ((uint)material.BaseColorTextureIndex >= (uint)scene.Textures.Count)
                throw new InvalidDataException(
                    $"Материал \"{material.Name}\" не ссылается на доступную текстуру.");
            ImportedTexture texture = scene.Textures[material.BaseColorTextureIndex];
            if (texture.Data.Length == 0)
                throw new InvalidDataException(
                    $"Текстура \"{texture.Name}\" не содержит изображения.");
        }
    }

    private void HideGeneratedMainBody_Changed(
        object sender,
        RoutedEventArgs e)
    {
        if (!IsLoaded)
            return;
        ClearFinalTexturedPreview();
        RefreshPreview();
    }

    private void ManualAdaptWeights_Changed(object sender, RoutedEventArgs e)
    {
        if (_settingManualAdaptWeights || !IsLoaded ||
            !UsesAdaptDonorWeightsPortingMode)
            return;

        _plan = null;
        _glbSkinTransferPlan = null;
        InvalidateAdaptedPortingPreparation();
        if (_bodyPoseEditorDirty)
        {
            _bodyPoseEditorDirty = false;
            _bodyPoseDraftSnapshot = null;
            _draftBodyPoseControls = _committedBodyPoseControls;
            WriteBodyPoseControls(_committedBodyPoseControls);
        }
        _rigPoseEditorDirty = false;
        LoadRigFittingEditorValues();
        if (ManualAdaptWeights)
        {
            if (!EnsureRigFittingState())
            {
                RefreshState();
                return;
            }
            try
            {
                _manualDonorAlignment = ReadTransform();
                _manualAlignmentEditorDirty = false;
            }
            catch (Exception exception) when (exception is InvalidOperationException or
                                              ArgumentException)
            {
                _manualAlignmentEditorDirty = true;
                _adaptedPortingPreparationIssue = exception.Message;
                RigFittingStatusText.Text = exception.Message;
                RefreshState();
                return;
            }
        }
        else
        {
            _manualAlignmentEditorDirty = false;
            WriteTransformEditor(_manualDonorAlignment);
        }

        UpdateAdaptedPortingPreparation();
        RefreshState();
    }

    private void RigFittingJoint_Changed(object sender, SelectionChangedEventArgs e)
    {
        if (_settingRigFittingControls || !IsLoaded)
            return;
        LoadRigFittingEditorValues(
            includeRootValues: false,
            clearPendingRootValues: false);
        RigFittingStatusText.Text =
            "Выбран сустав " +
            (RigFittingJointCombo.SelectedItem as TargetRigJointItem)?.Display +
            ". Ползунки показывают его текущее абсолютное local-вращение.";
        UpdateRigSkeletonScreenOverlay();
    }

    private void RigPoseEditorMode_Changed(object sender, SelectionChangedEventArgs e)
    {
        if (!IsLoaded || HumanPoseEditorPanel is null ||
            RigFittingEditorPanel is null)
        {
            return;
        }

        bool jointsMode = IsJointPoseEditorMode;
        RigFittingEditorPanel.Visibility = jointsMode
            ? Visibility.Visible
            : Visibility.Collapsed;
        HumanPoseEditorPanel.Visibility = jointsMode
            ? Visibility.Collapsed
            : Visibility.Visible;
        GeneratedAttachmentEditorPanel.Visibility =
            UsesGeneratedWeightsPortingMode && !jointsMode
            ? Visibility.Visible
            : Visibility.Collapsed;
        if (jointsMode)
        {
            LoadRigFittingEditorValues(
                includeRootValues: false,
                clearPendingRootValues: false);
            RigFittingStatusText.Text =
                "Режим «Суставы»: выберите сустав в списке или щёлкните по нему в окне просмотра.";
        }
        else
        {
            BodyPoseStatusText.Text = _bodyPoseEditorDirty
                ? "Показана общая неприменённая поза; значения режима «Суставы» сохранены."
                : "Режим «Человек» использует ту же применённую позу, что и режим «Суставы».";
        }
        RefreshRigFittingState();
        RefreshPreview();
    }

    private bool IsJointPoseEditorMode =>
        RigPoseEditorModeCombo?.SelectedIndex != 1;

    private void RigJointRotationSlider_ValueChanged(
        object sender,
        RoutedPropertyChangedEventArgs<double> e)
    {
        if (_settingRigFittingControls || !IsLoaded ||
            !IsJointPoseEditorMode || _nativeValidationRunning ||
            (!UsesAdaptDonorWeightsPortingMode &&
             !UsesGeneratedWeightsPortingMode) ||
            !EnsureRigFittingState() || _targetRigFittingPose is null ||
            RigFittingJointCombo.SelectedItem is not TargetRigJointItem selected)
        {
            return;
        }

        try
        {
            TargetRigFittingPoseSnapshot current =
                CaptureDisplayedRigFittingPose(
                    localRotationsOnly: UsesGeneratedWeightsPortingMode);
            TargetRigFittingPose draft = CreateMutablePose(
                current,
                localRotationsOnly: UsesGeneratedWeightsPortingMode);
            Vector3 euler = new(
                checked((float)RigLocalRotXSlider.Value),
                checked((float)RigLocalRotYSlider.Value),
                checked((float)RigLocalRotZSlider.Value));
            draft.SetLocalRotationDelta(
                selected.JointIndex,
                EulerDegreesToQuaternion(euler));
            if (!_bodyPoseEditorDirty)
                _draftBodyPoseControls = _committedBodyPoseControls;
            _bodyPoseDraftSnapshot = draft.Capture();
            _bodyPoseEditorDirty = true;
            if (_rigLocalEulerDegrees is not null &&
                (uint)selected.JointIndex < (uint)_rigLocalEulerDegrees.Length)
            {
                _rigLocalEulerDegrees[selected.JointIndex] = euler;
            }
            _plan = null;
            PlanSummaryText.Text =
                "Есть неприменённое вращение сустава.";
            if (ShowFittingPoseCheckBox.IsChecked != true)
                ShowFittingPoseCheckBox.IsChecked = true;
            RigFittingStatusText.Text =
                $"Сустав {selected.Display}: X {euler.X:F1}°, Y {euler.Y:F1}°, Z {euler.Z:F1}°. " +
                "Изменение показано; нажмите «Применить».";
            RefreshState();
        }
        catch (Exception exception) when (exception is InvalidDataException or
                                          InvalidOperationException or
                                          OverflowException or
                                          ArgumentException)
        {
            RigFittingStatusText.Text =
                "Вращение сустава недоступно: " + exception.Message;
            RefreshState();
        }
    }

    private void RigFittingEditor_TextChanged(object sender, TextChangedEventArgs e)
    {
        if (_settingRigFittingControls || !IsLoaded)
            return;
        _rigPoseEditorDirty = true;
        _plan = null;
        PlanSummaryText.Text = "Есть неприменённые значения позы подгонки.";
        RigFittingStatusText.Text =
            "Значения ещё не применены. Подготовка модели не пересчитывается во время ввода.";
        RefreshState();
    }

    private void BodyPoseSlider_ValueChanged(
        object sender,
        RoutedPropertyChangedEventArgs<double> e)
    {
        if (_settingBodyPoseControls || !IsLoaded ||
            (!UsesGeneratedWeightsPortingMode &&
             !(UsesAdaptDonorWeightsPortingMode && ManualAdaptWeights)))
        {
            return;
        }

        try
        {
            if (_rigPoseEditorDirty)
            {
                BodyPoseStatusText.Text =
                    "Сначала примените или исправьте значения корневого сустава.";
                return;
            }
            StageBodyPose(
                ReadBodyPoseControls(),
                "Показана предварительная групповая поза. Нажмите «Применить позу», " +
                "чтобы пересчитать веса и разрешить построение плана.");
        }
        catch (Exception exception) when (exception is InvalidDataException or
                                          InvalidOperationException or
                                          OverflowException or
                                          ArgumentException)
        {
            _bodyPoseDraftSnapshot = null;
            _bodyPoseEditorDirty = true;
            BodyPoseStatusText.Text = "Поза недоступна: " + exception.Message;
            RefreshState();
        }
    }

    private void AutoFitBodyPose_Click(object sender, RoutedEventArgs e)
    {
        if (_nativeValidationRunning || !EnsureRigFittingState() ||
            _targetRigDefinition is null || _sourceScene is null)
            return;

        bool previousDirty = _bodyPoseEditorDirty;
        BodyPoseControlValues previousControls = ReadBodyPoseControls();
        TargetRigFittingPoseSnapshot? previousDraft = _bodyPoseDraftSnapshot;
        TargetRigAutomaticPoseFitResult? previousAutoFit = _bodyPoseAutoFitResult;
        try
        {
            ImportedScene donor = ResolveBodyPoseDonorScene();
            ReplacementTransform alignment = UsesGeneratedWeightsPortingMode
                ? _generatedDonorAlignment ??
                  ComputeCoarseGeneratedDonorAlignment(donor)
                : _manualDonorAlignment;
            TargetRigAutomaticPoseFitResult result =
                TargetRigAutomaticPoseFitter.Fit(
                    _targetRigDefinition,
                    _sourceScene,
                    donor,
                    alignment);
            BodyPoseControlValues values = FromBodyPoseParameters(
                result.Parameters);
            WriteBodyPoseControls(values);
            _bodyPoseDraftSnapshot = RebaseBodyPose(
                values,
                result.Pose);
            _draftBodyPoseControls = values;
            _bodyPoseAutoFitResult = result;
            _generatedBodySelection = result.BodySelection;
            _bodyPoseEditorDirty = true;
            _bodyPoseAutoFitDetails = string.Join(" ", result.Diagnostics);
            BodyPoseStatusText.ToolTip = _bodyPoseAutoFitDetails;
            _plan = null;
            PlanSummaryText.Text = "Автоматическая поза показана, но ещё не применена.";
            if (ShowFittingPoseCheckBox.IsChecked != true)
                ShowFittingPoseCheckBox.IsChecked = true;
            BodyPoseStatusText.Text =
                $"Автоподгонка улучшила ошибку {result.ScoreBefore:G5} → " +
                $"{result.ScoreAfter:G5}; выбрано частей тела: " +
                $"{result.BodySelection.Components.Count}, исключено: " +
                $"{result.BodySelection.ExcludedComponentCount}. " +
                (result.ScoreAfter > 0.02f
                    ? "Поза улучшена, но размер или центр корпуса ещё заметно не совпадают; " +
                      "сначала уточните их выше и повторите автоподгонку. "
                    : string.Empty) +
                "Проверьте серую модель и нажмите «Применить позу».";
            RefreshState();
        }
        catch (Exception exception) when (exception is InvalidDataException or
                                          InvalidOperationException or
                                          OverflowException or
                                          ArgumentException or
                                          NotSupportedException)
        {
            _bodyPoseDraftSnapshot = previousDraft;
            _bodyPoseEditorDirty = previousDirty;
            _bodyPoseAutoFitResult = previousAutoFit;
            _draftBodyPoseControls = previousDirty
                ? previousControls
                : _committedBodyPoseControls;
            WriteBodyPoseControls(_draftBodyPoseControls);
            _bodyPoseAutoFitDetails = exception.Message;
            BodyPoseStatusText.ToolTip = _bodyPoseAutoFitDetails;
            BodyPoseStatusText.Text =
                "Автоподгонка не нашла однозначное тело: " + exception.Message +
                " Групповые ползунки остаются доступны.";
            StatusText.Text = BodyPoseStatusText.Text;
            RefreshState();
        }
    }

    private ImportedScene ResolveBodyPoseDonorScene()
    {
        ImportedScene source = UsesGeneratedWeightsPortingMode
            ? _generatedSkinningEffectiveScene ??
              _generatedSkinningBaseScene ??
              _baseReplacementScene ??
              _replacementScene ??
              throw new InvalidOperationException("Модель-донор не загружена.")
            : _replacementScene ??
              throw new InvalidOperationException("Модель-донор не загружена.");
        if (!source.HasSkinning)
            return source;
        ImportedMesh[] geometryOnlyMeshes = source.Meshes
            .Select(mesh => mesh with { Skinning = null })
            .ToArray();
        return source with { Meshes = geometryOnlyMeshes };
    }

    private void ApplyBodyPose_Click(object sender, RoutedEventArgs e)
    {
        bool preparationIsCurrent = UsesGeneratedWeightsPortingMode
            ? GeneratedSkinningPreparationIsCurrent
            : AdaptedPortingPreparationIsCurrent;
        if (_nativeValidationRunning || !EnsureRigFittingState() ||
            (!_bodyPoseEditorDirty && preparationIsCurrent))
        {
            return;
        }

        try
        {
            if (_bodyPoseDraftSnapshot is null)
            {
                BodyPoseControlValues currentValues = ReadBodyPoseControls();
                _bodyPoseDraftSnapshot = RebaseBodyPose(currentValues);
                _draftBodyPoseControls = currentValues;
                _bodyPoseEditorDirty = true;
            }
            TargetRigFittingPose candidate = CreateMutablePose(
                _bodyPoseDraftSnapshot,
                localRotationsOnly: UsesGeneratedWeightsPortingMode);
            _targetRigFittingPose = candidate;
            SynchronizeRigEulerFromPose(candidate);
            if (UsesGeneratedWeightsPortingMode)
                _rigRootEulerDegrees = Vector3.Zero;
            _committedBodyPoseControls = _draftBodyPoseControls;
            _bodyPoseEditorDirty = false;
            _bodyPoseDraftSnapshot = null;
            _rigPoseEditorDirty = false;
            _bodyPoseAutoFitDetails = null;
            BodyPoseStatusText.ToolTip = null;
            LoadRigFittingEditorValues();
            CommitRigFittingChange("Групповая поза тела применена.");
            BodyPoseStatusText.Text = GeneratedSkinningPreparationIsCurrent ||
                                      AdaptedPortingPreparationIsCurrent
                ? "Поза применена; окно просмотра и подготовка модели используют одну ревизию."
                : "Поза применена и видна на серой игровой модели. Подготовка весов всё ещё " +
                  "заблокирована: " +
                  (_generatedSkinningPreparationIssue ??
                   _adaptedPortingPreparationIssue ??
                   "см. диагностику ниже");
        }
        catch (Exception exception) when (exception is InvalidDataException or
                                          InvalidOperationException or
                                          OverflowException or
                                          ArgumentException or
                                          NotSupportedException)
        {
            BodyPoseStatusText.Text = "Поза не применена: " + exception.Message;
            StatusText.Text = BodyPoseStatusText.Text;
            RefreshState();
        }
    }

    private void ResetBodyPose_Click(object sender, RoutedEventArgs e)
    {
        if (_nativeValidationRunning || !EnsureRigFittingState())
            return;
        WriteBodyPoseControls(default);
        StageBodyPose(
            default,
            "Показана нейтральная симметричная поза: руки горизонтально, ноги вертикально. " +
            "Для сохранения изменения нажмите «Применить позу».");
    }

    private void StageBodyPose(
        BodyPoseControlValues values,
        string status)
    {
        if (!EnsureRigFittingState() || _targetRigDefinition is null)
            throw new InvalidOperationException(
                _rigFittingIssue ?? "Игровой скелет недоступен для подгонки.");
        _bodyPoseDraftSnapshot = RebaseBodyPose(values);
        _draftBodyPoseControls = values;
        _bodyPoseEditorDirty = true;
        _bodyPoseAutoFitDetails = null;
        BodyPoseStatusText.ToolTip = null;
        _plan = null;
        PlanSummaryText.Text = "Есть неприменённая групповая поза тела.";
        if (ShowFittingPoseCheckBox.IsChecked != true)
            ShowFittingPoseCheckBox.IsChecked = true;
        BodyPoseStatusText.Text = status;
        RefreshState();
    }

    private TargetRigFittingPoseSnapshot RebaseBodyPose(
        BodyPoseControlValues newValues,
        TargetRigFittingPoseSnapshot? exactNewHumanPose = null)
    {
        if (_targetRigDefinition is null || _targetRigFittingPose is null)
        {
            throw new InvalidOperationException(
                "Игровой скелет недоступен для подгонки тела.");
        }

        TargetRigFittingPoseSnapshot effectivePose =
            _bodyPoseEditorDirty && _bodyPoseDraftSnapshot is not null
                ? _bodyPoseDraftSnapshot
                : _targetRigFittingPose.Capture();
        BodyPoseControlValues oldValues = _bodyPoseEditorDirty
            ? _draftBodyPoseControls
            : _committedBodyPoseControls;
        TargetRigFittingPoseSnapshot rebased =
            TargetRigBodyPoseMapper.RebasePreservingCorrections(
                _targetRigDefinition,
                effectivePose,
                ToBodyPoseParameters(oldValues),
                ToBodyPoseParameters(newValues),
                exactNewHumanPose);
        return UsesGeneratedWeightsPortingMode &&
               (rebased.RootRotation != Quaternion.Identity ||
                rebased.RootTranslation != Vector3.Zero)
            ? CreateMutablePose(rebased, localRotationsOnly: true).Capture()
            : rebased;
    }

    private static TargetRigBodyPoseParameters ToBodyPoseParameters(
        BodyPoseControlValues values) => new(
        values.ArmRaiseDegrees,
        values.ArmForwardDegrees,
        values.ElbowBendDegrees,
        values.LegSpreadDegrees,
        values.KneeBendDegrees,
        values.TorsoPitchDegrees,
        values.NeckForwardDegrees);

    private static BodyPoseControlValues FromBodyPoseParameters(
        TargetRigBodyPoseParameters values) => new(
        values.ArmElevationDegrees,
        values.ArmForwardDegrees,
        values.ElbowBendDegrees,
        values.LegSpreadDegrees,
        values.KneeBendDegrees,
        values.TorsoPitchDegrees,
        values.NeckForward);

    private static TargetRigFittingPose CreateMutablePose(
        TargetRigFittingPoseSnapshot snapshot,
        bool localRotationsOnly)
    {
        TargetRigFittingPose pose = snapshot.Definition.CreateFittingPose();
        for (int jointIndex = 0;
             jointIndex < snapshot.LocalRotationDeltas.Count;
             jointIndex++)
        {
            pose.SetLocalRotationDelta(
                jointIndex,
                snapshot.LocalRotationDeltas[jointIndex]);
        }
        if (!localRotationsOnly)
            pose.SetRootTransform(snapshot.RootRotation, snapshot.RootTranslation);
        _ = pose.Capture();
        return pose;
    }

    private TargetRigFittingPoseSnapshot PreserveCurrentMode2Root(
        TargetRigFittingPoseSnapshot snapshot)
    {
        if (!UsesAdaptDonorWeightsPortingMode || !ManualAdaptWeights ||
            _targetRigFittingPose is null)
        {
            return snapshot;
        }

        TargetRigFittingPose pose = CreateMutablePose(
            snapshot,
            localRotationsOnly: true);
        pose.SetRootTransform(
            _targetRigFittingPose.RootRotation,
            _targetRigFittingPose.RootTranslation);
        return pose.Capture();
    }

    private void ApplyRigFitting_Click(object sender, RoutedEventArgs e)
    {
        if (_nativeValidationRunning ||
            (!UsesAdaptDonorWeightsPortingMode &&
             !UsesGeneratedWeightsPortingMode) ||
            !EnsureRigFittingState() ||
            _targetRigFittingPose is null ||
            _rigLocalEulerDegrees is null ||
            (!_bodyPoseEditorDirty && !_rigPoseEditorDirty &&
             !_manualAlignmentEditorDirty))
            return;

        try
        {
            TargetRigFittingPoseSnapshot displayed =
                CaptureDisplayedRigFittingPose(
                    localRotationsOnly: UsesGeneratedWeightsPortingMode);
            TargetRigFittingPose candidate = CreateMutablePose(
                displayed,
                localRotationsOnly: UsesGeneratedWeightsPortingMode);
            Vector3 rootEuler = _rigRootEulerDegrees;
            Vector3 rootTranslation = displayed.RootTranslation;
            ReplacementTransform donorAlignment = _manualDonorAlignment;
            if (UsesAdaptDonorWeightsPortingMode && ManualAdaptWeights)
            {
                rootEuler = ReadEulerDegrees(
                    RigRootRotXBox,
                    RigRootRotYBox,
                    RigRootRotZBox,
                    "global rotation");
                rootTranslation = ReadVector3(
                    RigRootMoveXBox,
                    RigRootMoveYBox,
                    RigRootMoveZBox,
                    "global translation");
                donorAlignment = ReadTransform();
            }

            if (UsesAdaptDonorWeightsPortingMode && ManualAdaptWeights)
            {
                candidate.SetRootTransform(
                    EulerDegreesToQuaternion(rootEuler),
                    rootTranslation);
            }
            else
            {
                candidate.SetRootTransform(
                    EulerDegreesToQuaternion(_rigRootEulerDegrees),
                    _targetRigFittingPose.RootTranslation);
            }
            TargetRigFittingPoseSnapshot candidateSnapshot = candidate.Capture();
            Vector3[] candidateEuler = GetPoseEulerDegrees(candidateSnapshot);

            if (UsesAdaptDonorWeightsPortingMode && ManualAdaptWeights)
            {
                if (_document is null || _replacementScene?.HasSkinning != true)
                {
                    throw new InvalidOperationException(
                        "Ручная адаптация требует target SMO и donor skin weights.");
                }

                // Prepare and validate the exact candidate before publishing any
                // editor state. A failed Core preparation therefore leaves the
                // current revision and its writer scene untouched.
                SkinnedModelPortingPreparation candidatePreparation =
                    SkinnedModelPortingPreparer.PrepareAdaptDonorWeights(
                        _document,
                        _replacementScene,
                        candidateSnapshot,
                        donorAlignment);
                GlbSkinTransferPlan candidateTransferPlan =
                    AnalyzeGlbSkinTransfer(
                        _document,
                        candidatePreparation.PreparedScene,
                        SelectedSkinnedTextureTransferMode);
                CommitManualAdaptCandidate(
                    candidate,
                    candidateEuler,
                    rootEuler,
                    donorAlignment,
                    candidatePreparation,
                    candidateTransferPlan);
                return;
            }

            _targetRigFittingPose = candidate;
            _rigLocalEulerDegrees = candidateEuler;
            if (_bodyPoseEditorDirty)
                _committedBodyPoseControls = _draftBodyPoseControls;
            _bodyPoseEditorDirty = false;
            _bodyPoseDraftSnapshot = null;
            _rigPoseEditorDirty = false;
            _manualAlignmentEditorDirty = false;
            CommitRigFittingChange("Поза и donor alignment применены.");
            BodyPoseStatusText.Text =
                "Общая поза применена. Оба режима редактора показывают те же вращения.";
        }
        catch (Exception exception) when (exception is InvalidDataException or
                                          InvalidOperationException or
                                          OverflowException or
                                          ArgumentException or
                                          NotSupportedException)
        {
            if (UsesAdaptDonorWeightsPortingMode && ManualAdaptWeights)
            {
                _rigPoseEditorDirty = true;
                _manualAlignmentEditorDirty = true;
                _adaptedPortingPreparationIssue = exception.Message;
                _plan = null;
                PlanSummaryText.Text =
                    "Кандидат позы не подготовлен; сохранённая ревизия не изменена.";
            }
            RigFittingStatusText.Text = "Поза не применена: " + exception.Message;
            StatusText.Text = RigFittingStatusText.Text;
            RefreshState();
        }
    }

    private void CommitManualAdaptCandidate(
        TargetRigFittingPose candidatePose,
        Vector3[] candidateEuler,
        Vector3 rootEuler,
        ReplacementTransform donorAlignment,
        SkinnedModelPortingPreparation candidatePreparation,
        GlbSkinTransferPlan candidateTransferPlan)
    {
        _targetRigFittingPose = candidatePose;
        _rigLocalEulerDegrees = candidateEuler;
        _rigRootEulerDegrees = rootEuler;
        _manualDonorAlignment = donorAlignment;
        _rigFittingRevision++;
        _plan = null;
        InvalidateGeneratedSkinningPreparation();
        _adaptedPortingPreparation = candidatePreparation;
        _adaptedPortingPreparationIssue = null;
        _adaptedPortingPreparationRevision = _rigFittingRevision;
        _glbSkinTransferPlan = candidateTransferPlan;
        if (_bodyPoseEditorDirty)
            _committedBodyPoseControls = _draftBodyPoseControls;
        _bodyPoseEditorDirty = false;
        _bodyPoseDraftSnapshot = null;
        _rigPoseEditorDirty = false;
        _manualAlignmentEditorDirty = false;

        SkinnedModelPortingAnalysis analysis = candidatePreparation.Analysis;
        PopulateAdaptedBoneMappingTree(analysis);
        string details = analysis.Messages.Count == 0
            ? string.Empty
            : "\n" + string.Join(
                "\n",
                analysis.Messages.Select(message => "• " + message));
        CompatibilityText.Text =
            $"Weights-only fitting; donor joints: {analysis.ActiveDonorJointCount}; " +
            $"mapped: {analysis.JointMappings.Count}; target deform joints: " +
            $"{analysis.TargetDeformJointCount}; revision {_rigFittingRevision}." +
            details;
        AppendGlbSkinTransferPlanMessages(candidateTransferPlan);
        ReplacementModePanel.Background = new SolidColorBrush(
            candidateTransferPlan.CanReplace
                ? Color.FromRgb(255, 243, 205)
                : Color.FromRgb(254, 226, 226));
        PlanSummaryText.Text = "План ещё не построен для текущей позы.";
        StatusText.Text = candidateTransferPlan.CanReplace
            ? "Ручная weights-only подгонка применена и подготовлена. Проверьте fitting preview."
            : "Поза подготовлена, но skinned-план несовместим. " +
              "Проверьте подробные сообщения плана.";
        RefreshState();
        RigFittingStatusText.Text =
            candidateTransferPlan.CanReplace
                ? $"Поза и donor alignment применены транзакционно. Revision {_rigFittingRevision}. Длины и иерархия проверены."
                : $"Поза применена в revision {_rigFittingRevision}, но сохранение заблокировано проверкой PreparedScene.";
    }

    private void ResetRigJoint_Click(object sender, RoutedEventArgs e)
    {
        if (_nativeValidationRunning || !EnsureRigFittingState() ||
            _targetRigFittingPose is null || _rigLocalEulerDegrees is null ||
            RigFittingJointCombo.SelectedItem is not TargetRigJointItem selected)
            return;

        TargetRigFittingPoseSnapshot current = CaptureDisplayedRigFittingPose(
            localRotationsOnly: UsesGeneratedWeightsPortingMode);
        TargetRigFittingPose draft = CreateMutablePose(
            current,
            localRotationsOnly: UsesGeneratedWeightsPortingMode);
        draft.ResetLocalRotationDelta(selected.JointIndex);
        if (!_bodyPoseEditorDirty)
            _draftBodyPoseControls = _committedBodyPoseControls;
        _bodyPoseDraftSnapshot = draft.Capture();
        _bodyPoseEditorDirty = true;
        _rigLocalEulerDegrees[selected.JointIndex] = Vector3.Zero;
        LoadRigFittingEditorValues(
            includeRootValues: false,
            clearPendingRootValues: false);
        _plan = null;
        PlanSummaryText.Text = "Есть неприменённый сброс сустава.";
        RigFittingStatusText.Text =
            $"Вращение сустава {selected.Display} сброшено в preview. Нажмите «Применить».";
        RefreshState();
    }

    private void ResetRigPose_Click(object sender, RoutedEventArgs e)
    {
        if (_nativeValidationRunning || !EnsureRigFittingState() ||
            _targetRigFittingPose is null || _rigLocalEulerDegrees is null)
            return;

        _targetRigFittingPose.Reset();
        Array.Fill(_rigLocalEulerDegrees, Vector3.Zero);
        _rigRootEulerDegrees = Vector3.Zero;
        _rigPoseEditorDirty = false;
        _bodyPoseEditorDirty = false;
        _bodyPoseDraftSnapshot = null;
        _committedBodyPoseControls = default;
        _draftBodyPoseControls = default;
        _bodyPoseAutoFitDetails = null;
        WriteBodyPoseControls(default);
        LoadRigFittingEditorValues();
        CommitRigFittingChange("Поза игрового скелета сброшена в каноническую.");
    }

    private void ShowFittingPose_Changed(object sender, RoutedEventArgs e)
    {
        if (!IsLoaded)
            return;
        ClearFinalTexturedPreview();
        RefreshPreview();
    }

    private void CommitRigFittingChange(string status)
    {
        _rigFittingRevision++;
        _plan = null;
        _glbSkinTransferPlan = null;
        InvalidateAdaptedPortingPreparation();
        InvalidateGeneratedSkinningPreparation();
        PlanSummaryText.Text = "План ещё не построен для текущей позы.";

        if (UsesAdaptDonorWeightsPortingMode)
            UpdateAdaptedPortingPreparation();
        else if (UsesGeneratedWeightsPortingMode)
            UpdateGeneratedSkinningPreparation();
        RefreshState();
        RigFittingStatusText.Text =
            $"{status} Revision {_rigFittingRevision}. Длины и иерархия проверены.";
    }

    private void LoadRigFittingEditorValues(
        bool includeRootValues = true,
        bool clearPendingRootValues = true)
    {
        if (_rigLocalEulerDegrees is null || _targetRigFittingPose is null)
            return;
        TargetRigFittingPoseSnapshot displayed = CaptureDisplayedRigFittingPose(
            localRotationsOnly: UsesGeneratedWeightsPortingMode);
        Vector3 local = Vector3.Zero;
        if (RigFittingJointCombo.SelectedItem is TargetRigJointItem selected &&
            (uint)selected.JointIndex < (uint)displayed.LocalRotationDeltas.Count)
        {
            local = QuaternionToEulerDegrees(
                displayed.LocalRotationDeltas[selected.JointIndex]);
            _rigLocalEulerDegrees[selected.JointIndex] = local;
        }
        Vector3 rootTranslation = _targetRigFittingPose?.RootTranslation ?? Vector3.Zero;
        _settingRigFittingControls = true;
        try
        {
            RigLocalRotXSlider.Value = local.X;
            RigLocalRotYSlider.Value = local.Y;
            RigLocalRotZSlider.Value = local.Z;
            if (includeRootValues)
            {
                WriteVector3(
                    _rigRootEulerDegrees,
                    RigRootRotXBox,
                    RigRootRotYBox,
                    RigRootRotZBox);
                WriteVector3(
                    rootTranslation,
                    RigRootMoveXBox,
                    RigRootMoveYBox,
                    RigRootMoveZBox);
            }
            if (clearPendingRootValues)
                _rigPoseEditorDirty = false;
        }
        finally
        {
            _settingRigFittingControls = false;
        }
    }

    private static Vector3 ReadEulerDegrees(
        TextBox xBox,
        TextBox yBox,
        TextBox zBox,
        string description) =>
        ReadVector3(xBox, yBox, zBox, description);

    private static Vector3 ReadVector3(
        TextBox xBox,
        TextBox yBox,
        TextBox zBox,
        string description)
    {
        if (!TryReadFiniteFloat(xBox, out float x) ||
            !TryReadFiniteFloat(yBox, out float y) ||
            !TryReadFiniteFloat(zBox, out float z))
        {
            throw new InvalidOperationException(
                $"Поля {description} должны содержать конечные числа.");
        }
        return new Vector3(x, y, z);
    }

    private static bool TryReadFiniteFloat(TextBox box, out float value)
    {
        bool parsed = float.TryParse(
                          box.Text,
                          NumberStyles.Float,
                          CultureInfo.InvariantCulture,
                          out value) ||
                      float.TryParse(box.Text, out value);
        return parsed && float.IsFinite(value);
    }

    private static Quaternion EulerDegreesToQuaternion(Vector3 degrees) =>
        TargetRigEulerAngles.ToQuaternion(degrees);

    private static Vector3 QuaternionToEulerDegrees(Quaternion value) =>
        TargetRigEulerAngles.FromQuaternion(value);

    private static Vector3[] GetPoseEulerDegrees(
        TargetRigFittingPoseSnapshot snapshot)
    {
        Vector3[] values = new Vector3[snapshot.LocalRotationDeltas.Count];
        for (int jointIndex = 0; jointIndex < values.Length; jointIndex++)
        {
            values[jointIndex] = QuaternionToEulerDegrees(
                snapshot.LocalRotationDeltas[jointIndex]);
        }
        return values;
    }

    private void SynchronizeRigEulerFromPose(TargetRigFittingPose pose) =>
        _rigLocalEulerDegrees = GetPoseEulerDegrees(pose.Capture());

    private static void WriteVector3(
        Vector3 value,
        TextBox xBox,
        TextBox yBox,
        TextBox zBox)
    {
        xBox.Text = value.X.ToString("G9", CultureInfo.InvariantCulture);
        yBox.Text = value.Y.ToString("G9", CultureInfo.InvariantCulture);
        zBox.Text = value.Z.ToString("G9", CultureInfo.InvariantCulture);
    }

    private BodyPoseControlValues ReadBodyPoseControls() => new(
        checked((float)BodyArmRaiseSlider.Value),
        checked((float)BodyArmForwardSlider.Value),
        checked((float)BodyElbowBendSlider.Value),
        checked((float)BodyLegSpreadSlider.Value),
        checked((float)BodyKneeBendSlider.Value),
        checked((float)BodyTorsoPitchSlider.Value),
        checked((float)BodyNeckForwardSlider.Value));

    private void WriteBodyPoseControls(BodyPoseControlValues values)
    {
        _settingBodyPoseControls = true;
        try
        {
            BodyArmRaiseSlider.Value = values.ArmRaiseDegrees;
            BodyArmForwardSlider.Value = values.ArmForwardDegrees;
            BodyElbowBendSlider.Value = values.ElbowBendDegrees;
            BodyLegSpreadSlider.Value = values.LegSpreadDegrees;
            BodyKneeBendSlider.Value = values.KneeBendDegrees;
            BodyTorsoPitchSlider.Value = values.TorsoPitchDegrees;
            BodyNeckForwardSlider.Value = values.NeckForwardDegrees;
        }
        finally
        {
            _settingBodyPoseControls = false;
        }
    }

    private void RebaseBindPose_Changed(object sender, RoutedEventArgs e)
    {
        if (!IsLoaded)
            return;
        ClearFinalTexturedPreview();
        _plan = null;
        PlanSummaryText.Text = "План ещё не построен.";
        RefreshState();
    }

    private void BoneCombo_Changed(object sender, SelectionChangedEventArgs e) =>
        RefreshPreview();

    private static BoneItem? FindPreferredHeadBone(IEnumerable<BoneItem> items) =>
        items.FirstOrDefault(item =>
            item.Slot == 8 && item.Display.EndsWith(
                " Head", StringComparison.OrdinalIgnoreCase));

    private void AutoFit_Click(object sender, RoutedEventArgs e)
    {
        if (_sourceScene is null || _replacementScene is null) return;
        try
        {
            if (UsesGeneratedWeightsPortingMode)
                ApplyAutomaticGeneratedDonorAlignment();
            else
                ApplyAutoFit();
        }
        catch (Exception exception) { ShowError(exception); }
    }

    private void ApplyAutomaticGeneratedDonorAlignment()
    {
        ImportedScene inputScene = ResolveGeneratedSkinningInputScene();
        ReplacementTransform alignment =
            ComputeCoarseGeneratedDonorAlignment(inputScene);
        CommitGeneratedDonorAlignment(
            alignment,
            "Автоподгонка по полным bounds применена. Крылья и отдельные аксессуары " +
            "участвуют в границах — проверьте масштаб и положение вручную.");
    }

    private void ApplyAutoFit()
    {
        if (_sourceScene is null || _replacementScene is null)
            throw new InvalidOperationException(
                "Для автоподгонки нужны исходная модель и модель замены.");
        IEnumerable<Vector3> replacementPositions = _replacementRigidTextureBundle is null
            ? _replacementScene.Meshes.SelectMany(mesh => mesh.Positions)
            : _replacementRigidTextureBundle.MaterialGroups
                .SelectMany(group => group.Meshes)
                .SelectMany(mesh => mesh.Positions);
        ReplacementTransform fit = ReplacementTransformFitter.FitByHeightAndCenter(
            _sourceScene.Meshes.SelectMany(mesh => mesh.Positions),
            replacementPositions);
        ApplyTransform(fit);
    }

    private void ApplyTransform(ReplacementTransform fit)
    {
        WriteTransformEditor(fit);
        StatusText.Text = $"Автоподгонка: scale {fit.Scale:G5}; центры моделей совмещены.";
        _framePreviewOnRefresh = true;
        RefreshPreview();
    }

    private void ApplyModelAlignment_Click(object sender, RoutedEventArgs e)
    {
        if (_nativeValidationRunning || !UsesGeneratedWeightsPortingMode ||
            _document is null || _replacementScene is null)
            return;

        try
        {
            ReplacementTransform alignment = ReadGeneratedDonorAlignment();
            CommitGeneratedDonorAlignment(
                alignment,
                $"Размер и положение применены: scale {alignment.Scale:G5}.");
        }
        catch (Exception exception) when (exception is InvalidDataException or
                                          InvalidOperationException or
                                          OverflowException or
                                          ArgumentException or
                                          NotSupportedException)
        {
            _generatedAlignmentEditorDirty = true;
            StatusText.Text = "Размер и положение не применены: " + exception.Message;
            RefreshState();
        }
    }

    private void CommitGeneratedDonorAlignment(
        ReplacementTransform alignment,
        string successPrefix)
    {
        ValidateGeneratedDonorAlignment(alignment);
        bool alignmentChanged = _generatedDonorAlignment is null ||
            !Equals(_generatedDonorAlignment, alignment);
        bool discardedBodyDraft = alignmentChanged && _bodyPoseEditorDirty;
        bool invalidatedAutomaticPose = alignmentChanged &&
            _bodyPoseAutoFitResult is not null;
        if (discardedBodyDraft)
        {
            _bodyPoseEditorDirty = false;
            _bodyPoseDraftSnapshot = null;
            _draftBodyPoseControls = _committedBodyPoseControls;
            WriteBodyPoseControls(_committedBodyPoseControls);
        }
        _generatedDonorAlignment = alignment;
        _generatedDonorAlignmentDraft = alignment;
        _generatedAlignmentEditorDirty = false;
        if (alignmentChanged)
        {
            _bodyPoseAutoFitResult = null;
            _generatedBodySelection = null;
            _bodyPoseAutoFitDetails = null;
            BodyPoseStatusText.ToolTip = null;
        }
        _rigFittingRevision++;
        _plan = null;
        _glbSkinTransferPlan = null;
        InvalidateAdaptedPortingPreparation();
        InvalidateGeneratedSkinningPreparation();
        PlanSummaryText.Text =
            "План ещё не построен для текущего размера и положения модели.";
        WriteTransformEditor(alignment);
        UpdateGeneratedSkinningPreparation();
        RefreshState();

        if (discardedBodyDraft || invalidatedAutomaticPose)
        {
            BodyPoseStatusText.Text = discardedBodyDraft
                ? "Размер или положение модели изменились; старый черновик позы отброшен. " +
                  "Повторите автоподгонку тела."
                : "Размер или положение модели изменились. Сохранённая поза оставлена " +
                  "как ручная, но для нового положения нужно повторить автоподгонку тела.";
        }

        StatusText.Text = GeneratedSkinningPreparationIsCurrent
            ? successPrefix + " Автоматические веса пересчитаны."
            : successPrefix + " Подготовка весов пока заблокирована: " +
              (_generatedSkinningPreparationIssue ?? "неизвестная ошибка подготовки");
    }

    private void WriteTransformEditor(ReplacementTransform transform)
    {
        _settingModelTransform = true;
        try
        {
            ScaleBox.Text = transform.Scale.ToString("G9", CultureInfo.InvariantCulture);
            WriteVector3(
                transform.RotationDegrees,
                RotXBox,
                RotYBox,
                RotZBox);
            WriteVector3(
                transform.Translation,
                MoveXBox,
                MoveYBox,
                MoveZBox);
        }
        finally
        {
            _settingModelTransform = false;
        }
    }

    private void Transform_Changed(object sender, TextChangedEventArgs e)
    {
        if (_settingModelTransform || !IsLoaded)
            return;
        ClearFinalTexturedPreview();
        if (UsesAdaptDonorWeightsPortingMode && ManualAdaptWeights)
        {
            _manualAlignmentEditorDirty = true;
            _plan = null;
            PlanSummaryText.Text =
                "Есть неприменённые значения donor alignment.";
            RigFittingStatusText.Text =
                "Donor alignment ещё не применён; тяжёлая подготовка не выполняется во время ввода.";
            RefreshState();
        }
        else if (UsesGeneratedWeightsPortingMode)
        {
            _generatedAlignmentEditorDirty = true;
            _generatedDonorAlignmentDraft =
                TryReadGeneratedDonorAlignment(out ReplacementTransform alignment)
                    ? alignment
                    : null;
            _plan = null;
            _generatedPreparedSceneViewedRevision = -1;
            SetGeneratedSkinningConfirmation(false);
            PlanSummaryText.Text =
                "Есть неприменённые значения размера или положения модели.";
            StatusText.Text =
                "Размер и положение ещё не применены. Raw preview обновляется без " +
                "пересчёта весов; нажмите «Применить размер и положение».";
            RefreshState();
        }
        else if (_replacementSmoDocument is null)
            RefreshPreview();
    }

    private void Reset_Click(object sender, RoutedEventArgs e)
    {
        if (_nativeValidationRunning)
            return;

        _sourcePath = null; _document = null; _sourceScene = null;
        _replacementPath = null; _baseReplacementScene = null;
        _replacementScene = null; _plan = null;
        _replacementRigidTextureBundle = null;
        _textureCatalogResult = null;
        _externalTextures.Clear();
        ResetSkinnedMaterialOverrides();
        _multiTextureDirectory = null;
        _rigidTextureBindingIssue = null;
        _geometryOnlyFallbackIssue = null;
        _rigidMultiMaterialAnalysis = null;
        _replacementSmoDocument = null; _replacementSmoScene = null;
        _smoReplacementPlan = null; _glbSkinTransferPlan = null;
        ResetRigFittingState();
        InvalidateAdaptedPortingPreparation();
        InvalidateGeneratedSkinningPreparation(clearGeometryBase: true);
        _portingModeRecommendation = null;
        SetPreserveOriginalTextures(false);
        SetPortingModeChoice(PortingModeUiChoice.Auto);
        SourcePathText.Text = "Не выбран"; SourceSummaryText.Text = "—";
        ReplacementPathText.Text = "Не выбрана"; ReplacementSummaryText.Text = "—";
        BoneCombo.ItemsSource = null;
        PlanSummaryText.Text = "План ещё не построен.";
        SplitModeText.Text =
            "Вся модель помещается в один основной body-slot; остальные slots получают " +
            "невидимый вырожденный triangle.";
        PlanButton.Content = "Построить план и проверить";
        ReplacementModeText.Text = "Режим ещё не определён";
        CompatibilityText.Text = "Выберите модель-донор.";
        BoneMappingTree.Items.Clear();
        BoneMappingPanel.Visibility = Visibility.Collapsed;
        ReplacementModePanel.Background = new SolidColorBrush(
            Color.FromRgb(232, 238, 245));
        ScaleBox.Text = "1"; RotXBox.Text = RotYBox.Text = RotZBox.Text = "0";
        MoveXBox.Text = MoveYBox.Text = MoveZBox.Text = "0";
        StatusText.Text = "Выберите исходный SMO.";
        NativeValidationResultBorder.Visibility = Visibility.Collapsed;
        RefreshTextureList();
        _framePreviewOnRefresh = true;
        RefreshState();
    }

    private void RefreshState()
    {
        bool smoMode = _replacementSmoDocument is not null;
        bool preserveOriginalTextures = !smoMode && PreserveOriginalTextures;
        bool multiTextureMode = !smoMode && UseRigidMultiTextureMode;
        bool generatedWeightsMode = !smoMode && UsesGeneratedWeightsPortingMode;
        bool skinnedGlbMode = !smoMode &&
            (((UsesPreparedModelPortingMode || UsesAdaptDonorWeightsPortingMode) &&
              AllReplacementMeshesAreSkinned) ||
             (generatedWeightsMode &&
              GeneratedSkinningPreparationIsCurrent));
        bool canRunExternalPipeline = !smoMode && CanRunSelectedPortingPipeline;
        bool legacyRigidMode = !smoMode && UsesLegacyRigidPortingMode;
        string? generatedTransferBlocker = generatedWeightsMode
            ? GetGeneratedSkinningTransferBlocker()
            : null;
        bool generatedTransferCompatible = generatedTransferBlocker is null;
        bool preparationReady =
            (!UsesAdaptDonorWeightsPortingMode ||
             AdaptedPortingPreparationIsCurrent) &&
            (!generatedWeightsMode || GeneratedSkinningIsReady) &&
            !RigFittingEditorHasPendingChanges;
        ExternalModelOptionsPanel.IsEnabled = !smoMode;
        ExternalModelOptionsPanel.Opacity = smoMode ? 0.5 : 1;
        PlanButton.IsEnabled = !_nativeValidationRunning && !smoMode &&
            _document is not null && _replacementScene is not null &&
            canRunExternalPipeline && preparationReady &&
            generatedTransferCompatible;
        bool manualDonorAlignmentMode =
            UsesAdaptDonorWeightsPortingMode && ManualAdaptWeights;
        bool modelAlignmentMode =
            legacyRigidMode || manualDonorAlignmentMode || generatedWeightsMode;
        AutoFitButton.IsEnabled = !_nativeValidationRunning &&
            (legacyRigidMode || generatedWeightsMode) &&
            _sourceScene is not null && _replacementScene is not null;
        TransformEditorGrid.IsEnabled =
            !_nativeValidationRunning && modelAlignmentMode;
        TransformEditorGrid.Opacity = TransformEditorGrid.IsEnabled ? 1 : 0.55;
        bool rotationAvailable = !_nativeValidationRunning &&
            (legacyRigidMode || manualDonorAlignmentMode);
        RotXBox.IsEnabled = RotYBox.IsEnabled = RotZBox.IsEnabled = rotationAvailable;
        RotationLabelText.Opacity = rotationAvailable ? 1 : 0.55;
        RotXBox.Opacity = RotYBox.Opacity = RotZBox.Opacity =
            rotationAvailable ? 1 : 0.55;
        RotationUnavailableHintText.Visibility = generatedWeightsMode
            ? Visibility.Visible
            : Visibility.Collapsed;
        ApplyModelAlignmentButton.Visibility = generatedWeightsMode
            ? Visibility.Visible
            : Visibility.Collapsed;
        ApplyModelAlignmentButton.IsEnabled = !_nativeValidationRunning &&
            generatedWeightsMode && _document is not null &&
            _replacementScene is not null && _generatedAlignmentEditorDirty &&
            TryReadGeneratedDonorAlignment(out _);
        ModelTransformHeadingText.Text = manualDonorAlignmentMode
            ? "3. Coherent donor alignment для ручной подгонки"
            : "3. Положение и масштаб";
        ModelTransformPanel.Opacity = smoMode ? 0.65 : 1;
        ModelTransformHintText.Text = smoMode
            ? "Для SMO → SMO положение и масштаб берутся из готовой donor-модели."
            : !HasExternalReplacement
                ? "Выберите внешнюю модель-донор и режим портирования."
                : UsesPreparedModelPortingMode
                    ? "Режим 1 требует модель, уже подготовленную точно под игровой " +
                      "скелет. Масштабирование сдвинуло бы геометрию относительно " +
                      "неизменных костей; для подгонки выберите режим 2 или 3."
                : generatedWeightsMode
                    ? "Сначала подгоните рост и положение модели, затем настраивайте " +
                      "игровой скелет. Автоподгонка использует полные bounds, поэтому " +
                      "крылья и отдельные аксессуары могут потребовать ручной коррекции."
                : manualDonorAlignmentMode
                    ? "Размер, поворот и положение задают единый weights-only donor alignment. " +
                      "Применение выполняется общей кнопкой редактора скелета ниже."
                : UsesAdaptDonorWeightsPortingMode
                    ? "Автоматический режим 2 использует donor bind. Включите ручную " +
                      "weights-only подгонку, чтобы изменить положение модели."
                : legacyRigidMode
                    ? "Подгоните rigid-модель по росту и положению перед построением плана."
                    : "Выбранный режим не использует ручную подгонку модели.";
        SaveButton.IsEnabled = !_nativeValidationRunning && (smoMode
            ? _smoReplacementPlan?.CanReplace == true
            : !canRunExternalPipeline || !preparationReady
                ? false
                : multiTextureMode
                ? _rigidMultiMaterialAnalysis?.CanPack == true
                : _plan is not null && (!skinnedGlbMode ||
                    _glbSkinTransferPlan?.CanReplace == true));
        SaveButton.Content = smoMode
            ? "Создать SMO-подмену"
            : !canRunExternalPipeline && HasExternalReplacement
                ? "4. Создание SMO пока недоступно"
                : multiTextureMode
                ? "4. Создать multi-texture SMO"
            : skinnedGlbMode
                ? "4. Создать skinned SMO"
                : "4. Создать новый SMO";
        if (!smoMode)
        {
            PlanButton.Content = !canRunExternalPipeline && HasExternalReplacement
                    ? "План недоступен для выбранного режима"
                : RigFittingEditorHasPendingChanges
                    ? "Сначала примените значения подгонки"
                : generatedWeightsMode && generatedTransferBlocker is not null
                    ? IsMaterialGroupTransferBlocker(generatedTransferBlocker)
                        ? "Сначала исправьте материалы или текстуры"
                        : "Сначала исправьте несовместимость модели"
                : generatedWeightsMode && !GeneratedSkinningPreparationIsCurrent
                    ? "Подготовка автоматических весов не завершена"
                : generatedWeightsMode && !GeneratedPreparedSceneViewedForCurrentRevision
                    ? "Сначала покажите итоговую модель"
                : generatedWeightsMode && !GeneratedSkinningIsConfirmed
                    ? "Сначала подтвердите итоговую модель"
                : UsesAdaptDonorWeightsPortingMode &&
                  !AdaptedPortingPreparationIsCurrent
                    ? "Адаптация весов не завершена"
                : legacyRigidMode && preserveOriginalTextures &&
                  _replacementRigidTextureBundle is not null
                ? "Проверить геометрию с исходной текстурой"
                : multiTextureMode
                    ? "Проверить multi-texture структуру"
                    : generatedWeightsMode
                        ? "Проверить созданные веса и построить palettes"
                    : skinnedGlbMode
                        ? "Проверить кости и построить palettes"
                        : "Построить план и проверить";
        }
        RigidBonePanel.Visibility = legacyRigidMode
            ? Visibility.Visible : Visibility.Collapsed;
        SkinnedGlbOptionsPanel.Visibility = UsesPreparedModelPortingMode &&
                                            skinnedGlbMode
            ? Visibility.Visible : Visibility.Collapsed;
        GeneratedSkinningPanel.Visibility = generatedWeightsMode
            ? Visibility.Visible : Visibility.Collapsed;
        UpdateGeneratedSkinningConfirmationAvailability();
        bool exactGeneratedSceneVisible = generatedWeightsMode &&
            GeneratedSkinningPreparationIsCurrent &&
            ShowFittingPoseCheckBox.IsChecked != true &&
            !_generatedAlignmentEditorDirty;
        ShowGeneratedPreparedSceneButton.IsEnabled = !_nativeValidationRunning &&
            generatedWeightsMode && GeneratedSkinningPreparationIsCurrent &&
            !RigFittingEditorHasPendingChanges &&
            (!exactGeneratedSceneVisible ||
             !GeneratedPreparedSceneViewedForCurrentRevision);
        ShowGeneratedPreparedSceneButton.Content =
            !GeneratedSkinningPreparationIsCurrent
                ? "Итоговая модель пока недоступна"
                : RigFittingEditorHasPendingChanges
                    ? "Сначала примените изменения подгонки"
                    : exactGeneratedSceneVisible &&
                      !GeneratedPreparedSceneViewedForCurrentRevision
                        ? "Подтвердить просмотр показанной итоговой модели"
                        : exactGeneratedSceneVisible
                            ? "Итоговая модель показана"
                        : GeneratedPreparedSceneViewedForCurrentRevision
                            ? "Снова показать итоговую модель"
                            : "Показать итог и разрешить подтверждение";
        UpdateGeneratedSkinningPrimaryStatus();
        BoneHighlightText.Visibility = legacyRigidMode
            ? Visibility.Visible : Visibility.Collapsed;
        BoneCombo.IsEnabled = legacyRigidMode && !multiTextureMode;
        TextureResourcesPanel.IsEnabled = !smoMode && _baseReplacementScene is not null;
        TextureResourcesPanel.Opacity = TextureResourcesPanel.IsEnabled ? 1 : 0.65;
        TextureImportSourcesPanel.IsEnabled = TextureResourcesPanel.IsEnabled &&
            !preserveOriginalTextures;
        TextureImportSourcesPanel.Opacity = TextureImportSourcesPanel.IsEnabled ? 1 : 0.55;
        SelectTextureFolderButton.IsEnabled = TextureImportSourcesPanel.IsEnabled &&
            !generatedWeightsMode;
        Mode3TextureFolderWarningText.Visibility = generatedWeightsMode
            ? Visibility.Visible
            : Visibility.Collapsed;
        Mode3TextureFolderWarningText.Text = generatedWeightsMode &&
            _replacementRigidTextureBundle is not null
                ? "Обнаруженный rigid matN bundle здесь полностью игнорируется. Режим 3 использует всю геометрию исходного файла; добавляйте однозначно сопоставимые изображения кнопкой «Добавить файлы…»."
                : "В режиме 3 папки matN не поддерживаются: создаваемые веса всегда используют полную геометрию исходного файла. Добавляйте однозначно сопоставимые изображения кнопкой «Добавить файлы…».";
        if (_showFinalTexturedPreview && !CanShowFinalTexturedPreview)
            ClearFinalTexturedPreview();
        bool finalTexturedPreviewAvailable = CanShowFinalTexturedPreview;
        ShowFinalTexturedPreviewButton.IsEnabled =
            finalTexturedPreviewAvailable && !_showFinalTexturedPreview;
        ShowFinalTexturedPreviewButton.Content = _showFinalTexturedPreview
            ? "Окончательный результат с текстурами показан"
            : PreserveOriginalTextures
                ? "Для итогового preview включите текстуры донора"
                : RigFittingEditorHasPendingChanges
                    ? "Сначала примените размер или позу"
                    : _replacementScene is not { Textures.Count: > 0 }
                        ? "Сначала добавьте и привяжите текстуры"
                        : !finalTexturedPreviewAvailable
                            ? "Окончательный результат пока не готов"
                            : "Показать окончательный результат с текстурами";
        UpdateRemoveTexturesAvailability();
        UpdateMaterialOverrideAvailability();
        UpdatePortingModePresentation();
        RefreshRigFittingState();
        RefreshPreview();
    }

    private void RefreshRigFittingState()
    {
        bool mode2 = UsesAdaptDonorWeightsPortingMode;
        bool mode3 = UsesGeneratedWeightsPortingMode;
        bool rigMode = mode2 || mode3;
        RigFittingPanel.Visibility = rigMode
            ? Visibility.Visible
            : Visibility.Collapsed;
        if (!rigMode)
            return;

        bool hasRig = EnsureRigFittingState();
        bool jointsMode = IsJointPoseEditorMode;
        RigFittingEditorPanel.Visibility = jointsMode
            ? Visibility.Visible
            : Visibility.Collapsed;
        HumanPoseEditorPanel.Visibility = jointsMode
            ? Visibility.Collapsed
            : Visibility.Visible;
        GeneratedAttachmentEditorPanel.Visibility = mode3 && !jointsMode
            ? Visibility.Visible
            : Visibility.Collapsed;
        ManualAdaptWeightsCheckBox.Visibility = mode2
            ? Visibility.Visible
            : Visibility.Collapsed;
        ManualAdaptWeightsCheckBox.IsEnabled =
            mode2 && AllReplacementMeshesAreSkinned && !_nativeValidationRunning;
        bool editorEnabled = hasRig && !_nativeValidationRunning &&
            (mode3 || ManualAdaptWeights) &&
            (!mode3 || !_generatedAlignmentEditorDirty);
        AutoFitBodyPoseButton.IsEnabled = !jointsMode && editorEnabled &&
            !_rigPoseEditorDirty &&
            _baseReplacementScene is { Meshes.Count: > 0 };
        bool bodyControlsEnabled = !jointsMode && editorEnabled &&
            !_rigPoseEditorDirty;
        BodyArmRaiseSlider.IsEnabled = bodyControlsEnabled;
        BodyArmForwardSlider.IsEnabled = bodyControlsEnabled;
        BodyElbowBendSlider.IsEnabled = bodyControlsEnabled;
        BodyLegSpreadSlider.IsEnabled = bodyControlsEnabled;
        BodyKneeBendSlider.IsEnabled = bodyControlsEnabled;
        BodyTorsoPitchSlider.IsEnabled = bodyControlsEnabled;
        BodyNeckForwardSlider.IsEnabled = bodyControlsEnabled;
        bool bodyPreparationIsCurrent = mode3
            ? GeneratedSkinningPreparationIsCurrent
            : AdaptedPortingPreparationIsCurrent;
        ApplyBodyPoseButton.IsEnabled = editorEnabled &&
            !_rigPoseEditorDirty &&
            ((_bodyPoseEditorDirty && _bodyPoseDraftSnapshot is not null) ||
             !bodyPreparationIsCurrent);
        ApplyBodyPoseButton.Content = !hasRig
            ? "Скелет недоступен"
            : _nativeValidationRunning
                ? "Идёт проверка…"
                : mode2 && !ManualAdaptWeights
                    ? "Включите ручную подгонку"
                    : mode3 && _generatedAlignmentEditorDirty
                        ? "Сначала: размер и центр"
                        : _rigPoseEditorDirty
                            ? "Сначала исправьте root-поля"
                            : _bodyPoseEditorDirty
                                ? "Применить изменения"
                                : bodyPreparationIsCurrent
                                    ? "Поза уже применена"
                                    : "Применить текущую позу";
        ResetBodyPoseButton.IsEnabled = editorEnabled;
        RigFittingEditorPanel.IsEnabled = jointsMode && editorEnabled;
        RigFittingEditorPanel.Opacity = RigFittingEditorPanel.IsEnabled ? 1 : 0.55;
        RigRootTransformExpander.Visibility =
            jointsMode && mode2 && ManualAdaptWeights
                ? Visibility.Visible
                : Visibility.Collapsed;
        RigRootTransformPanel.IsEnabled =
            jointsMode && editorEnabled && mode2 && ManualAdaptWeights;
        RigRootTransformPanel.Opacity = RigRootTransformPanel.IsEnabled ? 1 : 0.5;
        ShowFittingPoseCheckBox.IsEnabled = hasRig && !_nativeValidationRunning;
        RigFittingModeHintText.Text = mode3
            ? jointsMode
                ? "Выбирайте сустав в списке или прямо в окне просмотра и задавайте " +
                  "абсолютные local-углы X/Y/Z. Скелет всегда показан поверх модели."
                : "После размера и положения используйте автоподгонку или симметричные " +
                  "ползунки тела. Длины костей и связи между ними остаются прежними."
            : ManualAdaptWeights
                ? jointsMode
                    ? "Ручной weights-only режим: суставы и root-transform работают с " +
                      "той же общей позой, что и режим «Человек»."
                    : "Ручной weights-only режим: сначала задайте положение donor, затем " +
                      "используйте автоподгонку или симметричные ползунки тела."
                : "Используется автоматическая адаптация donor bind. Включите ручную " +
                  "подгонку, если хотите менять позу тела самостоятельно.";
        BoneHighlightText.Visibility = Visibility.Visible;
        BoneHighlightText.Text = jointsMode
            ? "Красный — выбранная deform-кость · голубой — deform · жёлтый — service ancestor"
            : mode3
                ? "Фиолетовый — выбранная жёсткая деталь · голубой — deform · жёлтый — service ancestor"
                : "Голубой — deform · жёлтый — service ancestor";
        UpdateGeneratedAttachmentEditorAvailability();
    }

    private void RefreshPreview()
    {
        if (SceneVisual is null) return;
        _generatedAttachmentMeshByModel.Clear();
        _generatedAttachmentPreviewCenters.Clear();
        ClearGeneratedAttachmentScreenOverlay();
        var group = new Model3DGroup();
        var all = new List<Point3D>();
        bool renderedCurrentGeneratedPreparedScene = false;
        bool showFinalTexturedPreview = _showFinalTexturedPreview &&
            _finalTexturedPreviewScene is not null &&
            CanShowFinalTexturedPreview;
        bool showFittingPose = !showFinalTexturedPreview &&
            ShowFittingPoseCheckBox.IsChecked == true;
        SmoExportScene? sourcePreviewScene = showFinalTexturedPreview
            ? null
            : _sourceScene;
        bool showTargetFittingPose =
            showFittingPose &&
            (UsesGeneratedWeightsPortingMode ||
             (UsesAdaptDonorWeightsPortingMode && ManualAdaptWeights)) &&
            _targetRigDefinition is not null &&
            _targetRigFittingPose is not null &&
            _rigLocalEulerDegrees is not null;
        if (sourcePreviewScene is not null && showTargetFittingPose)
        {
            try
            {
                TargetRigFittingPoseSnapshot targetPreviewPose =
                    CaptureDisplayedRigFittingPose(
                        localRotationsOnly: UsesGeneratedWeightsPortingMode);
                sourcePreviewScene = TargetRigFittingPreviewBuilder.Build(
                    sourcePreviewScene,
                    targetPreviewPose).Scene;
            }
            catch (Exception exception) when (exception is InvalidDataException or
                                              InvalidOperationException or
                                              OverflowException or
                                              ArgumentException)
            {
                // A target-pose preview is diagnostic only. Keep the canonical
                // gray target visible and leave donor preparation/writer state
                // untouched when preview skinning cannot be evaluated.
                sourcePreviewScene = _sourceScene;
                RigFittingStatusText.Text =
                    "Не удалось показать исходную модель в позе подгонки: " +
                    exception.Message + " Показана каноническая серая модель.";
            }
        }
        if (sourcePreviewScene is not null)
            foreach (SmoExportMesh mesh in sourcePreviewScene.Meshes)
                AddModel(group, mesh.Positions, mesh.TriangleIndices, Color.FromArgb(75, 170, 180, 190), all);
        if (_replacementScene is not null)
        {
            ImportedScene previewScene = showFinalTexturedPreview
                ? _finalTexturedPreviewScene!
                : _replacementScene;
            Matrix4x4 transform = showFinalTexturedPreview
                ? _finalTexturedPreviewTransform
                : UsesLegacyRigidPortingMode
                    ? ReadTransform(false).Matrix
                    : Matrix4x4.Identity;
            SetPortingPreviewStatus(null);
            if (showFinalTexturedPreview)
            {
                renderedCurrentGeneratedPreparedScene =
                    UsesGeneratedWeightsPortingMode &&
                    GeneratedSkinningPreparationIsCurrent;
                SetPortingPreviewStatus(
                    "Показан полный окончательный writer-вход с импортируемыми текстурами. " +
                    "Серая target-модель, скелет и изоляция деталей временно скрыты.");
            }
            else if (UsesGeneratedWeightsPortingMode)
            {
                if (GeneratedSkinningPreparationIsCurrent &&
                    !_generatedAlignmentEditorDirty)
                {
                    previewScene = showFittingPose
                        ? _generatedSkinningPreparation!.FittingPreviewScene
                        : _generatedSkinningPreparation!.PreparedScene;
                    renderedCurrentGeneratedPreparedScene =
                        !showFittingPose &&
                        ReferenceEquals(
                            previewScene,
                            _generatedSkinningPreparation.PreparedScene);
                    transform = Matrix4x4.Identity;
                    SetPortingPreviewStatus(showFittingPose
                        ? "Показана временная fitting pose с автоматически созданными весами; SMO сохранит канонический PreparedScene."
                        : GeneratedSkinningIsConfirmed
                            ? "Показан точный канонический PreparedScene writer-а. Результат подтверждён."
                            : "Показан точный канонический PreparedScene writer-а. Проверьте результат и подтвердите его слева.");
                }
                else
                {
                    previewScene = _generatedSkinningEffectiveScene ??
                        _generatedSkinningBaseScene ??
                        _replacementScene;
                    bool pendingAlignmentValid =
                        TryReadGeneratedDonorAlignment(
                            out ReplacementTransform pendingAlignment);
                    ReplacementTransform? previewAlignment =
                        _generatedAlignmentEditorDirty && pendingAlignmentValid
                            ? pendingAlignment
                            : _generatedDonorAlignment;
                    transform = previewAlignment?.Matrix ?? Matrix4x4.Identity;
                    SetPortingPreviewStatus(
                        _generatedAlignmentEditorDirty
                            ? pendingAlignmentValid
                                ? "Показан raw preview с введённым размером и положением; " +
                                  "веса ещё не пересчитаны. Нажмите «Применить размер и положение»."
                                : "В полях размера или положения есть ошибка. Показан raw " +
                                  "preview с последним применённым alignment."
                            : (_generatedSkinningPreparationIssue ??
                               "Автоматические веса для предпросмотра не подготовлены.") +
                              " Показана raw-модель с сохранённым alignment; " +
                              "plan/save остаются заблокированными.");
                }
            }
            else if (UsesAdaptDonorWeightsPortingMode)
            {
                if (AdaptedPortingPreparationIsCurrent)
                {
                    previewScene = showFittingPose
                        ? _adaptedPortingPreparation!.FittingPreviewScene
                        : _adaptedPortingPreparation!.PreparedScene;
                    transform = Matrix4x4.Identity;
                    SetPortingPreviewStatus(showFittingPose && ManualAdaptWeights
                        ? "Показана временная weights-only fitting pose и coherent donor alignment."
                        : showFittingPose
                            ? "Автоматический donor-bind режим не имеет отдельной fitting geometry; показан его подготовленный результат."
                            : "Показан точный канонический PreparedScene writer-а.");
                }
                else
                {
                    SetPortingPreviewStatus(
                        (_adaptedPortingPreparationIssue ??
                         "Адаптация весов для предпросмотра не подготовлена.") +
                        " Показана исходная модель.");
                }
            }
            else if (UsesPreparedModelPortingMode)
            {
                if (_document is not null && _glbSkinTransferPlan?.CanReplace == true)
                {
                    try
                    {
                        previewScene = SmoSkinnedGlbReplacer.PrepareGeometryPreview(
                            _document,
                            _replacementScene,
                            ReplacementTransform.Identity,
                            RebaseBindPoseCheckBox.IsChecked == true
                                ? SkinnedGeometryTransferMode.RetargetToGameBindPose
                                : SkinnedGeometryTransferMode.PreservePreparedGeometry,
                            SelectedSkinnedTextureTransferMode,
                            ResolveSkinnedMaterialProfile(
                                _replacementScene,
                                SelectedSkinnedTextureTransferMode));
                        // PrepareGeometryPreview already applies the exact writer
                        // geometry path. Applying the editor transform again would
                        // make preview and saved SMO disagree.
                        transform = Matrix4x4.Identity;
                    }
                    catch (Exception exception)
                    {
                        SetPortingPreviewStatus(
                            "Не удалось подготовить позу для предпросмотра: " +
                            exception.Message + " Показана исходная модель.");
                    }
                }
                else
                {
                    SetPortingPreviewStatus(
                        "Точная поза игры пока не показана: строгая проверка костей не пройдена. " +
                        "Показана исходная модель.");
                }
            }
            bool showAttachmentSelection = !showFinalTexturedPreview &&
                CanSelectGeneratedAttachments;
            bool hideGeneratedMainBody = showAttachmentSelection &&
                HideGeneratedMainBodyCheckBox.IsChecked == true;
            if (showAttachmentSelection)
                UpdateGeneratedAttachmentPreviewCenters(previewScene, transform);
            Dictionary<int, Material>? textureMaterials =
                showFinalTexturedPreview ? [] : null;
            for (int meshIndex = 0; meshIndex < previewScene.Meshes.Count; meshIndex++)
            {
                ImportedMesh mesh = previewScene.Meshes[meshIndex];
                uint[] renderedIndices = hideGeneratedMainBody
                    ? RemoveGeneratedMainBodyTriangles(
                        meshIndex,
                        mesh.TriangleIndices)
                    : mesh.TriangleIndices;
                if (renderedIndices.Length == 0)
                    continue;
                Vector3[] renderedPositions = mesh.Positions
                    .Select(value => Vector3.Transform(value, transform))
                    .ToArray();
                GeometryModel3D model = showFinalTexturedPreview
                    ? AddTexturedModel(
                        group,
                        previewScene,
                        mesh,
                        renderedPositions,
                        renderedIndices,
                        transform,
                        textureMaterials!,
                        all)
                    : AddModel(
                        group,
                        renderedPositions,
                        renderedIndices,
                        Color.FromArgb(190, 255, 125, 35),
                        all,
                        boundsFromIndices: hideGeneratedMainBody);
                if (showAttachmentSelection)
                {
                    _generatedAttachmentMeshByModel[model] = meshIndex;
                    AddGeneratedAttachmentHighlights(
                        group,
                        meshIndex,
                        renderedPositions,
                        renderedIndices);
                }
            }
        }
        else if (_replacementSmoScene is not null)
        {
            foreach (SmoExportMesh mesh in _replacementSmoScene.Meshes)
                AddModel(group, mesh.Positions, mesh.TriangleIndices,
                    Color.FromArgb(190, 255, 125, 35), all);
        }
        ResolveSelectedBonePosition();
        SceneVisual.Content = group;
        if (showFinalTexturedPreview)
            ClearRigSkeletonOverlay();
        else if (UsesAdaptDonorWeightsPortingMode || UsesGeneratedWeightsPortingMode)
            PrepareRigSkeletonOverlay(showFittingPose);
        else
            ClearRigSkeletonOverlay();
        if (_explicitGeneratedReviewRequested &&
            renderedCurrentGeneratedPreparedScene &&
            GeneratedSkinningPreparationIsCurrent)
        {
            _generatedPreparedSceneViewedRevision = _rigFittingRevision;
            UpdateGeneratedSkinningConfirmationAvailability();
            UpdateGeneratedSkinningPrimaryStatus();
        }
        _previewBounds.Clear();
        _previewBounds.AddRange(all);
        if (_framePreviewOnRefresh && all.Count > 0)
        {
            Frame(all);
            _framePreviewOnRefresh = false;
        }
        UpdateBoneMarkerOverlay();
        UpdateRigSkeletonScreenOverlay();
        UpdateGeneratedAttachmentScreenOverlay();
    }

    private void SetPortingPreviewStatus(string? message)
    {
        if (PortingPreviewStatusText is null)
            return;
        PortingPreviewStatusText.Text = message ?? string.Empty;
        PortingPreviewStatusText.Visibility = string.IsNullOrWhiteSpace(message)
            ? Visibility.Collapsed
            : Visibility.Visible;
    }

    private void UpdateGeneratedAttachmentPreviewCenters(
        ImportedScene previewScene,
        Matrix4x4 transform)
    {
        if (_generatedSkinningPreparation is null)
            return;

        foreach (GeneratedSkinningAttachment attachment in
                 _generatedSkinningPreparation.Analysis.Attachments)
        {
            if (!_selectedGeneratedAttachmentComponents.Contains(
                    attachment.ComponentIndex))
                continue;

            Vector3 sum = Vector3.Zero;
            int count = 0;
            foreach (TargetRigBodyVertexMembership membership in
                     attachment.VerticesByMesh)
            {
                if ((uint)membership.MeshIndex >= (uint)previewScene.Meshes.Count)
                    continue;
                Vector3[] positions = previewScene.Meshes[membership.MeshIndex].Positions;
                foreach (int vertexIndex in membership.VertexIndices)
                {
                    if ((uint)vertexIndex >= (uint)positions.Length)
                        continue;
                    sum += Vector3.Transform(positions[vertexIndex], transform);
                    count++;
                }
            }

            if (count > 0)
            {
                Vector3 center = sum / count;
                _generatedAttachmentPreviewCenters[attachment.ComponentIndex] =
                    new Point3D(center.X, center.Y, center.Z);
            }
        }
    }

    private void AddGeneratedAttachmentHighlights(
        Model3DGroup group,
        int meshIndex,
        Vector3[] positions,
        uint[] triangleIndices)
    {
        if (_generatedSkinningPreparation is null ||
            _selectedGeneratedAttachmentComponents.Count == 0)
            return;

        foreach (GeneratedSkinningAttachment attachment in
                 _generatedSkinningPreparation.Analysis.Attachments)
        {
            if (!_selectedGeneratedAttachmentComponents.Contains(
                    attachment.ComponentIndex))
                continue;
            TargetRigBodyVertexMembership? membership = attachment.VerticesByMesh
                .FirstOrDefault(value => value.MeshIndex == meshIndex);
            if (membership is null || membership.VertexIndices.Count == 0)
                continue;

            HashSet<int> selectedVertices = membership.VertexIndices.ToHashSet();
            var selectedTriangles = new Int32Collection();
            for (int index = 0; index + 2 < triangleIndices.Length; index += 3)
            {
                int first = checked((int)triangleIndices[index]);
                int second = checked((int)triangleIndices[index + 1]);
                int third = checked((int)triangleIndices[index + 2]);
                if (!selectedVertices.Contains(first) ||
                    !selectedVertices.Contains(second) ||
                    !selectedVertices.Contains(third))
                    continue;
                selectedTriangles.Add(first);
                selectedTriangles.Add(second);
                selectedTriangles.Add(third);
            }
            if (selectedTriangles.Count == 0)
                continue;

            Point3D centerPoint = _generatedAttachmentPreviewCenters.TryGetValue(
                attachment.ComponentIndex,
                out Point3D storedCenter)
                    ? storedCenter
                    : new Point3D(
                        attachment.AlignedCenter.X,
                        attachment.AlignedCenter.Y,
                        attachment.AlignedCenter.Z);
            var center = new Vector3(
                checked((float)centerPoint.X),
                checked((float)centerPoint.Y),
                checked((float)centerPoint.Z));
            var highlightPoints = new Point3DCollection(positions.Length);
            const float highlightScale = 1.012f;
            for (int vertexIndex = 0; vertexIndex < positions.Length; vertexIndex++)
            {
                Vector3 value = positions[vertexIndex];
                if (selectedVertices.Contains(vertexIndex))
                    value = center + (value - center) * highlightScale;
                highlightPoints.Add(new Point3D(value.X, value.Y, value.Z));
            }

            var geometry = new MeshGeometry3D
            {
                Positions = highlightPoints,
                TriangleIndices = selectedTriangles
            };
            var material = new MaterialGroup();
            material.Children.Add(new DiffuseMaterial(
                new SolidColorBrush(Color.FromArgb(210, 238, 64, 255))));
            material.Children.Add(new EmissiveMaterial(
                new SolidColorBrush(Color.FromArgb(180, 210, 20, 255))));
            var model = new GeometryModel3D(geometry, material)
            {
                BackMaterial = material
            };
            group.Children.Add(model);
            _generatedAttachmentMeshByModel[model] = meshIndex;
        }
    }

    private void ClearGeneratedAttachmentScreenOverlay()
    {
        if (RigOverlayCanvas is null)
            return;
        foreach (UIElement element in _generatedAttachmentOverlayElements)
            RigOverlayCanvas.Children.Remove(element);
        _generatedAttachmentOverlayElements.Clear();
    }

    private void UpdateGeneratedAttachmentScreenOverlay()
    {
        ClearGeneratedAttachmentScreenOverlay();
        if (!CanSelectGeneratedAttachments || RigOverlayCanvas is null)
            return;

        foreach ((int componentIndex, Point3D center) in
                 _generatedAttachmentPreviewCenters.OrderBy(pair => pair.Key))
        {
            if (!TryProjectToPreview(center, out Point screen, out _))
                continue;
            var label = new Border
            {
                Background = new SolidColorBrush(Color.FromArgb(225, 150, 20, 180)),
                BorderBrush = Brushes.White,
                BorderThickness = new Thickness(1),
                CornerRadius = new CornerRadius(8),
                Padding = new Thickness(5, 1, 5, 1),
                Child = new TextBlock
                {
                    Text = $"#{componentIndex}",
                    Foreground = Brushes.White,
                    FontSize = 11,
                    FontWeight = FontWeights.SemiBold
                }
            };
            Canvas.SetLeft(label, screen.X + 7);
            Canvas.SetTop(label, screen.Y - 10);
            RigOverlayCanvas.Children.Add(label);
            _generatedAttachmentOverlayElements.Add(label);
        }
    }

    private uint[] RemoveGeneratedMainBodyTriangles(
        int meshIndex,
        uint[] triangleIndices)
    {
        if (_generatedBodySelection is null)
            return triangleIndices;

        HashSet<int> bodyVertices = _generatedBodySelection.Components
            .SelectMany(component => component.VerticesByMesh)
            .Where(membership => membership.MeshIndex == meshIndex)
            .SelectMany(membership => membership.VertexIndices)
            .ToHashSet();
        if (bodyVertices.Count == 0)
            return triangleIndices;

        var visible = new List<uint>(triangleIndices.Length);
        for (int index = 0; index + 2 < triangleIndices.Length; index += 3)
        {
            uint first = triangleIndices[index];
            uint second = triangleIndices[index + 1];
            uint third = triangleIndices[index + 2];
            if (bodyVertices.Contains(checked((int)first)) &&
                bodyVertices.Contains(checked((int)second)) &&
                bodyVertices.Contains(checked((int)third)))
            {
                continue;
            }
            visible.Add(first);
            visible.Add(second);
            visible.Add(third);
        }
        return visible.ToArray();
    }

    private static GeometryModel3D AddTexturedModel(
        Model3DGroup group,
        ImportedScene scene,
        ImportedMesh mesh,
        Vector3[] positions,
        uint[] indices,
        Matrix4x4 transform,
        IDictionary<int, Material> materialCache,
        List<Point3D> all)
    {
        ImportedMaterial sourceMaterial = scene.Materials[mesh.MaterialIndex];
        int textureIndex = sourceMaterial.BaseColorTextureIndex;
        if (!materialCache.TryGetValue(textureIndex, out Material? material))
        {
            ImportedTexture texture = scene.Textures[textureIndex];
            try
            {
                using SixLabors.ImageSharp.Image<
                    SixLabors.ImageSharp.PixelFormats.Bgra32> image =
                    SixLabors.ImageSharp.Image.Load<
                        SixLabors.ImageSharp.PixelFormats.Bgra32>(texture.Data);
                byte[] pixels = new byte[checked(image.Width * image.Height * 4)];
                image.CopyPixelDataTo(pixels);
                BitmapSource bitmap = BitmapSource.Create(
                    image.Width,
                    image.Height,
                    96,
                    96,
                    System.Windows.Media.PixelFormats.Bgra32,
                    null,
                    pixels,
                    checked(image.Width * 4));
                bitmap.Freeze();
                var brush = new ImageBrush(bitmap)
                {
                    TileMode = TileMode.Tile,
                    ViewportUnits = BrushMappingMode.Absolute,
                    Viewport = new Rect(0, 0, 1, 1),
                    Stretch = Stretch.Fill
                };
                brush.Freeze();
                var diffuse = new DiffuseMaterial(brush);
                diffuse.Freeze();
                material = diffuse;
            }
            catch (Exception exception)
            {
                throw new InvalidDataException(
                    $"Текстура \"{texture.Name}\" не декодируется для preview.",
                    exception);
            }
            materialCache.Add(textureIndex, material);
        }

        var points = new Point3DCollection(positions.Length);
        foreach (Vector3 value in positions)
            points.Add(new Point3D(value.X, value.Y, value.Z));
        var triangles = new Int32Collection(indices.Length);
        foreach (uint index in indices)
            triangles.Add(checked((int)index));
        var textureCoordinates = new PointCollection(mesh.TextureCoordinates.Length);
        foreach (Vector2 uv in mesh.TextureCoordinates)
            textureCoordinates.Add(new Point(uv.X, uv.Y));
        var geometry = new MeshGeometry3D
        {
            Positions = points,
            TriangleIndices = triangles,
            TextureCoordinates = textureCoordinates
        };
        if (mesh.Normals.Length == positions.Length)
        {
            var normals = new Vector3DCollection(mesh.Normals.Length);
            foreach (Vector3 normal in mesh.Normals)
            {
                Vector3 transformed = Vector3.TransformNormal(normal, transform);
                if (transformed.LengthSquared() > 1e-12f)
                    transformed = Vector3.Normalize(transformed);
                normals.Add(new Vector3D(
                    transformed.X,
                    transformed.Y,
                    transformed.Z));
            }
            geometry.Normals = normals;
        }
        AddReferencedPositionsToBounds(points, indices, all);
        var model = new GeometryModel3D(geometry, material)
        {
            BackMaterial = material
        };
        group.Children.Add(model);
        return model;
    }

    private static GeometryModel3D AddModel(
        Model3DGroup group,
        Vector3[] positions,
        uint[] indices,
        Color color,
        List<Point3D> all,
        bool boundsFromIndices = false)
    {
        var points = new Point3DCollection(positions.Length);
        foreach (Vector3 value in positions)
        {
            var point = new Point3D(value.X, value.Y, value.Z);
            points.Add(point);
            if (!boundsFromIndices)
                all.Add(point);
        }
        if (boundsFromIndices)
            AddReferencedPositionsToBounds(points, indices, all);
        var triangles = new Int32Collection(indices.Length);
        foreach (uint index in indices) triangles.Add(checked((int)index));
        var geometry = new MeshGeometry3D { Positions = points, TriangleIndices = triangles };
        var material = new DiffuseMaterial(new SolidColorBrush(color));
        var model = new GeometryModel3D(geometry, material) { BackMaterial = material };
        group.Children.Add(model);
        return model;
    }

    private static void AddReferencedPositionsToBounds(
        Point3DCollection positions,
        IReadOnlyList<uint> indices,
        ICollection<Point3D> bounds)
    {
        foreach (uint index in indices.Distinct())
        {
            if (index >= positions.Count)
                throw new InvalidDataException(
                    $"Preview triangle index {index} is outside {positions.Count} positions.");
            bounds.Add(positions[checked((int)index)]);
        }
    }

    private void PrepareRigSkeletonOverlay(bool showFittingPose)
    {
        if (!EnsureRigFittingState() || _targetRigDefinition is null)
        {
            ClearRigSkeletonOverlay();
            return;
        }

        IReadOnlyList<Matrix4x4> matrices;
        bool useEditedPose = showFittingPose &&
            (UsesGeneratedWeightsPortingMode ||
             (UsesAdaptDonorWeightsPortingMode && ManualAdaptWeights));
        try
        {
            matrices = useEditedPose
                ? CaptureDisplayedRigFittingPose(
                    localRotationsOnly: UsesGeneratedWeightsPortingMode).WorldMatrices
                : _targetRigDefinition.Joints
                    .Select(joint => joint.BindWorldMatrix)
                    .ToArray();
        }
        catch (Exception exception) when (exception is InvalidDataException or
                                          InvalidOperationException or
                                          ArgumentException)
        {
            RigFittingStatusText.Text =
                "Skeleton overlay недоступен: " + exception.Message;
            ClearRigSkeletonOverlay();
            return;
        }

        _rigOverlayJointPositions = matrices
            .Select(matrix => new Vector3(matrix.M41, matrix.M42, matrix.M43))
            .ToArray();
    }

    private void ClearRigSkeletonOverlay()
    {
        _rigOverlayJointPositions = null;
        _rigJointScreenPoints.Clear();
        if (RigOverlayCanvas is null)
            return;
        foreach (UIElement element in _rigOverlayElements)
            RigOverlayCanvas.Children.Remove(element);
        _rigOverlayElements.Clear();
    }

    private void UpdateRigSkeletonScreenOverlay()
    {
        if (RigOverlayCanvas is null)
            return;
        foreach (UIElement element in _rigOverlayElements)
            RigOverlayCanvas.Children.Remove(element);
        _rigOverlayElements.Clear();
        _rigJointScreenPoints.Clear();
        if (_targetRigDefinition is null ||
            _rigOverlayJointPositions is null ||
            _rigOverlayJointPositions.Length != _targetRigDefinition.Joints.Count ||
            PreviewViewport.ActualWidth <= 0 || PreviewViewport.ActualHeight <= 0)
        {
            return;
        }

        Point?[] points = new Point?[_rigOverlayJointPositions.Length];
        double[] depths = new double[_rigOverlayJointPositions.Length];
        for (int jointIndex = 0; jointIndex < points.Length; jointIndex++)
        {
            Vector3 value = _rigOverlayJointPositions[jointIndex];
            var world = new Point3D(value.X, value.Y, value.Z);
            if (TryProjectToPreview(world, out Point screen, out double depth))
            {
                points[jointIndex] = screen;
                depths[jointIndex] = depth;
            }
        }

        int selectedJointIndex =
            RigFittingJointCombo.SelectedItem is TargetRigJointItem selected
                ? selected.JointIndex
                : -1;

        // Draw a dark outline first, then the colored segment. The Canvas is a
        // separate screen-space layer, so opaque model geometry can never hide
        // the rig.
        foreach (TargetRigJoint joint in _targetRigDefinition.Joints)
        {
            if (joint.ParentJointIndex < 0 ||
                points[joint.ParentJointIndex] is not Point parent ||
                points[joint.JointIndex] is not Point child)
            {
                continue;
            }
            AddRigScreenLine(parent, child, Brushes.Black, 5.5);
            AddRigScreenLine(
                parent,
                child,
                joint.IsDeformJoint
                    ? new SolidColorBrush(Color.FromRgb(38, 198, 255))
                    : new SolidColorBrush(Color.FromRgb(250, 204, 21)),
                2.5);
        }

        // Render the selected red joint last so dense finger markers cannot
        // cover it with a later cyan marker.
        foreach (TargetRigJoint joint in _targetRigDefinition.Joints
                     .OrderBy(joint => joint.JointIndex == selectedJointIndex ? 1 : 0))
        {
            if (points[joint.JointIndex] is not Point point)
                continue;
            bool isSelected = joint.JointIndex == selectedJointIndex;
            double diameter = isSelected ? 14 : joint.IsDeformJoint ? 9 : 7;
            Brush fill = isSelected
                ? new SolidColorBrush(Color.FromRgb(255, 45, 45))
                : joint.IsDeformJoint
                    ? new SolidColorBrush(Color.FromRgb(38, 198, 255))
                    : new SolidColorBrush(Color.FromRgb(250, 204, 21));
            var marker = new WpfEllipse
            {
                Width = diameter,
                Height = diameter,
                Fill = fill,
                Stroke = Brushes.Black,
                StrokeThickness = 2
            };
            Canvas.SetLeft(marker, point.X - diameter * 0.5);
            Canvas.SetTop(marker, point.Y - diameter * 0.5);
            RigOverlayCanvas.Children.Add(marker);
            _rigOverlayElements.Add(marker);
            if (joint.IsDeformJoint)
            {
                _rigJointScreenPoints.Add(new RigJointScreenPoint(
                    joint.JointIndex,
                    point,
                    depths[joint.JointIndex]));
            }
        }
    }

    private void AddRigScreenLine(
        Point start,
        Point end,
        Brush stroke,
        double thickness)
    {
        var line = new WpfLine
        {
            X1 = start.X,
            Y1 = start.Y,
            X2 = end.X,
            Y2 = end.Y,
            Stroke = stroke,
            StrokeThickness = thickness,
            StrokeStartLineCap = PenLineCap.Round,
            StrokeEndLineCap = PenLineCap.Round
        };
        RigOverlayCanvas.Children.Add(line);
        _rigOverlayElements.Add(line);
    }

    private static void AddRigSegment(
        Model3DGroup group,
        List<Point3D> all,
        Vector3 start,
        Vector3 end,
        float radius,
        Color color)
    {
        Vector3 direction = end - start;
        if (!float.IsFinite(direction.LengthSquared()) ||
            direction.LengthSquared() <= 0.0000000001f)
            return;
        direction = Vector3.Normalize(direction);
        Vector3 helper = MathF.Abs(Vector3.Dot(direction, Vector3.UnitY)) < 0.9f
            ? Vector3.UnitY
            : Vector3.UnitX;
        Vector3 sideA = Vector3.Normalize(Vector3.Cross(direction, helper)) * radius;
        Vector3 sideB = Vector3.Normalize(Vector3.Cross(direction, sideA)) * radius;
        Vector3[] positions =
        [
            start - sideA - sideB,
            start + sideA - sideB,
            start + sideA + sideB,
            start - sideA + sideB,
            end - sideA - sideB,
            end + sideA - sideB,
            end + sideA + sideB,
            end - sideA + sideB
        ];
        AddRigOverlayModel(group, all, positions, RigBoxTriangleIndices, color);
    }

    private static void AddRigJointMarker(
        Model3DGroup group,
        List<Point3D> all,
        Vector3 center,
        float radius,
        Color color)
    {
        Vector3 extent = new(radius);
        Vector3[] positions =
        [
            center + new Vector3(-extent.X, -extent.Y, -extent.Z),
            center + new Vector3( extent.X, -extent.Y, -extent.Z),
            center + new Vector3( extent.X,  extent.Y, -extent.Z),
            center + new Vector3(-extent.X,  extent.Y, -extent.Z),
            center + new Vector3(-extent.X, -extent.Y,  extent.Z),
            center + new Vector3( extent.X, -extent.Y,  extent.Z),
            center + new Vector3( extent.X,  extent.Y,  extent.Z),
            center + new Vector3(-extent.X,  extent.Y,  extent.Z)
        ];
        AddRigOverlayModel(group, all, positions, RigBoxTriangleIndices, color);
    }

    private static void AddRigOverlayModel(
        Model3DGroup group,
        List<Point3D> all,
        IReadOnlyList<Vector3> positions,
        IReadOnlyList<uint> indices,
        Color color)
    {
        var points = new Point3DCollection(positions.Count);
        foreach (Vector3 value in positions)
        {
            var point = new Point3D(value.X, value.Y, value.Z);
            points.Add(point);
            all.Add(point);
        }
        var triangles = new Int32Collection(indices.Count);
        foreach (uint index in indices)
            triangles.Add(checked((int)index));
        var geometry = new MeshGeometry3D
        {
            Positions = points,
            TriangleIndices = triangles
        };
        var material = new EmissiveMaterial(new SolidColorBrush(color));
        group.Children.Add(new GeometryModel3D(geometry, material)
        {
            BackMaterial = material
        });
    }

    private static readonly uint[] RigBoxTriangleIndices =
    [
        0, 2, 1, 0, 3, 2,
        4, 5, 6, 4, 6, 7,
        0, 1, 5, 0, 5, 4,
        1, 2, 6, 1, 6, 5,
        2, 3, 7, 2, 7, 6,
        3, 0, 4, 3, 4, 7
    ];

    private void ResolveSelectedBonePosition()
    {
        _selectedBonePosition = null;
        if (_replacementSmoDocument is not null || UsesPreparedModelPortingMode ||
            UsesAdaptDonorWeightsPortingMode || UsesGeneratedWeightsPortingMode)
            return;
        if (_document is null || BoneCombo.SelectedItem is not BoneItem selected)
        {
            if (BoneHighlightText is not null)
                BoneHighlightText.Text = "Красный — выбранная palette bone";
            return;
        }

        SmoObjectEntry? node = _document.Objects.FirstOrDefault(entry =>
            entry.Id == selected.ObjectId);
        IReadOnlyDictionary<int, Matrix4x4> bindMatrices =
            SmoSkinBindingResolver.ResolveBindWorldMatrices(_document);
        if (node is null || !bindMatrices.TryGetValue(node.Index, out Matrix4x4 bindWorld))
        {
            BoneHighlightText.Text = $"Palette bone: {selected.Display} — позиция недоступна";
            return;
        }

        _selectedBonePosition = new Point3D(bindWorld.M41, bindWorld.M42, bindWorld.M43);
        BoneHighlightText.Text = $"Красный — palette bone {selected.Display}";
    }

    private void UpdateBoneMarkerOverlay()
    {
        if (BoneMarker is null)
            return;
        if (_selectedBonePosition is not Point3D bone ||
            PreviewViewport.ActualWidth <= 0 || PreviewViewport.ActualHeight <= 0)
        {
            BoneMarker.Visibility = Visibility.Collapsed;
            return;
        }

        if (!TryProjectToPreview(bone, out Point screen, out _))
        {
            BoneMarker.Visibility = Visibility.Collapsed;
            return;
        }

        Canvas.SetLeft(BoneMarker, screen.X - BoneMarker.Width * 0.5);
        Canvas.SetTop(BoneMarker, screen.Y - BoneMarker.Height * 0.5);
        BoneMarker.Visibility = Visibility.Visible;
    }

    private bool TryProjectToPreview(
        Point3D world,
        out Point screen,
        out double depth)
    {
        screen = default;
        depth = 0;
        if (PreviewViewport.ActualWidth <= 0 || PreviewViewport.ActualHeight <= 0)
            return false;
        GetCameraBasis(out Vector3D forward, out Vector3D right, out Vector3D up);
        Vector3D fromCamera = world - Camera.Position;
        depth = Vector3D.DotProduct(fromCamera, forward);
        if (depth <= Camera.NearPlaneDistance)
            return false;

        double halfHeight = depth * Math.Tan(Camera.FieldOfView * Math.PI / 360.0);
        if (!double.IsFinite(halfHeight) || halfHeight <= 0)
            return false;
        double aspect = PreviewViewport.ActualWidth / PreviewViewport.ActualHeight;
        double normalizedX = Vector3D.DotProduct(fromCamera, right) / (halfHeight * aspect);
        double normalizedY = Vector3D.DotProduct(fromCamera, up) / halfHeight;
        double screenX = (normalizedX + 1) * PreviewViewport.ActualWidth * 0.5;
        double screenY = (1 - normalizedY) * PreviewViewport.ActualHeight * 0.5;
        if (!double.IsFinite(screenX) || !double.IsFinite(screenY) ||
            screenX < 0 || screenX > PreviewViewport.ActualWidth ||
            screenY < 0 || screenY > PreviewViewport.ActualHeight)
            return false;
        screen = new Point(screenX, screenY);
        return true;
    }

    private void Preview_SizeChanged(object sender, SizeChangedEventArgs e)
    {
        UpdateBoneMarkerOverlay();
        UpdateRigSkeletonScreenOverlay();
        UpdateGeneratedAttachmentScreenOverlay();
    }

    private void Frame(IReadOnlyList<Point3D> points)
    {
        double minX=points.Min(p=>p.X), maxX=points.Max(p=>p.X), minY=points.Min(p=>p.Y), maxY=points.Max(p=>p.Y), minZ=points.Min(p=>p.Z), maxZ=points.Max(p=>p.Z);
        _cameraTarget = new Point3D((minX+maxX)/2,(minY+maxY)/2,(minZ+maxZ)/2);
        double size = Math.Max(1, Math.Max(maxX-minX, Math.Max(maxY-minY,maxZ-minZ)));
        _cameraDistance = Math.Clamp(size * 2.5, MinimumCameraDistance, MaximumCameraDistance);
        UpdateCamera();
    }

    private void Preview_MouseDown(object sender, MouseButtonEventArgs e)
    {
        if (e.ChangedButton == MouseButton.Left && IsJointPoseEditorMode &&
            TrySelectRigJointAt(e.GetPosition(PreviewSurface)))
        {
            PreviewSurface.Focus();
            e.Handled = true;
            return;
        }
        if (e.ChangedButton == MouseButton.Left && CanSelectGeneratedAttachments)
        {
            int? componentIndex = FindGeneratedAttachmentComponentAt(
                e.GetPosition(PreviewViewport));
            SelectGeneratedAttachmentFromPreview(
                componentIndex,
                Keyboard.Modifiers.HasFlag(ModifierKeys.Control));
            PreviewSurface.Focus();
            e.Handled = true;
            return;
        }
        if (e.ChangedButton != MouseButton.Middle)
            return;

        ModifierKeys modifiers = Keyboard.Modifiers;
        _cameraNavigationMode = modifiers.HasFlag(ModifierKeys.Control)
            ? CameraNavigationMode.Zoom
            : modifiers.HasFlag(ModifierKeys.Shift)
                ? CameraNavigationMode.Pan
                : CameraNavigationMode.Orbit;
        _lastMousePosition = e.GetPosition(PreviewSurface);
        PreviewSurface.CaptureMouse();
        PreviewSurface.Focus();
        PreviewSurface.Cursor = _cameraNavigationMode == CameraNavigationMode.Orbit
            ? Cursors.SizeAll
            : Cursors.Hand;
        e.Handled = true;
    }

    private void Preview_MouseUp(object sender, MouseButtonEventArgs e)
    {
        if (e.ChangedButton != MouseButton.Middle)
            return;
        EndCameraNavigation();
        e.Handled = true;
    }

    private void Preview_LostMouseCapture(object sender, MouseEventArgs e)
    {
        _cameraNavigationMode = CameraNavigationMode.None;
        PreviewSurface.Cursor = null;
    }

    private void Preview_MouseMove(object sender, MouseEventArgs e)
    {
        if (_cameraNavigationMode == CameraNavigationMode.None)
        {
            bool selectable = IsJointPoseEditorMode
                ? FindRigJointAt(e.GetPosition(PreviewSurface)) is not null
                : CanSelectGeneratedAttachments &&
                  FindGeneratedAttachmentComponentAt(
                      e.GetPosition(PreviewViewport)) is not null;
            PreviewSurface.Cursor = selectable
                    ? Cursors.Hand
                    : null;
            return;
        }
        if (e.MiddleButton != MouseButtonState.Pressed)
        {
            EndCameraNavigation();
            return;
        }

        Point currentPosition = e.GetPosition(PreviewSurface);
        System.Windows.Vector delta = currentPosition - _lastMousePosition;
        _lastMousePosition = currentPosition;
        switch (_cameraNavigationMode)
        {
            case CameraNavigationMode.Orbit:
                _cameraYaw -= delta.X * OrbitSensitivity;
                _cameraPitch = Math.Clamp(
                    _cameraPitch + delta.Y * OrbitSensitivity,
                    -PitchLimit,
                    PitchLimit);
                break;
            case CameraNavigationMode.Pan:
                PanCamera(delta.X, delta.Y);
                break;
            case CameraNavigationMode.Zoom:
                ChangeCameraDistance(Math.Exp(delta.Y * DragZoomSensitivity));
                break;
        }
        UpdateCamera();
        e.Handled = true;
    }

    private void Preview_MouseWheel(object sender, MouseWheelEventArgs e)
    {
        ChangeCameraDistance(Math.Exp(-e.Delta / 120.0 * 0.14));
        UpdateCamera();
        PreviewSurface.Focus();
        e.Handled = true;
    }

    private void Preview_KeyDown(object sender, KeyEventArgs e)
    {
        if (e.Key != Key.Home || _previewBounds.Count == 0)
            return;
        Frame(_previewBounds);
        e.Handled = true;
    }

    private void PanCamera(double horizontalPixels, double verticalPixels)
    {
        GetCameraBasis(out _, out Vector3D right, out Vector3D up);
        double scale = GetWorldUnitsPerPixel();
        _cameraTarget += -right * (horizontalPixels * scale) + up * (verticalPixels * scale);
    }

    private void ChangeCameraDistance(double factor) =>
        _cameraDistance = Math.Clamp(
            _cameraDistance * factor,
            MinimumCameraDistance,
            MaximumCameraDistance);

    private double GetWorldUnitsPerPixel()
    {
        double viewportHeight = Math.Max(PreviewViewport.ActualHeight, 1);
        double halfFieldOfView = Camera.FieldOfView * Math.PI / 360.0;
        return 2 * _cameraDistance * Math.Tan(halfFieldOfView) / viewportHeight;
    }

    private void GetCameraBasis(out Vector3D forward, out Vector3D right, out Vector3D up)
    {
        forward = Camera.LookDirection;
        if (forward.LengthSquared < 1e-12) forward = new Vector3D(0, 0, -1);
        forward.Normalize();
        Vector3D upHint = Camera.UpDirection;
        if (upHint.LengthSquared < 1e-12) upHint = new Vector3D(0, 1, 0);
        upHint.Normalize();
        right = Vector3D.CrossProduct(forward, upHint);
        if (right.LengthSquared < 1e-12) right = new Vector3D(1, 0, 0);
        right.Normalize();
        up = Vector3D.CrossProduct(right, forward);
        up.Normalize();
    }

    private void UpdateCamera()
    {
        double horizontal = Math.Cos(_cameraPitch) * _cameraDistance;
        Vector3D offset = new(
            Math.Sin(_cameraYaw) * horizontal,
            Math.Sin(_cameraPitch) * _cameraDistance,
            Math.Cos(_cameraYaw) * horizontal);
        Camera.Position = _cameraTarget + offset;
        Camera.LookDirection = _cameraTarget - Camera.Position;
        Camera.UpDirection = Math.Abs(horizontal) < 1e-9
            ? (_cameraPitch >= 0 ? new Vector3D(0, 0, -1) : new Vector3D(0, 0, 1))
            : new Vector3D(0, 1, 0);
        Camera.NearPlaneDistance = Math.Max(_cameraDistance / 10000.0, 0.001);
        Camera.FarPlaneDistance = Math.Max(_cameraDistance * 100.0, 100.0);
        UpdateBoneMarkerOverlay();
        UpdateRigSkeletonScreenOverlay();
        UpdateGeneratedAttachmentScreenOverlay();
    }

    private bool TrySelectRigJointAt(Point point)
    {
        RigJointScreenPoint? hit = FindRigJointAt(point);
        if (hit is null)
            return false;
        TargetRigJointItem? item = RigFittingJointCombo.Items
            .OfType<TargetRigJointItem>()
            .FirstOrDefault(value => value.JointIndex == hit.Value.JointIndex);
        if (item is null)
            return false;
        RigFittingJointCombo.SelectedItem = item;
        UpdateRigSkeletonScreenOverlay();
        return true;
    }

    private int? FindGeneratedAttachmentComponentAt(Point point)
    {
        if (!CanSelectGeneratedAttachments ||
            _generatedAttachmentMeshByModel.Count == 0)
            return null;

        int? componentIndex = null;
        VisualTreeHelper.HitTest(
            PreviewViewport,
            filterCallback: null,
            resultCallback: result =>
            {
                if (result is not RayMeshGeometry3DHitTestResult rayHit ||
                    rayHit.ModelHit is not GeometryModel3D model ||
                    !_generatedAttachmentMeshByModel.TryGetValue(
                        model,
                        out int meshIndex))
                {
                    return HitTestResultBehavior.Continue;
                }

                componentIndex = ResolveGeneratedAttachmentComponent(
                    meshIndex,
                    rayHit.VertexIndex1,
                    rayHit.VertexIndex2,
                    rayHit.VertexIndex3);
                // This is the nearest donor surface. If it belongs to the smooth
                // body rather than a detached component, do not select a hidden
                // component behind it.
                return HitTestResultBehavior.Stop;
            },
            new PointHitTestParameters(point));
        return componentIndex;
    }

    private int? ResolveGeneratedAttachmentComponent(
        int meshIndex,
        params int[] vertexIndices)
    {
        if (!_generatedAttachmentComponentByMeshVertex.TryGetValue(
                meshIndex,
                out Dictionary<int, int>? componentsByVertex))
            return null;

        int? componentIndex = null;
        foreach (int vertexIndex in vertexIndices)
        {
            if (!componentsByVertex.TryGetValue(vertexIndex, out int candidate))
                return null;
            if (componentIndex is not null && componentIndex.Value != candidate)
                return null;
            componentIndex = candidate;
        }
        return componentIndex;
    }

    private void SelectGeneratedAttachmentFromPreview(
        int? componentIndex,
        bool toggle)
    {
        GeneratedAttachmentListItem? item = componentIndex is int value
            ? GeneratedAttachmentList.Items
                .OfType<GeneratedAttachmentListItem>()
                .FirstOrDefault(candidate => candidate.ComponentIndex == value)
            : null;

        _settingGeneratedAttachmentSelection = true;
        try
        {
            if (!toggle)
                GeneratedAttachmentList.SelectedItems.Clear();
            if (item is not null)
            {
                if (toggle && GeneratedAttachmentList.SelectedItems.Contains(item))
                    GeneratedAttachmentList.SelectedItems.Remove(item);
                else
                    GeneratedAttachmentList.SelectedItems.Add(item);
                GeneratedAttachmentList.ScrollIntoView(item);
            }
        }
        finally
        {
            _settingGeneratedAttachmentSelection = false;
        }
        SynchronizeGeneratedAttachmentSelectionFromList();
    }

    private RigJointScreenPoint? FindRigJointAt(Point point)
    {
        const double hitRadius = 18;
        double bestDistanceSquared = hitRadius * hitRadius;
        RigJointScreenPoint? best = null;
        foreach (RigJointScreenPoint candidate in _rigJointScreenPoints)
        {
            double dx = candidate.Screen.X - point.X;
            double dy = candidate.Screen.Y - point.Y;
            double distanceSquared = dx * dx + dy * dy;
            if (distanceSquared < bestDistanceSquared ||
                (Math.Abs(distanceSquared - bestDistanceSquared) < 0.01 &&
                 (best is null || candidate.Depth < best.Value.Depth)))
            {
                bestDistanceSquared = distanceSquared;
                best = candidate;
            }
        }
        return best;
    }

    private void EndCameraNavigation()
    {
        _cameraNavigationMode = CameraNavigationMode.None;
        PreviewSurface.Cursor = null;
        if (PreviewSurface.IsMouseCaptured)
            PreviewSurface.ReleaseMouseCapture();
    }

    private static (Vector3 Min, Vector3 Max) Bounds(IEnumerable<Vector3> positions)
    {
        using IEnumerator<Vector3> iterator = positions.GetEnumerator();
        if (!iterator.MoveNext()) throw new InvalidOperationException("Модель не содержит вершин.");
        Vector3 min = iterator.Current, max = iterator.Current;
        while (iterator.MoveNext())
        {
            min = Vector3.Min(min, iterator.Current);
            max = Vector3.Max(max, iterator.Current);
        }
        return (min, max);
    }

    private ReplacementTransform ReadTransform(bool strict = true)
    {
        bool valid = TryReadTransform(out ReplacementTransform transform);
        if (!valid && strict)
        {
            throw new InvalidOperationException(
                "Проверьте transform: scale должен быть конечным, положительным и обратимым, остальные поля — содержать конечные числа.");
        }
        return valid ? transform : ReplacementTransform.Identity;
    }

    private bool TryReadTransform(out ReplacementTransform transform)
    {
        bool valid =
            TryReadFiniteFloat(ScaleBox, out float scale) &
            TryReadFiniteFloat(RotXBox, out float rotationX) &
            TryReadFiniteFloat(RotYBox, out float rotationY) &
            TryReadFiniteFloat(RotZBox, out float rotationZ) &
            TryReadFiniteFloat(MoveXBox, out float translationX) &
            TryReadFiniteFloat(MoveYBox, out float translationY) &
            TryReadFiniteFloat(MoveZBox, out float translationZ) &&
            scale > 0;
        transform = valid
            ? new ReplacementTransform(
                scale,
                new Vector3(rotationX, rotationY, rotationZ),
                new Vector3(translationX, translationY, translationZ))
            : ReplacementTransform.Identity;
        if (valid)
        {
            Matrix4x4 matrix = transform.Matrix;
            valid = IsFiniteMatrix(matrix) && Matrix4x4.Invert(matrix, out _);
        }
        return valid;
    }

    private bool TryReadGeneratedDonorAlignment(
        out ReplacementTransform alignment)
    {
        bool valid = TryReadTransform(out alignment) &&
            alignment.RotationDegrees == Vector3.Zero;
        return valid;
    }

    private ReplacementTransform ReadGeneratedDonorAlignment()
    {
        if (!TryReadGeneratedDonorAlignment(out ReplacementTransform alignment))
        {
            throw new InvalidOperationException(
                "Масштаб должен быть конечным и положительным, положение — содержать " +
                "конечные числа, а поворот в режиме 3 должен оставаться нулевым.");
        }
        ValidateGeneratedDonorAlignment(alignment);
        return alignment;
    }

    private static void ValidateGeneratedDonorAlignment(
        ReplacementTransform alignment)
    {
        Matrix4x4 matrix = alignment.Matrix;
        if (!float.IsFinite(alignment.Scale) || alignment.Scale <= 0 ||
            alignment.RotationDegrees != Vector3.Zero ||
            !IsFiniteMatrix(matrix) || !Matrix4x4.Invert(matrix, out _))
        {
            throw new ArgumentException(
                "Alignment режима 3 должен иметь положительный uniform scale, " +
                "конечное положение и нулевой поворот.",
                nameof(alignment));
        }
    }

    private static bool IsFiniteMatrix(Matrix4x4 matrix) =>
        float.IsFinite(matrix.M11) && float.IsFinite(matrix.M12) &&
        float.IsFinite(matrix.M13) && float.IsFinite(matrix.M14) &&
        float.IsFinite(matrix.M21) && float.IsFinite(matrix.M22) &&
        float.IsFinite(matrix.M23) && float.IsFinite(matrix.M24) &&
        float.IsFinite(matrix.M31) && float.IsFinite(matrix.M32) &&
        float.IsFinite(matrix.M33) && float.IsFinite(matrix.M34) &&
        float.IsFinite(matrix.M41) && float.IsFinite(matrix.M42) &&
        float.IsFinite(matrix.M43) && float.IsFinite(matrix.M44);

    private void ShowError(Exception exception)
    {
        StatusText.Text = "Ошибка: " + exception.Message;
        MessageBox.Show(this, exception.Message, "Ошибка", MessageBoxButton.OK, MessageBoxImage.Error);
    }

    private sealed record BoneItem(int Slot, uint ObjectId, string Display);
    private sealed record TargetRigJointItem(int JointIndex, string Display);
    private sealed record GeneratedAttachmentListItem(
        GeneratedSkinningAttachment Attachment)
    {
        public int ComponentIndex => Attachment.ComponentIndex;

        public override string ToString()
        {
            string meshes = string.Join(", ", Attachment.MeshNames);
            string assignment = Attachment.ManualAssignment switch
            {
                GeneratedSkinningComponentAttachmentTarget.UpperBack =>
                    "вручную → спина",
                GeneratedSkinningComponentAttachmentTarget.Head =>
                    "вручную → голова",
                _ => $"авто → {Attachment.TargetBoneName}"
            };
            return $"#{ComponentIndex} · {meshes} · " +
                   $"{Attachment.VertexCount:N0} вершин · {assignment}";
        }
    }
    private readonly record struct RigJointScreenPoint(
        int JointIndex,
        Point Screen,
        double Depth);
    private readonly record struct BodyPoseControlValues(
        float ArmRaiseDegrees,
        float ArmForwardDegrees,
        float ElbowBendDegrees,
        float LegSpreadDegrees,
        float KneeBendDegrees,
        float TorsoPitchDegrees,
        float NeckForwardDegrees);
    private sealed record TextureResourceItem(
        string Display,
        string Details,
        string? ExternalPath,
        bool CanRemove,
        bool RemovesFolder = false,
        IReadOnlyList<int>? SourceMeshKeys = null,
        string MaterialModeStatus = "");

    private enum PortingModeUiChoice
    {
        Auto = 0,
        PreparedModel = 1,
        AdaptSkeleton = 2,
        GenerateWeights = 3,
        LegacyRigid = 4
    }

    private enum CameraNavigationMode
    {
        None,
        Orbit,
        Pan,
        Zoom
    }
}
