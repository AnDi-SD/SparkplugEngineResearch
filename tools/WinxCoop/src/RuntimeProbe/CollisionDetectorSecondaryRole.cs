using System.Reflection.PortableExecutable;
using System.Text;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// Own bounded Ghoul melee role substitution. Native named enable notifications
/// define attack windows; one actual shared HP commit consumes each window.
/// Native AI continues to select the original primary, and all damage stays native.
/// </summary>
internal sealed class CollisionDetectorSecondaryRole
{
    private const uint DetectorTable = 0x6F7DD0, GhoulMachineTable = 0x6FA2E8, GhoulAiTable = 0x6FF338;
    private const uint GhoulAttackTable = 0x6F8860, NodeTable = 0x6DC4F4;
    private const uint RoleBranch = 0x510CE2, RoleContinuation = 0x510CE8;
    private const uint Candidate = 0x510CFC, DuplicateBranch = 0x510D07, NextCandidate = 0x510F0C;
    private const uint Send = 0x510E1A, SendReturn = 0x510E1F;
    private readonly DebuggerHost _host;
    private readonly PEReader _image;
    private readonly Action<object> _log;
    private readonly int _ownerThread;
    private readonly Dictionary<uint, Window> _windows = [];
    private readonly Dictionary<uint, UpdateFrame> _updates = [];
    private readonly HashSet<uint> _flagPending = [];
    private readonly List<SendFrame> _sends = [];
    private readonly Dictionary<string, uint> _counts = [];
    private readonly List<object> _samples = [];
    private SharedHealthAlias? _health;
    private uint _instance, _root, _move, _character, _scene, _metadata, _profile, _primary, _generation, _worlds, _retireEntry;
    private bool _bound, _retired, _failed;

    private sealed record Source(uint Instance, uint Root, uint Character, uint Machine, uint Ai, uint Node, string Name);
    private sealed class Window(uint detector, uint generation, Source source)
    {
        internal readonly uint Detector = detector, Generation = generation;
        internal readonly Source Source = source;
        internal bool Enabled = true, SharedHitCommitted;
        internal uint Sends, OwnSends, Commits, Suppressions;
    }
    private sealed class UpdateFrame(Window window, Win32Native.X86Context c, uint caller)
    {
        internal readonly Window Window = window;
        internal readonly uint Esp = c.Esp, Caller = caller, Esi = c.Esi, Edi = c.Edi, Ebx = c.Ebx, Ebp = c.Ebp;
        internal uint RoleAdmittedTarget, CandidateTarget;
    }
    private sealed class SendFrame(UpdateFrame update, Win32Native.X86Context c, uint target, uint commits)
    {
        internal readonly UpdateFrame Update = update;
        internal readonly uint Esp = c.Esp, Target = target, Commits = commits;
        internal readonly uint Esi = c.Esi, Edi = c.Edi, Ebx = c.Ebx, Ebp = c.Ebp;
    }

    internal CollisionDetectorSecondaryRole(DebuggerHost host, PEReader image, Action<object> log)
    {
        _host = host ?? throw new ArgumentNullException(nameof(host));
        _image = image ?? throw new ArgumentNullException(nameof(image));
        _log = log ?? throw new ArgumentNullException(nameof(log));
        _ownerThread = Environment.CurrentManagedThreadId;
    }
    internal bool Active => _bound && !_retired && !_failed;
    internal bool HasPendingNativePair => _updates.Count != 0 || _flagPending.Count != 0 || _sends.Count != 0;
    internal uint OwnNativeDamageSendCount => _counts.GetValueOrDefault("native.own2717-callsite-reached");
    internal object Summary => new
    {
        bound = _bound, active = Active, retiredBeforeOwnDelete = _retired, terminalFailure = _failed,
        verifiedWorlds = _worlds, generationsReserved = _generation, knownDetectors = _windows.Count,
        pendingUpdates = _updates.Count, pendingEnablePairs = _flagPending.Count, pendingSends = _sends.Count,
        ownInstance = Hex(_instance), primaryInstance = Hex(_primary), scene = Hex(_scene), retireEntry = Hex(_retireEntry),
        counts = new Dictionary<string, uint>(_counts), samples = _samples.ToArray(),
        windows = _windows.Values.Select(w => new { detector = Hex(w.Detector), generation = w.Generation,
            sourceInstance = Hex(w.Source.Instance), sourceCharacter = Hex(w.Source.Character), sourceMachine = Hex(w.Source.Machine),
            sourceAi = Hex(w.Source.Ai), node = Hex(w.Source.Node), w.Source.Name, w.Enabled,
            w.SharedHitCommitted, w.Sends, w.OwnSends, w.Commits, w.Suppressions }).ToArray(),
        epochPolicy = "each paired matching native GhoulAttack271F true; repeated true resets the native attack window; false closes it",
        sharedHitPolicy = "first actual shared HP commit per native detector window; suppress later primary/own candidates before271C/275C/2717",
        existingEnabledDetectorsAdopted = false, pointerOrFrameBasedDamageDedup = false,
        nativeAiStillTargetsOriginalPrimary = true, closestBodyTargetingClaimed = false,
        metadataOrGameFieldWrites = 0, nativeCallsAdded = 0, privatePagesAllocated = 0,
        onlyContextChanges = "own role0 JZ510CE2->510CE8; consumed shared-player JE510D07->510F0C after native hit-seen store",
        runtimeOwnHitQualificationClaimed = false, fullE3Complete = false
    };

    internal void BindAtWorld(uint tid, ref Win32Native.X86Context context,
        uint ownInstance, uint ownRoot, uint ownMove, uint scene, SharedHealthAlias health)
    {
        Ensure(tid); ArgumentNullException.ThrowIfNull(health);
        Require(!_bound && !_retired && !_failed && context.Eip == NativeCallDispatcher.WorldReturn &&
            _host.ImageBase == 0x400000 && health.Active, "Ghoul role binding requires its first WorldRET and active shared HP.");
        _instance = ownInstance; _root = ownRoot; _move = ownMove; _scene = scene; _health = health;
        _character = Pointer(_move + 0x160); _metadata = Pointer(_move + 0x174);
        _profile = Pointer(0x765AD4); _primary = Pointer(_profile + 0x2B4); VerifyOwn();
        Pin(0x510270, "B808F27400C3"); Pin(0x561100, "B8283B7500C3");
        Pin(0x528400, "B8A8157500C3"); Pin(0x5179A0, "B828FC7400C3");
        Require(U(0x74F208) == 0x7FA9339C && U(0x753B28) == 0x4BBA28D3 && U(0x7515A8) == 0x3CE04060 &&
            U(0x74FC28) == 0x08A40E12 && ImageWord(DetectorTable + 0x10) == 0x510270 &&
            U(DetectorTable + 0x10) == 0x510270 && U(DetectorTable + 0x3C) == 0x510BA0 &&
            U(GhoulMachineTable + 0x10) == 0x528400 && U(GhoulAiTable + 0x10) == 0x561100 &&
            U(GhoulAttackTable + 0x10) == 0x5179A0, "Ghoul detector/AI/FSM/attack original identities differ.");
        Pin(0x510B85, "3D1F27000075E852E86EF8FFFF83C44CC20400");
        Pin(0x51044E, "898F640100008B451C8887400100005F5E5DC20400");
        Pin(0x510CA6, "0FB745048BD083E208895424240F84C8010000");
        Pin(0x510CC4, "8B4E243BF90F843D020000");
        Pin(0x510CCF, "8B966C0100003BD375118BD883E310895C2424");
        Pin(RoleContinuation, "33DB83FA01750D");
        Pin(0x510D02, "C644241301");
        Pin(0x510DCE, "8BC45189388BC4C600018B4624681C270000");
        Pin(0x510E02, "518BC45189188B8E680100008BC468172700008908578BCEE8114FF2FF");
        Pin(0x510E44, "89BE64010000E9BD000000"); Pin(0x510F26, "385C24137506899E640100005F5E5DB0015B83C438C3");
        Pin(0x435D30, "83EC208B4424288B542430894C2410"); Pin(0x435D71, "83C420C21000");
        Pin(0x517A47, "C744240C1F270000"); Pin(0x517A63, "C74424245C716F00");
        Pin(0x517ACD, "C700D0886F00");
        Pin(0x510470, "568BF1E808FEFFFF");
        _bound = true;
        Hook(0x510400, "558B6C2408568B7518578BF9", "native-enable", Enable);
        Hook(0x510BA0, "A19852750083EC3853555633DB", "native-update", Update);
        Hook(RoleBranch, "0F84C1000000", "role0-decision", Role);
        Hook(Candidate, "3BBE64010000", "shared-candidate", CandidateStop);
        Hook(DuplicateBranch, "0F84FF010000", "shared-duplicate-decision", DuplicateStop);
        Hook(Send, "E8114FF2FF", "native-send", NativeSend);
        Hook(0x510280, "FF25B0263B01", "native-destructor", Destroy);
        Sample("bound", new { primaryInstance = Hex(_primary), ownInstance = Hex(_instance),
            ownMetadata = Hex(_metadata), flagsWord = Word(_metadata + 4), existingActiveWindowsAdopted = false });
    }

    internal void VerifyAtWorld(uint tid, ref Win32Native.X86Context context)
    {
        Ensure(tid); Require(context.Eip == NativeCallDispatcher.WorldReturn, "Ghoul role verification requires WorldRET.");
        if (_retired) return;
        Require(Active && _health?.Active == true && !HasPendingNativePair && ++_worlds <= 36000,
            "Ghoul role reached WorldRET with an unmatched native operation or outside its budget.");
        VerifyOwn(); foreach (Window window in _windows.Values) VerifyWindow(window);
    }
    internal void RetireBeforeOwnDelete(uint tid, ref Win32Native.X86Context context)
    {
        Ensure(tid);
        Require(Active && _health?.Active == true && !HasPendingNativePair && context.Eip is
            NativeCallDispatcher.WorldReturn or 0x5F2300 or 0x5F86C0 or 0x4E8AA0 or 0x4E8C80 or 0x59AE80 or 0x59B190,
            "Ghoul role retirement requires its original pre-delete boundary and no pending operation.");
        VerifyOwn(); _retireEntry = context.Eip; _retired = true;
        Sample("retired-before-own-delete", new { entry = Hex(context.Eip), callbacksDormantBeforeDelete = true,
            historicalObjectReadsNowForbidden = true, foreignDetectorsNotDeletedByAdapter = true });
    }

    // The death peer may ask only while the actual native435D30 send is pending.
    // This exposes evidence; it never publishes command bytes or consumes HP.
    internal bool IsQualifiedNativeDamageFrame(uint detector, uint targetInstance)
    {
        Ensure(_host.MainThreadId);
        return Active && _sends.Count > 0 && _sends[^1].Target == targetInstance &&
            _sends[^1].Update.Window.Detector == detector && !_sends[^1].Update.Window.SharedHitCommitted &&
            _updates.TryGetValue(detector, out UpdateFrame? update) && ReferenceEquals(update, _sends[^1].Update);
    }

    private void Enable(uint tid, ref Win32Native.X86Context c)
    {
        Source? source = CaptureSource(c.Ecx); if (source is null) return;
        uint detector = c.Ecx, message = Pointer(c.Esp + 4), caller = U(c.Esp), esp = c.Esp;
        Require(caller == 0x510B92 && U(message) == 0x271F, "Ghoul enable lacks the original detector notification caller.");
        uint name = U(message + 0x18); if (name != 0 && Text(name, 32) != source.Name) return;
        byte enabled = Byte(message + 0x1C); Require(enabled <= 1, "Ghoul native enable is not a bool.");
        uint sender = Pointer(message + 0x10);
        if (enabled != 0)
            Require(U(sender) == GhoulAttackTable && U(sender + 0x14) == source.Machine,
                "A fresh Ghoul attack window requires the exact native GhoulAttack owner/state.");
        Require(!_updates.ContainsKey(detector) && !_flagPending.Contains(detector) && _flagPending.Count < 8,
            "Ghoul enable cannot replace a live detector update or pending window.");
        uint esi = c.Esi, edi = c.Edi, ebx = c.Ebx, ebp = c.Ebp;
        _flagPending.Add(detector);
        _host.Breakpoints.AddReturn(caller, tid, "CollisionDetectorSecondaryRole.enable.return",
            (uint rt, uint at, ref Win32Native.X86Context returned) =>
            {
                Ensure(rt);
                try
                {
                    Require(Active && _flagPending.Contains(detector) && at == caller && returned.Eip == caller &&
                        returned.Esp == checked(esp + 8) && returned.Esi == esi && returned.Edi == edi && returned.Ebx == ebx && returned.Ebp == ebp &&
                        CaptureSource(detector) == source && Byte(detector + 0x140) == enabled && U(detector + 0x164) == 0,
                        "Ghoul native named enable RET4 lost its source/frame/enable/reset witness.");
                    if (enabled != 0)
                    {
                        Require(++_generation <= 4096 && (_windows.ContainsKey(detector) || _windows.Count < 64), "Ghoul attack-window budget exceeded.");
                        _windows[detector] = new(detector, _generation, source); Add("window.native-enable-true-committed");
                    }
                    else if (_windows.TryGetValue(detector, out Window? previous))
                    { previous.Enabled = false; Add("window.native-enable-false-committed"); }
                    Sample("native-window-notification-return", new { detector = Hex(detector), sourceInstance = Hex(source.Instance),
                        node = Hex(source.Node), source.Name, enabled, generation = _windows.GetValueOrDefault(detector)?.Generation,
                        caller = Hex(caller), nativeEspMatched = true, nativeVictimResetObserved = true, adapterFieldWrites = 0 });
                    _flagPending.Remove(detector);
                }
                catch { _failed = true; throw; }
            });
    }
    private void Update(uint tid, ref Win32Native.X86Context c)
    {
        if (!_windows.TryGetValue(c.Ecx, out Window? window) || !window.Enabled) return;
        VerifyWindow(window); VerifyOwn();
        Require(_updates.Count < 8 && !_updates.ContainsKey(c.Ecx) && c.Esp >= 0x10100 && (c.Esp & 3) == 0,
            "Ghoul native update nesting or original stack exceeds its bound.");
        UpdateFrame frame = new(window, c, U(c.Esp)); _updates.Add(window.Detector, frame);
        _host.Breakpoints.AddReturn(frame.Caller, tid, "CollisionDetectorSecondaryRole.update.return",
            (uint rt, uint at, ref Win32Native.X86Context returned) =>
            {
                Ensure(rt);
                try
                {
                    Require(Active && at == frame.Caller && returned.Eip == at && returned.Esp == checked(frame.Esp + 4) &&
                        returned.Esi == frame.Esi && returned.Edi == frame.Edi && returned.Ebx == frame.Ebx && returned.Ebp == frame.Ebp &&
                        _updates.TryGetValue(window.Detector, out UpdateFrame? current) && ReferenceEquals(current, frame) &&
                        !_sends.Any(s => ReferenceEquals(s.Update, frame)), "Ghoul native update C3 return lost its exact paired frame.");
                    VerifyWindow(window); _updates.Remove(window.Detector); Add("update.native-returned");
                }
                catch { _failed = true; throw; }
            });
    }
    private void Role(uint tid, ref Win32Native.X86Context c)
    {
        if (c.Edi != _instance || !_updates.TryGetValue(c.Esi, out UpdateFrame? frame)) return;
        VerifyFrame(frame, c); VerifyOwn();
        Require(c.Ebp == _metadata && c.Edx == 0 && c.Ebx == 0 && (c.EFlags & 0x40) != 0 &&
            (Word(_metadata + 4) & 0x18) == 8 && c.Ecx == frame.Window.Source.Instance && U(_metadata + 8) == _instance,
            "Ghoul own role0 substitution lacks exact metadata/mask8/source-exclusion/JZ proof.");
        frame.RoleAdmittedTarget = _instance; Add("role.own-filter0-overridden"); c.Eip = RoleContinuation;
    }
    private void CandidateStop(uint tid, ref Win32Native.X86Context c)
    {
        if (c.Edi != _instance && c.Edi != _primary || !_updates.TryGetValue(c.Esi, out UpdateFrame? frame)) return;
        VerifyFrame(frame, c); VerifyOwn();
        Require(c.Ebx == 0 && c.Edx == 0 && U(c.Ebp + 8) == c.Edi && (Word(c.Ebp + 4) & 8) != 0 &&
            (c.Edi != _instance || c.Ebp == _metadata && frame.RoleAdmittedTarget == _instance),
            "Ghoul shared-player candidate lost its original accepted role/metadata frame.");
        frame.CandidateTarget = c.Edi;
    }
    private void DuplicateStop(uint tid, ref Win32Native.X86Context c)
    {
        if (c.Edi != _instance && c.Edi != _primary || !_updates.TryGetValue(c.Esi, out UpdateFrame? frame)) return;
        VerifyFrame(frame, c); VerifyOwn();
        Require(frame.CandidateTarget == c.Edi && Byte(c.Esp + 0x13) == 1,
            "Ghoul shared-player dedup lacks its original candidate and native hit-seen store.");
        if (!frame.Window.SharedHitCommitted) return;
        ++frame.Window.Suppressions; Add(c.Edi == _instance ? "shared-window.own-duplicate-suppressed" : "shared-window.primary-duplicate-suppressed");
        // Persistent overlap can reach this branch every native update. Keep
        // counters for all suppressions and reserve detail slots for later hits.
        if (_counts.GetValueOrDefault("shared-window.own-duplicate-suppressed") +
            _counts.GetValueOrDefault("shared-window.primary-duplicate-suppressed") <= 16)
            Sample("shared-window-candidate-suppressed", new { detector = Hex(c.Esi), generation = frame.Window.Generation,
                target = Hex(c.Edi), from = Hex(DuplicateBranch), to = Hex(NextCandidate), before271C275C2717 = true,
                sharedHpAlreadyCommitted = true, nativeHitSeenStoreAlreadyExecuted = true,
                nativeLastVictimWrittenByAdapter = false });
        c.Eip = NextCandidate;
    }
    private void NativeSend(uint tid, ref Win32Native.X86Context c)
    {
        uint target = U(c.Esp); if (target != _instance && target != _primary) return;
        if (!_updates.TryGetValue(c.Esi, out UpdateFrame? frame)) return; // Existing active windows are never adopted.
        VerifyFrame(frame, c, stackArguments: 16); VerifyOwn();
        Require(c.Ecx == c.Esi && c.Edi == target && frame.CandidateTarget == target && !frame.Window.SharedHitCommitted &&
            U(c.Esp + 4) == 0x2717 && U(c.Esp + 8) == 1 && U(c.Esp + 12) == 0 &&
            (target != _instance || frame.RoleAdmittedTarget == target) && _sends.Count < 8,
            "Ghoul native shared-player damage call differs from its one-damage accepted attack window.");
        SendFrame send = new(frame, c, target, _health!.CommittedDamageTransactions); _sends.Add(send);
        ++frame.Window.Sends;
        if (target == _instance) { ++frame.Window.OwnSends; Add("native.own2717-callsite-reached"); }
        else Add("native.primary2717-callsite-reached");
        Sample("native2717-callsite", new { detector = Hex(c.Esi), generation = frame.Window.Generation,
            sourceInstance = Hex(frame.Window.Source.Instance), target = Hex(target), damageRaw = 1,
            nativeEsp = Hex(c.Esp), nativeCallNotReplaced = true, canonicalCommitsBefore = send.Commits });
        _host.Breakpoints.AddReturn(SendReturn, tid, "CollisionDetectorSecondaryRole.send.return",
            (uint rt, uint at, ref Win32Native.X86Context returned) =>
            {
                Ensure(rt);
                try
                {
                    Require(Active && _sends.Count > 0 && ReferenceEquals(_sends[^1], send) && at == SendReturn && returned.Eip == at &&
                        returned.Esp == checked(send.Esp + 16) && returned.Esi == send.Esi && returned.Edi == send.Edi &&
                        returned.Ebx == send.Ebx && returned.Ebp == send.Ebp && _health!.Active,
                        "Ghoul native damage-send RET16 lost its exact paired source/stack/peer.");
                    uint commits = _health!.CommittedDamageTransactions;
                    Require(commits >= send.Commits && commits - send.Commits <= 1, "One Ghoul native send committed more than one shared damage transaction.");
                    if (commits != send.Commits)
                    { frame.Window.SharedHitCommitted = true; ++frame.Window.Commits; Add("shared-window.actual-health-commit"); }
                    Sample("native2717-return", new { detector = Hex(frame.Window.Detector), generation = frame.Window.Generation,
                        target = Hex(target), canonicalCommitsBefore = send.Commits, canonicalCommitsAfter = commits,
                        actualHpCommitConsumesWindow = commits != send.Commits, nativeImmunityDoesNotConsumeWindow = commits == send.Commits,
                        nativeEspMatched = true, nativeLastVictimStoreStillDeferred = true });
                    _sends.RemoveAt(_sends.Count - 1);
                }
                catch { _failed = true; throw; }
            });
    }
    private void Destroy(uint tid, ref Win32Native.X86Context c)
    {
        uint detector = c.Ecx;
        if (!_windows.TryGetValue(detector, out Window? window)) return;
        Require(!_updates.ContainsKey(detector) && !_flagPending.Contains(detector) && !_sends.Any(s => s.Update.Window.Detector == detector) &&
            U(detector) == DetectorTable && U(detector + 0x24) == window.Source.Instance && U(detector + 0x18) == window.Source.Node,
            "Ghoul detector destruction has a pending operation or changed allocation identity.");
        _windows.Remove(detector); Add("window.native-destructor-retired");
    }

    private Source? CaptureSource(uint detector)
    {
        if (U(detector) != DetectorTable) return null;
        uint instance = Pointer(detector + 0x24); if (instance == _instance || instance == _primary || U(instance) != 0x710F80) return null;
        uint root = Pointer(instance + 0x24); if (U(root) != NodeTable || U(root + 0x3C) != _scene) return null;
        string name = Text(detector + 0x141, 32); if (name is not ("hand_left" or "L_Toe")) return null;
        uint character = FindComponent(instance, 0x6F6438), machine = FindComponent(instance, GhoulMachineTable), ai = FindComponent(instance, GhoulAiTable);
        if (character == 0 || machine == 0 || ai == 0) return null;
        uint node = Pointer(detector + 0x18), cursor = node;
        var ancestors = new HashSet<uint>();
        while (cursor != root)
        {
            Require(ancestors.Count < 64 && ancestors.Add(cursor) && U(cursor) == NodeTable && U(cursor + 0x3C) == _scene,
                "Ghoul detector bound node has no bounded typed ancestry to its own source Root.");
            cursor = Pointer(cursor + 0x2C);
        }
        Require(U(character + 0x24) == instance && U(character + 0x18) == root && U(character + 0x124) == machine &&
            U(character + 0x134) == ai && U(machine + 0x24) == instance && U(machine + 0x124) == character &&
            U(ai + 0x24) == instance && U(ai + 0x144) == character && U(detector + 0x168) == 1 &&
            U(detector + 0x16C) == 0 && Byte(detector + 0x171) == 0,
            "Ghoul detector source/Character/FSM/AI/damage1/role0/ordinary-hurt graph differs.");
        return new(instance, root, character, machine, ai, node, name);
    }
    private uint FindComponent(uint instance, uint table)
    {
        uint count = U(instance + 0x20), head = Pointer(instance + 0x1C), link = U(head), previous = head, found = 0;
        Require(count is > 0 and <= 64, "Ghoul source component count is outside the bounded template.");
        var seen = new HashSet<uint>();
        for (uint i = 0; i < count; ++i)
        {
            Require(link != head && link >= 0x10000 && seen.Add(link) && U(link + 4) == previous, "Ghoul source component list differs.");
            uint item = Pointer(link + 8);
            if (U(item) == table) { Require(found == 0, "Ghoul source component is not unique."); found = item; }
            previous = link; link = U(link);
        }
        Require(link == head && U(head + 4) == previous, "Ghoul source component sentinel/count differs.");
        return found;
    }
    private void VerifyWindow(Window window) => Require(_windows.TryGetValue(window.Detector, out Window? current) &&
        ReferenceEquals(current, window) && CaptureSource(window.Detector) == window.Source &&
        Byte(window.Detector + 0x140) == (window.Enabled ? 1 : 0) && window.Commits <= 1,
        "Ghoul native detector window/source/enable identity changed without its paired witness.");
    private void VerifyFrame(UpdateFrame frame, Win32Native.X86Context c, uint stackArguments = 0)
    {
        VerifyWindow(frame.Window);
        Require(_updates.TryGetValue(c.Esi, out UpdateFrame? current) && ReferenceEquals(current, frame) && frame.Window.Enabled &&
            c.Esp == checked(frame.Esp - 0x48 - stackArguments) && _health?.Active == true,
            "Ghoul collision decision lost its exact C3 update frame/window/shared HP peer.");
    }
    private void VerifyOwn() => Require(_health?.Active == true && U(0x765AD4) == _profile && U(_profile) == 0x6F5A44 &&
        U(_profile + 0x2B4) == _primary && _primary != _instance && U(_primary) == 0x710F80 &&
        U(_instance) == 0x710F80 && U(_instance + 0x24) == _root && U(_root) == NodeTable && U(_root + 0x3C) == _scene &&
        U(_move) == 0x6F64F8 && U(_move + 0x24) == _instance && U(_move + 0x18) == _root &&
        U(_move + 0x160) == _character && U(_move + 0x174) == _metadata && U(_root + 0xC) == _metadata &&
        U(_metadata + 8) == _instance && (Word(_metadata + 4) & 0x18) == 8 && U(_character) == 0x6F6438 &&
        U(_character + 0x24) == _instance && U(_character + 0x12C) == _move,
        "Ghoul role lost its separate primary/own typed metadata owner; raw role10 is never enabled.");

    private delegate void Stop(uint tid, ref Win32Native.X86Context context);
    private void Hook(uint address, string hex, string name, Stop callback)
    {
        Pin(address, hex); byte[] expected = Convert.FromHexString(hex);
        Require(!_host.Breakpoints.Contains(address), "Ghoul role native hook already has an owner.");
        BreakpointCallback repeat = null!;
        repeat = (uint tid, uint at, ref Win32Native.X86Context c) =>
        {
            Ensure(tid); if (_retired || _failed) return;
            Require(Active && at == address && c.Eip == at && Read(address, expected.Length).SequenceEqual(expected), "Ghoul role hook identity/signature differs.");
            try { callback(tid, ref c); } catch { _failed = true; throw; }
            if (Active) _host.Breakpoints.AddReturn(address, tid, "CollisionDetectorSecondaryRole." + name, repeat);
        };
        _host.Breakpoints.AddReturn(address, _host.MainThreadId, "CollisionDetectorSecondaryRole." + name, repeat);
    }
    private void Pin(uint address, string hex)
    {
        byte[] expected = Convert.FromHexString(hex);
        Require(_image.GetSectionData(checked((int)(address - 0x400000))).GetContent(0, expected.Length).ToArray().SequenceEqual(expected) &&
            Read(address, expected.Length).SequenceEqual(expected), $"Ghoul role native pin differs at {Hex(address)}.");
    }
    private uint ImageWord(uint address) => BitConverter.ToUInt32(_image.GetSectionData(checked((int)(address - 0x400000))).GetContent(0, 4).ToArray());
    private string Text(uint address, int bound)
    { byte[] raw = Read(address, bound); int end = Array.IndexOf(raw, (byte)0); Require(end >= 0, "Ghoul role bounded name is unterminated."); return Encoding.ASCII.GetString(raw, 0, end); }
    private uint Pointer(uint address) { uint value = U(address); Require(value >= 0x10000 && (value & 3) == 0, "Ghoul role typed pointer is null/unaligned."); return value; }
    private uint U(uint address) => BitConverter.ToUInt32(Read(address, 4));
    private ushort Word(uint address) => BitConverter.ToUInt16(Read(address, 2));
    private byte Byte(uint address) => Read(address, 1)[0];
    private byte[] Read(uint address, int bytes)
    { Ensure(_host.MainThreadId); Require(address >= 0x10000 && bytes is > 0 and <= 256, "Ghoul role read exceeds its bound."); return Win32Native.ReadMemory(_host.ProcessHandle, address, bytes); }
    private void Add(string name) { uint count = _counts.GetValueOrDefault(name) + 1; Require(count <= 200000, "Ghoul role callback budget exceeded."); _counts[name] = count; }
    private void Sample(string name, object data)
    { if (_samples.Count >= 128) return; _samples.Add(new { name, data }); _log(new { @event = "collision-detector-secondary-role", name, data }); }
    private void Ensure(uint tid) => Require(_host.IsAtDebugStop && tid == _host.MainThreadId && Environment.CurrentManagedThreadId == _ownerThread,
        "Ghoul role requires its owned main-thread debug stop.");
    private static string Hex(uint value) => $"0x{value:X8}";
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidDataException(message); }
}
