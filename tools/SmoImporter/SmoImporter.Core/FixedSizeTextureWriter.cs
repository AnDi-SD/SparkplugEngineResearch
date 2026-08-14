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
    /// Diagnostic full-BGRA probe. The earlier RGBA crash was caused by treating
    /// the serializer marker at +0x3C as pixel data, not by a proven Alpha ban.
    /// The corrected full-BGRA path passed a controlled native load test. Production
    /// still preserves host Alpha via <see cref="ReplaceRgb"/> until broader visual
    /// and gameplay validation covers different material semantics.
    /// </summary>
    public static byte[] ReplaceRgbaDiagnosticUnsafe(
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
