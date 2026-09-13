namespace SmoViewer.Core;

/// <summary>Identifies one direct data-block field of an SBOO object.</summary>
public readonly record struct SmoFieldSelector(
    int FieldType,
    int Occurrence = 0,
    uint? PayloadSize = null)
{
    public bool Matches(SmoObjectField field) =>
        field.FieldType == FieldType &&
        field.Occurrence == Occurrence &&
        (!PayloadSize.HasValue || field.PayloadSize == PayloadSize.Value);
}

/// <summary>
/// One direct serialized field owned by an object. Payloads may themselves
/// contain inline objects; they remain opaque until the referenced catalog
/// object is inspected through its own entry.
/// </summary>
public sealed record SmoObjectField(
    int ObjectIndex,
    uint ObjectId,
    int FieldType,
    int Occurrence,
    byte RawHeader,
    SmoDataBlockSizeCode SizeKind,
    int HeaderSize,
    uint PayloadSize,
    int RelativeHeaderOffset,
    int RelativePayloadOffset,
    int AbsoluteHeaderOffset,
    int AbsolutePayloadOffset,
    ReadOnlyMemory<byte> Payload)
{
    public int EncodedSize => checked(HeaderSize + (int)PayloadSize);
    public int RelativeEnd => checked(RelativePayloadOffset + (int)PayloadSize);
    public int AbsoluteEnd => checked(AbsolutePayloadOffset + (int)PayloadSize);
    public SmoFieldSelector Selector => new(FieldType, Occurrence, PayloadSize);
}

/// <summary>
/// Lossless low-level view used by format research tools. It deliberately does
/// not assign semantics to fields which are absent from the schema registry.
/// </summary>
public sealed record SmoRawObject(
    SmoObjectEntry DirectoryEntry,
    IReadOnlyList<SmoObjectField> Fields);

/// <summary>
/// Authoritative direct-field reader shared by decoders, schemas and writers.
/// It never scans arbitrary bytes for a plausible field header.
/// </summary>
public static class SmoObjectFieldReader
{
    private const int ObjectSignatureSize = 8;

    public static IReadOnlyList<SmoObjectField> Read(
        SmoDocument document,
        SmoObjectEntry entry)
    {
        if (!TryRead(document, entry, out IReadOnlyList<SmoObjectField>? fields,
                out string error))
        {
            throw new InvalidDataException(error);
        }
        return fields;
    }

    public static SmoRawObject ReadObject(
        SmoDocument document,
        SmoObjectEntry entry) => new(entry, Read(document, entry));

    public static bool TryRead(
        SmoDocument document,
        SmoObjectEntry entry,
        out IReadOnlyList<SmoObjectField> fields,
        out string error)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(entry);
        fields = Array.Empty<SmoObjectField>();
        error = string.Empty;
        if (!entry.IsWithinDataSection || !entry.SignatureMatches ||
            entry.PhysicalOffset < 0 || entry.PhysicalOffset > int.MaxValue ||
            entry.SerializedSize > int.MaxValue ||
            entry.PhysicalEnd > document.Data.Length ||
            entry.SerializedSize < ObjectSignatureSize)
        {
            error = $"Object [{entry.Index}] is outside the confirmed SMO data section.";
            return false;
        }

        ReadOnlySpan<byte> serialized = document.Data.Span.Slice(
            checked((int)entry.PhysicalOffset), checked((int)entry.SerializedSize));
        var result = new List<SmoObjectField>();
        var occurrences = new Dictionary<int, int>();
        int offset = ObjectSignatureSize;
        while (offset < serialized.Length)
        {
            if (!SmoDataBlockReader.TryReadHeader(
                    serialized, offset, out SmoDataBlockHeader header))
            {
                error = $"Object [{entry.Index}] \"{entry.Name.TrimEnd('\0')}\" has " +
                        $"an unreadable direct field at relative offset 0x{offset:X}.";
                return false;
            }

            int occurrence = occurrences.GetValueOrDefault(header.FieldType);
            occurrences[header.FieldType] = occurrence + 1;
            int absoluteHeader = checked((int)entry.PhysicalOffset + header.Offset);
            int absolutePayload = checked((int)entry.PhysicalOffset + header.PayloadOffset);
            result.Add(new SmoObjectField(
                entry.Index,
                entry.Id,
                header.FieldType,
                occurrence,
                header.RawHeader,
                header.SizeKind,
                header.HeaderSize,
                header.PayloadSize,
                header.Offset,
                header.PayloadOffset,
                absoluteHeader,
                absolutePayload,
                document.Data.Slice(
                    absolutePayload, checked((int)header.PayloadSize))));

            int next = checked((int)header.PayloadEnd);
            if (next <= offset)
            {
                error = $"Object [{entry.Index}] contains a non-advancing field at " +
                        $"relative offset 0x{offset:X}.";
                return false;
            }
            offset = next;
        }

        fields = result.AsReadOnly();
        return true;
    }

    public static bool TryFind(
        SmoDocument document,
        SmoObjectEntry entry,
        SmoFieldSelector selector,
        out SmoObjectField? field)
    {
        field = null;
        if (selector.Occurrence < 0 ||
            !TryRead(document, entry, out IReadOnlyList<SmoObjectField>? fields, out _))
        {
            return false;
        }

        foreach (SmoObjectField candidate in fields)
        {
            if (!selector.Matches(candidate))
                continue;
            field = candidate;
            return true;
        }
        return false;
    }
}
