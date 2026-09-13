using SmoViewer.Sparkplug;

namespace SmoViewer.Core;

/// <summary>
/// The three-bit size form stored in the high bits of a Sparkplug data-block
/// header byte.
/// </summary>
public enum SmoDataBlockSizeCode : byte
{
    Empty = 0,
    Fixed1 = 1,
    Fixed2 = 2,
    Fixed4 = 3,
    Fixed8 = 4,
    UInt8 = 5,
    UInt16 = 6,
    UInt32 = 7
}

/// <summary>A decoded Sparkplug data-block field header.</summary>
public readonly record struct SmoDataBlockHeader(
    int Offset,
    byte RawHeader,
    int FieldType,
    byte SizeCode,
    int HeaderSize,
    uint PayloadSize)
{
    public int EncodedFieldType => RawHeader & 0x1F;
    public bool HasExtendedFieldType => EncodedFieldType == 0x1F;
    public SmoDataBlockSizeCode SizeKind => (SmoDataBlockSizeCode)SizeCode;
    public int PayloadOffset => Offset + HeaderSize;
    public long PayloadEnd => (long)PayloadOffset + PayloadSize;
}

/// <summary>Decodes the compact field headers used by spDataBlockSerializer.</summary>
public static class SmoDataBlockReader
{
    public static bool TryReadHeader(
        ReadOnlySpan<byte> data,
        out SmoDataBlockHeader header) =>
        TryReadHeader(data, 0, out header);

    public static unsafe bool TryReadHeader(
        ReadOnlySpan<byte> data,
        int offset,
        out SmoDataBlockHeader header)
    {
        header = default;
        if ((uint)offset >= (uint)data.Length) return false;
        fixed (byte* input = data)
        {
            if (NativeMethods.spv_read_field(input + offset, (uint)(data.Length-offset), out var native) == 0)
                return false;
            byte raw = data[offset];
            header = new(offset, raw, checked((int)native.Field), (byte)(raw >> 5),
                checked((int)native.HeaderSize), native.PayloadSize);
            return true;
        }
    }

    public static bool TryReadHeader(
        ReadOnlySpan<byte> data,
        int offset,
        out int fieldType,
        out int headerSize,
        out uint payloadSize)
    {
        if (TryReadHeader(data, offset, out SmoDataBlockHeader header))
        {
            fieldType = header.FieldType;
            headerSize = header.HeaderSize;
            payloadSize = header.PayloadSize;
            return true;
        }

        fieldType = 0;
        headerSize = 0;
        payloadSize = 0;
        return false;
    }

}
