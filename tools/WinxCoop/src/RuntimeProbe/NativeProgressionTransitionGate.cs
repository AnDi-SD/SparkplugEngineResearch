using System.Reflection.PortableExecutable;
using System.Security.Cryptography;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

// Local authority/observations only; these records are never network pointers.
internal sealed record NativeProgressionOutcomeMenuAuthority(DebuggerHost Owner, uint LevelEpoch,
    uint Generation, ulong Operation, uint Flow, uint GameOver, uint GameOverId,
    uint Menu, uint MenuId, bool ActualGameOverSettled, bool ActualOriginalPlayingExitReturned);
internal readonly record struct NativeProgressionOriginalRequestReturn(uint Sequence, uint Receiver,
    uint Destination, uint RawArgument, uint Caller, ulong OwnedOutcomeOperation);

/// <summary>
/// Opt-in, one-scene progression boundary. The known ChangeLevel action is
/// withheld on Guest before its first instruction; Host keeps its original
/// primary-only predicate and action. Flow requests are observations, not
/// completed level entries or permission to reuse the old native scene.
/// </summary>
internal sealed class NativeProgressionTransitionGate
{
    private NativeCallDispatcher? _ownedRequest;
    private NativeProgressionOutcomeMenuAuthority? _ownedOutcomeMenu;
    private uint _ownedDestination, _ownedArgument, _ownedRequestUses;
    internal void PermitOwnedRequestAtWorld(uint tid, in Win32Native.X86Context context,
        NativeCallDispatcher calls, uint destination, uint argument)
    {
        EnsureStop(tid);
        Require(_role == WorldStateRole.Host && !_story3 && _bound && !_retired && !_sceneRetired && !_failed &&
            context.Eip == NativeCallDispatcher.WorldReturn && !calls.Pending && !calls.HasRetiredPage &&
            _ownedRequest is null && destination == 28 && argument == 1,
            "Owned progression request requires its one ready Host Alfea reload boundary.");
        VerifyScene(requireReady: true);
        _ownedRequest = calls; _ownedDestination = destination; _ownedArgument = argument;
    }
    internal void PermitOwnedOutcomeMenuRequestAtWorld(uint tid, in Win32Native.X86Context context,
        NativeCallDispatcher calls, NativeProgressionOutcomeMenuAuthority authority)
    {
        EnsureStop(tid);
        Require(_story3 && _role == WorldStateRole.Host && _installed && _bound && _level == 3 &&
            _sceneRetired && !_retired && !_failed && !HasPendingNativeCall &&
            context.Eip == NativeCallDispatcher.WorldReturn && !calls.Pending && !calls.HasRetiredPage &&
            _ownedRequest is null && _ownedOutcomeMenu is null && _ownedRequestUses == 0 &&
            ReferenceEquals(authority.Owner, _host) && authority.LevelEpoch == _progressionEpoch &&
            authority.Generation == _progressionGeneration && authority.Operation > 0 &&
            authority.ActualGameOverSettled && authority.ActualOriginalPlayingExitReturned,
            "Outcome Menu authority requires the retired closed Story3 generation and its exact actual outcome owner.");
        VerifyFreshOutcomeMenu(authority);
        _ownedRequest = calls; _ownedDestination = authority.MenuId; _ownedArgument = 0;
        _ownedOutcomeMenu = authority;
    }
    internal const uint ChangeLevelTable = 0x6FD7A8, ChangeLevelAction = 0x54B4C0;
    internal const uint ChangeLevelPredicate = 0x54B520, RequestEntry = 0x595450;
    private const uint ActionReturnInstruction = 0x54B51B, OriginalActionCaller = 0x59061D;
    private const uint FlowTable = 0x702D2C, ProfileTable = 0x6F5A44;
    private const uint MaximumCallbacks = 10000, MaximumDetailedLogs = 64;
    private readonly DebuggerHost _host;
    private readonly Action<object> _log;
    private readonly WorldStateRole _role;
    private readonly bool _story3;
    private readonly CampaignSceneProfile? _campaign;
    private readonly uint _progressionEpoch, _progressionGeneration;
    private readonly IntPtr _process;
    private readonly uint _pid, _thread;
    private readonly int _owner;
    private readonly byte[] _actionBytes, _requestBytes;
    private bool _installationAttempted, _installed, _bound, _sceneRetired, _retired, _failed;
    private bool _actionProbePending, _requestProbePending;
    private uint _callbacks, _actionCalls, _guestSkips, _hostActionReturns, _requestCalls, _requestReturns;
    private uint _startupPassThrough, _retiredSceneActions, _worlds;
    private uint _flow, _playing, _playingTable, _level, _scene, _profile, _primary, _root, _engine;
    private uint _input, _device, _timer;
    private string _failure = "";
    private ActionFrame? _action;
    private RequestFrame? _request;
    private NativeProgressionOriginalRequestReturn? _lastRequestReturn;

    private sealed record ActionFrame(uint Receiver, uint Esp, uint Caller, uint Destination,
        uint RequestCount, uint[] QueueBefore);
    private sealed record RequestFrame(uint Receiver, uint Esp, uint Caller, uint Destination,
        uint RawArgument, uint Sequence, uint[] QueueBefore, bool DuringKnownTrigger, ulong OwnedOutcomeOperation = 0);

    internal NativeProgressionTransitionGate(DebuggerHost host, PEReader image, Action<object> log,
        WorldStateRole role, uint progressionSceneLevel = 28, ProgressionControlPermit? progressionControl = null,
        WorldStateIdentity? progressionWorld = null, CampaignSceneProfile? campaign = null)
    {
        ArgumentNullException.ThrowIfNull(host); ArgumentNullException.ThrowIfNull(image);
        ArgumentNullException.ThrowIfNull(log);
        Require(role is WorldStateRole.Host or WorldStateRole.Guest && host.IsAtDebugStop &&
            host.ProcessHandle != IntPtr.Zero && host.ProcessPid != 0 && host.MainThreadId != 0 &&
            host.ImageBase == 0x400000, "Progression gate requires its explicit role and owned original debug stop.");
        Require(campaign?.Admits(progressionSceneLevel) == true || progressionSceneLevel == 28 || progressionSceneLevel == 3 &&
            progressionControl is { OwnerPid: > 0, OwnerCreationFileTime: > 0 } &&
            progressionWorld is { } world && world.Role == role && world.LevelEpoch > 0 && world.Generation > 0,
            "Story3 transition authority requires its explicit progression permit and role-matched scene identity.");
        _story3 = progressionSceneLevel == 3; _campaign = campaign;
        _progressionEpoch = progressionWorld?.LevelEpoch ?? 0; _progressionGeneration = progressionWorld?.Generation ?? 0;
        Require(Convert.ToHexString(SHA256.HashData(image.GetEntireImage().GetContent().ToArray())) ==
            "3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F",
            "Progression gate requires the exact original PC image.");
        _host = host; _log = log; _role = role; _process = host.ProcessHandle;
        _pid = host.ProcessPid; _thread = host.MainThreadId; _owner = Environment.CurrentManagedThreadId;
        _actionBytes = Image(image, ChangeLevelAction, 0x5C);
        if (_story3)
            Require(Word(image, 0x70F778 + 0x10) == 0x5E0450 && Word(image, 0x7102A8 + 0x10) == 0x5EF690 &&
                Image(image, 0x5E0450, 6).SequenceEqual(Convert.FromHexString("B830867600C3")) &&
                Image(image, 0x5EF690, 6).SequenceEqual(Convert.FromHexString("B890897600C3")),
                "Story outcome GameOver/MainMenu original getter pins differ.");
        _requestBytes = Image(image, RequestEntry, 0x65);
        Require(_actionBytes.AsSpan(0, 7).SequenceEqual(Convert.FromHexString("A19452750085C0")) &&
            _actionBytes[^1] == 0xC3 && _requestBytes.AsSpan(0, 9).SequenceEqual(
                Convert.FromHexString("56578B7C240C83FF33")) &&
            _requestBytes.AsSpan(0x53, 3).SequenceEqual(Convert.FromHexString("C20800")) &&
            _requestBytes.AsSpan(0x62, 3).SequenceEqual(Convert.FromHexString("C20800")) &&
            Word(image, ChangeLevelTable + 0x10) == 0x54B430 &&
            Word(image, ChangeLevelTable + 0x4C) == ChangeLevelPredicate &&
            Word(image, ChangeLevelTable + 0x50) == ChangeLevelAction &&
            Image(image, 0x54B430, 6).SequenceEqual(Convert.FromHexString("B808317500C3")) &&
            Image(image, 0x590613, 12).SequenceEqual(Convert.FromHexString("C6864501000000FF5250EB14")),
            "Progression action slots, no-argument return, ignored return value or Flow RET8 ABI differ.");
    }

    internal bool SceneBound => _bound && !_sceneRetired && !_retired && !_failed;
    internal bool Healthy => _installed && !_failed && !HasPendingNativeCall;
    internal bool HasPendingNativeCall => _action is not null || _request is not null;
    internal uint OriginalRequestReturns => _requestReturns;
    internal NativeProgressionOriginalRequestReturn? LastOriginalRequestReturn => _lastRequestReturn;
    internal object Summary => new
    {
        role = _role.ToString(), installationAttempted = _installationAttempted,
        installed = _installed, sceneBound = SceneBound, sceneRetired = _sceneRetired,
        retired = _retired, failed = _failed, failure = _failure, worlds = _worlds,
        callbacks = _callbacks, actionCalls = _actionCalls, guestActionsWithheld = _guestSkips,
        hostOriginalActionReturns = _hostActionReturns, hostFlowRequests = _requestCalls,
        hostOriginalRequestReturns = _requestReturns, startupOriginalPassThrough = _startupPassThrough,
        retiredSceneOriginalActions = _retiredSceneActions, actionEntryProbePending = _actionProbePending,
        requestEntryProbePending = _requestProbePending, pairedNativeCallPending = HasPendingNativeCall,
        admittedSceneLevel = _bound ? _level : (uint?)null,
        knownChangeLevelClass = "0x56235DEA", knownChangeLevelTable = Hex(ChangeLevelTable),
        guestActionEntry = Hex(ChangeLevelAction), originalNoArgumentRet = Hex(ActionReturnInstruction),
        hostRequestEntry = Hex(RequestEntry), exactOriginalCaller = Hex(OriginalActionCaller),
        callbackContextWrites = _role == WorldStateRole.Guest ? new[] { "EIP" } : Array.Empty<string>(),
        nativeCallsAdded = 0, profileWrites = 0, triggerFieldWrites = 0, flowQueueWrites = 0,
        queuedRequestIsCompletedEntry = false, newSceneRebindImplemented = false,
        allTriggerFamiliesQualified = false, runtimeQualificationClaimed = false,
        breakpointRestorationOwner = "SoftwareBreakpointManager"
    };

    internal void InstallStopped()
    {
        EnsureStop(_thread);
        Require(!_installationAttempted && !_retired && !_failed, "Progression gate installation is single-use.");
        _installationAttempted = true;
        Require(Read(ChangeLevelAction, _actionBytes.Length).SequenceEqual(_actionBytes) &&
            (_role != WorldStateRole.Host || Read(RequestEntry, _requestBytes.Length).SequenceEqual(_requestBytes)),
            "Progression native entry bytes changed before probe publication.");
        _installed = true;
        Repeat(ChangeLevelAction, "Progression.ChangeLevel.action", OnAction);
        if (_role == WorldStateRole.Host) Repeat(RequestEntry, "Progression.Flow.request", OnRequest);
        _log(new { @event = "progression-transition-installed", role = _role.ToString(),
            dormantUntilQualifiedScene = true, guestActionEntry = Hex(ChangeLevelAction),
            hostQueueObservationEntry = _role == WorldStateRole.Host ? Hex(RequestEntry) : null,
            fullAuthorityOrLevelTransitionImplemented = false, nativeCallsAdded = 0 });
    }

    // Caller invokes only at its existing WorldRET observation boundary. Media
    // verification is necessary but does not make title/loading/menu ready.
    internal void ObserveWorld(uint tid, ref Win32Native.X86Context context, bool mediaVerified)
    {
        if (_retired || _failed || _sceneRetired || !mediaVerified) return;
        EnsureStop(tid);
        Require(_installed && context.Eip == NativeCallDispatcher.WorldReturn && ++_worlds <= NativeObservationBudget.WorldLimit,
            "Progression scene observation left its original owned WorldRET or bound.");
        if (_bound) { VerifyScene(requireReady: false); return; }
        uint flow = U(0x755294);
        if (flow == 0 || U(flow) != FlowTable) return;
        uint depth = U(flow + 0x1AC), current = U(flow + 0x1B0);
        if (depth == uint.MaxValue || current is not (28 or 47) && !(current == 3 && _story3) && _campaign?.Admits(current) != true || U(flow + 0x1B8) != 0) return;
        Require(depth < 20, "Progression ready Flow stack exceeds its original twenty slots.");
        uint playing = P(flow + 0x15C + depth * 4), table = U(playing);
        uint expected = current == 47 ? 0x7105C8u : 0x710874u;
        if (table != expected || U(playing + 0x10) != current) return;
        uint input = P(0x7552A0), device = P(input + 0x1C), timer = P(0x755298);
        Require(U(input) == 0x6F5508 && U(device) == 0x7116CC && U(timer) == 0x702F94,
            "Progression ready native input/device/timer types differ.");
        if (U(device + 0x14) != 3 || Read(timer + 0x40, 1)[0] != 0) return;
        uint profile = P(0x765AD4), primary = P(profile + 0x2B4), root = P(primary + 0x24);
        uint engine = P(0x755274), scene = P(engine + 0x18);
        Require(U(profile) == ProfileTable && U(primary) == 0x710F80 &&
            U(root) == VisualGraphReader.NodeVtable && U(root + 0x3C) == scene && U(scene) == 0x6E7358,
            "Progression ready scene/player ownership is not the admitted original graph.");
        _flow = flow; _playing = playing; _playingTable = table; _level = current;
        _input = input; _device = device; _timer = timer; _profile = profile;
        _primary = primary; _root = root; _engine = engine; _scene = scene; _bound = true;
        VerifyScene(requireReady: true);
        _log(new { @event = "progression-transition-scene-bound", role = _role.ToString(),
            level = _level, flow = Hex(flow), playing = Hex(playing), scene = Hex(scene),
            primaryInstance = Hex(primary), primaryRoot = Hex(root), actualReadyWorld = _worlds,
            hostPrimaryPredicatePreserved = true, newGenerationRecreationImplemented = false });
    }

    internal void RetireSceneCached()
    {
        EnsureOwner();
        Require(!HasPendingNativeCall, "Progression scene retirement cannot abandon an in-flight native action/request.");
        if (_sceneRetired || _retired) return;
        _sceneRetired = true;
        _log(new { @event = "progression-transition-scene-retired", role = _role.ToString(),
            noRemoteReads = true, oldSceneWillNotBeFollowed = true,
            hostFreshFlowObservationRetained = _role == WorldStateRole.Host,
            subsequentGuestActionRequiresFreshGeneration = true });
    }

    internal void RetireCached()
    {
        EnsureOwner();
        if (_retired) return;
        _retired = true; _sceneRetired = true;
        _log(new { @event = "progression-transition-retired", noRemoteReads = true,
            pendingNativeCallAtRetirement = HasPendingNativeCall, actionProbePending = _actionProbePending,
            requestProbePending = _requestProbePending, breakpointRestorationOwner = "SoftwareBreakpointManager" });
    }

    private void OnAction(uint tid, uint at, ref Win32Native.X86Context context)
    {
        CheckCallback(tid, at, ChangeLevelAction, context);
        if (!_bound) { ++_startupPassThrough; return; }
        if (_sceneRetired)
        {
            Require(_role == WorldStateRole.Host, "Guest ChangeLevel action requires a fresh scene generation after unload.");
            ++_retiredSceneActions; return; // Host keeps native behavior without any old-owner read.
        }
        Require(_action is null && U(context.Esp) == OriginalActionCaller,
            "ChangeLevel action is recursive or has an unqualified original caller.");
        VerifyScene(requireReady: true);
        uint trigger = context.Ecx, node = P(trigger + 0x18), destination = U(trigger + 0x148);
        Require(trigger >= 0x10000 && (trigger & 3) == 0 && U(trigger) == ChangeLevelTable &&
            U(ChangeLevelTable + 0x10) == 0x54B430 && U(0x753108) == 0x56235DEA &&
            U(ChangeLevelTable + 0x4C) == ChangeLevelPredicate && U(ChangeLevelTable + 0x50) == ChangeLevelAction &&
            U(node) == VisualGraphReader.NodeVtable && U(node + 0x3C) == _scene && destination is > 0 and <= 50 &&
            Read(ChangeLevelAction, _actionBytes.Length).SequenceEqual(_actionBytes),
            "ChangeLevel action receiver/type/node/scene/destination or original bytes differ.");
        ++_actionCalls;
        if (_role == WorldStateRole.Guest)
        {
            // v20 has no explicit arguments. The pinned original caller ignores
            // EAX; execute only the original lone RET, before native side effects.
            uint originalEsp = context.Esp;
            Require(Read(ActionReturnInstruction, 1)[0] == 0xC3, "ChangeLevel lone return instruction changed.");
            context.Eip = ActionReturnInstruction; ++_guestSkips;
            if (_guestSkips <= MaximumDetailedLogs)
                _log(new { @event = "progression-guest-change-level-withheld", level = _level,
                    destination, trigger = Hex(trigger), scene = Hex(_scene), caller = Hex(OriginalActionCaller),
                    originalEsp = Hex(originalEsp), next = Hex(context.Eip), onlyEipChangedByCallback = true,
                    originalActionFirstInstructionNotExecuted = true, nativeCallsAdded = 0,
                    triggerEligibilityOrAllExitFamiliesClaimed = false });
            return;
        }
        _action = new(trigger, context.Esp, OriginalActionCaller, destination, _requestCalls, Queue(_flow));
        _host.Breakpoints.AddReturn(OriginalActionCaller, _thread, "Progression.ChangeLevel.original-return", OnActionReturn);
        if (_actionCalls <= MaximumDetailedLogs)
            _log(new { @event = "progression-host-change-level-action", level = _level,
                destination, trigger = Hex(trigger), originalActionPreserved = true, nativeEligibilityCaller = true,
                queuedDecisionIsCompletedEntry = false });
    }

    private void OnActionReturn(uint tid, uint at, ref Win32Native.X86Context context)
    {
        if (_retired) return;
        EnsureStop(tid);
        ActionFrame frame = _action ?? throw new InvalidDataException("Host ChangeLevel return lost its entry pair.");
        Require(at == frame.Caller && context.Eip == at && context.Esp == checked(frame.Esp + 4),
            "Host ChangeLevel no-argument original return/stack differs.");
        VerifyScene(requireReady: false); _action = null; ++_hostActionReturns;
        if (_hostActionReturns <= MaximumDetailedLogs)
            _log(new { @event = "progression-host-change-level-action-returned", destination = frame.Destination,
                requestCallsDuringAction = _requestCalls - frame.RequestCount,
                queueBefore = frame.QueueBefore, queueAfter = Queue(_flow), originalRetExecuted = true,
                actualLevelEntryCompleted = false, nativeContextWritesByObserver = 0 });
    }

    private void OnRequest(uint tid, uint at, ref Win32Native.X86Context context)
    {
        CheckCallback(tid, at, RequestEntry, context);
        Require(_role == WorldStateRole.Host && _request is null &&
            Read(RequestEntry, _requestBytes.Length).SequenceEqual(_requestBytes),
            "Host progression request is nested or its original entry bytes differ.");
        uint flow = P(0x755294), caller = U(context.Esp);
        bool owned = _ownedRequest is { } dispatcher &&
            dispatcher.MatchesPendingOrdinaryEntry(RequestEntry, flow, [_ownedDestination, _ownedArgument], context);
        Require(context.Ecx == flow && U(flow) == FlowTable &&
            (caller is >= 0x400000 and < 0x6D0000 || owned),
            "Host progression request does not own its original Flow/caller.");
        uint destination = U(context.Esp + 4), argument = U(context.Esp + 8);
        ulong outcomeOperation = 0;
        if (owned)
        {
            if (_ownedOutcomeMenu is { } authority)
            {
                VerifyFreshOutcomeMenu(authority);
                outcomeOperation = authority.Operation;
            }
            Require(destination == _ownedDestination && argument == _ownedArgument && ++_ownedRequestUses == 1,
                "Owned progression request changed destination, argument or one-call interval.");
            _ownedRequest = null;
            _ownedOutcomeMenu = null;
            _log(new { @event = "progression-owned-flow-request-admitted", destination, argument,
                exactPendingDispatcherFunctionThisArgumentsCallerAndEspMatched = true,
                originalFlowRequestPreserved = true, genericPrivateCallerAllowed = false });
        }
        _request = new(flow, context.Esp, caller, destination, argument, ++_requestCalls, Queue(flow), _action is not null,
            outcomeOperation);
        _host.Breakpoints.AddReturn(caller, _thread, "Progression.Flow.original-request-return", OnRequestReturn);
        if (_requestCalls <= MaximumDetailedLogs)
            _log(new { @event = "progression-host-flow-request", sequence = _requestCalls,
                destinationRaw = destination, argumentRaw = argument, caller = Hex(caller),
                duringKnownChangeLevelAction = _action is not null, sceneAlreadyRetired = _sceneRetired,
                originalRequestPreserved = true, actualLevelEntryCompleted = false });
    }

    private void OnRequestReturn(uint tid, uint at, ref Win32Native.X86Context context)
    {
        if (_retired) return;
        EnsureStop(tid);
        RequestFrame frame = _request ?? throw new InvalidDataException("Host Flow return lost its entry pair.");
        Require(at == frame.Caller && context.Eip == at && context.Esp == checked(frame.Esp + 12) &&
            U(0x755294) == frame.Receiver && U(frame.Receiver) == FlowTable,
            "Host progression original RET8/frame or fresh Flow ownership differs.");
        uint[] after = Queue(frame.Receiver); _request = null; ++_requestReturns;
        _lastRequestReturn = new(frame.Sequence, frame.Receiver, frame.Destination, frame.RawArgument,
            frame.Caller, frame.OwnedOutcomeOperation);
        if (_requestReturns <= MaximumDetailedLogs)
            _log(new { @event = "progression-host-flow-request-returned", sequence = frame.Sequence,
                destinationRaw = frame.Destination, argumentRaw = frame.RawArgument,
                duringKnownChangeLevelAction = frame.DuringKnownTrigger,
                queueBefore = frame.QueueBefore, queueAfter = after, originalRet8Executed = true,
                rawQueueModesHaveNoNewSemanticNames = true, actualLevelEntryCompleted = false,
                noHistoricalSceneRead = true, nativeContextWritesByObserver = 0 });
    }

    private void VerifyScene(bool requireReady)
    {
        Require(_bound && !_sceneRetired && U(0x755294) == _flow && U(_flow) == FlowTable &&
            U(0x755274) == _engine && U(_engine + 0x18) == _scene && U(_scene) == 0x6E7358 &&
            U(0x765AD4) == _profile && U(_profile) == ProfileTable && U(_profile + 0x2B4) == _primary &&
            U(_primary) == 0x710F80 && U(_primary + 0x24) == _root &&
            U(_root) == VisualGraphReader.NodeVtable && U(_root + 0x3C) == _scene &&
            U(0x7552A0) == _input && U(_input) == 0x6F5508 && U(_input + 0x1C) == _device &&
            U(_device) == 0x7116CC && U(0x755298) == _timer && U(_timer) == 0x702F94 &&
            U(_playing) == _playingTable && U(_playing + 0x10) == _level,
            "Progression captured scene/primary/input/timer ownership changed before retirement.");
        uint depth = U(_flow + 0x1AC); Require(depth < 20, "Progression retained Flow stack exceeds its bound.");
        uint matches = 0;
        for (uint i = 0; i <= depth; ++i) if (U(_flow + 0x15C + 4 * i) == _playing) ++matches;
        Require(matches == 1, "Progression original Playing state is no longer retained exactly once.");
        if (requireReady)
            Require(U(_flow + 0x15C + 4 * depth) == _playing && U(_flow + 0x1B0) == _level &&
                U(_flow + 0x1B8) == 0 && U(_device + 0x14) == 3 && Read(_timer + 0x40, 1)[0] == 0,
                "ChangeLevel action requires the captured actual ready native scene.");
    }

    private void VerifyFreshOutcomeMenu(NativeProgressionOutcomeMenuAuthority authority)
    {
        // Fresh process-lifetime Flow and its current typed overlay only. Never
        // follow the retired scene/Playing/Profile/player/body captured earlier.
        Require(_story3 && _sceneRetired && authority.Flow == _flow && U(0x755294) == authority.Flow &&
            U(authority.Flow) == FlowTable && U(authority.Flow + 0x1B8) == 0,
            "Outcome Menu authority lost its settled process-lifetime Flow.");
        uint depth = U(authority.Flow + 0x1AC); Require(depth < 20, "Outcome Menu Flow exceeds its native bound.");
        uint top = P(authority.Flow + 0x15C + depth * 4);
        Require(top == authority.GameOver && U(top) == 0x70F778 && U(top + 0x10) == authority.GameOverId &&
            U(authority.Flow + 0x1B0) == authority.GameOverId && U(0x768630) == 0x331018ED &&
            U(0x70F778 + 0x10) == 0x5E0450 && Read(0x5E0450, 6).SequenceEqual(Convert.FromHexString("B830867600C3")) &&
            authority.MenuId is >= 51 and <= 80 && U(authority.Flow + 0x18 + authority.MenuId * 4) == authority.Menu &&
            U(authority.Menu) == 0x7102A8 && U(authority.Menu + 0x10) == authority.MenuId &&
            U(0x768990) == 0x284B3AA2 && U(0x7102A8 + 0x10) == 0x5EF690 &&
            Read(0x5EF690, 6).SequenceEqual(Convert.FromHexString("B890897600C3")),
            "Outcome Menu authority changed the current typed GameOver or registered original MainMenu.");
    }

    private void Repeat(uint site, string name, BreakpointCallback callback)
    {
        BreakpointCallback repeat = null!;
        repeat = (uint tid, uint at, ref Win32Native.X86Context context) =>
        {
            SetPending(site, false);
            if (_retired || _failed) return;
            try { callback(tid, at, ref context); }
            catch (Exception error) { _failed = true; _retired = true; _failure = error.Message; throw; }
            if (!_retired && !_failed)
            {
                _host.Breakpoints.AddReturn(site, _thread, name, repeat); SetPending(site, true);
            }
        };
        _host.Breakpoints.AddReturn(site, _thread, name, repeat); SetPending(site, true);
    }
    private void SetPending(uint site, bool value)
    { if (site == ChangeLevelAction) _actionProbePending = value; else _requestProbePending = value; }
    private void CheckCallback(uint tid, uint at, uint expected, in Win32Native.X86Context context)
    {
        EnsureStop(tid);
        Require(_installed && !_retired && !_failed && at == expected && context.Eip == at &&
            ++_callbacks <= MaximumCallbacks, "Progression callback escaped its exact owned site/thread/budget.");
    }
    private uint[] Queue(uint flow)
    {
        byte[] bytes = Read(flow + 0x1B8, 0x78); uint[] words = new uint[30];
        for (int i = 0; i < words.Length; ++i) words[i] = BitConverter.ToUInt32(bytes, i * 4);
        return words; // Ten original triples; no guessed operation semantics.
    }
    private uint P(uint address)
    { uint value = U(address); Require(value >= 0x10000 && (value & 3) == 0, "Progression pointer is invalid/unaligned."); return value; }
    private uint U(uint address) => BitConverter.ToUInt32(Read(address, 4));
    private byte[] Read(uint address, int bytes)
    {
        EnsureStop(_thread);
        Require(!_retired && !_failed && address >= 0x10000 && bytes is > 0 and <= 0x100 &&
            (ulong)address + (uint)bytes <= uint.MaxValue, "Progression native read is retired or outside its x86 bound.");
        return Win32Native.ReadMemory(_process, address, bytes);
    }
    private void EnsureOwner() => Require(Environment.CurrentManagedThreadId == _owner,
        "Progression gate has one debugger-owner thread.");
    private void EnsureStop(uint tid)
    {
        EnsureOwner();
        Require(_host.IsAtDebugStop && tid == _thread && _host.MainThreadId == _thread &&
            _host.ProcessHandle == _process && _host.ProcessPid == _pid && _host.ImageBase == 0x400000,
            "Progression gate requires its captured original main-thread debug stop.");
    }
    private static byte[] Image(PEReader image, uint address, int bytes) =>
        image.GetSectionData(checked((int)(address - 0x400000))).GetContent(0, bytes).ToArray();
    private static uint Word(PEReader image, uint address) => BitConverter.ToUInt32(Image(image, address, 4));
    private static string Hex(uint value) => $"0x{value:X8}";
    private static void Require(bool value, string message) { if (!value) throw new InvalidDataException(message); }
}
