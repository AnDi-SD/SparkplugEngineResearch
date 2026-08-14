using System.Buffers.Binary;
using SmoViewer.Core;

namespace SmoImporter.Core;

/// <summary>
/// One catalog object serialized inside a generated visual attachment.
/// <paramref name="RelativeOffset"/> is measured from the start of the
/// attachment's complete field bytes to the object's SBOO/type signature.
/// </summary>
internal sealed record SmoVisualForestEntry(
    uint Id,
    byte[] RawName,
    uint TypeHash,
    int RelativeOffset,
    uint SerializedSize);

/// <summary>
/// Complete FFPS field bytes to insert immediately before a target object's
/// terminal empty field, plus the catalog entries serialized inside them.
/// </summary>
internal sealed record SmoVisualForestAttachment(
    uint TargetOwnerId,
    byte[] FieldData,
    IReadOnlyList<SmoVisualForestEntry> Entries);

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

        ValidateAttachments(current, attachments);
        int insertedLength = attachments.Sum(attachment => attachment.FieldData.Length);
        byte[] insertedFields = new byte[insertedLength];
        int insertedCursor = 0;
        foreach (SmoVisualForestAttachment attachment in attachments)
        {
            attachment.FieldData.CopyTo(insertedFields, insertedCursor);
            insertedCursor = checked(insertedCursor + attachment.FieldData.Length);
        }

        int insertionLogical = checked((int)owner.LogicalEnd - 1);
        byte[] modifiedData = InsertVisualFields(current, insertionLogical, insertedFields);
        IReadOnlyList<DirectoryEntry> directory = BuildDirectory(
            current,
            insertionLogical,
            insertedLength,
            attachments);
        return BuildContainer(current, modifiedData, directory);
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
                if (BinaryPrimitives.ReadUInt32LittleEndian(fieldData[prefix..]) != entry.Id ||
                    BinaryPrimitives.ReadUInt32LittleEndian(fieldData[(prefix + sizeof(uint))..]) !=
                        entry.SerializedSize ||
                    BinaryPrimitives.ReadUInt32LittleEndian(serialized) != entry.TypeHash ||
                    !serialized.Slice(sizeof(uint), sizeof(uint)).SequenceEqual("SBOO"u8))
                {
                    throw new InvalidOperationException(
                        $"Generated visual object ID {entry.Id} has a stale inline prefix or signature.");
                }
            }
        }
    }

    private static byte[] InsertVisualFields(
        SmoDocument current,
        int insertionLogical,
        ReadOnlySpan<byte> insertedFields)
    {
        ReadOnlySpan<byte> sourceData = current.Data.Span.Slice(
            checked((int)current.Header.DataStart), checked((int)current.Header.DataSize));
        byte[] result = new byte[checked(sourceData.Length + insertedFields.Length)];
        sourceData[..insertionLogical].CopyTo(result);
        insertedFields.CopyTo(result.AsSpan(insertionLogical));
        sourceData[insertionLogical..].CopyTo(
            result.AsSpan(insertionLogical + insertedFields.Length));

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
        return result;
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

    private static byte[] BuildContainer(
        SmoDocument current,
        ReadOnlySpan<byte> dataSection,
        IReadOnlyList<DirectoryEntry> entries)
    {
        int tableSize = entries.Sum(entry => 18 + entry.RawName.Length);
        int dataStart = checked(SmoHeader.Size + tableSize + sizeof(uint));
        byte[] result = new byte[checked(dataStart + dataSection.Length)];
        current.Data.Span[..SmoHeader.Size].CopyTo(result);
        WriteUInt32(result, 0x0C, checked((uint)result.Length));
        WriteUInt32(result, 0x14, checked((uint)dataStart));
        WriteUInt32(result, 0x18, checked((uint)dataSection.Length));
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
        dataSection.CopyTo(result.AsSpan(dataStart));
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
