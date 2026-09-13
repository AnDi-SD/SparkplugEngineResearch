using System.Diagnostics.CodeAnalysis;
using System.Numerics;

namespace SmoViewer.Core;

/// <summary>Actual borrowed root list; Node retains authored metadata from the common Node reader.</summary>
public sealed record SmoZoneData(SmoNodeData Node,IReadOnlyList<SmoLoadedReference> LocalPartitionRoots);

/// <summary>Inspection projection of the actual shared ResourceGraph; not a second serializer.</summary>
public static class SmoZoneDecoder
{
    public static bool TryDecode(SmoDocument document,SmoObjectEntry entry,
        [NotNullWhen(true)] out SmoZoneData? value,out string error)
    {
        ArgumentNullException.ThrowIfNull(document);ArgumentNullException.ThrowIfNull(entry);
        value=null;error=string.Empty;
        try
        {
            var snapshot=SmoSpatialProjection.Snapshot(document,entry,SmoClassIds.Zone);
            var state=SmoSpatialProjection.Find(snapshot.Zones,p=>p.Id,entry.Id);
            value=new(SmoNavigationProjection.Node(document,entry),SmoSpatialProjection.References(document,state.Roots));
            return true;
        }
        catch(InvalidDataException exception){error=exception.Message;return false;}
    }
}
