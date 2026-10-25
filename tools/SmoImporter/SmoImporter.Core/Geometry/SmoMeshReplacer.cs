using SmoViewer.Core;

namespace SmoImporter.Core;

/// <summary>Bone slots for a stored mesh assigned to its actual containing Skin.</summary>
public static class SmoMeshReplacer
{
    public static IReadOnlyList<SmoBoneSlot> GetBoneSlots(
        SmoDocument document, SmoObjectEntry meshEntry)
    {
        if (!SmoRenderableCatalog.Get(document).TryGetStoredMeshOwner(meshEntry, out var resources) ||
            resources.Skin is not SmoSkin skin)
            return [];
        IReadOnlyDictionary<int, SmoObjectEntry> entries = document.Objects.ToDictionary(item => item.Index);
        return skin.Bones.Select(bone => new SmoBoneSlot(
            bone.PaletteIndex,
            bone.NodeObjectId,
            entries.TryGetValue(bone.NodeObjectIndex, out SmoObjectEntry? node)
                ? node.Name : $"node_{bone.NodeObjectId}")).ToArray();
    }
}
