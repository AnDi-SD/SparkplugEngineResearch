using System.Runtime.InteropServices;

namespace SmoViewer.Sparkplug;

internal static unsafe partial class NativeMethods
{
    [StructLayout(LayoutKind.Sequential)] internal struct ShaderLight
    {
        public uint Type, Known;
        public fixed float Diffuse[4], Specular[4], Position[4], Direction[4], Attenuation[4];
        public float Inner, Outer;
    }
    [StructLayout(LayoutKind.Sequential)] internal struct ShaderLighting
    {
        public uint ColorMode, Count, Specular, Known;
        public fixed float Ambient[4], Diffuse[4], ConstantColor[4];
        public float Power;
        public fixed byte Lights[8 * 96];
    }
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int spv_scene_shader_lighting(SceneHandle scene, uint renderNode, uint material,
        float* view, uint constantColor, out ShaderLighting output);
}
