using System.Windows.Media;
using System.Windows.Media.Media3D;
using Color = System.Windows.Media.Color;
using NumericsVector3 = System.Numerics.Vector3;

namespace SmoViewer;

public partial class MainWindow
{
    private AnimationSkeletonVisuals? _animationSkeletonVisuals;
    private SelectedBoneVisual? _selectedBoneVisual;

    // A pose changes transforms only. Rebuild the visual tree on model/radius
    // changes or UI visibility/selection events, not on every animation tick.
    private bool UpdateAnimationSkeletonVisuals(DecodedSmoFile file)
    {
        double radius = Math.Max(_sceneBounds.DiagonalLength * 0.006, 0.008);
        double attachmentRadius = Math.Max(_sceneBounds.DiagonalLength * 0.01, 0.012);
        bool rebuild = _animationSkeletonVisuals is null ||
            !ReferenceEquals(_animationSkeletonVisuals.File, file) ||
            _animationSkeletonVisuals.Radius != radius || _animationSkeletonVisuals.AttachmentRadius != attachmentRadius;
        if (rebuild)
            _animationSkeletonVisuals = new AnimationSkeletonVisuals(file, radius, attachmentRadius, _animatedBonePositions);
        _animationSkeletonVisuals!.Update(_animatedBonePositions,
            ShowSkeletonCheck?.IsChecked == true, ShowAttachmentsCheck?.IsChecked == true);
        return rebuild;
    }

    private void UpdateSelectedBoneVisual()
    {
        if (_selectedBoneVisual is not SelectedBoneVisual selected) return;
        NumericsVector3 position = selected.FileIndex == _animationFileIndex
            ? _animatedBonePositions.GetValueOrDefault(selected.Bone.ObjectIndex, selected.Bone.Position)
            : selected.Bone.Position;
        selected.Transform.Matrix = MarkerMatrix(ToViewportPoint(position), selected.Radius);
    }

    private static Matrix3D MarkerMatrix(Point3D center, double radius) => new(
        radius, 0, 0, 0, 0, radius, 0, 0, 0, 0, radius, 0, center.X, center.Y, center.Z, 1);

    private static Matrix3D BoneSegmentMatrix(Point3D start, Point3D end, double radius)
    {
        Vector3D delta = end-start;
        double length = delta.Length;
        // Coincident joints have no segment. Keep a finite degenerate transform
        // instead of producing NaN coordinates while normalizing a zero axis.
        if (length <= 1e-12)
            return new Matrix3D(0,0,0,0, 0,0,0,0, 0,0,0,0, start.X,start.Y,start.Z,1);
        Vector3D axis = delta/length;
        Vector3D reference = Math.Abs(Vector3D.DotProduct(axis, new Vector3D(0,1,0))) > 0.9
            ? new Vector3D(1,0,0) : new Vector3D(0,1,0);
        Vector3D side = Vector3D.CrossProduct(axis, reference); side.Normalize();
        Vector3D up = Vector3D.CrossProduct(side, axis); up.Normalize();
        // The shared +Z unit segment uses side=-X, up=+Y, z in [0,1].
        Vector3D x = -side*radius, y = up*radius;
        return new Matrix3D(x.X,x.Y,x.Z,0, y.X,y.Y,y.Z,0, delta.X,delta.Y,delta.Z,0, start.X,start.Y,start.Z,1);
    }

    private sealed class AnimationSkeletonVisuals
    {
        public DecodedSmoFile File { get; }
        public double Radius { get; }
        public double AttachmentRadius { get; }
        public Model3DGroup Bones { get; } = new();
        public Model3DGroup Attachments { get; } = new();
        private readonly List<AnimatedBoneVisual> _visuals = [];

        public AnimationSkeletonVisuals(DecodedSmoFile file, double radius, double attachmentRadius,
            IReadOnlyDictionary<int, NumericsVector3> positions)
        {
            File = file; Radius = radius; AttachmentRadius = attachmentRadius;
            Material line = CreateSolidMaterial(Color.FromRgb(70,220,255));
            Material point = CreateSolidMaterial(Color.FromRgb(255,218,75));
            Material attachment = CreateSolidMaterial(Color.FromRgb(255,70,190));
            Geometry3D pointGeometry = CreateOctahedron(new Point3D(), 1, point).Geometry;
            Geometry3D segmentGeometry = CreateBoneSegment(new Point3D(), new Point3D(0,0,1), 1, line).Geometry;
            foreach (SkeletonBone bone in file.Skeleton)
            {
                if (!positions.ContainsKey(bone.ObjectIndex)) continue;
                var marker = new MatrixTransform3D();
                Material material = bone.IsAttachment ? attachment : point;
                (bone.IsAttachment ? Attachments : Bones).Children.Add(new GeometryModel3D(pointGeometry, material)
                    { BackMaterial = material, Transform = marker });
                MatrixTransform3D? segment = null;
                if (!bone.IsAttachment && bone.ParentObjectIndex is int parent && positions.ContainsKey(parent))
                {
                    segment = new MatrixTransform3D();
                    Bones.Children.Add(new GeometryModel3D(segmentGeometry, line) { BackMaterial = line, Transform = segment });
                }
                _visuals.Add(new AnimatedBoneVisual(bone.ObjectIndex, bone.ParentObjectIndex, bone.IsAttachment, marker, segment));
            }
        }

        public void Update(IReadOnlyDictionary<int, NumericsVector3> positions, bool showBones, bool showAttachments)
        {
            foreach (AnimatedBoneVisual visual in _visuals)
            {
                if (visual.Attachment ? !showAttachments : !showBones) continue;
                Point3D child = ToViewportPoint(positions[visual.ObjectIndex]);
                visual.Marker.Matrix = MarkerMatrix(child, visual.Attachment ? AttachmentRadius : Radius);
                if (visual.Segment is not null && visual.Parent is int parent)
                    visual.Segment.Matrix = BoneSegmentMatrix(ToViewportPoint(positions[parent]), child, Radius*0.35);
            }
        }
    }

    private sealed record AnimatedBoneVisual(int ObjectIndex, int? Parent, bool Attachment,
        MatrixTransform3D Marker, MatrixTransform3D? Segment);
    private sealed record SelectedBoneVisual(int FileIndex, SkeletonBone Bone, double Radius, MatrixTransform3D Transform);
}
