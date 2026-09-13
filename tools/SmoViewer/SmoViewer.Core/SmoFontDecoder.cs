using System.Diagnostics.CodeAnalysis;
using SmoViewer.Sparkplug;

namespace SmoViewer.Core;

/// <summary>Actual shared spFont reader; the atlas is bound to catalog metadata.
/// Raw glyph values are retained. This does not create a font renderer.</summary>
public static class SmoFontDecoder
{
    public const int FirstCharacter = 0x20, LastCharacter = 0xFF;
    public const int GlyphCount = LastCharacter - FirstCharacter + 1;
    public const int SerializedGlyphSize = 17;

    public static unsafe bool TryDecode(SmoDocument document, SmoObjectEntry entry,
        [NotNullWhen(true)] out SmoFontData? value, out string error)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(entry);
        value = null; error = string.Empty;
        if ((uint)entry.Index >= (uint)document.Objects.Count || !ReferenceEquals(document.Objects[entry.Index], entry) ||
            entry.TypeHash != SmoClassIds.Font || !entry.IsWithinDataSection || !entry.SignatureMatches ||
            entry.PhysicalOffset < 0 || entry.SerializedSize < 9 || entry.PhysicalEnd > document.Data.Length ||
            entry.SerializedSize > int.MaxValue)
        {
            error = "Font inspector requires a complete catalogued Font.";
            return false;
        }
        var payload = document.Data.Span.Slice(checked((int)entry.PhysicalOffset + 8), checked((int)entry.SerializedSize - 8));
        try
        {
            NativeMethods.FontInfo info;
            var nativeGlyphs = new NativeMethods.FontGlyph[GlyphCount];
            fixed (byte* bytes = payload)
            fixed (NativeMethods.FontGlyph* glyphs = nativeGlyphs)
                NativeMethods.Check(NativeMethods.spv_font_read(bytes, (uint)payload.Length, out info, glyphs, GlyphCount));
            var image = new SmoNodeRelationship(0, 0, SmoNodeRelationshipEncoding.IdOnly, null, null, null);
            if (info.HasImage != 0)
            {
                if ((ulong)info.ImageOffset + info.ImageSize > (ulong)payload.Length ||
                    !SmoNodeDecoder.TryDecodeRelationship(document,
                        payload.Slice((int)info.ImageOffset, (int)info.ImageSize), out var decoded) || decoded is null)
                {
                    error = "Font image observation does not bind to the document.";
                    return false;
                }
                image = decoded;
                // Host catalog type/extent guard. Original ReadReference does
                // not enforce requestedClassID; this is not a game validation.
                if (image.ObjectId != 0 &&
                    (!SmoNodeDecoder.TryGetCataloguedObject(document, image.ObjectId, out var target) ||
                     target.TypeHash != SmoClassIds.TextureData || !target.IsWithinDataSection || !target.SignatureMatches ||
                     (image.Encoding == SmoNodeRelationshipEncoding.InlineObject &&
                      (target.PhysicalOffset != entry.PhysicalOffset + 8 + info.ImageOffset + 8 ||
                       target.SerializedSize != image.InlineSerializedSize))))
                {
                    error = "Font image is not a complete catalogued TextureData or explicit NULL.";
                    return false;
                }
            }
            var result = new SmoFontGlyph[GlyphCount];
            for (int i = 0; i < result.Length; ++i)
                result[i] = new SmoFontGlyph((byte)(i + FirstCharacter), checked((byte)nativeGlyphs[i].Width),
                    nativeGlyphs[i].Uv0, nativeGlyphs[i].Uv1);
            value = new SmoFontData(image, info.Height, info.HasBaseline != 0 ? info.Baseline : null, Array.AsReadOnly(result));
            return true;
        }
        catch (InvalidDataException exception) { error = exception.Message; return false; }
    }
}
