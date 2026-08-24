using System.Buffers.Binary;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;
using SixLabors.ImageSharp.Processing;
using SmoViewer.Core;

namespace SmoImporter.Core;

public static class FixedSizeTextureWriter
{
    private const int SerializedTextureDataMarkerOffset = 0x3C;

    public static byte[] ReplaceRgb(byte[] smoData, int textureIndex, ReadOnlySpan<byte> imageData)
    {
        byte[] output = (byte[])smoData.Clone();
        SMOTextureTool.Core.SmoDocument document = SMOTextureTool.Core.SmoDocument.Parse(output);
        SMOTextureTool.Core.TextureInfo texture = document.Textures.Single(item => item.Index == textureIndex);
        EnsureSupportedTexture(texture);

        using Image<Rgba32> image = Image.Load<Rgba32>(imageData);
        if (image.Width != texture.Width || image.Height != texture.Height)
            image.Mutate(context => context.Resize(texture.Width, texture.Height));

        int offset = texture.PixelDataOffset;
        image.ProcessPixelRows(accessor =>
        {
            for (int y = 0; y < texture.Height; y++)
            {
                ReadOnlySpan<Rgba32> row = accessor.GetRowSpan(y);
                foreach (Rgba32 pixel in row)
                {
                    // The serialized data marker is at block + 0x3C. PixelDataOffset
                    // points one byte later, at the first byte of the BGRA payload.
                    output[offset] = pixel.B;
                    output[offset + 1] = pixel.G;
                    output[offset + 2] = pixel.R;
                    // Preserve the host alpha at +3.
                    offset += 4;
                }
            }
        });

        EnsureSerializerMarker(output, texture);
        return output;
    }

    /// <summary>
    /// Replaces the complete BGRA payload of an existing fixed-size texture.
    /// The serializer marker at block +0x3C is kept outside the pixel span and
    /// is verified after the write. Callers are responsible for enabling alpha
    /// blending on every material that consumes a texture containing alpha.
    /// </summary>
    public static byte[] ReplaceRgba(
        byte[] smoData,
        int textureIndex,
        ReadOnlySpan<byte> imageData)
    {
        byte[] output = (byte[])smoData.Clone();
        SMOTextureTool.Core.SmoDocument document = SMOTextureTool.Core.SmoDocument.Parse(output);
        SMOTextureTool.Core.TextureInfo texture = document.Textures.Single(item => item.Index == textureIndex);
        EnsureSupportedTexture(texture);

        using Image<Rgba32> image = Image.Load<Rgba32>(imageData);
        if (image.Width != texture.Width || image.Height != texture.Height)
            image.Mutate(context => context.Resize(texture.Width, texture.Height));
        int offset = texture.PixelDataOffset;
        image.ProcessPixelRows(accessor =>
        {
            for (int y = 0; y < texture.Height; y++)
            {
                ReadOnlySpan<Rgba32> row = accessor.GetRowSpan(y);
                foreach (Rgba32 pixel in row)
                {
                    output[offset] = pixel.B;
                    output[offset + 1] = pixel.G;
                    output[offset + 2] = pixel.R;
                    output[offset + 3] = pixel.A;
                    offset += 4;
                }
            }
        });

        EnsureSerializerMarker(output, texture);
        return output;
    }

    /// <summary>
    /// Compatibility alias for the original diagnostic API. The corrected BGRA
    /// path is now used by the verified atlas writer through <see cref="ReplaceRgba"/>;
    /// ordinary single-texture replacement still uses <see cref="ReplaceRgb"/>
    /// unless its caller explicitly opts into complete alpha transfer.
    /// </summary>
    public static byte[] ReplaceRgbaDiagnosticUnsafe(
        byte[] smoData,
        int textureIndex,
        ReadOnlySpan<byte> imageData)
        => ReplaceRgba(smoData, textureIndex, imageData);

    /// <summary>
    /// Replaces RGB and keeps the encoded source image dimensions. Existing host
    /// alpha is resized to the new dimensions and preserved.
    /// </summary>
    public static byte[] ReplaceRgbWithoutDownscaling(
        byte[] smoData,
        int textureIndex,
        ReadOnlySpan<byte> imageData)
        => ReplaceWithoutDownscaling(smoData, textureIndex, imageData, replaceAlpha: false);

    /// <summary>
    /// Replaces complete RGBA and keeps the encoded source image dimensions,
    /// including non-power-of-two dimensions accepted by the native engine.
    /// </summary>
    public static byte[] ReplaceRgbaWithoutDownscaling(
        byte[] smoData,
        int textureIndex,
        ReadOnlySpan<byte> imageData)
        => ReplaceWithoutDownscaling(smoData, textureIndex, imageData, replaceAlpha: true);

    private static byte[] ReplaceWithoutDownscaling(
        byte[] smoData,
        int textureIndex,
        ReadOnlySpan<byte> imageData,
        bool replaceAlpha)
    {
        SmoDocument viewerBefore = SmoDocument.Parse(smoData);
        SMOTextureTool.Core.SmoDocument document =
            SMOTextureTool.Core.SmoDocument.Parse(smoData);
        SMOTextureTool.Core.TextureInfo texture =
            document.Textures.Single(item => item.Index == textureIndex);
        EnsureSupportedTexture(texture);

        using Image<Rgba32> replacement = Image.Load<Rgba32>(imageData);
        (int outputWidth, int outputHeight) = SelectRepresentableDimensions(
            replacement.Width, replacement.Height);
        if (replacement.Width != outputWidth || replacement.Height != outputHeight)
        {
            replacement.Mutate(context => context.Resize(new ResizeOptions
            {
                Size = new Size(outputWidth, outputHeight),
                Mode = ResizeMode.Stretch,
                Sampler = KnownResamplers.Bicubic,
                PremultiplyAlpha = true
            }));
        }
        if (!replaceAlpha)
        {
            using Image<Rgba32> host = document.Decode(texture);
            if (host.Width != replacement.Width || host.Height != replacement.Height)
                host.Mutate(context => context.Resize(replacement.Width, replacement.Height));
            replacement.ProcessPixelRows(host, (replacementAccessor, hostAccessor) =>
            {
                for (int y = 0; y < replacement.Height; y++)
                {
                    Span<Rgba32> replacementRow = replacementAccessor.GetRowSpan(y);
                    ReadOnlySpan<Rgba32> hostRow = hostAccessor.GetRowSpan(y);
                    for (int x = 0; x < replacement.Width; x++)
                        replacementRow[x].A = hostRow[x].A;
                }
            });
        }

        byte[] encoded;
        using (var stream = new MemoryStream())
        {
            replacement.SaveAsPng(stream);
            encoded = stream.ToArray();
        }
        byte[] output = document.RepackEncodedImages(
            new Dictionary<int, ReadOnlyMemory<byte>>
            {
                [textureIndex] = encoded
            },
            allowNonPowerOfTwoResize: true);
        int oldPixelSize = texture.PixelDataSize;
        int newPixelSize = checked(replacement.Width * replacement.Height * 4);
        if (newPixelSize != oldPixelSize)
        {
            PatchObjectCatalogAfterTextureResize(
                output,
                viewerBefore,
                texture.PixelDataOffset,
                oldPixelSize,
                checked(newPixelSize - oldPixelSize));
        }

        SMOTextureTool.Core.TextureInfo verified =
            SMOTextureTool.Core.SmoDocument.Parse(output).Textures
                .Single(item => item.Index == textureIndex);
        if (verified.Width != replacement.Width ||
            verified.Height != replacement.Height)
        {
            throw new InvalidDataException(
                $"Texture {textureIndex} was expected to keep source size " +
                $"{replacement.Width}x{replacement.Height}, but the repacked SMO " +
                $"contains {verified.Width}x{verified.Height}.");
        }
        EnsureSerializerMarker(output, verified);
        SmoDocument strict = SmoDocument.Parse(output);
        if (strict.HasErrors)
        {
            throw new InvalidDataException(
                $"Texture {textureIndex} resize left an invalid SMO object graph: " +
                string.Join(" | ", strict.Diagnostics
                    .Where(item => item.Severity == SmoDiagnosticSeverity.Error)
                    .Select(item => item.Message)));
        }
        VerifySkinPalettesUnchanged(viewerBefore, strict);
        return output;
    }

    private static void PatchObjectCatalogAfterTextureResize(
        Span<byte> output,
        SmoDocument before,
        int oldPixelOffset,
        int oldPixelSize,
        int delta)
    {
        long oldPixelEnd = checked((long)oldPixelOffset + oldPixelSize);
        long Map(long oldOffset) => oldOffset >= oldPixelEnd
            ? checked(oldOffset + delta)
            : oldOffset;

        byte[] source = before.Data.ToArray();
        foreach (SmoObjectEntry entry in before.Objects)
        {
            long newStart = Map(entry.PhysicalOffset);
            long newEnd = Map(entry.PhysicalEnd);
            uint newSize = checked((uint)(newEnd - newStart));
            int logicalOffsetField = checked(
                entry.TableOffset + sizeof(uint) + sizeof(ushort) +
                entry.NameLength + sizeof(uint));
            WriteUInt32(
                output,
                logicalOffsetField,
                checked((uint)(newStart - before.Header.DataStart)));
            WriteUInt32(output, logicalOffsetField + sizeof(uint), newSize);

            if (newSize == entry.SerializedSize)
                continue;

            ReadOnlySpan<byte> originalObject = source.AsSpan(
                checked((int)entry.PhysicalOffset),
                checked((int)entry.SerializedSize));
            if (!SmoDataBlockReader.TryReadHeader(
                    originalObject, 8, out SmoDataBlockHeader outer) ||
                outer.PayloadEnd + 1 != entry.SerializedSize)
            {
                continue;
            }
            if (outer.SizeKind != SmoDataBlockSizeCode.UInt32)
            {
                throw new InvalidDataException(
                    $"Texture resize changed wrapping object [{entry.Index}], " +
                    $"whose outer size field is not writable.");
            }
            int sizeOffset = checked(
                (int)newStart + outer.Offset + outer.HeaderSize - sizeof(uint));
            WriteUInt32(
                output,
                sizeOffset,
                checked(newSize - (uint)(8 + outer.HeaderSize + 1)));
        }

        // TextureData is serialized inline under a material, which is itself
        // inline under spSkin. Every enclosing data-block field must grow by
        // the pixel delta; catalog and outer-object sizes alone are not enough
        // for SmoSkinDecoder or the native sequential reader to reach the bone
        // palette that follows the material field.
        foreach (SmoObjectEntry entry in before.Objects)
        {
            ReadOnlySpan<byte> serialized = source.AsSpan(
                checked((int)entry.PhysicalOffset),
                checked((int)entry.SerializedSize));
            int fieldOffset = 8;
            while (fieldOffset < serialized.Length &&
                   SmoDataBlockReader.TryReadHeader(
                       serialized, fieldOffset, out SmoDataBlockHeader field))
            {
                long payloadStart = entry.PhysicalOffset + field.PayloadOffset;
                long payloadEnd = entry.PhysicalOffset + field.PayloadEnd;
                if (oldPixelOffset >= payloadStart && oldPixelEnd <= payloadEnd)
                {
                    WritePayloadSize(
                        output,
                        checked((int)Map(entry.PhysicalOffset + field.Offset)),
                        field,
                        checked((uint)(field.PayloadSize + delta)));
                }
                int next = checked((int)field.PayloadEnd);
                if (next <= fieldOffset)
                    break;
                fieldOffset = next;
            }
        }

        // Inline child objects also carry an [object ID][serialized size]
        // prefix outside the child's catalog interval. Keep those mirrors in
        // sync for every resized ancestor.
        foreach (SmoObjectEntry entry in before.Objects
                     .OrderByDescending(item => item.NestingDepth))
        {
            long newStart = Map(entry.PhysicalOffset);
            long newEnd = Map(entry.PhysicalEnd);
            uint newSize = checked((uint)(newEnd - newStart));
            if (newSize == entry.SerializedSize || entry.ParentIndex is null ||
                entry.PhysicalOffset < 8)
            {
                continue;
            }

            int oldPrefix = checked((int)entry.PhysicalOffset - 8);
            if (BinaryPrimitives.ReadUInt32LittleEndian(source.AsSpan(oldPrefix)) != entry.Id ||
                BinaryPrimitives.ReadUInt32LittleEndian(source.AsSpan(oldPrefix + 4)) !=
                entry.SerializedSize)
            {
                continue;
            }
            int newPrefix = checked((int)Map(oldPrefix));
            WriteUInt32(output, newPrefix + sizeof(uint), newSize);
        }
    }

    private static void VerifySkinPalettesUnchanged(
        SmoDocument before,
        SmoDocument after)
    {
        foreach (SmoObjectEntry beforeEntry in before.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.Skin))
        {
            if (!SmoSkinDecoder.TryDecode(
                    before, beforeEntry, out SmoSkin? beforeSkin, out _) ||
                beforeSkin is null)
            {
                continue;
            }
            SmoObjectEntry afterEntry = after.Objects.Single(entry =>
                entry.Id == beforeEntry.Id && entry.TypeHash == SmoClassIds.Skin);
            if (!SmoSkinDecoder.TryDecode(
                    after, afterEntry, out SmoSkin? afterSkin, out string error) ||
                afterSkin is null)
            {
                throw new InvalidDataException(
                    $"Texture resize damaged skin [{beforeEntry.Index}] " +
                    $"'{beforeEntry.Name}': {error}");
            }
            int[] beforeNodes = beforeSkin.Bones
                .Select(bone => bone.NodeObjectIndex).ToArray();
            int[] afterNodes = afterSkin.Bones
                .Select(bone => bone.NodeObjectIndex).ToArray();
            if (!beforeNodes.SequenceEqual(afterNodes))
            {
                throw new InvalidDataException(
                    $"Texture resize changed the node-reference palette of skin " +
                    $"[{beforeEntry.Index}] '{beforeEntry.Name}'.");
            }
        }
    }

    private static void WritePayloadSize(
        Span<byte> data,
        int headerOffset,
        SmoDataBlockHeader original,
        uint value)
    {
        int sizeOffset = headerOffset + original.HeaderSize;
        switch (original.SizeKind)
        {
            case SmoDataBlockSizeCode.UInt8:
                data[sizeOffset - 1] = checked((byte)value);
                break;
            case SmoDataBlockSizeCode.UInt16:
                BinaryPrimitives.WriteUInt16LittleEndian(
                    data[(sizeOffset - sizeof(ushort))..], checked((ushort)value));
                break;
            case SmoDataBlockSizeCode.UInt32:
                WriteUInt32(data, sizeOffset - sizeof(uint), value);
                break;
            default:
                throw new InvalidDataException(
                    $"Texture resize reached a non-writable inline size field " +
                    $"({original.SizeKind}).");
        }
    }

    private static (int Width, int Height) SelectRepresentableDimensions(
        int width,
        int height)
    {
        if (width is < 1 or > SMOTextureTool.Core.TextureInfo.MaximumCurrentHeaderDimension ||
            height is < 1 or > SMOTextureTool.Core.TextureInfo.MaximumCurrentHeaderDimension)
        {
            throw new InvalidDataException(
                $"Texture dimensions {width}x{height} are outside the supported " +
                $"1..{SMOTextureTool.Core.TextureInfo.MaximumCurrentHeaderDimension} range.");
        }

        // The low byte of each serialized E3 block size is also part of the
        // 0x32E3/0x43E3 format marker. Exact dimensions are representable when
        // width * height * 4 is divisible by 256 (width * height by 64).
        if (((long)width * height & 63) == 0)
            return (width, height);

        int compatibleWidth = CeilingPowerOfTwo(width);
        int compatibleHeight = CeilingPowerOfTwo(height);
        if (compatibleWidth > SMOTextureTool.Core.TextureInfo.MaximumCurrentHeaderDimension ||
            compatibleHeight > SMOTextureTool.Core.TextureInfo.MaximumCurrentHeaderDimension)
        {
            throw new InvalidDataException(
                $"Texture {width}x{height} requires a non-downscaling fallback of " +
                $"{compatibleWidth}x{compatibleHeight}, beyond the current SMO header limit.");
        }
        return (compatibleWidth, compatibleHeight);
    }

    private static int CeilingPowerOfTwo(int value)
    {
        int result = 1;
        while (result < value && result <= int.MaxValue / 2)
            result <<= 1;
        return result;
    }

    private static void WriteUInt32(Span<byte> data, int offset, uint value) =>
        BinaryPrimitives.WriteUInt32LittleEndian(data[offset..], value);

    private static void EnsureSupportedTexture(SMOTextureTool.Core.TextureInfo texture)
    {
        if (texture.FormatCode is not (0x32E3 or 0x43E3) ||
            texture.Layout != SMOTextureTool.Core.TextureLayout.Bgra)
            throw new NotSupportedException(
                $"Fixed-size texture replacement supports BGRA 0x32E3/0x43E3 only, " +
                $"not 0x{texture.FormatCode:X4}/{texture.Layout}.");
    }

    private static void EnsureSerializerMarker(
        ReadOnlySpan<byte> output,
        SMOTextureTool.Core.TextureInfo texture)
    {
        int markerOffset = checked(texture.BlockOffset + SerializedTextureDataMarkerOffset);
        if ((uint)markerOffset >= (uint)output.Length || output[markerOffset] != 0)
            throw new InvalidDataException(
                $"Texture {texture.Index} serializer marker at +0x3C was modified.");
    }
}
