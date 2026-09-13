using System.Runtime.InteropServices;

namespace SmoViewer.Sparkplug;

internal static unsafe partial class NativeMethods
{
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr spv_material_patch_scalars(byte* bytes, uint count,
        uint* states, uint stateCount, uint blend, uint* textureStates, uint textureStateCount, uint kind);
}
