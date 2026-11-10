using System.Reflection.PortableExecutable;
using System.Security.Cryptography;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

// Local owned-process authority only. This is neither a wire record nor a
// claim that the production progression protocol implements a pause command.
internal sealed record NativeProgressionOverlayPermit(DebuggerHost Owner, WorldStateRole Role,
    uint LevelEpoch, uint Generation, ulong Operation, int HoldMilliseconds = 500, int TimeoutMilliseconds = 15000);

/// <summary>One original Diary61 push/pop on the same controlled Alfea28 native scene.</summary>
internal sealed class NativeProgressionOverlayProbe
{
    private const uint Push = 0x5953E0, Pop = 0x595440, FlowTable = 0x702D2C;
    private const uint PlayingTable = 0x710874, DiaryTable = 0x70ED30, DiaryGetter = 0x5D53C0;
    private const uint DiaryRecord = 0x767FD8, DiaryClass = 0x0734C578;
    private const uint Suspend = 0x5F6750, DiaryEnter = 0x5D62D0, DiaryExit = 0x5D5A20, PlayingResume = 0x5F6420;
    private const uint MaximumWorlds = 100000;
    private readonly DebuggerHost _host;
    private readonly PEReader _image;
    private readonly Action<object> _log;
    private readonly NativeProgressionOverlayPermit _permit;
    private readonly WorldPresentationEntityReader _reader;
    private readonly IntPtr _process;
    private readonly uint _pid, _thread;
    private readonly int _owner;
    private readonly long _deadline;
    private long _lastNow, _operationDeadline, _pausedSince;
    private uint _stage, _worlds, _pausedWorlds, _pushReturnStop, _popReturnStop, _resumedWorldStop, _lastWorldStop;
    private bool _haveWorldStop;
    private uint _flow, _playing, _diary, _depth, _profile, _engine, _input, _command, _device, _timer;
    private uint _entryOrdinal, _exitReturns, _pausedTimer;
    private NativeWorldEntrySeed? _entry;
    private NativeProgressionExitObservation? _exit;
    private NativeCallDispatcher? _calls;
    private WorldPresentationNativeRig? _body;
    private byte[]? _seed, _witness;
    private readonly HashSet<uint> _entryProbes = [];
    private readonly HashSet<uint> _originalReturns = [];
    private OriginalFrame? _pending;
    private bool _pushReturned, _popReturned, _pauseQualified, _nativeResumed, _retired, _failed;
    private string _failure = "";
    private sealed record OriginalFrame(uint Entry, uint Receiver, uint Esp, uint Caller, uint Argument,
        uint Ebx, uint Ebp, uint Esi, uint Edi);

    internal NativeProgressionOverlayProbe(ProbeConfiguration config, DebuggerHost host, PEReader image,
        Action<object> log, NativeProgressionOverlayPermit permit)
    {
        ArgumentNullException.ThrowIfNull(config); ArgumentNullException.ThrowIfNull(host);
        ArgumentNullException.ThrowIfNull(image); ArgumentNullException.ThrowIfNull(log); ArgumentNullException.ThrowIfNull(permit);
        var world = config.DiagnosticWorldState ?? throw new InvalidDataException("Overlay requires its explicit World identity.");
        bool hostBody = world.Role == WorldStateRole.Host && config.Mode == "template-bloom-body-hair-idle" &&
            config.TemplateSharedPlayerProfile == "alfea-28-network-input" && config.TemplateNetworkInputFixture &&
            config.DiagnosticHostInput is { Role: DiagnosticInputRole.Host } && config.DiagnosticGuestInput is null &&
            config.TemplateProjectileManagerFixture && config.TemplateSharedHealthAlias && config.TemplatePlayerTraitsFixture &&
            config.TemplatePlayerTraitsPickupFx && config.TemplateTargetRegionFixture && config.TemplateConfigureTargetRegion;
        bool guestBody = world.Role == WorldStateRole.Guest && config.Mode == "client-presentation-live" &&
            config.DiagnosticGuestCaptureFixture && config.DiagnosticGuestInput is { Role: DiagnosticInputRole.Guest } &&
            config.DiagnosticHostInput is null && config.TemplateFile is null && config.TemplateSha256 is null &&
            config.TemplateSharedPlayerProfile is null && !config.TemplateNetworkInputFixture;
        var input = config.DiagnosticHostInput ?? config.DiagnosticGuestInput ??
            throw new InvalidDataException("Overlay requires its role-matched native input identity.");
        Require((hostBody || guestBody) && config.ProgressionControl is { OwnerPid: > 0, OwnerCreationFileTime: > 0, NextLevel: 28 } &&
            config.ProgressionSceneLevel == 28 && config.EnableProgressionAuthority && config.EnableWorldEntry &&
            config.UseProcessLocalMediaPath && !config.WorldBattlePresentation && !config.TemplateStoryDeathFixture &&
            !config.TemplateDeathBattleFixture && !config.TemplateHealthBattleFixture && !config.TemplateBattleCombatFixture &&
            !config.TemplateNetworkOrdinaryAttackFixture && !config.TemplateMosquitoPlayerTargets &&
            !config.DiagnosticAutoBattlePauseFixture && !config.ProgressionSavedReload && !config.ProgressionReloadSavedSlot &&
            !config.ProgressionCaptureSaveBeforeWriter && config.DurationSeconds is >= 5 and <= 600 &&
            (config.ProgressionAuthorityRole is null || config.ProgressionAuthorityRole == world.Role) &&
            input.SessionId == world.SessionId && input.ScenarioId == world.ScenarioId &&
            input.LevelEpoch == world.LevelEpoch && input.BodyGeneration == world.Generation &&
            ReferenceEquals(permit.Owner, host) && permit.Role == world.Role && permit.LevelEpoch == world.LevelEpoch &&
            permit.Generation == world.Generation && permit.LevelEpoch > 0 && permit.Generation > 0 && permit.Operation > 0 &&
            permit.HoldMilliseconds is >= 100 and <= 2000 && permit.TimeoutMilliseconds is >= 3000 and <= 30000 &&
            permit.TimeoutMilliseconds > permit.HoldMilliseconds,
            "Overlay requires its explicit controlled Alfea28 Host/Guest cohort and finite immutable local authority.");
        Require(host.IsAtDebugStop && host.ProcessHandle != IntPtr.Zero && host.ProcessPid != 0 &&
            host.MainThreadId != 0 && host.ImageBase == 0x400000 &&
            Hash(image.GetEntireImage().GetContent().ToArray()) == ProbeMain.OriginalSha256,
            "Overlay requires its owned exact original PC process.");
        _host = host; _image = image; _log = log; _permit = permit; _reader = new(host, image);
        _process = host.ProcessHandle; _pid = host.ProcessPid; _thread = host.MainThreadId;
        _owner = Environment.CurrentManagedThreadId; _lastNow = Environment.TickCount64;
        _deadline = checked(_lastNow + (long)config.DurationSeconds * 1000);
        VerifySources(live: false);
    }

    internal bool Started => _stage != 0;
    internal bool Paused => _pauseQualified && !_failed;
    internal bool NativeResumed => _nativeResumed && !_failed;
    // Completion is only the native push/pop cycle. Gameplay control remains
    // held until the external owner proves a new neutral InputRET/native ACK.
    internal bool Completed => NativeResumed;
    internal uint ResumedWorldStop => _resumedWorldStop;
    internal bool HasPendingNativeCall => _pending is not null || _stage == 1 && !_pushReturned || _stage == 4 && !_popReturned;
    internal object Summary => new { role = _permit.Role.ToString(), sourceLevel = 28, destinationLevel = 28,
        levelEpoch = _permit.LevelEpoch, generation = _permit.Generation, operation = _permit.Operation,
        stage = _stage, worlds = _worlds, pausedWorlds = _pausedWorlds, retired = _retired, failed = _failed, failure = _failure,
        pushReturned = _pushReturned, popReturned = _popReturned, paused = Paused, nativeResumed = NativeResumed,
        completedNativeCycle = Completed, pendingOriginalReturn = _pending is not null,
        outstandingEntryProbes = _entryProbes.Count, pairedOriginalStateReturns = _originalReturns.Count,
        nativeLocalEntryOrdinal = _entryOrdinal, playingExitReturnBaseline = _exitReturns,
        resumedWorldStop = _resumedWorldStop, nativeRequestCalls = (_stage >= 1 ? 1 : 0) + (_stage >= 4 ? 1 : 0),
        maximumNativeRequestCalls = 2, holdMilliseconds = _permit.HoldMilliseconds,
        directTimerFlowCommandOrArmWrites = 0, syntheticInputCalls = 0, newNativeTicks = 0,
        freshPostResumeInputAckProvenByThisOwner = false, gameplayResumeIssuedByThisOwner = false,
        externalFreshNeutralInputReturnAndNativeAckStillRequired = true, arbitraryCutsceneFamiliesQualified = false,
        sameSceneDoesNotCreateNewWorldEpoch = true, productionLanPauseProtocolClaimed = false };

    internal void StepAtWorld(uint tid, ref Win32Native.X86Context context, NativeCallDispatcher calls, Action resume,
        bool pauseAuthorized, bool continueAuthorized, NativeWorldEntrySeed entry,
        bool nativeCurrentWorldApplied, bool freshHeldNeutral, NativeProgressionExitObservation originalExit)
    {
        try
        {
            EnsureWorld(tid, context);
            Require(++_worlds <= MaximumWorlds, "Overlay exceeded its finite World budget.");
            Require(!calls.Pending && !calls.HasRetiredPage && _pending is null,
                "Overlay requires a later World after the whole original call and retired dispatcher page release.");
            _calls ??= calls; Require(ReferenceEquals(_calls, calls), "Overlay changed its native dispatcher owner.");
            if (_nativeResumed) return;
            if (_stage == 0)
            {
                Require(!continueAuthorized, "Overlay continue authority arrived before its one pause decision.");
                if (!pauseAuthorized || !nativeCurrentWorldApplied || !freshHeldNeutral) return;
                Capture(tid, ref context, entry, originalExit); VerifySources(live: true); InstallOriginalObservers();
                _operationDeadline = checked(_lastNow + _permit.TimeoutMilliseconds); _stage = 1;
                BeginRequest(tid, ref context, calls, resume, push: true); return;
            }
            VerifyGeneration(entry, originalExit); VerifyOwners(tid, ref context);
            Require(!continueAuthorized || _pauseQualified, "Overlay continue authority arrived before actual native pause qualification.");
            if (_stage == 2 || _stage == 3)
            {
                Require(_pushReturned && _host.DebugStopSequence > _pushReturnStop,
                    "Diary observation is not a later native World after the original push return.");
                if (!IsPaused()) return;
                Require(_originalReturns.Contains(Suspend) && _originalReturns.Contains(DiaryEnter),
                    "Settled Diary has no exact original Alfea Suspend and Diary Enter return pairs.");
                uint nowTimer = U(_timer + 0x1C);
                if (_pausedWorlds == 0) { _pausedSince = _lastNow; _pausedTimer = nowTimer; }
                else Require(nowTimer == _pausedTimer, "Original GameTimer advanced inside the measured settled Diary pause.");
                ++_pausedWorlds; _stage = 3;
                if (!_pauseQualified && _pausedWorlds >= 2)
                {
                    _pauseQualified = true;
                    _log(new { @event = "progression-overlay-paused", role = _permit.Role.ToString(),
                        operation = _permit.Operation, levelEpoch = _permit.LevelEpoch, generation = _permit.Generation,
                        pausedWorlds = _pausedWorlds, originalSuspendAndDiaryEnterReturns = true,
                        retainedPlayingAndCanonicalBodyOwnersUnchanged = true, noOriginalPlayingExitOrNewEntry = true,
                        nativeTimerPaused = 1, inputOrTimerWrites = 0, worldEpochChanged = false });
                }
                if (!continueAuthorized || !_pauseQualified || _lastNow - _pausedSince < _permit.HoldMilliseconds) return;
                Require(EmptyQueue(), "Overlay pop requires the original empty request queue.");
                _stage = 4; BeginRequest(tid, ref context, calls, resume, push: false); return;
            }
            if (_stage == 5)
            {
                Require(_popReturned && _host.DebugStopSequence > _popReturnStop,
                    "Overlay resume is not a later native World after original pop return.");
                if (!IsReady()) return;
                Require(_originalReturns.Count == 4 && _originalReturns.Contains(DiaryExit) &&
                    _originalReturns.Contains(PlayingResume) && _entryProbes.Count == 0,
                    "Retained Alfea resume has no exact original Diary Exit/Playing Resume return pairs.");
                _nativeResumed = true; _resumedWorldStop = _host.DebugStopSequence; _stage = 6;
                _log(new { @event = "progression-overlay-native-resumed", role = _permit.Role.ToString(),
                    operation = _permit.Operation, levelEpoch = _permit.LevelEpoch, generation = _permit.Generation,
                    pausedWorlds = _pausedWorlds, elapsedPausedMilliseconds = _lastNow - _pausedSince,
                    resumedWorldStop = _resumedWorldStop, originalDiaryExitAndPlayingResumeReturns = true,
                    retainedPlayingAndCanonicalBodyOwnersUnchanged = true, noOriginalPlayingExitOrNewEntry = true,
                    originalNativeTimerPaused = 0, originalDeviceContext = 3, inputOrTimerWrites = 0,
                    freshPostResumeInputAckStillExternal = true, gameplayControlResumeIssued = false });
            }
        }
        catch (Exception error) { Failed(error); throw; }
    }

    private void Capture(uint tid, ref Win32Native.X86Context context, NativeWorldEntrySeed entry,
        NativeProgressionExitObservation originalExit)
    {
        _entry = entry; _exit = originalExit;
        Require(entry is { Level: 28, WorldQualified: true, EntryCompleted: true, Retired: false, Failed: false } &&
            !entry.HasPendingOriginalEntry && entry.NativeLocalEntryOrdinal > 0 && originalExit.Healthy && !originalExit.HasPending,
            "Overlay lacks its actual completed original entry and live paired Playing Exit observer.");
        _seed = entry.GetSeed(); _witness = entry.GetWitness();
        var seed = WorldEntrySeedState.Unpack(_seed);
        Require(seed.Level == 28 && seed.LevelEpoch == _permit.LevelEpoch && seed.Generation == _permit.Generation,
            "Overlay entry belongs to a different level epoch/generation.");
        _entryOrdinal = entry.NativeLocalEntryOrdinal; _exitReturns = originalExit.ActualOriginalExitReturns;
        _flow = P(0x755294); _depth = U(_flow + 0x1AC); Require(_depth is >= 1 and <= 18, "Overlay stack has no bounded spare native slot.");
        _playing = P(_flow + 0x15C + _depth * 4); _profile = P(0x765AD4); _engine = P(0x755274);
        _input = P(0x7552A0); _command = P(_input + 0x18); _device = P(_input + 0x1C); _timer = P(0x755298);
        _body = _reader.ReadRigAtWorld(tid, ref context, P(_profile + 0x2B4), WorldPresentationActorState.BloomType);
        uint matches = 0;
        for (uint index = 51; index < 81; ++index)
        {
            uint candidate = U(_flow + 0x18 + index * 4);
            if (candidate < 0x10000 || U(candidate) != DiaryTable) continue;
            Require(index == 61 && U(candidate + 0x10) == index, "Original Diary registration is not the exact native61 ID.");
            _diary = candidate; ++matches;
        }
        Require(matches == 1, "Overlay requires exactly one original registered Diary61 state.");
        VerifyGeneration(entry, originalExit); VerifyOwners(tid, ref context);
        Require(IsReady() && EmptyQueue(), "Overlay pause requires the actual native ready Alfea28 and an empty request queue.");
    }

    private void BeginRequest(uint tid, ref Win32Native.X86Context context, NativeCallDispatcher calls, Action resume, bool push)
    {
        uint beforeDepth = U(_flow + 0x1AC), beforeCurrent = U(_flow + 0x1B0);
        byte beforeTimer = Read(_timer + 0x40, 1)[0];
        byte[] before = Read(_flow + 0x1B8, 120);
        calls.Begin(_host, tid, ref context, push ? Push : Pop, _flow, push ? [61u, 0u] : [0u], false, result =>
        {
            try
            {
                EnsureStop(); VerifyGeneration(_entry!, _exit!); VerifyBasicOwners();
                Require(!calls.Pending && calls.HasRetiredPage && _pending is null,
                    "Overlay request lost its actual whole-call return or private dispatcher-page lifetime.");
                byte[] expected = (byte[])before.Clone();
                BitConverter.GetBytes(push ? 61u : 81u).CopyTo(expected, 0);
                BitConverter.GetBytes(push ? 0u : 1u).CopyTo(expected, 4); BitConverter.GetBytes(1u).CopyTo(expected, 8);
                Require(Read(_flow + 0x1B8, 120).SequenceEqual(expected) && U(_flow + 0x1AC) == beforeDepth &&
                    U(_flow + 0x1B0) == beforeCurrent && Read(_timer + 0x40, 1)[0] == beforeTimer,
                    "Whole original overlay request changed more than its exact queued raw triple before World resume.");
                if (push) { Require(!_pushReturned && _stage == 1, "Overlay push returned twice."); _pushReturned = true; _pushReturnStop = _host.DebugStopSequence; _stage = 2; }
                else { Require(!_popReturned && _stage == 4, "Overlay pop returned twice."); _popReturned = true; _popReturnStop = _host.DebugStopSequence; _stage = 5; }
                _log(new { @event = "progression-overlay-original-request-returned", role = _permit.Role.ToString(),
                    operation = _permit.Operation, request = push ? "DiaryPush" : "SinglePop", opaqueResult = result,
                    originalWholeCallAndStackPaired = true, dispatcherPageStillRetired = true,
                    actualOverlayStateAlreadyClaimed = false, inputOrTimerWrites = 0 }); resume();
            }
            catch (Exception error) { Failed(error); throw; }
        });
    }

    private void InstallOriginalObservers()
    {
        foreach (uint entry in new[] { Suspend, DiaryEnter, DiaryExit, PlayingResume })
            Require(!_host.Breakpoints.Contains(entry), "Overlay original entry already has another breakpoint owner.");
        foreach (uint entry in new[] { Suspend, DiaryEnter, DiaryExit, PlayingResume })
        { _entryProbes.Add(entry); _host.Breakpoints.AddReturn(entry, _thread, "Progression.overlay.original-entry", OnOriginalEntry); }
    }
    private void OnOriginalEntry(uint tid, uint at, ref Win32Native.X86Context context)
    {
        if (_retired || _failed) return;
        try
        {
            EnsureStop(); VerifyGeneration(_entry!, _exit!);
            Require(tid == _thread && context.Eip == at && _pending is null && _entryProbes.Remove(at) &&
                _calls is { Pending: false },
                "Overlay original state entry is repeated, nested or belongs to another native frame.");
            uint receiver = at is Suspend or PlayingResume ? _playing : _diary;
            uint caller = at == Suspend ? 0x5964E2u : at == DiaryEnter ? 0x596542u : at == DiaryExit ? 0x596574u : 0x5965C3u;
            uint argument = at == Suspend || at == PlayingResume ? 61u : at == DiaryEnter ? 28u : 81u;
            Require((at is Suspend or DiaryEnter
                    ? _stage == 2 && _pushReturned && _host.DebugStopSequence > _pushReturnStop
                    : _stage == 5 && _popReturned && _host.DebugStopSequence > _popReturnStop) &&
                context.Ecx == receiver && U(context.Esp) == caller && U(context.Esp + 4) == argument &&
                U(receiver) == (receiver == _playing ? PlayingTable : DiaryTable) && !_host.Breakpoints.Contains(caller) &&
                (at != DiaryEnter || _originalReturns.Contains(Suspend)) &&
                (at != PlayingResume || _originalReturns.Contains(DiaryExit)),
                "Overlay original virtual call lost exact native caller, argument, receiver, order or free return slot.");
            _log(new { @event = "progression-overlay-original-state-entered", role = _permit.Role.ToString(),
                operation = _permit.Operation, nativeEntry = $"0x{at:X8}", nativeCaller = $"0x{caller:X8}",
                completedOwnedRequestReturned = true, sameDispatcherOwner = true,
                retiredDispatcherPageStillOwned = _calls!.HasRetiredPage,
                observationOnly = true, dispatcherPageReleasedHere = false, nativeContextWrites = 0 });
            _pending = new(at, receiver, context.Esp, caller, argument, context.Ebx, context.Ebp, context.Esi, context.Edi);
            _host.Breakpoints.AddReturn(caller, tid, "Progression.overlay.original-return", OnOriginalReturn);
        }
        catch (Exception error) { Failed(error); throw; }
    }
    private void OnOriginalReturn(uint tid, uint at, ref Win32Native.X86Context context)
    {
        if (_retired || _failed) return;
        try
        {
            EnsureStop(); VerifyGeneration(_entry!, _exit!);
            var frame = _pending ?? throw new InvalidDataException("Overlay native state return has no original entry pair.");
            Require(tid == _thread && at == frame.Caller && context.Eip == at && context.Esp == checked(frame.Esp + 8) &&
                context.Ebx == frame.Ebx && context.Ebp == frame.Ebp && context.Esi == frame.Esi && context.Edi == frame.Edi &&
                _originalReturns.Add(frame.Entry), "Overlay original RET4, callee-saved registers or one-use return differs.");
            _pending = null;
            _log(new { @event = "progression-overlay-original-state-returned", role = _permit.Role.ToString(),
                operation = _permit.Operation, nativeEntry = $"0x{frame.Entry:X8}", nativeCaller = $"0x{frame.Caller:X8}",
                argumentRaw = frame.Argument, originalRet4AndCalleeSavedRegistersMatched = true,
                actualSettledOverlayStateStillRequired = true, nativeContextWrites = 0 });
        }
        catch (Exception error) { Failed(error); throw; }
    }

    private void VerifyGeneration(NativeWorldEntrySeed entry, NativeProgressionExitObservation originalExit)
    {
        Require(ReferenceEquals(entry, _entry) && ReferenceEquals(originalExit, _exit) &&
            !entry.Retired && !entry.Failed && entry.WorldQualified && !entry.HasPendingOriginalEntry &&
            entry.NativeLocalEntryOrdinal == _entryOrdinal && originalExit.Healthy && !originalExit.HasPending &&
            originalExit.ActualOriginalExitReturns == _exitReturns && entry.GetSeed().SequenceEqual(_seed!) &&
            entry.GetWitness().SequenceEqual(_witness!),
            "Overlay cannot read a retired/replaced scene or cross an actual original Playing Exit/new-entry boundary.");
    }
    private void VerifyOwners(uint tid, ref Win32Native.X86Context context)
    {
        VerifyBasicOwners();
        Require(_reader.ReadRigAtWorld(tid, ref context, _body!.Instance, WorldPresentationActorState.BloomType) == _body,
            "Overlay retained native body component owners changed.");
        uint depth = U(_flow + 0x1AC); Require(depth == _depth || depth == _depth + 1, "Overlay added an unexpected native stack layer.");
        uint matches = 0;
        for (uint index = 0; index <= depth; ++index) if (U(_flow + 0x15C + index * 4) == _playing) ++matches;
        Require(matches == 1 && U(_flow + 0x15C + _depth * 4) == _playing,
            "Overlay did not retain exactly one original Playing in its original lower stack slot.");
    }
    private void VerifyBasicOwners()
    {
        Require(U(0x755294) == _flow && U(_flow) == FlowTable && U(0x765AD4) == _profile && U(_profile) == 0x6F5A44 &&
            U(_profile + 0x2B4) == _body!.Instance && U(_body.Instance) == 0x710F80 && U(_body.Instance + 0x24) == _body.Root &&
            U(_body.Root) == VisualGraphReader.NodeVtable && U(_body.Root + 0x3C) == _body.Scene &&
            U(_body.Scene) == 0x6E7358 && U(0x755274) == _engine && U(_engine + 0x18) == _body.Scene &&
            U(_playing) == PlayingTable && U(_playing + 0x10) == 28 && U(_flow + 0x18 + 61 * 4) == _diary &&
            U(_diary) == DiaryTable && U(_diary + 0x10) == 61 && U(0x7552A0) == _input && U(_input) == 0x6F5508 &&
            U(_input + 0x18) == _command && U(_command) == 0x703090 && U(_input + 0x1C) == _device &&
            U(_device) == 0x7116CC && U(0x755298) == _timer && U(_timer) == 0x702F94,
            "Overlay retained Flow/Profile/player/root/scene/input/timer ownership changed.");
    }
    private bool IsReady() => U(_flow + 0x1AC) == _depth && U(_flow + 0x15C + _depth * 4) == _playing &&
        U(_flow + 0x1B0) == 28 && U(_flow + 0x1B8) == 0 && Read(_timer + 0x40, 1)[0] == 0 && U(_device + 0x14) == 3;
    private bool IsPaused() => U(_flow + 0x1AC) == _depth + 1 && U(_flow + 0x15C + (_depth + 1) * 4) == _diary &&
        U(_flow + 0x1B0) == 61 && U(_flow + 0x1B8) == 0 && Read(_timer + 0x40, 1)[0] == 1;
    private bool EmptyQueue() => Enumerable.Range(0, 10).All(index => U(_flow + 0x1B8 + checked((uint)index * 12)) == 0);
    private void VerifySources(bool live)
    {
        Pin(Push, 0x5C, "EDE1B25590CF36E8BAC1CF3F21C99367D7EED205261E1906B13906587EF6F424", live);
        Pin(Pop, 14, "58F56985B789A52CA9AC91AFC6DF7F9A9A6539E962EEF4D4304AD569A43D679D", live);
        Pin(Suspend, 398, "972F4B8C1AF266CA9229769E8F6D030E8D1901875CB9B4F9B2DA97D733ADB13E", live);
        Pin(PlayingResume, 803, "F57C2992660AEDDF66F668749D6A9D00BE3C056F79A71773C3E98A41B2413494", live);
        Pin(DiaryEnter, 88, "9ECA276D16E175BAEF5832137D64CB6B364CF25B2C4816CDE33E69F329284091", live);
        Pin(DiaryExit, 31, "42273FEAEDCA6487163C9E24323083DBB36B19C566D829977DCDB39F4213B463", live);
        Pin(0x5964DA, 8, "A6062F347C90E80ACCFF2736B8A56085119853340BDB4D3DC0CE8F4548B0D9C1", live);
        Pin(0x5965B0, 19, "B34DDE79541435F4B32F75C83766D4A69E50E6B0C0FF26F2467C134BA97FF63F", live);
        PinHex(0x596535, "8B8C865C0100008B1155FF5220", live);
        PinHex(0x596568, "8B8C8E5C0100008B11FF5224", live);
        Require(ImageWord(PlayingTable + 0x28) == Suspend && ImageWord(PlayingTable + 0x2C) == PlayingResume &&
            ImageWord(DiaryTable + 0x20) == DiaryEnter && ImageWord(DiaryTable + 0x24) == DiaryExit &&
            ImageWord(DiaryTable + 0x10) == DiaryGetter &&
            Image(DiaryGetter, 6).SequenceEqual(Convert.FromHexString("B8D87F7600C3")),
            "Overlay original Playing/Diary virtual identities differ.");
        if (live)
            Require(U(PlayingTable + 0x28) == Suspend && U(PlayingTable + 0x2C) == PlayingResume &&
                U(DiaryTable + 0x20) == DiaryEnter && U(DiaryTable + 0x24) == DiaryExit &&
                U(DiaryTable + 0x10) == DiaryGetter && U(DiaryRecord) == DiaryClass,
                "Overlay live original Playing/Diary virtual identities differ.");
    }
    private void PinHex(uint at, string hex, bool live)
    {
        byte[] expected = Convert.FromHexString(hex);
        Require(Image(at, expected.Length).SequenceEqual(expected), "Overlay original virtual call-site pin differs.");
        if (!live) return;
        byte[] actual = Read(at, expected.Length);
        for (int index = 0; index < actual.Length; ++index)
            if (actual[index] == 0xCC && expected[index] != 0xCC && _host.Breakpoints.Contains(checked(at + (uint)index)))
                actual[index] = expected[index];
        Require(actual.SequenceEqual(expected), "Overlay live original virtual call-site pin differs outside owned breakpoints.");
    }
    private void Pin(uint at, int count, string sha256, bool live)
    {
        byte[] expected = Image(at, count); Require(Hash(expected) == sha256, "Overlay original source pin differs.");
        if (!live) return;
        byte[] actual = Read(at, count);
        for (int index = 0; index < actual.Length; ++index)
            if (actual[index] == 0xCC && expected[index] != 0xCC && _host.Breakpoints.Contains(checked(at + (uint)index)))
                actual[index] = expected[index];
        Require(actual.SequenceEqual(expected), "Overlay live source differs outside owned breakpoints.");
    }
    internal void RetireCached()
    { Require(Environment.CurrentManagedThreadId == _owner, "Overlay cached retirement changed owner."); _retired = true; }
    private void EnsureWorld(uint tid, in Win32Native.X86Context context)
    {
        EnsureStop(); Require(tid == _thread && context.Eip == NativeCallDispatcher.WorldReturn &&
            (!_haveWorldStop || _host.DebugStopSequence > _lastWorldStop),
            "Overlay requires a distinct actual main-thread WorldRET.");
        _lastWorldStop = _host.DebugStopSequence; _haveWorldStop = true;
    }
    private void EnsureStop()
    {
        long now = Environment.TickCount64;
        Require(!_retired && !_failed && Environment.CurrentManagedThreadId == _owner && _host.IsAtDebugStop &&
            _host.ProcessHandle == _process && _host.ProcessPid == _pid && _host.MainThreadId == _thread &&
            _host.ImageBase == 0x400000 && now >= _lastNow && now <= _deadline &&
            (_operationDeadline == 0 || _nativeResumed || now <= _operationDeadline),
            "Overlay owner/process/thread or bounded monotonic lifetime changed.");
        _lastNow = now;
    }
    private void Failed(Exception error) { _failed = true; _failure = error.Message; }
    private uint P(uint at) { uint value = U(at); Require(value >= 0x10000 && (value & 3) == 0, "Overlay pointer is invalid/unaligned."); return value; }
    private uint U(uint at) => Win32Native.ReadUInt32(_process, at);
    private byte[] Read(uint at, int bytes) => Win32Native.ReadMemory(_process, at, bytes);
    private byte[] Image(uint at, int bytes) => _image.GetSectionData(checked((int)(at - 0x400000))).GetContent(0, bytes).ToArray();
    private uint ImageWord(uint at) => BitConverter.ToUInt32(Image(at, 4));
    private static string Hash(byte[] bytes) => Convert.ToHexString(SHA256.HashData(bytes));
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidDataException(message); }
}
