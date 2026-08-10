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
        if (width <= 0 || height <= 0 || pixels.Length != checked(width * height * 4))
            throw new ArgumentException("Invalid BGRA32 texture dimensions or buffer length.");

        using var output = new MemoryStream();
        output.Write(Signature);
        Span<byte> header = stackalloc byte[13];
        BinaryPrimitives.WriteUInt32BigEndian(header, (uint)width);
        BinaryPrimitives.WriteUInt32BigEndian(header[4..], (uint)height);
        header[8] = 8;
        header[9] = 6;
        WriteChunk(output, "IHDR", header);

        using var raw = new MemoryStream(checked((width * 4 + 1) * height));
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
                raw.WriteByte(pixels[source + 3]);
            }
        }

        using var compressed = new MemoryStream();
        using (var zlib = new ZLibStream(compressed, CompressionLevel.SmallestSize, true))
            zlib.Write(raw.GetBuffer(), 0, checked((int)raw.Length));
        WriteChunk(output, "IDAT", compressed.ToArray());
        WriteChunk(output, "IEND", []);
        return output.ToArray();
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
