using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Media3D;
using System.Windows.Shapes;
using SmoExporter.Core;
using SmoImporter.Core;
using SmoViewer.Core;

namespace SmoImporter.Gui;

public partial class MainWindow
{
    private readonly List<RigJointScreenPoint> _wizardRigScreenPoints = [];
    private TargetRigFittingPoseSnapshot? _wizardDisplayedRigPose;
    private SmoExportScene? _wizardDisplayedRigScene;
    private Model3D? _wizardPosedSource;
    private string? _wizardRigPreviewIssue;

    private bool TryShowWizardPosedSource(SmoExportScene source, SmoDocument document)
    {
        if (!WizardShowsRigPreview || WizardRigPose is not { IsIdentityPose: false } pose)
        {
            ClearWizardRigPreview();
            return false;
        }

        if (!ReferenceEquals(pose, _wizardDisplayedRigPose) || !ReferenceEquals(source, _wizardDisplayedRigScene))
        {
            _wizardDisplayedRigPose = pose;
            _wizardDisplayedRigScene = source;
            _wizardPosedSource = null;
            _wizardRigPreviewIssue = null;
            try
            {
                _targetFittingGpuPreview ??= new TargetRigFittingGpuPreview();
                SmoExportScene posed = _targetFittingGpuPreview.Build(document, source, pose).Scene;
                _wizardRigPreviewIssue = SetInputPreview(PlacementSourceVisual,
                    () => BuildSourceInputPreview(posed, document, PlacementSourceColor), preserveOrientation: true, reframe: false);
                _wizardPosedSource = PlacementSourceVisual.Content;
            }
            catch (Exception exception) when (exception is InvalidDataException or InvalidOperationException or
                OverflowException or ArgumentException or NotSupportedException)
            {
                SessionLog.Error("WIZARD_RIG_PREVIEW", exception);
                _wizardRigPreviewIssue = "Не удалось показать модель в новой позе. Скелет остаётся видимым.";
            }
        }

        if (_wizardPosedSource is null)
            return false;
        PlacementSourceVisual.Content = _wizardPosedSource;
        return true;
    }

    private void ClearWizardRigPreview()
    {
        _wizardDisplayedRigPose = null;
        _wizardDisplayedRigScene = null;
        _wizardPosedSource = null;
        _wizardRigPreviewIssue = null;
    }

    private void RefreshWizardRigOverlay()
    {
        if (WizardRigOverlay is null || WizardRigViewportTools is null)
            return;

        WizardRigOverlay.Children.Clear();
        _wizardRigScreenPoints.Clear();
        WizardRigViewportTools.Visibility = WizardShowsRigPreview ? Visibility.Visible : Visibility.Collapsed;
        if (!WizardShowsRigPreview || WizardRigPose is not { } pose)
            return;

        IReadOnlyList<TargetRigJoint> joints = pose.Definition.Joints;
        var points = new Point?[joints.Count];
        foreach (TargetRigJoint joint in joints)
        {
            var world = pose.WorldMatrices[joint.JointIndex].Translation;
            if (!TryProjectToViewport(DonorInputViewport, DonorSelectionCamera,
                    new Point3D(world.X, world.Y, world.Z), out Point screen, out double depth))
                continue;
            points[joint.JointIndex] = screen;
            if (joint.IsDeformJoint)
                _wizardRigScreenPoints.Add(new RigJointScreenPoint(joint.JointIndex, screen, depth));
        }

        int? selected = WizardRigSelectedJointIndex;
        foreach (TargetRigJoint joint in joints)
        {
            if (joint.ParentJointIndex < 0 || points[joint.ParentJointIndex] is not { } parent ||
                points[joint.JointIndex] is not { } child)
                continue;
            bool active = joint.JointIndex == selected || joint.ParentJointIndex == selected;
            AddWizardBoneLine(parent, child, Brushes.Black, active ? 5 : 4);
            AddWizardBoneLine(parent, child, active ? Brushes.Orange : joint.IsDeformJoint ? Brushes.LightSkyBlue : Brushes.SlateGray,
                active ? 3 : 2);
        }

        foreach (RigJointScreenPoint point in _wizardRigScreenPoints)
            if (point.JointIndex != selected)
                AddWizardBoneMarker(point.Screen, Brushes.LightSkyBlue, 7);
        if (selected is int index && (uint)index < points.Length && points[index] is { } selectedPoint)
            AddWizardBoneMarker(selectedPoint, Brushes.Orange, 13);
    }

    private void AddWizardBoneLine(Point from, Point to, Brush brush, double thickness) =>
        WizardRigOverlay.Children.Add(new Line
        {
            X1 = from.X, Y1 = from.Y, X2 = to.X, Y2 = to.Y,
            Stroke = brush, StrokeThickness = thickness, StrokeStartLineCap = PenLineCap.Round, StrokeEndLineCap = PenLineCap.Round
        });

    private void AddWizardBoneMarker(Point point, Brush brush, double diameter)
    {
        var marker = new Ellipse { Width = diameter, Height = diameter, Fill = brush, Stroke = Brushes.Black, StrokeThickness = 1.5 };
        Canvas.SetLeft(marker, point.X - diameter / 2);
        Canvas.SetTop(marker, point.Y - diameter / 2);
        WizardRigOverlay.Children.Add(marker);
    }

    private bool TrySelectWizardRigJointAt(Point point) => _rigStep && !WizardBusy &&
        FindRigJointAt(point, _wizardRigScreenPoints) is { } joint && SelectWizardRigJoint(joint.JointIndex);
}
