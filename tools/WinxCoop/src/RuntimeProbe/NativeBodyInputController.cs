using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>Our input envelope, not a native command layout or network packet.</summary>
internal readonly record struct NativeBodyInputIntent(
    uint Generation, uint Sequence, float Magnitude, float RelativeDirection,
    float ReferenceHeading, bool JumpHeld, long ObservedAtMilliseconds, bool OrdinaryAttackHeld = false);

internal enum NativeBodyInputAvailability { Ready, Paused, Dead, Disconnected }
internal enum NativeBodyInputEnqueueResult
{
    Accepted, SuppressedBeforeReady, Full, WrongGeneration, SequenceMismatch,
    InvalidValue, InvalidTimestamp, Unavailable, Retired, SequenceExhausted
}
internal enum NativeBodyInputStep { Neutral, Applied, Held, Paused, TimedOut, Dead, Disconnected, Faulted, Retired }

/// <summary>
/// Unintegrated E4 boundary for one already initialized, separately owned body.
/// One debugger-owner thread queues and consumes ordered movement/jump levels.
/// This does not qualify a device, native scheduling, gameplay or guest delivery.
/// </summary>
internal sealed class NativeBodyInputController
{
    private const uint CommandTable = 0x703090;
    private const int CommandBytes = 0x74;
    private const float MaximumRadians = 3.1415927410125732421875f;
    private readonly bool _allowOrdinaryAttack;
    private ulong _ordinaryAttackWrites;
    private readonly DebuggerHost _host;
    private readonly uint _command, _generation;
    private readonly int _ownerThread, _capacity, _timeoutMilliseconds;
    private readonly Action _verifyOwnership;
    private readonly Func<uint, int, byte[]> _read;
    private readonly Action<uint, byte[]> _write;
    private readonly Func<long> _clock;
    private readonly Action<object>? _log;
    private readonly Queue<NativeBodyInputIntent> _pending;
    private NativeBodyInputIntent _current;
    private bool _haveCurrent, _paused, _dead, _disconnected, _faulted, _retired;
    private uint _lastEnqueued, _lastApplied, _lastDiscarded;
    private long _lastClock = -1, _lastObservation = -1, _freshSince;
    private ulong _accepted, _suppressed, _full, _rejected, _applied, _held, _timeouts, _neutralizations, _writeCalls;
    private NativeBodyInputStep _lastStep;

    /// <param name="verifyOwnership">
    /// Must check the exact live instance/Character/Move/BloomFSM/private-command
    /// graph, generation, and absence of in-flight native consumers/calls. It is
    /// invoked only at a stopped native boundary, before any memory operation.
    /// </param>
    /// <param name="clock">
    /// Host-local nonnegative monotonic milliseconds. Intent timestamps must use
    /// this same clock domain; an untranslated peer clock is invalid here.
    /// </param>
    internal NativeBodyInputController(DebuggerHost host, uint command, uint generation,
        int capacity, int timeoutMilliseconds, Action verifyOwnership,
        Func<uint, int, byte[]> read, Action<uint, byte[]> write, Func<long> clock,
        Action<object>? log = null, bool allowOrdinaryAttack = false)
    {
        if (command < 0x10000 || command > uint.MaxValue - CommandBytes || generation == 0 ||
            capacity is < 1 or > 256 || timeoutMilliseconds is < 1 or > 10000)
            throw new ArgumentOutOfRangeException(nameof(generation), "Invalid bounded body-input configuration.");
        _allowOrdinaryAttack = allowOrdinaryAttack;
        _host = host; _command = command; _generation = generation; _capacity = capacity;
        _timeoutMilliseconds = timeoutMilliseconds; _verifyOwnership = verifyOwnership;
        _read = read; _write = write; _clock = clock; _log = log;
        _ownerThread = Environment.CurrentManagedThreadId;
        _pending = new(capacity);
        EnsureStopped(); _verifyOwnership(); _ = ReadCommand(); _ = Now();
    }

    internal bool Retired => _retired;
    internal int PendingCount => _pending.Count;
    internal object Summary => new
    {
        generation = _generation, capacity = _capacity, timeoutMilliseconds = _timeoutMilliseconds,
        pending = _pending.Count, lastEnqueuedSequence = _lastEnqueued, lastAppliedSequence = _lastApplied,
        lastDiscardedSequence = _lastDiscarded, paused = _paused, dead = _dead,
        disconnected = _disconnected, faulted = _faulted, retired = _retired,
        accepted = _accepted, suppressedBeforeReady = _suppressed, backpressure = _full,
        rejected = _rejected, applied = _applied, held = _held, timeouts = _timeouts,
        neutralizations = _neutralizations, sparseWriteCalls = _writeCalls, lastStep = _lastStep.ToString(),
        ordinaryAttackAllowed = _allowOrdinaryAttack, ordinaryAttackWrites = _ordinaryAttackWrites,
        attackFieldsWritten = _ordinaryAttackWrites != 0, chargedAttackFieldWritten = false,
        nativePointersDeliveredByIntent = false, guestGameplayQualified = false
    };

    /// <summary>
    /// Owner-thread only; no game-memory access. Full does not consume a sequence,
    /// so the caller retains and retries that exact sample. Accepted samples are
    /// never coalesced. Pause/freshness suppression consumes/records sequences.
    /// A same-generation discontinuity or malformed sample fences the controller.
    /// </summary>
    internal NativeBodyInputEnqueueResult Enqueue(in NativeBodyInputIntent intent)
    {
        EnsureThread();
        if (_retired) return NativeBodyInputEnqueueResult.Retired;
        if (_dead || _disconnected || _faulted) return NativeBodyInputEnqueueResult.Unavailable;
        if (intent.Generation != _generation) { ++_rejected; return NativeBodyInputEnqueueResult.WrongGeneration; }
        if (_lastEnqueued == uint.MaxValue) return Fence(NativeBodyInputEnqueueResult.SequenceExhausted);
        if (intent.Sequence != _lastEnqueued + 1) return Fence(NativeBodyInputEnqueueResult.SequenceMismatch);
        if (intent.OrdinaryAttackHeld && !_allowOrdinaryAttack ||
            !float.IsFinite(intent.Magnitude) || intent.Magnitude is < 0 or > 1 ||
            !Angle(intent.RelativeDirection) || !Angle(intent.ReferenceHeading))
            return Fence(NativeBodyInputEnqueueResult.InvalidValue);
        long now = Now();
        if (intent.ObservedAtMilliseconds < 0 || intent.ObservedAtMilliseconds > now ||
            intent.ObservedAtMilliseconds < _lastObservation)
            return Fence(NativeBodyInputEnqueueResult.InvalidTimestamp);
        if (!_paused && _pending.Count == _capacity) { ++_full; return NativeBodyInputEnqueueResult.Full; }
        _lastEnqueued = intent.Sequence; _lastObservation = intent.ObservedAtMilliseconds;
        if (_paused || intent.ObservedAtMilliseconds < _freshSince)
        {
            _lastDiscarded = intent.Sequence; ++_suppressed;
            return NativeBodyInputEnqueueResult.SuppressedBeforeReady;
        }
        _pending.Enqueue(intent); ++_accepted;
        return NativeBodyInputEnqueueResult.Accepted;
    }

    /// <summary>
    /// Called at original main-thread WorldRET only. Consumes at most one FIFO
    /// sample; an empty queue may hold the last level until its freshness limit.
    /// Timeout discards the queued history and requires a fresh observation.
    /// Ready after pause never replays samples observed before that resume.
    /// </summary>
    internal NativeBodyInputStep StepAtWorldReturn(uint threadId, ref Win32Native.X86Context context,
        NativeBodyInputAvailability availability)
    {
        EnsureThread();
        if (_retired) return SetStep(NativeBodyInputStep.Retired);
        EnsureWorldReturn(threadId, context); _verifyOwnership();
        long now = Now(); byte[] before = ReadCommand();
        if (before[0x5F] != 0 || availability == NativeBodyInputAvailability.Dead) _dead = true;
        if (availability == NativeBodyInputAvailability.Disconnected) _disconnected = true;
        if (_dead || _disconnected || _faulted)
        {
            Discard(); Neutralize(before); ++_neutralizations;
            return SetStep(_dead ? NativeBodyInputStep.Dead : _disconnected ? NativeBodyInputStep.Disconnected : NativeBodyInputStep.Faulted);
        }
        if (availability == NativeBodyInputAvailability.Paused)
        {
            _paused = true; Discard(); Neutralize(before); ++_neutralizations;
            return SetStep(NativeBodyInputStep.Paused);
        }
        if (availability != NativeBodyInputAvailability.Ready)
            throw new InvalidDataException("Unknown body-input availability.");
        if (_paused)
        {
            _paused = false; _freshSince = now; Discard(); Neutralize(before); ++_neutralizations;
            return SetStep(NativeBodyInputStep.Neutral);
        }
        if ((_pending.Count != 0 && Expired(_pending.Peek(), now)) ||
            (_pending.Count == 0 && _haveCurrent && Expired(_current, now)))
        {
            Discard(); _freshSince = now; Neutralize(before); ++_timeouts; ++_neutralizations;
            return SetStep(NativeBodyInputStep.TimedOut);
        }
        if (_pending.Count != 0)
        {
            _current = _pending.Dequeue(); _haveCurrent = true; _lastApplied = _current.Sequence;
            Apply(before, _current); ++_applied;
            return SetStep(NativeBodyInputStep.Applied);
        }
        if (_haveCurrent)
        {
            Apply(before, _current); ++_held;
            return SetStep(NativeBodyInputStep.Held);
        }
        Neutralize(before);
        return SetStep(NativeBodyInputStep.Neutral);
    }

    /// <summary>Owner-thread notification; the next safe step neutralizes memory.</summary>
    internal void Disconnect()
    {
        EnsureThread();
        if (_retired) return;
        _disconnected = true; Discard();
    }

    /// <summary>
    /// Must run while the body is still alive. Normal retirement is at WorldRET.
    /// For a native pre-unload entry the caller must additionally supply its
    /// exact signature/receiver/phase verifier. After success no method reads or
    /// writes historical game memory, including repeated retirement/step calls.
    /// </summary>
    internal void RetireBeforeDelete(uint threadId, ref Win32Native.X86Context context,
        Action<uint, Win32Native.X86Context>? verifyPreUnloadBoundary = null)
    {
        EnsureThread();
        if (_retired) return;
        EnsureStopped();
        if (threadId != _host.MainThreadId) throw new InvalidDataException("Body input retirement is on the wrong native thread.");
        if (context.Eip != NativeCallDispatcher.WorldReturn)
        {
            if (verifyPreUnloadBoundary is null) throw new InvalidDataException("Unverified body input pre-unload boundary.");
            verifyPreUnloadBoundary(threadId, context);
        }
        _verifyOwnership(); byte[] before = ReadCommand();
        Discard(); Neutralize(before); ++_neutralizations; _retired = true;
        SetStep(NativeBodyInputStep.Retired);
    }

    private NativeBodyInputEnqueueResult Fence(NativeBodyInputEnqueueResult result)
    {
        ++_rejected; _faulted = true; Discard();
        _log?.Invoke(new { @event = "body-input-fenced", generation = _generation, reason = result.ToString(),
            neutralizationPendingAtSafeBoundary = true });
        return result;
    }
    private void Discard()
    {
        if (_pending.Count != 0) _lastDiscarded = _pending.Last().Sequence;
        _pending.Clear(); _haveCurrent = false;
    }
    private bool Expired(in NativeBodyInputIntent intent, long now) => now - intent.ObservedAtMilliseconds > _timeoutMilliseconds;
    private static bool Angle(float value) => float.IsFinite(value) && value is >= -MaximumRadians and <= MaximumRadians;
    private long Now()
    {
        long now = _clock();
        if (now < 0 || now < _lastClock) throw new InvalidDataException("Body input clock regressed or is negative.");
        _lastClock = now; return now;
    }
    private byte[] ReadCommand()
    {
        byte[] bytes = _read(_command, CommandBytes);
        if (bytes.Length != CommandBytes || BitConverter.ToUInt32(bytes, 0) != CommandTable)
            throw new InvalidDataException("Body input private command identity changed.");
        return bytes;
    }
    private void Apply(byte[] before, in NativeBodyInputIntent intent)
    {
        var fields = new List<(int Offset, byte[] Bytes)>
        {
            (4, BitConverter.GetBytes(intent.Magnitude)), (8, BitConverter.GetBytes(intent.RelativeDirection)),
            (0x14, BitConverter.GetBytes(intent.ReferenceHeading)), (0x1A, [(byte)(intent.JumpHeld ? 1 : 0)])
        };
        if (_allowOrdinaryAttack) fields.Add((0x20, [(byte)(intent.OrdinaryAttackHeld ? 1 : 0)]));
        WriteFields(before, fields.ToArray());
    }
    // Native heading/direction can change during hurt/turn feedback. Releasing
    // locomotion must preserve those current angles, not restore an old intent.
    private void Neutralize(byte[] before)
    {
        var fields = new List<(int Offset, byte[] Bytes)> { (4, BitConverter.GetBytes(0f)), (0x1A, [0]) };
        if (_allowOrdinaryAttack) fields.Add((0x20, [0]));
        WriteFields(before, fields.ToArray());
    }
    private void WriteFields(byte[] before, (int Offset, byte[] Bytes)[] fields)
    {
        foreach (var field in fields)
            if (!before.AsSpan(field.Offset, field.Bytes.Length).SequenceEqual(field.Bytes))
            { _write(checked(_command + (uint)field.Offset), field.Bytes); ++_writeCalls; if (field.Offset == 0x20) ++_ordinaryAttackWrites; }
        byte[] after = ReadCommand();
        foreach (var field in fields)
            if (!after.AsSpan(field.Offset, field.Bytes.Length).SequenceEqual(field.Bytes))
                throw new InvalidDataException("Body input sparse field readback failed.");
        for (int offset = 0; offset < CommandBytes; ++offset)
            if (!fields.Any(field => offset >= field.Offset && offset < field.Offset + field.Bytes.Length) && before[offset] != after[offset])
                throw new InvalidDataException("Body input changed an unowned command byte.");
    }
    private NativeBodyInputStep SetStep(NativeBodyInputStep step)
    {
        if (_lastStep != step && step is not (NativeBodyInputStep.Applied or NativeBodyInputStep.Held))
            _log?.Invoke(new { @event = "body-input-state", generation = _generation, previous = _lastStep.ToString(), current = step.ToString() });
        _lastStep = step; return step;
    }
    private void EnsureThread()
    {
        if (Environment.CurrentManagedThreadId != _ownerThread)
            throw new InvalidOperationException("Body input requires its single debugger-owner thread.");
    }
    private void EnsureStopped()
    {
        EnsureThread();
        if (!_host.IsAtDebugStop || _host.ProcessHandle == IntPtr.Zero || _host.MainThreadId == 0)
            throw new InvalidOperationException("Body input requires a stopped owned game process.");
    }
    private void EnsureWorldReturn(uint threadId, in Win32Native.X86Context context)
    {
        EnsureStopped();
        if (threadId != _host.MainThreadId || context.Eip != NativeCallDispatcher.WorldReturn)
            throw new InvalidDataException("Body input writes require the original main-thread WorldRET.");
    }
}
