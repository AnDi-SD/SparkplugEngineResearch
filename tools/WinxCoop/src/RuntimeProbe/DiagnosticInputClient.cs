using System.ComponentModel;
using System.IO.MemoryMappedFiles;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace WinxCoop.RuntimeProbe;

internal enum DiagnosticInputRole : uint { Host = 1, Guest = 2 }
internal enum DiagnosticBridgeState : uint { Waiting = 1, Connected = 2, Ready = 3, Paused = 4, Closed = 5 }
internal enum DiagnosticInputAvailability : uint { Waiting = 0, Ready = 1, Paused = 2, Retired = 3 }
internal enum DiagnosticInputTerminal : uint
{
    None = 0, Identity = 1, InvalidRecord = 2, Sequence = 3, Overflow = 4, Transport = 5,
    PeerClosed = 6, ActorClosed = 7, RunLimit = 8, Clock = 9, CounterExhausted = 10, LeaseExpired = 11, NativeSource = 12
}
internal enum DiagnosticPublishResult { Published, NotReady, EpochChanged, NoCredit, Full, Terminal, Retired }
internal enum DiagnosticDeliveryDisposition { Accepted, Suppressed, OldEpochDiscard, TerminalReject }
internal readonly record struct DiagnosticInputIdentity(string MappingName, DiagnosticInputRole Role, uint OwnerPid,
    ulong OwnerCreationFileTime, ulong Nonce, ulong SessionId, ulong ScenarioId, uint LevelEpoch, uint BodyGeneration);

[StructLayout(LayoutKind.Explicit, Size = 64, Pack = 8)]
internal struct DiagnosticInputHeader
{
    [FieldOffset(0)] internal uint Magic;
    [FieldOffset(4)] internal uint Version;
    [FieldOffset(8)] internal uint Bytes;
    [FieldOffset(12)] internal uint Capacity;
    [FieldOffset(16)] internal ulong Nonce;
    [FieldOffset(24)] internal uint OwnerPid;
    [FieldOffset(28)] internal DiagnosticInputRole Role;
    [FieldOffset(32)] internal ulong OwnerCreationFileTime;
    [FieldOffset(40)] internal ulong SessionId;
    [FieldOffset(48)] internal ulong ScenarioId;
    [FieldOffset(56)] internal uint LevelEpoch;
    [FieldOffset(60)] internal uint BodyGeneration;
}
[StructLayout(LayoutKind.Explicit, Size = 64, Pack = 8)]
internal struct DiagnosticBridgeStatus
{
    [FieldOffset(0)] internal uint Publication;
    [FieldOffset(4)] internal DiagnosticBridgeState State;
    [FieldOffset(8)] internal ulong ConnectionToken;
    [FieldOffset(16)] internal uint InputEpoch;
    [FieldOffset(20)] internal uint Grant;
    [FieldOffset(24)] internal ulong LeaseDeadlineMilliseconds;
    [FieldOffset(32)] internal uint MaximumSourceSequence;
    [FieldOffset(36)] internal uint AcknowledgedSourceSequence;
    [FieldOffset(40)] internal uint NeutralRequired;
    [FieldOffset(44)] internal DiagnosticInputTerminal TerminalReason;
    [FieldOffset(48)] internal ulong BridgeTickMilliseconds;
    [FieldOffset(56)] internal ulong Reserved;
}
[StructLayout(LayoutKind.Explicit, Size = 64, Pack = 8)]
internal struct DiagnosticActorStatus
{
    [FieldOffset(0)] internal uint Publication;
    [FieldOffset(4)] internal DiagnosticInputAvailability Availability;
    [FieldOffset(8)] internal ulong ParticipantCreationFileTime;
    [FieldOffset(16)] internal uint ParticipantPid;
    [FieldOffset(20)] internal uint AppliedEpoch;
    [FieldOffset(24)] internal uint AppliedSourceSequence;
    [FieldOffset(28)] internal uint ReadinessRevision;
    [FieldOffset(32)] internal ulong ObservedAtMilliseconds;
    [FieldOffset(40)] internal DiagnosticInputTerminal TerminalReason;
    [FieldOffset(44)] internal uint Reserved0;
    [FieldOffset(48)] internal ulong Reserved1;
    [FieldOffset(56)] internal ulong Reserved2;
}
[StructLayout(LayoutKind.Explicit, Size = 64, Pack = 8)]
internal struct DiagnosticInputRecord
{
    [FieldOffset(0)] internal uint InputEpoch;
    [FieldOffset(4)] internal uint SourceSequence;
    [FieldOffset(8)] internal ulong CaptureOrdinal;
    [FieldOffset(16)] internal ulong CaptureMilliseconds;
    [FieldOffset(24)] internal ulong HostIngressMilliseconds;
    [FieldOffset(32)] internal float Magnitude;
    [FieldOffset(36)] internal float RelativeDirection;
    [FieldOffset(40)] internal float ReferenceHeading;
    [FieldOffset(44)] internal uint Buttons;
    [FieldOffset(48)] internal uint Grant;
    [FieldOffset(52)] internal uint Reserved;
    [FieldOffset(56)] internal ulong LeaseDeadlineMilliseconds;
    internal readonly bool JumpHeld => (Buttons & 1) != 0;
    internal readonly bool OrdinaryAttackHeld => (Buttons & 2) != 0;
}
[StructLayout(LayoutKind.Explicit, Size = 64, Pack = 8)]
internal struct DiagnosticInputRingHeader
{
    [FieldOffset(0)] internal uint WriteCursor;
    [FieldOffset(4)] internal uint ReadCursor;
    [FieldOffset(8)] internal DiagnosticInputTerminal ProducerTerminal;
    [FieldOffset(12)] internal uint Reserved;
    [FieldOffset(16)] internal ulong Reserved0;
    [FieldOffset(24)] internal ulong Reserved1;
    [FieldOffset(32)] internal ulong Reserved2;
    [FieldOffset(40)] internal ulong Reserved3;
    [FieldOffset(48)] internal ulong Reserved4;
    [FieldOffset(56)] internal ulong Reserved5;
}
internal readonly record struct DiagnosticInputDelivery(uint ReadCursor, DiagnosticInputRecord Record);

/// <summary>IPC5 scalar/FIFO mirror only. No TCP, application Packet codec or game pointer crosses this boundary.</summary>
internal sealed unsafe class DiagnosticInputClient : IDisposable
{
    internal const uint Magic = 0x35494357, Version = 5, Capacity = 64;
    internal const int SharedBytes = 4352, BridgeOffset = 64, ActorOffset = 128, RingOffset = 192, RecordsOffset = 256;
    private const int GrantHistoryCapacity = 32, BridgeStatusRetryMilliseconds = 500;
    private readonly DiagnosticInputIdentity _identity;
    private readonly int _ownerThread = Environment.CurrentManagedThreadId;
    private readonly Action<object>? _log;
    private MemoryMappedFile? _mapping;
    private MemoryMappedViewAccessor? _view;
    private byte* _pointer;
    private bool _pointerHeld, _retired, _disposed;
    private DiagnosticInputTerminal _terminal;
    private DiagnosticBridgeStatus _bridge;
    private DiagnosticActorStatus _actor;
    private DiagnosticInputDelivery? _pending;
    private readonly Queue<(uint Epoch, uint Grant, ulong Deadline, uint MaximumSequence)> _grants = [];
    private uint _publishedEpoch, _publishedSequence, _lastWrite, _lastRead, _committedEpoch, _committedSequence;
    private ulong _publishedOrdinal, _publishedCaptureTime, _publications, _commits, _discarded, _acks;
    private long _lastOwnerCheckMilliseconds;
    private ulong _bridgeStatusContendedReads, _bridgeStatusRetries, _bridgeStatusRetryExpiries;
    private long _bridgeStatusMaximumRetryMilliseconds;

    internal DiagnosticInputClient(DiagnosticInputIdentity identity, Action<object>? log = null)
    {
        if (!OperatingSystem.IsWindows()) throw new PlatformNotSupportedException("Diagnostic IPC4 uses Windows process identity and mappings.");
        ValidateLayout(); _identity = identity; _log = log;
        Require(BitConverter.IsLittleEndian && !string.IsNullOrWhiteSpace(identity.MappingName) &&
            identity.MappingName.Length <= 240 && !identity.MappingName.Contains('\0') &&
            identity.Role is DiagnosticInputRole.Host or DiagnosticInputRole.Guest && identity.OwnerPid != 0 &&
            identity.OwnerCreationFileTime != 0 && identity.Nonce != 0 && identity.SessionId != 0 && identity.ScenarioId != 0 &&
            identity.LevelEpoch != 0 && identity.BodyGeneration != 0, "Invalid diagnostic mapping identity/platform.");
        Require(CreationFileTime(identity.OwnerPid) == identity.OwnerCreationFileTime, "Diagnostic bridge PID creation identity differs.");
        try
        {
            _mapping = MemoryMappedFile.OpenExisting(identity.MappingName, MemoryMappedFileRights.ReadWrite);
            _view = _mapping.CreateViewAccessor(0, SharedBytes, MemoryMappedFileAccess.ReadWrite);
            Require(_view.Capacity == SharedBytes, "Diagnostic mapping view has a different size.");
            byte* raw = null; _view.SafeMemoryMappedViewHandle.AcquirePointer(ref raw); _pointerHeld = true;
            _pointer = raw + _view.PointerOffset;
            ValidateHeader();
            Require(TryReadStatus(ActorOffset, out DiagnosticActorStatus previous) && previous.ParticipantPid == 0 &&
                previous.ParticipantCreationFileTime == 0 && previous.Publication == 0,
                "Diagnostic actor status already has a participant; endpoint reuse is forbidden.");
            DiagnosticInputRingHeader ring = ReadPlain<DiagnosticInputRingHeader>(RingOffset);
            Require(ring.WriteCursor == 0 && ring.ReadCursor == 0 && ring.ProducerTerminal == DiagnosticInputTerminal.None &&
                ring.Reserved == 0 && ring.Reserved0 == 0 && ring.Reserved1 == 0 && ring.Reserved2 == 0 &&
                ring.Reserved3 == 0 && ring.Reserved4 == 0 && ring.Reserved5 == 0,
                "Diagnostic endpoint must claim a fresh empty ring.");
            _actor = new() { ParticipantPid = checked((uint)Environment.ProcessId),
                ParticipantCreationFileTime = CreationFileTime(checked((uint)Environment.ProcessId)),
                Availability = DiagnosticInputAvailability.Waiting, ReadinessRevision = 1,
                ObservedAtMilliseconds = checked((ulong)LocalNowMilliseconds()) };
            Require(PublishActor(), "Diagnostic actor initial publication failed.");
        }
        catch { ReleaseMapping(); _disposed = true; throw; }
    }

    internal DiagnosticInputIdentity Identity => _identity;
    internal bool Retired => _retired;
    internal DiagnosticInputTerminal Terminal => _terminal;
    internal bool TerminalOrRetired => _terminal != DiagnosticInputTerminal.None || _retired || _disposed;
    internal object Summary => new
    {
        role = _identity.Role.ToString(), version = Version, ipcBytes = SharedBytes, capacity = Capacity,
        _identity.SessionId, _identity.ScenarioId, _identity.LevelEpoch, _identity.BodyGeneration,
        retired = _retired, disposed = _disposed, terminal = _terminal.ToString(),
        inputEpoch = _bridge.InputEpoch, connectionToken = _bridge.ConnectionToken, grant = _bridge.Grant,
        published = _publications, committed = _commits, staleDiscarded = _discarded, appliedAcknowledgments = _acks,
        pendingExactDelivery = _pending.HasValue, readinessRevision = _actor.ReadinessRevision,
        bridgeStatusContendedReads = _bridgeStatusContendedReads, bridgeStatusRetries = _bridgeStatusRetries,
        bridgeStatusMaximumRetryMilliseconds = _bridgeStatusMaximumRetryMilliseconds,
        bridgeStatusRetryExpiries = _bridgeStatusRetryExpiries, bridgeStatusRetryLimitMilliseconds = BridgeStatusRetryMilliseconds,
        hostIngressRestamped = false, nativePointersOrWireCodec = false
    };

    internal bool TryReadBridgeStatus(out DiagnosticBridgeStatus status)
    {
        EnsureThread(); status = default; if (TerminalOrRetired) return false;
        long started = LocalNowMilliseconds();
        bool contended = false;
        uint attempts = 0;
        for (;;)
        {
            ValidateHeader();
            long now = LocalNowMilliseconds();
            if (!ValidateBridgeOwner(now, force: false)) return false;
            DiagnosticInputTerminal pendingTerminal = (DiagnosticInputTerminal)Acquire(RingOffset + 8);
            if (!KnownTerminal(pendingTerminal)) { Fence(DiagnosticInputTerminal.Identity); return false; }
            if (pendingTerminal != DiagnosticInputTerminal.None) { Fence(pendingTerminal); return false; }
            bool copied = TryReadStatus(BridgeOffset, out status);
            long waited = Math.Max(0, LocalNowMilliseconds() - started);
            if (contended) _bridgeStatusMaximumRetryMilliseconds = Math.Max(_bridgeStatusMaximumRetryMilliseconds, waited);
            if (contended && waited >= BridgeStatusRetryMilliseconds)
            {
                ++_bridgeStatusRetryExpiries;
                _log?.Invoke(new { @event = "diagnostic-input-bridge-status-retry-expired", role = _identity.Role.ToString(),
                    waitedMilliseconds = waited, attempts, limitMilliseconds = BridgeStatusRetryMilliseconds,
                    cachedStatusAccepted = false, inputAppliedOrAcknowledged = false });
                Fence(DiagnosticInputTerminal.LeaseExpired); status = default; return false;
            }
            if (copied)
            {
                // A contended read never reuses _bridge. Revalidate the actual
                // final copy's header/owner before its ordinary field, terminal
                // and500ms heartbeat checks below can admit any input.
                ValidateHeader();
                if (contended && !ValidateBridgeOwner(LocalNowMilliseconds(), force: true)) return false;
                if (contended && _bridgeStatusContendedReads <= 8)
                    _log?.Invoke(new { @event = "diagnostic-input-bridge-status-retry-recovered", role = _identity.Role.ToString(),
                        waitedMilliseconds = waited, attempts, status.Publication, cachedStatusAccepted = false });
                break;
            }
            if (!contended) { contended = true; ++_bridgeStatusContendedReads; }
            ++_bridgeStatusRetries; ++attempts;
            // Let the independent sole writer finish its odd/even publication;
            // three immediate low-level reads can all observe the same odd word.
            // No cursor, command, Ready lease or ACK is advanced while waiting.
            if (attempts <= 8) Thread.Yield(); else Thread.Sleep(1);
        }
        if ((uint)status.State is < 1 or > 5 || status.Reserved != 0 ||
            status.NeutralRequired > 1 || !KnownTerminal(status.TerminalReason))
        { Fence(DiagnosticInputTerminal.Identity); return false; }
        _bridge = status;
        DiagnosticInputTerminal producer = (DiagnosticInputTerminal)Acquire(RingOffset + 8);
        if (!KnownTerminal(producer)) { Fence(DiagnosticInputTerminal.Identity); return false; }
        if (status.TerminalReason != DiagnosticInputTerminal.None || producer != DiagnosticInputTerminal.None || status.State == DiagnosticBridgeState.Closed)
        { Fence(status.TerminalReason != 0 ? status.TerminalReason : producer != 0 ? producer : DiagnosticInputTerminal.PeerClosed); return false; }
        if (status.State == DiagnosticBridgeState.Ready)
        {
            ulong localNow = checked((ulong)LocalNowMilliseconds());
            if (status.BridgeTickMilliseconds > localNow || localNow - status.BridgeTickMilliseconds > 500)
            {
                _log?.Invoke(new { @event = "diagnostic-input-bridge-heartbeat-expired", role = _identity.Role.ToString(),
                    localNowMilliseconds = localNow, status.BridgeTickMilliseconds, status.Publication,
                    status.InputEpoch, status.Grant, staleBridgeHeartbeatLimitMilliseconds = 500 });
                Fence(DiagnosticInputTerminal.LeaseExpired); return false;
            }
            if (status.ConnectionToken == 0 || status.InputEpoch == 0 || status.Grant == 0 || status.LeaseDeadlineMilliseconds == 0 ||
                status.MaximumSourceSequence < status.AcknowledgedSourceSequence || status.MaximumSourceSequence - status.AcknowledgedSourceSequence > 8)
            { Fence(DiagnosticInputTerminal.InvalidRecord); return false; }
            if (_grants.Count != 0 && _grants.Peek().Epoch != status.InputEpoch) _grants.Clear();
            DiagnosticBridgeStatus snapshot = status;
            if (!_grants.Any(x => x.Epoch == snapshot.InputEpoch && x.Grant == snapshot.Grant && x.Deadline == snapshot.LeaseDeadlineMilliseconds))
            { _grants.Enqueue((status.InputEpoch, status.Grant, status.LeaseDeadlineMilliseconds, status.MaximumSourceSequence)); if (_grants.Count > GrantHistoryCapacity) _grants.Dequeue(); }
        }
        return true;
    }

    private bool ValidateBridgeOwner(long now, bool force)
    {
        if (!force && now - _lastOwnerCheckMilliseconds < 250) return true;
        try
        {
            if (CreationFileTime(_identity.OwnerPid) != _identity.OwnerCreationFileTime)
            { Fence(DiagnosticInputTerminal.Identity); return false; }
        }
        catch (Exception error) when (error is Win32Exception or InvalidDataException)
        { Fence(DiagnosticInputTerminal.PeerClosed); return false; }
        _lastOwnerCheckMilliseconds = now; return true;
    }

    // Call at actual local owner observations (including unchanged availability)
    // so Ready is refreshed within the bridge's configured actor lease.
    internal bool SetAvailability(DiagnosticInputAvailability availability, bool forceRearmRevision = false)
    {
        EnsureThread(); if (TerminalOrRetired) return false;
        Require((uint)availability <= 3, "Unknown diagnostic availability.");
        if (_actor.Availability != availability || forceRearmRevision)
        {
            if (_actor.ReadinessRevision == uint.MaxValue) { Fence(DiagnosticInputTerminal.CounterExhausted); return false; }
            ++_actor.ReadinessRevision;
        }
        _actor.Availability = availability; _actor.ObservedAtMilliseconds = checked((ulong)LocalNowMilliseconds());
        if (!PublishActor()) { Fence(DiagnosticInputTerminal.CounterExhausted); return false; }
        return true;
    }

    internal DiagnosticPublishResult TryPublishCapture(in DiagnosticInputRecord record)
    {
        EnsureThread(); if (_retired || _disposed) return DiagnosticPublishResult.Retired;
        if (_terminal != 0) return DiagnosticPublishResult.Terminal;
        Require(_identity.Role == DiagnosticInputRole.Guest, "Only the Guest actor produces capture slots.");
        if (!TryReadBridgeStatus(out DiagnosticBridgeStatus status)) return _terminal != 0 ? DiagnosticPublishResult.Terminal : DiagnosticPublishResult.NotReady;
        if (status.State != DiagnosticBridgeState.Ready) return DiagnosticPublishResult.NotReady;
        if (record.InputEpoch != status.InputEpoch) return DiagnosticPublishResult.EpochChanged;
        DiagnosticInputRecord value = record;
        if (!ValidRecord(record, hostDelivery: false) || !_grants.Any(x => x.Epoch == value.InputEpoch && x.Grant == value.Grant &&
            x.Deadline == value.LeaseDeadlineMilliseconds))
        { Fence(DiagnosticInputTerminal.InvalidRecord); return DiagnosticPublishResult.Terminal; }
        uint expected = _publishedEpoch == record.InputEpoch ? _publishedSequence + 1 : 1;
        if (_publishedSequence == uint.MaxValue || record.SourceSequence != expected || record.CaptureOrdinal <= _publishedOrdinal ||
            record.CaptureMilliseconds < _publishedCaptureTime || record.CaptureMilliseconds > (ulong)LocalNowMilliseconds())
        { Fence(DiagnosticInputTerminal.Sequence); return DiagnosticPublishResult.Terminal; }
        if (record.SourceSequence > status.MaximumSourceSequence) { Fence(DiagnosticInputTerminal.Overflow); return DiagnosticPublishResult.NoCredit; }
        if (!_grants.Any(x => x.Epoch == value.InputEpoch && x.Grant == value.Grant &&
            x.Deadline == value.LeaseDeadlineMilliseconds && value.SourceSequence <= x.MaximumSequence))
        { Fence(DiagnosticInputTerminal.InvalidRecord); return DiagnosticPublishResult.Terminal; }
        if (!ReadCursors(out uint write, out uint read)) return DiagnosticPublishResult.Terminal;
        if (write != _lastWrite || write >= uint.MaxValue - 1) { Fence(DiagnosticInputTerminal.CounterExhausted); return DiagnosticPublishResult.Terminal; }
        if (write - read == Capacity) { Fence(DiagnosticInputTerminal.Overflow); return DiagnosticPublishResult.Full; }
        Unsafe.WriteUnaligned(_pointer + RecordsOffset + (write % Capacity) * 64, record);
        Thread.MemoryBarrier(); Release(RingOffset, write + 1); _lastWrite = write + 1;
        _publishedEpoch = record.InputEpoch; _publishedSequence = record.SourceSequence;
        _publishedOrdinal = record.CaptureOrdinal; _publishedCaptureTime = record.CaptureMilliseconds; ++_publications;
        return DiagnosticPublishResult.Published;
    }

    internal bool TryPeekDelivery(out DiagnosticInputDelivery delivery)
    {
        EnsureThread(); delivery = default; if (TerminalOrRetired) return false;
        Require(_identity.Role == DiagnosticInputRole.Host, "Only the Host actor consumes delivery slots.");
        if (!TryReadBridgeStatus(out _)) return false;
        if (_pending is DiagnosticInputDelivery retained) { delivery = retained; return true; }
        if (!ReadCursors(out uint write, out uint read)) return false;
        if (read != _lastRead) { Fence(DiagnosticInputTerminal.Identity); return false; }
        if (read == write) return false;
        Thread.MemoryBarrier(); DiagnosticInputRecord record = ReadPlain<DiagnosticInputRecord>(RecordsOffset + (read % Capacity) * 64);
        Thread.MemoryBarrier();
        if (!ValidRecord(record, hostDelivery: true)) { Fence(DiagnosticInputTerminal.InvalidRecord); return false; }
        delivery = new(read, record); _pending = delivery; return true;
    }

    // A Full destination queue must retain this exact ticket and must not call
    // Commit. Acceptance is distinct from the later native Applied ACK.
    internal bool CommitDelivery(in DiagnosticInputDelivery delivery, DiagnosticDeliveryDisposition disposition)
    {
        EnsureThread(); if (TerminalOrRetired) return false;
        Require(_identity.Role == DiagnosticInputRole.Host && _pending is not null, "No Host delivery is currently borrowed.");
        DiagnosticInputDelivery pending = _pending ?? throw new InvalidDataException("No Host delivery is currently borrowed.");
        DiagnosticInputRecord supplied = delivery.Record, original = pending.Record;
        if (delivery.ReadCursor != pending.ReadCursor || !new ReadOnlySpan<byte>(&supplied, 64).SequenceEqual(new ReadOnlySpan<byte>(&original, 64)) ||
            !ReadCursors(out uint write, out uint read) || read != delivery.ReadCursor || write <= read)
        { Fence(DiagnosticInputTerminal.Identity); return false; }
        if (read == uint.MaxValue) { Fence(DiagnosticInputTerminal.CounterExhausted); return false; }
        if (disposition is not (DiagnosticDeliveryDisposition.Accepted or DiagnosticDeliveryDisposition.Suppressed or
            DiagnosticDeliveryDisposition.OldEpochDiscard or DiagnosticDeliveryDisposition.TerminalReject))
            throw new InvalidDataException("Unknown diagnostic commit disposition.");
        Release(RingOffset + 4, read + 1); _lastRead = read + 1; _pending = null; ++_commits;
        if (disposition == DiagnosticDeliveryDisposition.OldEpochDiscard) ++_discarded;
        else if (disposition is DiagnosticDeliveryDisposition.Accepted or DiagnosticDeliveryDisposition.Suppressed)
        { _committedEpoch = original.InputEpoch; _committedSequence = original.SourceSequence; }
        else Fence(DiagnosticInputTerminal.InvalidRecord);
        return true;
    }

    internal NativeBodyInputIntent DestinationIntent(in DiagnosticInputRecord record, uint destinationSequence)
    {
        EnsureThread(); Require(!TerminalOrRetired && _identity.Role == DiagnosticInputRole.Host && destinationSequence != 0 &&
            ValidRecord(record, hostDelivery: true), "Destination intent requires a live Host delivery with preserved ingress time.");
        return new(_identity.BodyGeneration, destinationSequence, record.Magnitude, record.RelativeDirection, record.ReferenceHeading,
            record.JumpHeld, checked((long)record.HostIngressMilliseconds), record.OrdinaryAttackHeld);
    }

    // Caller supplies the actual WorldRET applied/suppressed verifier. This is
    // deliberately not called by CommitDelivery, peeking, or IPC queue drain.
    internal bool AcknowledgeApplied(uint epoch, uint sourceSequence, Action verifyNativeBoundary)
    {
        EnsureThread(); if (TerminalOrRetired) return false; ArgumentNullException.ThrowIfNull(verifyNativeBoundary);
        Require(_identity.Role == DiagnosticInputRole.Host && epoch != 0 && sourceSequence != 0 &&
            _committedEpoch == epoch && sourceSequence <= _committedSequence &&
            (_actor.AppliedEpoch == epoch ? sourceSequence == _actor.AppliedSourceSequence + 1 : sourceSequence == 1),
            "Applied acknowledgment is not a contiguous committed Host source record.");
        verifyNativeBoundary(); _actor.AppliedEpoch = epoch; _actor.AppliedSourceSequence = sourceSequence;
        _actor.ObservedAtMilliseconds = checked((ulong)LocalNowMilliseconds());
        if (!PublishActor()) { Fence(DiagnosticInputTerminal.CounterExhausted); return false; } ++_acks; return true;
    }

    internal void Fence(DiagnosticInputTerminal reason)
    {
        EnsureThread(); if (_retired || _disposed || _terminal != 0) return;
        Require(reason != 0 && KnownTerminal(reason), "Invalid diagnostic terminal reason.");
        _terminal = reason; _pending = null; _actor.TerminalReason = reason;
        _actor.ObservedAtMilliseconds = checked((ulong)LocalNowMilliseconds());
        if (_identity.Role == DiagnosticInputRole.Guest)
            Interlocked.CompareExchange(ref *(int*)(_pointer + RingOffset + 8), unchecked((int)reason), 0);
        _ = PublishActor();
        _log?.Invoke(new { @event = "diagnostic-input-terminal", role = _identity.Role.ToString(), reason = reason.ToString(), nextSafeWorldMustNeutralize = true });
    }
    internal void Retire(DiagnosticInputTerminal reason = DiagnosticInputTerminal.ActorClosed)
    {
        EnsureThread(); if (_retired || _disposed) return;
        if (_terminal == 0 && reason != 0) Fence(reason);
        _actor.Availability = DiagnosticInputAvailability.Retired;
        if (_actor.ReadinessRevision != uint.MaxValue) ++_actor.ReadinessRevision;
        _actor.ObservedAtMilliseconds = checked((ulong)LocalNowMilliseconds()); _ = PublishActor();
        _pending = null; _retired = true;
    }
    public void Dispose()
    {
        EnsureThread(); if (_disposed) return;
        try { Retire(); } finally { ReleaseMapping(); _disposed = true; }
    }

    internal static long LocalNowMilliseconds()
    { ulong now = GetTickCount64(); Require(now <= long.MaxValue, "Local GetTickCount64 exceeds the controller clock domain."); return (long)now; }
    private bool ReadCursors(out uint write, out uint read)
    {
        write = Acquire(RingOffset); read = Acquire(RingOffset + 4);
        if (write < read || write - read > Capacity) { Fence(DiagnosticInputTerminal.Identity); return false; }
        return true;
    }
    private bool ValidRecord(in DiagnosticInputRecord r, bool hostDelivery) => r.InputEpoch != 0 && r.SourceSequence != 0 && r.CaptureOrdinal != 0 &&
        r.Grant != 0 && r.LeaseDeadlineMilliseconds != 0 && r.Reserved == 0 && (r.Buttons & ~3u) == 0 &&
        float.IsFinite(r.Magnitude) && r.Magnitude is >= 0 and <= 1 && Angle(r.RelativeDirection) && Angle(r.ReferenceHeading) &&
        (hostDelivery ? r.HostIngressMilliseconds != 0 && r.HostIngressMilliseconds <= (ulong)LocalNowMilliseconds() : r.HostIngressMilliseconds == 0);
    private static bool Angle(float value) => float.IsFinite(value) && value is >= -3.1415927410125732421875f and <= 3.1415927410125732421875f;
    private void ValidateHeader()
    {
        DiagnosticInputHeader h = ReadPlain<DiagnosticInputHeader>(0);
        Require(h.Magic == Magic && h.Version == Version && h.Bytes == SharedBytes && h.Capacity == Capacity &&
            h.Nonce == _identity.Nonce && h.OwnerPid == _identity.OwnerPid && h.OwnerCreationFileTime == _identity.OwnerCreationFileTime &&
            h.Role == _identity.Role && h.SessionId == _identity.SessionId && h.ScenarioId == _identity.ScenarioId &&
            h.LevelEpoch == _identity.LevelEpoch && h.BodyGeneration == _identity.BodyGeneration, "Diagnostic IPC4 header identity changed.");
    }
    private bool TryReadStatus<T>(int offset, out T copy) where T : unmanaged
    {
        for (int attempt = 0; attempt < 3; ++attempt)
        {
            uint before = Acquire(offset); if ((before & 1) != 0) continue;
            Thread.MemoryBarrier(); copy = ReadPlain<T>(offset); Thread.MemoryBarrier();
            if (before == Acquire(offset)) return true;
        }
        copy = default; return false;
    }
    private bool PublishActor()
    {
        uint before = Acquire(ActorOffset); if ((before & 1) != 0 || before > 0xFFFFFFFCu) return false;
        Release(ActorOffset, before + 1); Thread.MemoryBarrier();
        DiagnosticActorStatus value = _actor;
        Buffer.MemoryCopy((byte*)&value + 4, _pointer + ActorOffset + 4, 60L, 60L);
        Thread.MemoryBarrier(); Release(ActorOffset, before + 2); _actor.Publication = before + 2; return true;
    }
    private T ReadPlain<T>(long offset) where T : unmanaged => Unsafe.ReadUnaligned<T>(_pointer + offset);
    private uint Acquire(int offset) => unchecked((uint)Interlocked.CompareExchange(ref *(int*)(_pointer + offset), 0, 0));
    private void Release(int offset, uint value) => Interlocked.Exchange(ref *(int*)(_pointer + offset), unchecked((int)value));
    private void ReleaseMapping()
    {
        if (_pointerHeld) { _view!.SafeMemoryMappedViewHandle.ReleasePointer(); _pointerHeld = false; }
        _pointer = null; _view?.Dispose(); _view = null; _mapping?.Dispose(); _mapping = null;
    }
    private void EnsureThread() => Require(Environment.CurrentManagedThreadId == _ownerThread, "Diagnostic IPC has one synchronous ActorProbe owner thread.");
    private static bool KnownTerminal(DiagnosticInputTerminal reason) => (uint)reason <= 12;
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidDataException(message); }
    internal static void ValidateLayout()
    {
        Require(Marshal.SizeOf<DiagnosticInputHeader>() == 64 && Marshal.SizeOf<DiagnosticBridgeStatus>() == 64 &&
            Marshal.SizeOf<DiagnosticActorStatus>() == 64 && Marshal.SizeOf<DiagnosticInputRecord>() == 64 &&
            Marshal.SizeOf<DiagnosticInputRingHeader>() == 64 && RecordsOffset + Capacity * 64 == SharedBytes,
            "Diagnostic IPC4 structure sizes differ from InputIpc.h.");
        Require(Marshal.OffsetOf<DiagnosticInputRecord>(nameof(DiagnosticInputRecord.HostIngressMilliseconds)).ToInt32() == 24 &&
            Marshal.OffsetOf<DiagnosticInputRecord>(nameof(DiagnosticInputRecord.Magnitude)).ToInt32() == 32 &&
            Marshal.OffsetOf<DiagnosticInputRecord>(nameof(DiagnosticInputRecord.Grant)).ToInt32() == 48 &&
            Marshal.OffsetOf<DiagnosticInputRecord>(nameof(DiagnosticInputRecord.LeaseDeadlineMilliseconds)).ToInt32() == 56,
            "Diagnostic IPC4 record offsets differ from InputIpc.h.");
    }
    private static ulong CreationFileTime(uint pid)
    {
        IntPtr process = OpenProcess(0x1000, false, pid);
        if (process == IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error(), "Cannot query diagnostic process identity.");
        try
        {
            if (!GetProcessTimes(process, out NativeFileTime creation, out _, out _, out _) || !GetExitCodeProcess(process, out uint exitCode))
                throw new Win32Exception(Marshal.GetLastWin32Error(), "Cannot read diagnostic process creation time.");
            Require(exitCode == 259, "Diagnostic process identity is no longer live.");
            return ((ulong)creation.High << 32) | creation.Low;
        }
        finally { _ = CloseHandle(process); }
    }
    [StructLayout(LayoutKind.Sequential)] private struct NativeFileTime { internal uint Low, High; }
    [DllImport("kernel32.dll", SetLastError = true)] private static extern IntPtr OpenProcess(uint access, [MarshalAs(UnmanagedType.Bool)] bool inherit, uint pid);
    [DllImport("kernel32.dll", SetLastError = true)] [return: MarshalAs(UnmanagedType.Bool)] private static extern bool GetProcessTimes(IntPtr process, out NativeFileTime creation, out NativeFileTime exit, out NativeFileTime kernel, out NativeFileTime user);
    [DllImport("kernel32.dll", SetLastError = true)] [return: MarshalAs(UnmanagedType.Bool)] private static extern bool GetExitCodeProcess(IntPtr process, out uint code);
    [DllImport("kernel32.dll")] [return: MarshalAs(UnmanagedType.Bool)] private static extern bool CloseHandle(IntPtr handle);
    [DllImport("kernel32.dll")] private static extern ulong GetTickCount64();
}
