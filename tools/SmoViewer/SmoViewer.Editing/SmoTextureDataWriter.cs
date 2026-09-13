using SmoViewer.Sparkplug;

namespace SmoViewer.Core;

/// <summary>
/// Editing adapter over the reconstructed single-mip BGRA writers. Lossless
/// replacement of surrounding opaque fields remains an editor operation.
/// </summary>
public static class SmoTextureDataWriter
{
    /// <summary>Creates an evidenced embedded PC BGRA leaf for a new resource.</summary>
    public static byte[] CreateBgraObject(int width, int height, ReadOnlySpan<byte> pixels)
    {
        ValidatePixels(width, height, pixels.Length);
        return SerializeBgra(width, height, pixels, 1, 0);
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
        else if (data.MipLevels.Count != 1)
            reason = "Замена текстуры с явно записанной цепочкой mip-уровней пока не поддерживается.";
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
        var actual = decoded.SelectedRepresentation!.MipLevels.Single();
        if (actual.Width != width || actual.Height != height || !actual.PixelData.Span.SequenceEqual(pixels))
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
        SmoDocument document, int objectIndex, int width, int height, ReadOnlySpan<byte> pixels)
    {
        ValidatePixels(width, height, pixels.Length);
        if ((uint)objectIndex >= (uint)document.Objects.Count)
            throw new ArgumentOutOfRangeException(nameof(objectIndex));
        SmoObjectEntry entry = document.Objects[objectIndex];
        if (!SmoTextureDataDecoder.TryDecode(document, entry, out var texture, out string error))
            throw new InvalidDataException(error);
        if (!CanReplace(texture, out string reason))
            throw new NotSupportedException(reason);
        int[] path = [3, 1, 0];
        ReadOnlyMemory<byte> serialized = document.Data.Slice(
            checked((int)entry.PhysicalOffset), checked((int)entry.SerializedSize));
        byte[] mip = SerializeBgra(width, height, pixels, texture.SelectedRepresentation!.NativeField1C, 1);
        byte[] fields = RewriteField(serialized[8..], path, 0, _ => mip);
        byte[] replacement = new byte[checked(8 + fields.Length)];
        serialized.Span[..8].CopyTo(replacement);
        fields.CopyTo(replacement, 8);
        return replacement;
    }

    private static byte[] RewriteField(
        ReadOnlyMemory<byte> fields, int[] path, int depth, Func<ReadOnlyMemory<byte>, byte[]> replace)
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
                    ? replace(payload) : RewriteField(payload, path, depth + 1, replace);
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
