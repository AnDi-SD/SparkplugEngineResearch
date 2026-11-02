using System.ComponentModel;
using System.Reflection.PortableExecutable;
using System.Security.Cryptography;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// Read-only typed stock InputRET capture for Alfea28/context3. It never polls
/// a device, injects a key, calls a tick, or writes a native command.
/// </summary>
internal sealed class NativeInputCapture
{
    internal const uint InputReturn = 0x4E2D3E;
    private const int CommandBytes = 0x74, LogLimit = 256, MaximumReadyReturnGapMilliseconds = 2000;
    private readonly int _heartbeatMilliseconds;
    private readonly bool _ordinaryAttack;
    private readonly bool _traceEdges;
    private readonly bool _limitContinuousChanges;
    private uint _lastSampleButtons;
    private bool _lastSampleMoving;
    private ulong _continuousChangesDeferred;
    private const uint EdgeLogLimit = 4096;
    private uint _extraEdgeLogs, _extraTransitionLogs, _transitionWindowRemaining;
    private readonly uint _level, _playingTable;
    private const int RawCapacity = 64, MaximumRawAgeMilliseconds = 500, MaximumFlushPerReturn = 8;
    private readonly DebuggerHost _host;
    private readonly IntPtr _process;
    private readonly uint _pid;
    private readonly int _ownerThread;
    private readonly DiagnosticInputClient _client;
    private readonly Action<object> _log;
    private StockTuple? _tuple;
    private DiagnosticBridgeStatus _observedGrant;
    private bool _haveObservedGrant, _armed, _retired, _failed, _waitingForNewEpoch, _everQualified;
    private uint _epoch, _blockedEpoch, _sourceSequence, _logs;
    private ulong _nativeReturns, _captureOrdinal, _published, _edges, _heartbeats, _unarmed, _noGrant;
    private long _lastPublicationMilliseconds = -1, _lastRawCaptureMilliseconds = -1, _lastEligibleReadyMilliseconds = -1;
    private readonly Queue<RawCapture> _raw = [];
    private readonly Dictionary<string, ulong> _rawDiscardReasons = [];
    private ulong _rawQueued, _rawDiscarded, _creditWaits;
    private int _maximumRawPending, _maximumFlushedPerReturn;
    private long _oldestRawAgeMilliseconds, _maximumRawAgeMilliseconds, _maximumEligibleReadyGapMilliseconds;
    private readonly record struct RawCapture(uint InputEpoch, ulong ConnectionToken, ulong CaptureOrdinal,
        ulong CaptureMilliseconds, float Magnitude, float RelativeDirection, float ReferenceHeading, uint Buttons, bool Edge);
    private byte[]? _lastFingerprint;
    private string _failure = "";

    private sealed record StockTuple(uint Input, uint Command, uint Device, uint Profile, uint Instance, uint Root, uint Scene,
        uint Flow, uint Timer, uint Head, uint FirstLink, uint LastLink, uint Character, uint Move, uint Bloom, uint Animator);

    internal NativeInputCapture(DebuggerHost host, PEReader image, DiagnosticInputClient client, Action<object> log,
        bool ordinaryAttack = false, int heartbeatMilliseconds = 50, bool traceEdges = false,
        bool limitContinuousChanges = false)
    {
        ArgumentNullException.ThrowIfNull(host); ArgumentNullException.ThrowIfNull(image);
        ArgumentNullException.ThrowIfNull(client); ArgumentNullException.ThrowIfNull(log);
        Require(heartbeatMilliseconds is >= 16 and <= 100, "Native capture heartbeat must be16..100ms.");
        _heartbeatMilliseconds = heartbeatMilliseconds; _ordinaryAttack = ordinaryAttack;
        _traceEdges = traceEdges;
        _limitContinuousChanges = limitContinuousChanges;
        _level = ordinaryAttack ? 47u : 28u; _playingTable = ordinaryAttack ? 0x7105C8u : 0x710874u;
        if (!host.IsAtDebugStop || host.ProcessHandle == IntPtr.Zero || host.ProcessPid == 0 || host.ImageBase != 0x400000 ||
            host.MainThreadId == 0 || client.Identity.Role != DiagnosticInputRole.Guest || client.TerminalOrRetired)
            throw new InvalidOperationException("Native capture requires its owned pinned game stop and live Guest IPC endpoint.");
        Require(Convert.ToHexString(SHA256.HashData(image.GetEntireImage().GetContent().ToArray())) ==
            "3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F" &&
            image.GetSectionData(checked((int)(InputReturn - 0x400000))).GetContent(0, 1)[0] == 0xC3,
            "Native input capture requires the exact original PE/UserInput C3.");
        _host = host; _process = host.ProcessHandle; _pid = host.ProcessPid;
        _ownerThread = Environment.CurrentManagedThreadId; _client = client; _log = log;
    }

    internal bool Retired => _retired;
    internal bool Failed => _failed;
    internal object Summary => new
    {
        source = "actual-stock-UserInput-RET", nativeReturn = $"0x{InputReturn:X8}", qualifiedLevel = _level, qualifiedDeviceContext = 3,
        retired = _retired, failed = _failed, failure = _failure, nativeReturns = _nativeReturns,
        captureOrdinal = _captureOrdinal, published = _published, edgeRecords = _edges, heartbeatRecords = _heartbeats,
        noGrantObservations = _noGrant, neutralJumpRearmWaits = _unarmed, armed = _armed,
        inputEpoch = _epoch, lastSourceSequence = _sourceSequence, logSamples = _logs, logSampleLimit = LogLimit,
        fingerprintFloatOffsets = new[] { 4, 8, 0x14 }, fingerprintJumpOffset = 0x1A,
        ordinaryAttackCaptureEnabled = _ordinaryAttack, fingerprintOrdinaryAttackOffset = _ordinaryAttack ? 0x20 : (int?)null,
        heartbeatTargetMilliseconds = _heartbeatMilliseconds, maximumReadyReturnGapMilliseconds = MaximumReadyReturnGapMilliseconds,
        unchangedSamplesContinueAfterLogLimit = true,
        extraEdgeTraceEnabled = _traceEdges, extraEdgeTraceSamples = _extraEdgeLogs,
        extraTransitionTraceSamples = _extraTransitionLogs, transitionWindowRecords = 16,
        extraEdgeTraceLimit = EdgeLogLimit,
        continuousChangeCadenceLimited = _limitContinuousChanges,
        continuousChangesDeferredBeforeSampling = _continuousChangesDeferred,
        buttonAndMovementStartStopChangesRemainImmediate = true,
        rawQueued = _rawQueued, rawPending = _raw.Count, maximumRawPending = _maximumRawPending,
        rawDiscarded = _rawDiscarded, rawDiscardReasons = _rawDiscardReasons.ToDictionary(x => x.Key, x => x.Value),
        creditWaitObservations = _creditWaits, oldestRawAgeMilliseconds = _oldestRawAgeMilliseconds,
        lastRawCaptureMilliseconds = _lastRawCaptureMilliseconds, lastPublicationMilliseconds = _lastPublicationMilliseconds,
        maximumObservedRawAgeMilliseconds = _maximumRawAgeMilliseconds, rawCapacity = RawCapacity,
        maximumRawAgeMilliseconds = MaximumRawAgeMilliseconds, maximumFlushPerReturn = MaximumFlushPerReturn,
        maximumObservedFlushPerReturn = _maximumFlushedPerReturn,
        maximumEligibleReadyReturnGapMilliseconds = _maximumEligibleReadyGapMilliseconds,
        cadenceMeasuresActualReadyInputReturns = true, heartbeatMeasuresRawCapture = true,
        rawCaptureClockUsesOnlyLocalAge = true, wireSequenceAndGrantAssignedOnlyAtPublication = true,
        nativeCommandWrites = 0, syntheticKeys = 0, nativeTicksAdded = 0,
        captureClockIsDiagnosticOnly = true, hostIngressAssignedByCapture = false,
        ipc = _client.Summary, twoGuestGamesOrLANQualified = false
    };

    // Caller composes this with the existing static UserInput-return observer.
    // Raw records are immutable observations under an already observed epoch.
    // Only publication binds a wire sequence and current immutable credit grant.
    internal void ObserveAtInputReturn(uint tid, uint address, ref Win32Native.X86Context context, bool mediaVerified)
    {
        EnsureOwner(); if (_retired || _failed) return;
        if (_client.TerminalOrRetired)
        { Fail("Guest IPC became terminal before the next actual InputRET.", DiagnosticInputTerminal.Transport); return; }
        EnsureStop();
        Require(tid == _host.MainThreadId && address == InputReturn && context.Eip == InputReturn && Read(InputReturn, 1)[0] == 0xC3,
            "Native capture was not called at the original main-thread UserInput RET.");
        Require(++_nativeReturns <= 1000000, "Native capture observation budget exceeded.");
        try
        {
            if (!mediaVerified) { Disarm("media-not-ready"); _client.SetAvailability(DiagnosticInputAvailability.Waiting); return; }
            if (!TryReadyTuple(out StockTuple? tuple, out DiagnosticInputAvailability unavailable))
            { Disarm("native-" + unavailable); _client.SetAvailability(unavailable); return; }
            _everQualified = true;
            bool changedOwner = _tuple is not null && !SameIdentity(_tuple, tuple!);
            if (changedOwner)
            { Fail("Native source tuple changed; a fresh mapping and generation are required.", DiagnosticInputTerminal.Identity); return; }
            _tuple = tuple;
            if (!_client.SetAvailability(DiagnosticInputAvailability.Ready))
            { Fail("Guest IPC could not publish actual native readiness.", DiagnosticInputTerminal.Transport); return; }
            DiagnosticBridgeStatus previousGrant = _observedGrant; bool hadGrant = _haveObservedGrant;
            if (!_client.TryReadBridgeStatus(out DiagnosticBridgeStatus current))
            {
                if (_client.TerminalOrRetired || hadGrant)
                    Fail("Bridge control could not be read after source admission.", DiagnosticInputTerminal.NativeSource);
                return;
            }
            if (current.State != DiagnosticBridgeState.Ready)
            { Disarm("bridge-not-ready"); ++_noGrant; return; }
            if (_waitingForNewEpoch && current.InputEpoch == _blockedEpoch) { ++_noGrant; return; }
            if (_epoch != current.InputEpoch)
            {
                DiscardRaw("epoch-changed");
                _epoch = current.InputEpoch; _sourceSequence = 0; _armed = false; _lastFingerprint = null;
                _lastPublicationMilliseconds = _lastRawCaptureMilliseconds = _lastEligibleReadyMilliseconds = -1;
                _waitingForNewEpoch = false; _observedGrant = current; _haveObservedGrant = true; ++_noGrant; return;
            }
            _observedGrant = current; _haveObservedGrant = true;
            if (!hadGrant || previousGrant.InputEpoch != current.InputEpoch || previousGrant.ConnectionToken != current.ConnectionToken)
            {
                DiscardRaw("epoch-or-connection-barrier"); _armed = false; _lastFingerprint = null;
                _lastRawCaptureMilliseconds = _lastEligibleReadyMilliseconds = -1; ++_noGrant; return;
            }

            // All four scalar fields come from this actual later native return.
            byte[] command = Read(tuple!.Command, CommandBytes);
            float magnitude = BitConverter.ToSingle(command, 4), relative = BitConverter.ToSingle(command, 8), reference = BitConverter.ToSingle(command, 0x14);
            byte jump = command[0x1A];
            byte ordinary = _ordinaryAttack ? command[0x20] : (byte)0;
            uint buttons = jump | ((uint)ordinary << 1);
            Require(float.IsFinite(magnitude) && magnitude is >= 0 and <= 1 && Angle(relative) && Angle(reference) && jump <= 1 && ordinary <= 1,
                "Native movement/jump command lies outside the explicitly admitted scalar domain.");
            Require(_captureOrdinal != ulong.MaxValue, "Native capture ordinal would wrap."); ++_captureOrdinal;
            long now = DiagnosticInputClient.LocalNowMilliseconds();
            if (_lastEligibleReadyMilliseconds >= 0)
            {
                Require(now >= _lastEligibleReadyMilliseconds, "Actual native source clock moved backwards.");
                long gap = now - _lastEligibleReadyMilliseconds;
                _maximumEligibleReadyGapMilliseconds = Math.Max(_maximumEligibleReadyGapMilliseconds, gap);
                // A real frame hitch is distinct from a stale captured record.
                // Allow the diagnostic actor publication lease; keep all
                // captured-record and command freshness guards at500ms.
                if (gap > MaximumReadyReturnGapMilliseconds)
                { Fail("Actual ready native InputRET exceeded the2000ms diagnostic actor lease budget.", DiagnosticInputTerminal.NativeSource); return; }
            }
            _lastEligibleReadyMilliseconds = now;
            if (!_armed)
            {
                if (buttons != 0) { ++_unarmed; return; }
                // Queue this genuine neutral observation before any later jump.
                _armed = true; _lastFingerprint = null;
            }

            int flushBudget = MaximumFlushPerReturn;
            if (!FlushRaw(previousGrant, current, ref flushBudget, out bool creditBlocked)) return;
            byte[] fingerprint = Fingerprint(command);
            bool edge = _lastFingerprint is null || !_lastFingerprint.SequenceEqual(fingerprint);
            bool due = _lastRawCaptureMilliseconds < 0 || now - _lastRawCaptureMilliseconds >= _heartbeatMilliseconds;
            bool immediate = _lastFingerprint is null || buttons != _lastSampleButtons || (magnitude > 0) != _lastSampleMoving;
            bool captureChange = edge && (!_limitContinuousChanges || immediate || due);
            bool heartbeat = !edge && due;
            // Optional network sampling policy: observe every real InputRET,
            // select buttons/start/stop immediately and continuous direction
            // at the configured cadence. Selected raw records are never
            // overwritten/coalesced and retain the same500ms freshness limit.
            if (edge && !captureChange) ++_continuousChangesDeferred;
            if (captureChange || heartbeat)
            {
                ObserveRawAge(now);
                if (_raw.Count == RawCapacity)
                { Fail("Bounded raw native capture FIFO reached64 records; no observation was overwritten.", DiagnosticInputTerminal.Overflow); return; }
                _raw.Enqueue(new(_epoch, current.ConnectionToken, _captureOrdinal, checked((ulong)now), magnitude, relative, reference, buttons, edge));
                ++_rawQueued; _maximumRawPending = Math.Max(_maximumRawPending, _raw.Count);
                _lastRawCaptureMilliseconds = now; _lastFingerprint = fingerprint;
                _lastSampleButtons = buttons; _lastSampleMoving = magnitude > 0;
            }
            if (!creditBlocked && flushBudget > 0) _ = FlushRaw(previousGrant, current, ref flushBudget, out _);
            _maximumFlushedPerReturn = Math.Max(_maximumFlushedPerReturn, MaximumFlushPerReturn - flushBudget);
        }
        catch (Exception error) when (error is Win32Exception or InvalidDataException or OverflowException)
        {
            if (!_everQualified && _epoch == 0)
            { Disarm("startup-not-ready"); _client.SetAvailability(DiagnosticInputAvailability.Waiting); Sample("not-ready", new { reason = error.Message, nativeRecordPublished = false }); }
            else Fail(error.Message, DiagnosticInputTerminal.NativeSource);
        }
    }

    // Only actual ready InputRET callbacks call this bounded, nonblocking drain.
    // NoCredit leaves RAW observations queued; Published records are never edited.
    private bool FlushRaw(DiagnosticBridgeStatus previousGrant, DiagnosticBridgeStatus observedCurrent,
        ref int budget, out bool creditBlocked)
    {
        creditBlocked = false;
        while (_raw.Count != 0 && budget > 0)
        {
            long now = DiagnosticInputClient.LocalNowMilliseconds(); ObserveRawAge(now);
            RawCapture raw = _raw.Peek();
            if (!_client.TryReadBridgeStatus(out DiagnosticBridgeStatus admissionGrant))
            { Fail("Bridge control became unreadable while flushing captured raw input.", DiagnosticInputTerminal.Transport); return false; }
            if (admissionGrant.State != DiagnosticBridgeState.Ready || admissionGrant.InputEpoch != _epoch ||
                admissionGrant.ConnectionToken != observedCurrent.ConnectionToken)
            { Disarm("flush-bridge-unready-or-epoch-changed"); ++_noGrant; return false; }
            Require(raw.InputEpoch == _epoch && raw.ConnectionToken == admissionGrant.ConnectionToken,
                "Raw capture authority differs from the active publication epoch/connection.");
            _observedGrant = admissionGrant; _haveObservedGrant = true;
            if (_sourceSequence == uint.MaxValue)
            { Fail("Source wire sequence would wrap.", DiagnosticInputTerminal.CounterExhausted); return false; }
            if (_sourceSequence + 1 > admissionGrant.MaximumSourceSequence)
            { ++_creditWaits; creditBlocked = true; return true; }
            DiagnosticInputRecord record = new()
            {
                InputEpoch = raw.InputEpoch, SourceSequence = _sourceSequence + 1, CaptureOrdinal = raw.CaptureOrdinal,
                CaptureMilliseconds = raw.CaptureMilliseconds, HostIngressMilliseconds = 0, Magnitude = raw.Magnitude,
                RelativeDirection = raw.RelativeDirection, ReferenceHeading = raw.ReferenceHeading, Buttons = raw.Buttons,
                Grant = admissionGrant.Grant, LeaseDeadlineMilliseconds = admissionGrant.LeaseDeadlineMilliseconds
            };
            DiagnosticPublishResult result = _client.TryPublishCapture(record);
            if (result is DiagnosticPublishResult.EpochChanged or DiagnosticPublishResult.NotReady)
            { Disarm("publication-epoch-changed-or-unready"); ++_noGrant; return false; }
            if (result != DiagnosticPublishResult.Published)
            {
                AdmissionWitness("ipc-admission-" + result, previousGrant, observedCurrent, admissionGrant, raw, now);
                Fail("IPC capture admission failed: " + result,
                    result is DiagnosticPublishResult.Full or DiagnosticPublishResult.NoCredit ? DiagnosticInputTerminal.Overflow : DiagnosticInputTerminal.Transport);
                return false;
            }
            _raw.Dequeue(); --budget; _sourceSequence = record.SourceSequence;
            _maximumFlushedPerReturn = Math.Max(_maximumFlushedPerReturn, MaximumFlushPerReturn - budget);
            _lastPublicationMilliseconds = now; ++_published;
            if (raw.Edge) ++_edges; else ++_heartbeats;
            ObserveRawAge(now);
            // Keep a bounded prefix after each actual command edge. A native
            // jump may enter state18 on a later heartbeat, after the source's
            // exact press record has already completed its first interval.
            if (raw.Edge) _transitionWindowRemaining = 16;
            bool transitionTrace = !raw.Edge && _transitionWindowRemaining > 0;
            if (transitionTrace) --_transitionWindowRemaining;
            Sample("published", new { inputEpoch = raw.InputEpoch, sourceSequence = _sourceSequence, captureOrdinal = raw.CaptureOrdinal,
                captureMilliseconds = raw.CaptureMilliseconds, record.Magnitude, record.RelativeDirection, record.ReferenceHeading,
                jumpHeld = (raw.Buttons & 1) != 0, ordinaryAttackHeld = (raw.Buttons & 2) != 0,
                grant = record.Grant, leaseDeadlineHostClock = record.LeaseDeadlineMilliseconds,
                rawCaptureWasPreserved = true, boundAtActualInputReturn = _nativeReturns, rawPending = _raw.Count,
                realInputReturn = true, edge = raw.Edge, hostIngressMilliseconds = 0 }, raw.Edge, transitionTrace);
        }
        return true;
    }
    private void ObserveRawAge(long now)
    {
        if (_raw.Count == 0) { _oldestRawAgeMilliseconds = 0; return; }
        RawCapture head = _raw.Peek();
        Require(now >= 0 && (ulong)now >= head.CaptureMilliseconds, "Raw native capture age clock moved backwards.");
        _oldestRawAgeMilliseconds = checked((long)((ulong)now - head.CaptureMilliseconds));
        _maximumRawAgeMilliseconds = Math.Max(_maximumRawAgeMilliseconds, _oldestRawAgeMilliseconds);
        Require(_oldestRawAgeMilliseconds <= MaximumRawAgeMilliseconds, "Oldest raw native capture exceeded the500ms source-local bound.");
    }
    private void DiscardRaw(string reason)
    {
        ulong count = checked((ulong)_raw.Count);
        if (count != 0)
        { _rawDiscarded += count; _rawDiscardReasons[reason] = _rawDiscardReasons.GetValueOrDefault(reason) + count; _raw.Clear(); }
        _oldestRawAgeMilliseconds = 0;
    }
    internal void Retire()
    {
        EnsureOwner(); if (_retired) return;
        Disarm("retired"); _ = _client.SetAvailability(DiagnosticInputAvailability.Retired); _retired = true; _tuple = null;
        Sample("retired", new { nativeReadsAfterRetirement = false, published = _published, captureOrdinal = _captureOrdinal, rawDiscarded = _rawDiscarded });
    }
    private void Disarm(string reason)
    {
        DiscardRaw(reason);
        if (_epoch != 0) { _blockedEpoch = _epoch; _waitingForNewEpoch = true; }
        _armed = false; _haveObservedGrant = false; _lastFingerprint = null;
        _lastPublicationMilliseconds = _lastRawCaptureMilliseconds = _lastEligibleReadyMilliseconds = -1;
    }
    // One terminal diagnostic bypasses the publication sample cap. It records
    // only already captured RAW values and control snapshots; no native reads.
    private void AdmissionWitness(string reason, DiagnosticBridgeStatus previousGrant, DiagnosticBridgeStatus current,
        DiagnosticBridgeStatus admissionGrant, RawCapture raw, long now)
    {
        _log(new
        {
            @event = "native-input-capture-admission-failed", reason, bypassesSampleLimit = true,
            nativeReturns = _nativeReturns, captureOrdinal = _captureOrdinal, published = _published,
            localNowMilliseconds = now, rawPending = _raw.Count, oldestRawAgeMilliseconds = _oldestRawAgeMilliseconds,
            previousGrant = new { previousGrant.InputEpoch, previousGrant.Grant, previousGrant.MaximumSourceSequence,
                previousGrant.AcknowledgedSourceSequence, previousGrant.LeaseDeadlineMilliseconds, previousGrant.BridgeTickMilliseconds },
            currentGrant = new { current.InputEpoch, current.Grant, current.MaximumSourceSequence,
                current.AcknowledgedSourceSequence, current.LeaseDeadlineMilliseconds, current.BridgeTickMilliseconds },
            admissionGrant = new { admissionGrant.InputEpoch, admissionGrant.Grant, admissionGrant.MaximumSourceSequence,
                admissionGrant.AcknowledgedSourceSequence, admissionGrant.LeaseDeadlineMilliseconds },
            attemptedRecord = new { inputEpoch = raw.InputEpoch, sourceSequence = (ulong)_sourceSequence + 1,
                captureOrdinal = raw.CaptureOrdinal, captureMilliseconds = raw.CaptureMilliseconds, magnitude = raw.Magnitude,
                relativeDirection = raw.RelativeDirection, referenceHeading = raw.ReferenceHeading,
                jumpHeld = (raw.Buttons & 1) != 0, ordinaryAttackHeld = (raw.Buttons & 2) != 0,
                grant = admissionGrant.Grant, leaseDeadlineHostClock = admissionGrant.LeaseDeadlineMilliseconds, hostIngressMilliseconds = 0 },
            edge = raw.Edge, nativeCommandWrites = 0, capturedNativeFieldsWereNotCoalesced = true
        });
    }
    private void Fail(string reason, DiagnosticInputTerminal terminal)
    {
        if (_failed || _retired) return;
        int rawPendingBeforeFence = _raw.Count; long oldestRawAgeBeforeFence = _oldestRawAgeMilliseconds;
        _failure = reason; _failed = true; Disarm("terminal"); _client.Fence(terminal);
        _log(new { @event = "native-input-capture", name = "fenced", bypassesSampleLimit = true,
            data = new { reason, terminal = terminal.ToString(), rawPendingBeforeFence, oldestRawAgeBeforeFence,
                rawQueued = _rawQueued, published = _published, rawDiscarded = _rawDiscarded,
                nativeCommandWrites = 0, nextHostWorldMustNeutralize = true } });
    }
    private void Sample(string name, object data, bool edge = false, bool transitionTrace = false)
    {
        if (_logs < LogLimit) ++_logs;
        else if (_traceEdges && (edge || transitionTrace) && _extraEdgeLogs + _extraTransitionLogs < EdgeLogLimit)
        { if (edge) ++_extraEdgeLogs; else ++_extraTransitionLogs; }
        else return;
        _log(new { @event = "native-input-capture", name, data });
    }

    private bool TryReadyTuple(out StockTuple? result, out DiagnosticInputAvailability unavailable)
    {
        result = null; unavailable = DiagnosticInputAvailability.Paused;
        uint flow = Pointer(0x755294), level = U(flow + 0x1B0), pending = U(flow + 0x1B8), stack = U(flow + 0x1AC);
        if (level != _level) { unavailable = DiagnosticInputAvailability.Waiting; return false; }
        Require(stack <= 20, "Native source flow stack exceeds its admitted bound.");
        uint top = Pointer(flow + 0x15C + stack * 4);
        if (pending != 0 || U(top) != _playingTable || U(top + 0x10) != _level) return false;
        uint timer = Pointer(0x755298); Table(timer, 0x702F94); byte paused = Read(timer + 0x40, 1)[0];
        Require(paused <= 1, "Native GameTimer pause byte is outside0/1."); if (paused != 0) return false;
        uint input = Pointer(0x7552A0); Table(input, 0x6F5508);
        uint command = Pointer(input + 0x18), device = Pointer(input + 0x1C); Table(command, 0x703090); Table(device, 0x7116CC);
        if (U(device + 0x14) != 3 || Read(input + 0x20, 1)[0] == 0) return false;
        uint profile = Pointer(0x765AD4); Table(profile, 0x6F5A44);
        uint instance = Pointer(profile + 0x2B4); Table(instance, 0x710F80);
        uint root = Pointer(instance + 0x24); Table(root, 0x6DC4F4); uint scene = Pointer(root + 0x3C); Table(scene, 0x6E7358);
        uint head = Pointer(instance + 0x1C); Require(U(instance + 0x20) == 11, "Native input source requires exactly eleven stock player components.");
        StockTuple? cached = _tuple;
        if (cached is not null && cached.Input == input && cached.Command == command && cached.Device == device && cached.Profile == profile &&
            cached.Instance == instance && cached.Root == root && cached.Scene == scene && cached.Flow == flow && cached.Timer == timer && cached.Head == head)
        {
            Require(U(head) == cached.FirstLink && U(head + 4) == cached.LastLink && U(cached.FirstLink + 4) == head && U(cached.LastLink) == head,
                "Cached native source component list lost its sentinel links.");
            VerifyRelationships(cached); result = cached; return true;
        }
        var byTable = new Dictionary<uint, List<uint>>(); var links = new HashSet<uint>(); var objects = new HashSet<uint>();
        uint previous = head, link = Pointer(head), first = link;
        for (uint i = 0; i < 11; ++i)
        {
            Require(link != head && links.Add(link) && U(link + 4) == previous, "Native source component list is not reciprocal/distinct.");
            uint item = Pointer(link + 8), table = U(item); Require(objects.Add(item), "Native source component alias encountered.");
            if (!byTable.TryGetValue(table, out List<uint>? values)) byTable[table] = values = [];
            values.Add(item); uint next = Pointer(link); Require(U(next + 4) == link, "Native source component next/back mismatch."); previous = link; link = next;
        }
        Require(link == head && U(head + 4) == previous, "Native source component count/sentinel mismatch.");
        uint Unique(uint table) => byTable.TryGetValue(table, out List<uint>? values) && values.Count == 1 ? values[0] :
            throw new InvalidDataException("Native source typed component is not unique.");
        result = new(input, command, device, profile, instance, root, scene, flow, timer, head, first, previous,
            Unique(0x6F6438), Unique(0x6F64F8), Unique(0x6FA288), Unique(0x6F670C));
        VerifyRelationships(result); return true;
    }
    private void VerifyRelationships(StockTuple t)
    {
        Table(t.Character, 0x6F6438); Table(t.Move, 0x6F64F8); Table(t.Bloom, 0x6FA288); Table(t.Animator, 0x6F670C);
        foreach (uint entity in new[] { t.Character, t.Move, t.Bloom, t.Animator })
            Require(U(entity + 0x24) == t.Instance, "Typed source component is no longer owned by the stock player.");
        Require(U(t.Character + 0x130) == t.Command && U(t.Character + 0x124) == t.Bloom && U(t.Character + 0x128) == t.Animator &&
            U(t.Character + 0x12C) == t.Move && U(t.Move + 0x160) == t.Character && U(t.Bloom + 0x124) == t.Character &&
            U(t.Bloom + 0x12C) == t.Command && U(t.Animator + 0x124) == t.Character,
            "Native source command/Character/Move/BloomFSM/Animator ownership changed.");
    }
    private static bool SameIdentity(StockTuple a, StockTuple b) => a == b;
    private byte[] Fingerprint(byte[] command) => BuildFingerprint(command, _ordinaryAttack);
    private static byte[] BuildFingerprint(byte[] command, bool ordinaryAttack)
    {
        byte[] fingerprint = new byte[ordinaryAttack ? 14 : 13]; Array.Copy(command, 4, fingerprint, 0, 8);
        Array.Copy(command, 0x14, fingerprint, 8, 4); fingerprint[12] = command[0x1A];
        if (ordinaryAttack) fingerprint[13] = command[0x20];
        return fingerprint;
    }
    private static bool Angle(float value) => float.IsFinite(value) && value is >= -3.1415927410125732421875f and <= 3.1415927410125732421875f;
    private void Table(uint address, uint table) => Require(U(address) == table, "Native input source vtable differs from its qualified tuple.");
    private uint Pointer(uint address)
    { uint value = U(address); Require(value >= 0x10000 && (value & 3) == 0, "Native input source borrowed pointer is invalid/unaligned."); return value; }
    private uint U(uint address) => BitConverter.ToUInt32(Read(address, 4));
    private byte[] Read(uint address, int count)
    {
        EnsureStop(); Require(!_retired && address >= 0x10000 && count is > 0 and <= 0x1000 && (ulong)address + (uint)count <= uint.MaxValue,
            "Native input source read is retired or exceeds its x86 bound.");
        return Win32Native.ReadMemory(_process, address, count);
    }
    private void EnsureOwner() => Require(Environment.CurrentManagedThreadId == _ownerThread, "Native source has one debugger-owner thread.");
    private void EnsureStop()
    { EnsureOwner(); Require(_host.IsAtDebugStop && _host.ProcessHandle == _process && _host.ProcessPid == _pid && _host.ImageBase == 0x400000,
        "Native source read escaped its owned game/debug-stop tuple."); }
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidDataException(message); }
}
