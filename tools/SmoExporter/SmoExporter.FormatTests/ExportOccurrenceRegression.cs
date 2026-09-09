using System.Buffers.Binary;
using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using SmoExporter.Core;
using SmoViewer.Core;

internal static class ExportOccurrenceRegression
{
    private static string Sha(string path) => Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path)));
    private static float[] Values(Matrix4x4 m) => [m.M11,m.M12,m.M13,m.M14,m.M21,m.M22,m.M23,m.M24,m.M31,m.M32,m.M33,m.M34,m.M41,m.M42,m.M43,m.M44];
    public static int Run(string source, string directory, bool fbx)
    {
        Directory.CreateDirectory(directory);
        int checks = 0;
        void Check(bool condition, string message)
        { if (!condition) throw new InvalidDataException(message); ++checks; }
        var document = SmoDocument.Load(source);
        var prepared = SmoViewer.Scene.SmoSceneBuilder.Build(document);
        var scene = SmoSceneBuilder.Build(document);
        var loaded = SmoLoadedResources.Get(document);
        Check(loaded.LoadIssue is null, loaded.LoadIssue ?? string.Empty);
        Check(scene.MeshPlacements.Count == prepared.Meshes.Count, "Exporter preserves every actual supported reference slot");
        Check(scene.Meshes.Select(value => value.VariantKey).Distinct().Count() == scene.Meshes.Count, "Unique mesh/model export variants");
        var variants = scene.Meshes.ToDictionary(value => value.VariantKey);
        var expected = prepared.Meshes.ToDictionary(value => value.OccurrenceKey!.Value);
        Check(scene.MeshPlacements.Select(value => value.OccurrenceKey).Distinct().Count() == scene.MeshPlacements.Count, "Repeated Model keeps distinct export placements");
        foreach (var placement in scene.MeshPlacements)
        {
            var actual = expected[placement.OccurrenceKey!.Value];var mesh = variants[placement.EffectiveMeshKey];
            Check(placement.SceneObjectIndex == actual.RenderableObjectIndex && mesh.RenderableObjectIndex == actual.RenderableObjectIndex, "Actual consumer identity survives export projection");
            Check(placement.MeshObjectIndex == actual.Mesh.ObjectIndex && placement.MaterialObjectIndex == actual.LoadedMaterial?.ObjectIndex, "Actual mesh/material reference survives export projection");
            Check(ReferenceEquals(mesh.LoadedMaterial, actual.LoadedMaterial), "Full source material remains available beside target-format projection");
            Check(placement.WorldMatrix == SmoExportCoordinateSystem.ToExportMatrix(actual.WorldTransform), "Native occurrence world only undergoes target coordinate reflection");
            Check(mesh.SkinObjectIndex == (actual.Mesh.HasSkinningData ? actual.SkinObjectIndex : null), "Each variant keeps its own actual Skin");
        }
        foreach (var group in scene.Meshes.GroupBy(value => value.ObjectIndex))
        {
            var first = group.First();
            Check(group.All(value => ReferenceEquals(value.Positions, first.Positions) && ReferenceEquals(value.TriangleIndices, first.TriangleIndices)), "Variants share converted physical geometry arrays");
        }
        string glb = Path.Combine(directory,"scene.glb");GlbExporter.Export(scene,glb);
        var bytes = File.ReadAllBytes(glb);int jsonLength = checked((int)BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(12)));
        using var parsed = JsonDocument.Parse(bytes.AsMemory(20,jsonLength));var root = parsed.RootElement;
        var gltfMeshes = root.GetProperty("meshes").EnumerateArray().ToArray();
        var gltfMaterials = root.GetProperty("materials").EnumerateArray().ToArray();
        var gltfPlacements = root.GetProperty("nodes").EnumerateArray().Where(value => value.TryGetProperty("mesh",out _)).ToArray();
        Check(gltfPlacements.Length == scene.MeshPlacements.Count, "Written GLB retains all placement nodes");
        var seen = new HashSet<SmoRenderOccurrenceKey>();
        foreach (var node in gltfPlacements)
        {
            var extras = node.GetProperty("extras");
            var key = new SmoRenderOccurrenceKey(extras.GetProperty("sparkplugContainerObjectIndex").GetInt32(),extras.GetProperty("sparkplugMemberSlot").GetInt32());
            Check(seen.Add(key), "GLB occurrence metadata is unique");
            var expectedMesh = variants[new(expected[key].Mesh.ObjectIndex, expected[key].RenderableObjectIndex)];
            var gltfMesh = gltfMeshes[node.GetProperty("mesh").GetInt32()];
            Check(gltfMesh.GetProperty("extras").GetProperty("sparkplugRenderableObjectIndex").GetInt32() == expectedMesh.RenderableObjectIndex, "GLB node selects its own material variant");
            var primitive = gltfMesh.GetProperty("primitives")[0];
            var factor = gltfMaterials[primitive.GetProperty("material").GetInt32()].GetProperty("pbrMetallicRoughness").GetProperty("baseColorFactor").EnumerateArray().Select(value => value.GetSingle()).ToArray();
            Check(factor.SequenceEqual(new[] { expectedMesh.MaterialColor.X,expectedMesh.MaterialColor.Y,expectedMesh.MaterialColor.Z,expectedMesh.MaterialColor.W }), "GLB retains each target material factor");
        }
        foreach (var group in gltfMeshes.GroupBy(value => value.GetProperty("extras").GetProperty("sparkplugObjectIndex").GetInt32()))
        {
            Check(group.Select(value => value.GetProperty("primitives")[0].GetProperty("attributes").GetProperty("POSITION").GetInt32()).Distinct().Count() == 1, "GLB variants share position accessor");
            Check(group.Select(value => value.GetProperty("primitives")[0].GetProperty("indices").GetInt32()).Distinct().Count() == 1, "GLB variants share index accessor");
        }
        int sdkMeshes = 0, sdkAttributes = 0;float sdkWorldError = 0;
        string? fbxHash = null, sdkHash = null;
        if (fbx)
        {
            string fbxPath=Path.Combine(directory,"scene.fbx"),sdkPath=Path.Combine(directory,"sdk-readback.json");
            FbxExporter.Export(scene,fbxPath);NativeFbxBridge.Run("inspect-export",[fbxPath,sdkPath],timeout:TimeSpan.FromSeconds(45));
            using var sdk=JsonDocument.Parse(File.ReadAllBytes(sdkPath));var sdkNodes=sdk.RootElement.GetProperty("meshes").EnumerateArray().ToArray();
            sdkMeshes=sdkNodes.Length;sdkAttributes=sdkNodes.Select(value=>value.GetProperty("attribute_id").GetInt64()).Distinct().Count();
            Check(sdkMeshes==scene.MeshPlacements.Count,"SDK independently reads every FBX occurrence");
            var placements=scene.MeshPlacements.ToDictionary(value=>value.OccurrenceKey!.Value);
            seen.Clear();
            foreach(var node in sdkNodes)
            {
                var key=new SmoRenderOccurrenceKey(node.GetProperty("container").GetInt32(),node.GetProperty("slot").GetInt32());
                Check(seen.Add(key),"FBX occurrence metadata remains unique");
                var placement=placements[key];var mesh=variants[placement.EffectiveMeshKey];
                Check(node.GetProperty("renderable").GetInt32()==placement.SceneObjectIndex && node.GetProperty("mesh").GetInt32()==placement.MeshObjectIndex,"SDK FBX actual reference provenance");
                Check(node.GetProperty("material").GetInt32()==(placement.MaterialObjectIndex??-1),"SDK FBX actual material identity");
                Check(node.GetProperty("vertices").GetInt32()==mesh.Positions.Length && node.GetProperty("polygons").GetInt32()==mesh.TriangleIndices.Length/3,"SDK FBX geometry extents");
                Check(node.GetProperty("skin_deformers").GetInt32()==(mesh.SkinObjectIndex.HasValue?1:0),"One Skin deformer per actual skinned occurrence");
                var color=node.GetProperty("materials")[0].GetProperty("diffuse").EnumerateArray().Select(value=>value.GetDouble()).ToArray();
                Check(Math.Abs(color[0]-mesh.MaterialColor.X)<1e-7 && Math.Abs(color[1]-mesh.MaterialColor.Y)<1e-7 && Math.Abs(color[2]-mesh.MaterialColor.Z)<1e-7,"SDK reads the consuming Model's exported RGB factor");
                var actual=node.GetProperty("world").EnumerateArray().Select(value=>value.GetDouble()).ToArray();var wanted=Values(placement.WorldMatrix);
                for(int i=0;i<16;++i)
                {
                    double error=Math.Abs(actual[i]-wanted[i]);sdkWorldError=Math.Max(sdkWorldError,(float)error);
                    Check(error<=(i is >=12 and <=14?.02:.0001)+Math.Abs(wanted[i])*1e-6,$"SDK world mismatch at {key}/{i}: {error}");
                }
            }
            fbxHash=Sha(fbxPath);sdkHash=Sha(sdkPath);
        }
        var report=new {status="passed",source=Path.GetFullPath(source),source_sha256=Sha(source),checks,
            physical_meshes=scene.Meshes.Select(value=>value.ObjectIndex).Distinct().Count(),variants=scene.Meshes.Count,
            placements=scene.MeshPlacements.Count,glb_sha256=Sha(glb),glb_bytes=bytes.Length,fbx_sha256=fbxHash,sdk_sha256=sdkHash,
            sdk_meshes=sdkMeshes,sdk_attributes=sdkAttributes,sdk_maximum_world_error=sdkWorldError,
            native_dll_sha256=Sha(Path.Combine(AppContext.BaseDirectory,"SparkplugViewerNative.dll")),
            fbx_bridge_sha256=fbx?Sha(NativeFbxBridge.ResolveExecutable()!):null,warnings=scene.Warnings};
        File.WriteAllText(Path.Combine(directory,"report.json"),JsonSerializer.Serialize(report,new JsonSerializerOptions{WriteIndented=true})+"\n");
        Console.WriteLine($"Export occurrences: {scene.MeshPlacements.Count} placements, {scene.Meshes.Count} variants, {checks} checks; FBX={fbx}");
        return 0;
    }
}
