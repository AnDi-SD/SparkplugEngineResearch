using System.Diagnostics.CodeAnalysis;
using System.Numerics;

namespace SmoViewer.Core;

/// <summary>One nonnull slot in the actual partition child array.</summary>
public sealed record SmoPartitionNodeChild(uint SlotIndex,SmoLoadedReference Relationship);
/// <summary>Loaded partition state; references do not imply their physical wire form.</summary>
public sealed record SmoPartitionNodeData(uint DebugColorArgb,SmoLoadedReference PartitionSystem,
    SmoLoadedReference Zone,IReadOnlyList<SmoPartitionNodeChild> Children,
    IReadOnlyList<SmoLoadedReference> CollisionInfos,IReadOnlyList<SmoLoadedReference> ZonePortals,
    IReadOnlyList<SmoLoadedReference> StaticRenderObjects,SmoLoadedReference PartitionRenderable)
{
    public SmoLoadedReference Parent { get; init; } = new(null);
    public uint ChildSlotCount { get; init; }
}

/// <summary>Inspection projection of the actual shared ResourceGraph; not a second serializer.</summary>
public static class SmoPartitionNodeDecoder
{
    public static bool TryDecode(SmoDocument document,SmoObjectEntry entry,
        [NotNullWhen(true)] out SmoPartitionNodeData? value,out string error)
    {
        ArgumentNullException.ThrowIfNull(document);ArgumentNullException.ThrowIfNull(entry);
        value=null;error=string.Empty;
        try
        {
            var snapshot=SmoSpatialProjection.Snapshot(document,entry,SmoClassIds.PartitionNode);
            var state=SmoSpatialProjection.Find(snapshot.Partitions,p=>p.Id,entry.Id);
            value=SmoSpatialProjection.Partition(document,state);
            return true;
        }
        catch(InvalidDataException exception){error=exception.Message;return false;}
    }
}
