using System.Numerics;

namespace SmoViewer.Sparkplug;

/// <summary>Owned view of one native spAnimTrack PRS role; contains no sampling equations.</summary>
public sealed class SparkplugAnimationChannel
{
    private readonly SparkplugAnimationClip _owner;
    private readonly uint _track, _role;
    public bool IsRotation => _role == 1;
    public bool HasKeys => SourceKeyCount != 0;
    public bool IsScalar { get; }
    public bool IsCubic => Representations.Any(value => value is 2 or 4);
    public int SourceKeyCount { get; }
    public IReadOnlyList<float> KeyTimes { get; }
    public IReadOnlyList<uint> Representations { get; }

    internal unsafe SparkplugAnimationChannel(SparkplugAnimationClip owner, uint track, uint role)
    {
        _owner = owner; _track = track; _role = role;
        NativeMethods.Check(NativeMethods.spv_clip_channel(owner.Handle, track, role, out var info));
        SourceKeyCount = checked((int)info.SourceKeys); IsScalar = info.Axes == 3;
        float[] times = new float[info.UniqueTimes];
        fixed (float* output = times) NativeMethods.Check(NativeMethods.spv_clip_times(owner.Handle, track, role, output, (uint)times.Length));
        KeyTimes = Array.AsReadOnly(times);
        uint[] representations = new uint[info.Axes];
        for (int i = 0; i < representations.Length; i++) representations[i] = info.Representations[i];
        Representations = Array.AsReadOnly(representations);
    }
    public Vector3 SampleVector(float time, Vector3 fallback)
    {
        if (IsRotation) throw new InvalidOperationException("Rotation channel requires SampleRotation.");
        var value = _owner.Sample(_track, time);
        return (value.ValidRoles & (1u << (int)_role)) == 0 ? fallback : _role == 0 ? value.Position : value.Scale;
    }
    public Quaternion SampleRotation(float time, Quaternion fallback)
    {
        if (!IsRotation) throw new InvalidOperationException("Vector channel requires SampleVector.");
        var value = _owner.Sample(_track, time);
        return (value.ValidRoles & 2) == 0 ? fallback : value.Rotation;
    }
}
