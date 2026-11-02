using System.ComponentModel;
using System.Reflection.PortableExecutable;
using System.Security.Cryptography;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// Lifetime driver for the finite client policy and its separate live acceptance.
/// It never creates a second stock profile/body or treats timeout as success.
/// </summary>
internal sealed class ClientPresentationSession
{
    private const uint ProbeWorldLimit = 300;
    private static uint LiveWorldLimit => NativeObservationBudget.WorldLimit;
    private readonly ProbeConfiguration _config;
    private readonly DebuggerHost _host;
    private readonly PEReader _image;
    private readonly Action<object> _log;
    private readonly IntPtr _process;
    private readonly uint _pid;
    private readonly int _ownerThread;
    private readonly bool _live;
    private readonly long _deadline;
    private ClientScenePolicy? _policy;
    private ClientNativeGate? _gate;
    private ClientWorldFlowGate? _worldFlow;
    private readonly Dictionary<uint, uint> _componentTables = [];
    private readonly Dictionary<string, uint> _lastCounters = [];
    private uint _flow, _playing, _profile, _record, _root, _rootParent, _core, _scene, _camera, _cameraTable;
    private uint _input, _command, _device, _timer, _character, _move, _machine, _animator, _intelliCam;
    private uint _worlds, _postUnloadWorlds, _nativePauseTransitions, _resumeTransitions;
    private uint _inputMovementSamples, _inputJumpSamples, _inputPauseSamples, _cameraChangesAfterInput;
    private bool _started, _afterUnload, _completed, _probeRestored, _unloadQualified;
    private bool _movementObserved, _jumpObserved, _nativePauseObserved, _lastMenuPaused;
    private bool _guestCaptureRetiredBeforeUnload;
    private byte _lastNativePaused;
    private long _pauseStarted, _longestPause, _resumedPause, _pendingReadyPauseDuration, _menuStarted, _menuHeld;
    private byte[] _rootPosition = [], _previousCameraPose = [];
    private string? _initialCameraPoseHash, _lastCameraPoseHash;
    private readonly uint _level, _playingTable, _exitEntry;
    private byte[]? _authoritativeLocalPose;
    private uint _authoritativeApplications;

    internal ClientPresentationSession(ProbeConfiguration config, DebuggerHost host, PEReader image, Action<object> log)
    {
        Require(config.Mode is "client-presentation-probe" or "client-presentation-live" &&
            config.DurationSeconds is >= 5 and <= 600 && host.IsAtDebugStop &&
            host.ProcessHandle != IntPtr.Zero && host.ProcessPid != 0 && host.MainThreadId != 0,
            "Client presentation session requires an explicit bounded mode and owned initial stop.");
        _config = config; _host = host; _image = image; _log = log;
        _process = host.ProcessHandle; _pid = host.ProcessPid; _ownerThread = Environment.CurrentManagedThreadId;
        _live = config.Mode == "client-presentation-live";
        _level = config.WorldBattlePresentation ? 47u : 28u;
        _playingTable = config.WorldBattlePresentation ? 0x7105C8u : 0x710874u;
        _exitEntry = config.WorldBattlePresentation ? 0x5F2300u : 0x5F86C0u;
        _deadline = checked(Environment.TickCount64 + (long)config.DurationSeconds * 1000);
    }

    internal bool Started => _started;
    internal bool AfterUnload => _afterUnload;
    internal bool WorldQuarantineActive => InputCaptureReady;
    internal void AcceptAuthoritativeRootAtWorld(uint tid, ref Win32Native.X86Context context)
    {
        EnsureStop(tid);
        Require(_config.DiagnosticWorldState?.Role == WorldStateRole.Guest && WorldQuarantineActive &&
            context.Eip == NativeCallDispatcher.WorldReturn, "Authoritative pose requires the explicit WorldState Guest quarantine.");
        _authoritativeLocalPose = Read(_root + 0x20, 12).Concat(Read(_root + 0x30, 12)).Concat(Read(_root + 0x40, 36)).ToArray();
        ++_authoritativeApplications;
    }
    internal bool ModeCompleted => _completed;
    // This is only a cached lifetime allowance. NativeInputCapture still
    // validates its actual ready tuple on every original UserInput return.
    internal bool InputCaptureReady => _started && !_afterUnload && !_completed && !_probeRestored &&
        _gate is { Enabled: true, Restored: false, Released: false };
    internal bool ModeSucceeded => _completed && _gate is { Restored: true, Released: true } && CountersQualified() &&
        (!_config.DiagnosticGuestCaptureFixture || _guestCaptureRetiredBeforeUnload) &&
        (_live ? _unloadQualified && _nativePauseObserved && _resumedPause >= 5000 && _resumeTransitions > 0 &&
            _movementObserved && _jumpObserved && _cameraChangesAfterInput > 0 && _menuHeld >= 30000 :
            _probeRestored && !_afterUnload && _worlds == ProbeWorldLimit);
    internal object Summary => new
    {
        started = _started, completed = _completed, succeeded = ModeSucceeded, live = _live,
        nativeWorlds = _worlds, requiredProbeWorlds = ProbeWorldLimit, liveWorldLimit = LiveWorldLimit,
        configuredDurationSeconds = _config.DurationSeconds,
        afterUnload = _afterUnload, nativeUnloadQualified = _unloadQualified,
        finiteWorldRestorationOnly = _probeRestored, postUnloadWorlds = _postUnloadWorlds,
        nativePauseObserved = _nativePauseObserved, nativePauseTransitions = _nativePauseTransitions,
        resumeTransitions = _resumeTransitions, longestNativePauseMilliseconds = _longestPause,
        longestResumedNativePauseMilliseconds = _resumedPause,
        requiredResumedNativePauseMilliseconds = 5000,
        movementCommandObserved = _movementObserved, jumpCommandObserved = _jumpObserved,
        movementCommandSamples = _inputMovementSamples, jumpCommandSamples = _inputJumpSamples,
        pauseCommandSamples = _inputPauseSamples,
        cameraPoseChangesAfterBothInputKinds = _cameraChangesAfterInput,
        initialCameraPoseSha256 = _initialCameraPoseHash, lastCameraPoseSha256 = _lastCameraPoseHash,
        continuousQualifiedMainMenuMilliseconds = _menuHeld, requiredMainMenuMilliseconds = 30000,
        clientRootRemainsAtCapturedPosition = _config.DiagnosticWorldState is null,
        networkStateApplied = _authoritativeApplications > 0, authoritativeApplications = _authoritativeApplications,
        visualAcceptanceRequiredSeparately = true, rootOrCommandWrites = 0,
        localEntityReadAfterUnload = false,
        diagnosticGuestCaptureFixture = _config.DiagnosticGuestCaptureFixture,
        inputCaptureLifetimeAllowed = InputCaptureReady,
        guestCaptureRetiredBeforeUnload = _guestCaptureRetiredBeforeUnload,
        counters = new Dictionary<string, uint>(_lastCounters), policy = _policy?.Summary, gate = _gate?.Summary,
        worldFlow = _worldFlow?.Summary
    };

    internal void ObserveWorld(uint thread, ref Win32Native.X86Context context, uint world, bool mediaVerified)
    {
        EnsureStop(thread);
        Require(context.Eip == NativeCallDispatcher.WorldReturn, "Client session observation is not its verified WorldRET.");
        if (_completed) return;
        Require(Environment.TickCount64 <= _deadline, "Client presentation session exceeded its configured wall-clock limit.");
        if (_afterUnload) { ObserveAfterUnload(thread, ref context); return; }
        if (_probeRestored)
        {
            ClientNativeGate gate = _gate ?? throw new InvalidDataException("Finite client restoration lost its gate.");
            CaptureCounters();
            if (!gate.ReleaseAtLaterWorld(thread, ref context)) return;
            _completed = true;
            _log(new { @event = "client-presentation-probe-complete", nativeWorlds = _worlds,
                nativeUnloadClaimed = false, originalPatchesRestored = gate.Restored,
                ownedPagesReleased = gate.Released, succeeded = ModeSucceeded });
            return;
        }
        if (!_started)
        {
            if (!mediaVerified || !ReadyAlfea()) return;
            CaptureOwners();
            _policy = new ClientScenePolicy(_host, _image, _log);
            _policy.Capture();
            _gate = new ClientNativeGate(_host, _image, _log, _config.WorldBattlePresentation);
            _policy.Configure(_gate, _config.DiagnosticGuestCaptureFixture, _config.WorldBattlePresentation);
            if (_config.WorldBattlePresentation && _config.DiagnosticWorldState?.Role == WorldStateRole.Guest)
            {
                _worldFlow = new(_host, _image, _log);
                _worldFlow.Configure(_gate);
            }
            _gate.Install(thread, ref context);
            _started = true;
            CaptureCounters();
            _log(new { @event = "client-presentation-session-begin", world, live = _live,
                playerRecord = Hex(_record), root = Hex(_root), camera = Hex(_camera), cameraTable = Hex(_cameraTable),
                command = Hex(_command), timer = Hex(_timer), nativePaused = _lastNativePaused,
                playingPauseArm = Read(_playing + 0x3C, 1)[0],
                playingPauseInhibited = Read(_playing + 0x17D, 1)[0],
                pauseViewInhibited = Read(Pointer(Pointer(0x755284) + 0x10C) + 0x144, 1)[0],
                initialCameraPoseSha256 = _initialCameraPoseHash, clientMovementIsNotLocallySimulated = true,
                diagnosticGuestCaptureFixture = _config.DiagnosticGuestCaptureFixture,
                inputCaptureLifetimeAllowed = InputCaptureReady,
                nativePolicyOwnsCodePatches = true, managedPerEntityCensusInstalled = false });
            return;
        }

        Require(++_worlds <= (_live ? LiveWorldLimit : ProbeWorldLimit), "Client presentation exceeded its native world budget.");
        bool ready = VerifyOwnersAndPause();
        _worldFlow?.ObserveAtWorld(thread, ref context, _playing, _scene, WorldQuarantineActive);
        ClientScenePolicy policy = _policy ?? throw new InvalidDataException("Client session lost its scene policy.");
        policy.VerifyStableFrame();
        if (_worlds % 60 == 0) policy.VerifyCallbackGraphs();
        CaptureCounters();
        ObserveInputAndCamera(ready);
        if (_worlds <= 2 || _worlds % 60 == 0)
            _log(new { @event = "client-presentation-world", world, ordinal = _worlds, ready,
                movementObserved = _movementObserved, jumpObserved = _jumpObserved,
                cameraChangesAfterInput = _cameraChangesAfterInput, nativePaused = _lastNativePaused,
                longestResumedPauseMilliseconds = _resumedPause,
                counters = new Dictionary<string, uint>(_lastCounters) });
        if (!_live && _worlds == ProbeWorldLimit)
        {
            Require(ready && CountersQualified(), "Finite native client interval did not qualify its coverage counters.");
            policy.VerifyCallbackGraphs();
            _gate!.RestoreAtWorldStop(thread, ref context); _probeRestored = true;
            _log(new { @event = "client-presentation-probe-restored", nativeWorlds = _worlds,
                nativeUnloadClaimed = false, laterWorldPageReleaseStillRequired = true });
        }
    }

    internal bool TryBeginUnload(uint thread, ref Win32Native.X86Context context, uint entry,
        Action? retireCaptureBeforeRestore = null)
    {
        EnsureStop(thread);
        if (!_started || _afterUnload || _completed) return false;
        if (entry != _exitEntry) return false;
        Require(context.Eip == entry && context.Ecx == _playing &&
            U(0x755294) == _flow && U(_playing) == _playingTable && U(_playing + 0x10) == _level &&
            U(0x765AD4) == _profile && U(_profile) == 0x6F5A44 && U(_profile + 0x2B4) == _record &&
            U(_record) == 0x710F80 && U(_record + 0x24) == _root && U(_root) == 0x6DC4F4,
            "Client presentation unload receiver or captured local owners changed before restoration.");
        (_policy ?? throw new InvalidDataException("Client unload lost its policy.")).VerifyCallbackGraphs();
        CaptureCounters();
        ClientNativeGate gate = _gate ?? throw new InvalidDataException("Client unload lost its gate.");
        if (_config.DiagnosticGuestCaptureFixture)
        {
            Require(retireCaptureBeforeRestore is not null,
                "Guest unload requires capture retirement before restoring native patches.");
            retireCaptureBeforeRestore!();
            _guestCaptureRetiredBeforeUnload = true;
            _log(new { @event = "client-presentation-capture-retired-before-unload", entry = Hex(entry),
                receiver = Hex(context.Ecx), captureRetirementCallbackCompleted = true,
                beforeNativePatchRestoration = !gate.Restored, beforeOriginalUnloadInstruction = true });
        }
        _worldFlow?.RetireAtStop(thread, ref context, entry);
        if (gate.Enabled) gate.RestoreAtUnload(thread, ref context, entry);
        Require(gate.Restored, "Client presentation reached unload without restoring its patches.");
        _afterUnload = true;
        _unloadQualified = _live;
        _log(new { @event = "client-presentation-unload-begin", entry = Hex(entry), receiver = Hex(context.Ecx),
            actualNativeUnload = true, liveAcceptanceMode = _live,
            gateRestoredBeforeOriginalInstruction = true,
            diagnosticGuestCaptureFixture = _config.DiagnosticGuestCaptureFixture,
            guestCaptureRetiredBeforeUnload = _guestCaptureRetiredBeforeUnload,
            oldPlayerAndSceneObservationsNowForbidden = true, ownBodyObjectsCreated = 0 });
        return true;
    }

    private bool ReadyAlfea()
    {
        uint flow = TryU(0x755294), profile = TryU(0x765AD4), record = TryU(profile + 0x2B4);
        if (flow < 0x10000 || TryU(flow + 0x1B0) != _level || TryU(flow + 0x1B8) != 0 ||
            TryU(profile) != 0x6F5A44 || TryU(record) != 0x710F80 || TryU(record + 0x20) != 11) return false;
        uint stack = TryU(flow + 0x1AC); if (stack > 20) return false;
        uint playing = TryU(flow + 0x15C + stack * 4), root = TryU(record + 0x24), input = TryU(0x7552A0);
        uint device = TryU(input + 0x1C), timer = TryU(0x755298);
        return TryU(playing) == _playingTable && TryU(playing + 0x10) == _level && TryU(root) == 0x6DC4F4 &&
            TryU(TryU(root + 0x3C)) == 0x6E7358 && TryU(input) == 0x6F5508 &&
            TryU(TryU(input + 0x18)) == 0x703090 && TryU(device) == 0x7116CC &&
            TryU(device + 0x14) == 3 && TryU(timer) == 0x702F94 && Read(timer + 0x40, 1)[0] == 0;
    }

    private void CaptureOwners()
    {
        _flow = Pointer(0x755294); uint stack = U(_flow + 0x1AC); Require(stack <= 20, "Client flow stack exceeds its bound.");
        _playing = Pointer(_flow + 0x15C + stack * 4);
        _profile = Pointer(0x765AD4); _record = Pointer(_profile + 0x2B4); _root = Pointer(_record + 0x24);
        _rootParent = Pointer(_root + 0x2C); _scene = Pointer(_root + 0x3C);
        _core = Pointer(0x755274); _camera = Pointer(_core + 0x1C); _cameraTable = Pointer(_camera);
        uint cameraClass = ClassId(_cameraTable);
        Require(cameraClass is 0x41672E34 or 0x24BB4C41 or 0x18DF3845,
            "Client camera is not a qualified native spCamera Node-derived type.");
        _input = Pointer(0x7552A0); _command = Pointer(_input + 0x18); _device = Pointer(_input + 0x1C);
        _timer = Pointer(0x755298);
        Require(ClassId(U(_timer)) == 0x78665312, "Client timer does not have the original GameTimer identity.");
        foreach (uint function in new uint[] { 0x5968F0, 0x596900, 0x596910 }) VerifyFunction(function, 6);
        foreach ((uint item, uint table) in Components()) _componentTables.Add(item, table);
        uint Unique(uint table) => _componentTables.Single(value => value.Value == table).Key;
        _character = Unique(0x6F6438); _move = Unique(0x6F64F8); _machine = Unique(0x6FA288);
        _animator = Unique(0x6F670C); _intelliCam = Unique(0x6F522C);
        foreach (uint entity in new[] { _character, _move, _machine, _animator, _intelliCam })
            Require(U(entity + 0x24) == _record, "Client component owner is not its captured stock instance.");
        _rootPosition = Read(_root + 0x74, 12);
        _previousCameraPose = CameraPose(); _initialCameraPoseHash = Hash(_previousCameraPose);
        _lastCameraPoseHash = _initialCameraPoseHash;
        _lastNativePaused = 0;
        VerifyOwnersAndPause();
    }

    private bool VerifyOwnersAndPause()
    {
        Require(!_afterUnload && U(0x755294) == _flow && U(0x765AD4) == _profile &&
            U(_profile) == 0x6F5A44 && U(_profile + 0x2B4) == _record && U(_record) == 0x710F80 &&
            U(_record + 0x24) == _root && U(_root) == 0x6DC4F4 && U(_root + 0x2C) == _rootParent &&
            U(_root + 0x3C) == _scene && U(0x755274) == _core && U(_core + 0x18) == _scene &&
            U(_core + 0x1C) == _camera && U(_camera) == _cameraTable &&
            U(0x7552A0) == _input && U(_input) == 0x6F5508 && U(_input + 0x18) == _command &&
            U(_command) == 0x703090 && U(_input + 0x1C) == _device && U(_device) == 0x7116CC &&
            U(0x755298) == _timer && U(_timer) == 0x702F94,
            "Client presentation left its captured scene/player/input/camera/timer ownership.");
        Require(Components().OrderBy(value => value.Item1).SequenceEqual(_componentTables
            .Select(value => (value.Key, value.Value)).OrderBy(value => value.Item1)),
            "Client presentation stock component identities changed.");
        foreach (uint entity in new[] { _character, _move, _machine, _animator, _intelliCam })
            Require(U(entity + 0x24) == _record, "Client component ownership changed.");
        Require(U(_character + 0x124) == _machine && U(_character + 0x128) == _animator &&
            U(_character + 0x12C) == _move && U(_character + 0x130) == _command &&
            U(_move + 0x160) == _character && U(_machine + 0x124) == _character && U(_machine + 0x12C) == _command,
            "Client presentation command/controller relationships changed.");
        Require(_authoritativeLocalPose is { } pose ?
            Read(_root + 0x20, 12).Concat(Read(_root + 0x30, 12)).Concat(Read(_root + 0x40, 36)).SequenceEqual(pose) :
            Read(_root + 0x74, 12).SequenceEqual(_rootPosition),
            "Client policy allowed local player translation without a server state application.");
        uint stack = U(_flow + 0x1AC); Require(stack <= 20, "Client presentation flow stack exceeded its bound.");
        bool retained = false; uint top = 0;
        for (uint index = 0; index <= stack; ++index)
        {
            uint state = U(_flow + 0x15C + index * 4); retained |= state == _playing;
            if (index == stack) top = state;
        }
        Require(retained && top >= 0x10000 && U(_playing) == _playingTable && U(_playing + 0x10) == _level,
            "Client presentation captured PlayingLevel was not retained under its overlay.");
        byte nativePaused = Read(_timer + 0x40, 1)[0]; Require(nativePaused is 0 or 1, "Native client timer left its pause0/1 domain.");
        bool ready = top == _playing && U(_flow + 0x1B0) == _level && U(_flow + 0x1B8) == 0 &&
            U(_device + 0x14) == 3 && nativePaused == 0;
        // The same timer also pauses when the application loses focus. Only
        // an actual, settled level61 DiaryMenu above this PlayingLevel qualifies.
        bool menuPaused = nativePaused == 1 && top != _playing && U(top + 0x10) == 61 &&
            U(_flow + 0x1B0) == 61 && U(_flow + 0x1B8) == 0;
        if (menuPaused)
            Require(top == U(_flow + 0x10C) && U(top) == 0x70ED30 &&
                U(0x70ED30 + 0x10) == 0x5D53C0 && ClassId(0x70ED30) == 0x0734C578,
                "Client pause is not the original level61 DiaryMenu state.");
        long now = Environment.TickCount64;
        if (menuPaused != _lastMenuPaused)
        {
            _log(new { @event = "client-presentation-menu-pause-transition", menuPaused,
                nativePaused, top = Hex(top), topTable = Hex(U(top)), topId = U(top + 0x10),
                currentLevel = U(_flow + 0x1B0), pending = U(_flow + 0x1B8),
                focusLossAloneDoesNotQualify = true });
            _lastMenuPaused = menuPaused;
        }
        if (menuPaused)
        {
            _nativePauseObserved = true;
            if (_pauseStarted == 0) { _pauseStarted = now; _pendingReadyPauseDuration = 0; }
            _longestPause = Math.Max(_longestPause, Math.Max(0, now - _pauseStarted));
        }
        else if (_pauseStarted != 0)
        {
            _pendingReadyPauseDuration = Math.Max(0, now - _pauseStarted);
            _longestPause = Math.Max(_longestPause, _pendingReadyPauseDuration);
            _pauseStarted = 0;
        }
        if (ready && _pendingReadyPauseDuration > 0)
        {
            _resumedPause = Math.Max(_resumedPause, _pendingReadyPauseDuration);
            Require(++_resumeTransitions <= 64, "Client ready-resume transition budget exceeded.");
            _log(new { @event = "client-presentation-ready-resumed", completedNativePauseMilliseconds = _pendingReadyPauseDuration,
                longestResumedNativePauseMilliseconds = _resumedPause, capturedPlaying = Hex(_playing),
                nativePaused, actualReady = true, nativeTimerWrites = 0 });
            _pendingReadyPauseDuration = 0;
        }
        if (nativePaused != _lastNativePaused)
        {
            Require(++_nativePauseTransitions <= 64, "Client native pause transition budget exceeded.");
            _log(new { @event = "client-presentation-native-pause-transition", previous = _lastNativePaused,
                nativePaused, menuPaused, ready, top = Hex(top), topTable = Hex(U(top)), topId = U(top + 0x10),
                currentLevel = U(_flow + 0x1B0), pending = U(_flow + 0x1B8),
                deviceContext = U(_device + 0x14), longestNativePauseMilliseconds = _longestPause,
                longestResumedNativePauseMilliseconds = _resumedPause, nativeTimerWrites = 0 });
            _lastNativePaused = nativePaused;
        }
        return ready;
    }

    private void ObserveInputAndCamera(bool ready)
    {
        byte[] command = Read(_command, 0x74);
        float movement = BitConverter.ToSingle(command, 4);
        Require(float.IsFinite(movement), "Client input contains nonfinite movement intent.");
        if (ready && movement != 0) { _movementObserved = true; ++_inputMovementSamples; }
        if (ready && command[0x1A] != 0) { _jumpObserved = true; ++_inputJumpSamples; }
        if (ready && command[0x2B] != 0 && ++_inputPauseSamples <= 8)
            _log(new { @event = "client-presentation-pause-command", samples = _inputPauseSamples,
                nativeCommandByte2B = command[0x2B], playingPauseArm = Read(_playing + 0x3C, 1)[0],
                playingPauseInhibited = Read(_playing + 0x17D, 1)[0] });
        byte[] pose = CameraPose();
        bool changed = !_previousCameraPose.SequenceEqual(pose);
        if (ready && changed && _movementObserved && _jumpObserved) ++_cameraChangesAfterInput;
        _previousCameraPose = pose; _lastCameraPoseHash = Hash(pose);
    }

    private byte[] CameraPose()
    {
        byte[] node = Read(_camera, 0xB4);
        Require(BitConverter.ToUInt32(node) == _cameraTable, "Client camera native Node identity changed.");
        var result = new List<byte>(120);
        foreach ((int offset, int length) in new[] { (0x20, 12), (0x30, 12), (0x40, 36), (0x74, 12), (0x80, 12), (0x8C, 36) })
        {
            for (int index = offset; index < offset + length; index += 4)
                Require(float.IsFinite(BitConverter.ToSingle(node, index)), "Client camera PRS contains a nonfinite field.");
            result.AddRange(node.AsSpan(offset, length).ToArray());
        }
        return result.ToArray();
    }

    private List<(uint, uint)> Components()
    {
        Require(U(_record + 0x20) == 11, "Client stock instance must have eleven components.");
        uint head = Pointer(_record + 0x1C), link = Pointer(head), previous = head;
        var result = new List<(uint, uint)>(); var seen = new HashSet<uint>(); var items = new HashSet<uint>();
        for (uint index = 0; index < 11; ++index)
        {
            Require(link != head && seen.Add(link) && U(link + 4) == previous,
                "Client component list is not reciprocal/distinct.");
            uint item = Pointer(link + 8); Require(items.Add(item), "Client component appears twice.");
            result.Add((item, U(item))); previous = link; link = Pointer(link);
        }
        Require(link == head && U(head + 4) == previous, "Client component list count/sentinel mismatch.");
        return result;
    }

    private void ObserveAfterUnload(uint thread, ref Win32Native.X86Context context)
    {
        // This branch must precede every cached player/scene/component read.
        ClientNativeGate gate = _gate ?? throw new InvalidDataException("Post-unload client session lost its gate.");
        Require(_afterUnload && gate.Restored, "Post-unload client session lacks restored native patches.");
        if (!gate.Released)
        {
            CaptureCounters();
            if (!gate.ReleaseAtLaterWorld(thread, ref context)) return;
        }
        Require(++_postUnloadWorlds <= LiveWorldLimit, "Client post-unload world budget exceeded.");
        uint FreshU(uint at)
        {
            Require(at >= 0x10000, "Fresh client flow read has a low/null pointer.");
            return Win32Native.ReadUInt32(_process, at);
        }
        uint FreshTryU(uint at)
        {
            if (at < 0x10000) return 0;
            try { return FreshU(at); } catch (Win32Exception) { return 0; }
        }
        uint flow = FreshTryU(0x755294), stack = flow >= 0x10000 ? FreshTryU(flow + 0x1AC) : uint.MaxValue;
        uint pending = stack <= 20 ? FreshTryU(flow + 0x1B8) : uint.MaxValue;
        uint top = stack <= 20 ? FreshTryU(flow + 0x15C + stack * 4) : 0;
        bool menu = top >= 0x10000 && FreshTryU(top) == 0x7102A8 && pending == 0;
        if (menu)
        {
            byte[] getter = [0xB8, 0x90, 0x89, 0x76, 0x00, 0xC3];
            Require(Image(0x5EF690, 6).SequenceEqual(getter) &&
                Win32Native.ReadMemory(_process, 0x5EF690, getter.Length).SequenceEqual(getter) &&
                FreshU(0x7102A8 + 0x10) == 0x5EF690 && FreshU(0x768990) == 0x284B3AA2,
                "Client fresh MainMenu table/getter/registration pin mismatch.");
            if (_menuStarted == 0) _menuStarted = Environment.TickCount64;
            _menuHeld = Math.Max(0, Environment.TickCount64 - _menuStarted);
        }
        else { _menuStarted = 0; _menuHeld = 0; }
        if (_postUnloadWorlds <= 2 || _postUnloadWorlds % 120 == 0 || _menuHeld >= 30000)
            _log(new { @event = "client-presentation-fresh-menu-only", world = _postUnloadWorlds,
                flow = Hex(flow), stack, pending, top = Hex(top), mainMenu = menu,
                continuousMenuMilliseconds = _menuHeld, oldPlayerOrSceneRead = false,
                pagesReleased = gate.Released });
        if (_menuHeld < 30000) return;
        _completed = true;
        _log(new { @event = "client-presentation-live-complete", succeeded = ModeSucceeded,
            continuousMenuMilliseconds = _menuHeld, resumedPauseMilliseconds = _resumedPause,
            movementObserved = _movementObserved, jumpObserved = _jumpObserved,
            cameraChangesAfterBothInputKinds = _cameraChangesAfterInput,
            nativeUnloadQualified = _unloadQualified, restored = gate.Restored, released = gate.Released,
            visualAcceptanceRequiredSeparately = true });
    }

    private void CaptureCounters()
    {
        ClientNativeGate gate = _gate ?? throw new InvalidDataException("Client counter capture lost its native gate.");
        Require(!gate.Released, "Client counter capture cannot read released native pages.");
        _lastCounters.Clear();
        foreach ((string name, uint value) in gate.ReadCounters()) _lastCounters.Add(name, value);
    }
    private bool CountersQualified()
    {
        uint Value(string key) => _lastCounters.GetValueOrDefault(key);
        return Value("typed-withheld") > 0 && Value("typed-presentation-preserved") > 0 &&
            Value("ai-calls-withheld") > 0 && Value("foreign-thread-calls") == 0 &&
            (Value("animation-state-tags-withheld") > 0 || _lastCounters.Any(pair =>
                pair.Key.StartsWith("region:actor-loop-callback-", StringComparison.Ordinal) && pair.Value > 0)) &&
            (!_config.DiagnosticGuestCaptureFixture ||
                Value(_config.WorldBattlePresentation ? "region:world-battle-pad-after-ordinary" : "region:diagnostic-pad-after-jump") > 0 &&
                Value("region:diagnostic-pad-after-confirm") > 0 &&
                Value("region:diagnostic-pad-after-pause") > 0);
    }
    private uint ClassId(uint table)
    {
        uint getter = U(table + 0x10); byte[] accessor = Image(getter, 6);
        Require(accessor[0] == 0xB8 && accessor[5] == 0xC3 && Read(getter, 6).SequenceEqual(accessor),
            "Client session type getter is not the pinned original registration accessor.");
        return U(BitConverter.ToUInt32(accessor, 1));
    }
    private void VerifyFunction(uint entry, int count) => Require(Read(entry, count).SequenceEqual(Image(entry, count)),
        "Client session original function signature changed.");
    private byte[] Image(uint at, int count)
    {
        Require(at is >= 0x401000 and < 0x740000 && count is >= 1 and <= 64,
            "Client session image lookup exceeds its original code bound.");
        return _image.GetSectionData(checked((int)(at - 0x400000))).GetContent(0, count).ToArray();
    }
    private uint Pointer(uint at) { uint value = U(at); Require(value >= 0x10000, "Client session null/low pointer."); return value; }
    private uint U(uint at) => BitConverter.ToUInt32(Read(at, 4));
    private uint TryU(uint at)
    {
        if (at < 0x10000) return 0;
        try { return U(at); } catch (Win32Exception) { return 0; }
    }
    private byte[] Read(uint at, int count)
    {
        EnsureStop(_host.MainThreadId);
        Require(!_afterUnload, "Client post-unload route used its historical object reader.");
        Require(at >= 0x10000 && count is >= 1 and <= 65536, "Client session read exceeds its pointer/length bound.");
        _ = checked(at + (uint)count - 1); return Win32Native.ReadMemory(_process, at, count);
    }
    private void EnsureStop(uint thread) => Require(_host.IsAtDebugStop && thread == _host.MainThreadId &&
        _host.ProcessHandle == _process && _host.ProcessPid == _pid && Environment.CurrentManagedThreadId == _ownerThread,
        "Client presentation session requires its captured owned main-thread debug stop.");
    private static string Hash(byte[] bytes) => Convert.ToHexString(SHA256.HashData(bytes));
    private static string Hex(uint value) => $"0x{value:X8}";
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidDataException(message); }
}
