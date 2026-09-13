using SmoViewer.Sparkplug;

namespace SmoViewer.Core;

public sealed record SmoFunctionalEvaluatorData(
    uint FunctionType,
    float Frequency,
    float Amplitude,
    float XOffset,
    float YOffset,
    float Pitch)
{
    internal static SmoFunctionalEvaluatorData FromNative(NativeMethods.FunctionInfo value) =>
        new(value.Type, value.Frequency, value.Amplitude, value.XOffset, value.YOffset, value.Pitch);
}

public sealed record SmoColorFunctionalEvaluatorData(
    uint Color1,
    uint Color2,
    uint FunctionType,
    float Frequency,
    float Amplitude,
    float XOffset,
    float YOffset,
    float Pitch);

public sealed record SmoMaterialColorControllerData(
    SmoColorFunctionalEvaluatorData Ambient,
    SmoColorFunctionalEvaluatorData Diffuse,
    SmoColorFunctionalEvaluatorData Specular,
    SmoColorFunctionalEvaluatorData Emissive,
    SmoFunctionalEvaluatorData Alpha);

/// <summary>Uses the original packed evaluator reader, shared with full controller loading.</summary>
public static class SmoMaterialColorControllerDecoder
{
    public static unsafe bool TryDecode(ReadOnlySpan<byte> payload, out SmoMaterialColorControllerData? value)
    {
        value = null;
        NativeMethods.ColorFunctions source;
        fixed (byte* bytes = payload)
            if (NativeMethods.spv_color_functions_read(bytes, (uint)payload.Length, out source) == 0) return false;
        value = new SmoMaterialColorControllerData(FromNative(source.Ambient), FromNative(source.Diffuse),
            FromNative(source.Specular), FromNative(source.Emissive), SmoFunctionalEvaluatorData.FromNative(source.Alpha));
        return true;
    }

    private static SmoColorFunctionalEvaluatorData FromNative(NativeMethods.ColorFunctionInfo value) =>
        new(value.First, value.Second, value.Function.Type, value.Function.Frequency, value.Function.Amplitude,
            value.Function.XOffset, value.Function.YOffset, value.Function.Pitch);
}
