using System.Buffers.Binary;
using System.Reflection.PortableExecutable;
using System.Security.Cryptography;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// Battle47-only admission of the second Character to each stock Mosquito's
/// original perception scan. Native range/cone/list/nearest selection remains
/// original; the three primary-only Node consumers use that actual selection.
/// Borrowed targets may survive until the synchronous original scene exit.
/// This helper cannot be retired for a live-scene body despawn or reconnect.
/// </summary>
internal sealed class NativeMosquitoPlayerTargets
{
    private const uint ScanEntry = 0x4F1C70, ScanList = 0x4F1CA8, ScanReturn = 0x4F1E78;
    private const uint AttackEntry = 0x5B4B40, AttackTick = 0x5B5090, AttackMoveNode = 0x5B4C6A;
    private const uint FacingNode = 0x5B4F58, ShotEntry = 0x510000, ShotNode = 0x510027;
    private const uint BattleExit = 0x5F2300, CharacterTable = 0x6F6438, AttackTable = 0x7096D0;
    private const int MaximumCandidates = 64, BufferBytes = 0x20 + MaximumCandidates * 4;
    private readonly DebuggerHost _host;
    private readonly PEReader _image;
    private readonly Action<object> _log;
    private readonly IntPtr _process;
    private readonly uint _pid, _thread;
    private readonly int _owner;
    private readonly Dictionary<uint, Enemy> _perceptions = [];
    private readonly Dictionary<uint, Enemy> _actions = [], _managers = [];
    private readonly Dictionary<uint, uint> _selectedActionTargets = [];
    private readonly List<object> _samples = [];
    private OwnedRemoteBuffer? _scratch;
    private ScanFrame? _frame;
    private NearestFrame? _nearest;
    private uint _instance, _root, _character, _scene, _profile, _primaryInstance, _primaryCharacter, _engine, _sceneManager, _sceneManagerTable;
    private bool _bound, _retired, _exitReturned, _sceneUnloaded, _failed;
    private string _failure = "";
    private uint _scans, _scanReturns, _nativeP2Memberships, _nodeOverrides, _p2Selections, _worlds, _nativeResets;
    private long _lastDetailedTick = -1;
    private bool _proofFailureLogged;
    private uint _aimAnchor, _aimAnchorParent, _tracedShotDirections;
    private string? _aimAnchorError;

    private sealed record Enemy(uint Perception, uint Ai, uint Character, uint Instance, uint Root, uint Move, uint Manager);
    private sealed record ScanFrame(Enemy Enemy, uint Esp, uint Caller, uint SavedEdi, uint SavedEsi,
        uint SavedEbp, uint SavedEbx, uint OriginalList, uint SourceBegin, uint SourceEnd, uint[] Members);
    private sealed record NearestFrame(Enemy Enemy, uint Action, uint Esp, uint Ebx, uint Ebp, uint Edi);

    internal NativeMosquitoPlayerTargets(DebuggerHost host, PEReader image, Action<object> log)
    {
        ArgumentNullException.ThrowIfNull(host); ArgumentNullException.ThrowIfNull(image); ArgumentNullException.ThrowIfNull(log);
        Require(host.IsAtDebugStop && host.ProcessHandle != IntPtr.Zero && host.ProcessPid != 0 &&
            host.MainThreadId != 0 && host.ImageBase == 0x400000,
            "Mosquito player targets require the owned original main-thread session.");
        Require(Convert.ToHexString(SHA256.HashData(image.GetEntireImage().GetContent().ToArray())) ==
            "3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F", "Mosquito target PE differs.");
        _host = host; _image = image; _log = log; _process = host.ProcessHandle; _pid = host.ProcessPid;
        _thread = host.MainThreadId; _owner = Environment.CurrentManagedThreadId;
    }

    internal bool Active => _bound && !_retired && !_failed;
    internal bool HasLiveOwnership => _frame is not null || _nearest is not null || _scratch is { Released: false };
    internal object Summary => new
    {
        active = Active, bound = _bound, retired = _retired, sceneUnloaded = _sceneUnloaded,
        originalBattleExitReturned = _exitReturned,
        failed = _failed, failure = _failure, nativeScans = _scans, nativeScanReturns = _scanReturns,
        nativeSecondCharacterMemberships = _nativeP2Memberships, actualSecondTargetSelections = _p2Selections,
        selectedNodeOverrides = _nodeOverrides, worlds = _worlds, knownMosquitoes = _perceptions.Count,
        nativePerceptionResets = _nativeResets,
        pendingScanFrame = _frame is not null, pendingNearestFrame = _nearest is not null, scratch = _scratch?.Summary, samples = _samples,
        targetChoice = "original per-enemy Perception range/cone/list and FindNearest",
        borrowedTargetLifetime = "Battle47 synchronous own Delete then original scene Exit; no live-scene despawn",
        profileWrites = 0, targetManagerWrites = 0, characterRoleWrites = 0,
        originalPerceptionListWritesByAdapter = 0, ownScratchWritesOnly = true,
        callbackRegisterWrites = new[] { "EAX at exact original candidate-list/selected-Node consumers" },
        wholeEnemyFamilyOrLevelTransitionsQualified = false
    };

    internal void BindAtWorld(uint tid, ref Win32Native.X86Context context,
        uint ownInstance, uint ownRoot, uint ownCharacter, uint scene)
    {
        Ensure(tid); Require(!_bound && !_retired && !_failed && context.Eip == NativeCallDispatcher.WorldReturn,
            "Mosquito target binding requires its first real WorldRET.");
        _instance = ownInstance; _root = ownRoot; _character = ownCharacter; _scene = scene;
        _profile = P(0x765AD4); _primaryInstance = P(_profile + 0x2B4); _engine = P(0x755274);
        _sceneManager = P(0x75DB90); _sceneManagerTable = U(_sceneManager);
        _primaryCharacter = ReadCharacter(_primaryInstance);
        VerifyPlayers();
        Pin(ScanEntry, "83EC0C5355568BF1"); Pin(ScanList, "8BD83BDD895C2418"); Pin(ScanReturn, "C3");
        Pin(0x4F1CA3, "E8B804FFFF"); Pin(0x4F1D40, "8A466584C08B3CA9");
        Pin(AttackEntry, "51568BF1"); Pin(AttackTick, "568BF1E898FEFFFF");
        Pin(AttackMoveNode, "8B4E208B9144010000"); Pin(FacingNode, "D940748B7720");
        Pin(ShotEntry, "A1F05A760083EC3C"); Pin(ShotNode, "85C08986E0010000");
        Pin(0x5B4B6E, "8986A8030000"); Pin(0x5B4C79, "8981E0010000");
        Pin(0x5B4B69, "E8A2CDF3FF"); Pin(0x4F19BD, "8BC75F83C41CC3");
        Pin(BattleExit, "568BF18B8E54020000"); Pin(0x5F2340, "5EC20400");
        Pin(0x45A7F9, "C74720000000005F5EC3");
        Pin(0x4F18E0, "568BF18D4E14E8A5CDFCFF");
        Pin(0x5100C5, "E8A6FDFFFF");
        Pin(0x6FF950, "10505600"); Pin(0x6FF9D0, "90545600");
        _scratch = new OwnedRemoteBuffer(_host, BufferBytes);
        ResolveOwnedAimAnchor();
        Require(_aimAnchor != 0, "Mosquito secondary aim requires one owned target_node: " + _aimAnchorError);
        uint primaryTargetNode = P(P(P(0x765AF0) + 0x28) + 0x18);
        Require(U(primaryTargetNode) == 0x6DC4F4 && ReadNodeName(primaryTargetNode) == "target_node",
            "Canonical primary TargetRegion no longer uses the measured target_node anchor.");
        NativeTargetNodeOwnership.VerifyAtStop(_host, primaryTargetNode, P(_primaryInstance + 0x24), _scene);
        VerifyAimAnchor();
        _bound = true;
        Hook(ScanEntry, "scan-entry", GuardScanEntry);
        Hook(0x4F18E0, "perception-native-reset", RetireResetPerception);
        Hook(ScanList, "candidate-list", AdmitCandidates);
        Hook(AttackEntry, "attack-entry", GuardActionEntry);
        Hook(AttackTick, "attack-tick", GuardActionEntry);
        Hook(0x5B4B69, "native-nearest-call", ObserveNearestCall);
        Hook(AttackMoveNode, "attack-move-node", (uint t, ref Win32Native.X86Context c) => RedirectActionNode(t, ref c, c.Esi, "move"));
        Hook(FacingNode, "attack-facing-node", (uint t, ref Win32Native.X86Context c) => RedirectActionNode(t, ref c, c.Edi, "facing"));
        Hook(ShotEntry, "shot-entry", GuardShotEntry);
        Hook(ShotNode, "shot-target-node", RedirectShotNode);
        Hook(0x5100C5, "shot-native-direction", TraceShotDirection);
        Sample("bound", new { ownInstance = Hex(_instance), ownCharacter = Hex(_character), primaryCharacter = Hex(_primaryCharacter),
            scene = Hex(_scene), scratch = _scratch.Summary, nativeChoiceRetained = true,
            targetNodeAnchor = Hex(_aimAnchor), targetRoot = Hex(_root), primaryTargetNode = Hex(primaryTargetNode) });
    }

    internal void VerifyAtWorld(uint tid, ref Win32Native.X86Context context)
    {
        Ensure(tid); Require(Active && context.Eip == NativeCallDispatcher.WorldReturn && _frame is null && _nearest is null,
            "Mosquito target WorldRET requires a live cohort with no pending scan borrow.");
        Require(++_worlds <= NativeObservationBudget.WorldLimit && U(0x755274) == _engine && U(_engine + 0x18) == _scene,
            "Mosquito target cohort escaped its original scene epoch.");
        if ((_worlds & 15) == 0)
        {
            VerifyPlayers();
            VerifyAimAnchor();
            foreach (Enemy enemy in _perceptions.Values) VerifyEnemy(enemy);
        }
    }

    internal void RetireBeforeOwnDelete(uint tid, ref Win32Native.X86Context context)
    {
        Ensure(tid); Require(Active && context.Eip == BattleExit && _frame is null && _nearest is null,
            "Mosquito target retirement is allowed only at exact Battle47 Exit, before synchronous own Delete/scene Exit.");
        VerifyPlayers();
        uint esp = context.Esp, caller = U(esp), ebx = context.Ebx, ebp = context.Ebp, esi = context.Esi, edi = context.Edi;
        Require(caller is >= 0x401000 and < 0x6DA000, "Battle Exit has no original native caller.");
        _host.Breakpoints.AddReturn(caller, tid, "MosquitoPlayerTargets.original-battle-exit-return",
            (uint rt, uint at, ref Win32Native.X86Context returned) =>
            {
                Ensure(rt); Require(_retired && !_sceneUnloaded && !_exitReturned && at == caller &&
                    returned.Esp == checked(esp + 8) && returned.Ebx == ebx && returned.Ebp == ebp &&
                    returned.Esi == esi && returned.Edi == edi,
                    "Original Battle Exit did not return through its paired native RET4 frame.");
                _exitReturned = true;
                Sample("original-scene-exit-returned", new { nativeFunction = Hex(BattleExit), caller = Hex(caller),
                    exactNativeRet4 = true, oldGraphRead = false });
            });
        _retired = true;
        Sample("retired-before-synchronous-scene-exit", new { entry = Hex(context.Eip), pendingScan = false,
            borrowedPerceptionAndActionTargetsReleasedByOriginalSceneExit = true, ownBytesMustNotBeReadAgain = true });
    }

    internal void ReleaseRetiredPagesAtWorld(uint tid, ref Win32Native.X86Context context)
    {
        Ensure(tid); Require(_retired && !_failed && _frame is null && _nearest is null && context.Eip == NativeCallDispatcher.WorldReturn,
            "Mosquito target scratch release requires a later original WorldRET after synchronous scene Exit.");
        if (_sceneUnloaded) return;
        // Read only the surviving engine owner, never the deleted own/enemy graphs.
        Require(_exitReturned && U(0x755274) == _engine && U(0x75DB90) == _sceneManager &&
            U(_sceneManager) == _sceneManagerTable && U(_sceneManager + 0x20) == 0,
            "Original Battle Exit has not completed at a later cleared SceneManager WorldRET.");
        _sceneUnloaded = true;
        if (_scratch is { Released: false }) _scratch.Release(callPending: false);
        Sample("released-after-original-scene-exit", new { originalScene = Hex(_scene), oldGraphRead = false,
            scratch = _scratch?.Summary, pendingScan = false });
    }

    private void GuardScanEntry(uint tid, ref Win32Native.X86Context context)
    {
        if (_retired) { RequireRetiredOwnerAbsent(_perceptions.ContainsKey(context.Ecx), "perception scan"); return; }
        Enemy? enemy = CapturePerception(context.Ecx);
        if (enemy is null) return;
        Require(_frame is null, "Mosquito perception scan nested an outstanding scratch borrow.");
    }

    private void AdmitCandidates(uint tid, ref Win32Native.X86Context context)
    {
        if (_retired) { RequireRetiredOwnerAbsent(_perceptions.ContainsKey(context.Esi), "candidate scan"); return; }
        Enemy? enemy = CapturePerception(context.Esi); if (enemy is null) return;
        VerifyPlayers(); Require(_frame is null && _scratch is { Released: false }, "Mosquito scratch page is absent or still borrowed.");
        uint source = context.Eax;
        if (source == 0) return; // Preserve an original missing query bucket.
        uint begin = U(source + 4), end = U(source + 8);
        Require(begin >= 0x10000 && end >= begin && (end - begin) % 4 == 0 && (end - begin) / 4 < MaximumCandidates,
            "Mosquito original character bucket exceeds its bounded live array.");
        uint[] members = ReadWords(begin, checked((int)((end - begin) / 4)));
        Require(members.Distinct().Count() == members.Length && members.All(x => x >= 0x10000 && U(x) == CharacterTable),
            "Mosquito original bucket is not an ordered unique Character array.");
        if (!members.Contains(_primaryCharacter)) return; // Never manufacture an absent primary query.
        if (members.Contains(_character)) { ++_nativeP2Memberships; return; }
        Require(U(enemy.Perception + 0x44) == 0 && Read(enemy.Perception + 0x65, 1)[0] == 0,
            "Mosquito player admission requires its original Character-type0/non-region perception query.");
        uint[] augmented = [.. members, _character];
        byte[] bytes = new byte[BufferBytes];
        OwnedRemoteBuffer buffer = _scratch ?? throw new InvalidDataException("Mosquito scratch allocation disappeared.");
        uint scratch = buffer.Address, values = checked(scratch + 0x20), finish = checked(values + (uint)augmented.Length * 4);
        BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(0, 4), U(source));
        BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(4, 4), values);
        BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(8, 4), finish);
        BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(12, 4), finish);
        for (int i = 0; i < augmented.Length; ++i) BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(0x20 + i * 4, 4), augmented[i]);
        buffer.Write(0, bytes);
        uint caller = U(context.Esp + 0x1C);
        Require(caller is >= 0x401000 and < 0x6DA000 && ++_scans <= 100000,
            "Mosquito perception scan has no original native C3 caller or exceeds its bound.");
        ScanFrame frame = new(enemy, context.Esp, caller, U(context.Esp), U(context.Esp + 4), U(context.Esp + 8),
            U(context.Esp + 12), source, begin, end, members);
        _frame = frame;
        _host.Breakpoints.AddReturn(caller, tid, "MosquitoPlayerTargets.scan-return",
            (uint rt, uint at, ref Win32Native.X86Context returned) => CompleteScan(rt, at, ref returned, frame));
        context.Eax = scratch;
        if (_scans <= 4) Sample("native-scan-admitted-two-players", new { perception = Hex(enemy.Perception), ai = Hex(enemy.Ai),
            primary = Hex(_primaryCharacter), secondary = Hex(_character), originalCount = members.Length,
            augmentedCount = augmented.Length, onlyEaxRedirected = true, nativeRangeConeAndNearestUnchanged = true });
    }

    private void CompleteScan(uint tid, uint at, ref Win32Native.X86Context context, ScanFrame frame)
    {
        Ensure(tid); Require(Active && ReferenceEquals(_frame, frame) && at == frame.Caller &&
            context.Esp == checked(frame.Esp + 0x20) && context.Edi == frame.SavedEdi && context.Esi == frame.SavedEsi &&
            context.Ebp == frame.SavedEbp && context.Ebx == frame.SavedEbx,
            "Mosquito perception scratch borrow did not return through its exact native C3 frame.");
        uint page = _scratch!.Address;
        // Caller-saved ECX/EDX/EAX may retain dead scan temporaries, as the
        // original loop epilogue does not clear them. Do not rewrite them.
        // The same RW page stays allocated until paired scene Exit + WorldRET.
        Require(new[] { context.Ebx, context.Esi, context.Edi, context.Ebp, context.Esp }
            .All(x => x < page || x >= page + 4096), "A native preserved register or stack still retains the scratch page.");
        Require(U(frame.OriginalList + 4) == frame.SourceBegin && U(frame.OriginalList + 8) == frame.SourceEnd &&
            ReadWords(frame.SourceBegin, frame.Members.Length).SequenceEqual(frame.Members),
            "Original Character query membership changed inside its borrowed scan.");
        VerifyEnemy(frame.Enemy); uint[] targets = ReadPerceptionTargets(frame.Enemy.Perception);
        if (targets.Contains(_character)) ++_nativeP2Memberships;
        ++_scanReturns; _frame = null;
        if (_scanReturns <= 4) Sample("native-scan-return", new { perception = Hex(frame.Enemy.Perception),
            targets = targets.Select(Hex).ToArray(), secondaryPassedNativeFilters = targets.Contains(_character),
            exactNativeC3 = true, originalBucketUnchanged = true, nativePersistentWrapperStores = 0,
            scratchRemainsLiveUntilOriginalSceneExit = true, volatileReturnRegistersUnmodified = true });
    }

    private void GuardActionEntry(uint tid, ref Win32Native.X86Context context)
    {
        if (_retired) { RequireRetiredOwnerAbsent(_actions.ContainsKey(context.Ecx), "attack action"); return; }
        _ = CaptureAction(context.Ecx);
    }

    private void GuardShotEntry(uint tid, ref Win32Native.X86Context context)
    {
        if (_retired) { RequireRetiredOwnerAbsent(_managers.ContainsKey(context.Ecx), "projectile manager"); return; }
        _ = CaptureManager(context.Ecx);
    }

    private void ObserveNearestCall(uint tid, ref Win32Native.X86Context context)
    {
        if (_retired) { RequireRetiredOwnerAbsent(_actions.ContainsKey(context.Esi), "nearest target query"); return; }
        Enemy? enemy = CaptureAction(context.Esi); if (enemy is null) return;
        Require(_nearest is null && context.Ecx == enemy.Perception && context.Esp >= 0x10000 &&
            U(context.Esp + 8) is >= 0x401000 and < 0x6DA000, "Mosquito nearest call lost its actual action/perception/native frame.");
        NearestFrame frame = new(enemy, context.Esi, context.Esp, context.Ebx, context.Ebp, context.Edi);
        _nearest = frame;
        _host.Breakpoints.AddReturn(0x5B4B6E, tid, "MosquitoPlayerTargets.native-nearest-return",
            (uint rt, uint at, ref Win32Native.X86Context returned) =>
            {
                Ensure(rt); Require(Active && ReferenceEquals(_nearest, frame) && at == 0x5B4B6E &&
                    returned.Esp == frame.Esp && returned.Esi == frame.Action && returned.Ebx == frame.Ebx &&
                    returned.Ebp == frame.Ebp && returned.Edi == frame.Edi && returned.Ecx == frame.Enemy.Perception,
                    "Original Mosquito FindNearest did not return through its paired call frame.");
                uint selected = returned.Eax;
                Require(selected == 0 || ReadPerceptionTargets(frame.Enemy.Perception).Contains(selected),
                    "Original Mosquito FindNearest returned a target outside its actual native perception list.");
                if (selected == _character) ++_p2Selections;
                _selectedActionTargets[frame.Action] = selected;
                Sample("native-nearest-selected", new { enemyInstance = Hex(frame.Enemy.Instance), ai = Hex(frame.Enemy.Ai),
                    action = Hex(frame.Action), perception = Hex(frame.Enemy.Perception), selectedCharacter = Hex(selected),
                    selectedSecondary = selected == _character, originalNativeFindNearest = true, nativeReturnPaired = true });
                _nearest = null;
            });
    }

    private void RedirectActionNode(uint tid, ref Win32Native.X86Context context, uint action, string kind)
    {
        if (_retired) { RequireRetiredOwnerAbsent(_actions.ContainsKey(action), kind); return; }
        Enemy? enemy = CaptureAction(action); if (enemy is null) return;
        RedirectSelectedNode(ref context, enemy, action, kind);
    }

    private void RedirectShotNode(uint tid, ref Win32Native.X86Context context)
    {
        if (_retired) { RequireRetiredOwnerAbsent(_managers.ContainsKey(context.Esi), "projectile target"); return; }
        Enemy? enemy = CaptureManager(context.Esi); if (enemy is null) return;
        uint action = U(enemy.Ai + 0x124);
        if (action < 0x10000 || U(action) != AttackTable) return;
        Require(CaptureAction(action) == enemy, "Mosquito shot and current attack action have different native owners.");
        RedirectSelectedNode(ref context, enemy, action, "shot");
    }

    private void TraceShotDirection(uint tid, ref Win32Native.X86Context context)
    {
        if (_retired) { RequireRetiredOwnerAbsent(_managers.ContainsKey(context.Esi), "native shot direction"); return; }
        if (_tracedShotDirections >= 16) return;
        Enemy? enemy = CaptureManager(context.Esi); if (enemy is null) return;
        uint action = U(enemy.Ai + 0x124), selected = action >= 0x10000 && U(action) == AttackTable ? U(action + 0x3A8) : 0;
        uint originalNode = P(P(P(0x765AF0) + 0x28) + 0x18), emitter = P(enemy.Manager + 0x1DC);
        NativeTargetNodeOwnership.VerifyAtStop(_host, emitter, enemy.Root, _scene);
        Sample("native-shot-direction", new { trace = ++_tracedShotDirections, enemyInstance = Hex(enemy.Instance),
            sourceManager = Hex(enemy.Manager), action = Hex(action), selectedCharacter = Hex(selected),
            nativeCall = Hex(0x50FE70), nativeArgumentsAtEsp = DiagnosticVector(context.Esp),
            emitter = DiagnosticNode(emitter), primaryRoot = DiagnosticNode(P(_primaryInstance + 0x24)),
            primaryTargetNode = DiagnosticNode(originalNode), secondaryRoot = DiagnosticNode(_root),
            diagnosticSecondaryTargetAnchor = DiagnosticNode(_aimAnchor),
            diagnosticSecondaryTargetAnchorError = _aimAnchorError,
            storedManagerTargetNode1E0 = Hex(U(enemy.Manager + 0x1E0)),
            actualAimTargetNode = Hex(_aimAnchor), trajectoryNotChangedByTrace = true, targetAnchorUsedForAim = true });
    }

    private void ResolveOwnedAimAnchor()
    {
        VisualGraphReadResult graph = new VisualGraphReader(_process).Read(_root, inspectSkins: false);
        if (!graph.Success) { _aimAnchorError = graph.Error; return; }
        uint[] matches = graph.Graph.Nodes.Where(node => node.Kind == NativeVisualNodeKind.Node && node.NamePointer != 0 &&
            Win32Native.ReadAnsiString(_process, checked(node.NamePointer + 9), 128) == "target_node")
            .Select(node => node.Address).ToArray();
        if (matches.Length != 1) { _aimAnchorError = "Owned graph target_node count=" + matches.Length; return; }
        NativeTargetNodeOwnership.VerifyAtStop(_host, matches[0], _root, _scene);
        _aimAnchor = matches[0]; _aimAnchorParent = P(_aimAnchor + 0x2C);
    }

    private string ReadNodeName(uint node)
    { uint name = P(node + 0x10); return Win32Native.ReadAnsiString(_process, checked(name + 9), 128); }

    private void VerifyAimAnchor()
    {
        byte[] bytes = Read(_aimAnchor, 0x80);
        Require(W(bytes, 0) == 0x6DC4F4 && W(bytes, 0x2C) == _aimAnchorParent && W(bytes, 0x3C) == _scene &&
            DiagnosticVectorValues(bytes, 0x20).All(value => value.HasValue) &&
            DiagnosticVectorValues(bytes, 0x74).All(value => value.HasValue),
            "Owned target_node lost its original type/parent/scene or finite local/world position.");
    }

    private object DiagnosticNode(uint node)
    {
        byte[] bytes = Read(node, 0x80);
        return new { address = Hex(node), table = Hex(W(bytes, 0)),
            localPosition = DiagnosticVectorValues(bytes, 0x20), worldPosition = DiagnosticVectorValues(bytes, 0x74) };
    }
    private object DiagnosticVector(uint address)
    {
        byte[] bytes = Read(address, 12);
        return new { values = DiagnosticVectorValues(bytes, 0), bits = Enumerable.Range(0, 3).Select(i => Hex(W(bytes, i * 4))).ToArray() };
    }
    private static float?[] DiagnosticVectorValues(byte[] bytes, int offset) => Enumerable.Range(0, 3)
        .Select(i => BitConverter.ToSingle(bytes, offset + i * 4)).Select(value => float.IsFinite(value) ? (float?)value : null).ToArray();

    private void RedirectSelectedNode(ref Win32Native.X86Context context, Enemy enemy, uint action, string kind)
    {
        VerifyPlayers(); VerifyCachedEnemy(enemy);
        uint selected = U(action + 0x3A8);
        if (selected == 0 || selected == _primaryCharacter) return;
        // Original3A8 intentionally retains the entry selection until v11.
        // A later perception refresh may remove a target that left its cone;
        // that does not revoke the native action's still-live borrowed target.
        Require(selected == _character && _selectedActionTargets.TryGetValue(action, out uint pairedSelection) && pairedSelection == selected,
            "Mosquito secondary action target lacks its actual paired native entry selection.");
        uint primaryRegion = P(P(0x765AF0) + 0x28), originalNode = P(primaryRegion + 0x18);
        Require(context.Eax == originalNode, "Mosquito primary-only Node consumer no longer holds the original selected Node.");
        NativeTargetNodeOwnership.VerifyAtStop(_host, originalNode, P(_primaryInstance + 0x24), _scene);
        VerifyAimAnchor();
        context.Eax = _aimAnchor; ++_nodeOverrides; ++_p2Selections;
        long now = Environment.TickCount64;
        if (kind == "shot" || now - _lastDetailedTick >= 1000)
        {
            _lastDetailedTick = now;
            Sample(kind == "shot" ? "shot-target-consumed" : "native-secondary-target-node", new { kind,
                enemyInstance = Hex(enemy.Instance), ai = Hex(enemy.Ai), action = Hex(action), perception = Hex(enemy.Perception),
                actualTargetCharacter = Hex(selected), targetNode = Hex(_aimAnchor), targetNodeAnchor = Hex(_aimAnchor),
                targetRoot = Hex(_root), originalNode = Hex(originalNode),
                targetCharacter = Hex(selected), sourceManager = Hex(enemy.Manager), selectedSecondary = true,
                nativeSelectionRetained = true, onlyEaxChanged = true });
        }
    }

    private Enemy? CapturePerception(uint perception)
    {
        if (_perceptions.TryGetValue(perception, out Enemy? cached)) { VerifyCachedEnemy(cached); return cached; }
        if (perception < 0x10000 || U(perception) != 0x6F6288) return null;
        uint ai = U(perception + 0x10); Enemy? enemy = CaptureAi(ai, perception);
        if (enemy is null) return null;
        if (_perceptions.TryGetValue(perception, out Enemy? known)) { Require(known == enemy, "Mosquito perception identity was replaced."); return known; }
        Require(_perceptions.Count < 64, "Mosquito owner budget exceeded.");
        _perceptions.Add(perception, enemy); _managers.TryAdd(enemy.Manager, enemy); return enemy;
    }

    private void RetireResetPerception(uint tid, ref Win32Native.X86Context context)
    {
        // Original Reset clears its native list and then detaches owner10.
        // Retire cached addresses before that operation or allocation reuse;
        // this path reads no enemy graph and also runs during final scene Exit.
        if (!_perceptions.TryGetValue(context.Ecx, out Enemy? enemy)) return;
        Require(_frame?.Enemy != enemy && _nearest?.Enemy != enemy,
            "Native Mosquito Perception Reset interrupted a paired target operation.");
        _perceptions.Remove(enemy.Perception);
        foreach (uint action in _actions.Where(item => item.Value == enemy).Select(item => item.Key).ToArray())
        { _actions.Remove(action); _selectedActionTargets.Remove(action); }
        foreach (uint manager in _managers.Where(item => item.Value == enemy).Select(item => item.Key).ToArray()) _managers.Remove(manager);
        ++_nativeResets;
        Sample("native-perception-reset-retired", new { perception = Hex(enemy.Perception), ai = Hex(enemy.Ai),
            enemyInstance = Hex(enemy.Instance), oldGraphRead = false, originalResetUnchanged = true });
    }

    private Enemy? CaptureAction(uint action)
    {
        if (_actions.TryGetValue(action, out Enemy? cached))
        {
            byte[] head = Read(action, 0x24);
            Require(W(head, 0) == AttackTable && W(head, 0x20) == cached.Ai, "Mosquito cached action changed its owner.");
            VerifyCachedEnemy(cached); return cached;
        }
        if (action < 0x10000 || U(action) != AttackTable) return null;
        uint ai = U(action + 0x20); Enemy? enemy = CaptureAi(ai, 0); if (enemy is null) return null;
        _ = CapturePerception(enemy.Perception);
        if (_actions.TryGetValue(action, out Enemy? old)) Require(old == enemy, "Mosquito action allocation changed owner.");
        else { Require(_actions.Count < 64, "Mosquito action budget exceeded."); _actions.Add(action, enemy); }
        return enemy;
    }

    private Enemy? CaptureManager(uint manager)
    {
        if (_managers.TryGetValue(manager, out Enemy? cached))
        {
            byte[] head = Read(manager, 0x28);
            Require(W(head, 0) == 0x6F7D70 && W(head, 0x24) == cached.Instance, "Mosquito cached manager changed its owner.");
            VerifyCachedEnemy(cached); return cached;
        }
        if (manager < 0x10000 || U(manager) != 0x6F7D70) return null;
        uint instance = U(manager + 0x24); if (instance < 0x10000 || U(instance) != 0x710F80) return null;
        uint character = ReadCharacter(instance), perception = U(character + 0x154);
        if (perception < 0x10000 || U(perception) != 0x6F6288) return null;
        uint ai = U(perception + 0x10);
        Enemy? enemy = CaptureAi(ai, 0); if (enemy is null) return null;
        Require(enemy.Manager == manager, "Mosquito projectile manager is not its Character's native component.");
        _ = CapturePerception(enemy.Perception); return enemy;
    }

    private Enemy? CaptureAi(uint ai, uint expectedPerception)
    {
        if (ai < 0x10000 || U(ai) is not (0x6FF940 or 0x6FF9C0)) return null;
        uint character = P(ai + 0x144), instance = P(character + 0x24), root = P(instance + 0x24);
        if (U(root + 0x3C) != _scene) return null;
        uint perception = U(character + 0x154), move = U(character + 0x12C), manager = U(character + 0x140);
        if (perception < 0x10000 || move < 0x10000 || manager < 0x10000 ||
            U(perception) != 0x6F6288 || U(perception + 0x10) != ai) return null;
        Enemy enemy = new(perception, ai, character, instance, root, move, manager);
        Require(expectedPerception == 0 || perception == expectedPerception, "Mosquito perception's reciprocal owner differs.");
        VerifyEnemy(enemy); return enemy;
    }

    private void VerifyCachedEnemy(Enemy enemy)
    {
        // Four bounded reads retain reciprocal owner/type/scene checks on hot
        // callbacks. Complete component proof runs on admission and each16Worlds.
        byte[] ai = Read(enemy.Ai, 0x148), character = Read(enemy.Character, 0x158);
        byte[] root = Read(enemy.Root, 0x40), perception = Read(enemy.Perception, 0x14);
        bool valid = W(ai, 0) is 0x6FF940 or 0x6FF9C0 && W(ai, 0x144) == enemy.Character &&
            W(ai, 0x24) == enemy.Instance && W(character, 0) == CharacterTable &&
            W(character, 0x154) == enemy.Perception &&
            W(character, 0x24) == enemy.Instance && W(character, 0x12C) == enemy.Move &&
            W(character, 0x140) == enemy.Manager && W(root, 0) == 0x6DC4F4 && W(root, 0x3C) == _scene &&
            W(perception, 0) == 0x6F6288 && W(perception, 0x10) == enemy.Ai;
        if (!valid && !_proofFailureLogged)
        {
            _proofFailureLogged = true;
            _log(new { @event = "mosquito-player-targets", name = "native-owner-proof-rejected", data = new
            {
                expected = enemy,
                expectedScene = Hex(_scene),
                ai = new { table = Hex(W(ai, 0)), instance24 = Hex(W(ai, 0x24)), character144 = Hex(W(ai, 0x144)) },
                character = new { table = Hex(W(character, 0)), instance24 = Hex(W(character, 0x24)),
                    move12C = Hex(W(character, 0x12C)), ai134 = Hex(W(character, 0x134)),
                    manager140 = Hex(W(character, 0x140)), perception154 = Hex(W(character, 0x154)) },
                root = new { table = Hex(W(root, 0)), scene3C = Hex(W(root, 0x3C)) },
                perception = new { table = Hex(W(perception, 0)), owner10 = Hex(W(perception, 0x10)) },
                originalPredicateUnchanged = true, nativeFieldsWritten = 0
            } });
        }
        Require(valid, "Mosquito cached reciprocal owner/type/scene proof changed.");
    }

    private void VerifyEnemy(Enemy enemy)
    {
        VerifyCachedEnemy(enemy);
        byte[] instance = Read(enemy.Instance, 0x28), manager = Read(enemy.Manager, 0x28), move = Read(enemy.Move, 0x28);
        bool valid = W(instance, 0) == 0x710F80 && W(instance, 0x24) == enemy.Root &&
            W(manager, 0) == 0x6F7D70 && W(manager, 0x24) == enemy.Instance &&
            W(move, 0) == 0x6F64F8 && W(move, 0x24) == enemy.Instance &&
            enemy.Instance != _instance && enemy.Instance != _primaryInstance;
        if (!valid && !_proofFailureLogged)
        {
            _proofFailureLogged = true;
            _log(new { @event = "mosquito-player-targets", name = "native-component-proof-rejected", data = new
            {
                expected = enemy,
                instance = new { table = Hex(W(instance, 0)), root24 = Hex(W(instance, 0x24)) },
                manager = new { table = Hex(W(manager, 0)), instance24 = Hex(W(manager, 0x24)) },
                move = new { table = Hex(W(move, 0)), instance24 = Hex(W(move, 0x24)) },
                expectedPrimaryInstance = Hex(_primaryInstance), expectedSecondaryInstance = Hex(_instance),
                originalPredicateUnchanged = true, nativeFieldsWritten = 0
            } });
        }
        Require(valid, "Mosquito target helper lost its exact native component ownership.");
    }

    private void VerifyPlayers()
    {
        byte[] instance = Read(_instance, 0x28), root = Read(_root, 0x40);
        byte[] own = Read(_character, 0x154), primary = Read(_primaryCharacter, 0x154);
        Require(U(0x765AD4) == _profile && U(_profile) == 0x6F5A44 && U(_profile + 0x2B4) == _primaryInstance &&
            W(instance, 0) == 0x710F80 && W(instance, 0x24) == _root && W(root, 0) == 0x6DC4F4 && W(root, 0x3C) == _scene &&
            W(own, 0) == CharacterTable && W(own, 0x24) == _instance && W(own, 0x18) == _root &&
            W(own, 0x148) == 1 && W(own, 0x14C) == 41 && W(primary, 0) == CharacterTable &&
            W(primary, 0x24) == _primaryInstance && W(primary, 0x148) == 0 &&
            _instance != _primaryInstance && _character != _primaryCharacter &&
            U(0x755274) == _engine && U(_engine + 0x18) == _scene,
            "Mosquito target helper's original primary/secondary/scene epoch changed.");
    }

    private uint ReadCharacter(uint instance)
    {
        Require(U(instance) == 0x710F80, "Mosquito target owner is not an original spInstance.");
        uint count = U(instance + 0x20), head = P(instance + 0x1C), link = U(head), previous = head, result = 0;
        Require(count is > 0 and <= 64, "Mosquito target component count exceeds64."); HashSet<uint> seen = [];
        for (uint i = 0; i < count; ++i)
        {
            Require(link != head && seen.Add(link) && U(link + 4) == previous, "Mosquito target component list is broken.");
            uint entity = P(link + 8);
            if (U(entity) == CharacterTable) { Require(result == 0, "Mosquito target instance has duplicate Characters."); result = entity; }
            previous = link; link = U(link);
        }
        Require(link == head && U(head + 4) == previous && result != 0, "Mosquito target Character/sentinel is missing."); return result;
    }

    private uint[] ReadPerceptionTargets(uint perception)
    {
        uint head = P(perception + 0x18), count = U(perception + 0x1C), link = U(head), previous = head;
        Require(count <= MaximumCandidates, "Mosquito perception list exceeds64."); HashSet<uint> links = []; List<uint> values = [];
        for (uint i = 0; i < count; ++i)
        {
            Require(link != head && links.Add(link) && U(link + 4) == previous, "Mosquito perception target list is broken.");
            uint value = P(link + 8); Require(!values.Contains(value), "Mosquito perception has duplicate target identity.");
            values.Add(value); previous = link; link = U(link);
        }
        Require(link == head && U(head + 4) == previous, "Mosquito perception sentinel/count changed."); return values.ToArray();
    }

    private void RequireRetiredOwnerAbsent(bool matches, string operation) => Require(!matches || _sceneUnloaded,
        "Retired Mosquito " + operation + " ran before original synchronous scene Exit; second-body pointers must not be followed.");
    private delegate void Stop(uint tid, ref Win32Native.X86Context context);
    private void Hook(uint address, string name, Stop operation)
    {
        Require(!_host.Breakpoints.Contains(address), "Mosquito target hook already has another owner.");
        byte[] original = Image(address, 1); BreakpointCallback repeat = null!;
        repeat = (uint tid, uint at, ref Win32Native.X86Context context) =>
        {
            Ensure(tid); if (_sceneUnloaded || _failed) return;
            Require(at == address && context.Eip == address && Read(at, 1).SequenceEqual(original), "Mosquito target stop identity changed.");
            try { operation(tid, ref context); }
            catch (Exception error) { _failed = true; _failure = error.Message; throw; }
            if (!_sceneUnloaded && !_failed) _host.Breakpoints.AddReturn(address, _thread, "MosquitoPlayerTargets." + name, repeat);
        };
        _host.Breakpoints.AddReturn(address, _thread, "MosquitoPlayerTargets." + name, repeat);
    }
    private void Pin(uint address, string hex)
    {
        byte[] expected = Convert.FromHexString(hex);
        Require(Image(address, expected.Length).SequenceEqual(expected) && ReadCode(address, expected.Length).SequenceEqual(expected),
            "Mosquito target original pin differs at " + Hex(address));
    }
    private byte[] ReadCode(uint address, int count)
    {
        byte[] actual = Read(address, count), original = Image(address, count);
        for (int i = 0; i < count; ++i)
            if (actual[i] == 0xCC && original[i] != 0xCC && _host.Breakpoints.Contains(address + (uint)i)) actual[i] = original[i];
        return actual;
    }
    private uint[] ReadWords(uint address, int count)
    { byte[] bytes = count == 0 ? [] : Read(address, checked(count * 4)); return Enumerable.Range(0, count).Select(i => BitConverter.ToUInt32(bytes, i * 4)).ToArray(); }
    private void Sample(string name, object data)
    { if (_samples.Count >= 64) return; var sample = new { name, data }; _samples.Add(sample); _log(new { @event = "mosquito-player-targets", name, data }); }
    private uint P(uint address) { uint value = U(address); Require(value >= 0x10000 && (value & 3) == 0, "Mosquito target pointer is invalid."); return value; }
    private static uint W(byte[] bytes, int offset) => BitConverter.ToUInt32(bytes, offset);
    private uint U(uint address) => BitConverter.ToUInt32(Read(address, 4));
    private byte[] Image(uint address, int count) => _image.GetSectionData(checked((int)(address - 0x400000))).GetContent(0, count).ToArray();
    private byte[] Read(uint address, int count)
    { Ensure(_thread); Require(address >= 0x10000 && count is > 0 and <= 512, "Mosquito target read exceeds its bound."); return Win32Native.ReadMemory(_process, address, count); }
    private void Ensure(uint tid) => Require(_host.IsAtDebugStop && tid == _thread && _host.MainThreadId == _thread &&
        _host.ProcessHandle == _process && _host.ProcessPid == _pid && Environment.CurrentManagedThreadId == _owner,
        "Mosquito targets require the same owned main-thread debug stop.");
    private static string Hex(uint value) => $"0x{value:X8}";
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidDataException(message); }
}
