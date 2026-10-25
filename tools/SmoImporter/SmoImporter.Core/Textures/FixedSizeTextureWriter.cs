using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;
using SixLabors.ImageSharp.Processing;
using SmoViewer.Core;

namespace SmoImporter.Core;

/// <summary>
/// Importer image/alpha policy over the shared, structurally verified texture
/// writer. Texture dimensions are real UInt32 fields, not size-header markers.
/// </summary>
public static class FixedSizeTextureWriter
{
    public static byte[] ReplaceRgb(byte[] smoData, int textureIndex, ReadOnlySpan<byte> imageData) =>
        Replace(smoData, textureIndex, imageData, resize: false, replaceAlpha: false);

    /// <summary>
    /// Replaces fixed-size RGBA. The caller controls the consuming materials'
    /// alpha-blend policy; this operation changes texture pixels only.
    /// </summary>
    public static byte[] ReplaceRgba(byte[] smoData, int textureIndex, ReadOnlySpan<byte> imageData) =>
        Replace(smoData, textureIndex, imageData, resize: false, replaceAlpha: true);

    /// <summary>Keeps supplied dimensions and preserves the resized host alpha.</summary>
    public static byte[] ReplaceRgbWithoutDownscaling(byte[] smoData, int textureIndex, ReadOnlySpan<byte> imageData) =>
        Replace(smoData, textureIndex, imageData, resize: true, replaceAlpha: false);

    /// <summary>Keeps supplied dimensions and RGBA, including non-power-of-two sizes.</summary>
    public static byte[] ReplaceRgbaWithoutDownscaling(byte[] smoData, int textureIndex, ReadOnlySpan<byte> imageData) =>
        Replace(smoData, textureIndex, imageData, resize: true, replaceAlpha: true);

    private static byte[] Replace(
        byte[] smoData, int textureIndex, ReadOnlySpan<byte> imageData, bool resize, bool replaceAlpha)
    {
        var document = SMOTextureTool.Core.SmoDocument.Parse(smoData);
        var texture = document.Textures.Single(item => item.Index == textureIndex);
        if (!texture.CanReplace || texture.Layout != SMOTextureTool.Core.TextureLayout.Bgra)
            throw new NotSupportedException(texture.ReplacementIssue ?? "Texture replacement requires embedded BGRA32 pixels.");
        using Image<Rgba32> image = SMOTextureTool.Core.TextureImageLoader.Load(imageData);
        if (image.Width is < 1 or > SMOTextureTool.Core.TextureInfo.MaximumCurrentHeaderDimension ||
            image.Height is < 1 or > SMOTextureTool.Core.TextureInfo.MaximumCurrentHeaderDimension)
            throw new InvalidDataException("Texture dimensions are outside the supported 1..16384 range.");
        if (!resize && (image.Width != texture.Width || image.Height != texture.Height))
            image.Mutate(context => context.Resize(texture.Width, texture.Height));
        if (!replaceAlpha)
        {
            using Image<Rgba32> host = document.Decode(texture);
            if (host.Width != image.Width || host.Height != image.Height)
                host.Mutate(context => context.Resize(image.Width, image.Height));
            image.ProcessPixelRows(host, (replacement, original) =>
            {
                for (int y = 0; y < image.Height; y++)
                {
                    Span<Rgba32> row = replacement.GetRowSpan(y);
                    ReadOnlySpan<Rgba32> hostRow = original.GetRowSpan(y);
                    for (int x = 0; x < image.Width; x++)
                        row[x].A = hostRow[x].A;
                }
            });
        }
        byte[] pixels = new byte[checked(image.Width * image.Height * 4)];
        image.ProcessPixelRows(accessor =>
        {
            int offset = 0;
            for (int y = 0; y < image.Height; y++)
                foreach (Rgba32 pixel in accessor.GetRowSpan(y))
                {
                    pixels[offset++] = pixel.B;
                    pixels[offset++] = pixel.G;
                    pixels[offset++] = pixel.R;
                    pixels[offset++] = pixel.A;
                }
        });
        return SmoTextureDataWriter.ReplaceBgra(SmoDocument.Parse(smoData),
            texture.ObjectIndex, image.Width, image.Height, pixels);
    }
}
