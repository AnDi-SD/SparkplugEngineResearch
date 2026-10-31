using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>Separate scoped admission of a native, non-player TargetRegion.</summary>
internal sealed partial class NativeTemplateProbe
{
    private bool TargetRegionFixture => _layout.Sha256 is NativeTemplateLayout.TargetRegionBodySha256 or NativeTemplateLayout.ProjectileManagerBodySha256;
    private uint _targetRegion, _targetRegionSetupCalls, _targetRegionSetupReturns;
    private readonly bool _configureTargetRegion;
    private ushort _targetRegionFlagsBeforeConfiguration;
    private bool _targetRegionConfigured;
    private TargetRegistry? _targetRegistryBefore, _targetRegistryInitialized, _targetRegistryBeforeDelete, _targetRegistryDeleted;
    private object? _targetRegionWitness;
    private sealed record TargetRegistry(uint Manager, uint Head, uint Count, uint[] Regions,
        uint Selection20, uint Selection24, uint Player28);
    private object TargetRegionSummary => new { enabled = TargetRegionFixture,
        setupEntries = _targetRegionSetupCalls, setupReturns = _targetRegionSetupReturns,
        initialized = _targetRegionWitness, before = _targetRegistryBefore,
        afterInit = _targetRegistryInitialized, beforeDelete = _targetRegistryBeforeDelete, afterDelete = _targetRegistryDeleted,
        configuredByNativeCall = _targetRegionConfigured,
        nativePlayerRoleEnabled = false, roleFilterAdapterEnabled = false,
        ownHitQualified = false, originalSchedulerResumed = _idle };

    private NativePostInitCallPlan? TemplatePostInitPlan()
    {
        if (!_playerTraitsPreparePickupFx) return null;
        NativePostInitCallPlan plan = PlayerTraitsPickupPlan();
        if (!_configureTargetRegion) return plan;
        NativePostInitCallPlan target = TargetRegionConfigurationPlan();
        if (_ordinaryAttackFixture) target = target with { Next = OrdinaryAimExclusionPlan() };
        return plan with { Next = target };
    }

    private NativePostInitCallPlan TargetRegionConfigurationPlan() => new(0x5113E0,
        instance =>
        {
            Require(_configureTargetRegion && TargetRegionFixture && _dependentInterval && instance == _instance &&
                _targetRegionWitness is not null && _playerTraitsOwnFxReady && !_targetRegionConfigured &&
                U(_targetRegion) == 0x6F7ED0 && U(_targetRegion + 0x24) == instance &&
                U(_targetRegion + 0x18) == _ownRoot && Read(_targetRegion + 0x148, 1)[0] == 0,
                "Native TargetRegion configuration lost its own non-player receiver.");
            uint metadata = Pointer(_move + 0x174);
            Require(U(_ownRoot + 0xC) == metadata && U(metadata + 8) == instance,
                "Native TargetRegion configuration would act on foreign metadata.");
            _targetRegionFlagsBeforeConfiguration = BitConverter.ToUInt16(Read(metadata + 4, 2));
            Require(_targetRegionFlagsBeforeConfiguration is 6 or 14,
                "Native TargetRegion configuration has an unqualified input flag word.");
            return _targetRegion;
        },
        region =>
        {
            Require(region == _targetRegion && _dependentInterval && _targetRegionSetupCalls == _targetRegionSetupReturns &&
                _targetRegionSetupReturns >= 1, "Native TargetRegion setup lacks its exact paired C3 witness.");
            uint metadata = Pointer(_move + 0x174);
            ushort flags = BitConverter.ToUInt16(Read(metadata + 4, 2));
            TargetRegistry current = ReadTargetRegistry(), initialized = _targetRegistryInitialized!;
            Require(flags == (_targetRegionFlagsBeforeConfiguration | 8) && U(_ownRoot + 0xC) == metadata &&
                U(metadata + 8) == _instance && current.Regions.SequenceEqual(initialized.Regions) &&
                current.Selection20 == initialized.Selection20 && current.Selection24 == initialized.Selection24 &&
                current.Player28 == initialized.Player28,
                "Native TargetRegion setup changed another flag or global target owner.");
            _targetRegionConfigured = true;
            _log(new { @event = "template-target-region-configured", region = Hex(region), metadata = Hex(metadata),
                beforeFlagsWord = _targetRegionFlagsBeforeConfiguration, afterFlagsWord = flags,
                nativeFunction = Hex(0x5113E0), nativeRole10Manufactured = false, adapterGameFieldWrites = 0 });
        });

    private TargetRegistry ReadTargetRegistry()
    {
        uint manager = U(0x765AF0);
        if (manager == 0) return new(0, 0, 0, [], 0, 0, 0);
        Table(manager, 0x703954);
        uint head = Pointer(manager + 0x18), count = U(manager + 0x1C);
        Require(count <= 1024, "TargetRegion registry exceeds its bounded candidate budget.");
        var regions = new List<uint>(); var seen = new HashSet<uint>();
        uint link = U(head), previous = head;
        for (uint index = 0; index < count; ++index)
        {
            Require(link != head && link >= 0x10000 && seen.Add(link) && U(link + 4) == previous,
                "TargetRegion candidate list has a broken link or count.");
            regions.Add(Pointer(link + 8)); previous = link; link = U(link);
        }
        Require(link == head && U(head + 4) == previous, "TargetRegion candidate sentinel/count mismatch.");
        return new(manager, head, count, regions.ToArray(), U(manager + 0x20), U(manager + 0x24), U(manager + 0x28));
    }

    private void CaptureTargetRegionBaseline()
    {
        if (!TargetRegionFixture) return;
        Require((!_idle || _bloomIdle && _configureTargetRegion && _sharedHealthAliasEnabled) && _stage == 0,
            "TargetRegion baseline must precede its admitted own construction.");
        _targetRegistryBefore = ReadTargetRegistry();
        VerifyFunction(0x5113E0);
        Require(Read(0x511423, 4).SequenceEqual(new byte[] { 0x80, 0x48, 0x04, 0x08 }),
            "TargetRegion native metadata producer differs from its pin.");
        Hook(0x5113E0, "TargetRegion.configure-owned-metadata", (uint tid, uint at, ref Win32Native.X86Context context) =>
        {
            if (!OwnReceiver(context.Ecx)) return;
            Table(context.Ecx, 0x6F7ED0);
            bool ownLiveCall = _idle && _idleActive && !_dependentInterval && _stage == 6 &&
                _configureTargetRegion && context.Ecx == _targetRegion;
            Require((_dependentInterval && _stage == 6 || ownLiveCall) &&
                ++_targetRegionSetupCalls <= (_idle ? ObservationNativeCallLimit + 1 : 8),
                "Own TargetRegion setup escaped its scoped or retained native lifetime.");
            uint region = context.Ecx, esp = context.Esp, caller = U(esp);
            _host.Breakpoints.AddReturn(caller, tid, "TargetRegion.configure-owned-metadata.return",
                (uint rt, uint returnedAt, ref Win32Native.X86Context returned) =>
                {
                    Require(rt == tid && returnedAt == caller && returned.Esp == checked(esp + 4) &&
                        (_dependentInterval || _idle && _idleActive && _stage == 6) && U(region) == 0x6F7ED0,
                        "Owned TargetRegion native C3 return did not pair with its entry.");
                    ++_targetRegionSetupReturns;
                    uint owner = Pointer(region + 0x24), root = Pointer(owner + 0x24), metadata = U(root + 0xC);
                    _log(new { @event = "template-target-region-native-setup-return", region = Hex(region),
                        caller = Hex(caller), nativeEspMatched = true, metadata = Hex(metadata),
                        metadataFlagsWord = metadata == 0 ? (ushort?)null : BitConverter.ToUInt16(Read(metadata + 4, 2)),
                        originalSchedulerCall = ownLiveCall, gameFieldsWrittenByAdapter = 0 });
                });
        });
        _log(new { @event = "template-target-region-baseline", registry = _targetRegistryBefore });
    }

    private void CaptureTargetRegionInitialized()
    {
        if (!TargetRegionFixture) return;
        Require((!_idle || _bloomIdle && _configureTargetRegion && _sharedHealthAliasEnabled) &&
            _dependentInterval && _stage == 6 && _targetRegistryBefore is not null,
            "TargetRegion initialized witness escaped its scoped lifecycle.");
        _targetRegion = Components(_instance, DescriptorCount).Single(c => c.ClassId == 0x4307193A).Address;
        Table(_targetRegion, 0x6F7ED0);
        Require(U(_targetRegion + 0x24) == _instance && U(_targetRegion + 0x18) == _ownRoot &&
            Read(_targetRegion + 0x148, 1)[0] == 0 && Read(_targetRegion + 0x145, 1)[0] == 1 &&
            Read(_targetRegion + 0x147, 1)[0] == 1,
            "TargetRegion is not the exact own, non-player, template-volume candidate.");
        TargetRegistry before = _targetRegistryBefore!, current = ReadTargetRegistry();
        Require(current.Player28 == before.Player28 && current.Selection20 == before.Selection20 &&
            current.Selection24 == before.Selection24 &&
            (current.Regions.SequenceEqual(before.Regions) || current.Regions.SequenceEqual(before.Regions.Append(_targetRegion))),
            "Own TargetRegion initialization changed another selection owner or candidate ordering.");
        _targetRegistryInitialized = current;
        _targetRegistryBeforeDelete = current; // The scoped batch permits no intervening native world.
        uint metadata = Pointer(_move + 0x174);
        Require(U(_ownRoot + 0xC) == metadata && U(metadata + 8) == _instance && U(0x765C00) == 0,
            "TargetRegion metadata is not borrowed from its own MoveCtrl or shutdown suppresses unregister.");
        _targetRegionWitness = new { region = Hex(_targetRegion), owner = Hex(_instance), boundRoot = Hex(_ownRoot),
            metadata = Hex(metadata), flagsWord = BitConverter.ToUInt16(Read(metadata + 4, 2)),
            auxiliaryMetadata14C = Hex(U(_targetRegion + 0x14C)),
            nativeIsPlayer = false, nativeIsTarget = true, nativeTemplateVolume = true,
            candidateRegistered = current.Regions.Contains(_targetRegion), primaryTargetOwnerUnchanged = true };
        _log(new { @event = "template-target-region-initialized", witness = _targetRegionWitness, registry = current });
    }

    private void CaptureTargetRegionConstructionBaseline()
    {
        if (!TargetRegionFixture) return;
        Require(_stage == 5 && !_dependentInterval && _instance == 0,
            "TargetRegion mutation baseline must immediately precede its dependent construction batch.");
        _targetRegistryBefore = ReadTargetRegistry();
        _log(new { @event = "template-target-region-before-dependent-create", registry = _targetRegistryBefore });
    }

    private void CaptureTargetRegionRetainedDeleteBaseline()
    {
        if (!TargetRegionFixture) return;
        Require(_idle && !_idleActive && !_dependentInterval && _stage == 6 && _targetRegion != 0 && U(0x765C00) == 0,
            "TargetRegion retained deletion lacks its live own destructor/unregister boundary.");
        _targetRegistryBeforeDelete = ReadTargetRegistry();
        Require(_targetRegistryBeforeDelete.Player28 == _targetRegistryBefore!.Player28,
            "The canonical player TargetRegion changed during the retained fixture.");
        _log(new { @event = "template-target-region-before-retained-delete", registry = _targetRegistryBeforeDelete });
    }

    private void VerifyTargetRegionDeleted()
    {
        if (!TargetRegionFixture) return;
        Require(_targetRegistryBeforeDelete is not null && _targetRegistryInitialized is not null &&
            _targetRegionSetupCalls == _targetRegionSetupReturns,
            "TargetRegion deletion lacks an initialized or paired native witness.");
        TargetRegistry current = ReadTargetRegistry(), before = _targetRegistryBeforeDelete!;
        uint[] remaining = before.Regions.Where(region => region != _targetRegion).ToArray();
        Require((before.Manager == 0 || current.Manager == before.Manager) &&
            current.Regions.SequenceEqual(remaining) && current.Count == remaining.Length &&
            current.Selection20 == (before.Selection20 == _targetRegion ? 0u : before.Selection20) &&
            current.Selection24 == (before.Selection24 == _targetRegion ? 0u : before.Selection24) &&
            current.Player28 == before.Player28,
            "Own TargetRegion destructor did not restore the original borrowed registry/selection state.");
        _targetRegistryDeleted = current;
        _log(new { @event = "template-target-region-delete-verified", registry = current,
            historicalRegionRead = false, nativeCandidateRegistryRestored = true });
    }
}
