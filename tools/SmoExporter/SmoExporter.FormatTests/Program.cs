using System.Buffers.Binary;
using System.Numerics;
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
    Check(scene.Meshes.All(mesh =>
            (mesh.MaterialColor.W >= 1f &&
             mesh.Colors.All(color => color.W >= 1f)) ||
            mesh.UsesAlphaBlend),
        "explicit material/COLOR_0 alpha enables the transparent pass");
    SmoExportTexture[] textures = scene.Meshes
        .SelectMany(mesh => new[] { mesh.Texture, mesh.EffectTexture })
        .Where(texture => texture is not null)
        .Cast<SmoExportTexture>()
        .DistinctBy(texture => texture.ObjectIndex)
        .ToArray();
    foreach (SmoExportTexture texture in textures)
    {
        bool hasOpacityMask = texture.OpacityMaskPngBytes is not null;
        Check(PngColorType(texture.PngBytes) == (hasOpacityMask ? 6 : 2),
            $"texture [{texture.ObjectIndex}] uses RGB unless source alpha is present");
        if (texture.OpacityMaskPngBytes is byte[] opacityMask)
        {
            Check(PngColorType(opacityMask) == 0,
                $"texture [{texture.ObjectIndex}] opacity mask is grayscale");
        }
        Check((texture.OpaqueRgbPngBytes is not null) == hasOpacityMask,
            $"texture [{texture.ObjectIndex}] only stores a distinct RGB variant when needed");
        if (texture.OpaqueRgbPngBytes is byte[] opaqueRgb)
        {
            Check(PngColorType(opaqueRgb) == 2,
                $"texture [{texture.ObjectIndex}] opaque variant is RGB");
        }
    }
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
    JsonElement gltf = json.RootElement;
    Check(gltf.GetProperty("asset").GetProperty("version").GetString() == "2.0", "glTF version");
    Check(gltf.GetProperty("meshes").GetArrayLength() == scene.Meshes.Count, "mesh count");
    JsonElement gltfMaterials = gltf.GetProperty("materials");
    for (int meshIndex = 0; meshIndex < scene.Meshes.Count; meshIndex++)
    {
        SmoExportMesh mesh = scene.Meshes[meshIndex];
        bool hasExplicitAlpha = mesh.MaterialColor.W < 1f ||
                                mesh.Colors.Any(color => color.W < 1f);
        if (hasExplicitAlpha)
        {
            Check(gltfMaterials[meshIndex].TryGetProperty(
                      "alphaMode", out JsonElement alphaMode) &&
                  alphaMode.GetString() == "BLEND",
                $"mesh [{mesh.ObjectIndex}] explicit alpha has GLB BLEND mode");
        }
        if (mesh.Texture is not null)
        {
            JsonElement pbr = gltfMaterials[meshIndex]
                .GetProperty("pbrMetallicRoughness");
            int textureIndex = pbr.GetProperty("baseColorTexture")
                .GetProperty("index").GetInt32();
            int imageIndex = gltf.GetProperty("textures")[textureIndex]
                .GetProperty("source").GetInt32();
            byte expectedColorType = mesh.UsesAlphaBlend
                ? PngColorType(mesh.Texture.PngBytes)
                : (byte)2;
            Check(EmbeddedPngColorType(bytes, gltf, imageIndex, jsonLength) ==
                  expectedColorType,
                $"mesh [{mesh.ObjectIndex}] GLB uses the correct alpha texture variant");
        }
        if (mesh.EffectTexture is not null)
        {
            int textureIndex = gltfMaterials[meshIndex]
                .GetProperty("emissiveTexture").GetProperty("index").GetInt32();
            int imageIndex = gltf.GetProperty("textures")[textureIndex]
                .GetProperty("source").GetInt32();
            Check(EmbeddedPngColorType(bytes, gltf, imageIndex, jsonLength) == 2,
                $"mesh [{mesh.ObjectIndex}] GLB emissive texture ignores service alpha");
        }
    }
    var expectedImageVariants = new HashSet<(int ObjectIndex, bool OpaqueRgb)>();
    foreach (SmoExportMesh mesh in scene.Meshes)
    {
        if (mesh.Texture is not null)
        {
            bool opaqueRgb = !mesh.UsesAlphaBlend &&
                             mesh.Texture.OpacityMaskPngBytes is not null &&
                             mesh.Texture.OpaqueRgbPngBytes is not null;
            expectedImageVariants.Add((mesh.Texture.ObjectIndex, opaqueRgb));
        }
        if (mesh.EffectTexture is not null)
        {
            bool opaqueRgb = mesh.EffectTexture.OpacityMaskPngBytes is not null &&
                             mesh.EffectTexture.OpaqueRgbPngBytes is not null;
            expectedImageVariants.Add((mesh.EffectTexture.ObjectIndex, opaqueRgb));
        }
    }
    int imageCount = gltf.TryGetProperty("images", out JsonElement gltfImages)
        ? gltfImages.GetArrayLength()
        : 0;
    Check(imageCount == expectedImageVariants.Count,
        "GLB caches only distinct RGB/RGBA texture variants");
    JsonElement gltfNodes = gltf.GetProperty("nodes");
    Check(gltfNodes.GetArrayLength() >= scene.Nodes.Count + scene.Meshes.Count,
        "node count includes source nodes, meshes and optional palette clones");
    int?[] nodeParents = BuildNodeParents(gltfNodes, out bool validNodeHierarchy);
    Check(validNodeHierarchy, "glTF node hierarchy indices and parents");

    bool uniqueSkinJoints = true;
    bool validSkeletonRoots = true;
    int gltfSkinCount = 0;
    if (gltf.TryGetProperty("skins", out JsonElement gltfSkins))
    {
        gltfSkinCount = gltfSkins.GetArrayLength();
        foreach (JsonElement skin in gltfSkins.EnumerateArray())
        {
            int[] joints = skin.GetProperty("joints").EnumerateArray()
                .Select(value => value.GetInt32()).ToArray();
            uniqueSkinJoints &= joints.Distinct().Count() == joints.Length;
            if (skin.TryGetProperty("skeleton", out JsonElement skeleton))
            {
                int rootNode = skeleton.GetInt32();
                validSkeletonRoots &= joints.All(joint =>
                    IsAncestor(rootNode, joint, nodeParents));
            }
        }
    }
    Check(gltfSkinCount == scene.Skins.Count(skin => skin.JointObjectIndices.Count > 0),
        "non-empty skin count");
    Check(uniqueSkinJoints, "skin joints are unique within every skin");
    Check(validSkeletonRoots, "skin skeleton is a common ancestor when present");

    bool uniqueAnimationTargets = true;
    bool animationViewsHaveNoTarget = true;
    if (gltf.TryGetProperty("animations", out JsonElement gltfAnimations))
    {
        JsonElement accessors = gltf.GetProperty("accessors");
        JsonElement bufferViews = gltf.GetProperty("bufferViews");
        foreach (JsonElement animation in gltfAnimations.EnumerateArray())
        {
            var targets = new HashSet<(int Node, string Path)>();
            foreach (JsonElement channel in animation.GetProperty("channels").EnumerateArray())
            {
                JsonElement target = channel.GetProperty("target");
                uniqueAnimationTargets &= targets.Add((
                    target.GetProperty("node").GetInt32(),
                    target.GetProperty("path").GetString() ?? string.Empty));
            }
            foreach (JsonElement sampler in animation.GetProperty("samplers").EnumerateArray())
            {
                animationViewsHaveNoTarget &= AccessorViewHasNoTarget(
                    accessors, bufferViews, sampler.GetProperty("input").GetInt32());
                animationViewsHaveNoTarget &= AccessorViewHasNoTarget(
                    accessors, bufferViews, sampler.GetProperty("output").GetInt32());
            }
        }
    }
    Check(uniqueAnimationTargets, "animation node/path targets are unique within each clip");
    Check(animationViewsHaveNoTarget, "animation accessor bufferViews have no GPU target");
    Check(scene.Meshes.Where(mesh => mesh.SkinObjectIndex is not null).All(mesh =>
        mesh.BlendWeights.Length == mesh.Positions.Length &&
        mesh.JointIndices.Length == mesh.Positions.Length), "skinned vertex attributes");
    string objText = File.ReadAllText(obj);
    string mtlText = File.ReadAllText(Path.ChangeExtension(obj, ".mtl"));
    Check(objText.Contains("mtllib sample.mtl"), "OBJ material library");
    Check(objText.Split('\n').Count(line => line.StartsWith("o ")) == scene.Meshes.Count, "OBJ object count");
    Check(objText.Contains("\nf "), "OBJ faces");
    foreach (SmoExportMesh mesh in scene.Meshes)
    {
        string materialBlock = GetMaterialBlock(mtlText, mesh.ObjectIndex);
        float vertexAlpha = mesh.Colors.Length == mesh.Positions.Length &&
                            mesh.Colors.Length > 0
            ? mesh.Colors[0].W
            : 1f;
        float expectedOpacity = mesh.UsesAlphaBlend
            ? mesh.MaterialColor.W * vertexAlpha
            : 1f;
        Check(materialBlock.Split('\n').Contains(
                FormattableString.Invariant($"d {expectedOpacity:R}")),
            $"OBJ material [{mesh.ObjectIndex}] writes its scalar opacity");
        string? opacityMap = materialBlock.Split('\n')
            .SingleOrDefault(line => line.StartsWith("map_d ", StringComparison.Ordinal));
        bool expectsOpacityMap = mesh.UsesAlphaBlend &&
                                 mesh.Texture?.OpacityMaskPngBytes is not null;
        Check((opacityMap is not null) == expectsOpacityMap,
            $"OBJ material [{mesh.ObjectIndex}] emits only the required opacity map");
        if (mesh.Texture is not null)
        {
            string colorMap = materialBlock.Split('\n')
                .Single(line => line.StartsWith("map_Kd ", StringComparison.Ordinal));
            string colorFile = colorMap["map_Kd ".Length..];
            Check(PngColorType(File.ReadAllBytes(Path.Combine(directory, colorFile))) == 2,
                $"OBJ material [{mesh.ObjectIndex}] map_Kd is RGB");
        }
        if (opacityMap is not null)
        {
            string opacityFile = opacityMap["map_d ".Length..];
            Check(opacityFile.EndsWith("_opacity.png", StringComparison.Ordinal),
                $"OBJ material [{mesh.ObjectIndex}] does not reuse its color atlas as map_d");
            Check(PngColorType(File.ReadAllBytes(Path.Combine(directory, opacityFile))) == 0,
                $"OBJ material [{mesh.ObjectIndex}] map_d is grayscale");
        }
    }
    TestUnsupportedAlphaCases(scene, directory);

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
        string shieldMaterialBlock = GetMaterialBlock(mtlText, shield.ObjectIndex);
        Check(shieldMaterialBlock.Contains(
                $"map_d sample_texture_{shield.Texture!.ObjectIndex}_opacity.png"),
            "knutBoss OBJ uses the separate shield opacity map");
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
        int glassesNode = FindMeshNode(gltfNodes, glassesMesh);
        Check(glassesNode >= 0, "Knut GLB glasses mesh node exists");
        Check(gltfNodes[parentNode]
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
    else if (sourceName.Equals("Icy.smo", StringComparison.OrdinalIgnoreCase))
    {
        SmoExportMesh translucent = scene.Meshes.Single(mesh =>
            mesh.MaterialColor.W is > 0f and < 1f);
        Check(translucent.UsesAlphaBlend,
            "Icy half-alpha material enables blending");
        Check(translucent.Texture is not null &&
              translucent.Texture.OpacityMaskPngBytes is null &&
              PngColorType(translucent.Texture.PngBytes) == 2,
            "Icy uses scalar material alpha with an opaque RGB texture");
        int materialIndex = scene.Meshes.ToList().IndexOf(translucent);
        Check(gltfMaterials[materialIndex].GetProperty("alphaMode").GetString() == "BLEND",
            "Icy half-alpha material keeps transparency in GLB");
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

byte PngColorType(ReadOnlySpan<byte> png)
{
    if (png.Length < 26 ||
        !png[..8].SequenceEqual(
            new byte[] { 137, 80, 78, 71, 13, 10, 26, 10 }))
    {
        throw new InvalidDataException("Expected a PNG image.");
    }
    return png[25];
}

byte EmbeddedPngColorType(
    byte[] glb,
    JsonElement root,
    int imageIndex,
    int jsonChunkLength)
{
    JsonElement image = root.GetProperty("images")[imageIndex];
    JsonElement view = root.GetProperty("bufferViews")[
        image.GetProperty("bufferView").GetInt32()];
    int byteOffset = view.TryGetProperty("byteOffset", out JsonElement offset)
        ? offset.GetInt32()
        : 0;
    int byteLength = view.GetProperty("byteLength").GetInt32();
    int binaryDataOffset = checked(28 + jsonChunkLength);
    return PngColorType(glb.AsSpan(
        checked(binaryDataOffset + byteOffset), byteLength));
}

string GetMaterialBlock(string mtl, int objectIndex)
{
    string normalized = mtl.Replace("\r", string.Empty, StringComparison.Ordinal);
    string marker = $"newmtl material_{objectIndex}\n";
    int start = normalized.IndexOf(marker, StringComparison.Ordinal);
    if (start < 0)
        throw new InvalidOperationException($"Missing OBJ material for [{objectIndex}].");
    int end = normalized.IndexOf("\nnewmtl ", start + marker.Length,
        StringComparison.Ordinal);
    return end >= 0 ? normalized[start..end] : normalized[start..];
}

void TestUnsupportedAlphaCases(SmoExportScene scene, string outputDirectory)
{
    SmoExportMesh source = scene.Meshes.First(mesh => mesh.Positions.Length >= 2);

    Vector4[] varyingAlpha = Enumerable.Repeat(Vector4.One, source.Positions.Length).ToArray();
    varyingAlpha[0] = new Vector4(1f, 1f, 1f, 0.5f);
    SmoExportMesh varyingVertexMesh = source with
    {
        Colors = varyingAlpha,
        Texture = null,
        EffectTexture = null,
        MaterialColor = Vector4.One,
        UsesAlphaBlend = true
    };
    SmoExportScene varyingVertexScene = scene with { Meshes = [varyingVertexMesh] };
    ExpectThrows<InvalidDataException>(
        () => ObjExporter.Export(
            varyingVertexScene, Path.Combine(outputDirectory, "unsupported-alpha.obj")),
        "OBJ rejects varying COLOR_0 alpha");
    ExpectThrows<InvalidDataException>(
        () => FbxExporter.Export(
            varyingVertexScene, Path.Combine(outputDirectory, "unsupported-alpha.fbx"),
            Path.Combine(outputDirectory, "missing-blender.exe")),
        "FBX rejects COLOR_0 alpha before invoking Blender");

    var alphaTexture = new SmoExportTexture(
        -1, "synthetic alpha", 1, 1,
        [137, 80, 78, 71, 13, 10, 26, 10],
        [137, 80, 78, 71, 13, 10, 26, 10]);
    SmoExportMesh compoundedAlphaMesh = source with
    {
        Colors = [],
        Texture = alphaTexture,
        EffectTexture = null,
        MaterialColor = new Vector4(1f, 1f, 1f, 0.5f),
        UsesAlphaBlend = true
    };
    ExpectThrows<InvalidDataException>(
        () => FbxExporter.Export(
            scene with { Meshes = [compoundedAlphaMesh] },
            Path.Combine(outputDirectory, "compounded-alpha.fbx"),
            Path.Combine(outputDirectory, "missing-blender.exe")),
        "FBX rejects compounded texture and material alpha before invoking Blender");

    SmoExportMesh invalidAlphaMesh = source with
    {
        Colors = [],
        Texture = null,
        EffectTexture = null,
        MaterialColor = new Vector4(1f, 1f, 1f, float.NaN),
        UsesAlphaBlend = true
    };
    SmoExportScene invalidAlphaScene = scene with { Meshes = [invalidAlphaMesh] };
    ExpectThrows<InvalidDataException>(
        () => ObjExporter.Export(
            invalidAlphaScene, Path.Combine(outputDirectory, "invalid-alpha.obj")),
        "OBJ rejects non-finite alpha before writing output");
    ExpectThrows<InvalidDataException>(
        () => FbxExporter.Export(
            invalidAlphaScene, Path.Combine(outputDirectory, "invalid-alpha.fbx"),
            Path.Combine(outputDirectory, "missing-blender.exe")),
        "FBX rejects non-finite alpha before invoking Blender");
}

void ExpectThrows<TException>(Action action, string description)
    where TException : Exception
{
    try
    {
        action();
    }
    catch (TException)
    {
        Check(true, description);
        return;
    }
    Check(false, description);
}

int?[] BuildNodeParents(JsonElement nodes, out bool valid)
{
    var parents = new int?[nodes.GetArrayLength()];
    valid = true;
    for (int parent = 0; parent < nodes.GetArrayLength(); parent++)
    {
        if (!nodes[parent].TryGetProperty("children", out JsonElement children))
            continue;
        foreach (JsonElement childValue in children.EnumerateArray())
        {
            int child = childValue.GetInt32();
            if ((uint)child >= (uint)parents.Length)
            {
                valid = false;
                continue;
            }
            if (parents[child] is int existing && existing != parent)
                valid = false;
            else
                parents[child] = parent;
        }
    }
    return parents;
}

bool IsAncestor(int ancestor, int node, IReadOnlyList<int?> parents)
{
    var visited = new HashSet<int>();
    while ((uint)node < (uint)parents.Count && visited.Add(node))
    {
        if (node == ancestor)
            return true;
        if (parents[node] is not int parent)
            return false;
        node = parent;
    }
    return false;
}

bool AccessorViewHasNoTarget(
    JsonElement accessors,
    JsonElement bufferViews,
    int accessorIndex)
{
    if ((uint)accessorIndex >= (uint)accessors.GetArrayLength() ||
        !accessors[accessorIndex].TryGetProperty("bufferView", out JsonElement viewValue))
    {
        return false;
    }
    int viewIndex = viewValue.GetInt32();
    return (uint)viewIndex < (uint)bufferViews.GetArrayLength() &&
           !bufferViews[viewIndex].TryGetProperty("target", out _);
}

int FindMeshNode(JsonElement nodes, int meshIndex)
{
    for (int index = 0; index < nodes.GetArrayLength(); index++)
    {
        if (nodes[index].TryGetProperty("mesh", out JsonElement mesh) &&
            mesh.GetInt32() == meshIndex)
        {
            return index;
        }
    }
    return -1;
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
