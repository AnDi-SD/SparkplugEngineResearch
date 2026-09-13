using System.Diagnostics;
using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using SmoViewer.Core;
using SmoViewer.Sparkplug;

namespace SmoViewer.FormatTests;

internal static class SmoMaterialDrawRegression
{
    internal static int Run(string source, string output)
    {
        if (File.Exists(output)) throw new IOException("Fresh report required.");
        var watch = Stopwatch.StartNew();
        int checks = 0, passes = 0, textures = 0, transforms = 0;
        void Check(bool ok, string message) { if (!ok) throw new InvalidDataException(message); ++checks; }
        var document = SmoDocument.Load(source);
        var hash = Convert.ToHexString(SHA256.HashData(document.Data.Span));
        var loaded = SmoLoadedResources.Get(document);
        Check(loaded.LoadIssue is null, loaded.LoadIssue ?? "");
        var materials = loaded.Models.Values.Select(m => m.Material).OfType<SmoLoadedMaterial>()
            .DistinctBy(m => m.ObjectIndex).OrderBy(m => m.ObjectIndex).ToArray();
        using var scene = new SparkplugSceneRuntime(document, new Dictionary<int, SmoSkin>());
        var runtime = scene.Materials;
        var controllers = materials.SelectMany(m => runtime.ReferencedControllers(m.ObjectIndex)).Distinct().ToArray();
        runtime.ApplyControllers(controllers, .125f);
        var reports = new List<object>();
        foreach (var initial in materials)
        {
            var before = runtime.ReadMaterial(initial.ObjectIndex);
            var draw = runtime.CaptureDraw(initial.ObjectIndex, 5);
            Check(draw.Passes.Count == initial.Passes.Count, "All actual material passes retained");
            var after = runtime.ReadMaterial(initial.ObjectIndex);
            foreach (var pass in draw.Passes)
            {
                ++passes;
                Check(pass.Raster.KnownMask == 0xffff, "Required mapped render states were produced");
                Check(pass.VertexAlpha == initial.VertexAlpha && pass.SpecularPower == initial.SpecularPower,
                    "Native vertex alpha and initialized power retained");
                // Modes2..5 and7 leave the copied color block unchanged (CP32).
                if (before.RenderStates[8] is 2 or 3 or 4 or 5 or 7)
                    Check(pass.Ambient == before.Colors[0] && pass.Diffuse == before.Colors[1] &&
                        pass.Specular == before.Colors[2] && pass.Emissive == before.Colors[3],
                        "Renderer installation uses the pre-update color block (CP33)");
                var actualPass = after.Passes[(int)pass.Ordinal];
                for (int s = 0; s < 8; ++s)
                {
                    var stage = pass.Stages[s];
                    Check(stage.KnownMask == 1023, "All mapped stage states produced");
                    if (s < actualPass.Layers.Count)
                        Check(stage.Texture?.ObjectId == actualPass.Layers[s].Texture?.ObjectId, "Canonical updated texture identity");
                    else Check(stage.Texture is null && stage.ColorOperation == 1, "Unused higher stage disabled through actual default material");
                    if (stage.Texture is not null) { ++textures; Check(stage.Texture.Texture is not null, stage.Texture.Issue ?? "Missing upload"); }
                    if (stage.TransformFlags != 0) { ++transforms; Check(stage.UVTransform.HasValue, "Enabled UV transform has actual producer"); }
                }
            }
            Check(after.RenderStates[7] == after.Passes[^1].Blend, "Actual final pass writes selected material blend");
            var repeat = runtime.CaptureDraw(initial.ObjectIndex, 5);
            Check(repeat.Passes.Count == draw.Passes.Count, "Repeated capture retains passes when device caches suppress callbacks");
            for (int p = 0; p < repeat.Passes.Count; ++p)
            {
                Check(repeat.Passes[p].Raster == draw.Passes[p].Raster, "Cached draw retains raster output");
                for (int s = 0; s < 8; ++s)
                {
                    var first = draw.Passes[p].Stages[s]; var next = repeat.Passes[p].Stages[s];
                    // Inactive UV registers legitimately retain the matrix of
                    // a later pass. Only an enabled transform affects this draw.
                    if (first.TransformFlags == 0 && next.TransformFlags == 0)
                    { first = first with { UVTransform = null }; next = next with { UVTransform = null }; }
                    Check(first == next, $"Cached active stage material {initial.ObjectIndex}/pass {p}/stage {s}");
                }
            }
            reports.Add(new { initial.ObjectIndex, initial.ObjectId, initial.RenderStates,
                passes = draw.Passes.Select(p => new { p.Ordinal, p.VertexAlpha, p.Raster,
                    stages = p.Stages.Select(s => new { texture = s.Texture?.ObjectId, s.ColorOperation,
                        s.AlphaOperation, s.Coordinates, s.TransformFlags, hasUV = s.UVTransform.HasValue }).ToArray() }).ToArray() });
        }
        // Reordering callers shares renderer state but not the immutable copies.
        foreach (var material in materials.Reverse())
            Check(runtime.CaptureDraw(material.ObjectIndex, 6).Passes.Count == material.Passes.Count, "Reverse draw order succeeds");
        runtime.Dispose();
        try { runtime.CaptureDraw(materials[0].ObjectIndex, 7); throw new Exception("Disposed runtime accepted"); }
        catch (ObjectDisposedException) { ++checks; }
        Check(Convert.ToHexString(SHA256.HashData(document.Data.Span)) == hash, "Source bytes unchanged");
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(output))!);
        File.WriteAllText(output, JsonSerializer.Serialize(new { source = Path.GetFullPath(source), hash,
            materials = materials.Length, passes, textures, transforms, checks,
            seconds = watch.Elapsed.TotalSeconds, reports }, new JsonSerializerOptions { WriteIndented = true }));
        Console.WriteLine($"PASS material draw: {materials.Length} materials, {passes} passes, {checks} checks");
        return 0;
    }
}
