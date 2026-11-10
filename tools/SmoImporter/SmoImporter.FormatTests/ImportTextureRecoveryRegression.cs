using System.Diagnostics;
using System.Numerics;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;
using SmoImporter.Core;

internal static class ImportTextureRecoveryRegression
{
    private sealed record Result(string Name, string Path, string Outcome, double Milliseconds,
        int Meshes, int Textures, IReadOnlyList<string> Warnings);

    public static int Run(string outputDirectory)
    {
        string directory = Path.GetFullPath(outputDirectory);
        Directory.CreateDirectory(directory);
        var results = new List<Result>();
        byte[] good = CreatePng();
        byte[] corrupt = CreateHeaderOnlyBmp();
        string objDirectory = Path.Combine(directory, "obj");
        Directory.CreateDirectory(Path.Combine(objDirectory, "images"));
        File.WriteAllBytes(Path.Combine(objDirectory, "images", "healthy.png"), good);
        File.WriteAllBytes(Path.Combine(objDirectory, "images", "corrupt.png"), corrupt);
        string mtl = Path.Combine(objDirectory, "fixture.mtl");
        File.WriteAllText(mtl, "newmtl Healthy\nmap_Kd images/healthy.png\n" +
            "newmtl Missing\nmap_Kd images/missing.png\n" +
            "newmtl Corrupt\nmap_Kd images/corrupt.png\nnewmtl UnboundMaterial\n");
        string obj = Path.Combine(objDirectory, "texture-recovery.obj");
        File.WriteAllText(obj, "mtllib fixture.mtl\nv 0 0 0\nv 1 0 0\nv 0 1 0\n" +
            "vt 0 0\nvt 1 0\nvt 0 1\n" +
            "g NoMaterial\nf 1/1 2/2 3/3\n" +
            string.Concat(new[] { "Healthy", "Missing", "Corrupt", "UnboundMaterial", "UndefinedMaterial" }
                .Select(name => $"g {name}\nusemtl {name}\nf 1/1 2/2 3/3\n")));
        Verify("OBJ mixed missing/corrupt/unbound", obj, () => ObjModelReader.Read(obj), 6, 1, scene =>
        {
            Assert(scene.Meshes[0].MaterialIndex == -1, "Unbound OBJ mesh acquired a material.");
            Assert(scene.Materials.Single(material => material.Name == "Healthy").BaseColorTextureIndex == 0,
                "Healthy OBJ image was not preserved.");
            Assert(scene.Materials.Single(material => material.Name == "Healthy").AlphaMode == ImportedMaterialAlphaMode.Blend,
                "OBJ alpha inference was lost.");
            Assert(scene.Materials.Where(material => material.Name != "Healthy").All(material => material.BaseColorTextureIndex == -1),
                "A failed or unbound OBJ material acquired a different image.");
            Assert(scene.ImportWarnings.Any(warning => warning.Contains("Corrupt", StringComparison.Ordinal)), "No named corrupt-material warning.");
        });
        Verify("OBJ explicit MTL recovery", obj, () => ObjModelReader.ReadWithMaterialLibrary(obj, mtl), 6, 1);

        string orphanDirectory = Path.Combine(directory, "obj-no-guesses");
        Directory.CreateDirectory(orphanDirectory);
        File.WriteAllBytes(Path.Combine(orphanDirectory, "Orphan.png"), good);
        string orphan = Path.Combine(orphanDirectory, "orphan.obj");
        File.WriteAllText(orphan, "o Orphan\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
        Verify("OBJ no guessed adjacent assignment", orphan, () => ObjModelReader.Read(orphan), 1, 0);

        string glb = Path.Combine(directory, "texture-recovery.glb");
        WriteGlb(glb, good, corrupt, "mixed");
        Verify("GLB mixed codec/binding failures", glb, () => GlbModelReader.Read(glb), 6, 1, scene =>
        {
            Assert(scene.Materials[0].BaseColorTextureIndex == 0, "Healthy GLB image was not preserved.");
            Assert(scene.Materials[0].AlphaMode == ImportedMaterialAlphaMode.Opaque,
                "GLB authored OPAQUE was overridden by PNG alpha.");
            Assert(scene.Materials.Skip(1).All(material => material.BaseColorTextureIndex == -1), "GLB bad image/reference was substituted.");
            Assert(scene.Textures[0].Data.SequenceEqual(good), "Healthy encoded GLB image changed.");
        });
        Verify("GLB geometry-only uses same texture recovery", glb, () => GlbModelReader.ReadGeometryOnly(glb), 6, 1);
        string unknownFormat = Path.Combine(directory, "unknown-image-format.glb");
        WriteGlb(unknownFormat, good, "This is not a decodable texture."u8.ToArray(), "mixed");
        Verify("GLB unknown codec recovery", unknownFormat, () => GlbModelReader.Read(unknownFormat), 6, 1);
        string badLinkTypes = Path.Combine(directory, "invalid-texture-link-types.glb");
        WriteGlb(badLinkTypes, good, corrupt, "bad-link-types");
        Verify("GLB malformed optional texture link recovery", badLinkTypes, () => GlbModelReader.Read(badLinkTypes), 6, 1);
        string empty = Path.Combine(directory, "empty.glb");
        WriteGlb(empty, good, corrupt, "empty");
        Reject("GLB empty mesh array fatal", empty, () => GlbModelReader.Read(empty));
        string structural = Path.Combine(directory, "texture-out-of-range.glb");
        WriteGlb(structural, good, corrupt, "bad-range");
        Reject("GLB image range is structural fatal", structural, () => GlbModelReader.Read(structural));
        string shortUv = Path.Combine(directory, "short-uv.glb");
        WriteGlb(shortUv, good, corrupt, "short-uv");
        Reject("GLB attribute count fatal", shortUv, () => GlbModelReader.Read(shortUv));
        string invalidGeometry = Path.Combine(directory, "nan-position.glb");
        WriteGlb(invalidGeometry, good, corrupt, "nan");
        Reject("GLB nonfinite geometry fatal", invalidGeometry, () => GlbModelReader.Read(invalidGeometry));

        string payload = Path.Combine(directory, "native-fbx-texture-recovery.bin");
        WriteFbxPayload(payload, good, corrupt);
        Verify("FBX bridge payload recovery", payload, () => FbxModelReader.ReadPayload(payload), 6, 1, scene =>
        {
            Assert(scene.Materials[0].BaseColorTextureIndex == 0, "Healthy FBX image was not preserved.");
            Assert(scene.Materials[0].AlphaMode == ImportedMaterialAlphaMode.Blend, "FBX alpha inference was lost.");
            Assert(scene.Materials.Skip(1).All(material => material.BaseColorTextureIndex == -1), "Failed FBX image/reference was substituted.");
        });

        var recovered = GlbModelReader.Read(glb);
        ImportedTexture replacement = new("corrupt", "image/png", 2, 2, good);
        ImportedTextureCatalogResult reassigned = ImportedTextureCatalog.ResolveExternalOverrides(
            recovered, [], new Dictionary<int, ImportedTexture> { [1] = replacement });
        Assert(reassigned.EffectiveScene.Materials[1].BaseColorTextureIndex >= 0,
            "An omitted image could not be assigned explicitly later.");
        ImportedScene recoveredObj = ObjModelReader.Read(obj);
        int unboundMaterial = recoveredObj.Materials.Select((material, index) => (material, index))
            .Single(item => item.material.Name == "UnboundMaterial").index;
        ImportedTexture nameMatchedImage = replacement with { Name = "UnboundMaterial" };
        ImportedTextureCatalogResult unboundCatalog = ImportedTextureCatalog.ResolveExternalOverrides(recoveredObj, [nameMatchedImage]);
        Assert(unboundCatalog.EffectiveScene.Materials[unboundMaterial].BaseColorTextureIndex == -1 &&
               unboundCatalog.UnusedExternalTextures.Count == 1,
            "An unbound material acquired an image automatically by material name.");
        ImportedTextureSlot unboundSlot = unboundCatalog.Slots.Single(slot => slot.MaterialIndices.Contains(unboundMaterial));
        ImportedTextureCatalogResult explicitUnbound = ImportedTextureCatalog.ResolveExternalOverrides(
            recoveredObj, [], new Dictionary<int, ImportedTexture> { [unboundSlot.Key] = nameMatchedImage });
        Assert(explicitUnbound.EffectiveScene.Materials[unboundMaterial].BaseColorTextureIndex >= 0 &&
               explicitUnbound.EffectiveScene.Materials.Single(material => material.Name == "UndefinedMaterial").BaseColorTextureIndex == -1 &&
               explicitUnbound.EffectiveScene.Meshes[0].MaterialIndex == -1,
            "Explicit assignment did not stay within its unbound material slot.");
        using var cancellation = new CancellationTokenSource();
        cancellation.Cancel();
        try { _ = GlbModelReader.Read(glb, cancellation.Token); throw new InvalidOperationException("Pre-cancelled GLB accepted."); }
        catch (OperationCanceledException) { }

        File.WriteAllText(Path.Combine(directory, "texture-recovery-results.json"),
            JsonSerializer.Serialize(new { passed = true, cases = results, note = "Native FBX cases exercise the managed bridge payload; SDK parsing is separate." },
                new JsonSerializerOptions { WriteIndented = true }));
        Console.WriteLine($"IMPORT TEXTURE RECOVERY PASS: {results.Count} retained fixture cases; explicit reassignment and cancellation passed.");
        return 0;

        void Verify(string name, string path, Func<ImportedScene> read, int meshes, int textures, Action<ImportedScene>? extra = null)
        {
            byte[] before = SHA256.HashData(File.ReadAllBytes(path));
            var stopwatch = Stopwatch.StartNew();
            ImportedScene scene = read();
            Assert(stopwatch.Elapsed < TimeSpan.FromSeconds(3), $"{name} exceeded three seconds.");
            Assert(scene.Meshes.Count == meshes && scene.Textures.Count == textures, $"{name}: unexpected mesh/texture count.");
            Assert(scene.ImportWarnings.Any(warning => warning.Contains("[TEXTURE_", StringComparison.Ordinal)), $"{name}: recovery warning absent.");
            Assert(scene.Materials.All(material => material.PreviewTextureUri is null), "Unavailable preview URI escaped recovery.");
            Assert(before.SequenceEqual(SHA256.HashData(File.ReadAllBytes(path))), "Source fixture was modified during import.");
            extra?.Invoke(scene);
            results.Add(new Result(name, path, "warning-and-continue", stopwatch.Elapsed.TotalMilliseconds,
                scene.Meshes.Count, scene.Textures.Count, scene.ImportWarnings));
        }
        void Reject(string name, string path, Func<ImportedScene> read)
        {
            var stopwatch = Stopwatch.StartNew();
            try { _ = read(); throw new InvalidOperationException($"{name}: damaged structural data was accepted."); }
            catch (InvalidDataException error)
            {
                Assert(stopwatch.Elapsed < TimeSpan.FromSeconds(3), $"{name} exceeded three seconds.");
                Assert(!string.IsNullOrWhiteSpace(error.Message), "Fatal error has no explanation.");
                results.Add(new Result(name, path, "fatal", stopwatch.Elapsed.TotalMilliseconds, 0, 0, [error.Message]));
            }
        }
    }

    private static byte[] CreatePng()
    {
        using var image = new Image<Rgba32>(2, 2);
        image[0, 0] = new Rgba32(255, 0, 0, 255);
        image[1, 0] = new Rgba32(0, 255, 0, 255);
        image[0, 1] = new Rgba32(0, 0, 255, 128);
        image[1, 1] = new Rgba32(255, 255, 255, 255);
        using var stream = new MemoryStream();
        image.SaveAsPng(stream);
        return stream.ToArray();
    }

    private static byte[] CreateHeaderOnlyBmp()
    {
        using var stream = new MemoryStream();
        using var writer = new BinaryWriter(stream);
        writer.Write((byte)'B'); writer.Write((byte)'M'); writer.Write(70); writer.Write(0); writer.Write(54);
        writer.Write(40); writer.Write(2); writer.Write(2); writer.Write((ushort)1); writer.Write((ushort)24);
        writer.Write(0); writer.Write(16); writer.Write(0); writer.Write(0); writer.Write(0); writer.Write(0);
        // Header declares sixteen pixel bytes which are deliberately absent.
        return stream.ToArray();
    }

    private static void WriteGlb(string path, byte[] good, byte[] corrupt, string variant)
    {
        using var binary = new MemoryStream();
        using (var writer = new BinaryWriter(binary, Encoding.UTF8, leaveOpen: true))
        {
            foreach (float value in new[] { variant == "nan" ? float.NaN : 0, 0, 0, 1, 0, 0, 0, 1, 0 }) writer.Write(value);
            writer.Write((ushort)0); writer.Write((ushort)1); writer.Write((ushort)2); writer.Write((ushort)0);
            foreach (float value in new[] { 0f, 0, 1, 0, 0, 1 }) writer.Write(value);
            writer.Write(good);
            writer.Write(corrupt);
        }
        object[] views =
        [
            new { buffer = 0, byteOffset = 0, byteLength = 36 },
            new { buffer = 0, byteOffset = 36, byteLength = 6 },
            new { buffer = 0, byteOffset = 44, byteLength = 24 },
            new { buffer = 0, byteOffset = 68, byteLength = variant == "bad-range" ? int.MaxValue : good.Length },
            new { buffer = 0, byteOffset = 68 + good.Length, byteLength = corrupt.Length }
        ];
        var document = new
        {
            asset = new { version = "2.0" }, buffers = new[] { new { byteLength = (int)binary.Length } }, bufferViews = views,
            accessors = new object[]
            {
                new { bufferView = 0, componentType = 5126, count = 3, type = "VEC3" },
                new { bufferView = 1, componentType = 5123, count = 3, type = "SCALAR" },
                new { bufferView = 2, componentType = 5126, count = variant == "short-uv" ? 2 : 3, type = "VEC2" }
            },
            images = new object[] { new { name = "healthy", mimeType = "image/png", bufferView = 3 }, new { name = "corrupt", mimeType = "image/png", bufferView = 4 } },
            textures = new object[] { new { source = 0 }, new { source = 1 },
                variant == "bad-link-types" ? (object)new { source = "invalid" } : new { source = 99 }, new { sampler = 0 } },
            materials = Enumerable.Range(0, 5).Select(index => new
            {
                name = new[] { "Healthy", "Corrupt", "MissingImage", "MissingSource", "BadTextureIndex" }[index],
                pbrMetallicRoughness = new { baseColorTexture = new { index = variant == "bad-link-types" && index == 4 ? (object)"invalid" : index == 4 ? 99 : index } }
            }).ToArray(),
            meshes = new[] { new { name = "Fixture", primitives = Enumerable.Range(0, 6).Select(index =>
                index == 5 ? (object)new { attributes = new { POSITION = 0, TEXCOORD_0 = 2 }, indices = 1 } :
                new { attributes = new { POSITION = 0, TEXCOORD_0 = 2 }, indices = 1, material = index }).ToArray() } },
            nodes = new[] { new { mesh = 0 } }, scenes = new[] { new { nodes = new[] { 0 } } }, scene = 0
        };
        object serializedDocument = variant == "empty" ? new
        {
            asset = new { version = "2.0" }, buffers = new[] { new { byteLength = (int)binary.Length } },
            meshes = Array.Empty<object>(), accessors = Array.Empty<object>()
        } : document;
        byte[] json = JsonSerializer.SerializeToUtf8Bytes(serializedDocument);
        int jsonSize = (json.Length + 3) & ~3;
        byte[] bytes = binary.ToArray();
        int binSize = (bytes.Length + 3) & ~3;
        using var stream = File.Create(path);
        using var output = new BinaryWriter(stream);
        output.Write(0x46546C67u); output.Write(2u); output.Write(28 + jsonSize + binSize);
        output.Write(jsonSize); output.Write(0x4E4F534Au); output.Write(json);
        for (int index = json.Length; index < jsonSize; index++) output.Write((byte)' ');
        output.Write(binSize); output.Write(0x004E4942u); output.Write(bytes);
        for (int index = bytes.Length; index < binSize; index++) output.Write((byte)0);
    }

    private static void WriteFbxPayload(string path, byte[] good, byte[] corrupt)
    {
        using var writer = new BinaryWriter(File.Create(path));
        writer.Write("SMOFBXI1"u8); writer.Write(1u); writer.Write(2u);
        Texture("healthy", good); Texture("corrupt", corrupt);
        writer.Write(5u);
        for (int index = 0; index < 5; index++)
        {
            Text(new[] { "Healthy", "Corrupt", "Missing", "BadIndex", "UnboundMaterial" }[index]);
            Text(index == 4 ? "" : new[] { "healthy", "corrupt", "missing", "bad" }[index]);
            writer.Write(new[] { 0, 1, -1, 99, -1 }[index]);
        }
        writer.Write(6u);
        for (int index = 0; index < 6; index++)
        {
            Text($"mesh_{index}"); writer.Write(index == 5 ? -1 : index);
            writer.Write(3u); foreach (float value in new[] { 0f, 0, 0, 1, 0, 0, 0, 1, 0 }) writer.Write(value);
            writer.Write(0u); writer.Write(0u); writer.Write(0u);
            writer.Write(3u); writer.Write(0u); writer.Write(1u); writer.Write(2u); writer.Write((byte)0);
        }
        void Texture(string name, byte[] data)
        {
            Text(name); Text("image/png"); writer.Write(0); writer.Write(0); writer.Write((uint)data.Length); writer.Write(data); Text(name + ".png");
        }
        void Text(string text)
        {
            byte[] data = Encoding.UTF8.GetBytes(text); writer.Write((uint)data.Length); writer.Write(data);
        }
    }

    private static void Assert(bool condition, string message)
    {
        if (!condition) throw new InvalidOperationException(message);
    }
}
