using System.Numerics;
using System.Runtime.CompilerServices;
using SmoViewer.Sparkplug;

namespace SmoViewer.Core;

public sealed record SmoAnimationKey<T>(float Time, T Value);

public sealed record SmoAnimationTrack(
    string NodeName,
    IReadOnlyList<SmoAnimationKey<Vector3>> Positions,
    IReadOnlyList<SmoAnimationKey<Quaternion>> Rotations,
    IReadOnlyList<SmoAnimationKey<Vector3>> Scales)
{
    public SparkplugAnimationChannel? PositionChannel { get; init; }
    public SparkplugAnimationChannel? RotationChannel { get; init; }
    public SparkplugAnimationChannel? ScaleChannel { get; init; }
    private static readonly ConditionalWeakTable<SmoAnimationTrack, SparkplugAnimationClip> LinearSnapshots = new();

    public SmoAnimationPose Sample(float time, Vector3 position, Quaternion rotation, Vector3 scale)
    {
        if (!float.IsFinite(time)) throw new ArgumentOutOfRangeException(nameof(time));
        SparkplugAnimationClip? linear =
            PositionChannel is null && Positions.Count != 0 ||
            RotationChannel is null && Rotations.Count != 0 ||
            ScaleChannel is null && Scales.Count != 0
                ? LinearSnapshots.GetValue(this, static track => SparkplugAnimationClip.CreateLinear(
                    track.Positions.Select(k => k.Time).ToArray(), track.Positions.Select(k => k.Value).ToArray(),
                    track.Rotations.Select(k => k.Time).ToArray(), track.Rotations.Select(k => k.Value).ToArray(),
                    track.Scales.Select(k => k.Time).ToArray(), track.Scales.Select(k => k.Value).ToArray()))
                : null;
        return new(
            (PositionChannel ?? linear?.Channel(0, 0))?.SampleVector(time, position) ?? position,
            (RotationChannel ?? linear?.Channel(0, 1))?.SampleRotation(time, rotation) ?? rotation,
            (ScaleChannel ?? linear?.Channel(0, 2))?.SampleVector(time, scale) ?? scale);
    }
}

public readonly record struct SmoAnimationPose(Vector3 Position, Quaternion Rotation, Vector3 Scale);

public sealed record SmoAnimationClip(string Path, float Duration, IReadOnlyList<SmoAnimationTrack> Tracks)
{
    public int FrameCount => Tracks.SelectMany(track => new[]
        { track.Positions.Count, track.Rotations.Count, track.Scales.Count }).DefaultIfEmpty().Max();
}

/// <summary>Tool API over the shared C++ SAN reader and sampler; no separate wire decoder.</summary>
public static class SmoAnimationDecoder
{
    public const uint AnimationClassHash = SmoClassIds.Animation;
    public const int MaximumFileBytes = 64*1024*1024;

    public static bool TryDecode(string path, out SmoAnimationClip? clip, out string error)
    {
        try { clip = Adapt(SparkplugAnimationClip.Load(path)); error = string.Empty; return true; }
        catch (Exception exception) { clip = null; error = exception.Message; return false; }
    }
    public static bool TryDecode(ReadOnlyMemory<byte> data, string path, out SmoAnimationClip? clip, out string error)
    {
        try { clip = Adapt(SparkplugAnimationClip.Load(data.Span, path)); error = string.Empty; return true; }
        catch (Exception exception) { clip = null; error = exception.Message; return false; }
    }
    private static SmoAnimationClip Adapt(SparkplugAnimationClip native)
    {
        try
        {
            var tracks = new SmoAnimationTrack[native.Tracks.Count];
            for (int i = 0; i < tracks.Length; i++)
            {
                var position = native.Channel(i, 0); var rotation = native.Channel(i, 1); var scale = native.Channel(i, 2);
                tracks[i] = new(native.Tracks[i].NodeName,
                    position.KeyTimes.Select(t => new SmoAnimationKey<Vector3>(t, position.SampleVector(t, Vector3.Zero))).ToArray(),
                    rotation.KeyTimes.Select(t => new SmoAnimationKey<Quaternion>(t, rotation.SampleRotation(t, Quaternion.Identity))).ToArray(),
                    scale.KeyTimes.Select(t => new SmoAnimationKey<Vector3>(t, scale.SampleVector(t, Vector3.One))).ToArray())
                    { PositionChannel = position, RotationChannel = rotation, ScaleChannel = scale };
            }
            // Channels retain the native clip. SafeHandle releases it when the last tool snapshot dies.
            return new(native.Path, native.Duration, Array.AsReadOnly(tracks));
        }
        catch { native.Dispose(); throw; }
    }
}
