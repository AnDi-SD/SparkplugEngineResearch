using System.Buffers.Binary;
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
/// Resolves the compact level-instancing convention where the final model
/// field is an eight-byte object reference instead of embedded mesh bytes.
/// </summary>
public static class SmoSharedMeshInstanceResolver
{
    public static IReadOnlyList<SmoSharedMeshInstanceInfo> ResolveAll(
        SmoDocument document)
    {
        ArgumentNullException.ThrowIfNull(document);

        Dictionary<uint, SmoObjectEntry> uniqueEntriesById = document.Objects
            .GroupBy(entry => entry.Id)
            .Where(group => group.Count() == 1)
            .ToDictionary(group => group.Key, group => group.Single());
        var result = new List<SmoSharedMeshInstanceInfo>();

        foreach (SmoObjectEntry model in document.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.Model))
        {
            if (document.Objects.Any(entry =>
                    entry.ParentIndex == model.Index &&
                    entry.TypeHash == SmoClassIds.MeshData) ||
                !TryDecodeReferencedMeshId(document, model, out uint meshId) ||
                !uniqueEntriesById.TryGetValue(meshId, out SmoObjectEntry? mesh) ||
                mesh.TypeHash != SmoClassIds.MeshData ||
                !TryFindStaticAncestor(document.Objects, model, out SmoObjectEntry? owner) ||
                owner is null)
            {
                continue;
            }

            SmoObjectEntry? material = document.Objects.FirstOrDefault(entry =>
                entry.ParentIndex == model.Index &&
                entry.TypeHash == SmoClassIds.MaterialData);
            result.Add(new SmoSharedMeshInstanceInfo(
                owner.Index,
                owner.Name,
                model.Index,
                model.Name,
                mesh.Index,
                mesh.Id,
                mesh.Name,
                material?.Index,
                SmoNodeTransformDecoder.ResolveModelWorldMatrix(document, model)));
        }

        return new ReadOnlyCollection<SmoSharedMeshInstanceInfo>(result);
    }

    private static bool TryDecodeReferencedMeshId(
        SmoDocument document,
        SmoObjectEntry model,
        out uint meshId)
    {
        meshId = 0;
        if (model.PhysicalOffset < 0 ||
            model.PhysicalOffset > int.MaxValue ||
            model.SerializedSize > int.MaxValue)
        {
            return false;
        }

        int physicalOffset = checked((int)model.PhysicalOffset);
        int serializedSize = checked((int)model.SerializedSize);
        if (physicalOffset > document.Data.Length - serializedSize ||
            serializedSize < 8)
        {
            return false;
        }

        ReadOnlySpan<byte> serialized = document.Data.Span.Slice(
            physicalOffset, serializedSize);
        if (BinaryPrimitives.ReadUInt32LittleEndian(serialized) != SmoClassIds.Model ||
            !serialized.Slice(4, 4).SequenceEqual("SBOO"u8))
        {
            return false;
        }

        uint candidate = 0;
        int candidateCount = 0;
        int offset = 8;
        while (offset < serialized.Length &&
               SmoDataBlockReader.TryReadHeader(
                   serialized, offset, out SmoDataBlockHeader field))
        {
            if (field.FieldType == 0 && field.PayloadSize == 8)
            {
                ReadOnlySpan<byte> payload = serialized.Slice(
                    field.PayloadOffset, checked((int)field.PayloadSize));
                uint referenceId = BinaryPrimitives.ReadUInt32LittleEndian(payload);
                uint trailing = BinaryPrimitives.ReadUInt32LittleEndian(payload[4..]);
                if (referenceId != 0 && trailing == 0)
                {
                    candidate = referenceId;
                    candidateCount++;
                }
            }
            offset = checked((int)field.PayloadEnd);
        }

        if (candidateCount != 1)
            return false;
        meshId = candidate;
        return true;
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
