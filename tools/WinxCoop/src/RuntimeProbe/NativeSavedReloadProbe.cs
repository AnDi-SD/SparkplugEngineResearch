using System.Reflection.PortableExecutable;
using System.Security.Cryptography;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// One standalone Host Alfea28 save reload, through the complete original
/// Profile LoadSave gateway. Separate native entry scopes prove both initial
/// and reloaded worlds. This is not E5 body/transport recreation or Guest load.
/// </summary>
internal sealed class NativeSavedReloadProbe
{
    private const uint LoadSave = 0x4E7580, LoadRet4 = 0x4E7B88, RequestCall = 0x4E7796;
    private const uint ProfileTable = 0x6F5A44, FlowTable = 0x702D2C, PlayingTable = 0x710874;
    private readonly DebuggerHost _host;
    private readonly PEReader _image;
    private readonly Action<object> _log;
    private readonly NativeWorldEntryRouter _router;
    private readonly string _savedFile;
    private readonly IntPtr _process;
    private readonly uint _pid, _thread;
    private readonly int _owner;
    private NativeWorldEntrySeed? _initial, _reloaded;
    private NativeCallDispatcher? _activeCalls;
    private bool _installed, _retired, _failed, _complete, _mediaAdmitted;
    private bool _initialWorldQualified, _loadCalled, _entrySeen, _queueCallSeen, _ret4Seen, _loadReturned;
    private bool _oldExitSeenAfterLoad;
    private string _failure = "", _savedHash = "";
    private uint _worlds, _callWorld, _returnStop, _nativeResult, _profile, _initialFlow;
    private uint _worldCallEsp, _entryEsp, _entryCaller, _entryEbx, _entryEbp, _entryEsi, _entryEdi;
    private uint _initialPlaying, _initialScene, _initialRoot, _newPlaying, _newScene, _newRoot;
    private int _savedBytes;

    internal NativeSavedReloadProbe(DebuggerHost host, PEReader image, Action<object> log, string savedFile)
    {
        ArgumentNullException.ThrowIfNull(host); ArgumentNullException.ThrowIfNull(image); ArgumentNullException.ThrowIfNull(log);
        Require(host.IsAtDebugStop && host.ProcessHandle != IntPtr.Zero && host.ProcessPid != 0 &&
            host.MainThreadId != 0 && host.ImageBase == 0x400000,
            "Saved reload requires its owned original main-thread initial stop.");
        Require(Convert.ToHexString(SHA256.HashData(image.GetEntireImage().GetContent().ToArray())) ==
            "3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F",
            "Saved reload requires the exact original PC executable.");
        _savedFile = Path.GetFullPath(savedFile);
        Require(Path.IsPathFullyQualified(savedFile) && Path.GetFileName(_savedFile).Equals("Saved00.wxs", StringComparison.OrdinalIgnoreCase),
            "Saved reload is limited to an explicit isolated Saved00 slot path.");
        _host = host; _image = image; _log = log; _process = host.ProcessHandle;
        _pid = host.ProcessPid; _thread = host.MainThreadId; _owner = Environment.CurrentManagedThreadId;
        PinImage(LoadSave, "6AFFE9AF70F3FF"); PinImage(LoadRet4, "C20400");
        PinImage(0x4E778D, "8B4C24286A01518BC8E8B5DC0A008B54242C");
        _router = new(host, image, log, WorldStateRole.Host);
    }

    internal bool InitialWorldQualified => _initialWorldQualified;
    internal bool Started => _loadCalled;
    internal bool ModeCompleted => _complete;
    internal bool ModeSucceeded => _complete && !_failed && _mediaAdmitted && _initialWorldQualified &&
        _loadCalled && _entrySeen && _queueCallSeen && _ret4Seen && _loadReturned && _oldExitSeenAfterLoad &&
        _reloaded is { EntryCompleted: true, WorldQualified: true, Failed: false };
    internal object Summary => new
    {
        installed = _installed, retired = _retired, failed = _failed, failure = _failure,
        completed = ModeCompleted, succeeded = ModeSucceeded, role = "Host", level = 28,
        worlds = _worlds, callWorld = _callWorld, mediaVerified = _mediaAdmitted,
        initialWorldQualified = _initialWorldQualified, loadCalled = _loadCalled,
        originalLoadEntrySeen = _entrySeen, originalDestination28CallSeen = _queueCallSeen,
        originalRet4InstructionSeen = _ret4Seen, originalLoadReturned = _loadReturned,
        nativeLoadResult = _loadReturned ? _nativeResult : (uint?)null,
        actualOldExitReceiptSeenAfterLoad = _oldExitSeenAfterLoad,
        freshSavedFileSha256 = _savedHash, savedFileBytes = _savedBytes, nativeSlot = 0,
        nativeCallsRequested = _loadCalled ? 1 : 0, nativeGateway = Hex(LoadSave),
        originalSaveFileUnchangedThroughLoadRequired = true, saveCodecDuplicated = false,
        ownProfileOrPhysicsWrites = 0, guestLoadAllowed = false,
        initialScope = _initial?.Summary, reloadedScope = _reloaded?.Summary, processEntryRouter = _router.Summary,
        initialEpoch = 1, initialGeneration = 1, reloadedEpoch = 2, reloadedGeneration = 2,
        playingAddressReused = _newPlaying == 0 ? (bool?)null : _newPlaying == _initialPlaying,
        rootAddressReused = _newRoot == 0 ? (bool?)null : _newRoot == _initialRoot,
        sceneAddressReused = _newScene == 0 ? (bool?)null : _newScene == _initialScene,
        pointerInequalityUsedAsGenerationProof = false, historicalSceneReadAfterExit = false,
        allSavedFieldsRoundTripQualified = false, e5ControllersTransportOrSecondBodyRecreated = false,
        wholeCoopProgressionOrGameplayAccepted = false, completesOnlyAtLaterWorldAfterDispatcherRelease = true
    };

    internal void InstallStopped()
    {
        EnsureStop(_thread); Require(!_installed && !_retired && !_failed,
            "Saved reload installation is single-use.");
        foreach (uint site in new[] { LoadSave, LoadRet4, RequestCall })
            Require(!_host.Breakpoints.Contains(site), "Saved reload native observation site already has a breakpoint owner.");
        VerifyLivePins();
        _router.InstallStopped();
        _initial = new(_host, _image, _log, WorldStateRole.Host, 28, 1, 1,
            captureEntryOrdinal: 1, nativeOrdinalBase: 0, router: _router);
        _router.AttachScope(_initial);
        _installed = true;
        Repeat(LoadSave, "SavedReload.LoadSave.entry", OnLoadEntry);
        Repeat(LoadRet4, "SavedReload.LoadSave.ret4", OnLoadRet4);
        Repeat(RequestCall, "SavedReload.LoadSave.request-call", OnRequestCall);
        _log(new { @event = "native-saved-reload-installed", role = "Host", initialTargetOrdinal = 1,
            level = 28, wholeOriginalLoadSaveRequired = true, nativeCallsRequested = 0,
            originalFlowRequestEntryHookAdded = false, newProcessEntrySitesPublishedOnce = 7 });
    }

    internal void StepAtWorld(uint tid, ref Win32Native.X86Context context, NativeCallDispatcher calls,
        Action resume, bool mediaVerified, bool saveCompleted, bool actualOldExitReturned)
    {
        if (_retired || _complete || _failed) return;
        try
        {
            EnsureStop(tid);
            Require(_installed && context.Eip == NativeCallDispatcher.WorldReturn &&
                ++_worlds <= NativeObservationBudget.WorldLimit,
                "Saved reload left its installed bounded original WorldRET.");
            if (!mediaVerified || calls.Pending) return;
            _mediaAdmitted = true;
            Require(!calls.HasRetiredPage, "Saved reload World composition must release the retired dispatcher page first.");
            if (!_loadCalled)
            {
                Require(!actualOldExitReturned, "Saved reload initial generation already has an old Playing Exit receipt.");
                if (!_initialWorldQualified)
                {
                    if (_initial?.EntryCompleted != true) return;
                    if (!TryCurrentScene(1, 1, false, out SharedProfileScene initialScene, out uint root)) return;
                    _router.ObserveWorld(tid, ref context, initialScene);
                    NativeWorldEntrySeed initial = _initial ?? throw new InvalidDataException("Missing original initial entry scope.");
                    if (!initial.WorldQualified) return;
                    Require(initial.EntryCompleted && !initial.HasPendingOriginalEntry && _router.NativeEntryOrdinal == 1,
                        "Initial actual Alfea28 scope is not its one complete original entry.");
                    _initialWorldQualified = true; _initialPlaying = initialScene.PlayingState;
                    _initialScene = initialScene.Scene; _initialRoot = root;
                    _log(new { @event = "native-saved-reload-initial-world-qualified", level = 28,
                        actualNativeEntryOrdinal = 1, initialEpoch = 1, initialGeneration = 1,
                        nativeEntryAndPlacementAndFirstWorldRequired = true, saveOrLoadNotYetRequested = true });
                }
                if (!saveCompleted) return;
                if (!TryCurrentScene(1, 1, true, out SharedProfileScene retained, out uint retainedRoot)) return;
                Require(retained.PlayingState == _initialPlaying && retained.Scene == _initialScene && retainedRoot == _initialRoot,
                    "Save completed outside the original already-qualified retained Alfea28 scene.");
                BeginLoad(tid, ref context, calls, resume);
                return;
            }
            Require(_loadReturned && _entrySeen && _queueCallSeen && _ret4Seen &&
                _host.DebugStopSequence > _returnStop && _worlds > _callWorld,
                "Reload continuation is not a later native World after the complete original loader return.");
            if (actualOldExitReturned) _oldExitSeenAfterLoad = true;
            VerifySavedHash();
            if (!_oldExitSeenAfterLoad || _reloaded?.EntryCompleted != true ||
                !TryCurrentScene(2, 2, false, out SharedProfileScene current, out uint currentRoot)) return;
            _router.ObserveWorld(tid, ref context, current);
            NativeWorldEntrySeed reloaded = _reloaded ?? throw new InvalidDataException("Missing replacement native entry scope.");
            if (!reloaded.WorldQualified) return;
            Require(reloaded.EntryCompleted && !reloaded.HasPendingOriginalEntry && _router.NativeEntryOrdinal == 2 &&
                reloaded.NativeLocalEntryOrdinal == 2,
                "Reload did not complete exactly one fresh original Alfea28 generation.");
            _newPlaying = current.PlayingState; _newScene = current.Scene; _newRoot = currentRoot;
            _complete = true;
            _log(new { @event = "native-saved-reload-complete", role = "Host", level = 28,
                nativeLoadCalls = 1, actualOldExitReturned = true, actualNewEntryOrdinal = 2,
                originalNewLoadAndPlacementAndFirstWorldQualified = true, dispatcherPageReleasedAtLaterWorld = true,
                unchangedSavedFileSha256 = _savedHash, oldScenePointersFollowed = false,
                playingAddressReused = _newPlaying == _initialPlaying, rootAddressReused = _newRoot == _initialRoot,
                oldEpoch = 1, oldGeneration = 1, newEpoch = 2, newGeneration = 2,
                allSavedFieldsRoundTripOrCoopProgressionClaimed = false });
        }
        catch (Exception error) { Fail(error); throw; }
    }

    private void BeginLoad(uint tid, ref Win32Native.X86Context context, NativeCallDispatcher calls, Action resume)
    {
        Require(_initialWorldQualified && !_loadCalled && !calls.Pending && !calls.HasRetiredPage &&
            _initial is { WorldQualified: true, EntryCompleted: true, HasPendingOriginalEntry: false, Failed: false } &&
            _router.NativeEntryOrdinal == 1,
            "Saved reload cannot begin before its initial original generation and completed native save.");
        VerifyLivePins();
        _savedHash = HashSavedFile(out _savedBytes);
        _profile = P(0x765AD4); Require(U(_profile) == ProfileTable, "Reload canonical Profile type differs.");
        _initialFlow = P(0x755294); Require(U(_initialFlow) == FlowTable, "Reload Flow type differs.");
        _initial!.RetireCached();
        _reloaded = new(_host, _image, _log, WorldStateRole.Host, 28, 2, 2,
            captureEntryOrdinal: checked(_router.NativeEntryOrdinal + 1),
            nativeOrdinalBase: _router.NativeEntryOrdinal, router: _router);
        _router.AttachScope(_reloaded);
        _activeCalls = calls; _worldCallEsp = context.Esp; _callWorld = _worlds; _loadCalled = true;
        _log(new { @event = "native-saved-reload-load-begin", level = 28, slot = 0,
            savedFileSha256 = _savedHash, savedBytes = _savedBytes, canonicalProfile = Hex(_profile),
            function = Hex(LoadSave), arguments = new uint[] { 0 }, wholeProtectedGatewayRequested = true,
            oldEntryScopeRetired = true, freshTargetEntryOrdinal = 2, sourceProfileFieldWrites = 0,
            savedFieldsOrQueueReturnNotUsedAsCompletedEntry = true });
        calls.Begin(_host, tid, ref context, LoadSave, _profile, [0], false, value =>
        {
            EnsureStop(_thread);
            Require(_loadCalled && !_loadReturned && _entrySeen && _queueCallSeen && _ret4Seen &&
                (value & 0xFF) == 1 && U(0x765AD4) == _profile && U(_profile) == ProfileTable,
                "Original complete LoadSave/RET4 did not return true through its one canonical Profile call.");
            VerifySavedHash();
            _nativeResult = value; _loadReturned = true; _returnStop = _host.DebugStopSequence;
            _log(new { @event = "native-saved-reload-load-returned", nativeResult = value,
                originalRet4AndDispatcherStackMatched = true, requestedDestination = 28,
                savedFileSha256 = _savedHash, actualOldExitOrNewEntryNotClaimed = true,
                dispatcherPageStillRetired = true });
            resume();
        });
    }

    private void OnLoadEntry(uint tid, uint at, ref Win32Native.X86Context context)
    {
        EnsureStop(tid);
        Require(_loadCalled && !_entrySeen && !_loadReturned && _activeCalls?.Pending == true &&
            at == LoadSave && context.Eip == at && context.Ecx == _profile && U(0x765AD4) == _profile &&
            U(_profile) == ProfileTable && context.Esp == checked(_worldCallEsp - 8) && U(context.Esp + 4) == 0,
            "Original LoadSave entry is not the exact once-only native dispatcher thiscall slot0 frame.");
        _entrySeen = true; _entryEsp = context.Esp; _entryCaller = U(context.Esp);
        Require(_entryCaller >= 0x10000, "Original LoadSave private dispatcher caller is absent.");
        _entryEbx = context.Ebx; _entryEbp = context.Ebp; _entryEsi = context.Esi; _entryEdi = context.Edi;
        _log(new { @event = "native-saved-reload-original-load-entry", entryEsp = Hex(_entryEsp),
            nativeCaller = Hex(_entryCaller), nativeSlot = 0, originalFirstInstructionNotReplaced = true });
    }

    private void OnRequestCall(uint tid, uint at, ref Win32Native.X86Context context)
    {
        EnsureStop(tid);
        Require(_entrySeen && !_queueCallSeen && !_ret4Seen && !_loadReturned && _activeCalls?.Pending == true &&
            at == RequestCall && context.Eip == at && context.Esi == _profile && context.Ecx == _initialFlow &&
            U(0x755294) == _initialFlow && U(_initialFlow) == FlowTable && context.Esp < _entryEsp &&
            _entryEsp - context.Esp < 4096 && U(context.Esp) == 28 && U(context.Esp + 4) == 1,
            "Original LoadSave destination/literal argument is not the bounded Alfea28 native request before its CALL.");
        _queueCallSeen = true;
        _log(new { @event = "native-saved-reload-original-request-call", nativeCall = Hex(RequestCall),
            nativeFunction = "0x00595450", originalDestination = 28, originalArgument = 1,
            callNotExecutedYet = true, requestArgumentsChanged = false, actualEntryCompleted = false });
    }

    private void OnLoadRet4(uint tid, uint at, ref Win32Native.X86Context context)
    {
        EnsureStop(tid);
        Require(_entrySeen && _queueCallSeen && !_ret4Seen && !_loadReturned && _activeCalls?.Pending == true &&
            at == LoadRet4 && context.Eip == at && context.Esp == _entryEsp && U(context.Esp) == _entryCaller &&
            U(context.Esp + 4) == 0 && (context.Eax & 255) == 1 && context.Ebx == _entryEbx &&
            context.Ebp == _entryEbp && context.Esi == _entryEsi && context.Edi == _entryEdi,
            "Original LoadSave success RET4 instruction/callee-saved frame did not match its actual entry.");
        _ret4Seen = true;
        _log(new { @event = "native-saved-reload-original-ret4", entryEsp = Hex(_entryEsp),
            nativeSuccessAl = 1, ret4NotExecutedYet = true, calleeSavedRegistersMatched = true,
            originalReturnInstructionPreserved = true });
    }

    private bool TryCurrentScene(uint epoch, uint generation, bool allowRetainedSaveMenu,
        out SharedProfileScene scene, out uint root)
    {
        scene = default; root = 0;
        uint flow = U(0x755294);
        if (flow < 0x10000) return false;
        Require(U(flow) == FlowTable, "Saved reload current Flow is not its native typed owner.");
        uint depth = U(flow + 0x1AC);
        if (depth == uint.MaxValue) return false;
        Require(depth < 20, "Saved reload current Flow exceeds its original twenty stack slots.");
        if (U(flow + 0x1B8) != 0) return false;
        uint current = U(flow + 0x1B0), top = P(flow + 0x15C + depth * 4), playing = 0;
        bool saveMenu = allowRetainedSaveMenu && current is 69 or 72 && U(top + 0x10) == current;
        if (current != 28 && !saveMenu) return false;
        if (saveMenu) VerifyOriginalAccessor(top);
        for (uint index = 0; index <= depth; ++index)
        {
            uint candidate = P(flow + 0x15C + index * 4);
            if (U(candidate) != PlayingTable || U(candidate + 0x10) != 28) continue;
            Require(playing == 0, "Saved reload found multiple retained Alfea28 states."); playing = candidate;
        }
        Require(playing != 0 && (saveMenu ? top != playing : top == playing),
            "Saved reload does not own its exact current or retained original Alfea28 state.");
        uint profile = P(0x765AD4), primary = P(profile + 0x2B4);
        root = P(primary + 0x24);
        uint engine = P(0x755274), nativeScene = P(engine + 0x18), input = P(0x7552A0);
        uint device = P(input + 0x1C), timer = P(0x755298);
        Require(U(profile) == ProfileTable && U(primary) == 0x710F80 && U(root) == VisualGraphReader.NodeVtable &&
            U(root + 0x3C) == nativeScene && U(nativeScene) == 0x6E7358 && U(input) == 0x6F5508 &&
            U(device) == 0x7116CC && U(timer) == 0x702F94 && Read(timer + 0x40, 1)[0] <= 1,
            "Saved reload current canonical Profile/body/scene/input/timer types differ.");
        if (!saveMenu && (U(device + 0x14) != 3 || Read(timer + 0x40, 1)[0] != 0)) return false;
        scene = new(epoch, generation, 28, playing, PlayingTable, nativeScene); return true;
    }

    private void VerifyOriginalAccessor(uint state)
    {
        uint table = U(state);
        Require(table is >= 0x6DA000 and < 0x744000, "Retained save overlay table is outside the original image.");
        uint getter = U(table + 0x10);
        Require(getter == BitConverter.ToUInt32(Image(table + 0x10, 4)) && getter is >= 0x401000 and < 0x6DA000,
            "Retained save overlay accessor is not the original table's literal RTTI getter.");
        byte[] literal = Image(getter, 6);
        Require(literal[0] == 0xB8 && literal[5] == 0xC3 && Read(getter, 6).SequenceEqual(literal) &&
            U(BitConverter.ToUInt32(literal, 1)) != 0, "Retained save overlay has no original registered class accessor.");
    }

    private string HashSavedFile(out int bytes)
    {
        Require(File.Exists(_savedFile) && (File.GetAttributes(_savedFile) & FileAttributes.ReparsePoint) == 0,
            "Saved reload requires the exact ordinary isolated Saved00 file after native writer completion.");
        byte[] data = File.ReadAllBytes(_savedFile); bytes = data.Length;
        Require(bytes is > 0 and <= 262144, "Saved reload file exceeds its bounded original slot input.");
        return Convert.ToHexString(SHA256.HashData(data));
    }
    private void VerifySavedHash() => Require(HashSavedFile(out int bytes) == _savedHash && bytes == _savedBytes,
        "Saved00 changed after its original reload input was pinned.");
    private void VerifyLivePins()
    {
        PinLive(LoadSave, "6AFFE9AF70F3FF"); PinLive(LoadRet4, "C20400");
        PinLive(0x4E778D, "8B4C24286A01518BC8E8B5DC0A008B54242C");
    }
    private void PinImage(uint at, string hex) => Require(Image(at, hex.Length / 2).SequenceEqual(Convert.FromHexString(hex)),
        "Saved reload original native gateway/call/RET4 image pin differs.");
    private void PinLive(uint at, string hex)
    {
        PinImage(at, hex); byte[] expected = Convert.FromHexString(hex), actual = Read(at, expected.Length);
        for (int index = 0; index < actual.Length; ++index)
            if (actual[index] == 0xCC && expected[index] != 0xCC && _host.Breakpoints.Contains(checked(at + (uint)index)))
                actual[index] = expected[index];
        Require(actual.SequenceEqual(expected), "Saved reload original native live pin changed outside owned breakpoints.");
    }
    private void Repeat(uint site, string name, BreakpointCallback callback)
    {
        BreakpointCallback repeat = null!;
        repeat = (uint tid, uint at, ref Win32Native.X86Context context) =>
        {
            if (_retired || _failed) return;
            try { callback(tid, at, ref context); }
            catch (Exception error) { Fail(error); throw; }
            if (!_retired && !_failed) _host.Breakpoints.AddReturn(site, tid, name, repeat);
        };
        _host.Breakpoints.AddReturn(site, _thread, name, repeat);
    }
    internal void RetireCached()
    {
        Require(Environment.CurrentManagedThreadId == _owner, "Saved reload retirement changed its synchronous owner thread.");
        if (_retired) return;
        bool pending = _activeCalls?.Pending == true || _entrySeen && !_loadReturned ||
            _router.CurrentScope?.HasPendingOriginalEntry == true;
        // The enclosing owned debugger may already be unwinding a failed
        // native frame. Preserve its original failure and pending evidence;
        // do not claim a strict router retirement that cannot yet be proved.
        if (!pending) _router.RetireProcess();
        _retired = true;
        _log(new { @event = "native-saved-reload-retired", completed = _complete, succeeded = ModeSucceeded,
            nativeReads = 0, nativeWrites = 0, originalLoadFrameStillPending = _entrySeen && !_loadReturned,
            dispatcherCallStillPending = _activeCalls?.Pending == true,
            originalEntryScopeStillPending = _router.CurrentScope?.HasPendingOriginalEntry == true,
            strictRouterRetirementDeferred = pending, immediateBreakpointRestorationClaimed = false });
    }
    private void EnsureStop(uint tid) => Require(Environment.CurrentManagedThreadId == _owner && _host.IsAtDebugStop &&
        _host.ProcessHandle == _process && _host.ProcessPid == _pid && _host.MainThreadId == _thread && tid == _thread &&
        _host.ImageBase == 0x400000, "Saved reload lost its owned original main-thread stop.");
    private uint U(uint address) => Win32Native.ReadUInt32(_process, address);
    private uint P(uint address)
    {
        uint result = U(address); Require(result >= 0x10000 && (result & 3) == 0,
            "Saved reload requires a current nonnull aligned native owner."); return result;
    }
    private byte[] Read(uint at, int bytes) => Win32Native.ReadMemory(_process, at, bytes);
    private byte[] Image(uint at, int bytes) => _image.GetSectionData(checked((int)(at - 0x400000))).GetContent(0, bytes).ToArray();
    private void Fail(Exception error) { _failed = true; _failure = error.Message; }
    private static string Hex(uint value) => $"0x{value:X8}";
    private static void Require(bool value, string message) { if (!value) throw new InvalidDataException(message); }
}
