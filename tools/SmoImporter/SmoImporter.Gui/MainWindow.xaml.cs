using System.Globalization;
using System.IO;
using System.Numerics;
using System.ComponentModel;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Media3D;
using Microsoft.Win32;
using SmoExporter.Core;
using SmoImporter.Core;
using SmoViewer.Core;

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
    private bool _isClosing;

    private string? _sourcePath;
    private SmoDocument? _document;
    private SmoExportScene? _sourceScene;
    private string? _replacementPath;
    private ImportedScene? _replacementScene;
    private RigidGlbTextureBundle? _replacementRigidTextureBundle;
    private SmoRigidMultiMaterialPackAnalysis? _rigidMultiMaterialAnalysis;
    private MeshSplitPlan? _plan;
    private SmoDocument? _replacementSmoDocument;
    private SmoExportScene? _replacementSmoScene;
    private SmoToSmoReplacementPlan? _smoReplacementPlan;
    private GlbSkinTransferPlan? _glbSkinTransferPlan;
    private string? _blenderPath;
    private string? _texturePath;
    private string? _multiTextureDirectory;
    private readonly List<Point3D> _previewBounds = new();
    private Point3D? _selectedBonePosition;
    private Point _lastMousePosition;
    private Point3D _cameraTarget;
    private double _cameraYaw;
    private double _cameraPitch;
    private double _cameraDistance = 10;
    private CameraNavigationMode _cameraNavigationMode;
    private bool _framePreviewOnRefresh = true;

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
            SourcePathText.Text = _sourcePath;
            SourceSummaryText.Text = $"{_sourceScene.Meshes.Count} mesh slots; {_sourceScene.Meshes.Sum(mesh => mesh.Positions.Length):N0} vertices; {_sourceScene.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3):N0} triangles.";
            BoneCombo.ItemsSource = boneItems;
            BoneCombo.SelectedIndex = BoneCombo.Items.Count > 0 ? 0 : -1;
            if (preferredHead is not null)
                BoneCombo.SelectedItem = preferredHead;
            if (_replacementSmoDocument is not null)
                UpdateSmoReplacementPlan();
            else if (_replacementScene?.HasSkinning == true)
                UpdateGlbSkinTransferPlan();
            else if (_replacementRigidTextureBundle is not null)
            {
                ApplyAutoFit();
                PlanSummaryText.Text = "Проверка multi-texture структуры ещё не выполнена.";
                StatusText.Text = "Целевой SMO изменён. Повторите проверку multi-texture структуры.";
            }
            else
                StatusText.Text = "Шаблон SMO загружен.";
            _framePreviewOnRefresh = true;
            RefreshState();
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

        _replacementPath = fullPath;
        _replacementSmoDocument = donor;
        _replacementSmoScene = donorScene;
        _replacementScene = null;
        _replacementRigidTextureBundle = null;
        _multiTextureDirectory = null;
        _rigidMultiMaterialAnalysis = null;
        _plan = null;
        _texturePath = null;
        EmbeddedTextureCombo.ItemsSource = null;
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
        UpdateSmoReplacementPlan();
    }

    private void LoadExternalReplacement(
        string path,
        string? textureDirectory = null)
    {
        string fullPath = Path.GetFullPath(path);
        RigidGlbTextureBundle? rigidTextureBundle = null;
        ImportedScene scene;
        if (!string.IsNullOrWhiteSpace(textureDirectory))
        {
            rigidTextureBundle = RigidGlbTextureBundleReader.ReadModel(
                fullPath,
                textureDirectory,
                _blenderPath);
            scene = rigidTextureBundle.Scene;
        }
        else if (RigidGlbTextureBundleReader.TryReadModel(
                     fullPath,
                     out rigidTextureBundle,
                     blenderPath: _blenderPath))
        {
            scene = rigidTextureBundle!.Scene;
        }
        else
        {
            scene = ImportedModelReader.Read(fullPath, _blenderPath);
        }
        BoneItem? preferredHead = rigidTextureBundle is not null && _document is not null
            ? FindPreferredHeadBone(BoneCombo.Items.OfType<BoneItem>())
            : null;
        IEnumerable<Vector3> replacementFitPositions = rigidTextureBundle is null
            ? scene.Meshes.SelectMany(mesh => mesh.Positions)
            : rigidTextureBundle.MaterialGroups
                .SelectMany(group => group.Meshes)
                .SelectMany(mesh => mesh.Positions);
        ReplacementTransform? automaticFit = rigidTextureBundle is not null &&
            _sourceScene is not null
                ? ReplacementTransformFitter.FitByHeightAndCenter(
                    _sourceScene.Meshes.SelectMany(mesh => mesh.Positions),
                    replacementFitPositions)
                : null;
        bool convertedFbx = Path.GetExtension(fullPath).Equals(
            ".fbx", StringComparison.OrdinalIgnoreCase);

        _replacementPath = fullPath;
        _replacementScene = scene;
        _replacementRigidTextureBundle = rigidTextureBundle;
        _multiTextureDirectory = rigidTextureBundle?.TextureDirectory;
        _rigidMultiMaterialAnalysis = null;
        _replacementSmoDocument = null;
        _replacementSmoScene = null;
        _smoReplacementPlan = null;
        _glbSkinTransferPlan = null;
        _plan = null;
        PlanSummaryText.Text = "План ещё не построен.";
        ReplacementPathText.Text = fullPath;
        int jointCount = scene.Meshes.Select(mesh => mesh.Skinning?.Skeleton)
            .FirstOrDefault(skeleton => skeleton is not null)?.JointNames.Count ?? 0;
        ReplacementSummaryText.Text = rigidTextureBundle is null
            ? $"{scene.Meshes.Count} source meshes; " +
              $"{scene.Meshes.Sum(mesh => mesh.Positions.Length):N0} vertices; " +
              $"{scene.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3):N0} triangles" +
              (scene.HasSkinning ? $"; {jointCount} skin joints." : ".")
            : $"{scene.Meshes.Count} meshes; " +
              $"{scene.Meshes.Sum(mesh => mesh.Positions.Length):N0} vertices; " +
              $"{scene.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3):N0} triangles; " +
              $"{rigidTextureBundle.MaterialGroups.Count} materials; " +
              $"{rigidTextureBundle.MaterialGroups.Sum(group => group.Frames.Count)} PNG frames.";
        EmbeddedTextureCombo.ItemsSource = new[]
            {
                new TextureItem(-1, "Не менять текстуру исходного SMO")
            }
            .Concat(scene.Textures.Select((texture, index) => new TextureItem(index,
                $"{texture.Name} — {texture.Width}×{texture.Height}, {texture.MimeType}")))
            .ToArray();
        EmbeddedTextureCombo.SelectedIndex = 0;
        _texturePath = null;
        TextureFolderPathText.Text = rigidTextureBundle is null
            ? $"Автопоиск рядом с моделью: {Path.GetDirectoryName(fullPath)}"
            : rigidTextureBundle.TextureDirectory;
        TexturePathText.Text = scene.Textures.Count > 0
            ? $"Встроенных цветовых текстур: {scene.Textures.Count}; выберите нужную или оставьте исходную."
            : "Встроенная цветовая текстура не найдена.";
        if (rigidTextureBundle is null && !scene.HasSkinning && BoneCombo.Items.Count > 0)
            BoneCombo.SelectedIndex = 0;
        if (rigidTextureBundle is not null)
        {
            string ignoredMeshWarning = rigidTextureBundle.IgnoredMeshes.Count == 0
                ? string.Empty
                : "\nИгнорируются служебные meshes: " +
                  string.Join(", ", rigidTextureBundle.IgnoredMeshes) + ".";
            string ignoredTextureWarning = rigidTextureBundle.IgnoredTextureFiles.Count == 0
                ? string.Empty
                : "\nНе относятся к активным matN и пропущены PNG: " +
                  string.Join(", ", rigidTextureBundle.IgnoredTextureFiles) + ".";
            EmbeddedTextureCombo.ItemsSource = null;
            TexturePathText.Text =
                $"PNG найдены в папке: " +
                $"{rigidTextureBundle.MaterialGroups.Count} материалов, " +
                $"{rigidTextureBundle.MaterialGroups.Sum(group => group.Frames.Count)} кадров. " +
                "POT сохраняются без изменения; остальные увеличиваются до следующей степени двойки.";
            MultiTextureSummaryText.Text = string.Join("; ",
                rigidTextureBundle.MaterialGroups.Select(group =>
                    $"{group.Name}: {group.Frames.Count} " +
                    (group.Frames.Count == 1 ? "texture" : "frames"))) +
                ignoredMeshWarning + ignoredTextureWarning;
            string modelKind = Path.GetExtension(fullPath).TrimStart('.').ToUpperInvariant();
            ReplacementModeText.Text = $"Multi-texture rigid {modelKind} → SMO";
            CompatibilityText.Text =
                "Каждый matN сохраняется отдельной material/mesh-веткой; вся модель rigid-привязана к Head." +
                ignoredMeshWarning + ignoredTextureWarning;
            ReplacementModePanel.Background = new SolidColorBrush(
                Color.FromRgb(220, 252, 231));
            BoneMappingTree.Items.Clear();
            BoneMappingPanel.Visibility = Visibility.Collapsed;
            SplitModeText.Text =
                "Геометрия не объединяется: material-группы и все PNG остаются раздельными. " +
                "Дополнительные mat3/mat4 используются как последовательности кадров.";
            PlanButton.Content = "Проверить multi-texture структуру";
            StatusText.Text = "Multi-texture набор загружен. Проверьте структуру и подгонку.";
            if (preferredHead is not null)
                BoneCombo.SelectedItem = preferredHead;
            if (automaticFit is not null)
                ApplyTransform(automaticFit);
        }
        else if (scene.HasSkinning)
        {
            RebaseBindPoseCheckBox.IsChecked = true;
            ApplyTransform(ReplacementTransform.Identity);
            if (scene.Textures.Count > 0)
                EmbeddedTextureCombo.SelectedIndex = 1;
            ReplacementModeText.Text = convertedFbx
                ? "Экспериментальный режим Skinned FBX → GLB → SMO"
                : "Экспериментальный режим Skinned GLB → SMO";
            SplitModeText.Text =
                "Triangles автоматически распределяются по существующим 16-bone palettes target; " +
                "object graph и IDs исходного SMO сохраняются.";
            PlanButton.Content = "Проверить кости и построить palettes";
            UpdateGlbSkinTransferPlan();
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
        }
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
        _glbSkinTransferPlan = SmoSkinnedGlbReplacer.Analyze(
            _document, _replacementScene);
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
            StatusText.Text = "Skinned GLB заблокирован: исправьте bone mapping.";
        }
    }

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
        try
        {
            if (_replacementRigidTextureBundle is not null)
            {
                _plan = null;
                _rigidMultiMaterialAnalysis = SmoRigidMultiMaterialPacker.Analyze(
                    _document, _replacementRigidTextureBundle);
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
            if (_replacementScene.HasSkinning)
            {
                _glbSkinTransferPlan = SmoSkinnedGlbReplacer.Analyze(
                    _document, _replacementScene);
                PopulateGlbBoneMappingTree(_glbSkinTransferPlan);
                if (!_glbSkinTransferPlan.CanReplace)
                    throw new InvalidOperationException(
                        "Skinned GLB несовместим:\n" +
                        string.Join("\n", _glbSkinTransferPlan.Messages.Select(
                            message => "• " + message)));
                _plan = MeshSplitter.Split(_replacementScene);
                PlanSummaryText.Text =
                    $"{_replacementScene.Meshes.Sum(mesh => mesh.Positions.Length):N0} vertices, " +
                    $"{_replacementScene.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3):N0} triangles; " +
                    $"{_glbSkinTransferPlan.ActiveJointCount} active joints → " +
                    $"{_glbSkinTransferPlan.MaterialGroupCount} material groups. " +
                    "16-bone palettes построены без изменения target graph.";
                StatusText.Text = "Skinned-план проверен. Можно создать экспериментальный SMO.";
                RefreshState();
                return;
            }
            int slots = _document.Objects.Count(entry => entry.TypeHash == SmoClassIds.MeshData);
            ImportedMesh combined = ImportedMeshCombiner.Combine(_replacementScene);
            _plan = MeshSplitter.Split(_replacementScene);
            if (_plan.Chunks.Count != 1)
                throw new InvalidOperationException("Модель превышает 65 535 уникальных вершин и пока требует умного пространственного разбиения.");
            PlanSummaryText.Text = $"{combined.Positions.Length:N0} vertices и {combined.TriangleIndices.Length / 3:N0} triangles → " +
                $"1 цельный rigid body-slot; ещё {slots - 1} slots получат невидимый degenerate triangle.";
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
        bool multiTextureMode = !smoMode && _replacementRigidTextureBundle is not null;
        bool skinnedGlbMode = !smoMode && _replacementScene?.HasSkinning == true;
        if (smoMode && _smoReplacementPlan?.CanReplace != true) return;
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
                ImportedTexture? selectedTexture = !string.IsNullOrWhiteSpace(_texturePath)
                    ? ImportedTextureFileReader.Read(_texturePath)
                    : EmbeddedTextureCombo.SelectedItem is TextureItem skinTexture &&
                      skinTexture.Index >= 0
                        ? _replacementScene!.Textures[skinTexture.Index]
                        : null;
                GlbSkinTransferResult skinResult = SmoSkinnedGlbReplacer.Replace(
                    _document,
                    _replacementScene!,
                    ReplacementTransform.Identity,
                    fullOutputPath,
                    RebaseBindPoseCheckBox.IsChecked == true
                        ? SkinnedGeometryTransferMode.RetargetToGameBindPose
                        : SkinnedGeometryTransferMode.PreservePreparedGeometry,
                    selectedTexture);
                outputPath = skinResult.OutputPath;
            }
            else
            {
                WholeModelReplacementResult result = SmoWholeModelReplacer.Replace(
                    _document, _replacementScene!, ReadTransform(), fullOutputPath,
                    BoneCombo.SelectedItem is BoneItem bone ? bone.Slot : 0,
                    texturePath: _texturePath,
                    embeddedTexture: EmbeddedTextureCombo.SelectedItem is TextureItem texture && texture.Index >= 0
                        ? _replacementScene!.Textures[texture.Index]
                        : null);
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

    private void SelectTextureFolder_Click(object sender, RoutedEventArgs e)
    {
        if (_replacementPath is null || _replacementSmoDocument is not null)
            return;

        var dialog = new OpenFolderDialog
        {
            Title = "Выберите папку с matN PNG-текстурами",
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
            // ReadModel completes all parsing and binding before LoadExternalReplacement
            // commits the new bundle, so a bad folder leaves the current donor intact.
            LoadExternalReplacement(_replacementPath, dialog.FolderName);
            _framePreviewOnRefresh = true;
            RefreshState();
            StatusText.Text =
                "Папка текстур подключена. Проверьте multi-texture структуру заново.";
        }
        catch (Exception exception)
        {
            ShowError(exception);
        }
    }

    private void SelectTexture_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new OpenFileDialog
        {
            Filter = "Изображение (*.png;*.jpg;*.jpeg)|*.png;*.jpg;*.jpeg|PNG (*.png)|*.png|JPEG (*.jpg;*.jpeg)|*.jpg;*.jpeg",
            CheckFileExists = true
        };
        if (dialog.ShowDialog(this) != true) return;
        _texturePath = dialog.FileName;
        EmbeddedTextureCombo.SelectedIndex = -1;
        TexturePathText.Text = _texturePath;
        StatusText.Text = "Текстура будет записана в основной character atlas при сохранении.";
    }

    private void EmbeddedTexture_Changed(object sender, SelectionChangedEventArgs e)
    {
        if (EmbeddedTextureCombo.SelectedItem is not TextureItem texture || _replacementScene is null)
            return;
        _texturePath = null;
        TexturePathText.Text = texture.Index < 0
            ? "Текстура исходного SMO останется без изменений."
            : $"Встроена в модель: {_replacementScene.Textures[texture.Index].Name}";
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
            ApplyAutoFit();
        }
        catch (Exception exception) { ShowError(exception); }
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
        ScaleBox.Text = fit.Scale.ToString("G9", CultureInfo.InvariantCulture);
        RotXBox.Text = RotYBox.Text = RotZBox.Text = "0";
        MoveXBox.Text = fit.Translation.X.ToString("G9", CultureInfo.InvariantCulture);
        MoveYBox.Text = fit.Translation.Y.ToString("G9", CultureInfo.InvariantCulture);
        MoveZBox.Text = fit.Translation.Z.ToString("G9", CultureInfo.InvariantCulture);
        StatusText.Text = $"Автоподгонка: scale {fit.Scale:G5}; центры моделей совмещены.";
        _framePreviewOnRefresh = true;
        RefreshPreview();
    }

    private void Transform_Changed(object sender, TextChangedEventArgs e)
    {
        if (_replacementSmoDocument is null)
            RefreshPreview();
    }

    private void Reset_Click(object sender, RoutedEventArgs e)
    {
        if (_nativeValidationRunning)
            return;

        _sourcePath = null; _document = null; _sourceScene = null;
        _replacementPath = null; _replacementScene = null; _plan = null;
        _replacementRigidTextureBundle = null;
        _multiTextureDirectory = null;
        _rigidMultiMaterialAnalysis = null;
        _replacementSmoDocument = null; _replacementSmoScene = null;
        _smoReplacementPlan = null; _glbSkinTransferPlan = null; _texturePath = null;
        SourcePathText.Text = "Не выбран"; SourceSummaryText.Text = "—";
        ReplacementPathText.Text = "Не выбрана"; ReplacementSummaryText.Text = "—";
        TextureFolderPathText.Text = "Автопоиск рядом с моделью";
        TexturePathText.Text = "Остаётся текстура исходного SMO"; BoneCombo.ItemsSource = null;
        EmbeddedTextureCombo.ItemsSource = null;
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
        _framePreviewOnRefresh = true;
        RefreshState();
    }

    private void RefreshState()
    {
        bool smoMode = _replacementSmoDocument is not null;
        bool multiTextureMode = !smoMode && _replacementRigidTextureBundle is not null;
        bool skinnedGlbMode = !smoMode && _replacementScene?.HasSkinning == true;
        ExternalModelOptionsPanel.IsEnabled = !smoMode;
        ExternalModelOptionsPanel.Opacity = smoMode ? 0.5 : 1;
        PlanButton.IsEnabled = !smoMode && _document is not null &&
            _replacementScene is not null;
        AutoFitButton.IsEnabled = !smoMode && !skinnedGlbMode && _sourceScene is not null &&
            _replacementScene is not null;
        TransformEditorGrid.IsEnabled = !skinnedGlbMode;
        TransformEditorGrid.Opacity = skinnedGlbMode ? 0.55 : 1;
        SaveButton.IsEnabled = !_nativeValidationRunning && (smoMode
            ? _smoReplacementPlan?.CanReplace == true
            : multiTextureMode
                ? _rigidMultiMaterialAnalysis?.CanPack == true
                : _plan is not null && (!skinnedGlbMode ||
                    _glbSkinTransferPlan?.CanReplace == true));
        SaveButton.Content = smoMode
            ? "Создать SMO-подмену"
            : multiTextureMode
                ? "4. Создать multi-texture SMO"
            : skinnedGlbMode
                ? "4. Создать skinned SMO"
                : "4. Создать новый SMO";
        RigidBonePanel.Visibility = skinnedGlbMode
            ? Visibility.Collapsed : Visibility.Visible;
        SkinnedGlbOptionsPanel.Visibility = skinnedGlbMode
            ? Visibility.Visible : Visibility.Collapsed;
        BoneHighlightText.Visibility = smoMode || skinnedGlbMode
            ? Visibility.Collapsed
            : Visibility.Visible;
        BoneCombo.IsEnabled = !multiTextureMode;
        TextureFolderPanel.Visibility = !smoMode && !skinnedGlbMode &&
            _replacementScene is not null
            ? Visibility.Visible : Visibility.Collapsed;
        SingleTexturePanel.Visibility = multiTextureMode
            ? Visibility.Collapsed : Visibility.Visible;
        MultiTextureSummaryPanel.Visibility = multiTextureMode
            ? Visibility.Visible : Visibility.Collapsed;
        RefreshPreview();
    }

    private void RefreshPreview()
    {
        if (SceneVisual is null) return;
        var group = new Model3DGroup();
        var all = new List<Point3D>();
        if (_sourceScene is not null)
            foreach (SmoExportMesh mesh in _sourceScene.Meshes)
                AddModel(group, mesh.Positions, mesh.TriangleIndices, Color.FromArgb(75, 170, 180, 190), all);
        if (_replacementScene is not null)
        {
            Matrix4x4 transform = ReadTransform(false).Matrix;
            foreach (ImportedMesh mesh in _replacementScene.Meshes)
                AddModel(group, mesh.Positions.Select(value => Vector3.Transform(value, transform)).ToArray(),
                    mesh.TriangleIndices, Color.FromArgb(190, 255, 125, 35), all);
        }
        else if (_replacementSmoScene is not null)
        {
            foreach (SmoExportMesh mesh in _replacementSmoScene.Meshes)
                AddModel(group, mesh.Positions, mesh.TriangleIndices,
                    Color.FromArgb(190, 255, 125, 35), all);
        }
        ResolveSelectedBonePosition();
        SceneVisual.Content = group;
        _previewBounds.Clear();
        _previewBounds.AddRange(all);
        if (_framePreviewOnRefresh && all.Count > 0)
        {
            Frame(all);
            _framePreviewOnRefresh = false;
        }
        UpdateBoneMarkerOverlay();
    }

    private static void AddModel(Model3DGroup group, Vector3[] positions, uint[] indices, Color color, List<Point3D> all)
    {
        var points = new Point3DCollection(positions.Length);
        foreach (Vector3 value in positions) { var point = new Point3D(value.X, value.Y, value.Z); points.Add(point); all.Add(point); }
        var triangles = new Int32Collection(indices.Length);
        foreach (uint index in indices) triangles.Add(checked((int)index));
        var geometry = new MeshGeometry3D { Positions = points, TriangleIndices = triangles };
        var material = new DiffuseMaterial(new SolidColorBrush(color));
        group.Children.Add(new GeometryModel3D(geometry, material) { BackMaterial = material });
    }

    private void ResolveSelectedBonePosition()
    {
        _selectedBonePosition = null;
        if (_replacementSmoDocument is not null || _replacementScene?.HasSkinning == true)
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

        GetCameraBasis(out Vector3D forward, out Vector3D right, out Vector3D up);
        Vector3D fromCamera = bone - Camera.Position;
        double depth = Vector3D.DotProduct(fromCamera, forward);
        if (depth <= Camera.NearPlaneDistance)
        {
            BoneMarker.Visibility = Visibility.Collapsed;
            return;
        }

        double halfHeight = depth * Math.Tan(Camera.FieldOfView * Math.PI / 360.0);
        double aspect = PreviewViewport.ActualWidth / PreviewViewport.ActualHeight;
        double normalizedX = Vector3D.DotProduct(fromCamera, right) / (halfHeight * aspect);
        double normalizedY = Vector3D.DotProduct(fromCamera, up) / halfHeight;
        double screenX = (normalizedX + 1) * PreviewViewport.ActualWidth * 0.5;
        double screenY = (1 - normalizedY) * PreviewViewport.ActualHeight * 0.5;
        if (!double.IsFinite(screenX) || !double.IsFinite(screenY) ||
            screenX < 0 || screenX > PreviewViewport.ActualWidth ||
            screenY < 0 || screenY > PreviewViewport.ActualHeight)
        {
            BoneMarker.Visibility = Visibility.Collapsed;
            return;
        }

        Canvas.SetLeft(BoneMarker, screenX - BoneMarker.Width * 0.5);
        Canvas.SetTop(BoneMarker, screenY - BoneMarker.Height * 0.5);
        BoneMarker.Visibility = Visibility.Visible;
    }

    private void Preview_SizeChanged(object sender, SizeChangedEventArgs e) =>
        UpdateBoneMarkerOverlay();

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
            return;
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
        bool Try(TextBox box, out float value) => float.TryParse(box.Text, NumberStyles.Float, CultureInfo.InvariantCulture, out value) || float.TryParse(box.Text, out value);
        float s=0,rx=0,ry=0,rz=0,x=0,y=0,z=0;
        bool valid = Try(ScaleBox,out s)&Try(RotXBox,out rx)&Try(RotYBox,out ry)&Try(RotZBox,out rz)&Try(MoveXBox,out x)&Try(MoveYBox,out y)&Try(MoveZBox,out z)&&s>0;
        if (!valid && strict) throw new InvalidOperationException("Проверьте transform: scale должен быть больше нуля, остальные поля должны содержать числа.");
        return valid ? new ReplacementTransform(s,new Vector3(rx,ry,rz),new Vector3(x,y,z)) : ReplacementTransform.Identity;
    }

    private void ShowError(Exception exception)
    {
        StatusText.Text = "Ошибка: " + exception.Message;
        MessageBox.Show(this, exception.Message, "Ошибка", MessageBoxButton.OK, MessageBoxImage.Error);
    }

    private sealed record BoneItem(int Slot, uint ObjectId, string Display);
    private sealed record TextureItem(int Index, string Display);

    private enum CameraNavigationMode
    {
        None,
        Orbit,
        Pan,
        Zoom
    }
}
