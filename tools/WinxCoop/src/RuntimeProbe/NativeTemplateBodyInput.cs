using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>Bounded local E4 source; no guest transport or replacement native tick.</summary>
internal sealed partial class NativeTemplateProbe
{
    private enum BodyInputPhase { Baseline, Walk, Turn, JumpHeld, Released, TimeoutArm, TimeoutWait, NeutralWait, Failed }
    private const int BodyInputTimeoutMilliseconds = 500;
    private bool _bodyInputFixture, _bodyInputStarted, _bodyInputRetired, _bodyInputWasPaused;
    private bool _bodyInputIntervalPending, _bodyInputWalkPassed, _bodyInputTurnPassed, _bodyInputJumpPassed;
    private bool _bodyInputState18, _bodyInputRise, _bodyInputFall, _bodyInputTimeoutObserved;
    private bool _bodyInputPauseNeutral, _bodyInputResumeNeutral, _bodyInputNeutralObserved;
    private NativeBodyInputController? _bodyInputController;
    private BodyInputPhase _bodyInputPhase;
    private uint _bodyInputSequence, _bodyInputPhaseWorlds, _bodyInputReadyWorlds, _bodyInputStableLanding;
    private uint _bodyInputPauseCount, _bodyInputResumeCount, _bodyInputSuppressedPauseProbes;
    private float _bodyInputReference, _bodyInputTurnInitialError, _bodyInputPeakY;
    private float[]? _bodyInputWalkStart, _bodyInputTurnBasis, _bodyInputJumpBaseline, _bodyInputLastLanding;
    private long _bodyInputTimeoutStarted;
    private byte[]? _bodyInputLastCommand;
    private string _bodyInputFailure = "";
    private BodyInputPose? _bodyInputLastPose;
    private NativeBodyInputStep _bodyInputLastStep;
    private sealed record BodyInputPose(float[] World, float[] Basis, float Heading, float Forward,
        float Vertical, float Lateral, uint State, uint Kind);

    internal bool BodyInputFixtureEnabled => _bodyInputFixture;
    internal bool BodyInputFixtureRetired => !_bodyInputFixture || _bodyInputController?.Retired == true;
    private bool BodyInputMotionPhase => _bodyInputPhase is BodyInputPhase.Baseline or BodyInputPhase.Walk or
        BodyInputPhase.Turn or BodyInputPhase.JumpHeld or BodyInputPhase.Released;
    internal bool BodyInputFixtureGoalPassed => _bodyInputFixture && _bodyInputStarted && _bodyInputFailure.Length == 0 &&
        _bodyInputWalkPassed && _bodyInputTurnPassed && _bodyInputJumpPassed && _bodyInputTimeoutObserved &&
        _bodyInputPauseNeutral && _bodyInputResumeNeutral && _bodyInputNeutralObserved;
    private object BodyInputFixtureSummary => new
    {
        enabled = _bodyInputFixture, started = _bodyInputStarted, retired = _bodyInputRetired,
        sourceKind = "finite-local-scripted-diagnostic-not-guest", generation = 1, sourceSequence = _bodyInputSequence,
        phase = _bodyInputPhase.ToString(), phaseReadyWorlds = _bodyInputPhaseWorlds, observedReadyWorlds = _bodyInputReadyWorlds,
        walkPassed = _bodyInputWalkPassed, turnPassed = _bodyInputTurnPassed, jumpPassed = _bodyInputJumpPassed,
        observedState18 = _bodyInputState18, observedRise = _bodyInputRise, observedFall = _bodyInputFall,
        stableLandingWorlds = _bodyInputStableLanding, peakY = _bodyInputPeakY,
        timeoutObserved = _bodyInputTimeoutObserved, timeoutMilliseconds = BodyInputTimeoutMilliseconds,
        timeoutSourceHeldMovement = false, pauseNeutralObserved = _bodyInputPauseNeutral,
        resumeFreshBarrierNeutral = _bodyInputResumeNeutral, pauseTransitions = _bodyInputPauseCount,
        resumeTransitions = _bodyInputResumeCount, suppressedPauseProbeSamples = _bodyInputSuppressedPauseProbes,
        neutralReadback = _bodyInputNeutralObserved, lastStep = _bodyInputLastStep.ToString(), lastNativePose = _bodyInputLastPose,
        controller = _bodyInputController?.Summary, failure = _bodyInputFailure, passed = BodyInputFixtureGoalPassed,
        nativeTickCalls = 0, nodeTransformWrites = 0, attackFieldsWritten = false, guestDeliveryQualified = false
    };

    private void ConfigureBodyInputFixture(ProbeConfiguration config)
    {
        _bodyInputFixture = config.TemplateBodyInputFixture;
        if (!_bodyInputFixture) return;
        Require(config.TemplateSharedPlayerProfile == "alfea-28-input" && config.Mode == "template-bloom-body-hair-idle" &&
            config.TemplateProjectileManagerFixture && config.TemplatePlayerTraitsFixture && config.TemplatePlayerTraitsPickupFx &&
            config.TemplateTargetRegionFixture && config.TemplateConfigureTargetRegion && config.TemplateSharedHealthAlias &&
            !config.TemplateHealthBattleFixture && !config.TemplateBattleCombatFixture && !config.PrepareFirstMagicStage &&
            !config.TemplateSecondAttackDegreeFixture && !config.TemplateDeathBattleFixture && !config.TemplateStoryDeathFixture &&
            !config.TemplateCollisionDetectorSecondaryRole && !config.TemplateStingerSecondaryRole &&
            !config.TemplateStarSecondaryEligibility && !config.TemplateStarPlacementFixture && !config.TemplateSharedMagicEnergyAlias,
            "Body input requires only its separate Alfea28 shared-HP/hair/PM diagnostic, without attacks or death adapters.");
    }

    private void BeginBodyInputFixtureAtWorld(uint tid, ref Win32Native.X86Context context)
    {
        if (!_bodyInputFixture) return;
        BodyInputWorld(tid, context);
        Require(!_bodyInputStarted && _bodyInputController is null && _sharedBattleCaptured &&
            _healthBattleHolding && _healthBattleFastRouteStarted && _idleWorldStops == 2 &&
            _ownCharacterTicks == 2 && _ownMoveUpdates == 2 && _ownBloomUpdates == 2 &&
            _sharedPlayerProfile.Name == "alfea-28-input" && _sharedPlayerProfile.Level == 28 &&
            _sharedPlayerProfile.Story && _sharedPlayerProfile.LiveResources &&
            ReadSharedBattleFlow(out _, out _, out _, out _, out _),
            "Body input starts only after the two counted ready native worlds and retained lifetime capture.");
        _bodyInputStarted = true;
        _bodyInputController = new(_host, _ownCommand, 1, 4, BodyInputTimeoutMilliseconds,
            VerifyBodyInputOwners, Read,
            (address, bytes) => Win32Native.WriteMemory(_host.ProcessHandle, address, bytes, executable: false),
            () => Environment.TickCount64, _log);
        _bodyInputLastPose = ReadBodyInputPose(); _bodyInputPhase = BodyInputPhase.Baseline;
        _bodyInputLastStep = _bodyInputController.StepAtWorldReturn(tid, ref context, NativeBodyInputAvailability.Ready);
        _bodyInputIntervalPending = true;
        _log(new { @event = "template-body-input-source-begin", sourceKind = "finite-local-scripted-diagnostic-not-guest",
            instance = Hex(_instance), command = Hex(_ownCommand), readyPhaseWorlds = new[] { 12, 6, 6, 4 },
            releaseLandingLimit = 160, timeoutMilliseconds = BodyInputTimeoutMilliseconds,
            ownNativePose = _bodyInputLastPose, nativeTickOrTransformWrites = 0 });
    }

    // Called even for an overlay before the caller's readiness early return.
    private void StepBodyInputFixtureAtWorld(uint tid, ref Win32Native.X86Context context, bool ready)
    {
        if (!_bodyInputFixture || !_bodyInputStarted || _bodyInputRetired) return;
        BodyInputWorld(tid, context); VerifyBodyInputOwners();
        NativeBodyInputController controller = _bodyInputController ?? throw new InvalidDataException("Missing body input owner.");
        bool dead = U(_traits + 0x124) == 0 || U(_playerTraitsPrimaryTraits + 0x124) == 0;
        if (dead)
        {
            BodyInputFail("Common HP reached zero during the movement-only fixture.");
            _bodyInputLastStep = controller.StepAtWorldReturn(tid, ref context, NativeBodyInputAvailability.Dead);
            BodyInputNeutralReadback(); return;
        }
        if (_bodyInputFailure.Length != 0)
        {
            _bodyInputLastStep = controller.StepAtWorldReturn(tid, ref context,
                ready ? NativeBodyInputAvailability.Ready : NativeBodyInputAvailability.Paused);
            BodyInputNeutralReadback(); return;
        }
        if (!ready)
        {
            _bodyInputLastStep = controller.StepAtWorldReturn(tid, ref context, NativeBodyInputAvailability.Paused);
            BodyInputNeutralReadback(); _bodyInputIntervalPending = false;
            _bodyInputPauseNeutral |= Read(_sharedBattleTimer + 0x40, 1)[0] == 1;
            if (!_bodyInputWasPaused)
            {
                _bodyInputWasPaused = true; ++_bodyInputPauseCount;
                byte[] command = ReadBodyInputFixtureCommand();
                var probe = new NativeBodyInputIntent(1, checked(++_bodyInputSequence), 1,
                    BodyInputAngle(BitConverter.ToSingle(command, 8)), BodyInputAngle(BitConverter.ToSingle(command, 0x14)), true,
                    Environment.TickCount64);
                Require(controller.Enqueue(probe) == NativeBodyInputEnqueueResult.SuppressedBeforeReady,
                    "Paused diagnostic request was not suppressed before reaching native memory.");
                ++_bodyInputSuppressedPauseProbes;
                _log(new { @event = "template-body-input-paused-neutral", nativePaused = Read(_sharedBattleTimer + 0x40, 1)[0],
                    phase = _bodyInputPhase.ToString(), sequence = _bodyInputSequence, heldProbeWrittenToNative = false });
            }
            return;
        }
        if (_bodyInputWasPaused)
        {
            _bodyInputLastStep = controller.StepAtWorldReturn(tid, ref context, NativeBodyInputAvailability.Ready);
            Require(_bodyInputLastStep == NativeBodyInputStep.Neutral, "Resume replayed an old body input request.");
            BodyInputNeutralReadback();
            _bodyInputResumeNeutral |= _bodyInputPauseNeutral && _sharedBattleLongestResumedNativePauseMilliseconds >= 5000;
            _bodyInputWasPaused = false; ++_bodyInputResumeCount; _bodyInputIntervalPending = false;
            if (_bodyInputPhase == BodyInputPhase.TimeoutWait) _bodyInputPhase = BodyInputPhase.TimeoutArm;
            _log(new { @event = "template-body-input-resume-fresh-barrier", phase = _bodyInputPhase.ToString(),
                sequence = _bodyInputSequence, firstReadyWorldNeutral = true, historicalHeldProbeReplayed = false });
            return;
        }
        if (BodyInputMotionPhase)
        {
            BodyInputPose pose = ReadBodyInputPose(); _bodyInputLastPose = pose;
            if (_bodyInputIntervalPending) ObserveBodyInputInterval(pose);
            if (++_bodyInputReadyWorlds > 240) BodyInputFail("Native movement/jump did not finish within240 ready worlds.");
        }
        if (_bodyInputFailure.Length == 0) SubmitBodyInputPhase();
        _bodyInputLastStep = controller.StepAtWorldReturn(tid, ref context, NativeBodyInputAvailability.Ready);
        if (_bodyInputLastStep is NativeBodyInputStep.Faulted or NativeBodyInputStep.Dead or NativeBodyInputStep.Disconnected)
            BodyInputFail("Body input controller became terminal: " + _bodyInputLastStep);
        if (_bodyInputPhase == BodyInputPhase.TimeoutWait)
        {
            if (_bodyInputLastStep == NativeBodyInputStep.TimedOut)
            {
                BodyInputNeutralReadback(); _bodyInputTimeoutObserved = true; _bodyInputPhase = BodyInputPhase.NeutralWait;
                _log(new { @event = "template-body-input-native-boundary-timeout", elapsedMilliseconds = Environment.TickCount64 - _bodyInputTimeoutStarted,
                    sourceHeldMovement = false, nativeIntentNeutral = true });
            }
            else if (Environment.TickCount64 - _bodyInputTimeoutStarted > 4000)
                BodyInputFail("No actual controller freshness timeout within4000ms of neutral-source stall.");
        }
        if (_bodyInputPhase is BodyInputPhase.Baseline or BodyInputPhase.NeutralWait or BodyInputPhase.Failed)
            BodyInputNeutralReadback();
        _bodyInputIntervalPending = BodyInputMotionPhase;
    }

    private void ObserveBodyInputInterval(BodyInputPose pose)
    {
        ++_bodyInputPhaseWorlds;
        _log(new { @event = "template-body-input-native-world", sourceKind = "finite-local-scripted-diagnostic-not-guest",
            phase = _bodyInputPhase.ToString(), interval = _bodyInputPhaseWorlds, sourceSequence = _bodyInputSequence,
            originalSchedulerPose = pose, manualTickOrPoseWrites = 0 });
        switch (_bodyInputPhase)
        {
            case BodyInputPhase.Baseline when _bodyInputPhaseWorlds == 12:
                _bodyInputWalkStart = (float[])pose.World.Clone(); _bodyInputReference = BodyInputAngle(pose.Heading);
                BodyInputNext(BodyInputPhase.Walk); break;
            case BodyInputPhase.Walk when _bodyInputPhaseWorlds == 6:
                _bodyInputWalkPassed = BodyInputHorizontalDistance(_bodyInputWalkStart!, pose.World) > 0.01;
                if (!_bodyInputWalkPassed) { BodyInputFail("Six movement requests produced no horizontal own-body displacement."); break; }
                _bodyInputTurnBasis = (float[])pose.Basis.Clone();
                _bodyInputTurnInitialError = BodyInputHeadingError(pose.Heading);
                BodyInputNext(BodyInputPhase.Turn); break;
            case BodyInputPhase.Turn when _bodyInputPhaseWorlds == 6:
                _bodyInputTurnPassed = _bodyInputTurnInitialError > 0.1f && BodyInputHeadingError(pose.Heading) < _bodyInputTurnInitialError - 0.001f &&
                    Enumerable.Range(0, 9).Any(i => MathF.Abs(pose.Basis[i] - _bodyInputTurnBasis![i]) > 0.001f);
                if (!_bodyInputTurnPassed || pose.Kind != 0 || pose.State != 0 || MathF.Abs(pose.Vertical) > 0.1f)
                { BodyInputFail("Native quarter-turn or grounded kind0/state0 jump baseline did not qualify."); break; }
                _bodyInputJumpBaseline = (float[])pose.World.Clone(); _bodyInputPeakY = pose.World[1];
                BodyInputNext(BodyInputPhase.JumpHeld); break;
            case BodyInputPhase.JumpHeld:
            case BodyInputPhase.Released:
                _bodyInputState18 |= pose.State == 18; _bodyInputPeakY = MathF.Max(_bodyInputPeakY, pose.World[1]);
                _bodyInputRise |= pose.World[1] > _bodyInputJumpBaseline![1] + 5 && pose.Vertical > 0.1f;
                _bodyInputFall |= _bodyInputRise && pose.Vertical < -0.1f;
                if (_bodyInputPhase == BodyInputPhase.JumpHeld && _bodyInputPhaseWorlds == 4) BodyInputNext(BodyInputPhase.Released);
                else if (_bodyInputPhase == BodyInputPhase.Released)
                {
                    bool landed = _bodyInputState18 && _bodyInputRise && _bodyInputFall && pose.State == 0 &&
                        MathF.Abs(pose.World[1] - _bodyInputJumpBaseline![1]) <= 2 && MathF.Abs(pose.Vertical) <= 0.01f &&
                        MathF.Abs(pose.Forward) <= 0.01f && MathF.Abs(pose.Lateral) <= 0.01f && _bodyInputLastLanding is not null &&
                        Enumerable.Range(0, 3).All(i => MathF.Abs(pose.World[i] - _bodyInputLastLanding![i]) <= 0.1f);
                    _bodyInputStableLanding = landed ? _bodyInputStableLanding + 1 : 0;
                    _bodyInputLastLanding = (float[])pose.World.Clone();
                    if (_bodyInputStableLanding >= 6) { _bodyInputJumpPassed = true; BodyInputNext(BodyInputPhase.TimeoutArm); }
                    else if (_bodyInputPhaseWorlds >= 160) BodyInputFail("No bounded native jump rise/fall/stable landing.");
                }
                break;
        }
    }

    private void SubmitBodyInputPhase()
    {
        if (_bodyInputPhase is BodyInputPhase.Baseline or BodyInputPhase.TimeoutWait or BodyInputPhase.NeutralWait or BodyInputPhase.Failed) return;
        byte[] command = ReadBodyInputFixtureCommand();
        float relative = BodyInputAngle(BitConverter.ToSingle(command, 8)), reference = BodyInputAngle(BitConverter.ToSingle(command, 0x14));
        float magnitude = 0; bool jump = _bodyInputPhase == BodyInputPhase.JumpHeld;
        if (_bodyInputPhase is BodyInputPhase.Walk or BodyInputPhase.Turn)
        { magnitude = 1; relative = _bodyInputPhase == BodyInputPhase.Turn ? MathF.PI / 2 : 0; reference = _bodyInputReference; }
        var intent = new NativeBodyInputIntent(1, checked(++_bodyInputSequence), magnitude, relative, reference, jump, Environment.TickCount64);
        Require(_bodyInputController!.Enqueue(intent) == NativeBodyInputEnqueueResult.Accepted, "Fresh diagnostic body intent was rejected.");
        if (_bodyInputPhase == BodyInputPhase.TimeoutArm)
        { _bodyInputTimeoutStarted = Environment.TickCount64; BodyInputNext(BodyInputPhase.TimeoutWait); }
    }

    private void RetireBodyInputFixtureBeforeUnload(uint tid, ref Win32Native.X86Context context, uint entry,
        Action<uint, Win32Native.X86Context> verifyPreUnloadBoundary)
    {
        if (!_bodyInputFixture || _bodyInputRetired) return;
        ArgumentNullException.ThrowIfNull(verifyPreUnloadBoundary);
        Require(_bodyInputStarted && _bodyInputController is not null && context.Eip == entry,
            "Body input retirement requires its live owner and original checked unload entry.");
        byte[] command = ReadBodyInputFixtureCommand();
        NativeBodyInputController controller = _bodyInputController ?? throw new InvalidDataException("Missing live body input owner.");
        controller.RetireBeforeDelete(tid, ref context, (thread, stopped) =>
        { Require(stopped.Eip == entry, "Body input unload entry changed."); verifyPreUnloadBoundary(thread, stopped); });
        Array.Clear(command, 4, 4); command[0x1A] = 0; _bodyInputLastCommand = command;
        _bodyInputRetired = controller.Retired; _bodyInputIntervalPending = false;
        _log(new { @event = "template-body-input-retired-before-unload", entry = Hex(entry), retired = _bodyInputRetired,
            controller = controller.Summary, cachedGoalBeforeDelete = BodyInputFixtureGoalPassed,
            nativeIntentNeutral = true, allSubsequentCommandReadsCached = true });
    }

    private byte[] ReadBodyInputFixtureCommand()
    {
        Require(_bodyInputFixture, "Body input command route requires its explicit fixture.");
        if (_bodyInputRetired) return _bodyInputLastCommand is not null ? (byte[])_bodyInputLastCommand.Clone() :
            throw new InvalidDataException("No retired body input snapshot.");
        Table(_ownCommand, 0x703090); byte[] command = Read(_ownCommand, 0x74);
        Require(new[] { 4, 8, 12, 16, 20 }.All(offset => float.IsFinite(BitConverter.ToSingle(command, offset))), "Body command contains a nonfinite native field.");
        if (!_bodyInputStarted) Require(BitConverter.ToSingle(command, 4) == 0 && command[0x1A] == 0, "Warmup body command is not neutral.");
        _bodyInputLastCommand = command; return command;
    }
    private void BodyInputNeutralReadback()
    {
        byte[] command = ReadBodyInputFixtureCommand();
        Require(BitConverter.ToSingle(command, 4) == 0 && command[0x1A] == 0, "Body input failed to release movement/jump.");
        _bodyInputNeutralObserved = true;
    }
    private void VerifyBodyInputOwners()
    {
        Require(_bodyInputFixture && _bodyInputStarted && !_bodyInputRetired,
            "Scripted body input is outside its live fixture interval.");
        VerifyRetainedInputBodyOwners();
    }
    // Shared fixed owner graph for the separately selected retained-input sources.
    private void VerifyRetainedInputBodyOwners()
    {
        Require((_bodyInputFixture || _networkInputFixture) && _host.IsAtDebugStop &&
            _stage == 6 && _idleActive && !_dependentInterval && _sharedBattleCaptured && !_sharedBattleUnloadStarted &&
            _healthBattleHolding && _healthBattleFastRouteStarted && _level == 28 && DescriptorCount == 8 &&
            _sharedHealthAlias?.Active == true && _ownCommand != _command && _ownRoot != _localRoot,
            "Body input escaped its stopped retained live ownership interval.");
        byte[] instance = Read(_instance, 0x28), root = Read(_ownRoot, 0x40), character = Read(_character, 0x150),
            move = Read(_move, 0x164), machine = Read(_machine, 0x134), animator = Read(_animator, 0x12C), traits = Read(_traits, 0x190);
        static uint W(byte[] bytes, int offset) => BitConverter.ToUInt32(bytes, offset);
        Require(W(instance, 0) == 0x710F80 && W(instance, 0x14) == _template && W(instance, 0x20) == 8 && W(instance, 0x24) == _ownRoot &&
            W(root, 0) == VisualGraphReader.NodeVtable && W(root, 0x2C) == _parent && W(root, 0x3C) == _scene &&
            W(character, 0) == 0x6F6438 && W(character, 0x18) == _ownRoot && W(character, 0x24) == _instance &&
            W(character, 0x124) == _machine && W(character, 0x128) == _animator && W(character, 0x12C) == _move &&
            W(character, 0x130) == _ownCommand && W(character, 0x138) == _traits && W(character, 0x140) == _projectileManager &&
            W(move, 0) == 0x6F64F8 && W(move, 0x160) == _character && W(machine, 0) == 0x6FA288 &&
            W(machine, 0x124) == _character && W(machine, 0x128) == 0 && W(machine, 0x12C) == _ownCommand &&
            W(animator, 0) == 0x6F670C && W(animator, 0x124) == _character && W(animator, 0x128) == _ownActor &&
            W(traits, 0) == 0x6F6990 && W(traits, 0x24) == _instance && W(traits, 0x18C) == _character &&
            U(_ownActor) == 0x703F80 && U(_projectileManager) == 0x6F73D8 &&
            U(0x765AD4) == _profile && U(_profile + 0x2B4) == _record && U(_localCharacter + 0x130) == _command &&
            U(_localCharacter + 0x138) == _playerTraitsPrimaryTraits && U(0x755294) == _flow && U(0x755298) == _sharedBattleTimer &&
            U(0x7552A0) == _input && U(_input + 0x18) == _command && U(_input + 0x1C) == _device &&
            U(0x755274) == _engine && U(_engine + 0x1C) == _camera,
            "Body input fixed own/primary command-controller-camera tuple changed.");
    }
    private BodyInputPose ReadBodyInputPose()
    {
        float[] Values(uint address, int count)
        {
            byte[] bytes = Read(address, checked(count * 4)); float[] values = Enumerable.Range(0, count).Select(i => BitConverter.ToSingle(bytes, i * 4)).ToArray();
            Require(values.All(value => float.IsFinite(value) && MathF.Abs(value) <= 10000000), "Nonfinite/out-of-bound native body pose."); return values;
        }
        float[] rates = Values(_move + 0x1A0, 7);
        return new(Values(_ownRoot + 0x74, 3), Values(_ownRoot + 0x40, 9), rates[0], rates[4], rates[5], rates[6], U(_machine + 0x130), U(_machine + 0x128));
    }
    private void BodyInputNext(BodyInputPhase phase) { _bodyInputPhase = phase; _bodyInputPhaseWorlds = 0; }
    private void BodyInputFail(string reason)
    {
        if (_bodyInputFailure.Length != 0) return;
        _bodyInputFailure = reason; _bodyInputPhase = BodyInputPhase.Failed; _bodyInputIntervalPending = false;
        _bodyInputController?.Disconnect();
        _log(new { @event = "template-body-input-source-failed", reason, nativeTransformOrTickRepair = false, neutralCleanupStillRequired = true });
    }
    private void BodyInputWorld(uint tid, in Win32Native.X86Context context) => Require(_host.IsAtDebugStop &&
        tid == _host.MainThreadId && context.Eip == NativeCallDispatcher.WorldReturn, "Body source requires the original main-thread WorldRET.");
    private static float BodyInputAngle(float angle)
    {
        Require(float.IsFinite(angle), "Body input angle is nonfinite.");
        return MathF.Abs(angle) <= MathF.PI ? angle : MathF.Atan2(MathF.Sin(angle), MathF.Cos(angle));
    }
    private float BodyInputHeadingError(float heading) => MathF.Abs(MathF.Atan2(MathF.Sin(_bodyInputReference + MathF.PI / 2 - heading), MathF.Cos(_bodyInputReference + MathF.PI / 2 - heading)));
    private static double BodyInputHorizontalDistance(float[] a, float[] b) => Math.Sqrt(Math.Pow((double)b[0] - a[0], 2) + Math.Pow((double)b[2] - a[2], 2));
}
