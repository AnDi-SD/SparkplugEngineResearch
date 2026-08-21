using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;
using SixLabors.ImageSharp.Processing;

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
