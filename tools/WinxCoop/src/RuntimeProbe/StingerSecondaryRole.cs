using System.Reflection.PortableExecutable;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// Own bounded inbound-Stinger/filter0 role substitution. A fresh, paired native
/// activation identifies each shot; native mask8, source exclusion, send and
/// retirement remain in the original collision routine. No damage ledger.
/// </summary>
internal sealed class StingerSecondaryRole
{
    private const uint StingerTable = 0x6F6C7C, SourceManagerTable = 0x6F7D70;
    private const uint RoleBranch = 0x501E6C, RoleContinuation = 0x501E6E;
    private readonly DebuggerHost _host;
    private readonly PEReader _image;
    private readonly Action<object> _log;
    private readonly int _ownerThread;
    private readonly Dictionary<uint, Shot> _shots = [];
    private readonly Dictionary<uint, Pair> _collisions = [];
    private readonly List<Pair> _pending = [];
    private readonly Dictionary<string, uint> _counts = [];
    private readonly List<object> _samples = [];
    private uint _instance, _root, _move, _character, _scene, _metadata, _engine, _timer;
    private uint _profile, _primaryInstance;
    private uint _epoch, _generation, _worlds, _retireEntry;
    private bool _bound, _retired, _failed;

    private sealed class Shot(uint projectile, uint generation, uint source, uint sourceInstance,
        uint sourceRoot, uint node)
    {
        internal readonly uint Projectile = projectile, Generation = generation, Source = source;
        internal readonly uint SourceInstance = sourceInstance, SourceRoot = sourceRoot, Node = node;
        internal bool Active, ActivationPending = true, DeactivationPending;
        internal uint Deadline, ActivatedAtRaw, Deactivations, RoleOverrides, OwnSendCallsites;
    }
    private sealed class Pair(string kind, Shot shot, uint esp, uint caller, uint delta, uint esi, uint edi)
    {
        internal readonly string Kind = kind;
        internal readonly Shot Shot = shot;
        internal readonly uint Esp = esp, Caller = caller, Delta = delta, Esi = esi, Edi = edi;
        internal bool RoleOverridden;
        internal uint DeactivationsAtEntry, OwnSends;
    }
    private sealed record Source(uint Manager, uint Instance, uint Root, uint Node);

    internal StingerSecondaryRole(DebuggerHost host, PEReader image, Action<object> log)
    {
        _host = host ?? throw new ArgumentNullException(nameof(host));
        _image = image ?? throw new ArgumentNullException(nameof(image));
        _log = log ?? throw new ArgumentNullException(nameof(log));
        _ownerThread = Environment.CurrentManagedThreadId;
    }

    internal bool Active => _bound && !_retired && !_failed;
    internal bool Retired => _retired;
    internal uint OwnRoleOverrides => _counts.GetValueOrDefault("role.own-filter0-overridden");
    // Reached native2717 CALL sites; HP commits remain the health adapter's result.
    internal uint OwnNativeDamageSendCount => _counts.GetValueOrDefault("native.own2717-callsite-reached");
    internal uint CompletedActivationGenerations => _counts.GetValueOrDefault("activation.native-return-committed");
    internal object Summary => new
    {
        bound = _bound, active = Active, retiredBeforeOwnDelete = _retired, terminalFailure = _failed,
        epoch = _epoch, generationsReserved = _generation, verifiedWorlds = _worlds,
        ownInstance = Hex(_instance), ownRoot = Hex(_root), ownMove = Hex(_move), scene = Hex(_scene),
        ownMetadata = Hex(_metadata), retireEntry = Hex(_retireEntry), pendingNativePairs = _pending.Count,
        knownProjectileBindings = _shots.Count, activeKnownGenerations = _shots.Values.Count(shot => shot.Active),
        maximumKnownProjectiles = 64, maximumNativePairs = 8,
        counts = new Dictionary<string, uint>(_counts), samples = _samples.ToArray(),
        shots = _shots.Values.Select(shot => new { projectile = Hex(shot.Projectile), generation = shot.Generation,
            source = Hex(shot.Source), sourceInstance = Hex(shot.SourceInstance), shot.Active,
            shot.Deadline, shot.ActivatedAtRaw, shot.Deactivations, shot.RoleOverrides, shot.OwnSendCallsites }).ToArray(),
        admittedNativeSource = "MosquitoManager6F7D70 pool3 / Stinger206A45A2 table6F6C7C",
        onlyContextChange = "EIP501E6C->501E6E for exact own metadata, native filter0, missing role10",
        metadataOrGameFieldWrites = 0, nativeCallsAdded = 0, privatePagesAllocated = 0,
        nativeMask8Unchanged = true, nativeSourceExclusionUnchanged = true, nativeSendAndRetirementUnchanged = true,
        pointerBasedDamageDedupLedger = false, nativeSehChainModifiedByAdapter = false,
        floatingPointInstructionsEmitted = false, runtimeOwnHitQualificationClaimed = false, fullE3Complete = false
    };

    // Convenience overloads independently require the actual native C3 debug
    // stop. Callers already holding the logical context should use that overload.
    internal void BindAtWorld(uint ownInstance, uint ownRoot, uint ownMove, uint scene)
    {
        Win32Native.X86Context context = WorldContext();
        BindAtWorld(_host.MainThreadId, ref context, ownInstance, ownRoot, ownMove, scene);
    }
    internal void BindAtWorld(uint tid, ref Win32Native.X86Context context,
        uint ownInstance, uint ownRoot, uint ownMove, uint scene)
    {
        Ensure(tid); Require(!_bound && !_retired && !_failed && context.Eip == NativeCallDispatcher.WorldReturn,
            "Secondary Stinger role binding requires its first exact owned WorldRET.");
        Require(_host.ImageBase == 0x400000, "Secondary Stinger role requires the original x86 image.");
        _instance = ownInstance; _root = ownRoot; _move = ownMove; _scene = scene;
        _character = Pointer(ownMove + 0x160); _metadata = Pointer(ownMove + 0x174);
        _engine = Pointer(0x755274); _timer = Pointer(0x755298); _epoch = _host.DebugStopSequence;
        _profile = Pointer(0x765AD4); _primaryInstance = Pointer(_profile + 0x2B4);
        VerifyOwn();
        Pin(0x501B50, "B8E8EA7400C3");
        Require(U(0x74EAE8) == 0x206A45A2 && ImageWord(StingerTable + 0x10) == 0x501B50 &&
            U(StingerTable + 0x10) == 0x501B50 && U(StingerTable + 0x20) == 0x502210 &&
            U(StingerTable + 0x24) == 0x501FF0 && U(StingerTable + 0x28) == 0x501AD0 &&
            U(StingerTable + 0x2C) == 0x501C40,
            "Secondary Stinger role requires the exact original Stinger class and native slots.");
        Pin(0x501FFE, "C6463001"); Pin(0x5021AF, "8B401C0588130000894628");
        Pin(0x5021CD, "8B481C894E2C"); Pin(0x5021E7, "B0015E83C43CC21800");
        Pin(0x501AD8, "C6463000C786F400000000000000");
        Pin(0x501B14, "C74628000000005EC3"); Pin(0x50FF07, "FF5224");
        Pin(0x50FE2A, "515257538BCEFF502C");
        Pin(0x4FF2F9, "8B44241C8B7C2420894618");
        Pin(0x50FCC0, "8B0E85C974068B016A01FF10C70600000000");
        Pin(0x501D20, "6AFF6868696C0064A100000000506489250000000083EC44568BF1");
        Pin(0x501D48, "57"); Pin(0x501D6D, "C744245400000000");
        Pin(0x501D81, "8B7818");
        Pin(0x501DAD, "E8EE5EF5FF"); Pin(0x457CA0, "FF25402F3B01");
        Pin(0x502250, "E8CBFAFFFF");
        Pin(0x501E09, "F6C4057A27");
        Pin(0x501E16, "8B410885C08986F40000007436");
        Pin(0x501E23, "8B56183B4224752E");
        Pin(0x501E59, "8B465085C0750E33D28A510483E2108954240C");
        Pin(RoleBranch, "7413"); Pin(RoleContinuation, "83F801751A");
        Pin(0x501E8D, "0FB6490483E108894C240C746A");
        Pin(0x501EF0, "6817270000508BCEE8333EF3FF");
        Pin(0x501EFD, "8B168BCEFF52288B068BCEFF5028");
        Pin(0x501F1C, "8B4C244C5FB00164890D000000005E83C450C3");
        _bound = true;
        Hook(0x501FF0, "83EC3C568BF1", "activation", Activate);
        Hook(0x501AD0, "568BF18B4E14", "deactivation", Deactivate);
        Hook(0x501D20, "6AFF6868696C00", "collision-entry", Collision);
        Hook(RoleBranch, "7413", "role-decision", RoleDecision);
        Hook(0x501EF8, "E8333EF3FF", "native-own-send-callsite", OwnSend);
        Hook(0x501B60, "6AFF6854696C00", "destructor-entry", Destroy);
        Sample("bound", new { epoch = _epoch, ownInstance = Hex(_instance), ownRoot = Hex(_root),
            ownMove = Hex(_move), ownMetadata = Hex(_metadata), flagsWord = Word(_metadata + 4),
            scene = Hex(_scene), existingActiveProjectilesPromotedToKnownGenerations = false });
    }

    internal void VerifyAtWorld()
    { Win32Native.X86Context context = WorldContext(); VerifyAtWorld(_host.MainThreadId, ref context); }
    internal void VerifyAtWorld(uint tid, ref Win32Native.X86Context context)
    {
        Ensure(tid); Require(context.Eip == NativeCallDispatcher.WorldReturn, "Stinger role verification requires WorldRET.");
        if (_retired) return;
        Require(Active && _pending.Count == 0 && _collisions.Count == 0,
            "Stinger role reached WorldRET with an inactive or unpaired native operation.");
        VerifyOwn(); Require(++_worlds <= 36000, "Stinger role native-world budget exceeded.");
        foreach (Shot shot in _shots.Values)
        {
            VerifyShot(shot);
            Require(!shot.ActivationPending && !shot.DeactivationPending && Byte(shot.Projectile + 0x30) == (shot.Active ? 1 : 0),
                "Stinger native active state changed without its paired lifetime witness.");
        }
    }
    internal void RetireBeforeOwnDelete()
    { Win32Native.X86Context context = WorldContext(); RetireBeforeOwnDelete(_host.MainThreadId, ref context); }
    internal void RetireBeforeOwnDelete(uint tid, ref Win32Native.X86Context context)
    {
        Ensure(tid); Require(Active && (context.Eip is NativeCallDispatcher.WorldReturn or 0x5F2300 or 0x5F86C0 or
            0x4E8AA0 or 0x4E8C80 or 0x59AE80 or 0x59B190) &&
            _pending.Count == 0 && _collisions.Count == 0, "Stinger role must retire at a verified pre-delete WorldRET or unload entry.");
        VerifyOwn(); _retireEntry = context.Eip; _retired = true;
        Sample("retired-before-own-delete", new { epoch = _epoch, entry = Hex(context.Eip),
            pendingNativePairs = 0, callbacksDormantBeforeDelete = true,
            nativeForeignProjectilesNotDeletedByAdapter = true, historicalObjectReadsNowForbidden = true });
    }

    private void Activate(uint tid, ref Win32Native.X86Context c)
    {
        uint projectile = c.Ecx;
        if (U(projectile) != StingerTable) return;
        Source? source = CaptureSource(projectile);
        if (source is null) { Add("activation.unqualified-source-native-pass-through"); return; }
        VerifyOwn(); RequireNoPairFor(projectile);
        Require(U(c.Esp) == 0x50FF0A && Byte(projectile + 0x30) == 0 && U(projectile + 0xF4) == 0 &&
            (!_shots.TryGetValue(projectile, out Shot? prior) || !prior.Active),
            "Stinger activation is not a fresh inactive native Mosquito pool call.");
        Require(_shots.ContainsKey(projectile) || _shots.Count < 64, "Stinger role known-projectile budget exceeded.");
        Require(++_generation <= 4096, "Stinger activation generation budget exceeded.");
        Shot shot = new(projectile, _generation, source.Manager, source.Instance, source.Root, source.Node);
        _shots[projectile] = shot;
        Pair pair = BeginPair(tid, "activation", shot, c, 0x1C,
            (Pair p, ref Win32Native.X86Context returned) =>
            {
                VerifyShot(shot);
                Require((returned.Eax & 0xFF) == 1 && Byte(projectile + 0x30) == 1 && U(projectile + 0xF4) == 0 &&
                    U(projectile + 0x28) != 0, "Stinger native RET18 did not complete a successful active generation.");
                shot.Deadline = U(projectile + 0x28); shot.ActivatedAtRaw = U(projectile + 0x2C);
                shot.ActivationPending = false; shot.Active = true; Add("activation.native-return-committed");
                Sample("activation-return", new { epoch = _epoch, generation = shot.Generation,
                    projectile = Hex(projectile), sourceManager = Hex(shot.Source), sourceInstance = Hex(shot.SourceInstance),
                    nativeCaller = Hex(p.Caller), entryEsp = Hex(p.Esp), returnedEsp = Hex(returned.Esp),
                    expectedEspDelta = 0x1C, nativeRetArgumentBytes = 0x18, nativeActive30 = 1,
                    deadline28 = shot.Deadline, activationTime2CRaw = shot.ActivatedAtRaw, nativeAl = returned.Eax & 0xFF });
            });
        Sample("activation-entry", new { epoch = _epoch, generationReserved = shot.Generation,
            projectile = Hex(projectile), caller = Hex(pair.Caller), sourceInstance = Hex(shot.SourceInstance) });
    }

    private void Deactivate(uint tid, ref Win32Native.X86Context c)
    {
        if (!_shots.TryGetValue(c.Ecx, out Shot? shot)) return;
        VerifyShot(shot);
        Require(!shot.ActivationPending && !shot.DeactivationPending, "Stinger deactivation nested an uncompleted lifetime operation.");
        shot.DeactivationPending = true;
        BeginPair(tid, "deactivation", shot, c, 4,
            (Pair p, ref Win32Native.X86Context returned) =>
            {
                VerifyShot(shot);
                Require(Byte(shot.Projectile + 0x30) == 0 && U(shot.Projectile + 0xF4) == 0 && U(shot.Projectile + 0x28) == 0,
                    "Stinger native C3 deactivation did not clear active/target/deadline.");
                shot.DeactivationPending = false; shot.Active = false; ++shot.Deactivations;
                Add("deactivation.native-return-committed");
                Sample("deactivation-return", new { epoch = _epoch, generation = shot.Generation,
                    projectile = Hex(shot.Projectile), caller = Hex(p.Caller), entryEsp = Hex(p.Esp),
                    returnedEsp = Hex(returned.Esp), expectedEspDelta = 4, shot.Deactivations,
                    nativeActive30 = 0, nativeTargetF4 = 0, nativeDeadline28 = 0,
                    nativeDoubleDeactivationDoesNotCreateGeneration = true });
            });
    }

    private void Collision(uint tid, ref Win32Native.X86Context c)
    {
        if (!_shots.TryGetValue(c.Ecx, out Shot? shot) || !shot.Active) return;
        VerifyOwn(); VerifyShot(shot);
        Require(!shot.ActivationPending && !shot.DeactivationPending && !_collisions.ContainsKey(shot.Projectile) &&
            U(c.Esp) == 0x502255 && c.Esi == shot.Projectile && Byte(shot.Projectile + 0x30) == 1 &&
            U(shot.Projectile + 0xF4) == 0, "Stinger collision is not the exact fresh native tick frame.");
        Pair frame = BeginPair(tid, "collision", shot, c, 4,
            (Pair p, ref Win32Native.X86Context returned) =>
            {
                VerifyShot(shot);
                Require(_collisions.TryGetValue(shot.Projectile, out Pair? active) && ReferenceEquals(active, p),
                    "Stinger collision returned through another native frame.");
                if (p.RoleOverridden)
                {
                    uint retirements = shot.Deactivations - p.DeactivationsAtEntry;
                    Require(!shot.Active && retirements is >= 1 and <= 2 && Byte(shot.Projectile + 0x30) == 0 &&
                        U(shot.Projectile + 0xF4) == 0 && U(shot.Projectile + 0x28) == 0,
                        "Overridden Stinger collision lacks its native single-contact retirement.");
                    Sample("own-collision-return", new { epoch = _epoch, generation = shot.Generation,
                        projectile = Hex(shot.Projectile), nativeCaller = Hex(p.Caller), entryEsp = Hex(p.Esp),
                        returnedEsp = Hex(returned.Esp), nativeRetirements = retirements, ownSendCallsites = p.OwnSends,
                        nativeOncePolicyUnmodified = true, resourceCommitOrOwnReactionClaimed = false });
                }
                _collisions.Remove(shot.Projectile);
            });
        frame.DeactivationsAtEntry = shot.Deactivations; _collisions.Add(shot.Projectile, frame);
    }

    private void RoleDecision(uint tid, ref Win32Native.X86Context c)
    {
        uint metadata = c.Ecx;
        if (metadata != _metadata)
        {
            Require(U(metadata + 8) != _instance, "Own Stinger candidate used an unqualified alternate metadata object.");
            Add("role.unrelated-candidate-native-pass-through"); return;
        }
        VerifyOwn();
        bool knownShot = _shots.TryGetValue(c.Esi, out Shot? shot);
        bool knownFrame = _collisions.TryGetValue(c.Esi, out Pair? frame);
        Require(knownShot && shot is not null && shot.Active && !shot.ActivationPending && !shot.DeactivationPending &&
            knownFrame && frame is not null, "Own Stinger candidate has no fresh paired native activation/collision.");
        Shot known = shot!; Pair nativeFrame = frame!; VerifyShot(known);
        uint stack = checked(nativeFrame.Esp - 0x58), previousSeh = U(stack + 0x4C);
        Require(c.Esp == stack && U(stack) == nativeFrame.Edi && U(stack + 4) == known.Projectile &&
            U(stack + 0x58) == 0x502255 && U(stack + 0x50) == 0x6C6968 && U(stack + 0x54) == 0 &&
            (previousSeh == uint.MaxValue || previousSeh >= nativeFrame.Esp && (previousSeh & 3) == 0) &&
            U(stack + 0xC) == 0 && c.Edi == _scene,
            "Own Stinger role stop lost its exact native stack/SEH/caller frame.");
        Require(c.Eax == 0 && U(known.Projectile + 0x50) == 0 && c.Edx == 0 &&
            (c.EFlags & 0x8C5) == 0x44 && Word(metadata + 4) == 0xE && U(metadata + 8) == _instance &&
            U(known.Projectile + 0xF4) == _instance && known.SourceInstance != _instance &&
            Byte(known.Projectile + 0x30) == 1 && !nativeFrame.RoleOverridden && known.RoleOverrides == 0,
            "Own Stinger role decision is not the qualified filter0/missing10/source-excluded case.");
        nativeFrame.RoleOverridden = true; ++known.RoleOverrides; Add("role.own-filter0-overridden");
        Sample("own-role-overridden", new { epoch = _epoch, generation = known.Generation,
            projectile = Hex(known.Projectile), sourceInstance = Hex(known.SourceInstance), metadata = Hex(metadata),
            ownInstance = Hex(_instance), flagsWord = Word(metadata + 4), nativeFilter50 = c.Eax,
            entryEsp = Hex(nativeFrame.Esp), frameEsp = Hex(c.Esp), previousSeh = Hex(previousSeh),
            nativeReturn = Hex(U(stack + 0x58)), originalEflags = Hex(c.EFlags),
            from = Hex(RoleBranch), to = Hex(RoleContinuation), nativeMask8StillExecutes = true,
            metadataOrTargetOrDamageWritten = false });
        c.Eip = RoleContinuation; // All other stopped context fields remain unchanged.
    }

    private void OwnSend(uint tid, ref Win32Native.X86Context c)
    {
        if (U(c.Esp) != _instance) return;
        if (U(c.Esi + 0x50) != 0) { Add("native.other-filter-own-send-pass-through"); return; }
        bool knownShot = _shots.TryGetValue(c.Esi, out Shot? shot);
        bool knownFrame = _collisions.TryGetValue(c.Esi, out Pair? frame);
        Require(knownShot && shot is not null && shot.Active && knownFrame && frame is not null &&
            frame.RoleOverridden && c.Ecx == c.Esi,
            "Own native Stinger send lacks its exact known generation/role frame.");
        Shot known = shot!; Pair nativeFrame = frame!; VerifyOwn(); VerifyShot(known);
        Require(c.Esp == checked(nativeFrame.Esp - 0x58 - 16) && U(c.Esp + 4) == 0x2717 &&
            U(c.Esp + 8) == U(known.Projectile + 0x3C) && U(c.Esp + 12) == 0 &&
            U(known.Projectile + 0xF4) == _instance && nativeFrame.OwnSends == 0 && known.OwnSendCallsites == 0,
            "Own Stinger native2717 arguments/frame differ or repeat within one generation.");
        ++nativeFrame.OwnSends; ++known.OwnSendCallsites; Add("native.own2717-callsite-reached");
        Sample("own-native2717-callsite", new { epoch = _epoch, generation = known.Generation,
            projectile = Hex(known.Projectile), target = Hex(_instance), nativeSenderEcx = Hex(c.Ecx),
            damageRaw = U(c.Esp + 8), nativeFrameEsp = Hex(c.Esp),
            nativeCallNotReplaced = true, canonicalHealthCommitClaimedHere = false });
    }

    private void Destroy(uint tid, ref Win32Native.X86Context c)
    {
        if (!_shots.TryGetValue(c.Ecx, out Shot? shot)) return;
        RequireNoPairFor(shot.Projectile);
        // A parent may already be unlinking its scene/pool. At this exact
        // native destructor entry inspect only the still-live projectile.
        Require(U(shot.Projectile) == StingerTable && U(shot.Projectile + 0x18) == shot.Source &&
            U(shot.Projectile + 0x14) == shot.Node, "Stinger destructor lost the captured native allocation identity.");
        _shots.Remove(shot.Projectile); Add("lifetime.native-destructor-retired");
        Sample("native-destructor-entry-retired", new { epoch = _epoch, generation = shot.Generation,
            projectile = Hex(shot.Projectile), historicalProjectileReadsNowForbidden = true });
    }

    private delegate void Returned(Pair pair, ref Win32Native.X86Context context);
    private Pair BeginPair(uint tid, string kind, Shot shot, Win32Native.X86Context c, uint delta, Returned completed)
    {
        uint caller = U(c.Esp);
        Require(_pending.Count < 8 && c.Esp >= 0x10000 + 0x100 && (c.Esp & 3) == 0 &&
            caller is >= 0x401000 and < 0x6DA000, "Stinger native pairing exceeds its stack/caller/nesting bounds.");
        Pair pair = new(kind, shot, c.Esp, caller, delta, c.Esi, c.Edi); _pending.Add(pair);
        _host.Breakpoints.AddReturn(caller, tid, "StingerSecondaryRole." + kind + ".return",
            (uint rt, uint at, ref Win32Native.X86Context returned) =>
            {
                Ensure(rt);
                Require(Active && at == pair.Caller && returned.Eip == at && returned.Esp == checked(pair.Esp + pair.Delta) &&
                    returned.Esi == pair.Esi && returned.Edi == pair.Edi && _pending.Count > 0 &&
                    ReferenceEquals(_pending[^1], pair), "Stinger native return lost its exact frame, caller or preserved registers.");
                try { completed(pair, ref returned); _pending.RemoveAt(_pending.Count - 1); }
                catch { _failed = true; throw; }
            });
        return pair;
    }

    private Source? CaptureSource(uint projectile)
    {
        uint source = Pointer(projectile + 0x18);
        if (U(source) != SourceManagerTable) return null;
        uint owner = Pointer(source + 0x24), root = Pointer(owner + 0x24), node = Pointer(projectile + 0x14);
        if (U(owner) != 0x710F80 || U(root + 0x3C) != _scene) return null;
        Require(U(root) == 0x6DC4F4 && owner != _instance &&
            Enumerable.Range(0, 3).Count(index => U(source + 0x1D0 + (uint)index * 4) == projectile) == 1,
            "Native Stinger source is not a distinct live Mosquito pool owner in the admitted scene.");
        return new(source, owner, root, node);
    }
    private void VerifyShot(Shot shot)
    {
        Require(_shots.TryGetValue(shot.Projectile, out Shot? current) && ReferenceEquals(current, shot) &&
            U(shot.Projectile) == StingerTable, "Stinger generation binding was replaced or destroyed.");
        Source? source = CaptureSource(shot.Projectile);
        Require(source is not null && source.Manager == shot.Source && source.Instance == shot.SourceInstance &&
            source.Root == shot.SourceRoot && source.Node == shot.Node, "Stinger source/scene/allocation ownership changed.");
    }
    private void VerifyOwn()
    {
        Require(U(0x765AD4) == _profile && U(_profile) == 0x6F5A44 && U(_profile + 0x2B4) == _primaryInstance &&
            _instance != _primaryInstance && U(_instance) == 0x710F80 && U(_instance + 0x24) == _root && U(_root) == 0x6DC4F4 &&
            U(_root + 0x3C) == _scene && U(_scene) == 0x6E7358 && U(_move) == 0x6F64F8 &&
            U(_move + 0x24) == _instance && U(_move + 0x18) == _root && U(_move + 0x160) == _character &&
            U(_character) == 0x6F6438 && U(_character + 0x24) == _instance && U(_character + 0x12C) == _move &&
            U(_move + 0x174) == _metadata && U(_root + 0xC) == _metadata && U(_metadata + 8) == _instance &&
            Word(_metadata + 4) == 0xE && U(0x755274) == _engine && U(_engine + 0x18) == _scene &&
            U(0x755298) == _timer && U(_timer) == 0x702F94,
            "Secondary Stinger role lost its exact native own graph/metadata WORD E/scene epoch.");
    }
    private void RequireNoPairFor(uint projectile) => Require(!_pending.Any(pair => pair.Shot.Projectile == projectile),
        "Stinger lifetime change would interrupt this projectile's native paired operation.");
    private Win32Native.X86Context WorldContext()
    {
        Ensure(_host.MainThreadId); Win32Native.X86Context context = Win32Native.GetX86Context(_host.MainThreadId, true);
        Require(context.Eip == NativeCallDispatcher.WorldReturn + 1 && ReadCode(NativeCallDispatcher.WorldReturn, 1)[0] == 0xC3,
            "Stinger role convenience API requires an actual native WorldRET breakpoint.");
        context.Eip = NativeCallDispatcher.WorldReturn; return context;
    }

    private delegate void Stop(uint tid, ref Win32Native.X86Context context);
    private void Hook(uint address, string hex, string name, Stop action)
    {
        byte[] expected = Convert.FromHexString(hex); Pin(address, hex);
        Require(!_host.Breakpoints.Contains(address), "Stinger role entry already has another subscription; multicast is not supported.");
        BreakpointCallback repeat = null!;
        repeat = (uint tid, uint at, ref Win32Native.X86Context c) =>
        {
            Ensure(tid); if (_retired) return;
            Require(Active && at == address && c.Eip == address && Read(at, expected.Length).SequenceEqual(expected),
                "Stinger role original instruction/lifetime differs from its pin.");
            try { Add(name); action(tid, ref c); }
            catch { _failed = true; throw; }
            if (Active) _host.Breakpoints.AddReturn(address, tid, "StingerSecondaryRole." + name, repeat);
        };
        _host.Breakpoints.AddReturn(address, _host.MainThreadId, "StingerSecondaryRole." + name, repeat);
    }
    private void Pin(uint address, string hex)
    {
        byte[] expected = Convert.FromHexString(hex);
        Require(Image(address, expected.Length).SequenceEqual(expected) && ReadCode(address, expected.Length).SequenceEqual(expected),
            $"Stinger role image/live source pin changed at {Hex(address)}.");
    }
    private byte[] ReadCode(uint address, int count)
    {
        byte[] actual = Read(address, count), original = Image(address, count);
        for (int offset = 0; offset < count; ++offset)
            if (actual[offset] == 0xCC && original[offset] != 0xCC && _host.Breakpoints.Contains(address + (uint)offset))
                actual[offset] = original[offset]; // Only an already manager-owned INT3 byte is normalized.
        return actual;
    }
    private void Add(string name)
    { uint count = _counts.GetValueOrDefault(name) + 1; Require(count <= 100000, "Stinger role callback budget exceeded."); _counts[name] = count; }
    private void Sample(string name, object data)
    { if (_samples.Count >= 256) return; var sample = new { name, data }; _samples.Add(sample); _log(new { @event = "stinger-secondary-role", name, data }); }
    private byte[] Image(uint address, int count) => _image.GetSectionData(checked((int)(address - 0x400000))).GetContent(0, count).ToArray();
    private uint ImageWord(uint address) => BitConverter.ToUInt32(Image(address, 4));
    private uint Pointer(uint address) { uint value = U(address); Require(value >= 0x10000, "Stinger role native pointer is null."); return value; }
    private uint U(uint address) => BitConverter.ToUInt32(Read(address, 4));
    private ushort Word(uint address) => BitConverter.ToUInt16(Read(address, 2));
    private byte Byte(uint address) => Read(address, 1)[0];
    private byte[] Read(uint address, int count)
    { Ensure(_host.MainThreadId); Require(address >= 0x10000 && count is > 0 and <= 512, "Stinger role read exceeds its bound."); return Win32Native.ReadMemory(_host.ProcessHandle, address, count); }
    private void Ensure(uint tid) => Require(_host.IsAtDebugStop && tid == _host.MainThreadId &&
        Environment.CurrentManagedThreadId == _ownerThread, "Stinger role requires its owned native main-thread debug stop.");
    private static string Hex(uint value) => $"0x{value:X8}";
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidDataException(message); }
}
