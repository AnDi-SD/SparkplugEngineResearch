using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Media3D;
using SmoImporter.Core;

namespace SmoImporter.Gui;

public partial class MainWindow
{
    private ModelTransformGizmoMode? _wizardGizmoMode;
    private ModelTransformGizmoDrag? _wizardGizmoDrag;
    private ReplacementTransform? _wizardGizmoPreview;

    private void WizardGizmoMode_Click(object sender, RoutedEventArgs e)
    {
        if (sender is Button { Tag: string tag } && Enum.TryParse(tag, out ModelTransformGizmoMode mode))
        {
            _wizardGizmoMode = _wizardGizmoMode == mode ? null : mode;
            RefreshWizardGizmoOverlay();
        }
    }

    private void WizardPlacementViewport_SizeChanged(object sender, SizeChangedEventArgs e) => RefreshWizardComparisonOverlays();

    private void RefreshWizardComparisonOverlays()
    {
        RefreshWizardGizmoOverlay();
        RefreshWizardRigOverlay();
    }

    private void RefreshWizardGizmoOverlay()
    {
        if (WizardPlacementOverlay is null || WizardGizmoToolbar is null || WizardFittingLegend is null)
            return;

        bool show = _sizeStep && _fullDonorPreview is not null;
        if (!show)
            _wizardGizmoMode = null;
        WizardGizmoToolbar.Visibility = show ? Visibility.Visible : Visibility.Collapsed;
        WizardPlacementOverlay.Children.Clear();
        WizardFittingLegend.Visibility = !_preparedStep && WizardShowsSourceComparison && _fullDonorPreview is not null ? Visibility.Visible : Visibility.Collapsed;
        if (!show)
            return;

        bool editable = !WizardBusy && !HasUnappliedWizardTransform;
        WizardMoveGizmoButton.IsEnabled = WizardRotateGizmoButton.IsEnabled = editable && WizardCanAdjustPlacement;
        WizardScaleGizmoButton.IsEnabled = editable && WizardCanAdjustScale;
        WizardMoveGizmoButton.Foreground = _wizardGizmoMode == ModelTransformGizmoMode.Translate ? Brushes.NavajoWhite : Brushes.Gainsboro;
        WizardRotateGizmoButton.Foreground = _wizardGizmoMode == ModelTransformGizmoMode.Rotate ? Brushes.NavajoWhite : Brushes.Gainsboro;
        WizardScaleGizmoButton.Foreground = _wizardGizmoMode == ModelTransformGizmoMode.Scale ? Brushes.NavajoWhite : Brushes.Gainsboro;
        if (!editable || _wizardGizmoMode is not { } mode || !CanUseWizardGizmo(mode) ||
            !TryGetWizardGizmoPivot(out var pivot, out var screen, out double depth))
            return;

        switch (mode)
        {
            case ModelTransformGizmoMode.Translate:
                DrawTranslationGizmo(pivot, screen, depth, wizard: true);
                break;
            case ModelTransformGizmoMode.Rotate:
                DrawRotationGizmo(pivot, depth, wizard: true);
                break;
            case ModelTransformGizmoMode.Scale:
                DrawScaleGizmo(screen, wizard: true);
                break;
        }
    }

    private bool CanUseWizardGizmo(ModelTransformGizmoMode mode) => mode == ModelTransformGizmoMode.Scale
        ? WizardCanAdjustScale : WizardCanAdjustPlacement;

    private void FrameWizardFittedModel()
    {
        if (DonorSelectionVisual.Content is not { } donor || donor.Bounds.IsEmpty)
            return;
        Rect3D bounds = DonorSelectionVisual.Transform.TransformBounds(donor.Bounds);
        if (PlacementSourceVisual.Content is { } source)
            bounds.Union(source.Bounds);
        if (WizardShowsRigPreview && WizardRigPose is { } pose)
        {
            foreach (var joint in pose.Definition.Joints.Where(joint => joint.IsDeformJoint))
            {
                var point = pose.WorldMatrices[joint.JointIndex].Translation;
                bounds.Union(new Point3D(point.X, point.Y, point.Z));
            }
        }
        FrameInputPreview(DonorSelectionVisual, bounds, preserveOrientation: true, resetZoom: true);
        RefreshWizardComparisonOverlays();
    }

    private void WizardFrameDonor_Click(object sender, RoutedEventArgs e)
    {
        if (WizardShowsComparison && !WizardBusy)
            FrameWizardFittedModel();
    }

    private bool TryGetWizardGizmoPivot(out Point3D pivot, out Point screen, out double depth)
    {
        Rect3D bounds = _fullDonorPreview?.Bounds ?? Rect3D.Empty;
        if (bounds.IsEmpty)
        {
            pivot = default;
            screen = default;
            depth = 0;
            return false;
        }
        pivot = DonorSelectionVisual.Transform.Transform(new Point3D(
            bounds.X + bounds.SizeX / 2, bounds.Y + bounds.SizeY / 2, bounds.Z + bounds.SizeZ / 2));
        return TryProjectToGizmoViewport(true, pivot, out screen, out depth);
    }

    private bool TryBeginWizardGizmoDrag(Border surface, Point point)
    {
        if (!_sizeStep || WizardBusy || HasUnappliedWizardTransform || _wizardGizmoMode is not { } mode ||
            !CanUseWizardGizmo(mode) || !TryGetWizardGizmoPivot(out var pivot, out var screen, out double depth) ||
            !TryResolveGizmoHandle(point, mode, pivot, screen, depth, wizard: true,
                out var axis, out Vector direction, out double worldUnitsPerPixel))
            return false;

        _wizardGizmoDrag = new ModelTransformGizmoDrag(mode, axis, point, _wizardTransform, direction, worldUnitsPerPixel);
        _wizardGizmoPreview = null;
        if (!TryCaptureInputPreviewMouse(surface))
        {
            _wizardGizmoDrag = null;
            return false;
        }
        surface.Focus();
        surface.Cursor = Cursors.Cross;
        return true;
    }

    private void UpdateWizardGizmoDrag(Point point)
    {
        if (_wizardGizmoDrag is not { } drag)
            return;
        ReplacementTransform transform = CalculateGizmoTransform(drag, point,
            MinimumWizardScalePercent / 100, MaximumWizardScalePercent / 100);
        if (!IsFiniteMatrix(transform.Matrix))
            return;
        _wizardGizmoPreview = transform;
        UpdateWizardSizePreview();
        RefreshWizardGizmoOverlay();
    }

    private void EndWizardGizmoDrag(Border surface, bool commit)
    {
        ReplacementTransform? preview = _wizardGizmoPreview;
        _wizardGizmoDrag = null;
        _wizardGizmoPreview = null;
        if (surface.IsMouseCaptured)
            surface.ReleaseMouseCapture();
        surface.Cursor = null;
        if (commit && preview is not null && preview != _wizardTransform)
            ApplyWizardTransform(preview);
        else
        {
            UpdateWizardSizePreview();
            RefreshWizardGizmoOverlay();
        }
    }

    private bool TryProjectToGizmoViewport(bool wizard, Point3D world, out Point screen, out double depth) => wizard
        ? TryProjectToViewport(DonorInputViewport, DonorSelectionCamera, world, out screen, out depth)
        : TryProjectToPreview(world, out screen, out depth);

    private double GetGizmoWorldUnitsPerPixel(bool wizard, double depth) => wizard
        ? 2 * depth * Math.Tan(DonorSelectionCamera.FieldOfView * Math.PI / 360) / Math.Max(DonorInputViewport.ActualWidth, 1)
        : GetWorldUnitsPerPixel(depth);

    private void AddGizmoElement(UIElement element, bool wizard)
    {
        if (wizard)
            WizardPlacementOverlay.Children.Add(element);
        else
        {
            RigOverlayCanvas.Children.Add(element);
            _modelTransformGizmoElements.Add(element);
        }
    }
}
