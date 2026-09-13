using System.Runtime.InteropServices;

namespace SmoViewer.Sparkplug;

internal static unsafe partial class NativeMethods
{
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr spv_skin_patch_sort_scalars(byte* bytes, uint count, uint alphaSort, uint priority);
}
