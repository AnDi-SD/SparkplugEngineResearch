using System.Collections.ObjectModel;

namespace SmoViewer.Core;

/// <summary>Resolves the packed diffuse color of non-textured spMaterialData.</summary>
public static class SmoMaterialColorResolver
{
    public static IReadOnlyDictionary<int, uint> ResolveAll(SmoDocument document)
    {
        ArgumentNullException.ThrowIfNull(document);
        var catalog = SmoRenderableCatalog.Get(document);
        var result = new Dictionary<int, uint>();
        foreach (var mesh in document.Objects.Where(entry => entry.TypeHash == SmoClassIds.MeshData))
        {
            if (catalog.TryGetStoredMeshOwner(mesh, out var owner) &&
                owner.Renderable.Material?.TargetObjectIndex is int material &&
                TryDecodeDiffuse(document, document.Objects[material], out uint color))
                result[mesh.Index] = color;
        }

        return new ReadOnlyDictionary<int, uint>(result);
    }

    public static unsafe bool TryDecodeDiffuse(SmoDocument document, SmoObjectEntry material, out uint argb)
    {
        argb = 0;
        if (!SmoMaterialInspection.TryRead(document, material, out var snapshot, out _)) return false;
        var info = snapshot!.Info;
        if (info.HasColor == 0) return false;
        argb = info.Colors[1];
        return true;
    }
}
