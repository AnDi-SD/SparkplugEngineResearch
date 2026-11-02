using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>Bounded Host IPC5 delivery into the original-World controller, with a separate ordinary-action opt-in.</summary>
internal sealed partial class NativeTemplateProbe
{
    private enum NetworkInputBarrier { None, AwaitReady }
    private bool _networkInputFixture, _networkInputStarted, _networkInputRetired, _networkInputEndpointReleased;
    private bool _networkInputWasPaused, _networkInputAwaitNewEpoch, _networkInputRefreshEpoch, _networkInputGoal;
    private bool _networkInputMoved, _networkInputSemanticJumpQualified, _networkInputObservedState18, _networkInputRise, _networkInputFall;
    private NativeCharacterJumpState? _networkInputJumpObserver;
    private uint _networkInputSemanticJumpWitnesses;
    private object? _networkInputSemanticJumpWitness;
    private int _networkInputDrainHoldMilliseconds = 300;
    private bool _networkInputDrainHoldQualifiedEarlyTimeout, _networkInputLastAppliedNeutral;
    private bool _networkInputDrainHoldRequested, _networkInputDrainHoldStarted, _networkInputDrainHoldCompleted;
    private bool _networkInputDrainHoldCancelled, _networkInputDrainHoldCompletionDue;
    private uint _networkInputDrainHoldArmedWorld, _networkInputDrainHoldBeginWorld, _networkInputDrainHoldEndWorld, _networkInputDrainHoldWorlds;
    private long _networkInputDrainHoldBeginMilliseconds, _networkInputDrainHoldEndMilliseconds, _networkInputDrainHoldElapsedMilliseconds;
    private string _networkInputDrainHoldCancelReason = "";
    private bool _networkInputPauseNeutral, _networkInputResumeNeutral, _networkInputTimeoutNeutral, _networkInputDisconnected;
    private bool _networkInputExpectedPeerRetirement;
    private DiagnosticInputIdentity? _networkInputIdentity;
    private DiagnosticInputClient? _networkInputClient;
    private NativeBodyInputController? _networkInputController;
    private object? _networkInputEndpointSummary;
    private readonly Queue<NetworkInputMap> _networkInputAccepted = [];
    private NetworkInputMap? _networkInputInFlight;
    private NetworkInputPose? _networkInputBeforeFlight;
    private NetworkInputBarrier _networkInputBarrier;
    private uint _networkInputEpoch, _networkInputNextSource = 1, _networkInputDestinationSequence, _networkInputAppliedTimer;
    private uint _networkInputWorlds, _networkInputAcceptedCount, _networkInputAppliedCount, _networkInputSuppressed;
    private uint _networkInputOldDiscarded, _networkInputFullRetries, _networkInputEpochChanges, _networkInputTimeouts;
    private uint _networkInputPauseCount, _networkInputResumeCount, _networkInputNativeAckCount, _networkInputRetiredDrops;
    private ulong _networkInputLastCaptureOrdinal, _networkInputLastIngress;
    private float? _networkInputJumpBaseline;
    private byte[]? _networkInputLastCommand;
    private string _networkInputFailure = "";
    private NativeBodyInputStep _networkInputLastStep;
    private uint _networkInputTransportAttempt = 1, _networkInputRebindAppliedBaseline;
    private bool _networkInputRebindAwaitWorld, _networkInputRebindMoved;
    private object? _networkInputPreviousTransport;
    private sealed record NetworkInputMap(DiagnosticInputRecord Record, uint DestinationSequence);
    private sealed record NetworkInputPose(float[] Position, uint State, float Vertical);

    internal bool NetworkInputFixtureRetired => !_networkInputFixture || _networkInputRetired;
    internal bool NetworkInputFixtureGoalPassed => _networkInputFixture && _networkInputGoal && _networkInputFailure.Length == 0 &&
        (!_networkInputDrainHoldRequested || _networkInputDrainHoldCompleted || _networkInputDrainHoldQualifiedEarlyTimeout) &&
        (!_ordinaryNetwork || NetworkOrdinaryFixtureGoalPassed);
    internal bool NetworkInputCaptureReady => _networkInputFixture && _networkInputStarted && !_networkInputRetired &&
        !_sharedBattleUnloadStarted && _networkInputFailure.Length == 0;
    internal bool NetworkInputDetachedForReconnect => _networkInputTransportAttempt == 1 &&
        _networkInputFixture && _networkInputStarted && !_networkInputRetired && _networkInputDisconnected &&
        _networkInputExpectedPeerRetirement && _networkInputFailure.Length == 0 &&
        _networkInputClient?.Terminal == DiagnosticInputTerminal.ActorClosed;
    internal bool ReboundInputNativeApplied => _networkInputTransportAttempt == 2 && !_networkInputRebindAwaitWorld &&
        _networkInputAppliedCount > _networkInputRebindAppliedBaseline && _networkInputRebindMoved && _networkInputFailure.Length == 0;
    private object NetworkInputFixtureSummary => new
    {
        enabled = _networkInputFixture, started = _networkInputStarted, retired = _networkInputRetired,
        endpointReleased = _networkInputEndpointReleased, passed = NetworkInputFixtureGoalPassed, failure = _networkInputFailure,
        transportAttempt = _networkInputTransportAttempt, reboundWaitingForNativeWorld = _networkInputRebindAwaitWorld,
        reboundActualNativeApplication = ReboundInputNativeApplied, previousTransport = _networkInputPreviousTransport,
        reboundNativeMovementObserved = _networkInputRebindMoved,
        source = "Host-IPC5-real-bridge-delivery", epoch = _networkInputEpoch, globalDestinationSequence = _networkInputDestinationSequence,
        accepted = _networkInputAcceptedCount, actualReadyNativeWorldApplied = _networkInputAppliedCount,
        explicitlySuppressed = _networkInputSuppressed, nativeAcknowledgments = _networkInputNativeAckCount,
        oldEpochDiscarded = _networkInputOldDiscarded, fullRetries = _networkInputFullRetries, epochChanges = _networkInputEpochChanges,
        worlds = _networkInputWorlds, pendingAccepted = _networkInputAccepted.Count, nativeIntervalPending = _networkInputInFlight is not null,
        backpressureFixture = new
        {
            requested = _networkInputDrainHoldRequested, started = _networkInputDrainHoldStarted,
            completed = _networkInputDrainHoldCompleted, cancelled = _networkInputDrainHoldCancelled,
            extendedNativeTimeoutDiagnostic = _networkInputDrainHoldMilliseconds > 300,
            qualifiedEarlyNativeTimeout = _networkInputDrainHoldQualifiedEarlyTimeout,
            sourceNoCreditMustBeVerifiedIndependently = _networkInputDrainHoldMilliseconds > 300,
            requestedMilliseconds = _networkInputDrainHoldMilliseconds, heldWorlds = _networkInputDrainHoldWorlds,
            heldMilliseconds = _networkInputDrainHoldElapsedMilliseconds, eligibilityWorld = _networkInputDrainHoldArmedWorld,
            beginWorld = _networkInputDrainHoldBeginWorld, endWorld = _networkInputDrainHoldEndWorld,
            beginMonotonicMilliseconds = _networkInputDrainHoldBeginMilliseconds, endMonotonicMilliseconds = _networkInputDrainHoldEndMilliseconds,
            cancelReason = _networkInputDrainHoldCancelReason, onlyHostDrainSkipped = true,
            nativeControllerAndPreviousAcknowledgmentsContinue = true, sourceNoCreditCoverageClaimed = false
        },
        moved = _networkInputMoved, semanticJumpQualified = _networkInputSemanticJumpQualified,
        semanticJumpNativeWitnesses = _networkInputSemanticJumpWitnesses, semanticJumpWitness = _networkInputSemanticJumpWitness,
        observedJumpState18 = _networkInputObservedState18, observedRise = _networkInputRise, observedFall = _networkInputFall,
        pauseNeutralObserved = _networkInputPauseNeutral, resumeFreshBarrierNeutral = _networkInputResumeNeutral,
        actualControllerTimeoutNeutral = _networkInputTimeoutNeutral, timeouts = _networkInputTimeouts,
        pauseTransitions = _networkInputPauseCount, resumeTransitions = _networkInputResumeCount, disconnected = _networkInputDisconnected,
        peerRetiredAfterQualifiedGoal = _networkInputExpectedPeerRetirement,
        acceptedRecordsSuppressedAtRetirementWithoutWireAck = _networkInputRetiredDrops,
        lastStep = _networkInputLastStep.ToString(), controller = _networkInputController?.Summary,
        endpoint = _networkInputEndpointSummary ?? _networkInputClient?.Summary,
        hostIngressPreserved = true, sourceClockUsedForAge = false, acceptedIsNotApplied = true,
        nativeTicksAdded = 0, nodePoseWrites = 0,
        onlyControllerFields = _ordinaryNetwork ? new[] { 4, 8, 0x14, 0x1A, 0x20 } : new[] { 4, 8, 0x14, 0x1A },
        goalUsesCachedEvidenceOnly = true, timeoutPauseAndRetirementAreSeparateEvidence = true,
        secondGuestGameOrLANPresentationQualified = false, fullE4Complete = false
    };

    private void ConfigureNetworkInputFixture(ProbeConfiguration config)
    {
        _networkInputFixture = config.TemplateNetworkInputFixture;
        if (!_networkInputFixture) return;
        _networkInputDrainHoldRequested = config.TemplateNetworkInputBackpressureFixture;
        _networkInputDrainHoldMilliseconds = config.TemplateNetworkInputDrainHoldMilliseconds;
        Require(_networkInputDrainHoldMilliseconds is >= 100 and <= 650 &&
            (_networkInputDrainHoldMilliseconds == 300 || _networkInputDrainHoldRequested) &&
            (_networkInputDrainHoldMilliseconds <= 300 ||
             config.DiagnosticGuestInput?.Role == DiagnosticInputRole.Guest && config.DiagnosticInputHeartbeatMilliseconds == 50),
            "Extended drain diagnostic requires explicit same-game Guest heartbeat50; default remains300ms.");
        _networkInputIdentity = config.DiagnosticHostInput;
        Require(_networkInputIdentity is { Role: DiagnosticInputRole.Host } &&
            config.TemplateSharedPlayerProfile == (config.TemplateNetworkOrdinaryAttackFixture ? "battle-01-network-input" : "alfea-28-network-input") &&
            config.Mode == "template-bloom-body-hair-idle" &&
            config.TemplateProjectileManagerFixture && config.TemplatePlayerTraitsFixture && config.TemplatePlayerTraitsPickupFx &&
            config.TemplateTargetRegionFixture && config.TemplateConfigureTargetRegion && config.TemplateSharedHealthAlias &&
            config.TemplateHealthBattleFixture == config.TemplateNetworkOrdinaryAttackFixture &&
            config.TemplateSharedBattleLifetime == config.TemplateNetworkOrdinaryAttackFixture && !config.TemplateBodyInputFixture &&
            !config.TemplateOrdinaryAttackFixture && config.TemplateBattleCombatFixture == config.TemplateNetworkOrdinaryAttackFixture &&
            config.PrepareFirstMagicStage == config.TemplateNetworkOrdinaryAttackFixture &&
            !config.TemplateSecondAttackDegreeFixture && !config.TemplateDeathBattleFixture && !config.TemplateStoryDeathFixture &&
            !config.TemplateCollisionDetectorSecondaryRole &&
            (!config.TemplateStingerSecondaryRole || config.TemplateNetworkOrdinaryAttackFixture) && !config.TemplateStarSecondaryEligibility &&
            !config.TemplateStarPlacementFixture && !config.TemplateSharedMagicEnergyAlias,
            "Network input requires its separate Alfea28 Host mapping/retained8-component movement-jump fixture.");
    }

    private void BeginNetworkInputAtWorld(uint tid, ref Win32Native.X86Context context)
    {
        if (!_networkInputFixture) return;
        NetworkInputWorld(tid, context);
        Require(!_networkInputStarted && _networkInputClient is null && _sharedBattleCaptured && _healthBattleHolding &&
            _healthBattleFastRouteStarted && _idleWorldStops == 2 && _ownCharacterTicks == 2 && _ownMoveUpdates == 2 &&
            _ownBloomUpdates == 2 && _level == (_ordinaryNetwork ? 47u : 28u) && DescriptorCount == 8 &&
            ReadSharedBattleFlow(out _, out _, out _, out _, out _), "Network input starts after two qualified retained native worlds.");
        VerifyRetainedInputBodyOwners();
        DiagnosticInputIdentity identity = _networkInputIdentity ?? throw new InvalidDataException("Missing Host input identity.");
        _networkInputClient = new(identity, _log);
        _networkInputController = new(_host, _ownCommand, identity.BodyGeneration, 4, 500,
            VerifyRetainedInputBodyOwners, Read,
            (address, bytes) => Win32Native.WriteMemory(_host.ProcessHandle, address, bytes, executable: false),
            DiagnosticInputClient.LocalNowMilliseconds, _log, allowOrdinaryAttack: _ordinaryNetwork);
        _networkInputStarted = true;
        _networkInputLastStep = _networkInputController.StepAtWorldReturn(tid, ref context, NativeBodyInputAvailability.Ready);
        NetworkInputNeutralReadback();
        Require(_networkInputClient.SetAvailability(DiagnosticInputAvailability.Ready), "Host mapping could not publish initial native readiness.");
        _log(new { @event = "template-network-input-begin", source = "Host-IPC5", generation = identity.BodyGeneration,
            command = Hex(_ownCommand), onlyFields = _ordinaryNetwork ? new[] { 4, 8, 0x14, 0x1A, 0x20 } : new[] { 4, 8, 0x14, 0x1A },
            hostClock = "GetTickCount64", ingressRestamped = false });
    }

    private void StepNetworkInputAtWorld(uint tid, ref Win32Native.X86Context context, bool ready)
    {
        if (!_networkInputFixture || !_networkInputStarted || _networkInputRetired) return;
        NetworkInputWorld(tid, context); VerifyRetainedInputBodyOwners();
        Require(++_networkInputWorlds <= NativeObservationBudget.WorldLimit, "Network input exceeded its retained native-world budget.");
        DiagnosticInputClient client = _networkInputClient!; NativeBodyInputController controller = _networkInputController!;
        bool dead = U(_traits + 0x124) == 0 || U(_playerTraitsPrimaryTraits + 0x124) == 0 || Read(_ownCommand + 0x5F, 1)[0] != 0;
        if (dead) { NetworkInputFail("Shared player died during the movement/jump delivery fixture.", DiagnosticInputTerminal.NativeSource); }
        if (_networkInputRebindAwaitWorld)
        {
            Require(!dead && client.SetAvailability(DiagnosticInputAvailability.Waiting),
                "Rebound input lost its Waiting endpoint before fresh native World apply.");
            NetworkInputPrivateFence(tid, ref context, acknowledgeSuppression: false);
            return;
        }
        if (client.TerminalOrRetired || _networkInputDisconnected)
        { ObserveNetworkInputTerminal(); NetworkInputDisconnectAtWorld(tid, ref context); return; }
        if (!client.SetAvailability(ready ? DiagnosticInputAvailability.Ready : DiagnosticInputAvailability.Paused))
        { ObserveNetworkInputTerminal(); NetworkInputDisconnectAtWorld(tid, ref context); return; }
        if (!ready)
        {
            if (!_networkInputWasPaused) { _networkInputWasPaused = true; ++_networkInputPauseCount; }
            NetworkInputPrivateFence(tid, ref context, acknowledgeSuppression: true);
            _networkInputAwaitNewEpoch = _networkInputEpoch != 0;
            _networkInputPauseNeutral |= Read(_sharedBattleTimer + 0x40, 1)[0] == 1;
            NetworkInputDiscardUnavailable(8); return;
        }
        if (_networkInputWasPaused) { _networkInputWasPaused = false; ++_networkInputResumeCount; }
        if (!client.TryReadBridgeStatus(out DiagnosticBridgeStatus status))
        {
            if (client.TerminalOrRetired)
            { ObserveNetworkInputTerminal(); NetworkInputDisconnectAtWorld(tid, ref context); }
            else if (_networkInputEpoch != 0)
            { NetworkInputFail("Admitted bridge control became terminal or unreadable.", DiagnosticInputTerminal.Transport); NetworkInputDisconnectAtWorld(tid, ref context); }
            else NetworkInputPrivateFence(tid, ref context, acknowledgeSuppression: false);
            return;
        }
        if (status.State != DiagnosticBridgeState.Ready)
        {
            NetworkInputPrivateFence(tid, ref context, acknowledgeSuppression: true);
            _networkInputAwaitNewEpoch = _networkInputEpoch != 0; NetworkInputDiscardUnavailable(8); return;
        }
        if (_networkInputAwaitNewEpoch && status.InputEpoch == _networkInputEpoch)
        { NetworkInputPrivateFence(tid, ref context, acknowledgeSuppression: true); NetworkInputDiscardUnavailable(8); return; }
        if (status.InputEpoch != _networkInputEpoch)
        {
            NetworkInputPrivateFence(tid, ref context, acknowledgeSuppression: true);
            _networkInputEpoch = status.InputEpoch; _networkInputNextSource = 1; _networkInputLastIngress = 0;
            _networkInputAwaitNewEpoch = false; ++_networkInputEpochChanges;
            _log(new { @event = "template-network-input-epoch-fence", epoch = _networkInputEpoch,
                privateControllerPausedOnly = true, actorReadinessRevisionChangedByEpoch = false });
            return;
        }
        if (_networkInputBarrier == NetworkInputBarrier.AwaitReady)
        {
            _networkInputLastStep = controller.StepAtWorldReturn(tid, ref context, NativeBodyInputAvailability.Ready);
            Require(_networkInputLastStep == NativeBodyInputStep.Neutral, "Fresh network epoch/resume replayed an old request.");
            NetworkInputNeutralReadback(); _networkInputBarrier = NetworkInputBarrier.None;
            _networkInputResumeNeutral |= _networkInputPauseNeutral && _networkInputResumeCount != 0;
            return;
        }

        NetworkInputPose pose = ReadNetworkInputPose();
        if (_networkInputInFlight is not null)
        {
            uint elapsed = unchecked(U(_sharedBattleTimer + 0x1C) - _networkInputAppliedTimer);
            if (elapsed != 0 && elapsed <= 10000) CompleteNetworkInputNativeInterval(tid, context, pose);
            else if ((ulong)DiagnosticInputClient.LocalNowMilliseconds() - _networkInputInFlight.Record.HostIngressMilliseconds > 500)
            {
                NetworkInputPrivateFence(tid, ref context, acknowledgeSuppression: true);
                Require(client.SetAvailability(DiagnosticInputAvailability.Ready, forceRearmRevision: true), "Stalled native interval could not request epoch rearm.");
                _networkInputAwaitNewEpoch = true; return;
            }
        }
        ObserveNetworkInputJump(pose);
        if (!HoldNetworkInputDrainForBackpressureFixture()) DrainNetworkInputAtWorld(tid, context, 8);
        _ = NetworkInputControlStillCurrent();
        if (_networkInputDisconnected || client.TerminalOrRetired)
        { ObserveNetworkInputTerminal(); NetworkInputDisconnectAtWorld(tid, ref context); return; }
        if (_networkInputRefreshEpoch)
        { _networkInputRefreshEpoch = false; NetworkInputPrivateFence(tid, ref context, acknowledgeSuppression: true); return; }
        CompleteNetworkInputDrainHoldAfterControlCheck();
        if (_networkInputInFlight is not null) return; // Preserve the exact command until an actual native interval consumes it.
        _networkInputLastStep = controller.StepAtWorldReturn(tid, ref context, NativeBodyInputAvailability.Ready);
        if (_networkInputLastStep == NativeBodyInputStep.Applied)
        {
            Require(_networkInputAccepted.Count != 0, "Controller applied an input without its exact source ledger.");
            _networkInputInFlight = _networkInputAccepted.Dequeue(); _networkInputBeforeFlight = pose;
            _networkInputAppliedTimer = U(_sharedBattleTimer + 0x1C);
            if (_ordinaryNetwork && !PrepareNetworkOrdinaryInputAtWorld(tid, ref context)) return;
            _log(new { @event = "template-network-input-command-published", epoch = _networkInputInFlight.Record.InputEpoch,
                sourceSequence = _networkInputInFlight.Record.SourceSequence, destinationSequence = _networkInputInFlight.DestinationSequence,
                hostIngress = _networkInputInFlight.Record.HostIngressMilliseconds,
                ordinaryAttackHeld = _networkInputInFlight.Record.OrdinaryAttackHeld, nativeApplyAcknowledged = false });
        }
        else if (_networkInputLastStep == NativeBodyInputStep.TimedOut)
        {
            CancelNetworkInputDrainHold("controller-timeout");
            NetworkInputSuppressLedger(tid, context, acknowledge: true); NetworkInputNeutralReadback();
            _networkInputTimeoutNeutral = true; ++_networkInputTimeouts;
            QualifyNetworkInputDrainHoldEarlyTimeout();
            Require(client.SetAvailability(DiagnosticInputAvailability.Ready, forceRearmRevision: true), "Native freshness timeout could not request a fresh epoch.");
            _networkInputAwaitNewEpoch = true; _networkInputBarrier = NetworkInputBarrier.AwaitReady;
        }
        else if (_networkInputLastStep is NativeBodyInputStep.Faulted or NativeBodyInputStep.Dead or NativeBodyInputStep.Disconnected)
        { NetworkInputFail("Native controller became terminal: " + _networkInputLastStep, DiagnosticInputTerminal.NativeSource); NetworkInputDisconnectAtWorld(tid, ref context); }
        _networkInputGoal |= _networkInputAcceptedCount != 0 && _networkInputAppliedCount != 0 &&
            _networkInputMoved && _networkInputSemanticJumpQualified && _networkInputRise;
    }

    // Explicit test-only hold of IPC drain; native worlds, command consumption,
    // existing ledger/ACKs and unconditional control freshness checks continue.
    private bool HoldNetworkInputDrainForBackpressureFixture()
    {
        if (!_networkInputDrainHoldRequested || _networkInputDrainHoldCompleted || _networkInputDrainHoldCancelled) return false;
        if (!_networkInputDrainHoldStarted)
        {
            if (_networkInputAppliedCount == 0 || _networkInputNativeAckCount == 0) return false;
            if (_networkInputDrainHoldMilliseconds <= 300)
            { if (!_networkInputMoved) return false; }
            else
            {
                // This interval must begin from real, already-ACKed neutral
                // applications, before the test driver requests movement/jump.
                if (_networkInputAppliedCount < 20 || !_networkInputLastAppliedNeutral ||
                    _networkInputAccepted.Count != 0 || _networkInputInFlight is not null) return false;
                NetworkInputNeutralReadback();
            }
            if (_networkInputDrainHoldArmedWorld == 0) { _networkInputDrainHoldArmedWorld = _networkInputWorlds; return false; }
            if (_networkInputWorlds <= _networkInputDrainHoldArmedWorld) return false;
            _networkInputDrainHoldStarted = true; _networkInputDrainHoldBeginWorld = _networkInputWorlds;
            _networkInputDrainHoldBeginMilliseconds = DiagnosticInputClient.LocalNowMilliseconds();
            _log(new { @event = "template-network-input-drain-hold-begin", milliseconds = _networkInputDrainHoldBeginMilliseconds,
                world = _networkInputWorlds, eligibilityWorld = _networkInputDrainHoldArmedWorld,
                requestedMilliseconds = _networkInputDrainHoldMilliseconds, actualNativeApplied = _networkInputAppliedCount,
                nativeAcknowledgments = _networkInputNativeAckCount, moved = _networkInputMoved,
                onlyDrainSkipped = true, controllerWorldAndAcknowledgmentPathsContinue = true });
        }
        long elapsed = DiagnosticInputClient.LocalNowMilliseconds() - _networkInputDrainHoldBeginMilliseconds;
        Require(elapsed >= _networkInputDrainHoldElapsedMilliseconds, "Drain hold monotonic clock moved backward.");
        _networkInputDrainHoldElapsedMilliseconds = elapsed;
        if (elapsed >= (_networkInputDrainHoldMilliseconds > 300 ? 650 : 500))
        { CancelNetworkInputDrainHold(_networkInputDrainHoldMilliseconds > 300 ? "hold-overran-diagnostic-bound" : "hold-overran-input-freshness"); return false; }
        if (elapsed >= _networkInputDrainHoldMilliseconds) { _networkInputDrainHoldCompletionDue = true; return false; }
        ++_networkInputDrainHoldWorlds; return true;
    }
    private void CompleteNetworkInputDrainHoldAfterControlCheck()
    {
        if (!_networkInputDrainHoldCompletionDue || _networkInputDrainHoldCompleted || _networkInputDrainHoldCancelled) return;
        Require(_networkInputDrainHoldStarted && _networkInputDrainHoldElapsedMilliseconds >= _networkInputDrainHoldMilliseconds,
            "Drain hold completion lacks its monotonic interval.");
        _networkInputDrainHoldEndMilliseconds = DiagnosticInputClient.LocalNowMilliseconds();
        _networkInputDrainHoldElapsedMilliseconds = _networkInputDrainHoldEndMilliseconds - _networkInputDrainHoldBeginMilliseconds;
        if (_networkInputDrainHoldElapsedMilliseconds >= (_networkInputDrainHoldMilliseconds > 300 ? 650 : 500))
        { CancelNetworkInputDrainHold(_networkInputDrainHoldMilliseconds > 300 ? "control-check-overran-diagnostic-bound" : "control-check-overran-input-freshness"); return; }
        _networkInputDrainHoldEndWorld = _networkInputWorlds; _networkInputDrainHoldCompletionDue = false; _networkInputDrainHoldCompleted = true;
        _log(new { @event = "template-network-input-drain-hold-end", milliseconds = _networkInputDrainHoldEndMilliseconds,
            world = _networkInputWorlds, beginWorld = _networkInputDrainHoldBeginWorld,
            heldMilliseconds = _networkInputDrainHoldElapsedMilliseconds, heldWorlds = _networkInputDrainHoldWorlds,
            completed = true, actualNativeApplied = _networkInputAppliedCount, nativeAcknowledgments = _networkInputNativeAckCount,
            unconditionalControlRecheckPassed = true, sourceNoCreditCoverageClaimed = false });
    }
    private void QualifyNetworkInputDrainHoldEarlyTimeout()
    {
        if (_networkInputDrainHoldMilliseconds <= 300 || !_networkInputDrainHoldRequested ||
            !_networkInputDrainHoldStarted || !_networkInputDrainHoldCancelled ||
            _networkInputDrainHoldCancelReason != "controller-timeout" || !_networkInputTimeoutNeutral ||
            _networkInputLastStep != NativeBodyInputStep.TimedOut ||
            _networkInputDrainHoldElapsedMilliseconds is < 0 or >= 650) return;
        Require(_networkInputAccepted.Count == 0 && _networkInputInFlight is null,
            "Native timeout diagnostic must retain no accepted record or in-flight acknowledgment.");
        NetworkInputNeutralReadback();
        _networkInputDrainHoldQualifiedEarlyTimeout = true;
        _log(new { @event = "template-network-input-drain-hold-qualified-early-timeout",
            requestedMilliseconds = _networkInputDrainHoldMilliseconds,
            actualHeldMilliseconds = _networkInputDrainHoldElapsedMilliseconds,
            nativeStep = _networkInputLastStep.ToString(), actualControllerTimeoutNeutral = true,
            fullRequestedDurationCompleted = false, sourceNoCreditRequiresIndependentEvidence = true,
            existingFreshEpochRearmWillContinue = true, sourceClockOrNativeTimeoutChanged = false });
    }

    private void CancelNetworkInputDrainHold(string reason)
    {
        if (!_networkInputDrainHoldRequested || _networkInputDrainHoldCompleted || _networkInputDrainHoldCancelled) return;
        if (!_networkInputDrainHoldStarted)
        {
            if (_networkInputDrainHoldArmedWorld != 0)
                _log(new { @event = "template-network-input-drain-hold-reset", world = _networkInputWorlds,
                    eligibilityWorld = _networkInputDrainHoldArmedWorld, reason, started = false, completed = false });
            _networkInputDrainHoldArmedWorld = 0; return;
        }
        _networkInputDrainHoldCancelled = true; _networkInputDrainHoldCompletionDue = false; _networkInputDrainHoldCancelReason = reason;
        _networkInputDrainHoldEndWorld = _networkInputWorlds; _networkInputDrainHoldEndMilliseconds = DiagnosticInputClient.LocalNowMilliseconds();
        _networkInputDrainHoldElapsedMilliseconds = Math.Max(0, _networkInputDrainHoldEndMilliseconds - _networkInputDrainHoldBeginMilliseconds);
        _log(new { @event = "template-network-input-drain-hold-cancelled", milliseconds = _networkInputDrainHoldEndMilliseconds,
            world = _networkInputWorlds, reason, completed = false, restartAllowed = false,
            heldMilliseconds = _networkInputDrainHoldElapsedMilliseconds, heldWorlds = _networkInputDrainHoldWorlds });
    }

    private void DrainNetworkInputAtWorld(uint tid, in Win32Native.X86Context context, int maximum)
    {
        DiagnosticInputClient client = _networkInputClient!; NativeBodyInputController controller = _networkInputController!;
        for (int i = 0; i < maximum && client.TryPeekDelivery(out DiagnosticInputDelivery ticket); ++i)
        {
            // TryPeek itself refreshes control even when it returns a retained
            // ticket. Never admit that ticket under an older outside snapshot.
            if (!NetworkInputControlStillCurrent()) return;
            DiagnosticInputRecord record = ticket.Record;
            if (record.InputEpoch < _networkInputEpoch)
            { if (!client.CommitDelivery(ticket, DiagnosticDeliveryDisposition.OldEpochDiscard)) return; ++_networkInputOldDiscarded; continue; }
            if (record.InputEpoch > _networkInputEpoch) { _networkInputRefreshEpoch = true; return; }
            if (record.SourceSequence != _networkInputNextSource || record.SourceSequence == uint.MaxValue ||
                record.CaptureOrdinal <= _networkInputLastCaptureOrdinal || record.HostIngressMilliseconds < _networkInputLastIngress ||
                _networkInputDestinationSequence == uint.MaxValue)
            { NetworkInputFail("Delivery source epoch/sequence/ordinal/ingress order is invalid.", DiagnosticInputTerminal.Sequence); return; }
            uint destination = _networkInputDestinationSequence + 1;
            NativeBodyInputEnqueueResult accepted = controller.Enqueue(client.DestinationIntent(record, destination));
            if (accepted == NativeBodyInputEnqueueResult.Full) { ++_networkInputFullRetries; return; }
            if (accepted is not (NativeBodyInputEnqueueResult.Accepted or NativeBodyInputEnqueueResult.SuppressedBeforeReady))
            { NetworkInputFail("Native input queue rejected delivery: " + accepted, DiagnosticInputTerminal.InvalidRecord); return; }
            bool suppressed = accepted == NativeBodyInputEnqueueResult.SuppressedBeforeReady;
            if (!client.CommitDelivery(ticket, suppressed ? DiagnosticDeliveryDisposition.Suppressed : DiagnosticDeliveryDisposition.Accepted))
            { NetworkInputFail("Exact accepted IPC ticket could not be committed.", DiagnosticInputTerminal.Identity); return; }
            _networkInputDestinationSequence = destination; ++_networkInputNextSource;
            _networkInputLastCaptureOrdinal = record.CaptureOrdinal; _networkInputLastIngress = record.HostIngressMilliseconds;
            NetworkInputMap map = new(record, destination);
            if (suppressed) { ++_networkInputSuppressed; AcknowledgeNetworkInputAtWorld(tid, context, map, "fresh-barrier-suppressed"); }
            else { Require(_networkInputAccepted.Count < 4, "Native source ledger exceeded the controller capacity."); _networkInputAccepted.Enqueue(map); ++_networkInputAcceptedCount; }
        }
    }
    private bool NetworkInputControlStillCurrent()
    {
        if (_networkInputClient!.TerminalOrRetired) return false;
        if (!_networkInputClient.TryReadBridgeStatus(out DiagnosticBridgeStatus current))
        {
            if (!_networkInputClient.TerminalOrRetired)
                NetworkInputFail("Admitted control changed or became unreadable during delivery.", DiagnosticInputTerminal.Transport);
            return false;
        }
        if (current.State != DiagnosticBridgeState.Ready || current.InputEpoch != _networkInputEpoch)
        { _networkInputRefreshEpoch = true; return false; }
        return true;
    }
    private void CompleteNetworkInputNativeInterval(uint tid, in Win32Native.X86Context context, NetworkInputPose pose)
    {
        NetworkInputMap map = _networkInputInFlight!; NetworkInputPose before = _networkInputBeforeFlight!;
        Require(map.Record.InputEpoch == _networkInputEpoch, "An old epoch reached native-applied interval acknowledgment.");
        if (map.Record.Magnitude > 0)
        {
            bool moved = NetworkInputHorizontalDistance(before.Position, pose.Position) > 0.01;
            _networkInputMoved |= moved;
            if (_networkInputTransportAttempt == 2) _networkInputRebindMoved |= moved;
        }
        if (map.Record.JumpHeld && _networkInputJumpBaseline is null) _networkInputJumpBaseline = before.Position[1];
        ObserveNetworkInputTypedJump(tid, context, map, pose);
        _networkInputLastAppliedNeutral = map.Record.Magnitude == 0 && !map.Record.JumpHeld && !map.Record.OrdinaryAttackHeld;
        ++_networkInputAppliedCount; AcknowledgeNetworkInputAtWorld(tid, context, map, "actual-ready-native-world");
        _networkInputInFlight = null; _networkInputBeforeFlight = null;
        _log(new { @event = "template-network-input-native-applied", epoch = map.Record.InputEpoch,
            sourceSequence = map.Record.SourceSequence, captureOrdinal = map.Record.CaptureOrdinal,
            destinationSequence = map.DestinationSequence, hostIngress = map.Record.HostIngressMilliseconds,
            actualWorld = _networkInputWorlds, nativePose = pose, moved = _networkInputMoved });
    }
    private void AcknowledgeNetworkInputAtWorld(uint tid, in Win32Native.X86Context context, NetworkInputMap map, string reason)
    {
        Win32Native.X86Context stopped = context;
        if (_networkInputClient!.TerminalOrRetired) return;
        Require(_networkInputClient.AcknowledgeApplied(map.Record.InputEpoch, map.Record.SourceSequence, () =>
        { NetworkInputWorld(tid, stopped); VerifyRetainedInputBodyOwners(); }), "Native source acknowledgment could not be published.");
        ++_networkInputNativeAckCount;
        _log(new { @event = "template-network-input-source-ack", epoch = map.Record.InputEpoch,
            sourceSequence = map.Record.SourceSequence, destinationSequence = map.DestinationSequence, reason,
            acceptedAloneWasNotAcknowledged = true });
    }
    private void NetworkInputPrivateFence(uint tid, ref Win32Native.X86Context context, bool acknowledgeSuppression)
    {
        CancelNetworkInputDrainHold("native-unready-pause-or-epoch-fence");
        _networkInputLastStep = _networkInputController!.StepAtWorldReturn(tid, ref context, NativeBodyInputAvailability.Paused);
        NetworkInputNeutralReadback(); NetworkInputSuppressLedger(tid, context, acknowledgeSuppression);
        _networkInputBarrier = NetworkInputBarrier.AwaitReady;
    }
    private void NetworkInputSuppressLedger(uint tid, in Win32Native.X86Context context, bool acknowledge)
    {
        if (_networkInputInFlight is not null)
        {
            ++_networkInputSuppressed; if (acknowledge) AcknowledgeNetworkInputAtWorld(tid, context, _networkInputInFlight, "safe-world-neutral-suppressed");
            _networkInputInFlight = null; _networkInputBeforeFlight = null;
        }
        while (_networkInputAccepted.Count != 0)
        {
            NetworkInputMap map = _networkInputAccepted.Dequeue(); ++_networkInputSuppressed;
            if (acknowledge) AcknowledgeNetworkInputAtWorld(tid, context, map, "safe-world-queued-suppressed");
        }
    }
    private void NetworkInputDiscardUnavailable(int maximum)
    {
        for (int i = 0; i < maximum && _networkInputClient!.TryPeekDelivery(out DiagnosticInputDelivery ticket); ++i)
        { if (!_networkInputClient.CommitDelivery(ticket, DiagnosticDeliveryDisposition.OldEpochDiscard)) return; ++_networkInputOldDiscarded; }
    }
    private void NetworkInputDisconnectAtWorld(uint tid, ref Win32Native.X86Context context)
    {
        CancelNetworkInputDrainHold("native-input-disconnected");
        _networkInputDisconnected = true; _networkInputController!.Disconnect();
        _networkInputLastStep = _networkInputController.StepAtWorldReturn(tid, ref context, NativeBodyInputAvailability.Disconnected);
        NetworkInputSuppressLedger(tid, context, acknowledge: false); NetworkInputNeutralReadback();
    }
    internal HostInputTransportRebindWitness PrepareHostInputRebindAtWorld(uint tid, ref Win32Native.X86Context context)
    {
        NetworkInputWorld(tid, context); VerifyRetainedInputBodyOwners();
        Require(NetworkInputDetachedForReconnect && _networkInputController is not null &&
            _networkInputAccepted.Count == 0 && _networkInputInFlight is null && _networkInputBeforeFlight is null &&
            (!_ordinaryNetwork || _networkOrdinarySend is null && _networkOrdinaryDamage is null &&
                (_networkOrdinaryAction is null || _networkOrdinaryAction.Completed)),
            "Host input rebind has no qualified settled terminal and exact empty native source ledger.");
        NetworkInputNeutralReadback();
        return new(_networkInputClient!.Identity, _host.DebugStopSequence, _instance, _ownRoot, _ownCommand, _scene);
    }
    internal void CommitHostInputRebindAtWorld(uint tid, ref Win32Native.X86Context context,
        DiagnosticInputIdentity next, DiagnosticInputClient client, HostInputTransportRebindWitness witness)
    {
        Require(PrepareHostInputRebindAtWorld(tid, ref context) == witness && client.Identity == next && !client.TerminalOrRetired &&
            next.Role == DiagnosticInputRole.Host && next.SessionId != witness.Previous.SessionId &&
            next.ScenarioId == witness.Previous.ScenarioId && next.LevelEpoch == witness.Previous.LevelEpoch &&
            next.BodyGeneration == witness.Previous.BodyGeneration,
            "Host input rebind commit lost the same-stop native command/body or fresh endpoint.");
        var controller = new NativeBodyInputController(_host, _ownCommand, next.BodyGeneration, 4, 500,
            VerifyRetainedInputBodyOwners, Read,
            (address, bytes) => Win32Native.WriteMemory(_host.ProcessHandle, address, bytes, executable: false),
            DiagnosticInputClient.LocalNowMilliseconds, _log, allowOrdinaryAttack: _ordinaryNetwork);
        _networkInputController!.RetireBeforeDelete(tid, ref context);
        NetworkInputNeutralReadback();
        _networkInputPreviousTransport = new { attempt = 1, identity = _networkInputClient!.Identity,
            endpoint = _networkInputClient.Summary, controller = _networkInputController.Summary };
        _networkInputClient.Dispose();
        _networkInputClient = client; _networkInputController = controller; _networkInputIdentity = next;
        _networkInputEndpointSummary = null; _networkInputTransportAttempt = 2;
        _networkInputRebindAppliedBaseline = _networkInputAppliedCount; _networkInputRebindAwaitWorld = true;
        // These fields belong to the NEW connection. Old client/controller
        // remain terminal/retired; native budgets and cumulative evidence remain.
        _networkInputDisconnected = false; _networkInputExpectedPeerRetirement = false;
        _networkInputEpoch = 0; _networkInputNextSource = 1; _networkInputDestinationSequence = 0;
        _networkInputLastCaptureOrdinal = 0; _networkInputLastIngress = 0;
        _networkInputAwaitNewEpoch = false; _networkInputRefreshEpoch = false; _networkInputWasPaused = false;
        _networkInputBarrier = NetworkInputBarrier.AwaitReady;
        _networkInputLastStep = controller.StepAtWorldReturn(tid, ref context, NativeBodyInputAvailability.Paused);
        NetworkInputNeutralReadback();
    }
    internal bool AllowReboundInputAtWorld(uint tid, ref Win32Native.X86Context context, bool nativeWorldApplied)
    {
        if (_networkInputTransportAttempt != 2 || !_networkInputRebindAwaitWorld || !nativeWorldApplied) return false;
        NetworkInputWorld(tid, context); VerifyRetainedInputBodyOwners();
        if (!ReadSharedBattleFlow(out _, out _, out _, out _, out _) || Read(_sharedBattleTimer + 0x40, 1)[0] != 0) return false;
        NetworkInputNeutralReadback();
        Require(_networkInputClient!.SetAvailability(DiagnosticInputAvailability.Ready),
            "Rebound input could not publish readiness after fresh native World apply.");
        _networkInputRebindAwaitWorld = false; return true;
    }
    private void ObserveNetworkInputTerminal()
    {
        if (_networkInputDisconnected || _networkInputFailure.Length != 0) return;
        DiagnosticInputTerminal terminal = _networkInputClient!.Terminal;
        bool actionSettled = !_ordinaryNetwork || _networkOrdinaryAction is null || _networkOrdinaryAction.Completed;
        if (terminal == DiagnosticInputTerminal.ActorClosed && NetworkInputFixtureGoalPassed && actionSettled)
        {
            _networkInputExpectedPeerRetirement = true;
            _log(new { @event = "template-network-input-peer-retired-after-goal", terminal = terminal.ToString(),
                qualifiedNativeApplications = _networkInputAppliedCount, actionSettled,
                terminalRemainsClosed = true, pendingInputWillBeDiscardedWithoutNativeAck = true,
                nextSafeWorldNeutralizes = true, reconnectOrReplayPermitted = false });
            return;
        }
        NetworkInputFail("Input channel ended outside a qualified, settled peer retirement: " + terminal,
            terminal == DiagnosticInputTerminal.None ? DiagnosticInputTerminal.Transport : terminal);
    }
    private void NetworkInputFail(string reason, DiagnosticInputTerminal terminal)
    {
        if (_networkInputFailure.Length == 0)
        { CancelNetworkInputDrainHold("native-input-failure"); _networkInputFailure = reason; _networkInputDisconnected = true; _networkInputClient?.Fence(terminal);
            _log(new { @event = "template-network-input-failed", reason, nextSafeWorldMustNeutralize = true, failedScopeWillNotResume = true }); }
    }

    private void RetireNetworkInputBeforeUnload(uint tid, ref Win32Native.X86Context context, uint entry,
        Action<uint, Win32Native.X86Context> verifyPreUnloadBoundary)
    {
        if (!_networkInputFixture || _networkInputRetired) return;
        ArgumentNullException.ThrowIfNull(verifyPreUnloadBoundary);
        Require(_networkInputStarted && _networkInputController is not null && context.Eip == entry,
            "Network input retirement requires its live owner and genuine checked pre-unload entry.");
        NativeBodyInputController controller = _networkInputController ?? throw new InvalidDataException("Missing network input controller.");
        CancelNetworkInputDrainHold("native-unload");
        controller.RetireBeforeDelete(tid, ref context, verifyPreUnloadBoundary);
        _networkInputLastCommand = ReadNetworkInputFixtureCommand();
        _networkInputRetiredDrops += checked((uint)_networkInputAccepted.Count + (_networkInputInFlight is null ? 0u : 1u));
        _networkInputAccepted.Clear(); _networkInputInFlight = null; _networkInputBeforeFlight = null;
        _networkInputClient?.Retire(); _networkInputEndpointSummary = _networkInputClient?.Summary;
        _networkInputRetired = true; ReleaseNetworkInputEndpoint();
        _log(new { @event = "template-network-input-retired-before-unload", entry = Hex(entry),
            cachedGoal = NetworkInputFixtureGoalPassed, commandNeutral = true, noUnloadAcceptedRecordWasCalledNativeApplied = true });
    }
    internal void ReleaseNetworkInputEndpoint()
    {
        if (!_networkInputFixture || _networkInputEndpointReleased) return;
        CancelNetworkInputDrainHold("endpoint-release");
        _networkInputClient?.Dispose(); _networkInputEndpointSummary = _networkInputClient?.Summary;
        _networkInputEndpointReleased = true; // Does not claim controller/native retirement after an aborted run.
    }
    private byte[] ReadNetworkInputFixtureCommand()
    {
        Require(_networkInputFixture, "Network command route requires its explicit fixture.");
        if (_networkInputRetired) return _networkInputLastCommand is not null ? (byte[])_networkInputLastCommand.Clone() :
            throw new InvalidDataException("Missing retired network command snapshot.");
        Table(_ownCommand, 0x703090); byte[] command = Read(_ownCommand, 0x74);
        Require(new[] { 4, 8, 12, 16, 20 }.All(offset => float.IsFinite(BitConverter.ToSingle(command, offset))) &&
            command[0x1A] <= 1 && command[0x1F] == 0 &&
            (_ordinaryNetwork ? command[0x20] <= 1 : command[0x20] == 0) && command[0x21] == 0,
            "Network controller changed an unadmitted native attack/defend/nonfinite field.");
        _networkInputLastCommand = command; return command;
    }
    private void NetworkInputNeutralReadback()
    { byte[] command = ReadNetworkInputFixtureCommand(); Require(BitConverter.ToSingle(command, 4) == 0 && command[0x1A] == 0 &&
        (!_ordinaryNetwork || command[0x20] == 0), "Network fence failed to release movement/jump/ordinary attack."); }
    private NetworkInputPose ReadNetworkInputPose()
    {
        byte[] raw = Read(_ownRoot + 0x74, 12); float[] position = Enumerable.Range(0, 3).Select(i => BitConverter.ToSingle(raw, i * 4)).ToArray();
        float vertical = BitConverter.ToSingle(Read(_move + 0x1B4, 4));
        Require(position.All(x => float.IsFinite(x) && MathF.Abs(x) <= 10000000) && float.IsFinite(vertical), "Native network-body pose is nonfinite/out of bounds.");
        return new(position, U(_machine + 0x130), vertical);
    }
    private void ObserveNetworkInputJump(NetworkInputPose pose)
    {
        if (_networkInputJumpBaseline is not float baseline) return;
        _networkInputObservedState18 |= pose.State == 18;
        _networkInputRise |= pose.Position[1] > baseline + 5 && pose.Vertical > 0.1f;
        _networkInputFall |= _networkInputRise && pose.Vertical < -0.1f;
    }
    private void ObserveNetworkInputTypedJump(uint tid, in Win32Native.X86Context context,
        NetworkInputMap map, NetworkInputPose pose)
    {
        if (!map.Record.JumpHeld) return;
        _networkInputJumpObserver ??= new(_host, _image);
        NativeCharacterJumpWitness? native = _networkInputJumpObserver.ReadAtWorld(tid, context, _machine, _animator, pose.State);
        if (native is null) return;
        _networkInputSemanticJumpQualified = true;
        if (_networkInputSemanticJumpWitnesses >= 16) return;
        ++_networkInputSemanticJumpWitnesses;
        _networkInputSemanticJumpWitness = new
        {
            epoch = map.Record.InputEpoch, sourceSequence = map.Record.SourceSequence, captureOrdinal = map.Record.CaptureOrdinal,
            destinationSequence = map.DestinationSequence, actualWorld = _networkInputWorlds, nativePose = pose,
            sourceJumpHeld = map.Record.JumpHeld, typedNativeJump = true,
            stateIndex = native.StateIndex, state = Hex(native.State), table = Hex(native.Table), getter = Hex(native.Getter),
            typeRecord = Hex(native.TypeRecord), classId = Hex(native.ClassId), selector = native.Selector,
            owner = Hex(native.Owner), animator = Hex(native.Animator),
            nativeRiseFromJumpBaseline = _networkInputJumpBaseline is float baseline &&
                pose.Position[1] > baseline + 5 && pose.Vertical > 0.1f,
            originalStateArrayAndRegisteredTypeVerified = true, nativeWrites = 0, nativeCalls = 0
        };
        _log(new { @event = "template-network-input-native-jump-qualified", data = _networkInputSemanticJumpWitness });
    }
    private static double NetworkInputHorizontalDistance(float[] a, float[] b) => Math.Sqrt(Math.Pow((double)a[0] - b[0], 2) + Math.Pow((double)a[2] - b[2], 2));
    private void NetworkInputWorld(uint tid, in Win32Native.X86Context context) => Require(_host.IsAtDebugStop &&
        tid == _host.MainThreadId && context.Eip == NativeCallDispatcher.WorldReturn, "Network input requires an actual owned main-thread WorldRET.");
}
