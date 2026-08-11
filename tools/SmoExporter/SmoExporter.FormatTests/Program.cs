using System.Buffers.Binary;
using System.Text.Json;
using SmoExporter.Core;
using SmoViewer.Core;

int checks = 0;

if (args.Length != 1 || !File.Exists(args[0]))
{
    Console.Error.WriteLine("Usage: SmoExporter.FormatTests <sample.smo>");
    return 2;
}

string directory = Path.Combine(Path.GetTempPath(), "smo-export-tests-" + Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(directory);
try
{
    SmoExportScene scene = SmoSceneBuilder.Build(SmoDocument.Load(args[0]));
    Check(scene.Meshes.Count > 0, "scene contains meshes");
    Check(scene.Meshes.All(mesh => mesh.Colors.Length == 0 ||
        mesh.Colors.Any(color => color.X > 0 || color.Y > 0 || color.Z > 0)),
        "COLOR_0 is omitted for all-zero diffuse placeholders");
    string glb = Path.Combine(directory, "sample.glb");
    string obj = Path.Combine(directory, "sample.obj");
    GlbExporter.Export(scene, glb);
    ObjExporter.Export(scene, obj);

    byte[] bytes = File.ReadAllBytes(glb);
    Check(BinaryPrimitives.ReadUInt32LittleEndian(bytes) == 0x46546C67, "GLB magic");
    Check(BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(4)) == 2, "GLB version 2");
    Check(BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(8)) == bytes.Length, "GLB length");
    int jsonLength = checked((int)BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(12)));
    Check(BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(16)) == 0x4E4F534A, "JSON chunk");
    using JsonDocument json = JsonDocument.Parse(bytes.AsMemory(20, jsonLength));
    Check(json.RootElement.GetProperty("asset").GetProperty("version").GetString() == "2.0", "glTF version");
    Check(json.RootElement.GetProperty("meshes").GetArrayLength() == scene.Meshes.Count, "mesh count");
    Check(json.RootElement.GetProperty("nodes").GetArrayLength() ==
        scene.Nodes.Count + scene.Meshes.Count, "node count");
    Check(json.RootElement.GetProperty("skins").GetArrayLength() == scene.Skins.Count, "skin count");
    Check(scene.Meshes.Where(mesh => mesh.SkinObjectIndex is not null).All(mesh =>
        mesh.BlendWeights.Length == mesh.Positions.Length &&
        mesh.JointIndices.Length == mesh.Positions.Length), "skinned vertex attributes");
    string objText = File.ReadAllText(obj);
    Check(objText.Contains("mtllib sample.mtl"), "OBJ material library");
    Check(objText.Split('\n').Count(line => line.StartsWith("o ")) == scene.Meshes.Count, "OBJ object count");
    Check(objText.Contains("\nf "), "OBJ faces");
    Console.WriteLine($"PASS: {checks} assertions; meshes={scene.Meshes.Count}; GLB={bytes.Length} bytes");
    return 0;
}
finally
{
    Directory.Delete(directory, true);
}

void Check(bool condition, string description)
{
    checks++;
    if (!condition) throw new InvalidOperationException("FAIL: " + description);
}
