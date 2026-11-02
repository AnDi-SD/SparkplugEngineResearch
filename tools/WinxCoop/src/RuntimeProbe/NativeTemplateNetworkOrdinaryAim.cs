using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>Separate, opt-in diagnostic aim/source policies; not native targeting reconstruction.</summary>
internal sealed partial class NativeTemplateProbe
{
    private bool _networkOrdinaryAimFixture, _networkPrimaryCaptureOnlyFixture;
    private uint _networkOrdinaryAimSelections, _networkOrdinaryAimFallbacks;
    private uint _networkPrimaryOrdinarySkips, _networkPrimaryOrdinarySkipReturns, _networkPrimaryOrdinaryRet4s;
    private uint _networkPrimaryOrdinaryActivationEscapes, _networkPrimaryOrdinaryPoolVerifications;
    private bool _networkPrimaryOrdinaryBaselineVerified, _networkPrimaryOrdinaryFinalInactiveVerified;
    private bool _networkPrimaryOrdinaryWrapperPending, _networkPrimaryOrdinaryRetSeen;
    private uint[] _networkPrimaryOrdinaryPools = [];

    private object NetworkOrdinaryOptionalPolicySummary => new
    {
        aimFixture = _networkOrdinaryAimFixture, selections = _networkOrdinaryAimSelections,
        noEligiblePointFallbacks = _networkOrdinaryAimFallbacks,
        aimPolicy = "fresh registered typed enemy: valid shared cache20, otherwise private nearest within PM340",
        primaryCaptureOnlyFixture = _networkPrimaryCaptureOnlyFixture,
        primaryPoolBaselineVerified = _networkPrimaryOrdinaryBaselineVerified,
        primaryPoolFinalInactiveVerified = _networkPrimaryOrdinaryFinalInactiveVerified,
        primaryOrdinaryPools = _networkPrimaryOrdinaryPools.Select(Hex).ToArray(),
        primaryPoolVerifications = _networkPrimaryOrdinaryPoolVerifications,
        primaryWrapperSkips = _networkPrimaryOrdinarySkips,
        primaryNativeRet4Witnesses = _networkPrimaryOrdinaryRet4s,
        primaryWrapperCallerReturns = _networkPrimaryOrdinarySkipReturns,
        primaryActivationEscapes = _networkPrimaryOrdinaryActivationEscapes,
        primaryWrapperFramePending = _networkPrimaryOrdinaryWrapperPending,
        captureOnlyBranchExercised = _networkPrimaryOrdinarySkips != 0,
        sourceCommandWrites = 0, enemyHealthWrites = 0, syntheticHitMessages = 0,
        targetOrProfileWrites = 0, pm34CWrites = 0, e5ObjectIdentityOrWorldSnapshot = false,
        fullPrimaryCombatGameplayQualified = false, nativeRuntimeQualificationClaimed = false,
        summaryReadsHistoricalObjects = false
    };

    private void ConfigureNetworkOrdinaryOptionalPolicies(ProbeConfiguration config)
    {
        _networkOrdinaryAimFixture = config.TemplateNetworkOrdinaryAimFixture;
        _networkPrimaryCaptureOnlyFixture = config.TemplateNetworkPrimaryCaptureOnlyFixture;
        Require(!(_networkOrdinaryAimFixture || _networkPrimaryCaptureOnlyFixture) ||
            config.TemplateNetworkOrdinaryAttackFixture && config.TemplateNetworkInputFixture &&
            config.TemplateSharedPlayerProfile == "battle-01-network-input" &&
            config.Mode == "template-bloom-body-hair-idle",
            "Optional aim/capture-only policies require the separate network ordinary Battle47 cohort.");
        Require(!_networkPrimaryCaptureOnlyFixture || config.DiagnosticGuestInput is { Role: DiagnosticInputRole.Guest },
            "Primary capture-only policy is restricted to the same-game diagnostic Guest source endpoint.");
    }

    private void InstallNetworkOrdinaryOptionalPolicyPins()
    {
        if (_networkPrimaryCaptureOnlyFixture)
        {
            OrdinaryPin(0x50A221, "C20400");
            // The caller does not consume wrapper EAX/EDX/flags before replacing
            // them. Only the skipped native callee's ordinary production is lost.
            OrdinaryPin(0x4FB7F5, "8BB6240100008B8E4401000085C974238BB62C01000085F68B01");
        }
        if (_networkOrdinaryAimFixture)
        {
            OrdinaryPin(0x59D130, "8B46088A884501000084C90F84A70000008A884401000084C90F8499000000");
            OrdinaryPin(0x5A3763, "C700D0617000");
            OrdinaryPin(0x4F627C, "E8DFD40A00");
            OrdinaryPin(0x511498, "66C742040800");
            OrdinaryPin(0x5114AD, "8B56248951088B46188B8E4C01000089480C");
            OrdinaryPin(0x4FF69A, "8B465085C0750E33C98A4D0483E110894C24147413");
        }
    }

    private sealed record NetworkOrdinaryAimCandidate(uint Region, uint Node, uint Instance,
        uint Info, uint Traits, uint Character, uint Health, double DistanceSquared);

    // Invoked only by the role inside its exact stopped native508289 frame.
    // The pointer is consumed immediately by native508230 and is never stored.
    private uint ResolveNetworkOrdinaryEnemyNode()
    {
        Require(_networkOrdinaryAimFixture && _ordinaryNetwork && _ordinaryStarted && !_ordinaryRetired &&
            _ordinaryAuthorization && _stage == 6 && !_dependentInterval && !_sharedBattleUnloadStarted &&
            _host.IsAtDebugStop && _level == 47 && _networkOrdinaryAction is { Committed: true, Completed: false } &&
            ReadSharedBattleFlow(out _, out _, out _, out _, out _),
            "Enemy point resolver requires a live admitted network ordinary action.");
        VerifyNetworkOrdinaryOwners();
        uint manager = Pointer(0x765AF0);
        Require(U(manager) == 0x703954 && U(manager + 0x1C) <= 64,
            "Finite Battle aim admits at most64 registered candidates.");
        TargetRegistry registry = ReadTargetRegistry();
        Require(registry.Manager == manager && _targetRegistryInitialized is not null &&
            manager == _targetRegistryInitialized.Manager && registry.Head == _targetRegistryInitialized.Head &&
            registry.Player28 == _targetRegistryInitialized.Player28 &&
            registry.Regions.Distinct().Count() == registry.Regions.Length,
            "Enemy point resolver lost its original registry/primary player identities.");
        uint emitter = Pointer(_projectileManager + 0x334);
        float[] origin = OrdinaryPosition(emitter);
        float range = BitConverter.ToSingle(Read(_projectileManager + 0x340, 4));
        Require(float.IsFinite(range) && range > 0 && range <= 5000, "Original PM range is outside this finite aim cohort.");
        List<NetworkOrdinaryAimCandidate> eligible = [];
        foreach (uint region in registry.Regions)
        {
            if (region == _targetRegion || region == registry.Player28 || U(region) != 0x6F7ED0 ||
                ClassId(region) != 0x4307193A || Read(region + 0x144, 1)[0] == 0 ||
                Read(region + 0x145, 1)[0] == 0 || Read(region + 0x148, 1)[0] != 0) continue;
            uint instance = U(region + 0x24);
            if (instance < 0x10000 || instance == _record || instance == _instance || U(instance) != 0x710F80) continue;
            uint node = U(region + 0x18), root = Pointer(instance + 0x24);
            if (node < 0x10000 || node == _ownRoot || node == Pointer(_record + 0x24) || U(node) != 0x6DC4F4 ||
                U(node + 0x3C) != _scene || U(_scene) != 0x6E7358) continue;
            NativeTargetNodeOwnership.VerifyAtStop(_host, node, root, _scene);
            uint info = U(root + 0x0C);
            if (info < 0x10000 || U(info) != 0x7061D0 || U(info + 8) != instance) continue;
            ushort flags = BitConverter.ToUInt16(Read(info + 4, 2));
            if ((flags & 0x18) != 8 || Read(region + 0x147, 1)[0] == 0 && U(region + 0x14C) != info) continue;
            if (!TryNetworkOrdinaryAimEnemyTraits(instance, out uint traits, out uint character, out uint health)) continue;
            float[] point = OrdinaryPosition(node);
            double distance = Enumerable.Range(0, 3).Sum(i => Math.Pow((double)point[i] - origin[i], 2));
            if (!double.IsFinite(distance) || distance < .0001 || distance > (double)range * range) continue;
            eligible.Add(new(region, node, instance, info, traits, character, health, distance));
        }
        NetworkOrdinaryAimCandidate? chosen = eligible.SingleOrDefault(c => c.Region == registry.Selection20);
        bool fromSharedCache = chosen is not null;
        chosen ??= eligible.OrderBy(c => c.DistanceSquared).FirstOrDefault();
        if (chosen is null) { ++_networkOrdinaryAimFallbacks; return 0; }
        Require(++_networkOrdinaryAimSelections <= NetworkOrdinaryMaximumActions, "Enemy point selection exceeded its action budget.");
        _log(new { @event = "template-network-ordinary-aim-point", actionId = _networkOrdinaryAction!.Id,
            policy = fromSharedCache ? "validated-current-shared-cache20" : "private-nearest-registered-enemy",
            region = Hex(chosen.Region), node = Hex(chosen.Node), instance = Hex(chosen.Instance),
            info = Hex(chosen.Info), traits = Hex(chosen.Traits), health = chosen.Health,
            chosen.DistanceSquared, range, eligible = eligible.Count, registryCount = registry.Count,
            targetOrProfileWrites = 0, nativeVectorMathWillRun = true });
        return chosen.Node;
    }

    private bool TryNetworkOrdinaryAimEnemyTraits(uint instance, out uint traits, out uint character, out uint health)
    {
        traits = character = health = 0;
        uint head = Pointer(instance + 0x1C), link = Pointer(head), previous = head;
        HashSet<uint> links = [], entities = [];
        while (link != head)
        {
            Require(links.Count < 64 && links.Add(link) && U(link + 4) == previous,
                "Enemy point component list is not bounded/reciprocal.");
            uint item = Pointer(link + 8); Require(entities.Add(item), "Enemy point component identity repeated.");
            uint table = U(item);
            if (table == 0x6F67F0 && ClassId(item) == 0x1BC6108E)
            { Require(traits == 0 && U(item + 0x24) == instance, "Enemy Traits owner/uniqueness changed."); traits = item; }
            if (table == 0x6F6438 && ClassId(item) == 0x0003CC73)
            { Require(character == 0 && U(item + 0x24) == instance, "Enemy Character owner/uniqueness changed."); character = item; }
            previous = link; link = Pointer(link);
        }
        Require(U(head + 4) == previous, "Enemy point component sentinel did not match.");
        if (traits == 0 || character == 0 || U(traits + 0x18C) != character || U(character + 0x138) != traits ||
            U(U(traits) + 0x40) != 0x4FCF00 || U(Pointer(character + 0x124) + 0x130) == 8) return false;
        health = U(traits + 0x124); uint maximum = U(traits + 0x128), minimum = U(traits + 0x12C);
        return health > 0 && maximum > 0 && maximum <= 100000 && health <= maximum && minimum <= health;
    }

    private void CaptureNetworkPrimaryOrdinaryBaseline()
    {
        if (!_networkPrimaryCaptureOnlyFixture) return;
        Require(!_networkPrimaryOrdinaryBaselineVerified && _networkPrimaryOrdinaryPools.Length == 0,
            "Primary ordinary capture-only baseline repeated.");
        _networkPrimaryOrdinaryPools = Enumerable.Range(0, 18).Select(i => Pointer(_projectilePrimaryManager + 0x1D0 + (uint)i * 4)).ToArray();
        Require(_networkPrimaryOrdinaryPools.Distinct().Count() == 18, "Primary ordinary pool aliases itself.");
        VerifyNetworkPrimaryOrdinaryPools(requireInactive: true); _networkPrimaryOrdinaryBaselineVerified = true;
        _log(new { @event = "template-network-primary-capture-only-baseline", pools = _networkPrimaryOrdinaryPools.Select(Hex).ToArray(),
            allNativeInactive = true, sourceCommandUnmodified = true, sourceGameplayWeaponSuppressionIsDiagnostic = true });
    }

    private void VerifyNetworkPrimaryOrdinaryPools(bool requireInactive)
    {
        Require(_networkPrimaryCaptureOnlyFixture && _host.IsAtDebugStop && !_ordinaryRetired && _stage == 6 &&
            !_sharedBattleUnloadStarted && _networkPrimaryOrdinaryPools.Length == 18 &&
            _projectilePrimaryManager != _projectileManager && U(_projectilePrimaryManager) == 0x6F73D8 &&
            U(_projectilePrimaryManager + 0x24) == _record && U(_projectilePrimaryManager + 0x124) == _localCharacter &&
            U(_localCharacter + 0x140) == _projectilePrimaryManager && U(_localCharacter + 0x148) == 0 &&
            U(_profile + 0x2B4) == _record, "Capture-only primary PM no longer has its live original owner.");
        for (int i = 0; i < 18; ++i)
        {
            uint projectile = _networkPrimaryOrdinaryPools[i];
            Require(U(_projectilePrimaryManager + 0x1D0 + (uint)i * 4) == projectile && U(projectile) == 0x6F6B04 &&
                U(projectile + 0x18) == _projectilePrimaryManager && U(projectile + 0x50) == 1 &&
                U(Pointer(projectile + 0x14) + 0x3C) == _scene && (!requireInactive || Read(projectile + 0x30, 1)[0] == 0),
                "Capture-only primary ordinary pool changed identity/scene/role or became active.");
        }
        ++_networkPrimaryOrdinaryPoolVerifications;
    }

    // Composed into the already installed network50A210 callback. No duplicate
    // permanent hook and no source UserInput/command/FSM/animation write.
    private bool TrySuppressNetworkPrimaryOrdinaryWrapper(uint tid, ref Win32Native.X86Context context)
    {
        if (!_networkPrimaryCaptureOnlyFixture || !_ordinaryStarted || context.Ecx != _projectilePrimaryManager) return false;
        Require(!_networkPrimaryOrdinaryWrapperPending && _networkPrimaryOrdinaryBaselineVerified && !_ordinaryRetired &&
            tid == _host.MainThreadId && context.Eip == 0x50A210 && _host.IsAtDebugStop &&
            U(context.Esp) == 0x4FB7F5 && U(context.Esp + 4) == 0 &&
            ReadSharedBattleFlow(out _, out _, out _, out _, out _),
            "Primary source wrapper escaped its one-game original ordinary event frame.");
        VerifyNetworkOrdinaryOwners(); VerifyNetworkPrimaryOrdinaryPools(requireInactive: true);
        uint machine = Pointer(_localCharacter + 0x124), state = Pointer(machine + 0x15C);
        Require(U(machine + 0x124) == _localCharacter && U(machine + 0x130) == 5 && U(state) == 0x6F8340 &&
            U(state + 0x14) == machine && ++_networkPrimaryOrdinarySkips <= 64,
            "Primary source wrapper is not its original state5 event or exceeds the finite skip budget.");
        OrdinaryNativeFrame frame = OrdinaryFrame(context);
        Require(!_host.Breakpoints.Contains(0x50A221),
            "Capture-only source cannot redirect into another pending RET4 breakpoint owner.");
        _networkPrimaryOrdinaryWrapperPending = true; _networkPrimaryOrdinaryRetSeen = false;
        _host.Breakpoints.AddReturn(0x50A221, tid, "NetworkOrdinary.primary-source-native-RET4",
            (uint rt, uint at, ref Win32Native.X86Context returned) =>
            {
                Require(_networkPrimaryOrdinaryWrapperPending && !_networkPrimaryOrdinaryRetSeen &&
                    rt == tid && at == 0x50A221 && returned.Eip == at && returned.Esp == frame.Esp &&
                    U(returned.Esp) == frame.Caller && OrdinaryPreserved(frame, returned),
                    "Primary source skip did not reach its exact original RET4.");
                _networkPrimaryOrdinaryRetSeen = true; ++_networkPrimaryOrdinaryRet4s;
            });
        OrdinaryReturn(tid, frame, 8, "primary-capture-only", (ref Win32Native.X86Context returned) =>
        {
            Require(_networkPrimaryOrdinaryWrapperPending && _networkPrimaryOrdinaryRetSeen,
                "Primary source skip lacks its native RET4/caller return.");
            VerifyNetworkPrimaryOrdinaryPools(requireInactive: true);
            _networkPrimaryOrdinaryWrapperPending = false; ++_networkPrimaryOrdinarySkipReturns;
            _log(new { @event = "template-network-primary-ordinary-suppressed", nativeReturn = "0050A221", caller = Hex(frame.Caller),
                entryEsp = Hex(frame.Esp), returnedEsp = Hex(returned.Esp), skip = _networkPrimaryOrdinarySkips,
                sourceCommandWrites = 0, nativePrimaryAnimationRetained = true, primaryOrdinaryPoolsInactive = true });
        });
        context.Eip = 0x50A221; return true;
    }

    private void RejectNetworkPrimaryOrdinaryActivation(uint tid, in Win32Native.X86Context context)
    {
        if (!_networkPrimaryCaptureOnlyFixture || !_ordinaryStarted || U(context.Ecx + 0x18) != _projectilePrimaryManager) return;
        Require(tid == _host.MainThreadId && context.Eip == 0x4FF800 && U(context.Ecx) == 0x6F6B04 &&
            _networkPrimaryOrdinaryPools.Contains(context.Ecx), "Escaping primary activation has an unqualified source identity.");
        ++_networkPrimaryOrdinaryActivationEscapes;
        throw new InvalidDataException("Primary ordinary activation bypassed the explicit one-game capture-only wrapper policy.");
    }

    private void VerifyNetworkPrimaryOrdinaryAtRetirement()
    {
        if (!_networkPrimaryCaptureOnlyFixture) return;
        Require(_networkPrimaryOrdinaryBaselineVerified && !_networkPrimaryOrdinaryWrapperPending &&
            _networkPrimaryOrdinaryActivationEscapes == 0 && _networkPrimaryOrdinarySkips == _networkPrimaryOrdinaryRet4s &&
            _networkPrimaryOrdinarySkips == _networkPrimaryOrdinarySkipReturns,
            "Capture-only policy has an escape or unmatched native source wrapper frame.");
        VerifyNetworkPrimaryOrdinaryPools(requireInactive: true); _networkPrimaryOrdinaryFinalInactiveVerified = true;
    }
}
