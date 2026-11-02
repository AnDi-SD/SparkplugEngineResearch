using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>Own opt-in Battle47 lifetime. E1 body-live state and policy stay separate.</summary>
internal sealed partial class NativeTemplateProbe
{
    // Root declares/configures _sharedBattleLifetimeEnabled and supplies the
    // managed gameplay-goal and adapter-retirement integration methods.
    private static uint SharedBattleWorldLimit => NativeObservationBudget.WorldLimit;
    private const uint SharedBattlePostWorldLimit = 3600;
    private const uint SharedBattleEmptyFlowWorldLimit = 120;
    private const long SharedBattleEmptyFlowMilliseconds = 10000;
    private bool _sharedBattleCaptured, _sharedBattlePaused, _sharedBattleUnloadStarted;
    private bool _sharedBattleBatchCompleted, _sharedBattleResourcesReleased, _sharedBattleGoalAtUnload;
    private bool _sharedBattleTemplateOwnerHeld, _sharedBattleNativePauseObserved;
    private uint _sharedBattleAnimationManager, _sharedBattleTimer, _sharedBattleWorlds, _sharedBattlePostWorlds;
    private uint _sharedBattleMenuWorlds, _sharedBattleUnloadEntry, _sharedBattleReferenceWrites;
    private uint _sharedBattleTemplateMembership, _sharedBattlePauseTransitions, _sharedBattleResumeTransitions;
    private uint _sharedBattleNativePauseTransitions;
    private uint _sharedBattleEmptyFlowWorlds;
    private uint _sharedBattleSceneManager, _sharedBattleSceneManagerTable;
    private long _sharedBattleEmptyFlowStarted;
    private ushort _sharedBattleLastTemplateRefs;
    private byte _sharedBattleLastNativePaused;
    private byte _sharedBattleLastInputEnabled = byte.MaxValue;
    private byte _sharedBattleLastMenuCommand = byte.MaxValue, _sharedBattleLastPauseArm = byte.MaxValue;
    private uint _sharedBattleMenuEdgeObservations;
    private long _sharedBattleStarted, _sharedBattleDeadline, _sharedBattlePauseStarted, _sharedBattlePausedMilliseconds;
    private long _sharedBattleNativePauseStarted, _sharedBattleLongestNativePauseMilliseconds;
    private long _sharedBattleLastNativePauseMilliseconds, _sharedBattleLongestResumedNativePauseMilliseconds;
    private int _sharedBattleDuration;
    private uint[] _sharedBattleDeletedEntities = [];
    private object? _sharedBattleUnloadWitness;

    internal bool SharedBattleLifetimeCaptured => _sharedBattleCaptured;
    internal bool SharedBattleUnloadStarted => _sharedBattleUnloadStarted;
    internal bool SharedBattleLifetimePaused => _sharedBattlePaused;
    internal bool SharedBattleLifetimeCompleted => _sharedBattleLifetimeEnabled &&
        _sharedBattleBatchCompleted && _sharedBattleResourcesReleased && _sharedBattleMenuWorlds >= 2 &&
        _cleaned && !_sharedBattleTemplateOwnerHeld && _sharedBattleReferenceWrites == 2 &&
        (_mosquitoPlayerTargets is null || !_mosquitoPlayerTargets.HasLiveOwnership);
    internal bool SharedBattleLifetimeSucceeded => SharedBattleLifetimeCompleted && _sharedBattleGoalAtUnload && SharedStoryOutcomePassed;
    internal object SharedBattleLifetimeSummary => new
    {
        enabled = _sharedBattleLifetimeEnabled, captured = _sharedBattleCaptured, paused = _sharedBattlePaused,
        configuredSeconds = _sharedBattleDuration, observedWorlds = _sharedBattleWorlds,
        postUnloadWorlds = _sharedBattlePostWorlds, consecutiveMainMenuWorlds = _sharedBattleMenuWorlds,
        pauseTransitions = _sharedBattlePauseTransitions, resumeTransitions = _sharedBattleResumeTransitions,
        notReadyMilliseconds = _sharedBattlePausedMilliseconds, nativePauseObserved = _sharedBattleNativePauseObserved,
        nativePauseTransitions = _sharedBattleNativePauseTransitions, nativePaused = _sharedBattleLastNativePaused,
        gameTimer = Hex(_sharedBattleTimer), longestNativePauseMilliseconds = _sharedBattleLongestNativePauseMilliseconds,
        longestResumedNativePauseMilliseconds = _sharedBattleLongestResumedNativePauseMilliseconds,
        pauseResumeObserved = _sharedBattleNativePauseObserved && _sharedBattleResumeTransitions != 0 &&
            _sharedBattleLongestResumedNativePauseMilliseconds > 0,
        pauseMinimumDurationRequired = false, overlayNotReadyIsNotNativePauseProof = true,
        templateAdapterOwnerHeld = _sharedBattleTemplateOwnerHeld, templateAdapterReferenceWrites = _sharedBattleReferenceWrites,
        lastTemplateReferences = _sharedBattleLastTemplateRefs, lastTemplateManagerMembership = _sharedBattleTemplateMembership,
        unloadStarted = _sharedBattleUnloadStarted, unloadEntry = Hex(_sharedBattleUnloadEntry),
        nativeBatchCompleted = _sharedBattleBatchCompleted, resourcesReleasedAtLaterWorld = _sharedBattleResourcesReleased,
        emptyPostUnloadFlowWorlds = _sharedBattleEmptyFlowWorlds,
        gameplayGoalObservedBeforeUnload = _sharedBattleGoalAtUnload, completed = SharedBattleLifetimeCompleted, succeeded = SharedBattleLifetimeSucceeded,
        storyOutcomeObservedAfterUnload = SharedStoryOutcomePassed,
        witness = _sharedBattleUnloadWitness, oldPlayerOrOwnObjectReadsAfterUnload = false,
        bodyLiveE1StateReused = false, nativePauseOrFlowWrites = 0,
        privateInputWrites = _bodyInputFixture || _ordinaryAttackFixture || _networkInputFixture ? (int?)null : 0,
        privateInputAdapterEnabled = _bodyInputFixture || _ordinaryAttackFixture || _networkInputFixture, fullE3Complete = false
    };

    private void CaptureSharedBattleLifetimeAtWorld(uint tid, ref Win32Native.X86Context context, int durationSeconds)
    {
        if (!_sharedBattleLifetimeEnabled) return;
        SharedBattleWorld(tid, ref context);
        Require(!_sharedBattleCaptured && !_bodyLive && _healthBattleFixture && PlayerTraitsFixture && _idle && _bloomIdle &&
            _stage == 6 && _idleActive && !_dependentInterval && _idleWorldStops == 2 &&
            _ownCharacterTicks == 2 && _ownMoveUpdates == 2 && _ownBloomNotifyTicks == 2 && _ownBloomUpdates == 2 &&
            _bloomIntervalVerified && _bloomBodyGraphVerified && _nearPositioned && HairModeSucceeded && !HairHasLiveOwnership &&
            _commandMovement is null && _sharedHealthAlias?.Active == true && durationSeconds is >= 5 and <= 600,
            "Shared Battle lifetime must activate at its two qualified own worlds before live hold.");
        _sharedBattleAnimationManager = Pointer(0x765ADC); Table(_sharedBattleAnimationManager, 0x703470);
        _sharedBattleTimer = Pointer(0x755298); Table(_sharedBattleTimer, 0x702F94);
        _sharedBattleSceneManager = Pointer(0x75DB90); _sharedBattleSceneManagerTable = U(_sharedBattleSceneManager);
        Require(ClassId(_sharedBattleTimer) == 0x78665312 && Read(_sharedBattleTimer + 0x40, 1)[0] == 0,
            "Shared Battle must first capture its typed native GameTimer unpaused.");
        if (_sharedPlayerProfile.Story)
        {
            PinSharedBattle(0x5F5450, "B8988C7600C3");
            PinSharedBattle(0x5F86C0, "5356518BC433DB8BF151889E7E010000");
            Require(ClassId(_active) == 0x70EB6440, "Story playing state has a different registered native class.");
        }
        else
        {
            PinSharedBattle(0x5F2080, "B8708B7600C3");
            PinSharedBattle(0x5F2300, "568BF18B8E540200008B812801000050E83BAAF0FF");
            PinSharedBattle(0x5F2338, "528BCEE8A0E30000");
            PinSharedBattle(0x60072C, "8B54240C528BCEE8887FFFFF");
        }
        PinSharedBattle(0x4628B5, "8BC68B48088B3666FF4908668379080075068B016A01FF10");
        PinSharedBattle(0x5968F0, "8A4140C3"); PinSharedBattle(0x596900, "C6414001"); PinSharedBattle(0x596910, "C6414000");
        PinSharedBattle(0x5EF690, "B890897600C3");
        Require(SharedBattleOriginalWord(_sharedPlayerProfile.PlayingTable + 0x10) == _sharedPlayerProfile.TypeGetter &&
            U(_sharedPlayerProfile.PlayingTable + 0x10) == _sharedPlayerProfile.TypeGetter &&
            SharedBattleOriginalWord(_sharedPlayerProfile.PlayingTable + 0x24) == _sharedPlayerProfile.ExitEntry &&
            U(_sharedPlayerProfile.PlayingTable + 0x24) == _sharedPlayerProfile.ExitEntry,
            "Shared Battle type/early exit slot differs from the original native class.");
        VerifySharedBattleBorrowedOwners(fullLocal: true); VerifySharedBattleOwnGraph();
        Require(ReadSharedBattleFlow(out _, out _, out _, out _, out _) &&
            !_sharedBattleTemplateOwnerHeld && _sharedBattleReferenceWrites == 0,
            "Shared Battle capture requires genuine ready Battle47 and no prior adapter template owner.");
        Table(_template, 0x703D24);
        Require(_templateRefs == 1 && Refs(_template) == 1 && U(_template + 4) == 0 && CountBodyTemplateManagerMembership() == 1,
            "Shared Battle template retain requires exactly its original manager registration.");
        WriteSharedBattleTemplateReferences(2);
        _sharedBattleTemplateOwnerHeld = true; _sharedBattleLastTemplateRefs = 2; _sharedBattleTemplateMembership = 1;
        _sharedBattleDuration = durationSeconds; _sharedBattleStarted = Environment.TickCount64;
        _sharedBattleDeadline = checked(_sharedBattleStarted + (long)durationSeconds * 1000);
        _sharedBattleCaptured = true;
        _log(new { @event = "template-shared-battle-lifetime-captured", battle = Hex(_active), battleTable = Hex(_sharedPlayerProfile.PlayingTable),
            level = _sharedPlayerProfile.Level, profile = _sharedPlayerProfile.Name,
            template = Hex(_template), nativeReferencesBefore = 1, references = 2, adapterOwners = 1,
            rootReferences = ExpectedOwnRootReferences, descriptors = DescriptorCount, ownEntities = _ownEntities.Length,
            gameTimer = Hex(_sharedBattleTimer), nativePaused = 0, configuredSeconds = durationSeconds,
            nativePauseOrFlowWrites = 0, nativeEntityFieldsWritten = 0 });
    }

    // Root calls before VerifyLocal, and holds its ordinary observation/command
    // phase while this returns false. False does not change Flow or timer state.
    private bool ObserveSharedBattleLifetimeAtWorld(uint tid, ref Win32Native.X86Context context)
    {
        if (!_sharedBattleLifetimeEnabled) return true;
        SharedBattleWorld(tid, ref context);
        Require(_sharedBattleCaptured && !_sharedBattleUnloadStarted && _stage == 6 && _idleActive &&
            !_dependentInterval && Environment.TickCount64 <= _sharedBattleDeadline && ++_sharedBattleWorlds <= SharedBattleWorldLimit,
            "Shared Battle lifetime escaped its captured bounded retained interval.");
        VerifySharedBattleBorrowedOwners(fullLocal: false); VerifySharedBattleTemplateOwner(false);
        bool ready = ReadSharedBattleFlow(out uint top, out uint stack, out uint current, out uint pending, out uint deviceContext);
        ObserveSharedBattlePause(ready, top, stack, current, pending, deviceContext);
        StepBodyInputFixtureAtWorld(tid, ref context, ready);
        StepOrdinaryAttackAtWorld(tid, ref context, ready);
        StepNetworkInputAtWorld(tid, ref context, ready);
        if (_sharedBattleWorlds <= 2 || _sharedBattleWorlds % 120 == 0)
            _log(new { @event = "template-shared-battle-lifetime-world", world = _sharedBattleWorlds, ready,
                elapsedMs = Math.Max(0, Environment.TickCount64 - _sharedBattleStarted), nativePaused = _sharedBattleLastNativePaused,
                current, pending, stack, deviceContext,
                templateReferences = _sharedBattleLastTemplateRefs, managerMembership = _sharedBattleTemplateMembership,
                inputEnabled = Read(_input + 0x20, 1)[0], nativeMenuCommand2B = Read(_command + 0x2B, 1)[0],
                playingPauseArm = Read(_active + 0x3C, 1)[0], playingPauseInhibited = Read(_active + 0x17D, 1)[0] });
        return ready;
    }

    private bool ReadSharedBattleFlow(out uint top, out uint stack, out uint current, out uint pending, out uint deviceContext)
    {
        stack = U(_flow + 0x1AC); Require(stack <= 20, "Shared Battle flow stack exceeds its bounded extent.");
        top = 0; bool retained = false;
        for (uint index = 0; index <= stack; ++index)
        { uint state = U(_flow + 0x15C + index * 4); if (state == _active) retained = true; if (index == stack) top = state; }
        Require(retained && top >= 0x10000 && U(_active) == _sharedPlayerProfile.PlayingTable &&
            U(_active + 0x10) == _sharedPlayerProfile.Level && _level == _sharedPlayerProfile.Level,
            "Captured shared-player scene is no longer a live member beneath its flow overlay.");
        current = U(_flow + 0x1B0); pending = U(_flow + 0x1B8); deviceContext = U(_device + 0x14);
        byte paused = Read(_sharedBattleTimer + 0x40, 1)[0];
        Require(paused <= 1, "Shared Battle typed GameTimer pause byte exceeds0/1.");
        return top == _active && current == _sharedPlayerProfile.Level && pending == 0 && deviceContext == 3 && paused == 0;
    }

    private void ObserveSharedBattlePause(bool ready, uint top, uint stack, uint current, uint pending, uint deviceContext)
    {
        long now = Environment.TickCount64; byte paused = Read(_sharedBattleTimer + 0x40, 1)[0];
        Require(paused <= 1, "Shared Battle GameTimer pause byte is not a native bool.");
        byte inputEnabled = Read(_input + 0x20, 1)[0];
        Require(inputEnabled <= 1, "Shared Battle UserInput enable byte is not a native bool.");
        byte menuCommand = Read(_command + 0x2B, 1)[0], pauseArm = Read(_active + 0x3C, 1)[0];
        Require(menuCommand <= 1 && pauseArm <= 1, "Shared Battle native menu command/arm left its bool domain.");
        if (menuCommand != _sharedBattleLastMenuCommand || pauseArm != _sharedBattleLastPauseArm)
        {
            Require(++_sharedBattleMenuEdgeObservations <= 256, "Shared Battle menu edge observation budget exceeded.");
            _sharedBattleLastMenuCommand = menuCommand; _sharedBattleLastPauseArm = pauseArm;
            _log(new { @event = "template-shared-battle-menu-arm-observed", menuCommand, pauseArm, inputEnabled,
                nativePaused = paused, current, pending, stack, deviceContext, observedAtMilliseconds = now,
                nativeCommandOrArmWrites = 0 });
        }
        if (inputEnabled != _sharedBattleLastInputEnabled)
        {
            _sharedBattleLastInputEnabled = inputEnabled;
            _log(new { @event = "template-shared-battle-input-enable-observed", inputEnabled,
                nativePaused = paused, top = Hex(top), stack, current, pending, deviceContext,
                privateInputWrites = 0, observedAtMilliseconds = now });
        }
        if (paused == 1)
        {
            _sharedBattleNativePauseObserved = true;
            if (_sharedBattleNativePauseStarted == 0) _sharedBattleNativePauseStarted = now;
            _sharedBattleLastNativePauseMilliseconds = Math.Max(0, now - _sharedBattleNativePauseStarted);
            _sharedBattleLongestNativePauseMilliseconds = Math.Max(_sharedBattleLongestNativePauseMilliseconds, _sharedBattleLastNativePauseMilliseconds);
        }
        else if (_sharedBattleNativePauseStarted != 0)
        {
            _sharedBattleLastNativePauseMilliseconds = Math.Max(0, now - _sharedBattleNativePauseStarted);
            _sharedBattleLongestNativePauseMilliseconds = Math.Max(_sharedBattleLongestNativePauseMilliseconds, _sharedBattleLastNativePauseMilliseconds);
            _sharedBattleNativePauseStarted = 0;
        }
        if (paused != _sharedBattleLastNativePaused)
        {
            Require(++_sharedBattleNativePauseTransitions <= 64, "Shared Battle pause-transition budget exceeded.");
            _log(new { @event = "template-shared-battle-native-pause-transition", previous = _sharedBattleLastNativePaused,
                nativePaused = paused, gameTimer = Hex(_sharedBattleTimer), top = Hex(top), stack, current, pending, deviceContext,
                timerCurrentRaw = Hex(U(_sharedBattleTimer + 0x1C)), timerDeltaRaw = Hex(U(_sharedBattleTimer + 0x28)), ready,
                observedPauseMilliseconds = _sharedBattleLastNativePauseMilliseconds, nativeTimerCallsOrWrites = 0 });
            _sharedBattleLastNativePaused = paused;
        }
        if (!ready && !_sharedBattlePaused)
        { _sharedBattlePaused = true; _sharedBattlePauseStarted = now; Require(++_sharedBattlePauseTransitions <= 64, "Shared Battle overlay budget exceeded."); }
        else if (ready && _sharedBattlePaused)
        {
            _sharedBattlePausedMilliseconds += Math.Max(0, now - _sharedBattlePauseStarted);
            _sharedBattleLongestResumedNativePauseMilliseconds = Math.Max(_sharedBattleLongestResumedNativePauseMilliseconds, _sharedBattleLastNativePauseMilliseconds);
            _sharedBattleLastNativePauseMilliseconds = 0; _sharedBattlePaused = false; ++_sharedBattleResumeTransitions;
            _log(new { @event = "template-shared-battle-resumed", nativePaused = paused, current, pending, deviceContext,
                notReadyMilliseconds = _sharedBattlePausedMilliseconds, resumedNativePauseMilliseconds = _sharedBattleLongestResumedNativePauseMilliseconds,
                heldInputReplayed = false, nativeInputWrites = _bodyInputFixture || _ordinaryAttackFixture || _networkInputFixture ? (int?)null : 0 });
        }
    }

    internal bool TryBeginSharedBattleUnload(DebuggerHost host, uint tid, ref Win32Native.X86Context context,
        uint entry, byte firstByte, NativeCallDispatcher calls)
    {
        if (!_sharedBattleLifetimeEnabled || !HasLiveOwnership || _sharedBattleUnloadStarted) return false;
        Require(ReferenceEquals(host, _host) && host.IsAtDebugStop && tid == host.MainThreadId && context.Eip == entry &&
            _sharedBattleCaptured && !_bodyLive && _stage == 6 && _idleActive && !_dependentInterval && _healthBattleHolding &&
            _healthBattleFastRouteStarted && !calls.Pending && !calls.HasRetiredPage && !HairHasLiveOwnership && HairModeSucceeded &&
            _commandMovement is null && _instance != 0 && _constructing == 0 && _template != 0 && _stream != 0 &&
            Environment.TickCount64 <= _sharedBattleDeadline,
            "Shared Battle unload cannot interrupt an unqualified or partial retained lifecycle.");
        Require(entry is 0x5F2300 or 0x5F86C0 or 0x4E8AA0 or 0x4E8C80 or 0x59AE80 or 0x59B190,
            "Shared Battle unload entry is outside its pinned native cohort.");
        VerifyFunction(entry); Require(Read(entry, 1)[0] == firstByte, "Shared Battle original unload byte differs.");
        VerifySharedBattleBorrowedOwners(fullLocal: true); VerifySharedBattleOwnGraph(); VerifySharedBattleTemplateOwner(true);
        _ = ReadSharedBattleFlow(out uint top, out _, out _, out _, out _);
        if (entry is 0x5F2300 or 0x5F86C0)
            Require((entry != 0x5F2300 || !_sharedPlayerProfile.Story) && context.Ecx == _active && top == _active &&
                U(context.Ecx) == _sharedPlayerProfile.PlayingTable,
                "Shared-player exit is not the exact captured playing-state receiver.");
        else if (entry is 0x4E8AA0 or 0x4E8C80)
            Require(context.Ecx == _profile, "Shared Battle profile removal lost its captured primary receiver.");
        else Require(context.Ecx == _sharedBattleAnimationManager, "Shared Battle cache cleanup is not its original animation manager.");
        Table(_stream, 0x6E7E50); Require(Refs(_stream) == _streamRefs && Refs(_instance) == _instanceRefs,
            "Shared Battle native deletion owners differ from their retained baseline.");
        foreach (uint function in new uint[] { 0x5FB350, 0x5A05B0, 0x465730 }) VerifyFunction(function);

        uint contextEsp = context.Esp, contextReceiver = context.Ecx;
        void VerifyInputUnloadBoundary(uint verifiedTid, Win32Native.X86Context verifiedContext)
        {
            Require(verifiedTid == tid && verifiedContext.Eip == entry && verifiedContext.Esp == contextEsp &&
                verifiedContext.Ecx == contextReceiver && _host.IsAtDebugStop && !_sharedBattleUnloadStarted &&
                _stage == 6 && _idleActive && _healthBattleHolding && !_dependentInterval && !calls.Pending &&
                Read(entry, 1)[0] == firstByte,
                "Body input retirement lost the validated original pre-unload context.");
            VerifySharedBattleBorrowedOwners(fullLocal: true); VerifySharedBattleOwnGraph(); VerifySharedBattleTemplateOwner(true);
        }
        RetireBodyInputFixtureBeforeUnload(tid, ref context, entry, VerifyInputUnloadBoundary);
        RetireNetworkInputBeforeUnload(tid, ref context, entry, VerifyInputUnloadBoundary);
        RetireOrdinaryAttackBeforeUnload(tid, ref context, entry, VerifyInputUnloadBoundary);
        PrepareProjectileManagerRetainedDeleteBaseline();

        _sharedBattleGoalAtUnload = SharedBattleGameplayGoalPassed;
        if (_sharedBattlePaused)
        { _sharedBattlePausedMilliseconds += Math.Max(0, Environment.TickCount64 - _sharedBattlePauseStarted); _sharedBattlePaused = false; }
        if (_sharedBattleNativePauseStarted != 0)
        {
            _sharedBattleLastNativePauseMilliseconds = Math.Max(0, Environment.TickCount64 - _sharedBattleNativePauseStarted);
            _sharedBattleLongestNativePauseMilliseconds = Math.Max(_sharedBattleLongestNativePauseMilliseconds, _sharedBattleLastNativePauseMilliseconds);
            _sharedBattleNativePauseStarted = 0; // Unload is not counted as an observed ready resume.
        }
        _healthBattleHolding = false; _idleActive = false;
        PreparePlayerTraitsRetainedDelete(); CaptureTargetRegionRetainedDeleteBaseline();
        // Root must retire every optional peer with this original entry context,
        // award observer before eligibility. No fabricated WorldRET is allowed.
        RetireSharedBattleAdaptersAtEntry(tid, ref context);
        ushort previous = Refs(_template), remaining = checked((ushort)(previous - 1));
        WriteSharedBattleTemplateReferences(remaining); _sharedBattleTemplateOwnerHeld = false;
        _sharedBattleLastTemplateRefs = remaining;
        uint deletedInstance = _instance, deletedTemplate = _template, deletedStream = _stream;
        _sharedBattleDeletedEntities = (uint[])_ownEntities.Clone();
        _sharedBattleUnloadEntry = entry; _sharedBattleUnloadStarted = true; _stage = 7;
        _sharedBattleUnloadWitness = new { entry = Hex(entry), receiver = Hex(context.Ecx), instance = Hex(_instance),
            traits = Hex(_traits), template = Hex(_template), stream = Hex(_stream), root = Hex(_ownRoot),
            originalEsp = Hex(context.Esp), originalReturn = Hex(U(context.Esp)), previousTemplateReferences = previous,
            remainingTemplateReferences = remaining, templateManagerMembership = _sharedBattleTemplateMembership,
            adapterReferenceWrites = _sharedBattleReferenceWrites, identitiesHeldThroughOwnDestructor = true,
            gameplayGoalBeforeUnload = _sharedBattleGoalAtUnload };
        _log(new { @event = "template-shared-battle-pre-unload-begin", witness = _sharedBattleUnloadWitness });
        NativeCallSpec[] batch =
        [ new(0x5FB350, deletedInstance, [1], false), new(0x5A05B0, deletedTemplate, [1], false), new(0x465730, deletedStream, [1], false) ];
        calls.BeginAtEntry(host, tid, ref context, entry, firstByte, batch, results =>
        {
            Require(results.Length == 3 && U(0x765ACC) == _gameManager && _playerTraitsDeletePending,
                "Shared Battle batch completion lost its live manager/result/delete witness.");
            // Original unload has not executed: primary is still live. These
            // helpers read primary/cache/registry, never deleted own bytes.
            VerifyPlayerTraitsAfterScopedDelete(); VerifyTargetRegionDeleted();
            _instance = 0; VerifyProjectileManagerDeleted();
            Require(Memberships(_sharedBattleDeletedEntities).Matches.Length == 0,
                "Shared Battle deleted components remain in fresh game-manager membership.");
            _instance = 0; _ownRoot = 0; _ownCommand = 0; _ownActor = 0;
            _character = 0; _move = 0; _machine = 0; _animator = 0; _traits = 0; _targetRegion = 0;
            _template = 0; _stream = 0; _buffer = 0; _ownEntities = []; _playerTraitsDeletePending = false;
            Require(!HasLiveOwnership, "Shared Battle native deletion left an owned borrower.");
            _sharedBattleBatchCompleted = true;
            _log(new { @event = "template-shared-battle-pre-unload-cleaned", entry = Hex(entry), nativeResults = results,
                deletedInstance = Hex(deletedInstance), deletedTemplate = Hex(deletedTemplate), deletedStream = Hex(deletedStream),
                oldOwnBytesRead = false, originalEntryStillNotExecuted = true, fullSavedEntryContextResumes = true });
        });
        return true;
    }

    internal void StepSharedBattlePostUnload(DebuggerHost host, uint tid, ref Win32Native.X86Context context,
        NativeCallDispatcher calls, bool freshMainMenuObserved, Action releaseOwnedResources)
    {
        ArgumentNullException.ThrowIfNull(releaseOwnedResources); SharedBattleWorld(tid, ref context);
        Require(_sharedBattleLifetimeEnabled && _sharedBattleUnloadStarted && _sharedBattleBatchCompleted &&
            ReferenceEquals(host, _host) && !calls.Pending && Environment.TickCount64 <= _sharedBattleDeadline &&
            ++_sharedBattlePostWorlds <= (_sharedPlayerProfile.Story && _deathBattleFixture ? 360000 : SharedBattlePostWorldLimit),
            "Shared Battle post-unload requires a later real WorldRET after its completed native entry batch.");
        calls.ReleaseRetiredAtWorldStop(host, tid, ref context);
        _mosquitoPlayerTargets?.ReleaseRetiredPagesAtWorld(tid, ref context);
        Require(!HasLiveOwnership && _instance == 0 && _traits == 0 && _template == 0 && _stream == 0 &&
            U(0x765ACC) == _gameManager && Memberships(_sharedBattleDeletedEntities).Matches.Length == 0,
            "Shared Battle post-unload still owns or reobserved a deleted graph in the current manager.");
        if (!_sharedBattleResourcesReleased)
        {
            releaseOwnedResources(); Require(!HairHasLiveOwnership, "Shared Battle still owns appearance data after native borrows ended.");
            _sharedBattleResourcesReleased = true;
            _log(new { @event = "template-shared-battle-post-unload-resources-released", atLaterWorld = true, oldPlayerRead = false });
        }
        // Only fresh Flow/type data is read here; no captured Profile, Instance,
        // old scene, animation resources, or body graph is dereferenced.
        uint flow = Pointer(0x755294), stack = U(flow + 0x1AC), pending = U(flow + 0x1B8);
        if (stack == uint.MaxValue)
        {
            // The original constructor and pop-until path define-1 as EMPTY.
            // Observe it only after native unload with all borrows released;
            // queued state/operation words remain diagnostics, not a promise
            // of a particular transition or permission to call the engine.
            Require(_sharedBattleResourcesReleased && !HasLiveOwnership && !calls.HasRetiredPage &&
                !_sharedBattleTemplateOwnerHeld && _sharedBattleReferenceWrites == 2 && !freshMainMenuObserved &&
                flow == _flow && U(flow) == 0x702D2C && U(flow + 0x10) == 0x702D28 &&
                U(0x702D3C) == 0x595DB0 && U(0x766088) == 0x078B20E8 &&
                U(0x755274) == _engine && U(0x765ACC) == _gameManager && U(_gameManager) == 0x70069C &&
                U(0x75DB90) == _sharedBattleSceneManager && U(_sharedBattleSceneManager) == _sharedBattleSceneManagerTable &&
                U(_sharedBattleSceneManager + 0x20) == 0 && U(flow + 0x1B0) == 0 &&
                Read(flow + 0x15C, 0x50).All(value => value == 0),
                "Empty post-unload Flow lacks its surviving owners, cleared stack or released native scene.");
            PinSharedBattle(0x595BC8, "C786AC010000FFFFFFFF");
            PinSharedBattle(0x5966B7, "8B8EAC010000C7848E5C010000000000008B86AC010000488986AC0100007810");
            PinSharedBattle(0x596746, "C786B001000000000000");
            PinSharedBattle(0x595DB0, "B888607600C3");
            long now = Environment.TickCount64;
            if (_sharedBattleEmptyFlowStarted == 0) _sharedBattleEmptyFlowStarted = now;
            Require(++_sharedBattleEmptyFlowWorlds <= SharedBattleEmptyFlowWorldLimit &&
                now - _sharedBattleEmptyFlowStarted <= SharedBattleEmptyFlowMilliseconds,
                "Original empty Flow did not progress within its bounded post-unload interval.");
            _sharedBattleMenuWorlds = 0;
            if (_sharedBattleEmptyFlowWorlds <= 2)
                _log(new { @event = "template-shared-battle-post-unload-empty-flow", world = _sharedBattlePostWorlds,
                    flow = Hex(flow), stack, current = 0, pending, operation = U(flow + 0x1BC), delay = U(flow + 0x1C0),
                    queuedWordsAreDiagnosticOnly = true, emptyStackSlots = 20,
                    emptyWorlds = _sharedBattleEmptyFlowWorlds, maximumWorlds = SharedBattleEmptyFlowWorldLimit,
                    maximumMilliseconds = SharedBattleEmptyFlowMilliseconds, originalEmptyStackEncodingVerified = true,
                    resourcesAlreadyReleased = true, settledMainMenu = false, topIndexedOrDereferenced = false,
                    oldPlayerOrOwnObjectRead = false, nativeFlowOrTimerWrites = 0 });
            return;
        }
        Require(stack <= 20, "Fresh post-unload flow stack is not bounded.");
        uint top = Pointer(flow + 0x15C + stack * 4), table = U(top);
        bool menu = table == 0x7102A8 && pending == 0;
        if (table == 0x7102A8)
            Require(U(table + 0x10) == 0x5EF690 && U(0x768990) == 0x284B3AA2 &&
                Read(0x5EF690, 6).SequenceEqual(new byte[] { 0xB8, 0x90, 0x89, 0x76, 0, 0xC3 }),
                "Fresh shared Battle post-unload MainMenu type differs.");
        Require(menu == freshMainMenuObserved, "Shared Battle caller/helper fresh MainMenu observations differ.");
        _sharedBattleMenuWorlds = menu ? _sharedBattleMenuWorlds + 1 : 0;
        if (_sharedBattlePostWorlds <= 2 || menu && _sharedBattleMenuWorlds <= 2)
            _log(new { @event = "template-shared-battle-post-unload-world", world = _sharedBattlePostWorlds,
                flow = Hex(flow), top = Hex(top), table = Hex(table), pending, settledMainMenu = menu,
                consecutiveMainMenuWorlds = _sharedBattleMenuWorlds, oldPlayerOrOwnObjectRead = false });
        if (_sharedBattleMenuWorlds >= 2)
        {
            _cleaned = true; _stage = 10; _idleChecksPassed = _sharedBattleGoalAtUnload;
            if (_sharedBattleMenuWorlds == 2)
                _log(new { @event = "template-shared-battle-lifetime-complete", nativeDeleteAndLaterWorldRelease = true,
                    gameplayGoalBeforeUnload = _sharedBattleGoalAtUnload, passed = SharedBattleLifetimeSucceeded,
                    nativePauseObserved = _sharedBattleNativePauseObserved, resumeTransitions = _sharedBattleResumeTransitions,
                    resumedNativePauseMilliseconds = _sharedBattleLongestResumedNativePauseMilliseconds, oldPlayerRead = false });
        }
    }

    private void VerifySharedBattleBorrowedOwners(bool fullLocal)
    {
        Require(U(0x765AD4) == _profile && U(_profile) == 0x6F5A44 && U(_profile + 0x2B4) == _record &&
            U(_record) == 0x710F80 && U(_record + 0x24) == _localRoot && U(_record + 0x20) == 11 &&
            U(_localRoot + 0x2C) == _parent && U(_localRoot + 0x3C) == _scene &&
            U(0x755294) == _flow && U(0x765ACC) == _gameManager && U(0x75DC64) == _templateManager &&
            U(0x765ADC) == _sharedBattleAnimationManager && U(_sharedBattleAnimationManager) == 0x703470 &&
            U(0x755298) == _sharedBattleTimer && U(_sharedBattleTimer) == 0x702F94 &&
            U(0x755274) == _engine && U(_engine + 0x1C) == _camera &&
            U(0x7552A0) == _input && U(_input + 0x14) == _inputReceiver && U(_input + 0x18) == _command && U(_input + 0x1C) == _device,
            "Shared Battle borrowed primary/input/flow/timer/cache ownership changed before unload.");
        if (fullLocal) Require(LocalComponentKeys() == _localComponentKeys && U(_localAnimator + 0x128) == _localActor,
            "Shared Battle pre-unload primary component/Actor identities changed.");
    }

    private void VerifySharedBattleOwnGraph()
    {
        Require(_ownEntities.Length == DescriptorCount - 1 && _bloomIntervalVerified && _bloomBodyGraphVerified &&
            U(_instance) == 0x710F80 && U(_instance + 0x20) == DescriptorCount && U(_instance + 0x14) == _template &&
            U(_instance + 0x24) == _ownRoot && U(_ownRoot) == VisualGraphReader.NodeVtable && Refs(_ownRoot) == ExpectedOwnRootReferences &&
            U(_character) == 0x6F6438 && U(_traits) == 0x6F6990 && U(_machine) == 0x6FA288 &&
            U(_move) == 0x6F64F8 && U(_animator) == 0x6F670C && U(_ownActor) == 0x703F80 &&
            U(_character + 0x24) == _instance && U(_character + 0x18) == _ownRoot &&
            U(_character + 0x138) == _traits && U(_traits + 0x24) == _instance && U(_traits + 0x18C) == _character &&
            U(_character + 0x124) == _machine && U(_character + 0x130) == _ownCommand &&
            U(_character + 0x128) == _animator && U(_character + 0x12C) == _move &&
            U(_machine + 0x24) == _instance && U(_machine + 0x124) == _character && U(_machine + 0x12C) == _ownCommand &&
            U(_machine + 0x128) == 0 && _observedBloomKind == 0 &&
            U(_move + 0x160) == _character && U(_animator + 0x124) == _character && U(_animator + 0x128) == _ownActor &&
            _ownCommand != _command && _ownActor != _localActor,
            "Shared Battle live own graph differs from its layout-derived component/reference contract.");
        VerifyOwnedInstance(scheduled: true); Table(_ownCommand, 0x703090);
        List<Component> components = Components(_instance, DescriptorCount);
        Require(_ownEntities.All(entity => components.Count(component => component.Address == entity &&
            component.Owner == _instance && component.BoundNode == _ownRoot) == 1),
            "Shared Battle own component identity/Root ownership changed.");
        byte[] command = Read(_ownCommand, 0x74);
        Require(new[] { 4, 8, 12, 16, 20 }.All(offset => float.IsFinite(BitConverter.ToSingle(command, offset))),
            "Shared Battle native intent contains a nonfinite field; it is not rewritten.");
    }

    private void VerifySharedBattleTemplateOwner(bool force)
    {
        Require(_sharedBattleTemplateOwnerHeld && _sharedBattleReferenceWrites == 1 && !_sharedBattleUnloadStarted,
            "Shared Battle template owner is outside its one-retain interval.");
        Table(_template, 0x703D24); ushort references = Refs(_template);
        if (force || references != _sharedBattleLastTemplateRefs)
        {
            uint membership = CountBodyTemplateManagerMembership();
            Require(references == 1 + membership, "Shared Battle template reference transition has an unknown owner.");
            _sharedBattleTemplateMembership = membership; _sharedBattleLastTemplateRefs = references;
            _log(new { @event = "template-shared-battle-template-owner-observed", template = Hex(_template),
                references, managerMembership = membership, adapterOwners = 1, forcedListObservation = force });
        }
        Require(references is 1 or 2 && references == 1 + _sharedBattleTemplateMembership && U(_template + 4) == 0,
            "Shared Battle template lost its exact adapter/manager ownership.");
    }

    private void WriteSharedBattleTemplateReferences(ushort references)
    {
        Require(_sharedBattleLifetimeEnabled && !_sharedBattleUnloadStarted && _host.IsAtDebugStop && _sharedBattleReferenceWrites < 2,
            "Shared Battle template reference write exceeds its two owned transitions.");
        Table(_template, 0x703D24); byte[] bytes = BitConverter.GetBytes(references);
        Win32Native.WriteMemory(_host.ProcessHandle, _template + 8, bytes, executable: false);
        Require(Read(_template + 8, 2).SequenceEqual(bytes), "Shared Battle owned template reference write/readback differs.");
        ++_sharedBattleReferenceWrites;
    }
    private void SharedBattleWorld(uint tid, ref Win32Native.X86Context context) => Require(_host.IsAtDebugStop &&
        tid == _host.MainThreadId && Environment.CurrentManagedThreadId == _ownerThread && context.Eip == NativeCallDispatcher.WorldReturn &&
        Read(NativeCallDispatcher.WorldReturn, 1)[0] == 0xC3, "Shared Battle lifetime requires its exact owned main-thread WorldRET.");
    private uint SharedBattleOriginalWord(uint address) => BitConverter.ToUInt32(_image.GetSectionData(checked((int)(address - 0x400000))).GetContent(0, 4).ToArray());
    private void PinSharedBattle(uint address, string hex)
    {
        byte[] expected = Convert.FromHexString(hex), actual = Read(address, expected.Length);
        byte[] original = _image.GetSectionData(checked((int)(address - 0x400000))).GetContent(0, expected.Length).ToArray();
        for (int index = 0; index < actual.Length; ++index)
            if (actual[index] == 0xCC && original[index] != 0xCC && _host.Breakpoints.Contains(address + (uint)index)) actual[index] = original[index];
        Require(original.SequenceEqual(expected) && actual.SequenceEqual(expected), $"Shared Battle original source pin differs at {Hex(address)}.");
    }
}
