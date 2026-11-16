using System.Reflection.PortableExecutable;
using System.Security.Cryptography;
using System.Text.Json;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

internal sealed record ProbeConfiguration(
    string Executable, string WorkingDirectory, string MediaDirectory,
    string OutputDirectory, int DurationSeconds = 90, int SampleEvery = 15, string Mode = "observe",
    string? TemplateFile = null, string? TemplateSha256 = null, bool PrepareFirstMagicStage = false,
    bool TemplatePlayerTraitsFixture = false, bool TemplatePlayerTraitsPickupFx = false,
    bool TemplateSharedHealthAlias = false, bool TemplateHealthBattleFixture = false,
    bool TemplateTargetRegionFixture = false, bool TemplateConfigureTargetRegion = false,
    bool TemplateStingerSecondaryRole = false, bool TemplateStarSecondaryEligibility = false,
    bool TemplateBattleCombatFixture = false, bool TemplateStarPlacementFixture = false,
    bool TemplateSharedMagicEnergyAlias = false, bool TemplateSecondAttackDegreeFixture = false,
    bool TemplateSharedBattleLifetime = false, bool TemplateDeathBattleFixture = false,
    string? TemplateSharedPlayerProfile = null, bool TemplateStoryDeathFixture = false,
    bool TemplateCollisionDetectorSecondaryRole = false, bool TemplateProjectileManagerFixture = false,
    bool TemplateBodyInputFixture = false, bool TemplateOrdinaryAttackFixture = false,
    bool TemplateNetworkInputFixture = false, DiagnosticInputIdentity? DiagnosticHostInput = null,
    DiagnosticInputIdentity? DiagnosticGuestInput = null, bool TemplateNetworkInputBackpressureFixture = false,
    bool DiagnosticGuestCaptureFixture = false, bool TemplateNetworkOrdinaryAttackFixture = false,
    bool TemplateHostBackgroundFixture = false, int DiagnosticInputHeartbeatMilliseconds = 50,
    bool TemplateNetworkOrdinaryAimFixture = false, bool TemplateNetworkPrimaryCaptureOnlyFixture = false,
    int TemplateNetworkInputDrainHoldMilliseconds = 300,
    WorldStateIdentity? DiagnosticWorldState = null, bool WorldBattlePresentation = false,
    bool EnableWorldEntry = false, uint WorldEntryCaptureOrdinal = 1, byte[]? WorldEntrySeed = null,
    byte[]? WorldEntryExpectedWitness = null, string? InitialSaveSha256 = null,
    bool ObserveNativeMenuInput = false, HostTransportRebindPermit? DiagnosticHostRebind = null,
    bool IsolatedMenuInputDiagnostic = false, bool RefreshOriginalGamePadDiagnostic = false,
    bool ReacquireOriginalGamePadDiagnostic = false, bool LimitContinuousGuestInput = false,
    bool TemplateMosquitoPlayerTargets = false, bool DiagnosticAutoBattlePauseFixture = false,
    bool EnableProgressionAuthority = false, bool UseProcessLocalMediaPath = false,
    WorldStateRole? ProgressionAuthorityRole = null, bool ProgressionCaptureSaveBeforeWriter = false,
    bool ProgressionReloadSavedSlot = false, ProgressionControlPermit? ProgressionControl = null,
    bool ProgressionHostContinuationSeed = false, uint ProgressionSceneLevel = 28,
    bool ProgressionSavedReload = false, NativeProgressionOutcomeCheckpoint? ProgressionOutcomeCheckpoint = null,
    bool ProgressionOverlayFixture = false, int WorldPresentationSmoothingMilliseconds = 0,
    bool GuestPresentationBackgroundFixture = false, bool ProgressionNeutralCaptureWhileUnfocused = false,
    bool ProgressionPreWaitingReloadPreparation = false, CampaignSceneProfile? CampaignScene = null, bool InteractiveSession = false, bool ObserveSceneInventory = false)
{
    // .NET 6 limits parameterized JSON constructors to64 arguments. Preserve
    // the positional defaults, then let JSON populate the existing init properties.
    [System.Text.Json.Serialization.JsonConstructor]
    public ProbeConfiguration() : this("", "", "", "") { }
}

internal static class ProbeMain
{
    internal const string OriginalSha256 = "3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F";
    private static readonly JsonSerializerOptions Json = new() { WriteIndented = true };

    internal static bool IsClosedStory3Host(ProbeConfiguration config) =>
        config.ProgressionControl is { NextLevel: 28, OwnerPid: > 0, OwnerCreationFileTime: > 0 } &&
        config.ProgressionOutcomeCheckpoint is { DestinationLevel: 28 } &&
        config.ProgressionSceneLevel == 3 && config.EnableProgressionAuthority && config.UseProcessLocalMediaPath &&
        config.EnableWorldEntry && config.ProgressionAuthorityRole is null or WorldStateRole.Host &&
        config.DiagnosticWorldState is { Role: WorldStateRole.Host } world &&
        config.DiagnosticHostInput is { Role: DiagnosticInputRole.Host } input && config.DiagnosticGuestInput is null &&
        world.LevelEpoch > 0 && world.Generation > 0 && input.SessionId == world.SessionId &&
        input.ScenarioId == world.ScenarioId && input.LevelEpoch == world.LevelEpoch && input.BodyGeneration == world.Generation &&
        config.Mode == "template-bloom-body-hair-idle" && config.TemplateSharedPlayerProfile == "gardenia-03-network-input" &&
        !string.IsNullOrWhiteSpace(config.TemplateFile) && config.TemplateSha256 == NativeTemplateLayout.TargetRegionBodySha256 &&
        config.TemplateNetworkInputFixture && config.TemplateStoryDeathFixture && config.TemplatePlayerTraitsFixture &&
        config.TemplatePlayerTraitsPickupFx && config.TemplateTargetRegionFixture && config.TemplateConfigureTargetRegion &&
        config.TemplateSharedHealthAlias && !config.TemplateProjectileManagerFixture && !config.TemplateBodyInputFixture &&
        !config.TemplateOrdinaryAttackFixture && !config.TemplateNetworkOrdinaryAttackFixture &&
        !config.TemplateNetworkOrdinaryAimFixture && !config.TemplateNetworkPrimaryCaptureOnlyFixture &&
        !config.TemplateHealthBattleFixture && !config.TemplateSharedBattleLifetime && !config.TemplateDeathBattleFixture &&
        !config.TemplateBattleCombatFixture && !config.PrepareFirstMagicStage && !config.TemplateSecondAttackDegreeFixture &&
        !config.TemplateStingerSecondaryRole && !config.TemplateMosquitoPlayerTargets && !config.TemplateStarSecondaryEligibility &&
        !config.TemplateStarPlacementFixture && !config.TemplateSharedMagicEnergyAlias && !config.TemplateNetworkInputBackpressureFixture &&
        !config.DiagnosticGuestCaptureFixture && !config.WorldBattlePresentation && config.DiagnosticHostRebind is null &&
        !config.DiagnosticAutoBattlePauseFixture && !config.ObserveNativeMenuInput && !config.IsolatedMenuInputDiagnostic &&
        !config.RefreshOriginalGamePadDiagnostic && !config.ReacquireOriginalGamePadDiagnostic &&
        !config.ProgressionSavedReload && !config.ProgressionCaptureSaveBeforeWriter && !config.ProgressionReloadSavedSlot;

    internal static bool IsClosedStory3Guest(ProbeConfiguration config) =>
        config.ProgressionControl is { NextLevel: 28, OwnerPid: > 0, OwnerCreationFileTime: > 0 } &&
        config.ProgressionSceneLevel == 3 && config.EnableProgressionAuthority && config.UseProcessLocalMediaPath &&
        config.EnableWorldEntry && config.ProgressionAuthorityRole is null or WorldStateRole.Guest &&
        config.DiagnosticWorldState is { Role: WorldStateRole.Guest } world &&
        config.DiagnosticGuestInput is { Role: DiagnosticInputRole.Guest } input && config.DiagnosticHostInput is null &&
        world.LevelEpoch > 0 && world.Generation > 0 && input.SessionId == world.SessionId &&
        input.ScenarioId == world.ScenarioId && input.LevelEpoch == world.LevelEpoch && input.BodyGeneration == world.Generation &&
        config.Mode == "client-presentation-live" && config.DiagnosticGuestCaptureFixture && !config.WorldBattlePresentation &&
        config.TemplateFile is null && config.TemplateSha256 is null && config.TemplateSharedPlayerProfile is null &&
        !config.TemplateNetworkInputFixture && !config.TemplateNetworkInputBackpressureFixture &&
        !config.TemplateNetworkOrdinaryAttackFixture && !config.TemplateHostBackgroundFixture &&
        !config.TemplateProjectileManagerFixture && !config.TemplateBodyInputFixture && !config.TemplateOrdinaryAttackFixture &&
        !config.TemplatePlayerTraitsFixture && !config.TemplatePlayerTraitsPickupFx && !config.TemplateSharedHealthAlias &&
        !config.TemplateHealthBattleFixture && !config.TemplateTargetRegionFixture && !config.TemplateConfigureTargetRegion &&
        !config.TemplateStingerSecondaryRole && !config.TemplateStarSecondaryEligibility && !config.TemplateBattleCombatFixture &&
        !config.TemplateStarPlacementFixture && !config.TemplateSharedMagicEnergyAlias && !config.TemplateSecondAttackDegreeFixture &&
        !config.TemplateSharedBattleLifetime && !config.TemplateDeathBattleFixture && !config.TemplateStoryDeathFixture &&
        !config.TemplateCollisionDetectorSecondaryRole && !config.PrepareFirstMagicStage && !config.TemplateMosquitoPlayerTargets &&
        config.DiagnosticHostRebind is null && !config.DiagnosticAutoBattlePauseFixture && !config.ObserveNativeMenuInput &&
        !config.IsolatedMenuInputDiagnostic && !config.RefreshOriginalGamePadDiagnostic && !config.ReacquireOriginalGamePadDiagnostic &&
        !config.ProgressionHostContinuationSeed && !config.ProgressionSavedReload &&
        !config.ProgressionCaptureSaveBeforeWriter && !config.ProgressionReloadSavedSlot && config.ProgressionOutcomeCheckpoint is null;

    internal static bool IsCampaign(ProbeConfiguration config)
    {
        if (config.CampaignScene is null) return false;
        config.CampaignScene.Validate();
        return config.ProgressionControl is { OwnerPid: > 0, OwnerCreationFileTime: > 0 } &&
            config.CampaignScene.Admits(config.ProgressionSceneLevel) && config.EnableProgressionAuthority &&
            config.EnableWorldEntry && config.UseProcessLocalMediaPath && !config.WorldBattlePresentation &&
            !config.TemplateStoryDeathFixture && !config.TemplateDeathBattleFixture &&
            !config.TemplateNetworkOrdinaryAttackFixture && !config.PrepareFirstMagicStage &&
            !config.TemplateMosquitoPlayerTargets && config.ProgressionOutcomeCheckpoint is null &&
            config.DiagnosticWorldState is { } world &&
            (world.Role == WorldStateRole.Host
                ? config.Mode == "template-bloom-body-hair-idle" && config.TemplateNetworkInputFixture &&
                    config.TemplateSharedPlayerProfile == CampaignSceneProfile.BodyProfile &&
                    config.DiagnosticHostInput is { Role: DiagnosticInputRole.Host } && config.DiagnosticGuestInput is null
                : world.Role == WorldStateRole.Guest && config.Mode == "client-presentation-live" &&
                    config.DiagnosticGuestCaptureFixture && config.DiagnosticGuestInput is { Role: DiagnosticInputRole.Guest } &&
                    config.DiagnosticHostInput is null);
    }

    private static int Main(string[] args)
    {
        try
        {
            string configPath = args.Length switch
            {
                0 => (Environment.ProcessPath ?? throw new InvalidOperationException("Missing apphost path")) + ".json",
                2 when args[0] == "--config" => args[1],
                _ => throw new ArgumentException("Use --config <fresh-case.json> or the apphost-adjacent .exe.json")
            };
            ProbeConfiguration config = JsonSerializer.Deserialize<ProbeConfiguration>(File.ReadAllText(configPath))
                ?? throw new InvalidDataException("Missing case configuration");
            // This one-shot diagnostic admits only the pure observe defaults.
            // No Campaign, transport, template, input or progression role is invented.
            if (config.ObserveSceneInventory && config != new ProbeConfiguration(
                config.Executable, config.WorkingDirectory, config.MediaDirectory, config.OutputDirectory,
                config.DurationSeconds, config.SampleEvery, Mode: "observe", ObserveSceneInventory: true))
                throw new InvalidDataException("Scene inventory requires the isolated pure observe configuration.");
            if (config.InteractiveSession && !IsCampaign(config))
                throw new InvalidDataException("Interactive lifetime requires the campaign session profile.");
            if (!NativeObservationBudget.DurationAllowed(config) || config.SampleEvery is < 1 or > 1000)
                throw new InvalidDataException("Duration requires5..600s diagnostics or explicit60..14400s interactive authority; sample interval1..1000");
            if (config.WorldPresentationSmoothingMilliseconds != 0 &&
                (config.WorldPresentationSmoothingMilliseconds is < 1 or > 200 ||
                 config.Mode != "client-presentation-live" || config.DiagnosticWorldState?.Role != WorldStateRole.Guest ||
                 !config.DiagnosticGuestCaptureFixture || !config.EnableWorldEntry || !config.EnableProgressionAuthority ||
                 config.ProgressionControl is null || config.ProgressionSceneLevel != 28 && !IsCampaign(config) || config.WorldBattlePresentation))
                throw new InvalidDataException("Display smoothing requires the explicit two-Bloom Alfea Guest progression quarantine.");
            if (config.GuestPresentationBackgroundFixture &&
                (config.Mode != "client-presentation-live" || config.DiagnosticWorldState?.Role != WorldStateRole.Guest ||
                 !config.DiagnosticGuestCaptureFixture || !config.EnableWorldEntry || !config.EnableProgressionAuthority ||
                 config.ProgressionControl is null || config.ProgressionSceneLevel != 28 && !IsCampaign(config) || config.WorldBattlePresentation ||
                 config.TemplateHostBackgroundFixture || config.DiagnosticHostInput is not null ||
                 config.DiagnosticGuestInput?.Role != DiagnosticInputRole.Guest))
                throw new InvalidDataException("Background Guest animation requires the explicit Alfea E2 presentation/capture progression cohort.");
            if (config.ProgressionNeutralCaptureWhileUnfocused &&
                (config.Mode != "client-presentation-live" || config.DiagnosticWorldState?.Role != WorldStateRole.Guest ||
                 config.DiagnosticGuestInput?.Role != DiagnosticInputRole.Guest || config.DiagnosticHostInput is not null ||
                 !config.DiagnosticGuestCaptureFixture || !config.GuestPresentationBackgroundFixture || !config.EnableWorldEntry ||
                 !config.EnableProgressionAuthority || !config.UseProcessLocalMediaPath || config.ProgressionControl is null ||
                 config.ProgressionSceneLevel != 28 && !IsCampaign(config) || config.WorldBattlePresentation || config.TemplateHostBackgroundFixture))
                throw new InvalidDataException("Unfocused held-neutral capture requires the explicit controlled Alfea Guest E2 quarantine/background cohort.");
            if (config.CampaignScene is not null && !IsCampaign(config))
                throw new InvalidDataException("Campaign admission requires its explicit role/owner/scene profile.");
            NativeObservationBudget.Configure(config);
            NativeSharedPlayerProfile sharedProfile = NativeSharedPlayerProfile.Select(config);
            bool story3Host = IsClosedStory3Host(config), story3Guest = IsClosedStory3Guest(config);
            if (!IsCampaign(config) && (config.ProgressionSceneLevel is not (28 or 3) ||
                config.ProgressionSceneLevel != 28 && config.ProgressionControl is null))
                throw new InvalidDataException("Progression scene admission is limited to explicit Alfea28 or Story3 cohorts.");
            if (config.ProgressionSceneLevel == 3 && !story3Host && !story3Guest)
                throw new InvalidDataException("Story3 progression requires its closed seven-body Host outcome or isolated Guest presentation cohort.");
            if (config.ProgressionOutcomeCheckpoint is not null && !story3Host)
                throw new InvalidDataException("An outcome checkpoint belongs only to the closed Story3 Host/control/destination28 owner.");
            if (config.EnableProgressionAuthority &&
                config.DiagnosticWorldState is null && config.Mode != "progression-save-check")
                throw new InvalidDataException("Progression authority requires a retained World cohort or its separate SavePoint diagnostic.");
            if (config.ProgressionAuthorityRole is { } explicitProgressionRole &&
                (!config.EnableProgressionAuthority || explicitProgressionRole is not (WorldStateRole.Host or WorldStateRole.Guest) ||
                    config.DiagnosticWorldState is { } progressionWorld && progressionWorld.Role != explicitProgressionRole))
                throw new InvalidDataException("Explicit progression role must match its enabled authority/session.");
            if (config.Mode == "progression-save-check" &&
                (!config.UseProcessLocalMediaPath || !config.EnableProgressionAuthority ||
                    config.ProgressionAuthorityRole is null || config.DiagnosticWorldState is not null))
                throw new InvalidDataException("SavePoint authority check requires its explicit standalone role and process-local MediaPath.");
            if (config.ProgressionCaptureSaveBeforeWriter &&
                (config.Mode != "progression-save-check" || config.ProgressionAuthorityRole != WorldStateRole.Host))
                throw new InvalidDataException("Current-state SavePoint capture requires the separate Host-only save diagnostic.");
            if (config.ProgressionReloadSavedSlot && !config.ProgressionCaptureSaveBeforeWriter)
                throw new InvalidDataException("Saved-state reload requires the explicit preceding whole native Host capture and writer.");
            if (config.EnableProgressionAuthority && config.TemplateMosquitoPlayerTargets)
                throw new InvalidDataException("Progression Exit observations and the old Mosquito Exit return probe require separate cohorts.");
            if (config.ProgressionControl is not null &&
                (!config.EnableProgressionAuthority || !config.UseProcessLocalMediaPath || !config.EnableWorldEntry ||
                 config.DiagnosticHostRebind is not null || config.WorldBattlePresentation ||
                 config.DiagnosticWorldState is null || config.Mode == "progression-save-check"))
                throw new InvalidDataException("Restart progression requires its separate Alfea World/entry/authority cohort.");
            if (config.ProgressionHostContinuationSeed &&
                (config.ProgressionControl is null || config.DiagnosticWorldState?.Role != WorldStateRole.Host))
                throw new InvalidDataException("Host continuation seed requires explicit progression ownership.");
            if (config.ProgressionSavedReload && (config.ProgressionControl is null ||
                config.ProgressionSceneLevel != 28 && !IsCampaign(config) || config.DiagnosticWorldState?.Role != WorldStateRole.Host ||
                config.TemplateSharedPlayerProfile != "alfea-28-network-input" && !IsCampaign(config) || !config.TemplateNetworkInputFixture))
                throw new InvalidDataException("Saved progression requires the explicit Alfea28 Host network-body owner.");
            if (config.ProgressionPreWaitingReloadPreparation && (!config.ProgressionSavedReload ||
                !config.EnableProgressionAuthority || !config.UseProcessLocalMediaPath || !config.EnableWorldEntry ||
                config.Mode != "template-bloom-body-hair-idle" || config.DiagnosticGuestInput is not null ||
                config.DiagnosticHostInput?.Role != DiagnosticInputRole.Host || config.ProgressionOverlayFixture ||
                config.WorldBattlePresentation || config.TemplateNetworkOrdinaryAttackFixture ||
                config.TemplateDeathBattleFixture || config.TemplateStoryDeathFixture || config.TemplateMosquitoPlayerTargets))
                throw new InvalidDataException("Pre-Waiting saved reload preparation requires the explicit held Alfea28 Host cohort.");
            if (config.ProgressionOverlayFixture && (config.ProgressionControl is null ||
                config.ProgressionSceneLevel != 28 || config.ProgressionSavedReload || config.ProgressionHostContinuationSeed ||
                config.InitialSaveSha256 is not null || config.WorldBattlePresentation || config.DiagnosticWorldState is null ||
                config.TemplateStoryDeathFixture || config.TemplateDeathBattleFixture || config.TemplateNetworkOrdinaryAttackFixture))
                throw new InvalidDataException("Native overlay requires its separate controlled initial Alfea28 Host/Guest cohort.");
            if (config.Mode == "process-media-path-check" &&
                (!config.UseProcessLocalMediaPath || config.EnableProgressionAuthority || config.DiagnosticWorldState is not null))
                throw new InvalidDataException("Process MediaPath check is a separate owned native-resolver diagnostic.");
            if (config.ReacquireOriginalGamePadDiagnostic && !config.RefreshOriginalGamePadDiagnostic)
                throw new InvalidDataException("One same-device acquisition cycle requires the explicit original-pad diagnostic.");
            if (config.RefreshOriginalGamePadDiagnostic && !config.IsolatedMenuInputDiagnostic)
                throw new InvalidDataException("One original GamePad refresh is confined to the isolated non-acceptance input diagnostic.");
            if (config.IsolatedMenuInputDiagnostic &&
                (!config.ObserveNativeMenuInput || config.DiagnosticWorldState?.Role != WorldStateRole.Host ||
                    config.TemplateSharedPlayerProfile != "battle-01-network-input" || config.DiagnosticHostRebind is not null))
                throw new InvalidDataException("Isolated menu input is an explicit non-acceptance Host Battle diagnostic only.");
            if (config.DiagnosticHostRebind is not null &&
                (!config.TemplateNetworkInputFixture || config.DiagnosticGuestInput is not null ||
                 config.DiagnosticHostInput is not { Role: DiagnosticInputRole.Host } rebindInput ||
                 config.DiagnosticWorldState is not { Role: WorldStateRole.Host } rebindWorld ||
                 rebindInput.SessionId != rebindWorld.SessionId || rebindInput.ScenarioId != rebindWorld.ScenarioId ||
                 rebindInput.LevelEpoch != rebindWorld.LevelEpoch || rebindInput.BodyGeneration != rebindWorld.Generation))
                throw new InvalidDataException("Diagnostic Host rebind requires matching explicit Host-only Input/World fixtures.");
            if (config.ObserveNativeMenuInput &&
                (config.DiagnosticWorldState?.Role != WorldStateRole.Host || !config.TemplateSharedBattleLifetime ||
                    config.TemplateSharedPlayerProfile != "battle-01-network-input"))
                throw new InvalidDataException("Native menu input observation requires its bounded Host Battle47 lifetime cohort.");
            if (config.EnableWorldEntry)
            {
                uint entryLevel = config.ProgressionControl is not null ? config.ProgressionSceneLevel : 28u;
                if (config.DiagnosticWorldState is not { } entryIdentity || config.WorldBattlePresentation ||
                    config.WorldEntryCaptureOrdinal is < 1 or > 2)
                    throw new InvalidDataException("Native entry bootstrap currently requires the explicit Alfea28 WorldState cohort.");
                if (entryIdentity.Role == WorldStateRole.Host)
                {
                    if (config.ProgressionControl is not null)
                    {
                        bool savedContinuation = config.ProgressionSavedReload && config.ProgressionHostContinuationSeed &&
                            config.InitialSaveSha256 is { Length: 64 } savedHash && savedHash.All(Uri.IsHexDigit);
                        if (config.WorldEntryCaptureOrdinal != (savedContinuation ? 2u : 1u) ||
                            config.InitialSaveSha256 is not null && !savedContinuation ||
                            (config.ProgressionHostContinuationSeed
                                ? config.WorldEntrySeed is null || config.WorldEntryExpectedWitness is not { Length: 64 }
                                : config.WorldEntrySeed is not null || config.WorldEntryExpectedWitness is not null))
                            throw new InvalidDataException("Progression Host requires its real entry or exact original-save second-entry continuation.");
                        if (config.WorldEntrySeed is { } continuation)
                        {
                            var resumed = WorldEntrySeedState.Unpack(continuation);
                            if (resumed.Level != entryLevel || resumed.LevelEpoch != entryIdentity.LevelEpoch || resumed.Generation != entryIdentity.Generation)
                                throw new InvalidDataException("Host continuation seed belongs to another scene generation.");
                        }
                    }
                    else if (config.WorldEntrySeed is not null || config.WorldEntryExpectedWitness is not null ||
                        config.WorldEntryCaptureOrdinal != 2 || config.InitialSaveSha256 is not { Length: 64 } entrySaveHash || !entrySaveHash.All(Uri.IsHexDigit))
                        throw new InvalidDataException("Host entry requires its real save bootstrap at original entry ordinal2.");
                }
                else
                {
                    if (config.WorldEntryCaptureOrdinal != 1 || config.InitialSaveSha256 is not null ||
                        config.WorldEntrySeed is null || config.WorldEntryExpectedWitness is not { Length: 64 })
                        throw new InvalidDataException("Guest entry requires the Host seed and native witness before startup.");
                    var seed = WorldEntrySeedState.Unpack(config.WorldEntrySeed);
                    if (seed.Level != entryLevel || seed.LevelEpoch != entryIdentity.LevelEpoch || seed.Generation != entryIdentity.Generation ||
                        (config.ProgressionControl is null && seed.HostEntryOrdinal != 2))
                        throw new InvalidDataException("Guest pre-init entry seed belongs to another native cohort.");
                }
            }
            else if (config.WorldEntrySeed is not null || config.WorldEntryExpectedWitness is not null ||
                config.InitialSaveSha256 is not null || config.WorldEntryCaptureOrdinal != 1)
                throw new InvalidDataException("Entry payload/options require the explicit native entry bootstrap.");
            if (config.DiagnosticWorldState is { } world &&
                (world.Role == WorldStateRole.Host ? !config.TemplateNetworkInputFixture :
                    !config.DiagnosticGuestCaptureFixture || config.Mode != "client-presentation-live"))
                throw new InvalidDataException("WorldState requires the explicit Host input-body or Guest presentation cohort.");
            if (config.WorldBattlePresentation &&
                (config.DiagnosticWorldState?.Role != WorldStateRole.Guest || !config.DiagnosticGuestCaptureFixture))
                throw new InvalidDataException("Battle presentation requires its separate Guest WorldState cohort.");
            if (config.LimitContinuousGuestInput &&
                (config.DiagnosticWorldState?.Role != WorldStateRole.Guest || !config.DiagnosticGuestCaptureFixture))
                throw new InvalidDataException("Continuous input cadence limiting requires its explicit World Guest capture cohort.");
            if (config.DiagnosticInputHeartbeatMilliseconds is < 16 or > 100 ||
                config.DiagnosticInputHeartbeatMilliseconds > 50 && !config.LimitContinuousGuestInput ||
                config.DiagnosticInputHeartbeatMilliseconds != 50 && config.DiagnosticGuestInput is null)
                throw new InvalidDataException("Native capture requires16..50ms, or up to100ms with the explicit World Guest cadence policy.");
            if (config.TemplateNetworkOrdinaryAttackFixture && !config.TemplateNetworkInputFixture)
                throw new InvalidDataException("Network ordinary attack requires its explicit network input fixture.");
            if ((config.TemplateNetworkOrdinaryAimFixture || config.TemplateNetworkPrimaryCaptureOnlyFixture) &&
                (!config.TemplateNetworkOrdinaryAttackFixture || !config.TemplateNetworkInputFixture))
                throw new InvalidDataException("Optional network aim/source policies require the explicit ordinary network fixture.");
            if (config.TemplateNetworkPrimaryCaptureOnlyFixture && config.DiagnosticGuestInput?.Role != DiagnosticInputRole.Guest)
                throw new InvalidDataException("Primary capture-only policy requires the same-game Guest endpoint.");
            if (config.TemplateHostBackgroundFixture &&
                (!config.TemplateNetworkInputFixture || config.DiagnosticHostInput is null || config.DiagnosticGuestInput is not null))
                throw new InvalidDataException("Background fixture requires a pure Host network body and no local source.");
            if (config.DiagnosticAutoBattlePauseFixture &&
                (config.Mode != "template-bloom-body-hair-idle" || config.TemplateSharedPlayerProfile != "battle-01-network-input" ||
                 !config.TemplateNetworkInputFixture || !config.TemplateNetworkOrdinaryAttackFixture ||
                 !config.TemplateNetworkOrdinaryAimFixture || !config.TemplateSharedBattleLifetime ||
                 !config.TemplateSharedHealthAlias || !config.TemplateHealthBattleFixture ||
                 !config.TemplateMosquitoPlayerTargets || !config.TemplateStingerSecondaryRole ||
                 config.DiagnosticHostInput?.Role != DiagnosticInputRole.Host || config.DiagnosticGuestInput is not null ||
                 config.DiagnosticWorldState?.Role != WorldStateRole.Host || config.DiagnosticGuestCaptureFixture ||
                 config.TemplateNetworkPrimaryCaptureOnlyFixture || config.IsolatedMenuInputDiagnostic))
                throw new InvalidDataException("Automatic diagnostic Diary pause requires its explicit two-game Battle47 Host targeting/ordinary/shared-HP lifetime cohort.");
            if (config.DiagnosticGuestCaptureFixture &&
                (config.Mode != "client-presentation-live" ||
                 config.DiagnosticGuestInput?.Role != DiagnosticInputRole.Guest || config.DiagnosticHostInput is not null ||
                 config.TemplateFile is not null || config.TemplateSha256 is not null || config.TemplateSharedPlayerProfile is not null ||
                 config.TemplateNetworkInputFixture || config.TemplateNetworkInputBackpressureFixture ||
                 config.TemplateNetworkOrdinaryAttackFixture || config.TemplateHostBackgroundFixture ||
                 config.TemplateProjectileManagerFixture || config.TemplateBodyInputFixture || config.TemplateOrdinaryAttackFixture ||
                 config.TemplatePlayerTraitsFixture || config.TemplatePlayerTraitsPickupFx || config.TemplateSharedHealthAlias ||
                 config.TemplateHealthBattleFixture || config.TemplateTargetRegionFixture || config.TemplateConfigureTargetRegion ||
                 config.TemplateStingerSecondaryRole || config.TemplateStarSecondaryEligibility || config.TemplateBattleCombatFixture ||
                 config.TemplateStarPlacementFixture || config.TemplateSharedMagicEnergyAlias || config.TemplateSecondAttackDegreeFixture ||
                 config.TemplateSharedBattleLifetime || config.TemplateDeathBattleFixture || config.TemplateStoryDeathFixture ||
                 config.TemplateCollisionDetectorSecondaryRole || config.PrepareFirstMagicStage))
                throw new InvalidDataException("Guest capture requires its separate Alfea28 client-presentation-live cohort, Guest endpoint and no Host/template/combat adapters.");
            if (config.TemplateNetworkInputBackpressureFixture && !config.TemplateNetworkInputFixture)
                throw new InvalidDataException("Backpressure requires its separate network input fixture.");
            if (config.TemplateNetworkInputDrainHoldMilliseconds is < 100 or > 650 ||
                config.TemplateNetworkInputDrainHoldMilliseconds != 300 && !config.TemplateNetworkInputBackpressureFixture)
                throw new InvalidDataException("Diagnostic drain hold must be100..650ms; nondefault duration requires its explicit backpressure fixture.");
            if (config.TemplateNetworkInputDrainHoldMilliseconds > 300 &&
                (!config.TemplateNetworkInputFixture || !config.TemplateNetworkInputBackpressureFixture ||
                 config.DiagnosticHostInput?.Role != DiagnosticInputRole.Host ||
                 config.DiagnosticGuestInput?.Role != DiagnosticInputRole.Guest ||
                 config.DiagnosticInputHeartbeatMilliseconds != 50))
                throw new InvalidDataException("Extended diagnostic drain hold requires same-game Host/Guest endpoints, its network/backpressure fixture and50ms source heartbeat.");
            if (config.TemplateNetworkInputFixture && !story3Host &&
                (config.Mode != "template-bloom-body-hair-idle" ||
                 config.TemplateSharedPlayerProfile != (config.TemplateNetworkOrdinaryAttackFixture ? "battle-01-network-input" : "alfea-28-network-input") && !IsCampaign(config) ||
                 config.DiagnosticHostInput?.Role != DiagnosticInputRole.Host ||
                 !config.TemplateProjectileManagerFixture || !config.TemplatePlayerTraitsFixture ||
                 !config.TemplatePlayerTraitsPickupFx || !config.TemplateTargetRegionFixture ||
                 !config.TemplateConfigureTargetRegion || !config.TemplateSharedHealthAlias ||
                 config.TemplateBodyInputFixture || config.TemplateOrdinaryAttackFixture ||
                 config.TemplateHealthBattleFixture != config.TemplateNetworkOrdinaryAttackFixture ||
                 config.TemplateSharedBattleLifetime != config.TemplateNetworkOrdinaryAttackFixture ||
                 config.TemplateBattleCombatFixture != config.TemplateNetworkOrdinaryAttackFixture ||
                 config.PrepareFirstMagicStage != config.TemplateNetworkOrdinaryAttackFixture || config.TemplateSecondAttackDegreeFixture ||
                 config.TemplateDeathBattleFixture || config.TemplateStoryDeathFixture ||
                 config.TemplateCollisionDetectorSecondaryRole ||
                 config.TemplateStingerSecondaryRole && !config.TemplateNetworkOrdinaryAttackFixture ||
                 config.TemplateStarSecondaryEligibility || config.TemplateStarPlacementFixture || config.TemplateSharedMagicEnergyAlias))
                throw new InvalidDataException("Network input requires its separate Alfea28 or explicit first-degree Battle47 retained manager/shared-HP fixture and Host IPC5 identity.");
            if (config.DiagnosticHostInput is not null && !config.TemplateNetworkInputFixture ||
                config.DiagnosticGuestInput is not null &&
                (config.DiagnosticGuestInput?.Role != DiagnosticInputRole.Guest ||
                 !(config.TemplateNetworkInputFixture || config.DiagnosticGuestCaptureFixture)))
                throw new InvalidDataException("Diagnostic IPC5 endpoints require the explicitly bounded network-input cohort.");
            if (config.TemplateBodyInputFixture &&
                (config.Mode != "template-bloom-body-hair-idle" || config.TemplateSharedPlayerProfile != "alfea-28-input" ||
                 !config.TemplateProjectileManagerFixture || !config.TemplatePlayerTraitsFixture ||
                 !config.TemplatePlayerTraitsPickupFx || !config.TemplateTargetRegionFixture ||
                 !config.TemplateConfigureTargetRegion || !config.TemplateSharedHealthAlias || config.TemplateOrdinaryAttackFixture || config.TemplateNetworkInputFixture ||
                 config.TemplateHealthBattleFixture || config.TemplateSharedBattleLifetime || config.TemplateBattleCombatFixture ||
                 config.PrepareFirstMagicStage || config.TemplateSecondAttackDegreeFixture ||
                 config.TemplateDeathBattleFixture || config.TemplateStoryDeathFixture ||
                 config.TemplateCollisionDetectorSecondaryRole || config.TemplateStingerSecondaryRole ||
                 config.TemplateStarSecondaryEligibility || config.TemplateStarPlacementFixture ||
                 config.TemplateSharedMagicEnergyAlias))
                throw new InvalidDataException("Body input requires its separate Alfea28 retained manager/shared-HP fixture without combat or reward adapters.");
            if (config.TemplateOrdinaryAttackFixture &&
                (config.Mode != "template-bloom-body-hair-idle" || config.TemplateSharedPlayerProfile != "battle-01-ordinary-attack" ||
                 !config.TemplateProjectileManagerFixture || !config.TemplatePlayerTraitsFixture ||
                 !config.TemplatePlayerTraitsPickupFx || !config.TemplateTargetRegionFixture ||
                 !config.TemplateConfigureTargetRegion || !config.TemplateSharedHealthAlias ||
                 !config.TemplateHealthBattleFixture || !config.TemplateSharedBattleLifetime ||
                 !config.TemplateBattleCombatFixture || !config.PrepareFirstMagicStage ||
                 config.TemplateBodyInputFixture || config.TemplateNetworkInputFixture || config.TemplateSecondAttackDegreeFixture ||
                 config.TemplateDeathBattleFixture || config.TemplateStoryDeathFixture ||
                 config.TemplateCollisionDetectorSecondaryRole || config.TemplateStingerSecondaryRole ||
                 config.TemplateStarSecondaryEligibility || config.TemplateStarPlacementFixture ||
                 config.TemplateSharedMagicEnergyAlias))
                throw new InvalidDataException("Ordinary attack requires its separate retained Battle47 first-degree manager/shared-HP fixture without other action adapters.");
            if (config.TemplateProjectileManagerFixture && !config.TemplateBodyInputFixture && !config.TemplateOrdinaryAttackFixture && !config.TemplateNetworkInputFixture &&
                (config.Mode != "template-bloom-body-hair-init" || !config.TemplatePlayerTraitsFixture ||
                 !config.TemplateTargetRegionFixture || config.TemplatePlayerTraitsPickupFx ||
                 config.TemplateConfigureTargetRegion || config.TemplateSharedHealthAlias ||
                 config.TemplateSharedPlayerProfile is not (null or "battle-01-projectile-init") || config.TemplateHealthBattleFixture ||
                 config.TemplateBattleCombatFixture || config.PrepareFirstMagicStage ||
                 config.TemplateSecondAttackDegreeFixture || config.TemplateSharedBattleLifetime ||
                 config.TemplateDeathBattleFixture || config.TemplateStoryDeathFixture ||
                 config.TemplateCollisionDetectorSecondaryRole || config.TemplateStingerSecondaryRole ||
                 config.TemplateStarSecondaryEligibility || config.TemplateStarPlacementFixture ||
                 config.TemplateSharedMagicEnergyAlias))
                throw new InvalidDataException("Projectile manager admission requires its separate Alfea or Battle scoped init/delete fixture without live input or resource adapters.");
            if (sharedProfile.Story && (!(config.TemplateSharedHealthAlias && config.Mode == "template-bloom-body-hair-idle") ||
                config.TemplateHealthBattleFixture || config.TemplateSharedBattleLifetime || config.TemplateDeathBattleFixture ||
                config.TemplateStingerSecondaryRole))
                throw new InvalidDataException("Story shared-player profiles require their separate shared-HP idle fixture without legacy battle selectors.");
            if (config.TemplateCollisionDetectorSecondaryRole && !(sharedProfile.Story && config.TemplateTargetRegionFixture &&
                config.TemplateConfigureTargetRegion && config.TemplateSharedHealthAlias))
                throw new InvalidDataException("CollisionDetector secondary role requires the explicit story/TargetRegion/shared-HP cohort.");
            if (config.TemplateStoryDeathFixture && !story3Host && !(sharedProfile.Story && config.TemplateCollisionDetectorSecondaryRole &&
                !config.TemplateStarSecondaryEligibility && !config.TemplateSharedMagicEnergyAlias))
                throw new InvalidDataException("Story death requires its separate CollisionDetector shared-player cohort without Star/magic goals.");
            if (config.PrepareFirstMagicStage && config.Mode != "shared-state-combat-live" && !config.TemplateBattleCombatFixture)
                throw new InvalidDataException("First-magic fixture initialization requires the explicit combat fixture mode.");
            if (config.TemplatePlayerTraitsFixture && !(config.Mode == "template-bloom-body-hair-init" ||
                config.Mode == "template-bloom-body-hair-idle" && config.TemplatePlayerTraitsPickupFx))
                throw new InvalidDataException("PlayerTraits body requires scoped init/delete or the explicit neutral scheduler fixture with native pickup setup.");
            if (config.TemplatePlayerTraitsPickupFx && !config.TemplatePlayerTraitsFixture)
                throw new InvalidDataException("PlayerTraits pickup setup requires the explicit PlayerTraits fixture.");
            if (config.TemplateSharedHealthAlias && !(config.TemplatePlayerTraitsFixture &&
                config.TemplatePlayerTraitsPickupFx && config.Mode == "template-bloom-body-hair-idle"))
                throw new InvalidDataException("Shared HP currently requires the separate neutral PlayerTraits scheduler fixture.");
            if (config.TemplateHealthBattleFixture && !config.TemplateSharedHealthAlias)
                throw new InvalidDataException("The battle HP fixture requires the explicit two-body shared health adapter.");
            if (config.TemplateTargetRegionFixture && !(config.TemplatePlayerTraitsFixture &&
                (config.Mode == "template-bloom-body-hair-init" || config.Mode == "template-bloom-body-hair-idle" &&
                config.TemplateConfigureTargetRegion && config.TemplateSharedHealthAlias)))
                throw new InvalidDataException("TargetRegion requires its separate scoped or neutral shared-health fixture.");
            if (config.TemplateConfigureTargetRegion && !(config.TemplateTargetRegionFixture && config.TemplatePlayerTraitsPickupFx))
                throw new InvalidDataException("Explicit TargetRegion setup requires its separate fixture and native pickup setup sequence.");
            if (config.TemplateMosquitoPlayerTargets &&
                (!config.TemplateNetworkInputFixture || !config.TemplateNetworkOrdinaryAttackFixture ||
                 !config.TemplateStingerSecondaryRole || !config.TemplateSharedBattleLifetime ||
                 config.TemplateSharedPlayerProfile != "battle-01-network-input" ||
                 config.DiagnosticWorldState?.Role != WorldStateRole.Host || config.DiagnosticHostRebind is not null))
                throw new InvalidDataException("Two-player Mosquito perception requires its explicit retained network Battle47 Host, Stinger role and unload-only lifecycle.");
            if (config.TemplateStingerSecondaryRole && !(config.TemplateTargetRegionFixture && config.TemplateConfigureTargetRegion &&
                config.TemplateSharedHealthAlias && config.TemplateHealthBattleFixture))
                throw new InvalidDataException("Own Stinger eligibility requires its explicit battle/TargetRegion/shared-health fixture.");
            if ((config.TemplateStarSecondaryEligibility || config.TemplateBattleCombatFixture) &&
                !(sharedProfile.LiveResources && config.TemplateSharedHealthAlias && config.Mode == "template-bloom-body-hair-idle"))
                throw new InvalidDataException("Star union and template combat preparation require the explicit battle/shared-health fixture.");
            if (config.TemplateStarPlacementFixture && !config.TemplateStarSecondaryEligibility)
                throw new InvalidDataException("One-time Star body placement requires the explicit Star union fixture.");
            if (config.TemplateSharedMagicEnergyAlias && !(sharedProfile.LiveResources && config.TemplateSharedHealthAlias))
                throw new InvalidDataException("Shared magic energy requires its explicit battle/shared-health fixture.");
            if (config.TemplateSecondAttackDegreeFixture && !(config.TemplateBattleCombatFixture && config.PrepareFirstMagicStage))
                throw new InvalidDataException("Second attack degree requires the explicit first-stage battle combat fixture.");
            if (config.TemplateSharedBattleLifetime && !(config.TemplateHealthBattleFixture && config.TemplateSharedHealthAlias))
                throw new InvalidDataException("Shared battle lifetime requires the explicit two-body battle fixture.");
            if (config.TemplateDeathBattleFixture && !(config.TemplateSharedBattleLifetime && config.TemplateStingerSecondaryRole &&
                !config.TemplateStarSecondaryEligibility && !config.TemplateSharedMagicEnergyAlias))
                throw new InvalidDataException("Shared death requires the separate retained Stinger battle fixture without Star/magic goals.");
            if (config.Mode is not ("observe" or "process-media-path-check" or "progression-save-check" or "shared-state-observe" or "shared-state-combat-live" or "control-observe" or "client-dispatch-observe" or "client-authority-gate" or "client-presentation-probe" or "client-presentation-live" or "native-call-check" or "visual-actor" or "visual-unload" or "template-create" or "template-init" or "template-bloom-body-init" or "template-bloom-body-hair-init" or "template-bloom-body-idle" or "template-bloom-body-hair-idle" or "template-bloom-body-hair-live" or "template-bloom-body-walk" or "template-bloom-body-hair-walk" or "template-bloom-body-turn" or "template-bloom-body-hair-turn" or "template-bloom-body-jump" or "template-bloom-body-hair-jump" or "template-idle" or "template-near-idle" or "template-near-walk" or "template-near-turn" or "template-bloom-idle" or "template-bloom-jump" or "template-bloom-walk" or "template-bloom-turn" or "controller-reset")) throw new InvalidDataException("Unknown probe mode");
            foreach (string path in new[] { config.Executable, config.WorkingDirectory, config.MediaDirectory, config.OutputDirectory })
                if (!Path.IsPathFullyQualified(path) || path.Any(c => c > 127))
                    throw new InvalidDataException("Explicit absolute ASCII paths required");
            if (!Directory.Exists(config.MediaDirectory) || !Directory.Exists(config.WorkingDirectory))
                throw new InvalidDataException("Prepared isolated game corpus required");
            if (config.Mode is "template-create" or "template-init" or "template-bloom-body-init" or "template-bloom-body-hair-init" or "template-bloom-body-idle" or "template-bloom-body-hair-idle" or "template-bloom-body-hair-live" or "template-bloom-body-walk" or "template-bloom-body-hair-walk" or "template-bloom-body-turn" or "template-bloom-body-hair-turn" or "template-bloom-body-jump" or "template-bloom-body-hair-jump" or "template-idle" or "template-near-idle" or "template-near-walk" or "template-near-turn" or "template-bloom-idle" or "template-bloom-jump" or "template-bloom-walk" or "template-bloom-turn") _ = NativeTemplateProbe.ReadVerifiedPayload(config);
            if (Directory.Exists(config.OutputDirectory) || File.Exists(config.OutputDirectory))
                throw new InvalidDataException("A fresh output directory is required");
            byte[] image = File.ReadAllBytes(config.Executable);
            string hash = Convert.ToHexString(SHA256.HashData(image));
            if (hash != OriginalSha256) throw new InvalidDataException("Unsupported original executable SHA256");
            using var pe = new PEReader(new MemoryStream(image, writable: false));
            if (pe.PEHeaders.CoffHeader.Machine != Machine.I386 || pe.PEHeaders.PEHeader?.ImageBase != 0x400000)
                throw new InvalidDataException("Expected the pinned x86 image at 0x00400000");
            ExecutableIdentification identification = ExecutableProfileCatalog.IdentifyAsync(config.Executable).GetAwaiter().GetResult();
            ExecutableProfile profile = identification.Profile ?? throw new InvalidDataException("Native signature profile unavailable");
            Directory.CreateDirectory(config.OutputDirectory);
            using var log = new StreamWriter(new FileStream(Path.Combine(config.OutputDirectory, "events.jsonl"), FileMode.CreateNew)) { AutoFlush = true };
            void Emit(object value)
            {
                if (!config.InteractiveSession)
                {
                    if (log.BaseStream.Position > 16 * 1024 * 1024) throw new IOException("Diagnostic log cap reached");
                    log.WriteLine(JsonSerializer.Serialize(new { utc = DateTime.UtcNow, data = value }));
                    return;
                }
                string record = JsonSerializer.Serialize(new { utc = DateTime.UtcNow, data = value });
                int bytes = checked(System.Text.Encoding.UTF8.GetByteCount(record) +
                    System.Text.Encoding.UTF8.GetByteCount(log.NewLine));
                if (bytes > InteractiveSessionLimits.ActorRecordBytes ||
                    log.BaseStream.Position > InteractiveSessionLimits.ActorJournalBytes - bytes)
                    throw new IOException("Interactive native journal reached its2GiB/1MiB record bound");
                log.WriteLine(record);
            }
            Emit(new { @event = "case", configuration = config, executableSha256 = hash,
                adapterMode = config.Mode switch { "control-observe" => "owned-debugger-control-observation",
                    "process-media-path-check" => "owned-debugger-process-local-media-resolver-check",
                    "progression-save-check" => "owned-debugger-native-save-authority-check",
                    "client-dispatch-observe" => "owned-debugger-client-dispatch-observation",
                    "client-authority-gate" => "owned-debugger-selected-client-authority-diagnostic",
                    "client-presentation-probe" or "client-presentation-live" => "owned-debugger-native-client-presentation",
                    "shared-state-observe" => "owned-debugger-readonly-battle-resource-observation",
                    "shared-state-combat-live" => "owned-debugger-native-combat-fixture",
                    "controller-reset" => "owned-debugger-native-movement-diagnostic", _ => "owned-debugger-stage-a-experiment" },
                gameWorldWrites = config.Mode.StartsWith("visual-") || config.Mode is "progression-save-check" or "shared-state-combat-live" or "client-authority-gate" or "client-presentation-probe" or "client-presentation-live" or "template-create" or "template-init" or "template-bloom-body-init" or "template-bloom-body-hair-init" or "template-bloom-body-idle" or "template-bloom-body-hair-idle" or "template-bloom-body-hair-live" or "template-bloom-body-walk" or "template-bloom-body-hair-walk" or "template-bloom-body-turn" or "template-bloom-body-hair-turn" or "template-bloom-body-jump" or "template-bloom-body-hair-jump" or "template-idle" or "template-near-idle" or "template-near-walk" or "template-near-turn" or "template-bloom-idle" or "template-bloom-jump" or "template-bloom-walk" or "template-bloom-turn" or "controller-reset",
                instrumentation = "verified software breakpoints and bounded main-thread native calls in owned process" });
            var host = new DebuggerHost(config.Executable, config.WorkingDirectory, Emit);
            using var observer = new ActorObserver(config, profile, pe, Emit);
            DebuggerRunResult result = host.Run(TimeSpan.FromSeconds(config.DurationSeconds), observer.Install, runner =>
            {
                observer.CheckTimeout();
                if (observer.ModeCompleted || (config.Mode is "visual-actor" or "visual-unload" or "template-create" or "template-init" or "template-bloom-body-init" or "template-bloom-body-hair-init" or "template-bloom-body-idle" or "template-bloom-body-hair-idle" or "template-bloom-body-hair-live" or "template-bloom-body-walk" or "template-bloom-body-hair-walk" or "template-bloom-body-turn" or "template-bloom-body-hair-turn" or "template-bloom-body-jump" or "template-bloom-body-hair-jump" or "template-idle" or "template-near-idle" or "template-near-walk" or "template-near-turn" or "template-bloom-idle" or "template-bloom-jump" or "template-bloom-walk" or "template-bloom-turn" or "controller-reset" && observer.ModeSucceeded))
                    runner.RequestSuccessfulCompletion();
            });
            observer.RetireDiagnosticInput();
            var report = new { kind = "WinxCoop-original-game-actor-observer", configuration = config,
                executableSha256 = hash, result, observer = observer.Summary,
                stageAComplete = false, gameWorldWrites = config.Mode.StartsWith("visual-") || config.Mode is "progression-save-check" or "shared-state-combat-live" or "client-authority-gate" or "client-presentation-probe" or "client-presentation-live" or "template-create" or "template-init" or "template-bloom-body-init" or "template-bloom-body-hair-init" or "template-bloom-body-idle" or "template-bloom-body-hair-idle" or "template-bloom-body-hair-live" or "template-bloom-body-walk" or "template-bloom-body-hair-walk" or "template-bloom-body-turn" or "template-bloom-body-hair-turn" or "template-bloom-body-jump" or "template-bloom-body-hair-jump" or "template-idle" or "template-near-idle" or "template-near-walk" or "template-near-turn" or "template-bloom-idle" or "template-bloom-jump" or "template-bloom-walk" or "template-bloom-turn" or "controller-reset", remoteActorCreated = observer.RemoteCreated };
            File.WriteAllText(Path.Combine(config.OutputDirectory, "report.json"), JsonSerializer.Serialize(report, Json));
            Console.WriteLine(JsonSerializer.Serialize(report));
            bool expectedExit = result.ExitCodeKnown &&
                ((result.TimedOut && result.ForcedTermination && result.ExitCode == 124) ||
                 (result.RequestedCompletion && !result.TimedOut && result.ForcedTermination && result.ExitCode == 0) ||
                 (!result.TimedOut && !result.ForcedTermination && result.ExitCode == 0));
            return result.CleanupConfirmed && result.FatalExceptions == 0 && result.Error is null &&
                expectedExit && (config.Mode == "process-media-path-check" ? observer.ProcessMediaPathVerified :
                    config.Mode == "progression-save-check" ? observer.NativeProgressionSaveCheckQualified : observer.ReadySamples > 0) &&
                observer.ModeSucceeded ? 0 : 4;
        }
        catch (Exception exception)
        {
            Console.Error.WriteLine(exception);
            return 3;
        }
    }
}
