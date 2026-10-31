using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>Read-only admission of separate scoped and explicit retained manager lifetimes.</summary>
internal sealed partial class NativeTemplateProbe
{
    private const uint ProjectileManagerTable = 0x6F73D8;
    private bool _projectileManagerFixture, _projectileBattleInit, _projectileRetainedAlfea, _projectileRetainedBattle;
    private bool ProjectileManagerRetained => _projectileRetainedAlfea || _projectileRetainedBattle;
    private bool ProjectileManagerInputRetired => _projectileRetainedBattle ? OrdinaryAttackFixtureRetired :
        _networkInputFixture ? NetworkInputFixtureRetired : BodyInputFixtureRetired;
    private uint ProjectileExpectedRole => _projectileRetainedBattle ? 1u : 0u;
    private uint ProjectileFixtureLevel => _projectileBattleInit ? 47u : 28u;
    private uint ProjectileInitC3 => _projectileBattleInit ? 0x50A0DCu : 0x50980Cu;
    private uint ProjectileInitAl => _projectileBattleInit ? 1u : 0u;
    private uint _projectileManager, _projectilePrimaryManager, _projectilePrimaryMove, _projectileEventManager;
    private uint _projectileConstructedCaptures, _projectileInitEntries, _projectileInitReturns;
    private uint _projectileInitEsp, _projectileInitCaller, _projectileInitC3;
    private uint _projectileDeleteEntries, _projectileDeleteReturns, _projectileDeleteEsp, _projectileDeleteCaller;
    private uint _projectileDeleteC3, _projectileBodyEntries, _projectileBodyReturns, _projectileBodyEsp;
    private uint _projectileBodyC3, _projectileRegister8, _projectileUnregister8, _projectileUnregister8Returned;
    private bool _projectileInitPending, _projectileDeletePending, _projectileBodyPending, _projectileDeleteVerified;
    private ProjectilePrimarySnapshot? _projectilePrimarySnapshot;
    private ProjectilePrimarySnapshot? _projectileRetainedDeletePrimarySnapshot;
    private uint _projectileRetainedDeleteBaselineCaptures, _projectileSceneChildrenBeforeRetainedDelete;
    private object? _projectileInitializedWitness;
    private readonly HashSet<uint> _projectileForeignObjects = [], _projectileBattlePools = [];
    private readonly HashSet<uint> _projectileBattleNodes = [], _projectileBattleActors = [];
    private readonly HashSet<uint> _projectileDeletedPools = [], _projectileUnregisteredPools = [];
    private readonly List<object> _projectileBattlePoolWitnesses = [], _projectileBattleResourceWitnesses = [];
    private uint _projectilePoolDeleteEntries, _projectilePoolDeleteReturns, _projectilePoolDeleteC3;
    private uint _projectileDeletingPool, _projectilePoolDeleteEsp, _projectilePoolDeleteCaller;
    private uint _projectileSceneChildrenBefore, _projectileSceneChildrenInitialized, _projectileSceneChildrenDeleted;
    private bool _projectilePoolDeletePending, _projectilePoolRet4Seen;

    private sealed record ProjectilePrimarySnapshot(byte[] CharacterLinks, byte[] ManagerFields, byte[] MovePrefix);

    private bool ProjectileManagerDeleteIntervalQualified => ProjectileManagerRetained
        ? _sharedBattleUnloadStarted && _stage == 7 && !_dependentInterval && !_idleActive &&
            _projectileRetainedDeleteBaselineCaptures == 1 && _projectileRetainedDeletePrimarySnapshot is not null &&
            ProjectileManagerInputRetired
        : _dependentInterval && _stage == 6;

    private bool ProjectileManagerFixtureSucceeded => !_projectileManagerFixture ||
        _projectileDeleteVerified && _projectileConstructedCaptures == 2 &&
        _projectileInitEntries == 1 && _projectileInitReturns == 1 && _projectileInitC3 == 1 &&
        _projectileDeleteEntries == 1 && _projectileDeleteReturns == 1 && _projectileDeleteC3 == 1 &&
        _projectileBodyEntries == 1 && _projectileBodyReturns == 1 && _projectileBodyC3 == 1 &&
        _projectileRegister8 == 1 && _projectileUnregister8 == 1 && _projectileUnregister8Returned == 1 &&
        !_projectileInitPending && !_projectileDeletePending && !_projectileBodyPending &&
        (!ProjectileManagerRetained || _projectileRetainedDeleteBaselineCaptures == 1 &&
            _projectileRetainedDeletePrimarySnapshot is not null) &&
        (!_projectileRetainedAlfea || !_projectileBattleInit) &&
        (!_projectileRetainedBattle || _projectileBattleInit) &&
        (!_projectileBattleInit || _projectileBattlePools.Count == 18 && _projectileDeletedPools.Count == 18 &&
            _projectilePoolDeleteEntries == 18 && _projectilePoolDeleteReturns == 18 && _projectilePoolDeleteC3 == 18 &&
            _projectileUnregisteredPools.Count == 18 && !_projectilePoolDeletePending);

    private object ProjectileManagerSummary => new
    {
        enabled = _projectileManagerFixture, scopedInitOnly = !ProjectileManagerRetained,
        retainedAlfea = _projectileRetainedAlfea, retainedOrdinaryBattle = _projectileRetainedBattle,
        battleFullInit = _projectileBattleInit, expectedNativeProjectileRole50 = ProjectileExpectedRole,
        level = ProjectileFixtureLevel, expectedInitC3 = Hex(ProjectileInitC3), expectedInitAl = ProjectileInitAl,
        manager = Hex(_projectileManager), primaryManager = Hex(_projectilePrimaryManager),
        borrowedPrimaryMove = Hex(_projectilePrimaryMove), constructionCaptures = _projectileConstructedCaptures,
        initEntries = _projectileInitEntries, initReturns = _projectileInitReturns, initC3Witnesses = _projectileInitC3,
        deletingEntries = _projectileDeleteEntries, deletingReturns = _projectileDeleteReturns,
        deletingRet4Witnesses = _projectileDeleteC3, destructorBodyEntries = _projectileBodyEntries,
        destructorBodyReturns = _projectileBodyReturns, destructorBodyC3Witnesses = _projectileBodyC3,
        nativeRegister8Callsites = _projectileRegister8, nativeUnregister8Callsites = _projectileUnregister8,
        nativeUnregister8Continuations = _projectileUnregister8Returned,
        initialized = _projectileInitializedWitness, deleted = _projectileDeleteVerified,
        battlePoolObjects = _projectileBattlePoolWitnesses, battleResources = _projectileBattleResourceWitnesses,
        poolDeletingEntries = _projectilePoolDeleteEntries, poolDeletingReturns = _projectilePoolDeleteReturns,
        poolRet4Witnesses = _projectilePoolDeleteC3, nativePoolChannel15UnregisterCalls = _projectileUnregisteredPools.Count,
        sceneParentChildrenBeforeCreate = _projectileSceneChildrenBefore,
        sceneParentChildrenAfterInit = _projectileSceneChildrenInitialized,
        sceneParentChildrenAfterDelete = _projectileSceneChildrenDeleted,
        sceneParentChildCountRestored = ProjectileManagerRetained ? (bool?)null :
            _projectileSceneChildrenBefore == _projectileSceneChildrenDeleted,
        sceneParentChildCountComparisonScope = ProjectileManagerRetained ? "record-retained-deltas-only" : "scoped-init-delete",
        retainedDeleteBaselineCaptures = _projectileRetainedDeleteBaselineCaptures,
        sceneParentChildrenBeforeRetainedDelete = _projectileRetainedDeleteBaselineCaptures == 0 ? (uint?)null :
            _projectileSceneChildrenBeforeRetainedDelete,
        retainedGameplaySceneParentChildDelta = _projectileRetainedDeleteBaselineCaptures == 0 ? (long?)null :
            (long)_projectileSceneChildrenBeforeRetainedDelete - _projectileSceneChildrenBefore,
        retainedDeleteSceneParentChildDelta = !ProjectileManagerRetained || !_projectileDeleteVerified ? (long?)null :
            (long)_projectileSceneChildrenDeleted - _projectileSceneChildrenBeforeRetainedDelete,
        wholePrimaryByteComparisonAcrossGameplay = false,
        resourceSubgraphsFullyTraversed = false, sharedResourceCacheByteEqualityRequired = false,
        nativeRegistryContentQualified = false, wholeMoveObjectCompared = false,
        noOwnGraphReadAfterDelete = true, gameplayOrTargetingQualified = false,
        adapterGameFieldWrites = 0, passed = ProjectileManagerFixtureSucceeded
    };

    private void ConfigureProjectileManagerFixture(ProbeConfiguration config)
    {
        _projectileManagerFixture = config.TemplateProjectileManagerFixture;
        Require(new[] { config.TemplateBodyInputFixture, config.TemplateOrdinaryAttackFixture, config.TemplateNetworkInputFixture }.Count(x => x) <= 1 &&
            (!(config.TemplateBodyInputFixture || config.TemplateOrdinaryAttackFixture || config.TemplateNetworkInputFixture) || _projectileManagerFixture),
            "Retained body input and ordinary Battle attacks require distinct projectile-manager fixtures.");
        if (!_projectileManagerFixture) return;
        _projectileRetainedAlfea = config.TemplateBodyInputFixture || config.TemplateNetworkInputFixture;
        _projectileRetainedBattle = config.TemplateOrdinaryAttackFixture;
        _projectileBattleInit = config.TemplateSharedPlayerProfile == "battle-01-projectile-init" || _projectileRetainedBattle;
        bool admittedCohort = _projectileRetainedBattle
            ? config.Mode == "template-bloom-body-hair-idle" && config.TemplateSharedPlayerProfile == "battle-01-ordinary-attack" &&
                config.TemplatePlayerTraitsPickupFx && config.TemplateConfigureTargetRegion && config.TemplateSharedHealthAlias &&
                config.TemplateHealthBattleFixture && config.TemplateSharedBattleLifetime && config.TemplateBattleCombatFixture &&
                config.PrepareFirstMagicStage
            : _projectileRetainedAlfea
            ? config.Mode == "template-bloom-body-hair-idle" &&
                (config.TemplateBodyInputFixture && config.TemplateSharedPlayerProfile == "alfea-28-input" ||
                 config.TemplateNetworkInputFixture && config.TemplateSharedPlayerProfile == "alfea-28-network-input") &&
                config.TemplatePlayerTraitsPickupFx && config.TemplateConfigureTargetRegion && config.TemplateSharedHealthAlias
            : config.Mode == "template-bloom-body-hair-init" && !config.TemplatePlayerTraitsPickupFx &&
                !config.TemplateConfigureTargetRegion && !config.TemplateSharedHealthAlias &&
                (config.TemplateSharedPlayerProfile is null || _projectileBattleInit);
        Require(admittedCohort && config.TemplatePlayerTraitsFixture &&
            config.TemplateTargetRegionFixture && (!config.TemplateHealthBattleFixture || _projectileRetainedBattle) &&
            (!config.TemplateBattleCombatFixture || _projectileRetainedBattle) && (!config.PrepareFirstMagicStage || _projectileRetainedBattle) &&
            !config.TemplateSecondAttackDegreeFixture && (!config.TemplateSharedBattleLifetime || _projectileRetainedBattle) &&
            !config.TemplateDeathBattleFixture && !config.TemplateStoryDeathFixture &&
            !config.TemplateCollisionDetectorSecondaryRole && !config.TemplateStingerSecondaryRole &&
            !config.TemplateStarSecondaryEligibility && !config.TemplateStarPlacementFixture &&
            !config.TemplateSharedMagicEnergyAlias,
            "Projectile manager admission requires its separate scoped, retained Alfea-input or ordinary Battle-attack fixture.");
    }

    private void CaptureProjectileManagerBaseline()
    {
        if (!_projectileManagerFixture) return;
        Require((_stage == 0 && !_dependentInterval || _stage == 6 && _dependentInterval) &&
            _instance == 0 && _constructing == 0 && _projectileManager == 0 && _level == ProjectileFixtureLevel,
            "Projectile baseline is outside the initial or stopped pre-Create scene boundary.");
        Table(_active, _projectileBattleInit ? 0x7105C8u : 0x710874u);
        uint manager = Pointer(_localCharacter + 0x140), move = Pointer(_localCharacter + 0x12C);
        Table(manager, ProjectileManagerTable); Table(move, 0x6F64F8);
        Require(U(manager + 0x24) == _record && U(manager + 0x124) == _localCharacter &&
            U(manager + 0x130) == move && U(move + 0x160) == _localCharacter,
            "Primary projectile manager/Move relationships are not the native local graph.");
        if (_projectilePrimaryManager != 0)
            Require(manager == _projectilePrimaryManager && move == _projectilePrimaryMove,
                "Primary projectile identities changed before the private lifecycle.");
        _projectilePrimaryManager = manager; _projectilePrimaryMove = move;
        _projectileEventManager = Pointer(0x75537C);
        if (_stage == 6)
        {
            Require(_projectilePrimarySnapshot is null, "Projectile stopped baseline was captured twice.");
            _projectilePrimarySnapshot = ReadProjectilePrimarySnapshot();
            _projectileSceneChildrenBefore = U(_parent + 0x1C);
            if (_projectileBattleInit) CaptureProjectileForeignObjects();
        }
        _log(new { @event = "template-projectile-baseline", manager = Hex(manager), move = Hex(move),
            eventManager = Hex(_projectileEventManager), stoppedBeforeCreate = _stage == 6,
            primaryManagerBytes = 0x40C - 0x124, primaryMoveBytes = 0x2D4 - 0x124,
            byteComparisonAcrossNativeWorlds = false, nativeRegistryContentQualified = false });
    }

    private ProjectilePrimarySnapshot ReadProjectilePrimarySnapshot()
    {
        Require(U(_profile + 0x2B4) == _record && U(_localCharacter + 0x140) == _projectilePrimaryManager &&
            U(_localCharacter + 0x12C) == _projectilePrimaryMove && U(0x75537C) == _projectileEventManager,
            "Projectile fixture changed a canonical primary or event-manager identity.");
        Table(_projectilePrimaryManager, ProjectileManagerTable); Table(_projectilePrimaryMove, 0x6F64F8);
        return new(Read(_localCharacter + 0x124, 0x3C),
            Read(_projectilePrimaryManager + 0x124, 0x40C - 0x124),
            Read(_projectilePrimaryMove + 0x124, 0x2D4 - 0x124));
    }

    private void VerifyProjectilePrimarySnapshot(string boundary)
    {
        bool retainedDelete = ProjectileManagerRetained && ProjectileManagerDeleteIntervalQualified;
        Require(!ProjectileManagerRetained || retainedDelete ||
            _dependentInterval && _stage == 6 && !_sharedBattleUnloadStarted && _projectileRetainedDeleteBaselineCaptures == 0,
            "Retained projectile primary bytes may only be compared within the stopped init or prepared delete interval.");
        ProjectilePrimarySnapshot before = (retainedDelete ? _projectileRetainedDeletePrimarySnapshot : _projectilePrimarySnapshot) ??
            throw new InvalidDataException("Missing stopped projectile pre-Create baseline.");
        ProjectilePrimarySnapshot current = ReadProjectilePrimarySnapshot();
        Require(current.CharacterLinks.SequenceEqual(before.CharacterLinks) &&
            current.ManagerFields.SequenceEqual(before.ManagerFields) && current.MovePrefix.SequenceEqual(before.MovePrefix),
            $"Projectile fixture changed the primary graph during the stopped lifecycle at {boundary}.");
    }

    // Root calls once after input retirement at the verified original unload
    // entry, before its stage7 deletion batch. Gameplay can legitimately change
    // primary bytes; capture a fresh baseline without comparing the init one.
    private void PrepareProjectileManagerRetainedDeleteBaseline()
    {
        if (!_projectileManagerFixture || !ProjectileManagerRetained) return;
        Require(_host.IsAtDebugStop && _idle && _idleActive && !_dependentInterval && _stage == 6 &&
            _sharedBattleLifetimeEnabled && _sharedBattleCaptured && !_sharedBattleUnloadStarted &&
            _sharedPlayerProfile.Name == (_projectileRetainedBattle ? "battle-01-ordinary-attack" :
                _networkInputFixture ? "alfea-28-network-input" : "alfea-28-input") &&
            _level == ProjectileFixtureLevel && _projectileBattleInit == _projectileRetainedBattle &&
            ProjectileManagerInputRetired && _projectileRetainedDeleteBaselineCaptures == 0 &&
            _projectileRetainedDeletePrimarySnapshot is null && _projectileInitializedWitness is not null &&
            _projectileConstructedCaptures == 2 && _projectileInitEntries == 1 && _projectileInitReturns == 1 &&
            !_projectileInitPending && !_projectileDeletePending && !_projectileBodyPending && !_projectileDeleteVerified &&
            _instance != 0 && _projectileManager != 0 && U(_character + 0x140) == _projectileManager,
            "Retained projectile deletion lacks its retired-input stopped pre-unload boundary.");
        Table(_projectileManager, ProjectileManagerTable);
        Require(U(_projectileManager + 0x24) == _instance && U(_projectileManager + 0x18) == _ownRoot &&
            U(_projectileManager + 0x124) == _character && U(_projectileManager + 0x130) == _projectilePrimaryMove,
            "Retained projectile ownership changed before its stopped deletion baseline.");
        if (_projectileRetainedBattle) VerifyRetainedBattlePoolDeleteOwnership();
        else Require(Read(_projectileManager + 0x1D0, 42 * 4).All(value => value == 0) &&
            new uint[] { 0x360, 0x364, 0x368, 0x39C, 0x3A0, 0x3A4, 0x3A8, 0x3E8, 0x3EC, 0x3F0, 0x3F4, 0x400, 0x404 }
                .All(offset => U(_projectileManager + offset) == 0),
            "Retained Alfea projectile ownership changed or allocated an unqualified pool/FX graph.");
        _projectileRetainedDeletePrimarySnapshot = ReadProjectilePrimarySnapshot();
        _projectileSceneChildrenBeforeRetainedDelete = U(_parent + 0x1C);
        ++_projectileRetainedDeleteBaselineCaptures;
        _log(new { @event = "template-projectile-before-retained-delete", manager = Hex(_projectileManager),
            primaryManager = Hex(_projectilePrimaryManager), primaryMove = Hex(_projectilePrimaryMove),
            freshStoppedPrimaryBaseline = true, comparedWithInitBytes = false,
            bodyInputRetired = _bodyInputFixture && BodyInputFixtureRetired,
            networkInputRetired = _networkInputFixture && NetworkInputFixtureRetired,
            ordinaryAttackRetired = _projectileRetainedBattle, allOwnPoolsUnallocated = !_projectileRetainedBattle,
            retainedBattlePools = _projectileBattlePools.Count, expectedNativeProjectileRole50 = ProjectileExpectedRole,
            sceneParentChildren = _projectileSceneChildrenBeforeRetainedDelete,
            sceneParentChildDeltaSinceBeforeCreate = (long)_projectileSceneChildrenBeforeRetainedDelete - _projectileSceneChildrenBefore,
            sceneParentChildRestorationInferred = false });
    }

    // One stopped pre-unload ownership check, never a per-frame pool inventory.
    // Active flags/positions can change through real gameplay and are not
    // compared with init; only the eighteen original owning pool identities.
    private void VerifyRetainedBattlePoolDeleteOwnership()
    {
        Require(_projectileRetainedBattle && _projectileBattleInit && _projectileBattlePools.Count == 18 &&
            _projectileDeletedPools.Count == 0 && !_projectilePoolDeletePending,
            "Retained Battle pool deletion lacks its original initialized ownership set.");
        byte[] pools = Read(_projectileManager + 0x1D0, 42 * 4);
        uint[] first = Enumerable.Range(0, 18).Select(index => BitConverter.ToUInt32(pools, index * 4)).ToArray();
        Require(first.Distinct().Count() == 18 && _projectileBattlePools.SetEquals(first) &&
            Enumerable.Range(18, 24).All(index => BitConverter.ToUInt32(pools, index * 4) == 0),
            "Retained Battle manager changed its original eighteen pool identities or trailing slots.");
        foreach (uint projectile in first)
        {
            Table(projectile, 0x6F6B04);
            Require(U(projectile + 0x18) == _projectileManager && U(projectile + 0x50) == ProjectileExpectedRole,
                "Retained Battle pool owner/native role changed before deletion.");
        }
        _log(new { @event = "template-projectile-before-retained-battle-pool-delete", pools = first.Select(Hex).ToArray(),
            ownersVerified = 18, expectedNativeRole50 = ProjectileExpectedRole, activeStateComparedWithInit = false,
            oneStoppedBoundaryOnly = true, perFramePoolInventory = false });
    }

    private void CaptureProjectileManagerConstructed()
    {
        if (!_projectileManagerFixture) return;
        Require(_dependentInterval && _stage == 6 && _level == ProjectileFixtureLevel && (!_idle || ProjectileManagerRetained) &&
            _projectilePrimarySnapshot is not null && !_projectileDeletePending && !_projectileDeleteVerified,
            "Projectile construction witness escaped its stopped initialization cohort.");
        uint manager = Components(_instance, DescriptorCount).Single(c => c.ClassId == 0x551015C7).Address;
        Table(manager, ProjectileManagerTable);
        Require(manager != _projectilePrimaryManager && U(manager + 0x24) == _instance &&
            U(manager + 0x18) == _ownRoot && U(_character + 0x140) == manager &&
            (_projectileManager == 0 || _projectileManager == manager) && ++_projectileConstructedCaptures <= 2,
            "Native Character did not bind the unique own projectile manager.");
        _projectileManager = manager;
        VerifyProjectilePrimarySnapshot("constructed");
        _log(new { @event = "template-projectile-constructed", manager = Hex(manager), ownInstance = Hex(_instance),
            character140 = Hex(U(_character + 0x140)), manager124 = Hex(U(manager + 0x124)),
            manager130 = Hex(U(manager + 0x130)), captures = _projectileConstructedCaptures });
    }

    private void CaptureProjectileManagerInitialized()
    {
        if (!_projectileManagerFixture) return;
        Require(_dependentInterval && _stage == 6 && _instance != 0 && _level == ProjectileFixtureLevel &&
            _projectileConstructedCaptures == 2 && _projectileInitEntries == 1 && _projectileInitReturns == 1 &&
            _projectileInitC3 == 1 && !_projectileInitPending && _projectileRegister8 == 1,
            "Projectile initialized witness lacks its exact native scene-specific return.");
        VerifyProjectilePrimarySnapshot("initialized");
        Table(_projectileManager, ProjectileManagerTable);
        Require(U(_character + 0x140) == _projectileManager && U(_projectileManager + 0x124) == _character &&
            U(_projectileManager + 0x130) == _projectilePrimaryMove &&
            Read(_projectileManager + 0x348, 1)[0] == 1,
            "Projectile manager owner or borrowed Move differs from the native init graph.");
        // Native stores: 5097B3 ->334(hand_right), 5097CF ->33C(hand_left),
        // 5097E8 ->338(hand_right). Memory order differs from lookup order.
        uint right = Pointer(_projectileManager + 0x334), otherRight = Pointer(_projectileManager + 0x338),
            left = Pointer(_projectileManager + 0x33C);
        _log(new { @event = "template-projectile-emitter-bindings", right334 = Hex(right),
            right338 = Hex(otherRight), left33C = Hex(left),
            lookup334 = "hand_right", lookup338 = "hand_right", lookup33C = "hand_left" });
        Require(right == otherRight && right != left, "Projectile emitters do not match right/right/left native bindings.");
        uint[] rightAncestry = ProjectileEmitterAncestry(right), leftAncestry = ProjectileEmitterAncestry(left);
        byte[] pools = Read(_projectileManager + 0x1D0, 7 * 6 * 4);
        if (_projectileBattleInit) CaptureBattleProjectileResources(pools);
        else Require(pools.All(value => value == 0) &&
            new uint[] { 0x360, 0x364, 0x368, 0x39C, 0x3A0, 0x3A4, 0x3A8, 0x3E8, 0x3EC, 0x3F0, 0x3F4, 0x400, 0x404 }
                .All(offset => U(_projectileManager + offset) == 0),
            "Alfea projectile init unexpectedly allocated a pool or active FX graph.");
        _projectileInitializedWitness = new { manager = Hex(_projectileManager), owner = Hex(_instance),
            character = Hex(_character), borrowedPrimaryMove = Hex(_projectilePrimaryMove),
            right = Hex(right), left = Hex(left), rightAncestry = rightAncestry.Select(Hex).ToArray(),
            leftAncestry = leftAncestry.Select(Hex).ToArray(), initReturnAl = ProjectileInitAl,
            poolPointers = 42, allPoolsUnallocated = !_projectileBattleInit,
            ordinaryPoolCount = _projectileBattlePools.Count, nativeRegistryFlag348 = 1,
            primaryUnchangedInStoppedInterval = true, originalSchedulerResumed = false,
            attackAndTargetingQualified = false, nativeRegistryContentQualified = false };
        _log(new { @event = "template-projectile-initialized", witness = _projectileInitializedWitness });
        _projectileSceneChildrenInitialized = U(_parent + 0x1C);
    }

    private uint[] ProjectileEmitterAncestry(uint emitter)
    {
        var nodes = new List<uint>(); var visited = new HashSet<uint>();
        for (uint node = emitter; ; node = Pointer(node + 0x2C))
        {
            Require(node >= 0x10000 && nodes.Count < 128 && visited.Add(node) && !_localNodes.Contains(node) &&
                U(node + 0x3C) == _scene, "Projectile emitter is outside the bounded own scene subtree.");
            nodes.Add(node);
            if (node == _ownRoot) return nodes.ToArray();
        }
    }

    private void CaptureProjectileForeignObjects()
    {
        Require(_projectileBattleInit && _dependentInterval && _projectileForeignObjects.Count == 0,
            "Battle projectile foreign-object baseline must be captured once before Create.");
        _projectileForeignObjects.UnionWith(_localNodes);
        _projectileForeignObjects.UnionWith(new[] { _projectilePrimaryManager, _projectilePrimaryMove, _localActor });
        for (uint i = 0; i < 42; ++i)
        {
            uint value = U(_projectilePrimaryManager + 0x1D0 + i * 4);
            if (value == 0) continue;
            _projectileForeignObjects.Add(value);
            if (U(value) == 0x6F6B04)
            {
                _projectileForeignObjects.Add(U(value + 0x14));
                _projectileForeignObjects.Add(U(value + 0x20));
            }
        }
        foreach (uint offset in new uint[] { 0x278, 0x27C, 0x280, 0x284, 0x288, 0x28C, 0x290,
            0x36C, 0x370, 0x37C, 0x380, 0x38C, 0x390, 0x39C, 0x3A0, 0x3E8, 0x3EC, 0x3F4, 0x400 })
            _projectileForeignObjects.Add(U(_projectilePrimaryManager + offset));
        _projectileForeignObjects.Remove(0);
    }

    private object CaptureProjectileResourceNode(uint node, string source)
    {
        if (node == 0) return new { source, node = Hex(0), missing = true, subtreeQualified = false };
        Require(node >= 0x10000 && !_projectileForeignObjects.Contains(node) &&
            !_projectileBattlePools.Contains(node) && !_projectileBattleActors.Contains(node) &&
            _projectileBattleNodes.Add(node), "Battle projectile Node aliases an existing owner or another new root.");
        uint table = U(node), parent = U(node + 0x2C), scene = U(node + 0x3C);
        Require(table is VisualGraphReader.NodeVtable or VisualGraphReader.RenderNodeVtable &&
            scene == _scene && parent != 0 && parent != node,
            "Battle projectile resource clone is not a Node in the captured scene.");
        uint name = U(node + 0x10);
        return new { source, node = Hex(node), table = Hex(table), parent = Hex(parent), scene = Hex(scene),
            name = name == 0 ? "" : Win32Native.ReadAnsiString(_host.ProcessHandle, checked(name + 9), 128),
            references = Refs(node), childCount = U(node + 0x1C), flags = Hex(U(node + 0xB0)),
            missing = false, subtreeQualified = false, ownBloomAncestryRequired = false };
    }

    private object CaptureProjectileResourceActor(uint actor, string source)
    {
        if (actor == 0) return new { source, actor = Hex(0), missing = true };
        Require(!_projectileForeignObjects.Contains(actor) && !_projectileBattlePools.Contains(actor) &&
            !_projectileBattleNodes.Contains(actor) && _projectileBattleActors.Add(actor),
            "Battle projectile Actor aliases a primary or another new owner.");
        Table(actor, 0x703F80);
        return new { source, actor = Hex(actor), table = Hex(0x703F80), missing = false,
            internalControllerGraphQualified = false };
    }

    private void CaptureBattleProjectileResources(byte[] pools)
    {
        Require(_projectileBattleInit && _projectileBattlePools.Count == 0 &&
            _projectileForeignObjects.Count != 0 && pools.Length == 42 * 4,
            "Battle projectile graph must be observed once after native full init.");
        VisualGraphReadResult body = new VisualGraphReader(_host.ProcessHandle).Read(_ownRoot, _localNodes, inspectSkins: false);
        Require(body.Success, "Own Bloom graph could not bound forbidden projectile aliases: " + body.Error);
        _projectileForeignObjects.UnionWith(body.Graph.Nodes.Select(node => node.Address));
        for (int index = 0; index < 42; ++index)
        {
            uint projectile = BitConverter.ToUInt32(pools, index * 4);
            if (index >= 18)
            {
                Require(projectile == 0, "Battle47 allocated a pool outside its native types0..2.");
                continue;
            }
            Require(projectile >= 0x10000 && !_projectileForeignObjects.Contains(projectile) &&
                _projectileBattlePools.Add(projectile), "Battle ordinary projectile is null, repeated or borrowed.");
            Table(projectile, 0x6F6B04);
            Require(U(projectile + 0x18) == _projectileManager && Read(projectile + 0x30, 1)[0] == 0 &&
                U(projectile + 0x50) == ProjectileExpectedRole, "Battle projectile owner/inactive/native cohort role differs.");
            uint node = Pointer(projectile + 0x14), actor = Pointer(projectile + 0x20);
            object nodeWitness = CaptureProjectileResourceNode(node, $"pool{index}.clone14"),
                actorWitness = CaptureProjectileResourceActor(actor, $"pool{index}.actor20");
            _projectileBattlePoolWitnesses.Add(new { index, type = index / 6, slot = index % 6,
                projectile = Hex(projectile), table = Hex(0x6F6B04), owner18 = Hex(_projectileManager),
                clone = nodeWitness, actor = actorWitness, inactive30 = 0, nativeRole50 = ProjectileExpectedRole,
                initialDamage3C = U(projectile + 0x3C), firedOrDamageQualified = false });
        }
        foreach (uint offset in new uint[] { 0x360, 0x364, 0x368, 0x3E8, 0x3EC, 0x3F0, 0x3F4, 0x3F8, 0x3FC, 0x400, 0x404 })
            Require(U(_projectileManager + offset) == 0,
                "Battle47 unexpectedly initialized a special projectile or dragon graph.");
        for (uint index = 0; index < 7; ++index)
        {
            uint offset = 0x278 + index * 4, node = U(_projectileManager + offset);
            _projectileBattleResourceWitnesses.Add(CaptureProjectileResourceNode(node, $"resource{offset:X}"));
        }
        for (uint index = 0; index < 3; ++index)
        {
            uint offset = 0x36C + index * 16;
            _projectileBattleResourceWitnesses.Add(new { source = $"commonFx{offset:X}",
                node = CaptureProjectileResourceNode(U(_projectileManager + offset), $"commonFx{offset:X}.node"),
                actor = CaptureProjectileResourceActor(U(_projectileManager + offset + 4), $"commonFx{offset:X}.actor"),
                animationReference = Hex(U(_projectileManager + offset + 8)),
                activeByte = Read(_projectileManager + offset + 12, 1)[0], animationReferenceInternalsQualified = false });
        }
        _projectileBattleResourceWitnesses.Add(new { source = "blast39C",
            node = CaptureProjectileResourceNode(U(_projectileManager + 0x39C), "blast39C.node"),
            actor = CaptureProjectileResourceActor(U(_projectileManager + 0x3A0), "blast39C.actor"),
            beginAnimationReference = Hex(U(_projectileManager + 0x3A4)),
            endAnimationReference = Hex(U(_projectileManager + 0x3A8)),
            shockwaveChild3B8 = Hex(U(_projectileManager + 0x3B8)), animationReferenceInternalsQualified = false });
        _log(new { @event = "template-projectile-battle-graph", pools = _projectileBattlePoolWitnesses,
            resources = _projectileBattleResourceWitnesses, independentNodeRoots = _projectileBattleNodes.Count,
            independentActors = _projectileBattleActors.Count, untouchedTrailingPoolSlots = 24,
            missingOptionalResourcesRecorded = true, resourceSubgraphsFullyTraversed = false,
            ownedNodesAreSceneClones = true, sharedResourceCacheByteEqualityRequired = false,
            nativeNpcProjectileRoleRetained = !_projectileRetainedBattle,
            explicitOrdinaryBattleNativeRole = _projectileRetainedBattle, targetingOrGameplayQualified = false });
    }

    // Retained body input enters the health fast route before deletion. Keep
    // these finite lifecycle witnesses; the default Hook policy would retire
    // them at their first later destructor stop. Scoped cohorts keep that policy.
    private void ProjectileManagerHook(uint entry, string name, BreakpointCallback callback) =>
        Hook(entry, name, callback, preserveDuringHealthBattle: ProjectileManagerRetained);

    private void InstallProjectileManagerWitnesses()
    {
        if (!_projectileManagerFixture) return;
        ProjectileManagerHook(0x5096E0, "Projectile.init-owned", (uint tid, uint at, ref Win32Native.X86Context context) =>
        {
            if (_projectileDeleteVerified || context.Ecx != _projectileManager || _projectileManager == 0) return;
            Require(_dependentInterval && _stage == 6 && _level == ProjectileFixtureLevel && !_projectileInitPending &&
                ++_projectileInitEntries == 1 && U(context.Esp) == 0x5093F6,
                "Projectile init entry caller/count/private interval mismatch.");
            Table(context.Ecx, ProjectileManagerTable); VerifyProjectilePrimarySnapshot("init-entry");
            if (_projectileRetainedBattle) BindOrdinaryRoleBeforeInit(tid, ref context);
            _projectileInitEsp = context.Esp; _projectileInitCaller = U(context.Esp); _projectileInitPending = true;
            _host.Breakpoints.AddReturn(_projectileInitCaller, tid, "Projectile.init-owned.return",
                (uint rt, uint ra, ref Win32Native.X86Context returned) =>
                {
                    Require(_projectileInitPending && rt == tid && ra == _projectileInitCaller && returned.Eip == ra &&
                        returned.Esp == checked(_projectileInitEsp + 4) && returned.Esi == _projectileManager &&
                        _projectileInitC3 == 1 && (returned.Eax & 255) == ProjectileInitAl && ++_projectileInitReturns == 1,
                        "Projectile init C3/caller/receiver/thread/ESP/AL pairing failed.");
                    _projectileInitPending = false; VerifyProjectilePrimarySnapshot("init-return");
                    _log(new { @event = "template-projectile-init-return", manager = Hex(_projectileManager),
                        caller = Hex(ra), postRetEsp = Hex(returned.Esp), c3 = Hex(ProjectileInitC3), al = ProjectileInitAl,
                        exactPair = true, unexpectedNestedReturnAccepted = false });
                });
        });
        ProjectileManagerHook(ProjectileInitC3, "Projectile.init-owned.scene.C3", (uint tid, uint at, ref Win32Native.X86Context context) =>
        {
            if (!_projectileInitPending || context.Esi != _projectileManager) return;
            Require(context.Esp == _projectileInitEsp && U(context.Esp) == _projectileInitCaller &&
                (context.Eax & 255) == ProjectileInitAl && ++_projectileInitC3 == 1,
                "Own projectile init did not reach its scene-specific exact C3 frame.");
        });
        ProjectileManagerHook(0x50972B, "Projectile.init-owned.register8", (uint tid, uint at, ref Win32Native.X86Context context) =>
        {
            if (!_projectileInitPending || context.Esi != _projectileManager) return;
            Require(context.Ecx == _projectileEventManager && U(context.Esp) == 8 &&
                U(context.Esp + 4) == _projectileManager && ++_projectileRegister8 == 1,
                "Own projectile channel8 registration callsite mismatch.");
        });
        ProjectileManagerHook(0x50A580, "Projectile.delete-owned", (uint tid, uint at, ref Win32Native.X86Context context) =>
        {
            if (_projectileDeleteVerified || context.Ecx != _projectileManager || _projectileManager == 0) return;
            Require(ProjectileManagerDeleteIntervalQualified && !_projectileInitPending &&
                _projectileInitializedWitness is not null && !_projectileDeletePending &&
                U(context.Esp + 4) == 1 && ++_projectileDeleteEntries == 1,
                "Projectile deleting destructor escaped its initialized scoped or prepared retained-delete lifetime.");
            Table(context.Ecx, ProjectileManagerTable); VerifyProjectilePrimarySnapshot("delete-entry");
            _projectileDeletePending = true; _projectileDeleteEsp = context.Esp; _projectileDeleteCaller = U(context.Esp);
            _host.Breakpoints.AddReturn(_projectileDeleteCaller, tid, "Projectile.delete-owned.return",
                (uint rt, uint ra, ref Win32Native.X86Context returned) =>
                {
                    Require(_projectileDeletePending && !_projectileBodyPending && rt == tid &&
                        ra == _projectileDeleteCaller && returned.Eip == ra &&
                        returned.Esp == checked(_projectileDeleteEsp + 8) && returned.Eax == _projectileManager &&
                        _projectileDeleteC3 == 1 && _projectileBodyReturns == 1 && ++_projectileDeleteReturns == 1,
                        "Projectile deleting RET4/caller/thread/ESP pairing failed.");
                    _projectileDeletePending = false; VerifyProjectilePrimarySnapshot("delete-return");
                    _log(new { @event = "template-projectile-delete-return", historicalManager = Hex(_projectileManager),
                        caller = Hex(ra), postRetEsp = Hex(returned.Esp), exactPair = true, freedGraphRead = false });
                });
        });
        ProjectileManagerHook(0x5092D0, "Projectile.destructor-owned.body", (uint tid, uint at, ref Win32Native.X86Context context) =>
        {
            if (!_projectileDeletePending || context.Ecx != _projectileManager) return;
            Require(!_projectileBodyPending && ++_projectileBodyEntries == 1 && U(context.Esp) == 0x50A588,
                "Projectile destructor body has an unexpected caller or nesting.");
            _projectileBodyPending = true; _projectileBodyEsp = context.Esp;
            _host.Breakpoints.AddReturn(0x50A588, tid, "Projectile.destructor-owned.body.return",
                (uint rt, uint ra, ref Win32Native.X86Context returned) =>
                {
                    Require(_projectileBodyPending && rt == tid && ra == 0x50A588 && returned.Eip == ra &&
                        returned.Esp == checked(_projectileBodyEsp + 4) && returned.Esi == _projectileManager &&
                        _projectileBodyC3 == 1 && ++_projectileBodyReturns == 1,
                        "Projectile destructor body C3/receiver/thread/ESP pairing failed.");
                    _projectileBodyPending = false; VerifyProjectilePrimarySnapshot("destructor-body-return");
                });
        });
        ProjectileManagerHook(0x5093B4, "Projectile.destructor-owned.body.C3", (uint tid, uint at, ref Win32Native.X86Context context) =>
        {
            if (!_projectileBodyPending || context.Esi != _projectileManager) return;
            Require(context.Esp == _projectileBodyEsp && U(context.Esp) == 0x50A588 && ++_projectileBodyC3 == 1,
                "Own projectile destructor body did not reach its exact C3 frame.");
        });
        ProjectileManagerHook(0x50A59B, "Projectile.delete-owned.RET4", (uint tid, uint at, ref Win32Native.X86Context context) =>
        {
            if (!_projectileDeletePending || context.Eax != _projectileManager) return;
            Require(!_projectileBodyPending && _projectileBodyReturns == 1 &&
                context.Esp == _projectileDeleteEsp && U(context.Esp) == _projectileDeleteCaller && ++_projectileDeleteC3 == 1,
                "Own projectile deleting wrapper did not reach its exact RET4 frame.");
        });
        ProjectileManagerHook(0x50747B, "Projectile.delete-owned.unregister8", (uint tid, uint at, ref Win32Native.X86Context context) =>
        {
            if (!_projectileBodyPending || context.Esi != _projectileManager) return;
            Require(context.Ecx == _projectileEventManager && U(context.Esp) == 8 &&
                U(context.Esp + 4) == _projectileManager && ++_projectileUnregister8 == 1,
                "Own projectile channel8 unregister callsite mismatch.");
        });
        ProjectileManagerHook(0x507480, "Projectile.delete-owned.unregister8.return", (uint tid, uint at, ref Win32Native.X86Context context) =>
        {
            if (!_projectileBodyPending || context.Esi != _projectileManager) return;
            Require(_projectileUnregister8 == 1 && ++_projectileUnregister8Returned == 1,
                "Own projectile cleanup missed its expected unregister continuation.");
        });
        if (_projectileBattleInit) InstallBattleProjectileDeleteWitnesses();
    }

    private void InstallBattleProjectileDeleteWitnesses()
    {
        ProjectileManagerHook(0x4FFE80, "Projectile.pool-owned.delete", (uint tid, uint at, ref Win32Native.X86Context context) =>
        {
            if (!_projectileBodyPending || !_projectileBattlePools.Contains(context.Ecx)) return;
            Require(ProjectileManagerDeleteIntervalQualified && !_projectilePoolDeletePending &&
                U(context.Esp) == 0x5074C1 && U(context.Esp + 4) == 1 &&
                _projectileDeletedPools.Add(context.Ecx) && ++_projectilePoolDeleteEntries <= 18,
                "Battle pool deletion escaped its unique manager cleanup frame.");
            Table(context.Ecx, 0x6F6B04);
            uint projectile = context.Ecx;
            _projectileDeletingPool = projectile; _projectilePoolDeleteEsp = context.Esp;
            _projectilePoolDeleteCaller = U(context.Esp); _projectilePoolDeletePending = true;
            _projectilePoolRet4Seen = false;
            _host.Breakpoints.AddReturn(_projectilePoolDeleteCaller, tid, "Projectile.pool-owned.delete.return",
                (uint rt, uint ra, ref Win32Native.X86Context returned) =>
                {
                    Require(_projectilePoolDeletePending && _projectilePoolRet4Seen &&
                        _projectileDeletingPool == projectile && rt == tid && ra == _projectilePoolDeleteCaller &&
                        returned.Eip == ra && returned.Esp == checked(_projectilePoolDeleteEsp + 8) &&
                        returned.Eax == projectile && _projectileUnregisteredPools.Contains(projectile) &&
                        ++_projectilePoolDeleteReturns <= 18,
                        "Battle pool deleting RET4/caller/thread/ESP/unregister pairing failed.");
                    _projectilePoolDeletePending = false; _projectileDeletingPool = 0;
                    _log(new { @event = "template-projectile-pool-delete-return", historicalProjectile = Hex(projectile),
                        returnedAt = Hex(ra), postRetEsp = Hex(returned.Esp), deletingReturns = _projectilePoolDeleteReturns,
                        exactRet4Pair = true, nativeChannel15UnregisterObserved = true, freedObjectRead = false });
                });
        });
        ProjectileManagerHook(0x4FFE9B, "Projectile.pool-owned.delete.RET4", (uint tid, uint at, ref Win32Native.X86Context context) =>
        {
            if (!_projectilePoolDeletePending || context.Eax != _projectileDeletingPool) return;
            Require(!_projectilePoolRet4Seen && context.Esp == _projectilePoolDeleteEsp &&
                U(context.Esp) == _projectilePoolDeleteCaller && ++_projectilePoolDeleteC3 <= 18,
                "Battle pool did not reach its exact deleting RET4 frame.");
            _projectilePoolRet4Seen = true;
        });
        ProjectileManagerHook(0x4FF1B5, "Projectile.pool-owned.unregister15", (uint tid, uint at, ref Win32Native.X86Context context) =>
        {
            if (!_projectilePoolDeletePending || context.Esi != _projectileDeletingPool) return;
            Require(context.Ecx == _projectileEventManager && U(context.Esp) == 0x15 &&
                U(context.Esp + 4) == _projectileDeletingPool && _projectileUnregisteredPools.Add(_projectileDeletingPool),
                "Battle pool channel15 unregister source does not match its owned destructor.");
        });
    }

    private void VerifyProjectileManagerDeleted()
    {
        if (!_projectileManagerFixture) return;
        Require(_instance == 0 && !_dependentInterval && !_projectileDeleteVerified &&
            (!ProjectileManagerRetained || ProjectileManagerDeleteIntervalQualified) &&
            _projectileConstructedCaptures == 2 && _projectileInitializedWitness is not null &&
            !_projectileInitPending && !_projectileBodyPending && !_projectileDeletePending &&
            _projectileInitReturns == 1 && _projectileDeleteReturns == 1 && _projectileBodyReturns == 1 &&
            _projectileRegister8 == 1 && _projectileUnregister8 == 1 && _projectileUnregister8Returned == 1,
            "Projectile deletion lacks its paired native lifecycle or retained pre-unload witnesses.");
        Require(!_projectileBattleInit || _projectileBattlePools.Count == 18 &&
            _projectileDeletedPools.Count == 18 && _projectilePoolDeleteEntries == 18 &&
            _projectilePoolDeleteReturns == 18 && _projectilePoolDeleteC3 == 18 &&
            _projectileUnregisteredPools.Count == 18 && !_projectilePoolDeletePending,
            "Battle projectile deletion lacks all18 owned pool destructor/RET4/unregister witnesses.");
        VerifyProjectilePrimarySnapshot("after-dependent-delete");
        _projectileSceneChildrenDeleted = U(_parent + 0x1C);
        _projectileDeleteVerified = true;
        _log(new { @event = "template-projectile-delete-verified", historicalManager = Hex(_projectileManager),
            primaryUnchangedInStoppedInterval = true, freedGraphRead = false, nativeRegistryContentQualified = false,
            retainedAlfea = _projectileRetainedAlfea, retainedOrdinaryBattle = _projectileRetainedBattle,
            freshRetainedDeleteBaseline = ProjectileManagerRetained,
            primaryComparisonAcrossGameplay = false,
            sceneParentChildDeltaDuringRetainedDelete = ProjectileManagerRetained ?
                (long?)((long)_projectileSceneChildrenDeleted - _projectileSceneChildrenBeforeRetainedDelete) : null,
            sceneParentChildRestorationInferred = !ProjectileManagerRetained && _projectileSceneChildrenBefore == _projectileSceneChildrenDeleted,
            nativeChannel8UnregisterCallAndContinuation = true, adapterGameFieldWrites = 0 });
    }
}
