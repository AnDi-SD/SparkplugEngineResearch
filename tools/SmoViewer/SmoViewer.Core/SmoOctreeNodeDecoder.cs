using System.Diagnostics.CodeAnalysis;
using System.Numerics;

namespace SmoViewer.Core;

/// <summary>Loaded Octree state; an unknown original pivot is reported as an explicit failure.</summary>
public sealed record SmoOctreeNodeData(SmoPartitionNodeData PartitionNode,Vector3 Pivot,Vector3 Minimum,Vector3 Maximum);

/// <summary>Inspection projection of the actual shared ResourceGraph; not a second serializer.</summary>
public static class SmoOctreeNodeDecoder
{
    public const int VectorPayloadSize=3*sizeof(float);
    public static bool TryDecode(SmoDocument document,SmoObjectEntry entry,
        [NotNullWhen(true)] out SmoOctreeNodeData? value,out string error)
    {
        ArgumentNullException.ThrowIfNull(document);ArgumentNullException.ThrowIfNull(entry);
        value=null;error=string.Empty;
        try
        {
            var snapshot=SmoSpatialProjection.Snapshot(document,entry,SmoClassIds.OctreeNode);
            var state=SmoSpatialProjection.Find(snapshot.Partitions,p=>p.Id,entry.Id);
            if(state.PivotBits is null)throw new InvalidDataException("spOctreeNode has an uninitialized pivot; its constructor initializes only bounds.");
            value=new(SmoSpatialProjection.Partition(document,state),SmoSpatialProjection.Vector(state.PivotBits),
                SmoSpatialProjection.Vector(state.MinimumBits!),SmoSpatialProjection.Vector(state.MaximumBits!));
            return true;
        }
        catch(InvalidDataException exception){error=exception.Message;return false;}
    }
}
