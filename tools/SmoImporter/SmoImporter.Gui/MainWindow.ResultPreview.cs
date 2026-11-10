using System.Diagnostics;
using System.IO;
using System.Numerics;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Media3D;
using System.Windows.Threading;
using SmoExporter.Core;
using SmoImporter.Core;
using SmoViewer.Core;
using SmoViewer.Sparkplug;

namespace SmoImporter.Gui;

public partial class MainWindow
{
    private sealed record WizardResultSurface(SmoExportMeshPlacement Placement, SmoExportMesh Mesh,
        GeometryModel3D Model);

    private SmoDocument? _wizardResultShownDocument;
    private SmoExportScene? _wizardResultShownScene;
    private Model3DGroup? _wizardResultModel;
    private readonly List<WizardResultSurface> _wizardResultSurfaces = [];
    private TargetRigFittingGpuPreview? _wizardResultAnimationGpu;
    private SparkplugSceneRuntime? _wizardResultAnimationRuntime;
    private SparkplugAnimationClip? _wizardResultAnimationClip;
    private DispatcherTimer? _wizardResultAnimationTimer;
    private double _wizardResultAnimationTime;
    private bool _wizardResultAnimationPlaying;
    private string? _wizardResultAnimationIssue;
    private long _wizardResultAnimationLastTick;
    private double _wizardResultAnimationSpeed = 1;

    private double WizardResultAnimationTime => _wizardResultAnimationTime;
    private double WizardResultAnimationDuration => _wizardResultAnimationClip?.Duration ?? 0;
    private bool WizardResultAnimationPlaying => _wizardResultAnimationPlaying;
    private string? WizardResultAnimationIssue => _wizardResultAnimationIssue;
    private string? WizardResultPreviewIssue { get; set; }
    private double WizardResultAnimationSpeed
    {
        get => _wizardResultAnimationSpeed;
        set => _wizardResultAnimationSpeed = double.IsFinite(value) && value > 0 && value <= 4 ? value : 1;
    }
    private bool WizardResultAnimationLoop { get; set; } = true;

    private void RefreshWizardResultPreview()
    {
        if (!_preparedStep)
        {
            _wizardResultAnimationPlaying = false;
            _wizardResultAnimationTimer?.Stop();
            return;
        }
        PlacementSourceVisual.Content = null;
        WizardProtectionVisual.Content = null;
        WizardRigOverlay.Children.Clear();
        WizardRigViewportTools.Visibility = Visibility.Collapsed;
        WizardProtectionViewportTools.Visibility = Visibility.Visible;
        WizardFittingLegend.Visibility = Visibility.Collapsed;
        _showingProtectionFit = false;
        DonorSelectionVisual.Transform = Transform3D.Identity;
        _shownDonorTransform = ReplacementTransform.Identity;
        if (WizardResultDocument is not { } document || WizardResultScene is not { } scene)
        {
            DonorSelectionVisual.Content = null;
            DonorPreviewEmptyText.Text = "Готовим модель…";
            DonorPreviewEmptyText.Visibility = Visibility.Visible;
            return;
        }
        if (!ReferenceEquals(_wizardResultShownDocument, document) || !ReferenceEquals(_wizardResultShownScene, scene))
        {
            ClearWizardResultPreview();
            _wizardResultShownDocument = document;
            _wizardResultShownScene = scene;
            try
            {
                var (model, note) = BuildSourceInputPreview(scene, document, captureModel: (placement, mesh, visual) =>
                {
                    _wizardResultSurfaces.Add(new WizardResultSurface(placement, mesh, visual));
                });
                _wizardResultModel = model;
                WizardResultPreviewIssue = note;
                if (!model.Bounds.IsEmpty)
                    FrameInputPreview(DonorSelectionVisual, model.Bounds, preserveOrientation: true, resetZoom: true);
            }
            catch (Exception error) when (error is InvalidDataException or InvalidOperationException or ArgumentException or OverflowException or NotSupportedException)
            {
                SessionLog.Error("WIZARD_RESULT_PREVIEW", error);
                WizardResultPreviewIssue = "Не удалось показать готовую модель с текстурами.";
                _wizardResultModel = null;
            }
        }
        DonorSelectionVisual.Content = _wizardResultModel;
        DonorPreviewEmptyText.Visibility = _wizardResultModel is null ? Visibility.Visible : Visibility.Collapsed;
        DonorPreviewEmptyText.Text = WizardResultPreviewIssue ?? "Предпросмотр готовой модели недоступен.";
    }

    private void LoadWizardResultAnimation(string path)
    {
        PauseWizardResultAnimation();
        _wizardResultAnimationRuntime?.Dispose(); _wizardResultAnimationRuntime = null;
        _wizardResultAnimationClip?.Dispose(); _wizardResultAnimationClip = null;
        _wizardResultAnimationTime = 0;
        _wizardResultAnimationIssue = null;
        SparkplugAnimationClip? clip = null;
        SparkplugSceneRuntime? runtime = null;
        try
        {
            if (_wizardResultShownDocument is not { } document || _wizardResultShownScene is not { } scene)
                throw new InvalidOperationException("Готовая модель ещё не подготовлена.");
            clip = SparkplugAnimationClip.Load(path);
            if (!float.IsFinite(clip.Duration) || clip.Duration < 0)
                throw new InvalidDataException("Анимация имеет недопустимую длительность.");
            if (!clip.Tracks.Any(track => scene.Nodes.Any(node => node.Name.Equals(track.NodeName, StringComparison.Ordinal))))
                throw new InvalidDataException("Анимация не содержит дорожек для скелета готовой модели.");
            var skins = SmoRenderableCatalog.Get(document).ByObjectIndex.Values.Where(value => value.Skin is not null)
                .ToDictionary(value => value.ObjectIndex, value => value.Skin!);
            runtime = new SparkplugSceneRuntime(document, skins);
            foreach (string warning in runtime.Bind(clip)) SessionLog.Info("WIZARD_RESULT_ANIMATION", warning);
            _wizardResultAnimationClip = clip; clip = null;
            _wizardResultAnimationRuntime = runtime; runtime = null;
            ApplyWizardResultAnimationPose();
        }
        catch (Exception error)
        {
            runtime?.Dispose(); clip?.Dispose();
            _wizardResultAnimationRuntime?.Dispose(); _wizardResultAnimationRuntime = null;
            _wizardResultAnimationClip?.Dispose(); _wizardResultAnimationClip = null;
            RestoreWizardResultBindPose();
            _wizardResultAnimationIssue = DescribeUserError(error, "анимацию");
            SessionLog.Error("WIZARD_RESULT_ANIMATION", error);
        }
        RefreshWizardResultAnimationTime();
    }

    private void PlayWizardResultAnimation()
    {
        if (_wizardResultAnimationClip is not { Duration: > 0 } || _wizardResultAnimationRuntime is null || !_preparedStep) return;
        if (_wizardResultAnimationTime >= WizardResultAnimationDuration)
            _wizardResultAnimationTime = 0;
        _wizardResultAnimationPlaying = true;
        _wizardResultAnimationLastTick = Stopwatch.GetTimestamp();
        _wizardResultAnimationTimer ??= CreateWizardResultAnimationTimer();
        _wizardResultAnimationTimer.Start();
        RefreshWizardResultAnimationTime();
    }

    private DispatcherTimer CreateWizardResultAnimationTimer()
    {
        var timer = new DispatcherTimer(DispatcherPriority.Render) { Interval = TimeSpan.FromMilliseconds(33) };
        timer.Tick += (_, _) =>
        {
            if (!_preparedStep || !_wizardResultAnimationPlaying) { PauseWizardResultAnimation(); return; }
            long now = Stopwatch.GetTimestamp();
            double next = _wizardResultAnimationTime + Stopwatch.GetElapsedTime(_wizardResultAnimationLastTick, now).TotalSeconds * WizardResultAnimationSpeed;
            _wizardResultAnimationLastTick = now;
            double duration = WizardResultAnimationDuration;
            if (duration <= 0) { PauseWizardResultAnimation(); return; }
            if (next >= duration && WizardResultAnimationLoop) next %= duration;
            else if (next >= duration) { next = duration; PauseWizardResultAnimation(); }
            _wizardResultAnimationTime = next;
            try { ApplyWizardResultAnimationPose(); }
            catch (Exception error)
            {
                PauseWizardResultAnimation();
                _wizardResultAnimationIssue = DescribeUserError(error, "воспроизведение анимации");
                SessionLog.Error("WIZARD_RESULT_ANIMATION", error);
            }
            RefreshWizardResultAnimationTime();
        };
        return timer;
    }

    private void PauseWizardResultAnimation()
    {
        _wizardResultAnimationPlaying = false;
        _wizardResultAnimationTimer?.Stop();
    }

    private void StopWizardResultAnimation() => SeekWizardResultAnimation(0);

    private void ClearWizardResultAnimation()
    {
        PauseWizardResultAnimation();
        _wizardResultAnimationRuntime?.Dispose(); _wizardResultAnimationRuntime = null;
        _wizardResultAnimationClip?.Dispose(); _wizardResultAnimationClip = null;
        _wizardResultAnimationTime = 0;
        _wizardResultAnimationIssue = null;
        RestoreWizardResultBindPose();
        RefreshWizardResultAnimationTime();
    }

    private void SeekWizardResultAnimation(double seconds)
    {
        PauseWizardResultAnimation();
        if (!double.IsFinite(seconds)) return;
        _wizardResultAnimationTime = Math.Clamp(seconds, 0, WizardResultAnimationDuration);
        try { ApplyWizardResultAnimationPose(); }
        catch (Exception error)
        {
            _wizardResultAnimationIssue = DescribeUserError(error, "воспроизведение анимации");
            SessionLog.Error("WIZARD_RESULT_ANIMATION", error);
        }
        RefreshWizardResultAnimationTime();
    }

    private void ApplyWizardResultAnimationPose()
    {
        if (_wizardResultAnimationRuntime is not { } runtime || _wizardResultShownDocument is not { } document ||
            _wizardResultShownScene is not { } scene) return;
        runtime.Sample((float)_wizardResultAnimationTime);
        _wizardResultAnimationGpu ??= new TargetRigFittingGpuPreview();
        var positions = _wizardResultAnimationGpu.ReadAnimatedPositions(document, scene, runtime);
        var nodes = scene.Nodes.ToDictionary(node => node.ObjectIndex);
        foreach (WizardResultSurface surface in _wizardResultSurfaces)
        {
            var geometry = (MeshGeometry3D)surface.Model.Geometry;
            if (surface.Mesh.SkinObjectIndex is not null && positions.TryGetValue(surface.Mesh.VariantKey, out Vector3[]? values))
            {
                geometry.Positions = new Point3DCollection(values.Select(value => new Point3D(value.X, value.Y, value.Z)));
                geometry.Normals = new Vector3DCollection();
            }
            else if (surface.Placement.ParentNodeObjectIndex is int node && runtime.Worlds.TryGetValue(node, out Matrix4x4 world) &&
                nodes.TryGetValue(node, out SmoExportNode? bind) && Matrix4x4.Invert(bind.BindWorldMatrix, out Matrix4x4 inverse))
                surface.Model.Transform = WizardResultTransform(surface.Placement.WorldMatrix * inverse * SmoExportCoordinateSystem.ToExportMatrix(world));
        }
    }

    private void RestoreWizardResultBindPose()
    {
        foreach (WizardResultSurface surface in _wizardResultSurfaces)
        {
            var geometry = (MeshGeometry3D)surface.Model.Geometry;
            geometry.Positions = new Point3DCollection(surface.Mesh.Positions.Select(value => new Point3D(value.X, value.Y, value.Z)));
            geometry.Normals = new Vector3DCollection(surface.Mesh.Normals.Select(value => new Vector3D(value.X, value.Y, value.Z)));
            surface.Model.Transform = surface.Mesh.SkinObjectIndex is null ? WizardResultTransform(surface.Placement.WorldMatrix) : Transform3D.Identity;
        }
    }

    private static Transform3D WizardResultTransform(Matrix4x4 value) => value == Matrix4x4.Identity ? Transform3D.Identity :
        new MatrixTransform3D(new Matrix3D(value.M11, value.M12, value.M13, value.M14, value.M21, value.M22, value.M23, value.M24,
            value.M31, value.M32, value.M33, value.M34, value.M41, value.M42, value.M43, value.M44));

    private void ClearWizardResultPreview()
    {
        PauseWizardResultAnimation();
        if (_wizardResultModel is not null && ReferenceEquals(DonorSelectionVisual.Content, _wizardResultModel))
            DonorSelectionVisual.Content = _fullDonorPreview;
        _wizardResultAnimationRuntime?.Dispose(); _wizardResultAnimationRuntime = null;
        _wizardResultAnimationClip?.Dispose(); _wizardResultAnimationClip = null;
        _wizardResultAnimationGpu?.Dispose(); _wizardResultAnimationGpu = null;
        _wizardResultAnimationTime = 0;
        _wizardResultAnimationIssue = null;
        WizardResultPreviewIssue = null;
        _wizardResultShownDocument = null; _wizardResultShownScene = null;
        _wizardResultModel = null; _wizardResultSurfaces.Clear();
    }
}
