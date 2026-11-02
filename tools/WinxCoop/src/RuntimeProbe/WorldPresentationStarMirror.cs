using System.Reflection.PortableExecutable;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

// The game owns these pooled Nodes. The caller combines the visual-only calls
// with its existing pre-unload batch and invokes Complete before stock unload.
internal sealed record WorldStarRetirementPlan(IReadOnlyList<NativeCallSpec> Calls, Action Complete);

/// <summary>
/// Our Guest-only borrowed Fairy Star presentation. No Star activation, tick,
/// award, ownership retain/release or native gameplay-state write is performed.
/// </summary>
internal sealed class WorldPresentationStarMirror
{
    private const uint ManagerSlot = 0x765B00, ManagerTable = 0x6F61CC, StarTable = 0x702160;
    private const uint SetNodeEnabled = 0x421640, Enabled = 0x200;
    private const int MaximumObservedActivations = 4096;
    private readonly DebuggerHost _host;
    private readonly PEReader _image;
    private readonly Func<bool> _nativeGameplaySuppressed;
    private readonly Action<object> _log;
    private readonly int _ownerThread = Environment.CurrentManagedThreadId;
    private readonly uint _pid, _thread;
    private readonly IntPtr _process;
    private readonly Dictionary<SlotKey, PoolSlot> _pool = [];
    private readonly Dictionary<SlotKey, Binding> _bindings = [];
    private readonly Dictionary<ulong, SlotKey> _observedIds = [];
    private readonly HashSet<ulong> _retiredIds = [];
    private readonly Dictionary<ulong, SlotKey> _activeIds = [];
    private readonly Queue<VisibilityCall> _visibility = [];
    private readonly Dictionary<SlotKey, (WorldStarState State, uint Stop)> _expectedCaches = [];
    private WorldPresentationStarPoolReader? _reader;
    private WorldStarState[]? _pending, _lastApplied;
    private uint _epoch, _scene, _parent, _manager, _applications, _nativeVisibilityCalls, _quietRanges;
    private uint _hideOnAbsence, _restoredBaselineRanges, _idempotentApplications;
    private uint _cacheSamples, _cacheMatches, _cacheMismatches, _lastCacheStop, _cancelledPendingApplies;
    private ulong _pendingRevision, _appliedRevision;
    private bool _retiring, _retired, _failed, _callPending;
    private string _failure = "";

    private readonly record struct SlotKey(uint Category, uint Slot);
    private sealed record PoolSlot(uint Star, uint Node);
    private sealed class Binding(SlotKey key, uint star, uint node, ushort references, uint originalState,
        byte[] position, byte[] scale, byte[] rotation, bool originalVisible)
    {
        internal SlotKey Key { get; } = key;
        internal uint Star { get; } = star;
        internal uint Node { get; } = node;
        internal ushort References { get; } = references;
        internal uint OriginalState { get; } = originalState;
        internal byte[] OriginalPosition { get; } = position;
        internal byte[] OriginalScale { get; } = scale;
        internal byte[] OriginalRotation { get; } = rotation;
        internal bool OriginalVisible { get; } = originalVisible;
        internal bool Visible { get; set; } = originalVisible;
    }
    private sealed record VisibilityCall(Binding Binding, bool Visible, bool Absence);
    private sealed record QuietRange(Binding Binding, uint Address, byte[] Before, byte[] Requested, string Name);

    internal WorldPresentationStarMirror(DebuggerHost host, PEReader image,
        Func<bool> nativeGameplaySuppressed, Action<object> log)
    {
        _host = host ?? throw new ArgumentNullException(nameof(host));
        _image = image ?? throw new ArgumentNullException(nameof(image));
        _nativeGameplaySuppressed = nativeGameplaySuppressed ?? throw new ArgumentNullException(nameof(nativeGameplaySuppressed));
        _log = log ?? throw new ArgumentNullException(nameof(log));
        _pid = host.ProcessPid; _thread = host.MainThreadId; _process = host.ProcessHandle;
        Require(host.IsAtDebugStop && _process != IntPtr.Zero && _pid != 0 && _thread != 0 && host.ImageBase == 0x400000,
            "Star mirror requires the stopped original process.");
        PinEnable();
    }

    internal bool Pending => _pending is not null;
    internal bool ApplyComplete => _pending is null && !_callPending && !_failed && !_retiring && !_retired;
    internal ulong AppliedRevision => _appliedRevision;
    internal ulong PendingRevision => _pendingRevision;
    internal object Summary => new { epoch = _epoch, scene = _scene, parent = _parent,
        retired = _retired, retiring = _retiring, failed = _failed, failure = _failure,
        appliedRevision = _appliedRevision, pendingRevision = _pendingRevision, pending = Pending,
        pendingNativeVisibility = _visibility.Count, nativeCallPending = _callPending,
        touchedPoolSlots = _bindings.Count, activePresentationIds = _activeIds.Count,
        observedActivationIds = _observedIds.Count, retiredActivationIds = _retiredIds.Count,
        applications = _applications, nativeVisibilityCalls = _nativeVisibilityCalls,
        quietRanges = _quietRanges, hideOnAbsence = _hideOnAbsence,
        restoredBaselineRanges = _restoredBaselineRanges, idempotentApplications = _idempotentApplications,
        cancelledUnacknowledgedApplies = _cancelledPendingApplies,
        nativeFollowingWorldCacheSamples = _cacheSamples, nativeIdentityParentCacheMatches = _cacheMatches,
        nativeIdentityParentCacheMismatches = _cacheMismatches,
        nativeStarLogicWrites = 0, nativeAwardCalls = 0, nativeObjectsOwned = 0,
        nativeRetainReleaseCalls = 0, recursiveVisibilityUsesOriginalNodeFunction = true,
        hostStateC8IsPresentationMetadata = true, guestStateC8Written = false,
        renderAcceptanceClaimed = false, runtimeQualificationClaimed = false };

    internal void BeginEpochAtWorld(uint tid, ref Win32Native.X86Context context,
        uint epoch, uint scene, uint parent)
    {
        EnsureWorld(tid, context);
        Require(_epoch == 0 && epoch != 0 && U(scene) == 0x6E7358,
            "Star mirror epoch must begin once in an admitted native scene.");
        _scene = scene; _parent = parent; VerifyParent();
        var reader = new WorldPresentationStarPoolReader(_host, _image);
        var inventory = reader.InventorySlotsAtWorld(tid, ref context, scene, inspectInactiveNodes: true);
        Require(inventory.RepeatedStarAddresses == 0 && inventory.RepeatedNodeAddresses == 0,
            "Borrowed Star presentation rejects aliased native pools.");
        foreach (var entry in inventory.Entries.Where(value => value.Star != 0))
        {
            Require(_pool.TryAdd(new(entry.Category, entry.Slot), new(entry.Star, entry.NodeCC)),
                "Borrowed Star inventory contains a repeated category/slot.");
        }
        _manager = inventory.Manager; _reader = reader; _epoch = epoch;
        _log(new { @event = "world-star-mirror-epoch-begin", epoch, scene, parent,
            poolSlots = _pool.Count, borrowedSceneNodes = inventory.Entries.Count(value => value.Node?.InExpectedScene == true),
            nativeObjectsRetainedOrCreated = 0, nativeStarLogicWrites = 0 });
    }

    internal void BeginApplyAtWorld(uint tid, ref Win32Native.X86Context context, uint epoch,
        ulong revision, IReadOnlyList<WorldStarState> states)
    {
        EnsureWorld(tid, context);
        Require(_epoch == epoch && _epoch != 0 && revision != 0 && states.Count <= WorldStarContract.MaximumStars,
            "Star presentation snapshot is outside its captured epoch/revision/budget.");
        WorldStarState[] copy = states.Select(Copy).ToArray();
        foreach (var state in copy) state.Validate();
        Require(copy.Select(value => value.ActivationId).Distinct().Count() == copy.Length &&
            copy.Select(value => new SlotKey(value.Category, value.Slot)).Distinct().Count() == copy.Length,
            "Star presentation snapshot duplicates an activation or source pool slot.");
        if (_pending is not null)
        {
            Require(revision == _pendingRevision && EqualStates(copy, _pending),
                "A different Star snapshot replaced an unfinished native apply.");
            return;
        }
        if (revision == _appliedRevision)
        {
            Require(_lastApplied is not null && EqualStates(copy, _lastApplied),
                "A repeated Star revision changed its immutable state.");
            ++_idempotentApplications; return;
        }
        Require(revision > _appliedRevision && !_callPending && _visibility.Count == 0,
            "Star presentation revision regressed or a native call is still pending.");
        VerifyParent();
        foreach (var binding in _bindings.Values) VerifyBinding(binding);
        var desired = new Dictionary<SlotKey, WorldStarState>();
        foreach (var state in copy)
        {
            var key = new SlotKey(state.Category, state.Slot);
            Require(!_retiredIds.Contains(state.ActivationId), "A retired Star activation reappeared in a later snapshot.");
            if (_observedIds.TryGetValue(state.ActivationId, out var originalKey))
                Require(originalKey == key, "A Star activation ID changed its source pool identity.");
            else
            {
                Require(_observedIds.Count < MaximumObservedActivations,
                    "Star activation observation budget exceeded.");
                _observedIds.Add(state.ActivationId, key);
            }
            if (!_bindings.ContainsKey(key)) _bindings.Add(key, Borrow(key));
            desired.Add(key, state);
        }
        List<QuietRange> ranges = [];
        foreach (var (key, state) in desired)
        {
            var binding = _bindings[key]; VerifyBinding(binding);
            Stage(ranges, binding, 0x20, Bytes(state.Position), "position");
            Stage(ranges, binding, 0x30, Bytes(state.Scale), "scale");
            Stage(ranges, binding, 0x40, Bytes(state.Rotation), "rotation");
            Stage(ranges, binding, 0xB0, BitConverter.GetBytes(U(binding.Node + 0xB0) | 1u), "dirty");
        }
        ApplyQuietTransaction(ranges, restore: false);
        foreach (var (key, state) in desired) _expectedCaches[key] = (Copy(state), _host.DebugStopSequence);
        foreach (var key in _expectedCaches.Keys.Where(key => !desired.ContainsKey(key)).ToArray()) _expectedCaches.Remove(key);
        foreach (var binding in _bindings.Values.OrderBy(value => value.Key.Category).ThenBy(value => value.Key.Slot))
        {
            bool present = desired.TryGetValue(binding.Key, out var state);
            bool visible = present && (state!.Flags & WorldStarContract.Visible) != 0;
            if (binding.Visible != visible) _visibility.Enqueue(new(binding, visible, !present));
        }
        _pending = copy; _pendingRevision = revision;
        if (_visibility.Count == 0) CompleteApply();
    }

    // true means one call was issued. Root keeps the exact delivery and ACK
    // pending until ApplyComplete and AppliedRevision equal that delivery.
    internal bool StepAtWorld(uint tid, ref Win32Native.X86Context context,
        NativeCallDispatcher calls, Action resume)
    {
        EnsureWorld(tid, context);
        Require(!calls.Pending && !_callPending, "Star visibility cannot nest an outstanding native call.");
        if (_pending is null) return false;
        if (!_visibility.TryPeek(out var operation)) { CompleteApply(); return false; }
        VerifyBinding(operation.Binding); PinEnable();
        _callPending = true;
        try
        {
            calls.Begin(_host, tid, ref context, SetNodeEnabled, operation.Binding.Node,
                [operation.Visible ? 1u : 0u, 1], false, _ =>
                {
                    try
                    {
                        EnsureStopped(tid); VerifyBinding(operation.Binding, verifyVisible: false);
                        Require(((U(operation.Binding.Node + 0xB0) & Enabled) != 0) == operation.Visible,
                            "Original recursive Star visibility call failed its root readback.");
                        Require(_visibility.TryDequeue(out var completed) && ReferenceEquals(completed, operation),
                            "Star visibility native completion changed its exact pending operation.");
                        operation.Binding.Visible = operation.Visible; ++_nativeVisibilityCalls;
                        if (operation.Absence) ++_hideOnAbsence;
                        _callPending = false;
                        _log(new { @event = "world-star-mirror-native-visibility-complete", revision = _pendingRevision,
                            category = operation.Binding.Key.Category, slot = operation.Binding.Key.Slot,
                            visible = operation.Visible, absence = operation.Absence, nativeFunction = SetNodeEnabled,
                            guestStarStillInactive = true, nativeRetainReleaseCalls = 0 });
                        if (_visibility.Count == 0) CompleteApply();
                        resume();
                    }
                    catch (Exception error) { Fail(error); throw; }
                });
            return true;
        }
        catch (Exception error) { Fail(error); throw; }
    }

    internal WorldStarRetirementPlan PrepareRetirementAtStop(uint tid,
        ref Win32Native.X86Context context, uint entry)
    {
        EnsureStopped(tid);
        Require(!_retired && !_retiring && !_failed && !_callPending &&
            _epoch != 0 && _nativeGameplaySuppressed() && context.Eip == entry &&
            entry is NativeCallDispatcher.WorldReturn or 0x5F2300 or 0x5F86C0,
            "Star retirement requires its idle owned world/pre-unload boundary and native quarantine.");
        VerifyParent(); PinEnable();
        if (_pending is not null)
        {
            ++_cancelledPendingApplies;
            _log(new { @event = "world-star-mirror-unacknowledged-apply-cancelled-for-unload",
                revision = _pendingRevision, unissuedVisibilityCalls = _visibility.Count, nativeAppliedAckSent = false });
            _pending = null; _pendingRevision = 0; _visibility.Clear();
        }
        List<QuietRange> ranges = [];
        foreach (var binding in _bindings.Values)
        {
            VerifyBinding(binding);
            Stage(ranges, binding, 0x20, binding.OriginalPosition, "position");
            Stage(ranges, binding, 0x30, binding.OriginalScale, "scale");
            Stage(ranges, binding, 0x40, binding.OriginalRotation, "rotation");
            Stage(ranges, binding, 0xB0, BitConverter.GetBytes(U(binding.Node + 0xB0) | 1u), "dirty");
        }
        ApplyQuietTransaction(ranges, restore: true);
        Binding[] touched = _bindings.Values.OrderBy(value => value.Key.Category).ThenBy(value => value.Key.Slot).ToArray();
        // Initial stock pool Nodes must have been disabled; enabling an existing
        // gameplay Star would never have been an admitted borrowed binding.
        NativeCallSpec[] calls = touched.Where(binding => binding.Visible)
            .Select(binding => new NativeCallSpec(SetNodeEnabled, binding.Node, [0, 1], false)).ToArray();
        Require(calls.Length <= WorldStarContract.MaximumStars,
            "Star retirement visual calls exceed the borrowed-slot budget.");
        _retiring = true; _reader!.RetireAtStop(tid, ref context);
        _log(new { @event = "world-star-mirror-retirement-prepared", calls = calls.Length,
            touchedSlots = touched.Length, baselinePrsRestoredBeforeStockUnload = true,
            nativeStarLogicWrites = 0, nativeDeleteOrReleaseCalls = 0 });
        return new(Array.AsReadOnly(calls), () =>
        {
            EnsureStopped(tid);
            Require(_retiring && !_retired && !_failed && !_callPending,
                "Star retirement completion was repeated or escaped its staged visual batch.");
            foreach (var binding in touched)
            {
                VerifyBinding(binding, verifyVisible: false);
                Require((U(binding.Node + 0xB0) & Enabled) == 0 &&
                    Read(binding.Node + 0x20, 12).SequenceEqual(binding.OriginalPosition) &&
                    Read(binding.Node + 0x30, 12).SequenceEqual(binding.OriginalScale) &&
                    Read(binding.Node + 0x40, 36).SequenceEqual(binding.OriginalRotation),
                    "Borrowed Star retirement did not hide/restore its original pool Node.");
                binding.Visible = false;
            }
            _retiring = false; _retired = true; _bindings.Clear(); _pool.Clear(); _activeIds.Clear(); _expectedCaches.Clear();
            _log(new { @event = "world-star-mirror-retired-before-stock-unload", touchedSlots = touched.Length,
                nativeObjectsDeletedOrReleased = 0, borrowedReferenceCountsUnchanged = true,
                oldPoolNodesWillNotBeFollowed = true });
        });
    }

    internal void ObserveFollowingWorldCaches(uint tid, ref Win32Native.X86Context context)
    {
        EnsureWorld(tid, context);
        if (_lastCacheStop == _host.DebugStopSequence) return;
        _lastCacheStop = _host.DebugStopSequence;
        VerifyParent();
        bool paused = Read(U(0x755298) + 0x40, 1)[0] != 0;
        foreach (var (key, expected) in _expectedCaches)
        {
            if (paused || expected.Stop >= _host.DebugStopSequence || !_bindings[key].Visible) continue;
            var binding = _bindings[key]; VerifyBinding(binding);
            float[] position = Floats(binding.Node + 0x74, 3), scale = Floats(binding.Node + 0x80, 3),
                rotation = Floats(binding.Node + 0x8C, 9);
            Require(position.Concat(scale).Concat(rotation).All(float.IsFinite), "Native Star world cache contains a nonfinite PRS value.");
            bool matches = Close(position, expected.State.Position, .02f) && Close(scale, expected.State.Scale, .0001f) &&
                Close(rotation, expected.State.Rotation, .0001f);
            ++_cacheSamples; if (matches) ++_cacheMatches; else ++_cacheMismatches;
            if (_cacheSamples <= 4 || _cacheSamples % 120 == 0)
                _log(new { @event = "world-star-mirror-native-cache", expected.State.ActivationId,
                    category = key.Category, slot = key.Slot, position, scale, rotation,
                    expectedLocalPosition = expected.State.Position, matches, identityParentComparison = true,
                    originalFollowingWorld = true, cacheWrites = 0, nativeCacheSamples = _cacheSamples });
        }
        static bool Close(float[] actual, float[] expected, float tolerance) =>
            actual.Zip(expected).All(pair => Math.Abs(pair.First - pair.Second) < tolerance);
    }

    private Binding Borrow(SlotKey key)
    {
        VerifyOwners();
        Require(_bindings.Count < WorldStarContract.MaximumStars,
            "Star snapshot exceeds the finite touched-slot budget for this epoch.");
        Require(_pool.TryGetValue(key, out var slot),
            "Star snapshot references an absent pool slot or exceeds the touched-slot budget.");
        PoolSlot selected = slot ?? throw new InvalidDataException("Star pool binding was not captured.");
        uint star = selected.Star, node = selected.Node;
        VerifySlot(key, star, node);
        Require((U(node + 0xB0) & Enabled) == 0,
            "A borrowed Star pool Node was already enabled before adapter presentation.");
        ushort references = Count(node); Require(references != 0, "Borrowed Star Node has no native owner.");
        var binding = new Binding(key, star, node, references, U(star + 0xC8),
            Read(node + 0x20, 12), Read(node + 0x30, 12), Read(node + 0x40, 36), false);
        VerifyBinding(binding);
        _log(new { @event = "world-star-mirror-slot-borrowed", category = key.Category, slot = key.Slot,
            nativeReferenceCount = references, guestInactive = true, commonParent = true,
            nativeRetainOrOwnershipTransfer = false, nativeObjectsCreated = 0 });
        return binding;
    }

    private void CompleteApply()
    {
        Require(_pending is not null && !_callPending && _visibility.Count == 0,
            "Star apply completion preceded its native visibility calls.");
        VerifyParent();
        var next = new Dictionary<ulong, SlotKey>();
        foreach (var state in _pending!)
        {
            var key = new SlotKey(state.Category, state.Slot); var binding = _bindings[key]; VerifyBinding(binding);
            Require(Read(binding.Node + 0x20, 12).SequenceEqual(Bytes(state.Position)) &&
                Read(binding.Node + 0x30, 12).SequenceEqual(Bytes(state.Scale)) &&
                Read(binding.Node + 0x40, 36).SequenceEqual(Bytes(state.Rotation)) &&
                binding.Visible == ((state.Flags & WorldStarContract.Visible) != 0),
                "Star native presentation PRS/visibility readback differs from its exact pending state.");
            next.Add(state.ActivationId, key);
        }
        var presentSlots = next.Values.ToHashSet();
        foreach (var binding in _bindings.Values.Where(binding => !presentSlots.Contains(binding.Key)))
        {
            VerifyBinding(binding);
            Require(!binding.Visible, "An absent Star retained its borrowed presentation after native completion.");
        }
        foreach (ulong id in _activeIds.Keys.Where(value => !next.ContainsKey(value))) _retiredIds.Add(id);
        _activeIds.Clear(); foreach (var (id, key) in next) _activeIds.Add(id, key);
        _lastApplied = _pending; _pending = null; _appliedRevision = _pendingRevision; _pendingRevision = 0;
        ++_applications;
        if (_applications <= 4 || _applications % 120 == 0)
            _log(new { @event = "world-star-mirror-snapshot-complete", revision = _appliedRevision,
                activeStars = _activeIds.Count, borrowedSlots = _bindings.Count,
                nativeVisualCallsComplete = true, nativeStarLogicWrites = 0,
                nativeRewardOrPickupCalls = 0, followingWorldCacheUpdateStillRequired = true });
    }

    private void Stage(List<QuietRange> ranges, Binding binding, uint offset, byte[] value, string name)
    {
        byte[] before = Read(binding.Node + offset, value.Length);
        if (!before.SequenceEqual(value)) ranges.Add(new(binding, binding.Node + offset, before, (byte[])value.Clone(), name));
    }
    private void ApplyQuietTransaction(List<QuietRange> plans, bool restore)
    {
        List<QuietRange> attempted = [];
        try
        {
            foreach (var plan in plans)
            {
                VerifyBinding(plan.Binding);
                Require(Read(plan.Address, plan.Before.Length).SequenceEqual(plan.Before),
                    "Star quiet before-image changed before a stopped write.");
                attempted.Add(plan);
                Win32Native.WriteMemory(_host.ProcessHandle, plan.Address, plan.Requested, executable: false);
                VerifyBinding(plan.Binding);
                Require(Read(plan.Address, plan.Requested.Length).SequenceEqual(plan.Requested),
                    "Star quiet PRS/dirty range write/readback differs.");
                if (restore) ++_restoredBaselineRanges; else ++_quietRanges;
            }
        }
        catch (Exception error)
        {
            var afterFailure = AfterImages(attempted); bool rolledBack = false; string? rollbackError = null;
            try
            {
                foreach (var plan in attempted.AsEnumerable().Reverse())
                {
                    VerifyBinding(plan.Binding);
                    Win32Native.WriteMemory(_host.ProcessHandle, plan.Address, plan.Before, executable: false);
                    VerifyBinding(plan.Binding);
                    Require(Read(plan.Address, plan.Before.Length).SequenceEqual(plan.Before),
                        "Star immediate transaction rollback readback differs.");
                }
                rolledBack = true;
            }
            catch (Exception rollbackFailure) { rollbackError = rollbackFailure.ToString(); }
            Fail(error);
            _log(new { @event = "world-star-mirror-quiet-write-failed", error = error.ToString(), afterFailure,
                rollbackVerified = rolledBack, rollbackError, afterRollback = AfterImages(attempted),
                nativeContinuationForbidden = true, oldSceneBaselineRestored = false });
            throw;
        }
    }
    private object[] AfterImages(List<QuietRange> plans)
    {
        List<object> result = [];
        foreach (var plan in plans)
        {
            string? actual = null, readError = null; bool current;
            try { VerifyBinding(plan.Binding); current = true; }
            catch { current = false; }
            if (current)
                try { actual = Convert.ToHexString(Read(plan.Address, plan.Before.Length)); }
                catch (Exception error) { readError = error.ToString(); }
            result.Add(new { range = plan.Name, category = plan.Binding.Key.Category, slot = plan.Binding.Key.Slot,
                before = Convert.ToHexString(plan.Before), requested = Convert.ToHexString(plan.Requested),
                actual, readError, readSkippedAfterOwnerLoss = !current });
        }
        return result.ToArray();
    }

    private void VerifyBinding(Binding binding, bool verifyVisible = true)
    {
        VerifySlot(binding.Key, binding.Star, binding.Node);
        Require(Count(binding.Node) == binding.References && U(binding.Star + 0xC8) == binding.OriginalState &&
            (!verifyVisible || ((U(binding.Node + 0xB0) & Enabled) != 0) == binding.Visible),
            "Borrowed Star reference count/inactive state/visibility changed outside presentation.");
    }
    private void VerifySlot(SlotKey key, uint star, uint node)
    {
        VerifyOwners();
        uint offset = key.Category switch { 0 => 0x6Cu, 1 => 0x134u, 2 => 0x1FCu, 3 => 0x2C4u,
            _ => throw new InvalidDataException("Unknown Star pool category.") };
        Require(key.Slot < (key.Category == 3 ? 100u : 50u) &&
            (key.Category != 3 || Read(_manager + 0x55, 1)[0] != 0) &&
            star >= 0x10000 && node >= 0x10000 && (star & 3) == 0 && (node & 3) == 0 &&
            U(_manager + offset + key.Slot * 4) == star && U(star) == StarTable && U(star + 0xCC) == node &&
            Read(star + 0x40, 1)[0] == 0 && U(node) == VisualGraphReader.NodeVtable &&
            U(node + 0x3C) == _scene && U(node + 0x2C) == _parent,
            "Borrowed Star pool slot/Node/scene/parent is no longer inactive and admitted.");
    }
    private void VerifyOwners() => Require(_epoch != 0 && U(ManagerSlot) == _manager &&
        U(_manager) == ManagerTable && U(_scene) == 0x6E7358,
        "Star mirror changed its native pool/scene owners.");
    private void VerifyParent()
    {
        Require(_parent >= 0x10000 && (_parent & 3) == 0 && U(_parent) == VisualGraphReader.NodeVtable &&
            U(_parent + 0x3C) == _scene, "Star mirror parent is not its original scene Node.");
        float[] position = Floats(_parent + 0x74, 3), scale = Floats(_parent + 0x80, 3), rotation = Floats(_parent + 0x8C, 9);
        Require(position.All(value => Math.Abs(value) < .00001f) && scale.All(value => Math.Abs(value - 1) < .00001f) &&
            rotation.Select((value, index) => Math.Abs(value - (index % 4 == 0 ? 1 : 0)) < .00001f).All(value => value),
            "Borrowed Star parent does not have the admitted identity world PRS basis.");
    }
    private void PinEnable()
    {
        byte[] expected = _image.GetSectionData(checked((int)(SetNodeEnabled - 0x400000))).GetContent(0, 8).ToArray();
        Require(U(VisualGraphReader.NodeVtable + 0x34) == SetNodeEnabled &&
            BitConverter.ToUInt32(_image.GetSectionData(checked((int)(VisualGraphReader.NodeVtable + 0x34 - 0x400000))).GetContent(0, 4).ToArray()) == SetNodeEnabled &&
            Read(SetNodeEnabled, expected.Length).SequenceEqual(expected),
            "Original recursive Node enable method/table slot changed.");
    }
    private void EnsureWorld(uint tid, in Win32Native.X86Context context)
    {
        EnsureStopped(tid);
        Require(!_retired && !_retiring && !_failed && context.Eip == NativeCallDispatcher.WorldReturn &&
            _nativeGameplaySuppressed(), "Star presentation requires the owned WorldRET and active native quarantine.");
    }
    private void EnsureStopped(uint tid) => Require(_host.IsAtDebugStop && _process != IntPtr.Zero &&
        _host.ProcessHandle == _process && _host.ProcessPid == _pid && _host.MainThreadId == _thread &&
        _host.ImageBase == 0x400000 && tid == _thread && Environment.CurrentManagedThreadId == _ownerThread,
        "Star mirror lost its original stopped process/main-thread owner.");
    private void Fail(Exception error) { _failed = true; _failure = error.ToString(); }
    private ushort Count(uint address) => BitConverter.ToUInt16(Read(address + 8, 2));
    private uint U(uint address) => Win32Native.ReadUInt32(_host.ProcessHandle, address);
    private byte[] Read(uint address, int count) => Win32Native.ReadMemory(_host.ProcessHandle, address, count);
    private float[] Floats(uint address, int count)
    { byte[] bytes = Read(address, count * 4); return Enumerable.Range(0, count).Select(index => BitConverter.ToSingle(bytes, index * 4)).ToArray(); }
    private static byte[] Bytes(float[] values) => values.SelectMany(BitConverter.GetBytes).ToArray();
    private static WorldStarState Copy(WorldStarState value) => new(value.ActivationId, value.Category, value.Slot,
        value.StateC8, value.Flags, (float[])value.Position.Clone(), (float[])value.Scale.Clone(), (float[])value.Rotation.Clone());
    private static bool EqualStates(IReadOnlyList<WorldStarState> left, IReadOnlyList<WorldStarState> right) =>
        left.Count == right.Count && left.Zip(right).All(pair => pair.First.ActivationId == pair.Second.ActivationId &&
            pair.First.Category == pair.Second.Category && pair.First.Slot == pair.Second.Slot &&
            pair.First.StateC8 == pair.Second.StateC8 && pair.First.Flags == pair.Second.Flags &&
            Bytes(pair.First.Position).SequenceEqual(Bytes(pair.Second.Position)) &&
            Bytes(pair.First.Scale).SequenceEqual(Bytes(pair.Second.Scale)) &&
            Bytes(pair.First.Rotation).SequenceEqual(Bytes(pair.Second.Rotation)));
    private static void Require(bool value, string message) { if (!value) throw new InvalidDataException(message); }
}
