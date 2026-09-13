using System.Numerics;
using System.Text.Json;
using SmoViewer.Sparkplug;

namespace SmoViewer.Core;

/// <summary>Canonical target from the loaded graph. No serialized encoding or ownership is inferred.</summary>
public sealed record SmoLoadedReference(SmoObjectEntry? Target)
{
    public uint ObjectId => Target?.Id ?? 0;
    public int? TargetObjectIndex => Target?.Index;
    public uint? TargetTypeHash => Target?.TypeHash;
    public string? TargetName => Target?.Name;
}

// Private host transfer DTOs: float bits and loaded IDs, never a SMO codec.
internal sealed record SpatialPartition(uint Id,uint Color,uint System,uint Zone,uint Parent,
    uint[] Children,uint[] Collisions,uint[] Portals,uint[] Statics,uint Payload,uint[]? PlaneBits,
    uint[][] PolygonBits,uint[]? PivotBits,uint[]? MinimumBits,uint[]? MaximumBits);
internal sealed record SpatialSystem(uint Id,uint Root,uint[] Renderables);
internal sealed record SpatialPayload(uint Id,uint Color,uint[] Renderables);
internal sealed record SpatialZone(uint Id,uint[] Roots);
internal sealed record SpatialPortal(uint Id,uint Destination,byte Open,uint[][] PolygonBits,uint[]? PlaneBits);
internal sealed record SpatialPortalNode(uint Id,uint[] Portals);
internal sealed record SpatialSnapshot(SpatialPartition[] Partitions,SpatialSystem[] Systems,
    SpatialPayload[] Payloads,SpatialZone[] Zones,SpatialPortal[] Portals,SpatialPortalNode[] PortalNodes)
{
    public static unsafe SpatialSnapshot Read(GraphHandle graph)
    {
        NativeMethods.Check(NativeMethods.spv_graph_spatial_json(graph,null,0,out uint size));
        if(size>16*1024*1024)throw new InvalidDataException("Spatial snapshot exceeds its host limit.");
        var bytes=new byte[checked((int)size)];
        fixed(byte* output=bytes)NativeMethods.Check(NativeMethods.spv_graph_spatial_json(graph,output,size,out _));
        try{return JsonSerializer.Deserialize<SpatialSnapshot>(bytes)
            ??throw new InvalidDataException("Missing loaded spatial snapshot.");}
        catch(JsonException error){throw new InvalidDataException("Invalid spatial bridge snapshot.",error);}
    }
}

internal static class SmoSpatialProjection
{
    public static SpatialSnapshot Snapshot(SmoDocument document,SmoObjectEntry entry,uint expectedClass)
    {
        ArgumentNullException.ThrowIfNull(document);ArgumentNullException.ThrowIfNull(entry);
        if(entry.TypeHash!=expectedClass)throw new InvalidDataException("Object has a different spatial class.");
        if(!SmoNodeDecoder.TryGetCataloguedObject(document,entry.Id,out var canonical)||canonical!=entry)
            throw new InvalidDataException("Spatial object is not a canonical entry in this document.");
        var loaded=SmoLoadedResources.Get(document);
        if(loaded.LoadIssue is not null)throw new InvalidDataException(loaded.LoadIssue);
        return loaded.Spatial??throw new InvalidDataException("Spatial resources were not loaded.");
    }
    public static T Find<T>(IEnumerable<T> values,Func<T,uint> id,uint expected) where T:class
        => values.FirstOrDefault(value=>id(value)==expected)??throw new InvalidDataException("Loaded spatial object is absent.");
    public static SmoLoadedReference Reference(SmoDocument document,uint id)
        => id==0?new(null):SmoNodeDecoder.TryGetCataloguedObject(document,id,out var entry)?new(entry)
            :throw new InvalidDataException($"Loaded resource ID {id} is absent from this document.");
    public static IReadOnlyList<SmoLoadedReference> References(SmoDocument document,IEnumerable<uint> ids)
        =>Array.AsReadOnly(ids.Select(id=>Reference(document,id)).ToArray());
    public static Vector3 Vector(uint[] bits)
    {
        if(bits.Length!=3)throw new InvalidDataException("Spatial bridge vector shape differs.");
        return new(BitConverter.UInt32BitsToSingle(bits[0]),BitConverter.UInt32BitsToSingle(bits[1]),BitConverter.UInt32BitsToSingle(bits[2]));
    }
    public static SmoBspPlane? Plane(uint[]? bits)
    {
        if(bits is null)return null;
        if(bits.Length!=4)throw new InvalidDataException("Spatial bridge plane shape differs.");
        return new(new(BitConverter.UInt32BitsToSingle(bits[0]),BitConverter.UInt32BitsToSingle(bits[1]),BitConverter.UInt32BitsToSingle(bits[2])),BitConverter.UInt32BitsToSingle(bits[3]));
    }
    public static IReadOnlyList<Vector3> Polygon(uint[][] bits)=>Array.AsReadOnly(bits.Select(Vector).ToArray());
    public static SmoPartitionNodeData Partition(SmoDocument document,SpatialPartition state)
        =>new(state.Color,Reference(document,state.System),Reference(document,state.Zone),
            Array.AsReadOnly(state.Children.Select((id,slot)=>(id,slot)).Where(item=>item.id!=0)
                .Select(item=>new SmoPartitionNodeChild((uint)item.slot,Reference(document,item.id))).ToArray()),
            References(document,state.Collisions),References(document,state.Portals),References(document,state.Statics),Reference(document,state.Payload))
            { Parent=Reference(document,state.Parent),ChildSlotCount=checked((uint)state.Children.Length) };
}
