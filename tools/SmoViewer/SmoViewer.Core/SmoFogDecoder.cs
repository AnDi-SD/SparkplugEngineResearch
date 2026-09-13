using SmoViewer.Sparkplug;

namespace SmoViewer.Core;

public enum SmoFogType : uint
{
    None = 0,
    Exponential = 1,
    ExponentialSquared = 2,
    Linear = 3
}

public sealed record SmoFogData(
    uint Type,
    uint Color,
    float Start,
    float End,
    float Density);

/// <summary>
/// Projects the single Fog payload through the shared spFogSerializer and
/// actual spFog class. This is field inspection, not whole-resource loading.
/// </summary>
public static class SmoFogDecoder
{
    public const int PayloadSize = 5 * sizeof(uint);

    public static unsafe bool TryDecode(
        ReadOnlySpan<byte> payload,
        out SmoFogData? value)
    {
        value = null;
        fixed (byte* bytes=payload)
        {
            if(NativeMethods.spv_fog_payload_read(bytes,checked((uint)payload.Length),out var fog)==0)return false;
            value=new(fog.Type,fog.Color,fog.Start,fog.End,fog.Density);return true;
        }
    }

    public static string GetTypeName(uint type) => type switch
    {
        (uint)SmoFogType.None => "none",
        (uint)SmoFogType.Exponential => "exponential",
        (uint)SmoFogType.ExponentialSquared => "exponential-squared",
        (uint)SmoFogType.Linear => "linear",
        _ => $"unknown-{type}"
    };

}
