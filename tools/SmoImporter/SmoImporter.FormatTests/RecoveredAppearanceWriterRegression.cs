using System.Diagnostics;
using System.Globalization;
using System.Numerics;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text.Json;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.Advanced;
using SixLabors.ImageSharp.PixelFormats;
using SmoImporter.Core;
using SmoViewer.Core;

internal static class RecoveredAppearanceWriterRegression
{
    public static int Run(string staticTargetPath, string humanoidTargetPath,
        string fixtureObj, string fixtureGlb, string outputDirectory)
    {
        string directory = Path.GetFullPath(outputDirectory);
        Directory.CreateDirectory(directory);
        string[] inputs = [staticTargetPath, humanoidTargetPath, fixtureObj, fixtureGlb];
        var hashes = inputs.ToDictionary(path => Path.GetFullPath(path), path => Hash(path));
        var results = new List<object>();
        SmoDocument staticTarget = SmoDocument.Load(staticTargetPath);
        foreach ((string name, string path) in new[] { ("static-obj", fixtureObj), ("static-glb", fixtureGlb) })
        {
            ImportedScene donor = ImportedModelReader.Read(path);
            string output = Path.Combine(directory, name + ".smo");
            var stopwatch = Stopwatch.StartNew();
            int mesh = staticTarget.Objects.First(entry => entry.TypeHash == SmoClassIds.MeshData).Index;
            SmoLevelModelGraphReplacer.ReplaceFile(staticTarget, mesh, donor, ReplacementTransform.Identity,
                output, SmoLevelModelGraphWriteOptions.StandaloneStatic, protectedInputPaths: inputs);
            results.Add(Verify(output, donor, stopwatch.Elapsed));
        }

        SmoDocument target = SmoDocument.Load(humanoidTargetPath);
        ImportedScene source = SmoModelReader.Read(humanoidTargetPath);
        int[] usedMaterials = source.Meshes.Select(mesh => mesh.MaterialIndex).Distinct()
            .Where(index => index >= 0 && source.Materials[index].BaseColorTextureIndex >= 0).ToArray();
        Assert(usedMaterials.Length >= 2, "The real humanoid fixture needs two independently assigned materials.");
        int omitted = usedMaterials[0];
        string healthyName = source.Textures[source.Materials[usedMaterials[1]].BaseColorTextureIndex].Name;
        foreach ((string name, string reference) in new[] { ("skin-missing", "missing-payload.png"), ("skin-no-name-guess", healthyName), ("skin-vertex-alpha", "missing-alpha.png") })
        {
            ImportedMaterial[] materials = source.Materials.ToArray();
            materials[omitted] = materials[omitted] with { BaseColorTextureIndex = -1, BaseColorTextureName = reference };
            if (name == "skin-vertex-alpha") materials[omitted] = materials[omitted] with { AlphaMode = ImportedMaterialAlphaMode.Blend };
            ImportedMesh[] sourceMeshes = name == "skin-vertex-alpha"
                ? source.Meshes.Select(mesh => mesh.MaterialIndex == omitted ? mesh with
                    { DiffuseColorsArgb = Enumerable.Repeat(0x80FFFFFFu, mesh.Positions.Length).ToArray() } : mesh).ToArray()
                : source.Meshes.ToArray();
            ImportedScene donor = ImportedTextureRecovery.Recover(source with { SourceMaterials = materials, Meshes = sourceMeshes }, "SMO-derived test");
            var stopwatch = Stopwatch.StartNew();
            GlbSkinTransferPlan plan = SmoSkinnedGlbReplacer.Analyze(target, donor);
            File.WriteAllText(Path.Combine(directory, name + "-plan.json"), JsonSerializer.Serialize(plan,
                new JsonSerializerOptions { WriteIndented = true }));
            Assert(plan.CanReplace, $"{name}: recovered material still blocks skinned export: {string.Join(" | ", plan.Messages)}");
            string output = Path.Combine(directory, name + ".smo");
            SmoSkinnedGlbReplacer.Replace(target, donor, ReplacementTransform.Identity, output,
                SkinnedGeometryTransferMode.PreservePreparedGeometry);
            results.Add(Verify(output, donor, stopwatch.Elapsed));
        }
        Assert(hashes.All(pair => Hash(pair.Key) == pair.Value), "A writer modified its input fixture.");
        File.WriteAllText(Path.Combine(directory, "recovered-appearance-results.json"),
            JsonSerializer.Serialize(new { passed = true, outputs = results, inputHashes = hashes },
                new JsonSerializerOptions { WriteIndented = true }));
        Console.WriteLine($"RECOVERED APPEARANCE WRITER PASS: {results.Count} production writes + strict geometry/material/image readbacks.");
        return 0;
    }

    private static object Verify(string path, ImportedScene donor, TimeSpan elapsed)
    {
        SmoDocument output = SmoDocument.Load(path);
        Assert(!output.HasErrors, $"{path}: output has structural parser errors.");
        var bindings = SmoTextureBindingResolver.ResolveAll(output);
        int texturedTriangles = 0;
        int untexturedTriangles = 0;
        foreach (SmoObjectEntry entry in output.Objects.Where(entry => entry.TypeHash == SmoClassIds.MeshData))
        {
            SmoMesh mesh = SmoMeshDecoder.Decode(output, entry);
            int triangles = NondegenerateTriangles(mesh);
            if (triangles == 0) continue;
            Assert(bindings.TryGetValue(entry.Index, out SmoTextureBinding? binding), "Written mesh has no loaded material binding.");
            Assert(binding!.Issue is null, $"{path}: written mesh material issue: {binding.Issue}");
            if (binding.Texture is not { } texture)
            {
                untexturedTriangles += triangles;
                ImportedMesh[] untexturedSources = donor.Meshes.Where(source => (uint)source.MaterialIndex < donor.Materials.Count &&
                    donor.Materials[source.MaterialIndex].BaseColorTextureIndex < 0).ToArray();
                HashSet<uint> expectedColors = untexturedSources.SelectMany(source => source.DiffuseColors.Length == source.Positions.Length
                    ? source.DiffuseColors : [0xFFFFFFFFu]).ToHashSet();
                Assert(!donor.HasSkinning || mesh.HasDiffuseColors && mesh.DiffuseColorsArgb.All(expectedColors.Contains),
                    "Untextured source vertex colors changed during skinned serialization.");
                if (donor.HasSkinning && expectedColors.Any(color => color >> 24 < 255))
                    Assert(binding.LoadedMaterial?.VertexAlpha == 1 &&
                           binding.LoadedMaterial.Passes.Single().Blend == 2 &&
                           entry.ParentIndex is int parent && SmoSkinDecoder.TryDecode(output,
                               output.Objects[parent], out SmoSkin? skin, out _) && skin?.AlphaSortEnable == 1,
                        "Untextured vertex/material alpha lost its native blend contract.");
                continue;
            }
            texturedTriangles += triangles;
            Assert(donor.Textures.Any(source => source.Width == texture.Width && source.Height == texture.Height &&
                ImportedTextureImageTools.SerializedBgraMatches(source.Data, texture.Width, texture.Height,
                    texture.Bgra32Pixels.Span, out _)), "Output substituted a template/neutral image for a missing donor image.");
        }
        int expectedTextured = donor.Meshes.Where(mesh => (uint)mesh.MaterialIndex < donor.Materials.Count &&
            donor.Materials[mesh.MaterialIndex].BaseColorTextureIndex >= 0).Sum(mesh => mesh.TriangleIndices.Length / 3);
        int expectedTotal = donor.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3);
        Assert(texturedTriangles == expectedTextured && untexturedTriangles == expectedTotal - expectedTextured,
            $"{path}: actual textured/untextured triangles {texturedTriangles}/{untexturedTriangles}, expected {expectedTextured}/{expectedTotal - expectedTextured}.");
        ImportedScene readback = SmoModelReader.Read(path);
        Assert(GeometryAndSkinFingerprint(donor).SequenceEqual(GeometryAndSkinFingerprint(readback)),
            $"{path}: triangle positions, winding, UV channels, named skin influences or healthy image assignment changed.");
        return new { path, sha256 = Hash(path), milliseconds = elapsed.TotalMilliseconds, texturedTriangles, untexturedTriangles };
    }

    private static string[] GeometryAndSkinFingerprint(ImportedScene scene)
    {
        var triangles = new List<string>();
        string[] imageKeys = scene.Textures.Select(texture =>
        {
            using Image<Rgba32> image = Image.Load<Rgba32>(texture.Data);
            using var hash = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
            for (int row = 0; row < image.Height; row++)
                hash.AppendData(MemoryMarshal.AsBytes(image.DangerousGetPixelRowMemory(row).Span));
            return $"{image.Width}x{image.Height}:" + Convert.ToHexString(hash.GetHashAndReset());
        }).ToArray();
        foreach (ImportedMesh mesh in scene.Meshes)
        {
            int imageIndex = (uint)mesh.MaterialIndex < scene.Materials.Count ? scene.Materials[mesh.MaterialIndex].BaseColorTextureIndex : -1;
            string imageKey = imageIndex >= 0 ? imageKeys[imageIndex] : "no-image";
            for (int index = 0; index < mesh.TriangleIndices.Length; index += 3)
            {
                int a = checked((int)mesh.TriangleIndices[index]);
                int b = checked((int)mesh.TriangleIndices[index + 1]);
                int c = checked((int)mesh.TriangleIndices[index + 2]);
                if (Vector3.Cross(mesh.Positions[b] - mesh.Positions[a], mesh.Positions[c] - mesh.Positions[a]).LengthSquared() <= 1e-20f)
                    continue;
                string[] corners = [Corner(a), Corner(b), Corner(c)];
                triangles.Add(Enumerable.Range(0, 3).Select(start => string.Join("|", Enumerable.Range(0, 3).Select(offset => corners[(start + offset) % 3])))
                    .Min(StringComparer.Ordinal)!);
            }
            string Corner(int vertex)
            {
                Vector3 position = mesh.Positions[vertex];
                Vector2 uv = mesh.TextureCoordinates.Length == mesh.Positions.Length ? mesh.TextureCoordinates[vertex] : Vector2.Zero;
                Vector2 uv1 = mesh.SecondaryTextureCoordinates.Length == mesh.Positions.Length ? mesh.SecondaryTextureCoordinates[vertex] : uv;
                var influences = new Dictionary<string, float>(StringComparer.Ordinal);
                if (mesh.Skinning is { } skin)
                {
                    ImportedJointIndices indices = skin.JointIndices[vertex];
                    Vector4 weights = skin.Weights[vertex];
                    Add(indices.X, weights.X); Add(indices.Y, weights.Y); Add(indices.Z, weights.Z); Add(indices.W, weights.W);
                    void Add(ushort slot, float weight)
                    {
                        if (weight > 0.000001f)
                        {
                            string name = skin.Skeleton.JointNames[slot];
                            influences[name] = influences.GetValueOrDefault(name) + weight;
                        }
                    }
                }
                float total = influences.Values.Sum();
                return imageKey + $";{Number(position.X, 3)},{Number(position.Y, 3)},{Number(position.Z, 3)};" +
                    $"{Number(uv.X, 5)},{Number(uv.Y, 5)};{Number(uv1.X, 5)},{Number(uv1.Y, 5)};" +
                    string.Join(",", influences.OrderBy(pair => pair.Key, StringComparer.Ordinal).Select(pair => pair.Key + ":" + Number(pair.Value / total, 4)));
            }
        }
        return triangles.Order(StringComparer.Ordinal).ToArray();
        static string Number(float value, int digits)
        {
            double rounded = Math.Round(value, digits);
            return (rounded == 0 ? 0 : rounded).ToString("F" + digits, CultureInfo.InvariantCulture);
        }
    }

    private static int NondegenerateTriangles(SmoMesh mesh)
    {
        int count = 0;
        for (int index = 0; index < mesh.TriangleIndices.Length; index += 3)
        {
            var a = mesh.Positions[mesh.TriangleIndices[index]];
            var b = mesh.Positions[mesh.TriangleIndices[index + 1]];
            var c = mesh.Positions[mesh.TriangleIndices[index + 2]];
            if (System.Numerics.Vector3.Cross(b - a, c - a).LengthSquared() > 1e-20f) count++;
        }
        return count;
    }

    private static string Hash(string path) => Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path)));
    private static void Assert(bool condition, string message)
    {
        if (!condition) throw new InvalidOperationException(message);
    }
}
