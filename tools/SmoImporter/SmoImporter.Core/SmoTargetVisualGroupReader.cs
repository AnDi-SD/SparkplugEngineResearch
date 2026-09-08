using System.Buffers.Binary;
using SmoViewer.Core;

namespace SmoImporter.Core;

internal static partial class SmoSkinnedVisualGraphPipeline
{
    private const float WeightEpsilon = 0.000001f;

    /// <summary>
    /// The small part of the target visual graph required by the clean
    /// rebuilder. It deliberately contains neither serialized donor objects
    /// nor donor texture pixels: the target is only a native layout template.
    /// </summary>
    private sealed record TargetMeshSource(
        SmoObjectEntry Entry,
        SmoObjectEntry SkinEntry,
        SmoMesh Mesh,
        SmoSkin Skin);

    private sealed record TargetVisualGroup(
        int TextureObjectIndex,
        IReadOnlyList<TargetMeshSource> Meshes);

    private static TargetVisualGroup[] ReadTargetVisualGroups(
        SmoDocument document,
        IReadOnlyDictionary<int, SmoTextureBinding> bindings)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(bindings);

        var groups = new Dictionary<int, List<TargetMeshSource>>();
        foreach (SmoObjectEntry meshEntry in document.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.MeshData))
        {
            // The clean replacement discards every old visual resource. Only
            // explicit, writable native branches are layout candidates; rigid
            // attachments and material-less continuations are not templates.
            // Their material inheritance is irrelevant to the generated graph.
            if (meshEntry.ParentIndex is not int skinIndex ||
                document.Objects[skinIndex].TypeHash != SmoClassIds.Skin)
                continue;
            if (!bindings.TryGetValue(meshEntry.Index, out SmoTextureBinding? binding) ||
                binding.Issue is not null || binding.Texture is null)
                continue;

            SmoObjectEntry skinEntry = document.Objects[skinIndex];
            if (!SmoSkinDecoder.TryDecode(
                    document, skinEntry, out SmoSkin? skin, out string error) ||
                skin is null)
            {
                throw new InvalidOperationException(error);
            }

            if (skin.BaseMesh.TargetObjectIndex != meshEntry.Index ||
                skin.Renderable.Material?.TargetObjectIndex is not int materialIndex ||
                document.Objects[materialIndex].ParentIndex != skinIndex ||
                document.Objects[binding.Texture.ObjectIndex].ParentIndex != materialIndex)
                continue;

            SmoMesh mesh = SmoMeshDecoder.Decode(document, meshEntry);
            if (mesh.Marker != SmoMeshDecoder.E1Marker || !mesh.HasSkinningData)
                continue;

            int textureObjectIndex = binding.Texture.ObjectIndex;
            if (!groups.TryGetValue(
                    textureObjectIndex, out List<TargetMeshSource>? sources))
            {
                sources = [];
                groups.Add(textureObjectIndex, sources);
            }
            sources.Add(new TargetMeshSource(meshEntry, skinEntry, mesh, skin));
        }

        return groups
            .OrderBy(pair => pair.Key)
            .Select(pair => new TargetVisualGroup(
                pair.Key,
                pair.Value.OrderBy(source => source.Entry.Index).ToArray()))
            .ToArray();
    }

    private static SmoObjectEntry FindParentSkin(
        SmoDocument document,
        SmoObjectEntry mesh)
    {
        SmoObjectEntry cursor = mesh;
        while (cursor.ParentIndex is int parentIndex)
        {
            cursor = document.Objects[parentIndex];
            if (cursor.TypeHash == SmoClassIds.Skin)
                return cursor;
        }
        throw new InvalidOperationException(
            $"Mesh [{mesh.Index}] is not nested in an spSkin object.");
    }

    private static void WriteUInt16(Span<byte> data, int offset, ushort value) =>
        BinaryPrimitives.WriteUInt16LittleEndian(data[offset..], value);

    private static void WriteUInt32(Span<byte> data, int offset, uint value) =>
        BinaryPrimitives.WriteUInt32LittleEndian(data[offset..], value);
}
