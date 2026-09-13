using System.Numerics;
using System.Runtime.InteropServices;

namespace SmoViewer.Sparkplug;

internal static partial class NativeMethods
{
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr spv_static_write_matrix_fields(in Matrix4x4 world, in Matrix4x4 inverse);
}
