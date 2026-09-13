using SmoViewer.Sparkplug;
using System.Numerics;

namespace SmoViewer.Core;

public sealed record SmoUvControllerData(
    SmoFunctionalEvaluatorData TranslationX,
    SmoFunctionalEvaluatorData TranslationY,
    SmoFunctionalEvaluatorData TranslationZ,
    SmoFunctionalEvaluatorData ScaleX,
    SmoFunctionalEvaluatorData ScaleY,
    SmoFunctionalEvaluatorData ScaleZ,
    SmoFunctionalEvaluatorData Rotation,
    Vector3 UvPivot,
    Vector3 RotationAxis);

/// <summary>Calls the original TransFunctionEval serializer for the UV field body.</summary>
public static class SmoUvControllerDecoder
{
    public const int EvaluatorCount = 7;
    public const int VectorPayloadSize = 3 * sizeof(float);

    public static unsafe bool TryDecode(ReadOnlySpan<byte> payload, out SmoUvControllerData? value)
    {
        value = null;
        NativeMethods.UvFunctions source;
        fixed (byte* bytes = payload)
            if (NativeMethods.spv_uv_functions_read(bytes, (uint)payload.Length, out source) == 0) return false;
        value = new SmoUvControllerData(
            SmoFunctionalEvaluatorData.FromNative(source.Tx), SmoFunctionalEvaluatorData.FromNative(source.Ty),
            SmoFunctionalEvaluatorData.FromNative(source.Tz), SmoFunctionalEvaluatorData.FromNative(source.Sx),
            SmoFunctionalEvaluatorData.FromNative(source.Sy), SmoFunctionalEvaluatorData.FromNative(source.Sz),
            SmoFunctionalEvaluatorData.FromNative(source.Rotation), source.Pivot, source.Axis);
        return true;
    }
}
