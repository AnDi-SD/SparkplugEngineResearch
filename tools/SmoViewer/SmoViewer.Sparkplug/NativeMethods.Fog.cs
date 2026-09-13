using System.Runtime.InteropServices;
namespace SmoViewer.Sparkplug;
internal static partial class NativeMethods
{
    [StructLayout(LayoutKind.Sequential)] internal struct FogDraw
    {
        internal uint Known, Enabled, Mode, Color;
        internal float Start, End, Density;
    }
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int spv_graph_fog_draw(GraphHandle graph, uint id, out FogDraw output);
}
