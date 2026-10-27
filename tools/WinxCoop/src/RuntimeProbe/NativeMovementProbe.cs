using System.ComponentModel;
using System.Reflection.PortableExecutable;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// One native reset on the already owned local movement controller. This is
/// an isolated diagnostic, not a warp or a server reconciliation backend.
/// </summary>
internal sealed class NativeMovementProbe(PEReader image, Action<object> log)
{
    private const uint Reset = 0x4D96A0;
    private const int MoveStart = 0x128, MoveBytes = 0x2D4 - MoveStart;
    private const int SettleFrames = 12, ObserveFrames = 120, SnapshotLimit = 256;
    private const float RateTolerance = 0.001f, PositionTolerance = 0.0001f, BasisTolerance = 0.00001f;
    // Original XOR EAX,EAX; JMP [013B125C]. No breakpoint is put on this entry.
    private static readonly byte[] ResetSignature = Convert.FromHexString("33C0FF255C123B01");
    private static readonly int[] RateOffsets = [0x188, 0x18C, 0x190, 0x1B0, 0x1B4, 0x1B8];
    private static readonly int[] PositionOffsets = [0x12C, 0x130, 0x134, 0x138, 0x13C, 0x140, 0x150, 0x154, 0x158];
    private static readonly int[] PoseOffsets = [0x20, 0x24, 0x28, 0x30, 0x34, 0x38,
        0x40, 0x44, 0x48, 0x4C, 0x50, 0x54, 0x58, 0x5C, 0x60,
        0x74, 0x78, 0x7C, 0x80, 0x84, 0x88,
        0x8C, 0x90, 0x94, 0x98, 0x9C, 0xA0, 0xA4, 0xA8, 0xAC];
    private DebuggerHost? _host;
    private IntPtr _process;
    private uint _pid;
    private int _ownerThread, _stage, _settled, _stableAfter, _snapshots;
    private uint _worldStops, _postWorldStops, _waiting, _inputBusyAfter;
    private Snapshot? _settleAnchor, _before, _afterAnchor, _lastLogged;
    private bool _callAccepted, _completed, _succeeded;

    private sealed record Identity(uint Profile, uint Record, uint Root, uint Parent, uint Scene,
        uint BodyRoot, uint Engine, uint DefaultScene, uint Camera, uint Flow, uint Active,
        uint SceneManager, uint Input, uint Input14, uint Command, uint Device,
        uint Character, uint Move, uint Bloom, uint Animator, string Components);
    private sealed record Snapshot(Identity Identity, byte[] Move, byte[] Root, byte[] Character,
        byte[] Input, byte[] Command, byte[] Move124, uint Fsm130, uint Packed140,
        uint Device14, object[] Components);

    internal bool HasBegun => _stage != 0;
    internal bool ModeSucceeded => _succeeded;
    internal uint ReadySamples { get; private set; }
    internal object Summary => new
    {
        stage = _stage, worldStops = _worldStops, postCallWorldStops = _postWorldStops,
        settleFramesRequired = SettleFrames, settledFrames = _settled,
        stablePostFramesRequired = ObserveFrames, stablePostFrames = _stableAfter,
        waitingObservations = _waiting, nonIdlePostObservations = _inputBusyAfter,
        fullSnapshots = _snapshots, fullSnapshotLimit = SnapshotLimit,
        typedAlfeaReadySamples = ReadySamples,
        nativeResetCallRequested = HasBegun, nativeResetCallBegun = _callAccepted, nativeResetCallCompleted = _completed,
        localIdentityPreserved = _completed && _postWorldStops > 0 && _succeeded,
        observationChecksPassed = _succeeded, externalPoseWrites = 0, manualControllerFieldWrites = 0,
        manualMovementOrCameraProofClaimed = false, nativeWarpQualified = false,
        independentControllerQualified = false, protectedResetBodyReconstructed = false,
        movementUpdateInvocationWitnessed = false, observationBoundary = "SceneManager WorldRET",
        postCallInputActivitySeen = _inputBusyAfter > 0
    };

    internal void Step(DebuggerHost host, uint thread, ref Win32Native.X86Context context,
        NativeCallDispatcher calls, Action resume, bool mediaVerified)
    {
        Bind(host);
        Require(thread == host.MainThreadId && context.Eip == NativeCallDispatcher.WorldReturn &&
            Read(context.Eip, 1)[0] == 0xC3 && !calls.Pending,
            "Controller reset requires the original main-thread world-return stop without a pending call.");
        ++_worldStops;
        if (_succeeded) return;
        if (!mediaVerified)
        {
            Require(!HasBegun, "Media identity was lost after the native reset began.");
            Wait("MediaPath has not been verified."); return;
        }

        Snapshot current;
        if (!HasBegun)
        {
            try { current = Capture(); }
            catch (Exception exception) when (exception is Win32Exception or InvalidDataException or OverflowException)
            { Wait(exception.Message); return; }
            if (!Idle(current, out string reason)) { Wait(reason); return; }
            if (_settleAnchor is null || current.Identity != _settleAnchor.Identity || !SamePose(current, _settleAnchor))
            { _settleAnchor = current; _settled = 1; return; }
            if (++_settled < SettleFrames) return;

            VerifySignature();
            Require(context.Esp >= 0x10100, "Original x86 stack is below the bounded reset preflight range.");
            _ = Read(context.Esp - 0x100, 0x100); // Readability only; no stack bytes are written by this probe.
            _before = current;
            Emit(current, "before-native-reset", force: true);
            _stage = 1;
            log(new { @event = "controller-reset-call-request", function = Hex(Reset),
                thisPointer = Hex(current.Identity.Move), callingConvention = "thiscall", argumentWords = 0,
                resultMeaning = "unspecified; original callers ignore EAX", externalPoseWrites = 0,
                originalTypedCallers = new[] { "004F62DE", "004F8D3A", "0051F903", "005A5A61" },
                sourceContract = "typed native init/Notify/existing-player Struggling/respawn",
                protectedEntryBodyKnown = false, entryBreakpointInstalled = false,
                idleGuard = new { fsm130 = current.Fsm130, packed140LowNibble = current.Packed140 & 15,
                    commandMagnitude04 = F(current.Command, 4), jumpRequestHeld1A = current.Command[0x1A],
                    platform1E4 = Hex(MoveWord(current, 0x1E4)), settledFrames = _settled,
                    rateOffsets = RateOffsets.Select(Offset).ToArray(), rateTolerance = RateTolerance,
                    positionTolerance = PositionTolerance, basisTolerance = BasisTolerance,
                    nativeGroundedFlagAssumed = false } });
            calls.Begin(host, thread, ref context, Reset, current.Identity.Move, [], false, result =>
            {
                _completed = true; _stage = 2;
                Snapshot returned;
                try { returned = Capture(); }
                catch (Exception exception)
                {
                    log(new { @event = "controller-reset-completion-capture-failed", resultBits = Hex(result),
                        error = exception.Message, cachedNativeMemoryReadAttempted = false, noRetry = true });
                    throw;
                }
                Emit(returned, "native-reset-completion", force: true);
                Require(returned.Identity == current.Identity, "Local identity changed during native reset.");
                Require(SamePose(returned, current), "Native reset changed local/world root PRS beyond the diagnostic tolerance.");
                Require(Idle(returned, out string idleReason), "Native reset did not preserve the bounded idle guards: " + idleReason);
                _afterAnchor = returned;
                log(new { @event = "controller-reset-returned", resultBits = Hex(result), resultMeaningAssigned = false,
                    typedIdentityPreserved = true, rootPrsWithinTolerance = true,
                    protectedPrefixOutcomeObserved = true, protectedPrefixSemanticsRecovered = false });
                resume();
            });
            _callAccepted = true;
            return;
        }

        Require(_completed && _stage == 2, "Unexpected controller-reset observation phase.");
        Snapshot before = _before ?? throw new InvalidDataException("Missing pre-reset snapshot.");
        current = Capture();
        ++_postWorldStops;
        if (current.Identity != before.Identity)
        {
            Emit(current, "later-world-identity-mismatch", force: true);
            throw new InvalidDataException("Local profile/input/camera/component/scene identity changed after native reset.");
        }
        bool idle = Idle(current, out string postReason);
        bool localInputActive = float.IsFinite(F(current.Command, 4)) &&
            (F(current.Command, 4) != 0 || current.Command[0x1A] != 0);
        if (localInputActive) ++_inputBusyAfter;
        if (!idle)
        {
            if (_inputBusyAfter == 0)
            {
                Emit(current, "later-world-idle-guard-failed", force: true);
                throw new InvalidDataException("Native reset lost its idle guards without observed local input: " + postReason);
            }
            // User input may delay this diagnostic. A new pose baseline is
            // admitted only after that observed input and native settling.
            _stableAfter = 0; _afterAnchor = null;
        }
        else
        {
            if (_afterAnchor is null) _afterAnchor = current;
            if (!SamePose(current, _afterAnchor))
            {
                Emit(current, "later-world-unexpected-pose-drift", force: true);
                throw new InvalidDataException("Idle root PRS drifted after native reset; do not rebase or accept the shifted pose.");
            }
            ++_stableAfter;
        }
        if (_postWorldStops <= 8 || _postWorldStops % 15 == 0)
            Emit(current, idle ? "later-world-stable" : "later-world-waiting-for-idle", force: false);
        if (_stableAfter < ObserveFrames) return;
        Emit(current, "controller-reset-observation-complete", force: true);
        _succeeded = true; _stage = 3;
        log(new { @event = "controller-reset-checks-completed", postCallWorldStops = _postWorldStops,
            consecutiveStableIdleWorldStops = _stableAfter, nativeResetCalls = 1,
            localIdentityPreserved = true, externalPoseWrites = 0, nativeWarpQualified = false,
            manualMovementOrCameraProofClaimed = false, completionRequestsOwnedCleanupSeparately = true });
    }

    private void Bind(DebuggerHost host)
    {
        if (_host is null)
        {
            Require(host.IsAtDebugStop && host.ImageBase == 0x400000 && host.ProcessHandle != IntPtr.Zero && host.ProcessPid != 0,
                "Controller-reset probe requires the pinned owned process at a debug stop.");
            _host = host; _process = host.ProcessHandle; _pid = host.ProcessPid;
            _ownerThread = Environment.CurrentManagedThreadId;
        }
        Require(ReferenceEquals(host, _host), "Controller-reset probe cannot change its owned debugger.");
        EnsureAtStop();
    }

    private Snapshot Capture()
    {
        uint profile = Pointer(0x765AD4); Table(profile, 0x6F5A44);
        uint record = Pointer(profile + 0x2B4); Table(record, 0x710F80);
        uint root = Pointer(record + 0x24); Table(root, 0x6DC4F4);
        byte[] rootBytes = Read(root, 0xB4);
        uint scene = Pointer(root + 0x3C); Table(scene, 0x6E7358);
        uint flow = Pointer(0x755294), level = U(flow + 0x1B0);
        int stack = unchecked((int)U(flow + 0x1AC));
        Require(level == 28 && U(flow + 0x1B8) == 0 && stack is >= 0 and <= 20,
            "The Alfea28 playing-level flow is not settled.");
        uint active = Pointer(flow + 0x15C + (uint)stack * 4);
        Require(U(active + 0x10) == 28, "Active playing state is not Alfea28.");
        uint sceneManager = Pointer(0x75DB90);
        Require(U(sceneManager + 0x20) == 0, "Scene-manager world context is still active.");
        uint head = Pointer(record + 0x1C), count = U(record + 0x20);
        Require(count == 11, "Expected exactly eleven stock local components.");
        var byTable = new Dictionary<uint, List<uint>>();
        var links = new HashSet<uint>(); var items = new HashSet<uint>();
        var componentKeys = new List<string>(); var components = new List<object>();
        uint previous = head, link = Pointer(head);
        for (uint i = 0; i < count; ++i)
        {
            Require(link != head && links.Add(link) && U(link + 4) == previous,
                "Stock player component chain is not reciprocal/distinct.");
            uint item = Pointer(link + 8), table = U(item);
            Require(items.Add(item), "Stock player component alias encountered.");
            if (!byTable.TryGetValue(table, out List<uint>? values)) byTable[table] = values = [];
            values.Add(item);
            componentKeys.Add($"{link:X8}:{item:X8}:{table:X8}");
            components.Add(new { link = Hex(link), address = Hex(item), vtable = Hex(table) });
            uint next = Pointer(link);
            Require(U(next + 4) == link, "Stock player component next/back link mismatch.");
            previous = link; link = next;
        }
        Require(link == head && U(head + 4) == previous, "Stock player component sentinel/count mismatch.");
        uint Unique(uint table) => byTable.TryGetValue(table, out List<uint>? values) && values.Count == 1 ? values[0]
            : throw new InvalidDataException($"Expected exactly one local component with table {Hex(table)}.");
        uint character = Unique(0x6F6438), move = Unique(0x6F64F8), bloom = Unique(0x6FA288), animator = Unique(0x6F670C);
        foreach (uint entity in new[] { character, move, bloom, animator })
            Require(U(entity + 0x24) == record, "Typed component owner differs from the stock player record.");
        uint input = Pointer(0x7552A0); Table(input, 0x6F5508);
        uint command = Pointer(input + 0x18); Table(command, 0x703090);
        uint device = Pointer(input + 0x1C); Table(device, 0x7116CC);
        byte[] characterBytes = Read(character + 0x124, 0x3C);
        byte[] moveBytes = Read(move + MoveStart, MoveBytes);
        Require(W(characterBytes, 0) == bloom && W(characterBytes, 4) == animator &&
            W(characterBytes, 8) == move && W(characterBytes, 0xC) == command &&
            W(moveBytes, 0x160 - MoveStart) == character && U(bloom + 0x124) == character && U(bloom + 0x12C) == command,
            "Typed Character/MoveCtrl/FSM/UserInput command ownership links do not match.");
        foreach (int offset in PoseOffsets) Require(float.IsFinite(F(rootBytes, offset)), "Root PRS contains a nonfinite word.");
        foreach (int offset in PositionOffsets.Concat(RateOffsets))
            Require(float.IsFinite(F(moveBytes, offset - MoveStart)), "Observed native position/rate word is nonfinite.");
        uint engine = Pointer(0x755274), camera = Pointer(engine + 0x1C);
        var identity = new Identity(profile, record, root, W(rootBytes, 0x2C), scene,
            U(profile + 0x18), engine, U(engine + 0x18), camera, flow, active, sceneManager,
            input, U(input + 0x14), command, device, character, move, bloom, animator,
            $"{head:X8}:" + string.Join(";", componentKeys));
        var result = new Snapshot(identity, moveBytes, rootBytes, characterBytes, Read(input + 0x14, 12),
            Read(command, 0x74), Read(move + 0x124, 4), U(bloom + 0x130), U(bloom + 0x140), U(device + 0x14), components.ToArray());
        ++ReadySamples;
        return result;
    }

    private static bool Idle(Snapshot value, out string reason)
    {
        reason = "";
        if (value.Fsm130 != 0 || (value.Packed140 & 15) != 0) reason = "Native FSM130/packed140 guards are not zero.";
        else if (W(value.Character, 0x148 - 0x124) != 0 || value.Command[0x18] != 0)
            reason = "Stock local selector148/command18 guards differ from the qualified context.";
        else if (value.Device14 != 3) reason = "Input device context is not the observed InGame value3.";
        else if (!float.IsFinite(F(value.Command, 4)) || F(value.Command, 4) != 0 || value.Command[0x1A] != 0)
            reason = "Local magnitude04 or jump request1A is active.";
        else if (value.Move124[0] != 0) reason = "Native movement update byte124 is nonzero.";
        else if (MoveWord(value, 0x1E4) != 0) reason = "Movement controller has a platform context1E4.";
        else if (RateOffsets.Any(offset => MathF.Abs(F(value.Move, offset - MoveStart)) > RateTolerance))
            reason = "Native raw rate offsets exceed the near-zero diagnostic tolerance.";
        return reason.Length == 0;
    }

    private static bool SamePose(Snapshot left, Snapshot right) => PoseOffsets.All(offset =>
        MathF.Abs(F(left.Root, offset) - F(right.Root, offset)) <=
            (offset is 0x20 or 0x24 or 0x28 or 0x74 or 0x78 or 0x7C ? PositionTolerance : BasisTolerance));

    private void VerifySignature()
    {
        byte[] source = image.GetSectionData((int)(Reset - 0x400000)).GetContent(0, ResetSignature.Length).ToArray();
        Require(source.SequenceEqual(ResetSignature) && Read(Reset, ResetSignature.Length).SequenceEqual(ResetSignature),
            "Pinned source/live movement reset entry signature mismatch.");
        log(new { @event = "verified-controller-reset-signature", address = Hex(Reset), signature = Convert.ToHexString(ResetSignature),
            firstSixBytes = Convert.ToHexString(ResetSignature.AsSpan(0, 6)), entryBreakpointInstalled = false });
    }

    private void Wait(string reason)
    {
        _settled = 0; _settleAnchor = null; ++_waiting;
        if (_waiting <= 16 || _waiting % 120 == 0)
            log(new { @event = "controller-reset-waiting", worldStop = _worldStops, reason, nativeCallsBegun = 0 });
    }

    private void Emit(Snapshot value, string phase, bool force)
    {
        // Reserve the last slot for the final result. Repeated input activity
        // can delay acceptance without turning the log into an unbounded trace.
        if (!force && _snapshots >= SnapshotLimit - 1) return;
        Require(_snapshots < SnapshotLimit, "Controller-reset full-snapshot cap reached.");
        Snapshot? previous = _lastLogged;
        ++_snapshots;
        log(new { @event = "controller-reset-snapshot", phase, sample = _snapshots, worldStop = _worldStops,
            postCallWorldStop = _postWorldStops, stablePostWorldStops = _stableAfter,
            diagnosticTick = System.Diagnostics.Stopwatch.GetTimestamp(), diagnosticTickFrequency = System.Diagnostics.Stopwatch.Frequency,
            identity = value.Identity, components = value.Components,
            fsm130 = value.Fsm130, packed140Bits = Hex(value.Packed140), inputDeviceWord14 = value.Device14,
            rawInterpretationsOnly = true, nativeGroundedFlagAssumed = false,
            moveWord124 = Convert.ToHexString(value.Move124),
            controller128Through2D0 = Block(value.Move, previous?.Move, MoveStart),
            rootPrefix = Block(value.Root, previous?.Root, 0),
            character124Through15F = Block(value.Character, previous?.Character, 0x124),
            userInput14Through1F = Block(value.Input, previous?.Input, 0x14),
            command00Through73 = Block(value.Command, previous?.Command, 0),
            controllerChangedOffsetsFromBefore = _before is null ? [] : ChangedOffsets(_before.Move, value.Move, MoveStart),
            rootChangedOffsetsFromBefore = _before is null ? [] : ChangedOffsets(_before.Root, value.Root, 0),
            rootPrsWithinBeforeTolerance = _before is null || SamePose(value, _before),
            idleGuardsPass = Idle(value, out string idleReason), idleReason });
        _lastLogged = value;
    }

    private static object Block(byte[] current, byte[]? previous, int start) => new
    {
        startOffset = Offset(start), bytes = current.Length, rawHex = Convert.ToHexString(current),
        alignedFloatInterpretations = Enumerable.Range(0, current.Length / 4).Select(i => new
        { offset = Offset(start + i * 4), interpretation = FloatWord(current, i * 4) }).ToArray(),
        rawByteDelta = previous is null ? [] : Enumerable.Range(0, current.Length)
            .Where(i => previous[i] != current[i]).Select(i => new { offset = Offset(start + i), before = previous[i], after = current[i] }).ToArray(),
        alignedFloatWordDelta = previous is null ? [] : Enumerable.Range(0, current.Length / 4)
            .Where(i => W(previous, i * 4) != W(current, i * 4)).Select(i => new
            { offset = Offset(start + i * 4), before = FloatWord(previous, i * 4), after = FloatWord(current, i * 4) }).ToArray()
    };

    private static string[] ChangedOffsets(byte[] before, byte[] after, int start) => Enumerable.Range(0, after.Length)
        .Where(i => before[i] != after[i]).Select(i => Offset(start + i)).ToArray();
    private static object FloatWord(byte[] bytes, int offset)
    {
        float value = F(bytes, offset);
        return new { bits = Hex(W(bytes, offset)), finite = float.IsFinite(value), value = float.IsFinite(value) ? (float?)value : null };
    }
    private static uint MoveWord(Snapshot value, int offset) => W(value.Move, offset - MoveStart);
    private static uint W(byte[] bytes, int offset) => BitConverter.ToUInt32(bytes, offset);
    private static float F(byte[] bytes, int offset) => BitConverter.ToSingle(bytes, offset);
    private void Table(uint address, uint expected) => Require(U(address) == expected, $"Unexpected native vtable at {Hex(address)}; expected {Hex(expected)}.");
    private uint Pointer(uint address)
    {
        uint value = U(address);
        Require(value >= 0x10000 && (value & 3) == 0, $"Invalid aligned borrowed pointer at {Hex(address)}.");
        return value;
    }
    private uint U(uint address) => W(Read(address, 4), 0);
    private byte[] Read(uint address, int count)
    {
        EnsureAtStop();
        Require(address >= 0x10000 && count is >= 1 and <= 0x1000 && (ulong)address + (uint)count <= uint.MaxValue,
            "Controller-reset observation exceeds its bounded x86 read range.");
        return Win32Native.ReadMemory(_process, address, count);
    }
    private void EnsureAtStop() => Require(_host is not null && _host.IsAtDebugStop && _host.ProcessHandle == _process &&
        _host.ProcessPid == _pid && _host.ImageBase == 0x400000 && Environment.CurrentManagedThreadId == _ownerThread,
        "Controller-reset reads require the same owned process at its debug-stop thread.");
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidDataException(message); }
    private static string Hex(uint value) => $"0x{value:X8}";
    private static string Offset(int value) => $"+0x{value:X2}";
}
