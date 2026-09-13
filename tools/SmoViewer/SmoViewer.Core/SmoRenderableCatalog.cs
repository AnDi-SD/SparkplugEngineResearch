using System.Collections.ObjectModel;
using System.Diagnostics.CodeAnalysis;
using System.Runtime.CompilerServices;

namespace SmoViewer.Core;

/// <summary>Document metadata read by the original Model/Skin serializers.</summary>
public sealed record SmoRenderableResources(
    int ObjectIndex, SmoRenderableData Renderable, SmoNodeRelationship Mesh, SmoSkin? Skin);

/// <summary>
/// Indexes actual serialized resource links once per immutable document.
/// It does not load a runtime scene or select a fallback material.
/// </summary>
public sealed class SmoRenderableCatalog
{
    private static readonly ConditionalWeakTable<SmoDocument, SmoRenderableCatalog> Cache = new();
    private readonly SmoDocument document;
    private readonly IReadOnlyDictionary<int, IReadOnlyList<SmoRenderableResources>> consumers;
    public IReadOnlyDictionary<int, SmoRenderableResources> ByObjectIndex { get; }
    public IReadOnlyList<string> Issues { get; }

    private SmoRenderableCatalog(SmoDocument document)
    {
        this.document = document;
        var entries = new Dictionary<int, SmoRenderableResources>();
        var issues = new List<string>();
        foreach (var entry in document.Objects)
        {
            if (entry.TypeHash == SmoClassIds.Model)
            {
                if (SmoModelDecoder.TryDecode(document, entry, out var model, out var error))
                    entries.Add(entry.Index, new(entry.Index, model.Renderable, model.BaseMesh, null));
                else issues.Add($"MODEL_RESOURCE_LINKS: [{entry.Index}] {entry.Name}: {error}");
            }
            else if (entry.TypeHash == SmoClassIds.Skin)
            {
                if (SmoSkinDecoder.TryDecode(document, entry, out var skin, out var error))
                    entries.Add(entry.Index, new(entry.Index, skin.Renderable, skin.BaseMesh, skin));
                else issues.Add($"SKIN_RESOURCE_LINKS: [{entry.Index}] {entry.Name}: {error}");
            }
        }
        ByObjectIndex = new ReadOnlyDictionary<int, SmoRenderableResources>(entries);
        Issues = issues.AsReadOnly();
        consumers = new ReadOnlyDictionary<int, IReadOnlyList<SmoRenderableResources>>(entries.Values
            .GroupBy(item => item.Mesh.TargetObjectIndex!.Value)
            .ToDictionary(group => group.Key, group => (IReadOnlyList<SmoRenderableResources>)Array.AsReadOnly(group.ToArray())));
    }

    public static SmoRenderableCatalog Get(SmoDocument document)
    {
        ArgumentNullException.ThrowIfNull(document);
        return Cache.GetValue(document, static value => new SmoRenderableCatalog(value));
    }

    public IReadOnlyList<SmoRenderableResources> GetMeshConsumers(int meshObjectIndex) =>
        consumers.TryGetValue(meshObjectIndex, out var values) ? values : Array.Empty<SmoRenderableResources>();

    /// <summary>
    /// Compatibility view for a physically stored MeshData. Its containing
    /// Model/Skin must actually reference it; sibling materials are irrelevant.
    /// Runtime occurrences are indexed by their Model/Skin ObjectIndex above.
    /// </summary>
    public bool TryGetStoredMeshOwner(SmoObjectEntry mesh,
        [NotNullWhen(true)] out SmoRenderableResources? value)
    {
        value = null;
        if ((uint)mesh.Index >= (uint)document.Objects.Count || !ReferenceEquals(document.Objects[mesh.Index], mesh)) return false;
        return FindStoredRenderableIndex(document, mesh) is int index &&
            ByObjectIndex.TryGetValue(index, out value) && value.Mesh.TargetObjectIndex == mesh.Index;
    }

    internal static int? FindStoredRenderableIndex(SmoDocument document, SmoObjectEntry entry)
    {
        int? cursor = entry.ParentIndex;
        var visited = new HashSet<int>();
        while (cursor is int index && (uint)index < (uint)document.Objects.Count && visited.Add(index))
        {
            var candidate = document.Objects[index];
            if (candidate.TypeHash is SmoClassIds.Model or SmoClassIds.Skin) return index;
            cursor = candidate.ParentIndex;
        }
        return null;
    }
}
