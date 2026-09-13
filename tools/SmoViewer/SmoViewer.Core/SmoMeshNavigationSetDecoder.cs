using System.Diagnostics.CodeAnalysis;

namespace SmoViewer.Core;

/// <summary>
/// Dense routing table used by <c>spNavigationSet</c>. A non-terminal UInt32
/// cell is an index into the current node's ordered neighbour list, not a
/// destination ID. Value 3 is also used as the terminal/unreachable marker
/// whenever it is outside the current node's neighbour list.
/// </summary>
public sealed record SmoNavigationTransitionTable(
    uint RowCount,
    uint ColumnCount,
    IReadOnlyList<uint> LinkSelectors)
{
    public uint this[int row,int column] =>
        LinkSelectors[checked(row * (int)ColumnCount + column)];
}

/// <summary>One byte-addressed navigation node and its ordered neighbours.</summary>
public sealed record SmoNavigationNodeLinks(
    byte NodeId,
    IReadOnlyList<byte> NeighbourNodeIds);

/// <summary>
/// Complete inherited <c>spNavigationSet</c> state plus the navigation mesh
/// relationship added by <c>spMeshNavigationSet</c>.
/// </summary>
public sealed record SmoMeshNavigationSetData(
    SmoNodeData Node,
    uint NodeCount,
    SmoNavigationTransitionTable NodeTransitions,
    SmoNavigationTransitionTable PortalTransitions,
    IReadOnlyList<SmoNavigationNodeLinks> Links,
    IReadOnlyList<SmoNodeRelationship> Portals,
    bool Enabled,
    SmoNodeRelationship NavigationMesh)
{
    public byte RawEnabled { get; init; }
    public byte GraphIndex { get; init; }
    public int LinkCount => Links.Sum(item => item.NeighbourNodeIds.Count);
    public bool HasReciprocalLinks => Links.All(item =>
        item.NeighbourNodeIds.All(neighbour =>
            neighbour < Links.Count && Links[neighbour].NeighbourNodeIds.Contains(item.NodeId)));
}

/// <summary>Inspection projection of the actual loaded spMeshNavigationSet.</summary>
public static class SmoMeshNavigationSetDecoder
{
    public const int MatrixHeaderSize = 2 * sizeof(uint);
    public const int LinksHeaderSize = sizeof(uint);
    public const int LinkRecordHeaderSize = sizeof(byte) + sizeof(uint);
    public static bool TryDecode(SmoDocument document,SmoObjectEntry entry,
        [NotNullWhen(true)] out SmoMeshNavigationSetData? value,out string error)
    {
        ArgumentNullException.ThrowIfNull(document);ArgumentNullException.ThrowIfNull(entry);
        value=null;error=string.Empty;
        try
        {
            if(entry.TypeHash!=SmoClassIds.MeshNavigationSet)throw new InvalidDataException("Object is not spMeshNavigationSet.");
            var state=SmoNavigationProjection.Snapshot(document).Sets.FirstOrDefault(item=>item.Id==entry.Id)
                ??throw new InvalidDataException("Loaded mesh navigation set is absent.");
            if(state.Transitions is null||state.PortalTransitions is null||state.Mesh==0||state.Neighbours.Length>256)
                throw new InvalidDataException("The loaded set cannot be represented by the complete byte-addressed inspection DTO (missing tables/mesh or more than 256 nodes).");
            var portals=SmoNavigationProjection.References(document,entry,"navigation_set.portal",state.Portals);
            var mesh=SmoNavigationProjection.References(document,entry,"navigation_set.mesh",[state.Mesh],true).Single();
            var links=state.Neighbours.Select((row,index)=>new SmoNavigationNodeLinks(checked((byte)index),
                Array.AsReadOnly(row.Select(item=>checked((byte)item)).ToArray()))).ToArray();
            value=new(SmoNavigationProjection.Node(document,entry),state.NodeCount,state.Transitions.Project(),
                state.PortalTransitions.Project(),Array.AsReadOnly(links),portals,state.Enabled!=0,mesh)
                { RawEnabled=state.Enabled,GraphIndex=state.Index };
            return true;
        }
        catch(InvalidDataException exception){error=exception.Message;return false;}
    }
}
