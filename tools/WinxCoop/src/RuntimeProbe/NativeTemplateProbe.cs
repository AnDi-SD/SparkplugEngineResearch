using System.Reflection.PortableExecutable;
using System.Security.Cryptography;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// Own bounded original-template construction diagnostic. This is not an NPC
/// movement backend or replacement game serializer. The template-idle mode
/// admits one bounded original-scheduler observation after scoped native init.
/// </summary>
internal sealed class NativeTemplateProbe
{
    internal const string StockSha256 = NativeTemplateLayout.StockSha256;
    private const string SchemaInspectorSha256 = "839EB8269BDB598EAFA960AC2D38BC12DAF9E0537BF0CDAF276E0537D53523D3";
    private readonly NativeTemplateLayout _layout;
    private int PayloadBytes => _layout.Bytes;
    private readonly PEReader _image;
    private readonly Action<object> _log;
    private readonly byte[] _payload;
    private readonly bool _initialize;
    private readonly bool _idle;
    private readonly bool _near;
    private readonly bool _walk;
    private readonly bool _turn;
    private readonly bool _bloomIdle;
    private readonly bool _jump;
    private readonly bool _bloomControl;
    private const uint IdleObservationWorldStops = 30, IdleNativeCallLimit = 256;
    private bool RetainedBloom => _bloomIdle || _bloomControl;
    private uint ObservationWorldLimit => _jump ? 136u : IdleObservationWorldStops;
    private uint ObservationEventLimit => _jump ? 4096u : IdleNativeCallLimit;
    private readonly IReadOnlyList<NativeTemplateDescriptor> _descriptors;
    private DebuggerHost _host = null!;
    private IntPtr _process;
    private uint _pid;
    private int _ownerThread;
    private int _stage;
    private uint _stream, _template, _instance, _constructing, _buffer;
    private uint _profile, _record, _localRoot, _parent, _scene, _input, _command, _device, _camera;
    private uint _inputReceiver, _defaultScene;
    private string _localComponentKeys = "";
    private uint _templateManager, _gameManager, _fileManager, _engine, _flow, _level, _active;
    private uint _character, _move, _animator, _machine, _traits, _ownRoot;
    private uint _localAnimator, _localActor, _ownNotify1C, _ownMoveInit, _ownAnimatorInit;
    private ushort _streamRefs, _templateRefs, _instanceRefs;
    private uint _foreignWitnesses, _ownNotify1D, _ownCharacterInit, _buildWitnesses;
    private readonly HashSet<uint> _localNodes = [];
    private uint[] _ownEntities = [];
    private object? _constructionWitness;
    private object? _initializedWitness;
    private bool _cleaned, _payloadWritten;
    private bool _dependentInterval;
    private bool _idleActive, _idleChecksPassed;
    private uint _idleWorldStops, _ownCharacterTicks, _ownMoveUpdates, _ownCommand, _ownActor;
    private uint _ownDeliveryChecks, _ownDeliveryPredicateFalse;
    private bool _deliveryWitnessInstalled;
    private uint[] _nearPositionWords = [];
    private byte[] _nearLocalPosition = [], _nearWorldPosition = [], _nearParentTransform = [];
    private bool _nearPositioned;
    private object? _nearBeforePlacement, _nearAfterPlacement;
    private NativeCommandMovementProbe? _commandMovement;
    private uint _localCharacter, _localMachine;
    private uint _ownBloomNotify1C, _ownBloomFirstPassInit, _ownBloomSetup, _ownBloomUpdates;
    private readonly List<uint> _ownBloomInitPasses = [];
    private BloomIsolation? _bloomIsolation;
    private bool _bloomIntervalVerified;
    private object? _bloomLastIsolationWitness, _bloomStateWitness;
    private uint _ownBloomNotifyTicks, _bloomEntryOrdinal, _bloomLastWorldCounter;
    private uint? _observedBloomKind;
    private byte[]? _bloomNeutralCommand;
    private string _bloomRetentionFailure = "";

    private sealed record Component(uint Address, uint Table, uint ClassId, uint Owner, uint BoundNode);
    private sealed record Relationship(string Operand, string Actual, string Expected, bool Matched);
    private sealed record Membership(uint[] Counts, uint MoveCount, uint MainListCount, uint SecondaryListCount,
        uint[] MainListMatches, uint[] SecondaryListMatches, uint[] ActiveMoveMatches,
        uint[] TableMatches, uint[] Matches, string TablesSha256);
    private sealed record BloomIsolation(uint GlobalCounter, uint CharacterManager, uint CachedManagerLink,
        string LocalComponents, byte[] Command, byte[] Position, byte[] Scale, byte[] Orientation,
        byte[] CachedWorld, byte[] CharacterLinks, byte[] MachineLinks);

    internal NativeTemplateProbe(ProbeConfiguration config, PEReader image, Action<object> log)
    {
        _image = image; _log = log;
        _layout = NativeTemplateLayout.Select(config);
        _bloomIdle = config.Mode == "template-bloom-idle";
        _jump = config.Mode == "template-bloom-jump";
        _bloomControl = config.Mode is "template-bloom-jump" or "template-bloom-walk" or "template-bloom-turn";
        _turn = config.Mode is "template-near-turn" or "template-bloom-turn";
        _walk = config.Mode is "template-near-walk" or "template-near-turn" or "template-bloom-jump" or "template-bloom-walk" or "template-bloom-turn";
        _near = config.Mode is "template-near-idle" or "template-near-walk" or "template-near-turn" or "template-bloom-idle" or "template-bloom-jump" or "template-bloom-walk" or "template-bloom-turn";
        _idle = config.Mode is "template-idle" or "template-near-idle" or "template-near-walk" or "template-near-turn" or "template-bloom-idle" or "template-bloom-jump" or "template-bloom-walk" or "template-bloom-turn";
        _initialize = config.Mode is "template-init" or "template-idle" or "template-near-idle" or "template-near-walk" or "template-near-turn" or "template-bloom-idle" or "template-bloom-jump" or "template-bloom-walk" or "template-bloom-turn";
        _payload = ReadVerifiedPayload(config, _layout);
        // Pinned StaticSeresa Animation Group property (DWORD at file2669).
        // Native setter555060/getter555070 access machine+128; it is a kind
        // value, not the Animator pointer held by Character+128.
        if (_layout.ExpectedKind is uint expectedKind)
            Require(_layout.KindSourceOffset is int kindOffset && BitConverter.ToUInt32(_payload, kindOffset) == expectedKind,
                "Pinned Students Animation Group property mismatch.");
        _descriptors = _layout.Descriptors;
    }

    internal bool ModeSucceeded => _cleaned && (!_idle || _idleChecksPassed) && (!_walk || _commandMovement?.Passed == true) &&
        (!_layout.IsDerivedBloom || _bloomIntervalVerified);
    internal bool ModeCompleted => _cleaned;
    internal bool HasLiveOwnership => _stream != 0 || _template != 0 || _instance != 0 || _constructing != 0;
    internal object Summary => new { stage = _stage, cleaned = _cleaned, hasLiveOwnership = HasLiveOwnership,
        templateSha256 = _layout.Sha256, templateLayout = _layout.Name, schemaInspectorSha256 = SchemaInspectorSha256,
        stream = Hex(_stream), template = Hex(_template), instance = Hex(_instance),
        ownNotify1D = _ownNotify1D, ownCharacterInit = _ownCharacterInit, buildWitnesses = _buildWitnesses,
        foreignWitnesses = _foreignWitnesses, construction = _constructionWitness, initialized = _initializedWitness,
        scopedInitializationMode = _initialize, dependentIntervalActive = _dependentInterval,
        ownNotify1C = _ownNotify1C, ownMoveInit = _ownMoveInit, ownAnimatorInit = _ownAnimatorInit,
        nativeScopedInitCallsObserved = _ownNotify1C == 0 ? 0 : 1,
        originalSchedulerObservation = new { enabled = _idle, active = _idleActive,
            worldStops = _idleWorldStops, requiredWorldStops = ObservationWorldLimit,
            ownCharacterTicks = _ownCharacterTicks, ownMoveUpdateEntries = _ownMoveUpdates,
            ownTypedDeliveryChecks = _ownDeliveryChecks, ownTypedDeliveryPredicateFalse = _ownDeliveryPredicateFalse,
            checksPassed = _idleChecksPassed, manualUpdateCalls = 0, originalOriginWalkabilityAssumed = false,
            privateCommand = Hex(_ownCommand), actor = Hex(_ownActor) },
        nativeInstancePosition = new { enabled = _near, function = Hex(0x599200), positioned = _nearPositioned,
            before = _nearBeforePlacement, after = _nearAfterPlacement, nativeResetCalled = false,
            rawRuntimePositionWrites = 0, protectedPlatformDetachSideEffectsQualified = false,
            standingCollisionOrMovementQualified = false },
        commandMovement = _commandMovement?.Summary, quarterTurnRequested = _turn, jumpProbeRequested = _jump,
        derivedBloomInitialization = new { enabled = _layout.IsDerivedBloom, initOnly = _layout.IsDerivedBloom && !RetainedBloom,
            ownNotify1C = _ownBloomNotify1C, ownFirstPassInit = _ownBloomFirstPassInit, ownSetupEntries = _ownBloomSetup,
            ownUpdates = _ownBloomUpdates, passIndices = _ownBloomInitPasses.ToArray(),
            privateIntervalVerifiedThroughDelete = _bloomIntervalVerified && !RetainedBloom,
            privateIntervalVerifiedBeforeFirstResume = _bloomIntervalVerified,
            isolation = _bloomLastIsolationWitness, states = _bloomStateWitness,
            expectedKind = _layout.ExpectedKind, retainedOrInputQualified = false },
        retainedBloomNeutral = new { enabled = _bloomIdle, observedKind = _observedBloomKind,
            ownNotifyTicks = _ownBloomNotifyTicks, ownUpdateEntries = _ownBloomUpdates, failure = _bloomRetentionFailure,
            originalSchedulerAccepted = _bloomIdle && _idleChecksPassed, nativeInputWrites = 0,
            globalCounterEqualityAfterResumeClaimed = false, jumpOrMovementQualified = false },
        independentMovementQualified = false, schedulerAbsenceClaimed = false,
        manualInitOrSchedulerCalls = 0, nativeEntityFieldWrites = 0, ownedPayloadBytesWritten = _payloadWritten ? PayloadBytes : 0 };

    internal static byte[] ReadVerifiedPayload(ProbeConfiguration config)
        => ReadVerifiedPayload(config, NativeTemplateLayout.Select(config));

    private static byte[] ReadVerifiedPayload(ProbeConfiguration config, NativeTemplateLayout layout)
    {
        string path = config.TemplateFile ?? Path.Combine(config.MediaDirectory, "Characters", "Bloom", "StaticSeresa.spt");
        if (!Path.IsPathFullyQualified(path) || path.Any(c => c is < ' ' or > '~'))
            throw new InvalidDataException("Template probe requires an absolute ASCII path and an allowed exact template pin.");
        var info = new FileInfo(path);
        if (!info.Exists || info.Length is < 1 or > 65536 || info.Length != layout.Bytes)
            throw new InvalidDataException("Template extent differs from its pinned diagnostic layout.");
        byte[] bytes = File.ReadAllBytes(path);
        if (bytes.Length != layout.Bytes || Convert.ToHexString(SHA256.HashData(bytes)) != layout.Sha256 ||
            BitConverter.ToUInt32(bytes, 0) != 0xDAB33F00 || BitConverter.ToUInt32(bytes, 0x44) != 6)
            throw new InvalidDataException("Pinned template bytes/hash/header mismatch.");
        return bytes;
    }

    internal void InstallWitnesses(DebuggerHost host)
    {
        Require(host.IsAtDebugStop && host.ProcessHandle != IntPtr.Zero && host.ProcessPid != 0 && host.ImageBase == 0x400000,
            "Template witness installation requires the pinned owned process at its debug stop.");
        _host = host;
        _process = host.ProcessHandle; _pid = host.ProcessPid; _ownerThread = Environment.CurrentManagedThreadId;
        Hook(0x5FB950, "Template.build", (uint tid, uint at, ref Win32Native.X86Context ctx) =>
        {
            if (_template != 0 && _stage == 6 && U(ctx.Ecx + 0x14) == _template)
            {
                Require(ctx.Ecx != _record && U(ctx.Ecx) == 0x710F80 && _constructing == 0,
                    "Unexpected new native template build identity.");
                _constructing = ctx.Ecx; ++_buildWitnesses;
                _log(new { @event = "template-owned-build-entry", threadId = tid, instance = Hex(ctx.Ecx), template = Hex(_template) });
            }
            else Foreign("Template.build", tid, ctx.Ecx, null);
        });
        Hook(0x4F45A0, "Character.Notify", (uint tid, uint at, ref Win32Native.X86Context ctx) =>
        {
            uint message = Pointer(ctx.Esp + 4), code = U(message);
            if (OwnReceiver(ctx.Ecx))
            {
                if (code == 0x1E)
                {
                    Require(_idle && _idleActive && !_dependentInterval && _stage == 6,
                        "Owned Character received a tick outside its bounded original-scheduler observation.");
                    Require(++_ownCharacterTicks <= IdleNativeCallLimit, "Owned Character tick witness cap exceeded.");
                }
                if (code == 0x1C)
                {
                    Require(_initialize && _dependentInterval && U(message + 0x10) == 0 && U(message + 0x18) <= 7,
                        "Owned Character received an unqualified scoped initialization message.");
                    Require(++_ownNotify1C == 1, "Repeated owned Character1C initialization is unqualified.");
                }
                if (code == 0x1D) ++_ownNotify1D;
                _log(new { @event = "template-owned-character-notify", threadId = tid, receiver = Hex(ctx.Ecx),
                    code = Hex(code), sender = Hex(U(message + 0x10)), instance = Hex(_constructing != 0 ? _constructing : _instance) });
            }
            else Foreign("Character.Notify", tid, ctx.Ecx, code);
        });
        Hook(0x4F3ED0, "Character.init", (uint tid, uint at, ref Win32Native.X86Context ctx) =>
        {
            if (OwnReceiver(ctx.Ecx))
            {
                Require(ClassId(ctx.Ecx) == 0x0003CC73, "Owned Character setup receiver type mismatch.");
                ++_ownCharacterInit;
                Require(_ownCharacterInit == 1, "Repeated owned Character setup is unqualified.");
                _log(new { @event = "template-owned-character-setup-entry", threadId = tid, receiver = Hex(ctx.Ecx),
                    owner = Hex(U(ctx.Ecx + 0x24)), returnAddress = Hex(U(ctx.Esp)), selector148 = U(ctx.Ecx + 0x148) });
            }
            else Foreign("Character.init", tid, ctx.Ecx, null);
        });
        foreach ((uint address, string name) in new[] { (0x4F60D0u, "MoveCtrl.init"), (0x4FB6E0u, "Animator.init") })
            Hook(address, name, (uint tid, uint at, ref Win32Native.X86Context ctx) =>
            {
                if (OwnReceiver(ctx.Ecx))
                {
                    _log(new { @event = "template-owned-component-init-entry", name, receiver = Hex(ctx.Ecx), threadId = tid,
                        scopedInterval = _initialize && _dependentInterval });
                    Require(_initialize && _dependentInterval, "Native owned component initialization occurred outside its scoped qualification interval.");
                    if (at == 0x4F60D0) Require(++_ownMoveInit == 1, "Repeated own MoveCtrl initialization.");
                    else Require(++_ownAnimatorInit == 1, "Repeated own Animator initialization.");
                    return;
                }
                Foreign(name, tid, ctx.Ecx, null);
            });
        if (_idle)
            Hook(0x4FA860, "MoveCtrl.update", (uint tid, uint at, ref Win32Native.X86Context ctx) =>
            {
                if (!OwnReceiver(ctx.Ecx)) { Foreign("MoveCtrl.update", tid, ctx.Ecx, null); return; }
                Require(_idleActive && !_dependentInterval && _stage == 6 && ctx.Ecx == _move && _ownCommand >= 0x10000,
                    "Owned MoveCtrl update occurred outside the retained initialized observation interval.");
                byte[] command = ReadGuardedCommand(); // Walk has its own exact intent guard; idle stays neutral.
                if (RetainedBloom) LogRetainedBloom("before-native-Move-consumption", tid);
                _commandMovement?.ObserveMoveEntry(tid, ref ctx);
                _log(new { @event = "template-owned-move-update-entry", threadId = tid, receiver = Hex(ctx.Ecx),
                    owner = Hex(U(ctx.Ecx + 0x24)), character = Hex(U(ctx.Ecx + 0x160)),
                    privateCommand = Hex(U(_character + 0x130)), originalCaller = Hex(U(ctx.Esp)),
                    previousDelta128 = FloatWord(Read(ctx.Ecx + 0x128, 4), 0),
                    magnitude04 = FloatWord(command, 4), jumpHeld1A = command[0x1A],
                    observationWorldStops = _idleWorldStops, invocation = _ownMoveUpdates + 1,
                    manualInvocation = false });
                Require(U(_move) == 0x6F64F8 && U(_move + 0x160) == _character &&
                    U(_character + 0x130) == _ownCommand && U(_machine + 0x12C) == _ownCommand,
                    "Owned MoveCtrl update receiver/Character/private-command identity is not the retained initialized cohort.");
                Require(++_ownMoveUpdates <= IdleNativeCallLimit, "Owned MoveCtrl update witness cap exceeded.");
            });
        if (_layout.IsDerivedBloom) InstallBloomInitWitnesses();
    }

    internal void Step(DebuggerHost host, uint tid, ref Win32Native.X86Context ctx,
        NativeCallDispatcher calls, Action resume)
    {
        Require(ReferenceEquals(host, _host) && host.IsAtDebugStop && tid == host.MainThreadId && ctx.Eip == NativeCallDispatcher.WorldReturn && !calls.Pending,
            "Template experiment requires the owned main-thread world stop.");
        if (_cleaned) return;
        if (_stage != 0) VerifyLocal();
        switch (_stage)
        {
            case 0:
                CaptureLocal(); _stage = 1;
                Call(tid, ref ctx, calls, resume, 0x465560, 0, [], true, value =>
                {
                    _stream = value; Table(_stream, 0x6E7E50); _streamRefs = Refs(_stream);
                    _log(new { @event = "template-stream-factory-return", stream = Hex(_stream), referenceCount = _streamRefs,
                        prefixHex = Convert.ToHexString(Read(_stream, 0x38)) });
                    Require(_streamRefs == 0, "Fresh stream has unexpected incoming owners.");
                }); break;
            case 1:
                _stage = 2;
                Call(tid, ref ctx, calls, resume, 0x465500, _stream, [(uint)PayloadBytes], false, value =>
                { Require((value & 255) != 0, "Native memory stream resize failed."); VerifyStream(); }); break;
            case 2:
                _stage = 3;
                Call(tid, ref ctx, calls, resume, 0x4CF2F0, _stream, [], false, value =>
                {
                    VerifyStream(); Require(value != 0 && value == U(_stream + 0x30), "Native owned buffer identity mismatch.");
                    _buffer = value;
                    _ = Read(_buffer, PayloadBytes);
                    Win32Native.WriteMemory(_host.ProcessHandle, _buffer, _payload, executable: false);
                    Require(Read(_buffer, PayloadBytes).SequenceEqual(_payload), "Owned stream payload readback mismatch.");
                    _payloadWritten = true;
                    _log(new { @event = "template-owned-payload-written", stream = Hex(_stream), buffer = Hex(_buffer),
                        bytes = PayloadBytes, sha256 = _layout.Sha256, nativeEntityFieldsWritten = 0 });
                }); break;
            case 3:
                _stage = 4;
                Call(tid, ref ctx, calls, resume, 0x59F630, 0, [], true, value =>
                {
                    _template = value; Table(_template, 0x703D24); _templateRefs = Refs(_template);
                    _log(new { @event = "template-factory-return", template = Hex(_template), referenceCount = _templateRefs,
                        baseList04 = Hex(U(_template + 4)), expectedConstructorRegistrationReferences = 1,
                        prefixHex = Convert.ToHexString(Read(_template, 0x60)) });
                    // 59F540 registers the new object through manager Add at
                    // 59F5FD. Stock factory return therefore has one reference,
                    // unlike MemoryStream. Its base +04 list is independent.
                    Require(_templateRefs == 1 && U(_template + 4) == 0,
                        "Template constructor registration baseline/base-list mismatch.");
                }); break;
            case 4:
                VerifyStream(); _stage = 5;
                Call(tid, ref ctx, calls, resume, 0x59E930, _template, [_stream], false, value =>
                { Require((value & 255) != 0, "Native template parse failed; unknown partial state."); InspectTemplate(); }); break;
            case 5:
                InspectTemplate(); Require(Read(Pointer(0x755278) + 0x21C, 1)[0] == 0, "Native1D setup guard is not admitted.");
                NativeInstancePositionCall? placement = _near ? CaptureNearDestination() : null;
                _stage = 6;
                if (_initialize)
                {
                    foreach (uint function in new uint[] { 0x59E7A0, 0x5991E0, 0x5FB350 }) VerifyFunction(function);
                    _dependentInterval = true;
                    if (_layout.IsDerivedBloom) CaptureBloomIsolation();
                    calls.BeginDependentLifecycle(host, tid, ref ctx, new(0x59E7A0, _template, [1], false), 0x5991E0, 0x5FB350,
                        value =>
                        {
                            _instance = value;
                            Require(_constructing == value && _buildWitnesses == 1, "Dependent Create result does not match its own build.");
                            _constructing = 0; CaptureInstance(initialized: false, allowSecondary: true);
                        },
                        value =>
                        {
                            Require(value == _instance, "Scoped init changed the owned instance identity.");
                            CaptureInstance(initialized: true, allowSecondary: true);
                            if (_near) _nearBeforePlacement = ObserveNearPlacement("initialized-before-native-position", afterPosition: false);
                            if (_idle)
                            {
                                _ownCommand = Pointer(_character + 0x130);
                                _ownActor = Pointer(_animator + 0x128);
                                _ = ObserveIdleState("initialized-before-original-scheduler", requirePromotion: false);
                            }
                        },
                        results =>
                        {
                            Require(results.Length == (_near ? 3 : _idle ? 2 : 3) && results[0] == _instance && _initializedWitness is not null &&
                                (!_near || _nearPositioned),
                                "Dependent lifecycle completion/results mismatch.");
                            if (_idle)
                            {
                                if (RetainedBloom)
                                {
                                    VerifyBloomIsolation("after-native-placement-before-first-original-resume");
                                    _bloomIntervalVerified = true;
                                    _bloomLastWorldCounter = U(0x765BC8);
                                }
                                _dependentInterval = false; _idleActive = true;
                                if (_walk)
                                {
                                    _commandMovement = new(_host, _instance, _ownRoot, _character, _move, _ownCommand,
                                        VerifyWalkOwnership, Read, U, Hook, VerifyFunction, _log, quarterTurn: _turn, jumpProbe: _jump);
                                    _commandMovement.Start();
                                }
                                _log(new { @event = "template-initialized-instance-retained", instance = Hex(_instance),
                                    originalUpdatesPermitted = true, worldStopLimit = ObservationWorldLimit,
                                    nativeEntryWitnessLimit = IdleNativeCallLimit, originalOriginWalkabilityAssumed = false,
                                    originalContextRestoreAfterScopedInitialization = true, privateIntervalResults = results });
                                resume(); return;
                            }
                            if (_layout.IsDerivedBloom)
                            {
                                VerifyBloomIsolation("after-native-delete-before-original-resume");
                                _bloomIntervalVerified = true;
                            }
                            uint deleted = _instance; _instance = 0; _ownRoot = 0; _dependentInterval = false;
                            VerifyLocal();
                            Require(Memberships(_ownEntities).Matches.Length == 0,
                                "Own entities survived in game-manager lists/tables after native scoped deletion.");
                            _stage = 7;
                            _log(new { @event = "template-scoped-init-and-delete-returned", deletedInstance = Hex(deleted),
                                privateIntervalResults = results, originalSchedulerResumedBetweenCalls = false, freedBytesRead = false });
                            resume();
                        }, retainAfterInitialization: _idle, positionAfterInitialization: placement,
                        positionedWitness: _near ? ObservePositionedInstance : null);
                }
                else
                    Call(tid, ref ctx, calls, resume, 0x59E7A0, _template, [1], false, value =>
                    {
                        _instance = value;
                        Require(_constructing == value && _buildWitnesses == 1, "Create result does not match the observed own build.");
                        _constructing = 0; CaptureInstance();
                    });
                break;
            case 6:
                if (_idle)
                {
                    Require(_idleActive && !_dependentInterval && _idleWorldStops < ObservationWorldLimit,
                        "Unexpected retained instance observation phase.");
                    if (!_deliveryWitnessInstalled) InstallDeliveryWitness();
                    ++_idleWorldStops;
                    Membership membership = ObserveIdleState("original-world-return", requirePromotion: !_bloomIdle && _idleWorldStops >= 2);
                    if (_idleWorldStops >= 2 && !_bloomIdle)
                        Require(_ownCharacterTicks != 0 && _ownMoveUpdates != 0,
                            "Original scheduler did not witness both own Character tick and own typed MoveCtrl update after two world returns.");
                    if (_bloomIdle)
                    {
                        LogRetainedBloom("after-original-world", tid);
                        if (_idleWorldStops >= 2 && (!LiveOwnMembership(membership) || _ownCharacterTicks != _idleWorldStops ||
                            _ownMoveUpdates != _idleWorldStops || _ownBloomNotifyTicks != _idleWorldStops || _ownBloomUpdates != _idleWorldStops))
                            RejectBloomRetention("Original scheduler did not provide one own Character/Bloom/Move callback per observed World.");
                        if (_bloomRetentionFailure.Length == 0 && _idleWorldStops < IdleObservationWorldStops) break;
                        _idleChecksPassed = _bloomRetentionFailure.Length == 0 && _idleWorldStops == IdleObservationWorldStops;
                        _idleActive = false;
                        _log(new { @event = "template-bloom-retention-finished", originalWorldReturns = _idleWorldStops,
                            ownCharacterTicks = _ownCharacterTicks, ownMoveUpdates = _ownMoveUpdates, ownBloomNotifyTicks = _ownBloomNotifyTicks,
                            ownBloomUpdateEntries = _ownBloomUpdates, passed = _idleChecksPassed, failure = _bloomRetentionFailure,
                            nativeInputWrites = 0, originalCommandOverwriteNotRepaired = true, nativeCleanupStillRequired = true });
                    }
                    else if (_walk)
                    {
                        if (_bloomControl)
                        {
                            LogRetainedBloom("after-original-world", tid);
                            if (_ownBloomNotifyTicks != _idleWorldStops || _ownBloomUpdates != _idleWorldStops)
                                RejectBloomRetention("Original controlled Bloom scheduler did not provide one own Notify/Update per observed World.");
                        }
                        NativeCommandMovementProbe movement = _commandMovement ??
                            throw new InvalidDataException("Missing private command phase observer.");
                        movement.EndWorld(_idleWorldStops);
                        if (!movement.Finished && _bloomRetentionFailure.Length == 0) break;
                        movement.PrepareCleanup();
                        _idleChecksPassed = movement.Passed && _bloomRetentionFailure.Length == 0; _idleActive = false;
                        _log(new { @event = "template-command-observation-finished", worldStops = _idleWorldStops,
                            ownCharacterTicks = _ownCharacterTicks, ownMoveUpdateEntries = _ownMoveUpdates,
                            passed = _idleChecksPassed, membership, privateInputInjected = true, fullE1Qualified = false });
                    }
                    else
                    {
                        if (_idleWorldStops < IdleObservationWorldStops) break;
                        _idleChecksPassed = true; _idleActive = false;
                        _log(new { @event = "template-original-scheduler-observation-complete", worldStops = _idleWorldStops,
                            ownCharacterTicks = _ownCharacterTicks, ownMoveUpdateEntries = _ownMoveUpdates,
                            membership, privateInputInjected = false, nativePhysicsOrMovementAuthorityClaimed = false });
                    }
                }
                VerifyOwnedInstance(scheduled: _idle); Require(Refs(_instance) == _instanceRefs, "Own instance refcount changed before deletion.");
                _stage = 7;
                Call(tid, ref ctx, calls, resume, 0x5FB350, _instance, [1], false, _ =>
                {
                    // Never read deleted instance, components, command or Nodes.
                    uint deleted = _instance; _instance = 0; _ownRoot = 0;
                    Require(Memberships(_ownEntities).Matches.Length == 0, "Deleted own components remain in bounded game-manager tables.");
                    _log(new { @event = "template-instance-delete-returned", deletedInstance = Hex(deleted), freedBytesRead = false });
                }); break;
            case 7:
                Table(_template, 0x703D24); Require(_templateRefs == 1 && Refs(_template) == _templateRefs && U(_template + 4) == 0,
                    "Template constructor registration baseline/base-list changed before deletion.");
                _stage = 8;
                // Original 5FD212/5FD21C parse-failure cleanup directly invokes
                // this deleting destructor at 5FD274 without a manual release.
                // Its 5A0490 body unregisters the template through 462A50.
                Call(tid, ref ctx, calls, resume, 0x5A05B0, _template, [1], false, _ =>
                { uint deleted = _template; _template = 0; _log(new { @event = "template-descriptor-owner-deleted", deletedTemplate = Hex(deleted), instanceAlreadyDeleted = true }); }); break;
            case 8:
                Table(_stream, 0x6E7E50); Require(Refs(_stream) == _streamRefs, "Stream owner changed before deletion.");
                _stage = 9;
                Call(tid, ref ctx, calls, resume, 0x465730, _stream, [1], false, _ =>
                { uint deleted = _stream; _stream = 0; _log(new { @event = "template-input-stream-deleted", deletedStream = Hex(deleted), nativeBorrowersCompleted = true }); }); break;
            case 9:
                VerifyLocal(); Require(!HasLiveOwnership && Memberships(_ownEntities).Matches.Length == 0,
                    "Native template cleanup retained an unexpected owner/membership.");
                _cleaned = true; _stage = 10;
                _log(new { @event = "template-create-cycle-cleaned", localIdentityPreserved = true,
                    boundedConstructionAndCleanupOnly = true, independentMovementQualified = false }); break;
            default: throw new InvalidOperationException("Unknown native template probe phase.");
        }
    }

    private void CaptureLocal()
    {
        _profile = Pointer(0x765AD4); Table(_profile, 0x6F5A44);
        _record = Pointer(_profile + 0x2B4); Table(_record, 0x710F80);
        _localRoot = Pointer(_record + 0x24); Table(_localRoot, VisualGraphReader.NodeVtable);
        _parent = Pointer(_localRoot + 0x2C); _scene = Pointer(_localRoot + 0x3C); Table(_scene, 0x6E7358);
        Require(U(_record + 0x20) == 11, "Expected the typed eleven-component local player.");
        _input = Pointer(0x7552A0); Table(_input, 0x6F5508);
        _command = Pointer(_input + 0x18); Table(_command, 0x703090);
        _device = Pointer(_input + 0x1C); Table(_device, 0x7116CC);
        _engine = Pointer(0x755274); _camera = Pointer(_engine + 0x1C);
        _inputReceiver = U(_input + 0x14); _defaultScene = U(_engine + 0x18);
        if (_idle || _layout.IsDerivedBloom) _localComponentKeys = LocalComponentKeys();
        _templateManager = Pointer(0x75DC64); Table(_templateManager, 0x6E77E0);
        _gameManager = Pointer(0x765ACC); Table(_gameManager, 0x70069C);
        _fileManager = Pointer(0x755278); Require(Read(_fileManager + 0x21C, 1)[0] == 0, "File manager suppresses native template setup.");
        _flow = Pointer(0x755294); _level = U(_flow + 0x1B0);
        Require(!(_idle || _layout.IsDerivedBloom) || (_level == 28 && U(_device + 0x14) == 3),
            "Original-scheduler observation requires the qualified Alfea28 InGame device context3.");
        int stack = unchecked((int)U(_flow + 0x1AC));
        Require(_level is >= 1 and <= 50 && stack is >= 0 and <= 20 && U(_flow + 0x1B8) == 0, "Playing flow is not settled.");
        _active = Pointer(_flow + 0x15C + (uint)stack * 4); Require(U(_active + 0x10) == _level, "Playing flow identity mismatch.");
        Require(U(_templateManager + 0x20) == 0 && U(Pointer(0x75DB90) + 0x20) == 0, "Native parse/world context is active.");
        var graph = new VisualGraphReader(_host.ProcessHandle).Read(_localRoot, inspectSkins: false);
        Require(graph.Success, "Cannot bound local node identities: " + graph.Error);
        foreach (var node in graph.Graph.Nodes) _localNodes.Add(node.Address);
        if (_initialize)
        {
            uint head = Pointer(_record + 0x1C), link = Pointer(head);
            for (int i = 0; i < 11; ++i)
            {
                Require(link != head, "Local Actor inventory ended before eleven components.");
                uint item = Pointer(link + 8);
                if (_layout.IsDerivedBloom && U(item) == 0x6F6438)
                {
                    Require(_localCharacter == 0 && U(item + 0x24) == _record && U(item + 0x18) == _localRoot,
                        "Local Character identity is not unique/bound.");
                    _localCharacter = item;
                }
                if (U(item) == 0x6F670C)
                {
                    Require(_localAnimator == 0 && U(item + 0x24) == _record, "Local Animator identity is not unique/bound.");
                    _localAnimator = item; _localActor = Pointer(item + 0x128); Table(_localActor, 0x703F80);
                }
                link = Pointer(link);
            }
            Require(link == head && _localAnimator != 0, "Local Actor inventory count/type mismatch.");
            if (_layout.IsDerivedBloom)
            {
                Table(_localCharacter, 0x6F6438);
                _localMachine = Pointer(_localCharacter + 0x124); Table(_localMachine, 0x6FA288);
                Require(U(_localCharacter + 0x130) == _command && U(_localMachine + 0x124) == _localCharacter,
                    "Local Bloom/Character/input relationships are unqualified.");
            }
        }
        _log(new { @event = "template-local-baseline", profile = Hex(_profile), record = Hex(_record), root = Hex(_localRoot),
            parent = Hex(_parent), scene = Hex(_scene), input = Hex(_input), command = Hex(_command), device = Hex(_device), camera = Hex(_camera),
            inputReceiver14 = Hex(_inputReceiver), defaultScene18 = Hex(_defaultScene), localComponentKeys = _localComponentKeys,
            parseContext = Hex(U(_templateManager + 0x20)), manager = Memberships([]), localNodeCount = _localNodes.Count,
            stockTemplateSha256 = _layout.IsDerivedBloom ? null : StockSha256,
            templateSha256 = _layout.Sha256, templateLayout = _layout.Name, schemaInspectorSha256 = SchemaInspectorSha256 });
    }

    private void InstallBloomInitWitnesses()
    {
        Hook(0x5261A0, "Bloom.Notify.init-only", (uint tid, uint at, ref Win32Native.X86Context ctx) =>
        {
            if (!OwnReceiver(ctx.Ecx)) return;
            Table(ctx.Ecx, _layout.MachineTable);
            uint message = Pointer(ctx.Esp + 4), code = U(message);
            if (code == 0x1E)
            {
                Require(RetainedBloom && _idleActive && !_dependentInterval && _stage == 6,
                    "Owned Bloom gameplay notification escaped its retained scheduler interval.");
                Require(++_ownBloomNotifyTicks <= IdleNativeCallLimit, "Own Bloom notification witness cap exceeded.");
                LogRetainedBloom("before-own-Bloom-Notify1E", tid);
                return;
            }
            if (code == 0x1C)
            {
                Require(_dependentInterval && _bloomIsolation is not null && !_bloomIntervalVerified,
                    "Owned Bloom initialization escaped its private interval.");
                uint pass = U(message + 0x18);
                Require(U(message + 0x10) == 0 && pass <= 7 && !_ownBloomInitPasses.Contains(pass) && _ownBloomNotify1C < 8,
                    "Derived Bloom received an unqualified scoped-init sender, repeated pass or pass bound.");
                ++_ownBloomNotify1C; _ownBloomInitPasses.Add(pass);
            }
            Require(_dependentInterval || (RetainedBloom && _bloomIntervalVerified && _stage is 6 or 7),
                "Owned Bloom notification has no qualified private/retained lifetime.");
            _log(new { @event = "template-owned-bloom-notify", threadId = tid, receiver = Hex(ctx.Ecx),
                code = Hex(code), passIndex18 = U(message + 0x18), responseWord1CBeforeCall = Hex(U(message + 0x1C)),
                sender10 = Hex(U(message + 0x10)), instance = Hex(U(ctx.Ecx + 0x24)), originalGameplayResumed = !_dependentInterval });
        });
        foreach ((uint address, string name) in new[] { (0x526150u, "Bloom.first-pass-init"), (0x526E30u, "Bloom.native-setup") })
            Hook(address, name, (uint tid, uint at, ref Win32Native.X86Context ctx) =>
            {
                if (!OwnReceiver(ctx.Ecx)) return;
                Table(ctx.Ecx, _layout.MachineTable);
                Require(_dependentInterval && _bloomIsolation is not null, "Derived Bloom setup escaped its private init interval.");
                if (at == 0x526150) Require(++_ownBloomFirstPassInit == 1, "Repeated Bloom first-pass initialization.");
                else Require(++_ownBloomSetup <= 8, "Bloom native setup witness cap exceeded.");
                _log(new { @event = "template-owned-bloom-init-entry", name, threadId = tid, receiver = Hex(ctx.Ecx),
                    owner124 = Hex(U(ctx.Ecx + 0x124)), observedKind128 = Hex(U(ctx.Ecx + 0x128)),
                    command12C = Hex(U(ctx.Ecx + 0x12C)), global765BC8 = U(0x765BC8), originalGameplayResumed = false });
            });
        Hook(0x526F90, "Bloom.update.forbidden-during-init", (uint tid, uint at, ref Win32Native.X86Context ctx) =>
        {
            if (!OwnReceiver(ctx.Ecx)) return;
            Require(RetainedBloom && _idleActive && !_dependentInterval && _stage == 6,
                "Derived Bloom reached gameplay outside its retained observation.");
            Require(++_ownBloomUpdates <= IdleNativeCallLimit, "Own Bloom update witness cap exceeded.");
            LogRetainedBloom("before-own-Bloom-native-update", tid);
        });
        // Jump's command probe owns this exact hook and its entry/return pair.
        // Avoid installing a second signature-checked breakpoint at 5268B0.
        if (_bloomIdle)
            Hook(0x5268B0, "Bloom.command-builder.observation", (uint tid, uint at, ref Win32Native.X86Context ctx) =>
            {
                if (!OwnReceiver(ctx.Ecx)) return;
                Require(_idleActive && !_dependentInterval && _stage == 6,
                    "Derived Bloom builder escaped its retained observation.");
                LogRetainedBloom("before-own-Bloom-command-builder", tid);
            });
        else if (!RetainedBloom) Hook(0x4FA860, "MoveCtrl.update.forbidden-during-bloom-init", (uint tid, uint at, ref Win32Native.X86Context ctx) =>
        {
            if (!OwnReceiver(ctx.Ecx)) return;
            ++_ownMoveUpdates;
            throw new InvalidDataException("Derived Bloom init-only diagnostic reached a native MoveCtrl update.");
        });
    }

    private void CaptureBloomIsolation()
    {
        Require(_layout.IsDerivedBloom && _initialize && (!_walk || _bloomControl) && (RetainedBloom ? _idle && _near : !_idle && !_near) && _dependentInterval &&
            _bloomIsolation is null && _instance == 0 && _constructing == 0,
            "Derived Bloom requires its explicit private initialization interval without input.");
        _bloomIsolation = ReadBloomIsolation();
        _log(new { @event = "template-bloom-private-interval-baseline", global765BC8 = _bloomIsolation.GlobalCounter,
            characterManager = Hex(_bloomIsolation.CharacterManager), cachedManagerLink28 = Hex(_bloomIsolation.CachedManagerLink),
            localCharacter = Hex(_localCharacter), localMachine = Hex(_localMachine),
            localComponents = _bloomIsolation.LocalComponents, localCommandHex = Convert.ToHexString(_bloomIsolation.Command),
            localPositionHex = Convert.ToHexString(_bloomIsolation.Position), localScaleHex = Convert.ToHexString(_bloomIsolation.Scale),
            localOrientationHex = Convert.ToHexString(_bloomIsolation.Orientation), cachedWorldHex = Convert.ToHexString(_bloomIsolation.CachedWorld),
            localCharacterLinksHex = Convert.ToHexString(_bloomIsolation.CharacterLinks),
            localMachineLinks124Hex = Convert.ToHexString(_bloomIsolation.MachineLinks),
            comparesOnlyThisPrivateInterval = true, originalGameplayResumed = false });
    }

    private BloomIsolation ReadBloomIsolation()
    {
        VerifyLocal(); Table(_localCharacter, 0x6F6438); Table(_localMachine, 0x6FA288);
        Require(U(_localCharacter + 0x24) == _record && U(_localCharacter + 0x18) == _localRoot &&
            U(_localCharacter + 0x124) == _localMachine && U(_localCharacter + 0x130) == _command &&
            U(_localMachine + 0x124) == _localCharacter && U(_localMachine + 0x12C) == _command,
            "Local Bloom links changed during derived initialization.");
        byte[] command = Read(_command, 0x74), position = Read(_localRoot + 0x20, 12), scale = Read(_localRoot + 0x30, 12),
            orientation = Read(_localRoot + 0x40, 36), world = Read(_localRoot + 0x74, 60);
        Require(new[] { 4, 8, 12, 16, 20 }.All(o => float.IsFinite(BitConverter.ToSingle(command, o))) &&
            new[] { position, scale, orientation, world }.All(bytes => Enumerable.Range(0, bytes.Length / 4)
                .All(i => float.IsFinite(BitConverter.ToSingle(bytes, i * 4)))),
            "Local command/PRS is nonfinite before or during derived initialization.");
        uint manager = Pointer(0x765AD8);
        return new(U(0x765BC8), manager, U(manager + 0x28), LocalComponentKeys(), command, position, scale,
            orientation, world, Read(_localCharacter + 0x124, 0x2C), Read(_localMachine + 0x124, 12));
    }

    private void VerifyBloomIsolation(string boundary)
    {
        BloomIsolation before = _bloomIsolation ?? throw new InvalidDataException("Missing derived Bloom private-interval baseline.");
        Require(_layout.IsDerivedBloom && _dependentInterval && !_bloomIntervalVerified,
            "Derived Bloom isolation comparison is outside the single private interval.");
        BloomIsolation after = ReadBloomIsolation();
        bool counterMatches = before.GlobalCounter == after.GlobalCounter,
            managerMatches = before.CharacterManager == after.CharacterManager && before.CachedManagerLink == after.CachedManagerLink,
            tupleMatches = before.LocalComponents == after.LocalComponents,
            commandMatches = before.Command.SequenceEqual(after.Command),
            positionMatches = before.Position.SequenceEqual(after.Position), scaleMatches = before.Scale.SequenceEqual(after.Scale),
            orientationMatches = before.Orientation.SequenceEqual(after.Orientation), worldMatches = before.CachedWorld.SequenceEqual(after.CachedWorld),
            localLinksMatch = before.CharacterLinks.SequenceEqual(after.CharacterLinks),
            machineLinksMatch = before.MachineLinks.SequenceEqual(after.MachineLinks);
        _bloomLastIsolationWitness = new { @event = "template-bloom-private-interval-isolation", boundary,
            global765BC8Before = before.GlobalCounter, global765BC8After = after.GlobalCounter, counterMatches,
            characterManager = Hex(after.CharacterManager), cachedManagerLink28 = Hex(after.CachedManagerLink), managerMatches,
            tupleMatches, commandMatches, positionMatches, scaleMatches, orientationMatches, worldMatches, localLinksMatch, machineLinksMatch,
            localCommandHex = Convert.ToHexString(after.Command), localPositionHex = Convert.ToHexString(after.Position),
            localScaleHex = Convert.ToHexString(after.Scale), localOrientationHex = Convert.ToHexString(after.Orientation),
            localWorldHex = Convert.ToHexString(after.CachedWorld), localCharacterLinksHex = Convert.ToHexString(after.CharacterLinks),
            localMachineLinks124Hex = Convert.ToHexString(after.MachineLinks),
            ownCharacterTicks = _ownCharacterTicks, ownMoveUpdates = _ownMoveUpdates, ownBloomUpdates = _ownBloomUpdates,
            originalGameplayResumed = false, noComparisonAcrossLaterWorldTicks = true };
        _log(_bloomLastIsolationWitness);
        Require(counterMatches && managerMatches && tupleMatches && commandMatches && positionMatches && scaleMatches &&
            orientationMatches && worldMatches && localLinksMatch && machineLinksMatch &&
            _ownCharacterTicks == 0 && _ownMoveUpdates == 0 && _ownBloomUpdates == 0,
            "Derived Bloom changed local state/global counter or executed gameplay inside its private init interval.");
    }

    private void VerifyBloomInstance(bool initialized, IReadOnlyList<Component> components)
    {
        Require(_layout.IsDerivedBloom && _dependentInterval && (!_walk || _bloomControl) && (RetainedBloom ? _idle && _near : !_idle && !_near) && _bloomIsolation is not null,
            "Derived Bloom instance escaped its init-only operation.");
        foreach (Component component in components)
            Table(component.Address, _descriptors.Single(value => value.ClassId == component.ClassId).Table);
        Require(_machine != _localMachine && _character != _localCharacter && U(_character + 0x14C) == 41 &&
            U(_character + 0x134) == 0 && U(_character + 0x13C) == 0,
            "Derived Bloom aliases local gameplay or has unexpected name/optional links.");
        if (initialized)
        {
            _observedBloomKind = U(_machine + 0x128);
            Require(!RetainedBloom || _observedBloomKind == 0, "Retained Bloom requires the actually observed kind0 init cohort.");
            Require(!_bloomControl || U(_machine + 0x130) == 0, "Controlled Bloom requires the actually observed native state0 initialization.");
            Require(_ownBloomFirstPassInit == 1 && _ownBloomInitPasses.Contains(0),
                "Derived Bloom did not witness its native first scoped-init pass.");
            var states = new List<object>();
            foreach (uint index in new uint[] { 0, 18 })
            {
                // 524C40 and 524ED5..524EFB explicitly bind these entries.
                // Observe their actual vtables; do not invent a state class ID.
                uint state = Pointer(_machine + 0x148 + index * 4);
                Require(state != U(_localMachine + 0x148 + index * 4) &&
                    U(state + 0x14) == _machine && U(state + 0x18) == _animator,
                    "Derived Bloom state owner/Animator binding aliases or mismatches.");
                states.Add(new { index, address = Hex(state), table = Hex(Pointer(state)), owner14 = Hex(U(state + 0x14)),
                    animator18 = Hex(U(state + 0x18)), stateTypeNameClaimed = false });
            }
            _bloomStateWitness = new { @event = "template-bloom-init-states", machine = Hex(_machine),
                observedKind128 = Hex(U(_machine + 0x128)), expectedKind128 = (uint?)null,
                enumValueNotAssumed = true, states, bodyRemainsSeresa = true, jumpOrRetainedGameplayQualified = false };
            _log(_bloomStateWitness);
        }
        VerifyBloomIsolation(initialized ? "after-native-init-before-delete" : "after-native-create-before-init");
    }

    private NativeInstancePositionCall CaptureNearDestination()
    {
        VerifyLocal(); VerifyFunction(0x599200);
        Require(_near && _nearPositionWords.Length == 0 && _defaultScene == _scene && U(_scene + 0x14) == _parent &&
            U(_parent + 0x2C) == 0 && U(_parent + 0x3C) == _scene,
            "Near placement requires the local player's actual default-scene System Root frame.");
        Table(_parent, VisualGraphReader.NodeVtable);
        Require((U(_localRoot + 0xB0) & 1) == 0 && (U(_parent + 0xB0) & 1) == 0,
            "Near placement requires clean native local-player and parent world caches.");
        _nearLocalPosition = Read(_localRoot + 0x20, 12);
        _nearWorldPosition = Read(_localRoot + 0x74, 12);
        _nearParentTransform = Read(_parent + 0x20, 0x90);
        _nearPositionWords = PositionWords(_nearLocalPosition);
        Require(FinitePosition(_nearLocalPosition) && FinitePosition(_nearWorldPosition),
            "Near placement source local/world position exceeds its finite diagnostic bound.");
        _log(new { @event = "template-near-native-position-destination", function = Hex(0x599200),
            sourceInstance = Hex(_record), sourceRoot = Hex(_localRoot), parent = Hex(_parent), scene = Hex(_scene),
            localPositionHex = Convert.ToHexString(_nearLocalPosition), worldPositionHex = Convert.ToHexString(_nearWorldPosition),
            byValuePositionWords = _nearPositionWords.Select(Hex).ToArray(), parentTransformHex = Convert.ToHexString(_nearParentTransform),
            sameSystemRootFrameRequiredBeforeCall = true, positionIsLocalNotCameraReference = true,
            templatePayloadSha256 = _layout.Sha256, derivedFixture = _layout.IsDerivedBloom,
            originalPayloadUnchanged = true, nativeResetCalled = false,
            rawRuntimePositionWrites = 0, standingOrCollisionClaimed = false });
        return new(0x599200, _nearPositionWords[0], _nearPositionWords[1], _nearPositionWords[2]);
    }

    private object ObserveNearPlacement(string boundary, bool afterPosition)
    {
        VerifyLocal(); Table(_instance, 0x710F80); Table(_ownRoot, VisualGraphReader.NodeVtable); Table(_move, 0x6F64F8);
        Require(_near && _nearPositionWords.Length == 3 && _defaultScene == _scene && U(_scene + 0x14) == _parent &&
            U(_parent + 0x2C) == 0 && U(_parent + 0x3C) == _scene && U(_instance + 0x24) == _ownRoot &&
            U(_ownRoot + 0x2C) == _parent && U(_ownRoot + 0x3C) == _scene &&
            U(_move + 0x24) == _instance && U(_move + 0x18) == _ownRoot && U(_move + 0x160) == _character && U(_move + 0x1E4) == 0 &&
            Read(_parent + 0x20, 0x90).SequenceEqual(_nearParentTransform) &&
            Read(_localRoot + 0x20, 12).SequenceEqual(_nearLocalPosition) && Read(_localRoot + 0x74, 12).SequenceEqual(_nearWorldPosition),
            "Native position checkpoint lost its own instance/controller, zero-platform or stable local-parent frame.");
        byte[] local = Read(_ownRoot + 0x20, 12), world = Read(_ownRoot + 0x74, 12),
            current = Read(_move + 0x12C, 12), previous = Read(_move + 0x138, 12), worldAnchor = Read(_move + 0x150, 12);
        Require(new[] { local, world, current, previous, worldAnchor }.All(FinitePosition),
            "Native position checkpoint contains a nonfinite or out-of-budget position/cache.");
        bool localMatches = PositionWords(local).SequenceEqual(_nearPositionWords),
            currentMatches = PositionWords(current).SequenceEqual(_nearPositionWords),
            previousMatches = PositionWords(previous).SequenceEqual(_nearPositionWords),
            worldMatches = Enumerable.Range(0, 3).All(i => MathF.Abs(BitConverter.ToSingle(world, i * 4) - BitConverter.ToSingle(_nearWorldPosition, i * 4)) <= 0.001f);
        Membership membership = Memberships(_ownEntities);
        Require(membership.MainListMatches.Length == 0 && membership.TableMatches.Length == 0 &&
            membership.SecondaryListMatches.Order().SequenceEqual(_ownEntities.Order()),
            "Native position changed the pre-scheduler owned component admission.");
        var witness = new { @event = "template-near-native-position-checkpoint", boundary, afterPosition,
            instance = Hex(_instance), root = Hex(_ownRoot), move = Hex(_move), parent = Hex(_parent), scene = Hex(_scene),
            requestedLocalHex = Convert.ToHexString(_nearLocalPosition), expectedWorldHex = Convert.ToHexString(_nearWorldPosition),
            ownLocalHex = Convert.ToHexString(local), ownWorldHex = Convert.ToHexString(world),
            currentCache12CHex = Convert.ToHexString(current), previousCache138Hex = Convert.ToHexString(previous),
            worldAnchor150Hex = Convert.ToHexString(worldAnchor), localMatches, currentMatches, previousMatches, worldMatches,
            worldTolerance = 0.001f, platform1E4 = Hex(U(_move + 0x1E4)), membership,
            instanceFlags0C = Hex(U(_instance + 0xC)), characterMask1A = Hex(U(_instance + 0xC) & 0x1A), moveMask18 = Hex(U(_instance + 0xC) & 0x18),
            nativePositionOnly = true, nativeResetCalled = false, protectedPlatformDetachSideEffectsQualified = false,
            originalSchedulerResumed = false, rawRuntimePositionWrites = 0, standingCollisionOrMovementQualified = false };
        _log(witness);
        Require(!afterPosition || (localMatches && currentMatches && worldMatches),
            "Native instance position did not establish requested local/current and native world positions.");
        return witness;
    }

    private void ObservePositionedInstance(uint value)
    {
        Require(_near && value == _instance && !_nearPositioned && _dependentInterval,
            "Unexpected native positioned-instance checkpoint.");
        _nearAfterPlacement = ObserveNearPlacement("after-native-position-before-original-scheduler", afterPosition: true);
        if (RetainedBloom)
        {
            _ = ReadNeutralCommand();
            _bloomNeutralCommand = Read(_ownCommand, 0x74);
            _log(new { @event = "template-bloom-retained-neutral-baseline", command = Hex(_ownCommand),
                commandHex = Convert.ToHexString(_bloomNeutralCommand), nativeInputWrites = 0, originalGameplayResumed = false });
        }
        _ = ObserveIdleState("positioned-before-original-scheduler", requirePromotion: false);
        _nearPositioned = true;
    }

    private static uint[] PositionWords(byte[] bytes) => Enumerable.Range(0, 3).Select(i => BitConverter.ToUInt32(bytes, i * 4)).ToArray();
    private static bool FinitePosition(byte[] bytes) => bytes.Length == 12 && Enumerable.Range(0, 3)
        .All(i => float.IsFinite(BitConverter.ToSingle(bytes, i * 4)) && MathF.Abs(BitConverter.ToSingle(bytes, i * 4)) <= 10000000);

    private void VerifyLocal()
    {
        Require(U(0x765AD4) == _profile && U(_profile + 0x2B4) == _record && U(_record) == 0x710F80 &&
            U(_record + 0x24) == _localRoot && U(_record + 0x20) == 11 && U(_localRoot + 0x2C) == _parent && U(_localRoot + 0x3C) == _scene &&
            U(0x7552A0) == _input && U(_input + 0x18) == _command && U(_input + 0x1C) == _device &&
            U(_input + 0x14) == _inputReceiver && U(0x755274) == _engine && U(_engine + 0x1C) == _camera &&
            U(_engine + 0x18) == _defaultScene && U(0x75DC64) == _templateManager &&
            U(_templateManager + 0x20) == 0 && U(0x765ACC) == _gameManager && U(0x755278) == _fileManager &&
            U(0x755294) == _flow && U(_flow + 0x1B0) == _level && U(_flow + 0x1B8) == 0 && U(_active + 0x10) == _level,
            "Local profile/record/input/device/camera/scene/parse identity changed during native template experiment.");
        int stack = unchecked((int)U(_flow + 0x1AC));
        Require(stack is >= 0 and <= 20 && U(_flow + 0x15C + (uint)stack * 4) == _active &&
            U(Pointer(0x75DB90) + 0x20) == 0 && Read(_fileManager + 0x21C, 1)[0] == 0,
            "Active flow/world/template-setup phase changed during the experiment.");
        Table(_command, 0x703090); Table(_input, 0x6F5508); Table(_device, 0x7116CC);
        if (_initialize) Require(U(_localAnimator + 0x128) == _localActor && U(_localActor) == 0x703F80,
            "Local Animator/Actor identity changed during scoped NPC initialization.");
        if (_idle || _layout.IsDerivedBloom) Require(LocalComponentKeys() == _localComponentKeys && U(_device + 0x14) == 3,
            "Stock local component/input-context identity changed during the original-scheduler observation.");
    }

    private string LocalComponentKeys()
    {
        uint head = Pointer(_record + 0x1C), link = Pointer(head), previous = head;
        var seenLinks = new HashSet<uint>(); var seenItems = new HashSet<uint>(); var keys = new List<string>(11);
        for (int i = 0; i < 11; ++i)
        {
            Require(link != head && seenLinks.Add(link) && U(link + 4) == previous,
                "Local component list is not bounded/distinct/reciprocal.");
            uint item = Pointer(link + 8);
            Require(seenItems.Add(item), "Local component alias encountered.");
            keys.Add($"{link:X8}:{item:X8}:{U(item):X8}");
            uint next = Pointer(link); Require(U(next + 4) == link, "Local component forward/back link mismatch.");
            previous = link; link = next;
        }
        Require(link == head && U(head + 4) == previous, "Local component sentinel/count mismatch.");
        return $"{head:X8}:" + string.Join(";", keys);
    }

    private void VerifyStream()
    {
        Table(_stream, 0x6E7E50);
        Require(U(_stream + 0x1C) == PayloadBytes && U(_stream + 0x20) == PayloadBytes && U(_stream + 0x2C) == 0 &&
            Read(_stream + 0x34, 1)[0] == 1 && Refs(_stream) == _streamRefs,
            "Owned stream extent/position/ownership mismatch.");
        _ = Read(Pointer(_stream + 0x30), PayloadBytes);
    }

    private void InspectTemplate()
    {
        VerifyLocal(); Table(_template, 0x703D24);
        Require(U(_template + 0x1C) == 6 && _templateRefs == 1 && Refs(_template) == _templateRefs && U(_template + 4) == 0,
            "Parsed template count/constructor registration/base-list mismatch.");
        uint descriptor = Pointer(_template + 0x14);
        var visited = new HashSet<uint>(); var rows = new List<object>();
        foreach (NativeTemplateDescriptor expected in _descriptors)
        {
            Require(visited.Add(descriptor) && ClassId(descriptor) == 0x014E1394 && U(descriptor + 0x20) == expected.Type &&
                U(descriptor + 0x12C) == expected.Id && U(descriptor + 0x130) == uint.MaxValue,
                "Native descriptor identity/type/parent mismatch.");
            string? hash = null;
            if (expected.Type == 1)
            {
                uint stream = Pointer(descriptor + 0x24); Table(stream, 0x6E7E50);
                Require(U(stream + 0x20) == expected.PropertyBytes, "Native property stream size mismatch.");
                byte[] bytes = Read(Pointer(stream + 0x30), expected.PropertyBytes);
                _log(new { @event = "template-native-property-stream", descriptor = Hex(descriptor), id = expected.Id,
                    stream = Hex(stream), bytes = bytes.Length, headerHex = Convert.ToHexString(bytes.AsSpan(0, 24)),
                    actualSha256 = Convert.ToHexString(SHA256.HashData(bytes)),
                    expectedSha256 = Convert.ToHexString(SHA256.HashData(_payload.AsSpan(expected.PropertyOffset, expected.PropertyBytes))) });
                Require(BitConverter.ToUInt32(bytes, 0) == 0x74474539 && BitConverter.ToUInt32(bytes, 4) == 2 &&
                    BitConverter.ToUInt32(bytes, 8) == expected.ClassId &&
                    bytes.SequenceEqual(_payload.AsSpan(expected.PropertyOffset, expected.PropertyBytes).ToArray()),
                    "Native scalar descriptor stream differs from the pinned original payload.");
                hash = Convert.ToHexString(SHA256.HashData(bytes));
            }
            else
            {
                byte[] actualPath = Read(descriptor + 0x2C, 0x100);
                byte[] expectedPath = _payload.AsSpan(expected.Offset + 4, 0x100).ToArray();
                int actualNul = Array.IndexOf(actualPath, (byte)0), expectedNul = Array.IndexOf(expectedPath, (byte)0);
                bool logicalMatch = actualNul >= 0 && actualNul == expectedNul &&
                    actualPath.AsSpan(0, actualNul + 1).SequenceEqual(expectedPath.AsSpan(0, expectedNul + 1));
                _log(new { @event = "template-native-asset-path", descriptor = Hex(descriptor), id = expected.Id,
                    nativePathAddress = Hex(descriptor + 0x2C), sourcePathOffset = expected.Offset + 4,
                    boundedFieldBytes = 256, actualFullHex = Convert.ToHexString(actualPath), expectedFullHex = Convert.ToHexString(expectedPath),
                    actualNulOffset = actualNul, expectedNulOffset = expectedNul,
                    actualPath = actualNul >= 0 ? System.Text.Encoding.Latin1.GetString(actualPath, 0, actualNul) : null,
                    expectedPath = expectedNul >= 0 ? System.Text.Encoding.Latin1.GetString(expectedPath, 0, expectedNul) : null,
                    actualPaddingHex = actualNul >= 0 ? Convert.ToHexString(actualPath.AsSpan(actualNul + 1)) : null,
                    expectedPaddingHex = expectedNul >= 0 ? Convert.ToHexString(expectedPath.AsSpan(expectedNul + 1)) : null,
                    rawFieldMatches = actualPath.SequenceEqual(expectedPath), logicalPathMatches = logicalMatch,
                    comparison = "exact-bytes-through-first-NUL; native5FCF00..5FCF08 does not copy following padding" });
                // Binary reader5FCEF2 selects descriptor+2C, then5FCF00..08
                // copies the source C string including NUL, not all256 bytes.
                // Preserve/report the untouched tail without imposing disk padding.
                Require(logicalMatch, "Native bounded NUL-terminated body asset path differs from the pinned descriptor.");
            }
            rows.Add(new { descriptor = Hex(descriptor), id = expected.Id, type = expected.Type, classId = Hex(expected.ClassId), propertySha256 = hash });
            descriptor = U(descriptor + 0x18);
        }
        Require(descriptor == 0, "Parsed descriptor chain exceeds six stock entries.");
        _log(new { @event = "template-native-parse-witness", template = Hex(_template), referenceCount = _templateRefs,
            descriptorCount = 6, descriptors = rows, parseContext = Hex(U(_templateManager + 0x20)) });
    }

    private void CaptureInstance(bool initialized = false, bool allowSecondary = false)
    {
        VerifyLocal(); Table(_instance, 0x710F80);
        _log(new { @event = "template-create-return-prefix", instance = Hex(_instance), template = Hex(_template),
            prefixHex = Convert.ToHexString(Read(_instance, 0x28)), ownBuildWitnesses = _buildWitnesses, initialized,
            ownNotify1D = _ownNotify1D, ownCharacterInit = _ownCharacterInit });
        Require(_instance != _record && U(_instance + 0x14) == _template && U(_instance + 0x20) == 6,
            "Constructed native instance identity/template/count mismatch.");
        _instanceRefs = Refs(_instance); Require(_instanceRefs == 0, "Fresh instance has unqualified incoming owners.");
        _ownRoot = Pointer(_instance + 0x24); Table(_ownRoot, VisualGraphReader.NodeVtable);
        Require(!_localNodes.Contains(_ownRoot) && U(_ownRoot + 0x2C) == _parent && U(_ownRoot + 0x3C) == _scene,
            "Constructed root scene/parent/identity mismatch.");
        _log(new { @event = "template-create-return-root", root = Hex(_ownRoot), referenceCount = Refs(_ownRoot),
            prefixHex = Convert.ToHexString(Read(_ownRoot, 0xB4)) });
        List<Component> items = Components(_instance, 6);
        _log(new { @event = "template-create-return-components", instance = Hex(_instance), components = items.ToArray() });
        uint Unique(uint id) => items.Single(v => v.ClassId == id).Address;
        _character = Unique(0x0003CC73); _traits = Unique(0x1BC6108E); _machine = Unique(_layout.MachineClassId);
        _animator = Unique(0xCD2B2876); _move = Unique(0x680838E4);
        _ownEntities = [_character, _traits, _machine, _animator, _move];
        _log(new { @event = "template-create-return-character", character = Hex(_character),
            ownFieldsHex = Convert.ToHexString(Read(_character + 0x124, 0x3C)),
            move160 = Hex(U(_move + 0x160)), animatorActor128 = Hex(U(_animator + 0x128)) });
        Require(items.Count(v => v.ClassId == 0x695C0F65) == 1 && _ownEntities.All(v => U(v + 0x24) == _instance && U(v + 0x18) == _ownRoot),
            "Constructed components lack the real own instance/Node binding.");
        // Instance owns one root reference, scene parent owns one and each of
        // the five stock Entity virtual24 bindings retains that same root.
        Require(Refs(_ownRoot) == 7, "Own root has an unqualified reference-owner count.");
        uint ownCommand = Pointer(_character + 0x130); Table(ownCommand, 0x703090);
        byte[] command = Read(ownCommand, 0x74);
        _log(new { @event = "template-create-return-private-command", command = Hex(ownCommand),
            commandHex = Convert.ToHexString(command), localCommand = Hex(_command) });
        Require(new[] { 4, 8, 12, 16, 20 }.All(o => float.IsFinite(BitConverter.ToSingle(command, o))),
            "Owned command has a nonfinite known float field.");
        Require(U(_character + 0x148) == 1 && ownCommand != _command && U(_character + 0x124) == _machine &&
            U(_character + 0x128) == _animator && U(_character + 0x12C) == _move && U(_character + 0x138) == _traits &&
            U(_character + 0x13C) == 0 && _ownNotify1D == 1 && _ownCharacterInit == 1,
            "Owned selector1/private-command/component setup did not match native typed witnesses.");
        var graph = new VisualGraphReader(_host.ProcessHandle).Read(_ownRoot, _localNodes, inspectSkins: false);
        Require(graph.Success, "Constructed graph is not bounded/disjoint: " + graph.Error);
        Membership membership = Memberships(_ownEntities);
        uint actor = U(_animator + 0x128);
        var witness = new { instance = Hex(_instance), template = Hex(_template), root = Hex(_ownRoot), initialized,
            instanceReferenceCount = _instanceRefs, rootReferenceCount = Refs(_ownRoot), parent = Hex(U(_ownRoot + 0x2C)), scene = Hex(U(_ownRoot + 0x3C)),
            instancePrefixHex = Convert.ToHexString(Read(_instance, 0x28)), components = items.ToArray(),
            characterOwnHex = Convert.ToHexString(Read(_character + 0x124, 0x3C)), privateCommand = Hex(ownCommand),
            commandHex = Convert.ToHexString(command), commandFloatWords = new[] { 4, 8, 12, 16, 20 }.Select(o => FloatWord(command, o)).ToArray(),
            move160 = Hex(U(_move + 0x160)), animatorActor128 = Hex(U(_animator + 0x128)),
            animatorCharacter124 = Hex(U(_animator + 0x124)), machineOwnHex = Convert.ToHexString(Read(_machine + 0x124, 0x24)),
            observedMachineKind128 = Hex(U(_machine + 0x128)), expectedMachineKind128 = _layout.ExpectedKind,
            actorPrefixHex = initialized && actor >= 0x10000 ? Convert.ToHexString(Read(actor, 0x54)) : null,
            nodeCount = graph.Graph.Nodes.Count, manager = membership, ownNotify1D = _ownNotify1D, ownCharacterInit = _ownCharacterInit,
            noSchedulerAbsenceClaim = true, directManualNotify1C = false, scopedNativeInitialization = initialized };
        if (initialized) _initializedWitness = witness; else _constructionWitness = witness;
        _log(new { @event = initialized ? "template-native-initialized-witness" : "template-native-construction-witness", witness });
        if (initialized)
        {
            List<Relationship> relationships =
            [
                Compare("scoped-mode", _initialize ? 1u : 0u, 1),
                Compare("private-interval-active", _dependentInterval ? 1u : 0u, 1),
                Compare("Character.Notify1C-count", _ownNotify1C, 1),
                Compare("MoveCtrl.init-count", _ownMoveInit, 1),
                Compare("Animator.init-count", _ownAnimatorInit, 1),
                Compare("MoveCtrl+160.Character", U(_move + 0x160), _character),
                Compare("Animator+124.Character", U(_animator + 0x124), _character),
                Compare("Machine+124.Character", U(_machine + 0x124), _character),
                Compare("Machine+12C.private-command", U(_machine + 0x12C), ownCommand),
                Compare("Animator.Actor-readable-pointer", actor >= 0x10000 ? 1u : 0u, 1),
                Compare("Animator.Actor-distinct-from-local", actor != _localActor ? 1u : 0u, 1),
                Compare("Animator.Actor-vtable", actor >= 0x10000 ? U(actor) : 0, 0x703F80)
            ];
            if (_layout.ExpectedKind is uint expectedKind)
                relationships.Add(Compare("Machine+128.AnimationGroup", U(_machine + 0x128), expectedKind));
            _log(new { @event = "template-scoped-init-relationships", instance = Hex(_instance),
                actor = Hex(actor), localActor = Hex(_localActor), animationGroupSourceOffset = _layout.KindSourceOffset,
                observedMachineKind128 = Hex(U(_machine + 0x128)), expectedMachineKind128 = _layout.ExpectedKind,
                comparisons = relationships, allMatched = relationships.All(value => value.Matched) });
            Require(relationships.All(value => value.Matched),
                "Scoped initialization reciprocal relationships failed: " +
                string.Join(", ", relationships.Where(value => !value.Matched).Select(value => value.Operand)));
            Table(actor, 0x703F80);
        }
        // Admission is observed, not inferred from the lack of explicit Add.
        if (allowSecondary)
            Require(_initialize && _dependentInterval && membership.MainListMatches.Length == 0 && membership.TableMatches.Length == 0 &&
                membership.SecondaryListMatches.Order().SequenceEqual(_ownEntities.Order()),
                "Scoped lifecycle has unexpected own manager admission before gameplay resume.");
        else
            Require(membership.Matches.Length == 0, "Own components were admitted to game-manager tables during construction; stop before an unqualified update.");
        if (_layout.IsDerivedBloom) VerifyBloomInstance(initialized, items);
    }

    private void VerifyOwnedInstance(bool scheduled = false)
    {
        Membership membership = Memberships(_ownEntities);
        bool cleanupMembership = LiveOwnMembership(membership) || (RetainedBloom && _bloomRetentionFailure.Length != 0 &&
            (membership.MainListMatches.Order().SequenceEqual(_ownEntities.Order()) && membership.SecondaryListMatches.Length == 0 ||
             membership.SecondaryListMatches.Order().SequenceEqual(_ownEntities.Order()) && membership.MainListMatches.Length == 0));
        Table(_instance, 0x710F80); Require(U(_instance + 0x14) == _template && U(_instance + 0x24) == _ownRoot &&
            U(_ownRoot + 0x2C) == _parent && U(_ownRoot + 0x3C) == _scene && U(_instance + 0x20) == 6 &&
            Refs(_ownRoot) == 7 && (scheduled
                ? cleanupMembership
                : membership.Matches.Length == 0),
            "Own instance scene/count/scheduling changed before deletion.");
    }

    private Membership ObserveIdleState(string boundary, bool requirePromotion)
    {
        VerifyLocal();
        Table(_instance, 0x710F80); Table(_character, 0x6F6438); Table(_move, 0x6F64F8);
        Table(_animator, 0x6F670C); Table(_ownCommand, 0x703090); Table(_ownActor, 0x703F80);
        Require(U(_instance + 0x14) == _template && U(_instance + 0x24) == _ownRoot &&
            U(_instance + 0x20) == 6 && Refs(_instance) == _instanceRefs && Refs(_ownRoot) == 7 &&
            U(_ownRoot + 0x2C) == _parent && U(_ownRoot + 0x3C) == _scene,
            "Retained original-scheduler instance lost its qualified scene/count/owners.");
        List<Component> components = Components(_instance, 6);
        Require(_ownEntities.All(v => components.Count(c => c.Address == v && c.Owner == _instance && c.BoundNode == _ownRoot) == 1),
            "Retained native components changed identity or Node binding.");
        bool kindMatches = _layout.ExpectedKind is uint expectedKind ? U(_machine + 0x128) == expectedKind :
            RetainedBloom && _observedBloomKind == 0 && U(_machine + 0x128) == _observedBloomKind;
        Require(U(_character + 0x124) == _machine && U(_character + 0x128) == _animator &&
            U(_character + 0x12C) == _move && U(_character + 0x130) == _ownCommand && U(_character + 0x138) == _traits &&
            U(_character + 0x148) == 1 && U(_character + 0x13C) == 0 &&
            U(_move + 0x160) == _character && U(_animator + 0x124) == _character && U(_animator + 0x128) == _ownActor &&
            U(_machine + 0x124) == _character && kindMatches && U(_machine + 0x12C) == _ownCommand &&
            _ownCommand != _command && _ownActor != _localActor,
            "Retained Character/Move/FSM/Animator/private-input relationships changed.");
        byte[] command = ReadGuardedCommand(), root = Read(_ownRoot, 0xB4), move = Read(_move + 0x128, 0x2D4 - 0x128);
        int[] rootFloatOffsets = [0x20, 0x24, 0x28, 0x30, 0x34, 0x38, 0x40, 0x44, 0x48, 0x4C, 0x50, 0x54, 0x58, 0x5C, 0x60,
            0x74, 0x78, 0x7C, 0x80, 0x84, 0x88, 0x8C, 0x90, 0x94, 0x98, 0x9C, 0xA0, 0xA4, 0xA8, 0xAC];
        Require(rootFloatOffsets.All(o => float.IsFinite(BitConverter.ToSingle(root, o))), "Retained root PRS contains a nonfinite field.");
        int[] moveFloatOffsets = [0x128, 0x12C, 0x130, 0x134, 0x138, 0x13C, 0x140, 0x150, 0x154, 0x158,
            0x188, 0x18C, 0x190, 0x194, 0x198, 0x19C, 0x1B0, 0x1B4, 0x1B8];
        Require(moveFloatOffsets.All(o => float.IsFinite(BitConverter.ToSingle(move, o - 0x128))),
            "Retained native controller position/rate contains a nonfinite field.");
        Require(new[] { 0x20, 0x24, 0x28, 0x74, 0x78, 0x7C }.All(o => MathF.Abs(BitConverter.ToSingle(root, o)) <= 10000000),
            "Retained instance exceeded the explicit diagnostic position budget.");
        Membership membership = Memberships(_ownEntities);
        bool promoted = LiveOwnMembership(membership);
        if (_idleWorldStops is 0 or 1 or 2 or 10 or IdleObservationWorldStops || _idleWorldStops == ObservationWorldLimit)
            _log(new { @event = "template-original-scheduler-snapshot", boundary, originalWorldReturns = _idleWorldStops,
                instance = Hex(_instance), root = Hex(_ownRoot), character = Hex(_character), move = Hex(_move),
                privateCommand = Hex(_ownCommand), actor = Hex(_ownActor), ownCharacterTicks = _ownCharacterTicks, ownMoveUpdateEntries = _ownMoveUpdates,
                ownTypedDeliveryChecks = _ownDeliveryChecks, ownTypedDeliveryPredicateFalse = _ownDeliveryPredicateFalse,
                instancePrefixHex = Convert.ToHexString(Read(_instance, 0x28)), localInstancePrefixHex = Convert.ToHexString(Read(_record, 0x28)),
                entityActivationPrefixes = _ownEntities.Select(entity => new { entity = Hex(entity), table = Hex(U(entity)),
                    basePrefixHex = Convert.ToHexString(Read(entity, 0x40)) }).ToArray(),
                managerReferenceAndLocalPositionHex2C = Convert.ToHexString(Read(_gameManager + 0x2C, 0x28)),
                rootPrefixHex = Convert.ToHexString(root), moveStartOffset = Hex(0x128), moveHex = Convert.ToHexString(move),
                characterOwnHex = Convert.ToHexString(Read(_character + 0x124, 0x3C)),
                machineOwnHex = Convert.ToHexString(Read(_machine + 0x124, 0x24)), commandHex = Convert.ToHexString(command),
                commandFloatWords = new[] { 4, 8, 12, 16, 20 }.Select(o => FloatWord(command, o)).ToArray(),
                membership, promoted, components, originalOriginWalkabilityAssumed = false,
                localComponentKeys = _localComponentKeys, inputReceiver14 = Hex(_inputReceiver), defaultCamera1C = Hex(_camera),
                rawNodeOrControllerWrites = 0, manualNativeUpdateCalls = 0 });
        Require(!requirePromotion || promoted,
            "Original scheduler did not establish all own entities in the live main list and own MoveCtrl in its counted active array after two world returns.");
        return membership;
    }

    private bool LiveOwnMembership(Membership membership) => membership.SecondaryListMatches.Length == 0 &&
        membership.MainListMatches.Order().SequenceEqual(_ownEntities.Order()) && membership.ActiveMoveMatches.Contains(_move);

    private byte[] ReadGuardedCommand() => _commandMovement?.Active == true ? _commandMovement.ReadQualifiedCommand() : ReadNeutralCommand();

    private void VerifyWalkOwnership()
    {
        VerifyLocal();
        Require(_walk && _idleActive && !_dependentInterval && _stage == 6 && _ownCommand != _command && _ownRoot != _localRoot,
            "Private command access is outside its own retained walk interval.");
        Table(_instance, 0x710F80); Table(_ownRoot, VisualGraphReader.NodeVtable); Table(_character, 0x6F6438);
        Table(_move, 0x6F64F8); Table(_machine, _layout.MachineTable); Table(_ownCommand, 0x703090);
        bool machineKindMatches = _bloomControl
            ? _layout.IsDerivedBloom && _layout.MachineTable == 0x6FA288 && _observedBloomKind == 0 && U(_machine + 0x128) == 0
            : !_layout.IsDerivedBloom && _layout.MachineTable == 0x6FB280 && U(_machine + 0x128) == 0x40;
        Require(U(_instance + 0x14) == _template && U(_instance + 0x24) == _ownRoot && U(_instance + 0x20) == 6 &&
            U(_ownRoot + 0x2C) == _parent && U(_ownRoot + 0x3C) == _scene &&
            U(_character + 0x24) == _instance && U(_character + 0x18) == _ownRoot && U(_character + 0x148) == 1 && U(_character + 0x14C) == 41 &&
            U(_character + 0x124) == _machine && U(_character + 0x12C) == _move && U(_character + 0x130) == _ownCommand &&
            U(_move + 0x24) == _instance && U(_move + 0x18) == _ownRoot && U(_move + 0x160) == _character &&
            U(_machine + 0x24) == _instance && U(_machine + 0x124) == _character && machineKindMatches && U(_machine + 0x12C) == _ownCommand &&
            U(_character + 0x128) == _animator && U(_character + 0x138) == _traits && U(_character + 0x134) == 0 && U(_character + 0x13C) == 0 &&
            U(_animator + 0x124) == _character && U(_animator + 0x128) == _ownActor && _ownActor != _localActor,
            "Private command/Character/Move/FSM/Animator/instance bindings changed.");
        if (_bloomControl) Require(U(_localCharacter + 0x124) == _localMachine && U(_localCharacter + 0x130) == _command &&
            U(_localMachine + 0x124) == _localCharacter && U(_localMachine + 0x12C) == _command,
            "Private controlled Bloom lost the qualified local Bloom/input separation.");
    }

    private byte[] ReadNeutralCommand()
    {
        byte[] command = Read(_ownCommand, 0x74);
        bool finite = new[] { 4, 8, 12, 16, 20 }.All(o => float.IsFinite(BitConverter.ToSingle(command, o)));
        bool neutral = finite && BitConverter.ToSingle(command, 4) == 0 && command[0x1A] == 0;
        if (!neutral) _log(new { @event = "template-neutral-command-guard-failed", privateCommand = Hex(_ownCommand),
            commandHex = Convert.ToHexString(command), commandFloatWords = new[] { 4, 8, 12, 16, 20 }.Select(o => FloatWord(command, o)).ToArray(),
            jumpHeld1A = command[0x1A], originalWorldReturns = _idleWorldStops, beforeFurtherNativeConsumption = true });
        Require(finite, "Owned original-scheduler command contains a nonfinite known request field.");
        if (_bloomIdle && _idleActive && _bloomNeutralCommand is not null)
        {
            bool intentUnchanged = command.AsSpan(4, 8).SequenceEqual(_bloomNeutralCommand.AsSpan(4, 8)) &&
                command.AsSpan(0x14, 4).SequenceEqual(_bloomNeutralCommand.AsSpan(0x14, 4)) &&
                command[0x1A] == _bloomNeutralCommand[0x1A];
            if (!neutral || !intentUnchanged)
                RejectBloomRetention("Original Bloom changed its private neutral intent before or after native consumption.");
        }
        else Require(neutral, "Owned original-scheduler command is no longer a finite neutral movement/jump request.");
        return command;
    }

    private void RejectBloomRetention(string reason)
    {
        if (_bloomRetentionFailure.Length != 0) return;
        _bloomRetentionFailure = reason;
        _log(new { @event = "template-bloom-retention-rejected", reason, originalWorldReturns = _idleWorldStops,
            privateCommand = Hex(_ownCommand), noRepairWrites = true, cleanupAtNextSafeWorldReturn = true });
    }

    private void LogRetainedBloom(string boundary, uint tid)
    {
        Require(RetainedBloom && _idleActive && !_dependentInterval && _bloomIntervalVerified && _stage == 6,
            "Bloom retained witness is outside the original scheduler interval.");
        VerifyLocal(); Table(_character, 0x6F6438); Table(_machine, 0x6FA288); Table(_move, 0x6F64F8);
        Table(_animator, 0x6F670C); Table(_ownCommand, 0x703090); Table(_ownActor, 0x703F80);
        Require(U(_character + 0x124) == _machine && U(_character + 0x128) == _animator &&
            U(_character + 0x12C) == _move && U(_character + 0x130) == _ownCommand && U(_character + 0x138) == _traits &&
            U(_character + 0x148) == 1 && U(_character + 0x14C) == 41 && U(_character + 0x134) == 0 && U(_character + 0x13C) == 0 &&
            U(_machine + 0x124) == _character && U(_machine + 0x128) == 0 && _observedBloomKind == 0 &&
            U(_machine + 0x12C) == _ownCommand && U(_move + 0x160) == _character &&
            U(_animator + 0x124) == _character && U(_animator + 0x128) == _ownActor &&
            _ownCommand != _command && _ownActor != _localActor &&
            U(_localCharacter + 0x124) == _localMachine && U(_localCharacter + 0x130) == _command &&
            U(_localMachine + 0x124) == _localCharacter && U(_localMachine + 0x12C) == _command &&
            LocalComponentKeys() == _localComponentKeys,
            "Retained Bloom lost private component/input/local-player separation.");
        Require(++_bloomEntryOrdinal <= ObservationEventLimit, "Retained Bloom observational event cap exceeded.");
        byte[] command = ReadGuardedCommand();
        uint counter = U(0x765BC8);
        _log(new { @event = "template-bloom-original-scheduler-witness", boundary, threadId = tid,
            ordinal = _bloomEntryOrdinal, originalWorldReturns = _idleWorldStops, machine = Hex(_machine),
            kind128 = U(_machine + 0x128), currentState130 = U(_machine + 0x130),
            packed140 = Hex(U(_machine + 0x140)), packed144 = Hex(U(_machine + 0x144)), field29C = Hex(U(_machine + 0x29C)),
            counterBeforeObservedFrame = _bloomLastWorldCounter, counterAtBoundary = counter,
            globalCounterEqualityRequired = false, commandHex = Convert.ToHexString(command),
            ownCharacterTicks = _ownCharacterTicks, ownBloomNotifyTicks = _ownBloomNotifyTicks,
            ownBloomUpdateEntries = _ownBloomUpdates, ownMoveUpdateEntries = _ownMoveUpdates,
            privateCommandWriterActive = _bloomControl && _commandMovement?.Active == true, protectedTailSuppressed = false });
        if (boundary == "after-original-world") _bloomLastWorldCounter = counter;
    }

    private void InstallDeliveryWitness()
    {
        Require(_idle && _initialize && !_dependentInterval && _idleActive && _idleWorldStops == 0 && !_deliveryWitnessInstalled,
            "Typed-delivery witness must install once at the first real WorldRET after scoped native init.");
        // Verified dispatcher ABI: this=GameManager; args=(message*, entity*,
        // predicate low byte, force low byte). Install late to avoid trapping
        // every pre-existing entity during game startup.
        Hook(0x573D50, "GameManager.typed-delivery-filter", (uint tid, uint at, ref Win32Native.X86Context ctx) =>
        {
            if (!_idleActive || _instance == 0) return;
            uint entity = U(ctx.Esp + 8);
            if (!_ownEntities.Contains(entity)) return;
            uint message = Pointer(ctx.Esp + 4), code = U(message);
            if (code != 0x1E) return;
            Require(_idleActive && !_dependentInterval && _stage == 6 && ctx.Ecx == _gameManager && U(entity + 0x24) == _instance,
                "Own typed-delivery candidate appeared outside its retained original-scheduler interval.");
            Require(++_ownDeliveryChecks <= ObservationEventLimit, "Own typed-delivery diagnostic witness cap exceeded.");
            byte predicate = Read(ctx.Esp + 0xC, 1)[0], forceArgument = Read(ctx.Esp + 0x10, 1)[0], forceManager = Read(_gameManager + 0x55F4, 1)[0];
            if (predicate == 0) ++_ownDeliveryPredicateFalse;
            uint flags = U(_instance + 0xC), predicateEntry = U(U(entity) + 0x2C);
            bool? expectedFromKnownNativePredicate = predicateEntry == 0x4F5000 ? (flags & 0x18) == 0 :
                predicateEntry == 0x4F4070 && U(entity + 0x148) == 1 ? (flags & 0x1A) == 0 : null;
            byte[] reference = Read(_gameManager + 0x2C, 12), world = Read(_ownRoot + 0x74, 12);
            double squaredDistance = Enumerable.Range(0, 3).Sum(i =>
            {
                double difference = (double)BitConverter.ToSingle(world, i * 4) - BitConverter.ToSingle(reference, i * 4);
                return difference * difference;
            });
            _log(new { @event = "template-own-typed-delivery-candidate", threadId = tid, manager = Hex(ctx.Ecx), entity = Hex(entity),
                entityTable = Hex(U(entity)), instance = Hex(_instance), message = Hex(code), predicateEntry = Hex(predicateEntry),
                predicateLowByte = predicate, forceArgumentLowByte = forceArgument, forceManagerByte55F4 = forceManager,
                instanceFlags0C = Hex(flags), characterMask1A = Hex(flags & 0x1A), moveMask18 = Hex(flags & 0x18),
                expectedFromKnownNativePredicate, recordedPredicateMatchesKnown = expectedFromKnownNativePredicate is null ? (bool?)null :
                    (predicate != 0) == expectedFromKnownNativePredicate.Value,
                entityBasePrefixHex = Convert.ToHexString(Read(entity, 0x40)),
                occlusionEnabled38 = Read(entity + 0x38, 1)[0], pauseEnabled39 = Read(entity + 0x39, 1)[0], distanceSquared3C = FloatWord(Read(entity + 0x3C, 4), 0),
                nativeReferencePointHex = Convert.ToHexString(reference), ownWorldPositionHex = Convert.ToHexString(world),
                diagnosticSquaredDistance = double.IsFinite(squaredDistance) ? (double?)squaredDistance : null,
                diagnosticUsesDoubleNotExactNativeX87 = true, originalWorldReturns = _idleWorldStops,
                messageDispatchWillRemainNative = true, noActivationOrPredicateOverride = true });
        });
        _deliveryWitnessInstalled = true;
    }

    private List<Component> Components(uint record, uint count)
    {
        uint head = Pointer(record + 0x1C), link = Pointer(head), previous = head;
        var links = new HashSet<uint>(); var addresses = new HashSet<uint>(); var result = new List<Component>();
        for (uint i = 0; i < count; ++i)
        {
            Require(link != head && links.Add(link) && U(link + 4) == previous, "Instance component list is not distinct/reciprocal.");
            uint item = Pointer(link + 8); Require(addresses.Add(item) && !_localNodes.Contains(item), "Instance component aliases another object.");
            uint id = ClassId(item); bool node = id == 0x695C0F65;
            Require(_descriptors.Count(d => d.ClassId == id) == 1, "Unexpected instance component class.");
            result.Add(new(item, U(item), id, node ? 0 : U(item + 0x24), node ? U(item + 0x2C) : U(item + 0x18)));
            uint next = Pointer(link); Require(U(next + 4) == link, "Instance component forward/back link mismatch.");
            previous = link; link = next;
        }
        Require(link == head && U(head + 4) == previous, "Instance list count/sentinel mismatch.");
        return result;
    }

    private Membership Memberships(uint[] own)
    {
        Table(_gameManager, 0x70069C);
        byte[] tables = Read(_gameManager + 0x54, 0x5280);
        uint[] counts = Enumerable.Range(0, 40).Select(i => BitConverter.ToUInt32(tables, i * 4)).ToArray();
        Require(counts.All(v => v <= 130), "Unqualified game-manager group count.");
        byte[] pending = Read(_gameManager + 0x52D4, 200 * 4), move = Read(_gameManager + 0x55F8, 130 * 4);
        uint moveCount = U(_gameManager + 0x5918); Require(moveCount <= 130, "Unqualified MoveCtrl manager count.");
        var values = new HashSet<uint>();
        foreach (byte[] block in new[] { tables.AsSpan(0x140, 40 * 130 * 4).ToArray(), pending, move })
            for (int i = 0; i < block.Length; i += 4) values.Add(BitConverter.ToUInt32(block, i));
        uint mainCount = U(_gameManager + 0x1C), secondaryCount = U(_gameManager + 0x28);
        uint[] main = ManagerList(Pointer(_gameManager + 0x18), mainCount, own);
        uint[] secondary = ManagerList(Pointer(_gameManager + 0x24), secondaryCount, own);
        uint[] tableMatches = own.Where(values.Contains).ToArray();
        var activeMoves = new HashSet<uint>(Enumerable.Range(0, checked((int)moveCount))
            .Select(i => BitConverter.ToUInt32(move, checked(i * sizeof(uint)))));
        uint[] activeMoveMatches = own.Where(activeMoves.Contains).ToArray();
        return new(counts, moveCount, mainCount, secondaryCount, main, secondary, activeMoveMatches, tableMatches,
            main.Concat(secondary).Concat(tableMatches).Distinct().ToArray(), Convert.ToHexString(SHA256.HashData(tables)));
    }

    private uint[] ManagerList(uint head, uint count, uint[] own)
    {
        Require(count <= 4096, "Unqualified game-manager list count.");
        uint link = Pointer(head), previous = head;
        var visited = new HashSet<uint>(); var matches = new HashSet<uint>();
        for (uint i = 0; i < count; ++i)
        {
            Require(link != head && visited.Add(link) && U(link + 4) == previous, "Game-manager list is not bounded/reciprocal.");
            uint item = Pointer(link + 8); if (own.Contains(item)) matches.Add(item);
            uint next = Pointer(link); Require(U(next + 4) == link, "Game-manager next/back link mismatch.");
            previous = link; link = next;
        }
        Require(link == head && U(head + 4) == previous, "Game-manager list count/sentinel mismatch.");
        return matches.ToArray();
    }

    private bool OwnReceiver(uint receiver) => (_constructing != 0 || _instance != 0) && receiver >= 0x10000 &&
        U(receiver + 0x24) == (_constructing != 0 ? _constructing : _instance);
    private void Foreign(string name, uint thread, uint receiver, uint? code)
    {
        if (++_foreignWitnesses <= 24)
            _log(new { @event = "template-foreign-native-witness", name, threadId = thread, receiver = Hex(receiver), code });
    }
    private void Hook(uint entry, string name, BreakpointCallback callback)
    {
        VerifyFunction(entry);
        BreakpointCallback repeat = null!;
        repeat = (uint tid, uint at, ref Win32Native.X86Context ctx) =>
        {
            Require(tid == _host.MainThreadId && at == entry && ctx.Eip == entry, "Template witness stop/thread identity mismatch.");
            callback(tid, at, ref ctx);
            _host.Breakpoints.AddReturn(entry, tid, name, repeat);
        };
        _host.Breakpoints.AddReturn(entry, _host.MainThreadId, name, repeat);
    }
    private void Call(uint tid, ref Win32Native.X86Context ctx, NativeCallDispatcher calls, Action resume,
        uint function, uint receiver, uint[] args, bool cdecl, Action<uint> completed)
    {
        VerifyFunction(function);
        calls.Begin(_host, tid, ref ctx, function, receiver, args, cdecl, value =>
        { completed(value); VerifyLocal(); resume(); });
    }
    private void VerifyFunction(uint function)
    {
        Require(function is >= 0x401000 and < 0x6DA000, "Native callable is outside the pinned ordinary code image.");
        byte[] expected = _image.GetSectionData(checked((int)(function - 0x400000))).GetContent(0, 6).ToArray();
        Require(Read(function, 6).SequenceEqual(expected), $"Pinned native signature mismatch at {Hex(function)}.");
        _log(new { @event = "template-native-signature", address = Hex(function), sourceBytes6 = Convert.ToHexString(expected) });
    }
    private uint ClassId(uint pointer)
    {
        uint table = Pointer(pointer), getter = Pointer(table + 0x10);
        Require(getter is >= 0x401000 and < 0x6DA000, "Unqualified RTTI getter pointer.");
        byte[] code = _image.GetSectionData(checked((int)(getter - 0x400000))).GetContent(0, 6).ToArray();
        Require(code[0] == 0xB8 && code[5] == 0xC3 && Read(getter, 6).SequenceEqual(code), "RTTI getter is not the pinned simple record accessor.");
        return U(BitConverter.ToUInt32(code, 1));
    }
    private void Table(uint pointer, uint expected) => Require(pointer >= 0x10000 && U(pointer) == expected, $"Native table mismatch at {Hex(pointer)}; expected {Hex(expected)}.");
    private ushort Refs(uint pointer) => BitConverter.ToUInt16(Read(pointer + 8, 2));
    private uint Pointer(uint address) { uint value = U(address); Require(value >= 0x10000, $"Null/low pointer from {Hex(address)}."); return value; }
    private uint U(uint address) => BitConverter.ToUInt32(Read(address, 4));
    private byte[] Read(uint address, int length)
    {
        Require(_host.IsAtDebugStop && _host.ProcessHandle == _process && _host.ProcessPid == _pid &&
            Environment.CurrentManagedThreadId == _ownerThread && address >= 0x10000 && length is >= 1 and <= 65536,
            "Template memory read requires the bounded owned debug stop.");
        _ = checked(address + (uint)length - 1);
        return Win32Native.ReadMemory(_host.ProcessHandle, address, length);
    }
    private static object FloatWord(byte[] bytes, int offset)
    {
        float value = BitConverter.ToSingle(bytes, offset);
        return new { offset = Hex((uint)offset), bits = Hex(BitConverter.ToUInt32(bytes, offset)), finite = float.IsFinite(value), value = float.IsFinite(value) ? (float?)value : null };
    }
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidDataException(message); }
    private static Relationship Compare(string name, uint actual, uint expected) => new(name, Hex(actual), Hex(expected), actual == expected);
    private static string Hex(uint value) => $"0x{value:X8}";
}
