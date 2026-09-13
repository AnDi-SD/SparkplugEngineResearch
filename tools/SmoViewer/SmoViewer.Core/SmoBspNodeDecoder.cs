using System.Diagnostics.CodeAnalysis;
using System.Numerics;

namespace SmoViewer.Core;

/// <summary>Actual four plane coefficients; the game reader does not normalize them.</summary>
public sealed record SmoBspPlane(Vector3 Normal,float Constant);
/// <summary>Null Plane explicitly represents a constructor-uninitialized plane.</summary>
public sealed record SmoBspNodeData(SmoPartitionNodeData PartitionNode,SmoBspPlane? Plane,IReadOnlyList<Vector3> Polygon);

/// <summary>Inspection projection of the actual shared ResourceGraph; not a second serializer.</summary>
public static class SmoBspNodeDecoder
{
    public const int PlanePayloadSize=4*sizeof(float);
    public static bool TryDecode(SmoDocument document,SmoObjectEntry entry,
        [NotNullWhen(true)] out SmoBspNodeData? value,out string error)
    {
        ArgumentNullException.ThrowIfNull(document);ArgumentNullException.ThrowIfNull(entry);
        value=null;error=string.Empty;
        try
        {
            var snapshot=SmoSpatialProjection.Snapshot(document,entry,SmoClassIds.BspNode);
            var state=SmoSpatialProjection.Find(snapshot.Partitions,p=>p.Id,entry.Id);
            value=new(SmoSpatialProjection.Partition(document,state),SmoSpatialProjection.Plane(state.PlaneBits),SmoSpatialProjection.Polygon(state.PolygonBits));
            return true;
        }
        catch(InvalidDataException exception){error=exception.Message;return false;}
    }
}
