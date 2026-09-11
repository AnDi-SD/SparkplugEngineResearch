using OpenTK.Wpf;
using SmoLVLcreator.Core;
using SmoLVLcreator.Viewport.Wpf;
using SmoImporter.Core;
using SmoViewer.Core;
using SmoViewer.Rendering.Wpf;
using SmoViewer.Scene;
using System.Numerics;
using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Media3D;
using Color = System.Windows.Media.Color;
using NumericsQuaternion = System.Numerics.Quaternion;
using Point = System.Windows.Point;

namespace SmoLVLcreator.Gui;

/// <summary>
/// Editor-owned viewport interaction. GPU drawing and SMO render decisions stay
/// in the shared SmoViewer modules.
/// </summary>
public partial class MainWindow
{
    private const double PerspectiveFieldOfView = 55;

    private static PerspectiveCamera CreatePerspectiveCamera() => new()
    {
        FieldOfView = PerspectiveFieldOfView,
        NearPlaneDistance = 0.01,
        FarPlaneDistance = 100000,
        UpDirection = new Vector3D(0, 1, 0)
    };

    private readonly SmoGpuSceneRenderer _gpuRenderer = new();
    private ProjectionCamera _sceneCamera = CreatePerspectiveCamera();
    private readonly HashSet<SmoRenderObjectKey> _highlightedRenderKeys = [];
    private readonly HashSet<SmoLevelEntityId> _selectedEntities = [];
    private readonly HashSet<SmoLevelEntityId> _hiddenEntities = [];
    private readonly Dictionary<int, PendingPlacementSelection> _pendingPlacementByScene = [];
    private PendingPlacementSelection? _selectedPendingPlacement;
    private PlacementClipboard? _placementClipboard;
    private PlacementSelectionKey? _activeSelectionKey;
    private int? _activeCollisionEntityIndex;
    private readonly SmoTranslationGizmoRenderer _translationGizmoRenderer = new();
    private readonly SmoRotationGizmoRenderer _rotationGizmoRenderer = new();
    private readonly SmoCollisionOverlayRenderer _collisionOverlayRenderer = new();
    private readonly SmoBoundsOverlayRenderer _boundsOverlayRenderer = new();
    private SmoTranslationGizmoLayout? _gizmoLayout;
    private SmoTranslationGizmoLayout? _gizmoDragLayout;
    private SmoRotationGizmoLayout? _rotationGizmoLayout;
    private SmoRotationGizmoLayout? _rotationGizmoDragLayout;
    private SmoGizmoAxis _hoverGizmoAxis;
    private SmoGizmoAxis _activeGizmoAxis;
    private SmoTransformSession? _gizmoTransformSession;
    private SmoPendingPlacementTransformSession? _pendingGizmoTransformSession;
    private Point _gizmoDragStart;
    private double _viewportDpiX = 1;
    private double _viewportDpiY = 1;
    private SmoScenePickIndex? _scenePickIndex;
    private SmoSceneMesh[] _scenePickMeshes = [];
    private readonly Dictionary<(SmoRenderObjectKey Key, int MeshObjectIndex), Vector3[]?>
        _scenePickingPositions = new();
    private readonly List<string> _scenePickingReadbackIssues = [];
    private bool _scenePickingNeedsGpuRefresh;
    private string? _scenePickingDiagnosticText;
    private bool _gpuRendererAvailable;
    private string? _gpuViewportFailureReason;
    private Point3D _sceneCenter;
    private double _sceneDiagonal = 1;
    private double _sceneFloorY;
    private double _sceneGridSpan = 20;
    private Point3D _cameraTarget;
    private double _cameraYaw = 0.65;
    private double _cameraPitch = -0.28;
    private double _cameraDistance = 10;
    private double _cameraDesiredDistance = 10;
    private readonly HashSet<Key> _cameraMovementKeys = [];
    private Vector3D _cameraVelocity;
    private bool _cameraOrbitingFocus;
    private (int MeshObjectIndex, int SceneObjectIndex, SmoRenderOccurrenceKey? Slot)? _cameraFocusKey;
    private int _activePlacementIndex;
    private Point _lastViewportMouse;
    private bool _viewportOrbiting;
    private bool _viewportPanning;
    private bool _showCollisions;
    private bool _showFloorGrid = true;
    private bool _showSelectionBounds;
    private SmoSceneRenderMode _sceneRenderMode = SmoSceneRenderMode.Lit;
    private readonly Dictionary<int, int> _collisionRegionByInfo = [];
    private SmoPreparedScene? _renderPreparedScene;
    private string? _renderedModelResourceSignature;
    private DateTime _viewportActionStatusUntilUtc;

    private void InitializeGpuViewport()
    {
        try
        {
            GpuViewport.Start(new GLWpfControlSettings
            {
                MajorVersion = 3,
                MinorVersion = 3,
                Samples = 0
            });
            _gpuRenderer.SetAntialiasingSamples(4);
            _gpuRenderer.RenderMode = _sceneRenderMode;
            _gpuRendererAvailable = true;
            _gpuViewportFailureReason = null;
            UpdateViewportModeButtons();
        }
        catch (Exception exception)
        {
            DisableGpuViewport(exception.Message);
        }
    }

    private void ProjectionModeButton_Click(object sender, RoutedEventArgs e)
    {
        ProjectionCamera previous = _sceneCamera;
        ProjectionCamera replacement;
        if (previous is PerspectiveCamera)
        {
            double aspect = Math.Max(GpuViewport.ActualWidth, 1) /
                            Math.Max(GpuViewport.ActualHeight, 1);
            double visibleHeight = 2 * Math.Max(_cameraDistance, 0.001) *
                Math.Tan(PerspectiveFieldOfView * Math.PI / 360);
            replacement = new OrthographicCamera
            {
                Width = Math.Max(visibleHeight * aspect, 0.001)
            };
        }
        else
        {
            replacement = new PerspectiveCamera
            {
                FieldOfView = PerspectiveFieldOfView
            };
        }

        replacement.Position = previous.Position;
        replacement.LookDirection = previous.LookDirection;
        replacement.UpDirection = previous.UpDirection;
        replacement.NearPlaneDistance = previous.NearPlaneDistance;
        replacement.FarPlaneDistance = previous.FarPlaneDistance;
        _sceneCamera = replacement;
        UpdateViewportModeButtons();
        UpdateCamera();
        StatusText.Text = _sceneCamera is OrthographicCamera
            ? "Проекция: Orthographic"
            : "Проекция: Perspective";
    }

    private void RenderModeButton_Click(object sender, RoutedEventArgs e)
    {
        _sceneRenderMode = _sceneRenderMode switch
        {
            SmoSceneRenderMode.Lit => SmoSceneRenderMode.Unlit,
            SmoSceneRenderMode.Unlit => SmoSceneRenderMode.Wireframe,
            _ => SmoSceneRenderMode.Lit
        };
        _gpuRenderer.RenderMode = _sceneRenderMode;
        UpdateViewportModeButtons();
        StatusText.Text = $"Режим отображения: {_sceneRenderMode}";
    }

    private void GridVisibilityButton_Click(object sender, RoutedEventArgs e)
    {
        _showFloorGrid = !_showFloorGrid;
        UpdateViewportModeButtons();
        StatusText.Text = _showFloorGrid
            ? "Сетка включена"
            : "Сетка выключена";
    }

    private void BoundsVisibilityButton_Click(object sender, RoutedEventArgs e)
    {
        _showSelectionBounds = !_showSelectionBounds;
        UpdateViewportModeButtons();
        StatusText.Text = _showSelectionBounds
            ? "Границы выделения включены"
            : "Границы выделения выключены";
    }

    private void UpdateViewportModeButtons()
    {
        if (ProjectionModeButton is null)
            return;
        ProjectionModeButton.Content = _sceneCamera is OrthographicCamera
            ? "Orthographic"
            : "Perspective";
        RenderModeButton.Content = _sceneRenderMode.ToString();
        SetViewportModeButtonState(GridVisibilityButton, _showFloorGrid);
        SetViewportModeButtonState(BoundsVisibilityButton, _showSelectionBounds);
    }

    private static void SetViewportModeButtonState(Button button, bool active)
    {
        button.Foreground = new SolidColorBrush(active
            ? Color.FromRgb(244, 181, 91)
            : Color.FromRgb(150, 163, 179));
        button.BorderBrush = new SolidColorBrush(active
            ? Color.FromRgb(169, 100, 27)
            : Color.FromRgb(47, 59, 73));
    }

    private void GpuViewport_OnRender(TimeSpan delta)
    {
        if (!_gpuRendererAvailable)
            return;

        try
        {
            UpdateCameraNavigation(delta);
            DpiScale dpi = VisualTreeHelper.GetDpi(GpuViewport);
            int width = Math.Max(
                1,
                (int)Math.Round(GpuViewport.ActualWidth * dpi.DpiScaleX));
            int height = Math.Max(
                1,
                (int)Math.Round(GpuViewport.ActualHeight * dpi.DpiScaleY));
            IReadOnlyList<SmoEditableCollision> visibleCollisions =
                _showCollisions && _document is not null
                    ? GetVisibleCollisions()
                    : Array.Empty<SmoEditableCollision>();
            CameraBounds? selectionBounds =
                _showSelectionBounds &&
                TryCalculateSelectionBounds(out CameraBounds bounds)
                    ? bounds
                    : null;
            Action? renderOverlay = visibleCollisions.Count == 0 &&
                                    selectionBounds is null
                ? null
                : () =>
                {
                    if (visibleCollisions.Count > 0)
                    {
                        _collisionOverlayRenderer.Render(
                            width,
                            height,
                            _sceneCamera,
                            visibleCollisions.Select(collision =>
                                new SmoCollisionOverlayItem(
                                    collision.Source.Positions,
                                    collision.Source.TriangleIndices,
                                    collision.WorldTransform,
                                    IsPartOfTransformSelection(
                                        collision.Entity.Id))));
                    }
                    if (selectionBounds is CameraBounds selected)
                    {
                        _boundsOverlayRenderer.Render(
                            width,
                            height,
                            _sceneCamera,
                            selected.Minimum,
                            selected.Maximum);
                    }
                };
            _gpuRenderer.Render(
                width,
                height,
                _sceneCamera,
                Color.FromRgb(13, 19, 26),
                0,
                _sceneCenter,
                _showFloorGrid,
                Color.FromRgb(49, 63, 78),
                _sceneFloorY,
                _sceneGridSpan,
                _sceneDiagonal,
                renderOverlay);
            RefreshScenePickingFromGpu();
            _viewportDpiX = dpi.DpiScaleX;
            _viewportDpiY = dpi.DpiScaleY;
            if (TryGetGizmoPivot(out Point3D gizmoPivot))
            {
                NumericsQuaternion orientation = GetGizmoOrientation();
                _gizmoLayout = _editorTool is "Перемещение" or "Масштаб"
                    ? _translationGizmoRenderer.CreateLayout(
                        _sceneCamera,
                        width,
                        height,
                        gizmoPivot,
                        orientation,
                        includeUniform: _editorTool == "Масштаб")
                    : null;
                _rotationGizmoLayout = _editorTool == "Вращение"
                    ? _rotationGizmoRenderer.CreateLayout(
                        _sceneCamera,
                        width,
                        height,
                        gizmoPivot,
                        orientation)
                    : null;
            }
            else
            {
                _gizmoLayout = null;
                _rotationGizmoLayout = null;
            }
            _translationGizmoRenderer.Render(
                width,
                height,
                _gizmoLayout,
                _hoverGizmoAxis,
                _activeGizmoAxis);
            _rotationGizmoRenderer.Render(
                width,
                height,
                _rotationGizmoLayout,
                _hoverGizmoAxis,
                _activeGizmoAxis);

            if (_gpuRenderer.ConsumeUploadReport() is GpuUploadReport upload &&
                DateTime.UtcNow >= _viewportActionStatusUntilUtc)
            {
                StatusText.Text =
                    $"GPU · {upload.GeometryCount:N0} mesh buffers · " +
                    $"{upload.TextureCount:N0} textures · " +
                    $"{upload.PlacementCount:N0} placements · " +
                    $"{upload.ElapsedMilliseconds:F1} ms";
            }
        }
        catch (Exception exception)
        {
            DisableGpuViewport(exception.Message);
        }
    }

    private void DisableGpuViewport(string reason)
    {
        _gpuRendererAvailable = false;
        ClearScenePickingPositions();
        UpdateScenePickIndex(_scenePickMeshes);
        _gpuViewportFailureReason = reason;
        if (GpuViewport is not null)
            GpuViewport.Visibility = Visibility.Collapsed;
        if (ViewportPlaceholder is not null)
            ViewportPlaceholder.Visibility = Visibility.Visible;
        if (ViewportEmptyTitleText is not null)
            ViewportEmptyTitleText.Text = "Viewport недоступен";
        if (ViewportStateText is not null)
            ViewportStateText.Text = $"GPU viewport недоступен.\n{reason}";
    }

    private void ResetRenderScene()
    {
        _gpuRenderer.Clear();
        _highlightedRenderKeys.Clear();
        _selectedEntities.Clear();
        _hiddenEntities.Clear();
        _pendingPlacementByScene.Clear();
        _selectedPendingPlacement = null;
        _activeSelectionKey = null;
        _activeCollisionEntityIndex = null;
        _collisionRegionByInfo.Clear();
        _scenePickIndex = null;
        _scenePickMeshes = [];
        ClearScenePickingPositions();
        ReportScenePickingIssues();
        _cameraOrbitingFocus = false;
        _cameraFocusKey = null;
        _cameraMovementKeys.Clear();
        _cameraVelocity = default;
        _cameraDesiredDistance = _cameraDistance;
        _renderPreparedScene = null;
        _renderedModelResourceSignature = null;
        if (ViewportPlaceholder is not null)
            ViewportPlaceholder.Visibility = Visibility.Visible;
    }

    private void LoadRenderScene(
        SmoPreparedScene scene,
        bool preserveEditorSelection = false)
    {
        bool preserveCamera = preserveEditorSelection &&
            _renderPreparedScene is not null;
        _gpuRenderer.Clear();
        ClearScenePickingPositions();
        _highlightedRenderKeys.Clear();
        if (!preserveEditorSelection)
        {
            _selectedEntities.Clear();
            _hiddenEntities.Clear();
            _activeSelectionKey = null;
            _activeCollisionEntityIndex = null;
            _collisionRegionByInfo.Clear();
        }
        _renderPreparedScene = scene;
        UpdateScenePickIndex(scene.Meshes);

        int index = 0;
        foreach (SmoSceneMesh mesh in scene.Meshes)
        {
            _gpuRenderer.Add(
                mesh,
                new SmoRenderObjectKey(0, mesh.SceneObjectIndex),
                PaletteColor(index++));
        }

        if (TryCalculateBounds(scene.Meshes, out CameraBounds sceneBounds))
        {
            _sceneCenter = sceneBounds.Center;
            _sceneFloorY = sceneBounds.Minimum.Y;
            _sceneDiagonal = sceneBounds.Diagonal;
            _sceneGridSpan = Math.Max(
                Math.Max(sceneBounds.Size.X, sceneBounds.Size.Z),
                _sceneDiagonal * 0.35);
            if (preserveCamera)
                UpdateCamera();
            else
                FrameBounds(sceneBounds, orbit: false);
        }
        ViewportPlaceholder.Visibility =
            _gpuRendererAvailable && scene.Meshes.Count > 0
                ? Visibility.Collapsed
                : Visibility.Visible;
        if (scene.Meshes.Count == 0)
        {
            ViewportEmptyTitleText.Text = "Нет отображаемой геометрии";
            ViewportStateText.Text =
                "SMO-файл открыт, но общая сцена не нашла моделей для отображения.";
        }
        ApplyPlacementHighlights();
    }

    private void LevelDocument_Changed(object? sender, EventArgs e)
    {
        if (_document is null || _workspace is null)
            return;

        RefreshPendingModelResourcesIfNeeded();
        SmoPreparedScene renderScene = _renderPreparedScene ??
            _workspace.PreparedScene;
        SmoSceneMesh[] effectiveMeshes = renderScene.Meshes
            .Select(mesh =>
            {
                if (_pendingPlacementByScene.TryGetValue(
                        mesh.SceneObjectIndex,
                        out PendingPlacementSelection? pending))
                {
                    if (pending.External &&
                        _document.ExternalPlacements.TryGetValue(
                            pending.Id,
                            out SmoLevelExternalPlacement? externalPlacement))
                    {
                        return mesh with
                        {
                            WorldTransform = externalPlacement.WorldTransform
                        };
                    }
                    if (!pending.External &&
                        pending.PlacementId is Guid placementId &&
                        _document.PlacementAdditions.TryGetValue(
                            placementId,
                            out SmoLevelPlacementAddition? addition))
                    {
                        return mesh with
                        {
                            WorldTransform = addition.WorldTransform
                        };
                    }
                }
                var id = new SmoPlacementId(
                    mesh.Mesh.ObjectIndex,
                    mesh.SceneObjectIndex, mesh.OccurrenceKey);
                return _document.TryGetPlacement(
                    id,
                    out SmoEditablePlacement? placement)
                    ? mesh with { WorldTransform = placement!.WorldTransform }
                    : mesh;
            })
            .ToArray();
        foreach (IGrouping<int, SmoSceneMesh> placement in effectiveMeshes
                     .GroupBy(mesh => mesh.SceneObjectIndex))
        {
            _gpuRenderer.SetModelTransform(
                new SmoRenderObjectKey(0, placement.Key),
                placement.First().WorldTransform);
        }
        UpdateScenePickIndex(effectiveMeshes.Where(IsSceneMeshVisible));
        UpdateDocumentCaption();
        if (!_document.HasActiveTransformSession)
            RefreshCatalogResourceEdits();

        if (TryResolveActiveSelection(
                out EditorAssetItem? activeItem,
                out _,
                out int placementIndex))
        {
            UpdateInspector(
                activeItem!,
                placementIndex,
                updateViewportSelection: false);
            UpdateSelectionCaption(activeItem!.Name, placementIndex);
        }
        else if (TryResolveActiveCollision(out SmoEditableCollision? collision))
        {
            UpdateCollisionInspector(collision!);
            UpdateCollisionSelectionCaption(collision!.Entity);
        }
        else if (_selectedPendingPlacement is PendingPlacementSelection pending)
        {
            UpdatePendingPlacementInspector(pending);
        }
    }

    private void RebuildScenePickIndex()
    {
        if (_document is null || _workspace is null)
        {
            _scenePickIndex = null;
            _scenePickMeshes = [];
            ClearScenePickingPositions();
            return;
        }
        SmoPreparedScene scene = _renderPreparedScene ?? _workspace.PreparedScene;
        UpdateScenePickIndex(scene.Meshes
            .Where(IsSceneMeshVisible)
            .Select(mesh =>
            {
                var id = new SmoPlacementId(
                    mesh.Mesh.ObjectIndex,
                    mesh.SceneObjectIndex, mesh.OccurrenceKey);
                return _document.TryGetPlacement(
                    id,
                    out SmoEditablePlacement? placement)
                    ? mesh with { WorldTransform = placement!.WorldTransform }
                    : ApplyPendingPlacementTransform(mesh);
            }));
    }

    private void ClearScenePickingPositions()
    {
        _scenePickingPositions.Clear();
        _scenePickingReadbackIssues.Clear();
        _scenePickingNeedsGpuRefresh = false;
    }

    private static (SmoRenderObjectKey Key, int MeshObjectIndex) ScenePickingKey(SmoSceneMesh mesh) =>
        (new SmoRenderObjectKey(0, mesh.SceneObjectIndex), mesh.Mesh.ObjectIndex);

    private Vector3[]? CachedScenePickingPositions(SmoSceneMesh mesh) =>
        _scenePickingPositions.TryGetValue(ScenePickingKey(mesh), out Vector3[]? positions)
            ? positions
            : null;

    private void UpdateScenePickIndex(IEnumerable<SmoSceneMesh> meshes)
    {
        _scenePickMeshes = meshes.ToArray();
        _scenePickIndex = new SmoScenePickIndex(_scenePickMeshes, CachedScenePickingPositions);
        _scenePickingNeedsGpuRefresh = _scenePickMeshes.Any(mesh =>
            mesh.Mesh.HasSkinningData && !_scenePickingPositions.ContainsKey(ScenePickingKey(mesh)));
        ReportScenePickingIssues();
    }

    private void RefreshScenePickingFromGpu()
    {
        if (!_scenePickingNeedsGpuRefresh)
            return;
        _scenePickingNeedsGpuRefresh = false;
        // This host supplies initial palettes at Add and does not animate them.
        // Local captures survive world/visibility edits, but every Clear/Load
        // invalidates them. Any future palette setter must invalidate them too.
        foreach (SmoSceneMesh mesh in _scenePickMeshes.Where(mesh => mesh.Mesh.HasSkinningData))
        {
            var key = ScenePickingKey(mesh);
            if (_scenePickingPositions.ContainsKey(key))
                continue;
            try
            {
                _scenePickingPositions.Add(key,
                    _gpuRenderer.ReadPositionsForPicking(key.Key, key.MeshObjectIndex));
            }
            catch (Exception exception)
            {
                // Remember a failed attempt for this scene too; do not repeat
                // expensive or ambiguous captures each frame or gizmo update.
                _scenePickingPositions.Add(key, null);
                _scenePickingReadbackIssues.Add(
                    $"GPU picking object [{mesh.SceneObjectIndex}], mesh [{mesh.Mesh.ObjectIndex}]: {exception.Message}");
            }
        }
        _scenePickIndex = new SmoScenePickIndex(_scenePickMeshes, CachedScenePickingPositions);
        ReportScenePickingIssues();
    }

    private void ReportScenePickingIssues()
    {
        if (_scenePickIndex is not { Issues.Count: > 0 } index)
        {
            if (_scenePickingDiagnosticText is not null &&
                DiagnosticsText.Text == _scenePickingDiagnosticText)
            {
                DiagnosticsText.Text = "Позиции для выбора под курсором готовы.";
                DiagnosticsText.Foreground = HealthyBrush;
            }
            if (StatusText.Text is
                "Скелетный picking ожидает GPU readback · выбор через каталог доступен" or
                "Скелетный picking недоступен · выбор через каталог доступен")
            {
                StatusText.Text = "Позиции для выбора под курсором готовы";
            }
            _scenePickingDiagnosticText = null;
            return;
        }
        // Synchronous click/drop operations use the last completed scene
        // snapshot. Until its first Render, missing captures stay explicit.
        _scenePickingDiagnosticText = string.Join("\n", index.Issues.Concat(_scenePickingReadbackIssues)) +
            "\nСкелетные поверхности исключены из выбора и размещения под курсором; " +
            "выбор через каталог остаётся доступен.";
        DiagnosticsText.Text = _scenePickingDiagnosticText;
        DiagnosticsText.Foreground = NoticeBrush;
        StatusText.Text = _gpuRendererAvailable && _scenePickingNeedsGpuRefresh
            ? "Скелетный picking ожидает GPU readback · выбор через каталог доступен"
            : "Скелетный picking недоступен · выбор через каталог доступен";
        _viewportActionStatusUntilUtc = DateTime.UtcNow.AddSeconds(3);
    }

    private bool IsSceneMeshVisible(SmoSceneMesh mesh)
    {
        if (_document is null)
            return true;
        if (_pendingPlacementByScene.ContainsKey(mesh.SceneObjectIndex))
            return true;
        return !_document.TryGetPlacement(
                   new SmoPlacementId(
                       mesh.Mesh.ObjectIndex,
                       mesh.SceneObjectIndex, mesh.OccurrenceKey),
                   out SmoEditablePlacement? placement) ||
               (!_hiddenEntities.Contains(placement!.Entity.Id) &&
                !_document.RemovedEntityIds.Contains(placement.Entity.Id));
    }

    private void RefreshPendingModelResourcesIfNeeded()
    {
        if (_document is null || _workspace is null)
            return;
        string signature = string.Join(
            '|',
            _document.ModelReplacements.Values
                .OrderBy(item => item.MeshObjectIndex)
                .Select(item =>
                    $"{item.MeshObjectIndex}:{item.SourcePath}:{item.Transform}:" +
                    $"{item.ReferenceWorldTransform.GetHashCode()}")) +
            "#" + string.Join(
                '|',
                _document.PlacementAdditions.Values
                    .OrderBy(item => item.Id)
                    .Select(item =>
                        $"{item.Id}:{item.MeshObjectIndex}"));
        signature += "#external:" + string.Join('|',
            _document.ExternalModels.Values.OrderBy(item => item.Id).Select(item =>
                $"{item.Id}:{item.SourcePath}:{item.ImportedScene.Meshes.Count}"));
        signature += "#external-placements:" + string.Join('|',
            _document.ExternalPlacements.Values.OrderBy(item => item.Id).Select(item =>
                $"{item.Id}:{item.ModelId}"));
        signature += "#removed:" + string.Join(',',
            _document.RemovedEntityIds.OrderBy(id => id.SceneObjectIndex)
                .Select(id => id.SceneObjectIndex));
        if (string.Equals(
                signature,
                _renderedModelResourceSignature,
                StringComparison.Ordinal))
            return;
        try
        {
            var replacementMeshes = new Dictionary<int,
                (SmoMesh Mesh, SmoTexture? Texture,
                 SmoMaterialRenderStateInfo? MaterialState)>();
            foreach (SmoLevelModelReplacement replacement in
                     _document.ModelReplacements.Values)
            {
                var previewTextures = new Dictionary<int, SmoTexture>();
                SmoLevelModelGraphPlan plan =
                    SmoLevelModelGraphReplacer.ResolvePlan(
                        _workspace.Document,
                        replacement.MeshObjectIndex);
                if (plan.Components.Count != replacement.ImportedScene.Meshes.Count)
                    throw new InvalidOperationException(
                        "Pending full-model replacement has a stale component mapping.");
                for (int index = 0; index < plan.Components.Count; index++)
                {
                    SmoLevelModelGraphComponent component = plan.Components[index];
                    ImportedMesh importedMesh = replacement.ImportedScene.Meshes[index];
                    var onePart = new ImportedScene(
                        [importedMesh],
                        replacement.ImportedScene.Textures,
                        replacement.ImportedScene.Materials);
                    SmoMesh previewMesh = SmoMeshResourceReplacer.CreatePreviewMesh(
                        _workspace.Document,
                        component.MeshObjectIndex,
                        onePart,
                        replacement.Transform,
                        referenceWorldTransform:
                            replacement.ReferenceWorldTransform);
                    SmoTexture? previewTexture = ResolveImportedPreviewTexture(
                        replacement.ImportedScene,
                        importedMesh,
                        component.MeshObjectIndex,
                        previewTextures);
                    SmoMaterialRenderStateInfo? materialState =
                        SmoLevelModelGraphReplacer.CreatePreviewMaterialState(
                            replacement.ImportedScene,
                            importedMesh,
                            previewMesh,
                            previewTexture);
                    replacementMeshes[component.MeshObjectIndex] =
                        (previewMesh, previewTexture, materialState);
                }
            }
            var meshes = _workspace.PreparedScene.Meshes
                .Where(mesh => !_document.TryGetPlacement(
                    new SmoPlacementId(mesh.Mesh.ObjectIndex, mesh.SceneObjectIndex, mesh.OccurrenceKey),
                    out SmoEditablePlacement? sourcePlacement) ||
                    !_document.RemovedEntityIds.Contains(sourcePlacement!.Entity.Id))
                .Select(mesh => replacementMeshes.TryGetValue(
                        mesh.Mesh.ObjectIndex,
                        out (SmoMesh Mesh, SmoTexture? Texture,
                             SmoMaterialRenderStateInfo? MaterialState) replacementPart)
                    ? mesh with
                    {
                        Mesh = replacementPart.Mesh,
                        Texture = replacementPart.Texture,
                        UsesAlphaBlend = replacementPart.MaterialState?.UsesAlphaBlend ??
                                         mesh.UsesAlphaBlend,
                        MaterialRenderState = replacementPart.MaterialState ??
                                              mesh.MaterialRenderState,
                        AnimationFrames = null,
                        AnimationFrameDuration = null,
                        BaseTexture = null
                    }
                    : mesh)
                .ToList();
            int additionOrdinal = 0;
            _pendingPlacementByScene.Clear();
            foreach (SmoLevelPlacementAddition addition in
                     _document.PlacementAdditions.Values.OrderBy(item => item.Id))
            {
                SmoSceneMesh template = meshes.First(mesh =>
                    mesh.Mesh.ObjectIndex == addition.MeshObjectIndex);
                int sceneIndex = int.MaxValue - additionOrdinal++;
                meshes.Add(template with
                {
                    SceneObjectIndex = sceneIndex,
                    SharedInstance = null,
                    WorldTransform = addition.WorldTransform,
                    SkinObjectIndex = null,
                    InitialSkinMatrices = null,
                    RigidNodeObjectIndex = null,
                    BoneInfluences = new Dictionary<int, float>()
                });
                _pendingPlacementByScene[sceneIndex] = new PendingPlacementSelection(
                    addition.GroupId,
                    External: false,
                    sceneIndex,
                    addition.Name,
                    addition.Id);
            }
            foreach (SmoLevelExternalPlacement placement in
                     _document.ExternalPlacements.Values.OrderBy(item => item.Id))
            {
                SmoLevelExternalModel model = _document.ExternalModels[placement.ModelId];
                foreach (SmoSceneMesh part in GetExternalPreviewMeshes(model))
                {
                    int sceneIndex = int.MaxValue - additionOrdinal++;
                    meshes.Add(part with
                    {
                        SceneObjectIndex = sceneIndex,
                        WorldTransform = placement.WorldTransform,
                        SharedInstance = null,
                        SkinObjectIndex = null,
                        InitialSkinMatrices = null,
                        RigidNodeObjectIndex = null,
                        BoneInfluences = new Dictionary<int, float>()
                    });
                    _pendingPlacementByScene[sceneIndex] = new PendingPlacementSelection(
                        placement.Id, External: true, sceneIndex, placement.Name);
                }
            }
            SmoPreparedScene scene = _workspace.PreparedScene with
            {
                TotalMeshCount = meshes.Count,
                Meshes = meshes
            };
            _renderedModelResourceSignature = signature;
            LoadRenderScene(scene, preserveEditorSelection: true);
            if (_selectedEntities.Count > 0 || _selectedPendingPlacement is not null)
                ApplyPlacementHighlights();
            if (replacementMeshes.Count > 0 || additionOrdinal > 0)
            {
                StatusText.Text =
                    "Геометрия сцены обновлена в памяти · SMO не перепаковывался";
            }
        }
        catch (Exception exception)
        {
            _renderedModelResourceSignature = null;
            StatusText.Text = $"Не удалось обновить геометрию: {exception.Message}";
        }
    }

    private static SmoTexture? ResolveImportedPreviewTexture(
        ImportedScene scene,
        ImportedMesh mesh,
        int objectIndex,
        IDictionary<int, SmoTexture>? cache = null)
    {
        if (mesh.MaterialIndex < 0 || mesh.MaterialIndex >= scene.Materials.Count)
            return null;
        int textureIndex = scene.Materials[mesh.MaterialIndex].BaseColorTextureIndex;
        if (textureIndex < 0 || textureIndex >= scene.Textures.Count)
            return null;
        if (cache is not null && cache.TryGetValue(
                textureIndex, out SmoTexture? cached))
        {
            return cached;
        }
        SmoTexture texture = SmoLevelModelGraphReplacer.CreatePreviewTexture(
            scene.Textures[textureIndex],
            objectIndex);
        cache?.Add(textureIndex, texture);
        return texture;
    }

    private static Color PaletteColor(int index)
    {
        Color[] colors =
        [
            Color.FromRgb(126, 174, 216),
            Color.FromRgb(165, 190, 132),
            Color.FromRgb(206, 157, 109),
            Color.FromRgb(170, 143, 202),
            Color.FromRgb(112, 190, 183)
        ];
        return colors[index % colors.Length];
    }

    private void ReplacePlacementSelection(
        SmoLevelAsset asset,
        int placementIndex,
        bool expandComposite = true)
    {
        if (asset.Placements.Count == 0)
            return;
        _activePlacementIndex = Math.Clamp(
            placementIndex,
            0,
            asset.Placements.Count - 1);
        SmoLevelPlacement placement = asset.Placements[_activePlacementIndex];
        var selectionKey = new PlacementSelectionKey(
            asset.ObjectIndex,
            placement.SceneObjectIndex, placement.OccurrenceKey);
        if (_document is null ||
            !_document.TryGetPlacement(
                new SmoPlacementId(asset.ObjectIndex, placement.SceneObjectIndex, placement.OccurrenceKey),
                out SmoEditablePlacement? editablePlacement))
        {
            return;
        }
        _selectedEntities.Clear();
        IReadOnlyList<SmoLevelEntityId> selected = expandComposite
            ? _document.ExpandCompositeEntities([editablePlacement!.Entity.Id])
            : [editablePlacement!.Entity.Id];
        _selectedEntities.UnionWith(selected);
        _activeSelectionKey = selectionKey;
        _activeCollisionEntityIndex = null;
        ApplyPlacementHighlights();
        if (expandComposite)
        {
            FocusPlacementOrComposite(
                editablePlacement.Entity,
                asset,
                _activePlacementIndex);
        }
        else
        {
            FocusPlacement(asset, _activePlacementIndex);
        }
        UpdateSelectionCaption(asset.DisplayName, _activePlacementIndex);
    }

    private bool TogglePlacementSelection(
        SmoLevelAsset asset,
        int placementIndex,
        bool expandComposite = true)
    {
        if (asset.Placements.Count == 0)
            return false;
        placementIndex = Math.Clamp(
            placementIndex,
            0,
            asset.Placements.Count - 1);
        SmoLevelPlacement placement = asset.Placements[placementIndex];
        var selectionKey = new PlacementSelectionKey(
            asset.ObjectIndex,
            placement.SceneObjectIndex, placement.OccurrenceKey);
        if (_document is null ||
            !_document.TryGetPlacement(
                new SmoPlacementId(asset.ObjectIndex, placement.SceneObjectIndex, placement.OccurrenceKey),
                out SmoEditablePlacement? editablePlacement))
        {
            return false;
        }
        SmoLevelEntityId entityId = editablePlacement!.Entity.Id;
        IReadOnlyList<SmoLevelEntityId> selectionGroup = expandComposite
            ? _document.ExpandCompositeEntities([entityId])
            : [entityId];
        bool selected;
        if (selectionGroup.All(_selectedEntities.Contains))
        {
            selected = false;
            foreach (SmoLevelEntityId id in selectionGroup)
                _selectedEntities.Remove(id);
            ActivateFirstSelectedEntity();
        }
        else
        {
            foreach (SmoLevelEntityId id in selectionGroup)
                _selectedEntities.Add(id);
            _activeSelectionKey = selectionKey;
            _activeCollisionEntityIndex = null;
            _activePlacementIndex = placementIndex;
            selected = true;
        }

        if (_selectedEntities.Count == 0)
        {
            _activeSelectionKey = null;
            _activeCollisionEntityIndex = null;
        }
        ApplyPlacementHighlights();
        return selected;
    }

    private void FocusPlacementOrComposite(
        SmoLevelEntity entity,
        SmoLevelAsset asset,
        int placementIndex)
    {
        if (_document is not null &&
            _workspace is not null &&
            _document.TryGetCompositeModel(entity.Id, out SmoCompositeModel? composite))
        {
            HashSet<(int Mesh, int Scene)> parts = composite!.Parts
                .Select(part => (
                    part.Asset.ObjectIndex,
                    part.Source.SceneObjectIndex))
                .ToHashSet();
            FrameMeshes((_renderPreparedScene ?? _workspace.PreparedScene).Meshes.Where(mesh =>
                parts.Contains((mesh.Mesh.ObjectIndex, mesh.SceneObjectIndex))));
            return;
        }
        FocusPlacement(asset, placementIndex);
    }

    private void ApplyPlacementHighlights()
    {
        _highlightedRenderKeys.Clear();

        if (_document is null)
        {
            UpdateCommandAvailability();
            return;
        }
        foreach (SmoLevelEntity entity in _document.Entities)
        {
            bool visible = !_hiddenEntities.Contains(entity.Id) &&
                           !_document.RemovedEntityIds.Contains(entity.Id);
            foreach (SmoRenderObjectKey key in entity.Parts
                         .Select(part => new SmoRenderObjectKey(
                             0,
                             part.Source.SceneObjectIndex))
                         .Distinct())
            {
                _gpuRenderer.SetAppearance(key, visible, false, 1);
            }
        }
        IReadOnlyList<SmoLevelEntityId> highlightedEntities =
            LinkedCollisionMovementToggle?.IsChecked == true
                ? _document.ExpandLinkedEntities(_selectedEntities)
                : _selectedEntities.ToArray();
        foreach (SmoRenderObjectKey key in highlightedEntities
                     .Where(id => !_hiddenEntities.Contains(id))
                     .SelectMany(selection => _document.GetEntity(selection).Parts)
                     .Select(part => new SmoRenderObjectKey(
                         0,
                         part.Source.SceneObjectIndex))
                     .Distinct())
        {
            _gpuRenderer.SetAppearance(key, true, true, 1);
            _highlightedRenderKeys.Add(key);
        }
        if (_selectedPendingPlacement is PendingPlacementSelection pending)
        {
            foreach ((int sceneIndex, PendingPlacementSelection candidate) in
                     _pendingPlacementByScene.Where(pair => pair.Value.Id == pending.Id &&
                         pair.Value.External == pending.External))
            {
                var key = new SmoRenderObjectKey(0, sceneIndex);
                _gpuRenderer.SetAppearance(key, true, true, 1);
                _highlightedRenderKeys.Add(key);
            }
        }
        RebuildScenePickIndex();
        UpdateCommandAvailability();
    }

    private void UpdateSelectionCaption(string activeName, int placementIndex)
    {
        if (_document is not null &&
            _activeSelectionKey is PlacementSelectionKey active &&
            _document.TryGetPlacement(
                new SmoPlacementId(active.MeshObjectIndex, active.SceneObjectIndex, active.OccurrenceKey),
                out SmoEditablePlacement? placement) &&
            _document.TryGetCompositeModel(
                placement!.Entity.Id,
                out SmoCompositeModel? composite) &&
            composite!.Entities.All(entity => _selectedEntities.Contains(entity.Id)) &&
            _selectedEntities.Count == composite.Entities.Count)
        {
            ViewportSelectionText.Text =
                $"SELECTED  ·  {composite.Name}  ·  {composite.Parts.Count:N0} parts";
            return;
        }
        ViewportSelectionText.Text = _selectedEntities.Count > 1
            ? $"SELECTED  ·  {_selectedEntities.Count:N0} entities  ·  " +
              $"active {activeName} #{placementIndex + 1}"
            : $"SELECTED  ·  {activeName}  ·  placement {placementIndex + 1}";
    }

    private PlacementSelectionKey? ResolveFirstSelectedPlacementKey()
    {
        if (_selectedEntities.Count == 0)
            return null;
        if (_document is null)
            return null;
        SmoLevelEntity entity = _document.GetEntity(_selectedEntities.First());
        foreach (SmoEditablePlacement part in entity.Parts)
        {
            return new PlacementSelectionKey(
                part.Asset.ObjectIndex,
                part.Source.SceneObjectIndex, part.Source.OccurrenceKey);
        }
        return null;
    }

    private void ActivateFirstSelectedEntity()
    {
        _activeSelectionKey = null;
        _activeCollisionEntityIndex = null;
        if (_document is null)
            return;
        foreach (SmoLevelEntityId id in _selectedEntities)
        {
            SmoLevelEntity entity = _document.GetEntity(id);
            if (entity.Parts.FirstOrDefault() is SmoEditablePlacement placement)
            {
                _activeSelectionKey = new PlacementSelectionKey(
                    placement.Asset.ObjectIndex,
                    placement.Source.SceneObjectIndex, placement.Source.OccurrenceKey);
                return;
            }
            if (entity.Collisions.Count > 0)
            {
                _activeCollisionEntityIndex = entity.Id.SceneObjectIndex;
                return;
            }
        }
    }

    private bool TryGetGizmoPivot(out Point3D pivot)
    {
        pivot = default;
        if (_editorTool is not ("Перемещение" or "Вращение" or "Масштаб") ||
            _document is null ||
            _workspace is null ||
            (_selectedEntities.Count == 0 && _selectedPendingPlacement is null))
        {
            return false;
        }

        SmoSceneMesh[] selectedMeshes = _selectedPendingPlacement is
            PendingPlacementSelection pending
            ? (_renderPreparedScene ?? _workspace.PreparedScene).Meshes
                .Where(mesh =>
                    _pendingPlacementByScene.TryGetValue(
                        mesh.SceneObjectIndex,
                        out PendingPlacementSelection? candidate) &&
                    candidate.Id == pending.Id &&
                    candidate.External == pending.External)
                .Select(ApplyPendingPlacementTransform)
                .ToArray()
            : (_renderPreparedScene ?? _workspace.PreparedScene).Meshes
                .Where(mesh =>
                {
                    var placementId = new SmoPlacementId(
                        mesh.Mesh.ObjectIndex,
                        mesh.SceneObjectIndex, mesh.OccurrenceKey);
                    return _document.TryGetPlacement(
                               placementId,
                               out SmoEditablePlacement? placement) &&
                           _selectedEntities.Contains(placement!.Entity.Id);
                })
                .Select(mesh =>
                {
                    var placementId = new SmoPlacementId(
                        mesh.Mesh.ObjectIndex,
                        mesh.SceneObjectIndex, mesh.OccurrenceKey);
                    return _document.TryGetPlacement(
                        placementId,
                        out SmoEditablePlacement? placement)
                        ? mesh with { WorldTransform = placement!.WorldTransform }
                        : mesh;
                })
                .ToArray();
        bool hasBounds = TryCalculateBounds(selectedMeshes, out CameraBounds bounds);
        foreach (SmoEditableCollision collision in _document.Collisions.Where(collision =>
                     _selectedEntities.Contains(collision.Entity.Id)))
        {
            foreach (Vector3 position in collision.Source.Positions)
            {
                Vector3 world = Vector3.Transform(position, collision.WorldTransform);
                world.Z = -world.Z;
                bounds = hasBounds
                    ? bounds.Include(world)
                    : new CameraBounds(world, world);
                hasBounds = true;
            }
        }
        if (!hasBounds)
            return false;
        pivot = bounds.Center;
        return true;
    }

    private NumericsQuaternion GetGizmoOrientation()
    {
        if (LocalTransformSpaceButton?.IsChecked != true || _document is null)
            return NumericsQuaternion.Identity;

        if (_selectedPendingPlacement is PendingPlacementSelection pending &&
            TryGetPendingPlacementTransform(pending, out Matrix4x4 pendingTransform))
        {
            return Matrix4x4.Decompose(
                pendingTransform,
                out _,
                out NumericsQuaternion pendingRotation,
                out _)
                ? NumericsQuaternion.Normalize(pendingRotation)
                : NumericsQuaternion.Identity;
        }

        SmoLevelEntity? entity = null;
        if (_activeCollisionEntityIndex is int collisionIndex &&
            _document.TryGetEntity(
                new SmoLevelEntityId(collisionIndex),
                out SmoLevelEntity? collisionEntity))
        {
            entity = collisionEntity;
        }
        else if (_activeSelectionKey is PlacementSelectionKey selection &&
                 _document.TryGetPlacement(
                     new SmoPlacementId(
                         selection.MeshObjectIndex,
                         selection.SceneObjectIndex, selection.OccurrenceKey),
                     out SmoEditablePlacement? placement))
        {
            entity = placement!.Entity;
        }
        entity ??= _selectedEntities.Count > 0
            ? _document.GetEntity(_selectedEntities.First())
            : null;
        if (entity is null ||
            !Matrix4x4.Decompose(
                entity.WorldTransform,
                out _,
                out NumericsQuaternion rotation,
                out _))
        {
            return NumericsQuaternion.Identity;
        }
        return NumericsQuaternion.Normalize(rotation);
    }

    private bool BeginGizmoDrag(SmoGizmoAxis axis, Point point)
    {
        bool hasLayout = _editorTool == "Вращение"
            ? _rotationGizmoLayout is not null
            : _gizmoLayout is not null;
        if (_document is null || !hasLayout ||
            (_selectedEntities.Count == 0 && _selectedPendingPlacement is null))
        {
            return false;
        }

        try
        {
            if (_selectedPendingPlacement is PendingPlacementSelection pending)
            {
                _pendingGizmoTransformSession =
                    _document.BeginPendingPlacementTransform(
                        pending.Id,
                        pending.External);
            }
            else
            {
                IReadOnlyList<SmoLevelEntityId> transformSelection =
                    GetTransformSelection();
                if (HasProject &&
                    !TryCaptureProjectTransforms(
                        transformSelection,
                        out _,
                        out string projectReason))
                {
                    StatusText.Text = projectReason;
                    return false;
                }
                if (!HasProject)
                {
                    SmoLevelEntity[] blocked = transformSelection
                        .Where(id => !_document.CanPersistTransform(id))
                        .Select(_document.GetEntity)
                        .ToArray();
                    if (blocked.Length > 0)
                    {
                        StatusText.Text =
                            "Нельзя изменить SAVE BLOCKED объект: " +
                            string.Join(", ", blocked.Select(entity =>
                                $"{entity.Name} [{entity.Id.SceneObjectIndex}]"));
                        return false;
                    }
                }
                _gizmoTransformSession = _document.BeginTransform(
                    transformSelection);
            }
            _gizmoDragLayout = _gizmoLayout;
            _rotationGizmoDragLayout = _rotationGizmoLayout;
            _gizmoDragStart = point;
            _activeGizmoAxis = axis;
            _hoverGizmoAxis = axis;
            ViewportSurface.Focus();
            ViewportSurface.CaptureMouse();
            ViewportSurface.Cursor = Cursors.Cross;
            return true;
        }
        catch (InvalidOperationException exception)
        {
            StatusText.Text = exception.Message;
            return false;
        }
    }

    private void CommitGizmoDrag()
    {
        if (!HasActiveGizmoTransform)
            return;
        SmoGizmoAxis axis = _activeGizmoAxis;
        int count = _pendingGizmoTransformSession?.PlacementIds.Count ??
            _gizmoTransformSession!.EntityIds.Count;
        Vector3 delta = _pendingGizmoTransformSession?.TranslationDelta ??
            _gizmoTransformSession!.TranslationDelta;
        float angle = _pendingGizmoTransformSession?.RotationRadians ??
            _gizmoTransformSession!.RotationRadians;
        Vector3 scale = _pendingGizmoTransformSession?.ScaleFactors ??
            _gizmoTransformSession!.ScaleFactors;
        SmoLevelEntityId[] projectEntityIds =
            HasProject && _gizmoTransformSession is not null
                ? _gizmoTransformSession.EntityIds.ToArray()
                : [];
        string operation = _editorTool switch
        {
            "Вращение" => "Rotate",
            "Масштаб" => "Scale",
            _ => "Move"
        };
        bool committed;
        try
        {
            committed = _pendingGizmoTransformSession is not null
                ? _pendingGizmoTransformSession.Commit(
                    $"{operation} pending placement along {axis}")
                : _gizmoTransformSession!.Commit(
                    $"{operation} {count:N0} entities along {axis}");
        }
        catch (Exception exception) when (
            exception is InvalidOperationException or NotSupportedException)
        {
            _gizmoTransformSession?.Cancel();
            _pendingGizmoTransformSession?.Cancel();
            _gizmoTransformSession = null;
            _pendingGizmoTransformSession = null;
            _gizmoDragLayout = null;
            _rotationGizmoDragLayout = null;
            _activeGizmoAxis = SmoGizmoAxis.None;
            _hoverGizmoAxis = SmoGizmoAxis.None;
            if (ViewportSurface.IsMouseCaptured)
                ViewportSurface.ReleaseMouseCapture();
            ViewportSurface.Cursor = null;
            StatusText.Text = exception.Message;
            RefreshActiveInspector();
            return;
        }
        _gizmoTransformSession = null;
        _pendingGizmoTransformSession = null;
        _gizmoDragLayout = null;
        _rotationGizmoDragLayout = null;
        _activeGizmoAxis = SmoGizmoAxis.None;
        _hoverGizmoAxis = SmoGizmoAxis.None;
        if (ViewportSurface.IsMouseCaptured)
            ViewportSurface.ReleaseMouseCapture();
        ViewportSurface.Cursor = null;
        if (committed && HasProject)
        {
            if (TryCaptureProjectTransforms(
                    projectEntityIds,
                    out ProjectTransformTarget[] targets,
                    out string reason))
            {
                _ = CommitProjectTransformsAsync(
                    $"{operation} {count:N0} entities along {axis}",
                    targets);
            }
            else
            {
                StatusText.Text = reason;
                _ = RefreshProjectPreviewAsync(
                    "Проектная трансформация отменена; preview восстановлен");
            }
            return;
        }
        StatusText.Text = committed
            ? operation switch
            {
                "Rotate" =>
                    $"Rotate {axis} · {count:N0} entities · {angle * 180 / MathF.PI:0.##}°",
                "Scale" =>
                    $"Scale {axis} · {count:N0} entities · " +
                    $"{scale.X:0.###}, {scale.Y:0.###}, {scale.Z:0.###}",
                _ => $"Move {axis} · {count:N0} entities · " +
                     $"Δ {delta.X:0.###}, {delta.Y:0.###}, {delta.Z:0.###}"
            }
            : $"{operation} отменён · transform не изменился";
    }

    private void CancelGizmoDrag()
    {
        if (!HasActiveGizmoTransform)
            return;
        _gizmoTransformSession?.Cancel();
        _pendingGizmoTransformSession?.Cancel();
        _gizmoTransformSession = null;
        _pendingGizmoTransformSession = null;
        _gizmoDragLayout = null;
        _rotationGizmoDragLayout = null;
        _activeGizmoAxis = SmoGizmoAxis.None;
        _hoverGizmoAxis = SmoGizmoAxis.None;
        if (ViewportSurface?.IsMouseCaptured == true)
            ViewportSurface.ReleaseMouseCapture();
        if (ViewportSurface is not null)
            ViewportSurface.Cursor = null;
        if (StatusText is not null)
            StatusText.Text = "Трансформация отменена";
    }

    private bool HasActiveGizmoTransform =>
        _gizmoTransformSession is not null ||
        _pendingGizmoTransformSession is not null;

    private bool TryGetPendingPlacementTransform(
        PendingPlacementSelection pending,
        out Matrix4x4 transform)
    {
        if (_document is not null)
        {
            if (pending.External &&
                _document.ExternalPlacements.TryGetValue(
                    pending.Id,
                    out SmoLevelExternalPlacement? externalPlacement))
            {
                transform = externalPlacement.WorldTransform;
                return true;
            }
            if (!pending.External)
            {
                SmoLevelPlacementAddition? addition = _document
                    .PlacementAdditions.Values.FirstOrDefault(candidate =>
                        candidate.GroupId == pending.Id);
                if (addition is not null)
                {
                    transform = addition.WorldTransform;
                    return true;
                }
            }
        }
        transform = Matrix4x4.Identity;
        return false;
    }

    private SmoSceneMesh ApplyPendingPlacementTransform(SmoSceneMesh mesh)
    {
        if (_document is null ||
            !_pendingPlacementByScene.TryGetValue(
                mesh.SceneObjectIndex,
                out PendingPlacementSelection? pending))
        {
            return mesh;
        }
        if (pending.External &&
            _document.ExternalPlacements.TryGetValue(
                pending.Id,
                out SmoLevelExternalPlacement? externalPlacement))
        {
            return mesh with
            {
                WorldTransform = externalPlacement.WorldTransform
            };
        }
        if (!pending.External &&
            pending.PlacementId is Guid placementId &&
            _document.PlacementAdditions.TryGetValue(
                placementId,
                out SmoLevelPlacementAddition? addition))
        {
            return mesh with
            {
                WorldTransform = addition.WorldTransform
            };
        }
        return mesh;
    }

    private Point ToGizmoPoint(Point point) =>
        new(point.X * _viewportDpiX, point.Y * _viewportDpiY);

    private static Vector3 ToNativePoint(Point3D point) =>
        new((float)point.X, (float)point.Y, (float)-point.Z);

    private void FrameAsset(SmoLevelAsset asset)
    {
        if (_workspace is null || asset.Placements.Count == 0)
            return;
        int placementIndex = Math.Clamp(
            _activePlacementIndex,
            0,
            asset.Placements.Count - 1);
        int sceneObjectIndex = asset.Placements[placementIndex].SceneObjectIndex;
        FrameMeshes((_renderPreparedScene ?? _workspace.PreparedScene).Meshes.Where(mesh =>
            mesh.Mesh.ObjectIndex == asset.ObjectIndex &&
            mesh.SceneObjectIndex == sceneObjectIndex && mesh.OccurrenceKey == asset.Placements[placementIndex].OccurrenceKey));
        _cameraFocusKey = (asset.ObjectIndex, sceneObjectIndex, asset.Placements[placementIndex].OccurrenceKey);
    }

    private void FrameMeshes(IEnumerable<SmoSceneMesh> meshes)
    {
        if (TryCalculateBounds(meshes, out CameraBounds bounds))
            FrameBounds(bounds, orbit: true);
    }

    private static bool TryCalculateBounds(
        IEnumerable<SmoSceneMesh> meshes,
        out CameraBounds bounds)
    {
        bool hasBounds = false;
        Vector3 minimum = new(float.PositiveInfinity);
        Vector3 maximum = new(float.NegativeInfinity);

        foreach (SmoSceneMesh sceneMesh in meshes)
        {
            foreach (Vector3 position in sceneMesh.Mesh.Positions)
            {
                Vector3 world = Vector3.Transform(position, sceneMesh.WorldTransform);
                world.Z = -world.Z;
                minimum = Vector3.Min(minimum, world);
                maximum = Vector3.Max(maximum, world);
                hasBounds = true;
            }
        }

        if (!hasBounds)
        {
            bounds = default;
            return false;
        }

        bounds = new CameraBounds(minimum, maximum);
        return true;
    }

    private bool TryCalculateSelectionBounds(out CameraBounds bounds)
    {
        bounds = default;
        if (_workspace is null || _document is null)
            return false;

        SmoSceneMesh[] selectedMeshes = _selectedPendingPlacement is
            PendingPlacementSelection pending
            ? (_renderPreparedScene ?? _workspace.PreparedScene).Meshes
                .Where(mesh =>
                    _pendingPlacementByScene.TryGetValue(
                        mesh.SceneObjectIndex,
                        out PendingPlacementSelection? candidate) &&
                    candidate.Id == pending.Id &&
                    candidate.External == pending.External)
                .Select(ApplyPendingPlacementTransform)
                .ToArray()
            : (_renderPreparedScene ?? _workspace.PreparedScene).Meshes
                .Where(mesh =>
                {
                    var placementId = new SmoPlacementId(
                        mesh.Mesh.ObjectIndex,
                        mesh.SceneObjectIndex, mesh.OccurrenceKey);
                    return _document.TryGetPlacement(
                               placementId,
                               out SmoEditablePlacement? placement) &&
                           _selectedEntities.Contains(placement!.Entity.Id);
                })
                .Select(mesh =>
                {
                    var placementId = new SmoPlacementId(
                        mesh.Mesh.ObjectIndex,
                        mesh.SceneObjectIndex, mesh.OccurrenceKey);
                    return _document.TryGetPlacement(
                        placementId,
                        out SmoEditablePlacement? placement)
                        ? mesh with { WorldTransform = placement!.WorldTransform }
                        : mesh;
                })
                .ToArray();
        bool hasBounds = TryCalculateBounds(selectedMeshes, out bounds);
        foreach (SmoEditableCollision collision in _document.Collisions.Where(
                     collision => _selectedEntities.Contains(collision.Entity.Id)))
        {
            foreach (Vector3 position in collision.Source.Positions)
            {
                Vector3 world = Vector3.Transform(position, collision.WorldTransform);
                world.Z = -world.Z;
                bounds = hasBounds
                    ? bounds.Include(world)
                    : new CameraBounds(world, world);
                hasBounds = true;
            }
        }
        return hasBounds;
    }

    private void FrameBounds(CameraBounds bounds, bool orbit)
    {
        _cameraTarget = bounds.Center;
        double halfFov = PerspectiveFieldOfView * Math.PI / 360.0;
        _cameraDistance = Math.Max(
            bounds.Diagonal * 0.65 / Math.Tan(halfFov),
            bounds.Diagonal * 0.75);
        _cameraDesiredDistance = _cameraDistance;
        _cameraVelocity = default;
        _cameraOrbitingFocus = orbit;
        if (!orbit)
            _cameraFocusKey = null;
        if (_sceneCamera is OrthographicCamera orthographic)
            orthographic.Width = Math.Max(bounds.Diagonal * 1.35, 0.01);
        PositionCameraFromOrbit();
        UpdateCamera();
    }

    private void FocusPlacement(SmoLevelAsset asset, int placementIndex)
    {
        if (_workspace is null || asset.Placements.Count == 0)
            return;
        SmoLevelPlacement placement = asset.Placements[Math.Clamp(
            placementIndex,
            0,
            asset.Placements.Count - 1)];
        var focusKey = (asset.ObjectIndex, placement.SceneObjectIndex, placement.OccurrenceKey);
        if (_cameraFocusKey == focusKey && _cameraOrbitingFocus)
            return;

        IEnumerable<SmoSceneMesh> meshes = (_renderPreparedScene ?? _workspace.PreparedScene).Meshes.Where(mesh =>
            mesh.Mesh.ObjectIndex == asset.ObjectIndex &&
            mesh.SceneObjectIndex == placement.SceneObjectIndex && mesh.OccurrenceKey == placement.OccurrenceKey);
        if (!TryCalculateBounds(meshes, out CameraBounds bounds))
            return;

        _cameraTarget = bounds.Center;
        Vector3D offset = _sceneCamera.Position - _cameraTarget;
        _cameraDistance = Math.Max(offset.Length, bounds.Diagonal * 0.75);
        if (offset.LengthSquared < 1e-12)
        {
            offset = -GetCameraForward() * _cameraDistance;
            _sceneCamera.Position = _cameraTarget + offset;
        }
        SetAnglesFromOffset(offset);
        _cameraOrbitingFocus = true;
        _cameraFocusKey = focusKey;
        _cameraDesiredDistance = _cameraDistance;
        _cameraVelocity = default;
        UpdateCamera();
    }

    private void PositionCameraFromOrbit()
    {
        double horizontal = Math.Cos(_cameraPitch) * _cameraDistance;
        var offset = new Vector3D(
            Math.Sin(_cameraYaw) * horizontal,
            Math.Sin(_cameraPitch) * _cameraDistance,
            Math.Cos(_cameraYaw) * horizontal);
        _sceneCamera.Position = _cameraTarget + offset;
    }

    private void SetAnglesFromOffset(Vector3D offset)
    {
        double length = Math.Max(offset.Length, 1e-9);
        _cameraYaw = Math.Atan2(offset.X, offset.Z);
        _cameraPitch = Math.Asin(Math.Clamp(offset.Y / length, -1, 1));
    }

    private void UpdateCamera()
    {
        if (_cameraOrbitingFocus)
        {
            PositionCameraFromOrbit();
            _sceneCamera.LookDirection = _cameraTarget - _sceneCamera.Position;
        }
        else
        {
            _sceneCamera.LookDirection = GetCameraForward();
        }
        _sceneCamera.UpDirection = new Vector3D(0, 1, 0);
        double sceneDistance = (_sceneCamera.Position - _sceneCenter).Length;
        double clipScale = Math.Max(
            Math.Max(_sceneDiagonal, _cameraDistance),
            sceneDistance);
        _sceneCamera.NearPlaneDistance = Math.Max(clipScale / 100000, 0.001);
        _sceneCamera.FarPlaneDistance = Math.Max(
            sceneDistance + _sceneDiagonal * 4,
            Math.Max(clipScale * 20, 100));
        if (CameraModeText is not null)
        {
            string mode = _cameraOrbitingFocus
                ? _cameraFocusKey is null ? "ORBIT" : "FOCUS ORBIT"
                : "FREE";
            string projection = _sceneCamera is OrthographicCamera
                ? "ORTHO"
                : $"{PerspectiveFieldOfView:0}°";
            CameraModeText.Text =
                $"Camera  {mode}  ·  {projection}  ·  MSAA 4×";
        }
    }

    private Vector3D GetCameraForward()
    {
        double horizontal = Math.Cos(_cameraPitch);
        var forward = new Vector3D(
            -Math.Sin(_cameraYaw) * horizontal,
            -Math.Sin(_cameraPitch),
            -Math.Cos(_cameraYaw) * horizontal);
        forward.Normalize();
        return forward;
    }

    private void UpdateCameraNavigation(TimeSpan delta)
    {
        double seconds = Math.Clamp(delta.TotalSeconds, 0, 0.05);
        if (seconds <= 0)
            return;

        bool cameraChanged = false;
        if (_cameraOrbitingFocus)
        {
            double zoomBlend = 1 - Math.Exp(-14 * seconds);
            double nextDistance = _cameraDistance +
                (_cameraDesiredDistance - _cameraDistance) * zoomBlend;
            if (Math.Abs(nextDistance - _cameraDistance) > 1e-7)
            {
                _cameraDistance = nextDistance;
                cameraChanged = true;
            }
        }

        Vector3D direction = GetCameraMovementDirection();
        if (direction.LengthSquared > 1e-12)
        {
            direction.Normalize();
            if (_cameraOrbitingFocus)
                ReleaseCameraFocus();
        }

        double speed = Math.Max(_sceneDiagonal * 0.16, 1);
        if (Keyboard.Modifiers.HasFlag(ModifierKeys.Shift))
            speed *= 4;
        else if (Keyboard.Modifiers.HasFlag(ModifierKeys.Control))
            speed *= 0.25;
        Vector3D desiredVelocity = direction * speed;
        double velocityBlend = 1 - Math.Exp(
            -(direction.LengthSquared > 1e-12 ? 11 : 8) * seconds);
        _cameraVelocity += (desiredVelocity - _cameraVelocity) * velocityBlend;
        if (direction.LengthSquared <= 1e-12 &&
            _cameraVelocity.Length < speed * 0.0001)
        {
            _cameraVelocity = default;
        }
        if (_cameraVelocity.LengthSquared > 1e-12)
        {
            _sceneCamera.Position += _cameraVelocity * seconds;
            cameraChanged = true;
        }

        if (cameraChanged)
            UpdateCamera();
    }

    private Vector3D GetCameraMovementDirection()
    {
        GetCameraBasis(out Vector3D right, out _);
        Vector3D direction = default;
        if (_cameraMovementKeys.Contains(Key.W))
            direction += GetCameraForward();
        if (_cameraMovementKeys.Contains(Key.S))
            direction -= GetCameraForward();
        if (_cameraMovementKeys.Contains(Key.A))
            direction -= right;
        if (_cameraMovementKeys.Contains(Key.D))
            direction += right;
        if (_cameraMovementKeys.Contains(Key.Q))
            direction.Y -= 1;
        if (_cameraMovementKeys.Contains(Key.E))
            direction.Y += 1;
        return direction;
    }

    private void ViewportSurface_DragOver(object sender, DragEventArgs e)
    {
        e.Effects = e.Data.GetDataPresent(CatalogModelDrag.Format)
            ? DragDropEffects.Copy
            : DragDropEffects.None;
        e.Handled = true;
    }

    private async void ViewportSurface_Drop(object sender, DragEventArgs e)
    {
        if (_document is null || _workspace is null ||
            e.Data.GetData(CatalogModelDrag.Format) is not CatalogModelDrag model)
            return;
        try
        {
            Vector3 target = ResolveDropPosition(e.GetPosition(ViewportSurface));
            if (HasProject)
            {
                bool placed = await PlaceProjectCatalogModelAsync(model, target);
                e.Effects = placed ? DragDropEffects.Copy : DragDropEffects.None;
                e.Handled = true;
                return;
            }
            else
                PlaceCatalogModel(model, target);
            e.Effects = DragDropEffects.Copy;
            e.Handled = true;
        }
        catch (Exception exception)
        {
            StatusText.Text = $"Не удалось добавить размещение: {exception.Message}";
            e.Effects = DragDropEffects.None;
            e.Handled = true;
        }
    }

    private void PlaceCatalogItem(CatalogItem item, Vector3 position)
    {
        if (!TryCreateCatalogDrag(item, out CatalogModelDrag? drag, out string error))
        {
            StatusText.Text = error;
            MessageBox.Show(
                this,
                error,
                "SmoLVLcreator — размещение модели",
                MessageBoxButton.OK,
                MessageBoxImage.Warning);
            return;
        }
        try
        {
            PlaceCatalogModel(drag!, position);
        }
        catch (Exception exception)
        {
            StatusText.Text = $"Не удалось добавить размещение: {exception.Message}";
            MessageBox.Show(
                this,
                exception.Message,
                "SmoLVLcreator — размещение модели",
                MessageBoxButton.OK,
                MessageBoxImage.Error);
        }
    }

    private void PlaceCatalogModel(CatalogModelDrag model, Vector3 position)
    {
        if (_document is null)
            return;
        _viewportActionStatusUntilUtc = DateTime.UtcNow.AddSeconds(3);
        if (model.ExternalModelId is Guid externalId)
        {
            Matrix4x4 transform =
                Matrix4x4.CreateScale(model.InitialScale) *
                Matrix4x4.CreateTranslation(
                    position - model.SourceAnchor * model.InitialScale);
            Guid placementId = _document.AddExternalPlacement(
                externalId,
                transform,
                model.Name);
            var selection = new PendingPlacementSelection(
                placementId, External: true, 0, model.Name);
            SelectPendingPlacement(selection);
            StatusText.Text =
                $"Размещена внешняя модель {model.Name} · " +
                $"{position.X:0.##}, {position.Y:0.##}, {position.Z:0.##}";
            return;
        }
        Vector3 delta = position - model.SourceAnchor;
        IReadOnlyList<Guid> ids = _document.AddSharedPlacements(model.Parts.Select(part =>
        {
            Matrix4x4 transform = part.SourceWorldTransform;
            transform.M41 += delta.X;
            transform.M42 += delta.Y;
            transform.M43 += delta.Z;
            return new SmoSharedPlacementRequest(
                part.MeshObjectIndex,
                transform,
                $"{part.Name}_copy");
        }).ToArray());
        var pendingSelection = new PendingPlacementSelection(
            _document.PlacementAdditions[ids[0]].GroupId,
            External: false,
            0,
            model.Name);
        SelectPendingPlacement(pendingSelection);
        string placementPoint =
            $"{position.X:0.##}, {position.Y:0.##}, {position.Z:0.##}";
        StatusText.Text = model.Parts.Length == 1
            ? $"Добавлено размещение {model.Name} · {placementPoint} · " +
              "геометрия не дублируется"
            : $"Добавлена составная модель {model.Name} · {placementPoint} · " +
              $"{model.Parts.Length} ссылочных частей";
    }

    private Vector3 ResolveDropPosition(Point viewportPoint)
    {
        ReportScenePickingIssues();
        SmoPickRay ray = SmoViewportMath.CreateSmoPickRay(
            _sceneCamera,
            viewportPoint.X,
            viewportPoint.Y,
            Math.Max(ViewportSurface.ActualWidth, 1),
            Math.Max(ViewportSurface.ActualHeight, 1));
        if (_scenePickIndex?.Pick(ray) is SmoSceneHit hit)
            return hit.Position;
        double fallbackDistance = _cameraOrbitingFocus
            ? _cameraDistance
            : Math.Clamp(_sceneDiagonal * 0.035, 25, 750);
        return ray.Origin + ray.Direction * (float)fallbackDistance;
    }

    private void ViewportSurface_MouseDown(object sender, MouseButtonEventArgs e)
    {
        if (e.ChangedButton == MouseButton.Left)
        {
            Point gizmoPoint = ToGizmoPoint(e.GetPosition(ViewportSurface));
            SmoGizmoAxis hitAxis = _editorTool switch
            {
                "Перемещение" or "Масштаб" =>
                    _gizmoLayout?.HitTest(gizmoPoint) ?? SmoGizmoAxis.None,
                "Вращение" =>
                    _rotationGizmoLayout?.HitTest(gizmoPoint) ?? SmoGizmoAxis.None,
                _ => SmoGizmoAxis.None
            };
            if (hitAxis != SmoGizmoAxis.None && BeginGizmoDrag(hitAxis, gizmoPoint))
            {
                e.Handled = true;
                return;
            }
            SelectObjectAt(e.GetPosition(ViewportSurface));
            ViewportSurface.Focus();
            e.Handled = true;
            return;
        }
        if (e.ChangedButton != MouseButton.Right)
            return;
        _lastViewportMouse = e.GetPosition(ViewportSurface);
        _viewportPanning = Keyboard.Modifiers.HasFlag(ModifierKeys.Shift);
        _viewportOrbiting = !_viewportPanning;
        ViewportSurface.Focus();
        ViewportSurface.CaptureMouse();
        ViewportSurface.Cursor = _viewportPanning ? Cursors.SizeAll : Cursors.Hand;
        e.Handled = true;
    }

    private void SelectObjectAt(Point viewportPoint)
    {
        if (_workspace is null || _scenePickIndex is null ||
            ViewportSurface.ActualWidth <= 0 || ViewportSurface.ActualHeight <= 0)
        {
            return;
        }

        ReportScenePickingIssues();
        SmoPickRay ray = SmoViewportMath.CreateSmoPickRay(
            _sceneCamera,
            viewportPoint.X,
            viewportPoint.Y,
            ViewportSurface.ActualWidth,
            ViewportSurface.ActualHeight);
        CollisionHit? collisionHit = _showCollisions
            ? PickCollision(ray)
            : null;
        SmoSceneHit? hit = _scenePickIndex.Pick(ray);
        bool toggle = Keyboard.Modifiers.HasFlag(ModifierKeys.Control);
        bool individualPart = Keyboard.Modifiers.HasFlag(ModifierKeys.Alt);
        float surfaceTolerance = (float)Math.Clamp(
            _sceneDiagonal * 0.0005,
            1,
            6);
        if (collisionHit is not null &&
            (hit is null || collisionHit.Distance <= hit.Distance + surfaceTolerance))
        {
            SelectCollision(collisionHit, toggle);
            return;
        }
        if (hit is null)
        {
            if (!toggle)
                ClearViewportSelection();
            StatusText.Text = _scenePickIndex.Issues.Count == 0
                ? "Под курсором нет объекта"
                : "Поддержанный объект не найден · скелетный picking недоступен";
            return;
        }

        if (_pendingPlacementByScene.TryGetValue(
                hit.SceneMesh.SceneObjectIndex,
                out PendingPlacementSelection? pending) && pending is not null)
        {
            SelectPendingPlacement(pending);
            return;
        }

        EditorAssetItem? item = _allAssets.FirstOrDefault(candidate =>
            candidate.Asset?.ObjectIndex == hit.SceneMesh.Mesh.ObjectIndex);
        if (item?.Asset is not SmoLevelAsset asset)
        {
            StatusText.Text =
                $"Объект [{hit.SceneMesh.SceneObjectIndex}] отсутствует в каталоге";
            return;
        }

        _selectedPendingPlacement = null;

        int placementIndex = FindPlacementIndex(
            asset,
            hit.SceneMesh.SceneObjectIndex, hit.SceneMesh.OccurrenceKey);
        if (!toggle)
        {
            ReplacePlacementSelection(
                asset,
                placementIndex,
                expandComposite: !individualPart);
            RevealSelection(item, placementIndex, updateViewportSelection: false);
        }
        else
        {
            bool selected = TogglePlacementSelection(
                asset,
                placementIndex,
                expandComposite: !individualPart);
            if (_selectedEntities.Count == 0)
            {
                ClearViewportSelection();
                StatusText.Text = $"Выбор снят · {item.Name}";
                return;
            }

            if (selected)
            {
                if (_document!.TryGetPlacement(
                        new SmoPlacementId(
                            asset.ObjectIndex,
                            asset.Placements[placementIndex].SceneObjectIndex, asset.Placements[placementIndex].OccurrenceKey),
                        out SmoEditablePlacement? editablePlacement))
                {
                    if (individualPart)
                        FocusPlacement(asset, placementIndex);
                    else
                        FocusPlacementOrComposite(
                            editablePlacement!.Entity,
                            asset,
                            placementIndex);
                }
                RevealSelection(item, placementIndex, updateViewportSelection: false);
            }
            else if (TryResolveActiveSelection(
                         out EditorAssetItem? activeItem,
                         out SmoLevelAsset? activeAsset,
                         out int activePlacementIndex))
            {
                _activePlacementIndex = activePlacementIndex;
                FocusPlacement(activeAsset!, activePlacementIndex);
                RevealSelection(
                    activeItem!,
                    activePlacementIndex,
                    updateViewportSelection: false);
                item = activeItem!;
                placementIndex = activePlacementIndex;
            }
        }

        UpdateSelectionCaption(item.Name, placementIndex);
        StatusText.Text = _selectedEntities.Count > 1
            ? $"Выбрано {_selectedEntities.Count:N0} entities · " +
              $"active {item.Name}"
            : $"Выбрано · {item.Name} · placement {placementIndex + 1} · " +
              $"triangle {hit.TriangleIndex:N0}";
    }

    private CollisionHit? PickCollision(SmoPickRay ray)
    {
        if (_document is null)
            return null;
        CollisionHit? closest = null;
        foreach (SmoEditableCollision collision in GetVisibleCollisions())
        {
            if (!Matrix4x4.Invert(collision.WorldTransform, out Matrix4x4 inverse))
                continue;
            SmoPickRay localRay = new(
                Vector3.Transform(ray.Origin, inverse),
                Vector3.TransformNormal(ray.Direction, inverse));
            IReadOnlyList<int> indices = collision.Source.TriangleIndices;
            IReadOnlyList<Vector3> positions = collision.Source.Positions;
            for (int offset = 0; offset + 2 < indices.Count; offset += 3)
            {
                int a = indices[offset];
                int b = indices[offset + 1];
                int c = indices[offset + 2];
                if ((uint)a >= (uint)positions.Count ||
                    (uint)b >= (uint)positions.Count ||
                    (uint)c >= (uint)positions.Count ||
                    !SmoPickingMath.TryIntersectTriangle(
                        localRay,
                        positions[a],
                        positions[b],
                        positions[c],
                        out float distance,
                        out _) || distance < 0)
                {
                    continue;
                }
                Vector3 localHit = localRay.Origin + localRay.Direction * distance;
                float worldDistance = Vector3.Distance(
                    ray.Origin,
                    Vector3.Transform(localHit, collision.WorldTransform));
                if (closest is not null && worldDistance >= closest.Distance)
                    continue;
                closest = new CollisionHit(collision, worldDistance, offset / 3);
            }
        }
        return closest;
    }

    private IReadOnlyList<SmoEditableCollision> GetVisibleCollisions()
    {
        if (_document is null || _workspace is null ||
            _document.Collisions.Count == 0)
        {
            return Array.Empty<SmoEditableCollision>();
        }

        SmoEditableCollision[] availableCollisions = _document.Collisions
            .Where(collision =>
                !_hiddenEntities.Contains(collision.Entity.Id) &&
                !_document.RemovedEntityIds.Contains(collision.Entity.Id))
            .ToArray();
        if (availableCollisions.Length == 0)
            return Array.Empty<SmoEditableCollision>();

        IGrouping<int, SmoEditableCollision>[] regions = availableCollisions
            .GroupBy(GetCollisionRegionIndex)
            .Where(group => group.Key >= 0)
            .ToArray();
        if (regions.Length == 0)
            return availableCollisions;

        int? selectedRegion = TryResolveActiveCollision(
            out SmoEditableCollision? selectedCollision)
                ? GetCollisionRegionIndex(selectedCollision!)
                : null;
        if (selectedCollision is not null && selectedRegion < 0)
            return [selectedCollision];
        IGrouping<int, SmoEditableCollision>? activeRegion = selectedRegion >= 0
            ? regions.FirstOrDefault(group => group.Key == selectedRegion)
            : null;
        if (activeRegion is null)
        {
            Vector3 nativeCamera = new(
                (float)_sceneCamera.Position.X,
                (float)_sceneCamera.Position.Y,
                (float)-_sceneCamera.Position.Z);
            activeRegion = regions
                .Select(group => new
                {
                    Group = group,
                    Bounds = CalculateCollisionBounds(group)
                })
                .Where(item => item.Bounds is not null)
                .OrderBy(item => item.Bounds!.Value.DistanceSquared(nativeCamera))
                .ThenBy(item => item.Bounds!.Value.Diagonal)
                .Select(item => item.Group)
                .FirstOrDefault();
        }

        if (activeRegion is null)
            return availableCollisions;

        SmoEditableCollision[] visible = activeRegion.ToArray();
        CollisionRegionBounds? activeBounds = CalculateCollisionBounds(visible);
        if (activeBounds is null)
            return visible;
        float margin = Math.Max(activeBounds.Value.Diagonal * 0.08f, 50f);
        CollisionRegionBounds expanded = activeBounds.Value.Expand(margin);
        return visible.Concat(availableCollisions.Where(collision =>
                GetCollisionRegionIndex(collision) < 0 &&
                CalculateCollisionBounds([collision]) is CollisionRegionBounds bounds &&
                expanded.Intersects(bounds)))
            .Distinct()
            .ToArray();
    }

    private int GetCollisionRegionIndex(SmoEditableCollision collision)
    {
        int collisionInfoIndex = collision.Source.CollisionInfoObjectIndex;
        if (collisionInfoIndex < 0 || collision.Source.NodeObjectIndex < 0)
            return -1;
        if (_collisionRegionByInfo.TryGetValue(
                collisionInfoIndex,
                out int cached))
        {
            return cached;
        }
        if (_workspace is null)
            return -1;

        SmoObjectEntry? cursor =
            _workspace.Document.Objects[collision.Source.NodeObjectIndex];
        var visited = new HashSet<int>();
        while (cursor is not null && visited.Add(cursor.Index))
        {
            string name = cursor.Name.TrimEnd('\0');
            if (name.StartsWith("sector", StringComparison.OrdinalIgnoreCase))
            {
                _collisionRegionByInfo[collisionInfoIndex] = cursor.Index;
                return cursor.Index;
            }
            cursor = cursor.ParentIndex is int parentIndex &&
                     (uint)parentIndex < (uint)_workspace.Document.Objects.Count
                ? _workspace.Document.Objects[parentIndex]
                : null;
        }

        _collisionRegionByInfo[collisionInfoIndex] = -1;
        return -1;
    }

    private static CollisionRegionBounds? CalculateCollisionBounds(
        IEnumerable<SmoEditableCollision> collisions)
    {
        bool found = false;
        Vector3 minimum = new(float.PositiveInfinity);
        Vector3 maximum = new(float.NegativeInfinity);
        foreach (SmoEditableCollision collision in collisions)
        {
            foreach (Vector3 position in collision.Source.Positions)
            {
                Vector3 world = Vector3.Transform(position, collision.WorldTransform);
                minimum = Vector3.Min(minimum, world);
                maximum = Vector3.Max(maximum, world);
                found = true;
            }
        }
        return found ? new CollisionRegionBounds(minimum, maximum) : null;
    }

    private void SelectCollision(CollisionHit hit, bool toggle)
    {
        SmoEditableCollision collision = hit.Collision;
        SmoLevelEntity entity = collision.Entity;
        bool selected = true;
        if (!toggle)
        {
            _selectedEntities.Clear();
            _selectedEntities.Add(entity.Id);
        }
        else if (!_selectedEntities.Remove(entity.Id))
        {
            _selectedEntities.Add(entity.Id);
        }
        else
        {
            selected = false;
        }

        if (selected)
        {
            _activeCollisionEntityIndex = entity.Id.SceneObjectIndex;
            _activeSelectionKey = null;
            FocusCollision(entity);
            UpdateCollisionInspector(collision);
            UpdateCollisionSelectionCaption(entity);
            RevealCollisionInTree(entity.Id.SceneObjectIndex);
        }
        else if (_selectedEntities.Count == 0)
        {
            ClearViewportSelection();
            StatusText.Text = $"Выбор снят · {entity.Name}";
            return;
        }
        else
        {
            ActivateFirstSelectedEntity();
            RefreshActiveInspector();
        }

        ApplyPlacementHighlights();
        StatusText.Text =
            $"Выбрана коллизия · {entity.Name} · triangle {hit.TriangleIndex:N0}";
    }

    private bool TryResolveActiveCollision(out SmoEditableCollision? collision)
    {
        collision = null;
        if (_document is null || _activeCollisionEntityIndex is not int entityIndex)
            return false;
        collision = _document.Collisions.FirstOrDefault(candidate =>
            candidate.Entity.Id.SceneObjectIndex == entityIndex);
        return collision is not null;
    }

    private void FocusCollision(SmoLevelEntity entity)
    {
        bool hasBounds = false;
        CameraBounds bounds = default;
        foreach (SmoEditableCollision collision in entity.Collisions)
        {
            foreach (Vector3 position in collision.Source.Positions)
            {
                Vector3 world = Vector3.Transform(position, collision.WorldTransform);
                world.Z = -world.Z;
                bounds = hasBounds ? bounds.Include(world) : new CameraBounds(world, world);
                hasBounds = true;
            }
        }
        if (hasBounds)
        {
            _cameraFocusKey = null;
            FrameBounds(bounds, orbit: true);
        }
    }

    private void UpdateCollisionSelectionCaption(SmoLevelEntity entity)
    {
        ViewportSelectionText.Text = _selectedEntities.Count > 1
            ? $"SELECTED · {_selectedEntities.Count:N0} entities · active COLLISION {entity.Name}"
            : $"SELECTED · COLLISION · {entity.Name}";
    }

    private void CollisionVisibility_Changed(object sender, RoutedEventArgs e)
    {
        _showCollisions = CollisionVisibilityToggle?.IsChecked == true;
        if (StatusText is not null)
        {
            StatusText.Text = _showCollisions
                ? $"Коллизии включены · {_document?.Collisions.Count ?? 0:N0} shapes"
                : "Коллизии скрыты";
        }
        if (!_showCollisions && _activeCollisionEntityIndex is not null)
            ClearViewportSelection();
    }

    private void LinkedCollisionMovement_Changed(object sender, RoutedEventArgs e)
    {
        if (StatusText is null)
            return;
        StatusText.Text = LinkedCollisionMovementToggle?.IsChecked == true
            ? "Связанные предметы и коллизии перемещаются вместе"
            : "Независимое перемещение: связь с коллизией временно отключена";
        ApplyPlacementHighlights();
    }

    private bool IsPartOfTransformSelection(SmoLevelEntityId entityId)
    {
        if (_selectedEntities.Contains(entityId))
            return true;
        if (_document is null ||
            LinkedCollisionMovementToggle?.IsChecked != true)
        {
            return false;
        }
        return _document.GetCollisionLinks(entityId).Any(link =>
            _selectedEntities.Contains(link.VisualEntityId) ||
            _selectedEntities.Contains(link.CollisionEntityId));
    }

    private IReadOnlyList<SmoLevelEntityId> GetTransformSelection()
    {
        if (_document is null ||
            LinkedCollisionMovementToggle?.IsChecked != true)
        {
            return _selectedEntities.ToArray();
        }
        return _document.ExpandLinkedEntities(_selectedEntities);
    }

    private static int FindPlacementIndex(SmoLevelAsset asset, int sceneObjectIndex, SmoRenderOccurrenceKey? occurrenceKey)
    {
        for (int index = 0; index < asset.Placements.Count; index++)
        {
            if (asset.Placements[index].SceneObjectIndex == sceneObjectIndex && asset.Placements[index].OccurrenceKey == occurrenceKey)
                return index;
        }
        throw new InvalidDataException("The selected occurrence is absent from this asset.");
    }

    private bool TryResolveActiveSelection(
        out EditorAssetItem? item,
        out SmoLevelAsset? asset,
        out int placementIndex)
    {
        item = null;
        asset = null;
        placementIndex = 0;
        if (_activeSelectionKey is not PlacementSelectionKey active)
            return false;

        item = _allAssets.FirstOrDefault(candidate =>
            candidate.Asset?.ObjectIndex == active.MeshObjectIndex);
        asset = item?.Asset;
        if (asset is null)
            return false;
        placementIndex = FindPlacementIndex(asset, active.SceneObjectIndex, active.OccurrenceKey);
        return true;
    }

    private void RevealSelection(
        EditorAssetItem item,
        int placementIndex,
        bool updateViewportSelection = true)
    {
        TreeViewItem? treeItem = FindSceneTreeItem(
            SceneTree.Items.OfType<TreeViewItem>(),
            item,
            placementIndex);
        if (treeItem is null && !string.IsNullOrEmpty(OutlinerFilterBox.Text))
        {
            OutlinerFilterBox.Clear();
            treeItem = FindSceneTreeItem(
                SceneTree.Items.OfType<TreeViewItem>(),
                item,
                placementIndex);
        }

        _syncingSelection = true;
        if (treeItem is not null)
        {
            treeItem.IsSelected = true;
            treeItem.BringIntoView();
        }
        _syncingSelection = false;
        SelectCatalogItemForPlacement(item, placementIndex);
        UpdateInspector(item, placementIndex, updateViewportSelection);
    }

    private static TreeViewItem? FindSceneTreeItem(
        IEnumerable<TreeViewItem> nodes,
        EditorAssetItem item,
        int placementIndex)
    {
        foreach (TreeViewItem node in nodes)
        {
            if (node.Tag is SceneTreeSelection selection &&
                ReferenceEquals(selection.Item, item) &&
                selection.PlacementIndex == placementIndex)
            {
                return node;
            }

            TreeViewItem? descendant = FindSceneTreeItem(
                node.Items.OfType<TreeViewItem>(),
                item,
                placementIndex);
            if (descendant is not null)
            {
                node.IsExpanded = true;
                return descendant;
            }
        }
        return null;
    }

    private void ClearViewportSelection()
    {
        _highlightedRenderKeys.Clear();
        _selectedEntities.Clear();
        _selectedPendingPlacement = null;
        _activeSelectionKey = null;
        _activeCollisionEntityIndex = null;
        ReleaseCameraFocus();
        UpdateCamera();

        _syncingSelection = true;
        AssetList.SelectedItem = null;
        if (SceneTree.SelectedItem is TreeViewItem selectedTreeItem)
            selectedTreeItem.IsSelected = false;
        _syncingSelection = false;

        ViewportSelectionText.Text = "NO SELECTION";
        SelectedNameText.Text = "Ничего не выбрано";
        SelectedPathText.Text = "—";
        ObjectIdentityText.Text = "—";
        ObjectIdText.Text = "—";
        SetPositionEditor(null);
        SetRotationScaleEditor(null, null);
        PlacementCountText.Text = "—";
        GeometryCountText.Text = "—";
        VertexLayoutText.Text = "—";
        ChannelsText.Text = "—";
        TextureNameText.Text = "—";
        MaterialText.Text = "—";
        DiagnosticsText.Text = "Объект не выбран.";
        DiagnosticsText.Foreground = HealthyBrush;
        ApplyPlacementHighlights();
        UpdateCommandAvailability();
    }

    private void SelectPendingPlacement(PendingPlacementSelection pending)
    {
        _selectedEntities.Clear();
        _activeSelectionKey = null;
        _activeCollisionEntityIndex = null;
        _selectedPendingPlacement = pending;
        ApplyPlacementHighlights();
        ViewportSelectionText.Text = $"SELECTED · {pending.Name} · новое размещение";
        SelectedNameText.Text = pending.Name;
        SelectedPathText.Text = pending.External
            ? "Внешняя модель / новое размещение"
            : "Ресурс SMO / новое ссылочное размещение";
        ObjectIdentityText.Text = pending.External ? "external placement" : "shared placement";
        ObjectIdText.Text = pending.Id.ToString("N")[..8];
        UpdatePendingPlacementInspector(pending);
        StatusText.Text =
            $"Выбрано новое размещение {pending.Name} · гизмо активно · Delete — удалить";
        UpdateCommandAvailability();
    }

    private void UpdatePendingPlacementInspector(PendingPlacementSelection pending)
    {
        if (TryGetPendingPlacementTransform(pending, out Matrix4x4 transform) &&
            Matrix4x4.Decompose(
                transform,
                out Vector3 scale,
                out NumericsQuaternion rotation,
                out Vector3 translation))
        {
            SetPositionEditor(translation, enabled: true);
            SetRotationScaleEditor(rotation, scale, enabled: true);
        }
        else
        {
            SetPositionEditor(null);
            SetRotationScaleEditor(null, null);
        }
        int partCount = _pendingPlacementByScene.Values.Count(candidate =>
            candidate.Id == pending.Id &&
            candidate.External == pending.External);
        PlacementCountText.Text = $"{Math.Max(partCount, 1):N0} · новое размещение";
    }

    private void DeleteSelected_Click(object sender, RoutedEventArgs e)
    {
        if (_document is null)
            return;
        if (HasProject)
        {
            _ = DeleteProjectSelectionAsync();
            return;
        }
        if (_activeCollisionEntityIndex is int collisionIndex)
        {
            var collisionId = new SmoLevelEntityId(collisionIndex);
            if (_document.RemoveGeneratedCollision(collisionId))
            {
                ClearViewportSelection();
                BuildSceneTree();
                UpdateStatistics();
                UpdateCommandAvailability();
                StatusText.Text = "Созданная коллизия удалена · Ctrl+Z — отменить";
                return;
            }
        }
        if (_selectedPendingPlacement is PendingPlacementSelection pending)
        {
            bool removed = pending.External
                ? _document.RemoveExternalPlacement(pending.Id)
                : _document.RemoveSharedPlacementGroup(pending.Id);
            if (removed)
            {
                _selectedPendingPlacement = null;
                ClearViewportSelection();
                StatusText.Text = $"Удалено размещение {pending.Name} · можно отменить через Ctrl+Z";
            }
            return;
        }
        if (_selectedEntities.Count == 0)
            return;
        SmoLevelEntityId[] removable = _selectedEntities.Where(id =>
            _document.TryGetEntity(id, out _) &&
            !_document.GeneratedCollisions.ContainsKey(id) &&
            !_document.RemovedEntityIds.Contains(id)).ToArray();
        int collisionCount = removable.Count(id =>
            _document.GetEntity(id).Kind == SmoLevelEntityKind.Collision);
        if (_document.RemoveEntities(removable))
        {
            int count = removable.Length;
            ClearViewportSelection();
            BuildSceneTree();
            UpdateStatistics();
            UpdateCommandAvailability();
            StatusText.Text = collisionCount == count
                ? $"Удалено коллизий: {count:N0} · Ctrl+Z — отменить"
                : collisionCount == 0
                    ? $"Удалено объектов: {count:N0} · общие ресурсы будут сохранены"
                    : $"Удалено сущностей: {count:N0}, коллизий: {collisionCount:N0} · Ctrl+Z — отменить";
        }
    }

    private void CopyPlacement_Click(object sender, RoutedEventArgs e)
    {
        if (HasProject)
        {
            if (!TryCaptureProjectPlacementClipboard(
                    out ProjectPlacementClipboard? projectClipboard,
                    out string reason))
            {
                StatusText.Text = reason;
                return;
            }
            _projectPlacementClipboard = projectClipboard;
            UpdateCommandAvailability();
            StatusText.Text = projectClipboard!.Parts.Length > 1
                ? $"Скопировано размещение · {projectClipboard.Parts.Length:N0} частей"
                : $"Скопировано размещение · {projectClipboard.Name}";
            return;
        }
        if (!TryCapturePlacementClipboard(out PlacementClipboard? clipboard))
            return;
        _placementClipboard = clipboard;
        UpdateCommandAvailability();
        StatusText.Text = clipboard!.Parts.Length > 1
            ? $"Скопировано размещение · {clipboard.Parts.Length:N0} частей"
            : $"Скопировано размещение · {clipboard.Name}";
    }

    private void PastePlacement_Click(object sender, RoutedEventArgs e)
    {
        if (HasProject)
        {
            if (_projectPlacementClipboard is null)
                return;
            Vector3 projectTarget = ResolveDropPosition(new Point(
                Math.Max(ViewportSurface.ActualWidth, 1) * 0.5,
                Math.Max(ViewportSurface.ActualHeight, 1) * 0.5));
            _ = PasteProjectPlacementClipboardAsync(
                _projectPlacementClipboard,
                projectTarget,
                "Вставлено");
            return;
        }
        if (_placementClipboard is not PlacementClipboard clipboard ||
            clipboard.SourceDocument != _document)
        {
            return;
        }
        Vector3 target = ResolveDropPosition(new Point(
            Math.Max(ViewportSurface.ActualWidth, 1) * 0.5,
            Math.Max(ViewportSurface.ActualHeight, 1) * 0.5));
        PastePlacementClipboard(clipboard, target, "Вставлено");
    }

    private void DuplicatePlacement_Click(object sender, RoutedEventArgs e)
    {
        if (HasProject)
        {
            if (!TryCaptureProjectPlacementClipboard(
                    out ProjectPlacementClipboard? projectClipboard,
                    out string reason))
            {
                StatusText.Text = reason;
                return;
            }
            float projectOffset = (float)Math.Clamp(_sceneDiagonal * 0.012, 1, 100);
            _ = PasteProjectPlacementClipboardAsync(
                projectClipboard!,
                projectClipboard!.SourceAnchor + new Vector3(projectOffset, 0, 0),
                "Дублировано");
            return;
        }
        if (!TryCapturePlacementClipboard(out PlacementClipboard? clipboard))
            return;
        float offset = (float)Math.Clamp(_sceneDiagonal * 0.012, 1, 100);
        PastePlacementClipboard(
            clipboard!,
            clipboard!.SourceAnchor + new Vector3(offset, 0, 0),
            "Дублировано");
    }

    private bool TryCapturePlacementClipboard(out PlacementClipboard? clipboard)
    {
        clipboard = null;
        if (_document is null)
            return false;
        if (_selectedPendingPlacement is PendingPlacementSelection pending)
        {
            if (pending.External &&
                _document.ExternalPlacements.TryGetValue(
                    pending.Id,
                    out SmoLevelExternalPlacement? external))
            {
                clipboard = new PlacementClipboard(
                    _document,
                    external.Name,
                    external.ModelId,
                    external.WorldTransform,
                    [],
                    GetTranslation(external.WorldTransform));
                return true;
            }

            SmoLevelPlacementAddition[] additions = _document.PlacementAdditions.Values
                .Where(addition => addition.GroupId == pending.Id)
                .ToArray();
            if (additions.Length == 0)
                return false;
            CatalogPlacementPart[] pendingParts = additions.Select(addition =>
                new CatalogPlacementPart(
                    addition.MeshObjectIndex,
                    addition.WorldTransform,
                    addition.Name)).ToArray();
            clipboard = new PlacementClipboard(
                _document,
                pending.Name,
                null,
                null,
                pendingParts,
                CalculatePlacementAnchor(pendingParts));
            return true;
        }

        CatalogPlacementPart[] parts = _selectedEntities
            .Where(id => _document.TryGetEntity(id, out SmoLevelEntity? entity) &&
                         entity!.Kind == SmoLevelEntityKind.Visual &&
                         !_document.RemovedEntityIds.Contains(id))
            .Select(_document.GetEntity)
            .SelectMany(entity => entity.Parts)
            .Select(part => new CatalogPlacementPart(
                part.Asset.ObjectIndex,
                part.WorldTransform,
                part.Source.Name.TrimEnd('\0')))
            .GroupBy(part => (
                part.MeshObjectIndex,
                part.SourceWorldTransform,
                part.Name))
            .Select(group => group.First())
            .ToArray();
        if (parts.Length == 0)
            return false;
        string name = _selectedEntities.Count == 1
            ? _document.GetEntity(_selectedEntities.First()).Name
            : $"Selection_{_selectedEntities.Count}";
        clipboard = new PlacementClipboard(
            _document,
            name,
            null,
            null,
            parts,
            CalculatePlacementAnchor(parts));
        return true;
    }

    private void PastePlacementClipboard(
        PlacementClipboard clipboard,
        Vector3 target,
        string action)
    {
        if (_document is null || clipboard.SourceDocument != _document)
            return;
        try
        {
            Vector3 delta = target - clipboard.SourceAnchor;
            if (clipboard.ExternalModelId is Guid modelId &&
                clipboard.ExternalTransform is Matrix4x4 externalTransform)
            {
                externalTransform.M41 += delta.X;
                externalTransform.M42 += delta.Y;
                externalTransform.M43 += delta.Z;
                Guid id = _document.AddExternalPlacement(
                    modelId,
                    externalTransform,
                    $"{clipboard.Name}_copy");
                SelectPendingPlacement(new PendingPlacementSelection(
                    id,
                    External: true,
                    0,
                    clipboard.Name));
            }
            else
            {
                IReadOnlyList<Guid> ids = _document.AddSharedPlacements(
                    clipboard.Parts.Select(part =>
                    {
                        Matrix4x4 transform = part.SourceWorldTransform;
                        transform.M41 += delta.X;
                        transform.M42 += delta.Y;
                        transform.M43 += delta.Z;
                        return new SmoSharedPlacementRequest(
                            part.MeshObjectIndex,
                            transform,
                            $"{part.Name}_copy");
                    }).ToArray());
                Guid groupId = _document.PlacementAdditions[ids[0]].GroupId;
                SelectPendingPlacement(new PendingPlacementSelection(
                    groupId,
                    External: false,
                    0,
                    clipboard.Name));
            }
            _viewportActionStatusUntilUtc = DateTime.UtcNow.AddSeconds(3);
            StatusText.Text =
                $"{action} · {clipboard.Name} · ресурсы геометрии не дублируются";
        }
        catch (Exception exception)
        {
            StatusText.Text = $"Не удалось вставить размещение: {exception.Message}";
        }
    }

    private static Vector3 CalculatePlacementAnchor(
        IReadOnlyList<CatalogPlacementPart> parts)
    {
        if (parts.Count == 0)
            return Vector3.Zero;
        return parts.Aggregate(
                   Vector3.Zero,
                   (sum, part) => sum + GetTranslation(part.SourceWorldTransform)) /
               parts.Count;
    }

    private static Vector3 GetTranslation(Matrix4x4 transform) =>
        new(transform.M41, transform.M42, transform.M43);

    private async void CreateCollision_Click(object sender, RoutedEventArgs e)
    {
        if (_document is null || _workspace is null)
            return;
        try
        {
            SmoPreparedScene scene = _renderPreparedScene ?? _workspace.PreparedScene;
            SmoSceneMesh[] selectedMeshes;
            SmoLevelEntityId? sourceEntityId = null;
            Guid? sourcePendingPlacementId = null;
            bool sourcePendingExternal = false;
            SmoLevelGeneratedCollision? replacement = null;
            uint? projectReplacementCollisionObjectId = null;
            string sourceName;
            if (HasProject &&
                _activeCollisionEntityIndex is int projectCollisionIndex)
            {
                var collisionEntityId = new SmoLevelEntityId(projectCollisionIndex);
                SmoLevelCollisionLink? link = _document
                    .GetCollisionLinks(collisionEntityId)
                    .FirstOrDefault();
                sourceEntityId = link?.VisualEntityId;
                sourceName = _document.GetEntity(collisionEntityId).Name;
                projectReplacementCollisionObjectId =
                    _workspace.Document.Objects[projectCollisionIndex].Id;
                if (sourceEntityId is SmoLevelEntityId visualId)
                {
                    selectedMeshes = scene.Meshes
                        .Where(mesh =>
                        {
                            var placementId = new SmoPlacementId(
                                mesh.Mesh.ObjectIndex,
                                mesh.SceneObjectIndex, mesh.OccurrenceKey);
                            return _document.TryGetPlacement(
                                       placementId,
                                       out SmoEditablePlacement? placement) &&
                                   placement!.Entity.Id == visualId;
                        })
                        .Select(mesh =>
                        {
                            var placementId = new SmoPlacementId(
                                mesh.Mesh.ObjectIndex,
                                mesh.SceneObjectIndex, mesh.OccurrenceKey);
                            return _document.TryGetPlacement(
                                placementId,
                                out SmoEditablePlacement? placement)
                                    ? mesh with
                                    {
                                        WorldTransform = placement!.WorldTransform
                                    }
                                    : mesh;
                        })
                        .ToArray();
                }
                else
                {
                    selectedMeshes = [];
                }
            }
            else if (_activeCollisionEntityIndex is int activeCollisionIndex &&
                _document.GeneratedCollisions.TryGetValue(
                    new SmoLevelEntityId(activeCollisionIndex),
                    out SmoLevelGeneratedCollision? generatedCollision))
            {
                replacement = generatedCollision;
                sourceEntityId = replacement.SourceVisualEntityId;
                sourcePendingPlacementId = replacement.SourcePendingPlacementId;
                sourcePendingExternal = replacement.SourcePendingExternal;
                sourceName = replacement.Name;
                if (sourceEntityId is SmoLevelEntityId visualId)
                {
                    selectedMeshes = scene.Meshes
                        .Where(mesh =>
                        {
                            var placementId = new SmoPlacementId(
                                mesh.Mesh.ObjectIndex,
                                mesh.SceneObjectIndex, mesh.OccurrenceKey);
                            return _document.TryGetPlacement(
                                       placementId,
                                       out SmoEditablePlacement? placement) &&
                                   placement!.Entity.Id == visualId;
                        })
                        .Select(mesh =>
                        {
                            var placementId = new SmoPlacementId(
                                mesh.Mesh.ObjectIndex,
                                mesh.SceneObjectIndex, mesh.OccurrenceKey);
                            return _document.TryGetPlacement(
                                placementId,
                                out SmoEditablePlacement? placement)
                                    ? mesh with
                                    {
                                        WorldTransform = placement!.WorldTransform
                                    }
                                    : mesh;
                        })
                        .ToArray();
                }
                else if (sourcePendingPlacementId is Guid pendingId)
                {
                    selectedMeshes = scene.Meshes
                        .Where(mesh =>
                            _pendingPlacementByScene.TryGetValue(
                                mesh.SceneObjectIndex,
                                out PendingPlacementSelection? candidate) &&
                            candidate.Id == pendingId &&
                            candidate.External == sourcePendingExternal)
                        .Select(ApplyPendingPlacementTransform)
                        .ToArray();
                }
                else
                {
                    selectedMeshes = [];
                }
            }
            else if (_selectedPendingPlacement is PendingPlacementSelection pending)
            {
                selectedMeshes = scene.Meshes
                    .Where(mesh =>
                        _pendingPlacementByScene.TryGetValue(
                            mesh.SceneObjectIndex,
                            out PendingPlacementSelection? candidate) &&
                        candidate.Id == pending.Id &&
                        candidate.External == pending.External)
                    .Select(ApplyPendingPlacementTransform)
                    .ToArray();
                sourceName = pending.Name;
                sourcePendingPlacementId = pending.Id;
                sourcePendingExternal = pending.External;
            }
            else
            {
                SmoLevelEntityId[] visualIds = _selectedEntities
                    .Where(id => _document.TryGetEntity(
                                     id,
                                     out SmoLevelEntity? entity) &&
                                 entity!.Kind == SmoLevelEntityKind.Visual)
                    .ToArray();
                HashSet<SmoLevelEntityId> selected = visualIds.ToHashSet();
                selectedMeshes = scene.Meshes
                    .Where(mesh =>
                    {
                        var placementId = new SmoPlacementId(
                            mesh.Mesh.ObjectIndex,
                            mesh.SceneObjectIndex, mesh.OccurrenceKey);
                        return _document.TryGetPlacement(
                                   placementId,
                                   out SmoEditablePlacement? placement) &&
                               selected.Contains(placement!.Entity.Id);
                    })
                    .Select(mesh =>
                    {
                        var placementId = new SmoPlacementId(
                            mesh.Mesh.ObjectIndex,
                            mesh.SceneObjectIndex, mesh.OccurrenceKey);
                        return _document.TryGetPlacement(
                            placementId,
                            out SmoEditablePlacement? placement)
                                ? mesh with
                                {
                                    WorldTransform = placement!.WorldTransform
                                }
                                : mesh;
                    })
                    .ToArray();
                if (visualIds.Length == 1)
                    sourceEntityId = visualIds[0];
                sourceName = visualIds.Length == 1
                    ? _document.GetEntity(visualIds[0]).Name
                    : "Selection";
            }

            Vector3[] worldPositions = selectedMeshes
                .SelectMany(mesh => mesh.Mesh.Positions.Select(position =>
                    Vector3.Transform(position, mesh.WorldTransform)))
                .ToArray();
            if (worldPositions.Length == 0)
            {
                StatusText.Text = "Для создания коллизии нужно выделить модель на сцене";
                return;
            }

            CollisionPreviewMesh[] previewMeshes = selectedMeshes.Select(mesh =>
                new CollisionPreviewMesh(
                    mesh.Mesh.Positions.Select(position =>
                        Vector3.Transform(position, mesh.WorldTransform)).ToArray(),
                    mesh.Mesh.TriangleIndices.Select(index => checked((int)index))
                        .ToArray())).ToArray();
            var dialog = new CollisionGenerationWindow(
                worldPositions,
                previewMeshes)
            {
                Owner = this
            };
            if (dialog.ShowDialog() != true ||
                dialog.Result is not SmoGeneratedCollisionMesh generated)
            {
                return;
            }

            if (HasProject)
            {
                await AddProjectCollisionAsync(
                    generated,
                    $"Collision_{sourceName}",
                    projectReplacementCollisionObjectId);
                return;
            }

            SmoLevelEntityId collisionId;
            if (replacement is not null)
            {
                collisionId = replacement.EntityId;
                if (!_document.ReplaceGeneratedCollision(
                        collisionId,
                        generated.Positions,
                        generated.TriangleIndices))
                {
                    throw new InvalidOperationException(
                        "Selected generated collision no longer exists.");
                }
            }
            else
            {
                collisionId = _document.AddGeneratedCollision(
                    $"Collision_{sourceName}",
                    generated.Positions,
                    generated.TriangleIndices,
                    sourceEntityId,
                    sourcePendingPlacementId,
                    sourcePendingExternal);
            }
            _showCollisions = true;
            CollisionVisibilityToggle.IsChecked = true;
            BuildSceneTree();
            SelectCollisionFromTree(collisionId.SceneObjectIndex, focus: false);
            UpdateStatistics();
            UpdateCommandAvailability();
            StatusText.Text =
                $"{(replacement is null ? "Создана" : "Пересоздана")} коллизия · " +
                $"{generated.Positions.Count:N0} вершин · " +
                $"{generated.TriangleCount:N0}/{dialog.TriangleBudget:N0} треугольников";
        }
        catch (Exception exception)
        {
            StatusText.Text = $"Не удалось создать коллизию: {exception.Message}";
            MessageBox.Show(
                this,
                exception.Message,
                "SmoLVLcreator — создание коллизии",
                MessageBoxButton.OK,
                MessageBoxImage.Error);
        }
    }

    private void ViewportSurface_MouseMove(object sender, MouseEventArgs e)
    {
        Point gizmoPoint = ToGizmoPoint(e.GetPosition(ViewportSurface));
        if (HasActiveGizmoTransform)
        {
            bool snap = Keyboard.Modifiers.HasFlag(ModifierKeys.Control);
            if (_editorTool == "Вращение" && _rotationGizmoDragLayout is not null)
            {
                float radians = _rotationGizmoDragLayout.CalculateRadians(
                    _gizmoDragStart,
                    gizmoPoint);
                if (snap)
                {
                    float step = MathF.PI / 12;
                    radians = MathF.Round(radians / step) * step;
                }
                Vector3 axis = _rotationGizmoDragLayout.GetNativeAxis(
                    _activeGizmoAxis);
                Vector3 pivot = ToNativePoint(_rotationGizmoDragLayout.Pivot);
                if (_pendingGizmoTransformSession is not null)
                {
                    _pendingGizmoTransformSession.PreviewRotation(
                        axis,
                        radians,
                        pivot);
                }
                else
                {
                    _gizmoTransformSession!.PreviewRotation(
                        axis,
                        radians,
                        pivot);
                }
                StatusText.Text =
                    $"Rotate {_activeGizmoAxis} · {radians * 180 / MathF.PI:0.##}°" +
                    (snap ? " · snap 15°" : string.Empty);
            }
            else if (_editorTool == "Масштаб" && _gizmoDragLayout is not null)
            {
                float factor = _gizmoDragLayout.CalculateScaleFactor(
                    _activeGizmoAxis,
                    _gizmoDragStart,
                    gizmoPoint);
                if (snap)
                    factor = MathF.Max(0.01f, MathF.Round(factor * 10) / 10);
                Vector3 factors = _activeGizmoAxis switch
                {
                    SmoGizmoAxis.X => new Vector3(factor, 1, 1),
                    SmoGizmoAxis.Y => new Vector3(1, factor, 1),
                    SmoGizmoAxis.Z => new Vector3(1, 1, factor),
                    SmoGizmoAxis.Uniform => new Vector3(factor),
                    _ => Vector3.One
                };
                Vector3 pivot = ToNativePoint(_gizmoDragLayout.Pivot);
                if (_pendingGizmoTransformSession is not null)
                {
                    _pendingGizmoTransformSession.PreviewScale(
                        factors,
                        _gizmoDragLayout.NativeOrientation,
                        pivot);
                }
                else
                {
                    _gizmoTransformSession!.PreviewScale(
                        factors,
                        _gizmoDragLayout.NativeOrientation,
                        pivot);
                }
                StatusText.Text =
                    $"Scale {_activeGizmoAxis} · {factor:0.###}" +
                    (snap ? " · snap 0.1" : string.Empty);
            }
            else if (_gizmoDragLayout is not null)
            {
                Vector3 gizmoDelta = _gizmoDragLayout.CalculateWorldDelta(
                    _activeGizmoAxis,
                    _gizmoDragStart,
                    gizmoPoint);
                if (snap)
                {
                    float distance = gizmoDelta.Length();
                    if (distance > 1e-6f)
                        gizmoDelta *= MathF.Round(distance) / distance;
                }
                if (_pendingGizmoTransformSession is not null)
                    _pendingGizmoTransformSession.PreviewTranslation(gizmoDelta);
                else
                    _gizmoTransformSession!.PreviewTranslation(gizmoDelta);
                StatusText.Text =
                    $"Move {_activeGizmoAxis} · " +
                    $"Δ {gizmoDelta.X:0.###}, {gizmoDelta.Y:0.###}, " +
                    $"{gizmoDelta.Z:0.###}";
            }
            e.Handled = true;
            return;
        }

        if (!_viewportOrbiting && !_viewportPanning)
        {
            _hoverGizmoAxis = _editorTool switch
            {
                "Перемещение" or "Масштаб" =>
                    _gizmoLayout?.HitTest(gizmoPoint) ?? SmoGizmoAxis.None,
                "Вращение" =>
                    _rotationGizmoLayout?.HitTest(gizmoPoint) ?? SmoGizmoAxis.None,
                _ => SmoGizmoAxis.None
            };
            ViewportSurface.Cursor = _hoverGizmoAxis == SmoGizmoAxis.None
                ? null
                : Cursors.Cross;
            return;
        }

        Point current = e.GetPosition(ViewportSurface);
        System.Windows.Vector delta = current - _lastViewportMouse;
        _lastViewportMouse = current;
        if (_viewportOrbiting)
        {
            _cameraYaw -= delta.X * 0.0045;
            _cameraPitch = Math.Clamp(
                _cameraPitch + delta.Y * 0.0045,
                -Math.PI * 0.49,
                Math.PI * 0.49);
        }
        else
        {
            GetCameraBasis(out Vector3D right, out Vector3D up);
            double navigationDistance = _cameraOrbitingFocus
                ? _cameraDistance
                : Math.Max(
                    (_sceneCamera.Position - _sceneCenter).Length,
                    _sceneDiagonal * 0.25);
            double unitsPerPixel = _sceneCamera is OrthographicCamera orthographic
                ? orthographic.Width /
                  Math.Max(ViewportSurface.ActualWidth, 1)
                : 2 * navigationDistance *
                  Math.Tan(PerspectiveFieldOfView * Math.PI / 360) /
                  Math.Max(ViewportSurface.ActualHeight, 1);
            Vector3D translation = right * (-delta.X * unitsPerPixel * 0.7) +
                up * (delta.Y * unitsPerPixel * 0.7);
            _sceneCamera.Position += translation;
            if (_cameraOrbitingFocus)
            {
                _cameraTarget += translation;
                _cameraFocusKey = null;
            }
        }
        UpdateCamera();
    }

    private void ViewportSurface_MouseUp(object sender, MouseButtonEventArgs e)
    {
        if (e.ChangedButton == MouseButton.Left && HasActiveGizmoTransform)
        {
            CommitGizmoDrag();
            e.Handled = true;
            return;
        }
        if (e.ChangedButton == MouseButton.Right)
            EndViewportNavigation();
    }

    private void ViewportSurface_LostMouseCapture(object sender, MouseEventArgs e)
    {
        if (HasActiveGizmoTransform)
            CancelGizmoDrag();
        EndViewportNavigation();
    }

    private void EndViewportNavigation()
    {
        _viewportOrbiting = false;
        _viewportPanning = false;
        ViewportSurface.Cursor = null;
        if (ViewportSurface.IsMouseCaptured)
            ViewportSurface.ReleaseMouseCapture();
    }

    private void ViewportSurface_MouseWheel(object sender, MouseWheelEventArgs e)
    {
        if (_sceneCamera is OrthographicCamera orthographic)
        {
            double factor = Math.Exp(-e.Delta / 120.0 * 0.11);
            orthographic.Width = Math.Clamp(
                orthographic.Width * factor,
                Math.Max(_sceneDiagonal * 0.0005, 0.001),
                Math.Max(_sceneDiagonal * 2000, 2000));
            _cameraDistance = Math.Clamp(
                _cameraDistance * factor,
                Math.Max(_sceneDiagonal * 0.001, 0.001),
                Math.Max(_sceneDiagonal * 1000, 1000));
            _cameraDesiredDistance = _cameraDistance;
            if (_cameraOrbitingFocus)
                PositionCameraFromOrbit();
            UpdateCamera();
            e.Handled = true;
            return;
        }
        if (_cameraOrbitingFocus)
        {
            double factor = Math.Exp(-e.Delta / 120.0 * 0.11);
            _cameraDesiredDistance = Math.Clamp(
                _cameraDesiredDistance * factor,
                Math.Max(_sceneDiagonal * 0.001, 0.001),
                Math.Max(_sceneDiagonal * 1000, 1000));
        }
        else
        {
            double impulse = Math.Max(_sceneDiagonal * 0.36, 1) *
                (e.Delta / 120.0);
            _cameraVelocity += GetCameraForward() * impulse;
        }
        e.Handled = true;
    }

    private void GetCameraBasis(out Vector3D right, out Vector3D up)
    {
        Vector3D forward = _sceneCamera.LookDirection;
        forward.Normalize();
        right = Vector3D.CrossProduct(forward, _sceneCamera.UpDirection);
        if (right.LengthSquared < 1e-12)
            right = new Vector3D(1, 0, 0);
        right.Normalize();
        up = Vector3D.CrossProduct(right, forward);
        up.Normalize();
    }

    private void Window_PreviewKeyDown(object sender, KeyEventArgs e)
    {
        if (AssetFilterBox.IsKeyboardFocusWithin ||
            OutlinerFilterBox.IsKeyboardFocusWithin ||
            PositionXBox.IsKeyboardFocusWithin ||
            PositionYBox.IsKeyboardFocusWithin ||
            PositionZBox.IsKeyboardFocusWithin ||
            RotationXBox.IsKeyboardFocusWithin ||
            RotationYBox.IsKeyboardFocusWithin ||
            RotationZBox.IsKeyboardFocusWithin ||
            ScaleXBox.IsKeyboardFocusWithin ||
            ScaleYBox.IsKeyboardFocusWithin ||
            ScaleZBox.IsKeyboardFocusWithin)
        {
            return;
        }

        bool control = Keyboard.Modifiers.HasFlag(ModifierKeys.Control);
        bool shift = Keyboard.Modifiers.HasFlag(ModifierKeys.Shift);
        if (control && e.Key == Key.O)
        {
            OpenLevel_Click(this, new RoutedEventArgs());
            e.Handled = true;
            return;
        }
        if (_workspace is null)
            return;
        if (control && e.Key == Key.S)
        {
            if (shift)
                SaveLevelAs_Click(this, new RoutedEventArgs());
            else
                SaveLevel_Click(this, new RoutedEventArgs());
            e.Handled = true;
            return;
        }
        if (control && e.Key == Key.C)
        {
            CopyPlacement_Click(this, new RoutedEventArgs());
            e.Handled = true;
            return;
        }
        if (control && e.Key == Key.V)
        {
            PastePlacement_Click(this, new RoutedEventArgs());
            e.Handled = true;
            return;
        }
        if (control && e.Key == Key.D)
        {
            DuplicatePlacement_Click(this, new RoutedEventArgs());
            e.Handled = true;
            return;
        }
        if (control && e.Key == Key.E)
        {
            if (shift)
                _ = ExportSelectionAsAsync();
            else
                _ = ExportSelectionQuickAsync();
            e.Handled = true;
            return;
        }
        if (control && e.Key == Key.Z)
        {
            if (shift)
                RedoLevelEdit();
            else
                UndoLevelEdit();
            e.Handled = true;
            return;
        }
        if (control && e.Key == Key.Y)
        {
            RedoLevelEdit();
            e.Handled = true;
            return;
        }

        if (e.Key == Key.Escape)
        {
            if (HasActiveGizmoTransform)
            {
                CancelGizmoDrag();
                e.Handled = true;
                return;
            }
            ClearViewportSelection();
            StatusText.Text = "Выбор очищен";
            e.Handled = true;
            return;
        }

        if (e.Key == Key.Delete)
        {
            DeleteSelected_Click(this, new RoutedEventArgs());
            e.Handled = true;
            return;
        }

        if (e.Key == Key.F)
        {
            if (_selectedPendingPlacement is PendingPlacementSelection pending)
            {
                SmoSceneMesh[] pendingMeshes =
                    (_renderPreparedScene ?? _workspace.PreparedScene).Meshes
                    .Where(mesh =>
                        _pendingPlacementByScene.TryGetValue(
                            mesh.SceneObjectIndex,
                            out PendingPlacementSelection? candidate) &&
                        candidate.Id == pending.Id &&
                        candidate.External == pending.External)
                    .Select(ApplyPendingPlacementTransform)
                    .ToArray();
                FrameMeshes(pendingMeshes);
                StatusText.Text = $"Фокус · новое размещение {pending.Name}";
            }
            else if (AssetList.SelectedItem is CatalogItem { AssetItem.Asset: { } })
                FocusCatalogSelection();
            else if (TryCalculateBounds(
                         (_renderPreparedScene ?? _workspace.PreparedScene).Meshes,
                         out CameraBounds sceneBounds))
                FrameBounds(sceneBounds, orbit: false);
            e.Handled = true;
            return;
        }

        if (!ViewportSurface.IsKeyboardFocusWithin)
            return;

        if (!IsCameraMovementKey(e.Key))
            return;
        _cameraMovementKeys.Add(e.Key);
        e.Handled = true;
    }

    private void Window_PreviewKeyUp(object sender, KeyEventArgs e)
    {
        if (IsCameraMovementKey(e.Key))
            _cameraMovementKeys.Remove(e.Key);
    }

    private void Window_Deactivated(object? sender, EventArgs e)
    {
        _cameraMovementKeys.Clear();
        _cameraVelocity = default;
    }

    private static bool IsCameraMovementKey(Key key) =>
        key is Key.W or Key.S or Key.A or Key.D or Key.Q or Key.E;

    private void UndoLevelEdit()
    {
        if (HasProject)
        {
            _ = UndoProjectAsync();
            return;
        }
        string? description = _document?.UndoDescription;
        bool changed = _document?.Undo() == true;
        if (changed)
        {
            if (_document is not null && _selectedEntities.Any(id =>
                    !_document.TryGetEntity(id, out _)))
            {
                ClearViewportSelection();
            }
            BuildSceneTree();
            UpdateStatistics();
            UpdateCommandAvailability();
        }
        StatusText.Text = changed ? $"Undo · {description}" : "Undo · история пуста";
    }

    private void RedoLevelEdit()
    {
        if (HasProject)
        {
            _ = RedoProjectAsync();
            return;
        }
        string? description = _document?.RedoDescription;
        bool changed = _document?.Redo() == true;
        if (changed)
        {
            BuildSceneTree();
            UpdateStatistics();
            UpdateCommandAvailability();
        }
        StatusText.Text = changed ? $"Redo · {description}" : "Redo · история пуста";
    }

    private void RefreshActiveInspector()
    {
        if (_selectedPendingPlacement is PendingPlacementSelection pending)
        {
            UpdatePendingPlacementInspector(pending);
            return;
        }
        if (TryResolveActiveCollision(out SmoEditableCollision? collision))
        {
            UpdateCollisionInspector(collision!);
            UpdateCollisionSelectionCaption(collision!.Entity);
            return;
        }
        if (!TryResolveActiveSelection(
                out EditorAssetItem? activeItem,
                out _,
                out int placementIndex))
        {
            SetPositionEditor(null);
            SetRotationScaleEditor(null, null);
            return;
        }

        UpdateInspector(
            activeItem!,
            placementIndex,
            updateViewportSelection: false);
        UpdateSelectionCaption(activeItem!.Name, placementIndex);
    }

    private void ApplyPositionFromInspector(Vector3 desiredPosition)
    {
        if (_document is null ||
            (_selectedEntities.Count == 0 && _selectedPendingPlacement is null))
        {
            return;
        }

        if (_selectedPendingPlacement is PendingPlacementSelection pending &&
            TryGetPendingPlacementTransform(pending, out Matrix4x4 pendingTransform))
        {
            Vector3 current = new(
                pendingTransform.M41,
                pendingTransform.M42,
                pendingTransform.M43);
            Vector3 pendingDelta = desiredPosition - current;
            if (pendingDelta.LengthSquared() < 1e-12f)
                return;
            using SmoPendingPlacementTransformSession pendingSession =
                _document.BeginPendingPlacementTransform(
                    pending.Id,
                    pending.External);
            pendingSession.PreviewTranslation(pendingDelta);
            pendingSession.Commit(
                $"Set position of pending placement {pending.Name}");
            StatusText.Text =
                $"Position · {desiredPosition.X:0.###}, " +
                $"{desiredPosition.Y:0.###}, {desiredPosition.Z:0.###}";
            return;
        }

        SmoLevelEntity? activeEntity = null;
        if (TryResolveActiveCollision(out SmoEditableCollision? collision))
        {
            activeEntity = collision!.Entity;
        }
        else if (_activeSelectionKey is PlacementSelectionKey active &&
                 _document.TryGetPlacement(
                     new SmoPlacementId(
                         active.MeshObjectIndex,
                         active.SceneObjectIndex, active.OccurrenceKey),
                     out SmoEditablePlacement? activePlacement))
        {
            activeEntity = activePlacement!.Entity;
        }
        if (activeEntity is null)
            return;

        Matrix4x4 transform = activeEntity!.WorldTransform;
        Vector3 currentPosition = new(transform.M41, transform.M42, transform.M43);
        Vector3 delta = desiredPosition - currentPosition;
        if (delta.LengthSquared() < 1e-12f)
            return;

        IReadOnlyList<SmoLevelEntityId> transformSelection =
            GetTransformSelection();
        if (HasProject &&
            !TryCaptureProjectTransforms(
                transformSelection,
                out _,
                out string projectReason))
        {
            StatusText.Text = projectReason;
            RefreshActiveInspector();
            return;
        }
        if (!HasProject)
        {
            SmoLevelEntity[] blocked = transformSelection
                .Where(id => !_document.CanPersistTransform(id))
                .Select(_document.GetEntity)
                .ToArray();
            if (blocked.Length > 0)
            {
                StatusText.Text =
                    "Нельзя изменить SAVE BLOCKED объект: " +
                    string.Join(", ", blocked.Select(entity =>
                        $"{entity.Name} [{entity.Id.SceneObjectIndex}]"));
                RefreshActiveInspector();
                return;
            }
        }
        try
        {
            using SmoTransformSession session = _document.BeginTransform(
                transformSelection);
            session.PreviewTranslation(delta);
            session.Commit($"Set position of {transformSelection.Count:N0} entities");
            if (HasProject)
            {
                CommitCurrentProjectTransforms(
                    $"Set position of {transformSelection.Count:N0} entities",
                    transformSelection);
                return;
            }
            StatusText.Text =
                $"Position · {desiredPosition.X:0.###}, " +
                $"{desiredPosition.Y:0.###}, {desiredPosition.Z:0.###}";
        }
        catch (Exception exception) when (
            exception is InvalidOperationException or NotSupportedException)
        {
            StatusText.Text = exception.Message;
            RefreshActiveInspector();
        }
    }

    private void ApplyRotationFromInspector(Vector3 desiredDegrees)
    {
        if (_document is null)
            return;
        NumericsQuaternion desiredRotation =
            SmoEulerAngles.FromDegrees(desiredDegrees);
        if (_selectedPendingPlacement is PendingPlacementSelection pending &&
            TryGetPendingPlacementTransform(pending, out Matrix4x4 pendingTransform) &&
            Matrix4x4.Decompose(
                pendingTransform,
                out _,
                out NumericsQuaternion pendingRotation,
                out Vector3 pendingPosition))
        {
            if (!TryCreateRotationDelta(
                    pendingRotation,
                    desiredRotation,
                    out Vector3 axis,
                    out float radians))
            {
                RefreshActiveInspector();
                return;
            }
            using SmoPendingPlacementTransformSession session =
                _document.BeginPendingPlacementTransform(
                    pending.Id,
                    pending.External);
            session.PreviewRotation(axis, radians, pendingPosition);
            session.Commit($"Set rotation of pending placement {pending.Name}");
            StatusText.Text =
                $"Rotation · {desiredDegrees.X:0.###}°, " +
                $"{desiredDegrees.Y:0.###}°, {desiredDegrees.Z:0.###}°";
            return;
        }

        if (!TryResolveInspectorEntity(out SmoLevelEntity? activeEntity) ||
            !Matrix4x4.Decompose(
                activeEntity!.WorldTransform,
                out _,
                out NumericsQuaternion currentRotation,
                out Vector3 pivot) ||
            !TryCreateRotationDelta(
                currentRotation,
                desiredRotation,
                out Vector3 worldAxis,
                out float worldRadians) ||
            !TryGetWritableTransformSelection(out IReadOnlyList<SmoLevelEntityId>? selection))
        {
            RefreshActiveInspector();
            return;
        }

        try
        {
            using SmoTransformSession session = _document.BeginTransform(selection!);
            session.PreviewRotation(worldAxis, worldRadians, pivot);
            session.Commit($"Set rotation of {selection!.Count:N0} entities");
            if (HasProject)
            {
                CommitCurrentProjectTransforms(
                    $"Set rotation of {selection!.Count:N0} entities",
                    selection!);
                return;
            }
            StatusText.Text =
                $"Rotation · {desiredDegrees.X:0.###}°, " +
                $"{desiredDegrees.Y:0.###}°, {desiredDegrees.Z:0.###}°";
        }
        catch (Exception exception) when (
            exception is InvalidOperationException or NotSupportedException)
        {
            StatusText.Text = exception.Message;
            RefreshActiveInspector();
        }
    }

    private void ApplyScaleFromInspector(Vector3 desiredScale)
    {
        if (_document is null)
            return;
        if (_selectedPendingPlacement is PendingPlacementSelection pending &&
            TryGetPendingPlacementTransform(pending, out Matrix4x4 pendingTransform) &&
            Matrix4x4.Decompose(
                pendingTransform,
                out Vector3 pendingScale,
                out NumericsQuaternion pendingRotation,
                out Vector3 pendingPosition))
        {
            if (!TryCalculateScaleFactors(
                    pendingScale,
                    desiredScale,
                    out Vector3 factors))
            {
                StatusText.Text = "Scale: исходный масштаб объекта вырожден";
                RefreshActiveInspector();
                return;
            }
            using SmoPendingPlacementTransformSession session =
                _document.BeginPendingPlacementTransform(
                    pending.Id,
                    pending.External);
            session.PreviewScale(factors, pendingRotation, pendingPosition);
            session.Commit($"Set scale of pending placement {pending.Name}");
            StatusText.Text =
                $"Scale · {desiredScale.X:0.###}, " +
                $"{desiredScale.Y:0.###}, {desiredScale.Z:0.###}";
            return;
        }

        if (!TryResolveInspectorEntity(out SmoLevelEntity? activeEntity) ||
            !Matrix4x4.Decompose(
                activeEntity!.WorldTransform,
                out Vector3 currentScale,
                out NumericsQuaternion currentRotation,
                out Vector3 pivot) ||
            !TryCalculateScaleFactors(
                currentScale,
                desiredScale,
                out Vector3 worldFactors) ||
            !TryGetWritableTransformSelection(out IReadOnlyList<SmoLevelEntityId>? selection))
        {
            RefreshActiveInspector();
            return;
        }

        try
        {
            using SmoTransformSession session = _document.BeginTransform(selection!);
            session.PreviewScale(worldFactors, currentRotation, pivot);
            session.Commit($"Set scale of {selection!.Count:N0} entities");
            if (HasProject)
            {
                CommitCurrentProjectTransforms(
                    $"Set scale of {selection!.Count:N0} entities",
                    selection!);
                return;
            }
            StatusText.Text =
                $"Scale · {desiredScale.X:0.###}, " +
                $"{desiredScale.Y:0.###}, {desiredScale.Z:0.###}";
        }
        catch (Exception exception) when (
            exception is InvalidOperationException or NotSupportedException)
        {
            StatusText.Text = exception.Message;
            RefreshActiveInspector();
        }
    }

    private bool TryResolveInspectorEntity(out SmoLevelEntity? entity)
    {
        entity = null;
        if (_document is null)
            return false;
        if (TryResolveActiveCollision(out SmoEditableCollision? collision))
        {
            entity = collision!.Entity;
            return true;
        }
        if (_activeSelectionKey is not PlacementSelectionKey active ||
            !_document.TryGetPlacement(
                new SmoPlacementId(
                    active.MeshObjectIndex,
                    active.SceneObjectIndex, active.OccurrenceKey),
                out SmoEditablePlacement? placement))
        {
            return false;
        }
        entity = placement!.Entity;
        return true;
    }

    private bool TryGetWritableTransformSelection(
        out IReadOnlyList<SmoLevelEntityId>? selection)
    {
        selection = null;
        if (_document is null)
            return false;
        IReadOnlyList<SmoLevelEntityId> candidate = GetTransformSelection();
        if (HasProject)
        {
            if (!TryCaptureProjectTransforms(
                    candidate,
                    out _,
                    out string reason))
            {
                StatusText.Text = reason;
                return false;
            }
            selection = candidate;
            return candidate.Count > 0;
        }
        SmoLevelEntity[] blocked = candidate
            .Where(id => !_document.CanPersistTransform(id))
            .Select(_document.GetEntity)
            .ToArray();
        if (blocked.Length > 0)
        {
            StatusText.Text = "Нельзя изменить SAVE BLOCKED объект: " +
                string.Join(", ", blocked.Select(entity =>
                    $"{entity.Name} [{entity.Id.SceneObjectIndex}]"));
            return false;
        }
        selection = candidate;
        return candidate.Count > 0;
    }

    private static bool TryCreateRotationDelta(
        NumericsQuaternion current,
        NumericsQuaternion desired,
        out Vector3 axis,
        out float radians)
    {
        Matrix4x4 currentMatrix = Matrix4x4.CreateFromQuaternion(
            NumericsQuaternion.Normalize(current));
        if (!Matrix4x4.Invert(currentMatrix, out Matrix4x4 inverse))
        {
            axis = Vector3.UnitY;
            radians = 0;
            return false;
        }
        NumericsQuaternion delta = NumericsQuaternion.CreateFromRotationMatrix(
            inverse * Matrix4x4.CreateFromQuaternion(
                NumericsQuaternion.Normalize(desired)));
        return SmoEulerAngles.TryGetAxisAngle(delta, out axis, out radians);
    }

    private static bool TryCalculateScaleFactors(
        Vector3 current,
        Vector3 desired,
        out Vector3 factors)
    {
        factors = Vector3.One;
        if (MathF.Abs(current.X) < 1e-7f ||
            MathF.Abs(current.Y) < 1e-7f ||
            MathF.Abs(current.Z) < 1e-7f)
        {
            return false;
        }
        factors = new Vector3(
            desired.X / current.X,
            desired.Y / current.Y,
            desired.Z / current.Z);
        return factors.X > 0 && factors.Y > 0 && factors.Z > 0 &&
               float.IsFinite(factors.X) && float.IsFinite(factors.Y) &&
               float.IsFinite(factors.Z);
    }

    private void ReleaseCameraFocus()
    {
        _cameraOrbitingFocus = false;
        _cameraFocusKey = null;
        _cameraDesiredDistance = _cameraDistance;
    }

    private readonly record struct CameraBounds(Vector3 Minimum, Vector3 Maximum)
    {
        public Vector3 Size => Maximum - Minimum;
        public double Diagonal => Math.Max(Size.Length(), 0.01);
        public Point3D Center
        {
            get
            {
                Vector3 center = (Minimum + Maximum) * 0.5f;
                return new Point3D(center.X, center.Y, center.Z);
            }
        }

        public CameraBounds Include(Vector3 point) =>
            new(Vector3.Min(Minimum, point), Vector3.Max(Maximum, point));
    }


    private sealed record CollisionHit(
        SmoEditableCollision Collision,
        float Distance,
        int TriangleIndex);

    private readonly record struct CollisionRegionBounds(
        Vector3 Minimum,
        Vector3 Maximum)
    {
        public float Diagonal => Vector3.Distance(Minimum, Maximum);

        public float DistanceSquared(Vector3 point)
        {
            Vector3 closest = Vector3.Clamp(point, Minimum, Maximum);
            return Vector3.DistanceSquared(point, closest);
        }

        public CollisionRegionBounds Expand(float amount) =>
            new(Minimum - new Vector3(amount), Maximum + new Vector3(amount));

        public bool Intersects(CollisionRegionBounds other) =>
            Minimum.X <= other.Maximum.X && Maximum.X >= other.Minimum.X &&
            Minimum.Y <= other.Maximum.Y && Maximum.Y >= other.Minimum.Y &&
            Minimum.Z <= other.Maximum.Z && Maximum.Z >= other.Minimum.Z;
    }

    private readonly record struct PlacementSelectionKey(
        int MeshObjectIndex,
        int SceneObjectIndex,
        SmoRenderOccurrenceKey? OccurrenceKey = null);

    private sealed record PendingPlacementSelection(
        Guid Id,
        bool External,
        int SceneObjectIndex,
        string Name,
        Guid? PlacementId = null);

    private sealed record PlacementClipboard(
        SmoLevelDocument SourceDocument,
        string Name,
        Guid? ExternalModelId,
        Matrix4x4? ExternalTransform,
        CatalogPlacementPart[] Parts,
        Vector3 SourceAnchor);
}
