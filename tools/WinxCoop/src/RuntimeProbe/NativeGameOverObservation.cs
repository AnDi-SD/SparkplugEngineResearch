using System.Reflection.PortableExecutable;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// Passive, bounded story GameOver witness. Dying requests and challenge-end
/// notifications are not substituted for this native Entry/return/Flow proof.
/// </summary>
internal sealed class NativeGameOverObservation
{
    private const uint GameOverTable = 0x70F778, GameOverGetter = 0x5E0450, GameOverRecord = 0x768630;
    private const uint GameOverClass = 0x331018ED, Entry = 0x5E04F0, EntryRet4 = 0x5E05F5;
    private const uint Exit = 0x5F14B0, ExitRet4 = 0x5F15A1, PlayingTable = 0x710874;
    private readonly DebuggerHost _host;
    private readonly PEReader _image;
    private readonly Action<object> _log;
    private readonly int _ownerThread;
    private readonly List<object> _samples = [];
    private uint _flow, _initialStoryState, _storyLevel, _gameOver, _sequence, _worlds;
    private uint _entryBegun, _entryReturned, _exitBegun, _exitReturned, _gameOverWorlds, _retireEntry;
    private bool _bound, _retired, _failed, _storyGameOverObserved;
    private Pair? _pending;

    private sealed class Pair(uint id, string kind, uint receiver, Win32Native.X86Context c, uint caller)
    {
        internal readonly uint Id = id, Receiver = receiver, Esp = c.Esp, Caller = caller;
        internal readonly uint Esi = c.Esi, Edi = c.Edi, Ebx = c.Ebx, Ebp = c.Ebp;
        internal readonly string Kind = kind;
        internal bool NativeRet4Reached;
    }

    internal NativeGameOverObservation(DebuggerHost host, PEReader image, Action<object> log)
    {
        _host = host ?? throw new ArgumentNullException(nameof(host));
        _image = image ?? throw new ArgumentNullException(nameof(image));
        _log = log ?? throw new ArgumentNullException(nameof(log));
        _ownerThread = Environment.CurrentManagedThreadId;
    }

    internal bool Active => _bound && !_retired && !_failed;
    internal bool HasPendingNativePair => _pending is not null;
    internal bool StoryGameOverObserved => _storyGameOverObserved;
    internal bool StoryGameOverExitReturned => _exitBegun == 1 && _exitReturned == 1 && _pending is null;
    internal object Summary => new
    {
        bound = _bound, active = Active, retiredAtFlowBoundary = _retired, terminalFailure = _failed,
        storyLevel = _storyLevel, initialStoryState = Hex(_initialStoryState), flow = Hex(_flow),
        gameOver = Hex(_gameOver), entryBegun = _entryBegun, entryReturned = _entryReturned,
        exitBegun = _exitBegun, exitReturned = _exitReturned, pendingNativePairs = _pending is null ? 0 : 1,
        verifiedWorlds = _worlds, settledTypedGameOverWorlds = _gameOverWorlds,
        storyGameOverObserved = StoryGameOverObserved, retireEntry = Hex(_retireEntry), samples = _samples.ToArray(),
        nativeEntry = Hex(Entry), nativeEntryRet4 = Hex(EntryRet4), nativeExit = Hex(Exit), nativeExitRet4 = Hex(ExitRet4),
        gameOverTable = Hex(GameOverTable), gameOverClass = Hex(GameOverClass),
        gameWorldWrites = 0, flowOrInputWrites = 0, nativeCallsAdded = 0, privateCallPages = 0,
        queuedDyingRequestCountedAsGameOver = false, challengeCompletionCountedAsGameOver = false,
        characterOrProfileReads = 0, fullGameOverClassRecovered = false, fullE3Complete = false
    };

    internal void BindAtWorld(uint tid, ref Win32Native.X86Context context, uint storyLevel = 3)
    {
        Ensure(tid);
        Require(!_bound && !_retired && !_failed && context.Eip == NativeCallDispatcher.WorldReturn &&
            _host.ImageBase == 0x400000 && storyLevel == 3,
            "GameOver observation requires its first owned Gardenia03 WorldRET.");
        try
        {
            Pin(0x5F5450, "B8988C7600C3");
            Pin(GameOverGetter, "B830867600C3");
            Pin(0x6D7550, "6A006860065E0068108B760068C0F77000687A32730C68ED181033B930867600E87BBAD3FFC3");
            Pin(Entry, "8B44240457508BF9E8C30E0100");
            Pin(0x5E05EC, "C6402001C64725005FC20400");
            Pin(Exit, "A1D45A760083EC205333DB3BC3568BF1");
            Pin(0x5F1594, "8B481C8B01FF50205E5B83C420C20400");
            Require(OriginalU(GameOverTable + 0x10) == GameOverGetter && U(GameOverTable + 0x10) == GameOverGetter &&
                OriginalU(GameOverTable + 0x20) == Entry && U(GameOverTable + 0x20) == Entry &&
                OriginalU(GameOverTable + 0x24) == Exit && U(GameOverTable + 0x24) == Exit &&
                U(GameOverRecord) == GameOverClass && U(PlayingTable + 0x10) == 0x5F5450 &&
                U(0x768C98) == 0x70EB6440,
                "GameOver/story class getter, record or Entry/Exit slots differ.");
            _flow = Pointer(0x755294); _storyLevel = storyLevel;
            Flow flow = ReadFlow(); _initialStoryState = flow.Top;
            Require(flow.Current == storyLevel && flow.Pending == 0 && flow.TopTable == PlayingTable &&
                U(flow.Top + 0x10) == storyLevel, "GameOver observation is not bound to the settled story scene.");
            _bound = true;
            Hook(Entry, "8B44240457508BF9E8C30E0100", "entry", (uint rt, ref Win32Native.X86Context c) => Begin(rt, ref c, "entry"));
            Hook(EntryRet4, "C20400", "entry-ret4", (uint rt, ref Win32Native.X86Context c) => NativeRet(rt, ref c, "entry"));
            Hook(Exit, "A1D45A760083EC205333DB3BC3568BF1", "exit", (uint rt, ref Win32Native.X86Context c) => Begin(rt, ref c, "exit"));
            Hook(ExitRet4, "C20400", "exit-ret4", (uint rt, ref Win32Native.X86Context c) => NativeRet(rt, ref c, "exit"));
            Sample("bound", new { storyLevel, initialStoryState = Hex(_initialStoryState), flow,
                nativeGameOverGetter = Hex(GameOverGetter), nativeGameOverRecord = Hex(GameOverRecord), stateWrites = 0 });
        }
        catch { _failed = true; throw; }
    }

    internal void VerifyAtWorld(uint tid, ref Win32Native.X86Context context)
    {
        Ensure(tid);
        if (_retired) return;
        Require(Active && _pending is null && context.Eip == NativeCallDispatcher.WorldReturn && ++_worlds <= 360000,
            "GameOver observation reached WorldRET with an inactive or unmatched native pair.");
        Flow flow = ReadFlow();
        if (flow.TopTable != GameOverTable || flow.Pending != 0) return;
        Require(_gameOver != 0 && flow.Top == _gameOver && _entryBegun == 1 && _entryReturned == 1 && _exitBegun == 0,
            "Fresh GameOver Flow lacks its single exact native Entry return.");
        Require(U(GameOverRecord) == GameOverClass && U(GameOverTable + 0x10) == GameOverGetter &&
            flow.Current == U(flow.Top + 0x10), "Fresh GameOver Flow/type state differs from its native class.");
        ++_gameOverWorlds; _storyGameOverObserved = true;
        if (_gameOverWorlds <= 2)
            Sample("settled-native-game-over", new { flow, nativeStateId = U(flow.Top + 0x10),
                entryReturned = _entryReturned, actualTypedGameOver = true, initialStoryStateRead = false });
    }

    internal void RetireAtFlowBoundary(uint tid, ref Win32Native.X86Context context)
    {
        Ensure(tid);
        Require(Active && _pending is null && context.Eip is NativeCallDispatcher.WorldReturn or 0x5F86C0 or
            0x4E8AA0 or 0x4E8C80 or 0x59AE80 or 0x59B190,
            "GameOver observation must retire at an original story pre-unload boundary with no pending native pair.");
        Require(U(0x755294) == _flow, "GameOver observation lost its original Flow before retirement.");
        _retireEntry = context.Eip; _retired = true;
        Sample("retired-at-flow-boundary", new { entry = Hex(context.Eip), StoryGameOverObserved,
            pendingNativePairs = 0, historicalObjectReadsNowForbidden = true });
    }

    private void Begin(uint tid, ref Win32Native.X86Context c, string kind)
    {
        if (kind == "exit" && c.Ecx != _gameOver) return; // Shared Menu exit also belongs to other native states.
        Require(_pending is null && U(c.Ecx) == GameOverTable && U(0x755294) == _flow &&
            c.Esp >= 0x10000 && (c.Esp & 3) == 0, "GameOver Entry/Exit has an unowned or nested native frame.");
        uint caller = U(c.Esp);
        Require(caller is >= 0x401000 and < 0x6DA000 && !_host.Breakpoints.Contains(caller),
            "GameOver Entry/Exit return caller is outside its unshared native code boundary.");
        if (kind == "entry")
        {
            Require(_entryBegun == 0 && _entryReturned == 0 && _gameOver == 0,
                "This GameOver cohort permits only one original story death Entry.");
            _gameOver = c.Ecx; ++_entryBegun;
        }
        else
        {
            Require(_entryReturned == 1 && _exitBegun == 0, "GameOver Exit is not paired with its original Entry.");
            ++_exitBegun;
        }
        Pair pair = new(++_sequence, kind, c.Ecx, c, caller); _pending = pair;
        _host.Breakpoints.AddReturn(caller, tid, "NativeGameOverObservation." + kind + ".return",
            (uint rt, uint at, ref Win32Native.X86Context rc) => Returned(rt, at, ref rc, pair));
        Sample(kind + "-entry", new { sequence = pair.Id, receiver = Hex(c.Ecx),
            nativeStateId = U(c.Ecx + 0x10), entryEsp = Hex(c.Esp), caller = Hex(caller), argument = Hex(U(c.Esp + 4)),
            flow = ReadFlow(), nativeFrameNotModified = true });
    }

    private void NativeRet(uint tid, ref Win32Native.X86Context c, string kind)
    {
        if (kind == "exit" && _pending?.Kind != "exit") return; // Foreign parent Menu exit.
        Pair pair = _pending ?? throw new InvalidDataException("GameOver RET4 has no owned Entry frame.");
        Require(pair.Kind == kind && !pair.NativeRet4Reached && c.Esp == pair.Esp && U(c.Esp) == pair.Caller &&
            c.Esi == pair.Esi && c.Edi == pair.Edi && c.Ebx == pair.Ebx && c.Ebp == pair.Ebp &&
            U(pair.Receiver) == GameOverTable && (kind != "entry" || Read(pair.Receiver + 0x25, 1)[0] == 0),
            "GameOver native RET4 lost its receiver, caller, stack or callee-saved register witness.");
        pair.NativeRet4Reached = true;
        Sample(kind + "-native-ret4", new { sequence = pair.Id, receiver = Hex(pair.Receiver),
            nativeEsp = Hex(c.Esp), caller = Hex(pair.Caller), originalRetArgumentBytes = 4, nativeRetStillExecutes = true });
    }

    private void Returned(uint tid, uint at, ref Win32Native.X86Context c, Pair pair)
    {
        Ensure(tid);
        try
        {
            Require(Active && ReferenceEquals(_pending, pair) && pair.NativeRet4Reached && c.Eip == at &&
                at == pair.Caller && c.Esp == checked(pair.Esp + 8) && c.Esi == pair.Esi && c.Edi == pair.Edi &&
                c.Ebx == pair.Ebx && c.Ebp == pair.Ebp && U(pair.Receiver) == GameOverTable && U(0x755294) == _flow,
                "GameOver Entry/Exit return differs in native RET4, caller, ESP, receiver or lifetime.");
            if (pair.Kind == "entry") ++_entryReturned; else ++_exitReturned;
            _pending = null;
            Sample(pair.Kind + "-return", new { sequence = pair.Id, receiver = Hex(pair.Receiver),
                caller = Hex(at), nativeEsp = Hex(c.Esp), nativeEspDelta = 8, nativeRet4Witness = true });
        }
        catch { _failed = true; throw; }
    }

    private sealed record Flow(uint Current, uint Pending, uint Stack, uint Top, uint TopTable);
    private Flow ReadFlow()
    {
        Require(U(0x755294) == _flow, "GameOver observation Flow singleton identity changed.");
        uint stack = U(_flow + 0x1AC);
        Require(stack <= 20, "GameOver observation Flow stack exceeds its bounded native layout.");
        uint top = Pointer(_flow + 0x15C + stack * 4);
        return new(U(_flow + 0x1B0), U(_flow + 0x1B8), stack, top, U(top));
    }

    private delegate void Callback(uint tid, ref Win32Native.X86Context c);
    private void Hook(uint address, string hex, string name, Callback callback)
    {
        byte[] expected = Convert.FromHexString(hex); Pin(address, hex);
        Require(!_host.Breakpoints.Contains(address), "GameOver native hook is already owned by another observer.");
        BreakpointCallback repeat = null!;
        repeat = (uint tid, uint at, ref Win32Native.X86Context c) =>
        {
            Ensure(tid); if (_retired || _failed) return;
            Require(Active && at == address && c.Eip == at && Read(at, expected.Length).SequenceEqual(expected),
                "GameOver native hook signature or lifetime differs.");
            try { callback(tid, ref c); } catch { _failed = true; throw; }
            if (Active) _host.Breakpoints.AddReturn(address, tid, "NativeGameOverObservation." + name, repeat);
        };
        _host.Breakpoints.AddReturn(address, _host.MainThreadId, "NativeGameOverObservation." + name, repeat);
    }
    private void Pin(uint at, string hex)
    {
        byte[] expected = Convert.FromHexString(hex), original = _image.GetSectionData(checked((int)(at - 0x400000))).GetContent(0, expected.Length).ToArray();
        Require(original.SequenceEqual(expected) && Read(at, expected.Length).SequenceEqual(expected),
            $"GameOver observation original/live source differs at {Hex(at)}.");
    }
    private uint OriginalU(uint at) => BitConverter.ToUInt32(_image.GetSectionData(checked((int)(at - 0x400000))).GetContent(0, 4).ToArray());
    private void Sample(string name, object data)
    { if (_samples.Count >= 64) return; _samples.Add(new { name, data }); _log(new { @event = "native-game-over-observation", name, data }); }
    private uint Pointer(uint at) { uint value = U(at); Require(value >= 0x10000 && (value & 3) == 0, "GameOver native pointer is null or unaligned."); return value; }
    private uint U(uint at) => BitConverter.ToUInt32(Read(at, 4));
    private byte[] Read(uint at, int count)
    { Ensure(_host.MainThreadId); Require(at >= 0x10000 && count is > 0 and <= 256, "GameOver observation read exceeds its bound."); return Win32Native.ReadMemory(_host.ProcessHandle, at, count); }
    private void Ensure(uint tid) => Require(_host.IsAtDebugStop && tid == _host.MainThreadId && Environment.CurrentManagedThreadId == _ownerThread,
        "GameOver observation requires its owned main-thread debug stop.");
    private static string Hex(uint value) => $"0x{value:X8}";
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidDataException(message); }
}
