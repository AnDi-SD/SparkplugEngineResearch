using SixLabors.ImageSharp;
using SixLabors.ImageSharp.Formats;
using SixLabors.ImageSharp.PixelFormats;

namespace SMOTextureTool.Core;

/// <summary>Bounds replacement decoding before allocating a full pixel image.</summary>
public static class TextureImageLoader
{
    public const long MaximumDecodedBytes = 128L * 1024 * 1024;
    private static DecoderOptions Options => new() { MaxFrames = 1 };

    public static Image<Rgba32> Load(string path)
    {
        using var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read);
        ImageInfo info = Image.Identify(Options, stream);
        ValidateDimensions(info.Width, info.Height);
        stream.Position = 0;
        return Image.Load<Rgba32>(Options, stream);
    }

    public static Image<Rgba32> Load(ReadOnlySpan<byte> data)
    {
        ImageInfo info = Image.Identify(Options, data);
        ValidateDimensions(info.Width, info.Height);
        return Image.Load<Rgba32>(Options, data);
    }

    public static void ValidateDimensions(int width, int height)
    {
        if (width is < 1 or > TextureInfo.MaximumCurrentHeaderDimension ||
            height is < 1 or > TextureInfo.MaximumCurrentHeaderDimension ||
            (long)width * height * 4 > MaximumDecodedBytes)
            throw new InvalidDataException("Изображение превышает предел 16384 пикселя на сторону или 128 МиБ несжатых пикселей.");
    }
}
