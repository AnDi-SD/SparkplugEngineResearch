using System.Runtime.InteropServices;

namespace SmoViewer.Sparkplug;
internal static unsafe partial class NativeMethods
{
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)]
    internal static extern int spv_graph_spatial_json(GraphHandle graph,byte* output,uint capacity,out uint size);
}
