namespace WinxCoop.RuntimeProbe;

/// <summary>
/// Two-Bloom display-only owner. Targets must already have completed canonical
/// native application and its ACK. This owner cannot acknowledge a snapshot,
/// change gameplay or predict beyond the last server target.
/// </summary>
internal sealed class WorldPresentationSmoothing : IDisposable
{
    private sealed class Target(NativePoseInterpolator native, WorldPresentationActorState canonical)
    {
        internal NativePoseInterpolator Native { get; } = native;
        internal WorldPresentationActorState Canonical { get; set; } = canonical;
    }
    private readonly uint _epoch, _generation, _duration;
    private readonly Action<object> _log;
    private readonly int _thread = Environment.CurrentManagedThreadId;
    private readonly Dictionary<uint, Target> _targets = [];
    private readonly Dictionary<uint, float[]> _scales = [];
    private ulong _latestRevision, _latestNativeTick, _acknowledgedSnapshots, _samples, _retargets, _snaps, _clears, _pausedSnapshots;
    private long _lastMilliseconds = -1;
    private bool _disposed;
    private const double SnapDistanceSquared = 512d * 512d;

    internal WorldPresentationSmoothing(uint epoch, uint generation, Action<object> log, int durationMilliseconds = 100)
    {
        Require(epoch > 0 && generation > 0 && durationMilliseconds is >= 1 and <= 200,
            "World display smoothing requires a finite immutable epoch/generation and 1..200ms duration.");
        _epoch = epoch; _generation = generation; _duration = checked((uint)durationMilliseconds); _log = log;
    }

    internal object Summary => new { epoch = _epoch, generation = _generation, disposed = _disposed,
        activeTargets = _targets.Count, qualifiedScales = _scales.Count, durationMilliseconds = _duration,
        latestAcknowledgedRevision = _latestRevision, latestAcknowledgedNativeTick = _latestNativeTick,
        acknowledgedSnapshots = _acknowledgedSnapshots, sampledBodies = _samples, retargets = _retargets,
        snaps = _snaps, clears = _clears, pausedSnapshots = _pausedSnapshots,
        bloomOnly = true, sharedCppInterpolator = true, extrapolation = false,
        snapshotAcknowledgmentsAdded = 0, gameplayWrites = 0, nativeGameCallsAdded = 0 };

    internal void AcceptAcknowledgedSnapshot(WorldStateSnapshot snapshot, long localMilliseconds)
    {
        EnsureOwner(); ObserveClock(localMilliseconds);
        WorldPresentationActorState[] bodies = snapshot.Bodies.OrderBy(value => value.StableId).ToArray();
        Require(snapshot.Revision > _latestRevision && snapshot.NativeTick > _latestNativeTick &&
            bodies.Length == 2 && bodies[0].StableId == 1 && bodies[1].StableId == 2 &&
            bodies.All(value => value.TypeId == WorldPresentationActorState.BloomType &&
                (value.Flags & WorldPresentationActorState.Present) != 0),
            "World display target is outside its two acknowledged Bloom identities/order.");
        foreach (var body in bodies)
        {
            body.Validate();
            if (_scales.TryGetValue(body.StableId, out var scale))
                Require(scale.SequenceEqual(body.Scale), "World display smoothing scale changed inside its immutable scene cohort.");
            else _scales.Add(body.StableId, (float[])body.Scale.Clone());
        }
        _latestRevision = snapshot.Revision; _latestNativeTick = snapshot.NativeTick; ++_acknowledgedSnapshots;
        if ((snapshot.Flags & WorldStateContract.NativePaused) != 0)
        {
            ++_pausedSnapshots;
            ClearCore("acknowledged-native-pause");
            return;
        }
        foreach (var body in bodies)
        {
            if (!_targets.TryGetValue(body.StableId, out var target))
            {
                var native = new NativePoseInterpolator(_log);
                try { native.Reset(body, localMilliseconds); }
                catch { native.Dispose(); throw; }
                _targets.Add(body.StableId, new(native, body)); ++_snaps;
                continue;
            }
            double distanceSquared = 0;
            for (int axis = 0; axis < 3; ++axis)
            {
                double delta = (double)body.Position[axis] - target.Canonical.Position[axis];
                distanceSquared += delta * delta;
            }
            if (distanceSquared > SnapDistanceSquared || (body.Flags & WorldPresentationActorState.Dead) != 0)
            {
                target.Native.Clear(); target.Native.Reset(body, localMilliseconds); ++_snaps;
                if (distanceSquared > SnapDistanceSquared)
                    _log(new { @event = "world-display-large-jump-snapped", epoch = _epoch, generation = _generation,
                        body.StableId, acknowledgedRevision = snapshot.Revision, distanceSquared,
                        thresholdSquared = SnapDistanceSquared, canonicalStateChanged = false });
            }
            else { target.Native.SetTarget(body, localMilliseconds, _duration); ++_retargets; }
            target.Canonical = body;
        }
        Require(_targets.Count == 2, "World display smoothing lost one of its finite targets.");
    }

    internal IReadOnlyList<WorldPresentationActorState> SampleAtWorld(long localMilliseconds)
    {
        EnsureOwner(); ObserveClock(localMilliseconds);
        if (_targets.Count == 0) return Array.Empty<WorldPresentationActorState>();
        Require(_targets.Count == 2, "World display smoothing has an incomplete target cohort.");
        var result = new WorldPresentationActorState[2];
        for (uint id = 1; id <= 2; ++id)
        {
            Target target = _targets[id];
            result[id - 1] = target.Native.Sample(target.Canonical, localMilliseconds);
            ++_samples;
        }
        // Always re-emit. A canonical ACK may have just overwritten the actual
        // root with the target even when this sampled float pose repeats.
        return result;
    }

    internal void Clear(string reason)
    {
        EnsureOwner(); Require(!string.IsNullOrWhiteSpace(reason) && reason.Length <= 120,
            "World display reset requires a bounded explicit reason.");
        ClearCore(reason);
    }

    private void ClearCore(string reason)
    {
        if (_targets.Count == 0) return;
        foreach (Target target in _targets.Values) target.Native.Dispose();
        _targets.Clear(); ++_clears;
        _log(new { @event = "world-display-smoothing-cleared", epoch = _epoch, generation = _generation,
            reason, latestAcknowledgedRevision = _latestRevision, nativeGameReads = 0, nativeGameWrites = 0,
            nextAcknowledgedTargetSnaps = true });
    }

    private void ObserveClock(long now)
    {
        Require(now >= 0 && now >= _lastMilliseconds, "World display local clock moved backwards.");
        _lastMilliseconds = now;
    }
    private void EnsureOwner() => Require(!_disposed && Environment.CurrentManagedThreadId == _thread,
        "World display smoothing left its owning Actor thread/lifetime.");
    public void Dispose()
    {
        if (_disposed) return;
        EnsureOwner();
        try { ClearCore("owner-disposed"); }
        finally { _disposed = true; _scales.Clear(); }
    }
    private static void Require(bool value, string message) { if (!value) throw new InvalidDataException(message); }
}
