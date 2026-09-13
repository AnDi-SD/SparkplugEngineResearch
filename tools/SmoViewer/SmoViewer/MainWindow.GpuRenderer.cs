using OpenTK.Wpf;
using SmoViewer.Core;
using SmoViewer.Rendering.Wpf;
using SmoViewer.Scene;
using System.IO;
using System.Numerics;
using System.Windows.Media;
using System.Windows.Media.Media3D;
using Color = System.Windows.Media.Color;

namespace SmoViewer;

/// <summary>
/// Viewer-specific GL host and adapter. Rendering behavior lives in the shared
/// SmoViewer.Rendering.Wpf module; this partial only supplies window state.
/// </summary>
public partial class MainWindow
{
    private readonly SmoGpuSceneRenderer _gpuRenderer = new();
    private bool _gpuRendererAvailable;
    private int _gpuAntialiasingSamples = 4;
    private System.Windows.Point? _pendingGpuPickPoint;
    private readonly HashSet<string> _reportedMaterialIssues = [];

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
            _gpuRenderer.SetAntialiasingSamples(_gpuAntialiasingSamples);
            _gpuRendererAvailable = true;
            AddLog(
                "GPU-preview включён через общий SmoViewer.Rendering.Wpf: " +
                "исходные texture, UV и vertex diffuse передаются видеокарте " +
                "без CPU triangle-atlas; MSAA 4× включён.");
        }
        catch (Exception exception)
        {
            _gpuRendererAvailable = false;
            GpuViewport.Visibility = System.Windows.Visibility.Collapsed;
            AddLog(
                $"ОШИБКА: GPU-preview недоступен ({exception.Message}). " +
                "Обычные 3D-модели не будут открываться.");
        }
    }

    private void GpuViewport_OnRender(TimeSpan delta)
    {
        if (!_gpuRendererAvailable)
            return;

        try
        {
            System.Windows.DpiScale dpi = VisualTreeHelper.GetDpi(GpuViewport);
            int width = Math.Max(
                1,
                (int)Math.Round(GpuViewport.ActualWidth * dpi.DpiScaleX));
            int height = Math.Max(
                1,
                (int)Math.Round(GpuViewport.ActualHeight * dpi.DpiScaleY));
            ProjectionCamera camera = _guiPreviewActive
                ? _guiCamera
                : SceneCamera;
            _gpuRenderer.Render(
                width,
                height,
                camera,
                _sceneBackgroundColor,
                _modelTurntableEnabled ? _modelTurntableAngle : 0,
                _sceneBounds.HasValue ? _sceneBounds.Center : new Point3D(),
                ShowFloorGridCheck?.IsChecked == true && !_guiPreviewActive,
                _floorGridColor,
                _sceneBounds.HasValue ? _sceneBounds.MinY : 0,
                _sceneBounds.HasValue
                    ? Math.Max(
                        Math.Max(_sceneBounds.Width, _sceneBounds.Depth),
                        _sceneBounds.DiagonalLength * 0.25)
                    : 20,
                _sceneBounds.HasValue ? _sceneBounds.DiagonalLength : 0);

            CompletePendingGpuPick();
            foreach (string issue in _gpuRenderer.RenderIssues)
                if (_reportedMaterialIssues.Add(issue)) AddLog(issue);

            if (_gpuRenderer.ConsumeUploadReport() is GpuUploadReport report)
            {
                AddLog(
                    $"GPU: загружено {report.GeometryCount} unique mesh buffers, " +
                    $"{report.TextureCount} unique textures за " +
                    $"{report.ElapsedMilliseconds:F1} ms; placements " +
                    $"{report.PlacementCount}.");
            }
            if (_gpuRenderer.ConsumeAntialiasingReport() is
                GpuAntialiasingReport antialiasing)
            {
                AddLog(antialiasing.ActualSamples > 1
                    ? $"Сглаживание OpenGL: MSAA {antialiasing.ActualSamples}×" +
                      (antialiasing.ActualSamples == antialiasing.RequestedSamples
                          ? "."
                          : $" (запрошено {antialiasing.RequestedSamples}×, " +
                            "ограничено возможностями GPU).")
                    : "Сглаживание OpenGL отключено.");
            }
        }
        catch (Exception exception)
        {
            if (_pendingGpuPickPoint.HasValue)
            {
                _pendingGpuPickPoint = null;
                AddLog("Выбор под курсором отменён: GPU frame недоступен; " +
                    "выбор через дерево остаётся доступен.");
            }
            _gpuRendererAvailable = false;
            GpuViewport.Visibility = System.Windows.Visibility.Collapsed;
            AddLog(
                $"ОШИБКА GPU-preview: {exception.Message}. Рендер остановлен.");
        }
    }

    private void RequireGpuRenderer()
    {
        if (!_gpuRendererAvailable)
        {
            throw new InvalidOperationException(
                "GPU renderer is unavailable; the obsolete WPF model fallback was removed.");
        }
    }

    private void AddDirectGpuMesh(
        SmoSceneMesh renderMesh,
        SceneObjectKey key,
        Color fallbackColor)
    {
        _gpuRenderer.Add(
            renderMesh,
            ToSharedRenderKey(key),
            fallbackColor);
    }

    private static SmoRenderObjectKey ToSharedRenderKey(SceneObjectKey key) =>
        new(key.FileIndex, key.ObjectIndex, key.OccurrenceKey);

    private void ResetDirectGpuScene()
    {
        _pendingGpuPickPoint = null;
        _gpuRenderer.Clear();
        _reportedMaterialIssues.Clear();
    }

    private void SelectObjectAt(System.Windows.Point viewportPoint)
    {
        if (_gpuRendererAvailable)
        {
            // GL readback needs the context current in OnRender. Keep only the
            // latest requested click if several arrive before the next frame.
            _pendingGpuPickPoint = viewportPoint;
            return;
        }

        if (_sceneGeometry.Any(pair => pair.Value.RenderMesh.Mesh.HasSkinningData &&
            IsGuiGeometryVisible(pair.Key, pair.Value)))
        {
            AddLog("Скелетный picking недоступен без GPU readback; " +
                "выбор через дерево остаётся доступен.");
        }
        SelectObjectAtPrepared(viewportPoint, new HashSet<SceneObjectKey>());
    }

    private void CompletePendingGpuPick()
    {
        if (_pendingGpuPickPoint is not System.Windows.Point viewportPoint)
            return;
        _pendingGpuPickPoint = null;

        var readableSkins = new HashSet<SceneObjectKey>();
        foreach ((SceneObjectKey key, SceneGeometry scene) in _sceneGeometry)
        {
            SmoMesh mesh = scene.RenderMesh.Mesh;
            if (!mesh.HasSkinningData || !IsGuiGeometryVisible(key, scene))
                continue;
            try
            {
                if (!scene.GpuRendered)
                    throw new InvalidOperationException("mesh has no GPU rendering instance");
                Vector3[] localPositions = _gpuRenderer.ReadPositionsForPicking(
                    ToSharedRenderKey(key), mesh.ObjectIndex);
                if (localPositions.Length != mesh.VertexCount)
                    throw new InvalidDataException("GPU position count differs from mesh vertex count");
                if (localPositions.Any(value => !IsFinitePickPosition(value)))
                    throw new InvalidDataException("GPU position snapshot contains non-finite coordinates");

                int[]? sourceIndices = scene.SourceVertexIndices;
                var positions = new Point3DCollection(sourceIndices?.Length ?? localPositions.Length);
                IEnumerable<int> vertices = sourceIndices ?? Enumerable.Range(0, localPositions.Length);
                foreach (int vertex in vertices)
                {
                    if ((uint)vertex >= (uint)localPositions.Length)
                        throw new InvalidDataException("Picking companion source index exceeds GPU positions");
                    Vector3 world = Vector3.Transform(localPositions[vertex], scene.RenderMesh.WorldTransform);
                    if (!IsFinitePickPosition(world))
                        throw new InvalidDataException("Picking world transform produces non-finite coordinates");
                    positions.Add(new Point3D(world.X, world.Y, -world.Z));
                }
                scene.Geometry.Positions = positions;
                readableSkins.Add(key);
            }
            catch (Exception exception)
            {
                AddLog($"Скелетный picking object [{key.ObjectIndex}] недоступен: " +
                    $"{exception.Message}. Выбор через дерево остаётся доступен.");
            }
        }
        // Only companions refreshed during this GL frame may contribute skin
        // hits. Failed readbacks never reuse their previous or raw positions.
        SelectObjectAtPrepared(viewportPoint, readableSkins);
    }

    private static bool IsFinitePickPosition(Vector3 value) =>
        float.IsFinite(value.X) && float.IsFinite(value.Y) && float.IsFinite(value.Z);

    private bool HasDirectGpuScene => _gpuRenderer.HasItems;

    private void UpdateDirectGpuAppearance(
        SceneObjectKey key,
        bool visible,
        bool highlighted,
        double opacity)
    {
        _gpuRenderer.SetAppearance(
            ToSharedRenderKey(key),
            visible,
            highlighted,
            (float)Math.Clamp(opacity, 0, 1));
    }

    private void UpdateDirectGpuSkinning(
        SceneObjectKey key,
        IReadOnlyList<Matrix4x4> boneMatrices) =>
        _gpuRenderer.SetBoneMatrices(ToSharedRenderKey(key), boneMatrices);

    private void UpdateDirectGpuModelTransform(
        SceneObjectKey key,
        Matrix4x4 modelTransform) =>
        _gpuRenderer.SetModelTransform(ToSharedRenderKey(key), modelTransform);

    private void GpuAntialiasingCombo_SelectionChanged(
        object sender,
        System.Windows.Controls.SelectionChangedEventArgs e)
    {
        if (GpuAntialiasingCombo?.SelectedItem is not
            System.Windows.Controls.ComboBoxItem item ||
            !int.TryParse(item.Tag?.ToString(), out int samples))
        {
            return;
        }

        _gpuAntialiasingSamples = samples;
        _gpuRenderer.SetAntialiasingSamples(samples);
    }
}
