using System.Reflection.PortableExecutable;
using System.Security.Cryptography;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// One original GameTimer Pause/Start pair for the closed Story3 Host startup.
/// World publication continues while the native game timer is paused. This
/// owner never changes health, AI, input or the timer field directly.
/// </summary>
internal sealed class NativeProgressionStartupHold
{
    private const uint FlowTable = 0x702D2C, PlayingTable = 0x710874, TimerTable = 0x702F94, SceneTable = 0x6E7358;
    private const uint Pause = 0x596900, Start = 0x596910;
    private readonly DebuggerHost _host;
    private readonly Action<object> _log;
    private readonly IntPtr _process;
    private readonly uint _pid, _thread, _epoch, _generation;
    private readonly int _owner;
    private readonly long _deadline, _maximumHoldMilliseconds;
    private readonly byte[] _pausePin, _startPin;
    private NativeWorldEntrySeed? _entry;
    private NativeProgressionExitObservation? _exits;
    private uint _flow, _playing, _timer, _engine, _scene, _depth, _ordinal;
    private uint _worlds, _lastWorldStop, _capturedStop, _pauseReturnStop, _startReturnStop, _pausedWorlds;
    private uint _pauseResult, _startResult;
    private long _holdDeadline;
    private bool _bound, _started, _pauseReturned, _startCalled, _startReturned, _completed, _failed, _unownedPauseLogged;
    private string _failure = "";

    internal NativeProgressionStartupHold(ProbeConfiguration config, DebuggerHost host, PEReader image, Action<object> log)
    {
        ArgumentNullException.ThrowIfNull(host); ArgumentNullException.ThrowIfNull(image); ArgumentNullException.ThrowIfNull(log);
        Require(ProbeMain.IsClosedStory3Host(config) && config.WorldEntryCaptureOrdinal == 1 &&
            config.InitialSaveSha256 is null && !config.ProgressionHostContinuationSeed &&
            config.DurationSeconds is >= 5 and <= 600 && host.IsAtDebugStop && host.ProcessHandle != IntPtr.Zero &&
            host.ProcessPid != 0 && host.MainThreadId != 0 && host.ImageBase == 0x400000,
            "Native startup hold requires only the closed initial Story3 Host cohort and its owned original stop.");
        Require(Convert.ToHexString(SHA256.HashData(image.GetEntireImage().GetContent().ToArray())) == ProbeMain.OriginalSha256,
            "Native startup hold requires the exact original PC executable.");
        _pausePin = Image(image, Pause, 9); _startPin = Image(image, Start, 9);
        Require(_pausePin.SequenceEqual(Convert.FromHexString("C6414001E9C79DEBFF")) &&
            _startPin.SequenceEqual(Convert.FromHexString("C6414000E9279FEBFF")) &&
            BitConverter.ToUInt32(Image(image, TimerTable + 0x24, 4)) == Pause &&
            BitConverter.ToUInt32(Image(image, TimerTable + 0x20, 4)) == Start,
            "Original native timer Pause/Start gateway or vtable slots differ.");
        _host = host; _log = log; _process = host.ProcessHandle; _pid = host.ProcessPid;
        _thread = host.MainThreadId; _owner = Environment.CurrentManagedThreadId;
        WorldStateIdentity identity = config.DiagnosticWorldState!.Value;
        _epoch = identity.LevelEpoch; _generation = identity.Generation;
        _deadline = checked(Environment.TickCount64 + (long)config.DurationSeconds * 1000);
        _maximumHoldMilliseconds = Math.Min(90000L, (long)config.DurationSeconds * 1000);
    }

    internal bool Started => _started;
    internal bool Completed => _completed && !_failed;
    internal object Summary => new
    {
        enabled = true, closedStory3HostOnly = true, levelEpoch = _epoch, generation = _generation,
        started = _started, completed = Completed, failed = _failed, failure = _failure, bound = _bound,
        worlds = _worlds, actualPausedWorlds = _pausedWorlds, nativeLocalEntryOrdinal = _ordinal,
        capturedDebugStop = _capturedStop, pauseReturnStop = _pauseReturnStop, startReturnStop = _startReturnStop,
        originalPauseReturned = _pauseReturned, originalStartCalled = _startCalled, originalStartReturned = _startReturned,
        pauseResultOpaque = _pauseReturned ? Hex(_pauseResult) : null,
        startResultOpaque = _startReturned ? Hex(_startResult) : null,
        nativeResultsUsedAsBool = false, nativeCallsRequested = (_started ? 1 : 0) + (_startCalled ? 1 : 0),
        nativePauseFunction = Hex(Pause), nativeStartFunction = Hex(Start), originalNoArgumentThiscalls = true,
        flow = Hex(_flow), playing = Hex(_playing), timer = Hex(_timer), scene = Hex(_scene),
        maximumHeldMilliseconds = _maximumHoldMilliseconds, waitingOnUnownedNativePause = _unownedPauseLogged && !_started,
        actualGuestNativeApplyRequiredBeforeStart = true, minimumLaterPausedWorlds = 2,
        worldPublicationSuppressedByThisOwner = false, nativeTimerFieldWritesByAdapter = 0,
        healthReadsOrWrites = 0, aiWrites = 0, inputWrites = 0, flowWrites = 0,
        forcedResumeOfUnownedPause = false, retryAfterUnknownPauseOrFailure = false,
        completionRequiresLaterWorldAndReleasedDispatcherPage = true,
        completionIsGameOverOrBothPeersGameplayAcceptance = false
    };

    internal bool StepAtWorld(uint tid, ref Win32Native.X86Context context, NativeCallDispatcher calls,
        Action resume, NativeWorldEntrySeed entry, bool nativeBodyReady, bool currentWorldApplied,
        NativeProgressionExitObservation exits)
    {
        if (_completed) return false; // Cached only; never read a later scene.
        try
        {
            EnsureStop(tid);
            Require(!_failed && context.Eip == NativeCallDispatcher.WorldReturn &&
                _host.DebugStopSequence > _lastWorldStop && ++_worlds <= NativeObservationBudget.WorldLimit,
                "Native startup hold left its fresh bounded original WorldRET.");
            _lastWorldStop = _host.DebugStopSequence;
            if (!_bound)
            {
                if (!entry.WorldQualified || !nativeBodyReady || calls.Pending) return false;
                Require(!calls.HasRetiredPage, "Startup hold caller must first release its prior dispatcher page at this later World.");
                BindCurrent(entry, exits);
            }
            VerifyCurrent(entry, exits);
            Require(Environment.TickCount64 <= _holdDeadline, "Native startup hold exceeded its finite ninety-second admission/hold interval.");
            if (calls.Pending) return false;
            Require(!calls.HasRetiredPage, "Startup hold cannot count a World or begin a call before its completed page is released.");
            byte paused = Read(_timer + 0x40, 1)[0];
            Require(paused <= 1, "Native startup timer pause byte is outside its original zero/one domain.");
            if (!_started)
            {
                if (paused != 0)
                {
                    if (!_unownedPauseLogged)
                    {
                        _unownedPauseLogged = true;
                        _log(new { @event = "progression-startup-hold-unowned-pause-wait", levelEpoch = _epoch,
                            generation = _generation, timer = Hex(_timer), originalPauseByte = paused,
                            noNativeStartRequested = true, ownershipNotInferredFromPauseByte = true });
                    }
                    return false;
                }
                VerifyPins(); _started = true;
                _log(new { @event = "progression-startup-original-pause-begin", levelEpoch = _epoch, generation = _generation,
                    nativeFunction = Hex(Pause), timer = Hex(_timer), nativeLocalEntryOrdinal = _ordinal,
                    actualEntryWorldAndOwnBodyRequired = true, originalPauseByteBefore = 0,
                    worldPublicationMustContinue = true, healthOrAiOrInputWrites = 0 });
                calls.Begin(_host, tid, ref context, Pause, _timer, [], false, value =>
                {
                    try
                    {
                        EnsureStop(_thread); VerifyCurrent(entry, exits);
                        Require(_started && !_pauseReturned && !_startCalled && Read(_timer + 0x40, 1)[0] == 1,
                            "Original native Pause did not return once with the same owned timer paused.");
                        _pauseResult = value; _pauseReturned = true; _pauseReturnStop = _host.DebugStopSequence;
                        _log(new { @event = "progression-startup-original-pause-returned", levelEpoch = _epoch,
                            generation = _generation, resultOpaque = Hex(value), nativePauseByteOne = true,
                            dispatcherPageStillRetired = true, guestApplyNotYetClaimed = true });
                        resume();
                    }
                    catch (Exception error) { Fail(error); throw; }
                });
                return true;
            }
            if (!_startCalled)
            {
                Require(_pauseReturned && _host.DebugStopSequence > _pauseReturnStop && paused == 1,
                    "Owned startup pause changed or lacks its actual returned call and later paused World.");
                ++_pausedWorlds;
                if (_pausedWorlds < 2 || !currentWorldApplied) return false;
                VerifyPins(); _startCalled = true;
                _log(new { @event = "progression-startup-original-start-begin", levelEpoch = _epoch, generation = _generation,
                    nativeFunction = Hex(Start), timer = Hex(_timer), actualLaterPausedWorlds = _pausedWorlds,
                    actualGuestNativeWorldApplyObserved = true, originalPauseByteBefore = 1,
                    sameOriginalEntryAndSceneRequired = true, healthOrAiOrInputWrites = 0 });
                calls.Begin(_host, tid, ref context, Start, _timer, [], false, value =>
                {
                    try
                    {
                        EnsureStop(_thread); VerifyCurrent(entry, exits);
                        Require(_pauseReturned && _startCalled && !_startReturned && Read(_timer + 0x40, 1)[0] == 0,
                            "Original native Start did not return once with the same owned timer running.");
                        _startResult = value; _startReturned = true; _startReturnStop = _host.DebugStopSequence;
                        _log(new { @event = "progression-startup-original-start-returned", levelEpoch = _epoch,
                            generation = _generation, resultOpaque = Hex(value), nativePauseByteZero = true,
                            dispatcherPageStillRetired = true, laterWorldCompletionStillRequired = true });
                        resume();
                    }
                    catch (Exception error) { Fail(error); throw; }
                });
                return true;
            }
            Require(_startReturned && _host.DebugStopSequence > _startReturnStop && paused == 0 && _pausedWorlds >= 2,
                "Startup hold completion lacks a later running World after the original Start and its page release.");
            _completed = true;
            _log(new { @event = "progression-startup-native-hold-completed", levelEpoch = _epoch, generation = _generation,
                nativeLocalEntryOrdinal = _ordinal, actualPausedWorlds = _pausedWorlds,
                originalPauseAndStartReturned = true, laterWorldDispatcherPageReleased = true,
                sameEntryAndTypedOriginalScene = true, noOriginalExitObserved = true,
                originalPauseByteNow = 0, healthOrAiOrInputOrTimerFieldWrites = 0,
                worldPublicationContinuedByCaller = true, gameplayOrGameOverAcceptanceClaimed = false });
            return false;
        }
        catch (Exception error) { Fail(error); throw; }
    }

    private void BindCurrent(NativeWorldEntrySeed entry, NativeProgressionExitObservation exits)
    {
        Require(entry.WorldQualified && entry.StoryLevel3Authorized && !entry.Retired && !entry.Failed &&
            !entry.HasPendingOriginalEntry && entry.Level == 3 && entry.NativeLocalEntryOrdinal == 1 &&
            exits.Healthy && !exits.HasPending && exits.ActualOriginalExitReturns == 0,
            "Startup hold cannot bind an unqualified, changed or already exited Story3 entry.");
        WorldEntrySeedState seed = WorldEntrySeedState.Unpack(entry.GetSeed());
        Require(seed.Level == 3 && seed.LevelEpoch == _epoch && seed.Generation == _generation && seed.HostEntryOrdinal == 1,
            "Startup hold actual entry belongs to another epoch/generation or original local ordinal.");
        _entry = entry; _exits = exits; _ordinal = entry.NativeLocalEntryOrdinal;
        _flow = Pointer(0x755294); _timer = Pointer(0x755298); _engine = Pointer(0x755274);
        Require(U(_flow) == FlowTable, "Startup hold initial native Flow type differs.");
        _depth = U(_flow + 0x1AC); Require(_depth < 20, "Startup hold initial Flow stack exceeds its original bound.");
        _playing = Pointer(_flow + 0x15C + _depth * 4); _scene = Pointer(_engine + 0x18);
        _capturedStop = _host.DebugStopSequence; _holdDeadline = checked(Environment.TickCount64 + _maximumHoldMilliseconds);
        _bound = true;
        VerifyCurrent(entry, exits);
    }

    private void VerifyCurrent(NativeWorldEntrySeed entry, NativeProgressionExitObservation exits)
    {
        Require(_bound && ReferenceEquals(entry, _entry) && ReferenceEquals(exits, _exits) &&
            entry.WorldQualified && entry.StoryLevel3Authorized && !entry.Retired && !entry.Failed &&
            !entry.HasPendingOriginalEntry && entry.Level == 3 && entry.NativeLocalEntryOrdinal == _ordinal &&
            exits.Healthy && !exits.HasPending && exits.ActualOriginalExitReturns == 0,
            "Startup hold lost its initial entry scope or observed an original Exit before completion.");
        // Only after checking the process-local original Exit observer may these
        // still-current scene owners be followed. Nothing is read after completion.
        Require(Pointer(0x755294) == _flow && U(_flow) == FlowTable && U(_flow + 0x1AC) == _depth &&
            U(_flow + 0x1B0) == 3 && U(_flow + 0x1B8) == 0 &&
            Pointer(_flow + 0x15C + _depth * 4) == _playing && U(_playing) == PlayingTable && U(_playing + 0x10) == 3 &&
            Pointer(0x755298) == _timer && U(_timer) == TimerTable &&
            U(TimerTable + 0x24) == Pause && U(TimerTable + 0x20) == Start &&
            Pointer(0x755274) == _engine && Pointer(_engine + 0x18) == _scene && U(_scene) == SceneTable,
            "Startup hold original Flow, Playing, timer or current scene identity changed.");
    }
    private void VerifyPins() => Require(Read(Pause, 9).SequenceEqual(_pausePin) && Read(Start, 9).SequenceEqual(_startPin),
        "Startup hold original Pause/Start live gateways differ from the pinned image.");
    private void EnsureStop(uint tid) => Require(_host.IsAtDebugStop && _host.ProcessHandle == _process &&
        _host.ProcessPid == _pid && _host.MainThreadId == _thread && tid == _thread &&
        Environment.CurrentManagedThreadId == _owner && _host.ImageBase == 0x400000 && Environment.TickCount64 <= _deadline &&
        (!_bound || Environment.TickCount64 <= _holdDeadline),
        "Startup hold requires its original owned bounded main-thread debug stop.");
    private void Fail(Exception error)
    {
        if (_failed) return;
        _failed = true; _failure = error.Message;
        _log(new { @event = "progression-startup-native-hold-failed", failure = _failure,
            originalPauseRequested = _started, originalStartRequested = _startCalled,
            nativeTimerFieldOrHealthOrAiOrInputWrites = 0, automaticRecoveryOrUnownedResumeRequested = false });
    }
    private uint Pointer(uint address)
    { uint value = U(address); Require(value >= 0x10000 && (value & 3) == 0, "Startup hold native owner is null or unaligned."); return value; }
    private uint U(uint address) => Win32Native.ReadUInt32(_process, address);
    private byte[] Read(uint address, int bytes) => Win32Native.ReadMemory(_process, address, bytes);
    private static byte[] Image(PEReader image, uint address, int bytes) =>
        image.GetSectionData(checked((int)(address - 0x400000))).GetContent(0, bytes).ToArray();
    private static string Hex(uint value) => $"0x{value:X8}";
    private static void Require(bool value, string message) { if (!value) throw new InvalidDataException(message); }
}
