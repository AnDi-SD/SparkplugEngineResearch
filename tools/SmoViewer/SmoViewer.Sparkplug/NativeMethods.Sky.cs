using System.Numerics;
using System.Runtime.InteropServices;
namespace SmoViewer.Sparkplug;
internal static unsafe partial class NativeMethods
{
    [StructLayout(LayoutKind.Sequential)] internal struct SkyPose
    {
        internal uint Flags;
        internal fixed float Position[3];
        internal fixed float Scale[3];
        internal fixed float Orientation[9];
        internal fixed float RetainedWorldPosition[3];
    }
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] internal static extern int spv_graph_sky_pose(GraphHandle graph,uint id,out SkyPose output);
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] internal static extern int spv_sky_camera_world(in SkyPose pose,in Matrix4x4 cameraWorld,out Matrix4x4 output,uint count);
}
