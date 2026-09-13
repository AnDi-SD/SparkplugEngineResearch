using System.Numerics;
using System.Runtime.InteropServices;

namespace SmoViewer.Sparkplug;
internal static unsafe partial class NativeMethods
{
    [StructLayout(LayoutKind.Sequential)]
    internal struct BoundingVolumeField {public Vector4 Values;public Vector3 HalfExtents;public float BoundingRadius;}
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)]
    internal static extern int spv_bv_scalar_read(uint classId,uint field,byte* bytes,uint size,out BoundingVolumeField value);
}
