using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>Separate E3 PlayerTraits admission and preservation of primary-owned animation set31.</summary>
internal sealed partial class NativeTemplateProbe
{
    private bool PlayerTraitsFixture => _layout.TraitsClassId == 0x28C56217;
    private uint _playerTraitsAnimationManager, _playerTraitsCallback, _playerTraitsSharedReleaseSkips;
    private byte[] _playerTraitsSharedBucket = [];
    private bool _playerTraitsPreDeleteCaptured, _playerTraitsSharedSetPreserved;
    private object? _playerTraitsInitialization;
    private readonly bool _playerTraitsPreparePickupFx;
    private bool _playerTraitsPrimaryFxReady, _playerTraitsOwnFxReady;
    private bool _playerTraitsDeletePending;
    private uint _playerTraitsPrimaryTraits, _playerTraitsPrimaryFx, _playerTraitsPrimaryController, _playerTraitsPrimaryAnimation;
    private object? _playerTraitsPickupWitness;
    private readonly HashSet<uint> _playerTraitsPickupParticles = [];
    private readonly bool _sharedHealthAliasEnabled;
    private readonly bool _healthBattleFixture;
    private readonly bool _stingerSecondaryRoleEnabled;
    private StingerSecondaryRole? _stingerSecondaryRole;
    private uint _healthBattleWarmupWorlds, _healthBattleIdleWorlds, _healthBattleMetadataSamples;
    private bool _healthBattleHolding, _healthBattleFastRouteStarted;
    private uint _healthBattleLiveWorlds;
    private long _healthBattleLiveStarted;
    internal bool HealthBattleFastRoute => _healthBattleFastRouteStarted;
    private SharedHealthAlias? _sharedHealthAlias;

    private object PlayerTraitsFixtureSummary => new
    {
        enabled = PlayerTraitsFixture, initialized = _playerTraitsInitialization,
        preDeleteCaptured = _playerTraitsPreDeleteCaptured,
        ownGlobalSetReleaseSkips = _playerTraitsSharedReleaseSkips,
        primaryGlobalSetPreserved = _playerTraitsSharedSetPreserved,
        retainedSchedulerChecksPassed = PlayerTraitsFixture && _idle && _idleChecksPassed,
        ownPickupGraphCreated = _playerTraitsOwnFxReady,
        primaryPickupPrepared = _playerTraitsPrimaryFxReady, pickupSetup = _playerTraitsPickupWitness,
        sharedHealth = _sharedHealthAlias?.Summary, battleHealthFixture = _healthBattleFixture,
        battleWarmupWorldsWithoutOwnTicks = _healthBattleWarmupWorlds,
        battleLaterWorldsWithoutOwnTicks = _healthBattleIdleWorlds,
        liveWorldsAfterTwoCountedTicks = _healthBattleLiveWorlds,
        countedNativeCallbacksRetired = _healthBattleFastRouteStarted,
        stingerSecondaryRole = _stingerSecondaryRole?.Summary
    };

    private void BeginHealthBattleLiveHold()
    {
        Require(_healthBattleFixture && !_healthBattleHolding && _idleWorldStops == 2 &&
            _ownCharacterTicks == 2 && _ownMoveUpdates == 2 && _ownBloomNotifyTicks == 2 && _ownBloomUpdates == 2 &&
            _bloomRetentionFailure.Length == 0 && HairModeSucceeded && _sharedHealthAlias?.Active == true,
            "Battle live hold requires two counted own native ticks and a bound common HP owner.");
        CaptureRetainedBloomBodyGraph("before-health-battle-live-hold");
        _healthBattleHolding = _healthBattleFastRouteStarted = true;
        _healthBattleLiveStarted = Environment.TickCount64;
        _log(new { @event = "template-health-battle-live-hold-begin", countedNativeTicks = _idleWorldStops,
            detailedTemplateHooksRetireOnNextHit = true, fullManagerInventoryEveryWorld = false });
    }

    private bool ObserveHealthBattleLiveWorld(uint tid, ref Win32Native.X86Context context)
    {
        Require(_healthBattleHolding && _idleActive && _stage == 6 && !_dependentInterval &&
            _sharedHealthAlias?.Active == true && ++_healthBattleLiveWorlds <= 36000,
            "Battle live HP observation escaped its bounded ownership interval.");
        _sharedHealthAlias!.VerifyAtWorld(tid, ref context);
        if (_stingerSecondaryRole?.Active == true) _stingerSecondaryRole.VerifyAtWorld(tid, ref context);
        VerifyTemplateStarAtWorld(tid, ref context);
        VerifySharedMagicAtWorld(tid, ref context);
        VerifySharedDeathAtWorld(tid, ref context);
        VerifyCollisionDetectorAtWorld(tid, ref context);
        if (_bodyInputFixture || _ordinaryAttackFixture || _networkInputFixture || _stingerSecondaryRoleEnabled || _starSecondaryEligibilityEnabled || _collisionDetectorSecondaryRoleEnabled)
        {
            // Native damage feedback may change the struck body's own intent.
            // This fixture adds no command writer; keep its actual action fields.
            Table(_ownCommand, 0x703090);
            byte[] command = Read(_ownCommand, 0x74);
            Require(new[] { 4, 8, 12, 16, 20 }.All(offset => float.IsFinite(BitConverter.ToSingle(command, offset))),
                "Own native damage feedback produced a nonfinite command field.");
        }
        else _ = ReadNeutralCommand();
        if (_healthBattleLiveWorlds <= 2 || _healthBattleLiveWorlds % 120 == 0)
            _log(new { @event = "template-health-battle-live-world", worlds = _healthBattleLiveWorlds,
                elapsedMs = Math.Max(0, Environment.TickCount64 - _healthBattleLiveStarted),
                committedDamageTransactions = _sharedHealthAlias.CommittedDamageTransactions });
        bool goal = SharedBattleGameplayGoalPassed;
        if (_sharedBattleLifetimeEnabled)
        {
            Require(_sharedHealthAlias.CommittedDamageTransactions <= (_deathBattleFixture ? 10u : 4u),
                "Retained battle exceeded its explicit damage budget before genuine exit.");
            return false; // Genuine native unload owns cleanup in this separate fixture.
        }
        if (!goal && _sharedHealthAlias.CommittedDamageTransactions < 4) return false;
        _healthBattleHolding = false; _idleActive = false; _idleChecksPassed = goal;
        _log(new { @event = "template-health-battle-live-finished", countedNativeTicks = _idleWorldStops,
            liveWorlds = _healthBattleLiveWorlds, committedDamageTransactions = _sharedHealthAlias.CommittedDamageTransactions,
            secondaryDamageCommits = _sharedHealthAlias.SecondaryCommittedDamageTransactions,
            ownNativeStingerSendCount = _stingerSecondaryRole?.OwnNativeDamageSendCount,
            secondaryStarAwards = _starAwardObservation?.CompletedSecondaryAwards,
            nativeMagicSpendTransactions = _sharedMagicEnergy?.CommittedSpendTransactions,
            deathQualified = false, passed = goal, stoppedBeforeUnqualifiedLethalTest = !goal });
        return true;
    }

    private bool SkipHealthBattleWorldWithoutTick(uint tid, ref Win32Native.X86Context context)
    {
        if (!_healthBattleFixture || _ownCharacterTicks != _idleWorldStops) return false;
        Require(_ownMoveUpdates == _idleWorldStops && _ownBloomNotifyTicks == _idleWorldStops &&
            _ownBloomUpdates == _idleWorldStops && ++_healthBattleIdleWorlds <= 36000,
            "Battle scheduler warmup is partial or exceeds its explicit native-world budget.");
        if (_idleWorldStops == 0) ++_healthBattleWarmupWorlds;
        _ = ObserveIdleState("battle-await-first-native-tick", requirePromotion: false);
        if (_sharedHealthAlias?.Active == true) _sharedHealthAlias.VerifyAtWorld(tid, ref context);
        if (_stingerSecondaryRole?.Active == true) _stingerSecondaryRole.VerifyAtWorld(tid, ref context);
        VerifyTemplateStarAtWorld(tid, ref context);
        VerifySharedMagicAtWorld(tid, ref context);
        if (_healthBattleIdleWorlds <= 2 || _healthBattleIdleWorlds % 120 == 0)
            _log(new { @event = "template-health-battle-world-without-own-tick", worlds = _healthBattleIdleWorlds,
                ownCharacterTicks = _ownCharacterTicks, nativeSchedulerCalledManually = false });
        return true;
    }

    private void InstallHealthBattleMetadataObservation()
    {
        if (!_healthBattleFixture) return;
        uint primaryMove = Pointer(_localCharacter + 0x12C), metadata = Pointer(primaryMove + 0x174);
        _log(new { @event = "template-health-primary-physics-metadata", move = Hex(primaryMove),
            metadata = Hex(metadata), flagsWord = BitConverter.ToUInt16(Read(metadata + 4, 2)),
            instance = Hex(U(metadata + 8)), rootMetadata0C = Hex(U(_localRoot + 0xC)) });
        Hook(0x501E8D, "Stinger.accepted-metadata-readonly", (uint tid, uint at, ref Win32Native.X86Context context) =>
        {
            if (_healthBattleMetadataSamples >= 16 || _sharedHealthAlias?.Active != true) return;
            uint candidate = U(context.Ecx + 8);
            if (candidate != _record && candidate != _instance) return;
            Table(context.Esi, 0x6F6C7C);
            Require(U(context.Esi + 0xF4) == candidate, "Stinger candidate differs from its actual physics metadata.");
            ++_healthBattleMetadataSamples;
            _log(new { @event = "template-health-stinger-metadata-readonly", sample = _healthBattleMetadataSamples,
                projectile = Hex(context.Esi), filter = U(context.Esi + 0x50), metadata = Hex(context.Ecx),
                flagsWord = BitConverter.ToUInt16(Read(context.Ecx + 4, 2)), candidate = Hex(candidate),
                isPrimary = candidate == _record, primaryMoveMetadata = Hex(metadata),
                ownMoveMetadata = _move == 0 ? null : Hex(U(_move + 0x174)),
                rootMetadata0C = Hex(U(Pointer(candidate + 0x24) + 0xC)),
                registersAndGameFieldsChanged = false });
        }, preserveDuringHealthBattle: true);
    }

    private void ObserveSharedHealthAtWorld(uint tid, ref Win32Native.X86Context context)
    {
        if (!_sharedHealthAliasEnabled) return;
        Require(PlayerTraitsFixture && _playerTraitsOwnFxReady && _bloomIdle && _idleActive &&
            _stage == 6 && !_dependentInterval && _idleWorldStops >= 1,
            "Shared health binding escaped its admitted original scheduler fixture.");
        if (_sharedHealthAlias is null)
        {
            _sharedHealthAlias = new(_host, _image, _log);
            _sharedHealthAlias.BindAtWorld(tid, ref context, _record, Pointer(_localCharacter + 0x138),
                _localCharacter, _instance, _traits, _character, _machine);
        }
        _sharedHealthAlias.VerifyAtWorld(tid, ref context);
        ObserveTemplateStarAtWorld(tid, ref context);
        ObserveSharedMagicAtWorld(tid, ref context);
        ObserveStoryGameOverAtWorld(tid, ref context);
        ObserveCollisionDetectorAtWorld(tid, ref context);
        ObserveSharedDeathAtWorld(tid, ref context);
        if (_stingerSecondaryRoleEnabled)
        {
            Require(TargetRegionFixture && _targetRegionConfigured, "Own Stinger eligibility lacks its native TargetRegion.");
            if (_stingerSecondaryRole is null)
            {
                _stingerSecondaryRole = new(_host, _image, _log);
                _stingerSecondaryRole.BindAtWorld(tid, ref context, _instance, _ownRoot, _move, _scene);
            }
            _stingerSecondaryRole.VerifyAtWorld(tid, ref context);
        }
    }

    private bool TryPreparePrimaryPlayerTraitsFx(uint tid, ref Win32Native.X86Context context,
        NativeCallDispatcher calls, Action resume)
    {
        if (!_playerTraitsPreparePickupFx || _playerTraitsPrimaryFxReady) return false;
        Require(PlayerTraitsFixture && _stage == 5 && !_dependentInterval && _instance == 0,
            "Primary pickup setup is outside the explicit pre-construction fixture phase.");
        _playerTraitsPrimaryTraits = Pointer(_localCharacter + 0x138); Table(_playerTraitsPrimaryTraits, 0x6F6990);
        VerifyFunction(0x4FE440);
        if (U(_playerTraitsPrimaryTraits + 0x1DC) != 0)
        {
            CapturePrimaryPickupFx(); return false;
        }
        Call(tid, ref context, calls, resume, 0x4FE440, _playerTraitsPrimaryTraits, [], false, _ => CapturePrimaryPickupFx());
        return true;
    }

    private void CapturePrimaryPickupFx()
    {
        VerifyLocal();
        Require(U(_localCharacter + 0x138) == _playerTraitsPrimaryTraits && U(_playerTraitsPrimaryTraits) == 0x6F6990,
            "Primary pickup preparation lost its original PlayerTraits.");
        _playerTraitsPrimaryFx = Pointer(_playerTraitsPrimaryTraits + 0x1DC);
        _playerTraitsPrimaryController = Pointer(_playerTraitsPrimaryTraits + 0x1E0);
        _playerTraitsPrimaryAnimation = U(_playerTraitsPrimaryTraits + 0x1E4);
        Require(U(_playerTraitsPrimaryFx + 0x2C) == _localRoot && U(_playerTraitsPrimaryFx + 0x3C) == _scene,
            "Native primary pickup graph is not attached to the primary root/scene.");
        _playerTraitsAnimationManager = Pointer(0x765ADC); Table(_playerTraitsAnimationManager, 0x703470);
        _playerTraitsSharedBucket = Read(_playerTraitsAnimationManager + 0x26C, 12);
        _playerTraitsPrimaryFxReady = true;
        _log(new { @event = "template-playertraits-primary-pickup-prepared", traits = Hex(_playerTraitsPrimaryTraits),
            fx = Hex(_playerTraitsPrimaryFx), controller = Hex(_playerTraitsPrimaryController),
            animation = _playerTraitsPrimaryAnimation, sharedSet = 0x31,
            sharedBucketRaw = Convert.ToHexString(_playerTraitsSharedBucket), nativeSetupFunction = Hex(0x4FE440) });
    }

    private NativePostInitCallPlan PlayerTraitsPickupPlan() => new(0x4FE440,
        instance =>
        {
            Require(_playerTraitsPreparePickupFx && _playerTraitsPrimaryFxReady && instance == _instance &&
                _dependentInterval && U(_traits) == 0x6F6990 && U(_traits + 0x24) == instance &&
                U(_traits + 0x1DC) == 0, "Own pickup setup receiver differs from its initialized instance.");
            return _traits;
        },
        traits =>
        {
            Require(traits == _traits && _dependentInterval && U(traits) == 0x6F6990,
                "Own native pickup setup lost its exact PlayerTraits receiver.");
            uint fx = Pointer(traits + 0x1DC), controller = Pointer(traits + 0x1E0), animation = U(traits + 0x1E4);
            Require(fx != _playerTraitsPrimaryFx && controller != _playerTraitsPrimaryController &&
                U(fx + 0x2C) == _ownRoot && U(fx + 0x3C) == _scene &&
                U(_playerTraitsPrimaryTraits + 0x1DC) == _playerTraitsPrimaryFx &&
                U(_playerTraitsPrimaryTraits + 0x1E0) == _playerTraitsPrimaryController &&
                Read(_playerTraitsAnimationManager + 0x26C, 12).SequenceEqual(_playerTraitsSharedBucket),
                "Own pickup graph aliases the primary or changes its shared animation set31.");
            _playerTraitsOwnFxReady = true;
            VisualGraphReadResult pickupGraph = new VisualGraphReader(_host.ProcessHandle).Read(fx, _localNodes, inspectSkins: false);
            Require(pickupGraph.Success && pickupGraph.Graph.Nodes.All(node => node.Scene == _scene),
                "Native pickup FX graph is not a bounded owned subtree.");
            foreach (uint renderable in pickupGraph.Graph.Nodes.Where(node => node.Renderables is not null)
                .SelectMany(node => node.Renderables!.Items))
            {
                Table(renderable, VisualGraphReader.ParticleVtable);
                Require(_playerTraitsPickupParticles.Add(renderable), "Pickup particle is shared between own render nodes.");
            }
            Require(_playerTraitsPickupParticles.Count is > 0 and <= 64, "Pickup particle count is outside its bounded fixture.");
            uint metadata = Pointer(_move + 0x174);
            _playerTraitsPickupWitness = new { traits = Hex(traits), fx = Hex(fx), controller = Hex(controller), animation,
                fxParent = Hex(U(fx + 0x2C)), primaryFx = Hex(_playerTraitsPrimaryFx),
                physicsMetadata = Hex(metadata), metadataFlags = Hex(U(metadata + 4)), metadataInstance = Hex(U(metadata + 8)),
                sharedSet31Unchanged = true, nativeFunction = Hex(0x4FE440), originalSchedulerResumed = false,
                pickupGraph = pickupGraph.Graph, particleRenderables = _playerTraitsPickupParticles.Select(Hex).ToArray() };
            _log(new { @event = "template-playertraits-own-pickup-prepared", witness = _playerTraitsPickupWitness });
        });

    private void CapturePlayerTraitsBeforeScopedDelete()
    {
        if (!PlayerTraitsFixture) return;
        Require((!_idle || _bloomIdle && _playerTraitsPreparePickupFx) && _dependentInterval && _stage == 6 && _initializedWitness is not null &&
            U(_traits) == 0x6F6990 && U(_traits + 0x24) == _instance,
            "PlayerTraits candidate is outside its scoped initialized admission.");
        uint fx = U(_traits + 0x1DC), controller = U(_traits + 0x1E0), animation = U(_traits + 0x1E4);
        Require(fx == 0 && controller == 0 && animation == 0,
            "Scoped PlayerTraits admission has an unqualified pickup graph before deletion.");
        _playerTraitsAnimationManager = Pointer(0x765ADC); Table(_playerTraitsAnimationManager, 0x703470);
        if (!_playerTraitsPreparePickupFx) _playerTraitsSharedBucket = Read(_playerTraitsAnimationManager + 0x26C, 12);
        _playerTraitsCallback = Pointer(_traits + 0x1CC);
        _playerTraitsInitialization = new
        {
            traits = Hex(_traits), table = Hex(U(_traits)), type190 = U(_traits + 0x190),
            currentHealth = U(_traits + 0x124), maximumHealth = U(_traits + 0x128), previousHealth = U(_traits + 0x130),
            magic = new { enabled1A0 = Read(_traits + 0x1A0, 1)[0], currentRaw = Hex(U(_traits + 0x1A8)),
                maximumRaw = Hex(U(_traits + 0x1AC)), minimumRaw = Hex(U(_traits + 0x1B0)), previousRaw = Hex(U(_traits + 0x1C0)),
                primaryEnabled1A0 = Read(Pointer(_localCharacter + 0x138) + 0x1A0, 1)[0],
                primaryCurrentRaw = Hex(U(Pointer(_localCharacter + 0x138) + 0x1A8)),
                primaryPreviousRaw = Hex(U(Pointer(_localCharacter + 0x138) + 0x1C0)) },
            owner = Hex(U(_traits + 0x24)), character = Hex(U(_traits + 0x18C)),
            callback = Hex(_playerTraitsCallback), callbackRaw = Convert.ToHexString(Read(_playerTraitsCallback, 16)),
            pickupFx = Hex(fx), pickupController = Hex(controller), pickupAnimation = animation,
            animationManager = Hex(_playerTraitsAnimationManager), sharedSet = 0x31,
            sharedBucketRaw = Convert.ToHexString(_playerTraitsSharedBucket)
        };
        Require(U(_traits + 0x190) == 0 && U(_traits + 0x18C) == _character,
            "PlayerTraits candidate native type/Character relationship differs from stock Player.");
        _playerTraitsPreDeleteCaptured = true;
        _log(new { @event = "template-playertraits-pre-delete", witness = _playerTraitsInitialization,
            schedulerResumed = false, fullPlayerLifecycleQualified = false });
    }

    private void PreparePlayerTraitsRetainedDelete()
    {
        if (!PlayerTraitsFixture) return;
        Require(_idle && !_idleActive && !_dependentInterval && _stage == 6 &&
            _playerTraitsPreDeleteCaptured && _playerTraitsOwnFxReady && _playerTraitsPrimaryFxReady &&
            U(_traits) == 0x6F6990 && U(_traits + 0x24) == _instance &&
            U(_traits + 0x1DC) != 0 && U(_traits + 0x1E0) != 0 &&
            Read(_playerTraitsAnimationManager + 0x26C, 12).SequenceEqual(_playerTraitsSharedBucket),
            "Retained PlayerTraits is not ready for its own native deletion.");
        _playerTraitsDeletePending = true;
        _log(new { @event = "template-playertraits-retained-pre-delete", traits = Hex(_traits),
            pickupFx = Hex(U(_traits + 0x1DC)), pickupController = Hex(U(_traits + 0x1E0)),
            pickupAnimation = U(_traits + 0x1E4), ownNativeTicks = _ownCharacterTicks,
            sharedBucketRaw = Convert.ToHexString(_playerTraitsSharedBucket) });
    }

    internal bool TryPreservePrimaryAnimationSetAtOwnDelete(uint tid, ref Win32Native.X86Context context,
        uint entry, bool nativeCallPending)
    {
        if (!PlayerTraitsFixture || entry != 0x59AE80) return false;
        bool ownDeleteInterval = !_idle && _dependentInterval && _stage == 6 ||
            _idle && !_idleActive && !_dependentInterval && _stage == 7 && _playerTraitsDeletePending;
        Require(_host.IsAtDebugStop && tid == _host.MainThreadId && ownDeleteInterval &&
            nativeCallPending && _playerTraitsPreDeleteCaptured && _playerTraitsSharedReleaseSkips == 0 &&
            context.Eip == entry && context.Esi == _traits && U(_traits) == 0x6F6990 &&
            U(_traits + 0x24) == _instance && U(_instance) == 0x710F80 && U(_instance + 0x14) == _template &&
            context.Ecx == _playerTraitsAnimationManager && U(0x765ADC) == _playerTraitsAnimationManager &&
            U(context.Esp) == 0x4FE18C && U(context.Esp + 4) == 0x31,
            "Global set31 release is not the exact owned PlayerTraits scoped destructor.");
        byte[] call = [0xE8, 0xF4, 0xCC, 0x09, 0x00];
        Require(Read(0x4FE187, 5).SequenceEqual(call) &&
            Read(_playerTraitsAnimationManager + 0x26C, 12).SequenceEqual(_playerTraitsSharedBucket),
            "PlayerTraits destructor call or primary-owned set31 changed.");
        uint previousEsp = context.Esp;
        // The original callee RET4 would remove the return DWORD and arg31.
        // Only this owned destructor's unowned process-global release is omitted.
        context.Eip = 0x4FE18C; context.Esp = checked(previousEsp + 8);
        ++_playerTraitsSharedReleaseSkips;
        _log(new { @event = "template-playertraits-shared-set-release-skipped", entry = Hex(entry),
            caller = Hex(0x4FE18C), ownedTraits = Hex(_traits), sharedSet = 0x31,
            previousEsp = Hex(previousEsp), resumedEsp = Hex(context.Esp),
            originalCalleeRetArgumentBytes = 4, primaryOwnsSharedSet = true,
            preDeleteFxWasZero = !_playerTraitsOwnFxReady, ownNativeDestructorContinues = true,
            gameFieldWrites = 0, registersExceptEipEspPreserved = true });
        return true;
    }

    private void VerifyPlayerTraitsAfterScopedDelete()
    {
        if (!PlayerTraitsFixture) return;
        Require(_playerTraitsPreDeleteCaptured && _playerTraitsSharedReleaseSkips == 1 &&
            U(0x765ADC) == _playerTraitsAnimationManager &&
            Read(_playerTraitsAnimationManager + 0x26C, 12).SequenceEqual(_playerTraitsSharedBucket),
            "Owned PlayerTraits deletion changed the primary-owned animation set31.");
        _playerTraitsSharedSetPreserved = true;
        if (_playerTraitsPreparePickupFx)
            Require(_playerTraitsOwnFxReady && U(_playerTraitsPrimaryTraits + 0x1DC) == _playerTraitsPrimaryFx &&
                U(_playerTraitsPrimaryTraits + 0x1E0) == _playerTraitsPrimaryController &&
                U(_playerTraitsPrimaryTraits + 0x1E4) == _playerTraitsPrimaryAnimation,
                "Own PlayerTraits deletion damaged the original primary pickup binding.");
        _log(new { @event = "template-playertraits-scoped-delete-verified", ownGlobalSetReleaseSkips = 1,
            primarySharedSetPreserved = true, oldTraitsOrCallbackRead = false });
    }
}
