using System.Diagnostics.CodeAnalysis;
using System.Numerics;

namespace SmoViewer.Core;

/// <summary>Actual root/support identities; Node retains authored metadata from the common Node reader.</summary>
public sealed record SmoPartitionSystemData(SmoNodeData Node,IReadOnlyList<SmoLoadedReference> Renderables,SmoLoadedReference PartitionRoot);

/// <summary>Inspection projection of the actual shared ResourceGraph; not a second serializer.</summary>
public static class SmoPartitionSystemDecoder
{
    public static bool TryDecode(SmoDocument document,SmoObjectEntry entry,
        [NotNullWhen(true)] out SmoPartitionSystemData? value,out string error)
    {
        ArgumentNullException.ThrowIfNull(document);ArgumentNullException.ThrowIfNull(entry);
        value=null;error=string.Empty;
        try
        {
            var snapshot=SmoSpatialProjection.Snapshot(document,entry,SmoClassIds.PartitionSystem);
            var state=SmoSpatialProjection.Find(snapshot.Systems,p=>p.Id,entry.Id);
            value=new(SmoNavigationProjection.Node(document,entry),SmoSpatialProjection.References(document,state.Renderables),SmoSpatialProjection.Reference(document,state.Root));
            return true;
        }
        catch(InvalidDataException exception){error=exception.Message;return false;}
    }
}
