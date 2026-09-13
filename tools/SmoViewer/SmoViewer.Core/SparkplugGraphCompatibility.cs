using SmoViewer.Sparkplug;
namespace SmoViewer.Core;
/// <summary>Explicit tool adaptations reported by the native graph owner.</summary>
internal static class SparkplugGraphCompatibility
{
    internal static unsafe uint[] ReadTextureIds(GraphHandle graph)
    {
        NativeMethods.Check(NativeMethods.spv_graph_legacy_texture_ids(graph,null,0,out uint count));
        if(count>8192)throw new InvalidDataException("Invalid compatibility resource count.");
        var ids=new uint[checked((int)count)];
        if(count!=0)fixed(uint* output=ids)NativeMethods.Check(NativeMethods.spv_graph_legacy_texture_ids(graph,output,count,out _));
        return ids;
    }
    internal static string Describe(uint id) => $"LEGACY_TEXTURE_COMPATIBILITY: [ID {id}] stored pixels loaded through the tool adapter; original source selection remains unverified.";
}
