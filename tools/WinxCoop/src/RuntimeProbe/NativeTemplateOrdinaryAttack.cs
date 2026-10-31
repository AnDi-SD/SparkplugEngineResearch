using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>One explicit first-degree ordinary attack through the original scheduler and animation event.</summary>
internal sealed partial class NativeTemplateProbe
{
    private bool _ordinaryAttackFixture, _ordinaryRoleBound, _ordinaryAimReady, _ordinaryStarted, _ordinaryRetired;
    private bool _ordinaryPressed, _ordinaryReleased, _ordinaryFenced, _ordinaryAuthorization, _ordinaryGoal;
    private bool _ordinaryPreUnloadVerified, _ordinaryChooserPending, _ordinaryChosen5, _ordinaryLookupPending;
    private bool _ordinaryEventPending, _ordinaryEventReturned, _ordinaryActivationPending, _ordinaryActivationRetSeen;
    private bool _ordinaryAnimationMapped, _ordinaryTrajectoryMoved;
    private bool _ordinaryProposal5, _ordinaryCommitStoreSeen, _ordinaryTransitionCommitted;
    private uint _ordinaryRegion, _ordinaryState, _ordinaryClock, _ordinaryReadyWorlds, _ordinaryBaselineWorlds;
    private uint _ordinaryTimerStores, _ordinaryWrapperEntries, _ordinaryEvents, _ordinaryActivations, _ordinaryActivationReturns;
    private uint _ordinaryAnimation, _ordinaryProjectile, _ordinaryProjectileNode, _ordinaryTrajectorySamples;
    private uint _ordinaryAttackWrites, _ordinaryReleaseWrites, _ordinaryPauseFences, _ordinaryCallbackCount;
    private uint _ordinaryEventEsp, _ordinaryEventCaller;
    private byte[]? _ordinaryLastCommand;
    private float[]? _ordinaryActivationPosition, _ordinaryLastProjectilePosition;
    private string _ordinaryFailure = "";
    private OrdinaryPlayerProjectileRole? _ordinaryRole;
    private OrdinaryNativeFrame? _ordinaryActivationFrame;
    private sealed record OrdinaryNativeFrame(uint Esp, uint Caller, uint Ebx, uint Ebp, uint Esi, uint Edi);

    internal bool OrdinaryAttackFixtureRetired => !_ordinaryAttackFixture || _ordinaryRetired;
    internal bool OrdinaryAttackFixtureGoalPassed => _ordinaryAttackFixture && _ordinaryGoal && _ordinaryFailure.Length == 0;
    private object OrdinaryAttackFixtureSummary => new
    {
        enabled = _ordinaryAttackFixture, started = _ordinaryStarted, retired = _ordinaryRetired,
        passed = OrdinaryAttackFixtureGoalPassed, failure = _ordinaryFailure, aimReady = _ordinaryAimReady,
        source = "finite-local-one-native-world-command20-pulse", readyWorlds = _ordinaryReadyWorlds,
        pressed = _ordinaryPressed, released = _ordinaryReleased, fenced = _ordinaryFenced,
        timerStores = _ordinaryTimerStores, nativeHeldClock = _ordinaryClock, chosenNativeState5 = _ordinaryChosen5,
        proposedNativeState5 = _ordinaryProposal5, nativeState5StoreObserved = _ordinaryTransitionCommitted,
        mainPackedKey = "0x00000500", mainAnimation = Hex(_ordinaryAnimation), selectedBattle66EntryMatched = _ordinaryAnimationMapped,
        selectedSanFromPinnedBattle1Row = "bloid.san", runtimeSanFilenameRead = false,
        nativeOrbEvents = _ordinaryEvents, nativeEventReturned = _ordinaryEventReturned,
        native50A210Entries = _ordinaryWrapperEntries, nativeActivations = _ordinaryActivations,
        nativeActivationRET18Returns = _ordinaryActivationReturns, projectile = Hex(_ordinaryProjectile),
        trajectorySamples = _ordinaryTrajectorySamples, nativeTrajectoryMoved = _ordinaryTrajectoryMoved,
        activationPosition = _ordinaryActivationPosition, lastProjectilePosition = _ordinaryLastProjectilePosition,
        attackByteWrites = _ordinaryAttackWrites, releaseByteWrites = _ordinaryReleaseWrites, pauseFences = _ordinaryPauseFences,
        rolePolicy = _ordinaryRole?.Summary, command21Written = false, manualTicks = 0, transformWrites = 0,
        rawRoleOrMetadataWrites = 0, enemyDamageRequired = false, guestInputQualified = false,
        goalUsesCachedEvidenceOnly = true, wholeE4Qualified = false
    };

    private void ConfigureOrdinaryAttackFixture(ProbeConfiguration config)
    {
        _ordinaryAttackFixture = config.TemplateOrdinaryAttackFixture;
        if (!_ordinaryAttackFixture) return;
        Require(config.TemplateSharedPlayerProfile == "battle-01-ordinary-attack" && config.Mode == "template-bloom-body-hair-idle" &&
            config.TemplateProjectileManagerFixture && config.TemplatePlayerTraitsFixture && config.TemplatePlayerTraitsPickupFx &&
            config.TemplateTargetRegionFixture && config.TemplateConfigureTargetRegion && config.TemplateSharedHealthAlias &&
            config.TemplateHealthBattleFixture && config.TemplateSharedBattleLifetime && config.TemplateBattleCombatFixture &&
            config.PrepareFirstMagicStage && !config.TemplateBodyInputFixture && !config.TemplateSecondAttackDegreeFixture &&
            !config.TemplateDeathBattleFixture && !config.TemplateStoryDeathFixture && !config.TemplateCollisionDetectorSecondaryRole &&
            !config.TemplateStingerSecondaryRole && !config.TemplateStarSecondaryEligibility && !config.TemplateStarPlacementFixture &&
            !config.TemplateSharedMagicEnergyAlias,
            "Ordinary attack requires its separate retained Battle47 first-degree/shared-HP/PM cohort.");
    }

    private void BindOrdinaryRoleBeforeInit(uint tid, ref Win32Native.X86Context context)
    {
        if (!_ordinaryAttackFixture) return;
        Require(!_ordinaryRoleBound && _dependentInterval && _stage == 6 && context.Eip == 0x5096E0 &&
            context.Ecx == _projectileManager && _instance != 0 && DescriptorCount == 8,
            "Ordinary role must bind at the actual constructed own PM Init entry.");
        _ordinaryRegion = Components(_instance, DescriptorCount).Single(c => c.ClassId == 0x4307193A).Address;
        _ordinaryRoleBound = true;
        _ordinaryRole = new(_host, _image, _log);
        _ordinaryRole.BindStoppedBeforeInit(tid, ref context, _instance, _character, _projectileManager, 0, _ordinaryRegion,
            _record, _localCharacter, _projectilePrimaryManager, Pointer(_projectilePrimaryMove + 0x174),
            () => _ordinaryRoleBound && !_ordinaryRetired && _stage == 6 && !_sharedBattleUnloadStarted && _instance != 0,
            () => _dependentInterval && _stage == 6 || _ordinaryPreUnloadVerified,
            () => _ordinaryAuthorization && !_ordinaryFenced && _ordinaryFailure.Length == 0,
            maximumOwnLaunches: 1);
        InstallOrdinaryAttackWitnesses();
    }

    private NativePostInitCallPlan OrdinaryAimExclusionPlan() => new(0x4E2DC0,
        instance =>
        {
            Require(_ordinaryAttackFixture && _ordinaryRoleBound && _ordinaryRole is not null && _dependentInterval &&
                _stage == 6 && instance == _instance && _targetRegionConfigured && _ordinaryRegion == _targetRegion && !_ordinaryAimReady,
                "Ordinary IsTarget(false) requires completed native metadata configuration before first resume.");
            OrdinaryPin(0x4E2DC0, "8A442404888145010000C20400");
            return _ordinaryRegion;
        },
        region =>
        {
            Require(region == _ordinaryRegion && _dependentInterval && _host.IsAtDebugStop,
                "Ordinary native setter returned outside the owned stopped initialization interval.");
            _ordinaryRole!.VerifyAimExclusionStopped(_host.MainThreadId); _ordinaryAimReady = true;
        }, Arguments: [0u]);

    private void BeginOrdinaryAttackAtWorld(uint tid, ref Win32Native.X86Context context)
    {
        if (!_ordinaryAttackFixture) return;
        OrdinaryWorld(tid, context);
        Require(!_ordinaryStarted && _ordinaryAimReady && _ordinaryRole is not null && _sharedBattleCaptured &&
            _healthBattleHolding && _healthBattleFastRouteStarted && _idleWorldStops == 2 &&
            _ownCharacterTicks == 2 && _ownMoveUpdates == 2 && _ownBloomUpdates == 2 &&
            ReadSharedBattleFlow(out _, out _, out _, out _, out _),
            "Ordinary source starts after exactly two qualified retained native worlds.");
        VerifyOrdinaryOwners();
        _ordinaryState = Pointer(_machine + 0x15C);
        Require(U(_ordinaryState) == 0x6F8340 && U(_ordinaryState + 0x10) == 5 &&
            U(_ordinaryState + 0x14) == _machine && U(_ordinaryState + 0x18) == _animator,
            "Ordinary state5 is not bound to the own BloomFSM/Animator.");
        _ordinaryStarted = true; _ordinaryLastCommand = ReadOrdinaryAttackFixtureCommand();
        _log(new { @event = "template-ordinary-source-begin", command = Hex(_ownCommand), state5 = Hex(_ordinaryState),
            actionOnlyWriteOffset = 0x20, nativeHeldWorlds = 1, heldNativeLimitMs = 300, normalMainKey = "00000500" });
    }

    private void StepOrdinaryAttackAtWorld(uint tid, ref Win32Native.X86Context context, bool ready)
    {
        if (!_ordinaryAttackFixture || !_ordinaryStarted || _ordinaryRetired) return;
        OrdinaryWorld(tid, context); VerifyOrdinaryOwners();
        if (!ready)
        {
            OrdinaryWriteAttackByte(tid, context, 0); _ordinaryAuthorization = false; _ordinaryFenced = true;
            if (++_ordinaryPauseFences == 1 && !_ordinaryGoal) OrdinaryFail("Overlay fenced the unfinished one-shot action; it will not be replayed.");
            return;
        }
        _ordinaryRole!.VerifyAtWorldStopped(tid, ref context);
        if (U(_traits + 0x124) == 0 || U(_playerTraitsPrimaryTraits + 0x124) == 0)
            OrdinaryFail("Common HP reached zero during the ordinary action.");
        if (_ordinaryFailure.Length != 0 || _ordinaryFenced || _ordinaryGoal)
        { OrdinaryWriteAttackByte(tid, context, 0); _ordinaryAuthorization = false; return; }
        Require(++_ordinaryReadyWorlds <= 240, "Ordinary action exceeded240 ready native worlds.");
        if (!_ordinaryPressed)
        {
            byte[] command = ReadOrdinaryAttackFixtureCommand();
            Require(U(_machine + 0x130) == 0 && (U(_machine + 0x140) & 0x1F800F) == 0 && U(_machine + 0x298) == 0 &&
                command[0x20] == 0 && command[0x1F] == 0 && Read(_machine + 0x294, 1)[0] == 0,
                "Ordinary press baseline is not neutral grounded moving state0.");
            if (++_ordinaryBaselineWorlds < 4) return;
            _ordinaryPressed = true; _ordinaryAuthorization = true; OrdinaryWriteAttackByte(tid, context, 1); return;
        }
        if (!_ordinaryReleased)
        {
            uint now = U(_sharedBattleTimer + 0x1C), elapsed = unchecked(now - _ordinaryClock);
            OrdinaryWriteAttackByte(tid, context, 0); _ordinaryReleased = true;
            Require(_ordinaryTimerStores == 1 && U(_machine + 0x298) == _ordinaryClock && elapsed <= 300,
                "The single held native world did not produce its timer within the short-release interval.");
            _log(new { @event = "template-ordinary-short-release", nativeElapsedMs = elapsed, command21Preserved = true });
            return;
        }
        OrdinaryWriteAttackByte(tid, context, 0);
        if (_ordinaryActivationReturns == 1)
        {
            Require(U(_ordinaryProjectile) == 0x6F6B04 && U(_ordinaryProjectile + 0x18) == _projectileManager &&
                U(_ordinaryProjectile + 0x14) == _ordinaryProjectileNode, "Ordinary trajectory lost its live own pool graph.");
            _ordinaryLastProjectilePosition = OrdinaryPosition(_ordinaryProjectileNode); ++_ordinaryTrajectorySamples;
            _ordinaryTrajectoryMoved |= OrdinaryDistance(_ordinaryActivationPosition!, _ordinaryLastProjectilePosition) > 0.01;
        }
        if (_ordinaryChosen5 && _ordinaryTransitionCommitted && _ordinaryAnimationMapped && _ordinaryEvents == 1 && _ordinaryEventReturned &&
            _ordinaryWrapperEntries == 1 && _ordinaryActivations == 1 && _ordinaryActivationReturns == 1 &&
            _ordinaryTrajectorySamples >= 2 && _ordinaryTrajectoryMoved && U(_machine + 0x298) == 0)
        {
            _ordinaryGoal = true; _ordinaryAuthorization = false;
            _log(new { @event = "template-ordinary-source-goal", cached = true, projectile = Hex(_ordinaryProjectile),
                nativeTag = "event_orb", nativeMainKey = "00000500", nativeTable66ResourceMatched = true,
                mappedSan = "bloid.san", runtimeSanFilenameRead = false, enemyDamageClaimed = false });
        }
    }

    private void RetireOrdinaryAttackBeforeUnload(uint tid, ref Win32Native.X86Context context, uint entry,
        Action<uint, Win32Native.X86Context> verifyPreUnloadBoundary)
    {
        if (!_ordinaryAttackFixture || _ordinaryRetired) return;
        ArgumentNullException.ThrowIfNull(verifyPreUnloadBoundary);
        Require(_ordinaryRoleBound && _ordinaryRole is not null && _host.IsAtDebugStop &&
            tid == _host.MainThreadId && context.Eip == entry && !_ordinaryChooserPending && !_ordinaryLookupPending &&
            !_ordinaryEventPending && !_ordinaryActivationPending, "Ordinary retirement has an unmatched native action frame.");
        verifyPreUnloadBoundary(tid, context); _ordinaryPreUnloadVerified = true;
        OrdinaryWriteAttackByte(tid, context, 0); _ordinaryAuthorization = false; _ordinaryFenced = true;
        _ordinaryRole!.RetireBeforeOwnDelete(tid, ref context); _ordinaryRetired = true; _ordinaryPreUnloadVerified = false;
        _log(new { @event = "template-ordinary-retired-before-unload", entry = Hex(entry),
            cachedGoal = OrdinaryAttackFixtureGoalPassed, commandReleased = true, subsequentCommandReadsCached = true });
    }

    private byte[] ReadOrdinaryAttackFixtureCommand()
    {
        Require(_ordinaryAttackFixture, "Ordinary command route requires its explicit fixture.");
        if (_ordinaryRetired) return _ordinaryLastCommand is not null ? (byte[])_ordinaryLastCommand.Clone() :
            throw new InvalidDataException("Missing retired ordinary command snapshot.");
        Table(_ownCommand, 0x703090); byte[] command = Read(_ownCommand, 0x74);
        Require(new[] { 4, 8, 12, 16, 20 }.All(offset => float.IsFinite(BitConverter.ToSingle(command, offset))) &&
            BitConverter.ToSingle(command, 4) == 0 && command[0x1A] == 0 && command[0x1F] == 0 && command[0x20] <= 1,
            "Ordinary command changed an unadmitted movement/jump/defend field.");
        _ordinaryLastCommand = command; return command;
    }
    private void OrdinaryWriteAttackByte(uint tid, in Win32Native.X86Context context, byte value)
    {
        Require(tid == _host.MainThreadId && _host.IsAtDebugStop && value <= 1 &&
            (context.Eip == NativeCallDispatcher.WorldReturn || value == 0 && _ordinaryPreUnloadVerified),
            "Ordinary action mutation requires WorldRET or verified pre-delete neutral cleanup.");
        byte[] before = ReadOrdinaryAttackFixtureCommand();
        if (before[0x20] != value)
        {
            Win32Native.WriteMemory(_host.ProcessHandle, _ownCommand + 0x20, [value], executable: false);
            if (value == 1) ++_ordinaryAttackWrites; else ++_ordinaryReleaseWrites;
        }
        byte[] after = ReadOrdinaryAttackFixtureCommand(); before[0x20] = value;
        Require(after.SequenceEqual(before), "Ordinary command mutation changed a byte other than20.");
    }

    private void InstallOrdinaryAttackWitnesses()
    {
        OrdinaryPin(0x4FAD88, "FF524C"); OrdinaryPin(0x4FB14E, "FF504C"); OrdinaryPin(0x4FB7F2, "FF523C");
        OrdinaryPin(0x50A210, "8B81340300008B5424045052E8CFDEFFFFC20400");
        OrdinaryPin(0x50859D, "FF5224"); OrdinaryPin(0x4FFC07, "C21800");
        OrdinaryPin(0x5E79E0, "518B51048B4204538A581584DB56578B7C241875178B3739700C73058B4008EB048BD08B008A581584DB74EB");
        OrdinaryPin(0x59A60E, "85F6751F"); OrdinaryPin(0x59A61C, "8D8F38030000");
        OrdinaryHook(0x52579B, "898E98020000", "held-timer-store", (uint tid, uint at, ref Win32Native.X86Context c) =>
        {
            if (!_ordinaryStarted || c.Esi != _machine) return;
            Require(_ordinaryAuthorization && _ordinaryPressed && !_ordinaryReleased && Read(_ownCommand + 0x20, 1)[0] == 1 &&
                c.Ecx != 0 && c.Ecx == U(_sharedBattleTimer + 0x1C) && ++_ordinaryTimerStores == 1,
                "Ordinary held timer did not originate in the one admitted command20 world.");
            _ordinaryClock = c.Ecx;
        });
        OrdinaryHook(0x525B00, "53568B74240C578BFE", "chooser", (uint tid, uint at, ref Win32Native.X86Context c) =>
        {
            if (!_ordinaryStarted || c.Ecx != _machine || _ordinaryChosen5 || ((U(c.Esp + 4) >> 7) & 255) != 10) return;
            _log(new { @event = "template-ordinary-chooser-entry", threadId = tid, caller = Hex(U(c.Esp)),
                esp = Hex(c.Esp), ecx = Hex(c.Ecx), eax = Hex(c.Eax), edx = Hex(c.Edx),
                ebx = Hex(c.Ebx), ebp = Hex(c.Ebp), esi = Hex(c.Esi), edi = Hex(c.Edi),
                nativeArgument = Hex(U(c.Esp + 4)), released = _ordinaryReleased, authorized = _ordinaryAuthorization,
                chooserPending = _ordinaryChooserPending, currentState = U(_machine + 0x130),
                nativeFlags140 = Hex(U(_machine + 0x140)), nativeFlags144 = Hex(U(_machine + 0x144)),
                heldClock298 = U(_machine + 0x298), readyWorlds = _ordinaryReadyWorlds,
                stackPrefixHex = Convert.ToHexString(Read(c.Esp, 0x24)), nativeContextChanged = false });
            uint caller = U(c.Esp);
            Require(_ordinaryReleased && _ordinaryAuthorization && !_ordinaryChooserPending && caller is 0x4FB151 or 0x4FAD8B,
                "Ordinary action10 chooser escaped its original release/caller.");
            _ordinaryChooserPending = true; OrdinaryNativeFrame frame = OrdinaryFrame(c);
            OrdinaryReturn(tid, frame, 8, "chooser", (ref Win32Native.X86Context returned) =>
            {
                Require(returned.Eax == 5, "Ordinary action10 did not choose native state5.");
                if (frame.Caller == 0x4FB151) _ordinaryProposal5 = true;
                else
                { Require(_ordinaryProposal5, "Ordinary transition lacks its paired native update proposal."); _ordinaryChosen5 = true; }
                _ordinaryChooserPending = false;
            });
        });
        OrdinaryHook(0x4FAD91, "898630010000", "state-commit-store", (uint tid, uint at, ref Win32Native.X86Context c) =>
        {
            if (!_ordinaryStarted || c.Esi != _machine || !_ordinaryChosen5 || _ordinaryTransitionCommitted) return;
            Require(_ordinaryAuthorization && !_ordinaryChooserPending && !_ordinaryCommitStoreSeen && c.Eax == 5,
                "Ordinary native state store lost its completed transition query.");
            _ordinaryCommitStoreSeen = true;
        });
        OrdinaryHook(0x4FAD97, "8B8644010000", "state-commit-stored", (uint tid, uint at, ref Win32Native.X86Context c) =>
        {
            if (!_ordinaryStarted || c.Esi != _machine || !_ordinaryCommitStoreSeen || _ordinaryTransitionCommitted) return;
            Require(_ordinaryAuthorization && U(_machine + 0x130) == 5,
                "Original native state store did not commit own state5.");
            _ordinaryTransitionCommitted = true;
            _log(new { @event = "template-ordinary-native-state5-committed", machine = Hex(_machine),
                originalStore = "004FAD91", proposalAndTransitionQueriesPaired = true, nativeStateWritesByAdapter = 0 });
        });
        OrdinaryHook(0x5141D9, "E802640800", "main-key", (uint tid, uint at, ref Win32Native.X86Context c) =>
        {
            if (!_ordinaryStarted || c.Edi != _ordinaryState || _ordinaryAnimationMapped) return;
            Require(_ordinaryChosen5 && _ordinaryTransitionCommitted && _ordinaryAuthorization && !_ordinaryLookupPending && U(c.Esp) == 0x500 && U(c.Esp + 4) == 0 &&
                c.Ecx == U(0x765ADC), "Ordinary main lookup has a different key/group/manager.");
            _ordinaryLookupPending = true; uint esp = c.Esp, ebx = c.Ebx, ebp = c.Ebp, esi = c.Esi, edi = c.Edi;
            _host.Breakpoints.AddReturn(0x5141DE, tid, "Ordinary.main-key.return", (uint rt, uint ra, ref Win32Native.X86Context returned) =>
            {
                Require(rt == tid && ra == 0x5141DE && returned.Eip == ra && returned.Esp == checked(esp + 8) &&
                    returned.Ebx == ebx && returned.Ebp == ebp && returned.Esi == esi && returned.Edi == edi && returned.Eax >= 0x10000,
                    "Ordinary main native animation lookup lost its return frame.");
                uint manager = Pointer(0x765ADC);
                Require(!TryOrdinaryAnimationEntry(manager + 0x20, 0x500, out _) &&
                    TryOrdinaryAnimationEntry(manager + 0x20 + 66 * 12, 0x500, out uint selected) && selected == returned.Eax,
                    "Ordinary selected animation is not the exact Battle66 key500 fallback resource.");
                _ordinaryAnimation = returned.Eax; _ordinaryAnimationMapped = true; _ordinaryLookupPending = false;
            });
        });
        OrdinaryHook(0x513FA0, "8B4424048B401C8B40105356578BD9", "orb-event", OrdinaryOrbEvent);
        OrdinaryHook(0x50A210, "8B81340300008B5424045052", "ordinary-launch-wrapper", (uint tid, uint at, ref Win32Native.X86Context c) =>
        {
            if (c.Ecx != _projectileManager) return;
            Require(_ordinaryEventPending && _ordinaryAuthorization && c.Esp == _ordinaryEventEsp && U(c.Esp) == _ordinaryEventCaller &&
                U(c.Esp + 4) == 0 && ++_ordinaryWrapperEntries == 1, "Own launch wrapper lacks its exact native orb-event frame/type0.");
        });
        OrdinaryHook(0x4FF800, "83EC4C535556578BF1", "pool-activation", OrdinaryActivation);
        OrdinaryHook(0x4FFC07, "C21800", "pool-activation-RET18", (uint tid, uint at, ref Win32Native.X86Context c) =>
        {
            if (!_ordinaryActivationPending) return;
            OrdinaryNativeFrame frame = _ordinaryActivationFrame!;
            Require(!_ordinaryActivationRetSeen && c.Esp == frame.Esp && U(c.Esp) == frame.Caller && OrdinaryPreserved(frame, c),
                "Own ordinary activation lacks its exact native RET18 witness.");
            _ordinaryActivationRetSeen = true;
        });
    }

    private void OrdinaryOrbEvent(uint tid, uint at, ref Win32Native.X86Context c)
    {
        if (!_ordinaryStarted || c.Ecx != _ordinaryState) return;
        uint message = Pointer(c.Esp + 4), tag = Pointer(Pointer(message + 0x1C) + 0x10);
        if (!Read(tag, 10).SequenceEqual("event_orb\0"u8.ToArray())) return;
        Require(_ordinaryReleased && _ordinaryAuthorization && _ordinaryChosen5 && _ordinaryAnimationMapped &&
            U(_machine + 0x130) == 5 && U(_ordinaryState + 0x24) == _ordinaryAnimation &&
            U(c.Esp) == 0x4FB7F5 && !_ordinaryEventPending && ++_ordinaryEvents == 1,
            "Own event_orb is not from its admitted native state/clip/Animator dispatch.");
        _ordinaryEventPending = true; _ordinaryEventEsp = c.Esp; _ordinaryEventCaller = U(c.Esp);
        OrdinaryNativeFrame frame = OrdinaryFrame(c);
        OrdinaryReturn(tid, frame, 8, "orb-event", (ref Win32Native.X86Context returned) =>
        {
            Require(_ordinaryWrapperEntries == 1 && _ordinaryActivationReturns == 1 && Read(_ordinaryState + 0x3C, 1)[0] == 1,
                "Native orb-event returned without its own wrapper, activation or event-seen store.");
            _ordinaryEventReturned = true; _ordinaryEventPending = false;
        });
    }
    private void OrdinaryActivation(uint tid, uint at, ref Win32Native.X86Context c)
    {
        if (U(c.Ecx + 0x18) != _projectileManager) return;
        uint projectile = c.Ecx;
        Require(_ordinaryEventPending && _ordinaryAuthorization && _ordinaryWrapperEntries == 1 && !_ordinaryActivationPending &&
            U(c.Ecx) == 0x6F6B04 && U(c.Ecx + 0x50) == 1 && Read(c.Ecx + 0x30, 1)[0] == 0 &&
            U(c.Esp) == 0x5085A0 && Enumerable.Range(0, 6).Count(i => U(_projectileManager + 0x1D0 + (uint)i * 4) == projectile) == 1 &&
            ++_ordinaryActivations == 1, "Own ordinary activation lost its typed type0 inactive pool/source/caller proof.");
        _ordinaryProjectile = c.Ecx; _ordinaryProjectileNode = Pointer(c.Ecx + 0x14);
        _ordinaryActivationPending = true; _ordinaryActivationRetSeen = false;
        OrdinaryNativeFrame frame = OrdinaryFrame(c); _ordinaryActivationFrame = frame;
        OrdinaryReturn(tid, frame, 28, "pool-activation", (ref Win32Native.X86Context returned) =>
        {
            Require(_ordinaryActivationRetSeen && (returned.Eax & 255) == 1 && Read(_ordinaryProjectile + 0x30, 1)[0] == 1 &&
                U(_ordinaryProjectile + 0x18) == _projectileManager && ++_ordinaryActivationReturns == 1,
                "Own native activation did not return active through RET18.");
            _ordinaryActivationPosition = OrdinaryPosition(_ordinaryProjectileNode); _ordinaryActivationPending = false;
        });
    }

    private delegate void OrdinaryReturned(ref Win32Native.X86Context context);
    private void OrdinaryReturn(uint tid, OrdinaryNativeFrame frame, uint delta, string name, OrdinaryReturned completed) =>
        _host.Breakpoints.AddReturn(frame.Caller, tid, "Ordinary." + name + ".return", (uint rt, uint ra, ref Win32Native.X86Context returned) =>
        {
            Require(_ordinaryRoleBound && !_ordinaryRetired && _host.IsAtDebugStop && rt == tid && tid == _host.MainThreadId &&
                ra == frame.Caller && returned.Eip == ra && returned.Esp == checked(frame.Esp + delta) && OrdinaryPreserved(frame, returned),
                "Ordinary native caller/receiver/thread/ESP return pair failed: " + name);
            completed(ref returned);
        });
    private OrdinaryNativeFrame OrdinaryFrame(Win32Native.X86Context c) => new(c.Esp, U(c.Esp), c.Ebx, c.Ebp, c.Esi, c.Edi);
    private static bool OrdinaryPreserved(OrdinaryNativeFrame frame, Win32Native.X86Context c) =>
        c.Ebx == frame.Ebx && c.Ebp == frame.Ebp && c.Esi == frame.Esi && c.Edi == frame.Edi;
    private bool TryOrdinaryAnimationEntry(uint tree, uint key, out uint value)
    {
        uint head = Pointer(tree + 4), node = Pointer(head + 4); var seen = new HashSet<uint>();
        while (Read(node + 0x15, 1)[0] == 0)
        {
            Require(seen.Count < 128 && seen.Add(node), "Ordinary numeric animation lookup exceeded its finite native tree bound.");
            uint candidate = U(node + 0xC);
            if (candidate == key) { value = U(node + 0x10); return true; }
            node = Pointer(node + (candidate < key ? 8u : 0u));
        }
        Require(node == head, "Ordinary animation lookup ended at a foreign sentinel."); value = 0; return false;
    }
    private void VerifyOrdinaryOwners()
    {
        Require(_ordinaryAttackFixture && _host.IsAtDebugStop && _stage == 6 && !_dependentInterval && _idleActive &&
            _sharedBattleCaptured && !_sharedBattleUnloadStarted && _healthBattleHolding && _healthBattleFastRouteStarted &&
            _level == 47 && DescriptorCount == 8 && _sharedHealthAlias?.Active == true && _ownCommand != _command &&
            U(_character + 0x130) == _ownCommand && U(_machine + 0x12C) == _ownCommand && U(_machine + 0x124) == _character &&
            U(_machine + 0x128) == 0 && U(_character + 0x148) == 1 && U(_character + 0x14C) == 41 &&
            U(_character + 0x140) == _projectileManager && U(_animator + 0x124) == _character && U(_animator + 0x128) == _ownActor &&
            U(_profile + 0x2B4) == _record && Read(_profile + 0x510, 1)[0] == 1 && Read(_profile + 0x512, 1)[0] == 0 &&
            U(_profile + 0x6E4) == 1 && (U(_profile + 0x42C) & 0x200000) == 0,
            "Ordinary action escaped the own first-degree normal Battle47 command/Animator/profile tuple.");
    }
    private float[] OrdinaryPosition(uint node)
    {
        Require(U(node + 0x3C) == _scene, "Ordinary projectile Node left the owned scene.");
        byte[] raw = Read(node + 0x74, 12); float[] pose = Enumerable.Range(0, 3).Select(i => BitConverter.ToSingle(raw, i * 4)).ToArray();
        Require(pose.All(x => float.IsFinite(x) && MathF.Abs(x) <= 10000000), "Ordinary projectile position is nonfinite/out of bounds."); return pose;
    }
    private static double OrdinaryDistance(float[] a, float[] b) => Math.Sqrt(Enumerable.Range(0, 3).Sum(i => Math.Pow((double)a[i] - b[i], 2)));
    private void OrdinaryFail(string reason)
    { if (_ordinaryFailure.Length == 0) { _ordinaryFailure = reason; _ordinaryAuthorization = false; _ordinaryFenced = true; _log(new { @event = "template-ordinary-source-failed", reason, replayAllowed = false }); } }
    private void OrdinaryWorld(uint tid, in Win32Native.X86Context c) => Require(_host.IsAtDebugStop &&
        tid == _host.MainThreadId && c.Eip == NativeCallDispatcher.WorldReturn, "Ordinary source requires actual main-thread WorldRET.");
    private void OrdinaryPin(uint address, string hex)
    {
        byte[] expected = Convert.FromHexString(hex), original = _image.GetSectionData(checked((int)(address - 0x400000))).GetContent(0, expected.Length).ToArray();
        byte[] live = Read(address, expected.Length);
        for (int i = 0; i < live.Length; ++i)
            if (live[i] == 0xCC && original[i] != 0xCC && _host.Breakpoints.Contains(address + (uint)i)) live[i] = original[i];
        Require(expected.SequenceEqual(original) && expected.SequenceEqual(live), "Ordinary action source/live pin differs: " + Hex(address));
    }
    private void OrdinaryHook(uint address, string hex, string name, BreakpointCallback observe)
    {
        OrdinaryPin(address, hex); Require(!_host.Breakpoints.Contains(address), "Ordinary action hook already has an owner.");
        BreakpointCallback repeat = null!;
        repeat = (uint tid, uint at, ref Win32Native.X86Context c) =>
        {
            Require(tid == _host.MainThreadId && at == address && c.Eip == at && _host.IsAtDebugStop, "Ordinary witness identity differs.");
            if (_ordinaryRetired) return;
            Require(++_ordinaryCallbackCount <= 100000, "Ordinary action callback budget exceeded.");
            try { OrdinaryPin(address, hex); observe(tid, at, ref c); }
            catch (Exception error)
            {
                string? stack = null, stackError = null;
                try { stack = Convert.ToHexString(Read(c.Esp, 0x30)); }
                catch (Exception readError) { stackError = readError.Message; }
                _log(new { @event = "template-ordinary-witness-rejected", name, address = Hex(at), threadId = tid,
                    error = error.Message, eip = Hex(c.Eip), esp = Hex(c.Esp), eax = Hex(c.Eax), ecx = Hex(c.Ecx),
                    edx = Hex(c.Edx), ebx = Hex(c.Ebx), ebp = Hex(c.Ebp), esi = Hex(c.Esi), edi = Hex(c.Edi),
                    stackPrefixHex = stack, stackReadError = stackError, nativeContextChangedByDiagnostic = false });
                throw;
            }
            if (!_ordinaryRetired) _host.Breakpoints.AddReturn(address, tid, "Ordinary." + name, repeat);
        };
        _host.Breakpoints.AddReturn(address, _host.MainThreadId, "Ordinary." + name, repeat);
    }
}
