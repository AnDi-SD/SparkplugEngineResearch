namespace SmoImporter.Core;

public sealed record ImportedTextureResolutionGroup(
    int Width,
    int Height,
    int Count);

/// <summary>
/// Conservative texture-memory estimate for a model before it is converted to
/// the game's final texture layout. Encoded bytes describe the source file;
/// RGBA values describe possible runtime residency and are intentionally kept
/// separate from the importer's bounded, sequential decoding working set.
/// </summary>
public sealed record ImportedTextureMemoryEstimate(
    int TextureCount,
    long EncodedBytes,
    long DecodedBaseRgbaBytes,
    long DecodedMipmappedRgbaBytes,
    long GameMemoryBudgetBytes,
    IReadOnlyList<ImportedTextureResolutionGroup> ResolutionGroups)
{
    public double BaseBudgetFraction => GameMemoryBudgetBytes <= 0
        ? 0
        : DecodedBaseRgbaBytes / (double)GameMemoryBudgetBytes;

    public double MipmappedBudgetFraction => GameMemoryBudgetBytes <= 0
        ? 0
        : DecodedMipmappedRgbaBytes / (double)GameMemoryBudgetBytes;
}

public static class ImportedTextureMemoryEstimator
{
    public static ImportedTextureMemoryEstimate Estimate(
        IEnumerable<ImportedTexture> textures,
        long gameMemoryBudgetBytes)
    {
        ArgumentNullException.ThrowIfNull(textures);
        if (gameMemoryBudgetBytes <= 0)
            throw new ArgumentOutOfRangeException(nameof(gameMemoryBudgetBytes));

        ImportedTexture[] materialized = textures.ToArray();
        long encodedBytes = 0;
        long decodedBaseBytes = 0;
        long decodedMipmappedBytes = 0;
        foreach (ImportedTexture texture in materialized)
        {
            if (texture.Width <= 0 || texture.Height <= 0)
                throw new InvalidDataException(
                    $"Texture '{texture.Name}' has invalid dimensions " +
                    $"{texture.Width}x{texture.Height}.");
            encodedBytes = checked(encodedBytes + texture.Data.LongLength);
            decodedBaseBytes = checked(decodedBaseBytes +
                (long)texture.Width * texture.Height * 4);
            decodedMipmappedBytes = checked(decodedMipmappedBytes +
                ComputeRgbaMipChainBytes(texture.Width, texture.Height));
        }

        ImportedTextureResolutionGroup[] groups = materialized
            .GroupBy(texture => (texture.Width, texture.Height))
            .OrderByDescending(group =>
                checked((long)group.Key.Width * group.Key.Height * group.Count()))
            .ThenByDescending(group => group.Key.Width)
            .ThenByDescending(group => group.Key.Height)
            .Select(group => new ImportedTextureResolutionGroup(
                group.Key.Width,
                group.Key.Height,
                group.Count()))
            .ToArray();

        return new ImportedTextureMemoryEstimate(
            materialized.Length,
            encodedBytes,
            decodedBaseBytes,
            decodedMipmappedBytes,
            gameMemoryBudgetBytes,
            Array.AsReadOnly(groups));
    }

    private static long ComputeRgbaMipChainBytes(int width, int height)
    {
        long pixels = 0;
        int levelWidth = width;
        int levelHeight = height;
        while (true)
        {
            pixels = checked(pixels + (long)levelWidth * levelHeight);
            if (levelWidth == 1 && levelHeight == 1)
                return checked(pixels * 4);
            levelWidth = Math.Max(1, levelWidth / 2);
            levelHeight = Math.Max(1, levelHeight / 2);
        }
    }
}
