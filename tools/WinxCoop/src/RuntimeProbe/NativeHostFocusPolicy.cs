using System.Reflection.PortableExecutable;
using System.Security.Cryptography;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// Explicit bounded Host-network-fixture policy at the original focus-pause
/// calls and their matched focus-start call. The caller supplies a cached own-body readiness predicate and retires
/// this policy before native unload. This is not a general background-play mode.
/// </summary>
internal sealed class NativeHostFocusPolicy
{
    private const uint PollPause = 0x40E680, MessagePause = 0x40E7FD, MessageStart = 0x40E886;
    private const uint TimerGlobal = 0x755298, TimerTable = 0x702F94, PauseFunction = 0x596900, StartFunction = 0x596910;
    private const uint InputGlobal = 0x7552A0, InputTable = 0x6F5508, CommandTable = 0x703090;
    private const uint MaximumCallbacks = 50000, MaximumDetailedLogs = 8;
    private static readonly byte[] PauseCall = [0xFF, 0x52, 0x24];
    private static readonly byte[] StartCall = [0xFF, 0x52, 0x20];
    private static readonly byte[] StartCaller = Convert.FromHexString("8B108BC8FF52208B44241C8B4C2414505751538BCE");
    private readonly DebuggerHost _host;
    private readonly Action<object> _log;
    private readonly Func<bool> _cachedHostOwnBodyReady;
    private readonly IntPtr _process;
    private readonly uint _pid, _thread;
    private readonly int _owner;
    private bool _installationAttempted, _installed, _retired, _failed;
    private bool _pollPending, _messagePending, _startPending, _qualifiedFocusPauseOutstanding;
    private uint _callbacks, _pollSkips, _messageSkips, _unreadyPassThrough, _pausedPassThrough, _retiredPassThrough;
    private uint _nonNeutralPassThrough;
    private uint _startSkips, _unpairedStartPassThrough, _unreadyStartPassThrough, _pausedStartPassThrough, _inactiveStartPassThrough;
    private uint _startDetailedLogs;
    private uint _detailedLogs, _lastStop, _timer, _input, _command;
    private string _failure = "";

    internal NativeHostFocusPolicy(DebuggerHost host, PEReader image, Action<object> log,
        Func<bool> cachedHostOwnBodyReady)
    {
        ArgumentNullException.ThrowIfNull(host); ArgumentNullException.ThrowIfNull(image);
        ArgumentNullException.ThrowIfNull(log); ArgumentNullException.ThrowIfNull(cachedHostOwnBodyReady);
        Require(host.IsAtDebugStop && host.ProcessHandle != IntPtr.Zero && host.ProcessPid != 0 &&
            host.MainThreadId != 0 && host.ImageBase == 0x400000,
            "Host focus policy requires its pinned owned main-thread debug session.");
        Require(Convert.ToHexString(SHA256.HashData(image.GetEntireImage().GetContent().ToArray())) ==
            "3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F",
            "Host focus policy requires the exact original PE.");
        foreach (uint site in new[] { PollPause, MessagePause })
            Require(Image(image, site, 3).SequenceEqual(PauseCall),
                "Host focus pause site is not the pinned FF5224 instruction.");
        Require(BitConverter.ToUInt32(Image(image, TimerTable + 0x24, 4)) == PauseFunction &&
            Image(image, PauseFunction, 9).SequenceEqual(new byte[] { 0xC6, 0x41, 0x40, 0x01, 0xE9, 0xC7, 0x9D, 0xEB, 0xFF }),
            "Host focus policy timer slot/body differs from the original pause implementation.");
        Require(Image(image, MessageStart - 4, StartCaller.Length).SequenceEqual(StartCaller) &&
            BitConverter.ToUInt32(Image(image, TimerTable + 0x20, 4)) == StartFunction &&
            Image(image, StartFunction, 9).SequenceEqual(Convert.FromHexString("C6414000E9279FEBFF")) &&
            Image(image, 0x450860, 22).SequenceEqual(Convert.FromHexString("A170F6750033D2F73550E07400C64618018946205EC3")),
            "Host focus start caller, ignored return value, timer slot or native time-base reset differs.");
        _host = host; _log = log; _cachedHostOwnBodyReady = cachedHostOwnBodyReady;
        _process = host.ProcessHandle; _pid = host.ProcessPid; _thread = host.MainThreadId;
        _owner = Environment.CurrentManagedThreadId;
    }

    internal object Summary => new
    {
        enabledOnlyByExplicitHostFixtureComposition = true,
        installationAttempted = _installationAttempted, installed = _installed, retired = _retired,
        failed = _failed, failure = _failure, callbacks = _callbacks,
        getFocusPollPauseSkips = _pollSkips, killFocusMessagePauseSkips = _messageSkips,
        unreadyNativePassThrough = _unreadyPassThrough, alreadyPausedNativePassThrough = _pausedPassThrough,
        nonNeutralNativePassThrough = _nonNeutralPassThrough,
        qualifiedFocusPauseOutstanding = _qualifiedFocusPauseOutstanding,
        pairedFocusStartSkips = _startSkips, unpairedStartNativePassThrough = _unpairedStartPassThrough,
        unreadyStartNativePassThrough = _unreadyStartPassThrough, pausedStartNativePassThrough = _pausedStartPassThrough,
        inactiveStartNativePassThrough = _inactiveStartPassThrough, startDetailedLogs = _startDetailedLogs,
        retiredNativePassThrough = _retiredPassThrough, pollReturnProbePending = _pollPending,
        messageReturnProbePending = _messagePending, startReturnProbePending = _startPending, detailedLogs = _detailedLogs,
        maximumCallbacks = MaximumCallbacks, lastQualifiedDebugStop = _lastStop,
        timer = Hex(_timer), primaryInput = Hex(_input), primaryCommand = Hex(_command),
        sites = new[] { new { site = Hex(PollPause), next = Hex(PollPause + 3) },
            new { site = Hex(MessagePause), next = Hex(MessagePause + 3) },
            new { site = Hex(MessageStart), next = Hex(MessageStart + 3) } },
        originalCallBytes = "FF5224", callbackContextWrites = new[] { "EIP" },
        originalFocusStartCallBytes = "FF5220", focusStartReturnValueOverwrittenByOriginalMovEax = true,
        timerWrites = 0, inputEnableWrites = 0, primaryCommandWrites = 0,
        deadByteReadOrWritten = false, forcedResumeCalls = 0,
        unreadyOrRetiredReadsBodyGraph = false, wholeAltTabCompatibilityClaimed = false
    };

    internal void InstallStopped()
    {
        EnsureStop(_thread);
        Require(!_installationAttempted && !_retired && !_failed,
            "Host focus policy installation is single-use.");
        _installationAttempted = true;
        try
        {
            // Validate all original complete instructions before publishing any probe.
            Require(Read(PollPause, 3).SequenceEqual(PauseCall) && Read(MessagePause, 3).SequenceEqual(PauseCall) &&
                Read(MessageStart - 4, StartCaller.Length).SequenceEqual(StartCaller),
                "Host focus pause/start bytes changed before installation.");
            _installed = true;
            InstallRepeater(PollPause, "HostFocus.GetFocus.pause");
            InstallRepeater(MessagePause, "HostFocus.WM_KILLFOCUS.pause");
            InstallRepeater(MessageStart, "HostFocus.WM_SETFOCUS.matched-start");
            _log(new { @event = "host-focus-policy-installed", poll = Hex(PollPause), message = Hex(MessagePause),
                matchedStart = Hex(MessageStart), originalStart = "FF5220",
                original = "FF5224", processPid = _pid, mainThread = _thread,
                onlyExplicitHostNetworkFixture = true, timerWrites = 0, inputEnableWrites = 0 });
        }
        catch (Exception error)
        {
            _failed = true; _retired = true; _failure = error.Message;
            throw;
        }
    }

    internal void RetireCached()
    {
        EnsureOwner();
        if (_retired) return;
        _retired = true; _qualifiedFocusPauseOutstanding = false;
        // No remote read, input write, timer operation or forced resume here.
        // Each remaining one-shot callback returns normally on its next hit;
        // the existing breakpoint owner also restores it during session cleanup.
        _log(new { @event = "host-focus-policy-retired", pollSkips = _pollSkips,
            messageSkips = _messageSkips, noRemoteReads = true,
            pendingPoll = _pollPending, pendingMessage = _messagePending, pendingStart = _startPending,
            pairedStartSkips = _startSkips, qualifiedFocusPauseOutstanding = false });
    }

    private void InstallRepeater(uint site, string name)
    {
        BreakpointCallback repeat = null!;
        repeat = (uint tid, uint at, ref Win32Native.X86Context context) =>
        {
            SetPending(site, false);
            try
            {
                if (site == MessageStart) ObserveMatchedStart(tid, at, ref context);
                else Observe(tid, at, site, ref context);
            }
            catch (Exception error)
            {
                _failed = true; _retired = true; _qualifiedFocusPauseOutstanding = false; _failure = error.Message;
                throw;
            }
            if (!_retired && !_failed)
            {
                _host.Breakpoints.AddReturn(site, _thread, name, repeat);
                SetPending(site, true);
            }
        };
        _host.Breakpoints.AddReturn(site, _thread, name, repeat);
        SetPending(site, true);
    }

    private void Observe(uint tid, uint at, uint site, ref Win32Native.X86Context context)
    {
        EnsureOwner();
        if (_retired || _failed) { ++_retiredPassThrough; return; }
        // The predicate MUST be cached; never follow stale native body owners
        // merely to decide whether this focus-only policy is still eligible.
        if (!_cachedHostOwnBodyReady()) { _qualifiedFocusPauseOutstanding = false; ++_unreadyPassThrough; return; }
        EnsureStop(tid);
        Require(_installed && tid == _thread && at == site && context.Eip == site &&
            site is PollPause or MessagePause && _host.DebugStopSequence > _lastStop &&
            ++_callbacks <= MaximumCallbacks,
            "Host focus callback escaped its exact site/thread/observation bound.");
        Require(Read(site, 3).SequenceEqual(PauseCall), "Host focus callback no longer sees the original CALL bytes.");
        uint timer = Pointer(TimerGlobal);
        Require(U(timer) == TimerTable && context.Ecx == timer && context.Edx == TimerTable &&
            U(TimerTable + 0x24) == PauseFunction,
            "Host focus timer receiver, vtable or native pause target changed.");
        byte paused = Read(timer + 0x40, 1)[0];
        Require(paused <= 1, "Host focus timer pause byte is outside its admitted domain.");
        if (paused != 0) { _qualifiedFocusPauseOutstanding = false; ++_pausedPassThrough; return; }
        uint input = Pointer(InputGlobal);
        Require(U(input) == InputTable, "Host focus primary UserInput type changed.");
        uint command = Pointer(input + 0x18);
        byte[] values = Read(command, 0x22); // Deliberately excludes dead-byte5F.
        Require(BitConverter.ToUInt32(values) == CommandTable,
            "Host focus primary command type changed.");
        if (BitConverter.ToSingle(values, 4) != 0f || values[0x1A] != 0 || values[0x20] != 0 || values[0x21] != 0)
        {
            _qualifiedFocusPauseOutstanding = false;
            ++_nonNeutralPassThrough;
            if (_nonNeutralPassThrough <= MaximumDetailedLogs)
                _log(new { @event = "host-focus-nonneutral-native-pause-passthrough", site = Hex(site),
                    primaryMagnitude = BitConverter.ToSingle(values, 4), primaryJump = values[0x1A],
                    primaryOrdinary = values[0x20], primaryCharged = values[0x21],
                    originalCallPreserved = true, contextWrites = 0, primaryCommandWrites = 0 });
            return;
        }
        Require(Read(input + 0x20, 1)[0] == 0,
            "Host focus bypass must preserve the preceding native input-disable operation.");
        Require(_timer == 0 || _timer == timer && _input == input && _command == command,
            "Host focus live timer/input/command owners changed inside this bounded policy.");
        _timer = timer; _input = input; _command = command; _lastStop = _host.DebugStopSequence;
        // FF5224 has no explicit stack arguments. Only this EIP redirects; no
        // ESP/GP/flags/FP field, native timer byte or command byte is written.
        context.Eip = checked(site + 3);
        _qualifiedFocusPauseOutstanding = true;
        if (site == PollPause) ++_pollSkips; else ++_messageSkips;
        if (_detailedLogs < MaximumDetailedLogs)
        {
            ++_detailedLogs;
            _log(new { @event = "host-focus-native-pause-skipped", site = Hex(site), next = Hex(context.Eip),
                debugStop = _lastStop, timer = Hex(timer), input = Hex(input), command = Hex(command),
                primaryMagnitude = BitConverter.ToSingle(values, 4), primaryJump = values[0x1A],
                primaryOrdinary = values[0x20], primaryCharged = values[0x21],
                originalEsp = Hex(context.Esp), onlyEipChangedByCallback = true,
                nativeInputRemainsDisabled = true, timerWrites = 0, privateCommandWrites = 0 });
        }
    }

    private void ObserveMatchedStart(uint tid, uint at, ref Win32Native.X86Context context)
    {
        EnsureOwner();
        if (_retired || _failed) { ++_retiredPassThrough; return; }
        if (!_qualifiedFocusPauseOutstanding) { ++_unpairedStartPassThrough; return; }
        if (!_cachedHostOwnBodyReady())
        {
            _qualifiedFocusPauseOutstanding = false; ++_unreadyStartPassThrough; return;
        }
        EnsureStop(tid);
        Require(_installed && at == MessageStart && context.Eip == at && _host.DebugStopSequence > _lastStop &&
            ++_callbacks <= MaximumCallbacks && Read(MessageStart - 4, StartCaller.Length).SequenceEqual(StartCaller),
            "Matched Host focus start escaped its exact original caller/thread/bound.");
        uint timer = Pointer(TimerGlobal);
        Require(timer == _timer && U(timer) == TimerTable && context.Ecx == timer && context.Edx == TimerTable &&
            U(TimerTable + 0x20) == StartFunction,
            "Matched Host focus start lost its prior qualified timer receiver/slot.");
        byte paused = Read(timer + 0x40, 1)[0];
        Require(paused <= 1, "Matched Host focus start timer pause byte exceeds0/1.");
        if (paused != 0)
        {
            _qualifiedFocusPauseOutstanding = false; ++_pausedStartPassThrough; return;
        }
        byte[] before = Read(timer + 0x18, 0x14);
        Require(before[0] <= 1, "Matched Host focus start timer active byte exceeds0/1.");
        if (before[0] != 1)
        {
            _qualifiedFocusPauseOutstanding = false; ++_inactiveStartPassThrough; return;
        }
        uint input = Pointer(InputGlobal), command = Pointer(input + 0x18);
        Require(input == _input && command == _command && U(input) == InputTable && U(command) == CommandTable &&
            Read(input + 0x20, 1)[0] == 1,
            "Matched Host focus start lost original primary owners or preceding native input-enable.");
        // Original WM_SETFOCUS enables input before this no-argument call.
        // Its next instruction overwrites EAX, so no synthetic return value is
        // needed. Preserve the running timer's time base when its matching
        // focus Pause was deliberately skipped. Real/native UI pauses pass.
        uint originalEsp = context.Esp;
        context.Eip = MessageStart + 3;
        _qualifiedFocusPauseOutstanding = false; ++_startSkips; _lastStop = _host.DebugStopSequence;
        byte[] after = Read(timer + 0x18, 0x14);
        Require(before.SequenceEqual(after) && Read(timer + 0x40, 1)[0] == 0,
            "Matched Host focus start changed timer state at its owned stopped skip.");
        if (_startDetailedLogs < MaximumDetailedLogs)
        {
            ++_startDetailedLogs;
            _log(new { @event = "host-focus-matched-native-start-skipped", site = Hex(MessageStart), next = Hex(context.Eip),
                debugStop = _lastStop, timer = Hex(timer), input = Hex(input), command = Hex(command),
                priorQualifiedFocusPauseSkipped = true, qualifiedFocusPauseOutstanding = false,
                before = TimerState(before), after = TimerState(after), nativePaused40 = paused,
                actualTimerStateUnchangedAtStop = true, originalEsp = Hex(originalEsp),
                onlyEipChangedByCallback = true, nativeInputRemainsEnabled = true,
                pairedFocusStartSkips = _startSkips, returnValueOverwrittenByOriginalNextInstruction = true,
                nativeCallsAdded = 0, timerWrites = 0, primaryCommandWrites = 0 });
        }
    }

    private static object TimerState(byte[] state) => new
    {
        rawActive18 = state[0], rawRelative19 = state[1],
        current1C = BitConverter.ToUInt32(state, 4), start20 = BitConverter.ToUInt32(state, 8),
        paused24 = BitConverter.ToUInt32(state, 12), delta28Bits = BitConverter.ToUInt32(state, 16).ToString("X8"),
        delta28FloatText = BitConverter.ToSingle(state, 16).ToString("R", System.Globalization.CultureInfo.InvariantCulture)
    };

    private void SetPending(uint site, bool value)
    {
        if (site == PollPause) _pollPending = value;
        else if (site == MessagePause) _messagePending = value;
        else { Require(site == MessageStart, "Unknown Host focus probe site."); _startPending = value; }
    }
    private uint Pointer(uint address)
    { uint value = U(address); Require(value >= 0x10000 && (value & 3) == 0, "Host focus pointer is invalid or unaligned."); return value; }
    private uint U(uint address) => BitConverter.ToUInt32(Read(address, 4));
    private byte[] Read(uint address, int count)
    {
        EnsureStop(_thread);
        Require(!_retired && !_failed && address >= 0x10000 && count is >= 1 and <= 0x100 &&
            (ulong)address + (uint)count <= uint.MaxValue, "Host focus read is retired or outside its x86 bound.");
        return Win32Native.ReadMemory(_process, address, count);
    }
    private void EnsureOwner() => Require(Environment.CurrentManagedThreadId == _owner,
        "Host focus policy has one debugger-owner thread.");
    private void EnsureStop(uint tid)
    {
        EnsureOwner();
        Require(_host.IsAtDebugStop && tid == _thread && _host.MainThreadId == _thread &&
            _host.ProcessHandle == _process && _host.ProcessPid == _pid && _host.ImageBase == 0x400000,
            "Host focus policy requires its captured owned main-thread debug stop.");
    }
    private static byte[] Image(PEReader image, uint address, int count) =>
        image.GetSectionData(checked((int)(address - 0x400000))).GetContent(0, count).ToArray();
    private static string Hex(uint value) => $"0x{value:X8}";
    private static void Require(bool condition, string message)
    { if (!condition) throw new InvalidDataException(message); }
}
