using System.Runtime.InteropServices;

namespace SmoViewer.Sparkplug;

internal static unsafe partial class NativeMethods
{
    [StructLayout(LayoutKind.Sequential)]
    internal struct SkinPaletteField
    {
        public uint HeaderOffset, PayloadOffset, PayloadSize, AssignmentOrder;
    }

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int spv_model_palette_fields(ModelHandle handle,
        SkinPaletteField* fields, uint capacity, out uint count);
}
