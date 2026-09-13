using Microsoft.Win32;
using SmoViewer.Core;
using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using Matrix4x4 = System.Numerics.Matrix4x4;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Media.Media3D;
using Brush = System.Windows.Media.Brush;
using Brushes = System.Windows.Media.Brushes;
using CheckBox = System.Windows.Controls.CheckBox;
using Color = System.Windows.Media.Color;
using Cursors = System.Windows.Input.Cursors;
using KeyEventArgs = System.Windows.Input.KeyEventArgs;
using MessageBox = System.Windows.MessageBox;
using MouseEventArgs = System.Windows.Input.MouseEventArgs;
using OpenFileDialog = Microsoft.Win32.OpenFileDialog;
using Point = System.Windows.Point;

namespace SmoViewer;

public partial class MainWindow : Window
{
    private const double OrbitSensitivity = 0.008;
    private const double DragZoomSensitivity = 0.012;
    private const double MinimumCameraDistance = 0.01;
    private const double MaximumCameraDistance = 1_000_000;
    private const double PitchLimit = Math.PI / 2 - 0.001;
    private const double FlyLookSensitivity = 0.004;

    private static readonly Color[] ModelColors =
    {
        Color.FromRgb(127, 180, 255),
        Color.FromRgb(255, 170, 120),
        Color.FromRgb(142, 218, 164),
        Color.FromRgb(210, 154, 255),
        Color.FromRgb(255, 218, 126),
        Color.FromRgb(116, 218, 218)
    };

    private static readonly Color DefaultSceneBackgroundColor =
        Color.FromRgb(17, 20, 26);
    private static readonly Color DefaultFloorGridColor =
        Color.FromRgb(89, 98, 115);

    private readonly BoundsBuilder _sceneBounds = new();
    private readonly List<LoadIssue> _allIssues = new();
    private readonly HashSet<Key> _pressedKeys = new();
    private readonly Dictionary<SceneObjectKey, SceneGeometry> _sceneGeometry = new();
    private readonly Dictionary<GeometryModel3D, SceneObjectKey> _geometryObjectKeys = new();
    private readonly Dictionary<SceneObjectKey, TreeViewItem> _visibleTreeItems = new();
    private readonly Dictionary<int, DecodedSmoFile> _treeFiles = new();
    private readonly List<SceneGeometry> _highlightedGeometry = new();
    private readonly List<AnimatedSceneMaterial> _animatedMaterials = new();
    private readonly List<BoneListItem> _allBoneItems = new();
    private readonly List<Model3D> _skeletonModels = new();
    private readonly List<Model3D> _attachmentModels = new();
    private readonly List<Model3D> _collisionModels = new();
    private readonly List<Model3D> _controlRigModels = new();
    private readonly List<Model3D> _markerModels = new();
    private readonly List<AuxiliaryObjectItem> _auxiliaryItems = new();
    private readonly List<AnimationListItem> _allAnimationItems = new();
    private readonly List<string> _loadedModelPaths = new();
    private readonly Dictionary<string, HashSet<string>> _animationGroupsByPath =
        new(StringComparer.OrdinalIgnoreCase);
    private readonly HashSet<string> _enabledAnimationGroups =
        new(StringComparer.OrdinalIgnoreCase);
    private readonly Dictionary<int, System.Numerics.Vector3> _animatedBonePositions = new();
    private BoneListItem? _selectedBone;
    private SceneObjectKey? _selectedAuxiliary;
    private SmoAnimationClip? _selectedAnimation;
    private bool _animationPlaying;
    private bool _updatingAnimationSlider;
    private bool _updatingAnimationGroups;
    private double _animationTime;
    private int _animationFileIndex;

    private Point _lastMousePosition;
    private Point3D _navigationStartTarget;
    private Point3D _cameraTarget = new(0, 0, 0);
    private CameraNavigationMode _navigationMode;
    private double _cameraYaw = 0.65;
    private double _cameraPitch = 0.35;
    private double _cameraDistance = 5;
    private double _navigationStartYaw;
    private double _navigationStartPitch;
    private double _navigationStartDistance;
    private CameraControlMode _cameraControlMode;
    private Point3D _flyPosition;
    private double _flySpeed = 5;
    private TimeSpan? _lastRenderTime;
    private int _loadedFileCount;
    private int _totalMeshCount;
    private int _decodedMeshCount;
    private int _diagnosticCount;
    private int _diagnosticErrorCount;
    private int _unsupportedMeshCount;
    private int _texturedMeshCount;
    private int _textureIssueCount;
    private int _failedFileCount;
    private Color _sceneBackgroundColor = DefaultSceneBackgroundColor;
    private Color _floorGridColor = DefaultFloorGridColor;

    public MainWindow()
    {
        InitializeComponent();
        ApplySceneColors();
        UpdateFloorGrid();
        UpdateCameraHelpToolTip();
        AddLog("SmoViewer запущен.");
        UpdateCamera();
        UpdateSceneStats();
        CompositionTarget.Rendering += CompositionTarget_Rendering;
    }

    private async void OpenSmo_Click(object sender, RoutedEventArgs e)
    {
        OpenFileDialog dialog = new()
        {
            Title = "Открыть SMO-объекты",
            Filter = "SMO models (*.smo)|*.smo|All files (*.*)|*.*",
            Multiselect = true,
            CheckFileExists = true
        };

        if (dialog.ShowDialog(this) != true)
            return;

        OpenSmoButton.IsEnabled = false;
        ResetLoadedScene();

        int addedFiles = 0;
        int totalMeshes = 0;
        int decodedMeshes = 0;
        int diagnostics = 0;
        int diagnosticErrors = 0;
        int unsupportedMeshes = 0;
        int texturedMeshes = 0;
        int textureIssues = 0;
        int failedFiles = 0;
        List<LoadIssue> batchIssues = new();

        try
        {
            foreach (string fileName in dialog.FileNames)
            {
                StatusText.Text = $"Чтение {Path.GetFileName(fileName)}…";

                try
                {
                    DecodedSmoFile decodedFile = await Task.Run(
                        () => DecodeFile(fileName));

                    AddFileTree(decodedFile, addedFiles);

                    SceneAddResult sceneAdd =
                        AddModelToScene(decodedFile.RenderMeshes, addedFiles);
                    AddSkeletonToScene(decodedFile, addedFiles);
                    _loadedModelPaths.Add(fileName);

                    addedFiles++;
                    totalMeshes += decodedFile.TotalMeshCount;
                    decodedMeshes += sceneAdd.MeshCount;
                    diagnostics += decodedFile.Diagnostics.Count;
                    diagnosticErrors += decodedFile.Diagnostics.Count(
                        diagnostic => diagnostic.Severity == SmoDiagnosticSeverity.Error);
                    unsupportedMeshes += decodedFile.DecodeErrors.Count;
                    texturedMeshes += sceneAdd.TexturedMeshCount;
                    textureIssues += decodedFile.TextureIssues.Count;

                    AddDocumentIssues(decodedFile, batchIssues);
                    AddLog(
                        $"Загружен {Path.GetFileName(fileName)}: " +
                        $"объектов {decodedFile.Objects.Count}, meshes {sceneAdd.MeshCount}, " +
                        $"с текстурой {sceneAdd.TexturedMeshCount}.");
                }
                catch (Exception exception)
                {
                    failedFiles++;
                    batchIssues.Add(new LoadIssue(
                        true,
                        $"{Path.GetFileName(fileName)}: {exception.GetType().Name}: " +
                        exception.Message));
                    AddLog($"ОШИБКА {Path.GetFileName(fileName)}: {exception.Message}");
                }
            }

            _loadedFileCount = addedFiles;
            _totalMeshCount = totalMeshes;
            _decodedMeshCount = decodedMeshes;
            _diagnosticCount = diagnostics;
            _diagnosticErrorCount = diagnosticErrors;
            _unsupportedMeshCount = unsupportedMeshes;
            _texturedMeshCount = texturedMeshes;
            _textureIssueCount = textureIssues;
            _failedFileCount = failedFiles;
            _allIssues.AddRange(batchIssues);
            foreach (LoadIssue issue in batchIssues)
                AddLog($"{(issue.IsError ? "ОШИБКА" : "ПРЕДУПРЕЖДЕНИЕ")}: {issue.Text}");

            foreach (string directory in dialog.FileNames.Select(Path.GetDirectoryName)
                         .Where(path => !string.IsNullOrWhiteSpace(path)).Distinct(StringComparer.OrdinalIgnoreCase)!)
                AddAnimationsFromDirectory(directory!);
            if (_allAnimationItems.Count == 0 &&
                TryFindDefaultBloomAnimationDirectory(dialog.FileNames, out string bloomDirectory))
            {
                AddAnimationsFromDirectory(bloomDirectory);
                AnimationSourceText.Text = $"Автоподстановка Bloom: {bloomDirectory}";
                AddLog($"Рядом с моделью нет SAN/ANM; подключена папка Bloom: {bloomDirectory}");
            }
            RefreshAnimationList();

            UpdateSceneStats();
            UpdateIssueToolTip();
            UpdateFloorGrid();

            if (decodedMeshes > 0)
                FrameScene();

            RefreshBoneList();
            UpdateSkeletonVisibility();
            UpdateMeshAppearance();

            StatusText.Text = BuildLoadStatus(
                addedFiles,
                decodedMeshes,
                totalMeshes,
                diagnostics,
                unsupportedMeshes,
                texturedMeshes,
                textureIssues,
                failedFiles,
                batchIssues);
            GameValidationPanel.SetModelPath(_loadedModelPaths.LastOrDefault());
            LaunchExporterButton.IsEnabled = _loadedModelPaths.Count > 0;
            LaunchImporterButton.IsEnabled = _loadedModelPaths.Count > 0;
        }
        finally
        {
            OpenSmoButton.IsEnabled = true;
        }
    }

    private void About_Click(object sender, RoutedEventArgs e)
    {
        var aboutWindow = new AboutWindow(
            FindExporterExecutable(),
            FindImporterExecutable())
        {
            Owner = this
        };
        aboutWindow.ShowDialog();
    }

    private void LaunchExporter_Click(object sender, RoutedEventArgs e)
    {
        if (_loadedModelPaths.Count == 0)
            return;

        string? executable = FindExporterExecutable();
        if (executable is null)
        {
            MessageBox.Show(this,
                "SmoExporter.Gui не найден. Сначала соберите проект tools/SmoExporter/SmoExporter.Gui.",
                "SMO Exporter", MessageBoxButton.OK, MessageBoxImage.Warning);
            return;
        }

        string animationManifest = Path.Combine(Path.GetTempPath(),
            $"smoviewer-animations-{Guid.NewGuid():N}.txt");
        string[] animationPaths = _allAnimationItems.Select(item => item.Path)
            .Distinct(StringComparer.OrdinalIgnoreCase).ToArray();
        File.WriteAllLines(animationManifest, animationPaths);

        var start = CreateToolStartInfo(executable);
        start.ArgumentList.Add(_loadedModelPaths[0]);
        start.ArgumentList.Add("--viewer-animation-list");
        start.ArgumentList.Add(animationManifest);
        try
        {
            Process.Start(start);
            AddLog($"Открыт экспортер для {Path.GetFileName(_loadedModelPaths[0])}; " +
                   $"передано SAN: {animationPaths.Length}.");
        }
        catch
        {
            try { File.Delete(animationManifest); }
            catch { }
            throw;
        }
    }

    private static string? FindExporterExecutable()
    {
        string installRoot = GetInstallRoot();
        string local = Path.Combine(AppContext.BaseDirectory, "SmoExporter.Gui.exe");
        if (File.Exists(local))
            return local;
        foreach (string bundled in new[]
        {
            Path.Combine(installRoot, "tools", "SmoExporter", "SmoExporter.Gui.exe"),
            Path.Combine(AppContext.BaseDirectory, "SmoExporter", "SmoExporter.Gui.exe")
        })
            if (File.Exists(bundled))
                return bundled;

        for (DirectoryInfo? directory = new(AppContext.BaseDirectory);
             directory is not null; directory = directory.Parent)
        {
            foreach (string configuration in new[] { "Release", "Debug" })
            {
                string candidate = Path.Combine(directory.FullName, "tools", "SmoExporter",
                    "SmoExporter.Gui", "bin", configuration, "net8.0-windows",
                    "SmoExporter.Gui.exe");
                if (File.Exists(candidate))
                    return candidate;
            }
        }
        return null;
    }

    private void LaunchImporter_Click(object sender, RoutedEventArgs e)
    {
        if (_loadedModelPaths.Count == 0)
            return;

        string? executable = FindImporterExecutable();
        if (executable is null)
        {
            MessageBox.Show(this,
                "SmoImporter.Gui не найден. Сначала соберите проект tools/SmoImporter/SmoImporter.Gui.",
                "SMO Importer", MessageBoxButton.OK, MessageBoxImage.Warning);
            return;
        }

        var start = CreateToolStartInfo(executable);
        start.ArgumentList.Add(_loadedModelPaths[0]);
        Process.Start(start);
        AddLog($"Открыт импортер для {Path.GetFileName(_loadedModelPaths[0])}.");
    }

    private static string? FindImporterExecutable()
    {
        string installRoot = GetInstallRoot();
        string local = Path.Combine(AppContext.BaseDirectory, "SmoImporter.Gui.exe");
        if (File.Exists(local))
            return local;
        foreach (string bundled in new[]
        {
            Path.Combine(installRoot, "tools", "SmoImporter", "SmoImporter.Gui.exe"),
            Path.Combine(AppContext.BaseDirectory, "SmoImporter", "SmoImporter.Gui.exe")
        })
            if (File.Exists(bundled))
                return bundled;

        for (DirectoryInfo? directory = new(AppContext.BaseDirectory);
             directory is not null; directory = directory.Parent)
        {
            foreach (string configuration in new[] { "Release", "Debug" })
            {
                string candidate = Path.Combine(directory.FullName, "tools", "SmoImporter",
                    "SmoImporter.Gui", "bin", configuration, "net8.0-windows",
                    "SmoImporter.Gui.exe");
                if (File.Exists(candidate))
                    return candidate;
            }
        }
        return null;
    }

    private static string GetInstallRoot()
    {
        var applicationDirectory = new DirectoryInfo(AppContext.BaseDirectory);
        return applicationDirectory.Name.Equals("app", StringComparison.OrdinalIgnoreCase) &&
               applicationDirectory.Parent is not null
            ? applicationDirectory.Parent.FullName
            : applicationDirectory.FullName;
    }

    private static ProcessStartInfo CreateToolStartInfo(string executable) => new(executable)
    {
        UseShellExecute = true,
        WorkingDirectory = Path.GetDirectoryName(executable) ?? AppContext.BaseDirectory
    };

    private void ResetLoadedScene()
    {
        if (GameValidationPanel is not null)
            GameValidationPanel.SetModelPath(null);
        LoadedModelsRoot.Children.Clear();
        SkeletonRoot.Children.Clear();
        AttachmentRoot.Children.Clear();
        CollisionRoot.Children.Clear();
        ControlRigRoot.Children.Clear();
        MarkerRoot.Children.Clear();
        SceneTree.Items.Clear();
        StateLog.Items.Clear();
        _sceneBounds.Reset();
        _allIssues.Clear();
        _sceneGeometry.Clear();
        _geometryObjectKeys.Clear();
        _visibleTreeItems.Clear();
        _treeFiles.Clear();
        _highlightedGeometry.Clear();
        _allBoneItems.Clear();
        _skeletonModels.Clear();
        _attachmentModels.Clear();
        _collisionModels.Clear();
        _controlRigModels.Clear();
        _markerModels.Clear();
        _auxiliaryItems.Clear();
        _allAnimationItems.Clear();
        _loadedModelPaths.Clear();
        if (LaunchExporterButton is not null)
            LaunchExporterButton.IsEnabled = false;
        if (LaunchImporterButton is not null)
            LaunchImporterButton.IsEnabled = false;
        _animationGroupsByPath.Clear();
        _enabledAnimationGroups.Clear();
        _animatedBonePositions.Clear();
        _selectedAnimation = null;
        _animationPlaying = false;
        _animationTime = 0;
        _selectedBone = null;
        _selectedAuxiliary = null;
        if (BoneList is not null)
            BoneList.ItemsSource = null;
        if (AuxiliaryObjectList is not null)
            AuxiliaryObjectList.ItemsSource = null;
        if (AnimationList is not null)
            AnimationList.ItemsSource = null;
        if (AnimationGroupsPanel is not null)
            AnimationGroupsPanel.Children.Clear();
        if (AnimationFilterBox is not null)
            AnimationFilterBox.Clear();
        if (AnimationSourceText is not null)
            AnimationSourceText.Text = "Анимации будут найдены рядом с открытой моделью.";
        if (AnimationTimeline is not null)
            AnimationTimeline.Visibility = Visibility.Collapsed;
        AddLog("Сцена очищена.");

        _loadedFileCount = 0;
        _totalMeshCount = 0;
        _decodedMeshCount = 0;
        _diagnosticCount = 0;
        _diagnosticErrorCount = 0;
        _unsupportedMeshCount = 0;
        _texturedMeshCount = 0;
        _textureIssueCount = 0;
        _failedFileCount = 0;

        EndCameraNavigation();

        _cameraTarget = new Point3D(0, 0, 0);
        _cameraYaw = 0.65;
        _cameraPitch = 0.35;
        _cameraDistance = 5;
        SceneCamera.NearPlaneDistance = 0.01;
        SceneCamera.FarPlaneDistance = 100000;

        StatusText.ToolTip = null;
        UpdateFloorGrid();
        UpdateCamera();
        UpdateSceneStats();
    }

    private static DecodedSmoFile DecodeFile(string fileName)
    {
        SmoDocument document = SmoDocument.Load(fileName);
        SmoObjectEntry[] meshEntries = document.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.MeshData)
            .ToArray();
        IReadOnlyDictionary<int, SmoTextureBinding> textureBindings =
            SmoTextureBindingResolver.ResolveAll(document);
        IReadOnlyDictionary<int, uint> materialColors =
            SmoMaterialColorResolver.ResolveAll(document);
        SmoNodeHierarchy nodeHierarchy = SmoNodeHierarchy.Decode(document);
        IReadOnlyDictionary<int, Matrix4x4> bindWorldMatrices =
            SmoSkinBindingResolver.ResolveBindWorldMatrices(document);
        Dictionary<int, SmoSkin> skins = document.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.Skin)
            .Select(entry => SmoSkinDecoder.TryDecode(
                document, entry, out SmoSkin? skin, out _) ? skin : null)
            .Where(skin => skin is not null)
            .Cast<SmoSkin>()
            .ToDictionary(skin => skin.ObjectIndex);
        Dictionary<int, List<BonePaletteReference>> paletteReferences = new();
        foreach (SmoSkin skin in skins.Values)
        foreach (SmoSkinBone bone in skin.Bones)
        {
            if (!paletteReferences.TryGetValue(
                    bone.NodeObjectIndex, out List<BonePaletteReference>? references))
            {
                references = [];
                paletteReferences.Add(bone.NodeObjectIndex, references);
            }
            references.Add(new BonePaletteReference(
                skin.ObjectIndex, skin.Name, bone.PaletteIndex));
        }

        List<DecodedRenderMesh> renderMeshes = new(meshEntries.Length);
        List<string> decodeErrors = new();
        List<string> textureIssues = new();

        foreach (SmoObjectEntry entry in meshEntries)
        {
            if (entry.Name.StartsWith("trail_mesh", StringComparison.OrdinalIgnoreCase))
            {
                textureIssues.Add(
                    $"AUXILIARY_TRAIL_MESH_SKIPPED: Mesh [{entry.Index}] " +
                    $"\"{entry.Name}\" is an effect trail, not regular model geometry.");
                continue;
            }

            if (SmoMeshDecoder.TryDecode(
                    document,
                    entry,
                    out SmoMesh? mesh,
                    out string error))
            {
                SmoTexture? texture = null;
                IReadOnlyList<SmoTexture>? animationFrames = null;
                TimeSpan? animationFrameDuration = null;
                SmoTexture? baseTexture = null;
                bool usesAlphaBlend = false;
                if (textureBindings.TryGetValue(entry.Index, out SmoTextureBinding? binding))
                {
                    usesAlphaBlend = binding.UsesAlphaBlend;
                    if (binding.Issue is not null)
                    {
                        textureIssues.Add(binding.Issue);
                    }
                    else if (binding.Texture is not null && mesh.HasTextureCoordinates)
                    {
                        texture = binding.Texture;
                        animationFrames = binding.AnimationFrames;
                        animationFrameDuration = binding.FrameDuration;
                        baseTexture = binding.BaseTexture;
                    }
                    else if (binding.Texture is not null)
                    {
                        textureIssues.Add(
                            $"UNSUPPORTED_VERTEX_UV_LAYOUT: Mesh [{mesh.ObjectIndex}] " +
                            $"\"{mesh.Name}\" uses format 0x{mesh.VertexFormat:X} " +
                            $"with serialized stride {mesh.Stride}.");
                    }
                }

                Matrix4x4 worldTransform =
                    SmoNodeTransformDecoder.ResolveModelWorldMatrix(document, entry);
                int? skinObjectIndex = FindAncestorObjectIndex(
                    document.Objects, entry, SmoClassIds.Skin);
                int? rigidNodeObjectIndex = mesh.HasSkinningData
                    ? null
                    : SmoRigidBindingResolver.ResolveAnimationNodeObjectIndex(
                        document, entry);
                IReadOnlyDictionary<int, float> boneInfluences =
                    skinObjectIndex is int skinIndex && skins.TryGetValue(skinIndex, out SmoSkin? skin)
                        ? ResolveBoneInfluences(mesh, skin)
                        : new Dictionary<int, float>();
                materialColors.TryGetValue(entry.Index, out uint materialColor);
                renderMeshes.Add(new DecodedRenderMesh(
                    mesh,
                    texture,
                    animationFrames,
                    animationFrameDuration,
                    baseTexture,
                    materialColor == 0 ? null : materialColor,
                    usesAlphaBlend,
                    worldTransform,
                    skinObjectIndex,
                    rigidNodeObjectIndex,
                    boneInfluences));
            }
            else
                decodeErrors.Add(error);
        }

        HashSet<int> boneIndices = bindWorldMatrices.Keys.ToHashSet();
        List<SkeletonBone> skeleton = new();
        foreach ((int nodeIndex, Matrix4x4 matrix) in bindWorldMatrices)
        {
            SmoObjectEntry node = document.Objects[nodeIndex];
            int? parentBone = FindNearestBoneParent(
                nodeIndex, boneIndices, document.Objects, nodeHierarchy);
            skeleton.Add(new SkeletonBone(
                nodeIndex,
                node.Name,
                new System.Numerics.Vector3(matrix.M41, matrix.M42, matrix.M43),
                matrix,
                parentBone,
                IsAttachmentPointName(node.Name),
                paletteReferences.TryGetValue(nodeIndex, out List<BonePaletteReference>? refs)
                    ? refs.ToArray() : []));
        }

        foreach (SmoObjectEntry node in document.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.Node &&
                     IsAttachmentPointName(entry.Name) &&
                     !boneIndices.Contains(entry.Index)))
        {
            if (!TryResolveNodeWorldMatrix(
                    document, node, nodeHierarchy, bindWorldMatrices, out Matrix4x4 world))
                continue;
            skeleton.Add(new SkeletonBone(
                node.Index,
                node.Name,
                new System.Numerics.Vector3(world.M41, world.M42, world.M43),
                world,
                FindNearestBoneParent(node.Index, boneIndices, document.Objects, nodeHierarchy),
                true,
                []));
        }

        List<CollisionVolume> collisionVolumes = DecodeCollisionVolumes(
            document, nodeHierarchy, bindWorldMatrices);
        List<HelperNode> controlRig = DecodeControlRig(
            document, nodeHierarchy, bindWorldMatrices);
        List<HelperNode> markers = DecodeServiceMarkers(
            document, nodeHierarchy, bindWorldMatrices);
        List<AnimationNode> animationNodes = document.Objects
            .Where(entry => entry.TypeHash is SmoClassIds.Node or SmoClassIds.RenderNode)
            .Select(entry =>
            {
                TryResolveNodeWorldMatrix(
                    document, entry, nodeHierarchy, bindWorldMatrices, out Matrix4x4 bindWorld);
                return new AnimationNode(
                    entry.Index, entry.Name, bindWorld,
                    GetLogicalParent(entry.Index, document.Objects, nodeHierarchy));
            }).ToList();
        List<AuxiliaryObjectInfo> auxiliaryObjects = document.Objects
            .Where(entry => entry.Index is 85 or 88 or 91 or 92 or 95 or 119 or 120)
            .Select(entry => new AuxiliaryObjectInfo(entry.Index, entry.Name,
                GetAuxiliaryRole(entry)))
            .ToList();

        return new DecodedSmoFile(
            fileName,
            meshEntries.Length,
            renderMeshes,
            document.Objects.Select(entry => new SceneObjectInfo(
                entry.Index,
                entry.ParentIndex,
                entry.TypeHash,
                entry.Name)).ToArray(),
            document.Diagnostics.ToArray(),
            decodeErrors,
            textureIssues,
            skeleton,
            collisionVolumes,
            controlRig,
            markers,
            auxiliaryObjects,
            skins,
            animationNodes);
    }

    private static List<CollisionVolume> DecodeCollisionVolumes(
        SmoDocument document, SmoNodeHierarchy hierarchy,
        IReadOnlyDictionary<int, Matrix4x4> bindWorldMatrices)
    {
        List<CollisionVolume> result = new();
        foreach (SmoObjectEntry node in document.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.Node &&
                     entry.Name.StartsWith("collision_volume_", StringComparison.OrdinalIgnoreCase)))
        {
            SmoObjectEntry? shape = document.Objects.FirstOrDefault(entry =>
                entry.ParentIndex is int infoIndex &&
                document.Objects[infoIndex].ParentIndex == node.Index &&
                entry.TypeHash == 0x4DA04889);
            if (shape is null || !TryReadVectorField(document, shape, 1, out var size) ||
                !TryResolveNodeWorldMatrix(document, node, hierarchy, bindWorldMatrices, out Matrix4x4 world))
                continue;
            result.Add(new CollisionVolume(node.Index, node.Name, world, size));
        }
        return result;
    }

    private static List<HelperNode> DecodeControlRig(
        SmoDocument document, SmoNodeHierarchy hierarchy,
        IReadOnlyDictionary<int, Matrix4x4> bindWorldMatrices)
    {
        SmoObjectEntry? root = document.Objects.FirstOrDefault(entry =>
            entry.TypeHash == SmoClassIds.Node &&
            entry.Name.Equals("SubMaster", StringComparison.OrdinalIgnoreCase));
        if (root is null) return [];
        HashSet<int> members = [root.Index];
        bool changed;
        do
        {
            changed = false;
            foreach (SmoObjectEntry entry in document.Objects.Where(entry => entry.TypeHash == SmoClassIds.Node))
            {
                // Keep the authored control subtree. esfNodeChild references from
                // C-lowerRoot/C-upperRoot point back into the deform skeleton and
                // must not turn the whole skin skeleton into control geometry.
                int? parent = entry.ParentIndex;
                if (parent is int parentIndex && members.Contains(parentIndex))
                    changed |= members.Add(entry.Index);
            }
        } while (changed);

        return members.Select(index => document.Objects[index]).Select(node =>
        {
            TryResolveNodeWorldMatrix(document, node, hierarchy, bindWorldMatrices, out Matrix4x4 world);
            int? parent = node.ParentIndex;
            return new HelperNode(node.Index, node.Name,
                new System.Numerics.Vector3(world.M41, world.M42, world.M43),
                parent is int value && members.Contains(value) ? value : null);
        }).ToList();
    }

    private static List<HelperNode> DecodeServiceMarkers(
        SmoDocument document, SmoNodeHierarchy hierarchy,
        IReadOnlyDictionary<int, Matrix4x4> bindWorldMatrices) => document.Objects
        .Where(entry => entry.TypeHash == SmoClassIds.Node &&
            (entry.Name.Equals("movement_tracker", StringComparison.OrdinalIgnoreCase) ||
             entry.Name.Equals("BLOOM", StringComparison.OrdinalIgnoreCase)))
        .Select(node =>
        {
            TryResolveNodeWorldMatrix(document, node, hierarchy, bindWorldMatrices, out Matrix4x4 world);
            return new HelperNode(node.Index, node.Name,
                new System.Numerics.Vector3(world.M41, world.M42, world.M43), null);
        }).ToList();

    private static string GetAuxiliaryRole(SmoObjectEntry entry) => entry.Index switch
    {
        85 or 88 or 92 => "collision volume",
        91 => "movement tracker",
        95 => "control / IK rig root",
        119 => "named service marker",
        120 => "ambient/light preset (class not identified)",
        _ => "auxiliary object"
    };

    private static bool TryReadVectorField(
        SmoDocument document, SmoObjectEntry entry, int expectedType,
        out System.Numerics.Vector3 value)
    {
        value = default;
        if (!entry.IsWithinDataSection || entry.SerializedSize > int.MaxValue ||
            entry.PhysicalOffset < 0 || entry.PhysicalEnd > document.Data.Length)
            return false;
        ReadOnlySpan<byte> data = document.Data.Span.Slice(
            (int)entry.PhysicalOffset, (int)entry.SerializedSize);
        int offset = data.Length >= 8 ? 8 : 0;
        if (!SmoDataBlockReader.TryReadHeader(data, offset, out SmoDataBlockHeader header) ||
            header.FieldType != expectedType || header.PayloadSize != 12)
            return false;
        ReadOnlySpan<byte> payload = data.Slice(header.PayloadOffset, 12);
        value = new System.Numerics.Vector3(
            BitConverter.Int32BitsToSingle(BinaryPrimitives.ReadInt32LittleEndian(payload)),
            BitConverter.Int32BitsToSingle(BinaryPrimitives.ReadInt32LittleEndian(payload[4..])),
            BitConverter.Int32BitsToSingle(BinaryPrimitives.ReadInt32LittleEndian(payload[8..])));
        return float.IsFinite(value.X) && float.IsFinite(value.Y) && float.IsFinite(value.Z);
    }

    private static IReadOnlyDictionary<int, float> ResolveBoneInfluences(
        SmoMesh mesh, SmoSkin skin)
    {
        var result = new Dictionary<int, float>();
        if (!mesh.HasSkinningData)
            return result;

        for (int vertex = 0; vertex < mesh.VertexCount; vertex++)
        {
            System.Numerics.Vector4 weights = mesh.BlendWeights[vertex];
            SmoBlendIndices indices = mesh.BlendIndices[vertex];
            Add(indices.X, weights.X);
            Add(indices.Y, weights.Y);
            Add(indices.Z, weights.Z);
            Add(indices.W, weights.W);
        }
        return result;

        void Add(int paletteIndex, float weight)
        {
            if (weight <= 0.000001f || (uint)paletteIndex >= (uint)skin.Bones.Count)
                return;
            int nodeIndex = skin.Bones[paletteIndex].NodeObjectIndex;
            result[nodeIndex] = Math.Max(result.GetValueOrDefault(nodeIndex), weight);
        }
    }

    private static int? FindAncestorObjectIndex(
        IReadOnlyList<SmoObjectEntry> entries,
        SmoObjectEntry entry,
        uint typeHash)
    {
        SmoObjectEntry? cursor = entry;
        while (cursor.ParentIndex is int parentIndex &&
               (uint)parentIndex < (uint)entries.Count)
        {
            cursor = entries[parentIndex];
            if (cursor.TypeHash == typeHash)
                return cursor.Index;
        }
        return null;
    }

    private static int? FindNearestBoneParent(
        int nodeIndex,
        IReadOnlySet<int> boneIndices,
        IReadOnlyList<SmoObjectEntry> entries,
        SmoNodeHierarchy hierarchy)
    {
        var visited = new HashSet<int> { nodeIndex };
        int? cursor = GetLogicalParent(nodeIndex, entries, hierarchy);
        while (cursor is int index && visited.Add(index))
        {
            if (boneIndices.Contains(index))
                return index;
            cursor = GetLogicalParent(index, entries, hierarchy);
        }
        return null;
    }

    private static int? GetLogicalParent(
        int objectIndex,
        IReadOnlyList<SmoObjectEntry> entries,
        SmoNodeHierarchy hierarchy) =>
        hierarchy.ParentsByChild.TryGetValue(
            objectIndex, out IReadOnlyList<int>? parents) && parents.Count == 1
                ? parents[0]
                : (uint)objectIndex < (uint)entries.Count
                    ? entries[objectIndex].ParentIndex
                    : null;

    private static bool TryResolveNodeWorldMatrix(
        SmoDocument document,
        SmoObjectEntry node,
        SmoNodeHierarchy hierarchy,
        IReadOnlyDictionary<int, Matrix4x4> bindWorldMatrices,
        out Matrix4x4 world)
    {
        world = Matrix4x4.Identity;
        var visited = new HashSet<int>();
        SmoObjectEntry? cursor = node;
        while (cursor is not null && visited.Add(cursor.Index))
        {
            if (bindWorldMatrices.TryGetValue(cursor.Index, out Matrix4x4 bindWorld))
            {
                world *= bindWorld;
                return true;
            }
            if (SmoNodeTransformDecoder.TryDecode(
                    document, cursor, out SmoNodeTransform? transform) && transform is not null)
                world *= transform.LocalMatrix;
            else if (TryReadVectorField(document, cursor, 0, out var position))
                world *= Matrix4x4.CreateTranslation(position);

            int? parentIndex = GetLogicalParent(cursor.Index, document.Objects, hierarchy);
            cursor = parentIndex is int index && (uint)index < (uint)document.Objects.Count
                ? document.Objects[index]
                : null;
        }
        return visited.Count > 0;
    }

    private static bool IsAttachmentPointName(string name) =>
        name.Contains("attach", StringComparison.OrdinalIgnoreCase) ||
        name.Contains("socket", StringComparison.OrdinalIgnoreCase) ||
        name.Contains("locator", StringComparison.OrdinalIgnoreCase) ||
        name.Contains("hook", StringComparison.OrdinalIgnoreCase) ||
        name.StartsWith("hair_top_", StringComparison.OrdinalIgnoreCase) ||
        name.StartsWith("hair_bottom_", StringComparison.OrdinalIgnoreCase);

    private static void AddDocumentIssues(
        DecodedSmoFile decodedFile,
        ICollection<LoadIssue> target)
    {
        string fileName = Path.GetFileName(decodedFile.Path);

        foreach (SmoDiagnostic diagnostic in decodedFile.Diagnostics
                     .OrderByDescending(item => item.Severity))
        {
            string location = diagnostic.Offset is long offset
                ? $" @0x{offset:X}"
                : string.Empty;
            target.Add(new LoadIssue(
                diagnostic.Severity == SmoDiagnosticSeverity.Error,
                $"{fileName}: {diagnostic.Severity} {diagnostic.Code}{location}: " +
                diagnostic.Message));
        }

        foreach (string decodeError in decodedFile.DecodeErrors)
        {
            target.Add(new LoadIssue(
                true,
                $"{fileName}: unsupported mesh: {decodeError}"));
        }

        foreach (string textureIssue in decodedFile.TextureIssues)
        {
            target.Add(new LoadIssue(
                false,
                $"{fileName}: texture fallback: {textureIssue}"));
        }
    }

    private SceneAddResult AddModelToScene(
        IReadOnlyList<DecodedRenderMesh> renderMeshes, int colorIndex)
    {
        Model3DGroup fileModel = new();
        BoundsBuilder fileBounds = new();
        Color baseColor = ModelColors[colorIndex % ModelColors.Length];
        int addedMeshes = 0;
        int texturedMeshes = 0;
        List<GeometryModel3D> opaqueModels = [];
        List<GeometryModel3D> transparentModels = [];

        foreach (DecodedRenderMesh renderMesh in renderMeshes)
        {
            SmoMesh mesh = renderMesh.Mesh;
            BoundsBuilder meshBounds = new();
            MeshGeometry3D? geometry = CreateGeometry(renderMesh, meshBounds);
            if (geometry is null)
                continue;
            fileBounds.Merge(meshBounds);

            Color color = VaryColor(baseColor, addedMeshes);
            Material material = CreateMaterial(renderMesh, color);
            GeometryModel3D model = new(geometry, material)
            {
                BackMaterial = material
            };

            if (renderMesh.AnimationFrames is { Count: > 1 } frames &&
                renderMesh.AnimationFrameDuration is TimeSpan frameDuration &&
                frameDuration > TimeSpan.Zero)
            {
                Material[] frameMaterials = frames
                    .Select(frame => CreateMaterial(renderMesh with { Texture = frame }, color))
                    .ToArray();
                model.Material = frameMaterials[0];
                model.BackMaterial = frameMaterials[0];
                material = frameMaterials[0];
                _animatedMaterials.Add(new AnimatedSceneMaterial(
                    model, frameMaterials, frameDuration));
            }

            (RequiresTransparentOrdering(renderMesh)
                    ? transparentModels
                    : opaqueModels)
                .Add(model);
            _sceneGeometry[new SceneObjectKey(colorIndex, mesh.ObjectIndex)] =
                new SceneGeometry(model, geometry, material, meshBounds, renderMesh);
            _geometryObjectKeys[model] = new SceneObjectKey(colorIndex, mesh.ObjectIndex);
            addedMeshes++;
            if (renderMesh.Texture is not null)
                texturedMeshes++;
        }

        // WPF 3D does not sort transparent GeometryModel3D instances. Render
        // opaque geometry first so translucent shields/effects do not fill the
        // depth buffer before the character behind them is drawn.
        foreach (GeometryModel3D model in opaqueModels)
            fileModel.Children.Add(model);
        foreach (GeometryModel3D model in transparentModels)
            fileModel.Children.Add(model);

        if (addedMeshes > 0)
        {
            LoadedModelsRoot.Children.Add(fileModel);
            _sceneBounds.Merge(fileBounds);
        }

        return new SceneAddResult(addedMeshes, texturedMeshes);
    }

    private static bool RequiresTransparentOrdering(DecodedRenderMesh renderMesh)
        => renderMesh.UsesAlphaBlend;

    private void AddSkeletonToScene(DecodedSmoFile file, int fileIndex)
    {
        Dictionary<int, SkeletonBone> byIndex = file.Skeleton
            .ToDictionary(bone => bone.ObjectIndex);

        BoundsBuilder bounds = new();
        foreach (SkeletonBone bone in file.Skeleton)
            bounds.Include(ToViewportPoint(bone.Position));
        double radius = Math.Max(bounds.DiagonalLength * 0.006, 0.008);
        Material boneMaterial = CreateSolidMaterial(Color.FromRgb(70, 220, 255));
        Material jointMaterial = CreateSolidMaterial(Color.FromRgb(255, 218, 75));
        Material attachmentMaterial = CreateSolidMaterial(Color.FromRgb(255, 70, 190));

        foreach (SkeletonBone bone in file.Skeleton)
        {
            Point3D point = ToViewportPoint(bone.Position);
            if (bone.IsAttachment)
            {
                _attachmentModels.Add(CreateOctahedron(
                    point, radius * 1.8, attachmentMaterial));
            }
            else
            {
                _skeletonModels.Add(CreateOctahedron(point, radius, jointMaterial));
            }

            if (!bone.IsAttachment && bone.ParentObjectIndex is int parentIndex &&
                byIndex.TryGetValue(parentIndex, out SkeletonBone? parent))
            {
                Point3D parentPoint = ToViewportPoint(parent.Position);
                if ((point - parentPoint).Length > radius * 0.25)
                    _skeletonModels.Add(CreateBoneSegment(
                        parentPoint, point, radius * 0.35, boneMaterial));
            }

            string palettes = bone.Palettes.Count == 0
                ? "без palette"
                : string.Join(", ", bone.Palettes.Select(reference =>
                    $"{reference.SkinName}[{reference.PaletteIndex}]"));
            _allBoneItems.Add(new BoneListItem(
                fileIndex,
                bone.ObjectIndex,
                bone.Name,
                $"{bone.Name}  ·  {palettes}"));
        }

        double helperRadius = Math.Max(bounds.DiagonalLength * 0.0035, 0.006);
        Material collisionMaterial = CreateSolidMaterial(Color.FromRgb(80, 255, 105));
        foreach (CollisionVolume collision in file.CollisionVolumes)
            _collisionModels.Add(CreateCollisionBox(collision, helperRadius, collisionMaterial));

        Material rigLineMaterial = CreateSolidMaterial(Color.FromRgb(180, 105, 255));
        Material rigPointMaterial = CreateSolidMaterial(Color.FromRgb(225, 175, 255));
        Dictionary<int, HelperNode> rigByIndex = file.ControlRig.ToDictionary(item => item.ObjectIndex);
        foreach (HelperNode node in file.ControlRig)
        {
            Point3D point = ToViewportPoint(node.Position);
            _controlRigModels.Add(CreateOctahedron(point, helperRadius * 1.25, rigPointMaterial));
            if (node.ParentObjectIndex is int parentIndex && rigByIndex.TryGetValue(parentIndex, out HelperNode? parent))
            {
                Point3D parentPoint = ToViewportPoint(parent.Position);
                if ((point - parentPoint).Length > helperRadius * 0.25)
                    _controlRigModels.Add(CreateBoneSegment(parentPoint, point, helperRadius * 0.3, rigLineMaterial));
            }
        }

        Material markerMaterial = CreateSolidMaterial(Color.FromRgb(255, 155, 45));
        foreach (HelperNode marker in file.Markers)
            _markerModels.Add(CreateOctahedron(
                ToViewportPoint(marker.Position), helperRadius * 2.2, markerMaterial));
        foreach (AuxiliaryObjectInfo item in file.AuxiliaryObjects)
            _auxiliaryItems.Add(new AuxiliaryObjectItem(
                fileIndex, item.ObjectIndex,
                $"[{item.ObjectIndex}] {item.Name} — {item.Role}"));
        AuxiliaryObjectList.ItemsSource = null;
        AuxiliaryObjectList.ItemsSource = _auxiliaryItems.ToArray();
    }

    private static Model3DGroup CreateCollisionBox(
        CollisionVolume collision, double radius, Material material)
    {
        var group = new Model3DGroup();
        System.Numerics.Vector3 half = collision.Size * 0.5f;
        System.Numerics.Vector3[] local =
        [
            new(-half.X,-half.Y,-half.Z), new(half.X,-half.Y,-half.Z),
            new(half.X,half.Y,-half.Z), new(-half.X,half.Y,-half.Z),
            new(-half.X,-half.Y,half.Z), new(half.X,-half.Y,half.Z),
            new(half.X,half.Y,half.Z), new(-half.X,half.Y,half.Z)
        ];
        Point3D[] points = local.Select(point => ToViewportPoint(
            System.Numerics.Vector3.Transform(point, collision.WorldTransform))).ToArray();
        int[] edges = [0,1, 1,2, 2,3, 3,0, 4,5, 5,6, 6,7, 7,4, 0,4, 1,5, 2,6, 3,7];
        for (int i = 0; i < edges.Length; i += 2)
            group.Children.Add(CreateBoneSegment(
                points[edges[i]], points[edges[i + 1]], radius, material));
        return group;
    }

    private static Point3D ToViewportPoint(System.Numerics.Vector3 point) =>
        new(point.X, point.Y, -point.Z);

    private static Material CreateSolidMaterial(Color color)
    {
        var group = new MaterialGroup();
        group.Children.Add(new DiffuseMaterial(new SolidColorBrush(color)));
        group.Children.Add(new EmissiveMaterial(new SolidColorBrush(
            Color.FromArgb(120, color.R, color.G, color.B))));
        group.Freeze();
        return group;
    }

    private static GeometryModel3D CreateOctahedron(
        Point3D center, double radius, Material material)
    {
        Point3DCollection positions =
        [
            new(center.X + radius, center.Y, center.Z),
            new(center.X - radius, center.Y, center.Z),
            new(center.X, center.Y + radius, center.Z),
            new(center.X, center.Y - radius, center.Z),
            new(center.X, center.Y, center.Z + radius),
            new(center.X, center.Y, center.Z - radius)
        ];
        Int32Collection triangles =
        [
            2, 0, 4, 2, 4, 1, 2, 1, 5, 2, 5, 0,
            3, 4, 0, 3, 1, 4, 3, 5, 1, 3, 0, 5
        ];
        var geometry = new MeshGeometry3D
        {
            Positions = positions,
            TriangleIndices = triangles
        };
        geometry.Freeze();
        return new GeometryModel3D(geometry, material) { BackMaterial = material };
    }

    private static GeometryModel3D CreateBoneSegment(
        Point3D start, Point3D end, double radius, Material material)
    {
        Vector3D axis = end - start;
        axis.Normalize();
        Vector3D reference = Math.Abs(Vector3D.DotProduct(axis, new Vector3D(0, 1, 0))) > 0.9
            ? new Vector3D(1, 0, 0)
            : new Vector3D(0, 1, 0);
        Vector3D side = Vector3D.CrossProduct(axis, reference);
        side.Normalize();
        Vector3D up = Vector3D.CrossProduct(side, axis);
        up.Normalize();
        const int sides = 8;
        var positions = new Point3DCollection(sides * 2);
        for (int index = 0; index < sides; index++)
        {
            double angle = 2 * Math.PI * index / sides;
            Vector3D offset = radius * (side * Math.Cos(angle) + up * Math.Sin(angle));
            positions.Add(start + offset);
            positions.Add(end + offset);
        }
        var triangles = new Int32Collection(sides * 6);
        for (int index = 0; index < sides; index++)
        {
            int next = (index + 1) % sides;
            triangles.Add(index * 2); triangles.Add(next * 2); triangles.Add(index * 2 + 1);
            triangles.Add(index * 2 + 1); triangles.Add(next * 2); triangles.Add(next * 2 + 1);
        }
        var geometry = new MeshGeometry3D
        {
            Positions = positions,
            TriangleIndices = triangles
        };
        geometry.Freeze();
        return new GeometryModel3D(geometry, material) { BackMaterial = material };
    }

    private static Material CreateMaterial(DecodedRenderMesh renderMesh, Color fallbackColor)
    {
        if (renderMesh.Texture is null || !renderMesh.Mesh.HasTextureCoordinates)
        {
            if (renderMesh.Mesh.VertexFormat == 0x0100 &&
                renderMesh.Mesh.HasDiffuseColors)
            {
                return CreateAverageVertexColorMaterial(renderMesh.Mesh);
            }

            if (!renderMesh.Mesh.HasSkinningData &&
                HasUniformVertexColor(renderMesh.Mesh))
            {
                return CreateAverageVertexColorMaterial(renderMesh.Mesh);
            }

            if (renderMesh.Mesh.HasDiffuseColors &&
                renderMesh.Mesh.HasTextureCoordinates &&
                UsesRenderableVertexColors(renderMesh.Mesh))
            {
                return CreateVertexColorMaterial(renderMesh.Mesh);
            }

            Color color = renderMesh.MaterialColorArgb is uint argb
                ? Color.FromArgb(
                    (byte)(argb >> 24),
                    (byte)(argb >> 16),
                    (byte)(argb >> 8),
                    (byte)argb)
                : fallbackColor;
            DiffuseMaterial fallback = new(new SolidColorBrush(color));
            fallback.Freeze();
            return fallback;
        }

        SmoTexture texture = renderMesh.BaseTexture ?? renderMesh.Texture;
        byte[] pixels;
        int textureWidth = texture.Width;
        int textureHeight = texture.Height;
        if (renderMesh.BaseTexture is not null &&
            renderMesh.Mesh.HasTextureCoordinates1)
        {
            (pixels, textureWidth, textureHeight) =
                CreateLayeredPixelBuffer(renderMesh);
        }
        else
        {
            pixels = CreateTintedPixelBuffer(renderMesh);
        }
        BitmapSource bitmap = BitmapSource.Create(
            textureWidth,
            textureHeight,
            96,
            96,
            PixelFormats.Bgra32,
            null,
            pixels,
            textureWidth * 4);
        bitmap.Freeze();

        ImageBrush brush = new(bitmap)
        {
            TileMode = TileMode.Tile,
            ViewportUnits = BrushMappingMode.Absolute,
            Viewport = new Rect(0, 0, 1, 1),
            Stretch = Stretch.Fill
        };
        brush.Freeze();
        DiffuseMaterial material = new(brush);
        material.Freeze();
        return material;
    }

    private static Material CreateAverageVertexColorMaterial(SmoMesh mesh)
    {
        // GUI state meshes in menu.smo use XYZ + diffuse ARGB without UVs.
        // WPF has no per-vertex color channel, but these meshes are normally
        // authored with one state color. Averaging preserves that information
        // instead of replacing every state with the viewer fallback color.
        ulong alpha = 0;
        ulong red = 0;
        ulong green = 0;
        ulong blue = 0;
        foreach (uint argb in mesh.DiffuseColorsArgb)
        {
            alpha += argb >> 24;
            red += (argb >> 16) & 0xFF;
            green += (argb >> 8) & 0xFF;
            blue += argb & 0xFF;
        }

        ulong count = (ulong)mesh.DiffuseColorsArgb.Length;
        Color color = Color.FromArgb(
            (byte)(alpha / count),
            (byte)(red / count),
            (byte)(green / count),
            (byte)(blue / count));
        DiffuseMaterial material = new(new SolidColorBrush(color));
        material.Freeze();
        return material;
    }

    private static bool HasUniformVertexColor(SmoMesh mesh) =>
        mesh.HasDiffuseColors &&
        mesh.DiffuseColorsArgb.Skip(1)
            .All(color => color == mesh.DiffuseColorsArgb[0]);

    private static (byte[] Pixels, int Width, int Height) CreateLayeredPixelBuffer(
        DecodedRenderMesh renderMesh)
    {
        SmoTexture baseTexture = renderMesh.BaseTexture!;
        SmoTexture effectTexture = renderMesh.Texture!;
        SmoMesh mesh = renderMesh.Mesh;
        int width = Math.Max(baseTexture.Width, effectTexture.Width);
        int height = Math.Max(baseTexture.Height, effectTexture.Height);
        byte[] pixels = ResizeTexturePixels(baseTexture, width, height);

        for (int triangle = 0; triangle < mesh.TriangleIndices.Length; triangle += 3)
        {
            int ia = checked((int)mesh.TriangleIndices[triangle]);
            int ib = checked((int)mesh.TriangleIndices[triangle + 1]);
            int ic = checked((int)mesh.TriangleIndices[triangle + 2]);
            RasterizeEffectTriangle(
                pixels,
                width,
                height,
                effectTexture,
                mesh.TextureCoordinates[ia],
                mesh.TextureCoordinates[ib],
                mesh.TextureCoordinates[ic],
                mesh.TextureCoordinates1[ia],
                mesh.TextureCoordinates1[ib],
                mesh.TextureCoordinates1[ic]);
        }

        return (pixels, width, height);
    }

    private static byte[] ResizeTexturePixels(
        SmoTexture texture,
        int width,
        int height)
    {
        if (texture.Width == width && texture.Height == height)
            return texture.Bgra32Pixels.ToArray();

        byte[] result = new byte[checked(width * height * 4)];
        ReadOnlySpan<byte> source = texture.Bgra32Pixels.Span;
        for (int y = 0; y < height; y++)
        {
            int sourceY = height == 1
                ? 0
                : (int)MathF.Round(y * (texture.Height - 1f) / (height - 1f));
            for (int x = 0; x < width; x++)
            {
                int sourceX = width == 1
                    ? 0
                    : (int)MathF.Round(x * (texture.Width - 1f) / (width - 1f));
                int sourceOffset = (sourceY * texture.Width + sourceX) * 4;
                int targetOffset = (y * width + x) * 4;
                source.Slice(sourceOffset, 4).CopyTo(result.AsSpan(targetOffset, 4));
            }
        }

        return result;
    }

    private static void RasterizeEffectTriangle(
        byte[] destination,
        int width,
        int height,
        SmoTexture effect,
        System.Numerics.Vector2 a,
        System.Numerics.Vector2 b,
        System.Numerics.Vector2 c,
        System.Numerics.Vector2 effectA,
        System.Numerics.Vector2 effectB,
        System.Numerics.Vector2 effectC)
    {
        if (!float.IsFinite(a.X) || !float.IsFinite(a.Y) ||
            !float.IsFinite(b.X) || !float.IsFinite(b.Y) ||
            !float.IsFinite(c.X) || !float.IsFinite(c.Y))
        {
            return;
        }

        int firstTileX = (int)MathF.Floor(MathF.Min(a.X, MathF.Min(b.X, c.X)));
        int lastTileX = (int)MathF.Floor(MathF.Max(a.X, MathF.Max(b.X, c.X)));
        int firstTileY = (int)MathF.Floor(MathF.Min(a.Y, MathF.Min(b.Y, c.Y)));
        int lastTileY = (int)MathF.Floor(MathF.Max(a.Y, MathF.Max(b.Y, c.Y)));
        for (int tileY = firstTileY; tileY <= lastTileY; tileY++)
        for (int tileX = firstTileX; tileX <= lastTileX; tileX++)
        {
            System.Numerics.Vector2 offset = new(tileX, tileY);
            RasterizeEffectTriangleTile(
                destination,
                width,
                height,
                effect,
                a - offset,
                b - offset,
                c - offset,
                effectA,
                effectB,
                effectC);
        }
    }

    private static void RasterizeEffectTriangleTile(
        byte[] destination,
        int width,
        int height,
        SmoTexture effect,
        System.Numerics.Vector2 a,
        System.Numerics.Vector2 b,
        System.Numerics.Vector2 c,
        System.Numerics.Vector2 effectA,
        System.Numerics.Vector2 effectB,
        System.Numerics.Vector2 effectC)
    {
        float ax = a.X * (width - 1);
        float ay = a.Y * (height - 1);
        float bx = b.X * (width - 1);
        float by = b.Y * (height - 1);
        float cx = c.X * (width - 1);
        float cy = c.Y * (height - 1);
        float area = Edge(ax, ay, bx, by, cx, cy);
        if (MathF.Abs(area) < 0.0000001f)
            return;

        int minX = Math.Clamp((int)MathF.Floor(MathF.Min(ax, MathF.Min(bx, cx))), 0, width - 1);
        int maxX = Math.Clamp((int)MathF.Ceiling(MathF.Max(ax, MathF.Max(bx, cx))), 0, width - 1);
        int minY = Math.Clamp((int)MathF.Floor(MathF.Min(ay, MathF.Min(by, cy))), 0, height - 1);
        int maxY = Math.Clamp((int)MathF.Ceiling(MathF.Max(ay, MathF.Max(by, cy))), 0, height - 1);
        ReadOnlySpan<byte> effectPixels = effect.Bgra32Pixels.Span;

        for (int y = minY; y <= maxY; y++)
        {
            for (int x = minX; x <= maxX; x++)
            {
                float wa = Edge(bx, by, cx, cy, x + 0.5f, y + 0.5f) / area;
                float wb = Edge(cx, cy, ax, ay, x + 0.5f, y + 0.5f) / area;
                float wc = 1f - wa - wb;
                if (MathF.Min(wa, MathF.Min(wb, wc)) < -0.00001f)
                    continue;

                float u = wa * effectA.X + wb * effectB.X + wc * effectC.X;
                float v = wa * effectA.Y + wb * effectB.Y + wc * effectC.Y;
                u -= MathF.Floor(u);
                v -= MathF.Floor(v);
                int effectX = Math.Clamp(
                    (int)MathF.Round(u * (effect.Width - 1)), 0, effect.Width - 1);
                int effectY = Math.Clamp(
                    (int)MathF.Round(v * (effect.Height - 1)), 0, effect.Height - 1);
                int source = (effectY * effect.Width + effectX) * 4;
                int target = (y * width + x) * 4;
                int alpha = effectPixels[source + 3];
                for (int channel = 0; channel < 3; channel++)
                {
                    destination[target + channel] = (byte)Math.Min(
                        255,
                        destination[target + channel] +
                        effectPixels[source + channel] * alpha / 255);
                }
            }
        }
    }

    private static byte[] CreateTintedPixelBuffer(DecodedRenderMesh renderMesh)
    {
        SmoTexture texture = renderMesh.Texture!;
        byte[] pixels = texture.Bgra32Pixels.ToArray();
        SmoMesh mesh = renderMesh.Mesh;
        if (!SmoVertexColorUsage.ShouldModulateTexture(mesh) ||
            !mesh.HasDiffuseColors ||
            !mesh.HasTextureCoordinates)
            return pixels;

        byte[] tint = new byte[texture.Width * texture.Height * 3];
        bool[] covered = new bool[texture.Width * texture.Height];
        int[] tintSamples = new int[texture.Width * texture.Height];
        Dictionary<int, (long Blue, long Green, long Red, int Samples)> degenerateUvColors = [];

        for (int triangle = 0; triangle < mesh.TriangleIndices.Length; triangle += 3)
        {
            int ia = checked((int)mesh.TriangleIndices[triangle]);
            int ib = checked((int)mesh.TriangleIndices[triangle + 1]);
            int ic = checked((int)mesh.TriangleIndices[triangle + 2]);
            System.Numerics.Vector2 uvA = mesh.TextureCoordinates[ia];
            System.Numerics.Vector2 uvB = mesh.TextureCoordinates[ib];
            System.Numerics.Vector2 uvC = mesh.TextureCoordinates[ic];
            float uvArea = MathF.Abs(Edge(uvA.X, uvA.Y, uvB.X, uvB.Y, uvC.X, uvC.Y));
            if (uvArea < 0.0000001f)
            {
                int x = Math.Clamp(
                    (int)MathF.Round(uvA.X * (texture.Width - 1)), 0, texture.Width - 1);
                int y = Math.Clamp(
                    (int)MathF.Round(uvA.Y * (texture.Height - 1)), 0, texture.Height - 1);
                int pixel = y * texture.Width + x;
                (long blue, long green, long red, int samples) =
                    degenerateUvColors.GetValueOrDefault(pixel);
                foreach (uint color in new[]
                         {
                             mesh.DiffuseColorsArgb[ia],
                             mesh.DiffuseColorsArgb[ib],
                             mesh.DiffuseColorsArgb[ic]
                         })
                {
                    blue += color & 0xFF;
                    green += (color >> 8) & 0xFF;
                    red += (color >> 16) & 0xFF;
                    samples++;
                }
                degenerateUvColors[pixel] = (blue, green, red, samples);
                continue;
            }
            RasterizeVertexColorTriangle(
                tint,
                covered,
                tintSamples,
                texture.Width,
                texture.Height,
                mesh.TextureCoordinates[ia],
                mesh.TextureCoordinates[ib],
                mesh.TextureCoordinates[ic],
                mesh.DiffuseColorsArgb[ia],
                mesh.DiffuseColorsArgb[ib],
                mesh.DiffuseColorsArgb[ic]);
        }

        DilateVertexColorCoverage(tint, covered, texture.Width, texture.Height, 2);

        for (int pixel = 0; pixel < covered.Length; pixel++)
        {
            if (!covered[pixel])
                continue;
            int pixelOffset = pixel * 4;
            int tintOffset = pixel * 3;
            pixels[pixelOffset] = (byte)(pixels[pixelOffset] * tint[tintOffset] / 255);
            pixels[pixelOffset + 1] =
                (byte)(pixels[pixelOffset + 1] * tint[tintOffset + 1] / 255);
            pixels[pixelOffset + 2] =
                (byte)(pixels[pixelOffset + 2] * tint[tintOffset + 2] / 255);
        }

        // Some character face polygons deliberately collapse all three UVs to
        // one sentinel texel and carry their appearance only in vertex diffuse.
        // WPF has no vertex-color input, so preserve their average diffuse in
        // the private texture copy instead of stretching the sentinel's black.
        foreach ((int pixel, (long blue, long green, long red, int samples)) in
                 degenerateUvColors)
        {
            int centerX = pixel % texture.Width;
            int centerY = pixel / texture.Width;
            for (int y = Math.Max(0, centerY - 1);
                 y <= Math.Min(texture.Height - 1, centerY + 1); y++)
            {
                for (int x = Math.Max(0, centerX - 1);
                     x <= Math.Min(texture.Width - 1, centerX + 1); x++)
                {
                    int offset = (y * texture.Width + x) * 4;
                    pixels[offset] = (byte)(blue / samples);
                    pixels[offset + 1] = (byte)(green / samples);
                    pixels[offset + 2] = (byte)(red / samples);
                    pixels[offset + 3] = 255;
                }
            }
        }

        return pixels;
    }

    private static void DilateVertexColorCoverage(
        byte[] tint,
        bool[] covered,
        int width,
        int height,
        int iterations)
    {
        for (int iteration = 0; iteration < iterations; iteration++)
        {
            byte[] sourceTint = (byte[])tint.Clone();
            bool[] sourceCovered = (bool[])covered.Clone();
            bool changed = false;
            for (int y = 0; y < height; y++)
            {
                for (int x = 0; x < width; x++)
                {
                    int pixel = y * width + x;
                    if (sourceCovered[pixel])
                        continue;

                    int blue = 0;
                    int green = 0;
                    int red = 0;
                    int count = 0;
                    for (int offsetY = -1; offsetY <= 1; offsetY++)
                    {
                        int neighborY = y + offsetY;
                        if (neighborY < 0 || neighborY >= height)
                            continue;
                        for (int offsetX = -1; offsetX <= 1; offsetX++)
                        {
                            if (offsetX == 0 && offsetY == 0)
                                continue;
                            int neighborX = x + offsetX;
                            if (neighborX < 0 || neighborX >= width)
                                continue;
                            int neighbor = neighborY * width + neighborX;
                            if (!sourceCovered[neighbor])
                                continue;
                            int source = neighbor * 3;
                            blue += sourceTint[source];
                            green += sourceTint[source + 1];
                            red += sourceTint[source + 2];
                            count++;
                        }
                    }

                    if (count == 0)
                        continue;
                    int destination = pixel * 3;
                    tint[destination] = (byte)(blue / count);
                    tint[destination + 1] = (byte)(green / count);
                    tint[destination + 2] = (byte)(red / count);
                    covered[pixel] = true;
                    changed = true;
                }
            }

            if (!changed)
                break;
        }
    }

    private static Material CreateVertexColorMaterial(SmoMesh mesh)
    {
        // A compact per-mesh lookup is sufficient for smoothly interpolated
        // diffuse lighting and avoids hundreds of 256x256 WPF bitmaps in levels.
        const int size = 64;
        byte[] tint = new byte[size * size * 3];
        bool[] covered = new bool[size * size];
        int[] tintSamples = new int[size * size];
        for (int triangle = 0; triangle < mesh.TriangleIndices.Length; triangle += 3)
        {
            int ia = checked((int)mesh.TriangleIndices[triangle]);
            int ib = checked((int)mesh.TriangleIndices[triangle + 1]);
            int ic = checked((int)mesh.TriangleIndices[triangle + 2]);
            RasterizeVertexColorTriangle(
                tint, covered, tintSamples, size, size,
                mesh.TextureCoordinates[ia],
                mesh.TextureCoordinates[ib],
                mesh.TextureCoordinates[ic],
                mesh.DiffuseColorsArgb[ia],
                mesh.DiffuseColorsArgb[ib],
                mesh.DiffuseColorsArgb[ic]);
        }

        byte[] pixels = new byte[size * size * 4];
        for (int pixel = 0; pixel < covered.Length; pixel++)
        {
            int destination = pixel * 4;
            int source = pixel * 3;
            if (covered[pixel])
            {
                pixels[destination] = tint[source];
                pixels[destination + 1] = tint[source + 1];
                pixels[destination + 2] = tint[source + 2];
            }
            else
            {
                pixels[destination] = 255;
                pixels[destination + 1] = 255;
                pixels[destination + 2] = 255;
            }
            pixels[destination + 3] = 255;
        }

        BitmapSource bitmap = BitmapSource.Create(
            size, size, 96, 96, PixelFormats.Bgra32, null, pixels, size * 4);
        bitmap.Freeze();
        ImageBrush brush = new(bitmap)
        {
            TileMode = TileMode.Tile,
            ViewportUnits = BrushMappingMode.Absolute,
            Viewport = new Rect(0, 0, 1, 1),
            Stretch = Stretch.Fill
        };
        brush.Freeze();
        DiffuseMaterial material = new(brush);
        material.Freeze();
        return material;
    }

    private static bool UsesRenderableVertexColors(SmoMesh mesh) =>
        (mesh.VertexFormat is 0x0100 or 0x0900 or 0x093E or 0x0940 or
            0x097E or 0x1940 or 0x197E) &&
        mesh.DiffuseColorsArgb
            .Select(color => color & 0x00FFFFFF)
            .Distinct()
            .Skip(1)
            .Any();

    private static void RasterizeVertexColorTriangle(
        byte[] tint,
        bool[] covered,
        int[] tintSamples,
        int width,
        int height,
        System.Numerics.Vector2 a,
        System.Numerics.Vector2 b,
        System.Numerics.Vector2 c,
        uint colorA,
        uint colorB,
        uint colorC)
    {
        float ax = a.X * (width - 1);
        float ay = a.Y * (height - 1);
        float bx = b.X * (width - 1);
        float by = b.Y * (height - 1);
        float cx = c.X * (width - 1);
        float cy = c.Y * (height - 1);
        float area = Edge(ax, ay, bx, by, cx, cy);
        if (MathF.Abs(area) < 0.0000001f)
            return;

        int minX = Math.Clamp((int)MathF.Floor(MathF.Min(ax, MathF.Min(bx, cx))), 0, width - 1);
        int maxX = Math.Clamp((int)MathF.Ceiling(MathF.Max(ax, MathF.Max(bx, cx))), 0, width - 1);
        int minY = Math.Clamp((int)MathF.Floor(MathF.Min(ay, MathF.Min(by, cy))), 0, height - 1);
        int maxY = Math.Clamp((int)MathF.Ceiling(MathF.Max(ay, MathF.Max(by, cy))), 0, height - 1);

        for (int y = minY; y <= maxY; y++)
        {
            for (int x = minX; x <= maxX; x++)
            {
                float wa = Edge(bx, by, cx, cy, x + 0.5f, y + 0.5f) / area;
                float wb = Edge(cx, cy, ax, ay, x + 0.5f, y + 0.5f) / area;
                float wc = 1f - wa - wb;
                if (MathF.Min(wa, MathF.Min(wb, wc)) < -0.00001f)
                    continue;

                byte red = InterpolateColor(colorA, colorB, colorC, 16, wa, wb, wc);
                byte green = InterpolateColor(colorA, colorB, colorC, 8, wa, wb, wc);
                byte blue = InterpolateColor(colorA, colorB, colorC, 0, wa, wb, wc);
                int pixel = y * width + x;
                int offset = pixel * 3;
                int samples = tintSamples[pixel];
                tint[offset] = (byte)((tint[offset] * samples + blue) / (samples + 1));
                tint[offset + 1] =
                    (byte)((tint[offset + 1] * samples + green) / (samples + 1));
                tint[offset + 2] = (byte)((tint[offset + 2] * samples + red) / (samples + 1));
                tintSamples[pixel] = samples + 1;
                covered[pixel] = true;
            }
        }
    }

    private static byte InterpolateColor(
        uint a, uint b, uint c, int shift, float wa, float wb, float wc) =>
        (byte)Math.Clamp(
            (int)MathF.Round(
                wa * ((a >> shift) & 0xFF) +
                wb * ((b >> shift) & 0xFF) +
                wc * ((c >> shift) & 0xFF)),
            0,
            255);

    private static float Edge(
        float ax, float ay, float bx, float by, float px, float py) =>
        (px - ax) * (by - ay) - (py - ay) * (bx - ax);

    private static MeshGeometry3D? CreateGeometry(
        DecodedRenderMesh renderMesh,
        BoundsBuilder bounds)
    {
        SmoMesh mesh = renderMesh.Mesh;
        if (mesh.Positions.Length == 0 || mesh.TriangleIndices.Length < 3)
            return null;

        Point3DCollection positions = new(mesh.Positions.Length);
        foreach (var source in mesh.Positions)
        {
            System.Numerics.Vector3 transformed = System.Numerics.Vector3.Transform(
                source, renderMesh.WorldTransform);
            // Sparkplug/D3D assets use a left-handed world while WPF 3D is
            // right-handed. Reflect Z once after the complete model-to-world
            // transform; triangle winding is reversed below to preserve fronts.
            Point3D position = new(transformed.X, transformed.Y, -transformed.Z);
            positions.Add(position);
            bounds.Include(position);
        }

        Int32Collection triangleIndices = new(mesh.TriangleIndices.Length);
        for (int triangle = 0; triangle < mesh.TriangleIndices.Length; triangle += 3)
        {
            uint a = mesh.TriangleIndices[triangle];
            uint b = mesh.TriangleIndices[triangle + 1];
            uint c = mesh.TriangleIndices[triangle + 2];
            if (a >= mesh.Positions.Length || b >= mesh.Positions.Length ||
                c >= mesh.Positions.Length)
                throw new InvalidDataException(
                    $"Decoded mesh [{mesh.ObjectIndex}] \"{mesh.Name}\" has an index " +
                    $"outside its {mesh.Positions.Length} positions.");

            triangleIndices.Add(checked((int)a));
            triangleIndices.Add(checked((int)c));
            triangleIndices.Add(checked((int)b));
        }

        MeshGeometry3D geometry = new()
        {
            Positions = positions,
            TriangleIndices = triangleIndices
        };

        if (mesh.HasNormals)
        {
            Matrix4x4 normalTransform = renderMesh.WorldTransform;
            if (Matrix4x4.Invert(renderMesh.WorldTransform, out Matrix4x4 inverseWorld))
                normalTransform = Matrix4x4.Transpose(inverseWorld);
            Vector3DCollection normals = new(mesh.Normals.Length);
            foreach (System.Numerics.Vector3 source in mesh.Normals)
            {
                System.Numerics.Vector3 transformed = System.Numerics.Vector3.TransformNormal(
                    source, normalTransform);
                transformed.Z = -transformed.Z;
                if (transformed.LengthSquared() > 0.000001f)
                    transformed = System.Numerics.Vector3.Normalize(transformed);
                normals.Add(new Vector3D(transformed.X, transformed.Y, transformed.Z));
            }
            geometry.Normals = normals;
        }

        if (mesh.HasTextureCoordinates)
        {
            PointCollection textureCoordinates = new(mesh.TextureCoordinates.Length);
            foreach (var source in mesh.TextureCoordinates)
                textureCoordinates.Add(new Point(source.X, source.Y));

            geometry.TextureCoordinates = textureCoordinates;
        }

        if (!mesh.HasSkinningData && renderMesh.RigidNodeObjectIndex is null)
            geometry.Freeze();
        return geometry;
    }

    private void ShowResourcesPanel_Click(object sender, RoutedEventArgs e) =>
        ShowSidebarPanel(ResourcesPanel);

    private void SkeletonPanelButton_Click(object sender, RoutedEventArgs e) =>
        ShowSidebarPanel(SkeletonPanel);

    private void SkeletonPanelClose_Click(object sender, RoutedEventArgs e) =>
        ShowSidebarPanel(ResourcesPanel);

    private void AnimationPanelButton_Click(object sender, RoutedEventArgs e) =>
        ShowSidebarPanel(AnimationPanel);

    private void NativeValidationPanelButton_Click(object sender, RoutedEventArgs e) =>
        ShowSidebarPanel(GameValidationPanel);

    private void AnimationPanelClose_Click(object sender, RoutedEventArgs e) =>
        ShowSidebarPanel(ResourcesPanel);

    private void ShowSidebarPanel(UIElement selected)
    {
        ResourcesPanel.Visibility = ReferenceEquals(selected, ResourcesPanel)
            ? Visibility.Visible : Visibility.Collapsed;
        SkeletonPanel.Visibility = ReferenceEquals(selected, SkeletonPanel)
            ? Visibility.Visible : Visibility.Collapsed;
        AnimationPanel.Visibility = ReferenceEquals(selected, AnimationPanel)
            ? Visibility.Visible : Visibility.Collapsed;
        GameValidationPanel.Visibility = ReferenceEquals(selected, GameValidationPanel)
            ? Visibility.Visible : Visibility.Collapsed;
    }

    private void GameValidationPanel_LogMessage(
        object? sender,
        NativeValidationPanelLogEventArgs e) =>
        AddLog(e.Message);

    private void AddAnimationFiles_Click(object sender, RoutedEventArgs e)
    {
        OpenFileDialog dialog = new()
        {
            Title = "Добавить анимации",
            Filter = "Sparkplug animations (*.san;*.anm)|*.san;*.anm|All files (*.*)|*.*",
            Multiselect = true,
            CheckFileExists = true
        };
        if (dialog.ShowDialog(this) != true) return;
        foreach (string file in dialog.FileNames)
        {
            if (Path.GetExtension(file).Equals(".anm", StringComparison.OrdinalIgnoreCase))
                AddAnimationsFromAnm(file);
            else
                AddAnimationFile(file, null, "Ручные SAN");
        }
        AnimationSourceText.Text = $"Добавлено вручную: {dialog.FileNames.Length}";
        RefreshAnimationList();
    }

    private void ChooseAnimationFolder_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new OpenFolderDialog { Title = "Папка с SAN/ANM-анимациями" };
        if (dialog.ShowDialog(this) != true) return;
        AddAnimationsFromDirectory(dialog.FolderName);
        AnimationSourceText.Text = dialog.FolderName;
        RefreshAnimationList();
    }

    private void AddAnimationsFromDirectory(string directory)
    {
        if (!Directory.Exists(directory)) return;
        foreach (string anm in Directory.EnumerateFiles(directory, "*.anm", SearchOption.AllDirectories))
            AddAnimationsFromAnm(anm);
        foreach (string san in Directory.EnumerateFiles(directory, "*.san", SearchOption.AllDirectories))
            if (!_allAnimationItems.Any(item => item.Path.Equals(
                    Path.GetFullPath(san), StringComparison.OrdinalIgnoreCase)))
                AddAnimationFile(san, null,
                    $"{new DirectoryInfo(Path.GetDirectoryName(san)!).Name} · без ANM");
        AnimationSourceText.Text = $"Автопоиск: {directory}";
    }

    private static bool TryFindDefaultBloomAnimationDirectory(
        IEnumerable<string> modelPaths, out string directory)
    {
        directory = string.Empty;
        IEnumerable<string> starts = modelPaths
            .Select(path => Path.GetDirectoryName(path) ?? string.Empty)
            .Where(path => path.Length > 0)
            .Concat([Environment.CurrentDirectory, AppContext.BaseDirectory]);
        foreach (string start in starts)
        {
            DirectoryInfo? cursor;
            try { cursor = new DirectoryInfo(start); }
            catch { continue; }
            while (cursor is not null)
            {
                string candidate = cursor.Name.Equals("Media", StringComparison.OrdinalIgnoreCase)
                    ? Path.Combine(cursor.FullName, "Characters", "Bloom")
                    : Path.Combine(cursor.FullName, "Media", "Characters", "Bloom");
                if (Directory.Exists(candidate) &&
                    (Directory.EnumerateFiles(candidate, "*.san", SearchOption.AllDirectories).Any() ||
                     Directory.EnumerateFiles(candidate, "*.anm", SearchOption.AllDirectories).Any()))
                {
                    directory = candidate;
                    return true;
                }
                cursor = cursor.Parent;
            }
        }
        return false;
    }

    private void AddAnimationsFromAnm(string anmPath)
    {
        string directory = Path.GetDirectoryName(anmPath) ?? string.Empty;
        string group = Path.GetFileNameWithoutExtension(anmPath);
        foreach (string line in File.ReadLines(anmPath))
        {
            string clean = line.Trim();
            if (clean.Length == 0 || clean.StartsWith('#') ||
                clean.StartsWith("end", StringComparison.OrdinalIgnoreCase)) continue;
            string[] fields = clean.TrimEnd(';').Split(',').Select(value => value.Trim()).ToArray();
            if (fields.Length < 8 || !fields[^1].EndsWith(".san", StringComparison.OrdinalIgnoreCase)) continue;
            string san = Path.Combine(directory, fields[^1]);
            string state = string.Join(" / ", fields.Take(6).Where(value =>
                value.Length > 0 && !value.Equals("none", StringComparison.OrdinalIgnoreCase)));
            AddAnimationFile(san, $"{group}: {state} [{fields[6]}]", group);
        }
    }

    private void AddAnimationFile(string path, string? state, string group)
    {
        if (!File.Exists(path)) return;
        string fullPath = Path.GetFullPath(path);
        int existingIndex = _allAnimationItems.FindIndex(item =>
            item.Path.Equals(fullPath, StringComparison.OrdinalIgnoreCase));
        if (existingIndex >= 0)
        {
            AddAnimationGroup(fullPath, group);
            if (!string.IsNullOrWhiteSpace(state) &&
                !_allAnimationItems[existingIndex].Display.Contains(state, StringComparison.OrdinalIgnoreCase))
                _allAnimationItems[existingIndex] = _allAnimationItems[existingIndex] with
                    { Display = _allAnimationItems[existingIndex].Display + $" · {state}" };
            return;
        }
        string display = Path.GetFileNameWithoutExtension(fullPath);
        if (!string.IsNullOrWhiteSpace(state)) display += $"  ·  {state}";
        _allAnimationItems.Add(new AnimationListItem(fullPath, display));
        AddAnimationGroup(fullPath, group);
    }

    private void AddAnimationGroup(string path, string group)
    {
        if (!_animationGroupsByPath.TryGetValue(path, out HashSet<string>? groups))
        {
            groups = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            _animationGroupsByPath.Add(path, groups);
        }
        groups.Add(group);
        _enabledAnimationGroups.Add(group);
    }

    private void AnimationFilter_Changed(object sender, TextChangedEventArgs e) => RefreshAnimationList();

    private void RefreshAnimationList()
    {
        if (AnimationList is null || AnimationFilterBox is null) return;
        RebuildAnimationGroupControls();
        string filter = AnimationFilterBox.Text.Trim();
        AnimationList.ItemsSource = _allAnimationItems.Where(item =>
            _animationGroupsByPath.TryGetValue(item.Path, out HashSet<string>? groups) &&
            groups.Any(_enabledAnimationGroups.Contains) &&
            (filter.Length == 0 || item.Display.Contains(filter, StringComparison.OrdinalIgnoreCase)))
            .OrderBy(item => item.Display, StringComparer.OrdinalIgnoreCase).ToArray();
    }

    private void RebuildAnimationGroupControls()
    {
        if (AnimationGroupsPanel is null) return;
        string[] groups = _animationGroupsByPath.Values.SelectMany(value => value)
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .OrderBy(value => value, StringComparer.OrdinalIgnoreCase).ToArray();
        if (AnimationGroupsPanel.Children.Count == groups.Length &&
            AnimationGroupsPanel.Children.OfType<CheckBox>().Select(box => box.Tag as string)
                .SequenceEqual(groups, StringComparer.OrdinalIgnoreCase)) return;

        _updatingAnimationGroups = true;
        AnimationGroupsPanel.Children.Clear();
        foreach (string group in groups)
        {
            var checkBox = new CheckBox
            {
                Content = group,
                Tag = group,
                Foreground = new SolidColorBrush(Color.FromRgb(216, 220, 228)),
                IsChecked = _enabledAnimationGroups.Contains(group),
                Margin = new Thickness(0, 2, 0, 2)
            };
            checkBox.Checked += AnimationGroupCheck_Changed;
            checkBox.Unchecked += AnimationGroupCheck_Changed;
            AnimationGroupsPanel.Children.Add(checkBox);
        }
        _updatingAnimationGroups = false;
    }

    private void AnimationGroupCheck_Changed(object sender, RoutedEventArgs e)
    {
        if (_updatingAnimationGroups || sender is not CheckBox { Tag: string group } checkBox) return;
        if (checkBox.IsChecked == true) _enabledAnimationGroups.Add(group);
        else _enabledAnimationGroups.Remove(group);
        RefreshAnimationList();
    }

    private void EnableAllAnimationGroups_Click(object sender, RoutedEventArgs e)
    {
        foreach (string group in _animationGroupsByPath.Values.SelectMany(value => value))
            _enabledAnimationGroups.Add(group);
        SetAnimationGroupChecks(true);
    }

    private void DisableAllAnimationGroups_Click(object sender, RoutedEventArgs e)
    {
        _enabledAnimationGroups.Clear();
        SetAnimationGroupChecks(false);
    }

    private void SetAnimationGroupChecks(bool enabled)
    {
        _updatingAnimationGroups = true;
        foreach (CheckBox checkBox in AnimationGroupsPanel.Children.OfType<CheckBox>())
            checkBox.IsChecked = enabled;
        _updatingAnimationGroups = false;
        RefreshAnimationList();
    }

    private void AnimationList_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (AnimationList.SelectedItem is not AnimationListItem item) return;
        if (!SmoAnimationDecoder.TryDecode(item.Path, out SmoAnimationClip? clip, out string error) || clip is null)
        {
            AddLog($"SAN {Path.GetFileName(item.Path)}: {error}");
            return;
        }
        _selectedAnimation = clip;
        _animationFileIndex = _treeFiles.FirstOrDefault(pair => pair.Value.Skeleton.Count > 0).Key;
        _animationTime = 0;
        _animationPlaying = false;
        AnimationPlayPauseButton.Content = "▶";
        AnimationSlider.Minimum = 0;
        AnimationSlider.Maximum = Math.Max(clip.Duration, 0.001f);
        AnimationTimeline.Visibility = Visibility.Visible;
        ApplyAnimationPose();
        int matched = _treeFiles.TryGetValue(_animationFileIndex, out DecodedSmoFile? animationFile)
            ? animationFile.Skeleton.Count(bone => clip.Tracks.Any(track =>
                track.NodeName.Equals(bone.Name, StringComparison.OrdinalIgnoreCase))) : 0;
        AddLog($"Анимация {Path.GetFileName(item.Path)}: {clip.Tracks.Count} tracks, " +
               $"совпало со skeleton {matched}, {clip.FrameCount} keys, {clip.Duration:G4} s.");
    }

    private void AnimationPlayPause_Click(object sender, RoutedEventArgs e)
    {
        if (_selectedAnimation is null) return;
        _animationPlaying = !_animationPlaying;
        AnimationPlayPauseButton.Content = _animationPlaying ? "⏸" : "▶";
    }

    private void PreviousAnimationFrame_Click(object sender, RoutedEventArgs e) => StepAnimation(-1);
    private void NextAnimationFrame_Click(object sender, RoutedEventArgs e) => StepAnimation(1);

    private void StepAnimation(int direction)
    {
        if (_selectedAnimation is null) return;
        _animationPlaying = false;
        AnimationPlayPauseButton.Content = "▶";
        double step = _selectedAnimation.FrameCount > 1
            ? _selectedAnimation.Duration / (_selectedAnimation.FrameCount - 1) : 1.0 / 30.0;
        _animationTime = Math.Clamp(_animationTime + direction * step, 0, _selectedAnimation.Duration);
        ApplyAnimationPose();
    }

    private void AnimationSlider_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
    {
        if (_updatingAnimationSlider || _selectedAnimation is null) return;
        _animationTime = e.NewValue;
        _animationPlaying = false;
        if (AnimationPlayPauseButton is not null) AnimationPlayPauseButton.Content = "▶";
        ApplyAnimationPose();
    }

    private void ApplyAnimationPose()
    {
        try
        {
            ApplyAnimationPoseCore();
        }
        catch (Exception exception)
        {
            _animationPlaying = false;
            if (AnimationPlayPauseButton is not null)
                AnimationPlayPauseButton.Content = "▶";
            AddLog($"ОШИБКА применения анимации: {exception.GetType().Name}: {exception.Message}");
        }
    }

    private void ApplyAnimationPoseCore()
    {
        if (_selectedAnimation is null ||
            !_treeFiles.TryGetValue(_animationFileIndex, out DecodedSmoFile? file)) return;

        Dictionary<string, SmoAnimationTrack> tracks = _selectedAnimation.Tracks
            .GroupBy(track => track.NodeName, StringComparer.OrdinalIgnoreCase)
            .ToDictionary(group => group.Key, group => group.First(), StringComparer.OrdinalIgnoreCase);
        Dictionary<int, AnimationNode> nodes = file.AnimationNodes.ToDictionary(node => node.ObjectIndex);
        Dictionary<int, Matrix4x4> worlds = new();
        HashSet<int> resolving = [];
        Matrix4x4 ResolveWorld(AnimationNode node)
        {
            if (worlds.TryGetValue(node.ObjectIndex, out Matrix4x4 cached)) return cached;
            if (!resolving.Add(node.ObjectIndex)) return node.BindWorldMatrix;
            Matrix4x4 parentWorld = Matrix4x4.Identity;
            if (node.ParentObjectIndex is int parentIndex && nodes.TryGetValue(parentIndex, out AnimationNode? parent))
                parentWorld = ResolveWorld(parent);
            Matrix4x4 bindLocal = node.BindWorldMatrix;
            if (node.ParentObjectIndex is int bindParentIndex && nodes.TryGetValue(bindParentIndex, out AnimationNode? bindParent) &&
                Matrix4x4.Invert(bindParent.BindWorldMatrix, out Matrix4x4 inverseParent))
                bindLocal = node.BindWorldMatrix * inverseParent;
            Matrix4x4.Decompose(bindLocal, out var scale, out var rotation, out var translation);
            if (tracks.TryGetValue(node.Name, out SmoAnimationTrack? track))
            {
                if (track.Positions.Count > 0) translation = SampleVector(track.Positions, (float)_animationTime);
                if (track.Rotations.Count > 0) rotation = SampleQuaternion(track.Rotations, (float)_animationTime);
                if (track.Scales.Count > 0) scale = SampleVector(track.Scales, (float)_animationTime);
            }
            Matrix4x4 world = Matrix4x4.CreateScale(scale) *
                              Matrix4x4.CreateFromQuaternion(rotation) *
                              Matrix4x4.CreateTranslation(translation) * parentWorld;
            resolving.Remove(node.ObjectIndex);
            worlds[node.ObjectIndex] = world;
            return world;
        }
        foreach (AnimationNode node in file.AnimationNodes) ResolveWorld(node);

        _animatedBonePositions.Clear();
        foreach ((int index, Matrix4x4 world) in worlds)
            _animatedBonePositions[index] = new System.Numerics.Vector3(world.M41, world.M42, world.M43);

        foreach ((SceneObjectKey key, SceneGeometry scene) in _sceneGeometry)
        {
            if (key.FileIndex != _animationFileIndex) continue;
            SmoMesh mesh = scene.RenderMesh.Mesh;
            if (!mesh.HasSkinningData)
            {
                if (scene.RenderMesh.RigidNodeObjectIndex is int rigidNodeIndex &&
                    worlds.TryGetValue(rigidNodeIndex, out Matrix4x4 animatedNode) &&
                    nodes.TryGetValue(rigidNodeIndex, out AnimationNode? rigidNode) &&
                    Matrix4x4.Invert(rigidNode.BindWorldMatrix, out Matrix4x4 inverseBind))
                {
                    Matrix4x4 transform = scene.RenderMesh.WorldTransform * inverseBind * animatedNode;
                    Point3DCollection rigidPositions = new(mesh.Positions.Length);
                    foreach (var source in mesh.Positions)
                    {
                        var value = System.Numerics.Vector3.Transform(source, transform);
                        rigidPositions.Add(new Point3D(value.X, value.Y, -value.Z));
                    }
                    scene.Geometry.Positions = rigidPositions;
                }
                continue;
            }
            if (scene.RenderMesh.SkinObjectIndex is not int skinIndex ||
                !file.Skins.TryGetValue(skinIndex, out SmoSkin? skin)) continue;
            if (mesh.BlendWeights.Length != mesh.Positions.Length ||
                mesh.BlendIndices.Length != mesh.Positions.Length)
                continue;
            Point3DCollection positions = new(mesh.Positions.Length);
            for (int vertex = 0; vertex < mesh.Positions.Length; vertex++)
            {
                System.Numerics.Vector3 result = default;
                float total = 0;
                System.Numerics.Vector4 weights = mesh.BlendWeights[vertex];
                SmoBlendIndices indices = mesh.BlendIndices[vertex];
                Add(indices.X, weights.X); Add(indices.Y, weights.Y);
                Add(indices.Z, weights.Z); Add(indices.W, weights.W);
                if (total <= 0.000001f) result = mesh.Positions[vertex];
                else if (Math.Abs(total - 1) > 0.0001f) result /= total;
                result = System.Numerics.Vector3.Transform(result, scene.RenderMesh.WorldTransform);
                positions.Add(new Point3D(result.X, result.Y, -result.Z));

                void Add(int paletteIndex, float weight)
                {
                    if (weight <= 0.000001f || (uint)paletteIndex >= (uint)skin.Bones.Count) return;
                    SmoSkinBone paletteBone = skin.Bones[paletteIndex];
                    Matrix4x4 boneWorld = worlds.GetValueOrDefault(
                        paletteBone.NodeObjectIndex, Matrix4x4.Identity);
                    result += System.Numerics.Vector3.Transform(mesh.Positions[vertex],
                        paletteBone.InverseBindMatrix * boneWorld) * weight;
                    total += weight;
                }
            }
            scene.Geometry.Positions = positions;
            if (mesh.HasNormals)
            {
                Vector3DCollection normals = new(mesh.Normals.Length);
                Matrix4x4 normalWorld = scene.RenderMesh.WorldTransform;
                if (Matrix4x4.Invert(normalWorld, out Matrix4x4 inverseWorld))
                    normalWorld = Matrix4x4.Transpose(inverseWorld);
                for (int vertex = 0; vertex < mesh.Normals.Length; vertex++)
                {
                    System.Numerics.Vector3 result = default;
                    float total = 0;
                    System.Numerics.Vector4 weights = mesh.BlendWeights[vertex];
                    SmoBlendIndices indices = mesh.BlendIndices[vertex];
                    Add(indices.X, weights.X); Add(indices.Y, weights.Y);
                    Add(indices.Z, weights.Z); Add(indices.W, weights.W);
                    if (total <= 0.000001f) result = mesh.Normals[vertex];
                    else if (Math.Abs(total - 1) > 0.0001f) result /= total;
                    result = System.Numerics.Vector3.TransformNormal(result, normalWorld);
                    result.Z = -result.Z;
                    if (result.LengthSquared() > 0.000001f)
                        result = System.Numerics.Vector3.Normalize(result);
                    normals.Add(new Vector3D(result.X, result.Y, result.Z));

                    void Add(int paletteIndex, float weight)
                    {
                        if (weight <= 0.000001f || (uint)paletteIndex >= (uint)skin.Bones.Count) return;
                        SmoSkinBone paletteBone = skin.Bones[paletteIndex];
                        Matrix4x4 boneWorld = worlds.GetValueOrDefault(
                            paletteBone.NodeObjectIndex, Matrix4x4.Identity);
                        result += System.Numerics.Vector3.TransformNormal(mesh.Normals[vertex],
                            paletteBone.InverseBindMatrix * boneWorld) * weight;
                        total += weight;
                    }
                }
                scene.Geometry.Normals = normals;
            }
        }
        UpdateSkeletonVisibility();
        _updatingAnimationSlider = true;
        AnimationSlider.Value = _animationTime;
        _updatingAnimationSlider = false;
        AnimationTimeText.Text = $"{_animationTime:0.000} / {_selectedAnimation.Duration:0.000} s";
    }

    private static System.Numerics.Vector3 SampleVector(
        IReadOnlyList<SmoAnimationKey<System.Numerics.Vector3>> keys, float time)
    {
        if (keys.Count == 1 || time <= keys[0].Time) return keys[0].Value;
        for (int i = 1; i < keys.Count; i++) if (time <= keys[i].Time)
        {
            float amount = (time - keys[i - 1].Time) / Math.Max(keys[i].Time - keys[i - 1].Time, 0.000001f);
            return System.Numerics.Vector3.Lerp(keys[i - 1].Value, keys[i].Value, amount);
        }
        return keys[^1].Value;
    }

    private static System.Numerics.Quaternion SampleQuaternion(
        IReadOnlyList<SmoAnimationKey<System.Numerics.Quaternion>> keys, float time)
    {
        if (keys.Count == 1 || time <= keys[0].Time) return keys[0].Value;
        for (int i = 1; i < keys.Count; i++) if (time <= keys[i].Time)
        {
            float amount = (time - keys[i - 1].Time) / Math.Max(keys[i].Time - keys[i - 1].Time, 0.000001f);
            return System.Numerics.Quaternion.Slerp(keys[i - 1].Value, keys[i].Value, amount);
        }
        return keys[^1].Value;
    }

    private void SkeletonOption_Changed(object sender, RoutedEventArgs e)
    {
        if (!IsInitialized)
            return;
        UpdateSkeletonVisibility();
        UpdateMeshAppearance();
    }

    private void FloorGridOption_Changed(object sender, RoutedEventArgs e)
    {
        if (IsInitialized)
            UpdateFloorGrid();
    }

    private void BackgroundColor_Click(object sender, RoutedEventArgs e)
    {
        if (ChooseColor(_sceneBackgroundColor) is not Color selected)
            return;

        _sceneBackgroundColor = selected;
        ApplySceneColors();
    }

    private void FloorGridColor_Click(object sender, RoutedEventArgs e)
    {
        if (ChooseColor(_floorGridColor) is not Color selected)
            return;

        _floorGridColor = selected;
        ApplySceneColors();
        UpdateFloorGrid();
    }

    private static Color? ChooseColor(Color current)
    {
        using var dialog = new System.Windows.Forms.ColorDialog
        {
            AllowFullOpen = true,
            AnyColor = true,
            FullOpen = true,
            SolidColorOnly = true,
            Color = System.Drawing.Color.FromArgb(
                current.R, current.G, current.B)
        };
        if (dialog.ShowDialog() != System.Windows.Forms.DialogResult.OK)
            return null;

        System.Drawing.Color selected = dialog.Color;
        return Color.FromRgb(selected.R, selected.G, selected.B);
    }

    private void ApplySceneColors()
    {
        SolidColorBrush background = new(_sceneBackgroundColor);
        background.Freeze();
        SceneSurface.Background = background;

        SolidColorBrush grid = new(_floorGridColor);
        grid.Freeze();
        BackgroundColorSwatch.Background = background;
        FloorGridColorSwatch.Background = grid;
        BackgroundColorText.Text = $"Фон · #{_sceneBackgroundColor.R:X2}" +
                                   $"{_sceneBackgroundColor.G:X2}" +
                                   $"{_sceneBackgroundColor.B:X2}";
        FloorGridColorText.Text = $"Сетка · #{_floorGridColor.R:X2}" +
                                  $"{_floorGridColor.G:X2}" +
                                  $"{_floorGridColor.B:X2}";
    }

    private void UpdateFloorGrid()
    {
        if (FloorGridRoot is null)
            return;

        FloorGridRoot.Children.Clear();
        if (ShowFloorGridCheck?.IsChecked != true)
            return;

        double centerX = 0;
        double centerZ = 0;
        double floorY = 0;
        double span = 20;
        double diagonal = 0;
        if (_sceneBounds.HasValue)
        {
            centerX = _sceneBounds.Center.X;
            centerZ = _sceneBounds.Center.Z;
            floorY = _sceneBounds.MinY;
            diagonal = _sceneBounds.DiagonalLength;
            span = Math.Max(
                Math.Max(_sceneBounds.Width, _sceneBounds.Depth),
                diagonal * 0.25);
        }

        double step = NiceGridStep(Math.Max(span / 20.0, 0.0001));
        const int halfLineCount = 12;
        double extent = step * halfLineCount;
        centerX = Math.Round(centerX / step) * step;
        centerZ = Math.Round(centerZ / step) * step;
        floorY -= Math.Max(step * 0.002, diagonal * 0.0005);
        double thickness = Math.Max(step * 0.012, 0.0001);
        Material minorMaterial = CreateGridMaterial(_floorGridColor, 150);
        Material majorMaterial = CreateGridMaterial(_floorGridColor, 235);

        for (int offset = -halfLineCount; offset <= halfLineCount; offset++)
        {
            double coordinate = offset * step;
            bool major = offset % 5 == 0;
            Material material = major ? majorMaterial : minorMaterial;
            double lineThickness = major ? thickness * 1.65 : thickness;
            FloorGridRoot.Children.Add(CreateFloorGridLine(
                new Point3D(centerX + coordinate, floorY, centerZ - extent),
                new Point3D(centerX + coordinate, floorY, centerZ + extent),
                lineThickness,
                material));
            FloorGridRoot.Children.Add(CreateFloorGridLine(
                new Point3D(centerX - extent, floorY, centerZ + coordinate),
                new Point3D(centerX + extent, floorY, centerZ + coordinate),
                lineThickness,
                material));
        }
    }

    private static double NiceGridStep(double value)
    {
        double power = Math.Pow(10, Math.Floor(Math.Log10(value)));
        double normalized = value / power;
        double factor = normalized <= 1 ? 1
            : normalized <= 2 ? 2
            : normalized <= 5 ? 5
            : 10;
        return factor * power;
    }

    private static Material CreateGridMaterial(Color color, byte alpha)
    {
        Color visible = Color.FromArgb(alpha, color.R, color.G, color.B);
        EmissiveMaterial material = new(new SolidColorBrush(visible));
        material.Freeze();
        return material;
    }

    private static GeometryModel3D CreateFloorGridLine(
        Point3D start,
        Point3D end,
        double thickness,
        Material material)
    {
        Vector3D direction = end - start;
        Vector3D side = new(-direction.Z, 0, direction.X);
        side.Normalize();
        side *= thickness * 0.5;
        Point3DCollection positions =
        [
            start - side,
            start + side,
            end + side,
            end - side
        ];
        MeshGeometry3D geometry = new()
        {
            Positions = positions,
            TriangleIndices = [0, 1, 2, 0, 2, 3]
        };
        geometry.Freeze();
        return new GeometryModel3D(geometry, material)
        {
            BackMaterial = material
        };
    }

    private void BoneFilter_Changed(object sender, TextChangedEventArgs e)
    {
        if (!IsInitialized)
            return;
        RefreshBoneList();
    }

    private void RefreshBoneList()
    {
        if (BoneList is null || BoneFilterBox is null)
            return;
        string filter = BoneFilterBox.Text.Trim();
        BoneListItem[] visible = _allBoneItems
            .Where(item => filter.Length == 0 ||
                item.Display.Contains(filter, StringComparison.OrdinalIgnoreCase))
            .OrderBy(item => item.FileIndex)
            .ThenBy(item => item.Name, StringComparer.OrdinalIgnoreCase)
            .ToArray();
        BoneListItem? selected = _selectedBone;
        BoneList.ItemsSource = visible;
        if (selected is not null)
        {
            BoneListItem? visibleSelection = visible.FirstOrDefault(item =>
                item.FileIndex == selected.FileIndex &&
                item.ObjectIndex == selected.ObjectIndex);
            BoneList.SelectedItem = visibleSelection;
            _selectedBone = visibleSelection ?? selected;
            UpdateSkeletonVisibility();
            UpdateMeshAppearance();
        }
    }

    private void BoneList_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        _selectedBone = BoneList.SelectedItem as BoneListItem;
        UpdateSkeletonVisibility();
        if (_selectedBone is not null)
        {
            AddLog($"Кость: {_selectedBone.Name}; подсвечены mesh с ненулевым весом.");
            TreeViewItem? item = RevealTreeObject(new SceneObjectKey(
                _selectedBone.FileIndex, _selectedBone.ObjectIndex));
            if (item is not null)
            {
                item.IsSelected = true;
                item.BringIntoView();
            }
        }
        UpdateMeshAppearance();
    }

    private void AuxiliaryObjectList_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (AuxiliaryObjectList.SelectedItem is not AuxiliaryObjectItem selected)
            return;
        TreeViewItem? item = RevealTreeObject(new SceneObjectKey(
            selected.FileIndex, selected.ObjectIndex));
        if (item is not null)
        {
            item.IsSelected = true;
            item.BringIntoView();
        }
    }

    private void UpdateSkeletonVisibility()
    {
        if (SkeletonRoot is null || AttachmentRoot is null || CollisionRoot is null ||
            ControlRigRoot is null || MarkerRoot is null)
            return;
        SkeletonRoot.Children.Clear();
        AttachmentRoot.Children.Clear();
        CollisionRoot.Children.Clear();
        ControlRigRoot.Children.Clear();
        MarkerRoot.Children.Clear();
        if (ShowSkeletonCheck?.IsChecked == true)
        {
            if (_animatedBonePositions.Count > 0 &&
                _treeFiles.TryGetValue(_animationFileIndex, out DecodedSmoFile? animatedFile))
            {
                double radius = Math.Max(_sceneBounds.DiagonalLength * 0.006, 0.008);
                Material line = CreateSolidMaterial(Color.FromRgb(70, 220, 255));
                Material point = CreateSolidMaterial(Color.FromRgb(255, 218, 75));
                foreach (SkeletonBone bone in animatedFile.Skeleton.Where(item => !item.IsAttachment))
                {
                    if (!_animatedBonePositions.TryGetValue(bone.ObjectIndex, out var position)) continue;
                    Point3D child = ToViewportPoint(position);
                    SkeletonRoot.Children.Add(CreateOctahedron(child, radius, point));
                    if (bone.ParentObjectIndex is int parentIndex &&
                        _animatedBonePositions.TryGetValue(parentIndex, out var parentPosition))
                        SkeletonRoot.Children.Add(CreateBoneSegment(
                            ToViewportPoint(parentPosition), child, radius * 0.35, line));
                }
            }
            else foreach (Model3D model in _skeletonModels)
                SkeletonRoot.Children.Add(model);
        }
        if (ShowAttachmentsCheck?.IsChecked == true)
        {
            if (_animatedBonePositions.Count > 0 &&
                _treeFiles.TryGetValue(_animationFileIndex, out DecodedSmoFile? animatedFile))
            {
                double radius = Math.Max(_sceneBounds.DiagonalLength * 0.01, 0.012);
                Material material = CreateSolidMaterial(Color.FromRgb(255, 70, 190));
                foreach (SkeletonBone bone in animatedFile.Skeleton.Where(item => item.IsAttachment))
                    if (_animatedBonePositions.TryGetValue(bone.ObjectIndex, out var position))
                        AttachmentRoot.Children.Add(CreateOctahedron(
                            ToViewportPoint(position), radius, material));
            }
            else foreach (Model3D model in _attachmentModels)
                AttachmentRoot.Children.Add(model);
        }
        if (ShowCollisionCheck?.IsChecked == true)
        {
            foreach (Model3D model in _collisionModels)
                CollisionRoot.Children.Add(model);
            if (_selectedAuxiliary is SceneObjectKey selected &&
                _treeFiles.TryGetValue(selected.FileIndex, out DecodedSmoFile? collisionFile))
            {
                CollisionVolume? collision = collisionFile.CollisionVolumes.FirstOrDefault(
                    item => item.ObjectIndex == selected.ObjectIndex);
                if (collision is not null)
                {
                    double radius = Math.Max(_sceneBounds.DiagonalLength * 0.007, 0.012);
                    CollisionRoot.Children.Add(CreateCollisionBox(
                        collision, radius,
                        CreateSolidMaterial(Color.FromRgb(255, 45, 45))));
                }
            }
        }
        if (ShowControlRigCheck?.IsChecked == true)
        {
            foreach (Model3D model in _controlRigModels)
                ControlRigRoot.Children.Add(model);
            AddSelectedHelperMarker(ControlRigRoot, true, 1.8);
        }
        if (ShowMarkersCheck?.IsChecked == true)
        {
            foreach (Model3D model in _markerModels)
                MarkerRoot.Children.Add(model);
            AddSelectedHelperMarker(MarkerRoot, false, 2.8);
        }

        if (_selectedBone is not null &&
            _treeFiles.TryGetValue(_selectedBone.FileIndex, out DecodedSmoFile? file))
        {
            SkeletonBone? bone = file.Skeleton.FirstOrDefault(item =>
                item.ObjectIndex == _selectedBone.ObjectIndex);
            if (bone is not null)
            {
                double radius = Math.Max(_sceneBounds.DiagonalLength * 0.01, 0.012);
                System.Numerics.Vector3 markerPosition = _animatedBonePositions.GetValueOrDefault(
                    bone.ObjectIndex, bone.Position);
                Model3D marker = CreateOctahedron(
                    ToViewportPoint(markerPosition), radius,
                    CreateSolidMaterial(Color.FromRgb(255, 45, 45)));
                if (bone.IsAttachment && ShowAttachmentsCheck?.IsChecked == true)
                    AttachmentRoot.Children.Add(marker);
                else if (!bone.IsAttachment && ShowSkeletonCheck?.IsChecked == true)
                    SkeletonRoot.Children.Add(marker);
            }
        }
    }

    private void AddSelectedHelperMarker(
        Model3DGroup target, bool controlRig, double radiusScale)
    {
        if (_selectedAuxiliary is not SceneObjectKey selected ||
            !_treeFiles.TryGetValue(selected.FileIndex, out DecodedSmoFile? file))
            return;
        IReadOnlyList<HelperNode> helpers = controlRig ? file.ControlRig : file.Markers;
        HelperNode? helper = helpers.FirstOrDefault(item => item.ObjectIndex == selected.ObjectIndex);
        if (helper is null)
            return;
        double radius = Math.Max(_sceneBounds.DiagonalLength * 0.008, 0.012) * radiusScale;
        target.Children.Add(CreateOctahedron(
            ToViewportPoint(helper.Position), radius,
            CreateSolidMaterial(Color.FromRgb(255, 45, 45))));
    }

    private void UpdateMeshAppearance()
    {
        if (TransparentModelCheck is null || HighlightInfluencedMeshesCheck is null)
            return;
        bool transparent = TransparentModelCheck.IsChecked == true;
        bool highlightBone = HighlightInfluencedMeshesCheck.IsChecked == true &&
                             _selectedBone is not null;
        Material influencedMaterial = CreateSolidMaterial(Color.FromRgb(255, 92, 35));

        foreach ((SceneObjectKey key, SceneGeometry geometry) in _sceneGeometry)
        {
            bool influenced = highlightBone &&
                key.FileIndex == _selectedBone!.FileIndex &&
                geometry.RenderMesh.BoneInfluences.ContainsKey(_selectedBone.ObjectIndex);
            double opacity = transparent || highlightBone && !influenced ? 0.22 : 1.0;
            Material material = influenced
                ? influencedMaterial
                : opacity < 1
                    ? CloneMaterialWithOpacity(geometry.OriginalMaterial, opacity)
                    : geometry.OriginalMaterial;
            geometry.Model.Material = material;
            geometry.Model.BackMaterial = material;
        }
    }

    private static Material CloneMaterialWithOpacity(Material source, double opacity)
    {
        Material result = source switch
        {
            DiffuseMaterial diffuse => new DiffuseMaterial(CloneBrush(diffuse.Brush, opacity)),
            EmissiveMaterial emissive => new EmissiveMaterial(CloneBrush(emissive.Brush, opacity)),
            SpecularMaterial specular => new SpecularMaterial(
                CloneBrush(specular.Brush, opacity), specular.SpecularPower),
            MaterialGroup group => new MaterialGroup
            {
                Children = new MaterialCollection(group.Children.Select(child =>
                    CloneMaterialWithOpacity(child, opacity)))
            },
            _ => source.CloneCurrentValue()
        };
        result.Freeze();
        return result;
    }

    private static Brush CloneBrush(Brush source, double opacity)
    {
        Brush brush = source.CloneCurrentValue();
        brush.Opacity *= opacity;
        brush.Freeze();
        return brush;
    }

    private void UpdateSceneStats()
    {
        SceneStatsText.Text =
            $"Файлов: {_loadedFileCount} · меши: {_decodedMeshCount}/{_totalMeshCount} · " +
            $"с текстурой: {_texturedMeshCount} · texture issues: {_textureIssueCount} · " +
            $"диагностики: {_diagnosticCount} (ошибок: {_diagnosticErrorCount}) · " +
            $"unsupported: {_unsupportedMeshCount} · сбоев: {_failedFileCount}";
    }

    private void UpdateIssueToolTip()
    {
        if (_allIssues.Count == 0)
        {
            StatusText.ToolTip = null;
            return;
        }

        TextBlock details = new()
        {
            Text = string.Join(Environment.NewLine, _allIssues.Select(issue => issue.Text)),
            TextWrapping = TextWrapping.Wrap,
            MaxWidth = 780
        };

        StatusText.ToolTip = new ScrollViewer
        {
            Content = details,
            MaxHeight = 420,
            VerticalScrollBarVisibility = ScrollBarVisibility.Auto
        };
    }

    private void AddLog(string message)
    {
        StateLog.Items.Add($"[{DateTime.Now:HH:mm:ss}] {message}");
        if (StateLog.Items.Count > 2000)
            StateLog.Items.RemoveAt(0);
        StateLog.ScrollIntoView(StateLog.Items[StateLog.Items.Count - 1]);
    }

    private void AddFileTree(DecodedSmoFile file, int fileIndex)
    {
        _treeFiles[fileIndex] = file;
        TreeViewItem root = CreateTreeItem(new SceneTreeNode(fileIndex, null));
        int palettes = file.Objects.Count(item => item.TypeHash == SmoClassIds.Skin);
        int bones = file.Skeleton.Count(item => !item.IsAttachment);
        root.Header = $"{Path.GetFileName(file.Path)}  ·  {file.TotalMeshCount} mesh · {palettes} palette · {bones} bones";
        root.FontWeight = FontWeights.SemiBold;
        AddExpansionPlaceholder(root, file.Objects.Any(item => item.ParentIndex is null));
        SceneTree.Items.Add(root);
    }

    private TreeViewItem CreateTreeItem(SceneTreeNode node)
    {
        string header;
        Brush foreground;
        if (node.ObjectIndex is not int objectIndex)
        {
            header = Path.GetFileName(_treeFiles[node.FileIndex].Path);
            foreground = Brushes.LightSteelBlue;
        }
        else
        {
            SceneObjectInfo info = _treeFiles[node.FileIndex].Objects[objectIndex];
            string name = string.IsNullOrWhiteSpace(info.Name) ? "<без имени>" : info.Name;
            header = $"{GetLogicalObjectKind(node.FileIndex, info)}  [{info.Index}] {name}";
            foreground = GetClassBrush(info.TypeHash);
        }

        TreeViewItem item = new()
        {
            Header = header,
            Tag = node,
            Foreground = foreground
        };
        item.Expanded += SceneTreeItem_Expanded;
        if (node.ObjectIndex is int index)
            _visibleTreeItems[new SceneObjectKey(node.FileIndex, index)] = item;
        return item;
    }

    private void AddExpansionPlaceholder(TreeViewItem item, bool hasChildren)
    {
        if (hasChildren)
            item.Items.Add(new TreeViewItem { Header = "…", Tag = ExpansionPlaceholder.Instance });
    }

    private void SceneTreeItem_Expanded(object sender, RoutedEventArgs e)
    {
        if (sender is not TreeViewItem item ||
            item.Items.Count != 1 ||
            item.Items[0] is not TreeViewItem placeholder ||
            placeholder.Tag is not ExpansionPlaceholder ||
            item.Tag is not SceneTreeNode node)
            return;

        item.Items.Clear();
        DecodedSmoFile file = _treeFiles[node.FileIndex];
        IEnumerable<SceneObjectInfo> children = node.ObjectIndex is int parentIndex
            ? file.Objects.Where(entry => entry.ParentIndex == parentIndex)
            : file.Objects.Where(entry => entry.ParentIndex is null);
        foreach (SceneObjectInfo child in children
                     .OrderBy(GetTreeSortPriority)
                     .ThenBy(entry => entry.Name, StringComparer.OrdinalIgnoreCase)
                     .ThenBy(entry => entry.Index))
        {
            TreeViewItem childItem = CreateTreeItem(
                new SceneTreeNode(node.FileIndex, child.Index));
            AddExpansionPlaceholder(
                childItem,
                file.Objects.Any(entry => entry.ParentIndex == child.Index));
            item.Items.Add(childItem);
        }

        e.Handled = true;
    }

    private string GetLogicalObjectKind(int fileIndex, SceneObjectInfo info)
    {
        if (info.TypeHash == SmoClassIds.Node)
        {
            DecodedSmoFile file = _treeFiles[fileIndex];
            if (file.CollisionVolumes.Any(item => item.ObjectIndex == info.Index)) return "Collision";
            if (file.ControlRig.Any(item => item.ObjectIndex == info.Index)) return "Control";
            if (file.Markers.Any(item => item.ObjectIndex == info.Index)) return "Marker";
            SkeletonBone? bone = file.Skeleton.FirstOrDefault(item =>
                item.ObjectIndex == info.Index);
            if (bone?.IsAttachment == true) return "Attachment";
            if (bone is not null) return "Bone";
            return "Node";
        }
        return info.TypeHash switch
        {
            SmoClassIds.Model => "Model",
            SmoClassIds.RenderNode => "Render",
            SmoClassIds.Skin => "Palette",
            SmoClassIds.MeshData => "Mesh",
            SmoClassIds.MaterialData => "Material",
            SmoClassIds.TextureData => "Texture",
            SmoClassIds.StaticRenderObject => "Instance",
            0x4DA04889 => "Collision shape?",
            0x5E6402DF => "Ambient?",
            _ => SmoClassRegistry.GetDisplayName(info.TypeHash)
        };
    }

    private static int GetTreeSortPriority(SceneObjectInfo info) => info.TypeHash switch
    {
        SmoClassIds.Model => 0,
        SmoClassIds.RenderNode => 1,
        SmoClassIds.Skin => 2,
        SmoClassIds.MeshData => 3,
        SmoClassIds.MaterialData => 4,
        SmoClassIds.TextureData => 5,
        SmoClassIds.Node => 6,
        _ => 10
    };

    private void SceneTree_SelectedItemChanged(
        object sender,
        RoutedPropertyChangedEventArgs<object> e)
    {
        if (e.NewValue is not TreeViewItem item || item.Tag is not SceneTreeNode node)
            return;

        BoneListItem? bone = node.ObjectIndex is int objectIndex
            ? _allBoneItems.FirstOrDefault(candidate =>
                candidate.FileIndex == node.FileIndex &&
                candidate.ObjectIndex == objectIndex)
            : null;
        CollisionVolume? collision = node.ObjectIndex is int collisionIndex
            ? _treeFiles[node.FileIndex].CollisionVolumes.FirstOrDefault(candidate =>
                candidate.ObjectIndex == collisionIndex)
            : null;
        HelperNode? control = node.ObjectIndex is int controlIndex
            ? _treeFiles[node.FileIndex].ControlRig.FirstOrDefault(candidate =>
                candidate.ObjectIndex == controlIndex)
            : null;
        HelperNode? marker = node.ObjectIndex is int markerIndex
            ? _treeFiles[node.FileIndex].Markers.FirstOrDefault(candidate =>
                candidate.ObjectIndex == markerIndex)
            : null;
        _selectedAuxiliary = collision is not null
            ? new SceneObjectKey(node.FileIndex, collision.ObjectIndex)
            : control is not null
                ? new SceneObjectKey(node.FileIndex, control.ObjectIndex)
                : marker is not null
                    ? new SceneObjectKey(node.FileIndex, marker.ObjectIndex)
                    : null;
        AuxiliaryObjectList.SelectedItem = node.ObjectIndex is int auxiliaryIndex
            ? _auxiliaryItems.FirstOrDefault(candidate =>
                candidate.FileIndex == node.FileIndex && candidate.ObjectIndex == auxiliaryIndex)
            : null;
        if (bone is null && _selectedBone is not null)
        {
            _selectedBone = null;
            BoneList.SelectedItem = null;
            UpdateSkeletonVisibility();
            UpdateMeshAppearance();
        }
        else if (bone is not null)
        {
            _selectedBone = bone;
            BoneList.SelectedItem = BoneList.Items.Cast<BoneListItem>().FirstOrDefault(candidate =>
                candidate.FileIndex == bone.FileIndex && candidate.ObjectIndex == bone.ObjectIndex);
            UpdateSkeletonVisibility();
        }

        UpdateSkeletonVisibility();
        if (collision is not null)
            AddLog($"Коллизия: {collision.Name}; выбранный объём подсвечен красным.");

        HighlightTreeSelection(node);
        if (bone is not null)
        {
            UpdateMeshAppearance();
        }
    }

    private void SceneTree_MouseDoubleClick(object sender, MouseButtonEventArgs e)
    {
        if (SceneTree.SelectedItem is not TreeViewItem item ||
            item.Tag is not SceneTreeNode node)
            return;

        FrameTreeNode(node);
        e.Handled = true;
    }

    private void SelectObjectAt(Point viewportPoint)
    {
        SceneObjectKey? selectedKey = null;
        VisualTreeHelper.HitTest(
            SceneViewport,
            null,
            result =>
            {
                if (result is RayMeshGeometry3DHitTestResult meshHit &&
                    meshHit.ModelHit is GeometryModel3D model &&
                    _geometryObjectKeys.TryGetValue(model, out SceneObjectKey key))
                {
                    selectedKey = key;
                    return HitTestResultBehavior.Stop;
                }

                return HitTestResultBehavior.Continue;
            },
            new PointHitTestParameters(viewportPoint));
        if (selectedKey is not SceneObjectKey key)
        {
            AddLog("ЛКМ: под курсором нет отображаемого SMO-объекта.");
            return;
        }

        TreeViewItem? item = RevealTreeObject(key);
        if (item is null)
        {
            AddLog($"Не удалось раскрыть object [{key.ObjectIndex}] в дереве.");
            return;
        }

        item.IsSelected = true;
        item.BringIntoView();
        item.Focus();
    }

    private TreeViewItem? RevealTreeObject(SceneObjectKey key)
    {
        if (!_treeFiles.TryGetValue(key.FileIndex, out DecodedSmoFile? file))
            return null;

        TreeViewItem? root = SceneTree.Items
            .OfType<TreeViewItem>()
            .FirstOrDefault(item =>
                item.Tag is SceneTreeNode node && node.FileIndex == key.FileIndex);
        if (root is null)
            return null;

        var path = new Stack<int>();
        int? cursor = key.ObjectIndex;
        while (cursor is int index)
        {
            path.Push(index);
            cursor = file.Objects[index].ParentIndex;
        }

        TreeViewItem current = root;
        current.IsExpanded = true;
        while (path.Count > 0)
        {
            int index = path.Pop();
            TreeViewItem? child = current.Items
                .OfType<TreeViewItem>()
                .FirstOrDefault(item =>
                    item.Tag is SceneTreeNode node && node.ObjectIndex == index);
            if (child is null)
                return null;
            current = child;
            if (path.Count > 0)
                current.IsExpanded = true;
        }

        return current;
    }

    private void HighlightTreeSelection(SceneTreeNode node)
    {
        foreach (SceneGeometry selected in _highlightedGeometry)
        {
            selected.Model.Material = selected.OriginalMaterial;
            selected.Model.BackMaterial = selected.OriginalMaterial;
        }
        _highlightedGeometry.Clear();
        _animatedMaterials.Clear();

        HashSet<int> related = GetRelatedObjectIndices(node);
        foreach ((SceneObjectKey key, TreeViewItem item) in _visibleTreeItems)
        {
            if (key.FileIndex != node.FileIndex)
                continue;
            SceneObjectInfo info = _treeFiles[key.FileIndex].Objects[key.ObjectIndex];
            item.Foreground = related.Contains(key.ObjectIndex)
                ? Brushes.DeepSkyBlue
                : GetClassBrush(info.TypeHash);
        }

        DiffuseMaterial highlight = new(new SolidColorBrush(Color.FromRgb(255, 176, 35)));
        highlight.Freeze();
        foreach (int index in related)
        {
            if (!_sceneGeometry.TryGetValue(
                    new SceneObjectKey(node.FileIndex, index), out SceneGeometry? geometry))
                continue;
            if (_animatedMaterials.Any(animated =>
                    ReferenceEquals(animated.Model, geometry.Model)))
                continue;
            geometry.Model.Material = highlight;
            geometry.Model.BackMaterial = highlight;
            _highlightedGeometry.Add(geometry);
        }

        if (node.ObjectIndex is int selectedIndex)
        {
            SceneObjectInfo info = _treeFiles[node.FileIndex].Objects[selectedIndex];
            AddLog($"Выбран [{selectedIndex}] {SmoClassRegistry.GetDisplayName(info.TypeHash)} {info.Name}");
        }
        if (_selectedBone is not null && HighlightInfluencedMeshesCheck?.IsChecked == true)
            UpdateMeshAppearance();
    }

    private HashSet<int> GetRelatedObjectIndices(SceneTreeNode node)
    {
        var result = new HashSet<int>();
        if (node.ObjectIndex is not int selected)
            return result;

        DecodedSmoFile file = _treeFiles[node.FileIndex];
        Dictionary<int, List<int>> children = file.Objects
            .Where(item => item.ParentIndex.HasValue)
            .GroupBy(item => item.ParentIndex!.Value)
            .ToDictionary(group => group.Key, group => group.Select(item => item.Index).ToList());
        var pending = new Stack<int>();
        pending.Push(selected);
        if (file.Objects[selected].ParentIndex is int siblingParent)
        {
            foreach (SceneObjectInfo sibling in file.Objects.Where(
                         item => item.ParentIndex == siblingParent &&
                                 item.TypeHash is SmoClassIds.MeshData or
                                     SmoClassIds.MaterialData or SmoClassIds.TextureData))
                pending.Push(sibling.Index);
        }
        while (pending.Count > 0)
        {
            int index = pending.Pop();
            if (!result.Add(index) || !children.TryGetValue(index, out List<int>? descendants))
                continue;
            foreach (int child in descendants)
                pending.Push(child);
        }

        int? cursor = file.Objects[selected].ParentIndex;
        while (cursor is int ancestor)
        {
            result.Add(ancestor);
            cursor = file.Objects[ancestor].ParentIndex;
        }
        return result;
    }

    private void FrameTreeNode(SceneTreeNode node)
    {
        if (node.ObjectIndex is int selectedIndex &&
            TryGetHelperBounds(node.FileIndex, selectedIndex, out BoundsBuilder helperBounds))
        {
            FrameBounds(helperBounds);
            AddLog("Камера перемещена к выбранному вспомогательному объекту.");
            return;
        }

        BoundsBuilder bounds = new();
        IEnumerable<int> indices = node.ObjectIndex is null
            ? _treeFiles[node.FileIndex].Objects.Select(item => item.Index)
            : GetRelatedObjectIndices(node);
        foreach (int index in indices)
        {
            if (_sceneGeometry.TryGetValue(
                    new SceneObjectKey(node.FileIndex, index), out SceneGeometry? geometry))
                bounds.Merge(geometry.Bounds);
        }
        if (!bounds.HasValue)
        {
            AddLog("У выбранного объекта нет отображаемой геометрии.");
            return;
        }

        FrameBounds(bounds);
        AddLog("Камера перемещена к выбранному объекту.");
    }

    private bool TryGetHelperBounds(int fileIndex, int objectIndex, out BoundsBuilder bounds)
    {
        bounds = new BoundsBuilder();
        if (!_treeFiles.TryGetValue(fileIndex, out DecodedSmoFile? file))
            return false;

        CollisionVolume? collision = file.CollisionVolumes.FirstOrDefault(item =>
            item.ObjectIndex == objectIndex);
        if (collision is not null)
        {
            System.Numerics.Vector3 half = collision.Size * 0.5f;
            for (int x = -1; x <= 1; x += 2)
            for (int y = -1; y <= 1; y += 2)
            for (int z = -1; z <= 1; z += 2)
            {
                var corner = new System.Numerics.Vector3(
                    half.X * x, half.Y * y, half.Z * z);
                bounds.Include(ToViewportPoint(System.Numerics.Vector3.Transform(
                    corner, collision.WorldTransform)));
            }
            return true;
        }

        SkeletonBone? bone = file.Skeleton.FirstOrDefault(item =>
            item.ObjectIndex == objectIndex);
        HelperNode? helper = file.ControlRig.Concat(file.Markers).FirstOrDefault(item =>
            item.ObjectIndex == objectIndex);
        System.Numerics.Vector3? position = bone?.Position ?? helper?.Position;
        if (position is not System.Numerics.Vector3 point)
            return false;

        Point3D center = ToViewportPoint(point);
        double padding = Math.Max(_sceneBounds.DiagonalLength * 0.025, 0.05);
        bounds.Include(new Point3D(center.X - padding, center.Y - padding, center.Z - padding));
        bounds.Include(new Point3D(center.X + padding, center.Y + padding, center.Z + padding));
        return true;
    }

    private static Brush GetClassBrush(uint typeHash) => typeHash switch
    {
        SmoClassIds.MeshData => Brushes.LightGreen,
        SmoClassIds.MaterialData => Brushes.Khaki,
        SmoClassIds.TextureData => Brushes.Plum,
        SmoClassIds.Skin => Brushes.LightSalmon,
        SmoClassIds.Node => Brushes.LightGoldenrodYellow,
        SmoClassIds.StaticRenderObject => Brushes.LightSkyBlue,
        SmoClassIds.Model => Brushes.PaleTurquoise,
        _ => Brushes.Gainsboro
    };

    private void FrameScene()
    {
        FrameBounds(_sceneBounds);
    }

    private void FrameBounds(BoundsBuilder bounds)
    {
        if (!bounds.HasValue)
            return;

        _cameraTarget = bounds.Center;

        double radius = Math.Max(bounds.DiagonalLength * 0.5, 0.01);
        double halfFieldOfView = SceneCamera.FieldOfView * Math.PI / 360.0;
        _cameraDistance = Math.Max(radius / Math.Tan(halfFieldOfView) * 1.25, 0.1);

        if (_cameraControlMode == CameraControlMode.Fly)
            _flyPosition = _cameraTarget - GetFlyForward() * _cameraDistance;

        UpdateCameraClippingPlanes();
        UpdateCamera();
    }

    private void Viewport_MouseDown(object sender, MouseButtonEventArgs e)
    {
        if (e.ChangedButton == MouseButton.Left)
        {
            SelectObjectAt(e.GetPosition(SceneViewport));
            e.Handled = true;
            return;
        }

        if (_cameraControlMode == CameraControlMode.Fly)
        {
            if (e.ChangedButton != MouseButton.Right)
                return;

            _navigationMode = CameraNavigationMode.FlyLook;
            _lastMousePosition = e.GetPosition(SceneViewport);
            _navigationStartYaw = _cameraYaw;
            _navigationStartPitch = _cameraPitch;
            SceneSurface.Cursor = Cursors.Cross;
            SceneSurface.CaptureMouse();
            SceneSurface.Focus();
            e.Handled = true;
            return;
        }

        if (e.ChangedButton == MouseButton.Right &&
            _navigationMode != CameraNavigationMode.None)
        {
            CancelCameraNavigation();
            e.Handled = true;
            return;
        }

        if (e.ChangedButton != MouseButton.Middle)
            return;

        ModifierKeys modifiers = Keyboard.Modifiers;
        bool control = (modifiers & ModifierKeys.Control) != 0;
        bool shift = (modifiers & ModifierKeys.Shift) != 0;
        _navigationMode = (control, shift) switch
        {
            (true, true) => CameraNavigationMode.Dolly,
            (true, false) => CameraNavigationMode.Zoom,
            (false, true) => CameraNavigationMode.Pan,
            _ => CameraNavigationMode.Orbit
        };

        _lastMousePosition = e.GetPosition(SceneViewport);
        _navigationStartTarget = _cameraTarget;
        _navigationStartYaw = _cameraYaw;
        _navigationStartPitch = _cameraPitch;
        _navigationStartDistance = _cameraDistance;

        SceneSurface.Cursor = _navigationMode switch
        {
            CameraNavigationMode.Pan => Cursors.Hand,
            CameraNavigationMode.Zoom or CameraNavigationMode.Dolly => Cursors.SizeNS,
            _ => Cursors.SizeAll
        };
        SceneSurface.CaptureMouse();
        SceneSurface.Focus();
        e.Handled = true;
    }

    private void Viewport_MouseUp(object sender, MouseButtonEventArgs e)
    {
        MouseButton expectedButton = _cameraControlMode == CameraControlMode.Fly
            ? MouseButton.Right
            : MouseButton.Middle;
        if (e.ChangedButton != expectedButton ||
            _navigationMode == CameraNavigationMode.None)
        {
            return;
        }

        EndCameraNavigation();
        e.Handled = true;
    }

    private void Viewport_LostMouseCapture(object sender, MouseEventArgs e)
    {
        _navigationMode = CameraNavigationMode.None;
        SceneSurface.Cursor = null;
    }

    private void Viewport_MouseMove(object sender, MouseEventArgs e)
    {
        if (_navigationMode == CameraNavigationMode.None)
            return;

        bool navigationButtonPressed = _navigationMode == CameraNavigationMode.FlyLook
            ? e.RightButton == MouseButtonState.Pressed
            : e.MiddleButton == MouseButtonState.Pressed;
        if (!navigationButtonPressed)
        {
            EndCameraNavigation();
            return;
        }

        Point currentPosition = e.GetPosition(SceneViewport);
        Vector delta = currentPosition - _lastMousePosition;
        _lastMousePosition = currentPosition;

        switch (_navigationMode)
        {
            case CameraNavigationMode.FlyLook:
                _cameraYaw -= delta.X * FlyLookSensitivity;
                _cameraPitch = Math.Clamp(
                    _cameraPitch + delta.Y * FlyLookSensitivity,
                    -PitchLimit,
                    PitchLimit);
                break;
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
            case CameraNavigationMode.Dolly:
                DollyCamera(delta.Y);
                break;
        }

        UpdateCamera();
        e.Handled = true;
    }

    private void Viewport_MouseWheel(object sender, MouseWheelEventArgs e)
    {
        if (_cameraControlMode == CameraControlMode.Fly)
        {
            _flySpeed = Math.Clamp(
                _flySpeed * Math.Exp(e.Delta / 120.0 * 0.18),
                0.01,
                100000);
            UpdateCameraHelpToolTip();
            SceneSurface.Focus();
            e.Handled = true;
            return;
        }

        ChangeCameraDistance(Math.Exp(-e.Delta / 120.0 * 0.14));
        UpdateCamera();
        SceneSurface.Focus();
        e.Handled = true;
    }

    private void Window_PreviewKeyDown(object sender, KeyEventArgs e)
    {
        // Window-level camera shortcuts are handled during the preview phase.  Let text
        // editing controls receive the keys first; otherwise keys shared with camera
        // controls (for example Shift+OemMinus, which types '_') are swallowed here.
        if (IsTextEditingInput(e.OriginalSource) ||
            IsTextEditingInput(Keyboard.FocusedElement))
        {
            return;
        }

        if (_cameraControlMode == CameraControlMode.Fly && IsFlyMovementKey(e.Key))
        {
            _pressedKeys.Add(e.Key);
            e.Handled = true;
            return;
        }

        if (e.Key == Key.Escape && _navigationMode != CameraNavigationMode.None)
        {
            CancelCameraNavigation();
            e.Handled = true;
            return;
        }

        ModifierKeys modifiers = Keyboard.Modifiers;
        bool control = (modifiers & ModifierKeys.Control) != 0;
        bool shift = (modifiers & ModifierKeys.Shift) != 0;
        bool handled = true;

        switch (e.Key)
        {
            case Key.Left:
                PanCameraByKey(horizontal: -1, vertical: 0, shift);
                break;
            case Key.Right:
                PanCameraByKey(horizontal: 1, vertical: 0, shift);
                break;
            case Key.Up:
                PanCameraByKey(horizontal: 0, vertical: 1, shift);
                break;
            case Key.Down:
                PanCameraByKey(horizontal: 0, vertical: -1, shift);
                break;
            case Key.Home:
            case Key.Decimal:
                FrameScene();
                break;
            case Key.Add:
            case Key.OemPlus:
                ChangeCameraDistance(0.85);
                UpdateCamera();
                break;
            case Key.Subtract:
            case Key.OemMinus:
                ChangeCameraDistance(1.0 / 0.85);
                UpdateCamera();
                break;
            case Key.NumPad1:
                SetAxisView(AxisView.Front, opposite: control);
                break;
            case Key.NumPad3:
                SetAxisView(AxisView.Right, opposite: control);
                break;
            case Key.NumPad7:
                SetAxisView(AxisView.Top, opposite: control);
                break;
            case Key.NumPad9:
                _cameraYaw += Math.PI;
                _cameraPitch = -_cameraPitch;
                UpdateCamera();
                break;
            case Key.NumPad2:
                NavigateWithNumpad(horizontal: 0, vertical: -1, control);
                break;
            case Key.NumPad4:
                NavigateWithNumpad(horizontal: -1, vertical: 0, control);
                break;
            case Key.NumPad6:
                NavigateWithNumpad(horizontal: 1, vertical: 0, control);
                break;
            case Key.NumPad8:
                NavigateWithNumpad(horizontal: 0, vertical: 1, control);
                break;
            default:
                handled = false;
                break;
        }

        e.Handled = handled;
    }

    private static bool IsTextEditingInput(object? source) =>
        source is System.Windows.Controls.Primitives.TextBoxBase or
        System.Windows.Controls.PasswordBox or
        System.Windows.Controls.ComboBox { IsEditable: true };

    private void Window_PreviewKeyUp(object sender, KeyEventArgs e)
    {
        if (_pressedKeys.Remove(e.Key))
            e.Handled = true;
    }

    private void Window_Deactivated(object? sender, EventArgs e)
    {
        _pressedKeys.Clear();
        EndCameraNavigation();
    }

    private void Window_Closed(object? sender, EventArgs e)
    {
        CompositionTarget.Rendering -= CompositionTarget_Rendering;
        GameValidationPanel.Dispose();
    }

    private void CameraModeSelector_SelectionChanged(
        object sender,
        SelectionChangedEventArgs e)
    {
        if (!IsInitialized || CameraModeSelector.SelectedItem is not ComboBoxItem item)
            return;

        CameraControlMode requested = Equals(item.Tag, "Fly")
            ? CameraControlMode.Fly
            : CameraControlMode.Orbit;
        if (requested == _cameraControlMode)
            return;

        EndCameraNavigation();
        _pressedKeys.Clear();
        if (requested == CameraControlMode.Fly)
        {
            _flyPosition = SceneCamera.Position;
            _flySpeed = Math.Clamp(_cameraDistance * 0.3, 0.01, 100000);
        }
        else
        {
            Vector3D forward = GetFlyForward();
            _cameraTarget = _flyPosition + forward * _cameraDistance;
        }

        _cameraControlMode = requested;
        UpdateCameraHelpToolTip();
        _lastRenderTime = null;
        UpdateCamera();
        SceneSurface.Focus();
    }

    private void UpdateCameraHelpToolTip()
    {
        CameraHelpIcon.ToolTip = _cameraControlMode == CameraControlMode.Fly
            ? "Управление камерой — Полёт\n" +
              "ПКМ — обзор · WASD — движение · Q/E — вниз/вверх\n" +
              $"Shift — быстрее · колесо — скорость ({_flySpeed:G4}) · Home — вся сцена"
            : "Управление камерой — Blender\n" +
              "СКМ — вращение · Shift+СКМ — сдвиг · Ctrl+СКМ/колесо — масштаб\n" +
              "Стрелки — камера · Home — вся сцена · Ctrl+Shift+СКМ — dolly\n" +
              "NumPad 1/3/7 — спереди/справа/сверху · NumPad 2/4/6/8 — шаговое вращение\n" +
              "ПКМ/Esc — отмена жеста";
    }

    private void CompositionTarget_Rendering(object? sender, EventArgs e)
    {
        if (e is not RenderingEventArgs rendering)
            return;

        UpdateAnimatedMaterials(rendering.RenderingTime);

        if (_lastRenderTime is not TimeSpan previous)
        {
            _lastRenderTime = rendering.RenderingTime;
            return;
        }

        double elapsed = Math.Clamp(
            (rendering.RenderingTime - previous).TotalSeconds, 0, 0.1);
        _lastRenderTime = rendering.RenderingTime;
        if (_animationPlaying && _selectedAnimation is not null)
        {
            _animationTime += elapsed;
            if (_animationTime >= _selectedAnimation.Duration)
                _animationTime = _selectedAnimation.Duration > 0
                    ? _animationTime % _selectedAnimation.Duration : 0;
            ApplyAnimationPose();
        }
        if (_cameraControlMode != CameraControlMode.Fly || _pressedKeys.Count == 0)
            return;

        GetCameraBasis(out Vector3D forward, out Vector3D right, out _);
        Vector3D worldUp = new(0, 1, 0);
        Vector3D movement = new();
        if (_pressedKeys.Contains(Key.W)) movement += forward;
        if (_pressedKeys.Contains(Key.S)) movement -= forward;
        if (_pressedKeys.Contains(Key.D)) movement += right;
        if (_pressedKeys.Contains(Key.A)) movement -= right;
        if (_pressedKeys.Contains(Key.E)) movement += worldUp;
        if (_pressedKeys.Contains(Key.Q)) movement -= worldUp;
        if (movement.LengthSquared < 1e-12)
            return;

        movement.Normalize();
        double multiplier = Keyboard.Modifiers.HasFlag(ModifierKeys.Shift) ? 4 : 1;
        _flyPosition += movement * (_flySpeed * multiplier * elapsed);
        UpdateCamera();
    }

    private void UpdateAnimatedMaterials(TimeSpan renderingTime)
    {
        foreach (AnimatedSceneMaterial animated in _animatedMaterials)
        {
            int frame = (int)(renderingTime.Ticks / animated.FrameDuration.Ticks %
                              animated.Materials.Length);
            if (!animated.Materials.Any(material =>
                    ReferenceEquals(animated.Model.Material, material)))
                continue;
            if (ReferenceEquals(animated.Model.Material, animated.Materials[frame]))
            {
                animated.CurrentFrame = frame;
                continue;
            }
            animated.CurrentFrame = frame;
            animated.Model.Material = animated.Materials[frame];
            animated.Model.BackMaterial = animated.Materials[frame];
        }
    }

    private static bool IsFlyMovementKey(Key key) =>
        key is Key.W or Key.A or Key.S or Key.D or Key.Q or Key.E;

    private void NavigateWithNumpad(int horizontal, int vertical, bool pan)
    {
        if (pan)
        {
            PanCameraByKey(horizontal, vertical, fast: false);
            return;
        }

        const double step = Math.PI / 12;
        _cameraYaw += horizontal * step;
        _cameraPitch = Math.Clamp(
            _cameraPitch + vertical * step,
            -PitchLimit,
            PitchLimit);
        UpdateCamera();
    }

    private void SetAxisView(AxisView view, bool opposite)
    {
        switch (view)
        {
            case AxisView.Front:
                _cameraYaw = opposite ? Math.PI : 0;
                _cameraPitch = 0;
                break;
            case AxisView.Right:
                _cameraYaw = opposite ? -Math.PI / 2 : Math.PI / 2;
                _cameraPitch = 0;
                break;
            case AxisView.Top:
                _cameraYaw = 0;
                _cameraPitch = opposite ? -Math.PI / 2 : Math.PI / 2;
                break;
        }

        UpdateCamera();
    }

    private void PanCamera(double horizontalPixels, double verticalPixels)
    {
        GetCameraBasis(out _, out Vector3D right, out Vector3D up);
        double scale = GetWorldUnitsPerPixel();
        _cameraTarget +=
            -right * (horizontalPixels * scale) +
            up * (verticalPixels * scale);
    }

    private void PanCameraByKey(int horizontal, int vertical, bool fast)
    {
        GetCameraBasis(out _, out Vector3D right, out Vector3D up);
        double screenPixels = fast ? 160 : 40;
        double scale = GetWorldUnitsPerPixel() * screenPixels;
        _cameraTarget += right * (horizontal * scale) + up * (vertical * scale);
        UpdateCamera();
    }

    private void DollyCamera(double verticalPixels)
    {
        GetCameraBasis(out Vector3D forward, out _, out _);
        double amount = -verticalPixels * GetWorldUnitsPerPixel() * 3;
        _cameraTarget += forward * amount;
    }

    private void ChangeCameraDistance(double factor)
    {
        _cameraDistance = Math.Clamp(
            _cameraDistance * factor,
            MinimumCameraDistance,
            MaximumCameraDistance);
        UpdateCameraClippingPlanes();
    }

    private double GetWorldUnitsPerPixel()
    {
        double viewportHeight = Math.Max(SceneViewport.ActualHeight, 1);
        double halfFieldOfView = SceneCamera.FieldOfView * Math.PI / 360.0;
        double visibleHeight = 2 * _cameraDistance * Math.Tan(halfFieldOfView);
        return visibleHeight / viewportHeight;
    }

    private void GetCameraBasis(
        out Vector3D forward,
        out Vector3D right,
        out Vector3D up)
    {
        forward = SceneCamera.LookDirection;
        if (forward.LengthSquared < 1e-12)
            forward = new Vector3D(0, 0, -1);
        forward.Normalize();

        Vector3D upHint = SceneCamera.UpDirection;
        if (upHint.LengthSquared < 1e-12)
            upHint = new Vector3D(0, 1, 0);
        upHint.Normalize();

        right = Vector3D.CrossProduct(forward, upHint);
        if (right.LengthSquared < 1e-12)
            right = new Vector3D(1, 0, 0);
        right.Normalize();

        up = Vector3D.CrossProduct(right, forward);
        up.Normalize();
    }

    private void CancelCameraNavigation()
    {
        _cameraTarget = _navigationStartTarget;
        _cameraYaw = _navigationStartYaw;
        _cameraPitch = _navigationStartPitch;
        _cameraDistance = _navigationStartDistance;
        UpdateCameraClippingPlanes();
        UpdateCamera();
        EndCameraNavigation();
    }

    private void EndCameraNavigation()
    {
        _navigationMode = CameraNavigationMode.None;
        SceneSurface.Cursor = null;
        if (SceneSurface.IsMouseCaptured)
            SceneSurface.ReleaseMouseCapture();
    }

    private void UpdateCameraClippingPlanes()
    {
        SceneCamera.NearPlaneDistance = Math.Max(_cameraDistance / 10000.0, 0.001);
        SceneCamera.FarPlaneDistance = Math.Max(_cameraDistance * 100.0, 100.0);
        SynchronizeSkeletonCamera();
    }

    private void UpdateCamera()
    {
        if (_cameraControlMode == CameraControlMode.Fly)
        {
            SceneCamera.Position = _flyPosition;
            SceneCamera.LookDirection = GetFlyForward();
            SceneCamera.UpDirection = new Vector3D(0, 1, 0);
            SynchronizeSkeletonCamera();
            return;
        }

        double horizontal = Math.Cos(_cameraPitch) * _cameraDistance;
        Vector3D offset = new(
            Math.Sin(_cameraYaw) * horizontal,
            Math.Sin(_cameraPitch) * _cameraDistance,
            Math.Cos(_cameraYaw) * horizontal);

        SceneCamera.Position = _cameraTarget + offset;
        SceneCamera.LookDirection = _cameraTarget - SceneCamera.Position;
        SceneCamera.UpDirection = Math.Abs(horizontal) < 1e-9
            ? (_cameraPitch >= 0
                ? new Vector3D(0, 0, -1)
                : new Vector3D(0, 0, 1))
            : new Vector3D(0, 1, 0);
        SynchronizeSkeletonCamera();
    }

    private void SynchronizeSkeletonCamera()
    {
        if (SkeletonCamera is null)
            return;
        SkeletonCamera.Position = SceneCamera.Position;
        SkeletonCamera.LookDirection = SceneCamera.LookDirection;
        SkeletonCamera.UpDirection = SceneCamera.UpDirection;
        SkeletonCamera.FieldOfView = SceneCamera.FieldOfView;
        SkeletonCamera.NearPlaneDistance = SceneCamera.NearPlaneDistance;
        SkeletonCamera.FarPlaneDistance = SceneCamera.FarPlaneDistance;
    }

    private Vector3D GetFlyForward()
    {
        double horizontal = Math.Cos(_cameraPitch);
        Vector3D forward = new(
            -Math.Sin(_cameraYaw) * horizontal,
            -Math.Sin(_cameraPitch),
            -Math.Cos(_cameraYaw) * horizontal);
        forward.Normalize();
        return forward;
    }

    private static Color VaryColor(Color baseColor, int meshIndex)
    {
        double factor = 0.82 + (meshIndex % 4) * 0.06;
        return Color.FromRgb(
            (byte)Math.Clamp(baseColor.R * factor, 0, 255),
            (byte)Math.Clamp(baseColor.G * factor, 0, 255),
            (byte)Math.Clamp(baseColor.B * factor, 0, 255));
    }

    private static string BuildLoadStatus(
        int files,
        int decodedMeshes,
        int totalMeshes,
        int diagnostics,
        int unsupportedMeshes,
        int texturedMeshes,
        int textureIssues,
        int failedFiles,
        IReadOnlyList<LoadIssue> issues)
    {
        string result =
            $"Разобрано файлов: {files}; меши: {decodedMeshes}/{totalMeshes}; " +
            $"диагностики: {diagnostics}; unsupported: {unsupportedMeshes}; " +
            $"с текстурой: {texturedMeshes}; texture issues: {textureIssues}; " +
            $"сбоев: {failedFiles}.";

        LoadIssue? firstError = issues.FirstOrDefault(issue => issue.IsError);
        LoadIssue? highlighted = firstError ?? issues.FirstOrDefault();
        if (highlighted is not null)
            result += $" {highlighted.Text}";

        return result;
    }

    private enum CameraNavigationMode
    {
        None,
        FlyLook,
        Orbit,
        Pan,
        Zoom,
        Dolly
    }

    private enum CameraControlMode
    {
        Orbit,
        Fly
    }

    private enum AxisView
    {
        Front,
        Right,
        Top
    }

    private sealed record DecodedRenderMesh(
        SmoMesh Mesh,
        SmoTexture? Texture,
        IReadOnlyList<SmoTexture>? AnimationFrames,
        TimeSpan? AnimationFrameDuration,
        SmoTexture? BaseTexture,
        uint? MaterialColorArgb,
        bool UsesAlphaBlend,
        Matrix4x4 WorldTransform,
        int? SkinObjectIndex,
        int? RigidNodeObjectIndex,
        IReadOnlyDictionary<int, float> BoneInfluences);

    private sealed class AnimatedSceneMaterial(
        GeometryModel3D model,
        Material[] materials,
        TimeSpan frameDuration)
    {
        public GeometryModel3D Model { get; } = model;
        public Material[] Materials { get; } = materials;
        public TimeSpan FrameDuration { get; } = frameDuration;
        public int CurrentFrame { get; set; }
    }

    private sealed record SceneAddResult(int MeshCount, int TexturedMeshCount);

    private sealed record DecodedSmoFile(
        string Path,
        int TotalMeshCount,
        IReadOnlyList<DecodedRenderMesh> RenderMeshes,
        IReadOnlyList<SceneObjectInfo> Objects,
        IReadOnlyList<SmoDiagnostic> Diagnostics,
        IReadOnlyList<string> DecodeErrors,
        IReadOnlyList<string> TextureIssues,
        IReadOnlyList<SkeletonBone> Skeleton,
        IReadOnlyList<CollisionVolume> CollisionVolumes,
        IReadOnlyList<HelperNode> ControlRig,
        IReadOnlyList<HelperNode> Markers,
        IReadOnlyList<AuxiliaryObjectInfo> AuxiliaryObjects,
        IReadOnlyDictionary<int, SmoSkin> Skins,
        IReadOnlyList<AnimationNode> AnimationNodes);

    private sealed record AnimationNode(
        int ObjectIndex, string Name, Matrix4x4 BindWorldMatrix,
        int? ParentObjectIndex);

    private sealed record CollisionVolume(
        int ObjectIndex, string Name, Matrix4x4 WorldTransform,
        System.Numerics.Vector3 Size);

    private sealed record HelperNode(
        int ObjectIndex, string Name, System.Numerics.Vector3 Position,
        int? ParentObjectIndex);

    private sealed record AuxiliaryObjectInfo(int ObjectIndex, string Name, string Role);
    private sealed record AuxiliaryObjectItem(
        int FileIndex, int ObjectIndex, string Display);

    private sealed record SkeletonBone(
        int ObjectIndex,
        string Name,
        System.Numerics.Vector3 Position,
        Matrix4x4 BindWorldMatrix,
        int? ParentObjectIndex,
        bool IsAttachment,
        IReadOnlyList<BonePaletteReference> Palettes);

    private sealed record BonePaletteReference(
        int SkinObjectIndex,
        string SkinName,
        int PaletteIndex);

    private sealed record BoneListItem(
        int FileIndex,
        int ObjectIndex,
        string Name,
        string Display);

    private sealed record SceneObjectInfo(
        int Index,
        int? ParentIndex,
        uint TypeHash,
        string Name);

    private readonly record struct SceneObjectKey(int FileIndex, int ObjectIndex);

    private sealed record SceneTreeNode(int FileIndex, int? ObjectIndex);

    private sealed record SceneGeometry(
        GeometryModel3D Model,
        MeshGeometry3D Geometry,
        Material OriginalMaterial,
        BoundsBuilder Bounds,
        DecodedRenderMesh RenderMesh);

    private sealed record AnimationListItem(string Path, string Display);

    private sealed class ExpansionPlaceholder
    {
        public static readonly ExpansionPlaceholder Instance = new();
        private ExpansionPlaceholder() { }
    }

    private sealed record LoadIssue(bool IsError, string Text);

    private sealed class BoundsBuilder
    {
        private double _minX;
        private double _minY;
        private double _minZ;
        private double _maxX;
        private double _maxY;
        private double _maxZ;

        public bool HasValue { get; private set; }

        public double MinY => HasValue ? _minY : 0;
        public double Width => HasValue ? _maxX - _minX : 0;
        public double Depth => HasValue ? _maxZ - _minZ : 0;

        public Point3D Center => new(
            (_minX + _maxX) * 0.5,
            (_minY + _maxY) * 0.5,
            (_minZ + _maxZ) * 0.5);

        public double DiagonalLength
        {
            get
            {
                double x = _maxX - _minX;
                double y = _maxY - _minY;
                double z = _maxZ - _minZ;
                return Math.Sqrt(x * x + y * y + z * z);
            }
        }

        public void Include(Point3D point)
        {
            if (!HasValue)
            {
                _minX = _maxX = point.X;
                _minY = _maxY = point.Y;
                _minZ = _maxZ = point.Z;
                HasValue = true;
                return;
            }

            _minX = Math.Min(_minX, point.X);
            _minY = Math.Min(_minY, point.Y);
            _minZ = Math.Min(_minZ, point.Z);
            _maxX = Math.Max(_maxX, point.X);
            _maxY = Math.Max(_maxY, point.Y);
            _maxZ = Math.Max(_maxZ, point.Z);
        }

        public void Merge(BoundsBuilder source)
        {
            if (!source.HasValue)
                return;

            Include(new Point3D(source._minX, source._minY, source._minZ));
            Include(new Point3D(source._maxX, source._maxY, source._maxZ));
        }

        public void Reset()
        {
            HasValue = false;
        }
    }
}
