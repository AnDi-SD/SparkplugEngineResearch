using System.Diagnostics.CodeAnalysis;
using System.Numerics;

namespace SmoViewer.Core;

/// <summary>Actual directed portal; raw OpenByte is preserved and destination is borrowed.</summary>
public sealed record SmoZonePortalData(SmoLoadedReference DestinationZone,IReadOnlyList<Vector3> Polygon,byte OpenByte)
{
    public bool IsOpen => OpenByte!=0;
    public SmoBspPlane? Plane { get; init; }
}

/// <summary>Inspection projection of the actual shared ResourceGraph; not a second serializer.</summary>
public static class SmoZonePortalDecoder
{
    public static bool TryDecode(SmoDocument document,SmoObjectEntry entry,
        [NotNullWhen(true)] out SmoZonePortalData? value,out string error)
    {
        ArgumentNullException.ThrowIfNull(document);ArgumentNullException.ThrowIfNull(entry);
        value=null;error=string.Empty;
        try
        {
            var snapshot=SmoSpatialProjection.Snapshot(document,entry,SmoClassIds.ZonePortal);
            var state=SmoSpatialProjection.Find(snapshot.Portals,p=>p.Id,entry.Id);
            value=new(SmoSpatialProjection.Reference(document,state.Destination),SmoSpatialProjection.Polygon(state.PolygonBits),state.Open)
                {Plane=SmoSpatialProjection.Plane(state.PlaneBits)};
            return true;
        }
        catch(InvalidDataException exception){error=exception.Message;return false;}
    }
}
