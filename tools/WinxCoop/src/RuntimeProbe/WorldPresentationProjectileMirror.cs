using System.Reflection.PortableExecutable;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

// The original scene owns every pooled object. Complete runs at the dispatcher
// stop before the saved original Exit instruction resumes, after gate restore.
internal sealed record WorldProjectileRetirementPlan(IReadOnlyList<NativeCallSpec> Calls, Action Complete);

/// <summary>Guest-only inactive Stinger model borrowing; no projectile simulation.</summary>
internal sealed class WorldPresentationProjectileMirror
{
    private const uint StingerType = 0x206A45A2, StingerTable = 0x6F6C7C, ManagerTable = 0x6F7D70;
    private const uint ActivationEntry = 0x501FF0, SetNodeEnabled = 0x421640, Enabled = 0x200;
    private const int MaximumSlots = 6, MaximumActivations = 4096;
    private readonly DebuggerHost _host;
    private readonly PEReader _image;
    private readonly Func<bool> _quarantine;
    private readonly Action<object> _log;
    private readonly IntPtr _process;
    private readonly uint _pid, _thread;
    private readonly int _ownerThread = Environment.CurrentManagedThreadId;
    private readonly List<Binding> _pool = [];
    private readonly Dictionary<uint, Binding> _active = [];
    private readonly HashSet<uint> _seenIds = [], _retiredIds = [];
    private readonly HashSet<int> _touched = [];
    private readonly Queue<VisibilityCall> _visibility = [];
    private readonly Dictionary<int, (WorldPresentationActorState State, uint Stop)> _expected = [];
    private Dictionary<uint, Binding>? _pendingBindings;
    private WorldPresentationActorState[]? _pending, _lastApplied;
    private uint _epoch, _scene, _engine, _parent, _applications, _visibilityCalls, _quietRanges, _restoredRanges;
    private uint _hideOnAbsence, _activationViolations, _idempotentApplies, _cancelledApplies;
    private uint _cacheSamples, _cacheMatches, _cacheMismatches, _lastCacheStop;
    private ulong _pendingRevision, _appliedRevision;
    private bool _retiring, _retired, _failed, _callPending, _ownerProofFailureLogged;
    private string _failure = "";

    private sealed record NodeWitness(uint Address, uint Table, uint Parent, ushort References);
    private sealed record Binding(int Index, WorldPresentationNativeRig Rig, uint Manager, uint Move,
        uint Slot, uint Projectile, uint Node, ushort References, uint TargetBefore,
        byte[] PositionBefore, byte[] ScaleBefore, byte[] RotationBefore, bool OriginalVisible,
        IReadOnlyList<NodeWitness> Tree)
    { internal bool Visible { get; set; } = OriginalVisible; }
    private sealed record VisibilityCall(Binding Binding, bool Visible, bool Absence);
    private sealed record QuietRange(Binding Binding, uint Address, byte[] Before, byte[] Requested, string Name);

    internal WorldPresentationProjectileMirror(DebuggerHost host, PEReader image, Func<bool> quarantine, Action<object> log)
    {
        _host = host ?? throw new ArgumentNullException(nameof(host));
        _image = image ?? throw new ArgumentNullException(nameof(image));
        _quarantine = quarantine ?? throw new ArgumentNullException(nameof(quarantine));
        _log = log ?? throw new ArgumentNullException(nameof(log));
        _process = host.ProcessHandle; _pid = host.ProcessPid; _thread = host.MainThreadId;
        EnsureStopped(_thread);
        PinEnable();
    }

    internal bool ApplyComplete => _pending is null && !_callPending && !_retired && !_retiring && !_failed;
    internal ulong AppliedRevision => _appliedRevision;
    internal ulong PendingRevision => _pendingRevision;
    internal object Summary => new { epoch = _epoch, scene = _scene, parent = _parent,
        retired = _retired, retiring = _retiring, failed = _failed, failure = _failure,
        poolSlots = _pool.Count, touchedPoolSlots = _touched.Count, activePresentationIds = _active.Count,
        observedActivationIds = _seenIds.Count, retiredActivationIds = _retiredIds.Count,
        appliedRevision = _appliedRevision, pendingRevision = _pendingRevision,
        pending = _pending is not null, nativeCallPending = _callPending, pendingVisibility = _visibility.Count,
        applications = _applications, nativeVisibilityCalls = _visibilityCalls, quietRanges = _quietRanges,
        restoredBaselineRanges = _restoredRanges, hideOnAbsence = _hideOnAbsence,
        idempotentApplications = _idempotentApplies, cancelledUnacknowledgedApplies = _cancelledApplies,
        nativeActivationViolations = _activationViolations,
        nativeFollowingWorldCacheSamples = _cacheSamples, nativeIdentityParentCacheMatches = _cacheMatches,
        nativeIdentityParentCacheMismatches = _cacheMismatches,
        guestActive30Writes = 0, nativeActivationCalls = 0, nativeUpdateOrCollisionCalls = 0,
        nativeDamageCalls = 0, nativeObjectsOwned = 0, nativeDeleteOrRetainReleaseCalls = 0,
        characterVisualHalfTurnApplied = false, rawHostProjectilePrsApplied = true,
        renderAcceptanceClaimed = false, runtimeQualificationClaimed = false };

    internal void BeginEpochAtWorld(uint tid, ref Win32Native.X86Context context, uint epoch, uint scene,
        IReadOnlyList<WorldPresentationNativeRig> stockMosquitos)
    {
        EnsureWorld(tid, context);
        Require(_epoch == 0 && epoch != 0 && stockMosquitos.Count is > 0 and <= 2 && U(scene) == 0x6E7358,
            "Projectile borrowing requires one initial Battle47 scene with at most two original Mosquitos.");
        _scene = scene; _engine = Pointer(U(0x755274)); _parent = Pointer(U(scene + 0x14));
        try
        {
            Pin(0x501B50, "B8E8EA7400C3"); Pin(ActivationEntry, "83EC3C568BF1");
            Require(U(StingerTable + 0x10) == 0x501B50 && U(0x74EAE8) == StingerType &&
                U(StingerTable + 0x20) == 0x502210 && U(StingerTable + 0x24) == ActivationEntry,
                "Borrowed projectile is not the exact original Stinger class/update/activation family.");
            VerifyManagerType(); VerifySceneParent();
            HashSet<uint> managers = [], projectiles = [], nodes = [];
            foreach (var rig in stockMosquitos)
            {
                Require(rig.TypeId == WorldPresentationActorState.MosquitoType && rig.Scene == scene,
                    "Projectile source inventory contains another scene/type.");
                uint manager = Pointer(U(rig.Character + 0x140)), move = Pointer(U(rig.Character + 0x12C));
                Require(managers.Add(manager), "Two original Mosquitos alias one projectile manager.");
                byte[] managerBytes = VerifyOwner(rig, manager, move);
                for (uint slot = 0; slot < 3; slot++)
                {
                    uint projectile = Pointer(Word(managerBytes, 0x1D0 + (int)slot * 4));
                    byte[] native = Read(projectile, 0x34); uint node = Word(native, 0x14);
                    _log(new { @event = "world-projectile-mirror-inventory-candidate", manager = Hex(manager), slot,
                        projectile = Hex(projectile), node = Hex(node), table = Hex(Word(native, 0)),
                        sourceManager = Hex(Word(native, 0x18)), active30 = native[0x30], nativeWrites = 0 });
                    Pointer(node); byte[] model = Read(node, 0xB4); ushort references = BitConverter.ToUInt16(model, 8);
                    _log(new { @event = "world-projectile-mirror-node-inventory", slot, projectile = Hex(projectile), node = Hex(node),
                        parent = Hex(Word(model, 0x2C)), expectedOriginalSceneRoot = Hex(_parent), scene = Hex(Word(model, 0x3C)),
                        references, enabled = (Word(model, 0xB0) & Enabled) != 0,
                        localPrsBits = RawPrs(model), nativeWrites = 0, ownershipNotYetGranted = true });
                    Require(Word(native, 0) == StingerTable && Word(native, 0x18) == manager && native[0x30] == 0 &&
                        projectiles.Add(projectile) && nodes.Add(node) && Word(model, 0) == VisualGraphReader.NodeVtable &&
                        Word(model, 0x2C) == _parent && Word(model, 0x3C) == scene && references > 0 &&
                        (Word(model, 0xB0) & Enabled) == 0,
                        "Inactive Stinger pool Node has an unqualified owner/scene/parent/refcount/visibility; inventory above is diagnostic only.");
                    Require(Prs(model).All(float.IsFinite), "Inactive Stinger local/cache PRS contains a nonfinite value.");
                    var graph = new VisualGraphReader(_process).Read(node, inspectSkins: false);
                    Require(graph.Success && graph.Graph.Nodes.Count is > 0 and <= 32,
                        "Inactive Stinger model subtree cannot be bounded before borrowing: " + graph.Error);
                    List<NodeWitness> tree = [];
                    foreach (var child in graph.Graph.Nodes)
                    {
                        byte[] childBytes = Read(child.Address, 0xB4);
                        Require(child.Scene == scene && Word(childBytes, 0x3C) == scene &&
                            (Word(childBytes, 0xB0) & Enabled) == 0 && BitConverter.ToUInt16(childBytes, 8) > 0,
                            "Original inactive Stinger subtree was not recursively disabled in its native scene.");
                        tree.Add(new(child.Address, Word(childBytes, 0), Word(childBytes, 0x2C), BitConverter.ToUInt16(childBytes, 8)));
                    }
                    _pool.Add(new(_pool.Count, rig, manager, move, slot, projectile, node, references, U(projectile + 0xF4),
                        Slice(model, 0x20, 12), Slice(model, 0x30, 12), Slice(model, 0x40, 36), false, tree.AsReadOnly()));
                }
            }
            Require(_pool.Count is > 0 and <= MaximumSlots, "Projectile borrow pool exceeds its six original Nodes.");
            _epoch = epoch;
            foreach (var binding in _pool) VerifyBinding(binding);
            InstallActivationGuard();
            _log(new { @event = "world-projectile-mirror-epoch-begin", epoch, scene = Hex(scene), parent = Hex(_parent),
                originalMosquitos = stockMosquitos.Count, poolSlots = _pool.Count, nativeWrites = 0,
                commonOriginalSceneRootVerified = true, originalRecursiveDisabledSubtreesVerified = true,
                nativeObjectsCreatedOrRetained = 0, guestGameplayStillSuppressed = true });
        }
        catch (Exception error) { Fail(error); throw; }
    }

    internal void BeginApplyAtWorld(uint tid, ref Win32Native.X86Context context, uint epoch, ulong revision,
        IReadOnlyList<WorldPresentationActorState> states)
    {
        EnsureWorld(tid, context);
        Require(_epoch == epoch && epoch != 0 && revision != 0 && states.Count <= _pool.Count,
            "Projectile apply lacks its captured epoch, revision or inactive model capacity.");
        var copy = states.Select(Copy).ToArray();
        foreach (var state in copy)
        {
            state.Validate();
            Require(state.TypeId == StingerType && state.StableId > 0x10000 && state.StableId <= 0x10000 + MaximumActivations &&
                (state.Flags & WorldPresentationActorState.Present) != 0 &&
                (state.Flags & ~(WorldPresentationActorState.Present | WorldPresentationActorState.Visible | WorldPresentationActorState.HasVelocity)) == 0 &&
                state.Health == -1 && state.MaximumHealth == -1 && state.Animation == 5,
                "Projectile presentation requires its opaque-generation Stinger/no-health/raw-PRS cohort.");
        }
        Require(copy.Select(value => value.StableId).Distinct().Count() == copy.Length, "Projectile snapshot repeats an activation ID.");
        if (_pending is not null)
        { Require(_pendingRevision == revision && EqualStates(copy, _pending), "Unfinished projectile delivery was replaced."); return; }
        if (_appliedRevision == revision)
        { Require(_lastApplied is not null && EqualStates(copy, _lastApplied), "Repeated projectile revision changed."); ++_idempotentApplies; return; }
        Require(revision > _appliedRevision && !_callPending && _visibility.Count == 0, "Projectile revision regressed or visibility remains pending.");
        VerifySceneParent(); foreach (var binding in _pool) VerifyBinding(binding);
        Dictionary<uint, Binding> next = [];
        foreach (var state in copy)
        {
            Require(!_retiredIds.Contains(state.StableId), "A retired projectile activation reappeared.");
            if (!_seenIds.Contains(state.StableId))
            { Require(_seenIds.Count < MaximumActivations, "Projectile activation budget exceeded."); _seenIds.Add(state.StableId); }
            if (_active.TryGetValue(state.StableId, out var binding)) next.Add(state.StableId, binding);
        }
        foreach (var state in copy.Where(value => !next.ContainsKey(value.StableId)))
        {
            var binding = _pool.FirstOrDefault(value => !next.Values.Contains(value))
                ?? throw new InvalidDataException("No inactive presentation slot remains for a Stinger activation.");
            next.Add(state.StableId, binding);
        }
        List<QuietRange> ranges = [];
        foreach (var state in copy)
        {
            var binding = next[state.StableId]; _touched.Add(binding.Index);
            Stage(ranges, binding, 0x20, Bytes(state.Position), "position");
            Stage(ranges, binding, 0x30, Bytes(state.Scale), "scale");
            Stage(ranges, binding, 0x40, Bytes(state.Rotation), "raw-projectile-rotation");
            Stage(ranges, binding, 0xB0, BitConverter.GetBytes(U(binding.Node + 0xB0) | 1u), "dirty");
        }
        ApplyQuietTransaction(ranges, restore: false);
        var desired = next.ToDictionary(pair => pair.Value.Index, pair => copy.Single(state => state.StableId == pair.Key));
        foreach (var binding in _pool)
        {
            bool present = desired.TryGetValue(binding.Index, out var state);
            bool visible = present && (state!.Flags & WorldPresentationActorState.Visible) != 0;
            if (binding.Visible != visible) _visibility.Enqueue(new(binding, visible, !present));
        }
        _expected.Clear();
        foreach (var (index, state) in desired) _expected.Add(index, (state, _host.DebugStopSequence));
        _pending = copy; _pendingBindings = next; _pendingRevision = revision;
        if (_visibility.Count == 0) CompleteApply();
    }

    internal bool StepAtWorld(uint tid, ref Win32Native.X86Context context, NativeCallDispatcher calls, Action resume)
    {
        EnsureWorld(tid, context); Require(!calls.Pending && !_callPending, "Projectile visibility cannot nest a native call.");
        if (_pending is null) return false;
        if (!_visibility.TryPeek(out var operation)) { CompleteApply(); return false; }
        VerifyBinding(operation.Binding); PinEnable(); _callPending = true;
        try
        {
            calls.Begin(_host, tid, ref context, SetNodeEnabled, operation.Binding.Node, [operation.Visible ? 1u : 0u, 1], false, _ =>
            {
                try
                {
                    EnsureStopped(tid); VerifyBinding(operation.Binding, verifyVisible: false);
                    VerifyTree(operation.Binding, operation.Visible);
                    Require(_visibility.TryDequeue(out var completed) && ReferenceEquals(completed, operation),
                        "Projectile native visibility returned to another pending operation.");
                    operation.Binding.Visible = operation.Visible; ++_visibilityCalls;
                    if (operation.Absence) ++_hideOnAbsence;
                    _callPending = false;
                    _log(new { @event = "world-projectile-mirror-native-visibility-complete", revision = _pendingRevision,
                        slot = operation.Binding.Index, visible = operation.Visible, absence = operation.Absence,
                        originalRecursiveEnable = true, guestActive30 = 0, noActivationOrDamageCall = true });
                    if (_visibility.Count == 0) CompleteApply(); resume();
                }
                catch (Exception error) { Fail(error); throw; }
            });
            return true;
        }
        catch (Exception error) { Fail(error); throw; }
    }

    internal WorldProjectileRetirementPlan PrepareRetirementAtStop(uint tid, ref Win32Native.X86Context context, uint entry)
    {
        EnsureStopped(tid);
        Require(_epoch != 0 && !_retired && !_retiring && !_failed && !_callPending && _quarantine() &&
            context.Eip == entry && entry is NativeCallDispatcher.WorldReturn or 0x5F2300,
            "Projectile retirement requires its owned idle World/Battle Exit while quarantine is still active.");
        VerifySceneParent(); PinEnable();
        if (_pending is not null)
        {
            ++_cancelledApplies;
            _log(new { @event = "world-projectile-mirror-unacknowledged-apply-cancelled-for-unload", revision = _pendingRevision,
                nativeAppliedAckSent = false, unissuedVisibilityCalls = _visibility.Count });
            _pending = null; _pendingBindings = null; _pendingRevision = 0; _visibility.Clear();
        }
        Binding[] touched = _pool.Where(value => _touched.Contains(value.Index)).ToArray();
        List<QuietRange> ranges = [];
        foreach (var binding in touched)
        {
            VerifyBinding(binding);
            Stage(ranges, binding, 0x20, binding.PositionBefore, "position");
            Stage(ranges, binding, 0x30, binding.ScaleBefore, "scale");
            Stage(ranges, binding, 0x40, binding.RotationBefore, "rotation");
            Stage(ranges, binding, 0xB0, BitConverter.GetBytes(U(binding.Node + 0xB0) | 1u), "dirty");
        }
        ApplyQuietTransaction(ranges, restore: true);
        NativeCallSpec[] calls = touched.Where(value => value.Visible != value.OriginalVisible)
            .Select(value => new NativeCallSpec(SetNodeEnabled, value.Node, [value.OriginalVisible ? 1u : 0u, 1], false)).ToArray();
        Require(calls.Length <= MaximumSlots, "Projectile cleanup exceeds its six borrowed Nodes."); _retiring = true;
        _log(new { @event = "world-projectile-mirror-retirement-prepared", touchedSlots = touched.Length, calls = calls.Length,
            baselinePrsRestoredBeforeOriginalExit = true, noNativeRetainReleaseDelete = true });
        return new(Array.AsReadOnly(calls), () =>
        {
            EnsureStopped(tid);
            Require(_retiring && !_retired && !_failed && !_callPending, "Projectile retirement completion lost its staged native pre-Exit interval.");
            // Gate restore has occurred, but the dispatcher has not resumed the
            // saved original Exit. These are still-live pool readbacks only.
            foreach (var binding in touched)
            {
                VerifyBinding(binding, verifyVisible: false); VerifyTree(binding, binding.OriginalVisible);
                Require(Read(binding.Node + 0x20, 12).SequenceEqual(binding.PositionBefore) &&
                    Read(binding.Node + 0x30, 12).SequenceEqual(binding.ScaleBefore) &&
                    Read(binding.Node + 0x40, 36).SequenceEqual(binding.RotationBefore),
                    "Projectile baseline restoration lacks its still-live native readback before original Exit.");
                binding.Visible = binding.OriginalVisible;
            }
            _retiring = false; _retired = true; _pool.Clear(); _active.Clear(); _expected.Clear();
            _log(new { @event = "world-projectile-mirror-retired-before-stock-unload", touchedSlots = touched.Length,
                borrowedReferenceCountsUnchanged = true, nativeObjectsDeletedOrReleased = 0,
                oldPoolNodesWillNotBeFollowed = true, originalExitNotYetResumed = true });
        });
    }

    internal void ObserveFollowingWorldCaches(uint tid, ref Win32Native.X86Context context)
    {
        EnsureWorld(tid, context);
        if (_lastCacheStop == _host.DebugStopSequence || _epoch == 0) return;
        _lastCacheStop = _host.DebugStopSequence;
        if (Read(U(0x755298) + 0x40, 1)[0] != 0) return;
        foreach (var (slot, expected) in _expected)
        {
            var binding = _pool[slot]; if (!binding.Visible || expected.Stop >= _host.DebugStopSequence) continue;
            VerifyBinding(binding); VerifySceneParent();
            byte[] model = Read(binding.Node, 0xB4);
            float[] actual = Floats(model, 0x74, 3);
            Require(Prs(model).All(float.IsFinite), "Projectile local/cache PRS contains a nonfinite native value.");
            bool matches = actual.Zip(expected.State.Position).All(pair => MathF.Abs(pair.First - pair.Second) < .02f);
            ++_cacheSamples; if (matches) ++_cacheMatches; else ++_cacheMismatches;
            if (_cacheSamples <= 4 || _cacheSamples % 120 == 0)
                _log(new { @event = "world-projectile-mirror-native-cache", expected.State.StableId, slot,
                    position = actual, expectedLocalPosition = expected.State.Position, matches,
                    originalFollowingWorld = true, identityParentVerified = true, cacheWrites = 0 });
        }
    }

    private void CompleteApply()
    {
        Require(_pending is not null && _pendingBindings is not null && !_callPending && _visibility.Count == 0,
            "Projectile ACK preceded its complete native PRS/visibility application.");
        foreach (var state in _pending!)
        {
            var binding = _pendingBindings![state.StableId]; VerifyBinding(binding);
            Require(Read(binding.Node + 0x20, 12).SequenceEqual(Bytes(state.Position)) &&
                Read(binding.Node + 0x30, 12).SequenceEqual(Bytes(state.Scale)) &&
                Read(binding.Node + 0x40, 36).SequenceEqual(Bytes(state.Rotation)) &&
                binding.Visible == ((state.Flags & WorldPresentationActorState.Visible) != 0),
                "Projectile native PRS/visibility readback differs from its immutable server record.");
        }
        foreach (uint id in _active.Keys.Where(id => !_pendingBindings!.ContainsKey(id))) _retiredIds.Add(id);
        foreach (var binding in _pool.Where(value => !_pendingBindings!.Values.Contains(value)))
        { VerifyBinding(binding); Require(!binding.Visible, "Absent projectile retained an enabled borrowed model."); }
        _active.Clear(); foreach (var pair in _pendingBindings!) _active.Add(pair.Key, pair.Value);
        _lastApplied = _pending; _pending = null; _pendingBindings = null;
        _appliedRevision = _pendingRevision; _pendingRevision = 0; ++_applications;
        if (_applications <= 4 || _applications % 120 == 0)
            _log(new { @event = "world-projectile-mirror-snapshot-complete", revision = _appliedRevision,
                activeProjectiles = _active.Count, nativeVisualCallsComplete = true, guestActive30Written = false,
                rawProjectileRotationApplied = true, characterVisualHalfTurn = false });
    }

    private void InstallActivationGuard()
    {
        Require(!_host.Breakpoints.Contains(ActivationEntry), "Guest Stinger activation guard already has another owner.");
        BreakpointCallback repeat = null!;
        repeat = (uint tid, uint at, ref Win32Native.X86Context context) =>
        {
            EnsureStopped(tid); if (_retired || _retiring || _failed) return;
            Require(at == ActivationEntry && context.Eip == at, "Guest Stinger activation stop changed identity.");
            uint candidate = context.Ecx;
            if (_pool.Any(value => value.Projectile == candidate))
            {
                ++_activationViolations;
                _log(new { @event = "world-projectile-mirror-native-activation-rejected", projectile = Hex(context.Ecx),
                    guestGameplaySuppressed = _quarantine(), nativeCallAllowed = false });
                var error = new InvalidDataException("Borrowed Guest Stinger received a native activation under presentation quarantine.");
                Fail(error); throw error;
            }
            _host.Breakpoints.AddReturn(at, tid, "WorldProjectileMirror.activation-guard", repeat);
        };
        _host.Breakpoints.AddReturn(ActivationEntry, _thread, "WorldProjectileMirror.activation-guard", repeat);
    }

    private byte[] VerifyOwner(WorldPresentationNativeRig rig, uint manager, uint move)
    {
        // Bounded same-stop blocks reduce remote calls without caching owners
        // across native execution or weakening any graph relationship.
        byte[] instance = Read(rig.Instance, 0x28), root = Read(rig.Root, 0x40), character = Read(rig.Character, 0x144),
            owner = Read(manager, 0x1DC), controller = Read(move, 0x164);
        // Mosquito Init50FFA0 uses Instance24, emitter1DC and target1E0.
        // Cached Character124/Move130 belong to the separately qualified Bloom
        // manager layout and are not Mosquito owner links.
        bool valid = Word(instance, 0) == 0x710F80 && Word(instance, 0x24) == rig.Root && Word(root, 0) == VisualGraphReader.NodeVtable &&
            Word(root, 0x3C) == _scene && Word(character, 0) == 0x6F6438 && Word(character, 0x24) == rig.Instance &&
            Word(character, 0x140) == manager && Word(character, 0x12C) == move && Word(owner, 0) == ManagerTable &&
            Word(owner, 0x24) == rig.Instance && Word(controller, 0) == 0x6F64F8 &&
            Word(controller, 0x24) == rig.Instance && Word(controller, 0x160) == rig.Character;
        if (!valid && !_ownerProofFailureLogged)
        {
            _ownerProofFailureLogged = true;
            _log(new { @event = "world-projectile-mirror-native-owner-proof-rejected", expectedScene = Hex(_scene),
                expected = new { instance = Hex(rig.Instance), root = Hex(rig.Root), character = Hex(rig.Character),
                    manager = Hex(manager), move = Hex(move) },
                instance = new { table = Hex(Word(instance, 0)), root24 = Hex(Word(instance, 0x24)) },
                root = new { table = Hex(Word(root, 0)), parent2C = Hex(Word(root, 0x2C)), scene3C = Hex(Word(root, 0x3C)) },
                character = new { table = Hex(Word(character, 0)), root18 = Hex(Word(character, 0x18)),
                    instance24 = Hex(Word(character, 0x24)), move12C = Hex(Word(character, 0x12C)),
                    ai134Unqualified = Hex(Word(character, 0x134)), manager140 = Hex(Word(character, 0x140)) },
                manager = new { table = Hex(Word(owner, 0)), instance24 = Hex(Word(owner, 0x24)),
                    unqualifiedWord124 = Hex(Word(owner, 0x124)), unqualifiedWord130 = Hex(Word(owner, 0x130)),
                    nativePool1D0 = Enumerable.Range(0, 3).Select(index => Hex(Word(owner, 0x1D0 + index * 4))).ToArray() },
                move = new { table = Hex(Word(controller, 0)), root18 = Hex(Word(controller, 0x18)),
                    instance24 = Hex(Word(controller, 0x24)), character160 = Hex(Word(controller, 0x160)) },
                actualBoundedBlocks = new { instance = Convert.ToHexString(instance), root = Convert.ToHexString(root),
                    character = Convert.ToHexString(character), manager = Convert.ToHexString(owner), move = Convert.ToHexString(controller) },
                nativeFieldsWrittenByDiagnostic = 0, nodePrsWriteAdmitted = false });
        }
        Require(valid, "Borrowed projectile's original hidden Mosquito/manager/Move graph changed.");
        return owner;
    }
    private void VerifyBinding(Binding binding, bool verifyVisible = true)
    {
        VerifySceneParent(); byte[] manager = VerifyOwner(binding.Rig, binding.Manager, binding.Move);
        byte[] projectile = Read(binding.Projectile, 0xF8), node = Read(binding.Node, 0xB4);
        Require(Word(manager, 0x1D0 + (int)binding.Slot * 4) == binding.Projectile && Word(projectile, 0) == StingerTable &&
            Word(projectile, 0x14) == binding.Node && Word(projectile, 0x18) == binding.Manager && projectile[0x30] == 0 &&
            Word(projectile, 0xF4) == binding.TargetBefore && Word(node, 0) == VisualGraphReader.NodeVtable &&
            Word(node, 0x2C) == _parent && Word(node, 0x3C) == _scene && BitConverter.ToUInt16(node, 8) == binding.References &&
            (!verifyVisible || ((Word(node, 0xB0) & Enabled) != 0) == binding.Visible),
            "Borrowed Stinger pool/Node/scene/refcount/active30/target/visibility changed outside presentation.");
    }
    private void VerifyTree(Binding binding, bool visible)
    {
        foreach (var witness in binding.Tree)
        {
            byte[] bytes = Read(witness.Address, 0xB4);
            Require(Word(bytes, 0) == witness.Table && Word(bytes, 0x2C) == witness.Parent && Word(bytes, 0x3C) == _scene &&
                BitConverter.ToUInt16(bytes, 8) == witness.References && ((Word(bytes, 0xB0) & Enabled) != 0) == visible,
                "Original recursive Stinger model visibility or borrowed subtree ownership changed.");
        }
    }
    private void VerifySceneParent()
    {
        Require(U(0x755274) == _engine && U(_engine + 0x18) == _scene && U(_scene) == 0x6E7358 &&
            U(_scene + 0x14) == _parent, "Borrowed projectile's original engine/scene root changed.");
        byte[] parent = Read(_parent, 0xB4);
        Require(Word(parent, 0) == VisualGraphReader.NodeVtable && Word(parent, 0x3C) == _scene &&
            Floats(parent, 0x74, 3).All(value => float.IsFinite(value) && MathF.Abs(value) < .00001f) &&
            Floats(parent, 0x80, 3).All(value => float.IsFinite(value) && MathF.Abs(value - 1) < .00001f) &&
            Floats(parent, 0x8C, 9).Select((value, index) => float.IsFinite(value) &&
                MathF.Abs(value - (index % 4 == 0 ? 1 : 0)) < .00001f).All(value => value),
            "Borrowed projectile parent is not the actual original identity SceneRoot.");
    }
    private void VerifyManagerType()
    {
        uint getter = U(ManagerTable + 0x10); byte[] literal = Image(getter, 6);
        Require(literal[0] == 0xB8 && literal[5] == 0xC3 && ReadCode(getter, 6).SequenceEqual(literal) &&
            U(BitConverter.ToUInt32(literal, 1)) == 0x7C0E575A,
            "Borrowed projectile manager has another original RTTI identity.");
    }

    private void Stage(List<QuietRange> ranges, Binding binding, uint offset, byte[] requested, string name)
    { byte[] before = Read(binding.Node + offset, requested.Length); if (!before.SequenceEqual(requested)) ranges.Add(new(binding, binding.Node + offset, before, (byte[])requested.Clone(), name)); }
    private void ApplyQuietTransaction(List<QuietRange> ranges, bool restore)
    {
        List<QuietRange> attempted = [];
        try
        {
            foreach (var range in ranges)
            {
                VerifyBinding(range.Binding);
                Require(Read(range.Address, range.Before.Length).SequenceEqual(range.Before), "Projectile quiet before-image changed.");
                attempted.Add(range); // A failed write may have partially committed.
                Win32Native.WriteMemory(_process, range.Address, range.Requested, executable: false);
                VerifyBinding(range.Binding);
                Require(Read(range.Address, range.Requested.Length).SequenceEqual(range.Requested), "Projectile quiet PRS/dirty readback differs.");
                if (restore) ++_restoredRanges; else ++_quietRanges;
            }
        }
        catch (Exception error)
        {
            object[] actualAfterFailure = AfterImages(attempted); bool rolledBack = false; string? rollbackError = null;
            try
            {
                foreach (var range in attempted.AsEnumerable().Reverse())
                {
                    VerifyBinding(range.Binding); Win32Native.WriteMemory(_process, range.Address, range.Before, executable: false);
                    VerifyBinding(range.Binding);
                    Require(Read(range.Address, range.Before.Length).SequenceEqual(range.Before), "Projectile immediate rollback readback differs.");
                }
                rolledBack = true;
            }
            catch (Exception failure) { rollbackError = failure.ToString(); }
            Fail(error);
            _log(new { @event = "world-projectile-mirror-quiet-write-failed", error = error.ToString(), actualAfterFailure,
                rollbackVerified = rolledBack, rollbackError, afterRollback = AfterImages(attempted), nativeContinuationForbidden = true });
            throw;
        }
    }
    private object[] AfterImages(List<QuietRange> attempted) => attempted.Select(range =>
    {
        string? actual = null, error = null; bool current = false;
        try { VerifyBinding(range.Binding, verifyVisible: false); current = true; actual = Convert.ToHexString(Read(range.Address, range.Before.Length)); }
        catch (Exception failure) { error = failure.Message; }
        return (object)new { range.Name, slot = range.Binding.Index, before = Convert.ToHexString(range.Before),
            requested = Convert.ToHexString(range.Requested), actual, error, ownerCurrent = current };
    }).ToArray();

    private void PinEnable()
    {
        byte[] expected = Image(SetNodeEnabled, 8);
        Require(U(VisualGraphReader.NodeVtable + 0x34) == SetNodeEnabled && ReadCode(SetNodeEnabled, 8).SequenceEqual(expected),
            "Projectile recursive visibility method changed from the original Node API.");
    }
    private void Pin(uint address, string expected)
    { byte[] bytes = Convert.FromHexString(expected); Require(Image(address, bytes.Length).SequenceEqual(bytes) && ReadCode(address, bytes.Length).SequenceEqual(bytes), "Original projectile pin changed at " + Hex(address)); }
    private byte[] ReadCode(uint address, int count)
    {
        byte[] current = Read(address, count), original = Image(address, count);
        for (int i = 0; i < count; i++) if (current[i] == 0xCC && original[i] != 0xCC && _host.Breakpoints.Contains(address + (uint)i)) current[i] = original[i];
        return current;
    }
    private void EnsureWorld(uint tid, in Win32Native.X86Context context)
    { EnsureStopped(tid); Require(!_retired && !_retiring && !_failed && _quarantine() && context.Eip == NativeCallDispatcher.WorldReturn, "Projectile presentation requires the same actual WorldRET and active Guest quarantine."); }
    private void EnsureStopped(uint tid) => Require(_host.IsAtDebugStop && _process != IntPtr.Zero && _pid != 0 && _thread != 0 &&
        _host.ProcessHandle == _process && _host.ProcessPid == _pid && _host.MainThreadId == _thread && tid == _thread &&
        _host.ImageBase == 0x400000 && Environment.CurrentManagedThreadId == _ownerThread, "Projectile mirror lost its stopped original process/main-thread owner.");
    private void Fail(Exception error) { _failed = true; _failure = error.ToString(); }
    private uint U(uint address) => Win32Native.ReadUInt32(_process, address);
    private byte[] Read(uint address, int count) => Win32Native.ReadMemory(_process, address, count);
    private byte[] Image(uint address, int count) => _image.GetSectionData(checked((int)(address - 0x400000))).GetContent(0, count).ToArray();
    private static uint Pointer(uint value) { Require(value >= 0x10000 && (value & 3) == 0, "Projectile inventory has a null/unaligned native pointer."); return value; }
    private static uint Word(byte[] bytes, int offset) => BitConverter.ToUInt32(bytes, offset);
    private static byte[] Slice(byte[] bytes, int offset, int count) => bytes.AsSpan(offset, count).ToArray();
    private static byte[] Bytes(float[] values) => values.SelectMany(BitConverter.GetBytes).ToArray();
    private static float[] Floats(byte[] bytes, int offset, int count) => Enumerable.Range(0, count).Select(index => BitConverter.ToSingle(bytes, offset + index * 4)).ToArray();
    private static IEnumerable<float> Prs(byte[] bytes) => Floats(bytes, 0x20, 3).Concat(Floats(bytes, 0x30, 3)).Concat(Floats(bytes, 0x40, 9))
        .Concat(Floats(bytes, 0x74, 3)).Concat(Floats(bytes, 0x80, 3)).Concat(Floats(bytes, 0x8C, 9));
    private static string[] RawPrs(byte[] bytes) => Prs(bytes).Select(value => BitConverter.SingleToUInt32Bits(value).ToString("X8")).ToArray();
    private static string Hex(uint value) => $"0x{value:X8}";
    private static WorldPresentationActorState Copy(WorldPresentationActorState value) => value with {
        Position = (float[])value.Position.Clone(), Scale = (float[])value.Scale.Clone(), Rotation = (float[])value.Rotation.Clone(), Velocity = (float[])value.Velocity.Clone() };
    private static bool EqualStates(IReadOnlyList<WorldPresentationActorState> left, IReadOnlyList<WorldPresentationActorState> right) =>
        left.Count == right.Count && left.Zip(right).All(pair => pair.First.StableId == pair.Second.StableId &&
            pair.First.TypeId == pair.Second.TypeId && pair.First.StateId == pair.Second.StateId && pair.First.Animation == pair.Second.Animation &&
            pair.First.Flags == pair.Second.Flags && pair.First.Health == pair.Second.Health && pair.First.MaximumHealth == pair.Second.MaximumHealth &&
            Bytes(pair.First.Position).SequenceEqual(Bytes(pair.Second.Position)) && Bytes(pair.First.Scale).SequenceEqual(Bytes(pair.Second.Scale)) &&
            Bytes(pair.First.Rotation).SequenceEqual(Bytes(pair.Second.Rotation)) && Bytes(pair.First.Velocity).SequenceEqual(Bytes(pair.Second.Velocity)));
    private static void Require(bool value, string message) { if (!value) throw new InvalidDataException(message); }
}
