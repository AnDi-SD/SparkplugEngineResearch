using System.Buffers.Binary;
using System.Numerics;
using SmoViewer.Sparkplug;

namespace SmoViewer.Core;

/// <summary>
/// Editing adapter over the reconstructed BGRA writers. Lossless
/// replacement of surrounding opaque fields remains an editor operation.
/// </summary>
public static class SmoTextureDataWriter
{
    // The native TextureData inspection bounds all stored BGRA pixels, not
    // just the top level (PcTextureStorageBudget.h / native section reader).
    private const long BgraInspectionLimit = 3000L * 3000 * 4;

    /// <summary>Creates an evidenced embedded PC BGRA leaf for a new resource.</summary>
    public static byte[] CreateBgraObject(int width, int height, ReadOnlySpan<byte> pixels) =>
        CreateBgraObject(width, height, pixels, alphaWeightedMips: false);

    public static byte[] CreateBgraObject(int width, int height, ReadOnlySpan<byte> pixels, bool alphaWeightedMips)
    {
        ValidatePixels(width, height, pixels.Length);
        if (!alphaWeightedMips || !ShouldAuthorMips(width, height, pixels))
            return SerializeBgra(width, height, pixels, 1, 0);

        byte[] serialized = SerializeBgra(width, height, pixels, 1, 0);
        byte[] mips = SerializeMipSection(width, height, pixels, 1, appendLowerMips: true);
        return RewriteObjectFields(serialized, [3, 1], mips);
    }

    private static unsafe byte[] SerializeBgra(int width, int height, ReadOnlySpan<byte> pixels, byte field1C, uint kind)
    {
        SerializedBytesHandle owner;
        fixed (byte* input = pixels)
            owner = new SerializedBytesHandle(NativeMethods.Check(NativeMethods.spv_texture_write_bgra(
                input, checked((uint)pixels.Length), checked((uint)width), checked((uint)height), field1C, kind)));
        using (owner)
        {
            NativeMethods.Check(NativeMethods.spv_serialized_bytes_size(owner, out uint count));
            byte[] result = new byte[checked((int)count)];
            fixed (byte* output = result)
                NativeMethods.Check(NativeMethods.spv_serialized_bytes_copy(owner, output, count));
            return result;
        }
    }

    public static bool CanReplace(SmoTextureDataInfo texture, out string reason)
    {
        if (texture.EditingIssue is not null)
        {
            reason = texture.EditingIssue;
            return false;
        }
        SmoTextureRepresentationData? data = texture.SelectedRepresentation;
        if (texture.SourceKind != SmoTextureSourceKind.Embedded || texture.CrossPlatform is not null ||
            data?.Kind != SmoTextureRepresentationKind.Direct3DBgra32)
            reason = "Запись проверена для встроенной PC Direct3D BGRA32-текстуры без второй платформенной копии.";
        else if (data.MipLevels.Count != 1 && !HasEditableFullMipChain(texture))
            reason = "Замена этой формы цепочки mip-уровней не поддерживается.";
        else
        {
            reason = string.Empty;
            return true;
        }
        return false;
    }

    public static byte[] ReplaceBgra(
        SmoDocument document, int objectIndex, int width, int height, ReadOnlySpan<byte> pixels)
    {
        byte[] replacement = BuildReplacementObjectBgra(document, objectIndex, width, height, pixels);
        byte[] output = SmoLeafObjectReplacer.Replace(document, objectIndex, replacement);
        SmoDocument after = SmoDocument.ParseOwned(output);
        if (!SmoTextureDataDecoder.TryDecode(after, after.Objects[objectIndex], out var decoded, out string error))
            throw new InvalidDataException(error);
        var actual = decoded.SelectedRepresentation!.MipLevels[0];
        if (actual.Width != width || actual.Height != height || !actual.PixelData.Span.SequenceEqual(pixels) ||
            (decoded.MipLevelCount > 1 && !HasEditableFullMipChain(decoded)))
            throw new InvalidDataException("Texture pixels failed post-write verification.");
        return output;
    }

    /// <summary>
    /// Builds one serialized leaf for graph import without copying the entire
    /// source document. The caller must insert it through a structural graph
    /// writer and verify the resulting document before installing any output.
    /// Supported representations are the same as <see cref="CanReplace"/>.
    /// </summary>
    public static byte[] BuildReplacementObjectBgra(
        SmoDocument document, int objectIndex, int width, int height, ReadOnlySpan<byte> pixels) =>
        BuildReplacementObjectBgra(document, objectIndex, width, height, pixels, alphaWeightedMips: false);

    public static byte[] BuildReplacementObjectBgra(
        SmoDocument document, int objectIndex, int width, int height, ReadOnlySpan<byte> pixels, bool alphaWeightedMips)
    {
        ValidatePixels(width, height, pixels.Length);
        if ((uint)objectIndex >= (uint)document.Objects.Count)
            throw new ArgumentOutOfRangeException(nameof(objectIndex));
        SmoObjectEntry entry = document.Objects[objectIndex];
        if (!SmoTextureDataDecoder.TryDecode(document, entry, out var texture, out string error))
            throw new InvalidDataException(error);
        if (!CanReplace(texture, out string reason))
            throw new NotSupportedException(reason);
        ReadOnlyMemory<byte> serialized = document.Data.Slice(
            checked((int)entry.PhysicalOffset), checked((int)entry.SerializedSize));
        byte field1C = texture.SelectedRepresentation!.NativeField1C;
        if (HasEditableFullMipChain(texture))
            return RewriteObjectFields(serialized, [3, 1], SerializeMipSection(
                width, height, pixels, field1C, CanStoreFullMipChain(width, height)));
        if (alphaWeightedMips && texture.HasCanonicalNativeMipFields && ShouldAuthorMips(width, height, pixels))
            return RewriteObjectFields(serialized, [3, 1], SerializeMipSection(
                width, height, pixels, field1C, appendLowerMips: true));

        return RewriteObjectFields(serialized, [3, 1, 0], SerializeBgra(width, height, pixels, field1C, 1));
    }

    private static byte[] RewriteObjectFields(ReadOnlyMemory<byte> serialized, int[] path, byte[] value)
    {
        byte[] fields = RewriteField(serialized[8..], path, 0, value);
        byte[] replacement = new byte[checked(8 + fields.Length)];
        serialized.Span[..8].CopyTo(replacement);
        fields.CopyTo(replacement, 8);
        return replacement;
    }

    private static bool ShouldAuthorMips(int width, int height, ReadOnlySpan<byte> pixels)
    {
        if (!CanStoreFullMipChain(width, height) || (width == 1 && height == 1))
            return false;
        for (int i = 3; i < pixels.Length; i += 4)
            if (pixels[i] != byte.MaxValue)
                return true;
        return false;
    }

    private static bool CanStoreFullMipChain(int width, int height)
    {
        if (!BitOperations.IsPow2(width) || !BitOperations.IsPow2(height))
            return false;

        long bytes = (long)width * height * 4;
        while (width > 1 || height > 1)
        {
            width = Math.Max(1, width >> 1);
            height = Math.Max(1, height >> 1);
            bytes += (long)width * height * 4;
        }
        return bytes <= BgraInspectionLimit;
    }

    private static bool HasEditableFullMipChain(SmoTextureDataInfo texture) =>
        texture.HasCanonicalNativeMipFields &&
        texture.SelectedRepresentation is { MipLevels.Count: > 1 } native &&
        BitOperations.IsPow2(native.Width) && BitOperations.IsPow2(native.Height) &&
        native.MipLevels.Count == BitOperations.Log2((uint)Math.Max(native.Width, native.Height)) + 1;

    private static byte[] SerializeMipSection(int width, int height, ReadOnlySpan<byte> pixels, byte field1C, bool appendLowerMips)
    {
        byte[] first = SerializeBgra(width, height, pixels, field1C, 1);
        using var output = new MemoryStream();
        output.Write(SmoDataBlockWriter.BuildReservedHeader(0, checked((uint)first.Length)));
        output.Write(first);
        Span<byte> rowHeader = stackalloc byte[3 * sizeof(uint)];
        while (appendLowerMips && (width > 1 || height > 1))
        {
            byte[] next = DownsampleAlphaWeighted(width, height, pixels);
            width = Math.Max(1, width >> 1);
            height = Math.Max(1, height >> 1);
            BinaryPrimitives.WriteUInt32LittleEndian(rowHeader, (uint)width);
            BinaryPrimitives.WriteUInt32LittleEndian(rowHeader[4..], (uint)width * 4);
            BinaryPrimitives.WriteUInt32LittleEndian(rowHeader[8..], (uint)height);
            output.Write(SmoDataBlockWriter.BuildReservedHeader(1, checked((uint)(rowHeader.Length + next.Length))));
            output.Write(rowHeader);
            output.Write(next);
            pixels = next;
        }
        output.WriteByte(0);
        return output.ToArray();
    }

    private static byte[] DownsampleAlphaWeighted(int width, int height, ReadOnlySpan<byte> pixels)
    {
        int footprintWidth = width > 1 ? 2 : 1;
        int footprintHeight = height > 1 ? 2 : 1;
        int nextWidth = width / footprintWidth;
        int nextHeight = height / footprintHeight;
        int sampleCount = footprintWidth * footprintHeight;
        byte[] next = new byte[checked(nextWidth * nextHeight * 4)];
        for (int y = 0; y < nextHeight; y++)
        for (int x = 0; x < nextWidth; x++)
        {
            int blue = 0, green = 0, red = 0, alpha = 0;
            for (int row = 0; row < footprintHeight; row++)
            for (int column = 0; column < footprintWidth; column++)
            {
                int index = ((y * footprintHeight + row) * width + x * footprintWidth + column) * 4;
                int sampleAlpha = pixels[index + 3];
                blue += pixels[index] * sampleAlpha;
                green += pixels[index + 1] * sampleAlpha;
                red += pixels[index + 2] * sampleAlpha;
                alpha += sampleAlpha;
            }
            int destination = (y * nextWidth + x) * 4;
            if (alpha != 0)
            {
                next[destination] = (byte)((blue + alpha / 2) / alpha);
                next[destination + 1] = (byte)((green + alpha / 2) / alpha);
                next[destination + 2] = (byte)((red + alpha / 2) / alpha);
            }
            next[destination + 3] = (byte)((alpha + sampleCount / 2) / sampleCount);
        }
        return next;
    }

    private static byte[] RewriteField(
        ReadOnlyMemory<byte> fields, int[] path, int depth, byte[] replacement)
    {
        using var output = new MemoryStream();
        bool changed = false;
        int offset = 0;
        while (offset < fields.Length)
        {
            if (!SmoDataBlockReader.TryReadHeader(fields.Span, offset, out var header))
                throw new InvalidDataException("Invalid texture field stream.");
            int end = checked((int)header.PayloadEnd);
            if (header.FieldType == path[depth] && header.PayloadSize > 0)
            {
                if (changed)
                    throw new InvalidDataException("Ambiguous texture serializer field.");
                var payload = fields.Slice(header.PayloadOffset, checked((int)header.PayloadSize));
                byte[] value = depth == path.Length - 1
                    ? replacement : RewriteField(payload, path, depth + 1, replacement);
                output.Write(SmoDataBlockWriter.BuildHeader(header.FieldType, checked((uint)value.Length), header));
                output.Write(value);
                changed = true;
            }
            else
                output.Write(fields.Span.Slice(offset, end - offset));
            offset = end;
        }
        if (!changed)
            throw new InvalidDataException("Expected texture serializer field is missing.");
        return output.ToArray();
    }

    private static void ValidatePixels(int width, int height, int length)
    {
        if (width is < 1 or > 16384 || height is < 1 or > 16384 ||
            (long)width * height * 4 != length)
            throw new ArgumentException("BGRA dimensions and byte length must agree (1..16384 per side).");
    }
}
