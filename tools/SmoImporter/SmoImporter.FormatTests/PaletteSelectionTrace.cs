using System.Collections;
using System.Numerics;
using System.Reflection;
using System.Security.Cryptography;
using System.Text.Json;
using SmoImporter.Core;

internal static class PaletteSelectionTrace
{
    internal static int Run(string report)
    {
        if (File.Exists(report)) throw new InvalidOperationException("Use a new report path");
        var names = Enumerable.Range(0, 48).Select(i => $"fixture_bone_{i:D2}").ToArray();
        var skeleton = new ImportedSkeleton("synthetic_palette_fixture", names,
            names.Select(_ => Matrix4x4.Identity).ToArray());
        var remap = names.ToDictionary(n => n, n => n, StringComparer.Ordinal);
        var inverse = names.ToDictionary(n => n, _ => Matrix4x4.Identity, StringComparer.Ordinal);
        int[] Range(int start, int count) => Enumerable.Range(start, count).ToArray();
        var cases = new[]
        {
            (Name: "capacity", Bones: new[] { Range(0,12), Range(4,12), Range(16,12) }, Mixed: false, IncludeBody: true, Expected: 2),
            (Name: "greedy-ties", Bones: new[] { Range(0,12), Range(8,12), Range(8,4), Range(8,4), Range(0,4) }, Mixed: false, IncludeBody: true, Expected: 2),
            (Name: "material-families", Bones: Enumerable.Range(0,12).Select(_ => Range(0,4)).ToArray(), Mixed: true, IncludeBody: true, Expected: 3),
            (Name: "separate-only", Bones: Enumerable.Range(0,12).Select(_ => Range(0,4)).ToArray(), Mixed: true, IncludeBody: false, Expected: 2),
            (Name: "ushort-vertices", Bones: Enumerable.Range(0,21846).Select(_ => Range(0,1)).ToArray(), Mixed: false, IncludeBody: true, Expected: 2)
        };
        var rows = new List<object>();
        int checks = 0;
        var method = typeof(SmoSkinnedBranchSplitBuilder).GetMethod("BuildPalettePlans", BindingFlags.NonPublic | BindingFlags.Static)!;
        foreach (var item in cases)
        {
            int vertices = item.Bones.Length * 3;
            var joints = new ImportedJointIndices[vertices];
            var weights = new Vector4[vertices];
            for (int t = 0; t < item.Bones.Length; t++)
            for (int vertex = 0; vertex < 3; vertex++)
            {
                int[] bones = Enumerable.Range(0,4).Select(slot => item.Bones[t][(vertex * 4 + slot) % item.Bones[t].Length]).ToArray();
                joints[t*3+vertex] = new((ushort)bones[0], (ushort)bones[1], (ushort)bones[2], (ushort)bones[3]);
                weights[t*3+vertex] = new Vector4(0.25f);
            }
            var mesh = new SmoSkinnedBranchSourceMesh(3, item.Name, new Vector3[vertices], new Vector3[vertices],
                new Vector2[vertices], [], Enumerable.Range(0,vertices).Select(v => (uint)v).ToArray(),
                new ImportedSkinning(skeleton,joints,weights));
            var families = Enumerable.Range(0,item.Bones.Length).Select(t =>
                (SmoSkinnedRenderableMaterialFamily)(item.Mixed ? t % 3 : 0)).ToArray();
            var opacity = new SmoSkinnedRenderableOpacityPlan(new Dictionary<int,SmoSkinnedRenderableMaterialFamily[]> { [3] = families },
                families.Count(f => (int)f == 0), families.Count(f => (int)f == 1), families.Count(f => (int)f == 2));
            var plans = ((IEnumerable)method.Invoke(null,new object[] { new[] {mesh}, opacity, remap, inverse, item.IncludeBody })!).Cast<object>().ToArray();
            Check(plans.Length == item.Expected, item.Name + " expected partition count");
            var observed = new List<int>();
            var snapshots = new List<object>();
            foreach (object plan in plans)
            {
                string[] bones = (string[])Value(plan,"PaletteNames");
                int[] triangles = ((IEnumerable)Value(plan,"Triangles")).Cast<object>()
                    .Select(t => (int)Value(t,"TriangleOrdinal")).ToArray();
                Check(bones.Length is >= 1 and <= 16, "native bone capacity");
                Check(triangles.Length * 3 <= ushort.MaxValue, "native vertex index capacity");
                int family = (int)(SmoSkinnedRenderableMaterialFamily)Value(plan,"MaterialFamily");
                Check(triangles.All(t => (int)families[t] == family), "material family isolated");
                observed.AddRange(triangles);
                snapshots.Add(new { ordinal = (int)Value(plan,"Ordinal"), mesh = (int)Value(plan,"SourceMeshKey"),
                    startsRenderable = (bool)Value(plan,"StartsRenderable"), family, bones, triangles });
            }
            int[] expectedTriangles = Enumerable.Range(0,item.Bones.Length)
                .Where(t => item.IncludeBody || (int)families[t] != 0).ToArray();
            Check(observed.Order().SequenceEqual(expectedTriangles), "each included triangle exactly once");
            if (item.Name == "greedy-ties")
            {
                int[] first = ((IEnumerable)Value(plans[0],"Triangles")).Cast<object>().Select(t => (int)Value(t,"TriangleOrdinal")).ToArray();
                Check(first.SequenceEqual(new[] {0,2,4}), "fewer new bones, fewer triangles, then ordinal");
            }
            rows.Add(new { item.Name, plans = snapshots });
        }
        byte[] trace = JsonSerializer.SerializeToUtf8Bytes(rows);
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(report))!);
        File.WriteAllText(report,JsonSerializer.Serialize(new { kind="palette-selection-trace",status="passed",checks,
            traceSha256=Convert.ToHexString(SHA256.HashData(trace)), rows,
            coreAssemblySha256=Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(typeof(SmoSkinnedBranchSplitBuilder).Assembly.Location))) },new JsonSerializerOptions {WriteIndented=true}));
        Console.WriteLine($"PASS {checks} palette checks, {Convert.ToHexString(SHA256.HashData(trace))}");
        return 0;

        void Check(bool condition,string reason) { checks++;if(!condition)throw new InvalidDataException(reason); }
    }

    private static object Value(object item,string name) => item.GetType().GetProperty(name)!.GetValue(item)!;
}
