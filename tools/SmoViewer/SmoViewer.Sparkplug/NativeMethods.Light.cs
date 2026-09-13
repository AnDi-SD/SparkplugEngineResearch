using System.Numerics;
using System.Runtime.InteropServices;

namespace SmoViewer.Sparkplug;

internal static unsafe partial class NativeMethods
{
    [StructLayout(LayoutKind.Sequential)]
    internal struct LightFields
    {
        public uint Type, ProjectShadow, Attenuation, Enabled;
        public Vector4 ColorRgba;
        public float Intensity, Range, Hotspot, Falloff;
    }

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int spv_light_fields_read(byte* bytes, uint size, out LightFields value);
}
