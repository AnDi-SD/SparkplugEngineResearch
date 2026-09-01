using SmoImporter.Core;
using SmoViewer.Core;

namespace SmoLVLcreator.Core;

public sealed record SmoProjectTextureReplacementResult(
    uint TextureObjectId,
    int TextureObjectIndex,
    int SerializedSize,
    bool ReplacedAlpha,
    Guid AssetId);

/// <summary>
/// Converts the verified native texture writer into a compact
/// project operation. Only the target TextureData SBOO is retained in the
/// journal; the temporary materialized SMO is released before returning.
/// </summary>
public static class SmoProjectTextureReplacement
{
    public static IReadOnlyList<uint> GetWritableTextureObjectIds(
        SmoProject project)
    {
        ArgumentNullException.ThrowIfNull(project);
        SmoDocument current = SmoProjectSerializer.CreateCurrentDocument(project);
        SMOTextureTool.Core.SmoDocument textureDocument =
            SMOTextureTool.Core.SmoDocument.Parse(current.Data.Span);
        HashSet<int> writableOffsets = textureDocument.Textures
            .Select(texture => texture.BlockOffset)
            .ToHashSet();
        return current.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.TextureData &&
                entry.PhysicalOffset is >= 0 and <= int.MaxValue &&
                writableOffsets.Contains(checked((int)entry.PhysicalOffset)))
            .Select(entry => entry.Id)
            .ToArray();
    }

    public static SmoProjectTextureReplacementResult Replace(
        SmoProject project,
        uint textureObjectId,
        ReadOnlySpan<byte> encodedImage,
        bool replaceAlpha)
    {
        ArgumentNullException.ThrowIfNull(project);
        if (encodedImage.IsEmpty)
            throw new ArgumentException("Replacement image is empty.", nameof(encodedImage));
        if (!project.CanUseObject(
                textureObjectId,
                SmoClassIds.TextureData,
                out string reason))
        {
            throw new ArgumentException(reason, nameof(textureObjectId));
        }

        SmoDocument current = SmoProjectSerializer.CreateCurrentDocument(project);
        SmoObjectEntry before = current.Objects.SingleOrDefault(entry =>
                entry.Id == textureObjectId && entry.TypeHash == SmoClassIds.TextureData)
            ?? throw new KeyNotFoundException(
                $"TextureData object ID {textureObjectId} is missing from the current project.");
        if (before.PhysicalOffset is < 0 or > int.MaxValue)
        {
            throw new InvalidDataException(
                $"TextureData object ID {textureObjectId} has an unsupported physical offset.");
        }
        SMOTextureTool.Core.SmoDocument textureDocument =
            SMOTextureTool.Core.SmoDocument.Parse(current.Data.Span);
        SMOTextureTool.Core.TextureInfo texture = textureDocument.Textures
            .SingleOrDefault(item =>
                item.BlockOffset == checked((int)before.PhysicalOffset))
            ?? throw new InvalidOperationException(
                $"TextureData object ID {textureObjectId} is not a decoded writable " +
                "fixed-size texture slot.");
        byte[] rewritten = replaceAlpha
            ? FixedSizeTextureWriter.ReplaceRgba(
                current.Data.ToArray(), texture.Index, encodedImage)
            : FixedSizeTextureWriter.ReplaceRgb(
                current.Data.ToArray(), texture.Index, encodedImage);
        SmoDocument verified = SmoDocument.ParseOwned(rewritten, current.SourcePath);
        if (verified.HasErrors)
            throw new InvalidDataException("Texture replacement produced an invalid SMO graph.");
        SmoObjectEntry after = verified.Objects.Single(entry =>
            entry.Id == textureObjectId && entry.TypeHash == SmoClassIds.TextureData);
        if (after.SerializedSize != before.SerializedSize)
        {
            throw new InvalidDataException(
                "Project texture replacement must preserve the existing texture slot size.");
        }
        ReadOnlySpan<byte> serialized = rewritten.AsSpan(
            checked((int)after.PhysicalOffset),
            checked((int)after.SerializedSize));
        Guid assetId = project.ReplaceObjectData(textureObjectId, serialized);
        return new SmoProjectTextureReplacementResult(
            textureObjectId,
            after.Index,
            serialized.Length,
            replaceAlpha,
            assetId);
    }
}
