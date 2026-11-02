using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// Bounded, separately selected network ordinary-action observation. The input
/// controller is the sole command20 writer. Native states, events, projectiles
/// and enemy health operations remain original.
/// </summary>
internal sealed partial class NativeTemplateProbe
{
    private const uint NetworkOrdinaryMaximumActions = 8;
    private bool _ordinaryNetwork, _networkOrdinaryLastHeld, _networkOrdinaryPaused, _networkOrdinaryAwaitNeutral;
    private bool _networkOrdinaryRetired, _networkOrdinaryGoal;
    private byte _networkOrdinaryCommand21;
    private uint _networkOrdinaryActions, _networkOrdinaryWorlds, _networkOrdinaryNativeActivations;
    private uint _networkOrdinaryEnemyCommits, _networkOrdinaryPauseFences, _networkOrdinaryRearms;
    private NetworkOrdinaryAction? _networkOrdinaryAction;
    private NetworkOrdinarySend? _networkOrdinarySend;
    private NetworkOrdinaryDamage? _networkOrdinaryDamage;
    private NativeCharacterHurtState? _networkOrdinaryHurtObserver;
    private NativeCharacterHurtWitness? _networkOrdinaryHurtFenceWitness;
    private bool _networkOrdinaryHurtAwaitNeutral;
    private uint _networkOrdinaryHurtSuppressed, _networkOrdinaryHurtNeutralRearms;
    private readonly Dictionary<uint, NetworkOrdinaryAction> _networkOrdinaryProjectiles = [];
    private readonly List<object> _networkOrdinaryCompleted = [];

    private sealed class NetworkOrdinaryAction(uint id, DiagnosticInputRecord source, uint destination, uint world)
    {
        internal readonly uint Id = id, Epoch = source.InputEpoch, SourceSequence = source.SourceSequence,
            DestinationSequence = destination, BeginWorld = world;
        internal readonly ulong CaptureOrdinal = source.CaptureOrdinal;
        internal bool Released, Proposed, Chosen, Committed, StoreSeen, LookupPending, AnimationMapped;
        internal bool ChooserPending, EventPending, EventReturned, ActivationPending, ActivationRetSeen, Completed;
        internal uint Clock, TimerStores, Animation, Events, Wrappers, Activations, ActivationReturns;
        internal uint Projectile, Node, TrajectorySamples, EnemyCommits, HeldNativeMilliseconds;
        internal bool TrajectoryMoved;
        internal float[]? Origin, LastPosition;
        internal OrdinaryNativeFrame? ActivationFrame;
    }

    private sealed class NetworkOrdinarySend(NetworkOrdinaryAction action, uint projectile, uint target,
        uint metadata, uint traits, uint character, OrdinaryNativeFrame frame, uint damage)
    {
        internal readonly NetworkOrdinaryAction Action = action;
        internal readonly uint Projectile = projectile, Target = target, Metadata = metadata, Traits = traits,
            Character = character, Damage = damage;
        internal readonly OrdinaryNativeFrame Frame = frame;
        internal uint DamageEntries, DamageReturns, Commits;
    }

    private sealed class NetworkOrdinaryDamage(NetworkOrdinarySend send, OrdinaryNativeFrame frame,
        uint before, uint maximum, uint minimum)
    {
        internal readonly NetworkOrdinarySend Send = send;
        internal readonly OrdinaryNativeFrame Frame = frame;
        internal readonly uint Before = before, Maximum = maximum, Minimum = minimum;
        internal uint Requested, After, Previous, SetterCall, SetterReturn;
        internal bool SetterSeen, SetterReturned, NativeRetSeen;
    }

    internal bool NetworkOrdinaryFixtureGoalPassed => _ordinaryNetwork && _networkOrdinaryGoal && _ordinaryFailure.Length == 0;
    internal bool NetworkOrdinaryFixtureRetired => !_ordinaryNetwork || _networkOrdinaryRetired;
    private object NetworkOrdinarySummary => new
    {
        enabled = _ordinaryNetwork, source = "IPC5-ordered-held20-levels", started = _ordinaryStarted,
        retired = _networkOrdinaryRetired, failure = _ordinaryFailure, passed = NetworkOrdinaryFixtureGoalPassed,
        actions = _networkOrdinaryActions, maximumActions = NetworkOrdinaryMaximumActions,
        nativeActivations = _networkOrdinaryNativeActivations, nativeEnemyHealthCommits = _networkOrdinaryEnemyCommits,
        pauseFences = _networkOrdinaryPauseFences, freshNeutralRearms = _networkOrdinaryRearms,
        hurtSuppressedSourceRecords = _networkOrdinaryHurtSuppressed,
        hurtActualNeutralRearms = _networkOrdinaryHurtNeutralRearms,
        hurtAwaitingActualNeutral = _networkOrdinaryHurtAwaitNeutral,
        pendingEnemySend = _networkOrdinarySend is not null, pendingDamage = _networkOrdinaryDamage is not null,
        rolePolicy = _ordinaryRole?.Summary, completedActions = _networkOrdinaryCompleted.ToArray(),
        optionalPolicies = NetworkOrdinaryOptionalPolicySummary,
        ownCommandWritesByThisObserver = 0, enemyHealthWrites = 0, syntheticHitMessages = 0,
        command21Written = false, chargedShieldSnowQualified = false, wholeE4Qualified = false
    };

    private void ConfigureNetworkOrdinary(ProbeConfiguration config)
    {
        ConfigureNetworkOrdinaryOptionalPolicies(config);
        Require(config.TemplateNetworkInputFixture && !config.TemplateOrdinaryAttackFixture &&
            config.TemplateSharedPlayerProfile == "battle-01-network-input" && config.Mode == "template-bloom-body-hair-idle" &&
            config.TemplateProjectileManagerFixture && config.TemplatePlayerTraitsFixture && config.TemplatePlayerTraitsPickupFx &&
            config.TemplateTargetRegionFixture && config.TemplateConfigureTargetRegion && config.TemplateSharedHealthAlias &&
            config.TemplateBattleCombatFixture && config.PrepareFirstMagicStage && !config.TemplateBodyInputFixture &&
            !config.TemplateSecondAttackDegreeFixture && !config.TemplateDeathBattleFixture && !config.TemplateStoryDeathFixture &&
            // Inbound Stinger eligibility may compose with this exact Battle47
            // cohort. It retains native metadata WORD E and its own activation
            // witnesses; ordinary aim exclusion does not change that metadata.
            !config.TemplateCollisionDetectorSecondaryRole &&
            !config.TemplateStarSecondaryEligibility && !config.TemplateStarPlacementFixture && !config.TemplateSharedMagicEnergyAlias,
            "Network ordinary actions require the separate Battle47 first-degree/shared-HP/PM Host cohort.");
    }

    private void BindNetworkOrdinaryRoleBeforeInit(uint tid, ref Win32Native.X86Context context)
    {
        Require(!_ordinaryRoleBound && _dependentInterval && _stage == 6 && context.Eip == 0x5096E0 &&
            context.Ecx == _projectileManager && _instance != 0 && DescriptorCount == 8,
            "Network ordinary role must bind before original own PM Init.");
        _ordinaryRegion = Components(_instance, DescriptorCount).Single(c => c.ClassId == 0x4307193A).Address;
        _ordinaryRoleBound = true; _ordinaryRole = new(_host, _image, _log);
        _ordinaryRole.BindStoppedBeforeInit(tid, ref context, _instance, _character, _projectileManager, 0, _ordinaryRegion,
            _record, _localCharacter, _projectilePrimaryManager, Pointer(_projectilePrimaryMove + 0x174),
            () => _ordinaryRoleBound && !_ordinaryRetired && _stage == 6 && !_sharedBattleUnloadStarted && _instance != 0,
            () => _dependentInterval && _stage == 6 || _ordinaryPreUnloadVerified,
            () => _ordinaryAuthorization && _ordinaryFailure.Length == 0,
            maximumOwnLaunches: NetworkOrdinaryMaximumActions,
            ownEnemyNodeProvider: _networkOrdinaryAimFixture ? ResolveNetworkOrdinaryEnemyNode : null,
            primaryLaunchForbidden: _networkPrimaryCaptureOnlyFixture);
    }

    private void BeginNetworkOrdinaryAtWorld(uint tid, ref Win32Native.X86Context context)
    {
        OrdinaryWorld(tid, context);
        Require(!_ordinaryStarted && _ordinaryAimReady && _ordinaryRole is not null && _ordinaryRole.NativeRolePoolsReady &&
            _sharedBattleCaptured && _healthBattleHolding && _healthBattleFastRouteStarted && _idleWorldStops == 2 &&
            ReadSharedBattleFlow(out _, out _, out _, out _, out _),
            "Network ordinary observer starts after two initialized native worlds and aim exclusion.");
        Require(_sharedHealthAlias?.Active == true,
            "Shared HP must finish its original code pins before installing enemy-damage witnesses.");
        InstallNetworkOrdinaryWitnesses();
        VerifyNetworkOrdinaryOwners();
        _ordinaryState = Pointer(_machine + 0x15C);
        Require(U(_ordinaryState) == 0x6F8340 && U(_ordinaryState + 0x10) == 5 &&
            U(_ordinaryState + 0x14) == _machine && U(_ordinaryState + 0x18) == _animator,
            "Network ordinary state5 is not owned by its private BloomFSM/Animator.");
        byte[] command = Read(_ownCommand, 0x74);
        Require(command[0x20] == 0 && command[0x21] == 0, "Network ordinary begins with neutral ordinary/charged fields.");
        _networkOrdinaryCommand21 = command[0x21];
        CaptureNetworkPrimaryOrdinaryBaseline(); _ordinaryStarted = true;
        _log(new { @event = "template-network-ordinary-begin", command = Hex(_ownCommand), state5 = Hex(_ordinaryState),
            maximumActions = NetworkOrdinaryMaximumActions, source = "actual-applied-IPC5-record", observerCommandWrites = 0 });
    }

    // Root invokes this only AFTER controller Apply and assignment of the exact
    // just-applied source ledger record, before resuming the original World.
    private bool PrepareNetworkOrdinaryInputAtWorld(uint tid, ref Win32Native.X86Context context)
    {
        if (!_ordinaryNetwork || !_ordinaryStarted || _networkOrdinaryRetired) return true;
        OrdinaryWorld(tid, context);
        Require(_networkInputLastStep == NativeBodyInputStep.Applied && _networkInputInFlight is not null &&
            !_sharedBattleUnloadStarted, "Network ordinary input has no exact newly applied native source ledger.");
        NetworkInputMap map = _networkInputInFlight!;
        byte[] bytes = Read(_ownCommand + 0x20, 2);
        Require(bytes[0] <= 1 && bytes[1] == _networkOrdinaryCommand21 &&
            (bytes[0] != 0) == map.Record.OrdinaryAttackHeld,
            "Network ordinary source level and actual command20 differ or charged field changed.");
        bool held = bytes[0] != 0;
        if (_networkOrdinaryHurtAwaitNeutral)
        {
            VerifyNetworkOrdinaryOwners();
            Require(_networkOrdinaryHurtFenceWitness is not null && !_networkOrdinaryPaused &&
                (_networkOrdinaryAction is null || _networkOrdinaryAction.Completed) && !_ordinaryAuthorization,
                "Hurt neutral fence overlapped an unfinished ordinary action or lost its original typed witness.");
            if (held)
            {
                SuppressNetworkOrdinaryHurtAtWorld(tid, ref context, map, initialHurt: null);
                return false;
            }
            _networkOrdinaryHurtAwaitNeutral = false; _networkOrdinaryLastHeld = false;
            _networkOrdinaryHurtFenceWitness = null; ++_networkOrdinaryHurtNeutralRearms;
            _log(new { @event = "template-network-ordinary-hurt-rearmed", epoch = map.Record.InputEpoch,
                sourceSequence = map.Record.SourceSequence, captureOrdinal = map.Record.CaptureOrdinal,
                destinationSequence = map.DestinationSequence, sourceOrdinaryHeld = false,
                actualSourceNeutralAtOwnedWorld = true, command20 = bytes[0], ordinaryActionCreated = false,
                nativeAppliedAcknowledgmentNotYetSent = true });
        }
        if (_networkOrdinaryAwaitNeutral)
        {
            Require(!held, "Network ordinary epoch must rearm through a fresh neutral ordinary level.");
            _networkOrdinaryAwaitNeutral = false; ++_networkOrdinaryRearms;
        }
        if (held && !_networkOrdinaryLastHeld)
        {
            VerifyNetworkOrdinaryOwners();
            if (!_networkOrdinaryPaused && (_networkOrdinaryAction is null || _networkOrdinaryAction.Completed) &&
                !_ordinaryAuthorization && _networkOrdinaryActions < NetworkOrdinaryMaximumActions &&
                U(_machine + 0x130) == 10 && Read(_ownCommand + 0x1F, 1)[0] == 0 && Read(_machine + 0x294, 1)[0] == 0)
            {
                _networkOrdinaryHurtObserver ??= new(_host, _image);
                NativeCharacterHurtWitness? hurt = _networkOrdinaryHurtObserver.ReadAtWorld(tid, context, _machine, _animator, 10);
                if (hurt is not null)
                {
                    SuppressNetworkOrdinaryHurtAtWorld(tid, ref context, map, hurt);
                    return false;
                }
            }
            Require(!_networkOrdinaryPaused && (_networkOrdinaryAction is null || _networkOrdinaryAction.Completed) &&
                _networkOrdinaryActions < NetworkOrdinaryMaximumActions && U(_machine + 0x130) == 0 &&
                U(_machine + 0x298) == 0 && (U(_machine + 0x140) & 0x1F800F) == 0 &&
                Read(_ownCommand + 0x1F, 1)[0] == 0 && Read(_machine + 0x294, 1)[0] == 0,
                "Network ordinary press overlaps an unfinished/unsupported action or exceeds its finite budget.");
            _networkOrdinaryAction = new(++_networkOrdinaryActions, map.Record, map.DestinationSequence, _networkOrdinaryWorlds);
            _ordinaryAuthorization = true;
            LogNetworkOrdinary("press-observed", _networkOrdinaryAction, new { command20 = 1, originalNativeWorldNotYetResumed = true });
        }
        else if (!held && _networkOrdinaryLastHeld)
        {
            NetworkOrdinaryAction action = RequireNetworkOrdinaryAction();
            Require(!action.Released && action.TimerStores == 1, "Network ordinary release lacks one actual native hold timer store.");
            uint elapsed = unchecked(U(_sharedBattleTimer + 0x1C) - action.Clock);
            Require(elapsed <= 300, "Bounded network ordinary press exceeded300 native milliseconds.");
            action.Released = true; action.HeldNativeMilliseconds = elapsed;
            LogNetworkOrdinary("release-observed", action, new { nativeHeldMilliseconds = elapsed, releaseSourceSequence = map.Record.SourceSequence });
        }
        _networkOrdinaryLastHeld = held;
        return true;
    }

    private void SuppressNetworkOrdinaryHurtAtWorld(uint tid, ref Win32Native.X86Context context,
        NetworkInputMap map, NativeCharacterHurtWitness? initialHurt)
    {
        Require(map.Record.OrdinaryAttackHeld && ReferenceEquals(map, _networkInputInFlight) &&
            !_ordinaryAuthorization && (_networkOrdinaryAction is null || _networkOrdinaryAction.Completed),
            "Hurt suppression lost its exact source ledger or overlapped an authorized ordinary action.");
        uint nativeState = U(_machine + 0x130);
        bool initial = initialHurt is not null;
        Require(initial || _networkOrdinaryHurtAwaitNeutral && _networkOrdinaryHurtFenceWitness is not null,
            "Held-source Hurt suppression lacks its typed initial fence.");
        if (initial) _networkOrdinaryHurtFenceWitness = initialHurt;
        // Existing controller remains the sole command writer. It discards the
        // entire queued/current source level and neutralizes before native World
        // resumes; the source ACK explicitly reports suppression, never apply.
        NetworkInputPrivateFence(tid, ref context, acknowledgeSuppression: true);
        Require(_networkInputInFlight is null && _networkInputAccepted.Count == 0 &&
            _networkInputLastStep == NativeBodyInputStep.Paused && Read(_ownCommand + 0x20, 1)[0] == 0,
            "Typed Hurt fence did not neutralize and retire its exact source ledger.");
        _networkOrdinaryLastHeld = false; _networkOrdinaryHurtAwaitNeutral = true;
        ++_networkOrdinaryHurtSuppressed;
        _log(new { @event = "template-network-ordinary-hurt-suppressed", epoch = map.Record.InputEpoch,
            sourceSequence = map.Record.SourceSequence, captureOrdinal = map.Record.CaptureOrdinal,
            destinationSequence = map.DestinationSequence, typedHurt = initial,
            awaitingActualNeutral = true, nativeState,
            table = initialHurt is null ? null : Hex(initialHurt.Table),
            getter = initialHurt is null ? null : Hex(initialHurt.Getter),
            classId = initialHurt is null ? null : Hex(initialHurt.ClassId),
            initialTypedHurtWitness = _networkOrdinaryHurtFenceWitness,
            sourceOrdinaryHeld = true, ordinaryActionCreated = false,
            reason = initial ? "typed-native-hurt-before-ordinary-admission" : "held-source-awaits-actual-neutral-after-typed-hurt",
            sourceAcknowledgmentReason = "safe-world-neutral-suppressed",
            nativeAppliedEventEmitted = false, originalNativeWorldNotYetResumed = true,
            commandNeutralizedByExistingControllerOnly = true, nativeFsmWrites = 0 });
    }

    private void StepNetworkOrdinaryAtWorld(uint tid, ref Win32Native.X86Context context, bool ready)
    {
        if (!_ordinaryStarted || _networkOrdinaryRetired) return;
        OrdinaryWorld(tid, context); ++_networkOrdinaryWorlds;
        if (!ready)
        {
            if (!_networkOrdinaryPaused)
            {
                _networkOrdinaryPaused = true; _networkOrdinaryAwaitNeutral = true;
                _networkOrdinaryLastHeld = false; ++_networkOrdinaryPauseFences;
                // A committed native animation is allowed to finish once;
                // input edges are fenced rather than replayed after resume.
                if (_networkOrdinaryAction is { Completed: false, Committed: false })
                    NetworkOrdinaryFail("Pause interrupted an uncommitted ordinary press; no replay or synthetic FSM cancellation is admitted.");
                _log(new { @event = "template-network-ordinary-pause-fence", committedNativeActionMayFinishOnce =
                    _networkOrdinaryAction is { Committed: true, Completed: false }, inputReplayAllowed = false });
            }
            return;
        }
        _networkOrdinaryPaused = false;
        if (_networkPrimaryCaptureOnlyFixture && _networkOrdinaryWorlds % 120 == 0)
            VerifyNetworkPrimaryOrdinaryPools(requireInactive: true);
        if (_networkOrdinaryWorlds % 120 == 0) { VerifyNetworkOrdinaryOwners(); _ordinaryRole!.VerifyAtWorldStopped(tid, ref context); }
        NetworkOrdinaryAction? current = _networkOrdinaryAction;
        if (current is { Completed: false })
        {
            Require(_networkOrdinaryWorlds - current.BeginWorld <= 1800, "Network ordinary action exceeded1800 observed native worlds.");
            if (current.ActivationReturns == 1)
            {
                current.LastPosition = OrdinaryPosition(current.Node); ++current.TrajectorySamples;
                current.TrajectoryMoved |= OrdinaryDistance(current.Origin!, current.LastPosition) > 0.01;
            }
            if (current.Released && current.Committed && current.AnimationMapped && current.EventReturned &&
                current.Events == 1 && current.Wrappers == 1 && current.ActivationReturns == 1 &&
                !current.ChooserPending && !current.LookupPending && !current.EventPending && !current.ActivationPending &&
                U(_machine + 0x130) == 0 && U(_machine + 0x298) == 0)
            {
                current.Completed = true; _ordinaryAuthorization = false;
                _networkOrdinaryCompleted.Add(new { current.Id, current.Epoch, current.SourceSequence, current.DestinationSequence,
                    current.CaptureOrdinal, current.HeldNativeMilliseconds, projectile = Hex(current.Projectile),
                    current.TrajectorySamples, current.TrajectoryMoved });
                LogNetworkOrdinary("native-action-completed", current, new { originalStateReturnedTo0 = true, nativeActivation = true });
            }
        }
        _networkOrdinaryGoal |= _networkOrdinaryNativeActivations >= 1 && _networkOrdinaryEnemyCommits >= 1;
    }

    private void RetireNetworkOrdinaryBeforeUnload(uint tid, ref Win32Native.X86Context context, uint entry,
        Action<uint, Win32Native.X86Context> verifyPreUnloadBoundary)
    {
        if (_networkOrdinaryRetired) return;
        Require(_ordinaryRoleBound && _ordinaryRole is not null && _host.IsAtDebugStop && tid == _host.MainThreadId &&
            context.Eip == entry && _networkOrdinarySend is null && _networkOrdinaryDamage is null &&
            (_networkOrdinaryAction is null || !_networkOrdinaryAction.ChooserPending && !_networkOrdinaryAction.LookupPending &&
                !_networkOrdinaryAction.EventPending && !_networkOrdinaryAction.ActivationPending),
            "Network ordinary retirement has an unmatched native action/damage frame.");
        verifyPreUnloadBoundary(tid, context); _ordinaryPreUnloadVerified = true;
        Require(Read(_ownCommand + 0x20, 1)[0] == 0,
            "The network input controller must retire/neutralize command20 before its ordinary observer.");
        VerifyNetworkPrimaryOrdinaryAtRetirement();
        _ordinaryAuthorization = false; _ordinaryRole!.RetireBeforeOwnDelete(tid, ref context);
        _networkOrdinaryRetired = true; _ordinaryRetired = true; _ordinaryPreUnloadVerified = false;
        _log(new { @event = "template-network-ordinary-retired", entry = Hex(entry),
            goal = NetworkOrdinaryFixtureGoalPassed, oldEnemyAndOwnPointersReadAfterRetirement = false, commandWrites = 0 });
    }

    private void InstallNetworkOrdinaryWitnesses()
    {
        InstallNetworkOrdinaryOptionalPolicyPins();
        OrdinaryPin(0x4FAD88, "FF524C"); OrdinaryPin(0x4FB14E, "FF504C"); OrdinaryPin(0x4FB7F2, "FF523C");
        OrdinaryHook(0x52579B, "898E98020000", "network-held-timer", (uint tid, uint at, ref Win32Native.X86Context c) =>
        {
            if (!_ordinaryStarted || c.Esi != _machine) return;
            NetworkOrdinaryAction action = RequireNetworkOrdinaryAction();
            Require(_ordinaryAuthorization && !action.Released && Read(_ownCommand + 0x20, 1)[0] == 1 &&
                c.Ecx != 0 && c.Ecx == U(_sharedBattleTimer + 0x1C) && ++action.TimerStores == 1,
                "Network ordinary held timer is not its one actual command20 producer.");
            action.Clock = c.Ecx; LogNetworkOrdinary("native-held-timer", action, new { clock = c.Ecx });
        });
        OrdinaryHook(0x525B00, "53568B74240C578BFE", "network-chooser", (uint tid, uint at, ref Win32Native.X86Context c) =>
        {
            if (!_ordinaryStarted || c.Ecx != _machine || ((U(c.Esp + 4) >> 7) & 255) != 10) return;
            NetworkOrdinaryAction action = RequireNetworkOrdinaryAction();
            if (action.Chosen) return;
            Require(action.Released && _ordinaryAuthorization && !action.ChooserPending && U(c.Esp) is 0x4FB151 or 0x4FAD8B,
                "Network ordinary chooser lacks its released source edge and original caller.");
            action.ChooserPending = true; OrdinaryNativeFrame frame = OrdinaryFrame(c);
            OrdinaryReturn(tid, frame, 8, "network-chooser", (ref Win32Native.X86Context returned) =>
            {
                Require(returned.Eax == 5 && ReferenceEquals(_networkOrdinaryAction, action), "Network ordinary chooser did not return state5.");
                if (frame.Caller == 0x4FB151) action.Proposed = true;
                else { Require(action.Proposed, "Network native transition has no earlier proposal."); action.Chosen = true; }
                action.ChooserPending = false;
            });
        });
        OrdinaryHook(0x4FAD91, "898630010000", "network-state-store", (uint tid, uint at, ref Win32Native.X86Context c) =>
        {
            if (!_ordinaryStarted || c.Esi != _machine || _networkOrdinaryAction is not { Chosen: true, Committed: false } action) return;
            Require(_ordinaryAuthorization && !action.ChooserPending && !action.StoreSeen && c.Eax == 5, "Network ordinary state store lost its query.");
            action.StoreSeen = true;
        });
        OrdinaryHook(0x4FAD97, "8B8644010000", "network-state-stored", (uint tid, uint at, ref Win32Native.X86Context c) =>
        {
            if (!_ordinaryStarted || c.Esi != _machine || _networkOrdinaryAction is not { StoreSeen: true, Committed: false } action) return;
            Require(_ordinaryAuthorization && U(_machine + 0x130) == 5, "Native network ordinary state5 store did not commit.");
            action.Committed = true; LogNetworkOrdinary("native-state5-committed", action, new { adapterStateWrites = 0 });
        });
        OrdinaryHook(0x5141D9, "E802640800", "network-main-key", NetworkOrdinaryMainKey);
        OrdinaryHook(0x513FA0, "8B4424048B401C8B40105356578BD9", "network-orb-event", NetworkOrdinaryOrbEvent);
        OrdinaryHook(0x50A210, "8B81340300008B5424045052", "network-launch-wrapper", (uint tid, uint at, ref Win32Native.X86Context c) =>
        {
            if (TrySuppressNetworkPrimaryOrdinaryWrapper(tid, ref c)) return;
            if (c.Ecx != _projectileManager) return;
            NetworkOrdinaryAction action = RequireNetworkOrdinaryAction();
            Require(action.EventPending && _ordinaryAuthorization && U(c.Esp) == 0x4FB7F5 && U(c.Esp + 4) == 0 &&
                ++action.Wrappers == 1, "Network ordinary wrapper lacks its own event_orb/type0 frame.");
        });
        OrdinaryHook(0x4FF800, "83EC4C535556578BF1", "network-pool-activation", NetworkOrdinaryActivation);
        OrdinaryHook(0x4FFC07, "C21800", "network-pool-activation-RET18", (uint tid, uint at, ref Win32Native.X86Context c) =>
        {
            if (_networkOrdinaryAction is not { ActivationPending: true } action) return;
            OrdinaryNativeFrame frame = action.ActivationFrame!;
            Require(!action.ActivationRetSeen && c.Esp == frame.Esp && U(c.Esp) == frame.Caller && OrdinaryPreserved(frame, c),
                "Network activation lost its exact RET18 frame."); action.ActivationRetSeen = true;
        });
        OrdinaryHook(0x4FF750, "E8DB65F3FF", "network-enemy-send", NetworkOrdinaryEnemySend);
        OrdinaryPin(0x4FF755, "837E5001");
        OrdinaryHook(0x4FCF00, "568BF18B8624010000", "network-enemy-damage", NetworkOrdinaryEnemyDamage);
        OrdinaryHook(0x4FD04B, "C20400", "network-enemy-damage-RET4", (uint tid, uint at, ref Win32Native.X86Context c) =>
        {
            if (_networkOrdinaryDamage is not { } damage) return;
            Require(!damage.NativeRetSeen && c.Esp == damage.Frame.Esp && U(c.Esp) == damage.Frame.Caller &&
                OrdinaryPreserved(damage.Frame, c), "Enemy damage did not reach its exact native RET4."); damage.NativeRetSeen = true;
        });
        OrdinaryHook(0x4FCF3A, "E811FEFFFF", "network-enemy-lethal-setter-call", NetworkOrdinaryEnemySetterCall);
        OrdinaryHook(0x4FCFB0, "E89BFDFFFF", "network-enemy-setter-call", NetworkOrdinaryEnemySetterCall);
        OrdinaryHook(0x4FCF3F, "51", "network-enemy-lethal-setter-return", NetworkOrdinaryEnemySetterReturned);
        OrdinaryHook(0x4FCFB5, "8A86A0010000", "network-enemy-setter-return", NetworkOrdinaryEnemySetterReturned);
    }

    private void NetworkOrdinaryMainKey(uint tid, uint at, ref Win32Native.X86Context c)
    {
        if (!_ordinaryStarted || c.Edi != _ordinaryState) return;
        NetworkOrdinaryAction action = RequireNetworkOrdinaryAction();
        if (action.AnimationMapped) return;
        Require(action.Committed && _ordinaryAuthorization && !action.LookupPending && U(c.Esp) == 0x500 && U(c.Esp + 4) == 0 &&
            c.Ecx == U(0x765ADC), "Network ordinary main-key lookup escaped own state5/group0/key500.");
        action.LookupPending = true; OrdinaryNativeFrame frame = OrdinaryFrame(c);
        OrdinaryReturn(tid, frame with { Caller = 0x5141DE }, 8, "network-main-key", (ref Win32Native.X86Context returned) =>
        {
            uint manager = Pointer(0x765ADC);
            Require(returned.Eax >= 0x10000 && !TryOrdinaryAnimationEntry(manager + 0x20, 0x500, out _) &&
                TryOrdinaryAnimationEntry(manager + 0x20 + 66 * 12, 0x500, out uint resource) && resource == returned.Eax,
                "Network ordinary selected animation is not the exact native Battle66 fallback.");
            action.Animation = returned.Eax; action.AnimationMapped = true; action.LookupPending = false;
        });
    }

    private void NetworkOrdinaryOrbEvent(uint tid, uint at, ref Win32Native.X86Context c)
    {
        if (!_ordinaryStarted || c.Ecx != _ordinaryState) return;
        uint message = Pointer(c.Esp + 4), tag = Pointer(Pointer(message + 0x1C) + 0x10);
        if (!Read(tag, 10).SequenceEqual("event_orb\0"u8.ToArray())) return;
        NetworkOrdinaryAction action = RequireNetworkOrdinaryAction();
        Require(action.Released && action.Committed && action.AnimationMapped && _ordinaryAuthorization &&
            U(_machine + 0x130) == 5 && U(_ordinaryState + 0x24) == action.Animation && U(c.Esp) == 0x4FB7F5 &&
            !action.EventPending && ++action.Events == 1, "Network event_orb escaped its native source action/state/clip.");
        action.EventPending = true; OrdinaryNativeFrame frame = OrdinaryFrame(c);
        OrdinaryReturn(tid, frame, 8, "network-orb-event", (ref Win32Native.X86Context returned) =>
        {
            Require(action.Wrappers == 1 && action.ActivationReturns == 1 && Read(_ordinaryState + 0x3C, 1)[0] == 1,
                "Network event_orb returned without one native activation/event-seen store.");
            action.EventReturned = true; action.EventPending = false;
            LogNetworkOrdinary("native-event-orb-returned", action, new { projectile = Hex(action.Projectile) });
        });
    }

    private void NetworkOrdinaryActivation(uint tid, uint at, ref Win32Native.X86Context c)
    {
        RejectNetworkPrimaryOrdinaryActivation(tid, c);
        if (U(c.Ecx + 0x18) != _projectileManager) return;
        uint projectile = c.Ecx;
        NetworkOrdinaryAction action = RequireNetworkOrdinaryAction();
        Require(action.EventPending && _ordinaryAuthorization && action.Wrappers == 1 && !action.ActivationPending &&
            U(c.Ecx) == 0x6F6B04 && U(c.Ecx + 0x50) == 1 && Read(c.Ecx + 0x30, 1)[0] == 0 && U(c.Esp) == 0x5085A0 &&
            Enumerable.Range(0, 6).Count(i => U(_projectileManager + 0x1D0 + (uint)i * 4) == projectile) == 1 &&
            ++action.Activations == 1, "Network ordinary activation lost its typed role1/type0/inactive/caller proof.");
        action.Projectile = c.Ecx; action.Node = Pointer(c.Ecx + 0x14);
        action.ActivationPending = true; OrdinaryNativeFrame frame = OrdinaryFrame(c); action.ActivationFrame = frame;
        OrdinaryReturn(tid, frame, 28, "network-pool-activation", (ref Win32Native.X86Context returned) =>
        {
            Require(action.ActivationRetSeen && (returned.Eax & 255) == 1 && Read(action.Projectile + 0x30, 1)[0] == 1 &&
                U(action.Projectile + 0x18) == _projectileManager && ++action.ActivationReturns == 1,
                "Network ordinary activation did not complete its real RET18/active return.");
            action.Origin = OrdinaryPosition(action.Node); action.ActivationPending = false;
            _networkOrdinaryProjectiles[action.Projectile] = action; ++_networkOrdinaryNativeActivations;
            LogNetworkOrdinary("native-projectile-activated", action, new { projectile = Hex(action.Projectile), node = Hex(action.Node) });
        });
    }

    private void NetworkOrdinaryEnemySend(uint tid, uint at, ref Win32Native.X86Context c)
    {
        if (!_ordinaryStarted || _ordinaryRole is null || !_ordinaryRole.TryOwnEnemySend(tid, c,
            out uint projectile, out uint target, out uint metadata)) return;
        bool activated = _networkOrdinaryProjectiles.TryGetValue(projectile, out NetworkOrdinaryAction? action);
        Require(_networkOrdinarySend is null && _networkOrdinaryDamage is null && activated && action is not null,
            "Own enemy hit lacks a completed typed native projectile activation.");
        (uint traits, uint character) = CaptureNetworkEnemyTraits(target);
        NetworkOrdinarySend send = new(action!, projectile, target, metadata, traits, character, OrdinaryFrame(c), U(c.Esp + 8));
        Require(send.Damage > 0 && send.Damage <= 100000, "Own enemy hit has unsupported native damage payload.");
        _networkOrdinarySend = send;
        _host.Breakpoints.AddReturn(0x4FF755, tid, "NetworkOrdinary.enemy-send.return", (uint rt, uint ra, ref Win32Native.X86Context returned) =>
        {
            Require(_ordinaryNetwork && !_ordinaryRetired && ReferenceEquals(_networkOrdinarySend, send) &&
                _networkOrdinaryDamage is null && rt == tid && ra == 0x4FF755 && returned.Eip == ra &&
                returned.Esp == checked(send.Frame.Esp + 16) && OrdinaryPreserved(send.Frame, returned),
                "Own enemy notification lost its original435D30/RET10 frame.");
            Require(send.DamageEntries == send.DamageReturns && send.Commits <= 1,
                "Own enemy notification has an unmatched or repeated native health operation.");
            if (send.Commits == 1)
            {
                ++send.Action.EnemyCommits; ++_networkOrdinaryEnemyCommits;
                LogNetworkOrdinary("native-enemy-hit-completed", send.Action, new { projectile = Hex(projectile), target = Hex(target),
                    traits = Hex(traits), actualNativeHealthCommits = 1, receiverAndDamageUnmodified = true });
            }
            _networkOrdinarySend = null;
        });
    }

    private (uint Traits, uint Character) CaptureNetworkEnemyTraits(uint target)
    {
        Require(target != _instance && target != _record && U(target) == 0x710F80 &&
            U(Pointer(target + 0x24) + 0x3C) == _scene, "Enemy target is a teammate or outside the owned scene.");
        uint head = Pointer(target + 0x1C), link = Pointer(head), previous = head, traits = 0, character = 0;
        HashSet<uint> seen = [];
        while (link != head)
        {
            Require(seen.Count < 64 && seen.Add(link) && U(link + 4) == previous, "Enemy components are not bounded/reciprocal.");
            uint item = Pointer(link + 8), id = ClassId(item);
            if (id == 0x1BC6108E) { Require(traits == 0 && U(item) == 0x6F67F0 && U(item + 0x24) == target, "Enemy base Traits is not unique/typed/owned."); traits = item; }
            if (id == 0x0003CC73) { Require(character == 0 && U(item + 0x24) == target, "Enemy Character is not unique/owned."); character = item; }
            previous = link; link = Pointer(link);
        }
        Require(U(head + 4) == previous && traits != 0 && character != 0 && U(traits + 0x18C) == character &&
            U(character + 0x138) == traits && U(U(traits) + 0x40) == 0x4FCF00,
            "Enemy Traits/Character/native damage receiver graph is not the bounded base type.");
        return (traits, character);
    }

    private void NetworkOrdinaryEnemyDamage(uint tid, uint at, ref Win32Native.X86Context c)
    {
        NetworkOrdinarySend? send = _networkOrdinarySend;
        if (send is null || c.Ecx != send.Traits) return;
        Require(_networkOrdinaryDamage is null && ++send.DamageEntries == 1 &&
            U(c.Ecx) == 0x6F67F0 && U(c.Ecx + 0x24) == send.Target && U(c.Ecx + 0x18C) == send.Character &&
            U(Pointer(c.Esp + 4) + 0x18) == send.Damage,
            "Own projectile enemy damage escaped its causal notification/typed receiver/payload.");
        NetworkOrdinaryDamage damage = new(send, OrdinaryFrame(c), U(c.Ecx + 0x124), U(c.Ecx + 0x128), U(c.Ecx + 0x12C));
        Require(damage.Maximum > 0 && damage.Maximum <= 100000 && damage.Before <= damage.Maximum && damage.Minimum <= damage.Before,
            "Enemy health view is uninitialized or outside the finite observed range.");
        _networkOrdinaryDamage = damage;
        OrdinaryReturn(tid, damage.Frame, 8, "network-enemy-damage", (ref Win32Native.X86Context returned) =>
        {
            Require(ReferenceEquals(_networkOrdinaryDamage, damage) && damage.NativeRetSeen &&
                (!damage.SetterSeen || damage.SetterReturned), "Enemy damage lacks its native RET4 or setter return.");
            ++send.DamageReturns;
            if (damage.SetterReturned && damage.After < damage.Before)
            {
                ++send.Commits;
                LogNetworkOrdinary("native-enemy-health-declined", send.Action, new { projectile = Hex(send.Projectile),
                    target = Hex(send.Target), traits = Hex(send.Traits), before = damage.Before, after = damage.After,
                    previous = damage.Previous, requested = damage.Requested, nativeSetterCall = Hex(damage.SetterCall),
                    nativeSetterReturn = Hex(damage.SetterReturn), enemyHealthWritesByAdapter = 0, historicalTargetRead = false });
            }
            _networkOrdinaryDamage = null;
        });
    }

    private void NetworkOrdinaryEnemySetterCall(uint tid, uint at, ref Win32Native.X86Context c)
    {
        if (_networkOrdinaryDamage is not { } damage || c.Ecx != damage.Send.Traits) return;
        Require(!damage.SetterSeen && c.Esi == damage.Send.Traits && c.Esp == checked(damage.Frame.Esp - 12),
            "Enemy native health setter call lost its unique actual damage frame.");
        damage.Requested = U(c.Esp); damage.SetterCall = at; damage.SetterReturn = at + 5; damage.SetterSeen = true;
        Require(at == 0x4FCF3A ? damage.Requested == 0 : damage.Requested > 0,
            "Enemy original setter call selected an inconsistent lethal/nonlethal branch.");
    }

    private void NetworkOrdinaryEnemySetterReturned(uint tid, uint at, ref Win32Native.X86Context c)
    {
        if (_networkOrdinaryDamage is not { } damage || c.Esi != damage.Send.Traits) return;
        Require(damage.SetterSeen && !damage.SetterReturned && at == damage.SetterReturn &&
            c.Esp == checked(damage.Frame.Esp - 8), "Enemy original protected health setter lost its exact native return.");
        uint traits = damage.Send.Traits;
        damage.After = U(traits + 0x124); damage.Previous = U(traits + 0x130);
        Require(damage.After <= damage.Before && U(traits + 0x128) == damage.Maximum && U(traits + 0x12C) == damage.Minimum,
            "Enemy native setter changed an unsupported health bound or increased HP.");
        damage.SetterReturned = true;
    }

    private NetworkOrdinaryAction RequireNetworkOrdinaryAction()
    {
        Require(_ordinaryNetwork && _ordinaryStarted && !_ordinaryRetired && _networkOrdinaryAction is not null &&
            !_networkOrdinaryAction.Completed && _ordinaryFailure.Length == 0, "Network ordinary callback has no current admitted action.");
        return _networkOrdinaryAction!;
    }

    private void VerifyNetworkOrdinaryOwners()
    {
        Require(_ordinaryNetwork && _host.IsAtDebugStop && _stage == 6 && !_dependentInterval && _idleActive &&
            _sharedBattleCaptured && !_sharedBattleUnloadStarted && _healthBattleHolding && _healthBattleFastRouteStarted &&
            _level == 47 && DescriptorCount == 8 && _sharedHealthAlias?.Active == true && _ownCommand != _command &&
            U(_character + 0x130) == _ownCommand && U(_machine + 0x12C) == _ownCommand && U(_machine + 0x124) == _character &&
            U(_machine + 0x128) == 0 && U(_character + 0x148) == 1 && U(_character + 0x14C) == 41 &&
            U(_character + 0x140) == _projectileManager && U(_animator + 0x124) == _character && U(_animator + 0x128) == _ownActor &&
            U(_profile + 0x2B4) == _record && Read(_profile + 0x510, 1)[0] == 1 && Read(_profile + 0x512, 1)[0] == 0 &&
            U(_profile + 0x6E4) == 1 && (U(_profile + 0x42C) & 0x200000) == 0,
            "Network ordinary action escaped the separate first-degree Battle47 owner/profile tuple.");
    }

    private void LogNetworkOrdinary(string name, NetworkOrdinaryAction action, object data) =>
        _log(new { @event = "template-network-ordinary", name, actionId = action.Id, epoch = action.Epoch,
            sourceSequence = action.SourceSequence, captureOrdinal = action.CaptureOrdinal,
            destinationSequence = action.DestinationSequence, data });

    private void NetworkOrdinaryFail(string reason)
    {
        if (_ordinaryFailure.Length != 0) return;
        _ordinaryFailure = reason; _ordinaryAuthorization = false;
        _log(new { @event = "template-network-ordinary-failed", reason, replayAllowed = false });
        throw new InvalidDataException(reason);
    }
}
