using System.Buffers.Binary;
using System.IO.Compression;
using System.Text;

namespace SmoExporter.Core;

internal static class PngEncoder
{
    private static ReadOnlySpan<byte> Signature =>
        [137, 80, 78, 71, 13, 10, 26, 10];

    public static byte[] EncodeBgra32(int width, int height, ReadOnlySpan<byte> pixels)
    {
        ValidateBgra32(width, height, pixels);
        bool hasAlpha = HasTransparency(pixels);
        int channels = hasAlpha ? 4 : 3;

        using var raw = new MemoryStream(checked((width * channels + 1) * height));
        for (int y = 0; y < height; y++)
        {
            raw.WriteByte(0);
            int row = y * width * 4;
            for (int x = 0; x < width; x++)
            {
                int source = row + x * 4;
                raw.WriteByte(pixels[source + 2]);
                raw.WriteByte(pixels[source + 1]);
                raw.WriteByte(pixels[source]);
                if (hasAlpha)
                    raw.WriteByte(pixels[source + 3]);
            }
        }

        return EncodePng(width, height, hasAlpha ? (byte)6 : (byte)2, raw);
    }

    public static byte[] EncodeBgr24(int width, int height, ReadOnlySpan<byte> pixels)
    {
        ValidateBgra32(width, height, pixels);
        using var raw = new MemoryStream(checked((width * 3 + 1) * height));
        for (int y = 0; y < height; y++)
        {
            raw.WriteByte(0);
            int row = y * width * 4;
            for (int x = 0; x < width; x++)
            {
                int source = row + x * 4;
                raw.WriteByte(pixels[source + 2]);
                raw.WriteByte(pixels[source + 1]);
                raw.WriteByte(pixels[source]);
            }
        }

        return EncodePng(width, height, 2, raw);
    }

    public static byte[]? EncodeOpacityMaskBgra32(
        int width, int height, ReadOnlySpan<byte> pixels)
    {
        ValidateBgra32(width, height, pixels);
        if (!HasTransparency(pixels))
            return null;

        using var raw = new MemoryStream(checked((width + 1) * height));
        for (int y = 0; y < height; y++)
        {
            raw.WriteByte(0);
            int row = y * width * 4;
            for (int x = 0; x < width; x++)
                raw.WriteByte(pixels[row + x * 4 + 3]);
        }

        return EncodePng(width, height, 0, raw);
    }

    private static byte[] EncodePng(
        int width, int height, byte colorType, MemoryStream raw)
    {
        using var output = new MemoryStream();
        output.Write(Signature);
        Span<byte> header = stackalloc byte[13];
        header.Clear();
        BinaryPrimitives.WriteUInt32BigEndian(header, (uint)width);
        BinaryPrimitives.WriteUInt32BigEndian(header[4..], (uint)height);
        header[8] = 8;
        header[9] = colorType;
        WriteChunk(output, "IHDR", header);

        using var compressed = new MemoryStream();
        using (var zlib = new ZLibStream(compressed, CompressionLevel.SmallestSize, true))
            zlib.Write(raw.GetBuffer(), 0, checked((int)raw.Length));
        WriteChunk(output, "IDAT", compressed.ToArray());
        WriteChunk(output, "IEND", []);
        return output.ToArray();
    }

    private static void ValidateBgra32(
        int width, int height, ReadOnlySpan<byte> pixels)
    {
        if (width <= 0 || height <= 0 || pixels.Length != checked(width * height * 4))
            throw new ArgumentException("Invalid BGRA32 texture dimensions or buffer length.");
    }

    private static bool HasTransparency(ReadOnlySpan<byte> pixels)
    {
        for (int index = 3; index < pixels.Length; index += 4)
        {
            if (pixels[index] < byte.MaxValue)
                return true;
        }
        return false;
    }

    private static void WriteChunk(Stream output, string type, ReadOnlySpan<byte> data)
    {
        Span<byte> size = stackalloc byte[4];
        BinaryPrimitives.WriteUInt32BigEndian(size, (uint)data.Length);
        output.Write(size);
        byte[] typeBytes = Encoding.ASCII.GetBytes(type);
        output.Write(typeBytes);
        output.Write(data);
        byte[] checksumInput = new byte[typeBytes.Length + data.Length];
        typeBytes.CopyTo(checksumInput, 0);
        data.CopyTo(checksumInput.AsSpan(typeBytes.Length));
        BinaryPrimitives.WriteUInt32BigEndian(size, Crc32(checksumInput));
        output.Write(size);
    }

    private static uint Crc32(ReadOnlySpan<byte> data)
    {
        uint crc = 0xFFFFFFFF;
        foreach (byte value in data)
        {
            crc ^= value;
            for (int bit = 0; bit < 8; bit++)
                crc = (crc >> 1) ^ (0xEDB88320u & (uint)-(int)(crc & 1));
        }
        return ~crc;
    }
}
