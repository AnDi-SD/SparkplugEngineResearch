using System.Buffers.Binary;
using System.Diagnostics.CodeAnalysis;

namespace SmoViewer.Core;

/// <summary>
/// Produces a BGRA32 preview from the structurally decoded <c>spTextureData</c>
/// representations. The source reader selects the PC representation; native PS2
/// indexed/swizzled buffers remain available through
/// <see cref="SmoTextureDataDecoder"/> but are not guessed into a PC bitmap.
/// </summary>
public static class SmoTextureDecoder
{
    public static bool TryDecode(
        SmoDocument document,
        SmoObjectEntry entry,
        [NotNullWhen(true)] out SmoTexture? texture,
        out string error)
    {
        texture = null;
        if (!SmoTextureDataDecoder.TryDecode(
                document, entry, out SmoTextureDataInfo? data, out error) ||
            data is null)
        {
            return false;
        }

        SmoTextureRepresentationData? representation = data.SelectedRepresentation;
        if (representation is null || representation.MipLevels.Count == 0 ||
            representation.Kind is not (
                SmoTextureRepresentationKind.CrossPlatformBgra32 or SmoTextureRepresentationKind.CrossPlatformBgrx32 or
                SmoTextureRepresentationKind.Direct3DBgra32))
        {
            error = representation?.Kind is SmoTextureRepresentationKind.Ps2Indexed4 or
                SmoTextureRepresentationKind.Ps2Indexed8 or SmoTextureRepresentationKind.Ps2Rgba32 or SmoTextureRepresentationKind.Ps2Raw
                ? "PS2_TEXTURE_PREVIEW_UNSUPPORTED: native palette/swizzle data is not prepared for this BGRA32 backend."
                : "TEXTURE_PREVIEW_UNSUPPORTED: the stored pixel format is not exposed as BGRA32 by this preview adapter.";
            return false;
        }

        SmoTextureMipLevelData first = representation.MipLevels[0];
        ReadOnlyMemory<byte> preview = representation.BgraPreview.IsEmpty ? first.PixelData : representation.BgraPreview;
        int expectedLength = checked(first.Width * first.Height * 4);
        if (preview.Length != expectedLength)
        {
            error = "TEXTURE_PIXEL_BUFFER_SIZE_MISMATCH: the selected BGRA32 " +
                    "representation has an inconsistent base mip length.";
            return false;
        }

        ushort legacyHeaderSignature = 0;
        if (entry.PhysicalOffset >= 0 && entry.PhysicalOffset <= int.MaxValue &&
            entry.SerializedSize >= 10 && entry.PhysicalEnd <= document.Data.Length)
        {
            legacyHeaderSignature = BinaryPrimitives.ReadUInt16LittleEndian(
                document.Data.Span.Slice(checked((int)entry.PhysicalOffset + 8), 2));
        }
        texture = new SmoTexture(
            entry.Index,
            entry.Name,
            legacyHeaderSignature,
            first.Width,
            first.Height,
            SmoTextureLayout.Bgra,
            preview.ToArray(),
            representation.Kind,
            representation.MipLevels.Count,
            data.PlatformType);
        error = string.Empty;
        return true;
    }
}
