using System.Collections.ObjectModel;
using System.Numerics;
using SmoViewer.Sparkplug;

namespace SmoViewer.Core;

/// <summary>Tool name binding: case sensitive, merge disjoint properties, report conflicting duplicates.</summary>
public static class SmoAnimationBinding
{
    public static IReadOnlyDictionary<string, SmoAnimationTrack> BindByName(
        IEnumerable<SmoAnimationTrack> source, ICollection<string> warnings)
    {
        var tracks = source.ToArray();
        var roles = SelectRoles(tracks.Select(t => (t.NodeName, t.Positions.Count, t.Rotations.Count, t.Scales.Count)), warnings);
        var result = new Dictionary<string, SmoAnimationTrack>(StringComparer.Ordinal);
        foreach (var (name, selected) in roles)
        {
            SmoAnimationTrack? position = selected[0] >= 0 ? tracks[selected[0]] : null;
            SmoAnimationTrack? rotation = selected[1] >= 0 ? tracks[selected[1]] : null;
            SmoAnimationTrack? scale = selected[2] >= 0 ? tracks[selected[2]] : null;
            result.Add(name, new(name, position?.Positions ?? [], rotation?.Rotations ?? [], scale?.Scales ?? [])
                { PositionChannel = position?.PositionChannel, RotationChannel = rotation?.RotationChannel,
                  ScaleChannel = scale?.ScaleChannel });
        }
        return new ReadOnlyDictionary<string, SmoAnimationTrack>(result);
    }

    // Shared application binding policy for both the scene adapter and export snapshots.
    // This is not the original actor/skeleton binding algorithm.
    internal static Dictionary<string, int[]> SelectRoles(
        IEnumerable<(string Name, int Position, int Rotation, int Scale)> tracks, ICollection<string> warnings)
    {
        var result = new Dictionary<string, int[]>(StringComparer.Ordinal);
        string[] labels = ["position", "rotation", "scale"];
        int index = 0;
        foreach (var track in tracks)
        {
            if (!result.TryGetValue(track.Name, out var roles)) result.Add(track.Name, roles = [-1, -1, -1]);
            int[] counts = [track.Position, track.Rotation, track.Scale];
            for (int role = 0; role < 3; role++) if (counts[role] > 0)
            {
                if (roles[role] >= 0) warnings.Add($"SAN '{track.Name}': repeated {labels[role]} curve; using the first.");
                else roles[role] = index;
            }
            index++;
        }
        return result;
    }
}

/// <summary>
/// Produces linear target-format keys by sampling the same prepared channels as
/// Viewer. Preserves source times and adjacent float32 values before key jumps;
/// curved motion is exact at the 30fps grid, approximated between exported keys.
/// </summary>
public static class SmoAnimationBaker
{
    public const int FramesPerSecond = 30;
    // Blender 4.5 merges near-coincident keys on import. An additional sample
    // 0.02 frame before each boundary limits the resulting jump ramp to this
    // small interval; the exact adjacent-float key is retained for receivers
    // that preserve it. This is target-format approximation, not native math.
    public const float BoundaryGuardSeconds = 1f/1500;
    public const int MaximumOutputKeys = 500_000;

    public static SmoAnimationTrack Bake(SmoAnimationTrack source, float duration, ref int keyBudget, float timeOffset = 0)
    {
        if (!float.IsFinite(duration) || duration < 0) throw new SmoFormatException("Invalid animation duration.");
        if (!float.IsFinite(timeOffset) || timeOffset < 0) throw new SmoFormatException("Invalid animation time offset.");
        return new(source.NodeName,
            VectorKeys(source.PositionChannel, source.Positions, duration, timeOffset, ref keyBudget),
            RotationKeys(source.RotationChannel, source.Rotations, duration, timeOffset, ref keyBudget),
            VectorKeys(source.ScaleChannel, source.Scales, duration, timeOffset, ref keyBudget));
    }

    private static IReadOnlyList<SmoAnimationKey<Vector3>> VectorKeys(SparkplugAnimationChannel? channel,
        IReadOnlyList<SmoAnimationKey<Vector3>> legacy, float duration, float offset, ref int keyBudget)
    {
        if (channel is null) return ShiftLegacy(legacy, offset, ref keyBudget);
        if (!channel.HasKeys) return [];
        var times = SampleTimes(channel, duration, offset, ref keyBudget);
        return times.Select(time => new SmoAnimationKey<Vector3>(time.Key, channel.SampleVector(time.Value, Vector3.Zero))).ToArray();
    }

    private static IReadOnlyList<SmoAnimationKey<Quaternion>> RotationKeys(SparkplugAnimationChannel? channel,
        IReadOnlyList<SmoAnimationKey<Quaternion>> legacy, float duration, float offset, ref int keyBudget)
    {
        if (channel is null) return ShiftLegacy(legacy, offset, ref keyBudget);
        if (!channel.HasKeys) return [];
        var times = SampleTimes(channel, duration, offset, ref keyBudget);
        return times.Select(time => new SmoAnimationKey<Quaternion>(time.Key, channel.SampleRotation(time.Value, Quaternion.Identity))).ToArray();
    }

    private static SortedDictionary<float, float> SampleTimes(SparkplugAnimationChannel channel, float duration, float offset, ref int keyBudget)
    {
        float sourceStart = Math.Min(0, channel.KeyTimes[0]), sourceEnd = Math.Max(duration, channel.KeyTimes[^1]);
        float start = Shift(sourceStart, offset), end = Shift(sourceEnd, offset);
        var times = new SortedDictionary<float, float> { [start] = sourceStart, [end] = sourceEnd };
        if (channel.SourceKeyCount == (channel.IsScalar ? 3 : 1))
        {
            Consume(times.Count, ref keyBudget);
            return times;
        }
        double firstFrame = Math.Ceiling((double)start*FramesPerSecond), lastFrame = Math.Floor((double)end*FramesPerSecond);
        if (lastFrame-firstFrame+1 > keyBudget || firstFrame < int.MinValue || lastFrame > int.MaxValue)
            throw new SmoFormatException("SAN sampling exceeds the 500000-key export limit; reduce the selected animation set.");
        for (long frame = (long)firstFrame; frame <= (long)lastFrame; frame++)
        {
            float time = (float)((double)frame/FramesPerSecond);
            times.TryAdd(time, (float)((double)time-offset));
        }
        // Choose the preceding representable time in the TARGET timeline.
        // Shifting a source BitDecrement(0) by +1 would round to 1 and erase
        // the two-key endpoint jump. Values still come from native source time.
        float previous = float.NegativeInfinity;
        var boundaries = new SortedDictionary<float, float>();
        foreach (float sourceTime in channel.KeyTimes)
        {
            float time = Shift(sourceTime, offset);
            if (time <= previous)
                throw new SmoFormatException("SAN source key times collapse after the float32 timeline shift.");
            float guard = time-BoundaryGuardSeconds;
            if (guard > previous && guard >= start && guard < time)
                times.TryAdd(guard, (float)((double)guard-offset));
            float before = MathF.BitDecrement(time);
            if (before >= start && before > previous)
                times[before] = MathF.BitDecrement(sourceTime);
            boundaries.Add(time, sourceTime);
            previous = time;
        }
        foreach (var boundary in boundaries) times[boundary.Key] = boundary.Value;
        Consume(times.Count, ref keyBudget);
        return times;
    }

    private static IReadOnlyList<SmoAnimationKey<T>> ShiftLegacy<T>(IReadOnlyList<SmoAnimationKey<T>> keys,
        float offset, ref int keyBudget)
    {
        Consume(keys.Count, ref keyBudget);
        if (offset == 0) return keys;
        var result = keys.Select(key => key with { Time = Shift(key.Time, offset) }).ToArray();
        for (int i = 1; i < result.Length; i++)
            if (result[i].Time <= result[i-1].Time)
                throw new SmoFormatException("SAN source key times collapse after the float32 timeline shift.");
        return result;
    }

    private static float Shift(float time, float offset)
    {
        float result = (float)((double)time+offset);
        if (!float.IsFinite(result)) throw new SmoFormatException("SAN shifted time is not finite.");
        return result;
    }

    private static void Consume(int count, ref int keyBudget)
    {
        if (count > keyBudget) throw new SmoFormatException("SAN sampling exceeds the 500000-key export limit.");
        keyBudget -= count;
    }
}
