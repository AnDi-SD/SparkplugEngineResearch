using System.Reflection.PortableExecutable;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// Own bounded Host Battle menu diagnostic, read-only by default. Original
/// input/reset/mapped reads run normally. An explicitly invoked diagnostic API
/// may dispatch one original pad-cache producer, or an explicitly separate own
/// three-call COM acquisition cycle. Neither path writes a game field directly.
/// Hooks are installed only after actual Diary pause/resume, then drain once.
/// </summary>
internal sealed class NativeMenuInputObservation
{
    private const uint InputEntry = 0x4E2C40, InputRet = 0x4E2D3E;
    private const uint ResetCall = 0x604E35, ResetReturn = 0x604E38;
    private const uint KeyCall = 0x604AAF, KeyReturn = 0x604AB4;
    private const uint PadCall = 0x604D9C, PadReturn = 0x604DA1;
    private const uint RawCall = 0x4D67DF, RawReturn = 0x4D67E2;
    private const uint ProducerEntry = 0x4CB240, ProducerEarlyReturn = 0x4CB290, ProducerCacheCopied = 0x4CB37F;
    private const uint PollCall = 0x4CB265, PollReturn = 0x4CB268, AcquireCall = 0x4CB272, AcquireReturn = 0x4CB275;
    private const uint StateCall = 0x4CB289, StateReturn = 0x4CB28C;
    private const uint MaximumInputReturns = 128, MaximumEvents = 512;
    private readonly DebuggerHost _host;
    private readonly PEReader _image;
    private readonly Action<object> _log;
    private readonly IntPtr _process;
    private readonly uint _pid, _thread;
    private readonly int _owner;
    private readonly Dictionary<uint, byte[]> _pins = [];
    private readonly HashSet<uint> _pendingHooks = [];
    private bool _installed, _sawDiary, _armed, _stopRequested, _retired, _failed;
    private uint _flow, _timer, _input, _command, _device, _playing, _playingCommand, _padRaw;
    private uint _inputReturns, _events, _resetPairs, _keyPairs, _padPairs, _rawPairs;
    private uint _keyHeld, _padHeld;
    private uint _producerEntries, _producerReturns, _diagnosticProducerEntries, _diagnosticProducerReturns, _comReturns;
    private bool _refreshAttempted, _refreshPending, _refreshRegistered, _refreshCompleted, _refreshAwaitInput;
    private bool _refreshEntryReturned, _refreshPostInputObserved, _refreshCommandReleased, _refreshArmRecovered;
    private byte _lastStock2B, _refreshCacheBefore;
    private uint _refreshEntryEsp, _refreshCaller, _refreshStub, _refreshResult;
    private byte[]? _refreshStubPin;
    private NativeCallDispatcher? _refreshDispatcher;
    private bool _reacquireAttempted, _reacquireActive, _reacquirePending, _reacquireCompleted;
    private bool _reacquireAwaitInput, _reacquirePostInputObserved, _reacquireCommandReleased, _reacquireArmRecovered;
    private uint _reacquireStage, _reacquireCallsBegun, _reacquireCallsCompleted;
    private uint _reacquireDevice, _reacquireTable, _reacquireUnacquire, _reacquireAcquire;
    private byte[]? _reacquireUnacquirePin, _reacquireAcquirePin;
    private int? _reacquireUnacquireHr, _reacquireAcquireHr;
    private long _reacquireStarted, _reacquireHardDeadline;
    private NativeCallDispatcher? _reacquireDispatcher;
    private ProducerFrame? _producerFrame;
    private ComFrame? _comFrame;
    private long _started, _deadline;
    private InputFrame? _inputFrame;
    private ResetFrame? _resetFrame;
    private ActionFrame? _actionFrame;
    private RawFrame? _rawFrame;
    private string _stopReason = "";
    private sealed record InputFrame(uint Esp, uint Caller, byte Enabled, byte Before, uint ResetPairs);
    private sealed record ResetFrame(uint Esp, byte Before);
    private sealed record ActionFrame(string Source, uint Mapper, uint Esp, uint Return, byte Before);
    private sealed record RawFrame(uint Esp, uint Receiver, uint Table, uint Function, uint Code);
    private sealed class ProducerFrame(uint esp, uint caller, uint device, uint table, byte before, bool diagnostic,
        uint ebx, uint esi, uint edi, uint ebp)
    {
        internal readonly uint Esp = esp, Caller = caller, Device = device, Table = table;
        internal readonly uint Ebx = ebx, Esi = esi, Edi = edi, Ebp = ebp;
        internal readonly byte Before = before;
        internal readonly bool Diagnostic = diagnostic;
        internal int? PollHr, AcquireHr, StateHr;
        internal byte? SuccessfulButton8;
        internal bool EarlyReturn, CacheCopied;
    }
    private sealed record ComFrame(string Operation, uint Esp, uint Return, uint PopBytes,
        uint Device, uint Table, uint Method, uint Buffer);

    internal NativeMenuInputObservation(DebuggerHost host, PEReader image, Action<object> log)
    {
        _host = host; _image = image; _log = log;
        Require(host.IsAtDebugStop && host.ProcessHandle != IntPtr.Zero && host.ImageBase == 0x400000 &&
            Convert.ToHexString(SHA256.HashData(image.GetEntireImage().GetContent().ToArray())) ==
            "3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F",
            "Menu observation requires the owned stopped pinned original PC image.");
        _process = host.ProcessHandle; _pid = host.ProcessPid; _thread = host.MainThreadId;
        _owner = Environment.CurrentManagedThreadId;
        foreach (uint site in new[] { InputEntry, InputRet, ResetCall, ResetReturn,
                     KeyCall, KeyReturn, PadCall, PadReturn, RawCall, RawReturn,
                     ProducerEntry, ProducerEarlyReturn, ProducerCacheCopied,
                     PollCall, PollReturn, AcquireCall, AcquireReturn, StateCall, StateReturn })
            _pins.Add(site, Image(site, site is InputRet ? 1 :
                site is ResetCall or RawCall or PollCall or AcquireCall or StateCall ? 3 : site == StateReturn ? 4 : 5));
        Require(Convert.ToHexString(_pins[InputEntry]) == "83EC0C568B" && _pins[InputRet][0] == 0xC3 &&
            _pins[ResetCall].AsSpan().SequenceEqual(new byte[] { 0xFF, 0x50, 0x04 }) &&
            _pins[RawCall].AsSpan().SequenceEqual(new byte[] { 0xFF, 0x50, 0x04 }) &&
            OriginalCallTarget(KeyCall) == 0x4D67A0 && OriginalCallTarget(PadCall) == 0x4D67A0,
            "Menu observation original entry/reset/action call pins differ.");
    }

    internal object Summary => new { installed = _installed, genuineDiaryPauseObserved = _sawDiary,
        armed = _armed, stopRequested = _stopRequested, retired = _retired, failed = _failed,
        stopReason = _stopReason, inputReturns = _inputReturns, resetPairs = _resetPairs,
        keyboardAction6Pairs = _keyPairs, padAction6Pairs = _padPairs, mappedPhysicalReadPairs = _rawPairs,
        keyboardHeldAction6Returns = _keyHeld, padHeldAction6Returns = _padHeld, events = _events,
        maximumInputReturns = MaximumInputReturns, maximumEventsBeforePairDrain = MaximumEvents,
        maximumAdmissionMilliseconds = 2000, pendingInput = _inputFrame is not null,
        pendingReset = _resetFrame is not null, pendingAction = _actionFrame is not null,
        pendingRawRead = _rawFrame is not null, remainingOneShotHookSites = _pendingHooks.Order().ToArray(),
        hooksInstalledBeforeRealResume = false, dormantCallbacksReadNativeGraph = false,
        dormantHooksRemovedOnTheirNextHitOrDebuggerCleanup = true,
        normalProducerEntries = _producerEntries, normalProducerReturns = _producerReturns,
        diagnosticProducerEntries = _diagnosticProducerEntries, diagnosticProducerReturns = _diagnosticProducerReturns,
        originalComReturns = _comReturns, pendingProducer = _producerFrame is not null, pendingCom = _comFrame is not null,
        originalRefreshAttempted = _refreshAttempted, originalRefreshCompleted = _refreshCompleted,
        refreshPostInputObserved = _refreshPostInputObserved, refreshCommandReleased = _refreshCommandReleased,
        refreshPauseArmRecovered = _refreshArmRecovered, refreshNativeResultRaw = _refreshResult,
        nativeQueriesAdded = _reacquireAttempted ? _reacquireCallsBegun : _refreshAttempted ? 1u : 0u,
        nativeDataWrites = 0, nativeContextChanges = 0,
        explicitRefreshUsesExistingDispatcher = _refreshAttempted, refreshReturnAlMeaningClaimed = false,
        reacquireAttempted = _reacquireAttempted, reacquireActive = _reacquireActive,
        reacquirePending = _reacquirePending, reacquireStage = _reacquireStage,
        reacquireCallsBegun = _reacquireCallsBegun, reacquireCallsCompleted = _reacquireCallsCompleted,
        reacquireCallsCompletedBeforePostInput = _reacquireCompleted, reacquireAwaitInput = _reacquireAwaitInput,
        reacquirePostInputObserved = _reacquirePostInputObserved,
        reacquireCommandReleased = _reacquireCommandReleased, reacquirePauseArmRecovered = _reacquireArmRecovered,
        reacquireUnacquireHr = _reacquireUnacquireHr, reacquireAcquireHr = _reacquireAcquireHr,
        reacquireHardDeadlineMilliseconds = _reacquireHardDeadline, reacquireMaximumMilliseconds = 5000,
        reacquireMaximumInjectedCalls = 3, reacquireIsOwnAdapterNotGameWrapper = true,
        nativeComGuidIdentityClaimed = false,
        firstCauseClaimedWithoutActualInputPairs = false };

    internal void InstallStopped()
    {
        Ensure(_thread); Require(!_installed, "Menu observation installation is single-use.");
        foreach (var pin in _pins) Require(Read(pin.Key, pin.Value.Length).SequenceEqual(pin.Value),
            "Menu observation site changed before admission.");
        _installed = true;
        Emit("waiting-for-real-diary-resume", new { highFrequencyHooksInstalled = 0 });
    }

    internal void ObserveAtWorld(uint tid, ref Win32Native.X86Context context)
    {
        if (_retired || _failed) return;
        Ensure(tid); Require(_installed && context.Eip == NativeCallDispatcher.WorldReturn,
            "Menu observation is not at the owned original WorldRET.");
        if (_armed)
        {
            CheckLimit();
            if (_reacquireActive && _reacquireCompleted && _reacquirePostInputObserved && !NativePairsPending)
            {
                VerifyOwners(); VerifyReacquireCom();
                _reacquireCommandReleased |= B(_command + 0x2B) == 0 && B(_playingCommand + 0x2B) == 0;
                _reacquireArmRecovered |= _reacquireCommandReleased && B(_playing + 0x3C) == 1;
                _reacquireActive = false;
                Emit("diagnostic-original-pad-reacquire-world-after-stock-input", new
                {
                    command2B = B(_command + 0x2B), playing2B = B(_playingCommand + 0x2B),
                    pauseArm = B(_playing + 0x3C), cachedA4 = B(_padRaw + 0xA4),
                    injectedCallsCompleted = _reacquireCallsCompleted,
                    elapsedMilliseconds = Math.Max(0, Environment.TickCount64 - _reacquireStarted),
                    commandReleaseObserved = _reacquireCommandReleased, armRecoveryObserved = _reacquireArmRecovered,
                    directGameFieldWrites = 0, ownAdapterNotOriginalGameWrapper = true
                });
            }
            FinishDrain(); return;
        }
        uint flow = P(0x755294), current = U(flow + 0x1B0), pending = U(flow + 0x1B8);
        if (!_sawDiary)
        {
            if (current != 61 || pending != 0) return;
            uint timer = P(0x755298); Require(U(timer) == 0x702F94, "Menu observation timer type differs.");
            if (B(timer + 0x40) != 1) return;
            uint stack = U(flow + 0x1AC); Require(stack is >= 2 and <= 20, "Diary native flow depth differs.");
            uint playing = 0;
            for (uint index = 0; index <= stack; ++index)
            {
                uint state = P(flow + 0x15C + index * 4);
                if (U(state) == 0x7105C8 && U(state + 0x10) == 47)
                { Require(playing == 0, "Diary contains duplicate Battle47 owners."); playing = state; }
            }
            Require(playing != 0, "Real Diary pause has no retained original Battle47 owner.");
            _flow = flow; _timer = timer; _playing = playing; _sawDiary = true;
            Emit("real-diary-pause-observed", new { current, pending, stack, nativePaused = 1 });
            return;
        }
        Require(flow == _flow && U(0x755298) == _timer && U(_timer) == 0x702F94 &&
            U(_playing) == 0x7105C8 && U(_playing + 0x10) == 47,
            "Menu observation retained Diary/Battle owners changed.");
        if (current != 47 || pending != 0 || B(_timer + 0x40) != 0) return;
        uint depth = U(flow + 0x1AC); Require(depth <= 20, "Menu resume flow depth exceeds20.");
        if (U(flow + 0x15C + depth * 4) != _playing) return;
        uint input = P(0x7552A0); Require(U(input) == 0x6F5508, "Menu resume UserInput type differs.");
        if (B(input + 0x20) != 1) return;
        _input = input; _command = P(input + 0x18); _device = P(input + 0x1C);
        _playingCommand = P(_playing + 0x4C);
        Require(U(_command) == 0x703090 && U(_playingCommand) == 0x703090 &&
            U(_device) == 0x7116CC && U(_device + 0x14) == 3,
            "Menu resume primary/Playing commands or InGame device context differ.");
        _padRaw = checked(P(_device + 0x3C) + 0x14); VerifyPadRaw();
        foreach (var pin in _pins) Require(Read(pin.Key, pin.Value.Length).SequenceEqual(pin.Value),
            "Menu observation native sites changed before delayed hook installation.");
        _armed = true; _started = Environment.TickCount64; _deadline = checked(_started + 2000);
        foreach (uint site in _pins.Keys) Repeat(site);
        Emit("armed-after-native-resume", new { current, pending, depth, nativePaused = 0,
            primaryCommand = Hex(_command), playingCommand = Hex(_playingCommand),
            playingCommandEqualsPrimary = _playingCommand == _command,
            primary2B = B(_command + 0x2B), playing2B = B(_playingCommand + 0x2B) });
    }

    // Root calls only for its explicit Observe+Isolated+Refresh diagnostic
    // composition. false makes no context change or injected native call.
    internal bool TryRefreshOriginalPadAtWorld(uint tid, ref Win32Native.X86Context context,
        NativeCallDispatcher calls, Action resume)
    {
        if (!_armed || _retired || _failed || _stopRequested || _refreshAttempted || _reacquireAttempted || Pending || calls.Pending ||
            _padHeld == 0 || _inputReturns == 0 || _lastStock2B != 1 || Environment.TickCount64 >= _deadline - 500) return false;
        Ensure(tid); Require(context.Eip == NativeCallDispatcher.WorldReturn, "Original pad refresh requires owned WorldRET.");
        if (GetWindowThreadProcessId(GetForegroundWindow(), out uint foreground) == 0 || foreground != _pid) return false;
        VerifyOwners(); VerifyPadRaw();
        Require(U(_flow + 0x1B0) == 47 && U(_flow + 0x1B8) == 0 && B(_timer + 0x40) == 0 && B(_input + 0x20) == 1,
            "Original pad refresh no longer has its actual unpaused enabled Host level.");
        return DispatchOriginalPadRefresh(tid, ref context, calls, resume, afterReacquire: false);
    }

    // Explicit own adapter, not an original game wrapper. Root admits only its
    // Isolated+Refresh+Reacquire diagnostic and uses a dispatcher timeout <=5s.
    // Each stage starts at a different genuine WorldRET. Callbacks only resume;
    // they never start the next stage or directly edit input/cache/arm fields.
    internal bool TryReacquireOriginalPadAtWorld(uint tid, ref Win32Native.X86Context context,
        NativeCallDispatcher calls, Action resume)
    {
        if (_retired || _failed || !_armed) return false;
        if (!_reacquireAttempted)
        {
            if (_stopRequested || _refreshAttempted || Pending || calls.Pending || _padHeld == 0 ||
                _inputReturns == 0 || _lastStock2B != 1 || Environment.TickCount64 >= _deadline - 500) return false;
            Ensure(tid); Require(context.Eip == NativeCallDispatcher.WorldReturn, "Pad acquisition cycle requires owned WorldRET.");
            if (GetWindowThreadProcessId(GetForegroundWindow(), out uint foreground) == 0 || foreground != _pid) return false;
            VerifyOwners(); VerifyPadRaw(); VerifyUnpausedHost();
            _reacquireDevice = P(_padRaw + 0x78); _reacquireTable = P(_reacquireDevice);
            _reacquireUnacquire = U(_reacquireTable + 0x20); _reacquireAcquire = U(_reacquireTable + 0x1C);
            Require(_reacquireUnacquire >= 0x10000 && _reacquireAcquire >= 0x10000,
                "Pad acquisition cycle COM methods are low/null.");
            _reacquireUnacquirePin = Read(_reacquireUnacquire, 8); _reacquireAcquirePin = Read(_reacquireAcquire, 8);
            _reacquireAttempted = _reacquireActive = true; _reacquireDispatcher = calls;
            _reacquireStarted = Environment.TickCount64; _reacquireHardDeadline = checked(_reacquireStarted + 5000);
            Emit("diagnostic-original-pad-reacquire-admitted", new
            {
                receiver = Hex(_padRaw), comDevice = Hex(_reacquireDevice), comTable = Hex(_reacquireTable),
                unacquireFunction = Hex(_reacquireUnacquire), acquireFunction = Hex(_reacquireAcquire),
                hardDeadlineMilliseconds = _reacquireHardDeadline, maximumInjectedCalls = 3,
                ownAdapterNotOriginalGameWrapper = true, nativeComGuidIdentityClaimed = false,
                directGameFieldWrites = 0, deviceReleaseOrRecreation = false
            });
        }
        if (!_reacquireActive || _reacquireCompleted) return false;
        Ensure(tid); Require(context.Eip == NativeCallDispatcher.WorldReturn, "Pad acquisition stage is not a real WorldRET.");
        CheckReacquireDeadline();
        if (_reacquirePending || NativePairsPending || calls.Pending) return false;
        Require(ReferenceEquals(calls, _reacquireDispatcher), "Pad acquisition cycle changed native dispatcher.");
        if (GetWindowThreadProcessId(GetForegroundWindow(), out uint currentForeground) == 0 || currentForeground != _pid) return false;
        VerifyOwners(); VerifyPadRaw(); VerifyUnpausedHost(); VerifyReacquireCom();
        Require(_reacquireCallsBegun == _reacquireCallsCompleted && _reacquireStage == _reacquireCallsCompleted &&
            _reacquireStage <= 2, "Pad acquisition stage/call budget differs.");
        if (_reacquireStage == 2)
        {
            _reacquirePending = true; ++_reacquireCallsBegun;
            return DispatchOriginalPadRefresh(tid, ref context, calls, resume, afterReacquire: true);
        }

        uint stage = _reacquireStage, function = stage == 0 ? _reacquireUnacquire : _reacquireAcquire;
        string operation = stage == 0 ? "Unacquire" : "Acquire";
        _reacquirePending = true; ++_reacquireCallsBegun;
        try
        {
            // IDirectInputDevice8 methods are stdcall: THIS is one stack DWORD.
            // Cdecl=false leaves that DWORD to RET4; ECX is irrelevant to COM.
            calls.Begin(_host, tid, ref context, function, _reacquireDevice, [_reacquireDevice], false, result =>
            {
                Ensure(_thread); CheckReacquireDeadline(); VerifyOwners(); VerifyPadRaw(); VerifyReacquireCom();
                Require(_reacquireActive && _reacquirePending && _reacquireStage == stage && !calls.Pending &&
                    _reacquireCallsBegun == stage + 1 && _reacquireCallsCompleted == stage,
                    "Pad acquisition callback does not match its single pending stage.");
                int hr = unchecked((int)result);
                if (stage == 0) _reacquireUnacquireHr = hr; else _reacquireAcquireHr = hr;
                _reacquirePending = false; ++_reacquireCallsCompleted; ++_reacquireStage;
                Emit("diagnostic-original-pad-reacquire-com-returned", new
                {
                    stage, operation, comDevice = Hex(_reacquireDevice), comTable = Hex(_reacquireTable),
                    function = Hex(function), hresultSigned = hr, hresultRaw = result, hresultHex = Hex(result),
                    nextStage = _reacquireStage, nextStageRequiresLaterRealWorld = true,
                    exactRetainedComAndMethods = true, directGameFieldWrites = 0,
                    ownAdapterNotOriginalGameWrapper = true
                });
                resume();
            });
            byte[] stub = Read(context.Eip, 23);
            Require(stub[0] == 0x68 && BitConverter.ToUInt32(stub, 1) == _reacquireDevice &&
                stub[5] == 0xB9 && BitConverter.ToUInt32(stub, 6) == _reacquireDevice &&
                stub[10] == 0xB8 && BitConverter.ToUInt32(stub, 11) == function &&
                stub[15] == 0xFF && stub[16] == 0xD0 && stub[17] == 0xA3 && stub[22] == 0xCC,
                "Pad acquisition dispatcher stdcall THIS/argument stub differs.");
            Emit("diagnostic-original-pad-reacquire-com-dispatched", new
            {
                stage, operation, comDevice = Hex(_reacquireDevice), comTable = Hex(_reacquireTable),
                function = Hex(function), stackThisArgument = Hex(_reacquireDevice), cdecl = false,
                dispatcherStub = Hex(context.Eip), expectedCalleeStackPopBytes = 4,
                injectedCallsBegun = _reacquireCallsBegun, directGameFieldWrites = 0,
                ownAdapterNotOriginalGameWrapper = true
            });
            return true;
        }
        catch { _failed = true; _retired = true; throw; }
    }

    private bool DispatchOriginalPadRefresh(uint tid, ref Win32Native.X86Context context,
        NativeCallDispatcher calls, Action resume, bool afterReacquire)
    {
        _refreshAttempted = _refreshPending = true; _refreshDispatcher = calls;
        _refreshCacheBefore = B(_padRaw + 0xA4); _refreshEntryEsp = checked(context.Esp - 4);
        try
        {
            calls.Begin(_host, tid, ref context, ProducerEntry, _padRaw, [], false, result =>
            {
                Ensure(_thread); VerifyOwners(); VerifyPadRaw();
                Require(_refreshRegistered && _refreshEntryReturned && _producerFrame is null && _comFrame is null,
                    "Original pad refresh lacks its exact registered native producer return.");
                _refreshResult = result; _refreshPending = false; _refreshCompleted = _refreshAwaitInput = true;
                if (afterReacquire)
                {
                    CheckReacquireDeadline(); VerifyReacquireCom();
                    Require(_reacquireActive && _reacquirePending && _reacquireStage == 2 &&
                        _reacquireCallsBegun == 3 && _reacquireCallsCompleted == 2,
                        "Pad acquisition producer return escaped the third and final stage.");
                    _reacquirePending = false; _reacquireStage = 3; ++_reacquireCallsCompleted;
                    _reacquireCompleted = _reacquireAwaitInput = true;
                    Emit("diagnostic-original-pad-reacquire-producer-returned", new
                    {
                        comDevice = Hex(_reacquireDevice), comTable = Hex(_reacquireTable),
                        cachedA4Before = _refreshCacheBefore, cachedA4After = B(_padRaw + 0xA4),
                        nativeResultRaw = result, nativeResultAL = (byte)result, returnAlMeaningClaimed = false,
                        injectedCallsCompleted = _reacquireCallsCompleted, awaitingActualStockInputC3 = true,
                        directGameFieldWrites = 0, ownAdapterNotOriginalGameWrapper = true
                    });
                }
                Emit("diagnostic-original-pad-refresh-returned", new { cachedA4Before = _refreshCacheBefore,
                    cachedA4After = B(_padRaw + 0xA4), nativeResultRaw = result, nativeResultAL = (byte)result,
                    returnAlMeaningClaimed = false, directGameFieldWrites = 0, awaitingActualStockInputC3 = true });
                resume();
            });
            _refreshStub = context.Eip; _refreshCaller = checked(_refreshStub + 12);
            _refreshStubPin = Read(_refreshStub, 18);
            Require(_refreshStubPin[0] == 0xB9 && BitConverter.ToUInt32(_refreshStubPin, 1) == _padRaw &&
                _refreshStubPin[5] == 0xB8 && BitConverter.ToUInt32(_refreshStubPin, 6) == ProducerEntry &&
                _refreshStubPin[10] == 0xFF && _refreshStubPin[11] == 0xD0 &&
                _refreshStubPin[12] == 0xA3 && _refreshStubPin[17] == 0xCC,
                "Original pad refresh dispatcher no-argument thiscall stub differs.");
            _refreshRegistered = true;
            Emit("diagnostic-original-pad-refresh-dispatched", new { function = Hex(ProducerEntry), receiver = Hex(_padRaw),
                registeredDispatcherCaller = Hex(_refreshCaller), expectedEntryEsp = Hex(_refreshEntryEsp),
                cachedA4Before = _refreshCacheBefore, nativeCallsAdded = 1, directGameFieldWrites = 0 });
            return true;
        }
        catch { _failed = true; _retired = true; throw; }
    }

    private void Observe(uint tid, uint at, ref Win32Native.X86Context c)
    {
        Ensure(tid); Require(c.Eip == at, "Menu observation callback EIP changed.");
        CheckLimit();
        if (_stopRequested && !Pending) { FinishDrain(); return; }
        Require(Read(at, _pins[at].Length).SequenceEqual(_pins[at]), "Menu observation instruction pin changed.");
        VerifyOwners();
        if (at is ProducerEntry or ProducerEarlyReturn or ProducerCacheCopied or PollCall or PollReturn or
            AcquireCall or AcquireReturn or StateCall or StateReturn)
        { ObserveProducer(tid, at, ref c); FinishDrain(); return; }
        if (at == InputEntry)
        {
            if (_stopRequested && !_refreshAwaitInput) return;
            Require(_inputFrame is null && c.Ecx == _input, "Menu UserInput entry is nested or has another receiver.");
            byte enabled = B(_input + 0x20); Require(enabled <= 1, "UserInput enable is not boolean.");
            _inputFrame = new(c.Esp, U(c.Esp), enabled, B(_command + 0x2B), _resetPairs);
        }
        else if (at == InputRet)
        {
            if (_inputFrame is not { } frame) return;
            Require(c.Esp == frame.Esp && U(c.Esp) == frame.Caller &&
                _resetFrame is null && _actionFrame is null && _rawFrame is null && _producerFrame is null && _comFrame is null,
                "Menu UserInput C3/ESP/caller or nested native pairs differ.");
            ++_inputReturns;
            _lastStock2B = B(_command + 0x2B);
            Emit("stock-input-return", new { ordinal = _inputReturns, enabledAtEntry = frame.Enabled,
                enabledNow = B(_input + 0x20), before2B = frame.Before, after2B = B(_command + 0x2B),
                playing2B = B(_playingCommand + 0x2B), pauseArm = B(_playing + 0x3C),
                resetPairsThisInput = _resetPairs - frame.ResetPairs,
                playingCommandEqualsPrimary = _playingCommand == _command, exactC3EspCaller = true });
            if (_refreshCompleted)
            {
                _refreshPostInputObserved = true; _refreshAwaitInput = false;
                _refreshCommandReleased |= _lastStock2B == 0;
                _refreshArmRecovered |= _lastStock2B == 0 && B(_playing + 0x3C) == 1;
                Emit("post-refresh-stock-input-return", new { ordinal = _inputReturns, command2B = _lastStock2B,
                    playing2B = B(_playingCommand + 0x2B), pauseArm = B(_playing + 0x3C),
                    cachedA4 = B(_padRaw + 0xA4), originalProducerResultRaw = _refreshResult,
                    commandReleaseObserved = _refreshCommandReleased, armRecoveryObserved = _refreshArmRecovered });
            }
            if (_reacquireCompleted && _reacquireAwaitInput)
            {
                CheckReacquireDeadline(); VerifyReacquireCom();
                _reacquireAwaitInput = false; _reacquirePostInputObserved = true;
                _reacquireCommandReleased |= _lastStock2B == 0 && B(_playingCommand + 0x2B) == 0;
                _reacquireArmRecovered |= _reacquireCommandReleased && B(_playing + 0x3C) == 1;
                Emit("post-reacquire-stock-input-return", new
                {
                    ordinal = _inputReturns, command2B = _lastStock2B, playing2B = B(_playingCommand + 0x2B),
                    pauseArm = B(_playing + 0x3C), cachedA4 = B(_padRaw + 0xA4),
                    unacquireHr = _reacquireUnacquireHr, acquireHr = _reacquireAcquireHr,
                    commandReleaseObserved = _reacquireCommandReleased, armRecoveryObserved = _reacquireArmRecovered,
                    nextObservationAtRealWorld = true, directGameFieldWrites = 0,
                    ownAdapterNotOriginalGameWrapper = true
                });
            }
            _inputFrame = null; CheckLimit();
        }
        else if (at == ResetCall)
        {
            if (_inputFrame is null || _stopRequested && !_refreshAwaitInput) return;
            Require(_resetFrame is null && c.Ecx == _command && c.Edi == _command && c.Esi == _device &&
                U(U(_command) + 4) == 0x5992E0, "Menu reset CALL does not own the primary original command.");
            _resetFrame = new(c.Esp, B(_command + 0x2B));
        }
        else if (at == ResetReturn)
        {
            if (_resetFrame is not { } frame) return;
            Require(c.Esp == frame.Esp && c.Edi == _command && c.Esi == _device,
                "Menu reset native C3 did not return at the same ESP/owners.");
            ++_resetPairs;
            Emit("actual-primary-reset-return", new { before2B = frame.Before, after2B = B(_command + 0x2B),
                nativeFunction = "005992E0", nativeCallExecuted = true, exactEspMatched = true });
            _resetFrame = null;
        }
        else if (at is KeyCall or PadCall)
        {
            if (_inputFrame is null || _stopRequested && !_refreshAwaitInput) return;
            uint mapper = P(_device + (at == KeyCall ? 0x38u : 0x3Cu));
            Require(_actionFrame is null && _rawFrame is null && c.Ecx == mapper && U(c.Esp) == 6,
                "Menu action6 CALL has another mapper, argument or nested frame.");
            _actionFrame = new(at == KeyCall ? "keyboard" : "pad", mapper, c.Esp,
                at == KeyCall ? KeyReturn : PadReturn, B(_command + 0x2B));
        }
        else if (at is KeyReturn or PadReturn)
        {
            if (_actionFrame is not { } frame) return;
            Require(at == frame.Return && c.Esp == checked(frame.Esp + 4) && _rawFrame is null,
                "Menu action6 native RET4/ESP or raw-read pairing differs.");
            byte result = (byte)c.Eax; Require(result <= 1, "Menu native action6 result is not boolean.");
            if (frame.Source == "keyboard") { ++_keyPairs; if (result != 0) ++_keyHeld; }
            else { ++_padPairs; if (result != 0) ++_padHeld; }
            Emit("native-action6-return", new { source = frame.Source, mapper = Hex(frame.Mapper),
                result, command2BBeforeQuery = frame.Before, command2BBeforeNativeRefill = B(_command + 0x2B),
                nativeWriter = frame.Source == "keyboard" ? "00604AB8" : "00604DA5", exactRet4Esp = true });
            _actionFrame = null;
        }
        else if (at == RawCall)
        {
            if (_actionFrame is not { } frame || _stopRequested && !_refreshAwaitInput) return;
            uint receiver = checked(frame.Mapper + 0x14), table = U(receiver), function = U(table + 4);
            Require(_rawFrame is null && c.Ecx == receiver && c.Ebx == receiver &&
                table is >= 0x6DA000 and < 0x744000 && function is >= 0x401000 and < 0x6DA000 &&
                Read(function, 8).SequenceEqual(Image(function, 8)),
                "Menu mapped physical read has another original receiver/function.");
            _rawFrame = new(c.Esp, receiver, table, function, U(c.Esp));
        }
        else if (at == RawReturn)
        {
            if (_rawFrame is not { } frame) return;
            Require(_actionFrame is not null && c.Esp == checked(frame.Esp + 4) && c.Ebx == frame.Receiver &&
                U(frame.Receiver) == frame.Table && U(frame.Table + 4) == frame.Function,
                "Menu mapped physical native RET4 changed its ESP/receiver/function.");
            ++_rawPairs;
            Emit("native-mapped-physical-return", new { source = _actionFrame!.Source, actionId = 6,
                physicalCode = frame.Code, receiver = Hex(frame.Receiver), originalReader = Hex(frame.Function),
                rawNativeResultAL = (byte)c.Eax, exactRet4Esp = true, nativeQueryAdded = false });
            _rawFrame = null;
        }
        FinishDrain();
    }

    private void ObserveProducer(uint tid, uint at, ref Win32Native.X86Context c)
    {
        if (at == ProducerEntry)
        {
            if (c.Ecx != _padRaw || _stopRequested && !_refreshPending && !_refreshAwaitInput) return;
            VerifyPadRaw();
            uint caller = U(c.Esp); bool diagnostic = _refreshPending && _refreshRegistered && caller == _refreshCaller;
            Require(_producerFrame is null && _comFrame is null &&
                (diagnostic ? c.Esp == _refreshEntryEsp && _refreshDispatcher?.Pending == true &&
                    Read(_refreshStub, 18).SequenceEqual(_refreshStubPin!) : caller is >= 0x401000 and < 0x6DA000),
                "Original pad producer entry escaped its exact normal or registered diagnostic caller.");
            uint device = P(_padRaw + 0x78), table = P(device);
            var frame = new ProducerFrame(c.Esp, caller, device, table, B(_padRaw + 0xA4), diagnostic,
                c.Ebx, c.Esi, c.Edi, c.Ebp);
            _producerFrame = frame;
            if (diagnostic) ++_diagnosticProducerEntries; else ++_producerEntries;
            Emit("original-pad-producer-entry", new { diagnostic, receiver = Hex(_padRaw), caller = Hex(caller),
                entryEsp = Hex(c.Esp), rawEnabled30 = B(_padRaw + 0x30), cachedA4 = frame.Before,
                comDevice = Hex(device), comTable = Hex(table), nativeCallAdded = diagnostic });
            Require(!_host.Breakpoints.Contains(caller), "Pad producer caller already has another pending breakpoint owner.");
            _host.Breakpoints.AddReturn(caller, tid, "MenuInput.pad-producer.return",
                (uint returnedTid, uint returnedAt, ref Win32Native.X86Context returned) =>
                {
                    try
                    {
                        Ensure(returnedTid); VerifyOwners(); VerifyPadRaw();
                        Require(ReferenceEquals(_producerFrame, frame) && _comFrame is null &&
                            returnedAt == frame.Caller && returned.Eip == frame.Caller &&
                            returned.Esp == checked(frame.Esp + 4) && returned.Ebx == frame.Ebx &&
                            returned.Esi == frame.Esi && returned.Edi == frame.Edi && returned.Ebp == frame.Ebp &&
                            U(_padRaw + 0x78) == frame.Device && U(frame.Device) == frame.Table,
                            "Original pad producer C3/caller/ESP/callee-saved/COM owner pairing differs.");
                        if (frame.Diagnostic)
                        {
                            Require(_refreshPending && _refreshRegistered && frame.Caller == _refreshCaller &&
                                Read(_refreshStub, 18).SequenceEqual(_refreshStubPin!),
                                "Diagnostic producer returned to a different dispatcher registration.");
                            ++_diagnosticProducerReturns; _refreshEntryReturned = true;
                        }
                        else ++_producerReturns;
                        Emit("original-pad-producer-return", new { diagnostic = frame.Diagnostic,
                            cachedA4Before = frame.Before, cachedA4After = B(_padRaw + 0xA4),
                            frame.PollHr, frame.AcquireHr, frame.StateHr, frame.SuccessfulButton8,
                            frame.EarlyReturn, frame.CacheCopied, nativeResultRaw = returned.Eax,
                            nativeResultAL = (byte)returned.Eax, returnAlMeaningClaimed = false,
                            exactC3EspAndPreservedRegisters = true });
                        _producerFrame = null; CheckLimit(); FinishDrain();
                    }
                    catch { _failed = true; _retired = true; throw; }
                });
            return;
        }
        if (_producerFrame is not { } producer) return;
        VerifyPadRaw();
        Require(c.Esi == _padRaw && U(_padRaw + 0x78) == producer.Device && U(producer.Device) == producer.Table,
            "Original pad producer body changed raw receiver or COM owner.");
        if (at is PollCall or AcquireCall or StateCall)
        {
            uint offset = at == PollCall ? 0x64u : at == AcquireCall ? 0x1Cu : 0x24u;
            uint pop = at == StateCall ? 12u : 4u;
            uint returned = at == PollCall ? PollReturn : at == AcquireCall ? AcquireReturn : StateReturn;
            Require(_comFrame is null && c.Eax == producer.Device && U(c.Esp) == producer.Device &&
                c.Esp == checked(producer.Esp - (at == StateCall ? 0x12Cu : 0x124u)) &&
                (at == AcquireCall ? c.Edx : c.Ecx) == producer.Table,
                "Original pad COM call has another this/stack/vtable frame.");
            uint buffer = 0;
            if (at == StateCall)
            {
                buffer = U(c.Esp + 8);
                Require(U(c.Esp + 4) == 0x110 && buffer == checked(c.Esp + 24) &&
                    buffer == checked(producer.Esp - 0x114), "Original GetDeviceState buffer/size is not its bounded native stack local.");
            }
            uint method = U(producer.Table + offset);
            Require(method >= 0x10000, "Original pad COM method pointer is low/null.");
            _comFrame = new(at == PollCall ? "Poll" : at == AcquireCall ? "Acquire" : "GetDeviceState",
                c.Esp, returned, pop, producer.Device, producer.Table, method, buffer);
        }
        else if (at is PollReturn or AcquireReturn or StateReturn)
        {
            Require(_comFrame is not null, "Original pad COM return lacks an admitted call.");
            ComFrame frame = _comFrame!;
            uint offset = frame.Operation == "Poll" ? 0x64u : frame.Operation == "Acquire" ? 0x1Cu : 0x24u;
            Require(at == frame.Return && c.Esp == checked(frame.Esp + frame.PopBytes) &&
                U(frame.Device) == frame.Table && U(frame.Table + offset) == frame.Method,
                "Original pad COM stdcall return/ESP/method differs.");
            int hr = unchecked((int)c.Eax); byte? button = null;
            if (at == PollReturn) producer.PollHr = hr;
            else if (at == AcquireReturn) producer.AcquireHr = hr;
            else
            {
                producer.StateHr = hr;
                // Failed/uninitialized buffer bytes are deliberately not read.
                if (hr >= 0) button = producer.SuccessfulButton8 = B(frame.Buffer + 0x38);
            }
            ++_comReturns;
            Emit("original-pad-com-return", new { diagnostic = producer.Diagnostic, operation = frame.Operation,
                hresultSigned = hr, hresultRaw = c.Eax, hresultHex = Hex(c.Eax),
                successfulButton8 = button, cachedA4BeforeCopy = B(_padRaw + 0xA4),
                failedBufferRead = false, stdcallEspDelta = frame.PopBytes, exactOwnerAndEsp = true });
            _comFrame = null;
        }
        else if (at == ProducerEarlyReturn)
        {
            Require(_comFrame is null && c.Esp == checked(producer.Esp - 0x120), "Original pad early-return frame differs.");
            producer.EarlyReturn = true;
            Emit("original-pad-producer-early-return", new { diagnostic = producer.Diagnostic,
                enabled30 = B(_padRaw + 0x30), cachedA4 = B(_padRaw + 0xA4),
                producer.PollHr, producer.AcquireHr, producer.StateHr,
                originalEarlyReturnPreservesCache = true, returnAlMeaningClaimed = false });
        }
        else if (at == ProducerCacheCopied)
        {
            Require(_comFrame is null && producer.StateHr is >= 0 && producer.SuccessfulButton8 is byte expected &&
                B(_padRaw + 0xA4) == expected && c.Esp == checked(producer.Esp - 0x124),
                "Original pad successful cache copy differs from the actual GetDeviceState stack result.");
            producer.CacheCopied = true;
            Emit("original-pad-cache-copied", new { diagnostic = producer.Diagnostic,
                actualBufferButton8 = producer.SuccessfulButton8, cachedA4 = B(_padRaw + 0xA4), nativeWritesByObserver = 0 });
        }
    }

    private void VerifyPadRaw() => Require(_padRaw >= 0x10014 && P(_device + 0x3C) == _padRaw - 0x14 &&
        U(_padRaw - 0x14) == 0x6F30B4 && U(_padRaw) == 0x6F3098 &&
        U(0x6F3098) == ProducerEntry && U(0x6F309C) == 0x4CB570,
        "Menu observer's exact original embedded pad receiver/type/slots changed.");

    private bool NativePairsPending => _inputFrame is not null || _resetFrame is not null || _actionFrame is not null ||
        _rawFrame is not null || _producerFrame is not null || _comFrame is not null;
    private bool Pending => NativePairsPending || _refreshPending || _refreshAwaitInput || _reacquireActive ||
        _reacquirePending || _reacquireAwaitInput;
    private void VerifyUnpausedHost() => Require(U(_flow + 0x1B0) == 47 && U(_flow + 0x1B8) == 0 &&
        B(_timer + 0x40) == 0 && B(_input + 0x20) == 1,
        "Pad acquisition cycle lost its actual unpaused enabled Host level.");
    private void VerifyReacquireCom() => Require(_reacquireAttempted && U(_padRaw + 0x78) == _reacquireDevice &&
        U(_reacquireDevice) == _reacquireTable && U(_reacquireTable + 0x20) == _reacquireUnacquire &&
        U(_reacquireTable + 0x1C) == _reacquireAcquire &&
        Read(_reacquireUnacquire, 8).SequenceEqual(_reacquireUnacquirePin!) &&
        Read(_reacquireAcquire, 8).SequenceEqual(_reacquireAcquirePin!),
        "Pad acquisition cycle changed its retained COM/vtable/method pointers or bytes.");
    private void CheckReacquireDeadline()
    {
        if (!_reacquireActive || Environment.TickCount64 < _reacquireHardDeadline) return;
        _failed = _retired = true;
        Emit("diagnostic-original-pad-reacquire-hard-deadline", new
        {
            elapsedMilliseconds = Math.Max(0, Environment.TickCount64 - _reacquireStarted),
            maximumMilliseconds = 5000, stage = _reacquireStage, pending = _reacquirePending,
            injectedCallsBegun = _reacquireCallsBegun, injectedCallsCompleted = _reacquireCallsCompleted,
            directGameFieldWrites = 0, furtherCallsAllowed = false
        });
        throw new InvalidDataException("Own diagnostic pad acquisition cycle exceeded its five-second hard deadline.");
    }
    private void CheckLimit()
    {
        CheckReacquireDeadline();
        if (!_stopRequested && (Environment.TickCount64 >= _deadline || _inputReturns >= MaximumInputReturns || _events >= MaximumEvents))
        {
            _stopRequested = true;
            _stopReason = _inputReturns >= MaximumInputReturns ? "128-native-input-returns" :
                _events >= MaximumEvents ? "512-events-drain" : "two-second-admission-ended";
        }
    }
    private void FinishDrain()
    {
        if (!_stopRequested || Pending || _retired) return;
        _retired = true;
        Emit("retired-after-native-pair-drain", new { reason = _stopReason,
            elapsedMilliseconds = Math.Max(0, Environment.TickCount64 - _started),
            normalProducerEntries = _producerEntries, normalProducerReturns = _producerReturns,
            diagnosticProducerEntries = _diagnosticProducerEntries, diagnosticProducerReturns = _diagnosticProducerReturns,
            originalRefreshAttempted = _refreshAttempted, originalRefreshCompleted = _refreshCompleted,
            refreshPostInputObserved = _refreshPostInputObserved, refreshCommandReleased = _refreshCommandReleased,
            refreshPauseArmRecovered = _refreshArmRecovered,
            reacquireAttempted = _reacquireAttempted, reacquireCallsCompleted = _reacquireCallsCompleted,
            reacquirePostInputObserved = _reacquirePostInputObserved,
            reacquireCommandReleased = _reacquireCommandReleased, reacquirePauseArmRecovered = _reacquireArmRecovered,
            reacquireUnacquireHr = _reacquireUnacquireHr, reacquireAcquireHr = _reacquireAcquireHr,
            dormantOneShotHooks = _pendingHooks.Count, furtherNativeGraphReads = false });
    }
    private void VerifyOwners() => Require(U(0x755294) == _flow && U(0x755298) == _timer && U(_timer) == 0x702F94 &&
        U(0x7552A0) == _input && U(_input) == 0x6F5508 && U(_input + 0x18) == _command &&
        U(_command) == 0x703090 && U(_input + 0x1C) == _device && U(_device) == 0x7116CC &&
        U(_playing) == 0x7105C8 && U(_playing + 0x10) == 47 && U(_playing + 0x4C) == _playingCommand &&
        U(_playingCommand) == 0x703090, "Menu observation native canonical owners changed.");
    private void Repeat(uint site)
    {
        BreakpointCallback callback = null!;
        callback = (uint tid, uint at, ref Win32Native.X86Context context) =>
        {
            _pendingHooks.Remove(site);
            if (_retired || _failed) return; // No remote read; one-shot is not rearmed.
            try { Observe(tid, at, ref context); }
            catch { _failed = true; _retired = true; throw; }
            if (!_retired && !_failed) { _host.Breakpoints.AddReturn(site, _thread, "MenuInput.observe", callback); _pendingHooks.Add(site); }
        };
        _host.Breakpoints.AddReturn(site, _thread, "MenuInput.observe", callback); _pendingHooks.Add(site);
    }
    private void Ensure(uint tid) => Require(Environment.CurrentManagedThreadId == _owner && _host.IsAtDebugStop &&
        _host.ProcessHandle == _process && _host.ProcessPid == _pid && _host.MainThreadId == _thread && tid == _thread,
        "Menu observation lost its owned stopped process/main thread.");
    private void Emit(string name, object data)
    { ++_events; _log(new { @event = "native-menu-input-observation", name, observedAtMilliseconds = Environment.TickCount64, data }); }
    private byte B(uint address) => Read(address, 1)[0];
    private uint U(uint address) => Win32Native.ReadUInt32(_process, address);
    private uint P(uint address)
    { uint value = U(address); Require(value >= 0x10000 && (value & 3) == 0, "Menu observation borrowed pointer is invalid."); return value; }
    private byte[] Read(uint address, int count) => Win32Native.ReadMemory(_process, address, count);
    private byte[] Image(uint address, int count) => _image.GetSectionData(checked((int)(address - 0x400000))).GetContent(0, count).ToArray();
    private uint OriginalCallTarget(uint site)
    { byte[] code = Image(site, 5); Require(code[0] == 0xE8, "Expected original direct native CALL.");
      return unchecked(site + 5 + (uint)BitConverter.ToInt32(code, 1)); }
    private static string Hex(uint value) => $"0x{value:X8}";
    [DllImport("user32.dll")] private static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
    private static void Require(bool value, string message)
    { if (!value) throw new InvalidDataException(message); }
}
