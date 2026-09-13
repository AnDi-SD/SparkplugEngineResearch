using System.Numerics;
using System.Runtime.InteropServices;
namespace SmoViewer.Sparkplug;
internal static unsafe partial class NativeMethods
{
    [StructLayout(LayoutKind.Sequential)]
    internal struct FontInfo
    {
        public uint Height, Baseline, HasBaseline, HasImage, ImageOffset, ImageSize;
    }
    [StructLayout(LayoutKind.Sequential)]
    internal struct FontGlyph
    {
        public uint Width;
        public Vector2 Uv0, Uv1;
    }
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int spv_font_read(byte* bytes, uint size, out FontInfo info, FontGlyph* glyphs, uint count);
}
