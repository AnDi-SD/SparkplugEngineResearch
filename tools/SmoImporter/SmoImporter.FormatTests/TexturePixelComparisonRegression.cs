using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;
using SmoImporter.Core;

internal static class TexturePixelComparisonRegression
{
    internal static int Run()
    {
        const int width = 17, height = 9;
        using var image = new Image<Rgba32>(width, height);
        byte[] expected = new byte[width * height * 4];
        for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
        {
            var pixel = new Rgba32((byte)(x * 13), (byte)(y * 29), (byte)(x + y), (byte)(x * y));
            image[x, y] = pixel;
            int offset = (y * width + x) * 4;
            expected[offset] = pixel.B;
            expected[offset + 1] = pixel.G;
            expected[offset + 2] = pixel.R;
            expected[offset + 3] = pixel.A;
        }
        using var stream = new MemoryStream();
        image.SaveAsPng(stream);
        byte[] encoded = stream.ToArray();
        int checks = 0;
        Check(Matches(expected, out string error) && error.Length == 0, "exact RGBA including transparent pixels");
        foreach (int pixel in new[] { 0, width - 1, width, width * height - 1 })
        for (int channel = 0; channel < 4; channel++)
        {
            byte[] changed = expected.ToArray();
            changed[pixel * 4 + channel] ^= 1;
            Check(!Matches(changed, out error) && error == $"RGBA pixels differ at byte offset {pixel * 4}",
                "first differing pixel offset across rows and channels");
        }
        Check(!Matches(expected[..^1], out error) && error.Contains("expected 612"), "short payload");
        Check(!Matches(expected.Concat(new byte[1]).ToArray(), out error), "long payload");
        Check(!ImportedTextureImageTools.SerializedBgraMatches(encoded, width + 1, height, expected, out error)
            && error.Contains("source image is 17x9"), "dimension mismatch");

        using var solid = new Image<Rgba32>(1, 1, new Rgba32(31, 63, 127, 255));
        stream.SetLength(0);
        solid.SaveAsPng(stream);
        byte[] resized = Enumerable.Range(0, 6).SelectMany(_ => new byte[] { 127, 63, 31, 255 }).ToArray();
        Check(ImportedTextureImageTools.SerializedBgraMatches(stream.ToArray(), 3, 2, resized, out error, true),
            "explicit resize comparison");
        Check(Matches(expected, out error), "input span remains intact after negative cases");
        Console.WriteLine($"PASS {checks} texture pixel comparison checks");
        return 0;

        bool Matches(byte[] bytes, out string reason) => ImportedTextureImageTools.SerializedBgraMatches(
            encoded, width, height, bytes, out reason);
        void Check(bool value, string reason)
        {
            checks++;
            if (!value) throw new InvalidDataException(reason);
        }
    }
}
