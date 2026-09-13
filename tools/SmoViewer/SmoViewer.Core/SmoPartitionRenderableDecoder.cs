using System.Diagnostics.CodeAnalysis;
using System.Numerics;

namespace SmoViewer.Core;

/// <summary>Loaded support members and current debug color.</summary>
public sealed record SmoPartitionRenderableData(uint DebugColorArgb,IReadOnlyList<SmoLoadedReference> Renderables);

/// <summary>Inspection projection of the actual shared ResourceGraph; not a second serializer.</summary>
public static class SmoPartitionRenderableDecoder
{
    public static bool TryDecode(SmoDocument document,SmoObjectEntry entry,
        [NotNullWhen(true)] out SmoPartitionRenderableData? value,out string error)
    {
        ArgumentNullException.ThrowIfNull(document);ArgumentNullException.ThrowIfNull(entry);
        value=null;error=string.Empty;
        try
        {
            var snapshot=SmoSpatialProjection.Snapshot(document,entry,SmoClassIds.PartitionRenderable);
            var state=SmoSpatialProjection.Find(snapshot.Payloads,p=>p.Id,entry.Id);
            value=new(state.Color,SmoSpatialProjection.References(document,state.Renderables));
            return true;
        }
        catch(InvalidDataException exception){error=exception.Message;return false;}
    }
}
