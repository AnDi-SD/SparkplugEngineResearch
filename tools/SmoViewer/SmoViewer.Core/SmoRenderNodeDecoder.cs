namespace SmoViewer.Core;

/// <summary>
/// Effective inherited node state plus the repeated renderable relationships
/// owned by <c>spRenderNodeSerializer</c>.
/// </summary>
public sealed record SmoRenderNodeData(
    SmoNodeData Node,
    IReadOnlyList<SmoNodeRelationship> Renderables);

/// <summary>
/// Strict read-only decoder for the two confirmed <c>spRenderNode</c>
/// serializer sections: inherited <c>spNode</c> followed by the repeated
/// <c>esfRenderNodeRenderable</c> field.
/// </summary>
public static class SmoRenderNodeDecoder
{
    public static bool TryDecode(
        SmoDocument document,
        SmoObjectEntry entry,
        out SmoRenderNodeData? renderNode)
        => TryDecodeFields(document, entry, SmoClassIds.RenderNode, "render_node.renderable", out renderNode);

    // Metadata projection of the one shared serializer's two registrations.
    // Actual reference resolution/ownership is performed by ResourceGraph.
    internal static bool TryDecodeFields(SmoDocument document,SmoObjectEntry entry,
        uint expectedClass,string relationshipKey,out SmoRenderNodeData? renderNode)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(entry);
        renderNode = null;
        if (entry.TypeHash != expectedClass ||
            !SmoNodeDecoder.TryDecodeNodeSection(
                document,entry,out SmoNodeData? node) ||
            node is null ||
            !SmoObjectFieldReader.TryRead(
                document,entry,out IReadOnlyList<SmoObjectField>? fields,out _))
        {
            return false;
        }

        var renderables = new List<SmoNodeRelationship>();
        for (int index = 0; index < fields.Count; index++)
        {
            SmoObjectField field = fields[index];
            if (!SmoSerializedFieldRegistry.TryDescribeField(
                    entry.TypeHash,fields,index,
                    out SmoSerializedFieldDescriptor? descriptor) ||
                descriptor?.Key != relationshipKey)
            {
                continue;
            }
            if (!SmoNodeDecoder.TryDecodeRelationship(
                    document,field.Payload.Span,
                    out SmoNodeRelationship? relationship) ||
                relationship is null)
            {
                return false;
            }
            renderables.Add(relationship);
        }

        renderNode = new SmoRenderNodeData(node,renderables.AsReadOnly());
        return true;
    }
}
