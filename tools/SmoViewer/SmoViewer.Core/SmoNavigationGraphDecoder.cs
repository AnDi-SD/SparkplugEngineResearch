using System.Diagnostics.CodeAnalysis;

namespace SmoViewer.Core;

/// <summary>
/// One alternative route. FirstPortalIndex is the first portal from the source
/// set; the second serialized byte is zero in every PC and PS2 corpus object
/// and remains explicitly exposed as Reserved until runtime semantics are found.
/// </summary>
public sealed record SmoNavigationPathAlternative(byte FirstPortalIndex,byte Reserved);

/// <summary>One row of the square source-set/destination-set route table.</summary>
public sealed record SmoNavigationGraphPath(
    uint SourceSetIndex,
    uint DestinationSetIndex,
    byte NextPortalIndex,
    IReadOnlyList<SmoNavigationPathAlternative> Alternatives)
{
    public bool IsReachable => NextPortalIndex != byte.MaxValue;
}

/// <summary>Complete inherited render-node and own navigation-graph state.</summary>
public sealed record SmoNavigationGraphData(
    SmoRenderNodeData RenderNode,
    IReadOnlyList<SmoNodeRelationship> NavigationSets,
    IReadOnlyList<SmoNodeRelationship> Portals,
    uint PathTableSize,
    IReadOnlyList<SmoNavigationGraphPath> Paths)
{
    public SmoNavigationGraphPath this[int source,int destination] =>
        Paths[checked(source * (int)PathTableSize + destination)];
}

/// <summary>Inspection projection of the actual loaded spNavigationGraph.</summary>
public static class SmoNavigationGraphDecoder
{
    public const int PathHeaderSize = 2 * sizeof(uint) + 2 * sizeof(byte);
    public static bool TryDecode(SmoDocument document,SmoObjectEntry entry,
        [NotNullWhen(true)] out SmoNavigationGraphData? value,out string error)
    {
        ArgumentNullException.ThrowIfNull(document);ArgumentNullException.ThrowIfNull(entry);
        value=null;error=string.Empty;
        try
        {
            if(entry.TypeHash!=SmoClassIds.NavigationGraph)throw new InvalidDataException("Object is not spNavigationGraph.");
            var state=SmoNavigationProjection.Snapshot(document).Graphs.FirstOrDefault(item=>item.Id==entry.Id)
                ??throw new InvalidDataException("Loaded navigation graph is absent.");
            if(!SmoRenderNodeDecoder.TryDecodeFields(document,entry,SmoClassIds.NavigationGraph,
                "render_node.renderable",out var renderNode)||renderNode is null)
                throw new InvalidDataException("Cannot project inherited RenderNode metadata.");
            var sets=SmoNavigationProjection.References(document,entry,"navigation_graph.set",state.Sets);
            var portals=SmoNavigationProjection.References(document,entry,"navigation_graph.portal",state.Portals);
            var paths=state.Paths.Select((path,index)=>new SmoNavigationGraphPath(
                (uint)index/state.Size,(uint)index%state.Size,path.Next,
                Array.AsReadOnly(path.Alternatives.Select(pair=>new SmoNavigationPathAlternative(
                    checked((byte)pair[0]),checked((byte)pair[1]))).ToArray()))).ToArray();
            value=new(renderNode,sets,portals,state.Size,Array.AsReadOnly(paths));return true;
        }
        catch(InvalidDataException exception){error=exception.Message;return false;}
    }
}
