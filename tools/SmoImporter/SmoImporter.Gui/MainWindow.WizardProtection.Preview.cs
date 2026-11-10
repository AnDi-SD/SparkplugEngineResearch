using System.Numerics;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Media3D;
using SmoImporter.Core;

namespace SmoImporter.Gui;

public partial class MainWindow
{
    private ImportedScene? _shownProtectionScene;
    private Model3DGroup? _protectionDonorModel;
    private readonly Dictionary<GeometryModel3D, int> _protectionMeshIndices = [];
    private bool _showingProtectionFit;
    private Point? _wizardProtectionRectangleStart;
    private bool _wizardProtectionRectangleDragging;
    private bool _wizardProtectionRectangleToggle;

    private bool WizardProtectionRectangleSelectionActive => _wizardProtectionRectangleStart is not null;

    private void RefreshWizardProtectionPreview()
    {
        if (WizardProtectionVisual is null || WizardProtectionViewportTools is null)
            return;

        bool donorOnly = _postRigStep || _preparedStep && !WizardShowsSourceComparison;
        RefreshWizardProtectionVisibility(donorOnly);
        WizardProtectionViewportTools.Visibility = donorOnly ? Visibility.Visible : Visibility.Collapsed;
        WizardProtectionVisual.Content = null;
        ImportedScene? scene = donorOnly ? WizardProtectionFittingScene : null;
        if (scene is null)
        {
            EndWizardProtectionRectangleSelection();
            if (_showingProtectionFit)
                DonorSelectionVisual.Content = _fullDonorPreview;
            _showingProtectionFit = false;
            _shownProtectionScene = null;
            _protectionDonorModel = null;
            _protectionMeshIndices.Clear();
            _shownProtectionVisibility = null;
            _shownProtectionVisibilityState = null;
            _shownProtectionVisibilityAnalysis = null;
            return;
        }

        GeneratedSkinningAnalysis? analysis = WizardProtectionPreparation?.Analysis;
        WizardProtectionVisibility visibility = ReadWizardProtectionVisibility();
        if (!ReferenceEquals(_shownProtectionScene, scene) || _shownProtectionVisibility != visibility ||
            _shownProtectionVisibilityState != _wizardProtectionState ||
            !ReferenceEquals(_shownProtectionVisibilityAnalysis, analysis))
        {
            EndWizardProtectionRectangleSelection();
            _shownProtectionScene = scene;
            _shownProtectionVisibility = visibility;
            _shownProtectionVisibilityState = _wizardProtectionState;
            _shownProtectionVisibilityAnalysis = analysis;
            _protectionMeshIndices.Clear();
            uint[][] triangles = FilterWizardProtectionTriangles(scene, analysis);
            var group = new Model3DGroup();
            for (int index = 0; index < scene.Meshes.Count; index++)
            {
                ImportedMesh mesh = scene.Meshes[index];
                var model = AddInputMesh(group, mesh.Positions, mesh.Normals, triangles[index], [], null, null, PlacementDonorColor);
                if (model is not null)
                    _protectionMeshIndices.Add(model, index);
            }
            group.Freeze();
            _protectionDonorModel = group;
        }

        DonorSelectionVisual.Content = _protectionDonorModel;
        _showingProtectionFit = true;
        if (_postRigStep && analysis is not null)
            ShowWizardProtectionOverlay(scene, analysis);
    }

    private void ShowWizardProtectionOverlay(ImportedScene scene, GeneratedSkinningAnalysis analysis)
    {
        var group = new Model3DGroup();
        if (WizardProtectionSelectedPlane is { } selectedPlane && analysis.SemanticRegions.SeparationPlanes
                .FirstOrDefault(plane => plane.Kind == selectedPlane && plane.IsAvailable) is { } plane)
        {
            var adjustment = GetWizardProtectionPlaneAdjustment(selectedPlane);
            if (adjustment != plane.Adjustment)
                plane = ApplySeparationPlaneDraft(plane, adjustment);
            AddSeparationPlaneModel(group, plane, Matrix4x4.Identity, plane.IsEnabled ? Colors.Gold : Colors.SlateGray, selected: true);
        }

        foreach (var selectedHand in WizardProtectionSelectedHands)
        {
            var hand = analysis.SemanticRegions.Regions.FirstOrDefault(region => region.Region == selectedHand);
            if (hand?.AutomaticVolume is not { } automaticVolume)
                continue;
            var adjustment = GetWizardProtectionHandAdjustment(selectedHand);
            var volume = adjustment == hand.Adjustment ? hand.ResolvedVolume ?? automaticVolume :
                ApplyProtectedRegionDraftVolume(automaticVolume, adjustment);
            AddProtectedRegionWireframe(group, selectedHand, volume, Matrix4x4.Identity,
                adjustment.Enabled ? Colors.DeepSkyBlue : Colors.SlateGray);
        }

        var selected = WizardProtectionSelectedComponentIndices;
        if (selected.Count > 0)
        {
            foreach (GeneratedSkinningComponentInfo component in analysis.Components.Where(component =>
                         selected.Contains(component.ComponentIndex) && IsWizardProtectionComponentVisible(component)))
            foreach (TargetRigBodyVertexMembership membership in component.VerticesByMesh)
            {
                if ((uint)membership.MeshIndex >= scene.Meshes.Count)
                    continue;
                ImportedMesh mesh = scene.Meshes[membership.MeshIndex];
                var vertices = membership.VertexIndices.ToHashSet();
                var triangles = new List<uint>();
                for (int index = 0; index + 2 < mesh.TriangleIndices.Length; index += 3)
                {
                    uint a = mesh.TriangleIndices[index], b = mesh.TriangleIndices[index + 1], c = mesh.TriangleIndices[index + 2];
                    if (vertices.Contains((int)a) && vertices.Contains((int)b) && vertices.Contains((int)c))
                        triangles.AddRange([a, b, c]);
                }
                if (triangles.Count == 0)
                    continue;
                Vector3 center = membership.VertexIndices.Where(vertex => (uint)vertex < mesh.Positions.Length)
                    .Select(vertex => mesh.Positions[vertex]).Aggregate(Vector3.Zero, (sum, point) => sum + point) / vertices.Count;
                Vector3[] highlight = mesh.Positions.Select(point => center + (point - center) * 1.008f).ToArray();
                AddInputMesh(group, highlight, mesh.Normals, triangles.ToArray(), [], null, null, Colors.Goldenrod);
            }
        }
        group.Freeze();
        WizardProtectionVisual.Content = group;
    }

    private bool TrySelectWizardProtectionComponentAt(Point point) =>
        FindWizardProtectionComponentAt(point) is int index && SelectWizardProtectionComponent(index);

    private int? FindWizardProtectionComponentAt(Point point)
    {
        if (!_postRigStep || !_showingProtectionFit || WizardProtectionPreparation?.Analysis is not { } analysis)
            return null;

        int? selected = null;
        VisualTreeHelper.HitTest(DonorInputViewport, filterCallback: null, resultCallback: result =>
        {
            if (result is not RayMeshGeometry3DHitTestResult hit || hit.ModelHit is not GeometryModel3D model ||
                !_protectionMeshIndices.TryGetValue(model, out int meshIndex))
                return HitTestResultBehavior.Continue;

            selected = analysis.Components.FirstOrDefault(component => IsWizardProtectionComponentVisible(component) && component.VerticesByMesh.Any(membership =>
                membership.MeshIndex == meshIndex && membership.VertexIndices.Contains(hit.VertexIndex1) &&
                membership.VertexIndices.Contains(hit.VertexIndex2) && membership.VertexIndices.Contains(hit.VertexIndex3)))?.ComponentIndex;
            return HitTestResultBehavior.Stop;
        }, new PointHitTestParameters(point));
        return selected;
    }

    private bool TryBeginWizardProtectionRectangleSelection(Border surface, Point point)
    {
        if (surface != DonorPreviewSurface || !CanEditWizardProtection || !_showingProtectionFit ||
            _shownProtectionScene is null)
            return false;

        _wizardProtectionRectangleStart = point;
        _wizardProtectionRectangleDragging = false;
        _wizardProtectionRectangleToggle = Keyboard.Modifiers.HasFlag(ModifierKeys.Control);
        WizardProtectionSelectionRectangle.Visibility = Visibility.Collapsed;
        if (!TryCaptureInputPreviewMouse(surface))
        {
            EndWizardProtectionRectangleSelection(releaseCapture: false);
            return false;
        }
        surface.Focus();
        surface.Cursor = Cursors.Cross;
        return true;
    }

    private bool UpdateWizardProtectionRectangleSelection(Point point, bool selectionPressed)
    {
        if (_wizardProtectionRectangleStart is not Point start)
            return false;
        if (!selectionPressed || !CanEditWizardProtection || !_showingProtectionFit)
        {
            EndWizardProtectionRectangleSelection();
            return true;
        }
        _wizardProtectionRectangleDragging |= (point - start).LengthSquared >= 25;
        if (_wizardProtectionRectangleDragging)
        {
            Rect rectangle = NormalizeSelectionRectangle(start, point);
            Canvas.SetLeft(WizardProtectionSelectionRectangle, rectangle.Left);
            Canvas.SetTop(WizardProtectionSelectionRectangle, rectangle.Top);
            WizardProtectionSelectionRectangle.Width = rectangle.Width;
            WizardProtectionSelectionRectangle.Height = rectangle.Height;
            WizardProtectionSelectionRectangle.Visibility = Visibility.Visible;
        }
        return true;
    }

    private bool CompleteWizardProtectionRectangleSelection(Point point)
    {
        if (_wizardProtectionRectangleStart is not Point start)
            return false;

        bool dragged = _wizardProtectionRectangleDragging || (point - start).LengthSquared >= 25;
        bool toggle = _wizardProtectionRectangleToggle;
        EndWizardProtectionRectangleSelection();
        if (!CanEditWizardProtection || !_showingProtectionFit)
            return true;
        if (dragged)
            SelectWizardProtectionComponents(FindWizardProtectionComponentsInRectangle(NormalizeSelectionRectangle(start, point)), toggle);
        else if (FindWizardProtectionComponentAt(point) is int selected)
            SelectWizardProtectionComponents([selected], toggle);
        return true;
    }

    private void EndWizardProtectionRectangleSelection(bool releaseCapture = true)
    {
        bool wasActive = WizardProtectionRectangleSelectionActive;
        _wizardProtectionRectangleStart = null;
        _wizardProtectionRectangleDragging = false;
        _wizardProtectionRectangleToggle = false;
        if (WizardProtectionSelectionRectangle is not null)
            WizardProtectionSelectionRectangle.Visibility = Visibility.Collapsed;
        if (wasActive && DonorPreviewSurface is { } surface)
        {
            surface.Cursor = null;
            if (releaseCapture && surface.IsMouseCaptured)
                surface.ReleaseMouseCapture();
        }
    }

    private int[] FindWizardProtectionComponentsInRectangle(Rect rectangle)
    {
        if (_shownProtectionScene is not { } scene || WizardProtectionPreparation?.Analysis is not { } analysis)
            return [];

        // Project each mesh only once. Test its actual component triangles:
        // the body bounding box can overlap a wing-only selection rectangle.
        var projected = new Dictionary<int, Point?[]>();
        var captured = new List<int>();
        foreach (GeneratedSkinningComponentInfo component in analysis.Components)
        {
            if (!IsWizardProtectionComponentVisible(component))
                continue;
            bool intersects = false;
            foreach (TargetRigBodyVertexMembership membership in component.VerticesByMesh)
            {
                if ((uint)membership.MeshIndex >= (uint)scene.Meshes.Count)
                    continue;
                ImportedMesh mesh = scene.Meshes[membership.MeshIndex];
                if (!projected.TryGetValue(membership.MeshIndex, out Point?[]? points))
                {
                    points = mesh.Positions.Select(position =>
                    {
                        Point3D world = DonorSelectionVisual.Transform.Transform(new Point3D(position.X, position.Y, position.Z));
                        return TryProjectToViewport(DonorInputViewport, DonorSelectionCamera, world,
                            out Point screen, out _, requireInside: false) ? (Point?)screen : null;
                    }).ToArray();
                    projected.Add(membership.MeshIndex, points);
                }
                var members = membership.VertexIndices.ToHashSet();
                for (int index = 0; index + 2 < mesh.TriangleIndices.Length; index += 3)
                {
                    int a = checked((int)mesh.TriangleIndices[index]);
                    int b = checked((int)mesh.TriangleIndices[index + 1]);
                    int c = checked((int)mesh.TriangleIndices[index + 2]);
                    if (!members.Contains(a) || !members.Contains(b) || !members.Contains(c) ||
                        (uint)a >= (uint)points.Length || (uint)b >= (uint)points.Length || (uint)c >= (uint)points.Length ||
                        points[a] is not Point first || points[b] is not Point second || points[c] is not Point third)
                        continue;
                    if (WizardProtectionTriangleIntersectsRectangle(first, second, third, rectangle))
                    {
                        intersects = true;
                        break;
                    }
                }
                if (intersects)
                    break;
            }
            if (intersects)
                captured.Add(component.ComponentIndex);
        }
        return captured.ToArray();
    }

    private static bool WizardProtectionTriangleIntersectsRectangle(Point first, Point second, Point third, Rect rectangle)
    {
        if (Math.Max(first.X, Math.Max(second.X, third.X)) < rectangle.Left ||
            Math.Min(first.X, Math.Min(second.X, third.X)) > rectangle.Right ||
            Math.Max(first.Y, Math.Max(second.Y, third.Y)) < rectangle.Top ||
            Math.Min(first.Y, Math.Min(second.Y, third.Y)) > rectangle.Bottom)
            return false;

        Point center = new(rectangle.X + rectangle.Width / 2, rectangle.Y + rectangle.Height / 2);
        for (int edge = 0; edge < 3; edge++)
        {
            System.Windows.Vector direction = edge switch
            {
                0 => second - first,
                1 => third - second,
                _ => first - third
            };
            double nx = -direction.Y, ny = direction.X;
            double a = first.X * nx + first.Y * ny;
            double b = second.X * nx + second.Y * ny;
            double c = third.X * nx + third.Y * ny;
            double middle = center.X * nx + center.Y * ny;
            double radius = Math.Abs(nx) * rectangle.Width / 2 + Math.Abs(ny) * rectangle.Height / 2;
            if (Math.Max(a, Math.Max(b, c)) < middle - radius ||
                Math.Min(a, Math.Min(b, c)) > middle + radius)
                return false;
        }
        return true;
    }
}
