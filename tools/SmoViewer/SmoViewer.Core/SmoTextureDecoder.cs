using System.Buffers.Binary;
using System.Diagnostics.CodeAnalysis;

namespace SmoViewer.Core;

/// <summary>
/// Strict decoder for the confirmed PC <c>spTextureData</c> layouts. Every
/// read is limited to the exact object-directory interval.
/// </summary>
public static class SmoTextureDecoder
{
    private const int MaximumDimension = 16384;

    public static bool TryDecode(
        SmoDocument document,
        SmoObjectEntry entry,
        [NotNullWhen(true)] out SmoTexture? texture,
        out string error)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(entry);
        texture = null;
        error = string.Empty;

        string context = $"Object [{entry.Index}] \"{entry.Name}\"";
        if (entry.TypeHash != SmoClassIds.TextureData)
        {
            error = $"NOT_TEXTURE_DATA: {context} is " +
                    $"{SmoClassRegistry.GetDisplayName(entry.TypeHash)}, not spTextureData.";
            return false;
        }

        if (!entry.SignatureMatches)
        {
            error = $"TEXTURE_SIGNATURE_MISMATCH: {context} does not start with its " +
                    "catalog class ID followed by SBOO.";
            return false;
        }

        if (!entry.IsWithinDataSection ||
            entry.PhysicalOffset < 0 ||
            entry.PhysicalOffset > int.MaxValue ||
            entry.SerializedSize > int.MaxValue ||
            entry.PhysicalEnd > document.Data.Length)
        {
            error = $"TEXTURE_ENTRY_OUTSIDE_FILE: {context} has invalid physical bounds.";
            return false;
        }

        int objectOffset = (int)entry.PhysicalOffset;
        int objectSize = (int)entry.SerializedSize;
        ReadOnlySpan<byte> serialized = document.Data.Span.Slice(objectOffset, objectSize);
        if (!CanRead(serialized, 0x08, sizeof(uint)))
        {
            error = $"TEXTURE_HEADER_TRUNCATED: {context} is too small for a format field.";
            return false;
        }

        ushort formatCode = (ushort)(ReadUInt32(serialized, 0x08) & 0xFFFF);
        return formatCode switch
        {
            0x0EE3 or 0x32E3 or 0x43E3 => TryDecodeKnownLayout(
                entry,
                serialized,
                formatCode,
                SmoTextureLayout.Bgra,
                widthOffset: 0x24,
                heightOffset: 0x28,
                pixelOffset: 0x3D,
                pixelMarkerOffset: 0x3C,
                out texture,
                out error),
            0x54E3 => TryDecodeKnownLayout(
                entry,
                serialized,
                formatCode,
                SmoTextureLayout.Bgra,
                widthOffset: 0x24,
                heightOffset: 0x28,
                pixelOffset: 0x3D,
                pixelMarkerOffset: null,
                out texture,
                out error),
            0x29E3 => TryDecodeKnownLayout(
                entry,
                serialized,
                formatCode,
                SmoTextureLayout.Bgra,
                widthOffset: 0x28,
                heightOffset: 0x30,
                pixelOffset: 0x34,
                pixelMarkerOffset: null,
                out texture,
                out error),
            _ => Fail(
                out texture,
                out error,
                $"UNSUPPORTED_TEXTURE_FORMAT: {context} uses format 0x{formatCode:X4}.")
        };
    }

    private static bool TryDecodeKnownLayout(
        SmoObjectEntry entry,
        ReadOnlySpan<byte> serialized,
        ushort formatCode,
        SmoTextureLayout layout,
        int widthOffset,
        int heightOffset,
        int pixelOffset,
        int? pixelMarkerOffset,
        [NotNullWhen(true)] out SmoTexture? texture,
        out string error)
    {
        texture = null;
        error = string.Empty;
        string context = $"Object [{entry.Index}] \"{entry.Name}\"";

        if (!CanRead(serialized, widthOffset, sizeof(int)) ||
            !CanRead(serialized, heightOffset, sizeof(int)))
        {
            return Fail(
                out texture,
                out error,
                $"TEXTURE_HEADER_TRUNCATED: {context} is missing dimensions.");
        }

        int width = ReadInt32(serialized, widthOffset);
        int height = ReadInt32(serialized, heightOffset);
        if (width is <= 0 or > MaximumDimension ||
            height is <= 0 or > MaximumDimension)
        {
            return Fail(
                out texture,
                out error,
                $"INVALID_TEXTURE_DIMENSIONS: {context} declares {width}x{height} pixels.");
        }

        // The older 0x29E3 BGRA layout duplicates dimensions and row stride.
        // 0x54E3 stores BGRA pixels too, but uses the newer nested header.
        if (formatCode == 0x29E3)
        {
            if (!CanRead(serialized, 0x1B, sizeof(int)) ||
                !CanRead(serialized, 0x1F, sizeof(int)) ||
                !CanRead(serialized, 0x2C, sizeof(int)))
            {
                return Fail(
                    out texture,
                    out error,
                    $"TEXTURE_HEADER_TRUNCATED: {context} is missing BGRA metadata.");
            }

            int crossPlatformWidth = ReadInt32(serialized, 0x1B);
            int crossPlatformHeight = ReadInt32(serialized, 0x1F);
            if (crossPlatformWidth != width || crossPlatformHeight != height)
            {
                return Fail(
                    out texture,
                    out error,
                    $"TEXTURE_DIMENSION_MISMATCH: {context} declares {width}x{height}, " +
                    $"but its cross-platform fields declare " +
                    $"{crossPlatformWidth}x{crossPlatformHeight}.");
            }

            int rowStride = ReadInt32(serialized, 0x2C);
            if (rowStride != checked(width * 4))
            {
                return Fail(
                    out texture,
                    out error,
                    $"TEXTURE_ROW_STRIDE_MISMATCH: {context} declares stride " +
                    $"{rowStride}, expected {width * 4}.");
            }
        }

        if (pixelMarkerOffset is int markerOffset)
        {
            if (!CanRead(serialized, markerOffset, 1))
            {
                return Fail(
                    out texture,
                    out error,
                    $"TEXTURE_HEADER_TRUNCATED: {context} is missing the pixel " +
                    "serializer marker.");
            }

            if (serialized[markerOffset] != 0)
            {
                return Fail(
                    out texture,
                    out error,
                    $"TEXTURE_PIXEL_MARKER_MISMATCH: {context} has pixel " +
                    $"serializer marker 0x{serialized[markerOffset]:X2}; expected 0x00.");
            }
        }

        long pixelBytesValue = (long)width * height * 4;
        if (pixelBytesValue > int.MaxValue)
        {
            return Fail(
                out texture,
                out error,
                $"TEXTURE_PIXEL_BUFFER_TOO_LARGE: {context} needs {pixelBytesValue} bytes.");
        }

        int pixelBytes = (int)pixelBytesValue;
        if (!CanRead(serialized, pixelOffset, pixelBytes))
        {
            return Fail(
                out texture,
                out error,
                $"TEXTURE_PIXEL_BUFFER_OUTSIDE_OBJECT: {context} pixel data crosses " +
                "the exact serialized object boundary.");
        }

        if (!HasExpectedBlockSizes(serialized, (uint)pixelBytes, formatCode))
        {
            return Fail(
                out texture,
                out error,
                $"TEXTURE_BLOCK_SIZE_MISMATCH: {context} nested serializer sizes do " +
                "not contain the complete pixel buffer.");
        }

        ReadOnlySpan<byte> source = serialized.Slice(pixelOffset, pixelBytes);
        byte[] bgra32 = new byte[pixelBytes];
        for (int offset = 0; offset < source.Length; offset += 4)
        {
            if (layout == SmoTextureLayout.Abgr)
            {
                bgra32[offset] = source[offset + 1];
                bgra32[offset + 1] = source[offset + 2];
                bgra32[offset + 2] = source[offset + 3];
                bgra32[offset + 3] = source[offset];
            }
            else
            {
                source.Slice(offset, 4).CopyTo(bgra32.AsSpan(offset, 4));
            }
        }

        texture = new SmoTexture(
            entry.Index,
            entry.Name,
            formatCode,
            width,
            height,
            layout,
            bgra32);
        return true;
    }

    private static bool HasExpectedBlockSizes(
        ReadOnlySpan<byte> data,
        uint pixelBytes,
        ushort formatCode)
    {
        (int Offset, byte RawHeader, uint Tail)[] fields = formatCode switch
        {
            0x54E3 =>
            [
                (0x08, 0xE3, 0x54),
                (0x19, 0xE1, 0x42),
                (0x1E, 0xE0, 0x1A)
            ],
            0x0EE3 =>
            [
                // 0x0EE3 stores a complete mip chain. The first nested E0
                // block is the ordinary full-resolution BGRA level; later
                // blocks contain the smaller mip levels and are intentionally
                // left for the renderer to generate/filter as needed.
                (0x08, 0xE3, 0x0E),
                (0x19, 0xE1, 0x20),
                (0x1E, 0xE0, 0x1A)
            ],
            0x32E3 or 0x43E3 =>
            [
                (0x08, 0xE3, 0x32),
                (0x19, 0xE1, 0x20),
                (0x1E, 0xE0, 0x1A)
            ],
            0x29E3 =>
            [
                (0x08, 0xE3, 0x29),
                (0x10, 0xE1, 0x20),
                (0x15, 0xE0, 0x1A)
            ],
            _ => []
        };

        foreach ((int offset, byte rawHeader, uint tail) in fields)
        {
            if (!SmoDataBlockReader.TryReadHeader(
                    data,
                    offset,
                    out SmoDataBlockHeader header) ||
                header.RawHeader != rawHeader ||
                header.PayloadSize < checked(pixelBytes + tail))
            {
                return false;
            }
        }

        return true;
    }

    private static bool Fail(
        out SmoTexture? texture,
        out string error,
        string message)
    {
        texture = null;
        error = message;
        return false;
    }

    private static bool CanRead(ReadOnlySpan<byte> data, int offset, int length) =>
        offset >= 0 && length >= 0 && offset <= data.Length - length;

    private static int ReadInt32(ReadOnlySpan<byte> data, int offset) =>
        BinaryPrimitives.ReadInt32LittleEndian(data.Slice(offset, sizeof(int)));

    private static uint ReadUInt32(ReadOnlySpan<byte> data, int offset) =>
        BinaryPrimitives.ReadUInt32LittleEndian(data.Slice(offset, sizeof(uint)));
}
