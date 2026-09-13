using System.Runtime.InteropServices;

namespace SmoViewer.Sparkplug;

internal static unsafe partial class NativeMethods
{
    [StructLayout(LayoutKind.Sequential)] internal struct SceneLightCache
    { public uint RenderNode, Count, Ambient; public fixed uint Lights[8]; }
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_scene_light_ids(SceneHandle handle, uint* ids, uint capacity, out uint count);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_scene_lighting_configure(SceneHandle handle, uint* ids, uint count, uint active);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_scene_lighting_active(SceneHandle handle, uint active);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_scene_lighting_clear(SceneHandle handle);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int spv_scene_lighting_caches(SceneHandle handle, SceneLightCache* output, uint capacity, out uint count);
}
