using System.Reflection.PortableExecutable;
using System.Security.Cryptography;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// Explicit one-generation Guest retirement for a coordinator-owned native
/// process restart. It requests the registered original MainMenu, delegates
/// pre-unload aggregate cleanup to the existing guard and never reuses old IPC.
/// </summary>
internal sealed class NativeGuestProgression
{
    private const uint Request = 0x595450, FlowTable = 0x702D2C, PlayingTable = 0x710874;
    private const uint MenuTable = 0x7102A8, MenuGetter = 0x5EF690, MenuRecord = 0x768990;
    private readonly DebuggerHost _host;
    private readonly PEReader _image;
    private readonly Action<object> _log;
    private readonly IntPtr _process;
    private readonly uint _pid, _thread;
    private readonly uint _level;
    private readonly CampaignSceneProfile? _campaign;
    private readonly int _owner;
    private readonly byte[] _request;
    private bool _started, _requestReturned, _inputRetiredBeforeRequest, _exitSeen, _retirementReady, _failed, _retired;
    private string _failure = "";
    private uint _worlds, _callWorld, _returnStop, _flow, _menuId, _menuObject, _requestResult;

    internal NativeGuestProgression(ProbeConfiguration config, DebuggerHost host, PEReader image, Action<object> log)
    {
        ArgumentNullException.ThrowIfNull(host); ArgumentNullException.ThrowIfNull(image); ArgumentNullException.ThrowIfNull(log);
        Require(config.DiagnosticWorldState?.Role == WorldStateRole.Guest && config.DiagnosticGuestCaptureFixture &&
            config.Mode == "client-presentation-live" && !config.WorldBattlePresentation &&
            host.IsAtDebugStop && host.ProcessHandle != IntPtr.Zero && host.ProcessPid != 0 &&
            host.MainThreadId != 0 && host.ImageBase == 0x400000,
            "Guest progression requires explicit live Alfea28 Guest capture and its owned original stop.");
        bool story3 = config.ProgressionControl is not null && config.ProgressionSceneLevel == 3;
        Require(!story3 || config.EnableProgressionAuthority && config.EnableWorldEntry,
            "Gardenia03 Guest retirement requires the closed progression/entry cohort.");
        _level = config.CampaignScene?.Level ?? (story3 ? 3u : 28u); _campaign = config.CampaignScene;
        Require(Convert.ToHexString(SHA256.HashData(image.GetEntireImage().GetContent().ToArray())) ==
            "3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F",
            "Guest progression requires the exact original PC executable.");
        _host = host; _image = image; _log = log; _process = host.ProcessHandle;
        _pid = host.ProcessPid; _thread = host.MainThreadId; _owner = Environment.CurrentManagedThreadId;
        _request = Image(Request, 0x65);
        Require(Convert.ToHexString(SHA256.HashData(_request)) ==
            "C899657754981014FD38AFAC593DD67E79F6C0A659630AE4197457138421FE4A",
            "Guest original Flow request complete source window differs.");
        PinImage(MenuGetter, "B890897600C3");
        Require(BitConverter.ToUInt32(Image(MenuTable + 0x10, 4)) == MenuGetter &&
            Image(0x5954A3, 3).SequenceEqual(Convert.FromHexString("C20800")) &&
            Image(0x5954B2, 3).SequenceEqual(Convert.FromHexString("C20800")),
            "Guest original MainMenu type or Flow RET8 pins differ.");
    }

    internal bool Started => _started;
    internal bool RetirementReady => _retirementReady && !_failed;
    internal bool ModeCompleted => RetirementReady;
    internal bool ModeSucceeded => RetirementReady;
    internal object Summary => new
    {
        role = "Guest", level = _level, started = _started, retired = _retired, failed = _failed, failure = _failure,
        retirementReady = RetirementReady, originalRequestReturned = _requestReturned,
        inputRetiredBeforeNativeRequest = _inputRetiredBeforeRequest, actualOriginalExitReturned = _exitSeen,
        worlds = _worlds, callWorld = _callWorld, nativeMenuId = _menuId, nativeMenuTable = Hex(MenuTable),
        requestFunction = Hex(Request), requestArguments = _started ? new uint[] { _menuId, 0 } : Array.Empty<uint>(),
        requestResultOpaque = _requestReturned ? Hex(_requestResult) : null, requestResultUsedAsBool = false,
        nativeCallsRequested = _started ? 1 : 0, resolvedFromOriginal81SlotFlowRegistry = true,
        hardcodedMenuStateId = false, originalPlayableExitAndLaterResourcesRequired = true,
        legacyPresentationModeSucceededClaimed = false, nativeInputOrProfileWrites = 0,
        sameProcessGenerationRecreation = false, newProcessEntryAndFullSnapshotApplyNotClaimed = true,
        nativeRequestReturnIsActualExitOrReady = false, oldSceneReadsAfterExit = 0
    };

    internal void StepAtWorld(uint tid, ref Win32Native.X86Context context, NativeCallDispatcher calls,
        Action resume, bool prepareAuthorized, Action retireInputBeforeQueue,
        ClientPresentationSession presentation, WorldStateSession world, NativeVisualActor? visual,
        bool actualOldExitReturned)
    {
        if (_retired || _failed || _retirementReady) return;
        try
        {
            EnsureStop(tid);
            Require(context.Eip == NativeCallDispatcher.WorldReturn && ++_worlds <= NativeObservationBudget.WorldLimit,
                "Guest progression left its bounded actual WorldRET.");
            if (calls.Pending) return;
            Require(!calls.HasRetiredPage, "Guest progression composition must free a retired dispatcher page at this later World first.");
            if (!_started)
            {
                if (!prepareAuthorized) return;
                Require(!actualOldExitReturned && world.GoalPassed && !world.Retired &&
                    presentation.Started && !presentation.AfterUnload && presentation.WorldQuarantineActive &&
                    visual is { WorldReady: true, HasActor: true },
                    "Guest Prepare precedes its current native world application or already follows an old Exit.");
                ResolveOriginalMenu();
                Require(Read(Request, _request.Length).SequenceEqual(_request),
                    "Guest original Flow request live bytes differ before the once-only native call.");
                retireInputBeforeQueue(); _inputRetiredBeforeRequest = true;
                _started = true; _callWorld = _worlds;
                _log(new { @event = "guest-progression-original-menu-request-begin", nativeMenuId = _menuId,
                    nativeMenuObject = Hex(_menuObject), nativeMenuTable = Hex(MenuTable), function = Hex(Request),
                    arguments = new uint[] { _menuId, 0 }, actualCurrentWorldApplicationRequired = true,
                    inputQuiescenceCallbackCompleted = true, inputCommandWrites = 0,
                    originalRequestQueuesMode2WithDelay1 = true, nativeExitNotYetClaimed = true });
                calls.Begin(_host, tid, ref context, Request, _flow, [_menuId, 0], false, value =>
                {
                    EnsureStop(_thread);
                    Require(_started && !_requestReturned && _inputRetiredBeforeRequest &&
                        U(0x755294) == _flow && U(_flow) == FlowTable,
                        "Guest original menu request returned to another Flow or invocation.");
                    _requestResult = value; _requestReturned = true; _returnStop = _host.DebugStopSequence;
                    _log(new { @event = "guest-progression-original-menu-request-returned", nativeMenuId = _menuId,
                        resultOpaque = Hex(value), nativeRet8StackMatchedByDispatcher = true,
                        queuedRequestNotCompletedExit = true, dispatcherPageStillRetired = true });
                    resume();
                });
                return;
            }
            Require(_requestReturned && _host.DebugStopSequence > _returnStop && _worlds > _callWorld,
                "Guest progression continuation is not a later actual World after its original request.");
            if (actualOldExitReturned) _exitSeen = true;
            if (!_exitSeen || !world.Retired || visual?.Cleaned != true) return;
            // Retired World and Visual owners expose cached results only. Their
            // aggregate native batch must have completed before original Exit
            // could return; no old body, scene or native binding is traversed.
            if (!presentation.TryQualifyNativeRetirementAtWorld(tid, ref context,
                    actualOriginalExitReturned: true, aggregateNativeActorCleanupComplete: true)) return;
            if (!FreshOriginalMenu()) return;
            Require(presentation.NativeResourcesRetired && !calls.Pending && !calls.HasRetiredPage,
                "Guest retirement still owns a native policy or dispatcher lease.");
            _retirementReady = true;
            _log(new { @event = "guest-progression-native-retirement-ready", actualOriginalExitReturned = true,
                inputRetiredBeforeRequest = true, worldRetired = true, nativeVisualCleaned = true,
                restoredPolicyPagesReleasedAtLaterWorld = true, dispatcherPageReleased = true,
                freshOriginalMainMenuObserved = true, nativeMenuId = _menuId,
                legacyMainMenu30OrMovementJumpAcceptanceClaimed = false,
                originalProcessRestartAndNextGuestEntryStillExternal = true, historicalGraphReads = 0 });
        }
        catch (Exception error) { _failed = true; _failure = error.Message; throw; }
    }

    internal void ReleaseRetiredRequestPageAtExit(uint tid, ref Win32Native.X86Context context,
        NativeCallDispatcher calls, uint entry, byte expectedFirstByte)
    {
        try
        {
            EnsureStop(tid);
            Require(!_retired && !_failed && _started && _requestReturned && _inputRetiredBeforeRequest &&
                !_exitSeen && !_retirementReady && !calls.Pending && calls.HasRetiredPage &&
                _host.DebugStopSequence > _returnStop && entry == 0x5F86C0 && expectedFirstByte == 0x53 && context.Eip == entry,
                "Guest progression page release requires its later original pre-Exit after the captured request returned.");
            uint flow = P(0x755294), playing = context.Ecx;
            Require(flow == _flow && U(flow) == FlowTable && playing >= 0x10000 && (playing & 3) == 0 &&
                U(playing) == PlayingTable && U(playing + 0x10) == _level &&
                _menuId is >= 51 and <= 80 && P(flow + 0x18 + _menuId * 4) == _menuObject &&
                U(_menuObject) == MenuTable && U(_menuObject + 0x10) == _menuId &&
                U(MenuTable + 0x10) == MenuGetter && U(MenuRecord) == 0x284B3AA2 &&
                Read(MenuGetter, 6).SequenceEqual(Image(MenuGetter, 6)),
                "Guest progression pre-Exit lost its current typed Playing or captured original MainMenu registration.");
            calls.ReleaseRetiredBeforeGuestProgressionExit(_host, tid, ref context, entry, expectedFirstByte,
                _menuId, _menuObject, _level, _campaign);
        }
        catch (Exception error) { _failed = true; _failure = error.Message; throw; }
    }

    private void ResolveOriginalMenu()
    {
        _flow = P(0x755294);
        Require(U(_flow) == FlowTable && U(_flow + 0x1B0) == _level && U(_flow + 0x1B8) == 0,
            "Guest Prepare requires its original idle Alfea28 Flow.");
        uint depth = U(_flow + 0x1AC); Require(depth < 20, "Guest Prepare Flow stack exceeds its original bound.");
        uint playing = P(_flow + 0x15C + depth * 4);
        Require(U(playing) == PlayingTable && U(playing + 0x10) == _level,
            "Guest Prepare current native Flow top is not Alfea28.");
        uint matches = 0;
        for (uint index = 0; index < 81; ++index)
        {
            uint candidate = U(_flow + 0x18 + index * 4);
            if (candidate == 0) continue;
            Require(candidate >= 0x10000 && (candidate & 3) == 0,
                "Original Flow registration array contains an unqualified native pointer.");
            if (U(candidate) != MenuTable) continue;
            Require(index is >= 51 and <= 80 && U(candidate + 0x10) == index &&
                U(MenuTable + 0x10) == MenuGetter && U(MenuRecord) == 0x284B3AA2 &&
                Read(MenuGetter, 6).SequenceEqual(Image(MenuGetter, 6)),
                "Original registered MainMenu ID/type does not use the admitted native nonplayable replace branch.");
            _menuId = index; _menuObject = candidate; ++matches;
        }
        Require(matches == 1, "Guest Prepare must resolve exactly one original registered MainMenu.");
    }

    private bool FreshOriginalMenu()
    {
        // Process-lifetime Flow registration and current top only; old scene,
        // Profile/Instance/Root and presentation bindings are never followed.
        uint flow = P(0x755294); Require(flow == _flow && U(flow) == FlowTable,
            "Guest post-exit current Flow singleton changed.");
        uint depth = U(flow + 0x1AC);
        if (depth == uint.MaxValue) return false;
        Require(depth < 20, "Guest post-exit Flow stack exceeds its native bound.");
        if (U(flow + 0x1B8) != 0) return false;
        uint top = P(flow + 0x15C + depth * 4);
        return top == _menuObject && U(top) == MenuTable && U(top + 0x10) == _menuId &&
            U(flow + 0x1B0) == _menuId && U(MenuTable + 0x10) == MenuGetter &&
            U(MenuRecord) == 0x284B3AA2 && Read(MenuGetter, 6).SequenceEqual(Image(MenuGetter, 6));
    }

    internal void RetireCached()
    {
        Require(Environment.CurrentManagedThreadId == _owner, "Guest progression retirement changed its managed owner.");
        if (_retired) return;
        _retired = true;
        _log(new { @event = "guest-progression-owner-retired", retirementReady = RetirementReady,
            nativeReads = 0, nativeWrites = 0, originalCallStillUnreturned = _started && !_requestReturned });
    }
    private void EnsureStop(uint tid) => Require(_host.IsAtDebugStop && _host.ProcessHandle == _process &&
        _host.ProcessPid == _pid && _host.MainThreadId == _thread && tid == _thread &&
        Environment.CurrentManagedThreadId == _owner && _host.ImageBase == 0x400000,
        "Guest progression requires its owned original main-thread stop.");
    private void PinImage(uint at, string hex) => Require(Image(at, hex.Length / 2).SequenceEqual(Convert.FromHexString(hex)),
        "Guest progression original type pin differs.");
    private byte[] Image(uint at, int bytes) => _image.GetSectionData(checked((int)(at - 0x400000))).GetContent(0, bytes).ToArray();
    private byte[] Read(uint at, int bytes) => Win32Native.ReadMemory(_process, at, bytes);
    private uint U(uint at) => Win32Native.ReadUInt32(_process, at);
    private uint P(uint at)
    { uint value = U(at); Require(value >= 0x10000 && (value & 3) == 0, "Guest progression native owner is null/unaligned."); return value; }
    private static string Hex(uint value) => $"0x{value:X8}";
    private static void Require(bool value, string message) { if (!value) throw new InvalidDataException(message); }
}
