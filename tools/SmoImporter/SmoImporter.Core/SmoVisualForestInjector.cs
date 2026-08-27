using System.Buffers.Binary;
using SmoViewer.Core;

namespace SmoImporter.Core;

/// <summary>
/// One catalog object serialized inside a generated visual attachment.
/// <paramref name="RelativeOffset"/> is measured from the start of the
/// attachment's complete field bytes to the object's SBOO/type signature.
/// </summary>
public sealed record SmoVisualForestEntry(
    uint Id,
    byte[] RawName,
    uint TypeHash,
    int RelativeOffset,
    uint SerializedSize);

/// <summary>
/// Complete FFPS field bytes to insert immediately before a target object's
/// terminal empty field, plus the catalog entries serialized inside them.
/// </summary>
public sealed record SmoVisualForestAttachment(
    uint TargetOwnerId,
    byte[] FieldData,
    IReadOnlyList<SmoVisualForestEntry> Entries);

public enum SmoVisualForestInsertionKind
{
    BeforeTerminal = 0,
    AfterLastFieldType
}

/// <summary>
/// A serializer-owned field attachment and a semantic insertion anchor. This
/// is the common boundary between format-specific import builders and clients
/// that either rewrite an SMO immediately or retain the attachment in a project.
/// </summary>
public sealed record SmoVisualForestOperation(
    SmoVisualForestAttachment Attachment,
    SmoVisualForestInsertionKind InsertionKind,
    int AnchorFieldType = 0);

/// <summary>
/// Low-level container writer shared by donor-graph and synthetic visual
/// builders. It inserts already serialized fields, grows every containing
/// FFPS payload/inline prefix, relocates existing directory entries and adds
/// the supplied generated-object entries without interpreting visual classes.
/// </summary>
internal static class SmoVisualForestInjector
{
    private const int ObjectSignatureSize = 8;
    private const int ObjectReferenceSize = 8;

    /// <summary>
    /// Inserts one ordered group of generated visual attachments into a single
    /// existing owner. Call again with the reparsed result to target another
    /// owner; this preserves deterministic directory and byte ordering.
    /// </summary>
    internal static byte[] Inject(
        SmoDocument current,
        uint targetOwnerId,
        IReadOnlyList<SmoVisualForestAttachment> attachments)
    {
        ArgumentNullException.ThrowIfNull(current);
        ArgumentNullException.ThrowIfNull(attachments);
        if (attachments.Count == 0)
            return current.Data.ToArray();
        if (attachments.Any(attachment => attachment is null ||
                                          attachment.TargetOwnerId != targetOwnerId))
        {
            throw new InvalidOperationException(
                "Attachment group has inconsistent target owners.");
        }

        SmoObjectEntry owner = current.Objects.Single(entry => entry.Id == targetOwnerId);
        ReadOnlySpan<byte> ownerData = ObjectBytes(current, owner);
        if (ownerData.Length == 0 || ownerData[^1] != 0)
        {
            throw new InvalidOperationException(
                $"Target owner [{owner.Index}] has no terminal empty field.");
        }

        return InjectAt(
            current,
            owner,
            checked((int)owner.LogicalEnd - 1),
            attachments);
    }

    /// <summary>
    /// Inserts fields immediately after the owner's last field of the requested
    /// type. This preserves the native ordering of repeated reference arrays
    /// whose entries precede other serialized properties.
    /// </summary>
    internal static byte[] InjectAfterLastFieldType(
        SmoDocument current,
        uint targetOwnerId,
        int fieldType,
        IReadOnlyList<SmoVisualForestAttachment> attachments)
    {
        ArgumentNullException.ThrowIfNull(current);
        ArgumentNullException.ThrowIfNull(attachments);
        SmoObjectEntry owner = current.Objects.Single(entry => entry.Id == targetOwnerId);
        ReadOnlySpan<byte> ownerData = ObjectBytes(current, owner);
        int offset = ObjectSignatureSize;
        int insertionOffset = -1;
        while (offset < ownerData.Length &&
               SmoDataBlockReader.TryReadHeader(
                   ownerData, offset, out SmoDataBlockHeader field))
        {
            if (field.FieldType == fieldType)
                insertionOffset = checked((int)field.PayloadEnd);
            offset = checked((int)field.PayloadEnd);
        }
        if (insertionOffset < 0)
        {
            throw new InvalidOperationException(
                $"Target owner [{owner.Index}] has no field type {fieldType}.");
        }
        return InjectAt(
            current,
            owner,
            checked((int)owner.LogicalOffset + insertionOffset),
            attachments);
    }

    internal static byte[] InjectAfterFieldPayload(
        SmoDocument current,
        uint targetOwnerId,
        int absolutePayloadOffset,
        byte[] fieldData)
    {
        ArgumentNullException.ThrowIfNull(current);
        ArgumentNullException.ThrowIfNull(fieldData);
        SmoObjectEntry owner = current.Objects.Single(entry => entry.Id == targetOwnerId);
        ReadOnlySpan<byte> ownerData = ObjectBytes(current, owner);
        int offset = ObjectSignatureSize;
        while (offset < ownerData.Length &&
               SmoDataBlockReader.TryReadHeader(
                   ownerData, offset, out SmoDataBlockHeader field))
        {
            if (checked((int)owner.PhysicalOffset + field.PayloadOffset) ==
                absolutePayloadOffset)
            {
                var attachment = new SmoVisualForestAttachment(
                    targetOwnerId,
                    fieldData,
                    Array.Empty<SmoVisualForestEntry>());
                return InjectAt(
                    current,
                    owner,
                    checked((int)owner.LogicalOffset + (int)field.PayloadEnd),
                    [attachment]);
            }
            offset = checked((int)field.PayloadEnd);
        }
        throw new InvalidOperationException(
            $"Target owner [{owner.Index}] has no field with payload at " +
            $"0x{absolutePayloadOffset:X}.");
    }

    private static byte[] InjectAt(
        SmoDocument current,
        SmoObjectEntry owner,
        int insertionLogical,
        IReadOnlyList<SmoVisualForestAttachment> attachments)
    {
        if (attachments.Any(attachment => attachment is null ||
                                          attachment.TargetOwnerId != owner.Id))
        {
            throw new InvalidOperationException(
                "Attachment group has inconsistent target owners.");
        }
        ValidateAttachments(current, attachments);
        int insertedLength = attachments.Sum(attachment => attachment.FieldData.Length);
        byte[] insertedFields = new byte[insertedLength];
        int insertedCursor = 0;
        foreach (SmoVisualForestAttachment attachment in attachments)
        {
            attachment.FieldData.CopyTo(insertedFields, insertedCursor);
            insertedCursor = checked(insertedCursor + attachment.FieldData.Length);
        }

        IReadOnlyList<DirectoryEntry> directory = BuildDirectory(
            current,
            insertionLogical,
            insertedLength,
            attachments);
        return InsertVisualFields(
            current,
            insertionLogical,
            insertedFields,
            directory);
    }

    /// <summary>
    /// Turns one ordinary reference-only field into the canonical inline
    /// definition of a new resource. The owner remains a normal scene
    /// placement and every other placement may reference the generated ID.
    /// </summary>
    internal static byte[] PromoteReferenceToInline(
        SmoDocument current,
        uint ownerId,
        byte fieldType,
        uint oldReferenceId,
        uint newObjectId,
        byte[] rawName,
        uint typeHash,
        byte[] objectData)
    {
        ArgumentNullException.ThrowIfNull(current);
        ArgumentNullException.ThrowIfNull(rawName);
        ArgumentNullException.ThrowIfNull(objectData);
        if (current.Objects.Any(entry => entry.Id == newObjectId))
            throw new InvalidOperationException($"Object ID {newObjectId} already exists.");
        if (objectData.Length < ObjectSignatureSize ||
            BinaryPrimitives.ReadUInt32LittleEndian(objectData) != typeHash ||
            !objectData.AsSpan(4, 4).SequenceEqual("SBOO"u8))
        {
            throw new InvalidDataException(
                "Inline resource bytes do not match the requested class signature.");
        }

        SmoObjectEntry owner = current.Objects.Single(entry => entry.Id == ownerId);
        SmoDataBlockHeader reference = FindReferenceField(
            current, owner, fieldType, oldReferenceId);
        byte[] field = new byte[checked(13 + objectData.Length)];
        field[0] = checked((byte)(0xE0 | fieldType));
        WriteUInt32(field, 1, checked((uint)(ObjectReferenceSize + objectData.Length)));
        WriteUInt32(field, 5, newObjectId);
        WriteUInt32(field, 9, checked((uint)objectData.Length));
        objectData.CopyTo(field, 13);
        int fieldLogicalOffset = checked((int)owner.LogicalOffset + reference.Offset);
        return ReplaceReferenceField(
            current,
            fieldLogicalOffset,
            checked((int)(reference.PayloadEnd - reference.Offset)),
            field,
            new DirectoryEntry(
                newObjectId,
                rawName,
                typeHash,
                checked((uint)(fieldLogicalOffset + 13)),
                checked((uint)objectData.Length)));
    }

    /// <summary>
    /// Replaces one inline leaf resource with another in a single container
    /// rewrite. This is the fixed-size graph equivalent of demote + promote,
    /// without materializing the large intermediate container between them.
    /// </summary>
    internal static byte[] ReplaceInlineLeaf(
        SmoDocument current,
        uint ownerId,
        byte fieldType,
        uint oldObjectId,
        uint newObjectId,
        byte[] rawName,
        uint typeHash,
        byte[] objectData)
    {
        ArgumentNullException.ThrowIfNull(current);
        ArgumentNullException.ThrowIfNull(rawName);
        ArgumentNullException.ThrowIfNull(objectData);
        if (oldObjectId != newObjectId &&
            current.Objects.Any(entry => entry.Id == newObjectId))
        {
            throw new InvalidOperationException(
                $"Object ID {newObjectId} already exists.");
        }
        if (objectData.Length < ObjectSignatureSize ||
            BinaryPrimitives.ReadUInt32LittleEndian(objectData) != typeHash ||
            !objectData.AsSpan(4, 4).SequenceEqual("SBOO"u8))
        {
            throw new InvalidDataException(
                "Inline replacement bytes do not match the requested class signature.");
        }

        SmoObjectEntry owner = current.Objects.Single(entry => entry.Id == ownerId);
        SmoObjectEntry child = current.Objects.Single(entry => entry.Id == oldObjectId);
        if (child.ParentIndex != owner.Index)
        {
            throw new InvalidOperationException(
                $"Object {oldObjectId} is not an inline child of owner {ownerId}.");
        }
        if (current.Objects.Any(entry => entry.ParentIndex == child.Index))
        {
            throw new NotSupportedException(
                $"Inline object {oldObjectId} is not a leaf and cannot be replaced safely.");
        }

        SmoDataBlockHeader inline = FindInlineField(
            current,
            owner,
            child,
            fieldType);
        byte[] field = new byte[checked(13 + objectData.Length)];
        field[0] = checked((byte)(0xE0 | fieldType));
        WriteUInt32(field, 1, checked((uint)(ObjectReferenceSize + objectData.Length)));
        WriteUInt32(field, 5, newObjectId);
        WriteUInt32(field, 9, checked((uint)objectData.Length));
        objectData.CopyTo(field, 13);
        int fieldLogicalOffset = checked((int)owner.LogicalOffset + inline.Offset);
        return ReplaceReferenceField(
            current,
            fieldLogicalOffset,
            checked((int)(inline.PayloadEnd - inline.Offset)),
            field,
            new DirectoryEntry(
                newObjectId,
                rawName,
                typeHash,
                checked((uint)(fieldLogicalOffset + 13)),
                checked((uint)objectData.Length)),
            removedObjectId: oldObjectId);
    }

    /// <summary>Removes one reference-only placement binding without touching its resource.</summary>
    internal static byte[] RemoveReference(
        SmoDocument current,
        uint ownerId,
        byte fieldType,
        uint referenceId)
    {
        ArgumentNullException.ThrowIfNull(current);
        SmoObjectEntry owner = current.Objects.Single(entry => entry.Id == ownerId);
        SmoDataBlockHeader reference = FindReferenceField(
            current, owner, fieldType, referenceId);
        int fieldLogicalOffset = checked((int)owner.LogicalOffset + reference.Offset);
        return ReplaceReferenceField(
            current,
            fieldLogicalOffset,
            checked((int)(reference.PayloadEnd - reference.Offset)),
            [],
            addedEntry: null);
    }

    internal static byte[] ReplaceReferenceId(
        SmoDocument current,
        uint ownerId,
        byte fieldType,
        uint oldReferenceId,
        uint newReferenceId)
    {
        ArgumentNullException.ThrowIfNull(current);
        SmoObjectEntry owner = current.Objects.Single(entry => entry.Id == ownerId);
        SmoDataBlockHeader reference = FindReferenceField(
            current, owner, fieldType, oldReferenceId);
        byte[] replacement = current.Data.Span.Slice(
            checked((int)owner.PhysicalOffset + reference.Offset),
            checked((int)(reference.PayloadEnd - reference.Offset))).ToArray();
        WriteUInt32(replacement, reference.PayloadOffset - reference.Offset, newReferenceId);
        int fieldLogicalOffset = checked((int)owner.LogicalOffset + reference.Offset);
        return ReplaceReferenceField(
            current,
            fieldLogicalOffset,
            replacement.Length,
            replacement,
            addedEntry: null);
    }

    internal static byte[] RemoveInlineBranch(
        SmoDocument current,
        uint ownerId,
        uint childObjectId)
    {
        ArgumentNullException.ThrowIfNull(current);
        SmoObjectEntry owner = current.Objects.Single(entry => entry.Id == ownerId);
        SmoObjectEntry child = current.Objects.Single(entry => entry.Id == childObjectId);
        if (child.ParentIndex != owner.Index)
            throw new InvalidOperationException(
                $"Object {childObjectId} is not an inline child of owner {ownerId}.");
        SmoDataBlockHeader inline = FindInlineField(current, owner, child);
        HashSet<uint> removedIds = current.Objects
            .Where(entry => entry.PhysicalOffset >= child.PhysicalOffset &&
                            entry.PhysicalEnd <= child.PhysicalEnd)
            .Select(entry => entry.Id)
            .ToHashSet();
        int fieldLogicalOffset = checked((int)owner.LogicalOffset + inline.Offset);
        return ReplaceReferenceField(
            current,
            fieldLogicalOffset,
            checked((int)(inline.PayloadEnd - inline.Offset)),
            [],
            addedEntry: null,
            removedObjectIds: removedIds);
    }

    /// <summary>
    /// Converts one inline leaf definition into an ordinary reference and
    /// removes its catalog entry. The preserved serialized object bytes can
    /// then be promoted into another reference field with the same object ID.
    /// </summary>
    internal static byte[] DemoteInlineLeafToReference(
        SmoDocument current,
        uint ownerId,
        byte fieldType,
        uint objectId)
    {
        ArgumentNullException.ThrowIfNull(current);
        SmoObjectEntry owner = current.Objects.Single(entry => entry.Id == ownerId);
        SmoObjectEntry child = current.Objects.Single(entry => entry.Id == objectId);
        if (child.ParentIndex != owner.Index)
        {
            throw new InvalidOperationException(
                $"Object {objectId} is not an inline child of owner {ownerId}.");
        }
        if (current.Objects.Any(entry => entry.ParentIndex == child.Index))
        {
            throw new NotSupportedException(
                $"Inline object {objectId} is not a leaf and cannot be relocated safely.");
        }

        SmoDataBlockHeader inline = FindInlineField(
            current, owner, child, fieldType);
        byte[] reference = new byte[13];
        reference[0] = checked((byte)(0xE0 | fieldType));
        WriteUInt32(reference, 1, ObjectReferenceSize);
        WriteUInt32(reference, 5, objectId);
        WriteUInt32(reference, 9, 0);
        int fieldLogicalOffset = checked((int)owner.LogicalOffset + inline.Offset);
        return ReplaceReferenceField(
            current,
            fieldLogicalOffset,
            checked((int)(inline.PayloadEnd - inline.Offset)),
            reference,
            addedEntry: null,
            removedObjectId: objectId);
    }

    private static void ValidateAttachments(
        SmoDocument current,
        IReadOnlyList<SmoVisualForestAttachment> attachments)
    {
        HashSet<uint> ids = current.Objects.Select(entry => entry.Id).ToHashSet();
        foreach (SmoVisualForestAttachment attachment in attachments)
        {
            ArgumentNullException.ThrowIfNull(attachment.FieldData);
            ArgumentNullException.ThrowIfNull(attachment.Entries);
            if (attachment.FieldData.Length == 0)
                throw new InvalidOperationException("A visual attachment has no field bytes.");

            ReadOnlySpan<byte> fieldData = attachment.FieldData;
            foreach (SmoVisualForestEntry entry in attachment.Entries)
            {
                if (entry is null)
                    throw new InvalidOperationException("A visual attachment has a null entry.");
                ArgumentNullException.ThrowIfNull(entry.RawName);
                if (!ids.Add(entry.Id))
                    throw new InvalidOperationException(
                        $"Generated visual object ID {entry.Id} is not unique.");
                if (entry.RawName.Length > ushort.MaxValue)
                {
                    throw new InvalidOperationException(
                        $"Generated visual object ID {entry.Id} has an invalid raw name.");
                }

                long end = (long)entry.RelativeOffset + entry.SerializedSize;
                if (entry.RelativeOffset < ObjectReferenceSize ||
                    entry.SerializedSize < ObjectSignatureSize ||
                    end > fieldData.Length)
                {
                    throw new InvalidOperationException(
                        $"Generated visual object ID {entry.Id} is outside its attachment field.");
                }

                ReadOnlySpan<byte> serialized = fieldData.Slice(
                    entry.RelativeOffset, checked((int)entry.SerializedSize));
                int prefix = entry.RelativeOffset - ObjectReferenceSize;
                uint actualId = BinaryPrimitives.ReadUInt32LittleEndian(fieldData[prefix..]);
                uint actualSize = BinaryPrimitives.ReadUInt32LittleEndian(
                    fieldData[(prefix + sizeof(uint))..]);
                uint actualTypeHash = BinaryPrimitives.ReadUInt32LittleEndian(serialized);
                bool signatureMatches = serialized.Slice(sizeof(uint), sizeof(uint))
                    .SequenceEqual("SBOO"u8);
                if (actualId != entry.Id || actualSize != entry.SerializedSize ||
                    actualTypeHash != entry.TypeHash || !signatureMatches)
                {
                    throw new InvalidOperationException(
                        $"Generated visual object ID {entry.Id} has a stale inline prefix or " +
                        $"signature at attachment offset 0x{entry.RelativeOffset:X}: " +
                        $"prefix=({actualId},0x{actualSize:X}), " +
                        $"expected=({entry.Id},0x{entry.SerializedSize:X}), " +
                        $"type=0x{actualTypeHash:X8}/0x{entry.TypeHash:X8}, " +
                        $"SBOO={signatureMatches}.");
                }
            }
        }
    }

    private static SmoDataBlockHeader FindReferenceField(
        SmoDocument current,
        SmoObjectEntry owner,
        byte fieldType,
        uint referenceId)
    {
        ReadOnlySpan<byte> bytes = ObjectBytes(current, owner);
        int offset = ObjectSignatureSize;
        SmoDataBlockHeader match = default;
        int matches = 0;
        while (offset < bytes.Length &&
               SmoDataBlockReader.TryReadHeader(
                   bytes, offset, out SmoDataBlockHeader field))
        {
            if (field.FieldType == fieldType && field.PayloadSize == ObjectReferenceSize &&
                BinaryPrimitives.ReadUInt32LittleEndian(bytes[field.PayloadOffset..]) ==
                    referenceId &&
                BinaryPrimitives.ReadUInt32LittleEndian(
                    bytes[(field.PayloadOffset + sizeof(uint))..]) == 0)
            {
                match = field;
                matches++;
            }
            offset = checked((int)field.PayloadEnd);
        }
        return matches == 1
            ? match
            : throw new InvalidOperationException(
                $"Owner {owner.Id} has {matches} reference-only field(s) " +
                $"type {fieldType} for object {referenceId}; exactly one is required.");
    }

    private static SmoDataBlockHeader FindInlineField(
        SmoDocument current,
        SmoObjectEntry owner,
        SmoObjectEntry child,
        byte fieldType)
    {
        ReadOnlySpan<byte> bytes = ObjectBytes(current, owner);
        int offset = ObjectSignatureSize;
        while (offset < bytes.Length &&
               SmoDataBlockReader.TryReadHeader(
                   bytes, offset, out SmoDataBlockHeader field))
        {
            long payloadPhysical = owner.PhysicalOffset + field.PayloadOffset;
            if (field.FieldType == fieldType &&
                field.PayloadSize == child.SerializedSize + ObjectReferenceSize &&
                payloadPhysical == child.PhysicalOffset - ObjectReferenceSize &&
                BinaryPrimitives.ReadUInt32LittleEndian(bytes[field.PayloadOffset..]) ==
                    child.Id &&
                BinaryPrimitives.ReadUInt32LittleEndian(
                    bytes[(field.PayloadOffset + sizeof(uint))..]) ==
                    child.SerializedSize)
            {
                return field;
            }
            offset = checked((int)field.PayloadEnd);
        }
        throw new InvalidOperationException(
            $"Owner {owner.Id} has no inline field type {fieldType} for object {child.Id}.");
    }

    private static SmoDataBlockHeader FindInlineField(
        SmoDocument current,
        SmoObjectEntry owner,
        SmoObjectEntry child)
    {
        ReadOnlySpan<byte> bytes = ObjectBytes(current, owner);
        int offset = ObjectSignatureSize;
        while (offset < bytes.Length &&
               SmoDataBlockReader.TryReadHeader(
                   bytes, offset, out SmoDataBlockHeader field))
        {
            long payloadPhysical = owner.PhysicalOffset + field.PayloadOffset;
            if (field.PayloadSize == child.SerializedSize + ObjectReferenceSize &&
                payloadPhysical == child.PhysicalOffset - ObjectReferenceSize &&
                BinaryPrimitives.ReadUInt32LittleEndian(bytes[field.PayloadOffset..]) ==
                    child.Id &&
                BinaryPrimitives.ReadUInt32LittleEndian(
                    bytes[(field.PayloadOffset + sizeof(uint))..]) ==
                    child.SerializedSize)
            {
                return field;
            }
            offset = checked((int)field.PayloadEnd);
        }
        throw new InvalidOperationException(
            $"Owner {owner.Id} has no inline field for object {child.Id}.");
    }

    private static byte[] ReplaceReferenceField(
        SmoDocument current,
        int fieldStart,
        int oldLength,
        ReadOnlySpan<byte> replacement,
        DirectoryEntry? addedEntry,
        uint? removedObjectId = null,
        IReadOnlySet<uint>? removedObjectIds = null)
    {
        int fieldEnd = checked(fieldStart + oldLength);
        int delta = checked(replacement.Length - oldLength);
        ReadOnlySpan<byte> source = current.Data.Span.Slice(
            checked((int)current.Header.DataStart), checked((int)current.Header.DataSize));
        var directory = current.Objects
            .Where(entry => entry.Id != removedObjectId &&
                            removedObjectIds?.Contains(entry.Id) != true)
            .Select(entry =>
        {
            bool contains = (ulong)entry.LogicalOffset <= (ulong)fieldStart &&
                            (ulong)fieldEnd <= entry.LogicalEnd;
            uint mappedOffset = entry.LogicalOffset >= fieldEnd
                ? checked((uint)(entry.LogicalOffset + delta))
                : entry.LogicalOffset;
            return new DirectoryEntry(
                entry.Id,
                entry.RawName.ToArray(),
                entry.TypeHash,
                mappedOffset,
                contains
                    ? checked((uint)(entry.SerializedSize + delta))
                    : entry.SerializedSize);
        }).ToList();
        if (addedEntry is not null)
            directory.Add(addedEntry);
        byte[] container = CreateContainer(
            current,
            checked(source.Length + delta),
            directory.OrderBy(entry => entry.LogicalOffset).ToArray(),
            out int dataStart);
        Span<byte> rewritten = container.AsSpan(
            dataStart,
            checked(source.Length + delta));
        source[..fieldStart].CopyTo(rewritten);
        replacement.CopyTo(rewritten[fieldStart..]);
        source[fieldEnd..].CopyTo(rewritten[(fieldStart + replacement.Length)..]);

        foreach (SmoObjectEntry entry in current.Objects)
        {
            ReadOnlySpan<byte> serialized = ObjectBytes(current, entry);
            int offset = ObjectSignatureSize;
            while (offset < serialized.Length &&
                   SmoDataBlockReader.TryReadHeader(
                       serialized, offset, out SmoDataBlockHeader field))
            {
                int absoluteHeader = checked((int)entry.LogicalOffset + field.Offset);
                long payloadStart = entry.LogicalOffset + field.PayloadOffset;
                long payloadEnd = entry.LogicalOffset + field.PayloadEnd;
                if (absoluteHeader != fieldStart &&
                    payloadStart <= fieldStart && fieldEnd <= payloadEnd)
                {
                    int mappedHeader = MapReplacedOffset(
                        absoluteHeader, fieldEnd, delta);
                    WritePayloadSize(
                        rewritten,
                        mappedHeader,
                        field,
                        checked((uint)(field.PayloadSize + delta)));
                }
                offset = checked((int)field.PayloadEnd);
            }
        }

        foreach (SmoObjectEntry entry in current.Objects.Where(entry =>
                     (ulong)entry.LogicalOffset <= (ulong)fieldStart &&
                     (ulong)fieldEnd <= entry.LogicalEnd))
        {
            int prefix = checked((int)entry.LogicalOffset - ObjectReferenceSize);
            if (prefix < 0 ||
                BinaryPrimitives.ReadUInt32LittleEndian(source[prefix..]) != entry.Id ||
                BinaryPrimitives.ReadUInt32LittleEndian(source[(prefix + 4)..]) !=
                    entry.SerializedSize)
            {
                continue;
            }
            int mappedPrefix = MapReplacedOffset(prefix, fieldEnd, delta);
            WriteUInt32(
                rewritten,
                mappedPrefix + sizeof(uint),
                checked((uint)(entry.SerializedSize + delta)));
        }

        return container;
    }

    private static int MapReplacedOffset(int offset, int oldEnd, int delta) =>
        offset >= oldEnd ? checked(offset + delta) : offset;

    private static byte[] InsertVisualFields(
        SmoDocument current,
        int insertionLogical,
        ReadOnlySpan<byte> insertedFields,
        IReadOnlyList<DirectoryEntry> directory)
    {
        ReadOnlySpan<byte> sourceData = current.Data.Span.Slice(
            checked((int)current.Header.DataStart), checked((int)current.Header.DataSize));
        int rewrittenLength = checked(sourceData.Length + insertedFields.Length);
        byte[] container = CreateContainer(
            current,
            rewrittenLength,
            directory,
            out int dataStart);
        Span<byte> result = container.AsSpan(dataStart, rewrittenLength);
        sourceData[..insertionLogical].CopyTo(result);
        insertedFields.CopyTo(result[insertionLogical..]);
        sourceData[insertionLogical..].CopyTo(
            result[(insertionLogical + insertedFields.Length)..]);

        foreach (SmoObjectEntry entry in current.Objects)
        {
            ReadOnlySpan<byte> serialized = sourceData.Slice(
                checked((int)entry.LogicalOffset), checked((int)entry.SerializedSize));
            int offset = ObjectSignatureSize;
            while (offset < serialized.Length &&
                   SmoDataBlockReader.TryReadHeader(
                       serialized, offset, out SmoDataBlockHeader field))
            {
                long start = entry.LogicalOffset + field.PayloadOffset;
                long end = entry.LogicalOffset + field.PayloadEnd;
                if (start <= insertionLogical && insertionLogical < end)
                {
                    int mappedHeader = checked((int)MapOffset(
                        entry.LogicalOffset + field.Offset,
                        insertionLogical,
                        insertedFields.Length));
                    WritePayloadSize(
                        result,
                        mappedHeader,
                        field,
                        checked(field.PayloadSize + (uint)insertedFields.Length));
                }
                offset = checked((int)field.PayloadEnd);
            }
        }

        foreach (SmoObjectEntry entry in current.Objects.Where(entry =>
                     entry.ParentIndex is not null && ContainsOffset(entry, insertionLogical)))
        {
            int oldPrefix = checked((int)entry.LogicalOffset - ObjectReferenceSize);
            if (oldPrefix < 0 ||
                BinaryPrimitives.ReadUInt32LittleEndian(sourceData[oldPrefix..]) != entry.Id ||
                BinaryPrimitives.ReadUInt32LittleEndian(
                    sourceData[(oldPrefix + sizeof(uint))..]) != entry.SerializedSize)
            {
                continue;
            }

            int newPrefix = checked((int)MapOffset(
                oldPrefix, insertionLogical, insertedFields.Length));
            WriteUInt32(
                result,
                newPrefix + sizeof(uint),
                checked(entry.SerializedSize + (uint)insertedFields.Length));
        }
        return container;
    }

    private static IReadOnlyList<DirectoryEntry> BuildDirectory(
        SmoDocument current,
        int insertionLogical,
        int insertedLength,
        IReadOnlyList<SmoVisualForestAttachment> attachments)
    {
        var result = current.Objects.Select(entry => new DirectoryEntry(
            entry.Id,
            entry.RawName.ToArray(),
            entry.TypeHash,
            checked((uint)MapOffset(entry.LogicalOffset, insertionLogical, insertedLength)),
            ContainsOffset(entry, insertionLogical)
                ? checked(entry.SerializedSize + (uint)insertedLength)
                : entry.SerializedSize)).ToList();

        int cursor = insertionLogical;
        foreach (SmoVisualForestAttachment attachment in attachments)
        {
            foreach (SmoVisualForestEntry entry in attachment.Entries)
            {
                result.Add(new DirectoryEntry(
                    entry.Id,
                    entry.RawName.ToArray(),
                    entry.TypeHash,
                    checked((uint)(cursor + entry.RelativeOffset)),
                    entry.SerializedSize));
            }
            cursor = checked(cursor + attachment.FieldData.Length);
        }
        if (cursor != insertionLogical + insertedLength)
        {
            throw new InvalidOperationException(
                "Inserted visual branch accounting is inconsistent.");
        }
        return result.OrderBy(entry => entry.LogicalOffset).ToArray();
    }

    private static byte[] CreateContainer(
        SmoDocument current,
        int dataLength,
        IReadOnlyList<DirectoryEntry> entries,
        out int dataStart)
    {
        int tableSize = entries.Sum(entry => 18 + entry.RawName.Length);
        dataStart = checked(SmoHeader.Size + tableSize + sizeof(uint));
        byte[] result = new byte[checked(dataStart + dataLength)];
        current.Data.Span[..SmoHeader.Size].CopyTo(result);
        WriteUInt32(result, 0x0C, checked((uint)result.Length));
        WriteUInt32(result, 0x14, checked((uint)dataStart));
        WriteUInt32(result, 0x18, checked((uint)dataLength));
        WriteUInt32(result, 0x1C, checked((uint)entries.Count));
        int cursor = SmoHeader.ObjectTableOffset;
        foreach (DirectoryEntry entry in entries)
        {
            WriteUInt32(result, cursor, entry.Id);
            BinaryPrimitives.WriteUInt16LittleEndian(
                result.AsSpan(cursor + sizeof(uint)), checked((ushort)entry.RawName.Length));
            entry.RawName.CopyTo(result.AsSpan(cursor + sizeof(uint) + sizeof(ushort)));
            int fields = cursor + sizeof(uint) + sizeof(ushort) + entry.RawName.Length;
            WriteUInt32(result, fields, entry.TypeHash);
            WriteUInt32(result, fields + sizeof(uint), entry.LogicalOffset);
            WriteUInt32(result, fields + 2 * sizeof(uint), entry.SerializedSize);
            cursor += 18 + entry.RawName.Length;
        }
        return result;
    }

    private static ReadOnlySpan<byte> ObjectBytes(
        SmoDocument document,
        SmoObjectEntry entry) => document.Data.Span.Slice(
            checked((int)entry.PhysicalOffset), checked((int)entry.SerializedSize));

    private static bool ContainsOffset(SmoObjectEntry entry, long logicalOffset) =>
        entry.LogicalOffset <= logicalOffset && logicalOffset < (long)entry.LogicalEnd;

    private static long MapOffset(long oldOffset, int insertionLogical, int insertedLength) =>
        oldOffset >= insertionLogical ? oldOffset + insertedLength : oldOffset;

    private static void WritePayloadSize(
        Span<byte> data,
        int headerOffset,
        SmoDataBlockHeader original,
        uint value)
    {
        int sizeEnd = headerOffset + original.HeaderSize;
        switch (original.SizeKind)
        {
            case SmoDataBlockSizeCode.UInt8:
                data[sizeEnd - 1] = checked((byte)value);
                break;
            case SmoDataBlockSizeCode.UInt16:
                BinaryPrimitives.WriteUInt16LittleEndian(
                    data[(sizeEnd - sizeof(ushort))..], checked((ushort)value));
                break;
            case SmoDataBlockSizeCode.UInt32:
                WriteUInt32(data, sizeEnd - sizeof(uint), value);
                break;
            default:
                throw new InvalidOperationException(
                    $"Resized ancestor field uses non-writable size form {original.SizeKind}.");
        }
    }

    private static void WriteUInt32(Span<byte> data, int offset, uint value) =>
        BinaryPrimitives.WriteUInt32LittleEndian(data[offset..], value);

    private sealed record DirectoryEntry(
        uint Id,
        byte[] RawName,
        uint TypeHash,
        uint LogicalOffset,
        uint SerializedSize);
}
