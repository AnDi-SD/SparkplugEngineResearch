namespace SmoViewer.Core;

/// <summary>Uses the actual Model/Skin mesh assignment for stored resource previews.</summary>
public static class SmoActiveVisualResourceResolver
{
    public static bool IsMeshActive(SmoDocument document, SmoObjectEntry mesh)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(mesh);
        if (mesh.TypeHash != SmoClassIds.MeshData) return false;
        // Standalone mesh inspection has no Model/Skin occurrence to select.
        if (SmoRenderableCatalog.FindStoredRenderableIndex(document, mesh) is null) return true;
        return SmoRenderableCatalog.Get(document).TryGetStoredMeshOwner(mesh, out _);
    }
}
