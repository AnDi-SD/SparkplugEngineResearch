namespace SmoImporter.Core;

/// <summary>
/// Hard safety budgets for untrusted external model data.  They are deliberately
/// well above the character assets used by the importer, but low enough to fail
/// before a malformed accessor or decoded image can exhaust the workstation.
/// </summary>
internal static class ImportedModelResourceLimits
{
    public const long MaximumModelFileBytes = 256L * 1024 * 1024;
    public const long MaximumNativePayloadBytes = 512L * 1024 * 1024;
    public const int MaximumJsonBytes = 32 * 1024 * 1024;
    public const int MaximumMeshes = 2048;
    public const int MaximumPrimitives = 16_384;
    public const int MaximumNodes = 16_384;
    public const int MaximumSkins = 256;
    public const int MaximumJointsPerSkin = 4096;
    public const int MaximumMaterials = 4096;
    public const int MaximumTextures = 512;
    public const int MaximumAccessors = 65_536;
    public const int MaximumBufferViews = 65_536;
    public const int MaximumVerticesPerPrimitive = 1_000_000;
    public const long MaximumTotalVertices = 1_000_000;
    public const int MaximumIndicesPerPrimitive = 6_000_000;
    public const long MaximumTotalIndices = 6_000_000;
    public const long MaximumDecodedAccessorBytes = 256L * 1024 * 1024;
    public const int MaximumTextureDimension = 8192;
    // ImportedTexture keeps the encoded PNG/JPEG payload. Large source catalogs
    // are therefore safe as long as consumers decode one image at a time and the
    // preview uses bounded proxies. This is a catalog limit, not permission to
    // keep the corresponding RGBA surfaces resident simultaneously.
    public const long MaximumCatalogTexturePixels = 256L * 1024 * 1024;
    public const int MaximumEncodedTextureBytes = 128 * 1024 * 1024;
    public const long MaximumTotalEncodedTextureBytes = 256L * 1024 * 1024;

    public static void ValidateInputFile(string path, string format)
    {
        var information = new FileInfo(path);
        if (!information.Exists)
            throw new FileNotFoundException($"{format} model was not found.", path);
        if (information.Length > MaximumModelFileBytes)
        {
            throw new InvalidDataException(
                $"{format} file is {FormatBytes(information.Length)}, above the safe " +
                $"import limit of {FormatBytes(MaximumModelFileBytes)}. Reduce or split " +
                "the model before importing it.");
        }
    }

    public static long AddTexturePixels(
        long currentPixels,
        int width,
        int height,
        string owner)
    {
        if (width <= 0 || height <= 0 ||
            width > MaximumTextureDimension || height > MaximumTextureDimension)
        {
            throw new InvalidDataException(
                $"{owner} is {width}x{height}; each texture side must be between 1 " +
                $"and {MaximumTextureDimension} pixels for a safe preview.");
        }

        long pixels = checked((long)width * height);
        long total = checked(currentPixels + pixels);
        if (total > MaximumCatalogTexturePixels)
        {
            throw new InvalidDataException(
                $"The encoded texture catalog describes more than " +
                $"{MaximumCatalogTexturePixels:N0} pixels " +
                $"({FormatBytes(MaximumCatalogTexturePixels * 4)} if decoded together). " +
                "Reduce texture resolution or remove unused materials; the catalog " +
                "is too large even for bounded sequential processing.");
        }
        return total;
    }

    public static void ValidateTextures(
        IReadOnlyList<ImportedTexture> textures,
        string owner)
    {
        ValidateCount(textures.Count, MaximumTextures, $"{owner} texture");
        long pixels = 0;
        long encodedBytes = 0;
        for (int index = 0; index < textures.Count; index++)
        {
            ImportedTexture texture = textures[index];
            if (texture.Data.Length > MaximumEncodedTextureBytes)
            {
                throw new InvalidDataException(
                    $"{owner} texture [{index}] '{texture.Name}' exceeds the safe " +
                    "encoded-texture limit.");
            }
            encodedBytes = checked(encodedBytes + texture.Data.LongLength);
            if (encodedBytes > MaximumTotalEncodedTextureBytes)
            {
                throw new InvalidDataException(
                    $"{owner} textures contain more than " +
                    $"{FormatBytes(MaximumTotalEncodedTextureBytes)} of encoded image " +
                    "data. Reduce or remove source textures before importing.");
            }
            pixels = AddTexturePixels(
                pixels,
                texture.Width,
                texture.Height,
                $"{owner} texture [{index}] '{texture.Name}'");
        }
    }

    public static void ValidateCount(
        long value,
        long maximum,
        string owner)
    {
        if (value < 0 || value > maximum)
        {
            throw new InvalidDataException(
                $"{owner} count {value:N0} exceeds the safe limit {maximum:N0}.");
        }
    }

    private static string FormatBytes(long bytes) =>
        $"{bytes / (1024d * 1024d):N1} MiB";
}
