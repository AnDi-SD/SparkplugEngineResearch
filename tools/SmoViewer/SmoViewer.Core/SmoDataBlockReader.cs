using System.Buffers.Binary;

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

    public static bool TryReadHeader(
        ReadOnlySpan<byte> data,
        int offset,
        out SmoDataBlockHeader header)
    {
        header = default;
        if (!CanRead(data, offset, 1))
            return false;

        byte rawHeader = data[offset];
        int fieldType = rawHeader & 0x1F;
        byte sizeCode = (byte)(rawHeader >> 5);
        int headerSize = 1;

        if (fieldType == 0x1F)
        {
            if (!CanRead(data, offset + headerSize, 1))
                return false;

            fieldType = data[offset + headerSize];
            headerSize++;
        }

        uint payloadSize;
        switch ((SmoDataBlockSizeCode)sizeCode)
        {
            case SmoDataBlockSizeCode.Empty:
                payloadSize = 0;
                break;
            case SmoDataBlockSizeCode.Fixed1:
                payloadSize = 1;
                break;
            case SmoDataBlockSizeCode.Fixed2:
                payloadSize = 2;
                break;
            case SmoDataBlockSizeCode.Fixed4:
                payloadSize = 4;
                break;
            case SmoDataBlockSizeCode.Fixed8:
                payloadSize = 8;
                break;
            case SmoDataBlockSizeCode.UInt8:
                if (!CanRead(data, offset + headerSize, 1))
                    return false;
                payloadSize = data[offset + headerSize];
                headerSize++;
                break;
            case SmoDataBlockSizeCode.UInt16:
                if (!CanRead(data, offset + headerSize, sizeof(ushort)))
                    return false;
                payloadSize = BinaryPrimitives.ReadUInt16LittleEndian(
                    data.Slice(offset + headerSize, sizeof(ushort)));
                headerSize += sizeof(ushort);
                break;
            case SmoDataBlockSizeCode.UInt32:
                if (!CanRead(data, offset + headerSize, sizeof(uint)))
                    return false;
                payloadSize = BinaryPrimitives.ReadUInt32LittleEndian(
                    data.Slice(offset + headerSize, sizeof(uint)));
                headerSize += sizeof(uint);
                break;
            default:
                return false;
        }

        long payloadOffset = (long)offset + headerSize;
        long payloadEnd = payloadOffset + payloadSize;
        if (payloadOffset < 0 || payloadEnd > data.Length)
            return false;

        header = new SmoDataBlockHeader(
            offset,
            rawHeader,
            fieldType,
            sizeCode,
            headerSize,
            payloadSize);
        return true;
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

    private static bool CanRead(ReadOnlySpan<byte> data, int offset, int length) =>
        offset >= 0 && length >= 0 && offset <= data.Length - length;
}
