using SmoViewer.Core;

namespace SmoImporter.Core;

/// <summary>Bone-slot metadata used by the shared-writer replacement path.</summary>
public static class SmoMeshReplacer
{
    public static IReadOnlyList<SmoBoneSlot> GetBoneSlots(
        SmoDocument document, SmoObjectEntry meshEntry)
    {
        SmoObjectEntry? skinEntry = FindAncestor(document, meshEntry, SmoClassIds.Skin);
        if (skinEntry is null || !SmoSkinDecoder.TryDecode(
                document, skinEntry, out SmoSkin? skin, out _) || skin is null)
            return [];
        IReadOnlyDictionary<int, SmoObjectEntry> entries = document.Objects.ToDictionary(item => item.Index);
        return skin.Bones.Select(bone => new SmoBoneSlot(
            bone.PaletteIndex,
            bone.NodeObjectId,
            entries.TryGetValue(bone.NodeObjectIndex, out SmoObjectEntry? node)
                ? node.Name : $"node_{bone.NodeObjectId}")).ToArray();
    }

    private static SmoObjectEntry? FindAncestor(
        SmoDocument document, SmoObjectEntry entry, uint classId)
    {
        IReadOnlyDictionary<int, SmoObjectEntry> entries = document.Objects.ToDictionary(item => item.Index);
        SmoObjectEntry? cursor = entry;
        while (cursor.ParentIndex is int parent && entries.TryGetValue(parent, out cursor))
            if (cursor.TypeHash == classId) return cursor;
        return null;
    }
}
