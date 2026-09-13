using System.Numerics;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;
namespace SmoViewer.Sparkplug;
internal static unsafe partial class NativeMethods
{
    [StructLayout(LayoutKind.Sequential)] internal struct TextViewInfo
    {public uint Font,Atlas,Vertices,Indices,PowerAssigned,Passes,Material;}
    [StructLayout(LayoutKind.Sequential)] internal struct TextVertex
    {public Vector3 Position;public uint Color;public Vector2 UV;}
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] internal static extern IntPtr spv_text_view_create(GraphHandle graph,uint text);
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] internal static extern void spv_text_view_destroy(IntPtr handle);
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] internal static extern int spv_text_view_info(TextViewHandle handle,out TextViewInfo output);
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] internal static extern int spv_text_view_vertices(TextViewHandle handle,TextVertex* output,uint count);
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] internal static extern int spv_text_view_indices(TextViewHandle handle,ushort* output,uint count);
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] internal static extern int spv_text_view_draws(TextViewHandle handle,MaterialDrawPass* output,uint count);
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] internal static extern int spv_text_view_capture(TextViewHandle handle,uint frame,MaterialDrawPass* output,uint count);
    [StructLayout(LayoutKind.Sequential)] internal struct GraphText
    {public uint Font,Color,Wrap,Alignment,Width,TextBytes,TextPresent,BoundsMask;public Vector4 Sphere;public Vector3 Minimum,Maximum;}
    [StructLayout(LayoutKind.Sequential)] internal struct GraphFont
    {public uint Height,Baseline,BaselinePresent,Image;}
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] internal static extern IntPtr spv_graph_load_for_tools(byte* bytes,uint size,uint captureTrace);
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] internal static extern int spv_graph_legacy_texture_ids(GraphHandle graph,uint* output,uint capacity,out uint count);
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] internal static extern int spv_graph_text(GraphHandle graph,uint id,out GraphText output);
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] internal static extern int spv_graph_text_bytes(GraphHandle graph,uint id,byte* output,uint count);
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] internal static extern int spv_graph_text_node(GraphHandle graph,uint id,out uint text);
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] internal static extern int spv_graph_font(GraphHandle graph,uint id,out GraphFont output,FontGlyph* glyphs,uint count);
}
internal sealed class TextViewHandle : SafeHandleZeroOrMinusOneIsInvalid
{
    internal TextViewHandle(IntPtr pointer):base(true)=>SetHandle(pointer);
    protected override bool ReleaseHandle(){NativeMethods.spv_text_view_destroy(handle);return true;}
}
