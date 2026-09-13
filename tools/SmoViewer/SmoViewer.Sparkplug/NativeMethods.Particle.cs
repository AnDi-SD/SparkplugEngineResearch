using System.Numerics;
using System.Runtime.InteropServices;

namespace SmoViewer.Sparkplug;

internal static unsafe partial class NativeMethods
{
    [StructLayout(LayoutKind.Sequential)] internal struct ParticlePoolInfo
    { public uint Count,First,Boundary,Initialized; }
    [StructLayout(LayoutKind.Sequential)] internal struct ParticleRecord
    { public Vector3 Position,Velocity;public float Birth,Lifetime;public uint Written,Previous,Next; }
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)]
    internal static extern int spv_graph_particle_pool(GraphHandle handle,uint id,out ParticlePoolInfo info,ParticleRecord* records,uint capacity);
}
