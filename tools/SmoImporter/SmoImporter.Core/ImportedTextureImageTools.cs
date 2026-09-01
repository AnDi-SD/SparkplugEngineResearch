using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;
using SixLabors.ImageSharp.Processing;

namespace SmoImporter.Core;

/// <summary>
/// Exact image inspection and serialized BGRA comparison helpers. This class does
/// not combine source images or remap texture coordinates.
/// </summary>
internal static class ImportedTextureImageTools
{
    internal static bool TextureContainsTransparency(ImportedTexture texture)
    {
        ArgumentNullException.ThrowIfNull(texture);
        using Image<Rgba32> decoded = LoadAndValidate(texture);
        bool result = false;
        decoded.ProcessPixelRows(accessor =>
        {
            for (int y = 0; y < accessor.Height && !result; y++)
            {
                foreach (Rgba32 pixel in accessor.GetRowSpan(y))
                {
                    if (pixel.A >= byte.MaxValue)
                        continue;
                    result = true;
                    break;
                }
            }
        });
        return result;
    }

    public static bool SerializedBgraMatches(
        ReadOnlySpan<byte> encodedImage,
        int expectedWidth,
        int expectedHeight,
        ReadOnlySpan<byte> serializedBgra,
        out string error,
        bool resizeToExpected = false)
    {
        using Image<Rgba32> image = Image.Load<Rgba32>(encodedImage);
        if (image.Width != expectedWidth || image.Height != expectedHeight)
        {
            if (!resizeToExpected)
            {
                error = $"source image is {image.Width}x{image.Height}, expected " +
                        $"{expectedWidth}x{expectedHeight}";
                return false;
            }
            image.Mutate(context => context.Resize(new ResizeOptions
            {
                Size = new Size(expectedWidth, expectedHeight),
                Mode = ResizeMode.Stretch,
                Sampler = KnownResamplers.Bicubic,
                PremultiplyAlpha = true
            }));
        }
        int expectedBytes = checked(expectedWidth * expectedHeight * 4);
        if (serializedBgra.Length != expectedBytes)
        {
            error = $"serialized BGRA payload has {serializedBgra.Length} bytes, " +
                    $"expected {expectedBytes}";
            return false;
        }
        byte[] serialized = serializedBgra.ToArray();
        int offset = 0;
        bool matches = true;
        image.ProcessPixelRows(accessor =>
        {
            for (int y = 0; y < accessor.Height && matches; y++)
            {
                foreach (Rgba32 pixel in accessor.GetRowSpan(y))
                {
                    if (serialized[offset] != pixel.B ||
                        serialized[offset + 1] != pixel.G ||
                        serialized[offset + 2] != pixel.R ||
                        serialized[offset + 3] != pixel.A)
                    {
                        matches = false;
                        break;
                    }
                    offset += 4;
                }
            }
        });
        error = matches ? string.Empty : $"RGBA pixels differ at byte offset {offset}";
        return matches;
    }

    private static Image<Rgba32> LoadAndValidate(ImportedTexture texture)
    {
        Image<Rgba32> decoded = Image.Load<Rgba32>(texture.Data);
        if (decoded.Width == texture.Width && decoded.Height == texture.Height)
            return decoded;

        int actualWidth = decoded.Width;
        int actualHeight = decoded.Height;
        decoded.Dispose();
        throw new InvalidDataException(
            $"Texture {texture.Name} declares {texture.Width}x{texture.Height}, " +
            $"but its image is {actualWidth}x{actualHeight}.");
    }
}
