using System.Numerics;
using System.Runtime.InteropServices;

namespace SmoViewer.Sparkplug;
internal static unsafe partial class NativeMethods
{
    [StructLayout(LayoutKind.Sequential)] internal struct AlphaInfo
    {public uint Queued,Priority,Particle;public Vector4 Sphere;}
    [StructLayout(LayoutKind.Sequential)] internal struct AlphaInput
    {public uint Token,Priority,Particle;public Vector3 Center;public Matrix4x4 World;}
    [StructLayout(LayoutKind.Sequential)] internal struct AlphaOutput
    {public uint Token,Priority,Particle;public float DistanceSquared;}
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)]
    internal static extern int spv_graph_alpha_info(GraphHandle graph,uint id,out AlphaInfo output);
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)]
    internal static extern int spv_alpha_order(AlphaInput* input,uint count,float* view,uint depthOnly,uint priorityBase,AlphaOutput* output);
}
