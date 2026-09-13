using System.Diagnostics.CodeAnalysis;

namespace SmoViewer.Core;

/// <summary>One pair of triangle/node IDs on the two portal endpoint sets.</summary>
public sealed record SmoNavigationPortalNodePair(byte FirstSetNodeId,byte SecondSetNodeId);

/// <summary>
/// Reverse membership row for an alternative route in spNavigationGraph. The
/// row says that this portal participates in alternative AlternativePathIndex
/// from SourceSetIndex to DestinationSetIndex; route order is reconstructed
/// from the portal endpoint topology.
/// </summary>
public sealed record SmoNavigationPortalPathMembership(
    byte SourceSetIndex,
    byte DestinationSetIndex,
    byte AlternativePathIndex);

/// <summary>Complete inherited node state and own spNavigationPortal fields.</summary>
public sealed record SmoNavigationPortalData(
    SmoNodeData Node,
    SmoNodeRelationship Graph,
    IReadOnlyList<SmoNodeRelationship> EndpointSets,
    IReadOnlyList<SmoNavigationPortalNodePair> NodePairs,
    IReadOnlyList<SmoNavigationPortalPathMembership> PathMemberships);

/// <summary>Inspection projection of the actual loaded spNavigationPortal.</summary>
public static class SmoNavigationPortalDecoder
{
    public static bool TryDecode(SmoDocument document,SmoObjectEntry entry,
        [NotNullWhen(true)] out SmoNavigationPortalData? value,out string error)
    {
        ArgumentNullException.ThrowIfNull(document);ArgumentNullException.ThrowIfNull(entry);
        value=null;error=string.Empty;
        try
        {
            if(entry.TypeHash!=SmoClassIds.NavigationPortal)throw new InvalidDataException("Object is not spNavigationPortal.");
            var state=SmoNavigationProjection.Snapshot(document).Portals.FirstOrDefault(item=>item.Id==entry.Id)
                ??throw new InvalidDataException("Loaded navigation portal is absent.");
            if(state.Graph is not uint graph||state.Sets is null)
                throw new InvalidDataException("Portal pointers were not initialized by the original reader; the complete inspection DTO is unavailable.");
            var graphRef=SmoNavigationProjection.References(document,entry,"navigation_portal.graph",[graph],true).Single();
            var sets=SmoNavigationProjection.References(document,entry,"navigation_portal.sets",state.Sets,true);
            value=new(SmoNavigationProjection.Node(document,entry),graphRef,sets,
                Array.AsReadOnly(state.FirstNodes.Select((node,index)=>new SmoNavigationPortalNodePair(
                    checked((byte)node),checked((byte)state.SecondNodes[index]))).ToArray()),
                Array.AsReadOnly(state.Paths.Select(row=>new SmoNavigationPortalPathMembership(
                    checked((byte)row[0]),checked((byte)row[1]),checked((byte)row[2]))).ToArray()));
            return true;
        }
        catch(InvalidDataException exception){error=exception.Message;return false;}
    }
}
