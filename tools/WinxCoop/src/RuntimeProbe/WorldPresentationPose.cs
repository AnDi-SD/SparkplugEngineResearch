using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

internal sealed record WorldPresentationBinding(uint StableId, uint TypeId, uint Root, uint Actor,
    uint Scene, uint Parent, bool CharacterVisualHalfTurn, WorldPresentationNativeRig? Rig = null);

// The bounded passive Bloom and Mosquito visuals use the same model-facing
// convention. Their network/native body rotation stays unchanged; only the
// receiver-owned visual wrapper receives this presentation basis.
internal static class WorldPresentationModelOrientation
{
    internal static float[] Apply(float[] nativeRotation, bool characterVisualHalfTurn)
    {
        if (nativeRotation.Length != 9) throw new InvalidDataException("Character model rotation requires nine values.");
        float[] rotation = (float[])nativeRotation.Clone();
        if (characterVisualHalfTurn)
            for (int i = 0; i < rotation.Length; ++i)
                if (i < 3 || i >= 6) rotation[i] = -rotation[i];
        return rotation;
    }
}

/// <summary>
/// Guest-only presentation mutation under the separately owned E2 quarantine.
/// No commands, FSM transitions, hit messages, AI, pickups or camera writes.
/// </summary>
internal sealed class WorldPresentationPoseApplier(DebuggerHost host, Func<bool> nativeGameplaySuppressed,
    Action<object> log)
{
    private readonly Dictionary<uint, WorldPresentationBinding> _bindings = [];
    private readonly Dictionary<uint, (int Health, int Maximum)> _health = [];
    private readonly Dictionary<uint, (float[] Position, uint Stop)> _expectedPositions = [];
    private readonly Dictionary<uint, float[]> _observedPositions = [];
    private readonly HashSet<uint> _nativeMoved = [];
    private uint _cacheSamples, _cacheMatches, _cacheMismatches, _cacheNonIdentityParents, _lastCacheStop;
    private readonly int _ownerThread = Environment.CurrentManagedThreadId;
    private uint _epoch, _scene, _applied, _nativeHealthMirrors, _characterVisualHalfTurnApplications;
    private bool _retired;
    internal uint Applied => _applied;
    internal object Summary => new { epoch = _epoch, scene = _scene, retired = _retired,
        bindings = _bindings.Count, applications = _applied, nativeHealthMirrors = _nativeHealthMirrors,
        characterVisualHalfTurnApplications = _characterVisualHalfTurnApplications,
        commandWrites = 0, cameraWrites = 0, nativeDamageCalls = 0, fsmTransitions = 0,
        collisionOrLootSimulationCreated = false, presentationHealth = _health.Select(x =>
            new { stableId = x.Key, hp = x.Value.Health, maximum = x.Value.Maximum }).ToArray(),
        nativeFollowingWorldCacheSamples = _cacheSamples, nativeIdentityParentCacheMatches = _cacheMatches,
        nativeIdentityParentCacheMismatches = _cacheMismatches, cacheNonIdentityParentObservations = _cacheNonIdentityParents,
        nativeCachedBodiesMoved = _nativeMoved.Order().ToArray() };

    internal void BeginEpochAtWorld(uint tid, ref Win32Native.X86Context context, uint epoch, uint scene)
    {
        Ensure(tid, context); Require(_epoch == 0 && !_retired && epoch != 0 && U(scene) == 0x6E7358,
            "World presentation epoch must begin once in its captured native scene.");
        _epoch = epoch; _scene = scene;
    }
    internal void BindAtWorld(uint tid, ref Win32Native.X86Context context, WorldPresentationBinding binding)
    {
        Ensure(tid, context); Require(_epoch != 0 && !_retired && binding.StableId != 0 && binding.Scene == _scene &&
            binding.TypeId is WorldPresentationActorState.BloomType or WorldPresentationActorState.MosquitoType &&
            (!binding.CharacterVisualHalfTurn || binding.Rig is null),
            "World presentation binding has an unqualified epoch/type/model basis.");
        VerifyBinding(binding); Require(_bindings.Count < 66 && _bindings.TryAdd(binding.StableId, binding),
            "World presentation binding is duplicate or exceeds the finite budget.");
    }
    internal void ApplyAtWorld(uint tid, ref Win32Native.X86Context context, uint epoch,
        WorldPresentationActorState state)
    {
        Ensure(tid, context); state.Validate();
        Require(!_retired && _epoch == epoch && _bindings.TryGetValue(state.StableId, out _),
            "World actor application lacks its admitted epoch/stable binding.");
        WorldPresentationBinding binding = _bindings[state.StableId]; VerifyBinding(binding);
        Require(binding.TypeId == state.TypeId && (state.Flags & WorldPresentationActorState.Present) != 0,
            "Absent actor must be removed through its native owner, not applied to a borrowed root.");
        ValidateRotation(state.Rotation);
        float[] rotation = WorldPresentationModelOrientation.Apply(state.Rotation, binding.CharacterVisualHalfTurn);
        // Same explicit diagnostic ABI writer as the qualified P passive route:
        // local PRS and dirty bit. Native engine updates world/skin caches later.
        Write(binding.Root + 0x20, Bytes(state.Position)); Write(binding.Root + 0x30, Bytes(state.Scale));
        Write(binding.Root + 0x40, Bytes(rotation));
        uint visible = (state.Flags & WorldPresentationActorState.Visible) != 0 ? 0x200u : 0;
        Write(binding.Root + 0xB0, BitConverter.GetBytes((U(binding.Root + 0xB0) & ~0x200u) | visible | 1));
        _expectedPositions[state.StableId] = ((float[])state.Position.Clone(), host.DebugStopSequence);
        bool hasHealth = (state.Flags & WorldPresentationActorState.HasHealth) != 0;
        if (hasHealth)
        {
            _health[state.StableId] = (state.Health, state.MaximumHealth);
            if (binding.Rig is { } rig)
            {
                // Mirror authoritative state only. This never calls the native
                // damage/death/award path and cannot qualify a new native hit.
                Require(rig.TypeId == WorldPresentationActorState.BloomType && U(rig.Traits) == 0x6F6990 &&
                    U(rig.Traits + 0x24) == rig.Instance && U(rig.Traits + 0x18C) == rig.Character,
                    "Guest health mirror is restricted to an admitted stock PlayerTraits owner.");
                Write(rig.Traits + 0x128, BitConverter.GetBytes(state.MaximumHealth));
                Write(rig.Traits + 0x124, BitConverter.GetBytes(state.Health)); ++_nativeHealthMirrors;
            }
        }
        ++_applied;
        if (binding.CharacterVisualHalfTurn) ++_characterVisualHalfTurnApplications;
        if (_applied <= 4 || _applied % 120 == 0)
            log(new { @event = "world-presentation-pose-applied", epoch, state.StableId, state.TypeId,
                state.StateId, state.Animation, state.Health, state.MaximumHealth, nativeGameplaySuppressed = true,
                characterVisualHalfTurn = binding.CharacterVisualHalfTurn,
                authoritativeHpMirror = hasHealth && binding.Rig is not null, passiveHpMetadata = hasHealth && binding.Rig is null,
                nativeWorldCacheUpdateStillRequired = true, nativeDamageOrHitClaimed = false });
    }
    internal void UnbindAtWorld(uint tid, ref Win32Native.X86Context context, uint stableId)
    { Ensure(tid, context); Require(!_retired && _bindings.Remove(stableId), "Unknown presentation removal."); _health.Remove(stableId); _expectedPositions.Remove(stableId); _observedPositions.Remove(stableId); }
    internal void ObserveFollowingWorldCaches(uint tid, ref Win32Native.X86Context context)
    {
        Ensure(tid, context);
        if (_lastCacheStop == host.DebugStopSequence) return;
        _lastCacheStop = host.DebugStopSequence;
        bool paused = Win32Native.ReadMemory(host.ProcessHandle, U(0x755298) + 0x40, 1)[0] != 0;
        foreach (var (id, expected) in _expectedPositions)
        {
            if (expected.Stop >= host.DebugStopSequence) continue;
            var binding = _bindings[id]; VerifyBinding(binding);
            float[] actual = Floats(binding.Root + 0x74, 3), parentPosition = Floats(binding.Parent + 0x74, 3),
                parentScale = Floats(binding.Parent + 0x80, 3), parentRotation = Floats(binding.Parent + 0x8C, 9);
            Require(actual.All(float.IsFinite), "Native cached world position is not finite.");
            bool identity = parentPosition.All(v => Math.Abs(v) < .00001) && parentScale.All(v => Math.Abs(v - 1) < .00001) &&
                parentRotation.Select((v, i) => Math.Abs(v - (i % 4 == 0 ? 1 : 0)) < .00001).All(v => v);
            bool matches = actual.Zip(expected.Position).All(v => Math.Abs(v.First - v.Second) < .02);
            if (paused) continue;
            ++_cacheSamples;
            if (!identity) ++_cacheNonIdentityParents;
            else if (matches) ++_cacheMatches; else ++_cacheMismatches;
            if (_observedPositions.TryGetValue(id, out var before) && actual.Zip(before).Sum(v => (v.First-v.Second)*(v.First-v.Second)) > 1)
                _nativeMoved.Add(id);
            _observedPositions[id] = actual;
            if (_cacheSamples <= 4 || _cacheSamples % 120 == 0)
                log(new { @event = "world-presentation-native-cache", stableId = id, nativeWorldCache = actual,
                    expectedLocalPosition = expected.Position, identityParentComparison = identity, matches,
                    flags = U(binding.Root + 0xB0), originalFollowingWorld = true, paused,
                    cachePositionWrites = 0, nativeCacheSamples = _cacheSamples });
        }
    }
    internal void RetireBeforeUnload(uint tid, ref Win32Native.X86Context context)
    {
        Require(host.IsAtDebugStop && tid == host.MainThreadId && Environment.CurrentManagedThreadId == _ownerThread &&
            !_retired && context.Eip is NativeCallDispatcher.WorldReturn or 0x5F86C0 or 0x5F2300 or 0x4E8AA0 or 0x4E8C80 or 0x59AE80 or 0x59B190,
            "World presentation retirement requires an owned pre-unload boundary.");
        _retired = true; _bindings.Clear(); _health.Clear(); _expectedPositions.Clear(); _observedPositions.Clear();
    }
    private void VerifyBinding(WorldPresentationBinding binding)
    {
        Require(U(binding.Root) == VisualGraphReader.NodeVtable && U(binding.Root + 0x3C) == binding.Scene &&
            U(binding.Root + 0x2C) == binding.Parent && U(binding.Actor) == 0x703F80,
            "World presentation root/parent/scene/actor ownership changed.");
        if (binding.Rig is { } rig)
            Require(rig.Root == binding.Root && rig.Actor == binding.Actor && U(rig.Instance) == 0x710F80 &&
                U(rig.Instance + 0x24) == rig.Root && U(rig.Animator + 0x128) == rig.Actor,
                "World presentation native rig changed.");
    }
    private static void ValidateRotation(float[] values)
    {
        for (int row = 0; row < 3; ++row)
        {
            double norm = Enumerable.Range(0, 3).Sum(i => (double)values[row * 3 + i] * values[row * 3 + i]);
            Require(Math.Abs(norm - 1) < .05, "World presentation rotation row is not unit length.");
            for (int other = row + 1; other < 3; ++other)
                Require(Math.Abs(Enumerable.Range(0, 3).Sum(i => (double)values[row * 3 + i] * values[other * 3 + i])) < .05,
                    "World presentation rotation rows are not orthogonal.");
        }
    }
    private void Ensure(uint tid, Win32Native.X86Context context) => Require(host.IsAtDebugStop && host.ProcessHandle != IntPtr.Zero &&
        tid == host.MainThreadId && context.Eip == NativeCallDispatcher.WorldReturn && !_retired &&
        Environment.CurrentManagedThreadId == _ownerThread && nativeGameplaySuppressed(),
        "World presentation writes require the owned WorldRET and active E2 native quarantine.");
    private uint U(uint address) => Win32Native.ReadUInt32(host.ProcessHandle, address);
    private float[] Floats(uint address, int count)
    { byte[] bytes = Win32Native.ReadMemory(host.ProcessHandle, address, checked(count * 4)); return Enumerable.Range(0, count).Select(i => BitConverter.ToSingle(bytes, i * 4)).ToArray(); }
    private void Write(uint address, byte[] bytes)
    {
        Win32Native.WriteMemory(host.ProcessHandle, address, bytes, false);
        Require(Win32Native.ReadMemory(host.ProcessHandle, address, bytes.Length).SequenceEqual(bytes), "World presentation write readback differs.");
    }
    private static byte[] Bytes(float[] values) => values.SelectMany(BitConverter.GetBytes).ToArray();
    private static void Require(bool value, string message) { if (!value) throw new InvalidDataException(message); }
}
