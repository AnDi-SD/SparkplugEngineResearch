using System.Diagnostics.CodeAnalysis;
using System.Numerics;

namespace SmoViewer.Core;

/// <summary>Actual borrowed portal list; no two-portal cardinality is invented.</summary>
public sealed record SmoZonePortalNodeData(SmoNodeData Node,IReadOnlyList<SmoLoadedReference> ZonePortals);

/// <summary>Inspection projection of the actual shared ResourceGraph; not a second serializer.</summary>
public static class SmoZonePortalNodeDecoder
{
    public static bool TryDecode(SmoDocument document,SmoObjectEntry entry,
        [NotNullWhen(true)] out SmoZonePortalNodeData? value,out string error)
    {
        ArgumentNullException.ThrowIfNull(document);ArgumentNullException.ThrowIfNull(entry);
        value=null;error=string.Empty;
        try
        {
            var snapshot=SmoSpatialProjection.Snapshot(document,entry,SmoClassIds.ZonePortalNode);
            var state=SmoSpatialProjection.Find(snapshot.PortalNodes,p=>p.Id,entry.Id);
            value=new(SmoNavigationProjection.Node(document,entry),SmoSpatialProjection.References(document,state.Portals));
            return true;
        }
        catch(InvalidDataException exception){error=exception.Message;return false;}
    }
}
