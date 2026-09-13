using SmoViewer.Sparkplug;

namespace SmoViewer.Core;
// Host transport only; actual readers and derived-size arithmetic live in Sparkplug.
internal static class SmoBoundingVolumeFieldProjection
{
    public static unsafe bool TryRead(uint classId,uint field,ReadOnlySpan<byte> payload,out NativeMethods.BoundingVolumeField value)
    {
        value=default;
        try{fixed(byte* bytes=payload)NativeMethods.Check(NativeMethods.spv_bv_scalar_read(classId,field,bytes,checked((uint)payload.Length),out value));return true;}
        catch(InvalidDataException){return false;}
    }
}
