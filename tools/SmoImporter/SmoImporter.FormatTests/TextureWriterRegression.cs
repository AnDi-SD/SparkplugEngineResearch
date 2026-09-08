using System.Security.Cryptography;
using System.Text.Json;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;
using SmoImporter.Core;
using TextureDocument = SMOTextureTool.Core.SmoDocument;

internal static class TextureWriterRegression
{
    public static void Run(string[] args)
    {
        if (args.Length > 16) throw new ArgumentException("At most 15 texture specimens.");
        Directory.CreateDirectory(args[0]);
        int checks = 0;
        var results = new List<object>();
        void Check(bool condition, string message)
        {
            checks++;
            if (!condition) throw new InvalidDataException(message);
        }
        foreach (string path in args.Skip(1))
        {
            byte[] original = File.ReadAllBytes(path);
            var doc = TextureDocument.Parse(original);
            var texture = doc.Textures.First(item => item.CanReplace);
            using var fixedImage = new Image<Rgba32>(texture.Width, texture.Height, new Rgba32(17, 71, 139, 53));
            byte[] encoded = Encode(fixedImage);
            foreach (bool alpha in new[] { false, true })
            {
                byte[] actual = alpha ? FixedSizeTextureWriter.ReplaceRgba(original, texture.Index, encoded)
                    : FixedSizeTextureWriter.ReplaceRgb(original, texture.Index, encoded);
                byte[] expected = (byte[])original.Clone();
                for (int offset = texture.PixelDataOffset; offset < texture.PixelDataOffset + texture.PixelDataSize; offset += 4)
                {
                    expected[offset] = 139; expected[offset + 1] = 71; expected[offset + 2] = 17;
                    if (alpha) expected[offset + 3] = 53;
                }
                Check(actual.AsSpan().SequenceEqual(expected), "fixed-size writer changes exactly the declared RGB/alpha bytes");
            }
            using var resizedImage = new Image<Rgba32>(13, 7, new Rgba32(23, 97, 173, 31));
            byte[] resizedPng = Encode(resizedImage);
            byte[] output = FixedSizeTextureWriter.ReplaceRgbaWithoutDownscaling(original, texture.Index, resizedPng);
            byte[] toolOutput = doc.RepackEncodedImages(new Dictionary<int, ReadOnlyMemory<byte>> { [texture.Index] = resizedPng });
            Check(output.AsSpan().SequenceEqual(toolOutput), "Importer and TextureTool use the same structural writer without dimension rounding");
            var after = TextureDocument.Parse(output);
            var resized = after.Textures.Single(item => item.ObjectIndex == texture.ObjectIndex);
            Check(resized.Width == 13 && resized.Height == 7, "exact non-power-of-two dimensions");
            byte[] rgbOutput = FixedSizeTextureWriter.ReplaceRgbWithoutDownscaling(original, texture.Index, resizedPng);
            var rgbDocument = TextureDocument.Parse(rgbOutput);
            var rgbTexture = rgbDocument.Textures.Single(item => item.ObjectIndex == texture.ObjectIndex);
            using Image<Rgba32> rgbImage = rgbDocument.Decode(rgbTexture);
            for (int y = 0; y < 7; y++)
                for (int x = 0; x < 13; x++)
                    Check(rgbImage[x, y].R == 23 && rgbImage[x, y].G == 97 && rgbImage[x, y].B == 173,
                        "RGB resize keeps supplied color channels");
            Check(File.ReadAllBytes(path).AsSpan().SequenceEqual(original), "source remains unchanged");
            string target = Path.Combine(args[0], Path.GetFileNameWithoutExtension(path) + "-importer-13x7.smo");
            File.WriteAllBytes(target, output);
            results.Add(new { path = Path.GetFullPath(path), inputSha256 = Hash(original),
                outputPath = Path.GetFullPath(target), outputSha256 = Hash(output), texture.ObjectIndex });
        }
        File.WriteAllText(Path.Combine(args[0], "report.json"), JsonSerializer.Serialize(new
            { kind = "shared-texture-writer-importer-regression", checks, results },
            new JsonSerializerOptions { WriteIndented = true }));
        Console.WriteLine($"PASS shared texture writer: {checks} assertions; {results.Count} specimens");
    }

    private static byte[] Encode(Image<Rgba32> image)
    {
        using var stream = new MemoryStream(); image.SaveAsPng(stream); return stream.ToArray();
    }
    private static string Hash(byte[] data) => Convert.ToHexString(SHA256.HashData(data)).ToLowerInvariant();
}
