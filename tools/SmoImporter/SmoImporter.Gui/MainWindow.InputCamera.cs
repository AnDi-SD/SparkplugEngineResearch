using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media.Media3D;
using SmoImporter.Core;

namespace SmoImporter.Gui;

public partial class MainWindow
{
    private sealed class InputCameraPose
    {
        public Point3D Center { get; set; }
        public double Radius { get; set; }
        public double FitDistance { get; set; }
        public double Distance { get; set; }
        public double Yaw { get; set; }
        public double Pitch { get; set; }
    }

    private readonly InputCameraPose _sourceInputCamera = new();
    private readonly InputCameraPose _donorInputCamera = new();
    private Point _inputCameraPointer;
    private Point _inputCameraStart;
    private bool _inputCameraDragging;
    private bool _inputCaptureStarting;
    private ReplacementTransform _shownDonorTransform = ReplacementTransform.Identity;
    private bool _showingFittingView;

    private (PerspectiveCamera Camera, InputCameraPose Pose, ModelVisual3D Visual) GetInputCamera(Border surface) =>
        surface == SourcePreviewSurface
            ? (SourceSelectionCamera, _sourceInputCamera, SourceSelectionVisual)
            : (DonorSelectionCamera, _donorInputCamera, DonorSelectionVisual);

    private void FrameInputPreview(ModelVisual3D visual, Rect3D bounds, bool preserveOrientation,
        bool resetZoom = false)
    {
        // All preview categories may be hidden. An empty or invalid extent must
        // leave the previous camera intact so showing them again restores the view.
        if (bounds.IsEmpty || !double.IsFinite(bounds.X) || !double.IsFinite(bounds.Y) || !double.IsFinite(bounds.Z) ||
            !double.IsFinite(bounds.SizeX) || !double.IsFinite(bounds.SizeY) || !double.IsFinite(bounds.SizeZ))
            return;

        Border surface = visual == SourceSelectionVisual ? SourcePreviewSurface : DonorPreviewSurface;
        var (camera, pose, _) = GetInputCamera(surface);
        double zoom = preserveOrientation && !resetZoom && pose.FitDistance > 0 ? pose.Distance / pose.FitDistance : 1;

        var center = new Point3D(bounds.X + bounds.SizeX / 2,
            bounds.Y + bounds.SizeY / 2, bounds.Z + bounds.SizeZ / 2);
        double radius = Math.Max(new Vector3D(bounds.SizeX, bounds.SizeY, bounds.SizeZ).Length / 2, 0.001);
        double fitDistance = CalculateInputFitDistance(camera, radius, new Size(surface.ActualWidth, surface.ActualHeight));
        double distance = fitDistance * zoom;
        if (!double.IsFinite(center.X) || !double.IsFinite(center.Y) || !double.IsFinite(center.Z) ||
            !double.IsFinite(radius) || !double.IsFinite(fitDistance) || !double.IsFinite(distance) ||
            fitDistance <= 0 || distance <= 0)
            return;

        pose.Center = center;
        pose.Radius = radius;
        pose.FitDistance = fitDistance;
        pose.Distance = distance;

        if (!preserveOrientation)
        {
            pose.Yaw = Math.Atan2(0.38, 1);
            pose.Pitch = Math.Atan2(0.18, Math.Sqrt(0.38 * 0.38 + 1));
        }

        ApplyInputCamera(camera, pose);
    }

    private static double CalculateInputFitDistance(PerspectiveCamera camera, double radius, Size size)
    {
        double halfHorizontalFov = camera.FieldOfView * Math.PI / 360;
        double aspect = size.Width > 0 && size.Height > 0 ? size.Width / size.Height : 1;
        double halfVerticalFov = Math.Atan(Math.Tan(halfHorizontalFov) / aspect);
        return radius / Math.Sin(Math.Min(halfHorizontalFov, halfVerticalFov)) * 1.25;
    }

    private void InputPreviewSurface_SizeChanged(object sender, SizeChangedEventArgs e)
    {
        if (_isClosing || sender is not Border surface || e.NewSize.Width <= 0 || e.NewSize.Height <= 0)
            return;
        var (camera, pose, _) = GetInputCamera(surface);
        if (pose.Radius <= 0 || pose.FitDistance <= 0)
            return;
        double zoom = pose.Distance / pose.FitDistance;
        double fitDistance = CalculateInputFitDistance(camera, pose.Radius, e.NewSize);
        double distance = fitDistance * zoom;
        if (!double.IsFinite(distance) || distance <= 0)
            return;

        // Layout changes (help/notices/window size) preserve orbit and the user's zoom.
        pose.FitDistance = fitDistance;
        pose.Distance = distance;
        ApplyInputCamera(camera, pose);
        if (surface == DonorPreviewSurface)
            RefreshWizardComparisonOverlays();
    }

    private void UpdateWizardSizePreview()
    {
        ReplacementTransform transform = WizardShowsComparison && !_showingProtectionFit
            ? _wizardGizmoPreview ?? _wizardTransform : ReplacementTransform.Identity;
        if (_shownDonorTransform != transform)
        {
            _shownDonorTransform = transform;
            var matrix = transform.Matrix;
            DonorSelectionVisual.Transform = transform == ReplacementTransform.Identity ? Transform3D.Identity :
                new MatrixTransform3D(new Matrix3D(matrix.M11, matrix.M12, matrix.M13, matrix.M14,
                    matrix.M21, matrix.M22, matrix.M23, matrix.M24, matrix.M31, matrix.M32, matrix.M33, matrix.M34,
                    matrix.M41, matrix.M42, matrix.M43, matrix.M44));
        }

        if (!WizardShowsComparison && _showingFittingView && _fullDonorPreview is { } preview)
            FrameInputPreview(DonorSelectionVisual, preview.Bounds, preserveOrientation: true, resetZoom: true);
        _showingFittingView = WizardShowsComparison;
    }

    private static void ApplyInputCamera(PerspectiveCamera camera, InputCameraPose pose)
    {
        double horizontal = Math.Cos(pose.Pitch) * pose.Distance;
        var offset = new Vector3D(Math.Sin(pose.Yaw) * horizontal,
            Math.Sin(pose.Pitch) * pose.Distance, Math.Cos(pose.Yaw) * horizontal);
        camera.Position = pose.Center + offset;
        camera.LookDirection = -offset;
        camera.NearPlaneDistance = Math.Min(pose.Radius * 0.01, pose.Distance * 0.05);
        camera.FarPlaneDistance = pose.Distance + pose.Radius * 3;
    }

    private void InputPreview_MouseDown(object sender, MouseButtonEventArgs e)
    {
        var surface = (Border)sender;
        if (WizardProtectionRectangleSelectionActive || _wizardGizmoDrag is not null)
        {
            e.Handled = true;
            return;
        }
        if (e.ChangedButton == MouseButton.Left)
        {
            if (surface == DonorPreviewSurface && (TrySelectWizardRigJointAt(e.GetPosition(DonorInputViewport)) ||
                TryBeginWizardGizmoDrag(surface, e.GetPosition(DonorInputViewport)) ||
                TryBeginWizardProtectionRectangleSelection(surface, e.GetPosition(DonorInputViewport))))
            {
                surface.Focus();
                e.Handled = true;
            }
            return;
        }
        if (e.ChangedButton != MouseButton.Right)
            return;
        if (GetInputCamera(surface).Visual.Content is null)
            return;

        _inputCameraPointer = e.GetPosition(surface);
        _inputCameraStart = _inputCameraPointer;
        _inputCameraDragging = false;
        if (!TryCaptureInputPreviewMouse(surface))
            return;
        surface.Focus();
        surface.Cursor = Cursors.SizeAll;
        e.Handled = true;
    }

    private void InputPreview_MouseMove(object sender, MouseEventArgs e)
    {
        // Capture can synchronously raise MouseMove before MouseDown returns.
        // Consume that transition with the already initialized gesture state.
        if (_inputCaptureStarting)
        {
            e.Handled = true;
            return;
        }
        var surface = (Border)sender;
        if (surface == DonorPreviewSurface && WizardProtectionRectangleSelectionActive)
        {
            UpdateWizardProtectionRectangleSelection(e.GetPosition(DonorInputViewport),
                e.LeftButton == MouseButtonState.Pressed);
            e.Handled = true;
            return;
        }
        if (!surface.IsMouseCaptured)
            return;
        if (surface == DonorPreviewSurface && _wizardGizmoDrag is not null)
        {
            if (e.LeftButton == MouseButtonState.Pressed)
                UpdateWizardGizmoDrag(e.GetPosition(DonorInputViewport));
            else
                EndWizardGizmoDrag(surface, commit: false);
            e.Handled = true;
            return;
        }
        if (e.RightButton != MouseButtonState.Pressed)
        {
            surface.ReleaseMouseCapture();
            return;
        }

        Point current = e.GetPosition(surface);
        if (!_inputCameraDragging && (current - _inputCameraStart).LengthSquared < 25)
            return;
        _inputCameraDragging = true;
        Vector delta = current - _inputCameraPointer;
        _inputCameraPointer = current;
        var (camera, pose, _) = GetInputCamera(surface);
        pose.Yaw = Math.IEEERemainder(pose.Yaw - delta.X * OrbitSensitivity, Math.Tau);
        pose.Pitch = Math.Clamp(pose.Pitch + delta.Y * OrbitSensitivity, -PitchLimit, PitchLimit);
        ApplyInputCamera(camera, pose);
        if (surface == DonorPreviewSurface)
            RefreshWizardComparisonOverlays();
        e.Handled = true;
    }

    private bool TryCaptureInputPreviewMouse(Border surface)
    {
        _inputCaptureStarting = true;
        try { return surface.CaptureMouse(); }
        finally { _inputCaptureStarting = false; }
    }

    private void InputPreview_MouseUp(object sender, MouseButtonEventArgs e)
    {
        var surface = (Border)sender;
        if (e.ChangedButton == MouseButton.Left)
        {
            if (surface == DonorPreviewSurface &&
                CompleteWizardProtectionRectangleSelection(e.GetPosition(DonorInputViewport)))
            {
                e.Handled = true;
                return;
            }
            if (surface == DonorPreviewSurface && _wizardGizmoDrag is not null)
            {
                UpdateWizardGizmoDrag(e.GetPosition(DonorInputViewport));
                EndWizardGizmoDrag(surface, commit: true);
                e.Handled = true;
            }
            return;
        }
        if (e.ChangedButton != MouseButton.Right || !surface.IsMouseCaptured ||
            WizardProtectionRectangleSelectionActive || _wizardGizmoDrag is not null)
            return;
        surface.ReleaseMouseCapture();
        e.Handled = true;
    }

    private void InputPreview_LostMouseCapture(object sender, MouseEventArgs e)
    {
        var surface = (Border)sender;
        _inputCameraDragging = false;
        if (surface == DonorPreviewSurface)
            EndWizardProtectionRectangleSelection(releaseCapture: false);
        if (surface == DonorPreviewSurface && _wizardGizmoDrag is not null)
            EndWizardGizmoDrag(surface, commit: false);
        surface.Cursor = null;
    }

    private void InputPreview_MouseWheel(object sender, MouseWheelEventArgs e)
    {
        if (WizardProtectionRectangleSelectionActive)
        {
            e.Handled = true;
            return;
        }
        var (camera, pose, visual) = GetInputCamera((Border)sender);
        if (visual.Content is null)
            return;

        double factor = Math.Exp(-e.Delta / 120.0 * 0.14);
        pose.Distance = Math.Clamp(pose.Distance * factor, pose.FitDistance * 0.1, pose.FitDistance * 10);
        ApplyInputCamera(camera, pose);
        if (ReferenceEquals(visual, DonorSelectionVisual))
            RefreshWizardComparisonOverlays();
        e.Handled = true;
    }

    private void InputPreview_KeyDown(object sender, KeyEventArgs e)
    {
        var surface = (Border)sender;
        if (surface == DonorPreviewSurface && e.Key == Key.Escape && WizardProtectionRectangleSelectionActive)
        {
            EndWizardProtectionRectangleSelection();
            e.Handled = true;
            return;
        }
        if (surface == DonorPreviewSurface && e.Key == Key.Escape && _wizardGizmoDrag is not null)
        {
            EndWizardGizmoDrag(surface, commit: false);
            e.Handled = true;
            return;
        }
        ModelVisual3D visual = GetInputCamera(surface).Visual;
        if (e.Key != Key.Home || visual.Content is null)
            return;

        if (surface == DonorPreviewSurface && WizardShowsComparison)
            FrameWizardFittedModel();
        else
            FrameInputPreview(visual, visual.Transform.TransformBounds(visual.Content.Bounds), preserveOrientation: false);
        e.Handled = true;
    }
}
