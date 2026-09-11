using System.Diagnostics;
using System.Security.Cryptography;
using System.Text.Json;
using SmoImporter.Core;
using SmoViewer.Core;

internal static class NativeMaterialTransferRegression
{
    public static int Run(string targetPath, string donorPath, string outputDirectory)
    {
        Directory.CreateDirectory(outputDirectory);
        string outputPath = Path.Combine(outputDirectory, "transferred.smo");
        if (File.Exists(outputPath)) throw new InvalidOperationException("Use a fresh transfer output directory.");
        byte[] targetBytes = File.ReadAllBytes(targetPath), donorBytes = File.ReadAllBytes(donorPath);
        var target = SmoDocument.ParseOwned(targetBytes.ToArray(), targetPath);
        var donor = SmoDocument.ParseOwned(donorBytes.ToArray(), donorPath);
        int checks = 0;
        void Check(bool value, string message)
        { if (!value) throw new InvalidDataException(message); ++checks; }
        var watch = Stopwatch.StartNew();
        var plan = SmoNativeVisualGraphReplacer.Analyze(target, donor);
        Check(plan.CanReplace, string.Join("; ", plan.Errors));
        double analyzeSeconds = watch.Elapsed.TotalSeconds;
        watch.Restart();
        var result = SmoNativeVisualGraphReplacer.Replace(target, donor, outputPath);
        double replaceSeconds = watch.Elapsed.TotalSeconds;
        var output = SmoDocument.Load(outputPath);
        var sourceResources = SmoLoadedResources.Get(donor);
        var outputResources = SmoLoadedResources.Get(output);
        Check(sourceResources.LoadIssue is null && outputResources.LoadIssue is null,
            sourceResources.LoadIssue ?? outputResources.LoadIssue ?? "Both material graphs must load.");
        var sourceMaterials = Materials(sourceResources, donor);
        var outputMaterials = Materials(outputResources, output);
        Check(sourceMaterials.Length == donor.Objects.Count(entry => entry.TypeHash == SmoClassIds.MaterialData) &&
            sourceMaterials.Length == outputMaterials.Length, "Every donor material must have an actual loaded counterpart.");
        int passes = 0, layers = 0, animationKeys = 0;
        var snapshots = new List<object>();
        for (int index = 0; index < sourceMaterials.Length; ++index)
        {
            var before = sourceMaterials[index]; var after = outputMaterials[index];
            Check(donor.Objects[before.ObjectIndex].RawName.Span.SequenceEqual(output.Objects[after.ObjectIndex].RawName.Span),
                "Material ordering/name identity changed.");
            string original = Snapshot(before, donor), transferred = Snapshot(after, output);
            Check(original == transferred, $"Material {index} pass/layer/controller/runtime texture state changed during native transfer.");
            passes += before.Passes.Count; layers += before.Passes.Sum(pass => pass.Layers.Count);
            animationKeys += before.Passes.SelectMany(pass => pass.Layers).Sum(layer => layer.Animation?.Keys.Count ?? 0);
            snapshots.Add(new { donorId = before.ObjectId, outputId = after.ObjectId, material = JsonSerializer.Deserialize<JsonElement>(original) });
        }
        var outputTrace = outputResources.ReferenceTrace ?? throw new InvalidDataException(outputResources.ReferenceTraceIssue);
        var observed = outputTrace.CaptureRange(output, 0, output.Data.Length);
        var outputIds = output.Objects.Select(entry => entry.Id).ToHashSet();
        Check(observed.Sites.All(site => site.ObjectId == 0 || outputIds.Contains(site.ObjectId)),
            "Every actual output reference resolves into the new catalog.");
        Check(target.Data.Span.SequenceEqual(targetBytes) && donor.Data.Span.SequenceEqual(donorBytes) &&
            File.ReadAllBytes(targetPath).AsSpan().SequenceEqual(targetBytes) &&
            File.ReadAllBytes(donorPath).AsSpan().SequenceEqual(donorBytes), "Source files and owned input arrays must remain unchanged.");
        var report = new { status = "passed", checks, target = Path.GetFullPath(targetPath), donor = Path.GetFullPath(donorPath),
            targetSha256 = Hash(targetBytes), donorSha256 = Hash(donorBytes), outputSha256 = result.Sha256,
            analyzeSeconds, replaceSeconds, plan.IgnoredDonorBoneNames, result.MeshCount, result.TextureCount,
            materials = sourceMaterials.Length, passes, layers, animationKeys, observedSites = observed.Sites.Count,
            processPeakWorkingSet = Process.GetCurrentProcess().PeakWorkingSet64, snapshots };
        File.WriteAllText(Path.Combine(outputDirectory, "report.json"), JsonSerializer.Serialize(report,
            new JsonSerializerOptions { WriteIndented = true }) + "\n");
        Console.WriteLine($"PASS native materials: {checks} checks, {sourceMaterials.Length} materials, {passes} passes, " +
            $"{layers} layers, {animationKeys} animation keys, {replaceSeconds:F3}s replacement");
        return 0;
    }

    private static SmoLoadedMaterial[] Materials(SmoLoadedResources resources, SmoDocument document) =>
        resources.Models.Values.Select(model => model.Material).OfType<SmoLoadedMaterial>()
            .DistinctBy(material => material.ObjectId).OrderBy(material => document.Objects[material.ObjectIndex].PhysicalOffset).ToArray();

    private static string Snapshot(SmoLoadedMaterial material, SmoDocument document)
    {
        string? Identity(uint id)
        {
            if (id == 0) return null;
            var entry = document.Objects.Single(value => value.Id == id);
            return $"{entry.TypeHash:X8}:{Convert.ToHexString(entry.RawName.Span)}";
        }
        object? Texture(SmoLoadedTexture? loaded)
        {
            if (loaded is null) return null;
            var texture = loaded.Texture ?? throw new InvalidDataException(loaded.Issue ?? "Missing actual texture.");
            if (!texture.HasRuntimeMipChain) throw new InvalidDataException("Material transfer acceptance requires actual runtime mip bytes.");
            return new { identity = Identity(loaded.ObjectId), texture.Width, texture.Height, texture.MipLevelCount,
                levels = texture.MipLevels.Select(level => new { level.Width, level.Height, sha256 = Hash(level.Bgra32Pixels.Span) }).ToArray() };
        }
        return JsonSerializer.Serialize(new { material.RenderStates, material.VertexAlpha,
            colors = material.Colors.Select(color => new[] { Bits(color.X), Bits(color.Y), Bits(color.Z), Bits(color.W) }),
            power = material.SpecularPower is float power ? Bits(power) : (uint?)null,
            colorController = Identity(material.ColorControllerId),
            passes = material.Passes.Select(pass => new { pass.Blend, layers = pass.Layers.Select(layer => new
            {
                layer.ClassId, texture = Texture(layer.Texture), uvController = Identity(layer.UvControllerId), layer.UvEnabled,
                uv = layer.UvMatrix.Select(Bits), layer.TextureStates, layer.AnimationBoundHere, layer.UvBoundHere,
                animation = layer.Animation is null ? null : new { identity = Identity(layer.Animation.ObjectId),
                    duration = Bits(layer.Animation.Duration), keys = layer.Animation.Keys.Select(key => new { time = Bits(key.Time), texture = Texture(key.Texture) }) }
            }) }) });
    }
    private static uint Bits(float value) => BitConverter.SingleToUInt32Bits(value);
    private static string Hash(ReadOnlySpan<byte> bytes) => Convert.ToHexString(SHA256.HashData(bytes));
}
