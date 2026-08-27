using System.Buffers.Binary;
using SmoViewer.Core;

namespace SmoImporter.Core;

public sealed record SmoCollisionBranchRemovalResult(
    byte[] Data,
    int RemovedObjectCount);

/// <summary>
/// Removes one native spCollisionInfo branch together with its collision
/// registry bindings and inline spMeshBV descendants.
/// </summary>
public static class SmoCollisionBranchRemover
{
    private const int ObjectSignatureSize = 8;
    private const int ObjectReferenceSize = 8;

    public static SmoCollisionBranchRemovalResult Remove(
        SmoDocument document,
        int collisionInfoObjectIndex)
    {
        ArgumentNullException.ThrowIfNull(document);
        if ((uint)collisionInfoObjectIndex >= (uint)document.Objects.Count)
            throw new ArgumentOutOfRangeException(nameof(collisionInfoObjectIndex));

        SmoObjectEntry collisionInfo = document.Objects[collisionInfoObjectIndex];
        if (collisionInfo.TypeHash != SmoClassIds.CollisionInfo)
        {
            throw new ArgumentException(
                $"Object [{collisionInfoObjectIndex}] is not an spCollisionInfo.",
                nameof(collisionInfoObjectIndex));
        }
        if (collisionInfo.ParentIndex is not int ownerIndex)
            throw new InvalidOperationException("The collision has no inline owner branch.");

        uint collisionId = collisionInfo.Id;
        uint ownerId = document.Objects[ownerIndex].Id;
        HashSet<uint> branchIds = document.Objects
            .Where(entry => entry.PhysicalOffset >= collisionInfo.PhysicalOffset &&
                            entry.PhysicalEnd <= collisionInfo.PhysicalEnd)
            .Select(entry => entry.Id)
            .ToHashSet();
        byte[] output = document.Data.ToArray();

        // A valid game collision is referenced by an spPartitionSystem field 7.
        // Remove every matching registry entry first so no dangling physics
        // reference survives even in a file which contains duplicate bindings.
        while (true)
        {
            SmoDocument current = SmoDocument.Parse(output, document.SourcePath);
            SmoObjectEntry? registryOwner = current.Objects.FirstOrDefault(entry =>
                entry.TypeHash == SmoClassIds.PartitionSystem &&
                HasReference(current, entry, fieldType: 7, collisionId));
            if (registryOwner is null)
                break;
            output = SmoVisualForestInjector.RemoveReference(
                current,
                registryOwner.Id,
                fieldType: 7,
                collisionId);
        }

        SmoDocument beforeBranchRemoval = SmoDocument.Parse(
            output,
            document.SourcePath);
        output = SmoVisualForestInjector.RemoveInlineBranch(
            beforeBranchRemoval,
            ownerId,
            collisionId);

        SmoDocument verified = SmoDocument.Parse(output, document.SourcePath);
        if ((!document.HasErrors && verified.HasErrors) ||
            verified.Objects.Any(entry => branchIds.Contains(entry.Id)) ||
            verified.Objects.Any(entry =>
                entry.TypeHash == SmoClassIds.PartitionSystem &&
                HasReference(verified, entry, fieldType: 7, collisionId)))
        {
            throw new InvalidDataException(
                "The removed collision branch failed structural verification.");
        }

        return new SmoCollisionBranchRemovalResult(
            output,
            document.Objects.Count - verified.Objects.Count);
    }

    private static bool HasReference(
        SmoDocument document,
        SmoObjectEntry owner,
        int fieldType,
        uint targetId)
    {
        ReadOnlySpan<byte> bytes = document.Data.Span.Slice(
            checked((int)owner.PhysicalOffset),
            checked((int)owner.SerializedSize));
        int offset = ObjectSignatureSize;
        while (offset < bytes.Length &&
               SmoDataBlockReader.TryReadHeader(
                   bytes,
                   offset,
                   out SmoDataBlockHeader field))
        {
            if (field.FieldType == fieldType &&
                field.PayloadSize == ObjectReferenceSize &&
                BinaryPrimitives.ReadUInt32LittleEndian(
                    bytes[field.PayloadOffset..]) == targetId &&
                BinaryPrimitives.ReadUInt32LittleEndian(
                    bytes[(field.PayloadOffset + sizeof(uint))..]) == 0)
            {
                return true;
            }
            offset = checked((int)field.PayloadEnd);
        }
        return false;
    }
}
