namespace SmoViewer.Core;

/// <summary>
/// Resolves the transform node that owns a rigid (non-skinned) mesh. Animated
/// accessories can be rooted directly under an <c>spRenderNode</c>, while
/// ordinary rigid parts are commonly rooted under an <c>spNode</c> bone.
/// </summary>
public static class SmoRigidBindingResolver
{
    public static int? ResolveAnimationNodeObjectIndex(
        SmoDocument document,
        SmoObjectEntry meshEntry)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(meshEntry);

        var visited = new HashSet<int> { meshEntry.Index };
        SmoObjectEntry cursor = meshEntry;
        while (cursor.ParentIndex is int parentIndex &&
               (uint)parentIndex < (uint)document.Objects.Count &&
               visited.Add(parentIndex))
        {
            cursor = document.Objects[parentIndex];
            if (cursor.TypeHash is SmoClassIds.Node or SmoClassIds.RenderNode)
                return cursor.Index;
        }

        return null;
    }
}
