using System.Diagnostics.CodeAnalysis;

namespace SmoViewer.Core;

/// <summary>
/// Metadata view of the inherited Node and ordered renderable relationships.
/// Models is the historical property name; the original shared serializer
/// requires spRenderable, without an observed-count or inline-only restriction.
/// </summary>
public sealed record SmoSkyBoxData(SmoNodeData Node,IReadOnlyList<SmoNodeRelationship> Models);

public static class SmoSkyBoxDecoder
{
    public static bool TryDecode(SmoDocument document,SmoObjectEntry entry,
        [NotNullWhen(true)] out SmoSkyBoxData? value,out string error)
    {
        value=null;error=string.Empty;
        if(!SmoRenderNodeDecoder.TryDecodeFields(document,entry,SmoClassIds.SkyBox,
            "sky_box.model",out var shared) || shared is null)
        {error="Cannot project SkyBox shared RenderNode serializer metadata.";return false;}
        value=new(shared.Node,shared.Renderables);return true;
    }
}
