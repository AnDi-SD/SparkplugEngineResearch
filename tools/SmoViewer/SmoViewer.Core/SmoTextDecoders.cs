using System.Diagnostics.CodeAnalysis;
using System.Numerics;

namespace SmoViewer.Core;

public sealed record SmoFontGlyph(byte Character,byte Width,Vector2 Uv0,Vector2 Uv1);
public sealed record SmoFontData(
    SmoNodeRelationship Image,uint Height,uint? Baseline,
    IReadOnlyList<SmoFontGlyph> Glyphs);
public sealed record SmoTextRenderableData(
    SmoRenderableData Renderable,SmoNodeRelationship Font,string Text,uint Color,
    uint? WrapWidth,uint? Alignment)
{
    /// <summary>Complete observed byte-string payload, including an authored trailing NUL.</summary>
    public ReadOnlyMemory<byte> RawTextBytes { get; init; }
    public bool TextWasNull { get; init; }
    public byte SerializedFieldMask { get; init; }
    public bool HasDerivedLayout => false;
}
public sealed record SmoTextNodeData(SmoRenderNodeData RenderNode);

/// <summary>TextNode uses the common RenderNode metadata projection, matching
/// original4423D0 delegation. This does not construct a runtime TextNode.</summary>
public static class SmoTextNodeDecoder
{
    public static bool TryDecode(SmoDocument document,SmoObjectEntry entry,
        [NotNullWhen(true)] out SmoTextNodeData? value,out string error)
    {
        ArgumentNullException.ThrowIfNull(document);ArgumentNullException.ThrowIfNull(entry);
        value=null;error=string.Empty;
        if ((uint)entry.Index >= (uint)document.Objects.Count || !ReferenceEquals(document.Objects[entry.Index],entry) ||
            !SmoRenderNodeDecoder.TryDecodeFields(document,entry,SmoClassIds.TextNode,
                "render_node.renderable",out var renderNode) || renderNode is null)
        {
            error="Cannot inspect TextNode through the common Node/RenderNode metadata reader.";
            return false;
        }
        value=new SmoTextNodeData(renderNode);return true;
    }
}
