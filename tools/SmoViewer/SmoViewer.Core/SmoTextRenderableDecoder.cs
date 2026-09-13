using System.Diagnostics.CodeAnalysis;
using System.Text;
using SmoViewer.Sparkplug;
namespace SmoViewer.Core;

/// <summary>Shared Text serializer observation. References remain catalog
/// metadata; this operation does not execute layout. Actual runtime state is
/// available from SmoLoadedResources.Texts.</summary>
public static class SmoTextRenderableDecoder
{
    public static unsafe bool TryDecode(SmoDocument document, SmoObjectEntry entry,
        [NotNullWhen(true)] out SmoTextRenderableData? value, out string error)
    {
        ArgumentNullException.ThrowIfNull(document); ArgumentNullException.ThrowIfNull(entry);
        value = null; error = string.Empty;
        if ((uint)entry.Index >= (uint)document.Objects.Count || !ReferenceEquals(document.Objects[entry.Index], entry) ||
            entry.TypeHash != SmoClassIds.TextRenderable || !entry.IsWithinDataSection || !entry.SignatureMatches ||
            entry.PhysicalOffset < 0 || entry.SerializedSize < 10 || entry.SerializedSize > int.MaxValue ||
            entry.PhysicalEnd > document.Data.Length)
        {
            error = "Text inspector requires a complete catalogued TextRenderable.";
            return false;
        }
        var payload = document.Data.Span.Slice(checked((int)entry.PhysicalOffset + 8), checked((int)entry.SerializedSize - 8));
        try
        {
            IntPtr pointer;
            fixed (byte* bytes = payload)
                pointer = NativeMethods.Check(NativeMethods.spv_text_inspection_read(bytes, (uint)payload.Length));
            using var handle = new TextInspectionHandle(pointer);
            NativeMethods.Check(NativeMethods.spv_text_inspection_info(handle, out var info));
            uint trailing = (info.TextFlags & 2) == 0 ? 0u : 1u;
            if (info.TextLength > ushort.MaxValue || info.TextByteCount != info.TextLength + trailing)
                throw new InvalidDataException("Text byte-string observation has an invalid extent.");
            byte[] raw = new byte[checked((int)info.TextByteCount)];
            fixed (byte* bytes = raw)
                NativeMethods.Check(NativeMethods.spv_text_inspection_bytes(handle, bytes, info.TextLength));
            if (!SmoModelInspection.TryRenderableFields(document, entry, info.RenderableMask, info.Alpha, info.Priority,
                    info.Material, info.Fog, out var renderable, out error) ||
                !SmoModelInspection.TryReference(document, entry, info.Font, SmoClassIds.Font, out var font, out error)) return false;
            font ??= new SmoNodeRelationship(0, 0, SmoNodeRelationshipEncoding.IdOnly, null, null, null);
            if (font.Encoding == SmoNodeRelationshipEncoding.InlineObject && font.TargetObjectIndex is int fontIndex &&
                (document.Objects[fontIndex].PhysicalOffset != entry.PhysicalOffset + 8 + info.Font.Offset + 8 ||
                 document.Objects[fontIndex].SerializedSize != font.InlineSerializedSize))
            {
                error = "Inline Font does not match its catalogued extent.";
                return false;
            }
            // Display-only one-to-one byte projection. No universal game code
            // page has been proved. Keep complete raw wire bytes separately.
            int textEnd = raw.AsSpan().IndexOf((byte)0);
            if (textEnd < 0) textEnd = raw.Length;
            string display = Encoding.Latin1.GetString(raw.AsSpan(0, textEnd));
            value = new SmoTextRenderableData(renderable!, font, display, info.Color,
                (info.FieldMask & 4) == 0 ? null : info.WrapWidth,
                (info.FieldMask & 8) == 0 ? null : info.Alignment)
            {
                RawTextBytes = raw, TextWasNull = (info.TextFlags & 1) != 0,
                SerializedFieldMask = (byte)info.FieldMask
            };
            return true;
        }
        catch (InvalidDataException exception) { error = exception.Message; return false; }
    }
}
