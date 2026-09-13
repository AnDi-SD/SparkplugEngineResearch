using System.Numerics;
using SmoViewer.Sparkplug;
namespace SmoViewer.Core;
/// <summary>Immutable local/cached transform observation. The common SkyBox
/// computes camera-parent world; graph membership and descendants are untouched.</summary>
public sealed class SmoSkyBoxPose
{
    private readonly NativeMethods.SkyPose _pose;
    private SmoSkyBoxPose(NativeMethods.SkyPose pose)=>_pose=pose;
    public bool Enabled=>(_pose.Flags&0x200)!=0;
    public uint Flags=>_pose.Flags;
    internal static SmoSkyBoxPose Read(GraphHandle graph,uint id)
    {
        NativeMethods.Check(NativeMethods.spv_graph_sky_pose(graph,id,out var pose));return new(pose);
    }
    /// <param name="cameraWorld">Unit-scale, game-coordinate camera world.</param>
    public Matrix4x4 CameraWorld(Matrix4x4 cameraWorld)
    {
        NativeMethods.Check(NativeMethods.spv_sky_camera_world(in _pose,in cameraWorld,out var world,16));return world;
    }
}
