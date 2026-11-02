using System.Reflection.PortableExecutable;
using System.Security.Cryptography;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// Finite ordinary-projectile policy candidate: own native role production,
/// known-teammate self-ignore, and one owned unfocused launch. No integration,
/// native-call dispatch, metadata write, shared target or Character swap.
/// </summary>
internal sealed class OrdinaryPlayerProjectileRole
{
    private const uint ManagerTable = 0x6F73D8, ProjectileTable = 0x6F6B04;
    private readonly DebuggerHost _host;
    private readonly PEReader _image;
    private readonly Action<object> _log;
    private readonly int _ownerThread;
    private readonly List<Frame> _frames = [];
    private readonly Dictionary<uint, Pool> _ownPools = [];
    private readonly HashSet<uint> _primaryPools = [], _primaryNodes = [];
    private readonly Dictionary<string, uint> _counts = [];
    private readonly List<object> _samples = [];
    private Func<bool> _ownsGeneration = () => false, _privateLifecycle = () => false, _ownActionAuthorized = () => false;
    private Func<uint>? _ownEnemyNodeProvider;
    private uint _ownDirectedLaunches, _ownForwardLaunches, _ownAimQueries;
    private bool _primaryLaunchForbidden;
    private uint _primaryLaunchEscapes;
    private uint _ownInstance, _ownCharacter, _ownManager, _ownMetadata, _ownRegion, _ownRoot, _ownMachine;
    private uint _primaryInstance, _primaryCharacter, _primaryManager, _primaryMetadata, _primaryRoot;
    private uint _profile, _targetManager, _selection20, _selection24, _player28, _scene, _epoch;
    private uint _factoryReturns, _ownLaunches, _maxOwnLaunches, _worlds, _retireEntry;
    private bool _bound, _retired, _failed, _aimVerified;

    private sealed class Pool(uint projectile, uint node, uint type, uint slot)
    {
        internal readonly uint Projectile = projectile, Node = node, Type = type, Slot = slot;
        internal bool NativeInitReturned, RoleComputed, RoleCommitted;
    }
    private sealed class Frame(string kind, uint receiver, uint esp, uint caller, uint delta,
        uint ebx, uint ebp, uint esi, uint edi)
    {
        internal readonly string Kind = kind;
        internal readonly uint Receiver = receiver, Esp = esp, Caller = caller, Delta = delta;
        internal readonly uint Ebx = ebx, Ebp = ebp, Esi = esi, Edi = edi;
        internal uint Type, PoolCount, TeamIgnores;
        internal Pool? Pool;
        internal bool NativeRetSeen, ForwardChosen;
    }

    internal OrdinaryPlayerProjectileRole(DebuggerHost host, PEReader image, Action<object> log)
    {
        _host = host ?? throw new ArgumentNullException(nameof(host));
        _image = image ?? throw new ArgumentNullException(nameof(image));
        _log = log ?? throw new ArgumentNullException(nameof(log));
        _ownerThread = Environment.CurrentManagedThreadId;
        Require(Convert.ToHexString(SHA256.HashData(image.GetEntireImage().GetContent().ToArray())) ==
            "3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F",
            "Ordinary role requires the pinned original PE image.");
    }

    internal bool Active => _bound && !_retired && !_failed;
    internal bool Retired => _retired;
    internal bool NativeRolePoolsReady => _ownPools.Count == 18 && _factoryReturns == 3 &&
        _ownPools.Values.All(pool => pool.NativeInitReturned && pool.RoleComputed && pool.RoleCommitted);
    internal object Summary => new
    {
        bound = _bound, active = Active, retiredBeforeOwnDelete = _retired, terminalFailure = _failed,
        epoch = _epoch, ownInstance = Hex(_ownInstance), primaryInstance = Hex(_primaryInstance),
        ownManager = Hex(_ownManager), ownMetadata = Hex(_ownMetadata), ownTargetRegion = Hex(_ownRegion),
        ownNativeRolePoolsReady = NativeRolePoolsReady, ownPools = _ownPools.Count, primaryPools = _primaryPools.Count,
        factoryReturns = _factoryReturns, aimExclusionVerified = _aimVerified, ownLaunches = _ownLaunches,
        maximumOwnLaunches = _maxOwnLaunches, verifiedWorlds = _worlds, retireEntry = Hex(_retireEntry),
        pendingNativeFrames = _frames.Count, maximumNativeFrames = 8, maximumTeamIgnores = 64,
        counts = new Dictionary<string, uint>(_counts), samples = _samples.ToArray(),
        onlyContextChanges = _ownEnemyNodeProvider is null ?
            "own factory507DC1:EAX=0; role1 teammate4FF664:EIP=4FF628; own launch508289:EIP=508320" :
            "own factory507DC1:EAX=0; teammate4FF664:EIP=4FF628; own508289:EAX=freshEnemyNode,EIP=508230 or EIP=508320",
        directedAimProviderConfigured = _ownEnemyNodeProvider is not null,
        directedLaunches = _ownDirectedLaunches, forwardLaunches = _ownForwardLaunches, aimQueries = _ownAimQueries,
        selectedNodePointerPersisted = false, targetOrPM34CWritesByProvider = 0,
        primaryLaunchForbidden = _primaryLaunchForbidden, primaryLaunchEscapes = _primaryLaunchEscapes,
        gameFieldWrites = 0, nativeCallsAdded = 0, rawMetadataOrRoleWrites = 0,
        targetOrCharacterSingletonSwaps = 0, floatingPointInstructionsEmitted = false,
        ordinaryPoolTable = Hex(ProjectileTable), otherProjectileFamiliesAdmitted = false,
        independentAutoaimClaimed = false, runtimeFirstShotQualified = false, noHistoricalReadsAfterRetirement = true
    };

    /// <summary>Bind after Create and before own Init, at the caller's stopped private lifecycle boundary.</summary>
    internal void BindStoppedBeforeInit(uint tid, ref Win32Native.X86Context context,
        uint ownInstance, uint ownCharacter, uint ownManager, uint ownMetadata, uint ownTargetRegion,
        uint primaryInstance, uint primaryCharacter, uint primaryManager, uint primaryMetadata,
        Func<bool> ownsCurrentGeneration, Func<bool> privateLifecycleActive, Func<bool> ownActionAuthorized,
        uint maximumOwnLaunches = 1, Func<uint>? ownEnemyNodeProvider = null, bool primaryLaunchForbidden = false)
    {
        Ensure(tid);
        Require(!_bound && !_retired && !_failed && _host.ImageBase == 0x400000 &&
            maximumOwnLaunches is >= 1 and <= 8 && context.Eip >= 0x401000,
            "Ordinary role binding is repeated, outside the original image, or exceeds the finite launch budget.");
        _ownsGeneration = ownsCurrentGeneration ?? throw new ArgumentNullException(nameof(ownsCurrentGeneration));
        _privateLifecycle = privateLifecycleActive ?? throw new ArgumentNullException(nameof(privateLifecycleActive));
        _ownActionAuthorized = ownActionAuthorized ?? throw new ArgumentNullException(nameof(ownActionAuthorized));
        _ownEnemyNodeProvider = ownEnemyNodeProvider;
        _primaryLaunchForbidden = primaryLaunchForbidden;
        Require(_ownsGeneration() && _privateLifecycle() && !_ownActionAuthorized(),
            "Ordinary role must bind in the owned stopped lifecycle before any own action.");
        _ownInstance = ownInstance; _ownCharacter = ownCharacter; _ownManager = ownManager;
        _ownMetadata = ownMetadata; _ownRegion = ownTargetRegion;
        _primaryInstance = primaryInstance; _primaryCharacter = primaryCharacter;
        _primaryManager = primaryManager; _primaryMetadata = primaryMetadata;
        _ownRoot = Pointer(ownInstance + 0x24); _primaryRoot = Pointer(primaryInstance + 0x24);
        _scene = Pointer(_primaryRoot + 0x3C); _profile = Pointer(0x765AD4); _targetManager = Pointer(0x765AF0);
        uint flow = Pointer(0x755294), stack = U(flow + 0x1AC);
        Require(U(flow + 0x1B0) == 47 && U(flow + 0x1B8) == 0 && stack <= 20 &&
            U(Pointer(flow + 0x15C + stack * 4)) == 0x7105C8,
            "Ordinary first-shot policy requires the settled Battle47 playing state.");
        _selection20 = U(_targetManager + 0x20); _selection24 = U(_targetManager + 0x24); _player28 = U(_targetManager + 0x28);
        _maxOwnLaunches = maximumOwnLaunches; _epoch = _host.DebugStopSequence;
        VerifyBaseGraph(initialized: false);
        Require(_ownRegion != _selection20 && _ownRegion != _selection24 && _ownRegion != _player28 &&
            Enumerable.Range(0, 42).All(i => U(_ownManager + 0x1D0 + (uint)i * 4) == 0),
            "Ordinary role must bind before any own pool initialization or own-region target selection.");
        for (uint i = 0; i < 18; ++i)
        {
            uint projectile = Pointer(_primaryManager + 0x1D0 + i * 4);
            Require(_primaryPools.Add(projectile), "Primary ordinary pool contains an alias.");
            VerifyProjectile(projectile, _primaryManager, expectedRole: 1);
            Require(_primaryNodes.Add(Pointer(projectile + 0x14)), "Primary ordinary pool clones alias each other.");
        }
        Pin(0x507DB5, "8B85240100008B804801000033C985C00F94C185DB894E50");
        Pin(0x507D8E, "8B5C24308B8C9DC8020000");
        Pin(0x507DA0, "8B06515257558BCEFF501C");
        Pin(0x509C28, "6A008BCEE88FDCFFFF6A018BCEE886DCFFFF6A028BCEE87DDCFFFF");
        Pin(0x5080AA, "8B442414893083C00489442414FF4C24180F851FFCFFFF");
        Pin(0x4FF664, "3B782474BF"); Pin(0x4FF661, "8B4618");
        Pin(0x4FF628, "8D4C243CC744245CFFFFFFFFE867F4F8FF32C0");
        Pin(0x4FFFE2, "E879F4FFFF"); Pin(0x4FF6DD, "0FB745048BC883E108");
        Pin(0x508289, "8B814801000085C08B8E44030000751B");
        Pin(0x508320, "8B46248B4024058C000000508D4C2428E8AB6EF0FF");
        if (_ownEnemyNodeProvider is not null)
        {
            Pin(0x508230, "D9407CD8642420D94078D864241CD94074D8642418D95C240CD95C2410D95C2414E9F7000000");
            Require(!_host.Breakpoints.Contains(0x508230), "Directed native branch entry already has a breakpoint owner.");
        }
        Pin(0x50A210, "8B81340300008B5424045052E8CFDEFFFFC20400");
        _bound = true;
        Hook(0x5078C0, "6AFFE94FF8F4FF", "factory-entry", Factory);
        Hook(0x4FF2C0, "64A1000000006AFF6851686C00", "pool-native-init-entry", PoolInit);
        Hook(0x4FF457, "C21000", "pool-native-init-RET10", (uint t, ref Win32Native.X86Context c) => NativeRet(t, ref c, "pool-init"));
        Hook(0x507DC1, "33C985C00F94C185DB894E50", "own-native-role-compute", Role);
        Hook(0x507DCD, "7409", "own-native-role-stored", RoleStored);
        Hook(0x5080E6, "C20400", "factory-RET4", (uint t, ref Win32Native.X86Context c) => NativeRet(t, ref c, "factory"));
        Hook(0x4FF460, "6AFF64A100000000", "ordinary-collision-entry", Collision);
        Hook(0x4FF664, "3B7824", "known-teammate-self-ignore", TeamIgnore);
        Hook(0x4FF64D, "C3", "ordinary-collision-ignore-C3", (uint t, ref Win32Native.X86Context c) => NativeRet(t, ref c, "collision"));
        Hook(0x4FF7FA, "C3", "ordinary-collision-hit-C3", (uint t, ref Win32Native.X86Context c) => NativeRet(t, ref c, "collision"));
        Hook(0x5080F0, "8B44240883EC3C", "own-launch-entry", Launch);
        Hook(0x508289, "8B8148010000", "own-forward-fallback", Forward);
        foreach (uint ret in new uint[] { 0x5084A0, 0x5084EF, 0x5085A6 })
            Hook(ret, "C20800", "own-launch-RET8", (uint t, ref Win32Native.X86Context c) => NativeRet(t, ref c, "launch"));
        Sample("bound-before-init", new { epoch = _epoch, ownManager = Hex(_ownManager), ownMetadataMayBeDeferred = ownMetadata == 0,
            primaryPools = _primaryPools.Count, modeLimit = "own BloomFSM140 low nibble must not be1/2" });
    }

    internal void VerifyAimExclusionStopped(uint tid, ref Win32Native.X86Context context)
        => VerifyAimExclusionStopped(tid);

    // Genuine stopped post-init callers have no logical World context to pass.
    internal void VerifyAimExclusionStopped(uint tid)
    {
        Ensure(tid); Require(Active && _privateLifecycle() && !_ownActionAuthorized() && _frames.Count == 0 && NativeRolePoolsReady,
            "Aim exclusion must be verified after paired own init and before gameplay resumes.");
        ResolveOwnMetadata(); VerifyBaseGraph(initialized: true); VerifyAim();
        Require(U(_targetManager + 0x20) == _selection20 && U(_targetManager + 0x24) == _selection24 &&
            U(_targetManager + 0x28) == _player28, "Own init or aim preparation changed the primary target cache.");
        _aimVerified = true;
        Sample("aim-exclusion-verified", new { region = Hex(_ownRegion), metadata = Hex(_ownMetadata),
            isTarget = false, flagsWord = Word(_ownMetadata + 4), nativeSetterCalledByThisClass = false });
    }

    internal void VerifyAtWorldStopped(uint tid, ref Win32Native.X86Context context)
    {
        Ensure(tid); if (_retired) return;
        Require(Active && context.Eip == NativeCallDispatcher.WorldReturn && _frames.Count == 0 && _aimVerified && NativeRolePoolsReady,
            "Ordinary role WorldRET has an unmatched frame or incomplete preparation.");
        VerifyBaseGraph(initialized: true); VerifyAim();
        Require(++_worlds <= NativeObservationBudget.WorldLimit, "Ordinary role World budget exceeded.");
    }

    internal void RetireBeforeOwnDelete(uint tid, ref Win32Native.X86Context context)
    {
        Ensure(tid); Require(Active && _privateLifecycle() && _frames.Count == 0,
            "Ordinary role must retire in the owned pre-delete interval with no pending native frame.");
        VerifyBaseGraph(initialized: _factoryReturns != 0); _retireEntry = context.Eip; _retired = true;
        Sample("retired-before-own-delete", new { epoch = _epoch, entry = Hex(context.Eip),
            callbacksDormantBeforeDelete = true, historicalObjectReadsNowForbidden = true });
    }

    // Read-only proof for the separate network-action observer. The existing
    // collision policy owns the frame and all context substitutions.
    internal bool TryOwnEnemySend(uint tid, in Win32Native.X86Context context,
        out uint projectile, out uint target, out uint metadata)
    {
        Ensure(tid); projectile = target = metadata = 0;
        if (!Active || _frames.Count == 0 || _frames[^1].Kind != "collision" ||
            _frames[^1].Receiver != context.Esi || U(context.Esi + 0x18) != _ownManager) return false;
        Frame frame = _frames[^1]; VerifyBaseGraph(initialized: true); VerifyKnownProjectile(context.Esi, _ownManager);
        Require(context.Eip == 0x4FF750 && context.Ecx == context.Esi &&
            context.Esp == checked(frame.Esp - 0x70) && U(context.Esp) == context.Edi &&
            U(context.Esp + 4) == 0x2717 && U(context.Esp + 8) == U(context.Esi + 0x3C) &&
            context.Edi != _ownInstance && context.Edi != _primaryInstance &&
            U(context.Ebp + 8) == context.Edi && (Word(context.Ebp + 4) & 8) != 0 &&
            Byte(context.Esi + 0x30) == 1 && frame.TeamIgnores == 0,
            "Own enemy send lost its paired native collision/target/metadata/active-pool proof.");
        projectile = context.Esi; target = context.Edi; metadata = context.Ebp; return true;
    }

    private void Factory(uint tid, ref Win32Native.X86Context c)
    {
        if (c.Ecx != _ownManager) return;
        Require(_privateLifecycle() && !_ownActionAuthorized() && _frames.Count == 0 && _factoryReturns < 3,
            "Owned ordinary factory escaped its stopped native initialization interval.");
        VerifyBaseGraph(initialized: false);
        uint type = U(c.Esp + 4), caller = U(c.Esp);
        Require(type == _factoryReturns && caller == 0x509C31 + type * 9,
            "Owned ordinary factory has a different native type/caller/order.");
        Frame frame = BeginPair(tid, "factory", _ownManager, c, 8, (Frame done, ref Win32Native.X86Context returned) =>
        {
            Require(_privateLifecycle() && done.PoolCount == 6 && done.Pool is null,
                "Own factory returned without six paired native role stores.");
            for (uint i = 0; i < 6; ++i)
            {
                uint slot = _ownManager + 0x1D0 + done.Type * 24 + i * 4, projectile = Pointer(slot);
                Require(_ownPools.TryGetValue(projectile, out Pool? pool) && pool is not null && pool.Slot == slot && pool.RoleCommitted,
                    "Own native factory did not commit its exact new pool objects.");
                VerifyProjectile(projectile, _ownManager, expectedRole: 1);
            }
            ++_factoryReturns;
        });
        frame.Type = type;
    }

    private void PoolInit(uint tid, ref Win32Native.X86Context c)
    {
        if (U(c.Esp + 4) != _ownManager) return;
        Require(_privateLifecycle() && _frames.Count > 0 && _frames[^1].Kind == "factory",
            "Own pool native Init has no matching native allocation factory.");
        Frame factory = _frames[^1]; uint node = Pointer(c.Esp + 8);
        Require(U(c.Esp) == 0x507DAB && c.Esp == checked(factory.Esp - 0x40) &&
            factory.PoolCount < 6 && factory.Pool is null && U(c.Ecx) == ProjectileTable &&
            !_primaryPools.Contains(c.Ecx) && !_ownPools.ContainsKey(c.Ecx) && !_primaryNodes.Contains(node) &&
            U(node + 0x3C) == _scene && U(node) == 0x6DC4F4 && OutsideBloomTrees(node),
            "Own pool Init lost its native receiver/clone/caller/ESP/scene allocation proof.");
        uint slot = _ownManager + 0x1D0 + factory.Type * 24 + factory.PoolCount * 4;
        Require(U(slot) == 0 && !_ownPools.Values.Any(pool => pool.Node == node),
            "Own new pool aliases an existing slot or clone.");
        Pool pool = new(c.Ecx, node, factory.Type, slot); factory.Pool = pool; _ownPools.Add(c.Ecx, pool);
        Frame initialized = BeginPair(tid, "pool-init", c.Ecx, c, 20, (Frame done, ref Win32Native.X86Context returned) =>
        {
            Require(_frames.Count >= 2 && ReferenceEquals(_frames[^2], factory) && ReferenceEquals(factory.Pool, pool),
                "Own native pool Init returned to a different allocation frame.");
            VerifyProjectile(pool.Projectile, _ownManager, expectedRole: null);
            Require(U(pool.Projectile + 0x14) == pool.Node && Byte(pool.Projectile + 0x30) == 0,
                "Own native pool Init returned with a foreign clone or active projectile.");
            pool.NativeInitReturned = true;
        });
        initialized.Pool = pool;
    }

    private void Role(uint tid, ref Win32Native.X86Context c)
    {
        if (c.Ebp != _ownManager) return;
        Frame frame = FactoryForRole(c); Pool pool = frame.Pool!;
        Require(pool.NativeInitReturned && !pool.RoleComputed && !pool.RoleCommitted && c.Eax == 1 &&
            U(_ownCharacter + 0x148) == 1, "Own native role selector lacks its original Character type1 input.");
        pool.RoleComputed = true; c.Eax = 0; Add("own-native-role-EAX-substituted", 18);
    }

    private void RoleStored(uint tid, ref Win32Native.X86Context c)
    {
        if (c.Ebp != _ownManager) return;
        Frame frame = FactoryForRole(c); Pool pool = frame.Pool!;
        Require(pool.RoleComputed && !pool.RoleCommitted && c.Eax == 0 && c.Ecx == 1 && U(pool.Projectile + 0x50) == 1,
            "Own native role store did not follow its admitted register-only selector.");
        pool.RoleCommitted = true; ++frame.PoolCount; frame.Pool = null; Add("own-native-role-store-witnessed", 18);
    }

    private Frame FactoryForRole(Win32Native.X86Context c)
    {
        Require(_privateLifecycle() && _frames.Count > 0 && _frames[^1].Kind == "factory",
            "Own role computation has no paired factory frame.");
        Frame frame = _frames[^1]; Pool? pool = frame.Pool;
        Require(pool is not null && c.Esp == checked(frame.Esp - 0x2C) && U(c.Esp + 0x14) == pool.Slot &&
            U(c.Esp + 0x18) == 6 - frame.PoolCount && c.Esi == pool.Projectile && c.Edi == pool.Node &&
            c.Ebx == frame.Type && U(pool.Projectile) == ProjectileTable && U(pool.Projectile + 0x18) == _ownManager &&
            U(pool.Projectile + 0x14) == pool.Node && U(_ownManager + 0x124) == _ownCharacter,
            "Own role lost the native allocation index/remaining/receiver/clone/ESP graph.");
        return frame;
    }

    private void Collision(uint tid, ref Win32Native.X86Context c)
    {
        if (U(c.Ecx) != ProjectileTable) return;
        uint manager = U(c.Ecx + 0x18);
        if (manager != _primaryManager && manager != _ownManager) return;
        Require(_aimVerified && NativeRolePoolsReady && !_privateLifecycle() && U(c.Esp) == 0x4FFFE7,
            "Known player collision escaped gameplay or its original ordinary update caller.");
        VerifyBaseGraph(initialized: true); VerifyKnownProjectile(c.Ecx, manager);
        BeginPair(tid, "collision", c.Ecx, c, 4, (Frame done, ref Win32Native.X86Context returned) =>
        {
            Require(done.TeamIgnores == 0 || (returned.Eax & 0xFF) == 0,
                "Native teammate self-ignore did not return false.");
        });
    }

    private void TeamIgnore(uint tid, ref Win32Native.X86Context c)
    {
        if (_frames.Count == 0 || _frames[^1].Kind != "collision" || _frames[^1].Receiver != c.Esi) return;
        Frame frame = _frames[^1]; uint manager = U(c.Esi + 0x18);
        uint teammate = manager == _primaryManager ? _ownInstance : _primaryInstance;
        uint metadata = manager == _primaryManager ? _ownMetadata : _primaryMetadata;
        if (c.Edi != teammate || c.Ebp != metadata) return;
        VerifyBaseGraph(initialized: true); VerifyKnownProjectile(c.Esi, manager);
        Require(c.Eax == manager && U(metadata + 8) == teammate && c.Esp == checked(frame.Esp - 0x60) &&
            U(c.Esi + 0x50) == 1 && frame.TeamIgnores == 0,
            "Known teammate collision lost its exact native source/metadata/role1/frame proof.");
        ++frame.TeamIgnores; Add("native-teammate-self-ignore", 64); c.Eip = 0x4FF628;
        Sample("teammate-self-ignore", new { projectile = Hex(c.Esi), source = Hex(U(manager + 0x24)),
            teammate = Hex(teammate), metadata = Hex(metadata), continuation = Hex(c.Eip), noHitNotificationAdded = true });
    }

    private void Launch(uint tid, ref Win32Native.X86Context c)
    {
        if (_primaryLaunchForbidden && c.Ecx == _primaryManager)
        {
            VerifyBaseGraph(initialized: true); VerifyAim(); ++_primaryLaunchEscapes;
            throw new InvalidDataException("Primary launch bypassed its explicit one-game capture-only wrapper policy.");
        }
        if (c.Ecx != _ownManager) return;
        Require(_aimVerified && NativeRolePoolsReady && !_privateLifecycle() && _ownActionAuthorized() &&
            U(c.Esp) == 0x50A221 && U(c.Esp + 4) <= 2 && _ownLaunches < _maxOwnLaunches,
            "Own ordinary launch lacks an authorized finite action and original native wrapper/caller.");
        VerifyBaseGraph(initialized: true); VerifyAim();
        uint mode = U(_ownMachine + 0x140) & 15, emitter = U(c.Esp + 8);
        Require(mode is not 1 and not 2 && emitter != 0 &&
            (emitter == U(_ownManager + 0x334) || emitter == U(_ownManager + 0x338) || emitter == U(_ownManager + 0x33C)),
            "Own forward launch refuses native aimed modes1/2 or a foreign emitter.");
        ++_ownLaunches;
        BeginPair(tid, "launch", _ownManager, c, 12, (Frame done, ref Win32Native.X86Context returned) =>
            Require(done.ForwardChosen, "Own native launch returned without its admitted forward fallback."));
    }

    private void Forward(uint tid, ref Win32Native.X86Context c)
    {
        if (c.Esi != _ownManager) return;
        Require(_frames.Count > 0 && _frames[^1].Kind == "launch" && _ownActionAuthorized(),
            "Own forward branch has no paired authorized ordinary launch.");
        Frame frame = _frames[^1]; uint mode = U(_ownMachine + 0x140) & 15;
        Require(frame.Receiver == _ownManager && !frame.ForwardChosen && c.Ecx == _ownCharacter &&
            c.Esp == checked(frame.Esp - 0x48) && c.Edi == _ownInstance && mode is not 1 and not 2,
            "Own forward branch changed its Character/Instance/ESP or entered an aimed mode.");
        VerifyBaseGraph(initialized: true); VerifyAim(); frame.ForwardChosen = true;
        if (_ownEnemyNodeProvider is not null)
        {
            uint selection20 = U(_targetManager + 0x20), selection24 = U(_targetManager + 0x24), player28 = U(_targetManager + 0x28),
                manager34C = U(_ownManager + 0x34C);
            Require(++_ownAimQueries <= _maxOwnLaunches, "Own aim provider exceeded its finite launch budget.");
            uint node = _ownEnemyNodeProvider();
            VerifyBaseGraph(initialized: true); VerifyAim();
            Require(U(_targetManager + 0x20) == selection20 && U(_targetManager + 0x24) == selection24 &&
                U(_targetManager + 0x28) == player28 && U(_ownManager + 0x34C) == manager34C,
                "Own point provider changed a target cache or the borrowed PM point field.");
            if (node != 0)
            {
                Require(node != _ownRoot && node != _primaryRoot && U(node) == 0x6DC4F4 && U(node + 0x3C) == _scene,
                    "Own point provider returned a teammate or a foreign/untyped Node.");
                Require(!_host.Breakpoints.Contains(0x508230), "Directed native branch cannot jump into another breakpoint owner.");
                Add("own-native-directed-point", _maxOwnLaunches); ++_ownDirectedLaunches;
                c.Eax = node; c.Eip = 0x508230;
                Sample("own-native-directed-point", new { node = Hex(node), continuation = Hex(c.Eip),
                    nativeEmitterAndVectorMathRetained = true, targetAndPM34CUnchanged = true });
                return;
            }
        }
        ++_ownForwardLaunches;
        Add("own-native-forward-fallback", _maxOwnLaunches); c.Eip = 0x508320;
    }

    private delegate void Returned(Frame frame, ref Win32Native.X86Context context);
    private Frame BeginPair(uint tid, string kind, uint receiver, Win32Native.X86Context c, uint delta, Returned completed)
    {
        uint caller = U(c.Esp);
        Require(_frames.Count < 8 && c.Esp >= 0x10100 && (c.Esp & 3) == 0 && caller is >= 0x401000 and < 0x6DA000,
            "Ordinary policy native frame exceeds its caller/stack/nesting bound.");
        Frame frame = new(kind, receiver, c.Esp, caller, delta, c.Ebx, c.Ebp, c.Esi, c.Edi); _frames.Add(frame);
        _host.Breakpoints.AddReturn(caller, tid, "OrdinaryPlayerProjectileRole." + kind + ".return",
            (uint rt, uint at, ref Win32Native.X86Context returned) =>
            {
                Ensure(rt);
                try
                {
                    Require(Active && _ownsGeneration() && _frames.Count > 0 && ReferenceEquals(_frames[^1], frame) &&
                        frame.NativeRetSeen && at == frame.Caller && returned.Eip == at && returned.Esp == checked(frame.Esp + frame.Delta) &&
                        Preserved(frame, returned), "Ordinary native RET/caller/receiver/thread/ESP pairing failed.");
                    completed(frame, ref returned); _frames.RemoveAt(_frames.Count - 1); Add(kind + ".paired-return", 100000);
                }
                catch { _failed = true; throw; }
            });
        return frame;
    }

    private void NativeRet(uint tid, ref Win32Native.X86Context c, string kind)
    {
        if (_frames.Count == 0 || _frames[^1].Kind != kind) return;
        Frame frame = _frames[^1];
        Require(!frame.NativeRetSeen && c.Esp == frame.Esp && U(c.Esp) == frame.Caller && Preserved(frame, c),
            "Ordinary native return instruction has the wrong frame/caller/preserved registers.");
        frame.NativeRetSeen = true; Add(kind + ".native-ret", 100000);
    }
    private static bool Preserved(Frame frame, Win32Native.X86Context c) =>
        c.Ebx == frame.Ebx && c.Ebp == frame.Ebp && c.Esi == frame.Esi && c.Edi == frame.Edi;

    private void ResolveOwnMetadata()
    {
        uint metadata = Pointer(_ownRoot + 0xC);
        Require(_ownMetadata == 0 || _ownMetadata == metadata, "Own metadata changed from its known constructed identity.");
        _ownMetadata = metadata; _ownMachine = Pointer(_ownCharacter + 0x124);
    }
    private void VerifyAim() => Require(Byte(_ownRegion + 0x145) == 0 && Byte(_ownRegion + 0x148) == 0 &&
        Word(_ownMetadata + 4) == 0xE && U(_targetManager + 0x20) != _ownRegion &&
        U(_targetManager + 0x24) != _ownRegion && U(_targetManager + 0x28) == _player28,
        "Own native aim exclusion lost physical bit8 or reached a primary target cache.");
    private void VerifyBaseGraph(bool initialized)
    {
        Require(_ownsGeneration() && U(0x765AD4) == _profile && U(_profile + 0x2B4) == _primaryInstance &&
            U(0x765AF0) == _targetManager && U(_targetManager) == 0x703954 &&
            _ownInstance != _primaryInstance && _ownCharacter != _primaryCharacter && _ownManager != _primaryManager &&
            U(_ownInstance) == 0x710F80 && U(_primaryInstance) == 0x710F80 &&
            U(_ownInstance + 0x24) == _ownRoot && U(_primaryInstance + 0x24) == _primaryRoot &&
            U(_ownRoot) == 0x6DC4F4 && U(_primaryRoot) == 0x6DC4F4 && _ownRoot != _primaryRoot &&
            U(_ownRoot + 0x3C) == _scene && U(_primaryRoot + 0x3C) == _scene && U(_scene) == 0x6E7358 &&
            U(_ownCharacter) == 0x6F6438 && U(_primaryCharacter) == 0x6F6438 &&
            U(_ownCharacter + 0x24) == _ownInstance && U(_primaryCharacter + 0x24) == _primaryInstance &&
            U(_ownCharacter + 0x148) == 1 && U(_primaryCharacter + 0x148) == 0 &&
            U(_ownManager) == ManagerTable && U(_primaryManager) == ManagerTable &&
            U(_ownManager + 0x24) == _ownInstance && U(_primaryManager + 0x24) == _primaryInstance &&
            U(_primaryCharacter + 0x140) == _primaryManager && U(_primaryManager + 0x124) == _primaryCharacter &&
            U(_primaryRoot + 0xC) == _primaryMetadata && U(_primaryMetadata + 8) == _primaryInstance &&
            (Word(_primaryMetadata + 4) & 0x10) != 0 && U(_ownRegion) == 0x6F7ED0 && U(_ownRegion + 0x24) == _ownInstance,
            "Ordinary role lost its distinct owned and primary native graph or generation.");
        if (!initialized) return;
        Require(_ownMetadata != 0 && _ownMetadata != _primaryMetadata && U(_ownRoot + 0xC) == _ownMetadata &&
            U(_ownMetadata + 8) == _ownInstance && U(_ownRegion + 0x18) == _ownRoot &&
            U(_ownCharacter + 0x140) == _ownManager && U(_ownManager + 0x124) == _ownCharacter &&
            U(_ownCharacter + 0x124) == _ownMachine && U(_ownMachine) == 0x6FA288,
            "Ordinary role lost its initialized own metadata/Character/PM/BloomFSM links.");
    }
    private void VerifyKnownProjectile(uint projectile, uint manager)
    {
        Require(manager == _primaryManager ? _primaryPools.Contains(projectile) :
            manager == _ownManager && _ownPools.ContainsKey(projectile), "Ordinary collision has an unbound pool object.");
        VerifyProjectile(projectile, manager, expectedRole: 1);
        Require(Enumerable.Range(0, 18).Count(i => U(manager + 0x1D0 + (uint)i * 4) == projectile) == 1,
            "Known ordinary projectile no longer belongs to its exact native pool.");
    }
    private void VerifyProjectile(uint projectile, uint manager, uint? expectedRole)
    {
        Require(U(projectile) == ProjectileTable && U(projectile + 0x18) == manager &&
            U(Pointer(projectile + 0x14) + 0x3C) == _scene &&
            (expectedRole is null || U(projectile + 0x50) == expectedRole),
            "Ordinary projectile table/source/node/role differs from the finite native pool.");
    }
    private bool OutsideBloomTrees(uint node)
    {
        var seen = new HashSet<uint>();
        for (; node != 0; node = U(node + 0x2C))
        {
            Require(node >= 0x10000 && seen.Count < 128 && seen.Add(node),
                "Ordinary clone parent chain is cyclic or exceeds its finite bound.");
            if (node == _ownRoot || node == _primaryRoot) return false;
        }
        return true;
    }

    private delegate void Stop(uint tid, ref Win32Native.X86Context context);
    private void Hook(uint address, string hex, string name, Stop action)
    {
        byte[] expected = Convert.FromHexString(hex); Pin(address, hex);
        Require(!_host.Breakpoints.Contains(address), "Ordinary policy hook already has an owner; multicast is not supported.");
        BreakpointCallback repeat = null!;
        repeat = (uint tid, uint at, ref Win32Native.X86Context c) =>
        {
            Ensure(tid); if (_retired) return;
            try
            {
                Require(Active && _ownsGeneration() && at == address && c.Eip == at &&
                    ReadCode(at, expected.Length).SequenceEqual(expected), "Ordinary policy original instruction/lifetime pin differs.");
                Add("callbacks.total", 100000); action(tid, ref c);
            }
            catch { _failed = true; throw; }
            if (Active) _host.Breakpoints.AddReturn(address, tid, "OrdinaryPlayerProjectileRole." + name, repeat);
        };
        _host.Breakpoints.AddReturn(address, _host.MainThreadId, "OrdinaryPlayerProjectileRole." + name, repeat);
    }
    private void Pin(uint address, string hex)
    {
        byte[] expected = Convert.FromHexString(hex);
        Require(Image(address, expected.Length).SequenceEqual(expected) && ReadCode(address, expected.Length).SequenceEqual(expected),
            $"Ordinary policy source/live pin differs at {Hex(address)}.");
    }
    private byte[] ReadCode(uint address, int count)
    {
        byte[] actual = Read(address, count), original = Image(address, count);
        for (int i = 0; i < count; ++i)
            if (actual[i] == 0xCC && original[i] != 0xCC && _host.Breakpoints.Contains(address + (uint)i)) actual[i] = original[i];
        return actual;
    }
    private void Add(string name, uint maximum)
    { uint count = _counts.GetValueOrDefault(name) + 1; Require(count <= maximum, "Ordinary policy finite event budget exceeded: " + name); _counts[name] = count; }
    private void Sample(string name, object data)
    { if (_samples.Count >= 128) return; _samples.Add(new { name, data }); _log(new { @event = "ordinary-player-projectile-role", name, data }); }
    private byte[] Image(uint address, int count) => _image.GetSectionData(checked((int)(address - 0x400000))).GetContent(0, count).ToArray();
    private uint Pointer(uint address) { uint value = U(address); Require(value >= 0x10000, "Ordinary role native pointer is null."); return value; }
    private uint U(uint address) => BitConverter.ToUInt32(Read(address, 4));
    private ushort Word(uint address) => BitConverter.ToUInt16(Read(address, 2));
    private byte Byte(uint address) => Read(address, 1)[0];
    private byte[] Read(uint address, int count)
    { Ensure(_host.MainThreadId); Require(address >= 0x10000 && count is > 0 and <= 512, "Ordinary role read exceeds its bound."); return Win32Native.ReadMemory(_host.ProcessHandle, address, count); }
    private void Ensure(uint tid) => Require(_host.IsAtDebugStop && tid == _host.MainThreadId &&
        Environment.CurrentManagedThreadId == _ownerThread, "Ordinary role requires its owned main-thread native debug stop.");
    private static string Hex(uint value) => $"0x{value:X8}";
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidDataException(message); }
}
