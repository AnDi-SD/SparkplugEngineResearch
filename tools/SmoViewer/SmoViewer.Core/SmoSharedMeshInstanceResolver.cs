using System.Collections.ObjectModel;
using System.Numerics;

namespace SmoViewer.Core;

/// <summary>
/// A reference-only spModel which reuses one physical spMeshData object while
/// its surrounding spStaticRenderObject supplies a distinct level placement.
/// </summary>
public sealed record SmoSharedMeshInstanceInfo(
    int StaticObjectIndex,
    string StaticObjectName,
    int ModelObjectIndex,
    string ModelObjectName,
    int SourceMeshObjectIndex,
    uint SourceMeshObjectId,
    string SourceMeshName,
    int? MaterialObjectIndex,
    Matrix4x4 WorldTransform);

/// <summary>
/// Resolves the original Model mesh assignment when it references an existing
/// resource. Material identity comes from that Model's Renderable section.
/// </summary>
public static class SmoSharedMeshInstanceResolver
{
    public static IReadOnlyList<SmoSharedMeshInstanceInfo> ResolveAll(
        SmoDocument document)
    {
        ArgumentNullException.ThrowIfNull(document);

        var result = new List<SmoSharedMeshInstanceInfo>();
        foreach (var resources in SmoRenderableCatalog.Get(document).ByObjectIndex.Values)
        {
            var model = document.Objects[resources.ObjectIndex];
            if (model.TypeHash != SmoClassIds.Model ||
                resources.Mesh.Encoding == SmoNodeRelationshipEncoding.InlineObject ||
                resources.Mesh.TargetObjectIndex is not int meshIndex ||
                !TryFindStaticAncestor(document.Objects, model, out var owner) || owner is null)
                continue;
            var mesh = document.Objects[meshIndex];
            result.Add(new SmoSharedMeshInstanceInfo(owner.Index, owner.Name, model.Index, model.Name,
                mesh.Index, mesh.Id, mesh.Name, resources.Renderable.Material?.TargetObjectIndex,
                SmoNodeTransformDecoder.ResolveModelWorldMatrix(document, model)));
        }

        return new ReadOnlyCollection<SmoSharedMeshInstanceInfo>(result);
    }

    private static bool TryFindStaticAncestor(
        IReadOnlyList<SmoObjectEntry> objects,
        SmoObjectEntry entry,
        out SmoObjectEntry? owner)
    {
        owner = null;
        int? cursor = entry.ParentIndex;
        var visited = new HashSet<int>();
        while (cursor is int index &&
               (uint)index < (uint)objects.Count &&
               visited.Add(index))
        {
            SmoObjectEntry candidate = objects[index];
            if (candidate.TypeHash == SmoClassIds.StaticRenderObject)
            {
                owner = candidate;
                return true;
            }
            cursor = candidate.ParentIndex;
        }
        return false;
    }
}
