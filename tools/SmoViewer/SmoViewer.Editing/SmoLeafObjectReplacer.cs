using System.Buffers.Binary;

namespace SmoViewer.Core;

/// <summary>
/// Replaces a catalogued leaf without interpreting unrelated payloads. Only
/// confirmed direct-field containers and inline ID/size prefixes are rewritten.
/// Compact size headers are widened when necessary; object IDs stay unchanged.
/// </summary>
public static class SmoLeafObjectReplacer
{
    public static byte[] Replace(
        SmoDocument document, int objectIndex, ReadOnlySpan<byte> replacement)
    {
        ArgumentNullException.ThrowIfNull(document);
        if (document.HasErrors)
            throw new InvalidDataException("Cannot rewrite an inconsistent SMO catalog.");
        if ((uint)objectIndex >= (uint)document.Objects.Count)
            throw new ArgumentOutOfRangeException(nameof(objectIndex));
        SmoObjectEntry target = document.Objects[objectIndex];
        if (document.Objects.Any(entry => entry.Index != objectIndex &&
                entry.PhysicalOffset >= target.PhysicalOffset &&
                entry.PhysicalEnd <= target.PhysicalEnd))
            throw new NotSupportedException("The replacement object must be a catalog leaf.");
        ReadOnlySpan<byte> original = document.Data.Span;
        int start = checked((int)target.PhysicalOffset);
        if (replacement.Length < 8 || !replacement[..8].SequenceEqual(original.Slice(start, 8)))
            throw new InvalidDataException("Replacement must preserve the object's class/SBOO signature.");
        var edits = new List<Edit> { new(start, checked((int)target.SerializedSize), replacement.ToArray()) };
        SmoObjectEntry[] ancestors = document.Objects
            .Where(entry => entry.PhysicalOffset <= target.PhysicalOffset &&
                entry.PhysicalEnd >= target.PhysicalEnd)
            .OrderBy(entry => entry.SerializedSize).ToArray();

        foreach (SmoObjectEntry entry in ancestors)
        {
            if (entry.Index != objectIndex)
            {
                IReadOnlyList<SmoObjectField> fields = SmoObjectFieldReader.Read(document, entry);
                SmoObjectField[] containers = fields.Where(field =>
                    field.AbsolutePayloadOffset <= start &&
                    field.AbsoluteEnd >= target.PhysicalEnd).ToArray();
                if (containers.Length != 1)
                    throw new NotSupportedException($"Object [{entry.Index}] has no unique direct field enclosing the edited leaf.");
                SmoObjectField field = containers[0];
                long payloadDelta = DeltaInside(edits, field.AbsolutePayloadOffset, field.AbsoluteEnd);
                if (payloadDelta != 0)
                {
                    if (!SmoDataBlockReader.TryReadHeader(original, field.AbsoluteHeaderOffset, out var header))
                        throw new InvalidDataException("Unreadable enclosing field header.");
                    byte[] newHeader = SmoDataBlockWriter.BuildHeader(
                        field.FieldType, checked((uint)(field.PayloadSize + payloadDelta)), header);
                    edits.Add(new Edit(field.AbsoluteHeaderOffset, field.HeaderSize, newHeader));
                }
            }
            long delta = DeltaInside(edits, entry.PhysicalOffset, entry.PhysicalEnd);
            if (delta != 0 && entry.ParentIndex.HasValue)
            {
                int prefix = checked((int)entry.PhysicalOffset - 8);
                if (prefix < document.Header.DataStart ||
                    BinaryPrimitives.ReadUInt32LittleEndian(original[prefix..]) != entry.Id ||
                    BinaryPrimitives.ReadUInt32LittleEndian(original[(prefix + 4)..]) != entry.SerializedSize)
                    throw new NotSupportedException($"Object [{entry.Index}] has no confirmed inline ID/size prefix.");
                byte[] size = new byte[4];
                BinaryPrimitives.WriteUInt32LittleEndian(size, checked((uint)(entry.SerializedSize + delta)));
                edits.Add(new Edit(prefix + 4, 4, size));
            }
        }

        Edit[] ordered = edits.OrderBy(edit => edit.Start).ToArray();
        for (int index = 1; index < ordered.Length; index++)
            if (ordered[index].Start < ordered[index - 1].End)
                throw new InvalidDataException("Structural rewrite intervals overlap.");
        byte[] output = new byte[checked((int)(original.Length + ordered.Sum(edit => edit.Delta)))];
        int read = 0, write = 0;
        foreach (Edit edit in ordered)
        {
            original.Slice(read, edit.Start - read).CopyTo(output.AsSpan(write));
            write += edit.Start - read;
            edit.Data.CopyTo(output, write);
            write += edit.Data.Length;
            read = edit.End;
        }
        original[read..].CopyTo(output.AsSpan(write));

        long Map(long offset) => offset + ordered.Where(edit => edit.End <= offset).Sum(edit => edit.Delta);
        var entries = new SmoContainerEntry[document.Objects.Count];
        foreach (SmoObjectEntry entry in document.Objects)
        {
            long mappedStart = Map(entry.PhysicalOffset);
            long mappedEnd = Map(entry.PhysicalEnd);
            entries[entry.Index] = new(entry.Id, entry.RawName, entry.TypeHash,
                checked((uint)(mappedStart - document.Header.DataStart)), checked((uint)(mappedEnd - mappedStart)));
        }
        var envelope = new SmoContainerEnvelope(document.Header, entries,
            checked((int)(output.Length - document.Header.DataStart)));
        if (envelope.DataStart != document.Header.DataStart)
            throw new InvalidDataException("Leaf replacement must preserve the original envelope extent.");
        envelope.WritePrefix(output.AsSpan(0, envelope.DataStart));
        SmoDocument verified = SmoDocument.ParseOwned(output);
        if (verified.HasErrors || verified.Objects.Count != document.Objects.Count)
            throw new InvalidDataException("The rewritten SMO failed catalog verification.");
        foreach (SmoObjectEntry entry in ancestors)
            SmoObjectFieldReader.Read(verified, verified.Objects[entry.Index]);
        return output;
    }

    private static long DeltaInside(IEnumerable<Edit> edits, long start, long end) =>
        edits.Where(edit => edit.Start >= start && edit.End <= end).Sum(edit => edit.Delta);

    private sealed record Edit(int Start, int Length, byte[] Data)
    {
        public int End => checked(Start + Length);
        public long Delta => (long)Data.Length - Length;
    }
}
