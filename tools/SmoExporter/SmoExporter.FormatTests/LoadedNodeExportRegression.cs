using System.Buffers.Binary;
using System.Numerics;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text.Json;
using SmoExporter.Core;
using SmoViewer.Core;

internal static class LoadedNodeExportRegression
{
    private static string Bits(Matrix4x4 value)
    {
        Span<byte> bytes = stackalloc byte[64];
        MemoryMarshal.Write(bytes, in value);
        return Convert.ToHexString(bytes);
    }
    private static float[] Values(Matrix4x4 value) =>
        [value.M11,value.M12,value.M13,value.M14,value.M21,value.M22,value.M23,value.M24,
         value.M31,value.M32,value.M33,value.M34,value.M41,value.M42,value.M43,value.M44];
    public static int Run(string path, string output)
    {
        int checks = 0;
        void Check(bool condition, string message)
        { if (!condition) throw new InvalidDataException(message); ++checks; }
        var document = SmoDocument.Load(path);
        var loaded = SmoLoadedResources.Get(document);
        Check(loaded.LoadIssue is null && loaded.SceneIssue is null, loaded.LoadIssue ?? loaded.SceneIssue ?? string.Empty);
        var scene = SmoSceneBuilder.Build(document, new SmoExportOptions(Resources:
            SmoExportResourceTypes.Skeleton | SmoExportResourceTypes.ServiceNodes));
        Check(scene.Nodes.Count == loaded.Nodes.Count, "Every loaded derived Node reaches the export hierarchy");
        foreach (var node in scene.Nodes)
        {
            Check(Bits(node.BindWorldMatrix) == Bits(SmoExportCoordinateSystem.ToExportMatrix(loaded.Nodes[node.ObjectIndex].World)),
                "Exporter uses actual file pose, without inverse-bind or C# FK reconstruction");
            Check(node.ParentObjectIndex == loaded.Nodes[node.ObjectIndex].ParentObjectIndex, "Parent comes from actual loaded graph");
        }
        string glb = Path.ChangeExtension(output, ".glb");
        GlbExporter.Export(scene, glb);
        byte[] bytes = File.ReadAllBytes(glb);
        int length = checked((int)BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(12)));
        using var json = JsonDocument.Parse(bytes.AsMemory(20, length));
        var nodes = json.RootElement.GetProperty("nodes").EnumerateArray().ToArray();
        var parents = new Dictionary<int, int>();
        for (int i = 0; i < nodes.Length; ++i)
            if (nodes[i].TryGetProperty("children", out var children))
                foreach (var child in children.EnumerateArray()) Check(parents.TryAdd(child.GetInt32(), i), "GLB hierarchy has one parent per node");
        var worlds = new Dictionary<int, Matrix4x4>();
        Matrix4x4 World(int index)
        {
            if (worlds.TryGetValue(index, out var cached)) return cached;
            var node = nodes[index];
            float[] Read(string name, float[] fallback) => node.TryGetProperty(name, out var values)
                ? values.EnumerateArray().Select(value => value.GetSingle()).ToArray() : fallback;
            Matrix4x4 local;
            if (node.TryGetProperty("matrix", out var matrix))
            {
                var v = matrix.EnumerateArray().Select(value => value.GetSingle()).ToArray();
                local = new(v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7],v[8],v[9],v[10],v[11],v[12],v[13],v[14],v[15]);
            }
            else
            {
                var p = Read("translation", [0,0,0]); var q = Read("rotation", [0,0,0,1]); var s = Read("scale", [1,1,1]);
                local = Matrix4x4.CreateScale(s[0],s[1],s[2]) * Matrix4x4.CreateFromQuaternion(new(q[0],q[1],q[2],q[3]))
                    * Matrix4x4.CreateTranslation(p[0],p[1],p[2]);
            }
            return worlds[index] = parents.TryGetValue(index, out int parent) ? local * World(parent) : local;
        }
        float maximumLinearError = 0, maximumTranslationError = 0;
        var exported = new Dictionary<int, int>();
        for (int i = 0; i < nodes.Length; ++i)
            if (nodes[i].TryGetProperty("extras", out var extras)
                && extras.TryGetProperty("sparkplugObjectIndex", out var objectIndex)
                && !extras.TryGetProperty("sparkplugPaletteClone", out _)) exported.Add(objectIndex.GetInt32(), i);
        foreach (var node in scene.Nodes)
        {
            var actual = Values(World(exported[node.ObjectIndex])); var expected = Values(node.BindWorldMatrix);
            for (int component = 0; component < 16; ++component)
            {
                float error = Math.Abs(actual[component] - expected[component]);
                bool translation = component is >= 12 and <= 14;
                if (translation) maximumTranslationError = Math.Max(maximumTranslationError, error);
                else maximumLinearError = Math.Max(maximumLinearError, error);
                Check(error <= (translation ? .02f : .0001f) + Math.Abs(expected[component]) * 1e-6f,
                    $"GLB world differs for Node [{node.ObjectIndex}], component {component}: actual {actual[component]}, expected {expected[component]}, error {error}");
            }
        }
        var report = new
        {
            status = "passed", path = Path.GetFullPath(path), checks, nodes = scene.Nodes.Count,
            source_sha256 = Convert.ToHexString(SHA256.HashData(document.Data.Span)),
            native_dll_sha256 = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(Path.Combine(AppContext.BaseDirectory, "SparkplugViewerNative.dll")))) ,
            glb, glb_sha256 = Convert.ToHexString(SHA256.HashData(bytes)),
            maximum_linear_error = maximumLinearError, maximum_translation_error = maximumTranslationError,
            scope = "Actual loaded file pose and parent identity; independent GLB static hierarchy readback. Animation conversion is a separate scope."
        };
        File.WriteAllText(output, JsonSerializer.Serialize(report, new JsonSerializerOptions { WriteIndented = true }) + "\n");
        Console.WriteLine($"Loaded node export: {scene.Nodes.Count} nodes, {checks} checks, max linear/translation error {maximumLinearError}/{maximumTranslationError}");
        return 0;
    }
}
