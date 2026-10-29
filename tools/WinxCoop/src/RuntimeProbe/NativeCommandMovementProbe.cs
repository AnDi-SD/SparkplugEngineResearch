using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>Our bounded private-command experiment, not a replacement movement implementation.</summary>
internal sealed class NativeCommandMovementProbe(
    DebuggerHost host, uint instance, uint root, uint character, uint move, uint command,
    Action verifyOwnership, Func<uint, int, byte[]> read, Func<uint, uint> word,
    Action<uint, string, BreakpointCallback> hook, Action<uint> verifyCode, Action<object> log, bool quarterTurn = false, bool jumpProbe = false)
{
    private const uint Baseline = 12, WalkEnd = 18, JumpHeldEnd = 16;
    private uint Limit => jumpProbe ? 136u : 30u;
    private uint EventLimit => jumpProbe ? 4096u : 512u;
    private bool _active, _started, _moveInFlight, _resetInFlight, _flagsCall, _flagsGate, _magnitudeRead;
    private uint _completed, _entryOrdinal, _intentUpdate = 1, _magnitudeBits, _headingBits, _directionBits;
    private uint _moveEsp, _moveReturn, _resetEsp, _resetReturn, _walkReads, _walkBranches, _characters;
    private uint _writes, _resets, _flagOrdinal, _gateOrdinal;
    private bool _ownPreRetSeen;
    private uint _ownPreRetAddress, _ownPreRetCount, _ignoredSharedReturns;
    private uint _turnYawReads;
    private double? _initialTurnError, _lastTurnError;
    private float[]? _turnStartBasis, _turnFinishBasis;
    private NativePhysicsObservationProbe? _physics;
    private uint _jumpMachine, _builderEsp, _helperEsp, _heldUpdates, _landingStable;
    private uint _builderCalls, _helperCalls, _helperReturns, _builderReturns;
    private bool _builderInFlight, _helperInFlight, _jumpConsumedThisIntent, _jumpBaselineQualified, _sawState18, _sawRise, _sawFall;
    private byte _requestedJump;
    private float[]? _jumpBaseline;
    private float[]? _lastJumpWorld;
    private float _peakY;
    private readonly HashSet<uint> _heldConsumerUpdates = [];
    private string _failure = "";
    private float[]? _walkStart, _walkFinish;
    private object? _lastPose;

    internal bool Active => _active;
    internal bool Finished => _completed == Limit || _failure.Length != 0 || (jumpProbe && JumpPassed);
    internal bool Passed => jumpProbe ? JumpPassed : _completed == Limit && _failure.Length == 0 && _walkReads == 6 && _walkBranches == 6 && HorizontalDisplacement > 0.01 && (!quarterTurn || TurnPassed);
    private bool JumpPassed => _failure.Length == 0 && _jumpBaselineQualified && _heldUpdates == 4 && _heldConsumerUpdates.Count != 0 &&
        _sawState18 && _sawRise && _sawFall && _landingStable >= 6 && _completed >= JumpHeldEnd + 6 && _requestedJump == 0;
    private double BasisChange => _turnStartBasis is null || _turnFinishBasis is null ? 0 :
        Enumerable.Range(0, 9).Max(i => Math.Abs((double)_turnFinishBasis[i] - _turnStartBasis[i]));
    private bool TurnPassed => _turnYawReads == 6 && _initialTurnError is > 0.1 && _lastTurnError.HasValue &&
        _lastTurnError.Value < _initialTurnError.Value - 0.001 && BasisChange > 0.001;
    private double HorizontalDisplacement => _walkStart is null || _walkFinish is null ? 0 :
        Math.Sqrt(Math.Pow((double)_walkFinish[0] - _walkStart[0], 2) + Math.Pow((double)_walkFinish[2] - _walkStart[2], 2));
    internal object Summary => new { active = _active, started = _started, completedOwnUpdates = _completed,
        requiredOwnUpdates = Limit, baselineUpdates = Baseline, walkUpdates = jumpProbe ? 0 : 6, neutralAfterUpdates = jumpProbe ? 120 : 12,
        walkNativeMagnitudeReads = _walkReads, walkNativeBranches = _walkBranches,
        commandWrites = _writes, ownResets = _resets, eventOrdinals = _entryOrdinal,
        ownUpdatePreRetWitnesses = _ownPreRetCount, ignoredNestedSharedNotifyReturns = _ignoredSharedReturns,
        quarterTurn, nativeYawReads = _turnYawReads, initialTurnError = _initialTurnError, lastTurnError = _lastTurnError,
        basisChange = BasisChange, initialBasis = _turnStartBasis, finalBasis = _turnFinishBasis, nativeTurnPassed = quarterTurn && TurnPassed,
        failure = _failure, passed = Passed, horizontalWalkDisplacement = HorizontalDisplacement,
        walkStartWorld = _walkStart, walkFinishWorld = _walkFinish, lastPose = _lastPose,
        jumpProbe, jumpBaselineQualified = _jumpBaselineQualified, heldOwnUpdates = _heldUpdates, heldConsumerUpdates = _heldConsumerUpdates.Order().ToArray(),
        observedState18 = _sawState18, observedRise = _sawRise, observedFall = _sawFall, landingStableUpdates = _landingStable,
        jumpBaselineWorld = _jumpBaseline, peakY = _peakY, ownBloomBuilderCalls = _builderCalls, ownKind0HelperCalls = _helperCalls,
        ownKind0HelperReturns = _helperReturns, ownBloomBuilderReturns = _builderReturns, pendingBuilder = _builderInFlight, pendingHelper = _helperInFlight,
        nativeJumpQualified = jumpProbe && JumpPassed, nativePhysicsObservation = _physics?.Summary,
        groundingOrCollisionQualified = false, rawPositionOrMotionFlagWrites = 0 };

    internal void Start()
    {
        Require(!_started && !_active, "Private command probe started twice.");
        Require(!(jumpProbe && quarterTurn), "Jump and quarter-turn are separate bounded experiments.");
        verifyOwnership();
        byte[] initial = ReadCommand();
        Require(word(command) == 0x703090 && word(command + 4) == 0 && initial[0x1A] == 0,
            "Private command probe requires the native initial neutral command.");
        _directionBits = word(command + 8); _headingBits = word(command + 0x14);
        _started = true; _active = true;
        if (jumpProbe)
        {
            _jumpMachine = word(character + 0x124);
            Require(word(_jumpMachine) == 0x6FA288 && word(_jumpMachine + 0x128) == 0 && word(_jumpMachine + 0x12C) == command,
                "Jump requires the actual owned Bloom kind0/private command.");
            if (word(_jumpMachine + 0x130) != 0) Reject("Initial native Bloom state is not0; no jump will be submitted.");
            InstallJumpWitnesses();
            _physics = new(host, root, move, () => _active && _moveInFlight, () => _intentUpdate,
                verifyOwnership, read, word, hook, log);
            _physics.Install();
        }
        hook(0x4F4250, "OwnCommand.Character.tick", (uint tid, uint at, ref Win32Native.X86Context ctx) =>
        {
            if (!_active || ctx.Ecx != character) return;
            verifyOwnership(); ++_characters; CheckIntent("character-angle-consumer", ReadCommand());
            Emit("own-command-character-consumer", new { receiver = Hex(ctx.Ecx), command = Hex(command), intentUpdate = _intentUpdate });
        });
        hook(0x5992E0, "OwnCommand.reset", (uint tid, uint at, ref Win32Native.X86Context ctx) =>
        {
            if (!_active || ctx.Ecx != command) return;
            verifyOwnership(); Require(!_resetInFlight, "Nested own private-command reset is unqualified.");
            _resetEsp = ctx.Esp; _resetReturn = ReturnAddress(ctx.Esp); _resetInFlight = true; ++_resets;
            Emit("own-command-reset-entry", new { intentUpdate = _intentUpdate, commandHex = Convert.ToHexString(ReadCommand()) });
            host.Breakpoints.AddReturn(_resetReturn, tid, "OwnCommand.reset.return",
                (uint rt, uint ra, ref Win32Native.X86Context rc) =>
                {
                    Emit("own-command-reset-return-hit", new { actualThread = rt, expectedThread = host.MainThreadId,
                        actualAddress = Hex(ra), actualEip = Hex(rc.Eip), expectedCaller = Hex(_resetReturn),
                        actualEsp = Hex(rc.Esp), expectedEsp = Hex(checked(_resetEsp + 4)), actualEsi = Hex(rc.Esi), actualEcx = Hex(rc.Ecx),
                        active = _active, resetInFlight = _resetInFlight });
                    Require(_active && _resetInFlight && rt == host.MainThreadId && ra == _resetReturn && rc.Eip == ra && rc.Esp == checked(_resetEsp + 4),
                        "Own reset return thread/caller/ESP pairing failed.");
                    verifyOwnership(); _resetInFlight = false;
                    byte[] after = ReadCommand();
                    if ((jumpProbe && !_jumpConsumedThisIntent) || (!jumpProbe && _completed < _intentUpdate)) CheckIntent("after-native-reset-before-consumption", after);
                    Emit("own-command-reset-return", new { intentUpdate = _intentUpdate, commandHex = Convert.ToHexString(after), afterPairedMoveReturn = _completed == _intentUpdate });
                });
        });
        hook(0x4F6710, "OwnCommand.motion-flags", (uint tid, uint at, ref Win32Native.X86Context ctx) =>
        {
            if (!_active || ctx.Ecx != move) return;
            verifyOwnership(); Require(_moveInFlight, "Own flags helper ran outside its paired Move update.");
            _flagsCall = true; _flagOrdinal = Emit("own-command-flags-entry", new { update = _intentUpdate, move = Hex(move) });
        });
        hook(0x4F911C, "OwnCommand.motion-flags-continuation", (uint tid, uint at, ref Win32Native.X86Context ctx) =>
        {
            if (!_active || ctx.Esi != move) return;
            verifyOwnership(); Require(_moveInFlight, "Own flags continuation has no paired Move update.");
            uint flags = word(move + 0x1DC); _flagsGate = _flagsCall && (flags & 1) != 0;
            _gateOrdinal = Emit("own-command-flags-result", new { update = _intentUpdate, receiverEsi = Hex(ctx.Esi), flags = Hex(flags), mask01 = _flagsGate, flagsEntryOrdinal = _flagOrdinal });
        });
        hook(0x4F4A65, "OwnCommand.native-magnitude-read", (uint tid, uint at, ref Win32Native.X86Context ctx) =>
        {
            if (!_active || ctx.Esi != move) return;
            verifyOwnership(); Require(_moveInFlight && ctx.Ecx == command, "Native magnitude load does not address the own command/Move pair.");
            byte[] actual = ReadCommand(); CheckIntent("native-fld-command04", actual);
            if (_magnitudeRead) Reject("More than one native magnitude-consumer entry in one own update.");
            _magnitudeRead = true;
            float delta = Float(move + 0x128);
            Require(float.IsFinite(delta) && delta >= 0, "Native consumed delta is nonfinite/negative.");
            if (_magnitudeBits == 0x3F800000)
            {
                ++_walkReads;
                if (!_flagsGate || !(delta > 0)) Reject("Walk did not reach a selected native locomotion branch with fresh positive dt.");
            }
            Emit("own-command-native-magnitude-read", new { update = _intentUpdate, receiverEsi = Hex(ctx.Esi), commandEcx = Hex(ctx.Ecx),
                nextInstruction = "fld dword ptr [ecx+4]", commandHex = Convert.ToHexString(actual), delta,
                deltaObservedAfterNativeStore = true, flagsGateOrdinal = _gateOrdinal, readOnlyAtLiveX87Stack = true });
        });
        if (quarterTurn)
            hook(0x4F4B00, "OwnCommand.native-wrapped-yaw", (uint tid, uint at, ref Win32Native.X86Context ctx) =>
            {
                if (!_active || ctx.Esi != move) return;
                verifyOwnership(); Require(_moveInFlight && _magnitudeRead && ctx.Edi == checked(move + 0x164),
                    "Native wrapped-yaw checkpoint lost its own Move/target pairing.");
                CheckIntent("native-wrapped-yaw", ReadCommand());
                float target = Float(move + 0x164), heading = Float(move + 0x1A0);
                Require(float.IsFinite(target) && float.IsFinite(heading), "Native yaw/current heading is nonfinite.");
                float requestedSum = Float(command + 0x14) + Float(command + 8);
                double targetError = Math.Abs(WrappedDifference(target, requestedSum)), headingError = Math.Abs(WrappedDifference(target, heading));
                if (_magnitudeBits == 0x3F800000)
                {
                    ++_turnYawReads; _initialTurnError ??= headingError; _lastTurnError = headingError;
                    if (targetError > 0.0001) Reject("Original wrapped target does not match the submitted polar angle.");
                }
                Emit("own-command-native-yaw", new { update = _intentUpdate, receiverEsi = Hex(ctx.Esi), targetPointerEdi = Hex(ctx.Edi),
                    target, heading, requestedSum, targetError, headingError, nativeWrapAlreadyReturned = true,
                    diagnosticAngleComparisonOnly = true, rawOrientationWrites = false });
            });
        foreach (uint address in new uint[] { 0x4FAADC, 0x4FAB08 })
            hook(address, "OwnCommand.Move.actual-C3", (uint tid, uint at, ref Win32Native.X86Context ctx) =>
            {
                if (!_active || !_moveInFlight || ctx.Esp != _moveEsp) return;
                // At these C3 sites ESI/EBX were already popped. The original
                // Notify caller kept this Move in ESI before its vslot3C call.
                Emit("own-command-move-pre-ret", new { actualThread = tid, expectedThread = host.MainThreadId,
                    actualC3 = Hex(at), actualEip = Hex(ctx.Eip), actualEsp = Hex(ctx.Esp), expectedPreRetEsp = Hex(_moveEsp),
                    stackedReturn = Hex(word(ctx.Esp)), expectedCaller = Hex(_moveReturn), actualEsi = Hex(ctx.Esi), expectedMove = Hex(move) });
                Require(tid == host.MainThreadId && ctx.Eip == at && _moveReturn == 0x4F8A45 && ctx.Esi == move &&
                    word(ctx.Esp) == _moveReturn && !_ownPreRetSeen, "Own update C3/caller/receiver/stack witness failed.");
                _ownPreRetSeen = true; _ownPreRetAddress = at; ++_ownPreRetCount;
            });
        Emit("own-command-probe-start", new { instance = Hex(instance), root = Hex(root), character = Hex(character), move = Hex(move),
            command = Hex(command), initialCommandHex = Convert.ToHexString(initial), initialIntentSource = "native-neutral-no-write",
            phaseCounts = jumpProbe ? new[] { 12, 4, 120 } : new[] { 12, 6, 12 }, noNativeResetCall = true, noInputOrCameraSingletonWrites = true });
    }

    internal byte[] ReadQualifiedCommand()
    {
        verifyOwnership(); byte[] actual = ReadCommand();
        if (_active && ((jumpProbe && !_jumpConsumedThisIntent) || (!jumpProbe && _completed < _intentUpdate))) CheckIntent("owned-state-guard-before-consumption", actual);
        return actual;
    }

    internal void ObserveMoveEntry(uint tid, ref Win32Native.X86Context ctx)
    {
        if (!_active) return;
        verifyOwnership(); Require(ctx.Ecx == move && !_moveInFlight && _completed < Limit && _intentUpdate == _completed + 1,
            "Own Move entry identity/ordinal/nesting failed.");
        CheckIntent("own-move-entry", ReadCommand());
        _moveEsp = ctx.Esp; _moveReturn = ReturnAddress(ctx.Esp); _moveInFlight = true;
        _ownPreRetSeen = false; _ownPreRetAddress = 0;
        _flagsCall = _flagsGate = _magnitudeRead = false; _flagOrdinal = _gateOrdinal = 0;
        float[] before = Position(root + 0x20);
        Emit("own-command-move-entry", new { update = _intentUpdate, originalEsp = Hex(ctx.Esp), originalReturn = Hex(_moveReturn),
            commandHex = Convert.ToHexString(ReadCommand()), previousDeltaNotCurrentUpdate = Float(move + 0x128), localBefore = before });
        BreakpointCallback awaitOwnReturn = null!;
        awaitOwnReturn = (uint rt, uint ra, ref Win32Native.X86Context rc) =>
            {
                uint expectedEsp = checked(_moveEsp + 4);
                bool matched = _active && _moveInFlight && rt == host.MainThreadId && ra == _moveReturn && rc.Eip == ra &&
                    rc.Esp == expectedEsp && rc.Esi == move && _ownPreRetSeen;
                bool nestedCandidate = !matched && _active && _moveInFlight && !_ownPreRetSeen && rt == host.MainThreadId &&
                    ra == _moveReturn && rc.Eip == ra && _moveReturn == 0x4F8A45 && rc.Esp >= 0x10000 && rc.Esp < expectedEsp &&
                    expectedEsp - rc.Esp <= 65536;
                uint? receiverTable = null, messageAddress = null, messageCode = null; string? readError = null;
                if (nestedCandidate)
                {
                    try
                    {
                        receiverTable = word(rc.Esi);
                        // Notify4F8A00 subESP2C/pushESI/pushEDI: its message
                        // argument is at this shared epilogue's ESP+38.
                        messageAddress = word(checked(rc.Esp + 0x38)); messageCode = word(messageAddress.Value);
                    }
                    catch (Exception error) { readError = error.GetType().Name; }
                }
                bool ignored = nestedCandidate && receiverTable == 0x6F64F8 && messageCode.HasValue;
                Emit("own-command-move-return-hit", new { actualThread = rt, expectedThread = host.MainThreadId,
                    actualAddress = Hex(ra), actualEip = Hex(rc.Eip), expectedCaller = Hex(_moveReturn),
                    actualEsp = Hex(rc.Esp), expectedPostRetEsp = Hex(expectedEsp), expectedPreRetEsp = Hex(_moveEsp),
                    actualEsi = Hex(rc.Esi), expectedMove = Hex(move), actualEcx = Hex(rc.Ecx),
                    active = _active, moveInFlight = _moveInFlight, ownC3Seen = _ownPreRetSeen, ownC3 = Hex(_ownPreRetAddress),
                    matched, ignoredKnownNestedNotify = ignored, receiverTable, messageAddress, messageCode, readError });
                if (ignored)
                {
                    Require(++_ignoredSharedReturns <= 64, "Nested shared Notify return observation budget exceeded.");
                    verifyOwnership();
                    // Core consumes a return probe by thread alone. Re-add this
                    // SAME pending probe; its exact own post-RET gate is intact.
                    host.Breakpoints.AddReturn(_moveReturn, tid, "OwnCommand.Move.return", awaitOwnReturn);
                    return;
                }
                Require(matched, "Own Move return C3/thread/caller/receiver/ESP pairing failed.");
                verifyOwnership();
                float[] local = Position(root + 0x20), world = Position(root + 0x74), current = Position(move + 0x12C), previous = Position(move + 0x138);
                double step = Math.Sqrt(Enumerable.Range(0, 3).Sum(i => Math.Pow((double)local[i] - before[i], 2)));
                Require(step <= 1000, "Own movement exceeded the explicit per-update diagnostic displacement budget.");
                if (!read(root + 0x20, 12).SequenceEqual(read(move + 0x12C, 12)) || (word(root + 0xB0) & 1) != 0)
                    Reject("Native Move return did not provide matching current/root and clean world caches.");
                if (_magnitudeBits == 0x3F800000)
                {
                    if (_flagsGate && _magnitudeRead) ++_walkBranches;
                    else Reject("Walk update returned before the required original locomotion consumer/branch.");
                }
                float delta = Float(move + 0x128), forward = Float(move + 0x1B0), vertical = Float(move + 0x1B4), lateral = Float(move + 0x1B8),
                    heading = Float(move + 0x1A0), target = Float(move + 0x164);
                Require(new[] { delta, forward, vertical, lateral, heading, target }.All(float.IsFinite), "Own native rates/headings are nonfinite.");
                _physics?.VerifyNoPending();
                if (jumpProbe) ObserveJumpResponse(world, vertical);
                ++_completed; _moveInFlight = false;
                _lastPose = new { local, world, current, previous, delta, forwardRate = forward,
                    verticalRate = vertical, lateralRate = lateral, flags = Hex(word(move + 0x1DC)),
                    currentHeading = heading, targetHeading = target, platform1E4 = Hex(word(move + 0x1E4)),
                    rootLocalOrientation = quarterTurn ? Orientation() : null };
                Emit("own-command-move-return", new { update = _completed, requestedMagnitudeBits = Hex(_magnitudeBits),
                    flagsCall = _flagsCall, flagsGate = _flagsGate, nativeMagnitudeRead = _magnitudeRead, localStep = step, pose = _lastPose,
                    finalPoseFromOriginalMoveReturn = true, returnEaxMeaningAssigned = false });
            };
        host.Breakpoints.AddReturn(_moveReturn, tid, "OwnCommand.Move.return", awaitOwnReturn);
    }

    internal void EndWorld(uint originalWorldReturns)
    {
        verifyOwnership(); Require(_active && !_moveInFlight && !_resetInFlight && _completed == originalWorldReturns,
            "World boundary did not pair exactly one own Move completion.");
        if (_characters != 1) Reject("World interval did not contain exactly one own Character tick consumer.");
        _characters = 0;
        Emit("own-command-end-world", new { completedOwnUpdates = _completed, commandHex = Convert.ToHexString(ReadCommand()),
            nativePostConsumptionMutationsRecordedNotReplayed = true });
        if (jumpProbe) { EndJumpWorld(); return; }
        if (_completed == Baseline)
        {
            _walkStart = Position(root + 0x74); _headingBits = word(move + 0x1A0); _directionBits = quarterTurn ? 0x3FC90FDBu : 0;
            if (quarterTurn) _turnStartBasis = Orientation();
            Require(float.IsFinite(BitConverter.Int32BitsToSingle(unchecked((int)_headingBits))) && MathF.Abs(Float(move + 0x1A0)) <= 10,
                "Own native heading is outside the finite diagnostic range.");
        }
        if (_completed == WalkEnd)
        {
            _walkFinish = Position(root + 0x74);
            if (quarterTurn) _turnFinishBasis = Orientation();
            _directionBits = 0;
        }
        if (_completed == Limit && (_walkReads != 6 || _walkBranches != 6 || HorizontalDisplacement <= 0.01))
            Reject("The bounded walk did not establish six actual native consumptions/branches and horizontal displacement.");
        if (_completed == Limit && quarterTurn && !TurnPassed)
            Reject("Native quarter-turn did not establish six yaw reads, heading-error progress and a changed finite basis.");
        if (Finished) return;
        _intentUpdate = _completed + 1;
        Submit(_completed is >= Baseline and < WalkEnd ? 0x3F800000u : 0u, "next-original-world");
    }

    internal void PrepareCleanup()
    {
        verifyOwnership(); Require(_active && !_moveInFlight && !_resetInFlight, "Cannot neutralize/delete during an own native call.");
        Require(!_builderInFlight && !_helperInFlight, "Cannot clean up within the native jump builder window.");
        _physics?.VerifyNoPending(); _requestedJump = 0;
        _directionBits = 0;
        Submit(0, "neutral-before-native-delete");
        _active = false;
        Emit("own-command-probe-finished", new { passed = Passed, completedOwnUpdates = _completed, failure = _failure,
            horizontalWalkDisplacement = HorizontalDisplacement, nativeCleanupStillRequired = true });
    }

    private void Submit(uint magnitude, string boundary)
    {
        verifyOwnership(); Require(host.IsAtDebugStop && !_moveInFlight && !_resetInFlight, "Private input writes require a safe original-world boundary.");
        byte[] before = ReadCommand(); _magnitudeBits = magnitude;
        foreach ((uint offset, uint bits) in new[] { (4u, magnitude), (8u, _directionBits), (0x14u, _headingBits) })
            Win32Native.WriteMemory(host.ProcessHandle, checked(command + offset), BitConverter.GetBytes(bits), executable: false);
        if (jumpProbe) Win32Native.WriteMemory(host.ProcessHandle, command + 0x1A, [_requestedJump], executable: false);
        byte[] after = ReadCommand();
        Require(Enumerable.Range(0, before.Length).All(i => (i is >= 4 and < 12 or >= 0x14 and < 0x18) || (jumpProbe && i == 0x1A) || before[i] == after[i]) &&
            BitConverter.ToUInt32(after, 4) == magnitude && BitConverter.ToUInt32(after, 8) == _directionBits && BitConverter.ToUInt32(after, 0x14) == _headingBits &&
            (!jumpProbe || after[0x1A] == _requestedJump),
            "Private intent readback changed bytes outside its three exact scalar fields.");
        ++_writes;
        if (jumpProbe) _jumpConsumedThisIntent = false;
        Emit("own-command-intent-write", new { boundary, nextOwnUpdate = _intentUpdate, magnitudeBits = Hex(magnitude),
            relativeDirectionBits = Hex(_directionBits), referenceHeadingBits = Hex(_headingBits),
            commandHexBefore = Convert.ToHexString(before), commandHexAfter = Convert.ToHexString(after), addressedBytes = jumpProbe ? 13 : 12,
            outsideAddressedFieldsPreserved = true, jumpByteWritten = jumpProbe, requestedJump = _requestedJump, rawNodeOrMotionFlagsWritten = false });
    }

    private void InstallJumpWitnesses()
    {
        hook(0x5268B0, "OwnJump.Bloom.command-builder", (uint tid, uint at, ref Win32Native.X86Context ctx) =>
        {
            if (!_active || ctx.Ecx != _jumpMachine) return;
            verifyOwnership();
            Require(!_builderInFlight && !_helperInFlight && word(_jumpMachine + 0x128) == 0 && word(ctx.Esp) == 0x4FB11C,
                "Own kind0 Bloom builder caller/nesting is unqualified.");
            CheckIntent("before-own-Bloom-builder", ReadCommand());
            _builderEsp = ctx.Esp; _builderInFlight = true; ++_builderCalls;
            Emit("own-jump-builder-entry", new { update = _intentUpdate, entryEsp = Hex(ctx.Esp), stackedCaller = Hex(word(ctx.Esp)), snapshot = JumpSnapshot() });
        });
        hook(0x524AA0, "OwnJump.Bloom.kind0-native-helper", (uint tid, uint at, ref Win32Native.X86Context ctx) =>
        {
            if (!_active || ctx.Ecx != _jumpMachine) return;
            verifyOwnership();
            Require(_builderInFlight && !_helperInFlight && ctx.Esi == _jumpMachine && word(_jumpMachine + 0x128) == 0 && word(ctx.Esp) == 0x526977,
                "Own native kind0 helper caller/receiver/frame is unqualified.");
            byte[] actual = ReadCommand(); CheckIntent("before-own-kind0-native-helper", actual);
            bool intact = BitConverter.ToUInt32(actual, 4) == _magnitudeBits && BitConverter.ToUInt32(actual, 8) == _directionBits &&
                BitConverter.ToUInt32(actual, 0x14) == _headingBits && actual[0x1A] == _requestedJump;
            if (intact)
            {
                _jumpConsumedThisIntent = true;
                if (_requestedJump == 1 && _intentUpdate is > Baseline and <= JumpHeldEnd) _heldConsumerUpdates.Add(_intentUpdate);
            }
            _helperEsp = ctx.Esp; _helperInFlight = true; ++_helperCalls;
            uint localCommand = word(word(0x7552A0) + 0x18);
            Require(localCommand != command, "Own jump command aliases the local UserInput.");
            Emit("own-jump-kind0-consumption-window", new { update = _intentUpdate, entryEsp = Hex(ctx.Esp), stackedCaller = Hex(word(ctx.Esp)),
                intactBeforeNativeWindow = intact, requestedJump = _requestedJump, actualJump = actual[0x1A], localInputJump = read(localCommand + 0x1A, 1)[0],
                snapshot = JumpSnapshot(), protectedNativeHelper = true, exactInternal1AReadInstructionClaimed = false });
        });
        hook(0x526977, "OwnJump.Bloom.kind0-helper-return", (uint tid, uint at, ref Win32Native.X86Context ctx) =>
        {
            if (!_active || ctx.Esi != _jumpMachine) return;
            verifyOwnership();
            Emit("own-jump-kind0-return-hit", new { update = _intentUpdate, actualEsp = Hex(ctx.Esp), expectedEsp = Hex(checked(_helperEsp + 4)),
                pending = _helperInFlight, snapshot = JumpSnapshot(), returnEaxMeaningAssigned = false });
            Require(_builderInFlight && _helperInFlight && ctx.Esp == checked(_helperEsp + 4), "Own kind0 helper return pairing failed.");
            _helperInFlight = false; ++_helperReturns; CheckIntent("after-own-kind0-native-helper", ReadCommand());
        });
        hook(0x4FB11C, "OwnJump.Bloom.command-builder-return", (uint tid, uint at, ref Win32Native.X86Context ctx) =>
        {
            if (!_active || ctx.Esi != _jumpMachine) return;
            verifyOwnership();
            Emit("own-jump-builder-return-hit", new { update = _intentUpdate, actualEsp = Hex(ctx.Esp), expectedEsp = Hex(checked(_builderEsp + 4)),
                pendingBuilder = _builderInFlight, pendingHelper = _helperInFlight, snapshot = JumpSnapshot(), returnEaxMeaningAssigned = false });
            Require(_builderInFlight && !_helperInFlight && ctx.Esp == checked(_builderEsp + 4), "Own Bloom builder return pairing failed.");
            _builderInFlight = false; ++_builderReturns; CheckIntent("after-own-Bloom-builder", ReadCommand());
        });
    }

    private object JumpSnapshot() => new { machine = Hex(_jumpMachine), kind128 = word(_jumpMachine + 0x128),
        currentState130 = word(_jumpMachine + 0x130), packed140 = Hex(word(_jumpMachine + 0x140)), proposed144 = Hex(word(_jumpMachine + 0x144)),
        commandHex = Convert.ToHexString(ReadCommand()) };

    private void ObserveJumpResponse(float[] world, float vertical)
    {
        uint state = word(_jumpMachine + 0x130);
        Require(word(_jumpMachine + 0x128) == 0, "Owned Bloom kind changed during jump observation.");
        _jumpBaseline ??= (float[])world.Clone();
        if (_intentUpdate <= Baseline)
        {
            bool settled = state == 0 && MathF.Abs(vertical) <= 0.01f && MathF.Abs(Float(move + 0x1B0)) <= 0.01f && MathF.Abs(Float(move + 0x1B8)) <= 0.01f &&
                Enumerable.Range(0, 3).All(i => MathF.Abs(world[i] - _jumpBaseline[i]) <= 0.1f);
            if (!settled) Reject("Neutral native kind0/state0/finite settled baseline did not qualify; no held jump will be submitted.");
            _peakY = world[1];
        }
        else
        {
            if (_intentUpdate <= JumpHeldEnd && _requestedJump == 1) ++_heldUpdates;
            _sawState18 |= state == 18;
            _peakY = MathF.Max(_peakY, world[1]);
            _sawRise |= world[1] > _jumpBaseline[1] + 5 && vertical > 0.1f;
            _sawFall |= _sawRise && vertical < -0.1f;
            bool landed = _intentUpdate > JumpHeldEnd && _requestedJump == 0 && _sawRise && _sawFall && state == 0 &&
                MathF.Abs(world[1] - _jumpBaseline[1]) <= 2 && MathF.Abs(vertical) <= 0.01f &&
                MathF.Abs(Float(move + 0x1B0)) <= 0.01f && MathF.Abs(Float(move + 0x1B8)) <= 0.01f &&
                _lastJumpWorld is not null && Enumerable.Range(0, 3).All(i => MathF.Abs(world[i] - _lastJumpWorld[i]) <= 0.1f);
            _landingStable = landed ? _landingStable + 1 : 0;
        }
        _lastJumpWorld = (float[])world.Clone();
        Emit("own-jump-native-response", new { update = _intentUpdate, requestedJump = _requestedJump, world, verticalRate = vertical,
            state130 = state, peakY = _peakY, observed18 = _sawState18, observedRise = _sawRise, observedFall = _sawFall,
            landingStableUpdates = _landingStable, landingHeightTolerance = 2, diagnosticBoundsNotGameConstants = true });
    }

    private void EndJumpWorld()
    {
        Require(!_builderInFlight && !_helperInFlight, "World boundary reached with an unpaired jump builder/helper.");
        if (_completed == Baseline && _failure.Length == 0)
        {
            _jumpBaselineQualified = word(_jumpMachine + 0x128) == 0 && word(_jumpMachine + 0x130) == 0;
            if (!_jumpBaselineQualified) Reject("Native neutral state0/kind0 was lost before jump submission.");
        }
        if (_completed == Limit && !JumpPassed) Reject("No qualified native state18/rise/fall/landing within the bounded jump window.");
        if (Finished) return;
        Require(_completed < Baseline || _jumpBaselineQualified, "Jump submission attempted without a qualified neutral baseline.");
        _requestedJump = _completed is >= Baseline and < JumpHeldEnd ? (byte)1 : (byte)0;
        _directionBits = 0; _headingBits = 0; _intentUpdate = _completed + 1;
        Submit(0, "next-original-world-jump-held-level");
    }

    private byte[] ReadCommand()
    {
        byte[] bytes = read(command, 0x74);
        Require(BitConverter.ToUInt32(bytes, 0) == 0x703090 && new[] { 4, 8, 12, 16, 20 }.All(i => float.IsFinite(BitConverter.ToSingle(bytes, i))),
            "Own command identity or known floating-point fields are invalid.");
        return bytes;
    }
    private void CheckIntent(string boundary, byte[] bytes)
    {
        if (BitConverter.ToUInt32(bytes, 4) != _magnitudeBits || BitConverter.ToUInt32(bytes, 8) != _directionBits ||
            BitConverter.ToUInt32(bytes, 0x14) != _headingBits || bytes[0x1A] != (jumpProbe ? _requestedJump : (byte)0))
        {
            if (jumpProbe && _jumpConsumedThisIntent)
                Emit("own-command-after-jump-consumer-mutation", new { boundary, update = _intentUpdate, commandHex = Convert.ToHexString(bytes), replayed = false });
            else Reject("Private intent changed before/at native consumption: " + boundary);
        }
    }
    private uint ReturnAddress(uint esp)
    {
        Require(esp >= 0x10000 && esp <= uint.MaxValue - 4, "Invalid own native entry stack.");
        uint address = word(esp); Require(address is >= 0x401000 and < 0x6DA000, "Own native return is outside the pinned original code image.");
        verifyCode(address); return address;
    }
    private float[] Position(uint address)
    {
        byte[] bytes = read(address, 12); float[] values = Enumerable.Range(0, 3).Select(i => BitConverter.ToSingle(bytes, i * 4)).ToArray();
        Require(values.All(v => float.IsFinite(v) && MathF.Abs(v) <= 10000000), "Own native pose/cache is outside the finite diagnostic bound.");
        return values;
    }
    private float Float(uint address) => BitConverter.ToSingle(read(address, 4));
    private float[] Orientation()
    {
        byte[] bytes = read(root + 0x40, 36); float[] basis = Enumerable.Range(0, 9).Select(i => BitConverter.ToSingle(bytes, i * 4)).ToArray();
        Require(basis.All(v => float.IsFinite(v) && MathF.Abs(v) <= 4), "Native local orientation is outside its finite diagnostic bound.");
        return basis;
    }
    private static double WrappedDifference(double a, double b) => Math.Atan2(Math.Sin(a - b), Math.Cos(a - b));
    private void Reject(string reason)
    {
        if (_failure.Length != 0) return;
        _failure = reason; Emit("own-command-probe-rejected", new { reason, intentUpdate = _intentUpdate, completedOwnUpdates = _completed,
            neutralCleanupAtNextSafeWorldRequired = true, requestWillNotBeReplayed = true });
    }
    private uint Emit(string name, object data)
    {
        Require(++_entryOrdinal <= EventLimit, "Private command observation cap exceeded.");
        log(new { @event = name, ordinal = _entryOrdinal, data }); return _entryOrdinal;
    }
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidDataException(message); }
    private static string Hex(uint value) => $"0x{value:X8}";
}
