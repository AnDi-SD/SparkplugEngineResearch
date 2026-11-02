using System.ComponentModel;
using System.IO.MemoryMappedFiles;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// One ActorProbe owner of the isolated WorldState1 local mapping. C++ alone
/// owns TCP/Packet framing. Reading a Guest slot never acknowledges native apply.
/// </summary>
internal sealed unsafe class WorldStateClient : IDisposable
{
    private const ulong RetryMilliseconds = 500, BridgeHeartbeatMilliseconds = 500;
    private readonly WorldStateIdentity _identity;
    private readonly Action<object>? _log;
    private readonly int _ownerThread = Environment.CurrentManagedThreadId;
    private MemoryMappedFile? _mapping;
    private MemoryMappedViewAccessor? _view;
    private byte* _pointer;
    private bool _pointerHeld, _retired, _disposed, _fencing;
    private WorldStateTerminal _terminal;
    private WorldStateActorStatus _actor;
    private WorldStateBridgeStatus _lastBridge; // Diagnostics only; never used as a fresh control grant.
    private WorldStateDelivery? _pending;
    private ulong _connectionToken, _lastNow, _lastHostRevision, _lastHostTick, _lastGuestRevision;
    private uint _lastSyncEpoch, _hostPublication, _lastGuestPublication, _lastGuestEpoch;
    private ulong _published, _delivered, _acknowledged, _discarded, _actorPublications;
    private ulong _contendedReads, _retries, _retryExpiries, _maximumRetryMilliseconds;

    internal WorldStateClient(WorldStateIdentity identity, Action<object>? log = null)
    {
        if (!OperatingSystem.IsWindows()) throw new PlatformNotSupportedException("WorldState1 IPC uses Windows mappings/process identity.");
        WorldStateContract.ValidateLayout(); _identity = identity; _log = log;
        const string prefix = "Local\\WinxCoop.WorldState1.";
        Require(identity.MappingName is not null && identity.MappingName.StartsWith(prefix, StringComparison.Ordinal) &&
            identity.MappingName.Length > prefix.Length && identity.MappingName.Length <= 160 &&
            identity.MappingName.Skip(prefix.Length).All(c => char.IsAsciiLetterOrDigit(c) || c is '.' or '-' or '_') &&
            identity.Role is WorldStateRole.Host or WorldStateRole.Guest && identity.OwnerPid != 0 &&
            identity.OwnerCreationFileTime != 0 && identity.Nonce != 0 && identity.SessionId != 0 && identity.ScenarioId != 0 &&
            identity.LevelEpoch != 0 && identity.Generation != 0, "WorldState1 mapping identity is invalid.");
        Require(CreationFileTime(identity.OwnerPid) == identity.OwnerCreationFileTime, "WorldState1 bridge creation identity differs.");
        try
        {
            _mapping = MemoryMappedFile.OpenExisting(identity.MappingName!, MemoryMappedFileRights.ReadWrite);
            _view = _mapping.CreateViewAccessor(0, WorldStateContract.SharedBytes, MemoryMappedFileAccess.ReadWrite);
            Require(_view.Capacity == WorldStateContract.SharedBytes, "WorldState1 mapping view has a different size.");
            byte* raw = null; _view.SafeMemoryMappedViewHandle.AcquirePointer(ref raw); _pointerHeld = true;
            _pointer = raw + _view.PointerOffset; ValidateHeader();
            Require(TryReadCoherent(WorldStateContract.ActorOffset, out WorldStateActorStatus previous) &&
                previous.Publication == 0 && previous.Availability == 0 && previous.ParticipantPid == 0 &&
                previous.ReadinessRevision == 0 && previous.ParticipantCreationFileTime == 0 && previous.ObservedAtMilliseconds == 0 &&
                previous.AppliedEpoch == 0 && previous.TerminalReason == 0 && previous.AppliedRevision == 0 &&
                previous.Reserved1 == 0 && previous.Reserved2 == 0,
                "WorldState1 Actor endpoint is not fresh; reuse is forbidden.");
            Require(new ReadOnlySpan<byte>(_pointer + WorldStateContract.SnapshotOffset, WorldStateContract.SnapshotSlotBytes)
                .IndexOfAnyExcept((byte)0) < 0, "WorldState1 snapshot endpoint is not fresh.");
            _actor = new() { Availability = WorldStateAvailability.Waiting, ParticipantPid = checked((uint)Environment.ProcessId),
                ReadinessRevision = 1, ParticipantCreationFileTime = CreationFileTime(checked((uint)Environment.ProcessId)),
                ObservedAtMilliseconds = Now() };
            Require(PublishActor(claim: true), "WorldState1 Actor endpoint was claimed concurrently.");
        }
        catch { ReleaseMapping(); _disposed = true; throw; }
    }

    internal WorldStateIdentity Identity => _identity;
    internal bool Retired => _retired;
    internal WorldStateTerminal Terminal => _terminal;
    internal bool TerminalOrRetired => _terminal != WorldStateTerminal.None || _retired || _disposed;
    internal object Stats => new
    {
        role = _identity.Role.ToString(), version = WorldStateContract.Version, ipcBytes = WorldStateContract.SharedBytes,
        _identity.SessionId, _identity.ScenarioId, _identity.LevelEpoch, _identity.Generation,
        retired = _retired, disposed = _disposed, terminal = _terminal.ToString(),
        phase = _lastBridge.Phase.ToString(), connectionToken = _connectionToken, syncEpoch = _lastSyncEpoch,
        sentRevision = _lastBridge.SentRevision, appliedRevision = _lastBridge.AppliedRevision,
        receivedRevision = _lastBridge.ReceivedRevision, lastHostRevision = _lastHostRevision,
        lastGuestRevision = _lastGuestRevision, published = _published, delivered = _delivered,
        actualApplyAcknowledgments = _acknowledged, pendingNativeApply = _pending is not null, discardedPending = _discarded,
        actorPublications = _actorPublications, readinessRevision = _actor.ReadinessRevision,
        contendedReads = _contendedReads, retries = _retries, retryExpiries = _retryExpiries,
        maximumRetryMilliseconds = _maximumRetryMilliseconds, retryLimitMilliseconds = RetryMilliseconds,
        cachedStatusAccepted = false, readImpliesAck = false, nativePointersOrNetworkCodec = false
    };
    internal object Summary => Stats;

    /// <summary>Root calls at real scene observations, also unchanged Ready, to refresh the native-owner lease.</summary>
    internal bool SetReady(bool ready, bool forceRevision = false)
    {
        EnsureThread(); if (TerminalOrRetired || !TryReadBridgeStatus(out _)) return false;
        WorldStateAvailability next = ready ? WorldStateAvailability.Ready : WorldStateAvailability.Waiting;
        if (_actor.Availability != next || forceRevision)
        {
            if (_actor.ReadinessRevision == uint.MaxValue) return Fence(WorldStateTerminal.CounterExhausted);
            ++_actor.ReadinessRevision;
            if (_pending is not null) { _pending = null; ++_discarded; }
        }
        _actor.Availability = next; _actor.ObservedAtMilliseconds = Now();
        if (!PublishActor()) return Fence(WorldStateTerminal.Identity);
        if (_actorPublications <= 3 || forceRevision)
            Emit(ready ? "actor-ready" : "actor-waiting", new { _actor.ReadinessRevision, _actor.ObservedAtMilliseconds });
        return true;
    }

    internal bool TryReadBridgeStatus(out WorldStateBridgeStatus status)
    {
        EnsureThread(); status = default; if (TerminalOrRetired) return false;
        return ReadBridgeFresh(Now(), out status);
    }

    /// <summary>Host replaces its local full snapshot. Connected/Waiting can precede the first native Guest ACK/Ready.</summary>
    internal bool TryPublish(WorldStateSnapshot snapshot)
    {
        EnsureThread(); if (TerminalOrRetired) return false;
        Require(_identity.Role == WorldStateRole.Host, "Only the Host Actor publishes WorldState1 snapshots.");
        if (_actor.Availability != WorldStateAvailability.Ready || !TryReadBridgeStatus(out _)) return false;
        try
        {
            byte[] payload = WorldStateContract.PackLocalSnapshot(snapshot);
            if (snapshot.Revision <= _lastHostRevision || snapshot.NativeTick < _lastHostTick)
                return Fence(WorldStateTerminal.Sequence);
            ValidateHeader();
            uint before = Acquire(WorldStateContract.SnapshotOffset);
            if (before != _hostPublication || (before & 1) != 0) return Fence(WorldStateTerminal.Identity);
            if (before > 0xFFFFFFFCu) return Fence(WorldStateTerminal.CounterExhausted);
            byte[] slot = new byte[WorldStateContract.SnapshotSlotBytes];
            var prefix = new WorldStateSnapshotPrefix { Bytes = (uint)payload.Length, Revision = snapshot.Revision,
                NativeTick = snapshot.NativeTick, Flags = snapshot.Flags };
            MemoryMarshal.Write(slot.AsSpan(0, 64), in prefix); payload.CopyTo(slot, 64);
            if (!ClaimOdd(WorldStateContract.SnapshotOffset, before)) return Fence(WorldStateTerminal.Identity);
            Thread.MemoryBarrier();
            fixed (byte* source = slot)
                Buffer.MemoryCopy(source + 4, _pointer + WorldStateContract.SnapshotOffset + 4,
                    WorldStateContract.SnapshotSlotBytes - 4, WorldStateContract.SnapshotSlotBytes - 4);
            Thread.MemoryBarrier(); Release(WorldStateContract.SnapshotOffset, before + 2); _hostPublication = before + 2;
            ValidateHeader(); _lastHostRevision = snapshot.Revision; _lastHostTick = snapshot.NativeTick; ++_published;
            if (_published <= 2 || _published % 120 == 0)
                Emit("host-published", new { snapshot.Revision, snapshot.NativeTick, snapshot.Flags, bytes = payload.Length, publication = _hostPublication });
            return true;
        }
        catch (Exception error) when (error is InvalidDataException or ArgumentException or OverflowException)
        { Emit("host-publish-rejected", new { error = error.ToString() }); return Fence(WorldStateTerminal.InvalidRecord); }
    }

    /// <summary>Returns one complete exact Guest delivery. Root applies it; this method never advances an ACK.</summary>
    internal bool TryRead(out WorldStateDelivery delivery)
    {
        EnsureThread(); delivery = null!; if (TerminalOrRetired) return false;
        Require(_identity.Role == WorldStateRole.Guest, "Only the Guest Actor reads WorldState1 deliveries.");
        if (_actor.Availability != WorldStateAvailability.Ready) return false;
        ulong started = Now(); uint attempts = 0;
        for (;;)
        {
            if (!ReadBridgeFresh(started, out WorldStateBridgeStatus status)) return false;
            if (status.Phase is not (WorldStatePhase.Connected or WorldStatePhase.Ready) || status.SyncEpoch == 0 || status.ReceivedRevision == 0)
            { DiscardPendingIfEpochChanged(status); return false; }
            if (_pending is not null)
            {
                if (_pending.SyncEpoch == status.SyncEpoch && _pending.Snapshot.Revision == status.ReceivedRevision) return false;
                _pending = null; ++_discarded;
            }
            if (!ReadSlotFresh(started, out byte[] slot)) return false;
            WorldStateSnapshotPrefix prefix = MemoryMarshal.Read<WorldStateSnapshotPrefix>(slot);
            if (prefix.Publication == 0 || prefix.Bytes == 0) return false;
            if (!ValidPrefix(prefix)) return Fence(WorldStateTerminal.InvalidRecord);
            if (!ReadBridgeFresh(started, out WorldStateBridgeStatus after)) return false;
            // The Bridge writes the slot and BridgeStatus separately. Do not
            // turn that legitimate publication interval into a permanent fail.
            if (after.SyncEpoch != status.SyncEpoch || after.ReceivedRevision != prefix.Revision ||
                Acquire(WorldStateContract.SnapshotOffset) != prefix.Publication)
            { if (!Retry(started, ref attempts, "slot-control-pair")) return false; continue; }
            if (after.Phase is not (WorldStatePhase.Connected or WorldStatePhase.Ready) || after.SyncEpoch == 0) return false;
            if (prefix.Publication == _lastGuestPublication) return false;
            if (prefix.Publication < _lastGuestPublication ||
                (after.SyncEpoch == _lastGuestEpoch && prefix.Revision <= _lastGuestRevision))
                return Fence(WorldStateTerminal.Sequence);
            try
            {
                WorldStateSnapshot snapshot = WorldStateContract.UnpackLocalSnapshot(slot.AsSpan(64, checked((int)prefix.Bytes)));
                if (snapshot.Revision != prefix.Revision || snapshot.NativeTick != prefix.NativeTick || snapshot.Flags != prefix.Flags)
                    return Fence(WorldStateTerminal.InvalidRecord);
                _pending = new(prefix.Publication, after.SyncEpoch, after.ConnectionToken, snapshot);
                _lastGuestPublication = prefix.Publication; _lastGuestEpoch = after.SyncEpoch; _lastGuestRevision = snapshot.Revision;
                ++_delivered; delivery = _pending;
                if (_delivered <= 2 || _delivered % 120 == 0)
                    Emit("guest-delivered", new { snapshot.Revision, snapshot.NativeTick, prefix.Publication, after.SyncEpoch,
                        nativeApplyAcknowledged = false });
                return true;
            }
            catch (Exception error) when (error is InvalidDataException or ArgumentException or OverflowException)
            { Emit("guest-delivery-rejected", new { error = error.ToString() }); return Fence(WorldStateTerminal.InvalidRecord); }
        }
    }

    /// <summary>Root alone calls this AFTER its complete stopped native apply/readback has succeeded.</summary>
    internal bool Acknowledge(WorldStateDelivery delivery)
    {
        EnsureThread(); if (TerminalOrRetired) return false;
        Require(_identity.Role == WorldStateRole.Guest, "Only the Guest native owner publishes WorldState1 apply ACKs.");
        if (!ReferenceEquals(delivery, _pending) || delivery is null || _actor.Availability != WorldStateAvailability.Ready)
            return Fence(WorldStateTerminal.NativeApply);
        ulong started = Now();
        if (!ReadBridgeFresh(started, out WorldStateBridgeStatus status)) return false;
        if (!CurrentDelivery(status, delivery)) { _pending = null; ++_discarded; return false; }
        if (!ReadSlotFresh(started, out byte[] slot)) return false;
        WorldStateSnapshotPrefix prefix = MemoryMarshal.Read<WorldStateSnapshotPrefix>(slot);
        byte[] expected = WorldStateContract.PackLocalSnapshot(delivery.Snapshot);
        if (prefix.Publication != delivery.Publication || !ValidPrefix(prefix) || prefix.Revision != delivery.Snapshot.Revision ||
            prefix.Bytes != expected.Length || !slot.AsSpan(64, expected.Length).SequenceEqual(expected))
            return Fence(WorldStateTerminal.NativeApply);
        if (!ReadBridgeFresh(started, out WorldStateBridgeStatus after)) return false;
        if (!CurrentDelivery(after, delivery)) { _pending = null; ++_discarded; return false; }
        if (Acquire(WorldStateContract.SnapshotOffset) != delivery.Publication) return Fence(WorldStateTerminal.NativeApply);
        _actor.AppliedEpoch = delivery.SyncEpoch; _actor.AppliedRevision = delivery.Snapshot.Revision;
        _actor.ObservedAtMilliseconds = Now();
        if (!PublishActor()) return Fence(WorldStateTerminal.Identity);
        ++_acknowledged; _pending = null;
        if (_acknowledged <= 2 || _acknowledged % 120 == 0)
            Emit("guest-applied-ack", new { delivery.SyncEpoch, delivery.Snapshot.Revision,
                exactDeliveredPayloadRechecked = true, rootReportedCompleteNativeApply = true });
        return true;
    }

    internal void Retire(WorldStateTerminal reason = WorldStateTerminal.ActorClosed)
    {
        EnsureThread(); if (_retired || _disposed) return;
        if (!KnownTerminal(reason) || reason == WorldStateTerminal.None) reason = WorldStateTerminal.ActorClosed;
        if (_terminal == WorldStateTerminal.None) _terminal = reason;
        _actor.Availability = WorldStateAvailability.Retired; _actor.TerminalReason = _terminal;
        if (_actor.ReadinessRevision != uint.MaxValue) ++_actor.ReadinessRevision;
        _actor.ObservedAtMilliseconds = Now(); _pending = null;
        bool published = false;
        try { published = PublishActor(); } catch (InvalidDataException) { }
        _retired = true; Emit("retired", new { terminal = _terminal.ToString(), actorTerminalPublished = published, acknowledgmentAdded = false });
    }
    public void Dispose()
    {
        EnsureThread(); if (_disposed) return;
        try { Retire(); } finally { ReleaseMapping(); _disposed = true; }
    }

    private bool ReadBridgeFresh(ulong started, out WorldStateBridgeStatus status)
    {
        status = default; if (TerminalOrRetired) return false; uint attempts = 0;
        try
        {
            for (;;)
            {
                ValidateHeader(); ValidateBridgeOwner();
                if (TryReadCoherent(WorldStateContract.BridgeOffset, out status))
                {
                    ValidateHeader(); ValidateBridgeOwner(); ulong now = Now();
                    if (!RecoveredWithinDeadline(started, attempts, "bridge-status")) { status = default; return false; }
                    if ((uint)status.Phase is < 1 or > 4 || !KnownTerminal(status.TerminalReason) || status.Reserved != 0 ||
                        status.Publication == 0 || status.SyncEpoch < _lastSyncEpoch)
                        return Fence(WorldStateTerminal.Identity);
                    if (status.TerminalReason != WorldStateTerminal.None || status.Phase == WorldStatePhase.Closed)
                    { _lastBridge = status; return Fence(status.TerminalReason != 0 ? status.TerminalReason : WorldStateTerminal.PeerClosed); }
                    if (status.BridgeTickMilliseconds == 0 || status.BridgeTickMilliseconds > now || now - status.BridgeTickMilliseconds > BridgeHeartbeatMilliseconds)
                        return Fence(WorldStateTerminal.ActorLeaseExpired);
                    if (status.ConnectionToken != 0)
                    {
                        if (_connectionToken != 0 && status.ConnectionToken != _connectionToken) return Fence(WorldStateTerminal.Identity);
                        _connectionToken = status.ConnectionToken;
                    }
                    else if (_connectionToken != 0 || _identity.Role == WorldStateRole.Host || status.SyncEpoch != 0 || status.Phase != WorldStatePhase.Waiting)
                        return Fence(WorldStateTerminal.Identity);
                    if (status.Phase == WorldStatePhase.Ready && (status.ConnectionToken == 0 || status.SyncEpoch == 0 || status.AppliedRevision == 0))
                        return Fence(WorldStateTerminal.InvalidRecord);
                    if (_identity.Role == WorldStateRole.Host ? status.ReceivedRevision != 0 || status.AppliedRevision > status.SentRevision :
                        status.SentRevision != 0 || status.AppliedRevision > status.ReceivedRevision)
                        return Fence(WorldStateTerminal.InvalidRecord);
                    _lastBridge = status; _lastSyncEpoch = status.SyncEpoch; return true;
                }
                if (!Retry(started, ref attempts, "bridge-status")) { status = default; return false; }
            }
        }
        catch (OwnerExitedException error) { Emit("bridge-owner-exited", new { error = error.ToString() }); return Fence(WorldStateTerminal.PeerClosed); }
        catch (Win32Exception error) { Emit("bridge-owner-unavailable", new { error = error.ToString() }); return Fence(WorldStateTerminal.PeerClosed); }
        catch (InvalidDataException error) { Emit("bridge-identity-rejected", new { error = error.ToString() }); return Fence(WorldStateTerminal.Identity); }
    }

    private bool ReadSlotFresh(ulong started, out byte[] slot)
    {
        slot = new byte[WorldStateContract.SnapshotSlotBytes]; uint attempts = 0;
        try
        {
            for (;;)
            {
                ValidateHeader(); ValidateBridgeOwner(); uint before = Acquire(WorldStateContract.SnapshotOffset);
                if ((before & 1) == 0)
                {
                    Thread.MemoryBarrier();
                    new ReadOnlySpan<byte>(_pointer + WorldStateContract.SnapshotOffset, slot.Length).CopyTo(slot);
                    Thread.MemoryBarrier();
                    if (before == Acquire(WorldStateContract.SnapshotOffset))
                    {
                        ValidateHeader(); ValidateBridgeOwner();
                        if (!RecoveredWithinDeadline(started, attempts, "snapshot-slot")) { slot = []; return false; }
                        return true;
                    }
                }
                if (!Retry(started, ref attempts, "snapshot-slot")) { slot = []; return false; }
            }
        }
        catch (OwnerExitedException error) { Emit("slot-owner-exited", new { error = error.ToString() }); return Fence(WorldStateTerminal.PeerClosed); }
        catch (Win32Exception error) { Emit("slot-owner-unavailable", new { error = error.ToString() }); return Fence(WorldStateTerminal.PeerClosed); }
        catch (InvalidDataException error) { Emit("slot-identity-rejected", new { error = error.ToString() }); return Fence(WorldStateTerminal.Identity); }
    }

    private bool Retry(ulong started, ref uint attempts, string source)
    {
        ulong waited = Now() - started; _maximumRetryMilliseconds = Math.Max(_maximumRetryMilliseconds, waited);
        if (waited >= RetryMilliseconds)
        {
            ++_retryExpiries; Emit("fresh-read-expired", new { source, waitedMilliseconds = waited,
                cachedStatusAccepted = false, inputAppliedOrAcknowledged = false });
            return Fence(WorldStateTerminal.ActorLeaseExpired);
        }
        if (attempts == 0) ++_contendedReads; ++attempts; ++_retries;
        if (attempts <= 8) Thread.Yield(); else Thread.Sleep(1); return true;
    }

    private bool RecoveredWithinDeadline(ulong started, uint attempts, string source)
    {
        if (attempts == 0) return true;
        ulong waited = Now() - started; _maximumRetryMilliseconds = Math.Max(_maximumRetryMilliseconds, waited);
        if (waited >= RetryMilliseconds)
        {
            ++_retryExpiries; Emit("fresh-read-expired", new { source, waitedMilliseconds = waited,
                cachedStatusAccepted = false, inputAppliedOrAcknowledged = false });
            return Fence(WorldStateTerminal.ActorLeaseExpired);
        }
        return true;
    }

    private static bool CurrentDelivery(WorldStateBridgeStatus status, WorldStateDelivery delivery) =>
        (status.Phase is WorldStatePhase.Connected or WorldStatePhase.Ready) && status.SyncEpoch == delivery.SyncEpoch &&
        status.ConnectionToken == delivery.ConnectionToken && status.ReceivedRevision == delivery.Snapshot.Revision;
    private void DiscardPendingIfEpochChanged(WorldStateBridgeStatus status)
    {
        if (_pending is not null && (status.SyncEpoch != _pending.SyncEpoch || status.ReceivedRevision != _pending.Snapshot.Revision))
        { _pending = null; ++_discarded; }
    }
    private static bool ValidPrefix(WorldStateSnapshotPrefix value) => value.Publication != 0 && (value.Publication & 1) == 0 &&
        value.Bytes is >= WorldStateContract.SnapshotHeaderBytes and <= WorldStateContract.MaximumSnapshotBytes &&
        value.Revision != 0 && value.NativeTick != 0 && (value.Flags & ~WorldStateContract.NativePaused) == 0 &&
        value.Reserved0 == 0 && value.Reserved1 == 0 && value.Reserved2 == 0 && value.Reserved3 == 0 && value.Reserved4 == 0;
    private void ValidateHeader()
    {
        WorldStateHeader value = ReadPlain<WorldStateHeader>(0);
        Require(value.Magic == WorldStateContract.Magic && value.Version == WorldStateContract.Version &&
            value.Bytes == WorldStateContract.SharedBytes && value.MaximumSnapshotBytes == WorldStateContract.MaximumSnapshotBytes &&
            value.Nonce == _identity.Nonce && value.OwnerPid == _identity.OwnerPid && value.Role == _identity.Role &&
            value.OwnerCreationFileTime == _identity.OwnerCreationFileTime && value.SessionId == _identity.SessionId &&
            value.ScenarioId == _identity.ScenarioId && value.LevelEpoch == _identity.LevelEpoch && value.Generation == _identity.Generation,
            "WorldState1 immutable mapping identity changed.");
    }
    private void ValidateBridgeOwner() => Require(CreationFileTime(_identity.OwnerPid) == _identity.OwnerCreationFileTime,
        "WorldState1 bridge creation identity changed.");
    private bool TryReadCoherent<T>(int offset, out T value) where T : unmanaged
    {
        for (int attempt = 0; attempt < 3; ++attempt)
        {
            uint before = Acquire(offset); if ((before & 1) != 0) continue;
            Thread.MemoryBarrier(); value = ReadPlain<T>(offset); Thread.MemoryBarrier();
            if (before == Acquire(offset)) return true;
        }
        value = default; return false;
    }
    private bool PublishActor(bool claim = false)
    {
        ValidateHeader(); uint before = Acquire(WorldStateContract.ActorOffset);
        if (before != _actor.Publication || (before & 1) != 0 || before > 0xFFFFFFFCu) return false;
        if (!claim)
        {
            WorldStateActorStatus actual = ReadPlain<WorldStateActorStatus>(WorldStateContract.ActorOffset);
            if (actual.ParticipantPid != _actor.ParticipantPid || actual.ParticipantCreationFileTime != _actor.ParticipantCreationFileTime) return false;
        }
        if (!ClaimOdd(WorldStateContract.ActorOffset, before)) return false;
        Thread.MemoryBarrier(); WorldStateActorStatus value = _actor;
        Buffer.MemoryCopy((byte*)&value + 4, _pointer + WorldStateContract.ActorOffset + 4, 60, 60);
        Thread.MemoryBarrier(); Release(WorldStateContract.ActorOffset, before + 2); _actor.Publication = before + 2;
        ++_actorPublications; return true;
    }
    private bool Fence(WorldStateTerminal reason)
    {
        if (_terminal != WorldStateTerminal.None || _fencing) return false;
        _terminal = reason == WorldStateTerminal.None ? WorldStateTerminal.InvalidRecord : reason; _pending = null; _fencing = true;
        bool published = false;
        try { _actor.TerminalReason = _terminal; if (_pointer != null && !_disposed) published = PublishActor(); }
        catch (InvalidDataException) { }
        finally { _fencing = false; }
        Emit("fenced", new { terminal = _terminal.ToString(), actorTerminalPublished = published, cachedStatusAccepted = false }); return false;
    }
    private T ReadPlain<T>(int offset) where T : unmanaged => Unsafe.ReadUnaligned<T>(_pointer + offset);
    private uint Acquire(int offset) => unchecked((uint)Interlocked.CompareExchange(ref *(int*)(_pointer + offset), 0, 0));
    private bool ClaimOdd(int offset, uint before) => unchecked((uint)Interlocked.CompareExchange(ref *(int*)(_pointer + offset),
        unchecked((int)(before + 1)), unchecked((int)before))) == before;
    private void Release(int offset, uint value) => Interlocked.Exchange(ref *(int*)(_pointer + offset), unchecked((int)value));
    private ulong Now()
    {
        ulong now = GetTickCount64();
        if (now < _lastNow) { Fence(WorldStateTerminal.Clock); throw new InvalidDataException("WorldState1 local monotonic clock moved backwards."); }
        _lastNow = now; return now;
    }
    private void ReleaseMapping()
    {
        if (_pointerHeld) { _view!.SafeMemoryMappedViewHandle.ReleasePointer(); _pointerHeld = false; }
        _pointer = null; _view?.Dispose(); _view = null; _mapping?.Dispose(); _mapping = null;
    }
    private static ulong CreationFileTime(uint pid)
    {
        IntPtr process = OpenProcess(0x1000, false, pid);
        if (process == IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error(), "Cannot query WorldState1 process identity.");
        try
        {
            if (!GetProcessTimes(process, out NativeFileTime creation, out _, out _, out _) || !GetExitCodeProcess(process, out uint code))
                throw new Win32Exception(Marshal.GetLastWin32Error(), "Cannot read WorldState1 process creation time.");
            if (code != 259) throw new OwnerExitedException("WorldState1 process owner is no longer live.");
            return ((ulong)creation.High << 32) | creation.Low;
        }
        finally { _ = CloseHandle(process); }
    }
    private void EnsureThread() => Require(Environment.CurrentManagedThreadId == _ownerThread, "WorldState1 IPC has one synchronous ActorProbe owner thread.");
    private sealed class OwnerExitedException(string message) : IOException(message);
    private static bool KnownTerminal(WorldStateTerminal value) => (uint)value <= 11;
    private void Emit(string name, object data) => _log?.Invoke(new { @event = "world-state-client", name, data });
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidDataException(message); }
    [StructLayout(LayoutKind.Sequential)] private struct NativeFileTime { internal uint Low, High; }
    [DllImport("kernel32.dll", SetLastError = true)] private static extern IntPtr OpenProcess(uint access, [MarshalAs(UnmanagedType.Bool)] bool inherit, uint pid);
    [DllImport("kernel32.dll", SetLastError = true)] [return: MarshalAs(UnmanagedType.Bool)] private static extern bool GetProcessTimes(IntPtr process, out NativeFileTime creation, out NativeFileTime exit, out NativeFileTime kernel, out NativeFileTime user);
    [DllImport("kernel32.dll", SetLastError = true)] [return: MarshalAs(UnmanagedType.Bool)] private static extern bool GetExitCodeProcess(IntPtr process, out uint code);
    [DllImport("kernel32.dll")] [return: MarshalAs(UnmanagedType.Bool)] private static extern bool CloseHandle(IntPtr handle);
    [DllImport("kernel32.dll")] private static extern ulong GetTickCount64();
}
