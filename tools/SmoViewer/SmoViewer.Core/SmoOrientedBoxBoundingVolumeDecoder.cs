using System.Numerics;

namespace SmoViewer.Core;
/// <summary>Host inspection DTO; half extents are a derived view, not invented persistent members.</summary>
public sealed record SmoOrientedBoxSizeData(Vector3 FullSize,Vector3 HalfExtents,float BoundingSphereRadius);
/// <summary>Scalar projection of the actual common class/reader. No local size arithmetic.</summary>
public static class SmoOrientedBoxBoundingVolumeDecoder
{
    public const int VectorPayloadSize=3*sizeof(float);
    public static bool TryDecodePosition(ReadOnlySpan<byte> payload,out Vector3 value)
    {
        value=default;
        if(!SmoBoundingVolumeFieldProjection.TryRead(SmoClassIds.OrientedBoxBoundingVolume,0,payload,out var state))return false;
        value=new(state.Values.X,state.Values.Y,state.Values.Z);return true;
    }
    public static bool TryDecodeSize(ReadOnlySpan<byte> payload,out SmoOrientedBoxSizeData? value)
    {
        value=null;
        if(!SmoBoundingVolumeFieldProjection.TryRead(SmoClassIds.OrientedBoxBoundingVolume,1,payload,out var state))return false;
        value=new(new(state.Values.X,state.Values.Y,state.Values.Z),state.HalfExtents,state.BoundingRadius);return true;
    }
    public const int QuaternionPayloadSize=4*sizeof(float);
    public static bool TryDecodeRotation(ReadOnlySpan<byte> payload,out Quaternion value)
    {
        value=Quaternion.Identity;
        if(!SmoBoundingVolumeFieldProjection.TryRead(SmoClassIds.OrientedBoxBoundingVolume,2,payload,out var state))return false;
        value=new(state.Values.X,state.Values.Y,state.Values.Z,state.Values.W);return true;
    }
}
