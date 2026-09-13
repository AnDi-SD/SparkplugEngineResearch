using System.Collections;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Media3D;
using SmoViewer;
using SmoViewer.Scene;
using V3 = System.Numerics.Vector3;

internal static class AnimationVisualRegression
{
    private static readonly BindingFlags Hidden = BindingFlags.NonPublic | BindingFlags.Instance;
    private static readonly ConditionalWeakTable<MainWindow, Previous> PreviousByWindow = new();
    private sealed class Previous { public object? File; public GeometryModel3D[] Models = []; }
    private static object? Field(object owner, string name) => owner.GetType().GetField(name, Hidden)!.GetValue(owner);
    private static object? Property(object owner, string name) => owner.GetType().GetProperty(name)!.GetValue(owner);
    private static object InvokeStatic(string name, params object[] args) =>
        typeof(MainWindow).GetMethod(name, BindingFlags.NonPublic | BindingFlags.Static)!.Invoke(null, args)!;

    internal static void Check(MainWindow window, Action<bool,string> check)
    {
        object cache = Field(window, "_animationSkeletonVisuals")!;
        object file = Property(cache, "File")!;
        var positions = (IReadOnlyDictionary<int,V3>)Field(window, "_animatedBonePositions")!;
        var bones = (Model3DGroup)Property(cache, "Bones")!;
        var attachments = (Model3DGroup)Property(cache, "Attachments")!;
        double radius = (double)Property(cache, "Radius")!, attachmentRadius = (double)Property(cache, "AttachmentRadius")!;
        int boneIndex = 0, attachmentIndex = 0;
        Point3D Point(V3 value) => new(value.X, value.Y, -value.Z);
        foreach (object bone in (IEnumerable)Property(file, "Skeleton")!)
        {
            int index = (int)Property(bone, "ObjectIndex")!;
            if (!positions.TryGetValue(index, out V3 position)) continue;
            bool attachment = (bool)Property(bone, "IsAttachment")!;
            Point3D child = Point(position);
            var marker = (GeometryModel3D)(attachment ? attachments.Children[attachmentIndex++] : bones.Children[boneIndex++]);
            var expectedMarker = (GeometryModel3D)InvokeStatic("CreateOctahedron", child, attachment ? attachmentRadius : radius, marker.Material);
            Compare(marker, expectedMarker, check);
            if (!attachment && Property(bone, "ParentObjectIndex") is int parent && positions.TryGetValue(parent, out V3 parentPosition))
            {
                var segment = (GeometryModel3D)bones.Children[boneIndex++];
                Point3D start = Point(parentPosition);
                if ((child-start).Length <= 1e-12)
                    check(((MeshGeometry3D)segment.Geometry).Positions.All(point =>
                        (segment.Transform.Transform(point)-start).Length < 1e-10), "Coincident bone segment is finite and degenerate.");
                else
                    Compare(segment, (GeometryModel3D)InvokeStatic("CreateBoneSegment", start, child, radius*.35, segment.Material), check);
            }
        }
        check(boneIndex == bones.Children.Count && attachmentIndex == attachments.Children.Count,
            "Cached animated visual topology matches original helper output.");
        GeometryModel3D[] models = bones.Children.Concat(attachments.Children).Cast<GeometryModel3D>().ToArray();
        Previous previous = PreviousByWindow.GetOrCreateValue(window);
        if (ReferenceEquals(previous.File, file))
            check(models.Length == previous.Models.Length && models.Zip(previous.Models).All(pair => ReferenceEquals(pair.First, pair.Second)),
                "Animation frames reuse the same model and frozen mesh objects.");
        previous.File = file; previous.Models = models;
    }

    private static void Compare(GeometryModel3D actual, GeometryModel3D expected, Action<bool,string> check)
    {
        var mesh = (MeshGeometry3D)actual.Geometry;
        var reference = (MeshGeometry3D)expected.Geometry;
        check(mesh.IsFrozen && mesh.TriangleIndices.SequenceEqual(reference.TriangleIndices) && mesh.Positions.Count == reference.Positions.Count,
            "Shared frozen visual mesh retains original topology.");
        double maximum = mesh.Positions.Zip(reference.Positions).Max(pair => (actual.Transform.Transform(pair.First)-pair.Second).Length);
        check(maximum < 1e-8, $"Transformed cached visual matches old geometry helper: {maximum:G5}.");
    }

    internal static void CheckVisibilityAndSelection(MainWindow window, Action<bool,string> check)
    {
        CheckPicking(window, check);
        var bonesCheck = (CheckBox)window.FindName("ShowSkeletonCheck");
        var attachmentsCheck = (CheckBox)window.FindName("ShowAttachmentsCheck");
        var bonesRoot = (Model3DGroup)window.FindName("SkeletonRoot");
        var attachmentRoot = (Model3DGroup)window.FindName("AttachmentRoot");
        object cache = Field(window, "_animationSkeletonVisuals")!;
        bonesCheck.IsChecked = false; attachmentsCheck.IsChecked = false;
        check(bonesRoot.Children.Count == 0 && attachmentRoot.Children.Count == 0, "Visibility handlers hide all bone layers.");
        bonesCheck.IsChecked = true; attachmentsCheck.IsChecked = true;
        check(bonesRoot.Children.Contains((Model3D)Property(cache, "Bones")!) &&
              attachmentRoot.Children.Contains((Model3D)Property(cache, "Attachments")!), "Visibility handlers restore cached layers.");
        var items = ((IEnumerable)Field(window, "_allBoneItems")!).Cast<object>().ToArray();
        if (items.Length == 0) return;
        object item = items[0];
        // Same state used by the real bone-list selection; the visibility
        // method and subsequent pose update are production code.
        typeof(MainWindow).GetField("_selectedBone", Hidden)!.SetValue(window, item);
        typeof(MainWindow).GetMethod("UpdateSkeletonVisibility", Hidden)!.Invoke(window, null);
        typeof(MainWindow).GetMethod("ApplyAnimationPoseCore", Hidden)!.Invoke(window, null);
        object selected = Field(window, "_selectedBoneVisual")!;
        var transform = (MatrixTransform3D)Property(selected, "Transform")!;
        int objectIndex = (int)Property(item, "ObjectIndex")!;
        V3 value = ((IReadOnlyDictionary<int,V3>)Field(window, "_animatedBonePositions")!)[objectIndex];
        check((transform.Transform(new Point3D())-new Point3D(value.X,value.Y,-value.Z)).Length < 1e-8,
            "Selected bone marker follows the animated pose.");
        typeof(MainWindow).GetField("_selectedBone", Hidden)!.SetValue(window, null);
        typeof(MainWindow).GetMethod("UpdateSkeletonVisibility", Hidden)!.Invoke(window, null);
    }

    private static void CheckPicking(MainWindow window, Action<bool,string> check)
    {
        var geometries = (IDictionary)Field(window, "_sceneGeometry")!;
        foreach (DictionaryEntry item in geometries)
        {
            object scene = item.Value!;
            if (!(bool)Property(scene, "GpuRendered")! ||
                !((SmoSceneMesh)Property(scene, "RenderMesh")!).Mesh.HasSkinningData) continue;
            var model = (GeometryModel3D)Property(scene, "Model")!;
            var mesh = (MeshGeometry3D)model.Geometry;
            for (int i = 0; i+2 < mesh.TriangleIndices.Count; i += 3)
            {
                Point3D a = mesh.Positions[mesh.TriangleIndices[i]], b = mesh.Positions[mesh.TriangleIndices[i+1]],
                    c = mesh.Positions[mesh.TriangleIndices[i+2]];
                Vector3D normal = Vector3D.CrossProduct(b-a, c-a);
                if (normal.Length < 1e-8) continue;
                normal.Normalize();
                Point3D center = new((a.X+b.X+c.X)/3, (a.Y+b.Y+c.Y)/3, (a.Z+b.Z+c.Z)/3);
                var visual = new ModelVisual3D { Content = model };
                List<(int,int,int,double)> Hits()
                {
                    var results = new List<(int,int,int,double)>();
                    VisualTreeHelper.HitTest(visual, null, result =>
                    {
                        if (result is RayMeshGeometry3DHitTestResult hit)
                            results.Add((hit.VertexIndex1,hit.VertexIndex2,hit.VertexIndex3,hit.DistanceToRayOrigin));
                        return HitTestResultBehavior.Continue;
                    }, new RayHitTestParameters(center+normal, -normal));
                    return results;
                }
                var before = Hits();
                Vector3DCollection previous = mesh.Normals;
                List<(int,int,int,double)> withoutNormals;
                try { mesh.Normals = new Vector3DCollection(); withoutNormals = Hits(); }
                finally { mesh.Normals = previous; visual.Content = null; }
                check(before.Count > 0 && before.SequenceEqual(withoutNormals),
                    "Actual WPF ray picking on the animated GPU companion is independent of stored normals.");
                return;
            }
        }
    }

    internal static void CheckForeignSelection(MainWindow window, Action<bool,string> check)
    {
        var files = (IDictionary)Field(window, "_treeFiles")!;
        var positions = (IReadOnlyDictionary<int,V3>)Field(window, "_animatedBonePositions")!;
        var foreignBones = ((IEnumerable)Property(files[1]!, "Skeleton")!).Cast<object>()
            .ToDictionary(bone => (int)Property(bone, "ObjectIndex")!);
        object item = ((IEnumerable)Field(window, "_allBoneItems")!).Cast<object>().First(item =>
        {
            int index = (int)Property(item, "ObjectIndex")!;
            return (int)Property(item, "FileIndex")! == 1 && positions.TryGetValue(index, out V3 animated) &&
                V3.Distance(animated, (V3)Property(foreignBones[index], "Position")!) > .001f;
        });
        typeof(MainWindow).GetField("_selectedBone", Hidden)!.SetValue(window, item);
        typeof(MainWindow).GetMethod("UpdateSkeletonVisibility", Hidden)!.Invoke(window, null);
        typeof(MainWindow).GetMethod("ApplyAnimationPoseCore", Hidden)!.Invoke(window, null);
        object selected = Field(window, "_selectedBoneVisual")!;
        var transform = (MatrixTransform3D)Property(selected, "Transform")!;
        V3 expected = (V3)Property(foreignBones[(int)Property(item, "ObjectIndex")!], "Position")!;
        check((transform.Transform(new Point3D())-new Point3D(expected.X,expected.Y,-expected.Z)).Length < 1e-8,
            "A selected bone in another loaded model must not reuse the animated file's coincident object index.");
    }
}
