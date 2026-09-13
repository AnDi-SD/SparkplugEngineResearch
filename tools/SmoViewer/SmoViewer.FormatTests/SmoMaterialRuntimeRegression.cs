using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using SmoViewer.Core;
using SmoViewer.Sparkplug;

namespace SmoViewer.FormatTests;

internal static class SmoMaterialRuntimeRegression
{
    internal static int Run(string source, string output)
    {
        Directory.CreateDirectory(output);
        int checks = 0;
        void Check(bool condition, string message)
        { if (!condition) throw new InvalidDataException(message); ++checks; }
        void Reject(Action action)
        {
            try { action(); }
            catch (Exception error) when (error is InvalidDataException or ArgumentOutOfRangeException or ObjectDisposedException)
            { ++checks; return; }
            throw new InvalidDataException("Expected explicit rejection");
        }
        var document = SmoDocument.Load(source);
        var loaded = SmoLoadedResources.Get(document);
        Check(loaded.LoadIssue is null, loaded.LoadIssue ?? string.Empty);
        var originals = loaded.Models.Values.Where(model => model.Material is not null)
            .Select(model => model.Material!).DistinctBy(material => material.ObjectIndex)
            .OrderBy(material => material.ObjectIndex).ToArray();
        using var scene = new SparkplugSceneRuntime(document, new Dictionary<int, SmoSkin>());
        var runtime = scene.Materials;
        int animatedLayers = 0, uvLayers = 0, aliasLayers = 0;
        foreach (var original in originals)
        {
            var actual = runtime.ReadMaterial(original.ObjectIndex);
            Check(actual.RenderStates.SequenceEqual(original.RenderStates) && actual.Colors.SequenceEqual(original.Colors), "Initial material projection agrees with immutable snapshot");
            Check(actual.Passes.Count == original.Passes.Count && actual.ColorControllerId == original.ColorControllerId, "Initial pass and controller identities");
            for (int p = 0; p < actual.Passes.Count; ++p)
            {
                var pass = actual.Passes[p]; var expected = original.Passes[p];
                Check(pass.Blend == expected.Blend && pass.Layers.Count == expected.Layers.Count, "Initial pass shape");
                for (int l = 0; l < pass.Layers.Count; ++l)
                {
                    var layer = pass.Layers[l]; var wanted = expected.Layers[l];
                    Check(layer.Texture?.ObjectId == wanted.Texture?.ObjectId && layer.Animation?.ObjectId == wanted.Animation?.ObjectId && layer.UvControllerId == wanted.UvControllerId, "Initial canonical material links");
                    Check(layer.UvMatrix.SequenceEqual(wanted.UvMatrix) && layer.TextureStates.SequenceEqual(wanted.TextureStates), "Initial UV and texture states");
                    if (layer.Animation is not null) ++animatedLayers;
                    if (layer.UvControllerId != 0) ++uvLayers;
                    if (layer.Animation is not null && !layer.AnimationBoundHere || layer.UvControllerId != 0 && !layer.UvBoundHere) ++aliasLayers;
                }
            }
        }
        var controllers = originals.SelectMany(material => runtime.ReferencedControllers(material.ObjectIndex)).Distinct().ToArray();
        var before = controllers.ToDictionary(index => index, runtime.ReadClock);
        runtime.ApplyControllers(controllers, .125f);
        foreach (int index in controllers)
        {
            var clock = runtime.ReadClock(index);
            Check(clock.Accumulated == before[index].Accumulated + .125f && clock.Applied == before[index].Applied,
                "Actual Apply accumulates without evaluating a material");
        }
        int submitted = 0, changedUv = 0, colorEvaluations = 0;
        var colorEvents = new List<object>();
        foreach (var original in originals)
        {
            int? colorIndex = original.ColorControllerId == 0 ? null :
                document.Objects.Single(entry => entry.Id == original.ColorControllerId).Index;
            var pendingColor = colorIndex is int index ? runtime.ReadClock(index) : null;
            bool evaluated = runtime.UpdateColor(original.ObjectIndex, 5);
            Check(evaluated == (pendingColor is not null && pendingColor.Applied != pendingColor.Accumulated),
                "Color frame update consumes an actual pending controller, including shared aliases");
            if (evaluated) ++colorEvaluations;
            if (colorIndex is int controllerIndex)
            {
                var clock = runtime.ReadClock(controllerIndex);
                Check(clock.Applied == clock.Accumulated, "Actual color update consumes pending time");
                var material = runtime.ReadMaterial(original.ObjectIndex);
                colorEvents.Add(new { material = original.ObjectIndex, controller = original.ColorControllerId,
                    frame = 5u, force = false, evaluated, clock.Accumulated, clock.Applied,
                    colors = material.Colors.Select(color => new[] { color.X, color.Y, color.Z, color.W }).ToArray() });
            }
            for (uint p = 0; p < original.Passes.Count; ++p)
            {
                var result = runtime.UpdatePass(original.ObjectIndex, p);
                submitted += result.UVSubmissions.Count;
                var pass = result.Material.Passes[(int)p];
                foreach (var submission in result.UVSubmissions)
                {
                    Check(submission.Stage < pass.Layers.Count && submission.Matrix3x3.All(float.IsFinite), "Actual UV submission stage and finite matrix");
                    var layer = pass.Layers[(int)submission.Stage];
                    if (layer.UvBoundHere || layer.UvControllerId == 0)
                        Check(submission.Matrix3x3.SequenceEqual(layer.UvMatrix), "Backend receives the updated owning holder's matrix");
                }
                for (int l = 0; l < pass.Layers.Count; ++l)
                {
                    var layer = pass.Layers[l];
                    if (!layer.UvMatrix.SequenceEqual(original.Passes[(int)p].Layers[l].UvMatrix)) ++changedUv;
                }
            }
        }
        foreach (int index in controllers)
        {
            var clock = runtime.ReadClock(index);
            Check(clock.Applied == clock.Accumulated, "Referenced controllers consume pending time through actual pass updates");
        }
        // Exercise the real material frame gate through the managed consumer.
        // The controller's formulas remain exclusively in the common class.
        foreach (var original in originals.Where(material => material.ColorControllerId != 0))
        {
            int index = document.Objects.Single(entry => entry.Id == original.ColorControllerId).Index;
            runtime.ApplyControllers([index], .25f);
            var pending = runtime.ReadClock(index);
            var prior = runtime.ReadMaterial(original.ObjectIndex);
            Check(!runtime.UpdateColor(original.ObjectIndex, 5), "Same material frame defers newly accumulated color time");
            Check(runtime.ReadClock(index) == pending && runtime.ReadMaterial(original.ObjectIndex).Colors.SequenceEqual(prior.Colors),
                "Cached color frame preserves both pending time and output");
            Check(runtime.UpdateColor(original.ObjectIndex, 5, force: true), "Force bypasses the material frame cache");
            var clock = runtime.ReadClock(index);
            Check(clock.Applied == pending.Accumulated && clock.Accumulated == pending.Accumulated,
                "Forced color update consumes the existing pending input once");
            var material = runtime.ReadMaterial(original.ObjectIndex);
            colorEvents.Add(new { material = original.ObjectIndex, controller = original.ColorControllerId,
                frame = 5u, force = true, evaluated = true, clock.Accumulated, clock.Applied,
                colors = material.Colors.Select(color => new[] { color.X, color.Y, color.Z, color.W }).ToArray() });
            Check(!runtime.UpdateColor(original.ObjectIndex, 5, force: true) && !runtime.UpdateColor(original.ObjectIndex, 6),
                "Neither force nor a new frame invents pending color time");
        }
        // A render pass without new elapsed input must not re-evaluate UV.
        foreach (var original in originals.Where(material => material.Passes.Any(pass => pass.Layers.Any(layer => layer.UvControllerId != 0))))
        {
            var prior = runtime.ReadMaterial(original.ObjectIndex);
            for (uint p = 0; p < prior.Passes.Count; ++p) runtime.UpdatePass(original.ObjectIndex, p);
            var current = runtime.ReadMaterial(original.ObjectIndex);
            Check(current.Passes.SelectMany(pass => pass.Layers).SelectMany(layer => layer.UvMatrix)
                .SequenceEqual(prior.Passes.SelectMany(pass => pass.Layers).SelectMany(layer => layer.UvMatrix)), "Idle UV update preserves evaluated matrix");
        }
        var animationEvents = new List<object>();
        foreach (var animation in originals.SelectMany(material => material.Passes).SelectMany(pass => pass.Layers)
                     .Select(layer => layer.Animation).OfType<SmoLoadedTextureAnimation>().DistinctBy(value => value.ObjectId))
        {
            using var playbackScene = new SparkplugSceneRuntime(document, new Dictionary<int, SmoSkin>());
            var playbackRuntime = playbackScene.Materials;
            var references = originals.SelectMany(material => material.Passes.SelectMany((pass, p) => pass.Layers.Select((layer, l) =>
                (Material: material.ObjectIndex, Pass: (uint)p, Layer: l, Value: layer))))
                .Where(value => value.Value.Animation?.ObjectId == animation.ObjectId).ToArray();
            var target = references.Single(value => value.Value.AnimationBoundHere);
            var trigger = references[0];
            float firstEnd = animation.Keys[0].Time;
            foreach (float delta in new[] { 0f, firstEnd, MathF.BitIncrement(firstEnd) - firstEnd, -.25f, animation.Duration, animation.Duration * 2, 0f })
            {
                var previous = playbackRuntime.ReadClock(animation.ObjectIndex);
                playbackRuntime.ApplyControllers([animation.ObjectIndex], delta);
                var pending = playbackRuntime.ReadClock(animation.ObjectIndex);
                float next = previous.Playback!.Value + (pending.Accumulated - pending.Applied);
                double wrapped = next;
                while (wrapped > animation.Duration) wrapped -= animation.Duration;
                float expectedTime = (float)wrapped;
                var expectedKey = animation.Keys.Last();
                if (expectedTime < expectedKey.Time)
                    expectedKey = animation.Keys.First(key => key.Time > expectedTime);
                playbackRuntime.UpdatePass(trigger.Material, trigger.Pass);
                var clock = playbackRuntime.ReadClock(animation.ObjectIndex);
                var targetLayer = playbackRuntime.ReadMaterial(target.Material).Passes[(int)target.Pass].Layers[target.Layer];
                Check(clock.Playback == expectedTime && clock.Applied == pending.Accumulated, "Original strict-duration wrapping and float clock boundaries");
                Check(targetLayer.Texture?.ObjectId == expectedKey.Texture?.ObjectId, "Original endpoint interval selects actual texture on last-bound holder");
                animationEvents.Add(new { controller = animation.ObjectId, delta, clock.Accumulated, clock.Applied, clock.Playback, texture = targetLayer.Texture?.ObjectId });
            }
        }
        if (controllers.Length > 0)
        {
            var clock = runtime.ReadClock(controllers[0]);
            Reject(() => runtime.ApplyControllers([controllers[0], controllers[0]], 1));
            Reject(() => runtime.ApplyControllers([controllers[0]], float.NaN));
            Reject(() => runtime.ApplyControllers([controllers[0], -1], 1));
            Check(runtime.ReadClock(controllers[0]) == clock, "Rejected batches leave clocks unchanged");
        }
        Reject(() => runtime.ReadMaterial(-1));
        if (originals.Length > 0) Reject(() => runtime.UpdatePass(originals[0].ObjectIndex, uint.MaxValue));
        scene.Dispose();
        Reject(() => runtime.ApplyControllers([], 0));
        if (originals.Length > 0) Reject(() => runtime.ReadMaterial(originals[0].ObjectIndex));
        var report = new { status = "passed", source = Path.GetFullPath(source), source_sha256 = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(source))),
            native_dll_sha256 = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(Path.Combine(AppContext.BaseDirectory, "SparkplugViewerNative.dll")))),
            checks, materials = originals.Length, controllers = controllers.Length, animated_layers = animatedLayers, uv_layers = uvLayers,
            alias_layers = aliasLayers, uv_submissions = submitted, changed_uv_layers = changedUv,
            color_evaluations = colorEvaluations, color_events = colorEvents, animation_events = animationEvents };
        File.WriteAllText(Path.Combine(output, "report.json"), JsonSerializer.Serialize(report, new JsonSerializerOptions { WriteIndented = true }) + "\n");
        Console.WriteLine($"Material runtime: {originals.Length} materials, {controllers.Length} controllers, {submitted} UV submissions, {checks} checks");
        return 0;
    }
}
