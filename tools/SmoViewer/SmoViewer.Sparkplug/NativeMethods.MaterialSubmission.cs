using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;

namespace SmoViewer.Sparkplug;

internal static unsafe partial class NativeMethods
{
    [StructLayout(LayoutKind.Sequential)] internal struct MaterialDrawPass
    {
        public uint Pass, VertexAlpha, KnownRender, KnownUV;
        public fixed uint Render[16];
        public fixed float Colors[17];
        public fixed uint Textures[8], Stages[80], KnownStages[8];
        public fixed float UV[128];
    }
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr spv_material_submission_create(GraphHandle graph);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void spv_material_submission_destroy(IntPtr handle);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int spv_material_submission_capture(MaterialSubmissionHandle handle,
        uint material, uint frame, MaterialDrawPass* output, uint capacity, out uint count);
}

internal sealed class MaterialSubmissionHandle : SafeHandleZeroOrMinusOneIsInvalid
{
    internal MaterialSubmissionHandle(IntPtr pointer) : base(true) => SetHandle(pointer);
    protected override bool ReleaseHandle() { NativeMethods.spv_material_submission_destroy(handle); return true; }
}
