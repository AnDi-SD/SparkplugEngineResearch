using SixLabors.ImageSharp;
using SixLabors.ImageSharp.Formats.Png;
using SixLabors.ImageSharp.PixelFormats;
using SixLabors.ImageSharp.Processing;

namespace SmoImporter.Core;

/// <summary>
/// SMO stores embedded level textures as uncompressed BGRA. A perfectly small
/// GLB can therefore expand by hundreds of MiB when it contains many 4K PNGs.
/// This policy keeps one imported model's final level footprint bounded while
/// preserving aspect ratio, alpha, material indices and power-of-two sizes.
/// </summary>
public static class SmoLevelEmbeddedTextureBudget
{
    public const long MaximumDecodedTextureBytes = 128L * 1024 * 1024;

    public static ImportedScene Prepare(ImportedScene scene)
    {
        ArgumentNullException.ThrowIfNull(scene);
        int[] usedIndices = scene.Meshes
            .Where(mesh => mesh.MaterialIndex >= 0 &&
                           mesh.MaterialIndex < scene.Materials.Count)
            .Select(mesh => scene.Materials[mesh.MaterialIndex].BaseColorTextureIndex)
            .Where(index => index >= 0 && index < scene.Textures.Count)
            .Distinct()
            .OrderBy(index => index)
            .ToArray();
        long decodedBytes = usedIndices.Sum(index => checked(
            (long)scene.Textures[index].Width * scene.Textures[index].Height * 4));
        if (decodedBytes <= MaximumDecodedTextureBytes)
            return scene;

        double scale = Math.Sqrt(MaximumDecodedTextureBytes / (double)decodedBytes);
        ImportedTexture[] textures = scene.Textures.ToArray();
        var changes = new List<string>();
        foreach (int index in usedIndices)
        {
            ImportedTexture source = textures[index];
            int width = FloorPowerOfTwo(Math.Max(1, (int)Math.Floor(source.Width * scale)));
            int height = FloorPowerOfTwo(Math.Max(1, (int)Math.Floor(source.Height * scale)));
            width = Math.Min(width, source.Width);
            height = Math.Min(height, source.Height);
            if (width == source.Width && height == source.Height)
                continue;

            using Image<Rgba32> image = Image.Load<Rgba32>(source.Data);
            if (image.Width != source.Width || image.Height != source.Height)
            {
                throw new InvalidDataException(
                    $"Texture {source.Name} dimensions do not match its image payload.");
            }
            image.Mutate(context => context.Resize(new ResizeOptions
            {
                Size = new Size(width, height),
                Mode = ResizeMode.Stretch,
                Sampler = KnownResamplers.Bicubic,
                PremultiplyAlpha = true
            }));
            using var encoded = new MemoryStream();
            image.Save(encoded, new PngEncoder());
            textures[index] = source with
            {
                MimeType = "image/png",
                Width = width,
                Height = height,
                Data = encoded.ToArray()
            };
            changes.Add($"{source.Name}: {source.Width}x{source.Height}->{width}x{height}");
        }

        long finalBytes = usedIndices.Sum(index => checked(
            (long)textures[index].Width * textures[index].Height * 4));
        if (finalBytes > MaximumDecodedTextureBytes)
        {
            throw new InvalidDataException(
                $"Embedded level textures still require " +
                $"{finalBytes / (1024d * 1024d):N1} MiB after safe resizing.");
        }

        return scene with
        {
            EmbeddedTextures = textures,
            ImportWarnings = scene.ImportWarnings.Concat(
            [
                $"Level texture budget: decoded BGRA reduced from " +
                $"{decodedBytes / (1024d * 1024d):N1} MiB to " +
                $"{finalBytes / (1024d * 1024d):N1} MiB ({string.Join("; ", changes)})."
            ]).ToArray()
        };
    }

    private static int FloorPowerOfTwo(int value)
    {
        int result = 1;
        while (result <= value / 2)
            result *= 2;
        return result;
    }
}
