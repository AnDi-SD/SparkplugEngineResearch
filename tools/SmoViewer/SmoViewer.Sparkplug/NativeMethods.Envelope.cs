using System.Runtime.InteropServices;

namespace SmoViewer.Sparkplug;

internal static unsafe partial class NativeMethods
{
    [StructLayout(LayoutKind.Sequential)] internal struct EnvelopeEntry
    { public uint Id, ClassId, Offset, Size, NameOffset, NameSize; }
    [StructLayout(LayoutKind.Sequential)] internal struct EnvelopeLayout
    { public uint DataOffset, FileSize; }
    [StructLayout(LayoutKind.Sequential)] internal struct EnvelopeHeader
    { public uint Signature, Version, ExportTag, PlatformMask; }
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int spv_envelope_measure(EnvelopeEntry* entries, uint count,
        byte* names, uint nameSize, uint dataSize, out EnvelopeLayout layout);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int spv_envelope_write(in EnvelopeHeader header,
        EnvelopeEntry* entries, uint count, byte* names, uint nameSize, uint dataSize,
        byte* output, uint capacity);
}
