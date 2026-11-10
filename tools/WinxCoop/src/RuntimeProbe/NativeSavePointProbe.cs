using System.Reflection.PortableExecutable;
using System.Security.Cryptography;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// One standalone diagnostic call through a real Alfea SavePoint's original
/// no-argument writer wrapper. It does not use E5 controllers/quarantine or
/// claim that native return alone proves a durable, current-state save.
/// </summary>
internal sealed class NativeSavePointProbe
{
    private const uint Wrapper = 0x543BA0, SavePointTable = 0x6FCE20, SavePointGetter = 0x543C60;
    private const uint SavePointRecord = 0x752B68, SavePointClass = 0x099029B2;
    private const uint ProfileTable = 0x6F5A44, FlowTable = 0x702D2C, EntityManagerTable = 0x70069C;
    private const uint Limit = 4096;
    private const uint CaptureCallback = 0x543B10, CaptureGateway = 0x4E7B90;
    private const string WrapperSha256 = "BDBFC3781A5A33440759524322A5253EFCB95E1CC5292285E484552EE79BDCEC";
    private const string CaptureCallbackSha256 = "98CB886E2878DB425C1AF7A27DD68F0F8EDF72108E683C5FB03E781FB911FA64";
    private readonly DebuggerHost _host;
    private readonly PEReader _image;
    private readonly Action<object> _log;
    private readonly WorldStateRole _role;
    private readonly NativeSaveAuthorityGate _gate;
    private readonly byte[] _wrapper;
    private readonly bool _captureBeforeWriter;
    private readonly byte[]? _captureCallback;
    private readonly WorldPresentationEntityReader? _captureReader;
    private readonly IntPtr _process;
    private readonly uint _pid, _thread;
    private readonly int _owner;
    private uint _stage, _worlds, _callWorld, _returnStop, _wrapperResult;
    private uint _flow, _playing, _profile, _primary, _root, _engine, _scene, _manager, _input, _device, _timer;
    private uint _savePoint, _savePointNode, _savePointCount, _guestBefore, _hostBefore;
    private bool _called, _returned, _complete, _failed, _mediaAdmitted;
    private string _failure = "";
    private bool _captureCalled, _captureReturned, _captureVerified, _captureLaterWorldVerified;
    private uint _captureReturnStop, _captureCallWorld, _captureResult, _captureHostBefore, _captureGuestBefore;
    private uint _captureRegistry, _captureSettleWorlds;
    private WorldPresentationNativeRig? _captureRig;
    private CaptureTuple? _captureExpected, _captureBefore, _captureActual;

    private sealed record CaptureTuple(uint Checkpoint6D8, uint Level57C, uint Progression580,
        string WorldPosition584Hex, uint CurrentHealth590, uint MaximumHealth594);

    internal NativeSavePointProbe(DebuggerHost host, PEReader image, Action<object> log,
        WorldStateRole role, NativeSaveAuthorityGate gate, bool captureBeforeWriter = false)
    {
        ArgumentNullException.ThrowIfNull(host); ArgumentNullException.ThrowIfNull(image);
        ArgumentNullException.ThrowIfNull(log); ArgumentNullException.ThrowIfNull(gate);
        Require(role is WorldStateRole.Host or WorldStateRole.Guest && gate.Role == role &&
            host.IsAtDebugStop && host.ImageBase == 0x400000 && host.ProcessHandle != IntPtr.Zero &&
            host.ProcessPid != 0 && host.MainThreadId != 0,
            "SavePoint probe requires explicit matching authority and its owned original debug stop.");
        Require(Convert.ToHexString(SHA256.HashData(image.GetEntireImage().GetContent().ToArray())) ==
            "3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F",
            "SavePoint probe requires the exact original PC executable.");
        _host = host; _image = image; _log = log; _role = role; _gate = gate;
        Require(!captureBeforeWriter || role == WorldStateRole.Host,
            "Current-state capture is a separate Host-only diagnostic, never a Guest save permission.");
        _captureBeforeWriter = captureBeforeWriter;
        _process = host.ProcessHandle; _pid = host.ProcessPid; _thread = host.MainThreadId;
        _owner = Environment.CurrentManagedThreadId;
        _wrapper = Image(image, Wrapper, 0x42);
        Require(Convert.ToHexString(SHA256.HashData(_wrapper)) == WrapperSha256 && _wrapper[^1] == 0xC3 &&
            Image(image, SavePointGetter, 6).SequenceEqual(Convert.FromHexString("B8682B7500C3")) &&
            BitConverter.ToUInt32(Image(image, SavePointTable + 0x10, 4)) == SavePointGetter &&
            Image(image, 0x543BBF, 5).SequenceEqual(Convert.FromHexString("E81C1AFAFF")),
            "SavePoint wrapper, getter, type or inner known writer caller differs.");
        if (_captureBeforeWriter)
        {
            _captureCallback = Image(image, CaptureCallback, 0x86);
            Require(Convert.ToHexString(SHA256.HashData(_captureCallback)) == CaptureCallbackSha256 &&
                _captureCallback[^1] == 0xC3 &&
                BitConverter.ToUInt32(Image(image, SavePointTable + 0x54, 4)) == CaptureCallback &&
                Image(image, CaptureGateway, 6).SequenceEqual(Convert.FromHexString("FF25A4213B01")) &&
                Image(image, 0x4E2280, 6).SequenceEqual(Convert.FromHexString("B888E17400C3")),
                "Original SavePoint capture callback, slot54 or protected gateway differs.");
            _captureReader = new(host, image);
        }
    }

    internal bool ModeCompleted => _complete;
    internal bool ModeSucceeded => _complete && !_failed && _called && _returned && _mediaAdmitted &&
        (!_captureBeforeWriter || _captureCalled && _captureReturned && _captureVerified && _captureLaterWorldVerified) &&
        _gate.Healthy && (_role == WorldStateRole.Guest
            ? _gate.GuestRejections == checked(_guestBefore + 1) && _gate.HostNativeReturns == _hostBefore
            : _gate.HostNativeReturns == checked(_hostBefore + 1) && _gate.GuestRejections == _guestBefore);
    internal object Summary => new
    {
        role = _role.ToString(), stage = _stage, completed = ModeCompleted, succeeded = ModeSucceeded,
        failed = _failed, failure = _failure, worlds = _worlds, callWorld = _callWorld,
        called = _called, originalWrapperReturned = _returned, wrapperResultOpaque = Hex(_wrapperResult),
        wrapperResultUsedAsWriterBool = false, wrapper = Hex(Wrapper), wrapperSha256 = WrapperSha256,
        savePoint = Hex(_savePoint), savePointNode = Hex(_savePointNode), candidateCount = _savePointCount,
        scene = Hex(_scene), primaryInstance = Hex(_primary), profile = Hex(_profile),
        guestRejectionsBefore = _guestBefore, hostNativeReturnsBefore = _hostBefore,
        guestRejectionsAfter = _gate.GuestRejections, hostNativeReturnsAfter = _gate.HostNativeReturns,
        mediaVerified = _mediaAdmitted, nativeCallsRequested = _called ? 1 : 0,
        captureBeforeWriter = _captureBeforeWriter, captureCalled = _captureCalled,
        captureReturned = _captureReturned, captureSelectedTupleVerified = _captureVerified,
        captureLaterWorldVerified = _captureLaterWorldVerified, captureCallWorld = _captureCallWorld,
        captureCallback = Hex(CaptureCallback), captureCallbackSha256 = CaptureCallbackSha256,
        captureResultOpaque = Hex(_captureResult), captureResultUsedAsBool = false,
        beforeCaptureTuple = _captureBefore, expectedLiveCaptureTuple = _captureExpected,
        actualCaptureTuple = _captureActual, captureRegistry = Hex(_captureRegistry),
        captureSettleWorlds = _captureSettleWorlds, nativeCaptureInternalsReconstructed = false,
        captureTupleIsFullSavedStateOrEveryManager = false, checkpoint6D8IsSerializedClaimed = false,
        ownGameplayFieldWrites = 0, e5ControllersOrQuarantineClaimed = false,
        originalSavePointFailureBroadcastPreserved = true,
        fileDurabilityQualified = false,
        currentStateCaptureQualified = _captureBeforeWriter && _captureVerified && _captureLaterWorldVerified,
        nativeLoadOrRoundTripQualified = false, successfulNativeSceneUnloadClaimed = false,
        nativeCaptureCallsRequested = _captureCalled ? 1 : 0,
        totalNativeCallsRequested = (_called ? 1 : 0) + (_captureCalled ? 1 : 0),
        completesOnlyAtLaterWorldAfterDispatcherPageRelease = true
    };

    internal void StepAtWorld(uint tid, ref Win32Native.X86Context context,
        NativeCallDispatcher calls, Action resume, bool mediaVerified)
    {
        if (_complete || _failed) return;
        try
        {
            EnsureStop(tid);
            Require(context.Eip == NativeCallDispatcher.WorldReturn && ++_worlds <= NativeObservationBudget.WorldLimit,
                "SavePoint probe escaped its bounded original WorldRET.");
            if (!mediaVerified || calls.Pending) return;
            Require(_gate.Healthy, "SavePoint probe lost its active unpaired authority gate.");
            if (_stage == 0)
            {
                if (!TryCaptureReady()) return;
                _mediaAdmitted = true;
                VerifyCapturedScene();
                VerifyFunctionAndTypePins();
                uint[] candidates = CurrentSavePoints();
                Require(candidates.Length > 0 && candidates.Length <= 16,
                    "SavePoint probe needs at least one bounded actual current-scene SavePoint; no synthetic receiver is allowed.");
                _savePointCount = (uint)candidates.Length; _savePoint = candidates[0];
                _savePointNode = P(_savePoint + 0x18);
                Require(U(_savePointNode) == VisualGraphReader.NodeVtable && U(_savePointNode + 0x3C) == _scene,
                    "SavePoint wrapper receiver is not bound to its captured original scene node.");
                if (_captureBeforeWriter)
                {
                    BeginCapture(tid, ref context, calls, resume);
                    return;
                }
                BeginWriter(tid, ref context, calls, resume);
                return;
            }
            if (_stage == 11)
            {
                Require(_captureBeforeWriter && _captureCalled && _captureReturned && _captureVerified &&
                    _host.DebugStopSequence > _captureReturnStop && _worlds > _captureCallWorld &&
                    ++_captureSettleWorlds <= 120,
                    "Capture continuation is not a bounded later World after its original return.");
                if (calls.HasRetiredPage) return;
                VerifyCapturedScene(allowRetainedSaveMenu: true, allowPendingSaveMenu: true);
                RequireCaptureAuthorityUnchanged();
                if (U(_flow + 0x1B8) != 0) return;
                Require(ReadSavedCaptureTuple() == _captureExpected,
                    "Selected captured tuple changed before its original writer.");
                _captureLaterWorldVerified = true;
                _log(new { @event = "native-save-point-probe-capture-later-world", role = _role.ToString(),
                    laterWorld = _worlds, dispatcherPageReleased = true, actualSavedTuple = _captureActual,
                    originalPlayingRetained = true, readyGameClaimedForSaveOverlay = false,
                    gameDataWritesByProbe = 0 });
                BeginWriter(tid, ref context, calls, resume);
                return;
            }
            Require(_stage == 2 && _called && _returned && _host.DebugStopSequence > _returnStop && _worlds > _callWorld,
                "SavePoint completion is not a later original World after its native return.");
            // The composition owner releases the retired dispatcher page before
            // calling this method. Never free pages from the completion callback.
            if (calls.HasRetiredPage) return;
            VerifyCapturedScene(allowRetainedSaveMenu: _captureBeforeWriter, allowPendingSaveMenu: _captureBeforeWriter);
            if (_captureBeforeWriter && U(_flow + 0x1B8) != 0)
            {
                Require(++_captureSettleWorlds <= 120, "Save overlay failed to settle before final writer completion.");
                return;
            }
            RequireAuthorityConsequence();
            _complete = true; _stage = 3;
            _log(new { @event = "native-save-point-probe-complete", role = _role.ToString(),
                succeeded = ModeSucceeded, laterWorld = _worlds, dispatcherPageReleased = true,
                gameStarted = true, writerGateRuntimeExercised = true, saveDurabilityClaimed = false,
                networkWorldOrE5GameplayAcceptanceClaimed = false });
        }
        catch (Exception error) { _failed = true; _failure = error.Message; throw; }
    }

    private void BeginWriter(uint tid, ref Win32Native.X86Context context, NativeCallDispatcher calls, Action resume)
    {
        Require(!_called && !_returned && !_complete && !calls.Pending && !calls.HasRetiredPage &&
            (!_captureBeforeWriter || _captureLaterWorldVerified),
            "SavePoint writer may run only once after its admitted capture/page interval.");
        VerifyFunctionAndTypePins();
        _guestBefore = _gate.GuestRejections; _hostBefore = _gate.HostNativeReturns;
        _callWorld = _worlds; _stage = 1; _called = true;
        _log(new { @event = "native-save-point-probe-call", role = _role.ToString(),
            function = Hex(Wrapper), thisPointer = Hex(_savePoint), node = Hex(_savePointNode),
            candidateCount = _savePointCount, scene = Hex(_scene), arguments = Array.Empty<uint>(),
            cdecl = false, nativeReturnInstruction = "C3", knownInnerWriterCaller = "0x00543BC4",
            upstreamCaptureInWrapper = false, gameDataWritesByProbe = 0 });
        calls.Begin(_host, tid, ref context, Wrapper, _savePoint, [], false, value =>
        {
            EnsureStop(_thread);
            Require(_stage == 1 && _called && !_returned && _gate.Healthy,
                "SavePoint wrapper returned outside its one original diagnostic call.");
            RequireAuthorityConsequence();
            _wrapperResult = value; _returned = true; _stage = 2;
            _returnStop = _host.DebugStopSequence;
            _log(new { @event = "native-save-point-probe-wrapper-returned", role = _role.ToString(),
                resultOpaque = Hex(value), writerBoolNotDerivedFromWrapperEax = true,
                guestRejections = _gate.GuestRejections, hostNativeReturns = _gate.HostNativeReturns,
                fileDurabilityClaimed = false, nativeLoadRoundTripClaimed = false,
                dispatcherPageStillRetired = true });
            resume();
        });
    }

    private void BeginCapture(uint tid, ref Win32Native.X86Context context, NativeCallDispatcher calls, Action resume)
    {
        Require(_captureBeforeWriter && _role == WorldStateRole.Host && !_captureCalled && !_called &&
            _captureReader is not null && _captureCallback is not null && !calls.Pending && !calls.HasRetiredPage,
            "Native capture requires the separate Host-only one-call interval.");
        byte[] callback = _captureCallback ?? throw new InvalidDataException("Original capture callback was not pinned.");
        WorldPresentationEntityReader reader = _captureReader ?? throw new InvalidDataException("Capture rig reader was not admitted.");
        Require(Read(CaptureCallback, callback.Length).SequenceEqual(callback) &&
            U(SavePointTable + 0x54) == CaptureCallback &&
            Read(CaptureGateway, 6).SequenceEqual(Convert.FromHexString("FF25A4213B01")),
            "Live original capture callback/gateway pin differs.");
        _captureRig = reader.ReadRigAtWorld(tid, ref context, _primary, WorldPresentationActorState.BloomType);
        _captureRegistry = P(0x765AD8);
        Require(U(_captureRegistry) == 0x6F5488 && U(0x6F5498) == 0x4E2280 &&
            Read(0x4E2280, 6).SequenceEqual(Convert.FromHexString("B888E17400C3")) &&
            U(0x74E188) == 0x6EAA030D && U(_captureRegistry + 0x28) == _captureRig.Character,
            "Capture's original CharacterRegistry fast path is not the canonical primary Character.");
        _captureExpected = ReadLiveCaptureTuple();
        _captureBefore = ReadSavedCaptureTuple();
        Require(!SameSerializedCaptureFields(_captureBefore, _captureExpected),
            "Selected tuple is already coherent; a new meaningful native capture cannot be qualified without changing game data.");
        _captureGuestBefore = _gate.GuestRejections; _captureHostBefore = _gate.HostNativeReturns;
        _captureCalled = true; _captureCallWorld = _worlds; _stage = 10;
        _log(new { @event = "native-save-point-probe-capture-begin", role = _role.ToString(),
            function = Hex(CaptureCallback), thisPointer = Hex(_savePoint), arguments = Array.Empty<uint>(),
            originalProfileGateway = Hex(CaptureGateway), authoredArgumentReadByOriginalCallback = U(_savePoint + 0x1C8),
            beforeSavedTuple = _captureBefore, expectedLiveTuple = _captureExpected,
            sourceProducer = "13BB01A/57C/580/584..58C/590/594", dataWritesByProbe = 0,
            wholeOriginalCaptureCallbackRequested = true, protectedInternalsReconstructed = false });
        calls.Begin(_host, tid, ref context, CaptureCallback, _savePoint, [], false, value =>
        {
            EnsureStop(_thread);
            Require(_stage == 10 && _captureCalled && !_captureReturned && !_called,
                "Original native capture returned outside its one-call interval.");
            VerifyCapturedScene(allowRetainedSaveMenu: true, allowPendingSaveMenu: true);
            RequireCaptureAuthorityUnchanged();
            Require(ReadLiveCaptureTuple() == _captureExpected,
                "Original capture callback changed its selected source tuple; expected state will not be normalized.");
            _captureActual = ReadSavedCaptureTuple();
            Require(_captureActual == _captureExpected && !SameSerializedCaptureFields(_captureActual, _captureBefore!),
                "Original native capture did not produce a meaningful exact selected current-state tuple.");
            _captureResult = value; _captureReturned = true; _captureVerified = true; _stage = 11;
            _captureReturnStop = _host.DebugStopSequence;
            _log(new { @event = "native-save-point-probe-capture-returned", resultOpaque = Hex(value),
                wholeOriginalCaptureCallbackReturned = true,
                actualSavedTuple = _captureActual, expectedLiveTuple = _captureExpected,
                meaningfulChangedSerializedSelectedTuple = true, selectedTupleReadbackVerified = true,
                writerNotYetCalled = true, protectedGatewayInternalsQualified = false,
                dispatcherPageStillRetired = true, gameDataWritesByProbe = 0 });
            resume();
        });
    }

    private CaptureTuple ReadLiveCaptureTuple()
    {
        WorldPresentationNativeRig rig = _captureRig ?? throw new InvalidDataException("Capture primary rig was not admitted.");
        Require(U(0x765AD8) == _captureRegistry && U(_captureRegistry) == 0x6F5488 &&
            U(_captureRegistry + 0x28) == rig.Character && U(rig.Character) == 0x6F6438 &&
            U(rig.Character + 0x24) == _primary && U(rig.Character + 0x138) == rig.Traits &&
            U(rig.Traits) == 0x6F6990 && U(rig.Traits + 0x24) == _primary &&
            U(rig.Traits + 0x18C) == rig.Character,
            "Capture's original primary Character/Traits source ownership changed.");
        byte[] position = Read(_root + 0x74, 12);
        Require(Enumerable.Range(0, 3).Select(i => BitConverter.ToSingle(position, i * 4))
            .All(value => float.IsFinite(value) && Math.Abs(value) <= 1e7),
            "Capture source world position is nonfinite or exceeds the existing scene budget.");
        uint hp = U(rig.Traits + 0x124), maximum = U(rig.Traits + 0x128);
        Require(hp <= maximum && maximum is > 0 and <= 100000, "Capture source HP leaves its original bounded domain.");
        return new(U(_savePoint + 0x1C8), 28, U(_profile + 0x514), Convert.ToHexString(position), hp, maximum);
    }
    private CaptureTuple ReadSavedCaptureTuple() => new(U(_profile + 0x6D8), U(_profile + 0x57C),
        U(_profile + 0x580), Convert.ToHexString(Read(_profile + 0x584, 12)), U(_profile + 0x590), U(_profile + 0x594));
    private static bool SameSerializedCaptureFields(CaptureTuple first, CaptureTuple second) =>
        first.Level57C == second.Level57C && first.Progression580 == second.Progression580 &&
        first.WorldPosition584Hex == second.WorldPosition584Hex &&
        first.CurrentHealth590 == second.CurrentHealth590 && first.MaximumHealth594 == second.MaximumHealth594;
    private void RequireCaptureAuthorityUnchanged() => Require(_gate.Healthy &&
        _gate.GuestRejections == _captureGuestBefore && _gate.HostNativeReturns == _captureHostBefore,
        "Capture unexpectedly invoked a save writer before its separately admitted writer interval.");

    private bool TryCaptureReady()
    {
        uint flow = U(0x755294);
        if (flow < 0x10000 || U(flow) != FlowTable) return false;
        uint depth = U(flow + 0x1AC);
        if (depth == uint.MaxValue || U(flow + 0x1B0) != 28 || U(flow + 0x1B8) != 0) return false;
        Require(depth < 20, "SavePoint ready Flow exceeds the original twenty stack slots.");
        uint playing = P(flow + 0x15C + depth * 4);
        if (U(playing) != 0x710874 || U(playing + 0x10) != 28) return false;
        uint profile = P(0x765AD4), primary = P(profile + 0x2B4), root = P(primary + 0x24);
        uint engine = P(0x755274), scene = P(engine + 0x18), input = P(0x7552A0);
        uint device = P(input + 0x1C), timer = P(0x755298), manager = P(0x765ACC);
        Require(U(profile) == ProfileTable && U(primary) == 0x710F80 &&
            U(root) == VisualGraphReader.NodeVtable && U(root + 0x3C) == scene && U(scene) == 0x6E7358 &&
            U(manager) == EntityManagerTable && U(input) == 0x6F5508 && U(device) == 0x7116CC && U(timer) == 0x702F94,
            "SavePoint ready state has unqualified canonical profile/scene/entity/input/timer types.");
        if (U(device + 0x14) != 3 || Read(timer + 0x40, 1)[0] != 0) return false;
        _flow = flow; _playing = playing; _profile = profile; _primary = primary; _root = root;
        _engine = engine; _scene = scene; _input = input; _device = device; _timer = timer; _manager = manager;
        return true;
    }

    private void VerifyCapturedScene(bool allowRetainedSaveMenu = false, bool allowPendingSaveMenu = false)
    {
        // Check current singleton/scene ownership before following captured
        // player/entity addresses; no prior generation is revived after unload.
        Require(U(0x755294) == _flow && U(_flow) == FlowTable &&
            U(0x755274) == _engine && U(_engine + 0x18) == _scene &&
            U(_scene) == 0x6E7358 && U(0x765ACC) == _manager && U(_manager) == EntityManagerTable &&
            U(0x765AD4) == _profile && U(_profile) == ProfileTable,
            "SavePoint diagnostic left its current canonical scene before completion.");
        uint depth = U(_flow + 0x1AC);
        Require(depth < 20, "SavePoint retained Flow exceeds its twenty original slots.");
        uint current = U(_flow + 0x1B0), pending = U(_flow + 0x1B8), top = P(_flow + 0x15C + depth * 4);
        uint retained = 0;
        for (uint index = 0; index <= depth; ++index)
            if (U(_flow + 0x15C + index * 4) == _playing) ++retained;
        bool playingReady = current == 28 && pending == 0 && top == _playing;
        bool retainedSaveMenu = allowRetainedSaveMenu && current is 69 or 72 && pending == 0 &&
            top != _playing && U(top + 0x10) == current && OriginalFlowAccessor(top);
        bool pendingSaveMenu = allowRetainedSaveMenu && allowPendingSaveMenu && pending is 69 or 72 &&
            (current == 28 && top == _playing || current is 69 or 72 && top != _playing &&
                U(top + 0x10) == current && OriginalFlowAccessor(top));
        Require((playingReady || retainedSaveMenu || pendingSaveMenu) && retained == 1 && U(_playing) == 0x710874 &&
            U(_playing + 0x10) == 28 &&
            U(_profile + 0x2B4) == _primary && U(_primary) == 0x710F80 && U(_primary + 0x24) == _root &&
            U(_root) == VisualGraphReader.NodeVtable && U(_root + 0x3C) == _scene &&
            U(0x7552A0) == _input && U(_input) == 0x6F5508 && U(_input + 0x1C) == _device &&
            U(_device) == 0x7116CC &&
            U(0x755298) == _timer && U(_timer) == 0x702F94 && Read(_timer + 0x40, 1)[0] <= 1 &&
            (!playingReady || U(_device + 0x14) == 3 && Read(_timer + 0x40, 1)[0] == 0),
            "SavePoint diagnostic captured player/input/playing ownership changed.");
        if (_savePoint != 0)
            Require(CurrentSavePoints().Contains(_savePoint) && P(_savePoint + 0x18) == _savePointNode,
                "SavePoint receiver ceased to be a live typed member of the captured scene.");
    }

    // Only the original69/72 save overlays requested by this SavePoint family
    // may retain the captured Playing28. Validate the live literal accessor
    // against the original PE rather than admitting an arbitrary new top state.
    private bool OriginalFlowAccessor(uint state)
    {
        uint table = U(state);
        Require(table is >= 0x6DA000 and < 0x744000,
            "Retained save overlay has an out-of-image native table.");
        uint getter = U(table + 0x10);
        Require(getter == BitConverter.ToUInt32(Image(_image, table + 0x10, 4)) &&
            getter is >= 0x401000 and < 0x6DA000,
            "Retained save overlay getter is not the original typed accessor.");
        byte[] literal = Image(_image, getter, 6);
        Require(literal[0] == 0xB8 && literal[5] == 0xC3 && Read(getter, 6).SequenceEqual(literal),
            "Retained save overlay lacks its original literal RTTI accessor.");
        uint record = BitConverter.ToUInt32(literal, 1);
        Require(record >= 0x10000 && U(record) != 0,
            "Retained save overlay lacks its live registered native class.");
        return true;
    }

    private uint[] CurrentSavePoints()
    {
        uint[] all = EntityList(P(_manager + 0x18), U(_manager + 0x1C))
            .Concat(EntityList(P(_manager + 0x24), U(_manager + 0x28))).ToArray();
        Require(all.Length <= Limit && all.Distinct().Count() == all.Length,
            "SavePoint current entity inventory is too large or contains duplicate objects.");
        return all.Where(entity => U(entity) == SavePointTable).Where(entity =>
        {
            uint node = P(entity + 0x18);
            Require(U(node) == VisualGraphReader.NodeVtable, "Typed SavePoint has an unqualified bound node.");
            return U(node + 0x3C) == _scene;
        }).ToArray();
    }
    private uint[] EntityList(uint head, uint count)
    {
        Require(count <= Limit, "SavePoint entity list exceeds its finite bound.");
        uint link = P(head), previous = head; HashSet<uint> seen = []; List<uint> objects = [];
        for (uint i = 0; i < count; ++i)
        {
            Require(link != head && seen.Add(link) && U(link + 4) == previous,
                "SavePoint entity list lost its reciprocal finite links.");
            objects.Add(P(link + 8)); previous = link; link = P(link);
        }
        Require(link == head && U(head + 4) == previous,
            "SavePoint entity inventory count or sentinel differs.");
        return objects.ToArray();
    }
    private void VerifyFunctionAndTypePins() => Require(
        Read(Wrapper, _wrapper.Length).SequenceEqual(_wrapper) &&
        U(SavePointTable + 0x10) == SavePointGetter && U(SavePointTable + 4) == 0x543FA0 &&
        Read(SavePointGetter, 6).SequenceEqual(Convert.FromHexString("B8682B7500C3")) &&
        U(SavePointRecord) == SavePointClass && Read(0x543BBF, 5).SequenceEqual(Convert.FromHexString("E81C1AFAFF")),
        "Live SavePoint wrapper/type/known writer-call pins differ from the admitted original.");
    private void RequireAuthorityConsequence() => Require(_gate.Healthy && (_role == WorldStateRole.Guest
        ? _gate.GuestRejections == checked(_guestBefore + 1) && _gate.HostNativeReturns == _hostBefore
        : _gate.HostNativeReturns == checked(_hostBefore + 1) && _gate.GuestRejections == _guestBefore),
        "Original SavePoint wrapper lacks its exact single inner writer rejection or Host native return.");
    private void EnsureStop(uint tid) => Require(Environment.CurrentManagedThreadId == _owner &&
        _host.IsAtDebugStop && tid == _thread && _host.MainThreadId == _thread &&
        _host.ProcessHandle == _process && _host.ProcessPid == _pid && _host.ImageBase == 0x400000,
        "SavePoint probe requires its owned original main-thread debug stop.");
    private static byte[] Image(PEReader image, uint address, int bytes) => image.GetSectionData(
        checked((int)(address - 0x400000))).GetContent(0, bytes).ToArray();
    private uint P(uint address)
    {
        uint value = U(address);
        Require(value >= 0x10000 && (value & 3) == 0, "SavePoint probe has a null or unaligned native owner.");
        return value;
    }
    private uint U(uint address) => BitConverter.ToUInt32(Read(address, 4));
    private byte[] Read(uint address, int bytes)
    {
        Require(address >= 0x10000 && bytes is > 0 and <= 256,
            "SavePoint probe remote read exceeds its bounded native field.");
        return Win32Native.ReadMemory(_process, address, bytes);
    }
    private static string Hex(uint value) => $"0x{value:X8}";
    private static void Require(bool value, string message) { if (!value) throw new InvalidDataException(message); }
}
