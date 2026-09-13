using System.Collections.ObjectModel;
using System.Numerics;
using System.Runtime.CompilerServices;

namespace SmoViewer.Core;

public sealed record SmoTextureBinding(
    SmoTexture? Texture,
    string? Issue,
    IReadOnlyList<SmoTexture>? AnimationFrames = null,
    TimeSpan? FrameDuration = null,
    SmoTexture? BaseTexture = null,
    bool UsesAlphaBlend = false,
    uint? DiffuseArgb = null,
    SmoMaterialRenderStateInfo? MaterialRenderState = null)
{
    public SmoLoadedMaterial? LoadedMaterial { get; init; }
    public IReadOnlyList<string> Diagnostics { get; init; } = Array.Empty<string>();
}

/// <summary>
/// Upload bindings from actual loaded Model/Skin -> Material -> Pass -> Layer
/// objects. ResolveAll is a compatibility view for physically stored meshes;
/// scene instances must use ResolveByRenderable. The complete material remains
/// available even where the current single-layer frontend cannot render it.
/// </summary>
public static class SmoTextureBindingResolver
{
    private sealed record Snapshot(IReadOnlyDictionary<int, SmoTextureBinding> ByRenderable,
        IReadOnlyDictionary<int, SmoTextureBinding> ByStoredMesh);
    private static readonly ConditionalWeakTable<SmoDocument, Lazy<Snapshot>> Cache = new();

    public static IReadOnlyDictionary<int, SmoTextureBinding> ResolveAll(SmoDocument document) => Get(document).ByStoredMesh;
    public static IReadOnlyDictionary<int, SmoTextureBinding> ResolveByRenderable(SmoDocument document) => Get(document).ByRenderable;

    private static Snapshot Get(SmoDocument document)
    {
        ArgumentNullException.ThrowIfNull(document);
        return Cache.GetValue(document, static value => new Lazy<Snapshot>(() => Build(value))).Value;
    }

    private static Snapshot Build(SmoDocument document)
    {
        var loaded = SmoLoadedResources.Get(document);
        var byModel = new Dictionary<int, SmoTextureBinding>();
        var byMesh = new Dictionary<int, SmoTextureBinding>();
        foreach (var model in loaded.Models.Values)
            byModel.Add(model.ObjectIndex, Bind(model));
        var metadata = SmoRenderableCatalog.Get(document);
        foreach (var mesh in document.Objects.Where(entry => entry.TypeHash == SmoClassIds.MeshData))
        {
            if (loaded.LoadIssue is not null)
                byMesh.Add(mesh.Index, new(null, loaded.LoadIssue));
            else if (metadata.TryGetStoredMeshOwner(mesh, out var owner) &&
                loaded.Models.TryGetValue(owner.ObjectIndex, out var model) && model.MeshObjectIndex == mesh.Index &&
                byModel.TryGetValue(owner.ObjectIndex, out var binding))
                byMesh.Add(mesh.Index, binding);
        }
        return new(new ReadOnlyDictionary<int, SmoTextureBinding>(byModel),
            new ReadOnlyDictionary<int, SmoTextureBinding>(byMesh));
    }

    private static SmoTextureBinding Bind(SmoLoadedModel model)
    {
        if (model.Issue is not null) return new(null, model.Issue);
        if (model.MaterialId == 0)
            return new(null, $"NULL_MATERIAL_RENDER_CONTEXT: Model [{model.ObjectIndex}] has no material; renderer default/current state is required.");
        if (model.Material is not SmoLoadedMaterial material)
            return new(null, $"MISSING_LOADED_MATERIAL: Model [{model.ObjectIndex}], material ID {model.MaterialId}.");
        SmoMaterialRenderStateInfo? state = material.Passes.Count == 0 ? null
            : SmoMaterialRenderState.Classify(material.Passes[0].Blend, material.RenderStates);
        uint? diffuse = PackPreviewColor(material.Colors[1]);
        SmoTextureBinding Result(SmoTexture? texture, string? issue, IReadOnlyList<string>? diagnostics = null) =>
            new(texture, issue, UsesAlphaBlend: state?.UsesAlphaBlend == true, DiffuseArgb: diffuse, MaterialRenderState: state)
            { LoadedMaterial = material, Diagnostics = diagnostics ?? Array.Empty<string>() };
        if (material.Passes.Count != 1 || material.Passes[0].Layers.Count != 1)
            return Result(null, $"MATERIAL_FRONTEND_SHAPE: Material [{material.ObjectIndex}] retains all {material.Passes.Count} passes; the current texture slot requires exactly one pass and one layer.");
        var layer = material.Passes[0].Layers[0];
        var diagnostics = new List<string>();
        if (layer.Animation is not null)
            diagnostics.Add($"TEXTURE_CONTROLLER_FRONTEND_PENDING: Material [{material.ObjectIndex}] uses controller [{layer.Animation.ObjectIndex}] with {layer.Animation.Keys.Count} actual end-time keys; a uniform frame timer is not used.");
        if (layer.UvEnabled || layer.UvControllerId != 0)
            diagnostics.Add($"MATERIAL_UV_FRONTEND_PENDING: Material [{material.ObjectIndex}] retains its actual UV matrix/controller for renderer integration.");
        if (diffuse is null)
            diagnostics.Add($"MATERIAL_COLOR_UPLOAD: Material [{material.ObjectIndex}] has a non-finite diffuse component; raw float colors remain available.");
        return Result(layer.Texture?.Texture, layer.Texture?.Issue, diagnostics.AsReadOnly());
    }

    // Host RGBA8 upload conversion only. Raw engine float colors remain in the
    // loaded material; zero/black is a valid value, never a missing-color marker.
    private static uint? PackPreviewColor(Vector4 value)
    {
        if (!float.IsFinite(value.X) || !float.IsFinite(value.Y) || !float.IsFinite(value.Z) || !float.IsFinite(value.W)) return null;
        static uint Byte(float channel) => (uint)MathF.Round(Math.Clamp(channel, 0, 1) * 255);
        return Byte(value.W) << 24 | Byte(value.X) << 16 | Byte(value.Y) << 8 | Byte(value.Z);
    }
}
