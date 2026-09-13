using System.Numerics;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text.Json;
using SmoViewer.Core;
using SmoViewer.Scene;
using SmoViewer.Sparkplug;

namespace SmoViewer.FormatTests;

internal static class SmoLoadedSceneRegression
{
    private static string Bits(Matrix4x4 value)
    {
        Span<byte> bytes = stackalloc byte[64];
        MemoryMarshal.Write(bytes, in value);
        return Convert.ToHexString(bytes);
    }
    private static string Sha(string path) => Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path)));

    internal static int Run(string path, string output, string? sanPath)
    {
        int checks = 0;
        void Check(bool condition, string message)
        { if (!condition) throw new InvalidDataException(message); ++checks; }
        var document = SmoDocument.Load(path);
        var loaded = SmoLoadedResources.Get(document);
        Check(loaded.LoadIssue is null, loaded.LoadIssue ?? string.Empty);
        Check(loaded.SceneIssue is null, loaded.SceneIssue ?? string.Empty);
        var metadata = SmoRenderableCatalog.Get(document);
        var skins = metadata.ByObjectIndex.Values.Where(value => value.Skin is not null)
            .ToDictionary(value => value.ObjectIndex, value => value.Skin!);
        using var runtime = new SparkplugSceneRuntime(document, skins);
        Check(runtime.Worlds.Count == loaded.Nodes.Count, "Live runtime retains every actual derived Node");
        foreach (var node in loaded.Nodes.Values)
        {
            Check(Bits(node.World) == Bits(runtime.Worlds[node.ObjectIndex]), "Snapshot/live loaded Node world bits differ");
            Check(SmoNodeTransformDecoder.TryResolveNodeWorldMatrix(document, document.Objects[node.ObjectIndex], out var world)
                && Bits(world) == Bits(node.World), "Node transform view uses actual loaded world");
            Check(node.ParentObjectIndex is not int parent || loaded.Nodes.ContainsKey(parent), "Loaded node parent must be present");
        }
        foreach (var skin in skins.Values)
        {
            var actual = loaded.Models[skin.ObjectIndex];
            Check(actual.SkinIssue is null, actual.SkinIssue ?? string.Empty);
            Check(actual.SkinWeightCount == skin.BlendInfluenceCountHint, "Loaded Skin weight count");
            Check(actual.InitialSkinPalette is not null && actual.InitialSkinPalette.Count == skin.Bones.Count, "Loaded Skin bone count");
            Check(actual.InitialSkinPalette!.Select(Bits).SequenceEqual(runtime.Palette(skin.ObjectIndex).Select(Bits)), "Actual Skin palettes survive graph handle disposal");
        }
        var prepared = SmoSceneBuilder.Build(document);
        foreach (var mesh in prepared.Meshes.Where(mesh => mesh.SkinObjectIndex.HasValue))
        {
            int index = mesh.SkinObjectIndex!.Value;
            Check(mesh.InitialSkinMatrices is not null && mesh.InitialSkinMatrices.Select(Bits).SequenceEqual(runtime.Palette(index).Select(Bits)),
                "Prepared initial skin palette is actual file pose, without inverse-bind pose inference");
        }
        var frames = new List<object>();
        IReadOnlyList<string> warnings = Array.Empty<string>();
        string? firstWorld = null, firstPalette = null;
        if (sanPath is not null)
        {
            using var clip = SparkplugAnimationClip.Load(sanPath);
            warnings = runtime.Bind(clip);
            foreach (float time in new[] { 0f, clip.Duration * .37f, clip.Duration, clip.Duration * .11f, 0f })
            {
                runtime.Sample(time);
                var worlds = loaded.Nodes.Values.Select(node => new { id = node.ObjectId, world_hex = Bits(runtime.Worlds[node.ObjectIndex]) }).ToArray();
                var palettes = skins.Keys.Select(index => new { id = document.Objects[index].Id, matrices = runtime.Palette(index).Select(Bits).ToArray() }).ToArray();
                string worldSignature = string.Concat(worlds.Select(value => value.world_hex));
                string paletteSignature = string.Concat(palettes.SelectMany(value => value.matrices));
                if (time == 0)
                {
                    if (firstWorld is null) { firstWorld = worldSignature; firstPalette = paletteSignature; }
                    else Check(firstWorld == worldSignature && firstPalette == paletteSignature, "Backward seek restores exact loaded Node/Skin results");
                }
                frames.Add(new { time, worlds, palettes });
            }
        }
        var members = loaded.RenderContainers.SelectMany(container => container.RenderableObjectIndices).ToArray();
        var report = new
        {
            status = "passed", file = Path.GetFullPath(path), file_sha256 = Sha(path), checks,
            native_dll_sha256 = Sha(Path.Combine(AppContext.BaseDirectory, "SparkplugViewerNative.dll")),
            core_dll_sha256 = Sha(typeof(SmoDocument).Assembly.Location),
            nodes = loaded.Nodes.Values.Select(node => new { id = node.ObjectId, index = node.ObjectIndex,
                class_id = document.Objects[node.ObjectIndex].TypeHash, parent = node.ParentObjectIndex is int parent ? document.Objects[parent].Id : 0,
                world_hex = Bits(node.World) }).ToArray(),
            skins = skins.Values.Select(skin => new { id = document.Objects[skin.ObjectIndex].Id,
                weights = loaded.Models[skin.ObjectIndex].SkinWeightCount,
                bones = skin.Bones.Select(bone => new { id = bone.NodeObjectId, inverse_hex = Bits(bone.InverseBindMatrix) }).ToArray(),
                matrices = loaded.Models[skin.ObjectIndex].InitialSkinPalette!.Select(Bits).ToArray() }).ToArray(),
            containers = loaded.RenderContainers.Select(container => new { id = container.ObjectId, index = container.ObjectIndex,
                kind = container.Kind.ToString(), world_hex = container.World is Matrix4x4 world ? Bits(world) : null,
                inverse_hex = container.Inverse is Matrix4x4 inverse ? Bits(inverse) : null,
                members = container.RenderableObjectIndices.Select(index => document.Objects[index].Id).ToArray() }).ToArray(),
            member_count = members.Length, unique_members = members.Distinct().Count(),
            model_count = loaded.Models.Count,
            models_without_support = loaded.Models.Keys.Except(members).Select(index => document.Objects[index].Id).ToArray(),
            multiply_attached = members.GroupBy(index => index).Where(group => group.Count() > 1)
                .Select(group => new { id = document.Objects[group.Key].Id, count = group.Count() }).ToArray(),
            scene_errors = prepared.DecodeErrors,
            san = sanPath is null ? null : new { path = Path.GetFullPath(sanPath), sha256 = Sha(sanPath) }, warnings, frames
        };
        File.WriteAllText(output, JsonSerializer.Serialize(report, new JsonSerializerOptions { WriteIndented = true }) + "\n");
        Console.WriteLine($"Loaded scene: {loaded.Nodes.Count} Nodes, {skins.Count} Skins, {loaded.RenderContainers.Count} supports/{members.Length} members, {checks} checks");
        return 0;
    }
}
