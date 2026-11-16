using System.Reflection.PortableExecutable;
using System.Security.Cryptography;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// Passive original Playing/Battle Exit entry-to-caller-return pairing. The
/// existing unload guard supplies the entry; this owner adds no entry patch,
/// native call, context mutation, resource release or new-scene readiness claim.
/// </summary>
internal sealed class NativeProgressionExitObservation
{
    private const uint PlayingExit = 0x5F86C0, BattleExit = 0x5F2300;
    private const uint PlayingTable = 0x710874, BattleTable = 0x7105C8, FlowTable = 0x702D2C;
    private const uint ReplaceCaller = 0x596617, PopCaller = 0x5966B0, SinglePopCaller = 0x596574, NestedBattleBaseCaller = 0x600738;
    private const uint MaximumPairs = 64, MaximumDuplicateEntries = 256;
    private readonly DebuggerHost _host;
    private readonly PEReader _image;
    private readonly Action<object> _log;
    private readonly IntPtr _process;
    private readonly uint _pid, _thread;
    private readonly uint _playingLevel;
    private readonly int _owner;
    private bool _retired, _failed;
    private uint _entries, _returns, _duplicates, _nestedBaseEntries;
    private string _failure = "";
    private ExitFrame? _pending, _lastReturned;

    private sealed record ExitFrame(uint Sequence, uint Entry, uint Level, uint Receiver, uint Table,
        uint Flow, uint Esp, uint Caller, uint Argument, uint Eax, uint Edx, uint Ebx, uint Ebp,
        uint Esi, uint Edi, uint EFlags, uint EntryStack, uint EntryCurrent, uint EntryPending);

    internal NativeProgressionExitObservation(DebuggerHost host, PEReader image, Action<object> log,
        uint playingLevel = 28, ProgressionControlPermit? progressionControl = null, CampaignSceneProfile? campaign = null)
    {
        ArgumentNullException.ThrowIfNull(host); ArgumentNullException.ThrowIfNull(image); ArgumentNullException.ThrowIfNull(log);
        Require(host.IsAtDebugStop && host.ProcessHandle != IntPtr.Zero && host.ProcessPid != 0 &&
            host.MainThreadId != 0 && host.ImageBase == 0x400000,
            "Progression Exit observation requires its owned original main-thread debug stop.");
        Require(campaign?.Admits(playingLevel) == true || playingLevel == 28 || playingLevel == 3 &&
            progressionControl is { OwnerPid: > 0, OwnerCreationFileTime: > 0 },
            "Story3 Exit observation requires its explicit closed progression owner permit.");
        _playingLevel = playingLevel;
        Require(Convert.ToHexString(SHA256.HashData(image.GetEntireImage().GetContent().ToArray())) ==
            "3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F",
            "Progression Exit observation requires the pinned original PC executable.");
        _host = host; _image = image; _log = log; _process = host.ProcessHandle;
        _pid = host.ProcessPid; _thread = host.MainThreadId; _owner = Environment.CurrentManagedThreadId;
        VerifyImageExtent(PlayingExit, 0x490, "B501505FFF179E2B993018B29B57278314C6C9C18F835B450BBA1B850BF0D8F9");
        VerifyImageExtent(BattleExit, 0x44, "C966ED313FC18AB425A85C1187C0CC13879B7ACC108B62F84A257F905B21D45C");
        PinImage(0x5F8B4D, "C20400"); PinImage(0x5F2341, "C20400");
        PinImage(ReplaceCaller - 4, "50FF52248BCEE822FBFFFF");
        PinImage(PopCaller - 4, "50FF52248BCEE889FAFFFF");
        PinImage(0x596559, "8B8EAC01000085C9500F8C8D0000008B8C8E5C0100008B11FF52248BCEE8C5FBFFFF");
        PinImage(NestedBattleBaseCaller - 12, "8B54240C528BCEE8887FFFFF");
        PinImage(0x5F5450, "B8988C7600C3"); PinImage(0x5F2080, "B8708B7600C3");
        Require(ImageWord(PlayingTable + 0x24) == PlayingExit && ImageWord(BattleTable + 0x24) == BattleExit &&
            ImageWord(PlayingTable + 0x10) == 0x5F5450 && ImageWord(BattleTable + 0x10) == 0x5F2080,
            "Progression Exit original vtable/getter slots differ.");
    }

    internal bool HasPending => _pending is not null;
    internal bool Healthy => !_failed;
    internal uint ActualOriginalExitReturns => _returns;
    internal object Summary => new
    {
        retired = _retired, failed = _failed, failure = _failure, pendingOriginalExitReturn = HasPending,
        actualExitEntries = _entries, actualOriginalExitReturns = _returns,
        exactDeferredEntryDuplicates = _duplicates, knownNestedBattleBaseEntries = _nestedBaseEntries,
        maximumPairs = MaximumPairs, maximumExactDuplicateEntries = MaximumDuplicateEntries,
        lastReturned = _lastReturned is { } frame ? new
        {
            sequence = frame.Sequence, level = frame.Level, entry = Hex(frame.Entry),
            caller = Hex(frame.Caller), entryEsp = Hex(frame.Esp), returnedEsp = Hex(checked(frame.Esp + 8)),
            nativeRet4FrameMatched = true, calleeSavedRegistersMatched = true,
            noHistoricalReceiverOrSceneReadAfterReturn = true
        } : null,
        entryBreakpointAdded = false, nativeCallsAdded = 0, nativeContextWrites = 0,
        originalExitReturnIsLaterResourceRelease = false, nextLevelOrMainMenuReadyClaimed = false,
        returnProbeCollisionPolicy = "fail closed if exact caller already has any breakpoint owner",
        returnProbeRestorationOwner = "SoftwareBreakpointManager"
    };

    // Root calls this before its cleanup can redirect the saved entry context.
    // A deferred native batch may present precisely the same entry a second time;
    // that is one original invocation and must never add another return probe.
    internal void ObserveExitEntered(uint tid, uint entry, in Win32Native.X86Context context)
    {
        if (entry is not (PlayingExit or BattleExit)) return;
        try
        {
            EnsureStop(tid); Require(!_retired && !_failed && context.Eip == entry &&
                context.Ecx >= 0x10000 && (context.Ecx & 3) == 0 &&
                context.Esp >= 0x10000 && (context.Esp & 3) == 0,
                "Progression Exit entry lost its owned instruction/receiver/stack boundary.");
            uint table = U(context.Ecx), caller = U(context.Esp);
            if (entry == PlayingExit && table == BattleTable)
            {
                ExitFrame outer = _pending ?? throw new InvalidDataException("Nested Battle base Exit lacks its actual original outer entry.");
                Require(outer.Entry == BattleExit && outer.Receiver == context.Ecx &&
                    caller == NestedBattleBaseCaller && context.Esp < outer.Esp,
                    "Playing Exit with Battle receiver is not the known nested original Battle base call.");
                VerifyLivePin(NestedBattleBaseCaller - 12, "8B54240C528BCEE8887FFFFF");
                ++_nestedBaseEntries;
                _log(new { @event = "progression-exit-known-nested-base", sequence = outer.Sequence,
                    entry = Hex(entry), originalOuterEntry = Hex(BattleExit), caller = Hex(caller),
                    independentCompletionClaimed = false, nativeContextWrites = 0 });
                return;
            }
            uint level = entry == BattleExit ? 47u : _playingLevel;
            uint expectedTable = entry == BattleExit ? BattleTable : PlayingTable;
            uint flow = P(0x755294), stack = U(flow + 0x1AC), argument = U(checked(context.Esp + 4));
            Require(table == expectedTable && U(context.Ecx + 0x10) == level && U(table + 0x24) == entry &&
                U(flow) == FlowTable && stack < 20 && U(flow + 0x15C + 4 * stack) == context.Ecx &&
                caller is ReplaceCaller or PopCaller or SinglePopCaller && context.Edx == table && context.Eax == argument,
                "Progression Exit receiver, retained Flow top, original virtual caller or pushed argument differs.");
            VerifyLiveExtent(entry, entry == BattleExit ? 0x44 : 0x490);
            VerifyCallerPin(caller);
            var frame = new ExitFrame(checked(_entries + 1), entry, level, context.Ecx, table, flow, context.Esp,
                caller, argument, context.Eax, context.Edx, context.Ebx, context.Ebp, context.Esi, context.Edi,
                context.EFlags, stack, U(flow + 0x1B0), U(flow + 0x1B8));
            if (_pending is { } pending)
            {
                Require(frame with { Sequence = pending.Sequence } == pending && ++_duplicates <= MaximumDuplicateEntries,
                    "Progression Exit nested/repeated entry is not exactly its pending deferred original frame.");
                _log(new { @event = "progression-exit-deferred-entry-duplicate", sequence = pending.Sequence,
                    entry = Hex(entry), caller = Hex(caller), originalInvocationCountUnchanged = true,
                    returnProbeAdded = false, nativeContextWrites = 0 });
                return;
            }
            Require(_entries < MaximumPairs && !_host.Breakpoints.Contains(caller),
                "Progression Exit caller probe is already owned or its bounded pair inventory is exhausted.");
            _entries = frame.Sequence; _pending = frame;
            _host.Breakpoints.AddReturn(caller, tid, "Progression.original-exit-return", Returned);
            _log(new { @event = "progression-original-exit-entered", sequence = frame.Sequence,
                entry = Hex(entry), level, receiver = Hex(frame.Receiver), table = Hex(frame.Table), flow = Hex(flow),
                caller = Hex(caller), entryEsp = Hex(frame.Esp), argumentRaw = argument,
                currentRaw = frame.EntryCurrent, pendingRaw = frame.EntryPending, stack,
                expectedReturnEspDelta = 8, entryDoesNotClaimCompletedExit = true, nativeContextWrites = 0 });
        }
        catch (Exception error) { Fail(error); throw; }
    }

    private void Returned(uint tid, uint at, ref Win32Native.X86Context context)
    {
        try
        {
            EnsureStop(tid);
            ExitFrame frame = _pending ?? throw new InvalidDataException("Progression Exit return lost its exact entry frame.");
            Require(!_retired && !_failed && at == frame.Caller && context.Eip == frame.Caller &&
                context.Esp == checked(frame.Esp + 8) && context.Ebx == frame.Ebx && context.Ebp == frame.Ebp &&
                context.Esi == frame.Esi && context.Edi == frame.Edi,
                "Original progression Exit did not return through its paired RET4/callee-saved frame.");
            VerifyCallerPin(frame.Caller);
            // Only process-lifetime globals and this surviving typed Flow are
            // read. The historical receiver/player/scene is never dereferenced.
            uint flow = P(0x755294); Require(flow == frame.Flow && U(flow) == FlowTable,
                "Original progression Exit returned to another current Flow owner.");
            uint stack = U(flow + 0x1AC); Require(stack < 20 || stack == uint.MaxValue,
                "Progression Exit return current Flow stack is outside its original finite/EMPTY domain.");
            _pending = null; _lastReturned = frame; ++_returns;
            _log(new { @event = "progression-original-exit-returned", sequence = frame.Sequence,
                entry = Hex(frame.Entry), level = frame.Level, caller = Hex(frame.Caller),
                entryEsp = Hex(frame.Esp), returnedEsp = Hex(context.Esp), nativeRet4FrameMatched = true,
                calleeSavedRegistersMatched = true, currentFlow = Hex(flow), stack,
                currentRaw = U(flow + 0x1B0), pendingRaw = U(flow + 0x1B8),
                currentEngineGlobal = Hex(U(0x755274)), currentProfileGlobal = Hex(U(0x765AD4)),
                noHistoricalReceiverOrSceneRead = true, laterWorldResourceReleaseClaimed = false,
                newSceneEntryOrReadyClaimed = false, nativeContextWrites = 0 });
        }
        catch (Exception error) { Fail(error); throw; }
    }

    internal void RetireCached()
    {
        EnsureOwner(); if (_retired) return;
        Require(_pending is null, "Progression Exit observation cannot retire an unmatched native invocation.");
        _retired = true;
        _log(new { @event = "progression-exit-observation-retired", actualExitEntries = _entries,
            actualOriginalExitReturns = _returns, pendingOriginalExitReturn = false, remoteReads = 0 });
    }
    private void VerifyImageExtent(uint address, int bytes, string hash) => Require(
        Convert.ToHexString(SHA256.HashData(Image(address, bytes))) == hash,
        "Original progression Exit exact visible extent hash differs.");
    private void VerifyCallerPin(uint caller)
    {
        switch (caller)
        {
            case ReplaceCaller: VerifyLivePin(caller - 4, "50FF52248BCEE822FBFFFF"); break;
            case PopCaller: VerifyLivePin(caller - 4, "50FF52248BCEE889FAFFFF"); break;
            case SinglePopCaller:
                VerifyLivePin(0x596559, "8B8EAC01000085C9500F8C8D0000008B8C8E5C0100008B11FF52248BCEE8C5FBFFFF"); break;
            default: throw new InvalidDataException("Unknown original progression Exit caller.");
        }
    }
    private void PinImage(uint address, string hex) => Require(Image(address, hex.Length / 2)
        .SequenceEqual(Convert.FromHexString(hex)), "Original progression Exit ABI image pin differs.");
    private void VerifyLivePin(uint address, string hex)
    {
        byte[] expected = Convert.FromHexString(hex); PinImage(address, hex);
        Require(NormalizedLive(address, expected.Length).SequenceEqual(expected), "Original progression Exit ABI live bytes differ.");
    }
    private void VerifyLiveExtent(uint address, int bytes) => Require(NormalizedLive(address, bytes).SequenceEqual(Image(address, bytes)),
        "Original progression Exit live visible extent changed outside owned breakpoints.");
    private byte[] NormalizedLive(uint address, int bytes)
    {
        byte[] actual = Read(address, bytes), expected = Image(address, bytes);
        for (int i = 0; i < actual.Length; ++i)
            if (actual[i] == 0xCC && expected[i] != 0xCC && _host.Breakpoints.Contains(checked(address + (uint)i))) actual[i] = expected[i];
        return actual;
    }
    private void EnsureOwner() => Require(Environment.CurrentManagedThreadId == _owner,
        "Progression Exit observation changed its synchronous owner thread.");
    private void EnsureStop(uint tid)
    {
        EnsureOwner(); Require(_host.IsAtDebugStop && _host.ProcessHandle == _process && _host.ProcessPid == _pid &&
            _host.MainThreadId == _thread && tid == _thread && _host.ImageBase == 0x400000,
            "Progression Exit observation lost its owned original main-thread stop.");
    }
    private uint U(uint address) => BitConverter.ToUInt32(Read(address, 4));
    private uint P(uint address) { uint value = U(address); Require(value >= 0x10000 && (value & 3) == 0,
        "Progression Exit current pointer is null, low or unaligned."); return value; }
    private byte[] Read(uint address, int bytes) => Win32Native.ReadMemory(_process, address, bytes);
    private byte[] Image(uint address, int bytes) => _image.GetSectionData(checked((int)(address - 0x400000))).GetContent(0, bytes).ToArray();
    private uint ImageWord(uint address) => BitConverter.ToUInt32(Image(address, 4));
    private void Fail(Exception error) { _failed = true; _failure = error.Message; }
    private static string Hex(uint value) => $"0x{value:X8}";
    private static void Require(bool value, string message) { if (!value) throw new InvalidDataException(message); }
}
