using OpenTK.Wpf;
using SmoImporter.Core;
using SmoLVLcreator.Viewport.Wpf;
using SmoViewer.Core;
using SmoViewer.Rendering.Wpf;
using SmoViewer.Scene;
using System.Globalization;
using System.Numerics;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Media3D;
using Color = System.Windows.Media.Color;
using NumericsQuaternion = System.Numerics.Quaternion;
using Point = System.Windows.Point;
using WpfVector = System.Windows.Vector;

namespace SmoLVLcreator.Gui;

public partial class ModelFitWindow : Window
{
    private readonly int _meshObjectIndex;
    private readonly Vector3[] _sourceFitPositions;
    private readonly Vector3[] _importedPositions;
    private readonly ImportedBounds _importedBounds;
    private readonly Matrix4x4 _referenceWorldTransform;
    private readonly SmoGpuSceneRenderer _gpuRenderer = new();
    private readonly SmoTranslationGizmoRenderer _translationGizmoRenderer = new();
    private readonly SmoRotationGizmoRenderer _rotationGizmoRenderer = new();
    private readonly PerspectiveCamera _camera = new()
    {
        FieldOfView = 55,
        NearPlaneDistance = 0.001,
        FarPlaneDistance = 100000,
        UpDirection = new Vector3D(0, 1, 0)
    };
    private readonly List<SmoRenderObjectKey> _originalKeys = [];
    private readonly List<SmoRenderObjectKey> _replacementKeys = [];

    private bool _gpuAvailable;
    private bool _firstFrameRendered;
    private bool _updatingFields;
    private double _dpiX = 1;
    private double _dpiY = 1;
    private Point3D _targetCenter;
    private double _targetDiagonal = 1;
    private Point3D _cameraTarget;
    private double _cameraYaw = 0.65;
    private double _cameraPitch = -0.24;
    private double _cameraDistance = 10;
    private Point _lastMouse;
    private bool _cameraOrbiting;
    private bool _cameraPanning;
    private FitTool _tool = FitTool.Move;
    private SmoTranslationGizmoLayout? _translationLayout;
    private SmoRotationGizmoLayout? _rotationLayout;
    private SmoTranslationGizmoLayout? _dragTranslationLayout;
    private SmoRotationGizmoLayout? _dragRotationLayout;
    private SmoGizmoAxis _hoverAxis;
    private SmoGizmoAxis _activeAxis;
    private Point _gizmoDragStart;
    private ReplacementTransform _gizmoStartTransform = ReplacementTransform.Identity;

    public ModelFitWindow(
        SmoDocument document,
        SmoSceneMesh sourceMesh,
        ImportedScene importedScene,
        Matrix4x4 referenceWorldTransform,
        string sourcePath)
        : this(
            document,
            sourceMesh,
            explicitSourceParts: null,
            importedScene,
            referenceWorldTransform,
            sourcePath,
            compositeName: null,
            compositePlacementCount: 0)
    {
    }

    public ModelFitWindow(
        SmoDocument document,
        IReadOnlyList<SmoSceneMesh> sourceMeshes,
        ImportedScene importedScene,
        Matrix4x4 referenceWorldTransform,
        string sourcePath,
        string compositeName,
        int compositePlacementCount)
        : this(
            document,
            sourceMeshes?.FirstOrDefault() ?? throw new ArgumentException(
                "Composite fitting needs at least one source mesh.",
                nameof(sourceMeshes)),
            sourceMeshes,
            importedScene,
            referenceWorldTransform,
            sourcePath,
            compositeName,
            compositePlacementCount)
    {
    }

    private ModelFitWindow(
        SmoDocument document,
        SmoSceneMesh sourceMesh,
        IReadOnlyList<SmoSceneMesh>? explicitSourceParts,
        ImportedScene importedScene,
        Matrix4x4 referenceWorldTransform,
        string sourcePath,
        string? compositeName,
        int compositePlacementCount)
    {
        InitializeComponent();
        _meshObjectIndex = sourceMesh.Mesh.ObjectIndex;
        _referenceWorldTransform = referenceWorldTransform;
        SourcePath = sourcePath;

        if (importedScene.Meshes.Count == 0 ||
            importedScene.Meshes.Any(mesh =>
                mesh.Positions.Length == 0 ||
                mesh.Positions.Length > ushort.MaxValue))
        {
            throw new InvalidOperationException(
                $"Каждый меш внешней модели должен содержать от 1 до " +
                $"{ushort.MaxValue:N0} вершин.");
        }

        _importedPositions = importedScene.Meshes
            .SelectMany(mesh => mesh.Positions)
            .ToArray();
        _importedBounds = CalculateImportedBounds(_importedPositions);
        SmoPreparedScene prepared = SmoSceneBuilder.Build(document);
        bool composite = explicitSourceParts is not null;
        SmoSceneMesh[] sourceParts;
        if (composite)
        {
            sourceParts = explicitSourceParts!
                .DistinctBy(mesh => (mesh.Mesh.ObjectIndex, mesh.SceneObjectIndex))
                .ToArray();
        }
        else
        {
            SmoLevelModelGraphPlan modelPlan =
                SmoLevelModelGraphReplacer.ResolvePlan(document, _meshObjectIndex);
            if (modelPlan.Components.Count != importedScene.Meshes.Count)
                throw new InvalidOperationException(
                    $"Complete model needs {modelPlan.Components.Count} imported mesh parts, " +
                    $"but {importedScene.Meshes.Count} were supplied.");
            sourceParts = modelPlan.Components.Select(component =>
                prepared.Meshes.First(mesh =>
                    mesh.Mesh.ObjectIndex == component.MeshObjectIndex &&
                    mesh.SharedInstance is null)).ToArray();
        }
        _sourceFitPositions = sourceParts
            .SelectMany(part => part.Mesh.Positions.Select(position =>
                Vector3.Transform(
                    position,
                    composite ? part.WorldTransform : referenceWorldTransform)))
            .Select(position => new Vector3(position.X, position.Y, -position.Z))
            .ToArray();
        SetTargetBounds(_sourceFitPositions);

        for (int index = 0; index < sourceParts.Length; index++)
        {
            SmoSceneMesh originalSceneMesh = sourceParts[index] with
            {
                WorldTransform = composite
                    ? sourceParts[index].WorldTransform
                    : referenceWorldTransform
            };
            var originalKey = new SmoRenderObjectKey(0, index);
            _originalKeys.Add(originalKey);
            _gpuRenderer.Add(
                originalSceneMesh,
                originalKey,
                Color.FromRgb(190, 205, 220));
            _gpuRenderer.SetAppearance(originalKey, true, false, 0.28f);
        }

        int externalTemplateIndex = composite
            ? SmoExternalLevelModelAppender.FindTemplateMeshObjectIndex(
                document,
                requireMaterial: importedScene.Textures.Count > 0)
            : -1;
        var previewTextures = new Dictionary<int, SmoTexture>();
        for (int index = 0; index < importedScene.Meshes.Count; index++)
        {
            ImportedMesh importedMesh = importedScene.Meshes[index];
            int templateObjectIndex = composite
                ? externalTemplateIndex
                : sourceParts[index].Mesh.ObjectIndex;
            SmoSceneMesh previewTemplate = composite
                ? prepared.Meshes.First(mesh =>
                    mesh.Mesh.ObjectIndex == templateObjectIndex)
                : sourceParts[index];
            var onePart = new ImportedScene(
                [importedMesh],
                importedScene.Textures,
                importedScene.Materials)
            {
                ImportWarnings = importedScene.ImportWarnings
            };
            SmoMesh previewMesh = SmoMeshResourceReplacer.CreatePreviewMesh(
                document,
                templateObjectIndex,
                onePart,
                ReplacementTransform.Identity,
                referenceWorldTransform: referenceWorldTransform,
                transientObjectIndex: int.MinValue + 1000 + index);
            SmoTexture? previewTexture = ResolvePreviewTexture(
                importedScene,
                importedMesh,
                int.MinValue + 2000 + index,
                previewTextures);
            SmoMaterialRenderStateInfo? materialState =
                SmoLevelModelGraphReplacer.CreatePreviewMaterialState(
                    importedScene,
                    importedMesh,
                    previewMesh,
                    previewTexture);
            SmoSceneMesh replacementSceneMesh = previewTemplate with
            {
                Mesh = previewMesh,
                Texture = previewTexture,
                UsesAlphaBlend = materialState?.UsesAlphaBlend ?? false,
                MaterialRenderState = materialState,
                WorldTransform = referenceWorldTransform,
                SceneObjectIndex = int.MinValue + 3000 + index,
                SharedInstance = null,
                AnimationFrames = null,
                AnimationFrameDuration = null,
                BaseTexture = null,
                SkinObjectIndex = null,
                InitialSkinMatrices = null,
                RigidNodeObjectIndex = null,
                BoneInfluences = new Dictionary<int, float>()
            };
            var replacementKey = new SmoRenderObjectKey(1, index);
            _replacementKeys.Add(replacementKey);
            _gpuRenderer.Add(replacementSceneMesh, replacementKey, Color.FromRgb(174, 188, 205));
            _gpuRenderer.SetAppearance(replacementKey, true, false, 1);
        }

        Transform = ReplacementTransformFitter.FitByHeightAndCenter(
            _sourceFitPositions,
            _importedPositions);
        ApplyTransform();
        FrameTarget();

        ModelNameText.Text = composite
            ? $"Полная замена {compositeName}"
            : $"Замена Mesh_{_meshObjectIndex}";
        SourcePathText.Text = sourcePath;
        GeometryText.Text =
            $"Внешняя модель: {importedScene.Meshes.Count:N0} мешей · " +
            $"{_importedPositions.Length:N0} вершин · " +
            $"{importedScene.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3):N0} треугольников · " +
            $"alpha-материалов: " +
            $"{importedScene.Materials.Count(material => material.UsesTextureAlpha):N0}\n" +
            (composite
                ? $"Составной ресурс: {sourceParts.Length:N0} старых частей · " +
                  $"{compositePlacementCount:N0} размещений будут переведены на новый ресурс."
                : $"Ресурс SMO: [{_meshObjectIndex}] · все его размещения получат новую геометрию.");
        WarningText.Text = importedScene.ImportWarnings.Count > 0
            ? string.Join("\n", importedScene.ImportWarnings)
            : string.Empty;
        WriteFields();

        InitializeGpuViewport();
    }

    public string SourcePath { get; }
    public ReplacementTransform Transform { get; private set; }

    private void InitializeGpuViewport()
    {
        try
        {
            FitGpuViewport.Start(new GLWpfControlSettings
            {
                MajorVersion = 3,
                MinorVersion = 3,
                Samples = 0
            });
            _gpuRenderer.SetAntialiasingSamples(4);
            _gpuAvailable = true;
            PreviewStateText.Visibility = Visibility.Visible;
            PreviewStateText.Text = "Запуск GPU-предпросмотра…";
            StatusText.Text = "Инициализация OpenGL…";
        }
        catch (Exception exception)
        {
            PreviewStateText.Text = $"OpenGL-предпросмотр недоступен: {exception.Message}";
            StatusText.Text = "Предпросмотр не построен";
        }
    }

    private void FitGpuViewport_OnRender(TimeSpan delta)
    {
        if (!_gpuAvailable)
            return;
        try
        {
            DpiScale dpi = VisualTreeHelper.GetDpi(FitGpuViewport);
            int width = Math.Max(1, (int)Math.Round(FitGpuViewport.ActualWidth * dpi.DpiScaleX));
            int height = Math.Max(1, (int)Math.Round(FitGpuViewport.ActualHeight * dpi.DpiScaleY));
            _dpiX = dpi.DpiScaleX;
            _dpiY = dpi.DpiScaleY;

            _gpuRenderer.Render(
                width,
                height,
                _camera,
                Color.FromRgb(13, 19, 26),
                0,
                _targetCenter,
                false,
                Color.FromRgb(49, 63, 78),
                _targetCenter.Y,
                Math.Max(_targetDiagonal * 2, 1),
                _targetDiagonal);

            Point3D pivot = CalculateReplacementCenter();
            _translationLayout = _tool is FitTool.Move or FitTool.Scale
                ? _translationGizmoRenderer.CreateLayout(
                    _camera,
                    width,
                    height,
                    pivot,
                    NumericsQuaternion.Identity,
                    includeUniform: _tool == FitTool.Scale)
                : null;
            _rotationLayout = _tool == FitTool.Rotate
                ? _rotationGizmoRenderer.CreateLayout(
                    _camera,
                    width,
                    height,
                    pivot,
                    NumericsQuaternion.Identity)
                : null;
            _translationGizmoRenderer.Render(
                width, height, _translationLayout, _hoverAxis, _activeAxis);
            _rotationGizmoRenderer.Render(
                width, height, _rotationLayout, _hoverAxis, _activeAxis);
            if (!_firstFrameRendered)
            {
                _firstFrameRendered = true;
                PreviewStateText.Visibility = Visibility.Collapsed;
                StatusText.Text =
                    $"{_importedPositions.Length:N0} вершин · GPU-предпросмотр · " +
                    "без перепаковки SMO";
            }
        }
        catch (Exception exception)
        {
            _gpuAvailable = false;
            PreviewStateText.Visibility = Visibility.Visible;
            PreviewStateText.Text = $"Ошибка GPU-предпросмотра: {exception.Message}";
            StatusText.Text = "Предпросмотр остановлен";
        }
    }

    private void AutoFit_Click(object sender, RoutedEventArgs e)
    {
        Transform = ReplacementTransformFitter.FitByHeightAndCenter(
            _sourceFitPositions,
            _importedPositions);
        ApplyTransform();
        WriteFields();
    }

    private void Reset_Click(object sender, RoutedEventArgs e)
    {
        Transform = ReplacementTransform.Identity;
        ApplyTransform();
        WriteFields();
    }

    private void NumericBox_LostFocus(object sender, RoutedEventArgs e)
    {
        if (_updatingFields)
            return;
        if (TryReadFields(out ReplacementTransform? transform))
        {
            Transform = transform!;
            ApplyTransform();
        }
        else
        {
            WriteFields();
        }
    }

    private void ToolButton_Checked(object sender, RoutedEventArgs e)
    {
        if (sender is not RadioButton button)
            return;
        _tool = button.Name switch
        {
            nameof(RotateToolButton) => FitTool.Rotate,
            nameof(ScaleToolButton) => FitTool.Scale,
            _ => FitTool.Move
        };
        _hoverAxis = SmoGizmoAxis.None;
    }

    private void Preview_MouseDown(object sender, MouseButtonEventArgs e)
    {
        if (e.ChangedButton == MouseButton.Left)
        {
            Point gizmoPoint = ToGizmoPoint(e.GetPosition(PreviewSurface));
            SmoGizmoAxis axis = _tool switch
            {
                FitTool.Rotate => _rotationLayout?.HitTest(gizmoPoint) ?? SmoGizmoAxis.None,
                _ => _translationLayout?.HitTest(gizmoPoint) ?? SmoGizmoAxis.None
            };
            if (axis != SmoGizmoAxis.None)
            {
                _activeAxis = axis;
                _hoverAxis = axis;
                _gizmoDragStart = gizmoPoint;
                _gizmoStartTransform = Transform;
                _dragTranslationLayout = _translationLayout;
                _dragRotationLayout = _rotationLayout;
                PreviewSurface.CaptureMouse();
                PreviewSurface.Cursor = Cursors.Cross;
                PreviewSurface.Focus();
                e.Handled = true;
            }
            return;
        }
        if (e.ChangedButton != MouseButton.Right)
            return;
        _lastMouse = e.GetPosition(PreviewSurface);
        _cameraPanning = Keyboard.Modifiers.HasFlag(ModifierKeys.Shift);
        _cameraOrbiting = !_cameraPanning;
        PreviewSurface.CaptureMouse();
        PreviewSurface.Cursor = _cameraPanning ? Cursors.SizeAll : Cursors.Hand;
        PreviewSurface.Focus();
        e.Handled = true;
    }

    private void Preview_MouseMove(object sender, MouseEventArgs e)
    {
        Point current = e.GetPosition(PreviewSurface);
        Point gizmoPoint = ToGizmoPoint(current);
        if (_activeAxis != SmoGizmoAxis.None)
        {
            PreviewGizmoDrag(gizmoPoint);
            e.Handled = true;
            return;
        }

        if (!_cameraOrbiting && !_cameraPanning)
        {
            _hoverAxis = _tool switch
            {
                FitTool.Rotate => _rotationLayout?.HitTest(gizmoPoint) ?? SmoGizmoAxis.None,
                _ => _translationLayout?.HitTest(gizmoPoint) ?? SmoGizmoAxis.None
            };
            PreviewSurface.Cursor = _hoverAxis == SmoGizmoAxis.None
                ? null
                : Cursors.Cross;
            return;
        }

        WpfVector delta = current - _lastMouse;
        _lastMouse = current;
        if (_cameraOrbiting)
        {
            _cameraYaw -= delta.X * 0.008;
            _cameraPitch = Math.Clamp(
                _cameraPitch + delta.Y * 0.008,
                -Math.PI * 0.49,
                Math.PI * 0.49);
        }
        else
        {
            GetCameraBasis(out Vector3D right, out Vector3D up);
            double unitsPerPixel =
                2 * _cameraDistance * Math.Tan(_camera.FieldOfView * Math.PI / 360) /
                Math.Max(PreviewSurface.ActualHeight, 1);
            Vector3D translation = right * (-delta.X * unitsPerPixel) +
                up * (delta.Y * unitsPerPixel);
            _cameraTarget += translation;
        }
        UpdateCamera();
    }

    private void PreviewGizmoDrag(Point current)
    {
        bool snap = Keyboard.Modifiers.HasFlag(ModifierKeys.Control);
        if (_tool == FitTool.Rotate && _dragRotationLayout is not null)
        {
            float radians = _dragRotationLayout.CalculateRadians(_gizmoDragStart, current);
            float degrees = radians * 180 / MathF.PI;
            if (snap)
                degrees = MathF.Round(degrees / 15) * 15;
            Vector3 rotation = _gizmoStartTransform.RotationDegrees;
            rotation = _activeAxis switch
            {
                SmoGizmoAxis.X => rotation with { X = rotation.X + degrees },
                SmoGizmoAxis.Y => rotation with { Y = rotation.Y + degrees },
                SmoGizmoAxis.Z => rotation with { Z = rotation.Z - degrees },
                _ => rotation
            };
            Transform = _gizmoStartTransform with { RotationDegrees = rotation };
            StatusText.Text = $"Rotate {_activeAxis} · {degrees:0.##}°" +
                (snap ? " · snap 15°" : string.Empty);
        }
        else if (_tool == FitTool.Scale && _dragTranslationLayout is not null)
        {
            float factor = _dragTranslationLayout.CalculateScaleFactor(
                _activeAxis, _gizmoDragStart, current);
            if (snap)
                factor = MathF.Max(0.01f, MathF.Round(factor * 10) / 10);
            Transform = _gizmoStartTransform with
            {
                Scale = Math.Clamp(_gizmoStartTransform.Scale * factor, 0.00001f, 100000f)
            };
            StatusText.Text = $"Scale · {factor:0.###}" +
                (snap ? " · snap 0.1" : string.Empty);
        }
        else if (_dragTranslationLayout is not null)
        {
            Vector3 nativeDelta = _dragTranslationLayout.CalculateWorldDelta(
                _activeAxis, _gizmoDragStart, current);
            Vector3 renderDelta = new(nativeDelta.X, nativeDelta.Y, -nativeDelta.Z);
            if (snap)
                renderDelta = new Vector3(
                    MathF.Round(renderDelta.X),
                    MathF.Round(renderDelta.Y),
                    MathF.Round(renderDelta.Z));
            Transform = _gizmoStartTransform with
            {
                Translation = _gizmoStartTransform.Translation + renderDelta
            };
            StatusText.Text =
                $"Move {_activeAxis} · Δ {renderDelta.X:0.###}, " +
                $"{renderDelta.Y:0.###}, {renderDelta.Z:0.###}" +
                (snap ? " · snap 1" : string.Empty);
        }
        ApplyTransform();
        WriteFields();
    }

    private void Preview_MouseUp(object sender, MouseButtonEventArgs e)
    {
        if (e.ChangedButton == MouseButton.Left && _activeAxis != SmoGizmoAxis.None)
        {
            EndGizmoDrag();
            e.Handled = true;
            return;
        }
        if (e.ChangedButton == MouseButton.Right)
            EndCameraNavigation();
    }

    private void Preview_LostMouseCapture(object sender, MouseEventArgs e)
    {
        EndGizmoDrag(releaseCapture: false);
        EndCameraNavigation(releaseCapture: false);
    }

    private void EndGizmoDrag(bool releaseCapture = true)
    {
        if (_activeAxis == SmoGizmoAxis.None)
            return;
        _activeAxis = SmoGizmoAxis.None;
        _hoverAxis = SmoGizmoAxis.None;
        _dragTranslationLayout = null;
        _dragRotationLayout = null;
        PreviewSurface.Cursor = null;
        if (releaseCapture && PreviewSurface.IsMouseCaptured)
            PreviewSurface.ReleaseMouseCapture();
    }

    private void EndCameraNavigation(bool releaseCapture = true)
    {
        _cameraOrbiting = false;
        _cameraPanning = false;
        PreviewSurface.Cursor = null;
        if (releaseCapture && PreviewSurface.IsMouseCaptured)
            PreviewSurface.ReleaseMouseCapture();
    }

    private void Preview_MouseWheel(object sender, MouseWheelEventArgs e)
    {
        double factor = Math.Exp(-e.Delta / 120.0 * 0.14);
        _cameraDistance = Math.Clamp(
            _cameraDistance * factor,
            Math.Max(_targetDiagonal * 0.001, 0.001),
            Math.Max(_targetDiagonal * 1000, 1000));
        UpdateCamera();
        e.Handled = true;
    }

    private void Preview_KeyDown(object sender, KeyEventArgs e)
    {
        if (e.Key != Key.F)
            return;
        FrameTarget();
        e.Handled = true;
    }

    private void Confirm_Click(object sender, RoutedEventArgs e)
    {
        if (!TryReadFields(out ReplacementTransform? transform))
        {
            MessageBox.Show(
                this,
                "Проверьте числовые значения трансформации.",
                "SmoLVLcreator — подгонка модели",
                MessageBoxButton.OK,
                MessageBoxImage.Warning);
            return;
        }
        Transform = transform!;
        DialogResult = true;
    }

    private void ApplyTransform()
    {
        Matrix4x4 previewModelTransform =
            SmoMeshResourceReplacer.CreatePreviewModelTransform(
                _referenceWorldTransform,
                Transform);
        foreach (SmoRenderObjectKey replacementKey in _replacementKeys)
            _gpuRenderer.SetModelTransform(replacementKey, previewModelTransform);
    }

    private static SmoTexture? ResolvePreviewTexture(
        ImportedScene scene,
        ImportedMesh mesh,
        int objectIndex,
        IDictionary<int, SmoTexture> cache)
    {
        if (mesh.MaterialIndex < 0 || mesh.MaterialIndex >= scene.Materials.Count)
            return null;
        int textureIndex = scene.Materials[mesh.MaterialIndex].BaseColorTextureIndex;
        if (textureIndex < 0 || textureIndex >= scene.Textures.Count)
            return null;
        if (cache.TryGetValue(textureIndex, out SmoTexture? cached))
            return cached;
        SmoTexture texture = SmoLevelModelGraphReplacer.CreatePreviewTexture(
            scene.Textures[textureIndex], objectIndex);
        cache.Add(textureIndex, texture);
        return texture;
    }

    private Point3D CalculateReplacementCenter()
    {
        Vector3 center = Vector3.Transform(_importedBounds.Center, Transform.Matrix);
        return new Point3D(center.X, center.Y, center.Z);
    }

    private void FrameTarget()
    {
        _cameraTarget = _targetCenter;
        double halfFov = _camera.FieldOfView * Math.PI / 360;
        _cameraDistance = Math.Max(
            _targetDiagonal * 0.65 / Math.Tan(halfFov),
            _targetDiagonal * 0.75);
        PositionCameraFromOrbit();
        UpdateCamera();
    }

    private void PositionCameraFromOrbit()
    {
        double horizontal = Math.Cos(_cameraPitch) * _cameraDistance;
        var offset = new Vector3D(
            Math.Sin(_cameraYaw) * horizontal,
            Math.Sin(_cameraPitch) * _cameraDistance,
            Math.Cos(_cameraYaw) * horizontal);
        _camera.Position = _cameraTarget + offset;
    }

    private void UpdateCamera()
    {
        PositionCameraFromOrbit();
        _camera.LookDirection = _cameraTarget - _camera.Position;
        _camera.UpDirection = new Vector3D(0, 1, 0);
        double clipScale = Math.Max(_targetDiagonal, _cameraDistance);
        _camera.NearPlaneDistance = Math.Max(clipScale / 100000, 0.001);
        _camera.FarPlaneDistance = Math.Max(clipScale * 20, 100);
    }

    private void GetCameraBasis(out Vector3D right, out Vector3D up)
    {
        Vector3D forward = _camera.LookDirection;
        forward.Normalize();
        right = Vector3D.CrossProduct(forward, _camera.UpDirection);
        if (right.LengthSquared < 1e-12)
            right = new Vector3D(1, 0, 0);
        right.Normalize();
        up = Vector3D.CrossProduct(right, forward);
        up.Normalize();
    }

    private Point ToGizmoPoint(Point point) =>
        new(point.X * _dpiX, point.Y * _dpiY);

    private void SetTargetBounds(IReadOnlyList<Vector3> positions)
    {
        if (positions.Count == 0)
        {
            _targetCenter = new Point3D();
            _targetDiagonal = 1;
            return;
        }
        Vector3 minimum = positions[0];
        Vector3 maximum = positions[0];
        for (int index = 1; index < positions.Count; index++)
        {
            minimum = Vector3.Min(minimum, positions[index]);
            maximum = Vector3.Max(maximum, positions[index]);
        }
        Vector3 center = (minimum + maximum) * 0.5f;
        _targetCenter = new Point3D(center.X, center.Y, center.Z);
        _targetDiagonal = Math.Max((maximum - minimum).Length(), 0.001f);
    }

    private static ImportedBounds CalculateImportedBounds(IReadOnlyList<Vector3> positions)
    {
        if (positions.Count == 0)
            return new ImportedBounds(-Vector3.One, Vector3.One);
        Vector3 minimum = positions[0];
        Vector3 maximum = positions[0];
        for (int index = 1; index < positions.Count; index++)
        {
            minimum = Vector3.Min(minimum, positions[index]);
            maximum = Vector3.Max(maximum, positions[index]);
        }
        return new ImportedBounds(minimum, maximum);
    }

    private bool TryReadFields(out ReplacementTransform? transform)
    {
        transform = null;
        if (!TryNumber(ScaleBox.Text, out float scale) || scale <= 0 ||
            !TryNumber(RotationXBox.Text, out float rx) ||
            !TryNumber(RotationYBox.Text, out float ry) ||
            !TryNumber(RotationZBox.Text, out float rz) ||
            !TryNumber(TranslationXBox.Text, out float tx) ||
            !TryNumber(TranslationYBox.Text, out float ty) ||
            !TryNumber(TranslationZBox.Text, out float tz))
            return false;
        transform = new ReplacementTransform(
            scale,
            new Vector3(rx, ry, rz),
            new Vector3(tx, ty, tz));
        return true;
    }

    private void WriteFields()
    {
        _updatingFields = true;
        ScaleBox.Text = Format(Transform.Scale);
        RotationXBox.Text = Format(Transform.RotationDegrees.X);
        RotationYBox.Text = Format(Transform.RotationDegrees.Y);
        RotationZBox.Text = Format(Transform.RotationDegrees.Z);
        TranslationXBox.Text = Format(Transform.Translation.X);
        TranslationYBox.Text = Format(Transform.Translation.Y);
        TranslationZBox.Text = Format(Transform.Translation.Z);
        _updatingFields = false;
    }

    private static bool TryNumber(string text, out float value) =>
        float.TryParse(text, NumberStyles.Float, CultureInfo.CurrentCulture, out value) ||
        float.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out value);

    private static string Format(float value) =>
        value.ToString("0.######", CultureInfo.CurrentCulture);

    private enum FitTool
    {
        Move,
        Rotate,
        Scale
    }

    private readonly record struct ImportedBounds(Vector3 Minimum, Vector3 Maximum)
    {
        public Vector3 Center => (Minimum + Maximum) * 0.5f;
    }
}
