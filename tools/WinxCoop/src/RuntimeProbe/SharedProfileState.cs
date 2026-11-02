using System.Security.Cryptography;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>Local stopped-scene identity. These native pointers never enter a snapshot.</summary>
internal readonly record struct SharedProfileScene(uint LevelEpoch, uint BodyGeneration, uint Level,
    uint PlayingState, uint PlayingTable, uint Scene);

internal sealed record SharedProfileEntryApply(string BeforeHash, string AppliedHash,
    IReadOnlyList<string> ChangedRanges, int VerifiedWrites, int BytesWritten);

/// <summary>Copy-owned canonical values, without native pointers or a second wire codec.</summary>
internal sealed class SharedProfileSnapshot
{
    private readonly byte[] _payload;

    internal SharedProfileSnapshot(uint levelEpoch, uint bodyGeneration, uint level, uint revision,
        byte[] payload, uint schema = SharedProfileState.SchemaVersion)
    {
        ArgumentNullException.ThrowIfNull(payload);
        _payload = (byte[])payload.Clone();
        SharedProfileState.ValidatePayload(_payload);
        if (schema != SharedProfileState.SchemaVersion || levelEpoch == 0 || bodyGeneration == 0 ||
            level == 0 || revision == 0)
            throw new InvalidDataException("Shared Profile snapshot schema/identity/revision is invalid.");
        Schema = schema; LevelEpoch = levelEpoch; BodyGeneration = bodyGeneration; Level = level; Revision = revision;
    }

    internal uint Schema { get; }
    internal uint LevelEpoch { get; }
    internal uint BodyGeneration { get; }
    internal uint Level { get; }
    internal uint Revision { get; }
    internal byte[] Payload => (byte[])_payload.Clone();
    internal byte[] CopyPayload() => (byte[])_payload.Clone();
    internal ReadOnlySpan<byte> Values => _payload;
}

/// <summary>
/// Captures selected canonical scalar/flag ranges on Host and applies their
/// absolute values quietly on Guest. No grants, shop events, save loading,
/// native function calls, historical-pointer restore, or UI replay belong here.
/// </summary>
internal sealed class SharedProfileState
{
    internal const uint SchemaVersion = 1;
    internal const int PayloadBytes = 411;
    private const uint ProfileTable = 0x6F5A44, ManagerTable = 0x6FDEDC;
    private const uint InstanceTable = 0x710F80, NodeTable = 0x6DC4F4;
    private const uint MaximumSnapshots = 100000;
    private readonly DebuggerHost _host;
    private readonly Action<object> _log;
    private readonly bool _guestApply;
    private readonly int _ownerThread;
    private readonly Dictionary<string, ulong> _rangeWrites = [];
    private SharedProfileScene? _scene;
    private IntPtr _process;
    private uint _pid, _profile, _manager, _flow, _record, _root;
    private uint _lastCaptureRevision, _lastApplyRevision, _retireEntry;
    private ulong _captures, _applications, _writesAttempted, _writesVerified, _bytesWritten;
    private ulong _rollbackAttempts, _rollbackWrites, _rollbackVerified, _unchangedApplications;
    private bool _bound, _retired, _terminalFailure, _applying;
    private string? _lastCaptureHash, _lastAppliedHash;

    private enum Owner { Profile, Manager }
    private sealed record Range(string Name, Owner Owner, uint Offset, int Length, int PayloadOffset, bool BooleanOnly = false);
    private sealed record WritePlan(Range Range, uint Address, byte[] Before, byte[] Requested);
    private static readonly Range[] Ranges =
    [
        new("gem-mask", Owner.Profile, 0x3B8, 48, 0),
        new("gem-counts", Owner.Profile, 0x3E8, 6, 48),
        new("gem-spent", Owner.Profile, 0x3F0, 4, 54),
        new("narrative-bits", Owner.Profile, 0x3F4, 52, 58),
        new("tutorial-bits", Owner.Profile, 0x428, 12, 110),
        new("progression", Owner.Profile, 0x514, 4, 122),
        new("profile-flags", Owner.Profile, 0x518, 50, 126, true),
        new("attacks-enabled", Owner.Profile, 0x510, 1, 176),
        new("attack-context", Owner.Profile, 0x512, 1, 177),
        new("power-rank", Owner.Profile, 0x6E4, 4, 178),
        new("fairy-stars", Owner.Profile, 0x6F0, 4, 182),
        new("item-and-quest-flags", Owner.Manager, 0x2C, 215, 186, true),
        new("fashion-availability", Owner.Manager, 0x14C, 10, 401, true)
    ];
    // The same selected canonical ranges precede the two original spawn inputs.
    // This is an entry-only transaction, never a recurring World setter.
    private static readonly Range[] EntryRanges = [.. Ranges,
        new("entry-load-flag", Owner.Profile, 0x6E0, 1, PayloadBytes, true),
        new("entry-saved-position", Owner.Profile, 0x584, 12, PayloadBytes + 1)];
    internal const int EntryValuesBytes = PayloadBytes + 1 + 12;

    internal SharedProfileState(DebuggerHost host, Action<object> log, bool guestApply)
    {
        _host = host ?? throw new ArgumentNullException(nameof(host));
        _log = log ?? throw new ArgumentNullException(nameof(log));
        _guestApply = guestApply; _ownerThread = Environment.CurrentManagedThreadId;
        ValidateLayout();
    }

    internal bool Active => _bound && !_retired && !_terminalFailure;
    internal bool Retired => _retired;
    internal object Summary => new
    {
        bound = _bound, active = Active, retired = _retired, terminalFailure = _terminalFailure,
        guestApply = _guestApply, schema = SchemaVersion, payloadBytes = PayloadBytes,
        levelEpoch = _scene?.LevelEpoch, bodyGeneration = _scene?.BodyGeneration, level = _scene?.Level,
        profile = Hex(_profile), sideQuestManager = Hex(_manager), scene = Hex(_scene?.Scene ?? 0),
        lastCaptureRevision = _lastCaptureRevision, lastAppliedRevision = _lastApplyRevision,
        captures = _captures, applicationsReadbackVerified = _applications,
        unchangedApplications = _unchangedApplications, rangeWriteAttempts = _writesAttempted,
        rangeWritesReadbackVerified = _writesVerified, bytesRequestedBySuccessfulWrites = _bytesWritten,
        rollbackAttempts = _rollbackAttempts, rollbackWritesReadbackVerified = _rollbackWrites,
        completeTransactionRollbacksVerified = _rollbackVerified, pendingApply = _applying,
        lastCaptureHash = _lastCaptureHash, lastAppliedHash = _lastAppliedHash, retireEntry = Hex(_retireEntry),
        rangeWrites = new Dictionary<string, ulong>(_rangeWrites), nativeCallsAdded = 0,
        grantOrSpendOperationsReplayed = false, uiEventsReplayed = false, wholeProfileCopied = false,
        pointersInPayload = false, baselineRestoredAtUnload = false, fireflyKeyedCollectionIncluded = false,
        runtimeGameplayQualificationClaimed = false
    };

    internal void BindAtWorld(uint tid, ref Win32Native.X86Context context, in SharedProfileScene scene)
    {
        EnsureWorld(tid, context);
        Require(!_bound && !_retired && !_terminalFailure && scene.LevelEpoch != 0 && scene.BodyGeneration != 0 &&
            scene.Level != 0 && scene.PlayingState >= 0x10000 && scene.Scene >= 0x10000 &&
            scene.PlayingTable is 0x710874 or 0x7105C8,
            "Shared Profile binding has an invalid/reused bounded scene identity.");
        try
        {
            _process = _host.ProcessHandle; _pid = _host.ProcessPid; _scene = scene;
            Require(_host.ImageBase == 0x400000 && _process != IntPtr.Zero && _pid != 0,
                "Shared Profile requires its owned original x86 image.");
            _profile = Pointer(0x765AD4); _manager = Pointer(0x765AE0); _flow = Pointer(0x755294);
            Require(U(_profile) == ProfileTable && U(_manager) == ManagerTable,
                "Shared Profile singleton types differ before following their native body graph.");
            _record = Pointer(_profile + 0x2B4); _root = Pointer(_record + 0x24);
            uint stop = _host.DebugStopSequence;
            VerifyOwners(tid, context, stop, requirePlayingOnTop: true);
            byte[] baseline = ReadValues(); ValidatePayload(baseline);
            VerifyOwners(tid, context, stop, requirePlayingOnTop: true);
            _bound = true;
            Emit("bound", new { scene.LevelEpoch, scene.BodyGeneration, scene.Level, guestApply = _guestApply,
                profile = Hex(_profile), sideQuestManager = Hex(_manager), scene = Hex(scene.Scene),
                nativeInitializedBaselineHash = Hash(baseline), baselineRetainedForUnloadRestore = false,
                schema = SchemaVersion, payloadBytes = PayloadBytes, scalarRangeCount = Ranges.Length });
        }
        catch { _terminalFailure = true; throw; }
    }

    internal SharedProfileSnapshot CaptureAtWorld(uint tid, ref Win32Native.X86Context context, uint revision)
    {
        EnsureWorld(tid, context);
        Require(Active && !_guestApply && !_applying && revision != 0 && revision > _lastCaptureRevision &&
            _captures < MaximumSnapshots, "Shared Profile capture is not a fresh active Host observation.");
        try
        {
            uint stop = _host.DebugStopSequence;
            VerifyOwners(tid, context, stop);
            byte[] values = ReadValues(); ValidatePayload(values);
            VerifyOwners(tid, context, stop);
            SharedProfileScene scene = Scene;
            var snapshot = new SharedProfileSnapshot(scene.LevelEpoch, scene.BodyGeneration, scene.Level, revision, values);
            ++_captures; _lastCaptureRevision = revision;
            string hash = Hash(values); bool changed = hash != _lastCaptureHash; _lastCaptureHash = hash;
            if (changed || _captures <= 2 || _captures % 120 == 0)
                Emit("host-captured", new { revision, scene.LevelEpoch, scene.BodyGeneration, scene.Level,
                    hash, changed, payloadBytes = values.Length, fieldWrites = 0 });
            return snapshot;
        }
        catch { _terminalFailure = true; throw; }
    }

    internal void ApplyAtWorld(uint tid, ref Win32Native.X86Context context, SharedProfileSnapshot snapshot)
    {
        ArgumentNullException.ThrowIfNull(snapshot);
        EnsureWorld(tid, context);
        Require(Active && _guestApply && !_applying && _applications < MaximumSnapshots,
            "Shared Profile quiet apply requires its active Guest at an idle WorldRET.");
        uint stop = _host.DebugStopSequence;
        List<WritePlan> attempted = [];
        byte[]? before = null;
        try
        {
            SharedProfileScene scene = Scene;
            Require(snapshot.Schema == SchemaVersion && snapshot.LevelEpoch == scene.LevelEpoch &&
                snapshot.BodyGeneration == scene.BodyGeneration && snapshot.Level == scene.Level &&
                snapshot.Revision != 0 && snapshot.Revision > _lastApplyRevision,
                "Shared Profile snapshot has a stale/foreign scene generation or revision.");
            byte[] requested = snapshot.CopyPayload(); ValidatePayload(requested);
            VerifyOwners(tid, context, stop);
            before = ReadValues(); ValidatePayload(before);
            // Every allowed destination and its complete before-image is staged
            // before the first write; no caller supplies an address or length.
            List<WritePlan> plans = [];
            foreach (Range range in Ranges)
            {
                uint address = Address(range);
                byte[] old = before.AsSpan(range.PayloadOffset, range.Length).ToArray();
                byte[] next = requested.AsSpan(range.PayloadOffset, range.Length).ToArray();
                if (!old.AsSpan().SequenceEqual(next)) plans.Add(new(range, address, old, next));
            }
            VerifyOwners(tid, context, stop);
            Require(ReadValues().AsSpan().SequenceEqual(before),
                "Shared Profile before-image changed while staging a stopped transaction.");
            _applying = true;
            foreach (WritePlan plan in plans)
            {
                VerifyOwners(tid, context, stop);
                Require(Read(plan.Address, plan.Range.Length).AsSpan().SequenceEqual(plan.Before),
                    "Shared Profile selected range changed before its absolute quiet write.");
                attempted.Add(plan); ++_writesAttempted;
                Win32Native.WriteMemory(_process, plan.Address, plan.Requested, executable: false);
                VerifyOwners(tid, context, stop);
                Require(Read(plan.Address, plan.Range.Length).AsSpan().SequenceEqual(plan.Requested),
                    "Shared Profile quiet range write/readback differs.");
                ++_writesVerified; _bytesWritten += (uint)plan.Range.Length;
                _rangeWrites[plan.Range.Name] = _rangeWrites.GetValueOrDefault(plan.Range.Name) + 1;
            }
            VerifyOwners(tid, context, stop);
            byte[] actual = ReadValues();
            Require(actual.AsSpan().SequenceEqual(requested), "Shared Profile complete quiet apply readback differs.");
            ++_applications; if (plans.Count == 0) ++_unchangedApplications;
            _lastApplyRevision = snapshot.Revision; _lastAppliedHash = Hash(actual); _applying = false;
            if (plans.Count != 0 || _applications <= 2 || _applications % 120 == 0)
                Emit("guest-quiet-applied", new { snapshot.Revision, scene.LevelEpoch, scene.BodyGeneration, scene.Level,
                    hash = _lastAppliedHash, beforeHash = Hash(before), changedRanges = plans.Select(p => p.Range.Name).ToArray(),
                    readbackVerified = true, nativeCallsAdded = 0, uiEventsReplayed = false });
        }
        catch (Exception error)
        {
            _terminalFailure = true;
            // A native WriteProcessMemory failure can follow a partial write.
            // Preserve actual bytes BEFORE rollback, including the attempted
            // failing range. Never invent an all-or-nothing after-state.
            bool sameOwners = OwnersStillCurrent(tid, context, stop);
            object[] actualAfterFailure = ObserveAttempted(tid, context, stop, attempted, sameOwners);
            bool rolledBack = attempted.Count == 0;
            string? rollbackError = null;
            if (attempted.Count != 0 && sameOwners)
            {
                ++_rollbackAttempts;
                try
                {
                    for (int index = attempted.Count - 1; index >= 0; --index)
                    {
                        VerifyOwners(tid, context, stop);
                        WritePlan plan = attempted[index];
                        Win32Native.WriteMemory(_process, plan.Address, plan.Before, executable: false);
                        VerifyOwners(tid, context, stop);
                        Require(Read(plan.Address, plan.Range.Length).AsSpan().SequenceEqual(plan.Before),
                            "Shared Profile rollback range readback differs.");
                        ++_rollbackWrites;
                    }
                    VerifyOwners(tid, context, stop);
                    Require(before is not null && ReadValues().AsSpan().SequenceEqual(before),
                        "Shared Profile full transaction rollback readback differs.");
                    rolledBack = true; ++_rollbackVerified;
                }
                catch (Exception rollbackFailure) { rollbackError = rollbackFailure.ToString(); }
            }
            bool ownersAfterRollback = OwnersStillCurrent(tid, context, stop);
            Emit("guest-quiet-apply-failed", new { snapshot.Revision, error = error.ToString(),
                attemptedRanges = attempted.Select(p => p.Range.Name).ToArray(), sameOwnersBeforeRollback = sameOwners,
                actualAfterFailure, transactionRollbackVerified = rolledBack, rollbackError,
                ownersAfterRollback, actualAfterRollback = ObserveAttempted(tid, context, stop, attempted, ownersAfterRollback),
                historicalPointersReadOrWrittenAfterOwnerLoss = false, nativeContinuationForbidden = true });
            _applying = false;
            throw;
        }
    }

    internal void RetireAtStoppedBoundary(uint tid, ref Win32Native.X86Context context, uint nativeEntry)
    {
        EnsureStopped(tid);
        Require(Active && !_applying && context.Eip == nativeEntry &&
            (nativeEntry is NativeCallDispatcher.WorldReturn or 0x5F86C0 or 0x5F2300) &&
            (nativeEntry == NativeCallDispatcher.WorldReturn || context.Ecx == Scene.PlayingState),
            "Shared Profile retirement is not its idle owned WorldRET/native playing unload.");
        _retired = true; _retireEntry = nativeEntry;
        Emit("retired-before-unload", new { entry = Hex(nativeEntry), fieldWrites = 0,
            baselineRestored = false, historicalOwnerReadsNowForbidden = true });
    }

    internal static void ValidatePayload(ReadOnlySpan<byte> payload)
    {
        Require(payload.Length == PayloadBytes, "Shared Profile schema1 payload is not exactly411 bytes.");
        foreach (Range range in Ranges)
            if (range.BooleanOnly)
                foreach (byte value in payload.Slice(range.PayloadOffset, range.Length))
                    Require(value <= 1, $"Shared Profile {range.Name} left its native boolean0/1 domain.");
        // Counts, bitfields, rank and mode512 stay raw: no guessed game cap,
        // wallet clamp, packed-pointer interpretation or attack-mode enum.
    }

    internal static void ValidateEntryValues(ReadOnlySpan<byte> values)
    {
        ValidateEntryLayout();
        Require(values.Length == EntryValuesBytes, "Entry selected values must be exactly424 bytes.");
        ValidatePayload(values[..PayloadBytes]);
        Require(values[PayloadBytes] <= 1, "Original pre-entry load flag is outside0/1.");
        for (int index = PayloadBytes + 1; index < EntryValuesBytes; index += 4)
        {
            float value = BitConverter.ToSingle(values.Slice(index, 4));
            Require(float.IsFinite(value) && MathF.Abs(value) <= 10000000,
                "Original pre-entry saved position is not finite/in bounds.");
        }
    }

    internal static byte[] CaptureEntryValuesAtStop(DebuggerHost host, uint tid,
        in Win32Native.X86Context context, NativeWorldEntryFrame frame, Action<object> log)
    {
        frame.VerifyAtStop(host, tid, context);
        byte[] values = ReadEntryValues(host, frame); ValidateEntryValues(values);
        frame.VerifyAtStop(host, tid, context);
        Require(ReadEntryValues(host, frame).AsSpan().SequenceEqual(values),
            "Entry selected values changed inside the stopped observation.");
        log(new { @event = "world-entry-profile-captured", selectedBytes = EntryValuesBytes,
            profileBytes = PayloadBytes, hash = Hash(values), nativeCalls = 0, fieldWrites = 0 });
        return values;
    }

    internal static SharedProfileEntryApply ApplyEntryValuesAtStop(DebuggerHost host, uint tid,
        in Win32Native.X86Context context, NativeWorldEntryFrame frame, byte[] requested,
        Action<object> log)
    {
        requested = (byte[])requested.Clone();
        ValidateEntryValues(requested); frame.VerifyAtStop(host, tid, context);
        byte[] before = ReadEntryValues(host, frame); ValidateEntryValues(before);
        List<WritePlan> plans = [], attempted = [];
        foreach (Range range in EntryRanges)
        {
            byte[] old = before.AsSpan(range.PayloadOffset, range.Length).ToArray();
            byte[] next = requested.AsSpan(range.PayloadOffset, range.Length).ToArray();
            if (!old.AsSpan().SequenceEqual(next)) plans.Add(new(range,
                RangeAddress(frame.Profile, frame.SideQuestManager, range), old, next));
        }
        int verified = 0, bytes = 0;
        try
        {
            frame.VerifyAtStop(host, tid, context);
            Require(ReadEntryValues(host, frame).AsSpan().SequenceEqual(before),
                "Entry before-image changed while staging the stopped transaction.");
            foreach (WritePlan plan in plans)
            {
                frame.VerifyAtStop(host, tid, context);
                Require(Win32Native.ReadMemory(host.ProcessHandle, plan.Address, plan.Range.Length)
                    .AsSpan().SequenceEqual(plan.Before), "Entry selected range changed before its absolute write.");
                attempted.Add(plan); // A failed write may already have changed some bytes.
                Win32Native.WriteMemory(host.ProcessHandle, plan.Address, plan.Requested, executable: false);
                frame.VerifyAtStop(host, tid, context);
                Require(Win32Native.ReadMemory(host.ProcessHandle, plan.Address, plan.Range.Length)
                    .AsSpan().SequenceEqual(plan.Requested), "Entry selected range write/readback differs.");
                ++verified; bytes += plan.Range.Length;
            }
            frame.VerifyAtStop(host, tid, context);
            Require(ReadEntryValues(host, frame).AsSpan().SequenceEqual(requested),
                "Complete entry selected-state write/readback differs.");
            var result = new SharedProfileEntryApply(Hash(before), Hash(requested),
                Array.AsReadOnly(plans.Select(p => p.Range.Name).ToArray()), verified, bytes);
            log(new { @event = "world-entry-profile-quiet-applied", result.BeforeHash, result.AppliedHash,
                result.ChangedRanges, result.VerifiedWrites, result.BytesWritten,
                transactionBytes = EntryValuesBytes, readbackVerified = true,
                grantSpendOrQuestCalls = 0, nativeCalls = 0, historicalBaselineRestored = false });
            return result;
        }
        catch (Exception error)
        {
            bool owners = frame.IsCurrentAtStop(host, tid, context);
            object[] actualAfterFailure = ObserveEntryPlans(host, tid, context, frame, attempted, owners);
            bool rollbackVerified = false; string? rollbackError = null;
            if (attempted.Count != 0 && owners)
                try
                {
                    for (int index = attempted.Count - 1; index >= 0; --index)
                    {
                        frame.VerifyAtStop(host, tid, context); WritePlan plan = attempted[index];
                        Win32Native.WriteMemory(host.ProcessHandle, plan.Address, plan.Before, executable: false);
                        frame.VerifyAtStop(host, tid, context);
                        Require(Win32Native.ReadMemory(host.ProcessHandle, plan.Address, plan.Range.Length)
                            .AsSpan().SequenceEqual(plan.Before), "Entry transaction rollback range readback differs.");
                    }
                    frame.VerifyAtStop(host, tid, context);
                    Require(ReadEntryValues(host, frame).AsSpan().SequenceEqual(before),
                        "Complete entry transaction rollback differs from its immediate before-image.");
                    rollbackVerified = true;
                }
                catch (Exception rollbackFailure) { rollbackError = rollbackFailure.ToString(); }
            bool afterOwners = frame.IsCurrentAtStop(host, tid, context);
            log(new { @event = "world-entry-profile-quiet-apply-failed", error = error.ToString(),
                attemptedRanges = attempted.Select(p => p.Range.Name).ToArray(), sameOwnersBeforeRollback = owners,
                actualAfterFailure, transactionRollbackVerified = rollbackVerified, rollbackError,
                ownersAfterRollback = afterOwners,
                actualAfterRollback = ObserveEntryPlans(host, tid, context, frame, attempted, afterOwners),
                oldSceneOrHistoricalOwnerRestored = false, nativeContinuationForbidden = true });
            throw;
        }
    }

    private static byte[] ReadEntryValues(DebuggerHost host, NativeWorldEntryFrame frame)
    {
        byte[] result = new byte[EntryValuesBytes];
        foreach (Range range in EntryRanges)
            Win32Native.ReadMemory(host.ProcessHandle, RangeAddress(frame.Profile, frame.SideQuestManager, range), range.Length)
                .CopyTo(result, range.PayloadOffset);
        return result;
    }

    private static void ValidateEntryLayout()
    {
        ValidateLayout(); int size = 0;
        foreach (Range range in EntryRanges)
        {
            Require(range.PayloadOffset == size && range.Length > 0, "Entry selected ranges have a gap/overlap.");
            size = checked(size + range.Length);
        }
        Require(size == EntryValuesBytes, "Entry selected ranges have a wrong complete size.");
        foreach (Owner owner in Enum.GetValues<Owner>())
        {
            uint end = 0;
            foreach (Range range in EntryRanges.Where(r => r.Owner == owner).OrderBy(r => r.Offset))
            {
                Require(range.Offset >= end, "Entry native scalar ranges overlap.");
                end = checked(range.Offset + (uint)range.Length);
            }
        }
    }

    private static object[] ObserveEntryPlans(DebuggerHost host, uint tid, in Win32Native.X86Context context,
        NativeWorldEntryFrame frame, List<WritePlan> plans, bool owners)
    {
        List<object> result = [];
        foreach (WritePlan plan in plans)
        {
            string? actual = null, readError = null;
            owners = owners && frame.IsCurrentAtStop(host, tid, context);
            if (owners)
                try { actual = Convert.ToHexString(Win32Native.ReadMemory(host.ProcessHandle, plan.Address, plan.Range.Length)); }
                catch (Exception error) { readError = error.ToString(); }
            result.Add(new { range = plan.Range.Name, address = Hex(plan.Address),
                before = Convert.ToHexString(plan.Before), requested = Convert.ToHexString(plan.Requested),
                actual, readError, readSkippedAfterOwnerLoss = !owners });
        }
        return result.ToArray();
    }

    private void VerifyOwners(uint tid, in Win32Native.X86Context context, uint stop, bool requirePlayingOnTop = false)
    {
        EnsureWorld(tid, context);
        SharedProfileScene scene = Scene;
        Require(_host.DebugStopSequence == stop && _host.ProcessHandle == _process && _host.ProcessPid == _pid &&
            U(0x765AD4) == _profile && U(_profile) == ProfileTable &&
            U(0x765AE0) == _manager && U(_manager) == ManagerTable &&
            U(_profile + 0x2B4) == _record && U(_record) == InstanceTable &&
            U(_record + 0x24) == _root && U(_root) == NodeTable && U(_root + 0x3C) == scene.Scene &&
            U(0x755294) == _flow && U(_flow + 0x1B8) == 0 &&
            U(scene.PlayingState) == scene.PlayingTable && U(scene.PlayingState + 0x10) == scene.Level,
            "Shared Profile stopped process/profile/manager/body/scene/generation owners changed.");
        uint stack = U(_flow + 0x1AC);
        Require(stack <= 20, "Shared Profile playing-state stack exceeds its native bound.");
        uint matches = 0, top = 0;
        for (uint index = 0; index <= stack; ++index)
        {
            uint state = U(_flow + 0x15C + index * 4);
            if (state == scene.PlayingState) ++matches;
            if (index == stack) top = state;
        }
        Require(matches == 1 && top >= 0x10000 && (!requirePlayingOnTop || top == scene.PlayingState) &&
            U(_flow + 0x1B0) == (top == scene.PlayingState ? scene.Level : U(top + 0x10)),
            "Shared Profile native playing state is absent/duplicated under the current UI stack.");
        // Root owns qualification of allowed UI/pause contexts. A retained
        // playing state keeps this exact scene identity; no new scene is bound.
    }

    private bool OwnersStillCurrent(uint tid, in Win32Native.X86Context context, uint stop)
    { try { VerifyOwners(tid, context, stop); return true; } catch { return false; } }

    private object[] ObserveAttempted(uint tid, in Win32Native.X86Context context, uint stop,
        List<WritePlan> attempted, bool sameOwners)
    {
        List<object> observations = [];
        foreach (WritePlan plan in attempted)
        {
            string? actual = null, readError = null;
            sameOwners = sameOwners && OwnersStillCurrent(tid, context, stop);
            if (sameOwners)
                try { actual = Convert.ToHexString(Read(plan.Address, plan.Range.Length)); }
                catch (Exception error) { readError = error.ToString(); }
            observations.Add(new { range = plan.Range.Name, address = Hex(plan.Address),
                before = Convert.ToHexString(plan.Before), requested = Convert.ToHexString(plan.Requested),
                actual, readError, readSkippedAfterOwnerLoss = !sameOwners });
        }
        return observations.ToArray();
    }

    private byte[] ReadValues()
    {
        byte[] values = new byte[PayloadBytes];
        foreach (Range range in Ranges) Read(Address(range), range.Length).CopyTo(values, range.PayloadOffset);
        return values;
    }

    private uint Address(Range range)
        => RangeAddress(_profile, _manager, range);

    private static uint RangeAddress(uint profile, uint manager, Range range)
    {
        uint owner = range.Owner == Owner.Profile ? profile : manager;
        Require(owner >= 0x10000 && range.Length is > 0 and <= 215, "Shared Profile destination range is invalid.");
        uint address = checked(owner + range.Offset); _ = checked(address + (uint)range.Length - 1);
        return address;
    }

    private static void ValidateLayout()
    {
        int payload = 0;
        foreach (Range range in Ranges)
        {
            Require(range.PayloadOffset == payload && range.Length > 0, "Shared Profile fixed schema has a gap/overlap.");
            payload = checked(payload + range.Length);
        }
        Require(payload == PayloadBytes, "Shared Profile fixed schema has an invalid length.");
        foreach (Owner owner in Enum.GetValues<Owner>())
        {
            uint end = 0;
            foreach (Range range in Ranges.Where(r => r.Owner == owner).OrderBy(r => r.Offset))
            {
                Require(range.Offset >= end, "Shared Profile native scalar ranges overlap.");
                end = checked(range.Offset + (uint)range.Length);
            }
        }
    }

    private SharedProfileScene Scene => _scene ?? throw new InvalidDataException("Shared Profile scene is not bound.");
    private uint Pointer(uint address) { uint value = U(address); Require(value >= 0x10000, "Shared Profile pointer is null/low."); return value; }
    private uint U(uint address) => BitConverter.ToUInt32(Read(address, 4));
    private byte[] Read(uint address, int count)
    {
        EnsureStopped(_host.MainThreadId);
        Require(!_retired && address >= 0x10000 && count is > 0 and <= 411,
            "Shared Profile read is retired or outside its pointer/length bound.");
        _ = checked(address + (uint)count - 1);
        return Win32Native.ReadMemory(_host.ProcessHandle, address, count);
    }
    private void EnsureWorld(uint tid, in Win32Native.X86Context context)
    { EnsureStopped(tid); Require(context.Eip == NativeCallDispatcher.WorldReturn, "Shared Profile requires the qualified actual WorldRET."); }
    private void EnsureStopped(uint tid) => Require(_host.IsAtDebugStop && tid == _host.MainThreadId &&
        Environment.CurrentManagedThreadId == _ownerThread, "Shared Profile requires its owned main-thread debug stop.");
    private void Emit(string name, object data) => _log(new { @event = "shared-profile-state", name, data });
    private static string Hash(byte[] values) => Convert.ToHexString(SHA256.HashData(values));
    private static string Hex(uint value) => $"0x{value:X8}";
    private static void Require(bool condition, string message)
    { if (!condition) throw new InvalidDataException(message); }
}
