using System.Numerics;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text.Json;
using SmoViewer.Core;
using SmoViewer.Scene;

namespace SmoViewer.FormatTests;

internal static class SmoRenderOccurrenceRegression
{
    private static string Bits(Matrix4x4 value)
    {
        Span<byte> bytes = stackalloc byte[64];
        MemoryMarshal.Write(bytes, in value);
        return Convert.ToHexString(bytes);
    }
    private static string Sha(string path) => Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path)));
    internal static int Run(string path, string output)
    {
        int checks = 0;
        void Check(bool condition, string message)
        { if (!condition) throw new InvalidDataException(message); ++checks; }
        var document = SmoDocument.Load(path);
        var loaded = SmoLoadedResources.Get(document);
        Check(loaded.LoadIssue is null && loaded.SceneIssue is null, loaded.LoadIssue ?? loaded.SceneIssue ?? string.Empty);
        var scene = SmoSceneBuilder.Build(document);
        var expected = loaded.RenderOccurrences.Where(value => value.Issue is null
            && loaded.Models[value.RenderableObjectIndex].MeshObjectIndex is not null).ToArray();
        Check(scene.Meshes.Count == expected.Length, "Every actual Model/Skin support slot must become a scene occurrence");
        Check(scene.Meshes.All(value => value.OccurrenceKey.HasValue), "Each occurrence has an explicit host reference-slot identity");
        Check(scene.Meshes.Select(value => value.OccurrenceKey).Distinct().Count() == scene.Meshes.Count, "Duplicate Model pointers retain separate occurrence keys");
        var prepared = scene.Meshes.ToDictionary(value => value.OccurrenceKey!.Value);
        foreach (var occurrence in expected)
        {
            var mesh = prepared[occurrence.Key];
            var model = loaded.Models[occurrence.RenderableObjectIndex];
            Check(mesh.RenderableObjectIndex == occurrence.RenderableObjectIndex && mesh.SceneObjectIndex == occurrence.RenderableObjectIndex,
                "Scene selection points to the consuming renderable, not the physical mesh owner");
            Check(mesh.Mesh.ObjectIndex == model.MeshObjectIndex, "Occurrence uses the actual assigned mesh");
            Check(ReferenceEquals(mesh.LoadedMaterial, model.Material), "Shared mesh occurrence retains its own material");
            Check(Bits(mesh.WorldTransform) == Bits(occurrence.InputWorld!.Value), "Input world comes from actual native occurrence");
            Check(mesh.RigidNodeObjectIndex == occurrence.RigidNodeObjectIndex, "Rigid animation follows actual support node");
            bool skin = document.Objects[occurrence.RenderableObjectIndex].TypeHash == SmoClassIds.Skin;
            Check(mesh.SkinObjectIndex == (skin ? occurrence.RenderableObjectIndex : null), "Model cannot inherit another consumer's Skin");
            Check(ReferenceEquals(mesh.InitialSkinMatrices, model.InitialSkinPalette), "Actual initial palette is reused without recalculation");
        }
        foreach (var group in scene.Meshes.GroupBy(value => value.Mesh.ObjectIndex))
            Check(group.All(value => ReferenceEquals(value.Mesh, group.First().Mesh)), "Shared geometry is decoded once");
        foreach (var occurrence in loaded.RenderOccurrences.Where(value => value.Issue is not null))
            Check(scene.DecodeErrors.Contains(occurrence.Issue!), "Unsupported renderer remains an explicit diagnostic");
        Check(scene.Meshes.Select(value => value.RenderableObjectIndex!.Value).Distinct().Order()
            .SequenceEqual(loaded.Models.Values.Where(value => value.MeshObjectIndex.HasValue).Select(value => value.ObjectIndex).Order()),
            "Selected corpus has no missing Model/Skin consumers");
        var report = new
        {
            status = "passed", file = Path.GetFullPath(path), file_sha256 = Sha(path), checks,
            native_dll_sha256 = Sha(Path.Combine(AppContext.BaseDirectory, "SparkplugViewerNative.dll")),
            scene_dll_sha256 = Sha(typeof(SmoSceneBuilder).Assembly.Location),
            physical_meshes = scene.Meshes.Select(value => value.Mesh.ObjectIndex).Distinct().Count(),
            models = loaded.Models.Count, occurrences = scene.Meshes.Count,
            unsupported = loaded.RenderOccurrences.Where(value => value.Issue is not null).Select(value => new
            { container = document.Objects[value.Key.ContainerObjectIndex].Id, slot = value.Key.MemberSlot,
                renderable = document.Objects[value.RenderableObjectIndex].Id, issue = value.Issue }).ToArray(),
            repeated = scene.Meshes.GroupBy(value => value.RenderableObjectIndex).Where(group => group.Count() > 1)
                .Select(group => new { renderable = document.Objects[group.Key!.Value].Id, count = group.Count() }).ToArray(),
            placements = scene.Meshes.Select(value => new
            {
                container = document.Objects[value.OccurrenceKey!.Value.ContainerObjectIndex].Id,
                slot = value.OccurrenceKey.Value.MemberSlot, renderable = document.Objects[value.RenderableObjectIndex!.Value].Id,
                mesh = document.Objects[value.Mesh.ObjectIndex].Id,
                material = value.LoadedMaterial?.ObjectId ?? 0,
                skin = value.SkinObjectIndex is int skin ? document.Objects[skin].Id : 0,
                rigid_node = value.RigidNodeObjectIndex is int node ? document.Objects[node].Id : 0,
                world_hex = Bits(value.WorldTransform)
            }).ToArray(),
            scene_errors = scene.DecodeErrors, texture_issues = scene.TextureIssues
        };
        File.WriteAllText(output, JsonSerializer.Serialize(report, new JsonSerializerOptions { WriteIndented = true }) + "\n");
        Console.WriteLine($"Render occurrences: {scene.Meshes.Count} slots/{loaded.Models.Count} Model/Skin, {checks} checks");
        return 0;
    }
}
