using System.Buffers.Binary;
using System.Text.Json;
using SmoExporter.Core;
using SmoViewer.Core;

int checks = 0;

if (args is ["--blender-path-tests"])
    return TestBlenderPathResolution();

if (args.Length != 1 || !File.Exists(args[0]))
{
    Console.Error.WriteLine("Usage: SmoExporter.FormatTests <sample.smo> | --blender-path-tests");
    return 2;
}

string directory = Path.Combine(Path.GetTempPath(), "smo-export-tests-" + Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(directory);
try
{
    string sourceName = Path.GetFileName(args[0]);
    string? animationPath = sourceName.Equals(
            "knut.smo", StringComparison.OrdinalIgnoreCase)
        ? Path.Combine(Path.GetDirectoryName(args[0])!, "Knid.san")
        : null;
    SmoExportOptions options = new(
        AnimationPaths: animationPath is not null && File.Exists(animationPath)
            ? [animationPath]
            : null);
    SmoExportScene scene = SmoSceneBuilder.Build(SmoDocument.Load(args[0]), options);
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
    string mtlText = File.ReadAllText(Path.ChangeExtension(obj, ".mtl"));
    Check(objText.Contains("mtllib sample.mtl"), "OBJ material library");
    Check(objText.Split('\n').Count(line => line.StartsWith("o ")) == scene.Meshes.Count, "OBJ object count");
    Check(objText.Contains("\nf "), "OBJ faces");

    if (sourceName.Equals("knutBoss.smo", StringComparison.OrdinalIgnoreCase))
    {
        SmoExportMesh shield = scene.Meshes.Single(mesh => mesh.ObjectIndex == 13);
        SmoExportMesh body = scene.Meshes.Single(mesh => mesh.ObjectIndex == 28);
        Check(shield.Texture?.Name == "gr_01", "knutBoss shield texture");
        Check(shield.UsesAlphaBlend, "knutBoss shield alpha-blend state");
        Check(!body.UsesAlphaBlend, "knutBoss body remains opaque");
        int shieldIndex = scene.Meshes.ToList().FindIndex(mesh => mesh.ObjectIndex == 13);
        JsonElement shieldMaterial = json.RootElement.GetProperty("materials")[shieldIndex];
        Check(shieldMaterial.GetProperty("alphaMode").GetString() == "BLEND",
            "knutBoss GLB shield alphaMode");
        Check(mtlText.Contains("map_d "), "knutBoss OBJ alpha map");
    }
    else if (sourceName.Equals("knut.smo", StringComparison.OrdinalIgnoreCase))
    {
        SmoExportMesh glasses = scene.Meshes.Single(mesh => mesh.ObjectIndex == 6);
        Check(glasses.Texture is null, "Knut glasses stay untextured");
        Check(glasses.MaterialColor.X == 0 && glasses.MaterialColor.Y == 0 &&
              glasses.MaterialColor.Z == 0, "Knut glasses export as black");
        Check(glasses.ParentNodeObjectIndex == 2,
            "Knut glasses attach to animated render node");
        Check(scene.Nodes.Any(node => node.ObjectIndex == 2 &&
              node.Name == "Knut_TEMP_glasses"),
            "Knut animated render node is exported");
        Check(scene.Animations.SelectMany(animation => animation.Tracks)
              .Any(track => track.NodeObjectIndex == 2),
            "Knut rigid animation track is exported");

        int parentNode = scene.Nodes.ToList().FindIndex(node => node.ObjectIndex == 2);
        int glassesMesh = scene.Meshes.ToList().FindIndex(mesh => mesh.ObjectIndex == 6);
        int glassesNode = scene.Nodes.Count + glassesMesh;
        Check(json.RootElement.GetProperty("nodes")[parentNode]
              .GetProperty("children").EnumerateArray()
              .Any(child => child.GetInt32() == glassesNode),
            "Knut GLB glasses node is parented to render node");
    }
    else if (sourceName.Equals("Spirit.smo", StringComparison.OrdinalIgnoreCase))
    {
        SmoExportMesh[] compactSkins = scene.Meshes
            .Where(mesh => mesh.VertexFormat == 0x093E)
            .ToArray();
        Check(compactSkins.Length > 0, "Spirit compact skin meshes");
        Check(compactSkins.All(mesh =>
              mesh.BlendWeights.Length == mesh.Positions.Length &&
              mesh.JointIndices.Length == mesh.Positions.Length),
            "Spirit compact skin attributes are exported");
    }
    else if (sourceName.Equals("bloomx.smo", StringComparison.OrdinalIgnoreCase))
    {
        foreach (int meshIndex in new[] { 103, 105 })
        {
            SmoExportMesh layered = scene.Meshes.Single(mesh => mesh.ObjectIndex == meshIndex);
            Check(layered.Texture?.Name == "bloom_xc",
                $"bloomx [{meshIndex}] base texture");
            Check(layered.EffectTexture?.Name == "sparkles0001",
                $"bloomx [{meshIndex}] first effect frame");
            int materialIndex = scene.Meshes.ToList()
                .FindIndex(mesh => mesh.ObjectIndex == meshIndex);
            Check(json.RootElement.GetProperty("materials")[materialIndex]
                  .TryGetProperty("emissiveTexture", out JsonElement emissive) &&
                  emissive.GetProperty("texCoord").GetInt32() == 1,
                $"bloomx [{meshIndex}] GLB UV1 effect stage");
        }
    }

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

int TestBlenderPathResolution()
{
    string root = Path.Combine(Path.GetTempPath(), "smo-blender-path-tests-" + Guid.NewGuid().ToString("N"));
    string installation = Path.Combine(root, "Nonstandard Blender Location");
    string executable = Path.Combine(installation, "blender.exe");
    string otherExecutable = Path.Combine(installation, "not-blender.exe");
    Directory.CreateDirectory(installation);
    File.WriteAllBytes(executable, [0]);
    File.WriteAllBytes(otherExecutable, [0]);
    string? previous = Environment.GetEnvironmentVariable("SMO_TEST_BLENDER_ROOT");
    string? previousBlenderPath = Environment.GetEnvironmentVariable("BLENDER_PATH");
    try
    {
        Environment.SetEnvironmentVariable("SMO_TEST_BLENDER_ROOT", installation);
        Check(FbxExporter.ResolveBlenderExecutable(installation) == executable,
            "installation directory resolves blender.exe");
        Check(FbxExporter.ResolveBlenderExecutable(executable) == executable,
            "direct executable resolves");
        Check(FbxExporter.ResolveBlenderExecutable($"\"{executable}\"") == executable,
            "quoted executable resolves");
        Check(FbxExporter.ResolveBlenderExecutable("%SMO_TEST_BLENDER_ROOT%") == executable,
            "environment variable expands");
        Check(FbxExporter.ResolveBlenderExecutable(otherExecutable) is null,
            "non-Blender executable is rejected");
        Check(FbxExporter.ResolveBlenderExecutable(Path.Combine(root, "missing")) is null,
            "missing path is rejected");
        Check(FbxExporter.FindBlenderExecutable(installation) == executable,
            "manual path has priority");
        Environment.SetEnvironmentVariable("BLENDER_PATH", installation);
        Check(FbxExporter.FindBlenderExecutable() == executable,
            "BLENDER_PATH participates in automatic discovery");
        Console.WriteLine($"PASS: {checks} Blender path assertions");
        return 0;
    }
    finally
    {
        Environment.SetEnvironmentVariable("SMO_TEST_BLENDER_ROOT", previous);
        Environment.SetEnvironmentVariable("BLENDER_PATH", previousBlenderPath);
        Directory.Delete(root, true);
    }
}
