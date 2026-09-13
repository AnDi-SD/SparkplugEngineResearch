using System.Numerics;
using System.Runtime.InteropServices;

namespace SmoViewer.Sparkplug;

internal static unsafe partial class NativeMethods
{
    [StructLayout(LayoutKind.Sequential)]
    internal struct SkinPaletteBinding
    {
        internal uint NodeId;
        internal Matrix4x4 InverseBind;
    }

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr spv_skin_write_palette(GraphHandle graph, uint weights,
        SkinPaletteBinding* bindings, uint count);
}
