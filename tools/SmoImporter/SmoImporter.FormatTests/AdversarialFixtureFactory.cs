using System.Buffers.Binary;
using System.Diagnostics;
using System.Numerics;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;
using SmoExporter.Core;
using SmoImporter.Core;
using SmoViewer.Core;
using SmoViewer.Sparkplug;

/// <summary>
/// Small, retained inputs for the importer and its GUI. The manifest describes
/// intended outcomes; it is not a claim that those outcomes have been verified.
/// Run reader cases in separate, externally time-limited processes so a broken
/// reader cannot strand a background Task or hide a native crash.
/// </summary>
internal static class AdversarialFixtureFactory
{
    private static readonly JsonSerializerOptions JsonOptions = new() { WriteIndented = true };
    private const string TinyObj = """
        mtllib tiny.mtl
        v 0 0 0
        v 1 0 0
        v 0 1 0
        v 2 0 0
        v 3 0 0
        v 2 1 0
        vt 0 0
        vt 1 0
        vt 0 1
        vn 0 0 1
        o left_part
        usemtl tiny
        f 1/1/1 2/2/1 3/3/1
        o right_part
        usemtl tiny
        f 4/1/1 5/2/1 6/3/1
        """;

    public static int Emit(string repositoryRoot, string outputDirectory)
    {
        string root = Path.GetFullPath(repositoryRoot);
        string output = Path.GetFullPath(outputDirectory);
        if (Directory.Exists(output) || File.Exists(output))
            throw new InvalidOperationException("Use a new fixture output directory.");

        var provenance = new List<SourceFile>();
        string Source(string relative)
        {
            string path = Path.Combine(root, relative);
            if (!File.Exists(path)) throw new FileNotFoundException("Fixture source is absent.", path);
            provenance.Add(new SourceFile(path, Hash(path)));
            return path;
        }

        string bloom = Source("local-data/pc-pristine/Media/Characters/Bloom/bloom_jeans.smo");
        string princess = Source("local-data/pc-pristine/Media/Characters/Bloom/bloom_princess.smo");
        string creature = Source("local-data/pc-pristine/Media/Characters/IceWorm/Iceworm.smo");
        string barrel = Source("local-data/pc-pristine/Media/SFX/barrel.smo");
        string bodySan = Source("local-data/pc-pristine/Media/Characters/Bloom/blwalk.san");
        Directory.CreateDirectory(output);
        Directory.CreateDirectory(Path.Combine(output, "models"));
        Directory.CreateDirectory(Path.Combine(output, "broken"));
        Directory.CreateDirectory(Path.Combine(output, "animations"));

        var cases = new List<FixtureCase>();
        void Add(string id, string relative, string expected, string reason,
            string reader = "model", string? geometryOnlyExpected = null, int timeoutSeconds = 15)
        {
            cases.Add(new FixtureCase(id, relative.Replace('\\', '/'), reader, expected,
                geometryOnlyExpected ?? expected, timeoutSeconds, reason, Hash(Path.Combine(output, relative))));
        }
        string At(string relative) => Path.Combine(output, relative);
        void Copy(string source, string relative) => File.Copy(source, At(relative));
        void Write(string relative, string content) => File.WriteAllText(At(relative), content, new UTF8Encoding(false));

        using (var image = new Image<Rgba32>(2, 2, new Rgba32(70, 180, 120, 255)))
            image.SaveAsPng(At("models/tiny.png"));
        byte[] png = File.ReadAllBytes(At("models/tiny.png"));
        Write("models/tiny.mtl", "newmtl tiny\nKd 1 1 1\nmap_Kd tiny.png\n");
        Write("models/tiny.obj", TinyObj + "\n");
        Add("tiny-obj", "models/tiny.obj", "pass", "Two textured rigid meshes, six vertices, two triangles.");

        (JsonObject document, byte[] binary) = CreateTinyGlb(png);
        WriteGlb(At("models/tiny.glb"), document, binary);
        Add("tiny-glb", "models/tiny.glb", "pass", "Two textured rigid mesh instances with independent placements.");

        var staticTarget = SmoDocument.Load(barrel);
        var tinyScene = ImportedModelReader.Read(At("models/tiny.obj"));
        SmoLevelModelGraphReplacer.ReplaceFile(staticTarget,
            staticTarget.Objects.First(entry => entry.TypeHash == SmoClassIds.MeshData).Index,
            tinyScene, ReplacementTransform.Identity, At("models/tiny-static.smo"),
            SmoLevelModelGraphWriteOptions.StandaloneStatic,
            protectedInputPaths: [barrel, At("models/tiny.obj")]);
        Add("tiny-static-smo", "models/tiny-static.smo", "pass", "Production static writer output, strict readback required.");

        Copy(bloom, "models/humanoid-target.smo");
        Copy(princess, "models/humanoid-donor.smo");
        Copy(creature, "models/creature-target.smo");
        Copy(creature, "models/creature-donor.smo");
        Add("humanoid-target", "models/humanoid-target.smo", "pass", "Pristine humanoid; use the animations directory for explicit player checks.");
        Add("humanoid-donor", "models/humanoid-donor.smo", "pass", "Different native humanoid appearance for direct transfer.");
        Add("creature-target", "models/creature-target.smo", "pass", "Real nonhuman target: no automatic humanoid fallback should be offered as usable.");
        Add("creature-donor", "models/creature-donor.smo", "pass", "Real nonhuman donor for exact skeleton/native transfer.");
        var humanoidScene = SmoSceneBuilder.Build(SmoDocument.Load(bloom), new SmoExportOptions(AnimationPaths: null));
        GlbExporter.Export(humanoidScene, At("models/prepared-bloom.glb"));
        Add("prepared-bloom-glb", "models/prepared-bloom.glb", "pass", "Exporter-produced real game skeleton, skin weights and textures.", timeoutSeconds: 30);
        MutateGlb(At("models/prepared-bloom.glb"), At("models/alias-bloom.glb"), value =>
        {
            foreach (JsonNode? node in value["nodes"]!.AsArray())
                if (node?["name"]?.GetValue<string>() == "Pelvis") node["name"] = "hips";
        });
        Add("alias-bloom-glb", "models/alias-bloom.glb", "pass", "Reviewed hips-to-Pelvis alias exercises adaptation instead of guessed mapping.", timeoutSeconds: 30);
        MutateGlb(At("models/prepared-bloom.glb"), At("models/unmapped-bloom.glb"), value =>
        {
            foreach (JsonNode? skin in value["skins"]!.AsArray())
                foreach (JsonNode? joint in skin!["joints"]!.AsArray())
                {
                    int nodeIndex = joint!.GetValue<int>();
                    value["nodes"]![nodeIndex]!["name"] = $"UnmappedWeightedJoint_{nodeIndex}";
                }
        });
        Add("unmapped-bloom-glb", "models/unmapped-bloom.glb", "pass", "Structurally valid unknown weighted joints; adapt must reject without guessing.", timeoutSeconds: 30);
        MutateGlb(At("models/prepared-bloom.glb"), At("broken/glb-missing-weight-attribute.glb"), value =>
        {
            foreach (JsonNode? mesh in value["meshes"]!.AsArray())
                foreach (JsonNode? primitive in mesh!["primitives"]!.AsArray())
                    primitive!["attributes"]!.AsObject().Remove("WEIGHTS_0");
        });
        Add("glb-missing-weight-attribute", "broken/glb-missing-weight-attribute.glb", "fatal",
            "Broken rig can be ignored only by the explicit geometry-only/create-weights mode.", geometryOnlyExpected: "pass", timeoutSeconds: 30);
        MutateGlb(At("models/prepared-bloom.glb"), At("broken/glb-invalid-skin-joint.glb"),
            value => value["skins"]![0]!["joints"]![0] = 999_999);
        Add("glb-invalid-skin-joint", "broken/glb-invalid-skin-joint.glb", "fatal",
            "Invalid skin-node index must reject in normal import; geometry-only may retain usable surfaces.", geometryOnlyExpected: "pass", timeoutSeconds: 30);
        var creatureScene = SmoSceneBuilder.Build(SmoDocument.Load(creature), new SmoExportOptions(AnimationPaths: null));
        GlbExporter.Export(creatureScene, At("models/prepared-creature.glb"));
        Add("prepared-creature-glb", "models/prepared-creature.glb", "pass", "Prepared nonhuman skeleton; test without humanoid protection step.", timeoutSeconds: 30);

        Copy(bodySan, "animations/body.san");
        Add("body-animation", "animations/body.san", "pass", "Actual native SAN: play, seek, speed, loop and stop in result preview.", reader: "san");
        string? creatureSan = Directory.EnumerateFiles(Path.GetDirectoryName(creature)!, "*.san")
            .Order(StringComparer.OrdinalIgnoreCase).FirstOrDefault();
        if (creatureSan is not null)
        {
            Source(Path.GetRelativePath(root, creatureSan));
            Copy(creatureSan, "animations/creature.san");
            Add("creature-animation", "animations/creature.san", "pass", "Actual adjacent SAN for the creature/manual directory route.", reader: "san");
        }

        void GlbVariant(string id, Action<JsonObject> mutate, string reason)
        {
            JsonObject copy = JsonNode.Parse(document.ToJsonString())!.AsObject();
            mutate(copy);
            string relative = $"broken/{id}.glb";
            WriteGlb(At(relative), copy, binary);
            Add(id, relative, "fatal", reason);
        }
        GlbVariant("glb-cycle", value =>
        {
            value["nodes"]![0]!["children"] = new JsonArray(1);
            value["nodes"]![1]!["children"] = new JsonArray(0);
        }, "Cyclic node hierarchy must reject promptly.");
        GlbVariant("glb-multiple-parents", value =>
        {
            value["nodes"]!.AsArray().Add(new JsonObject { ["children"] = new JsonArray(1) });
            value["nodes"]![0]!["children"] = new JsonArray(1);
        }, "A node shared by two parents has no unambiguous world placement.");
        GlbVariant("glb-excessive-accessor", value => value["accessors"]![0]!["count"] = 12_000_001,
            "Tiny file declares excessive decoded geometry; reject before allocation.");
        GlbVariant("glb-missing-position", value => value["meshes"]![0]!["primitives"]![0]!["attributes"]!.AsObject().Remove("POSITION"),
            "Missing mandatory POSITION needs a clear model-data error.");
        GlbVariant("glb-unsupported-lines", value => value["meshes"]![0]!["primitives"]![0]!["mode"] = 1,
            "Line primitives cannot be silently converted into triangle geometry.");
        GlbVariant("glb-accessor-outside-view", value => value["accessors"]![0]!["byteOffset"] = 8,
            "Accessor crosses its bufferView even though bytes remain elsewhere in BIN.");
        GlbVariant("glb-buffer-view-index", value => value["accessors"]![0]!["bufferView"] = 999,
            "Invalid bufferView index must reject with a visible data error.");

        byte[] badIndex = binary.ToArray();
        BinaryPrimitives.WriteUInt16LittleEndian(badIndex.AsSpan(96), 99);
        WriteGlb(At("broken/glb-index-out-of-range.glb"), document, badIndex);
        Add("glb-index-out-of-range", "broken/glb-index-out-of-range.glb", "fatal", "Triangle index 99 references only three positions.");
        byte[] nanPosition = binary.ToArray();
        BinaryPrimitives.WriteInt32LittleEndian(nanPosition, BitConverter.SingleToInt32Bits(float.NaN));
        WriteGlb(At("broken/glb-nonfinite-position.glb"), document, nanPosition);
        Add("glb-nonfinite-position", "broken/glb-nonfinite-position.glb", "fatal", "Nonfinite POSITION cannot enter camera bounds or the writer.");
        byte[] zeroNormals = binary.ToArray();
        Array.Clear(zeroNormals, 36, 36);
        WriteGlb(At("broken/glb-zero-normals.glb"), document, zeroNormals);
        Add("glb-zero-normals", "broken/glb-zero-normals.glb", "warning", "Repair normals from triangles, preserve geometry, surface repair notice.");

        byte[] tinyGlb = File.ReadAllBytes(At("models/tiny.glb"));
        byte[] wrongLength = tinyGlb.ToArray();
        BinaryPrimitives.WriteUInt32LittleEndian(wrongLength.AsSpan(8), (uint)(wrongLength.Length + 4));
        File.WriteAllBytes(At("broken/glb-wrong-file-length.glb"), wrongLength);
        Add("glb-wrong-file-length", "broken/glb-wrong-file-length.glb", "fatal", "GLB declared total length contradicts actual bytes.");
        byte[] wrongChunk = tinyGlb.ToArray();
        BinaryPrimitives.WriteUInt32LittleEndian(wrongChunk.AsSpan(16), 0x004E4942);
        File.WriteAllBytes(At("broken/glb-wrong-json-chunk-type.glb"), wrongChunk);
        Add("glb-wrong-json-chunk-type", "broken/glb-wrong-json-chunk-type.glb", "fatal", "First GLB chunk must be JSON, regardless of parseable payload.");
        File.WriteAllBytes(At("broken/glb-truncated.glb"), tinyGlb[..^9]);
        Add("glb-truncated", "broken/glb-truncated.glb", "fatal", "Truncated BIN/image payload must fail within the reader budget.");

        Write("broken/obj-empty.obj", "v 0 0 0\n");
        Add("obj-empty", "broken/obj-empty.obj", "fatal", "A model with no faces is unusable.");
        Write("broken/obj-index-out-of-range.obj", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 99\n");
        Add("obj-index-out-of-range", "broken/obj-index-out-of-range.obj", "fatal", "Face index 99 is outside the vertex table.");
        Write("broken/obj-nonfinite-position.obj", "v NaN 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
        Add("obj-nonfinite-position", "broken/obj-nonfinite-position.obj", "fatal", "Nonfinite source position must not poison camera/fitting.");
        Write("broken/obj-invalid-number.obj", "v wrong 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
        Add("obj-invalid-number", "broken/obj-invalid-number.obj", "fatal", "Malformed numeric token requires a concise user-facing error.");
        Write("broken/unsupported.gltf", "{\"asset\":{\"version\":\"2.0\"}}\n");
        Add("unsupported-format", "broken/unsupported.gltf", "fatal", "JSON glTF is outside the four supported donor formats; explain supported choices.");
        Write("broken/obj-zero-height.obj", "v 0 0 0\nv 1 0 0\nv 0 0 1\nf 1 2 3\n");
        Add("obj-zero-height", "broken/obj-zero-height.obj", "pass", "Valid flat geometry; height fitting uses the finite bounds diagonal as its fallback extent.");
        Write("models/flat-plane.obj", "mtllib tiny.mtl\nv 0 0 0\nv 1 0 0\nv 0 0 1\nvt 0 0\nvt 1 0\nvt 0 1\nusemtl tiny\nf 1/1 2/2 3/3\n");
        Add("obj-textured-flat-plane", "models/flat-plane.obj", "pass", "Textured zero-height plane is a valid static donor and must fit by diagonal.");
        Write("models/zero-extent.obj", "mtllib tiny.mtl\nv 0 0 0\nv 0 0 0\nv 0 0 0\nvt 0 0\nvt 1 0\nvt 0 1\nusemtl tiny\nf 1/1 2/2 3/3\n");
        Add("obj-zero-extent", "models/zero-extent.obj", "pass", "Finite but fully collapsed geometry; fitting must visibly reject its zero extent.");

        byte[] native = File.ReadAllBytes(bloom);
        File.WriteAllBytes(At("broken/smo-truncated.smo"), native[..64]);
        Add("smo-truncated", "broken/smo-truncated.smo", "fatal", "Native container ends inside its metadata.");
        byte[] badMagic = native.ToArray();
        Array.Fill(badMagic, (byte)0, 0, Math.Min(16, badMagic.Length));
        File.WriteAllBytes(At("broken/smo-bad-header.smo"), badMagic);
        Add("smo-bad-header", "broken/smo-bad-header.smo", "fatal", "Intact payload with invalid native header; do not manufacture metadata.");
        byte[] san = File.ReadAllBytes(bodySan);
        File.WriteAllBytes(At("animations/truncated.san"), san[..Math.Min(32, san.Length)]);
        Add("san-truncated", "animations/truncated.san", "fatal", "Animation selection must report failure and leave the result preview usable.", reader: "san");
        byte[] badSan = san.ToArray();
        Array.Fill(badSan, (byte)0, 0, Math.Min(16, badSan.Length));
        File.WriteAllBytes(At("animations/bad-header.san"), badSan);
        Add("san-bad-header", "animations/bad-header.san", "fatal", "Bad animation header must not clear or corrupt the prepared model.", reader: "san");

        var workflows = new[]
        {
            new WorkflowCase("static-obj", "static", "models/tiny-static.smo", "models/tiny.obj", "pass", 60),
            new WorkflowCase("static-glb", "static", "models/tiny-static.smo", "models/tiny.glb", "pass", 60),
            new WorkflowCase("native-humanoid", "native", "models/humanoid-target.smo", "models/humanoid-donor.smo", "pass", 60),
            new WorkflowCase("prepared-humanoid", "prepared", "models/humanoid-target.smo", "models/prepared-bloom.glb", "pass", 60),
            new WorkflowCase("adapt-humanoid", "adapt", "models/humanoid-target.smo", "models/prepared-bloom.glb", "pass", 60),
            new WorkflowCase("adapt-reviewed-alias", "adapt", "models/humanoid-target.smo", "models/alias-bloom.glb", "pass", 60),
            new WorkflowCase("adapt-unknown-weighted-block", "adapt", "models/humanoid-target.smo", "models/unmapped-bloom.glb", "fatal", 30),
            new WorkflowCase("prepared-broken-rig-block", "prepared", "models/humanoid-target.smo", "broken/glb-missing-weight-attribute.glb", "fatal", 30),
            new WorkflowCase("native-creature", "native", "models/creature-target.smo", "models/creature-donor.smo", "pass", 60),
            new WorkflowCase("prepared-creature", "prepared", "models/creature-target.smo", "models/prepared-creature.glb", "pass", 60),
            new WorkflowCase("generate-creature-block", "generate", "models/creature-target.smo", "models/tiny.obj", "fatal", 30),
            new WorkflowCase("flat-plane-fit", "static", "models/tiny-static.smo", "models/flat-plane.obj", "pass", 60),
            new WorkflowCase("zero-extent-fit-block", "static", "models/tiny-static.smo", "models/zero-extent.obj", "fatal", 30)
        };
        foreach (SourceFile source in provenance)
            if (Hash(source.Path) != source.Sha256) throw new InvalidOperationException("Fixture generation changed a source.");
        File.WriteAllText(At("manifest.json"), JsonSerializer.Serialize(new
        {
            Schema = 1,
            Description = "Expected outcomes, not verification results. Each reader/SAN case needs process isolation; playback requires GUI checks too.",
            Cases = cases,
            WorkflowCases = workflows,
            SourceFiles = provenance
        }, JsonOptions) + "\n", new UTF8Encoding(false));
        Console.WriteLine($"ADVERSARIAL FIXTURES EMITTED: {cases.Count} reader/animation cases, {workflows.Length} workflows; {At("manifest.json")}");
        return 0;
    }

    public static int RunReaderCase(string fixtureDirectory, string caseId, bool geometryOnly = false)
    {
        string directory = Path.GetFullPath(fixtureDirectory);
        using JsonDocument manifest = JsonDocument.Parse(File.ReadAllBytes(Path.Combine(directory, "manifest.json")));
        FixtureCase fixture = manifest.RootElement.GetProperty("Cases").EnumerateArray()
            .Select(value => value.Deserialize<FixtureCase>()!)
            .Single(value => value.Id == caseId);
        if (fixture.Reader is not ("model" or "san")) throw new ArgumentException("Unknown fixture reader.");
        string path = Path.GetFullPath(Path.Combine(directory, fixture.Path));
        string expected = geometryOnly ? fixture.GeometryOnlyExpected : fixture.Expected;
        string before = Hash(path);
        if (before != fixture.Sha256) throw new InvalidDataException("Fixture hash does not match its manifest.");
        var watch = Stopwatch.StartNew();
        ImportedScene? scene = null;
        float? animationDuration = null;
        int? animationTracks = null;
        int? animationFrames = null;
        Exception? rejection = null;
        try
        {
            if (fixture.Reader == "san")
            {
                using SparkplugAnimationClip clip = SparkplugAnimationClip.Load(path);
                animationDuration = clip.Duration;
                animationTracks = clip.Tracks.Count;
                animationFrames = clip.FrameCount;
            }
            else
                scene = geometryOnly ? ImportedModelReader.ReadGeometryOnly(path) : ImportedModelReader.Read(path);
        }
        catch (Exception error) when (error is SmoFormatException or InvalidDataException or IOException or JsonException or
            FormatException or OverflowException or ArgumentException or InvalidOperationException or KeyNotFoundException or NotSupportedException)
        {
            rejection = error;
        }
        bool expectedRejection = expected == "fatal";
        bool passed = expectedRejection ? rejection is not null : rejection is null;
        string? invariantIssue = scene is null ? null : SceneIssue(scene);
        if (animationDuration is float duration && (!float.IsFinite(duration) || duration <= 0 || animationTracks.GetValueOrDefault() <= 0))
            invariantIssue = "Native loader accepted an unusable animation duration/track set.";
        passed &= invariantIssue is null;
        if (expected == "warning") passed &= scene is { ImportWarnings.Count: > 0 };
        passed &= before == Hash(path);
        passed &= watch.Elapsed <= TimeSpan.FromSeconds(fixture.TimeoutSeconds);
        var report = new
        {
            Id = caseId, GeometryOnly = geometryOnly, Expected = expected, Passed = passed,
            ElapsedSeconds = watch.Elapsed.TotalSeconds,
            RejectionType = rejection?.GetType().FullName, Error = rejection?.Message,
            InvariantIssue = invariantIssue, Warnings = scene?.ImportWarnings,
            Meshes = scene?.Meshes.Count, Vertices = scene?.Meshes.Sum(mesh => mesh.Positions.Length),
            Triangles = scene?.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3),
            Textures = scene?.Textures.Count, Sha256 = before,
            AnimationDuration = animationDuration, AnimationTracks = animationTracks, AnimationFrames = animationFrames
        };
        string name = $"result-{caseId}-{(geometryOnly ? "geometry" : "full")}.json";
        File.WriteAllText(Path.Combine(directory, name), JsonSerializer.Serialize(report, JsonOptions) + "\n", new UTF8Encoding(false));
        Console.WriteLine(JsonSerializer.Serialize(report));
        return passed ? 0 : 1;
    }

    private static string? SceneIssue(ImportedScene scene)
    {
        if (scene.Meshes.Count == 0) return "Reader accepted a scene with no meshes.";
        if (scene.Meshes.All(mesh => mesh.TriangleIndices.Length == 0)) return "Reader accepted a scene with no triangles.";
        foreach (ImportedMesh mesh in scene.Meshes)
        {
            if (mesh.Positions.Any(value => !float.IsFinite(value.X) || !float.IsFinite(value.Y) || !float.IsFinite(value.Z)))
                return $"Reader accepted nonfinite positions in '{mesh.Name}'.";
            if (mesh.TriangleIndices.Length % 3 != 0 || mesh.TriangleIndices.Any(value => value >= mesh.Positions.Length))
                return $"Reader accepted invalid triangle indices in '{mesh.Name}'.";
        }
        return null;
    }

    private static (JsonObject, byte[]) CreateTinyGlb(byte[] png)
    {
        var binary = new byte[104 + png.Length];
        Buffer.BlockCopy(new float[] { 0, 0, 0, 1, 0, 0, 0, 1, 0 }, 0, binary, 0, 36);
        Buffer.BlockCopy(new float[] { 0, 0, 1, 0, 0, 1, 0, 0, 1 }, 0, binary, 36, 36);
        Buffer.BlockCopy(new float[] { 0, 0, 1, 0, 0, 1 }, 0, binary, 72, 24);
        Buffer.BlockCopy(new ushort[] { 0, 1, 2 }, 0, binary, 96, 6);
        Buffer.BlockCopy(png, 0, binary, 104, png.Length);
        JsonObject document = JsonSerializer.SerializeToNode(new
        {
            asset = new { version = "2.0" }, buffers = new[] { new { byteLength = binary.Length } },
            bufferViews = new[]
            {
                new { buffer = 0, byteOffset = 0, byteLength = 36 },
                new { buffer = 0, byteOffset = 36, byteLength = 36 },
                new { buffer = 0, byteOffset = 72, byteLength = 24 },
                new { buffer = 0, byteOffset = 96, byteLength = 6 },
                new { buffer = 0, byteOffset = 104, byteLength = png.Length }
            },
            accessors = new object[]
            {
                new { bufferView = 0, componentType = 5126, count = 3, type = "VEC3", min = new[] { 0, 0, 0 }, max = new[] { 1, 1, 0 } },
                new { bufferView = 1, componentType = 5126, count = 3, type = "VEC3" },
                new { bufferView = 2, componentType = 5126, count = 3, type = "VEC2" },
                new { bufferView = 3, componentType = 5123, count = 3, type = "SCALAR" }
            },
            meshes = new[] { TinyMesh("left_part"), TinyMesh("right_part") },
            nodes = new object[] { new { name = "left_part", mesh = 0 }, new { name = "right_part", mesh = 1, translation = new[] { 2, 0, 0 } } },
            scenes = new[] { new { nodes = new[] { 0, 1 } } }, scene = 0,
            images = new[] { new { bufferView = 4, mimeType = "image/png", name = "tiny" } },
            textures = new[] { new { source = 0 } },
            materials = new[] { new { name = "tiny", pbrMetallicRoughness = new { baseColorTexture = new { index = 0 } } } }
        })!.AsObject();
        return (document, binary);
    }

    private static object TinyMesh(string name) => new
    {
        name, primitives = new[] { new { attributes = new Dictionary<string, int> { ["POSITION"] = 0, ["NORMAL"] = 1, ["TEXCOORD_0"] = 2 }, indices = 3, material = 0 } }
    };

    private static void MutateGlb(string inputPath, string outputPath, Action<JsonObject> mutate)
    {
        byte[] file = File.ReadAllBytes(inputPath);
        int jsonLength = checked((int)BinaryPrimitives.ReadUInt32LittleEndian(file.AsSpan(12)));
        int binaryHeader = checked(20 + jsonLength);
        int binaryLength = checked((int)BinaryPrimitives.ReadUInt32LittleEndian(file.AsSpan(binaryHeader)));
        JsonObject document = JsonNode.Parse(file.AsSpan(20, jsonLength))!.AsObject();
        mutate(document);
        WriteGlb(outputPath, document, file.AsSpan(binaryHeader + 8, binaryLength).ToArray());
    }

    private static void WriteGlb(string path, JsonObject document, byte[] binary)
    {
        byte[] json = JsonSerializer.SerializeToUtf8Bytes(document);
        int jsonLength = checked((json.Length + 3) & ~3);
        int binaryLength = checked((binary.Length + 3) & ~3);
        using var stream = File.Create(path);
        using var writer = new BinaryWriter(stream, Encoding.UTF8);
        writer.Write(0x46546C67u); writer.Write(2u); writer.Write(checked((uint)(28 + jsonLength + binaryLength)));
        writer.Write((uint)jsonLength); writer.Write(0x4E4F534Au); writer.Write(json);
        for (int index = json.Length; index < jsonLength; index++) writer.Write((byte)' ');
        writer.Write((uint)binaryLength); writer.Write(0x004E4942u); writer.Write(binary);
        for (int index = binary.Length; index < binaryLength; index++) writer.Write((byte)0);
    }

    private static string Hash(string path) => Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path)));
    private sealed record SourceFile(string Path, string Sha256);
    private sealed record FixtureCase(string Id, string Path, string Reader, string Expected,
        string GeometryOnlyExpected, int TimeoutSeconds, string Reason, string Sha256);
    private sealed record WorkflowCase(string Id, string Mode, string Target, string Donor, string Expected, int TimeoutSeconds);
}
