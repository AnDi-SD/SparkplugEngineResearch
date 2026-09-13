using System.Diagnostics.CodeAnalysis;

namespace SmoViewer.Core;

/// <summary>Actual loaded references; no serialized encoding or omission claim.</summary>
public sealed record SmoLoadedRenderableData(SmoObjectEntry? Material,SmoObjectEntry? Fog,
    uint AlphaSortEnable,uint Priority);
public sealed record SmoLensFlareElement(SmoObjectEntry? Material,uint Color,float RelativeDistance,float Scale);
public sealed record SmoLensFlareData(SmoLoadedRenderableData Renderable,SmoLensFlareElement Primary,
    IReadOnlyList<SmoLensFlareElement> Elements,float OcclusionSphereRadius,float OcclusionSpeed,SmoObjectEntry? RenderNode);

/// <summary>Projection of actual spLensFlare/spQuad state in the common graph.</summary>
public static class SmoLensFlareDecoder
{
    public static bool TryDecode(SmoDocument document,SmoObjectEntry entry,
        [NotNullWhen(true)] out SmoLensFlareData? value,out string error)
    {
        ArgumentNullException.ThrowIfNull(document);ArgumentNullException.ThrowIfNull(entry);
        value=null;error=string.Empty;
        if(entry.TypeHash!=SmoClassIds.LensFlare){error="Object is not spLensFlare.";return false;}
        var loaded=SmoLoadedResources.Get(document);
        if(loaded.LoadIssue is not null){error=loaded.LoadIssue;return false;}
        if(!loaded.LensFlares.TryGetValue(entry.Index,out value)){error="Actual LensFlare is absent from the loaded graph.";return false;}
        return true;
    }
}
