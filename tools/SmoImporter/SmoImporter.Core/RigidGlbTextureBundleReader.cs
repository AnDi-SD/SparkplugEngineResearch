using System.Text.RegularExpressions;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.Formats.Png;
using SixLabors.ImageSharp.PixelFormats;

namespace SmoImporter.Core;

public sealed record RigidTextureFrame(
    int FrameNumber,
    string SourcePath,
    int SourceWidth,
    int SourceHeight,
    bool WasUpscaled,
    ImportedTexture Texture);

public sealed record RigidMaterialGroup(
    int MaterialNumber,
    string Name,
    IReadOnlyList<ImportedMesh> Meshes,
    IReadOnlyList<RigidTextureFrame> Frames)
{
    public RigidTextureFrame BaseFrame => Frames[0];
}

public sealed record RigidGlbTextureBundle(
    string GlbPath,
    string TextureDirectory,
    ImportedScene Scene,
    IReadOnlyList<RigidMaterialGroup> MaterialGroups,
    IReadOnlyList<string>? IgnoredMeshNames = null,
    IReadOnlyList<string>? IgnoredTextureFileNames = null)
{
    public string ModelPath => GlbPath;
    public IReadOnlyList<string> IgnoredMeshes => IgnoredMeshNames ?? [];
    public IReadOnlyList<string> IgnoredTextureFiles => IgnoredTextureFileNames ?? [];
}

public sealed class RigidTextureBundleContainsSkinnedMeshesException
    : IOException
{
    public RigidTextureBundleContainsSkinnedMeshesException(string message)
        : base(message)
    {
    }
}

/// <summary>
/// Reads a rigid GLB/OBJ/FBX whose external PNG files use the ripped-model matN
/// naming convention. Active matN meshes and their frames are resolved strictly;
/// unrelated helper meshes and PNG files are retained as explicit warnings.
/// </summary>
public static class RigidGlbTextureBundleReader
{
    public const int DefaultMaximumTextureDimension = 2048;
    public const int AbsoluteMaximumTextureDimension = 2048;

    private static readonly Regex NodeMaterialPattern = new(
        @"(?:^|[^a-z0-9])mat(?<material>[1-9][0-9]*)(?=(?:\.?png)?(?:[^a-z0-9]|$))",
        RegexOptions.IgnoreCase | RegexOptions.CultureInvariant);

    private static readonly Regex TextureFilePattern = new(
        @"^(?:.+_)?mat(?<material>[1-9][0-9]*)(?:\.(?<frame>[1-9][0-9]*))?\.png$",
        RegexOptions.IgnoreCase | RegexOptions.CultureInvariant);

    /// <summary>
    /// Tries the external-texture convention in the GLB's directory. False means
    /// only that no matN(.frame).png candidate exists. Once a candidate exists,
    /// malformed or incomplete bundles throw just like <see cref="Read"/>.
    /// </summary>
    public static bool TryRead(
        string glbPath,
        out RigidGlbTextureBundle? bundle,
        int maximumTextureDimension = DefaultMaximumTextureDimension)
    {
        bundle = null;
        if (!HasCandidateTextureFiles(glbPath))
            return false;
        bundle = Read(glbPath, maximumTextureDimension: maximumTextureDimension);
        return true;
    }

    public static bool TryReadModel(
        string modelPath,
        out RigidGlbTextureBundle? bundle,
        string? textureDirectory = null,
        string? blenderPath = null,
        int maximumTextureDimension = DefaultMaximumTextureDimension)
    {
        bundle = null;
        if (!HasCandidateTextureFiles(modelPath, textureDirectory))
            return false;
        bundle = ReadModel(
            modelPath, textureDirectory, blenderPath, maximumTextureDimension);
        return true;
    }

    public static bool HasCandidateTextureFiles(
        string modelPath,
        string? textureDirectory = null)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(modelPath);
        string fullModelPath = Path.GetFullPath(modelPath);
        string? directory = textureDirectory is null
            ? Path.GetDirectoryName(fullModelPath)
            : Path.GetFullPath(textureDirectory);
        return directory is not null && Directory.Exists(directory) &&
            Directory.EnumerateFiles(directory, "*", SearchOption.TopDirectoryOnly)
                .Select(Path.GetFileName)
                .Any(fileName => fileName is not null && TextureFilePattern.IsMatch(fileName));
    }

    public static RigidGlbTextureBundle Read(
        string glbPath,
        string? textureDirectory = null,
        int maximumTextureDimension = DefaultMaximumTextureDimension)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(glbPath);
        string fullGlbPath = Path.GetFullPath(glbPath);
        return ReadScene(
            fullGlbPath,
            GlbModelReader.Read(fullGlbPath),
            textureDirectory,
            maximumTextureDimension);
    }

    public static RigidGlbTextureBundle ReadModel(
        string modelPath,
        string? textureDirectory = null,
        string? blenderPath = null,
        int maximumTextureDimension = DefaultMaximumTextureDimension)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(modelPath);
        string fullModelPath = Path.GetFullPath(modelPath);
        ImportedScene scene = Path.GetExtension(fullModelPath).Equals(
            ".fbx", StringComparison.OrdinalIgnoreCase)
                ? FbxModelReader.ReadRigid(fullModelPath, blenderPath)
                : ImportedModelReader.Read(fullModelPath, blenderPath);
        return Bind(
            fullModelPath,
            scene,
            textureDirectory,
            maximumTextureDimension);
    }

    public static RigidGlbTextureBundle Bind(
        string modelPath,
        ImportedScene scene,
        string? textureDirectory = null,
        int maximumTextureDimension = DefaultMaximumTextureDimension)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(modelPath);
        ArgumentNullException.ThrowIfNull(scene);
        return ReadScene(
            Path.GetFullPath(modelPath),
            scene,
            textureDirectory,
            maximumTextureDimension);
    }

    /// <summary>
    /// Builds a one-frame material bundle from the textures already resolved in an
    /// imported scene. The legacy rigid replacer remains preferable for zero or one
    /// texture; two or more distinct texture groups require the multi-material writer.
    /// </summary>
    public static bool TryBindSceneTextures(
        string modelPath,
        ImportedScene scene,
        out RigidGlbTextureBundle? bundle,
        int maximumTextureDimension = DefaultMaximumTextureDimension)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(modelPath);
        ArgumentNullException.ThrowIfNull(scene);
        bundle = null;
        if (scene.HasSkinning)
            return false;
        ValidateMaximumTextureDimension(maximumTextureDimension);

        var meshesByTexture = new Dictionary<int, List<ImportedMesh>>();
        var unresolvedMeshes = new List<string>();
        var unresolvedReferencedMeshes = new List<string>();
        foreach (ImportedMesh mesh in scene.Meshes)
        {
            if (mesh.MaterialIndex < 0 || mesh.MaterialIndex >= scene.Materials.Count)
            {
                unresolvedMeshes.Add(mesh.Name);
                continue;
            }

            ImportedMaterial material = scene.Materials[mesh.MaterialIndex];
            int textureIndex = material.BaseColorTextureIndex;
            if (textureIndex < 0)
            {
                unresolvedMeshes.Add(mesh.Name);
                if (!string.IsNullOrWhiteSpace(material.BaseColorTextureName))
                    unresolvedReferencedMeshes.Add(mesh.Name);
                continue;
            }
            if (textureIndex >= scene.Textures.Count)
            {
                throw new InvalidDataException(
                    $"Material '{material.Name}' references missing texture index {textureIndex}.");
            }
            if (!meshesByTexture.TryGetValue(textureIndex, out List<ImportedMesh>? meshes))
                meshesByTexture.Add(textureIndex, meshes = []);
            meshes.Add(mesh);
        }

        if (meshesByTexture.Count < 2)
        {
            if (meshesByTexture.Count == 1 && unresolvedReferencedMeshes.Count > 0)
            {
                throw new InvalidDataException(
                    "The model has a resolved texture group, but these meshes still " +
                    "reference missing image payloads: " +
                    string.Join(", ", unresolvedReferencedMeshes) + ".");
            }
            return false;
        }
        if (unresolvedMeshes.Count > 0)
        {
            throw new InvalidDataException(
                "A model with several textures also contains meshes without a resolved " +
                "base-color texture: " + string.Join(", ", unresolvedMeshes) + ". " +
                "Add the missing texture files or fix the material references before import.");
        }

        string fullModelPath = Path.GetFullPath(modelPath);
        string modelDirectory = Path.GetDirectoryName(fullModelPath) ??
            Directory.GetCurrentDirectory();
        var groups = new List<RigidMaterialGroup>(meshesByTexture.Count);
        int materialNumber = 1;
        foreach ((int textureIndex, List<ImportedMesh> meshes) in
                 meshesByTexture.OrderBy(pair => pair.Key))
        {
            ImportedTexture source = scene.Textures[textureIndex];
            RigidTextureFrame frame = ReadTextureFrame(
                source,
                source.SourcePath ?? $"embedded:{source.Name}",
                materialNumber,
                frameNumber: 0,
                maximumTextureDimension);
            groups.Add(new RigidMaterialGroup(
                materialNumber,
                MaterialName(materialNumber),
                meshes.AsReadOnly(),
                new[] { frame }));
            materialNumber++;
        }

        HashSet<int> usedTextureIndices = meshesByTexture.Keys.ToHashSet();
        string[] unusedTextures = scene.Textures
            .Select((texture, index) => (texture, index))
            .Where(item => !usedTextureIndices.Contains(item.index))
            .Select(item => item.texture.SourcePath is null
                ? item.texture.Name
                : Path.GetFileName(item.texture.SourcePath))
            .ToArray();
        bundle = new RigidGlbTextureBundle(
            fullModelPath,
            modelDirectory,
            scene,
            groups.AsReadOnly(),
            [],
            unusedTextures);
        return true;
    }

    private static RigidGlbTextureBundle ReadScene(
        string fullModelPath,
        ImportedScene scene,
        string? textureDirectory,
        int maximumTextureDimension)
    {
        ValidateMaximumTextureDimension(maximumTextureDimension);

        if (!File.Exists(fullModelPath))
            throw new FileNotFoundException("Rigid model file was not found.", fullModelPath);
        string fullTextureDirectory = Path.GetFullPath(textureDirectory ??
            Path.GetDirectoryName(fullModelPath) ?? Directory.GetCurrentDirectory());
        if (!Directory.Exists(fullTextureDirectory))
            throw new DirectoryNotFoundException(
                $"Texture directory was not found: {fullTextureDirectory}");

        if (scene.Meshes.Count == 0)
            throw new InvalidDataException("Rigid model contains no meshes.");
        SortedDictionary<int, List<ImportedMesh>> meshesByMaterial =
            GroupMeshesByMaterial(scene, out IReadOnlyList<string> ignoredMeshes);
        if (meshesByMaterial.Values.SelectMany(meshes => meshes)
            .Any(mesh => mesh.Skinning is not null))
        {
            throw new RigidTextureBundleContainsSkinnedMeshesException(
                "A matN mesh in the rigid texture bundle contains JOINTS_0/WEIGHTS_0 skinning.");
        }
        SortedDictionary<int, SortedDictionary<int, string>> filesByMaterial =
            GroupTextureFiles(
                fullTextureDirectory,
                meshesByMaterial.Keys.ToHashSet(),
                out IReadOnlyList<string> ignoredTextureFiles);

        var groups = new List<RigidMaterialGroup>(meshesByMaterial.Count);
        foreach ((int materialNumber, List<ImportedMesh> meshes) in meshesByMaterial)
        {
            if (!filesByMaterial.TryGetValue(materialNumber, out SortedDictionary<int, string>? files))
                throw new InvalidDataException(
                    $"Material {MaterialName(materialNumber)} has model meshes but no PNG files.");
            if (!files.ContainsKey(0))
                throw new InvalidDataException(
                    $"Material {MaterialName(materialNumber)} has no base _mat{materialNumber}.png frame.");

            int expectedFrame = 0;
            var frames = new List<RigidTextureFrame>(files.Count);
            foreach ((int frameNumber, string path) in files)
            {
                if (frameNumber != expectedFrame)
                    throw new InvalidDataException(
                        $"Material {MaterialName(materialNumber)} is missing frame {expectedFrame}; " +
                        $"found frame {frameNumber} instead.");
                frames.Add(ReadTextureFrame(
                    path, materialNumber, frameNumber, maximumTextureDimension));
                expectedFrame++;
            }

            groups.Add(new RigidMaterialGroup(
                materialNumber,
                MaterialName(materialNumber),
                meshes.AsReadOnly(),
                frames.AsReadOnly()));
        }

        ImportedMesh[] activeMeshes = groups
            .SelectMany(group => group.Meshes)
            .ToArray();
        var activeScene = new ImportedScene(
            activeMeshes,
            scene.Textures,
            scene.Materials);
        return new RigidGlbTextureBundle(
            fullModelPath,
            fullTextureDirectory,
            activeScene,
            groups.AsReadOnly(),
            ignoredMeshes,
            ignoredTextureFiles);
    }

    private static SortedDictionary<int, List<ImportedMesh>> GroupMeshesByMaterial(
        ImportedScene scene,
        out IReadOnlyList<string> ignoredMeshes)
    {
        var result = new SortedDictionary<int, List<ImportedMesh>>();
        var ignored = new List<string>();
        foreach (ImportedMesh mesh in scene.Meshes)
        {
            var candidates = new List<string>();
            if (mesh.MaterialIndex >= 0 && mesh.MaterialIndex < scene.Materials.Count)
            {
                ImportedMaterial material = scene.Materials[mesh.MaterialIndex];
                candidates.Add(material.Name);
                if (!string.IsNullOrWhiteSpace(material.BaseColorTextureName))
                    candidates.Add(material.BaseColorTextureName);
            }
            candidates.Add(mesh.Name);
            int[] materialNumbers = candidates
                .SelectMany(value => NodeMaterialPattern.Matches(value).Cast<Match>())
                .Select(match => ParseNumber(match.Groups["material"].Value,
                    $"material number for mesh '{mesh.Name}'"))
                .Distinct()
                .ToArray();
            if (materialNumbers.Length == 0)
            {
                ignored.Add(mesh.Name);
                continue;
            }
            if (materialNumbers.Length != 1)
                throw new InvalidDataException(
                    $"Model mesh '{mesh.Name}' maps to multiple materials: " +
                    string.Join(", ", materialNumbers.Select(MaterialName)) + ".");

            int materialNumber = materialNumbers[0];
            if (!result.TryGetValue(materialNumber, out List<ImportedMesh>? materialMeshes))
                result.Add(materialNumber, materialMeshes = []);
            materialMeshes.Add(mesh);
        }
        if (result.Count == 0)
            throw new InvalidDataException(
                "No model mesh maps to a matN material or texture name.");
        ignoredMeshes = ignored.AsReadOnly();
        return result;
    }

    private static SortedDictionary<int, SortedDictionary<int, string>> GroupTextureFiles(
        string textureDirectory,
        IReadOnlySet<int> expectedMaterials,
        out IReadOnlyList<string> ignoredTextureFiles)
    {
        string[] pngFiles = Directory.EnumerateFiles(
                textureDirectory, "*", SearchOption.TopDirectoryOnly)
            .Where(path => Path.GetExtension(path).Equals(".png", StringComparison.OrdinalIgnoreCase))
            .OrderBy(path => Path.GetFileName(path), StringComparer.OrdinalIgnoreCase)
            .ThenBy(path => path, StringComparer.Ordinal)
            .ToArray();
        if (pngFiles.Length == 0)
            throw new InvalidDataException(
                $"Texture directory contains no PNG files: {textureDirectory}");

        var result = new SortedDictionary<int, SortedDictionary<int, string>>();
        var ignored = new List<string>();
        foreach (string path in pngFiles)
        {
            string fileName = Path.GetFileName(path);
            Match match = TextureFilePattern.Match(fileName);
            if (!match.Success)
            {
                ignored.Add(fileName);
                continue;
            }

            int materialNumber = ParseNumber(
                match.Groups["material"].Value, $"material number in PNG '{fileName}'");
            if (!expectedMaterials.Contains(materialNumber))
            {
                ignored.Add(fileName);
                continue;
            }
            int frameNumber = match.Groups["frame"].Success
                ? ParseNumber(match.Groups["frame"].Value, $"frame number in PNG '{fileName}'")
                : 0;
            if (!result.TryGetValue(materialNumber, out SortedDictionary<int, string>? frames))
                result.Add(materialNumber, frames = []);
            if (!frames.TryAdd(frameNumber, Path.GetFullPath(path)))
                throw new InvalidDataException(
                    $"Material {MaterialName(materialNumber)} frame {frameNumber} is ambiguous: " +
                    $"'{Path.GetFileName(frames[frameNumber])}' and '{fileName}'.");
        }

        ignoredTextureFiles = ignored.AsReadOnly();
        return result;
    }

    private static RigidTextureFrame ReadTextureFrame(
        string path,
        int materialNumber,
        int frameNumber,
        int maximumTextureDimension)
    {
        byte[] sourceData = File.ReadAllBytes(path);
        if (!HasPngSignature(sourceData))
            throw new InvalidDataException($"Texture is not a PNG file: {path}");

        try
        {
            using Image<Rgba32> source = Image.Load<Rgba32>(sourceData);
            var imported = new ImportedTexture(
                Path.GetFileNameWithoutExtension(path),
                "image/png",
                source.Width,
                source.Height,
                sourceData,
                Path.GetFullPath(path));
            return NormalizeTextureFrame(
                imported,
                Path.GetFullPath(path),
                materialNumber,
                frameNumber,
                maximumTextureDimension,
                source);
        }
        catch (UnknownImageFormatException exception)
        {
            throw new InvalidDataException($"Texture is not a valid PNG file: {path}", exception);
        }
    }

    private static RigidTextureFrame ReadTextureFrame(
        ImportedTexture imported,
        string sourceDescription,
        int materialNumber,
        int frameNumber,
        int maximumTextureDimension)
    {
        try
        {
            using Image<Rgba32> source = Image.Load<Rgba32>(imported.Data);
            if (source.Width != imported.Width || source.Height != imported.Height)
            {
                throw new InvalidDataException(
                    $"Texture '{sourceDescription}' declares {imported.Width}x{imported.Height}, " +
                    $"but its image is {source.Width}x{source.Height}.");
            }
            return NormalizeTextureFrame(
                imported,
                sourceDescription,
                materialNumber,
                frameNumber,
                maximumTextureDimension,
                source);
        }
        catch (UnknownImageFormatException exception)
        {
            throw new InvalidDataException(
                $"Texture is not a supported image: {sourceDescription}", exception);
        }
    }

    private static RigidTextureFrame NormalizeTextureFrame(
        ImportedTexture imported,
        string sourceDescription,
        int materialNumber,
        int frameNumber,
        int maximumTextureDimension,
        Image<Rgba32> source)
    {
        int sourceWidth = source.Width;
        int sourceHeight = source.Height;
        if (sourceWidth > maximumTextureDimension || sourceHeight > maximumTextureDimension)
        {
            throw new InvalidDataException(
                $"Texture '{sourceDescription}' is {sourceWidth}x{sourceHeight}; " +
                $"maximum is {maximumTextureDimension}x{maximumTextureDimension}. " +
                "Downscaling is forbidden.");
        }

        int width = CeilingPowerOfTwo(sourceWidth);
        int height = CeilingPowerOfTwo(sourceHeight);
        if (width > maximumTextureDimension || height > maximumTextureDimension)
        {
            throw new InvalidDataException(
                $"Texture '{sourceDescription}' requires a {width}x{height} POT upscale, " +
                $"which exceeds the {maximumTextureDimension}x{maximumTextureDimension} maximum.");
        }

        bool wasUpscaled = width != sourceWidth || height != sourceHeight;
        byte[] normalizedData = wasUpscaled
            ? EncodeAlphaAwareUpscale(source, width, height)
            : imported.Data;
        string logicalName = frameNumber == 0
            ? MaterialName(materialNumber)
            : $"{MaterialName(materialNumber)}.{frameNumber}";
        var texture = new ImportedTexture(
            logicalName,
            wasUpscaled ? "image/png" : imported.MimeType,
            width,
            height,
            normalizedData,
            imported.SourcePath);
        return new RigidTextureFrame(
            frameNumber,
            sourceDescription,
            sourceWidth,
            sourceHeight,
            wasUpscaled,
            texture);
    }

    private static void ValidateMaximumTextureDimension(int maximumTextureDimension)
    {
        if (maximumTextureDimension is < 1 or > AbsoluteMaximumTextureDimension)
        {
            throw new ArgumentOutOfRangeException(nameof(maximumTextureDimension),
                $"Maximum texture dimension must be between 1 and " +
                $"{AbsoluteMaximumTextureDimension}.");
        }
    }

    private static byte[] EncodeAlphaAwareUpscale(
        Image<Rgba32> source,
        int width,
        int height)
    {
        var sourcePixels = new Rgba32[checked(source.Width * source.Height)];
        source.ProcessPixelRows(accessor =>
        {
            for (int y = 0; y < source.Height; y++)
                accessor.GetRowSpan(y).CopyTo(
                    sourcePixels.AsSpan(y * source.Width, source.Width));
        });

        using var output = new Image<Rgba32>(width, height);
        output.ProcessPixelRows(accessor =>
        {
            for (int y = 0; y < height; y++)
            {
                Span<Rgba32> row = accessor.GetRowSpan(y);
                float sourceY = Math.Clamp(
                    (y + 0.5f) * source.Height / height - 0.5f,
                    0f,
                    source.Height - 1f);
                int y0 = (int)MathF.Floor(sourceY);
                int y1 = Math.Min(y0 + 1, source.Height - 1);
                float fy = sourceY - y0;
                for (int x = 0; x < width; x++)
                {
                    float sourceX = Math.Clamp(
                        (x + 0.5f) * source.Width / width - 0.5f,
                        0f,
                        source.Width - 1f);
                    int x0 = (int)MathF.Floor(sourceX);
                    int x1 = Math.Min(x0 + 1, source.Width - 1);
                    float fx = sourceX - x0;
                    row[x] = BlendPremultiplied(
                        sourcePixels[y0 * source.Width + x0],
                        sourcePixels[y0 * source.Width + x1],
                        sourcePixels[y1 * source.Width + x0],
                        sourcePixels[y1 * source.Width + x1],
                        fx,
                        fy);
                }
            }
        });

        using var encoded = new MemoryStream();
        output.SaveAsPng(encoded, new PngEncoder
        {
            ColorType = PngColorType.RgbWithAlpha,
            BitDepth = PngBitDepth.Bit8
        });
        return encoded.ToArray();
    }

    private static Rgba32 BlendPremultiplied(
        Rgba32 topLeft,
        Rgba32 topRight,
        Rgba32 bottomLeft,
        Rgba32 bottomRight,
        float x,
        float y)
    {
        float w00 = (1f - x) * (1f - y);
        float w10 = x * (1f - y);
        float w01 = (1f - x) * y;
        float w11 = x * y;
        float alpha = topLeft.A * w00 + topRight.A * w10 +
            bottomLeft.A * w01 + bottomRight.A * w11;

        byte BlendChannel(byte c00, byte c10, byte c01, byte c11)
        {
            float premultiplied = c00 * topLeft.A * w00 + c10 * topRight.A * w10 +
                c01 * bottomLeft.A * w01 + c11 * bottomRight.A * w11;
            if (alpha > float.Epsilon)
                return ToByte(premultiplied / alpha);
            return ToByte(c00 * w00 + c10 * w10 + c01 * w01 + c11 * w11);
        }

        return new Rgba32(
            BlendChannel(topLeft.R, topRight.R, bottomLeft.R, bottomRight.R),
            BlendChannel(topLeft.G, topRight.G, bottomLeft.G, bottomRight.G),
            BlendChannel(topLeft.B, topRight.B, bottomLeft.B, bottomRight.B),
            ToByte(alpha));
    }

    private static byte ToByte(float value) =>
        (byte)Math.Clamp((int)MathF.Round(value), 0, byte.MaxValue);

    private static int CeilingPowerOfTwo(int value)
    {
        if (value < 1)
            throw new InvalidDataException("PNG dimensions must be positive.");
        int result = 1;
        while (result < value)
            result = checked(result << 1);
        return result;
    }

    private static int ParseNumber(string value, string description)
    {
        if (!int.TryParse(value, out int result) || result < 1)
            throw new InvalidDataException($"Invalid {description}: '{value}'.");
        return result;
    }

    private static bool HasPngSignature(ReadOnlySpan<byte> data) =>
        data.Length >= 8 && data[..8].SequenceEqual(
            new byte[] { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A });

    private static string MaterialName(int materialNumber) => $"mat{materialNumber}";
}
