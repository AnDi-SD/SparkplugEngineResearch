using System.Numerics;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;
using SmoImporter.Core;
using SmoViewer.Core;

internal static class StaticModelReplacementRegression
{
    public static void RunSharedTextureTarget(string targetArgument)
    {
        string targetPath = Path.GetFullPath(targetArgument);
        byte[] targetBefore = File.ReadAllBytes(targetPath);
        SmoDocument target = SmoDocument.Load(targetPath);
        ImportedTexture sharedTexture = CreateTexture(
            "shared",
            16,
            8,
            new Rgba32(40, 190, 120, 255));
        var donor = new ImportedScene(
            [
                CreateTriangle("shared_part_a", 0, 0),
                CreateTriangle("shared_part_b", 2, 0)
            ],
            [sharedTexture],
            [new ImportedMaterial("shared", "shared", 0)]);
        int selectedMeshIndex = FirstMeshIndex(target);
        SmoLevelModelGraphReplacementAnalysis plan =
            SmoLevelModelGraphReplacer.Analyze(
                target,
                selectedMeshIndex,
                donor,
                SmoLevelModelGraphWriteOptions.StandaloneStatic);
        Assert(plan.CanReplace,
            "shared-texture static analysis failed: " +
            string.Join(" | ", plan.Messages));
        Assert(plan.TargetMeshCount == 2 && plan.ImportedMeshCount == 2,
            "multipart static plan did not retain both native branches");

        string directory = Path.Combine(
            Path.GetTempPath(),
            $"SmoStaticSharedTexture-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        try
        {
            string output = Path.Combine(directory, "static-shared-texture.smo");
            SmoLevelModelGraphReplacer.ReplaceFile(
                target,
                selectedMeshIndex,
                donor,
                ReplacementTransform.Identity,
                output,
                SmoLevelModelGraphWriteOptions.StandaloneStatic);
            SmoDocument verified = SmoDocument.Load(output);
            Assert(!verified.HasErrors, "multipart static output has parser errors");
            SmoObjectEntry[] meshes = verified.Objects.Where(entry =>
                    entry.TypeHash == SmoClassIds.MeshData)
                .OrderBy(entry => entry.LogicalOffset)
                .ToArray();
            Assert(meshes.Length == 2,
                "multipart static output changed the native mesh count");
            IReadOnlyDictionary<int, SmoTextureBinding> bindings =
                SmoTextureBindingResolver.ResolveAll(verified);
            SmoTextureBinding[] resolved = meshes
                .Select(mesh => bindings[mesh.Index])
                .ToArray();
            Assert(resolved.All(binding =>
                    binding.Issue is null && binding.Texture is not null),
                "multipart static output lost a native texture binding");
            Assert(resolved.Select(binding => binding.Texture!.ObjectIndex)
                    .Distinct().Count() == 1,
                "shared native texture was unexpectedly split");
            Assert(resolved.All(binding =>
                    binding.Texture!.Width == 16 &&
                    binding.Texture.Height == 8),
                "shared donor texture dimensions changed");
            Assert(File.ReadAllBytes(targetPath).SequenceEqual(targetBefore),
                "multipart static regression changed its source target");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    public static void Run(string targetArgument)
    {
        string targetPath = Path.GetFullPath(targetArgument);
        SmoDocument target = SmoDocument.Load(targetPath);
        ImportedScene donor = BuildDonor();
        ModelPortingModeRecommendation recommendation =
            ModelPortingModeAnalyzer.Recommend(target, donor);
        Assert(recommendation.Mode == ModelPortingMode.StaticModel,
            "unskinned target/donor did not select the static porting mode");
        int selectedMeshIndex = FirstMeshIndex(target);
        SmoLevelModelGraphReplacementAnalysis plan =
            SmoLevelModelGraphReplacer.Analyze(
                target,
                selectedMeshIndex,
                donor,
                SmoLevelModelGraphWriteOptions.StandaloneStatic);
        Assert(plan.CanReplace,
            "static analysis failed: " + string.Join(" | ", plan.Messages));
        Assert(plan.ImportedMeshCount == 2,
            "static plan did not retain both donor material parts");

        string directory = Path.Combine(
            Path.GetTempPath(),
            $"SmoStaticModelRegression-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        try
        {
            string output = Path.Combine(directory, "static-two-part.smo");
            SmoLevelModelGraphFileResult result =
                SmoLevelModelGraphReplacer.ReplaceFile(
                    target,
                    selectedMeshIndex,
                    donor,
                    new ReplacementTransform(
                        1.5f,
                        Vector3.Zero,
                        new Vector3(2, 3, 4)),
                    output,
                    SmoLevelModelGraphWriteOptions.StandaloneStatic);
            Assert(File.Exists(output), "static writer did not install output");
            Assert(result.MeshCount == 2, "static result reports wrong mesh count");
            Assert(result.TextureCount == 2,
                "static result reports wrong imported texture count");
            Assert(result.BackupPath is null,
                "first static install unexpectedly created a backup");

            SmoDocument verified = SmoDocument.Load(output);
            Assert(!verified.HasErrors, "static output has parser errors");
            Assert(!verified.Objects.Any(entry =>
                    entry.TypeHash == SmoClassIds.Skin),
                "static output contains an spSkin object");
            SmoObjectEntry[] meshes = verified.Objects.Where(entry =>
                    entry.TypeHash == SmoClassIds.MeshData)
                .OrderBy(entry => entry.LogicalOffset)
                .ToArray();
            Assert(meshes.Length == 2,
                "static output did not create two mesh branches");
            Assert(meshes.All(entry =>
                    !SmoMeshDecoder.Decode(verified, entry).HasSkinningData),
                "static output wrote a skinned vertex layout");
            Assert(meshes.Select(entry => SmoMeshDecoder.Decode(verified, entry))
                    .All(mesh => mesh.VertexCount == 3 && mesh.TriangleCount == 1),
                "static output geometry counts changed");

            IReadOnlyDictionary<int, SmoTextureBinding> bindings =
                SmoTextureBindingResolver.ResolveAll(verified);
            SmoTexture[] textures = meshes.Select(entry => bindings[entry.Index])
                .Select(binding =>
                {
                    Assert(binding.Issue is null && binding.Texture is not null,
                        "static mesh has no unambiguous texture binding");
                    return binding.Texture!;
                })
                .ToArray();
            Assert(textures.Select(texture => (texture.Width, texture.Height))
                    .OrderBy(size => size.Width)
                    .SequenceEqual(new[] { (8, 8), (16, 8) }),
                "static textures were not written with donor dimensions");

            SmoLevelModelGraphFileResult replaced =
                SmoLevelModelGraphReplacer.ReplaceFile(
                    target,
                    selectedMeshIndex,
                    donor,
                    ReplacementTransform.Identity,
                    output,
                    SmoLevelModelGraphWriteOptions.StandaloneStatic);
            Assert(replaced.BackupPath is not null &&
                   File.Exists(replaced.BackupPath),
                "second static install did not retain an atomic backup");
            SmoDocument identityOutput = SmoDocument.Load(output);
            SmoDocument transformedBackup = SmoDocument.Load(replaced.BackupPath!);
            Vector3[] identityPositions = DecodeAllPositions(identityOutput);
            Vector3[] transformedPositions = DecodeAllPositions(transformedBackup);
            Assert(!identityPositions.SequenceEqual(transformedPositions),
                "static writer ignored the requested replacement transform");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    private static ImportedScene BuildDonor()
    {
        ImportedTexture firstTexture = CreateTexture(
            "red",
            8,
            8,
            new Rgba32(220, 30, 40, 255));
        ImportedTexture secondTexture = CreateTexture(
            "blue",
            16,
            8,
            new Rgba32(20, 90, 230, 255));
        var materials = new[]
        {
            new ImportedMaterial("red", "red", 0),
            new ImportedMaterial("blue", "blue", 1)
        };
        return new ImportedScene(
            [
                CreateTriangle("red_part", 0, 0),
                CreateTriangle("blue_part", 2, 1)
            ],
            [firstTexture, secondTexture],
            materials);
    }

    private static Vector3[] DecodeAllPositions(SmoDocument document) =>
        document.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.MeshData)
            .OrderBy(entry => entry.LogicalOffset)
            .SelectMany(entry => SmoMeshDecoder.Decode(document, entry).Positions)
            .ToArray();

    private static int FirstMeshIndex(SmoDocument document) =>
        document.Objects.First(entry =>
            entry.TypeHash == SmoClassIds.MeshData).Index;

    private static ImportedMesh CreateTriangle(
        string name,
        float x,
        int materialIndex) => new(
            name,
            [
                new Vector3(x, 0, 0),
                new Vector3(x + 1, 0, 0),
                new Vector3(x, 1, 0)
            ],
            [Vector3.UnitZ, Vector3.UnitZ, Vector3.UnitZ],
            [Vector2.Zero, Vector2.UnitX, Vector2.UnitY],
            [0, 1, 2],
            [0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF],
            materialIndex);

    private static ImportedTexture CreateTexture(
        string name,
        int width,
        int height,
        Rgba32 color)
    {
        using var image = new Image<Rgba32>(width, height, color);
        using var stream = new MemoryStream();
        image.SaveAsPng(stream);
        return new ImportedTexture(
            name,
            "image/png",
            width,
            height,
            stream.ToArray());
    }

    private static void Assert(bool condition, string message)
    {
        if (!condition)
            throw new InvalidOperationException(message);
    }
}
