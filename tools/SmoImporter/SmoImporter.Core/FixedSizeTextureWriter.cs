using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;
using SixLabors.ImageSharp.Processing;

namespace SmoImporter.Core;

public static class FixedSizeTextureWriter
{
    public static byte[] ReplaceRgb(byte[] smoData, int textureIndex, ReadOnlySpan<byte> imageData)
    {
        byte[] output = (byte[])smoData.Clone();
        SMOTextureTool.Core.SmoDocument document = SMOTextureTool.Core.SmoDocument.Parse(output);
        SMOTextureTool.Core.TextureInfo texture = document.Textures.Single(item => item.Index == textureIndex);
        if (texture.Layout != SMOTextureTool.Core.TextureLayout.Abgr)
            throw new NotSupportedException($"Safe texture replacement currently supports ABGR only, not {texture.Layout}.");

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
                    // Preserve the host alpha byte. Only the three proven mutable
                    // colour bytes in the existing fixed-size ABGR buffer change.
                    output[offset + 1] = pixel.B;
                    output[offset + 2] = pixel.G;
                    output[offset + 3] = pixel.R;
                    offset += 4;
                }
            }
        });

        return output;
    }

    /// <summary>
    /// Unsafe diagnostic probe. Game testing proved that replacing the Alpha
    /// bytes of an otherwise valid fixed-size ABGR leaf can crash the runtime.
    /// Production writers must use <see cref="ReplaceRgb"/> instead.
    /// </summary>
    public static byte[] ReplaceRgbaDiagnosticUnsafe(
        byte[] smoData,
        int textureIndex,
        ReadOnlySpan<byte> imageData)
    {
        byte[] output = (byte[])smoData.Clone();
        SMOTextureTool.Core.SmoDocument document = SMOTextureTool.Core.SmoDocument.Parse(output);
        SMOTextureTool.Core.TextureInfo texture = document.Textures.Single(item => item.Index == textureIndex);
        if (texture.Layout != SMOTextureTool.Core.TextureLayout.Abgr)
            throw new NotSupportedException(
                $"Full texture replacement currently supports ABGR only, not {texture.Layout}.");

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
                    output[offset] = pixel.A;
                    output[offset + 1] = pixel.B;
                    output[offset + 2] = pixel.G;
                    output[offset + 3] = pixel.R;
                    offset += 4;
                }
            }
        });
        return output;
    }
}
