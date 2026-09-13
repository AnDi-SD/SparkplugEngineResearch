using SmoViewer.Sparkplug;

namespace SmoViewer.Core;

// Common host ownership/byte-copy boundary for typed, same-length scalar edits.
// Assignment scope and scalar serialization remain in the actual native classes.
internal static class SmoScalarTemplateEdit
{
    internal static ReadOnlySpan<byte> Source(SmoDocument document, SmoObjectEntry entry,
        uint classId, uint minimumSize)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(entry);
        if (entry.TypeHash != classId || !entry.IsWithinDataSection || !entry.SignatureMatches ||
            entry.PhysicalOffset < 0 || entry.SerializedSize < minimumSize ||
            entry.PhysicalEnd > document.Data.Length || entry.SerializedSize > int.MaxValue ||
            (uint)entry.Index >= (uint)document.Objects.Count || !ReferenceEquals(document.Objects[entry.Index], entry))
            throw new InvalidDataException("Scalar edit requires a complete catalogued object of the requested class.");
        return document.Data.Span.Slice(checked((int)entry.PhysicalOffset), checked((int)entry.SerializedSize));
    }

    internal static unsafe byte[] CopyResult(ReadOnlySpan<byte> source, SerializedBytesHandle owner)
    {
        using (owner)
        {
            NativeMethods.Check(NativeMethods.spv_serialized_bytes_size(owner, out uint size));
            if (size != source.Length - 8)
                throw new InvalidDataException("Scalar edit changed the existing object extent.");
            byte[] result = source.ToArray();
            fixed (byte* output = result.AsSpan(8))
                NativeMethods.Check(NativeMethods.spv_serialized_bytes_copy(owner, output, size));
            return result;
        }
    }
}
