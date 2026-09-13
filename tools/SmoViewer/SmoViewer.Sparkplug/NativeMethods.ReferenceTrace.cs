using System.Runtime.InteropServices;

namespace SmoViewer.Sparkplug;

internal static unsafe partial class NativeMethods
{
    [StructLayout(LayoutKind.Sequential)]
    internal struct ReferenceRead
    {
        public uint ConsumerId, Id, InlineSize, IdPhysicalOffset, SizePhysicalOffset, Resolution, Success;
    }
    [StructLayout(LayoutKind.Sequential)]
    internal struct PayloadRead
    {
        public uint ObjectId, WireClassId, PhysicalOffset, Size, Kind, Complete;
    }
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr spv_graph_load_with_trace(byte* bytes, uint size);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int spv_graph_reference_trace_info(GraphHandle graph,
        out uint references, out uint payloads, out uint origin);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int spv_graph_reference_reads(GraphHandle graph, ReferenceRead* output, uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int spv_graph_payload_reads(GraphHandle graph, PayloadRead* output, uint count);
}
