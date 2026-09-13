using System.Text.Json;
using SmoViewer.Sparkplug;

namespace SmoViewer.Core;

// Private host ABI DTOs. These are snapshots of the same ResourceGraph used by
// models and nodes, not parsers or replicas of navigation engine algorithms.
internal sealed record NavigationTable(uint Rows,uint Columns,uint[] Values)
{
    public SmoNavigationTransitionTable Project() => new(Rows,Columns,Array.AsReadOnly(Values));
}
internal sealed record NavigationSetSnapshot(uint Id,uint NodeCount,byte Index,byte Enabled,
    NavigationTable? Transitions,NavigationTable? PortalTransitions,uint[][] Neighbours,
    uint[] Portals,uint Mesh,uint[] MinimumBits,uint[] MaximumBits,uint[] SphereBits);
internal sealed record NavigationPath(byte Next,uint[][] Alternatives);
internal sealed record NavigationGraphSnapshot(uint Id,uint[] Sets,uint[] Portals,uint Size,NavigationPath[] Paths);
internal sealed record NavigationPortalSnapshot(uint Id,byte Index,byte Enabled,uint? Graph,uint[]? Sets,
    uint[] FirstNodes,uint[] SecondNodes,uint[][] Paths);
internal sealed record NavigationSnapshot(NavigationSetSnapshot[] Sets,NavigationGraphSnapshot[] Graphs,
    NavigationPortalSnapshot[] Portals)
{
    public static unsafe NavigationSnapshot Read(GraphHandle graph)
    {
        NativeMethods.Check(NativeMethods.spv_graph_navigation_json(graph,null,0,out uint size));
        if(size>16*1024*1024)throw new InvalidDataException("Navigation snapshot exceeds its host limit.");
        var bytes=new byte[checked((int)size)];
        fixed(byte* output=bytes)
            NativeMethods.Check(NativeMethods.spv_graph_navigation_json(graph,output,size,out _));
        try{return JsonSerializer.Deserialize<NavigationSnapshot>(bytes)
            ?? throw new InvalidDataException("Missing loaded navigation snapshot.");}
        catch(JsonException error){throw new InvalidDataException("Invalid navigation bridge snapshot.",error);}
    }
}

internal static class SmoNavigationProjection
{
    public static NavigationSnapshot Snapshot(SmoDocument document)
    {
        var loaded=SmoLoadedResources.Get(document);
        if(loaded.LoadIssue is not null)throw new InvalidDataException(loaded.LoadIssue);
        return loaded.Navigation??throw new InvalidDataException("Navigation graph was not loaded.");
    }
    public static SmoNodeData Node(SmoDocument document,SmoObjectEntry entry)
        => SmoNodeDecoder.TryDecodeNodeSection(document,entry,out var node)&&node is not null?node
            :throw new InvalidDataException("Cannot project inherited Node metadata.");

    // Preserve physical reference provenance for existing inspectors. Reference
    // prefixes are read by spSerializer; loaded IDs remain authoritative.
    public static IReadOnlyList<SmoNodeRelationship> References(SmoDocument document,SmoObjectEntry entry,
        string key,IReadOnlyList<uint> ids,bool lastField=false)
    {
        if(!SmoObjectFieldReader.TryRead(document,entry,out var fields,out string error))
            throw new InvalidDataException(error);
        var selected=fields.Where((field,index)=>SmoSerializedFieldRegistry.TryDescribeField(
            entry.TypeHash,fields,index,out var descriptor)&&descriptor?.Key==key).ToArray();
        if(lastField)selected=selected.TakeLast(1).ToArray();
        var result=new List<SmoNodeRelationship>();
        foreach(var field in selected)
        {
            var remaining=field.Payload.Span;
            while(!remaining.IsEmpty)
            {
                if(!SmoNodeDecoder.TryReadReference(remaining,checked((uint)remaining.Length),2,out var prefix))
                    throw new InvalidDataException("Cannot project native relationship prefix.");
                int length=checked((int)(prefix.Id==0?4:8+prefix.InlineSize));
                if(!SmoNodeDecoder.TryDecodeRelationship(document,remaining[..length],out var relationship)||relationship is null)
                    throw new InvalidDataException("Cannot project navigation relationship metadata.");
                result.Add(relationship);remaining=remaining[length..];
            }
        }
        if(!result.Select(item=>item.ObjectId).SequenceEqual(ids))
            throw new InvalidDataException("Navigation reference provenance differs from loaded object identities.");
        return result.AsReadOnly();
    }
}
