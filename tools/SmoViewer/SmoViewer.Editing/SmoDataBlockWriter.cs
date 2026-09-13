using SmoViewer.Sparkplug;

namespace SmoViewer.Core;

/// <summary>Field/header adapter over the shared Sparkplug writer.</summary>
public static class SmoDataBlockWriter
{
    public static byte[] BuildField(int fieldType, ReadOnlySpan<byte> payload)
    {
        byte[] header = BuildHeader(fieldType, checked((uint)payload.Length));
        byte[] result = new byte[checked(header.Length + payload.Length)];
        header.CopyTo(result, 0);
        payload.CopyTo(result.AsSpan(header.Length));
        return result;
    }

    /// <summary>
    /// Retains a supported observed size reservation when it fits. Zero size
    /// without an explicit nonempty reservation is the legacy shorthand for
    /// the original section terminator. Rare unsupported wire forms fail.
    /// </summary>
    public static byte[] BuildHeader(int fieldType, uint payloadSize, SmoDataBlockHeader? preferred = null)
    {
        if (fieldType is < 0 or > byte.MaxValue) throw new ArgumentOutOfRangeException(nameof(fieldType));
        uint code = uint.MaxValue, extended = 0;
        if (preferred is SmoDataBlockHeader original && original.FieldType == fieldType)
        {
            code = original.SizeCode;
            extended = original.HasExtendedFieldType ? 1u : 0u;
        }
        Span<byte> output = stackalloc byte[6];
        int count = EncodeIntoSpan(output, fieldType, payloadSize, code, extended);
        return output[..count].ToArray();
    }

    /// <summary>Builds a nonempty header in the requested variable-size reservation, without widening it.</summary>
    public static byte[] BuildReservedHeader(int fieldType, uint payloadSize,
        SmoDataBlockSizeCode reservation = SmoDataBlockSizeCode.UInt32)
    {
        Span<byte> output = stackalloc byte[6];
        int count = EncodeReservedIntoSpan(output, fieldType, payloadSize, reservation);
        return output[..count].ToArray();
    }

    /// <summary>Retains the exact supported header shape observed by the shared reader.</summary>
    public static byte[] BuildReservedHeader(SmoDataBlockHeader original, uint payloadSize)
    {
        Span<byte> output = stackalloc byte[6];
        _ = EncodeObservedIntoSpan(output, original, original.PayloadSize);
        int count = EncodeObservedIntoSpan(output, original, payloadSize);
        return output[..count].ToArray();
    }

    /// <summary>
    /// Patches a relocated copy of an observed header. All validation and both
    /// native encodes complete before destination changes; payload bytes are not inspected.
    /// </summary>
    public static void PatchReservedHeader(Span<byte> destination, int mappedHeaderOffset,
        SmoDataBlockHeader original, uint payloadSize)
    {
        Span<byte> before = stackalloc byte[6];
        int beforeCount = EncodeObservedIntoSpan(before, original, original.PayloadSize);
        if ((uint)mappedHeaderOffset > (uint)destination.Length ||
            beforeCount > destination.Length - mappedHeaderOffset)
            throw new ArgumentOutOfRangeException(nameof(mappedHeaderOffset), "Reserved header is outside the destination.");
        Span<byte> target = destination.Slice(mappedHeaderOffset, beforeCount);
        if (!target.SequenceEqual(before[..beforeCount]))
            throw new InvalidDataException("Reserved header bytes do not match the observed source header.");
        Span<byte> after = stackalloc byte[6];
        int afterCount = EncodeObservedIntoSpan(after, original, payloadSize);
        after[..afterCount].CopyTo(target);
    }

    private static int EncodeObservedIntoSpan(Span<byte> output, SmoDataBlockHeader original, uint payloadSize)
    {
        if (original.Offset < 0 || original.HeaderSize is < 2 or > 6 || original.PayloadSize == 0 ||
            (original.RawHeader >> 5) != original.SizeCode ||
            (original.HasExtendedFieldType && original.FieldType < 31))
            throw new NotSupportedException("Reserved header has an unsupported observed shape.");
        int count = EncodeReservedIntoSpan(output, original.FieldType, payloadSize, original.SizeKind);
        if (count != original.HeaderSize || output[0] != original.RawHeader)
            throw new InvalidDataException("Reserved header metadata does not match its canonical original encoding.");
        return count;
    }

    private static int EncodeReservedIntoSpan(Span<byte> output, int fieldType, uint payloadSize,
        SmoDataBlockSizeCode reservation)
    {
        if (fieldType is < 0 or > byte.MaxValue) throw new ArgumentOutOfRangeException(nameof(fieldType));
        if (fieldType == 31 || payloadSize == 0 ||
            reservation is not (SmoDataBlockSizeCode.UInt8 or SmoDataBlockSizeCode.UInt16 or SmoDataBlockSizeCode.UInt32))
            throw new NotSupportedException("Reserved headers require a supported ID, nonempty payload and UInt8/16/32 reservation.");
        int count = EncodeIntoSpan(output, fieldType, payloadSize, (uint)reservation, 0);
        // Capacity remains the existing native ABI's host guard. Its ordinary
        // fallback is allowed by BuildHeader, but cannot resize this reservation.
        if ((output[0] >> 5) != (byte)reservation)
            throw new OverflowException("Payload size does not fit the existing field-header reservation.");
        return count;
    }

    private static unsafe int EncodeIntoSpan(Span<byte> output, int fieldType, uint payloadSize,
        uint preferredCode, uint preferExtended)
    {
        fixed (byte* bytes = output)
        {
            NativeMethods.Check(NativeMethods.spv_write_field_header((uint)fieldType, payloadSize,
                preferredCode, preferExtended, bytes, checked((uint)output.Length), out uint count));
            if (count is 0 or > 6 || count > output.Length)
                throw new InvalidDataException("Shared header writer returned an invalid output extent.");
            return checked((int)count);
        }
    }
}
