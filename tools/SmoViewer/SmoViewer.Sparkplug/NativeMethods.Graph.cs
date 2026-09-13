using System.Runtime.InteropServices;
using System.Numerics;
using Microsoft.Win32.SafeHandles;

namespace SmoViewer.Sparkplug;

internal static unsafe partial class NativeMethods
{
    [StructLayout(LayoutKind.Sequential)] internal struct GraphModel
    { public uint Mesh, Material, Fog, Alpha, Priority, Projection; }
    [StructLayout(LayoutKind.Sequential)] internal struct GraphRenderable { public uint Material,Fog,Alpha,Priority; }
    [StructLayout(LayoutKind.Sequential)] internal struct FogFields { public uint Type,Color;public float Start,End,Density; }
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] internal static extern int spv_fog_payload_read(byte* bytes,uint size,out FogFields value);
    [StructLayout(LayoutKind.Sequential)] internal struct LensFlareInfo { public uint Elements,RenderNode; public float Radius,Speed; }
    [StructLayout(LayoutKind.Sequential)] internal struct LensFlareElement { public uint Material,Color; public float Distance,Scale; }
    [StructLayout(LayoutKind.Sequential)] internal unsafe struct ParticleInfo {
        public Vector3 AccelerationBegin,AccelerationEnd,Direction;
        public Vector2 Velocity,Angle,Scale;public uint ColorBegin,ColorEnd;
        public Vector2 Times;public Vector4 Sphere;public float Rate;
        public uint Loop,WorldSpace,Iterative,RegionType,RegionValues,RenderNode;
        public fixed float Region[8];public fixed uint Pool[5];
    }
    [StructLayout(LayoutKind.Sequential)] internal struct GraphMaterial
    {
        public fixed uint States[11]; public uint VertexAlpha, PowerInitialized;
        public fixed float Colors[16]; public float Power; public uint ColorController, Passes;
    }
    [StructLayout(LayoutKind.Sequential)] internal struct GraphPass { public uint Blend, Layers; }
    [StructLayout(LayoutKind.Sequential)] internal struct GraphLayer
    {
        public uint ClassId, Texture, Animation, UvController, UvEnabled, AnimationBoundHere, UvBoundHere;
        public fixed uint States[12]; public fixed float Uv[9];
    }
    [StructLayout(LayoutKind.Sequential)] internal struct GraphTexture { public uint Width, Height, SurfaceFormat, Mips; }
    [StructLayout(LayoutKind.Sequential)] internal struct GraphTextureKey { public float Time; public uint Texture; }
    [StructLayout(LayoutKind.Sequential)] internal struct OctreeFields
    { public uint PivotKnown; public Vector3 Pivot, Minimum, Maximum; }
    [StructLayout(LayoutKind.Sequential)] internal struct GraphOctree
    { public uint Parent; public fixed uint Children[8]; public OctreeFields Fields; }
    [StructLayout(LayoutKind.Sequential)] internal struct GraphControllerClock
    { public uint ClassId; public float Accumulated, Applied, Playback; public uint HasPlayback, Enabled; }
    [StructLayout(LayoutKind.Sequential)] internal struct GraphUVSubmission
    { public uint Stage; public fixed float Matrix[9]; }
    [StructLayout(LayoutKind.Sequential)] internal struct GraphNode
    {
        public uint ParentId, Flags, Children, Collisions; public Vector3 Position;
        public fixed float Orientation[9]; public Vector3 Scale; public Quaternion Rotation;
    }
    [StructLayout(LayoutKind.Sequential)] internal struct GraphRenderContainer
    { public uint Id, Kind, Renderables; public Matrix4x4 World, Inverse; }
    [StructLayout(LayoutKind.Sequential)] internal struct GraphRenderOccurrence
    { public uint Renderable, RigidNode; public Matrix4x4 World; }
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern IntPtr spv_graph_load(byte* bytes, uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern void spv_graph_destroy(IntPtr handle);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_graph_model(GraphHandle handle, uint id, out GraphModel model);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_graph_renderable(GraphHandle handle,uint id,out GraphRenderable value);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_graph_lens_flare(GraphHandle handle,uint id,out LensFlareInfo value);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_graph_lens_flare_element(GraphHandle handle,uint id,uint ordinal,out LensFlareElement value);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_graph_particle(GraphHandle handle,uint id,out ParticleInfo value);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_graph_material(GraphHandle handle, uint id, out GraphMaterial material);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_graph_pass(GraphHandle handle, uint id, uint pass, out GraphPass value);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_graph_layer(GraphHandle handle, uint id, uint pass, uint layer, out GraphLayer value);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_graph_texture(GraphHandle handle, uint id, out GraphTexture texture);
    [StructLayout(LayoutKind.Sequential)] internal struct GraphTextureMip {public uint Width,Height,Bytes;}
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_graph_texture_mip_info(GraphHandle handle,uint id,uint level,out GraphTextureMip mip);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_graph_texture_mip_bgra(GraphHandle handle,uint id,uint level,byte* bytes,uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_graph_texture_bgra(GraphHandle handle, uint id, byte* bytes, uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_graph_texture_track(GraphHandle handle, uint id, out uint keys, out float duration);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_graph_texture_keys(GraphHandle handle, uint id, GraphTextureKey* keys, uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_graph_controller_clock(GraphHandle handle, uint id, out GraphControllerClock clock);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_graph_apply_controllers(GraphHandle handle, uint* ids, uint count, float elapsed);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_graph_update_material_color(GraphHandle handle, uint id, uint frame, uint force, out uint evaluated);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_graph_update_material_pass(GraphHandle handle, uint id, uint pass, GraphUVSubmission* output, uint capacity, out uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_graph_node(GraphHandle handle, uint id, out GraphNode node);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_octree_fields_read(byte* bytes, uint count, out OctreeFields fields);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_graph_octree(GraphHandle handle, uint id, out GraphOctree node);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_graph_navigation_json(GraphHandle handle, byte* bytes, uint capacity, out uint size);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern IntPtr spv_graph_scene_all(GraphHandle handle);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_scene_node_count(SceneHandle handle, out uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_scene_graph_node_ids(SceneHandle handle, uint* ids, uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_scene_graph_skin_info(SceneHandle handle, uint skin, out uint weights, out uint bones);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_scene_graph_skin_palette(SceneHandle handle, uint skin, Matrix4x4* output, uint floats);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_graph_render_containers(GraphHandle handle, GraphRenderContainer* output, uint capacity, out uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_graph_render_members(GraphHandle handle, uint id, uint* output, uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_graph_render_occurrence(GraphHandle handle, uint id, uint slot, out GraphRenderOccurrence output);
}

internal sealed class GraphHandle : SafeHandleZeroOrMinusOneIsInvalid
{
    internal GraphHandle(IntPtr value) : base(true) => SetHandle(value);
    protected override bool ReleaseHandle() { NativeMethods.spv_graph_destroy(handle); return true; }
}
