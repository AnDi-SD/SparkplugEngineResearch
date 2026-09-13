using System.Diagnostics;
using System.Security.Cryptography;
using System.Text.Json;
using SmoViewer.Core;
using SmoViewer.Scene;

namespace SmoViewer.FormatTests;

/// <summary>Selected real-file checks: loaded object identity and scene consumers.</summary>
internal static class SmoLoadedResourceRegression
{
    internal static int Run(string path, string output)
    {
        int assertions = 0;
        void Check(bool condition, string message)
        { if (!condition) throw new InvalidDataException(message); ++assertions; }
        var document = SmoDocument.Load(path);
        var watch = Stopwatch.StartNew();
        var loaded = SmoLoadedResources.Get(document);
        watch.Stop();
        Check(loaded.LoadIssue is null, loaded.LoadIssue ?? string.Empty);
        Check(ReferenceEquals(loaded, SmoLoadedResources.Get(document)), "One immutable graph snapshot per document");
        var metadata = SmoRenderableCatalog.Get(document);
        Check(loaded.Models.Count == metadata.ByObjectIndex.Count, "Loaded/inspected renderable counts differ");
        var bindings = SmoTextureBindingResolver.ResolveByRenderable(document);
        Check(bindings.Count == loaded.Models.Count, "Each actual Model/Skin has its own binding");
        var materialIds = new HashSet<uint>();
        var textures = new Dictionary<uint, SmoLoadedTexture>();
        int materialViews = 0;
        foreach (var model in loaded.Models.Values)
        {
            Check(model.Issue is null, model.Issue ?? string.Empty);
            var source = metadata.ByObjectIndex[model.ObjectIndex];
            Check(model.MeshId == source.Mesh.ObjectId, $"Mesh reference [{model.ObjectIndex}]");
            Check(model.MaterialId == (source.Renderable.Material?.ObjectId ?? 0), $"Material reference [{model.ObjectIndex}]");
            var binding = bindings[model.ObjectIndex];
            Check(ReferenceEquals(binding.LoadedMaterial, model.Material), "Binding retains its actual material snapshot");
            Check(binding.AnimationFrames is null && binding.FrameDuration is null, "Uniform frame inference must be absent");
            if (model.Material is not SmoLoadedMaterial material || !materialIds.Add(model.MaterialId)) continue;
            if (SmoMaterialDataDecoder.TryDecode(document, document.Objects[material.ObjectIndex], out var inspected, out _))
            {
                ++materialViews;
                Check(material.RenderStates.SequenceEqual(inspected!.RenderStates), "Material states preserved");
                Check((material.VertexAlpha != 0) == inspected.UsesVertexAlpha, "Material alpha preserved");
                Check(material.Passes.Count == inspected.Passes.Count, "All material passes retained");
                Check(material.SpecularPower == inspected.Color.SpecularPower, "Specular power preserved");
                foreach (var pass in material.Passes.Select((value, index) => (value, index)))
                {
                    Check(pass.value.Blend == inspected.Passes[pass.index].FinalBlendOperation, "Pass blend preserved");
                    Check(pass.value.Layers[0].TextureStates.Take(9).SequenceEqual(inspected.Passes[pass.index].TextureStates), "Layer states preserved");
                }
            }
            foreach (var layer in material.Passes.SelectMany(pass => pass.Layers))
            {
                if (layer.Texture is not null) textures.TryAdd(layer.Texture.ObjectId, layer.Texture);
                if (layer.Animation is not null)
                    foreach (var key in layer.Animation.Keys)
                        if (key.Texture is not null) textures.TryAdd(key.Texture.ObjectId, key.Texture);
            }
        }
        var scene = SmoSceneBuilder.Build(document);
        int instances = 0;
        foreach (var mesh in scene.Meshes)
        {
            if (mesh.RenderableObjectIndex is not int index) continue;
            Check(ReferenceEquals(mesh.LoadedMaterial, loaded.Models[index].Material), $"Occurrence [{index}] material identity");
            Check(mesh.MaterialColorArgb == bindings[index].DiffuseArgb, $"Occurrence [{index}] color including zero");
            if (mesh.SharedInstance is not null)
            {
                ++instances;
                Check(mesh.SkinObjectIndex is null && mesh.InitialSkinMatrices is null, "Model instance does not inherit source Skin palette");
                Check(ReferenceEquals(mesh.Texture, bindings[index].Issue is null && mesh.Mesh.HasTextureCoordinates ? bindings[index].Texture : null),
                    $"Instance [{index}] texture belongs to its own material");
            }
        }
        int distinctMaterialGroups = loaded.Models.Values.Where(model => model.MeshId != 0).GroupBy(model => model.MeshId)
            .Count(group => group.Select(model => model.MaterialId).Distinct().Count() > 1);
        var report = new
        {
            status = "passed", file = Path.GetFullPath(path), file_sha256 = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path))),
            objects = document.Objects.Count, models = loaded.Models.Count, materials = materialIds.Count,
            material_inspection_comparisons = materialViews, distinct_material_shared_mesh_groups = distinctMaterialGroups,
            scene_meshes = scene.Meshes.Count, shared_instances = instances, snapshot_ms = watch.Elapsed.TotalMilliseconds,
            assertions, scene_errors = scene.DecodeErrors, frontend_issues = scene.TextureIssues,
            textures = textures.Values.OrderBy(value => value.ObjectId).Select(value => new
            {
                id = value.ObjectId, object_index = value.ObjectIndex, issue = value.Issue,
                width = value.Texture?.Width, height = value.Texture?.Height,
                sha256 = value.Texture is null ? null : Convert.ToHexString(SHA256.HashData(value.Texture.Bgra32Pixels.Span))
            }).ToArray()
        };
        File.WriteAllText(output, JsonSerializer.Serialize(report, new JsonSerializerOptions { WriteIndented = true }) + "\n");
        Console.WriteLine($"Loaded resources: {loaded.Models.Count} renderables, {instances} instances, {assertions} assertions; {watch.Elapsed.TotalMilliseconds:F2} ms");
        return 0;
    }
}
