using System.Text;
using System.Numerics;

namespace SmoViewer.Sparkplug;

public sealed record SparkplugAnimationTrack(string NodeName, int PositionKeys, int RotationKeys, int ScaleKeys);

/// <summary>Owns a reconstructed C++ spAnimation loaded by spSerializerManager.</summary>
public sealed class SparkplugAnimationClip : IDisposable
{
    internal ClipHandle Handle { get; }
    public string Path { get; }
    public float Duration { get; }
    public IReadOnlyList<SparkplugAnimationTrack> Tracks { get; }
    private readonly SparkplugAnimationChannel?[,] _channels;
    private readonly Dictionary<uint, (float Time, NativeMethods.Sample Value)> _samples = [];
    private readonly object _gate = new();
    public int FrameCount => Tracks.Select(t => Math.Max(t.PositionKeys, Math.Max(t.RotationKeys, t.ScaleKeys))).DefaultIfEmpty().Max();
    private unsafe SparkplugAnimationClip(ClipHandle handle, string path)
    {
        Handle = handle;
        try
        {
            NativeMethods.Check(NativeMethods.spv_clip_info(Handle, out float duration, out uint count));
            Duration = duration; Path = path;
            _channels = new SparkplugAnimationChannel[count, 3];
            var tracks = new List<SparkplugAnimationTrack>((int)count);
            byte[] name = new byte[65536];
            for (uint i = 0; i < count; i++)
            {
                NativeMethods.TrackInfo info;
                fixed (byte* output = name) NativeMethods.Check(NativeMethods.spv_clip_track(Handle, i, output, (uint)name.Length, out info));
                tracks.Add(new(Encoding.Latin1.GetString(name.AsSpan(0, Array.IndexOf(name, (byte)0))),
                    (int)info.PositionKeys, (int)info.RotationKeys, (int)info.ScaleKeys));
            }
            Tracks = tracks.AsReadOnly();
        }
        catch { Handle.Dispose(); throw; }
    }
    public static SparkplugAnimationClip Load(string path)
    {
        using var stream = File.OpenRead(path);
        if (stream.Length > 64*1024*1024) throw new InvalidDataException("SAN exceeds 64 MiB.");
        byte[] bytes = new byte[checked((int)stream.Length)]; stream.ReadExactly(bytes);
        return Load(bytes, path);
    }
    public static unsafe SparkplugAnimationClip Load(ReadOnlySpan<byte> bytes, string name = "memory.san")
    {
        fixed (byte* input = bytes)
            return new(new(NativeMethods.Check(NativeMethods.spv_clip_load(input, (uint)bytes.Length))), name);
    }
    public SparkplugAnimationChannel Channel(int track, int role)
    {
        if ((uint)track >= Tracks.Count || (uint)role >= 3) throw new ArgumentOutOfRangeException();
        lock (_gate)
        {
            ObjectDisposedException.ThrowIf(Handle.IsClosed, this);
            return _channels[track, role] ??= new(this, (uint)track, (uint)role);
        }
    }
    internal NativeMethods.Sample Sample(uint track, float time)
    {
        lock (_gate)
        {
            ObjectDisposedException.ThrowIf(Handle.IsClosed, this);
            if (_samples.TryGetValue(track, out var cached) && cached.Time == time) return cached.Value;
            NativeMethods.Check(NativeMethods.spv_clip_sample(Handle, track, time, out var sample));
            _samples[track] = (time, sample);
            return sample;
        }
    }
    // Adapts tool-created flat key lists to the same spAnimTrack implementation.
    public static unsafe SparkplugAnimationClip CreateLinear(
        float[] positionTimes, Vector3[] positions, float[] rotationTimes, Quaternion[] rotations,
        float[] scaleTimes, Vector3[] scales)
    {
        if (positionTimes.Length != positions.Length || rotationTimes.Length != rotations.Length || scaleTimes.Length != scales.Length)
            throw new ArgumentException("Key time/value counts differ.");
        fixed (float* pt = positionTimes, rt = rotationTimes, st = scaleTimes)
        fixed (Vector3* pv = positions, sv = scales)
        fixed (Quaternion* rv = rotations)
        {
            NativeMethods.LinearChannel* channels = stackalloc NativeMethods.LinearChannel[3];
            channels[0] = new() { Times = pt, Values = (float*)pv, Count = (uint)positions.Length };
            channels[1] = new() { Times = rt, Values = (float*)rv, Count = (uint)rotations.Length };
            channels[2] = new() { Times = st, Values = (float*)sv, Count = (uint)scales.Length };
            return new(new(NativeMethods.Check(NativeMethods.spv_clip_create_linear(channels, 3))), "tool-linear-keys");
        }
    }
    public void Dispose() { lock (_gate) Handle.Dispose(); }
}
