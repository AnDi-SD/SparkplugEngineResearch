using System.Numerics;

namespace SmoViewer.Core;
/// <summary>Scalar projection from the actual shared SphereBV reader.</summary>
public static class SmoSphereBoundingVolumeDecoder
{
    public const int PositionPayloadSize=3*sizeof(float);
    public const int RadiusPayloadSize=sizeof(float);
    public static bool TryDecodePosition(ReadOnlySpan<byte> payload,out Vector3 value)
    {
        value=default;
        if(!SmoBoundingVolumeFieldProjection.TryRead(SmoClassIds.SphereBoundingVolume,0,payload,out var state))return false;
        value=new(state.Values.X,state.Values.Y,state.Values.Z);return true;
    }
    public static bool TryDecodeRadius(ReadOnlySpan<byte> payload,out float value)
    {
        value=default;
        if(!SmoBoundingVolumeFieldProjection.TryRead(SmoClassIds.SphereBoundingVolume,1,payload,out var state))return false;
        value=state.Values.X;return true;
    }
}
