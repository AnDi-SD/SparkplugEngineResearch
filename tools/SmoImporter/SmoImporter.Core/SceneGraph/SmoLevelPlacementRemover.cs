using System.Buffers.Binary;
using SmoViewer.Core;

namespace SmoImporter.Core;

public sealed record SmoLevelPlacementRemovalResult(
    byte[] Data,
    int RemovedObjectCount,
    IReadOnlyList<uint> RelocatedResourceIds);

/// <summary>Removes one static visual branch while preserving shared leaf resources.</summary>
public static class SmoLevelPlacementRemover
{
    public static SmoLevelPlacementRemovalResult Remove(
        SmoDocument document,
        int sceneObjectIndex)
    {
        ArgumentNullException.ThrowIfNull(document);
        if ((uint)sceneObjectIndex >= (uint)document.Objects.Count)
            throw new ArgumentOutOfRangeException(nameof(sceneObjectIndex));
        SmoObjectEntry sceneObject = document.Objects[sceneObjectIndex];
        int? staticIndex = SmoPlacementTransformWriter.FindStaticPlacementOwnerIndex(
            document,
            sceneObjectIndex);
        SmoObjectEntry root = staticIndex is int placementOwnerIndex
            ? document.Objects[placementOwnerIndex]
            : FindAncestor(document, sceneObject, SmoClassIds.Model)
              ?? (sceneObject.TypeHash is SmoClassIds.Node or SmoClassIds.RenderNode
                  ? sceneObject
                  : null)
              ?? throw new InvalidOperationException(
                  "The visual object has neither a static placement, an owning " +
                  "spModel branch nor a removable spNode/spRenderNode root.");
        if (root.ParentIndex is not int parentIndex)
            throw new InvalidOperationException("The visual placement has no inline parent branch.");
        uint rootId = root.Id;
        uint parentId = document.Objects[parentIndex].Id;
        HashSet<uint> branchIds = document.Objects.Where(entry =>
                entry.PhysicalOffset >= root.PhysicalOffset &&
                entry.PhysicalEnd <= root.PhysicalEnd)
            .Select(entry => entry.Id)
            .ToHashSet();
        byte[] output = document.Data.ToArray();
        var relocated = new List<uint>();

        foreach (SmoObjectEntry resource in document.Objects
                     .Where(entry => branchIds.Contains(entry.Id) &&
                         entry.TypeHash is SmoClassIds.TextureData or SmoClassIds.MeshData)
                     .OrderBy(entry => entry.TypeHash == SmoClassIds.TextureData ? 0 : 1))
        {
            if (resource.ParentIndex is not int ownerIndex ||
                document.Objects.Any(entry => entry.ParentIndex == resource.Index))
                continue;
            byte fieldType = resource.TypeHash == SmoClassIds.TextureData ? (byte)10 : (byte)0;
            SmoObjectEntry? consumer = document.Objects.FirstOrDefault(entry =>
                !branchIds.Contains(entry.Id) &&
                HasReferenceOnly(document, entry, fieldType, resource.Id));
            if (consumer is null)
                continue;

            byte[] objectBytes = document.Data.Span.Slice(
                checked((int)resource.PhysicalOffset),
                checked((int)resource.SerializedSize)).ToArray();
            byte[] rawName = resource.RawName.ToArray();
            uint ownerId = document.Objects[ownerIndex].Id;
            SmoDocument current = SmoDocument.Parse(output, document.SourcePath);
            output = SmoVisualForestInjector.DemoteInlineLeafToReference(
                current,
                ownerId,
                fieldType,
                resource.Id);
            current = SmoDocument.Parse(output, document.SourcePath);
            output = SmoVisualForestInjector.PromoteReferenceToInline(
                current,
                consumer.Id,
                fieldType,
                resource.Id,
                resource.Id,
                rawName,
                resource.TypeHash,
                objectBytes);
            relocated.Add(resource.Id);
        }

        SmoDocument beforeRemoval = SmoDocument.Parse(output, document.SourcePath);
        output = SmoVisualForestInjector.RemoveInlineBranch(
            beforeRemoval,
            parentId,
            rootId);
        SmoDocument verified = SmoDocument.Parse(output, document.SourcePath);
        if (verified.HasErrors || verified.Objects.Any(entry => entry.Id == rootId))
            throw new InvalidDataException("The removed visual branch failed structural verification.");
        foreach (uint resourceId in relocated)
        {
            if (!verified.Objects.Any(entry => entry.Id == resourceId))
                throw new InvalidDataException($"Shared resource {resourceId} was lost while deleting a placement.");
        }
        return new(
            output,
            document.Objects.Count - verified.Objects.Count,
            relocated);
    }

    private static SmoObjectEntry? FindAncestor(
        SmoDocument document,
        SmoObjectEntry entry,
        uint typeHash)
    {
        SmoObjectEntry? cursor = entry;
        var visited = new HashSet<int>();
        while (cursor is not null && visited.Add(cursor.Index))
        {
            if (cursor.TypeHash == typeHash)
                return cursor;
            cursor = cursor.ParentIndex is int parentIndex &&
                     (uint)parentIndex < (uint)document.Objects.Count
                ? document.Objects[parentIndex]
                : null;
        }
        return null;
    }

    private static bool HasReferenceOnly(
        SmoDocument document,
        SmoObjectEntry owner,
        byte fieldType,
        uint objectId)
    {
        ReadOnlySpan<byte> data = document.Data.Span.Slice(
            checked((int)owner.PhysicalOffset),
            checked((int)owner.SerializedSize));
        int offset = 8;
        while (offset < data.Length &&
               SmoDataBlockReader.TryReadHeader(data, offset, out SmoDataBlockHeader field))
        {
            if (field.FieldType == fieldType && field.PayloadSize == 8 &&
                BinaryPrimitives.ReadUInt32LittleEndian(data[field.PayloadOffset..]) == objectId &&
                BinaryPrimitives.ReadUInt32LittleEndian(data[(field.PayloadOffset + 4)..]) == 0)
                return true;
            offset = checked((int)field.PayloadEnd);
        }
        return false;
    }
}
