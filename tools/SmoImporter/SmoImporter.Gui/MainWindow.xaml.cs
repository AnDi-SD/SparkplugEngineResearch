using System.Globalization;
using System.IO;
using System.Numerics;
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

    private string? _sourcePath;
    private SmoDocument? _document;
    private SmoExportScene? _sourceScene;
    private string? _replacementPath;
    private ImportedScene? _replacementScene;
    private MeshSplitPlan? _plan;
    private SmoDocument? _replacementSmoDocument;
    private SmoExportScene? _replacementSmoScene;
    private SmoToSmoReplacementPlan? _smoReplacementPlan;
    private GlbSkinTransferPlan? _glbSkinTransferPlan;
    private string? _blenderPath;
    private string? _texturePath;
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
            _sourcePath = Path.GetFullPath(path);
            _document = SmoDocument.Load(_sourcePath);
            _sourceScene = SmoSceneBuilder.Build(_document);
            _plan = null;
            SourcePathText.Text = _sourcePath;
            SourceSummaryText.Text = $"{_sourceScene.Meshes.Count} mesh slots; {_sourceScene.Meshes.Sum(mesh => mesh.Positions.Length):N0} vertices; {_sourceScene.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3):N0} triangles.";
            BoneCombo.ItemsSource = SmoWholeModelReplacer.GetRigidBoneChoices(_document)
                .Select(bone => new BoneItem(
                    bone.Slot, bone.ObjectId, $"[{bone.Slot}] {bone.Name}"))
                .ToArray();
            BoneCombo.SelectedIndex = BoneCombo.Items.Count > 0 ? 0 : -1;
            if (_replacementSmoDocument is not null)
                UpdateSmoReplacementPlan();
            else if (_replacementScene?.HasSkinning == true)
                UpdateGlbSkinTransferPlan();
            else
                StatusText.Text = "Шаблон SMO загружен.";
            _framePreviewOnRefresh = true;
            RefreshState();
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
            "Полный render/service graph target сохраняется; geometry, textures и " +
            "reference-only skin palettes заполняются данными донора.";
        ReplacementModeText.Text = "Режим SMO → SMO";
        UpdateSmoReplacementPlan();
    }

    private void LoadExternalReplacement(string path)
    {
        string fullPath = Path.GetFullPath(path);
        ImportedScene scene = ImportedModelReader.Read(fullPath, _blenderPath);
        bool convertedFbx = Path.GetExtension(fullPath).Equals(
            ".fbx", StringComparison.OrdinalIgnoreCase);

        _replacementPath = fullPath;
        _replacementScene = scene;
        _replacementSmoDocument = null;
        _replacementSmoScene = null;
        _smoReplacementPlan = null;
        _glbSkinTransferPlan = null;
        _plan = null;
        ReplacementPathText.Text = fullPath;
        int jointCount = scene.Meshes.Select(mesh => mesh.Skinning?.Skeleton)
            .FirstOrDefault(skeleton => skeleton is not null)?.JointNames.Count ?? 0;
        ReplacementSummaryText.Text =
            $"{scene.Meshes.Count} source meshes; " +
            $"{scene.Meshes.Sum(mesh => mesh.Positions.Length):N0} vertices; " +
            $"{scene.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3):N0} triangles" +
            (scene.HasSkinning ? $"; {jointCount} skin joints." : ".");
        EmbeddedTextureCombo.ItemsSource = new[]
            {
                new TextureItem(-1, "Не менять текстуру исходного SMO")
            }
            .Concat(scene.Textures.Select((texture, index) => new TextureItem(index,
                $"{texture.Name} — {texture.Width}×{texture.Height}, {texture.MimeType}")))
            .ToArray();
        EmbeddedTextureCombo.SelectedIndex = 0;
        _texturePath = null;
        TexturePathText.Text = scene.Textures.Count > 0
            ? $"Встроенных base-color текстур: {scene.Textures.Count}; выберите нужную или оставьте исходную."
            : "Встроенная base-color текстура не найдена.";
        if (scene.HasSkinning)
        {
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
            ReplacementModeText.Text = "Экспериментальный режим OBJ/GLB → SMO";
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
            $"{_glbSkinTransferPlan.MaterialGroupCount}." + details;
        if (_glbSkinTransferPlan.CanReplace)
        {
            ReplacementModePanel.Background = new SolidColorBrush(
                Color.FromRgb(255, 243, 205));
            StatusText.Text =
                "Skinned GLB совместим. Проверьте дерево костей и подтвердите построение palettes.";
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
                    "Скелет SMO-донора совпадает. Можно перенести меши и текстуры в target-контейнер.";
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
        catch (Exception exception) { _plan = null; RefreshState(); ShowError(exception); }
    }

    private void Save_Click(object sender, RoutedEventArgs e)
    {
        if (_document is null || _sourcePath is null) return;
        bool smoMode = _replacementSmoDocument is not null;
        bool skinnedGlbMode = !smoMode && _replacementScene?.HasSkinning == true;
        if (smoMode && _smoReplacementPlan?.CanReplace != true) return;
        if (!smoMode && (_replacementScene is null || _plan is null)) return;
        string donorStem = _replacementPath is null
            ? "replacement"
            : Path.GetFileNameWithoutExtension(_replacementPath);
        var dialog = new SaveFileDialog
        {
            Filter = "Sparkplug model (*.smo)|*.smo",
            FileName = smoMode
                ? Path.GetFileNameWithoutExtension(_sourcePath) + "_from_" + donorStem + ".smo"
                : skinnedGlbMode
                    ? Path.GetFileNameWithoutExtension(_sourcePath) + "_skinned_" + donorStem + ".smo"
                    : Path.GetFileNameWithoutExtension(_sourcePath) + "_whole_replaced.smo",
            InitialDirectory = Path.GetDirectoryName(_sourcePath)
        };
        if (dialog.ShowDialog(this) != true) return;
        try
        {
            if (smoMode)
            {
                SmoToSmoReplacementResult smoResult = SmoToSmoReplacer.Replace(
                    _document, _replacementSmoDocument!, dialog.FileName);
                StatusText.Text =
                    $"Готово: target graph сохранён — {smoResult.MeshCount} mesh slots, " +
                    $"{smoResult.TextureCount} textures, {smoResult.TriangleCount:N0} triangles; " +
                    $"SHA-256 {smoResult.Sha256[..12]}….";
                MessageBox.Show(this,
                    $"SMO-подмена сохранена и проверена strict parser:\n{smoResult.OutputPath}\n\n" +
                    "Полный object graph, IDs, skeleton, служебные объекты и неизвестные связи " +
                    "целевого SMO сохранены. В его существующие mesh/texture slots записаны " +
                    "геометрия, UV, цвета и текстуры донора; skin weights перепривязаны к " +
                    "целевым palettes по сопоставлению костей. " +
                    "Исходные файлы не изменены.\n\n" +
                    $"SHA-256 результата:\n{smoResult.Sha256}",
                    "SMO-подмена готова", MessageBoxButton.OK,
                    MessageBoxImage.Information);
                return;
            }

            if (skinnedGlbMode)
            {
                ImportedTexture? embedded = EmbeddedTextureCombo.SelectedItem is TextureItem skinTexture &&
                                            skinTexture.Index >= 0
                    ? _replacementScene!.Textures[skinTexture.Index]
                    : null;
                GlbSkinTransferResult skinResult = SmoSkinnedGlbReplacer.Replace(
                    _document,
                    _replacementScene!,
                    ReadTransform(),
                    dialog.FileName,
                    RebaseBindPoseCheckBox.IsChecked == true,
                    embedded);
                StatusText.Text =
                    $"Готово: {skinResult.TriangleCount:N0} triangles, " +
                    $"{skinResult.PaletteCount} palettes; SHA-256 {skinResult.Sha256[..12]}….";
                MessageBox.Show(this,
                    $"Skinned GLB перенесён в копию target SMO:\n{skinResult.OutputPath}\n\n" +
                    "Target object graph и IDs сохранены. JOINTS_0/WEIGHTS_0 распределены по " +
                    "существующим 16-bone palettes. У текстуры заменён только RGB; " +
                    "проверенный target Alpha сохранён. Проверьте результат во Viewer и игре — " +
                    "режим экспериментальный.\n\n" +
                    $"SHA-256:\n{skinResult.Sha256}",
                    "Экспериментальный skinned SMO готов",
                    MessageBoxButton.OK,
                    MessageBoxImage.Warning);
                return;
            }

            WholeModelReplacementResult result = SmoWholeModelReplacer.Replace(
                _document, _replacementScene!, ReadTransform(), dialog.FileName,
                BoneCombo.SelectedItem is BoneItem bone ? bone.Slot : 0,
                texturePath: _texturePath,
                embeddedTexture: EmbeddedTextureCombo.SelectedItem is TextureItem texture && texture.Index >= 0
                    ? _replacementScene!.Textures[texture.Index]
                    : null);
            StatusText.Text = $"Готово: {result.MeshCount} meshes, {result.VertexCount:N0} vertices, {result.TriangleCount:N0} triangles.";
            MessageBox.Show(this,
                $"Новая копия сохранена и проверена strict parser:\n{result.OutputPath}\n\nТекстура записана безопасно: RGB заменён, исходный Alpha и структура SMO сохранены.",
                "Экспериментальный SMO готов", MessageBoxButton.OK, MessageBoxImage.Information);
        }
        catch (Exception exception) { ShowError(exception); }
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
            : $"Встроена в GLB: {_replacementScene.Textures[texture.Index].Name}";
    }

    private void BoneCombo_Changed(object sender, SelectionChangedEventArgs e) =>
        RefreshPreview();

    private void AutoFit_Click(object sender, RoutedEventArgs e)
    {
        if (_sourceScene is null || _replacementScene is null) return;
        try
        {
            Vector3[] source = _sourceScene.Meshes.SelectMany(mesh => mesh.Positions).ToArray();
            Vector3[] replacement = _replacementScene.Meshes.SelectMany(mesh => mesh.Positions).ToArray();
            if (source.Length == 0 || replacement.Length == 0)
                throw new InvalidOperationException("Одна из моделей не содержит вершин.");

            (Vector3 sourceMin, Vector3 sourceMax) = Bounds(source);
            (Vector3 replacementMin, Vector3 replacementMax) = Bounds(replacement);
            Vector3 sourceSize = sourceMax - sourceMin;
            Vector3 replacementSize = replacementMax - replacementMin;
            float sourceHeight = sourceSize.Y > 0.000001f ? sourceSize.Y : sourceSize.Length();
            float replacementHeight = replacementSize.Y > 0.000001f ? replacementSize.Y : replacementSize.Length();
            if (replacementHeight <= 0.000001f)
                throw new InvalidOperationException("Невозможно определить размер модели замены.");

            float scale = sourceHeight / replacementHeight;
            Vector3 sourceCenter = (sourceMin + sourceMax) * 0.5f;
            Vector3 replacementCenter = (replacementMin + replacementMax) * 0.5f;
            Vector3 translation = sourceCenter - replacementCenter * scale;
            ScaleBox.Text = scale.ToString("G9", CultureInfo.InvariantCulture);
            RotXBox.Text = RotYBox.Text = RotZBox.Text = "0";
            MoveXBox.Text = translation.X.ToString("G9", CultureInfo.InvariantCulture);
            MoveYBox.Text = translation.Y.ToString("G9", CultureInfo.InvariantCulture);
            MoveZBox.Text = translation.Z.ToString("G9", CultureInfo.InvariantCulture);
            StatusText.Text = $"Автоподгонка: scale {scale:G5}; центры моделей совмещены.";
            _framePreviewOnRefresh = true;
            RefreshPreview();
        }
        catch (Exception exception) { ShowError(exception); }
    }

    private void Transform_Changed(object sender, TextChangedEventArgs e)
    {
        if (_replacementSmoDocument is null)
            RefreshPreview();
    }

    private void Reset_Click(object sender, RoutedEventArgs e)
    {
        _sourcePath = null; _document = null; _sourceScene = null;
        _replacementPath = null; _replacementScene = null; _plan = null;
        _replacementSmoDocument = null; _replacementSmoScene = null;
        _smoReplacementPlan = null; _glbSkinTransferPlan = null; _texturePath = null;
        SourcePathText.Text = "Не выбран"; SourceSummaryText.Text = "—";
        ReplacementPathText.Text = "Не выбрана"; ReplacementSummaryText.Text = "—";
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
        _framePreviewOnRefresh = true;
        RefreshState();
    }

    private void RefreshState()
    {
        bool smoMode = _replacementSmoDocument is not null;
        bool skinnedGlbMode = !smoMode && _replacementScene?.HasSkinning == true;
        ExternalModelOptionsPanel.IsEnabled = !smoMode;
        ExternalModelOptionsPanel.Opacity = smoMode ? 0.5 : 1;
        PlanButton.IsEnabled = !smoMode && _document is not null &&
            _replacementScene is not null;
        AutoFitButton.IsEnabled = !smoMode && _sourceScene is not null &&
            _replacementScene is not null;
        SaveButton.IsEnabled = smoMode
            ? _smoReplacementPlan?.CanReplace == true
            : _plan is not null && (!skinnedGlbMode ||
                _glbSkinTransferPlan?.CanReplace == true);
        SaveButton.Content = smoMode
            ? "Создать SMO-подмену"
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
