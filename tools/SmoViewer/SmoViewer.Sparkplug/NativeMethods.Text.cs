using Microsoft.Win32.SafeHandles;
using System.Runtime.InteropServices;
namespace SmoViewer.Sparkplug;
internal static unsafe partial class NativeMethods
{
    [StructLayout(LayoutKind.Sequential)]
    internal struct TextInspectionInfo
    {
        public uint RenderableMask, FieldMask, Alpha, Priority, Color, WrapWidth, Alignment, TextLength, TextByteCount, TextFlags;
        public MaterialReference Material, Fog, Font;
    }
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr spv_text_inspection_read(byte* bytes, uint size);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void spv_text_inspection_destroy(IntPtr handle);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int spv_text_inspection_info(TextInspectionHandle handle, out TextInspectionInfo info);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int spv_text_inspection_bytes(TextInspectionHandle handle, byte* output, uint count);
}
internal sealed class TextInspectionHandle : SafeHandleZeroOrMinusOneIsInvalid
{
    internal TextInspectionHandle(IntPtr value) : base(true) => SetHandle(value);
    protected override bool ReleaseHandle() { NativeMethods.spv_text_inspection_destroy(handle); return true; }
}
