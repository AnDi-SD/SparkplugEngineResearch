using System.Numerics;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.Formats.Png;
using SixLabors.ImageSharp.PixelFormats;
using SixLabors.ImageSharp.Processing;

namespace SmoImporter.Core;

internal sealed record ImportedTextureAtlasSourceGroup(
    ImportedTexture Texture,
    IReadOnlyList<int> MeshIndices,
    string DisplayName);

internal sealed record ImportedTextureAtlasRepackResult(
    ImportedScene Scene,
    int AtlasTextureIndex,
    bool UsesTransparency,
    IReadOnlyList<string> Messages);

/// <summary>
/// Preserves every source material image and its mesh assignment when a target
/// SMO exposes fewer texture branches than an imported scene. Source images are
/// resized into one deterministic fixed-size atlas and every affected UV stream is moved
/// into its assigned atlas rectangle. A wrapped gutter keeps the small negative
/// and greater-than-one seam coordinates used by character rips local to their
/// own image instead of sampling a neighbouring material.
/// </summary>
internal static class ImportedTextureAtlasRepacker
{
    private const int WrappedGutter = 4;

    public static ImportedTextureAtlasRepackResult RepackToSingleAtlas(
        ImportedScene source,
        IReadOnlyList<ImportedTextureAtlasSourceGroup> sourceGroups,
        int atlasWidth,
        int atlasHeight)
    {
        ArgumentNullException.ThrowIfNull(source);
        ArgumentNullException.ThrowIfNull(sourceGroups);
        if (sourceGroups.Count < 2)
            throw new ArgumentException(
                "Texture atlas repacking requires at least two source groups.",
                nameof(sourceGroups));
        if (atlasWidth <= WrappedGutter * 2 || atlasHeight <= WrappedGutter * 2)
            throw new InvalidOperationException(
                $"Target atlas {atlasWidth}x{atlasHeight} is too small for wrapped gutters.");

        ValidateGroupCoverage(source, sourceGroups);
        bool[] transparencyBySourceGroup = sourceGroups
            .Select(group => SourceGroupContainsTransparency(group))
            .ToArray();
        string[] transparentGroups = sourceGroups
            .Where((_, index) => transparencyBySourceGroup[index])
            .Select(group => group.DisplayName)
            .ToArray();
        string[] opaqueGroups = sourceGroups
            .Where((_, index) => !transparencyBySourceGroup[index])
            .Select(group => group.DisplayName)
            .ToArray();

        (int columns, int rows) = SelectGrid(sourceGroups, atlasWidth, atlasHeight);
        using var atlas = new Image<Rgba32>(atlasWidth, atlasHeight, new Rgba32(0, 0, 0, 0));
        var placementByMesh = new Dictionary<int, AtlasPlacement>();
        bool usesTransparency = transparencyBySourceGroup.Any(value => value);

        for (int ordinal = 0; ordinal < sourceGroups.Count; ordinal++)
        {
            ImportedTextureAtlasSourceGroup group = sourceGroups[ordinal];
            int column = ordinal % columns;
            int row = ordinal / columns;
            int cellLeft = column * atlasWidth / columns;
            int cellRight = (column + 1) * atlasWidth / columns;
            int cellTop = row * atlasHeight / rows;
            int cellBottom = (row + 1) * atlasHeight / rows;
            var placement = new AtlasPlacement(
                cellLeft + WrappedGutter,
                cellTop + WrappedGutter,
                cellRight - cellLeft - WrappedGutter * 2,
                cellBottom - cellTop - WrappedGutter * 2,
                cellLeft,
                cellTop,
                cellRight,
                cellBottom);
            if (placement.Width <= 0 || placement.Height <= 0)
                throw new InvalidOperationException(
                    $"Target atlas {atlasWidth}x{atlasHeight} cannot hold " +
                    $"{sourceGroups.Count} material groups with wrapped gutters.");

            using Image<Rgba32> decoded = LoadAndValidate(group.Texture);
            using Image<Rgba32> resized = decoded.Clone(context => context.Resize(
                new ResizeOptions
                {
                    Size = new Size(placement.Width, placement.Height),
                    Mode = ResizeMode.Stretch,
                    Sampler = KnownResamplers.Bicubic,
                    PremultiplyAlpha = true
                }));
            CopyWithWrappedGutter(atlas, resized, placement);
            VerifyWrappedSamples(atlas, resized, placement, group.DisplayName);

            foreach (int meshIndex in group.MeshIndices)
            {
                if (!placementByMesh.TryAdd(meshIndex, placement))
                    throw new InvalidDataException(
                        $"Imported mesh {meshIndex} belongs to more than one texture group.");
            }
        }

        byte[] encoded;
        using (var stream = new MemoryStream())
        {
            atlas.SaveAsPng(stream, new PngEncoder
            {
                ColorType = PngColorType.RgbWithAlpha
            });
            encoded = stream.ToArray();
        }
        VerifyEncodedAtlas(atlas, encoded);

        var atlasTexture = new ImportedTexture(
            "__smo_import_atlas.png",
            "image/png",
            atlasWidth,
            atlasHeight,
            encoded);
        ImportedMaterial[] materials = source.Materials.Count == 0
            ? [new ImportedMaterial("__smo_import_atlas", atlasTexture.Name, 0)]
            : source.Materials
                .Select(material => material with
                {
                    BaseColorTextureName = atlasTexture.Name,
                    BaseColorTextureIndex = 0
                })
                .ToArray();
        ImportedMesh[] meshes = source.Meshes.Select((mesh, index) =>
        {
            if (!placementByMesh.TryGetValue(index, out AtlasPlacement placement))
                throw new InvalidDataException(
                    $"Imported mesh {index} ({mesh.Name}) has no atlas placement.");
            if (mesh.TextureCoordinates.Length != mesh.Positions.Length)
            {
                throw new InvalidDataException(
                    $"Imported mesh {index} ({mesh.Name}) has no complete TEXCOORD_0 stream.");
            }
            Vector2[] remapped = mesh.TextureCoordinates
                .Select(uv => RemapUv(
                    uv, placement, atlasWidth, atlasHeight, mesh.Name))
                .ToArray();
            return mesh with
            {
                TextureCoordinates = remapped,
                MaterialIndex = source.Materials.Count == 0 ? 0 : mesh.MaterialIndex
            };
        }).ToArray();
        ImportedScene result = source with
        {
            Meshes = meshes,
            EmbeddedTextures = [atlasTexture],
            SourceMaterials = materials
        };
        bool mixesOpaqueAndTransparentGroups =
            transparentGroups.Length > 0 && opaqueGroups.Length > 0;
        string transparencyMessage = mixesOpaqueAndTransparentGroups
            ? "The shared donor atlas contains transparent source groups " +
              $"({string.Join(", ", transparentGroups)}) and fully opaque source " +
              $"groups ({string.Join(", ", opaqueGroups)}). Geometry consumers " +
              "must remain on separate opaque and alpha material/spSkin branches " +
              "even though both branches reference this same atlas."
            : usesTransparency
                ? "The donor atlas contains transparency in every source group; " +
                  "alpha-sampling geometry requires a separate alpha material/spSkin " +
                  "branch that references this shared atlas."
                : "The donor atlas is fully opaque.";
        string[] messages =
        [
            $"Packed {sourceGroups.Count} donor texture groups into a verified " +
            $"{atlasWidth}x{atlasHeight} RGBA atlas ({columns}x{rows} cells); " +
            "all material UVs were remapped with wrapped gutters.",
            transparencyMessage
        ];
        return new ImportedTextureAtlasRepackResult(
            result, 0, usesTransparency, messages);
    }

    private static bool SourceGroupContainsTransparency(
        ImportedTextureAtlasSourceGroup group)
    {
        using Image<Rgba32> decoded = LoadAndValidate(group.Texture);
        return ContainsTransparency(decoded);
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

    private static void ValidateGroupCoverage(
        ImportedScene source,
        IReadOnlyList<ImportedTextureAtlasSourceGroup> groups)
    {
        int[] meshIndices = groups.SelectMany(group => group.MeshIndices).Order().ToArray();
        int[] expected = Enumerable.Range(0, source.Meshes.Count).ToArray();
        if (!meshIndices.SequenceEqual(expected))
            throw new InvalidDataException(
                "Texture atlas source groups do not cover every imported mesh exactly once.");
        foreach (ImportedTextureAtlasSourceGroup group in groups)
        {
            if (group.Texture.Width <= 0 || group.Texture.Height <= 0 ||
                group.Texture.Data.Length == 0)
            {
                throw new InvalidDataException(
                    $"Texture {group.Texture.Name} has no valid image payload.");
            }
        }
    }

    private static (int Columns, int Rows) SelectGrid(
        IReadOnlyList<ImportedTextureAtlasSourceGroup> groups,
        int atlasWidth,
        int atlasHeight)
    {
        double weightedAspect = Math.Exp(groups
            .Select(group => Math.Log((double)group.Texture.Width / group.Texture.Height))
            .Average());
        return Enumerable.Range(1, groups.Count)
            .Select(columns =>
            {
                int rows = (groups.Count + columns - 1) / columns;
                int cellWidth = atlasWidth / columns - WrappedGutter * 2;
                int cellHeight = atlasHeight / rows - WrappedGutter * 2;
                double aspectPenalty = cellWidth > 0 && cellHeight > 0
                    ? Math.Abs(Math.Log((double)cellWidth / cellHeight / weightedAspect))
                    : double.PositiveInfinity;
                int minimumDimension = Math.Min(cellWidth, cellHeight);
                return (columns, rows, aspectPenalty, minimumDimension);
            })
            .OrderBy(item => item.aspectPenalty)
            .ThenByDescending(item => item.minimumDimension)
            .ThenBy(item => item.columns)
            .Select(item => (item.columns, item.rows))
            .First();
    }

    private static Vector2 RemapUv(
        Vector2 uv,
        AtlasPlacement placement,
        int atlasWidth,
        int atlasHeight,
        string meshName)
    {
        if (!float.IsFinite(uv.X) || !float.IsFinite(uv.Y))
            throw new InvalidDataException(
                $"Mesh {meshName} contains a non-finite texture coordinate.");
        float localX = uv.X * placement.Width;
        float localY = uv.Y * placement.Height;
        if (localX < -WrappedGutter || localX > placement.Width + WrappedGutter ||
            localY < -WrappedGutter || localY > placement.Height + WrappedGutter)
        {
            throw new InvalidOperationException(
                $"Mesh {meshName} uses UV ({uv.X:R}, {uv.Y:R}) outside the " +
                $"verified wrapped atlas gutter. Normalize or unwrap this material first.");
        }
        return new Vector2(
            (placement.X + localX) / atlasWidth,
            (placement.Y + localY) / atlasHeight);
    }

    private static bool ContainsTransparency(Image<Rgba32> image)
    {
        bool result = false;
        image.ProcessPixelRows(accessor =>
        {
            for (int y = 0; y < accessor.Height && !result; y++)
            {
                foreach (Rgba32 pixel in accessor.GetRowSpan(y))
                {
                    if (pixel.A < byte.MaxValue)
                    {
                        result = true;
                        break;
                    }
                }
            }
        });
        return result;
    }

    private static void CopyWithWrappedGutter(
        Image<Rgba32> atlas,
        Image<Rgba32> source,
        AtlasPlacement placement)
    {
        atlas.ProcessPixelRows(source, (atlasAccessor, sourceAccessor) =>
        {
            for (int y = placement.CellTop; y < placement.CellBottom; y++)
            {
                Span<Rgba32> targetRow = atlasAccessor.GetRowSpan(y);
                int sourceY = Mod(y - placement.Y, source.Height);
                ReadOnlySpan<Rgba32> sourceRow = sourceAccessor.GetRowSpan(sourceY);
                for (int x = placement.CellLeft; x < placement.CellRight; x++)
                {
                    int sourceX = Mod(x - placement.X, source.Width);
                    targetRow[x] = sourceRow[sourceX];
                }
            }
        });
    }

    private static void VerifyWrappedSamples(
        Image<Rgba32> atlas,
        Image<Rgba32> source,
        AtlasPlacement placement,
        string displayName)
    {
        (int TargetX, int TargetY, int SourceX, int SourceY)[] probes =
        [
            (placement.X, placement.Y, 0, 0),
            (placement.X + placement.Width / 2,
                placement.Y + placement.Height / 2,
                placement.Width / 2,
                placement.Height / 2),
            (placement.X - 1, placement.Y + placement.Height / 2,
                placement.Width - 1, placement.Height / 2),
            (placement.X + placement.Width, placement.Y + placement.Height / 2,
                0, placement.Height / 2),
            (placement.X + placement.Width / 2, placement.Y - 1,
                placement.Width / 2, placement.Height - 1),
            (placement.X + placement.Width / 2, placement.Y + placement.Height,
                placement.Width / 2, 0)
        ];
        foreach (var probe in probes)
        {
            if (atlas[probe.TargetX, probe.TargetY] != source[probe.SourceX, probe.SourceY])
            {
                throw new InvalidDataException(
                    $"Atlas sample verification failed for {displayName} at " +
                    $"({probe.TargetX}, {probe.TargetY}).");
            }
        }
    }

    private static void VerifyEncodedAtlas(Image<Rgba32> expected, byte[] encoded)
    {
        using Image<Rgba32> decoded = Image.Load<Rgba32>(encoded);
        if (decoded.Width != expected.Width || decoded.Height != expected.Height)
            throw new InvalidDataException("Encoded atlas dimensions changed during PNG roundtrip.");
        (int X, int Y)[] probes =
        [
            (0, 0),
            (expected.Width / 2, expected.Height / 2),
            (expected.Width - 1, expected.Height - 1)
        ];
        if (probes.Any(probe => decoded[probe.X, probe.Y] != expected[probe.X, probe.Y]))
            throw new InvalidDataException("Encoded atlas RGBA samples changed during PNG roundtrip.");
    }

    public static bool SerializedBgraMatches(
        ReadOnlySpan<byte> encodedImage,
        int expectedWidth,
        int expectedHeight,
        ReadOnlySpan<byte> serializedBgra,
        out string error)
    {
        using Image<Rgba32> image = Image.Load<Rgba32>(encodedImage);
        if (image.Width != expectedWidth || image.Height != expectedHeight)
        {
            error = $"source image is {image.Width}x{image.Height}, expected " +
                    $"{expectedWidth}x{expectedHeight}";
            return false;
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

    private static int Mod(int value, int modulus)
    {
        int result = value % modulus;
        return result < 0 ? result + modulus : result;
    }

    private readonly record struct AtlasPlacement(
        int X,
        int Y,
        int Width,
        int Height,
        int CellLeft,
        int CellTop,
        int CellRight,
        int CellBottom);
}
