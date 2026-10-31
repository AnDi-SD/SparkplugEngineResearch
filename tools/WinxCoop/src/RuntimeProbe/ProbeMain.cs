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
    DiagnosticInputIdentity? DiagnosticGuestInput = null, bool TemplateNetworkInputBackpressureFixture = false);

internal static class ProbeMain
{
    internal const string OriginalSha256 = "3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F";
    private static readonly JsonSerializerOptions Json = new() { WriteIndented = true };

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
            if (config.DurationSeconds is < 5 or > 600 || config.SampleEvery is < 1 or > 1000)
                throw new InvalidDataException("Duration must be 5..600 seconds, sample interval 1..1000 updates");
            NativeSharedPlayerProfile sharedProfile = NativeSharedPlayerProfile.Select(config);
            if (config.TemplateNetworkInputBackpressureFixture && !config.TemplateNetworkInputFixture)
                throw new InvalidDataException("Backpressure requires its separate network input fixture.");
            if (config.TemplateNetworkInputFixture &&
                (config.Mode != "template-bloom-body-hair-idle" || config.TemplateSharedPlayerProfile != "alfea-28-network-input" ||
                 config.DiagnosticHostInput?.Role != DiagnosticInputRole.Host ||
                 !config.TemplateProjectileManagerFixture || !config.TemplatePlayerTraitsFixture ||
                 !config.TemplatePlayerTraitsPickupFx || !config.TemplateTargetRegionFixture ||
                 !config.TemplateConfigureTargetRegion || !config.TemplateSharedHealthAlias ||
                 config.TemplateBodyInputFixture || config.TemplateOrdinaryAttackFixture ||
                 config.TemplateHealthBattleFixture || config.TemplateSharedBattleLifetime || config.TemplateBattleCombatFixture ||
                 config.PrepareFirstMagicStage || config.TemplateSecondAttackDegreeFixture ||
                 config.TemplateDeathBattleFixture || config.TemplateStoryDeathFixture ||
                 config.TemplateCollisionDetectorSecondaryRole || config.TemplateStingerSecondaryRole ||
                 config.TemplateStarSecondaryEligibility || config.TemplateStarPlacementFixture || config.TemplateSharedMagicEnergyAlias))
                throw new InvalidDataException("Network input requires its separate Alfea28 retained manager/shared-HP fixture and Host IPC4 identity.");
            if (config.DiagnosticHostInput is not null && !config.TemplateNetworkInputFixture ||
                config.DiagnosticGuestInput is not null &&
                (config.DiagnosticGuestInput?.Role != DiagnosticInputRole.Guest || !config.TemplateNetworkInputFixture))
                throw new InvalidDataException("Diagnostic IPC4 endpoints require the explicitly bounded network-input cohort.");
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
            if (config.TemplateStoryDeathFixture && !(sharedProfile.Story && config.TemplateCollisionDetectorSecondaryRole &&
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
            if (config.Mode is not ("observe" or "shared-state-observe" or "shared-state-combat-live" or "control-observe" or "client-dispatch-observe" or "client-authority-gate" or "client-presentation-probe" or "client-presentation-live" or "native-call-check" or "visual-actor" or "visual-unload" or "template-create" or "template-init" or "template-bloom-body-init" or "template-bloom-body-hair-init" or "template-bloom-body-idle" or "template-bloom-body-hair-idle" or "template-bloom-body-hair-live" or "template-bloom-body-walk" or "template-bloom-body-hair-walk" or "template-bloom-body-turn" or "template-bloom-body-hair-turn" or "template-bloom-body-jump" or "template-bloom-body-hair-jump" or "template-idle" or "template-near-idle" or "template-near-walk" or "template-near-turn" or "template-bloom-idle" or "template-bloom-jump" or "template-bloom-walk" or "template-bloom-turn" or "controller-reset")) throw new InvalidDataException("Unknown probe mode");
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
                if (log.BaseStream.Position > 16 * 1024 * 1024) throw new IOException("Diagnostic log cap reached");
                log.WriteLine(JsonSerializer.Serialize(new { utc = DateTime.UtcNow, data = value }));
            }
            Emit(new { @event = "case", configuration = config, executableSha256 = hash,
                adapterMode = config.Mode switch { "control-observe" => "owned-debugger-control-observation",
                    "client-dispatch-observe" => "owned-debugger-client-dispatch-observation",
                    "client-authority-gate" => "owned-debugger-selected-client-authority-diagnostic",
                    "client-presentation-probe" or "client-presentation-live" => "owned-debugger-native-client-presentation",
                    "shared-state-observe" => "owned-debugger-readonly-battle-resource-observation",
                    "shared-state-combat-live" => "owned-debugger-native-combat-fixture",
                    "controller-reset" => "owned-debugger-native-movement-diagnostic", _ => "owned-debugger-stage-a-experiment" },
                gameWorldWrites = config.Mode.StartsWith("visual-") || config.Mode is "shared-state-combat-live" or "client-authority-gate" or "client-presentation-probe" or "client-presentation-live" or "template-create" or "template-init" or "template-bloom-body-init" or "template-bloom-body-hair-init" or "template-bloom-body-idle" or "template-bloom-body-hair-idle" or "template-bloom-body-hair-live" or "template-bloom-body-walk" or "template-bloom-body-hair-walk" or "template-bloom-body-turn" or "template-bloom-body-hair-turn" or "template-bloom-body-jump" or "template-bloom-body-hair-jump" or "template-idle" or "template-near-idle" or "template-near-walk" or "template-near-turn" or "template-bloom-idle" or "template-bloom-jump" or "template-bloom-walk" or "template-bloom-turn" or "controller-reset",
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
                stageAComplete = false, gameWorldWrites = config.Mode.StartsWith("visual-") || config.Mode is "shared-state-combat-live" or "client-authority-gate" or "client-presentation-probe" or "client-presentation-live" or "template-create" or "template-init" or "template-bloom-body-init" or "template-bloom-body-hair-init" or "template-bloom-body-idle" or "template-bloom-body-hair-idle" or "template-bloom-body-hair-live" or "template-bloom-body-walk" or "template-bloom-body-hair-walk" or "template-bloom-body-turn" or "template-bloom-body-hair-turn" or "template-bloom-body-jump" or "template-bloom-body-hair-jump" or "template-idle" or "template-near-idle" or "template-near-walk" or "template-near-turn" or "template-bloom-idle" or "template-bloom-jump" or "template-bloom-walk" or "template-bloom-turn" or "controller-reset", remoteActorCreated = observer.RemoteCreated };
            File.WriteAllText(Path.Combine(config.OutputDirectory, "report.json"), JsonSerializer.Serialize(report, Json));
            Console.WriteLine(JsonSerializer.Serialize(report));
            bool expectedExit = result.ExitCodeKnown &&
                ((result.TimedOut && result.ForcedTermination && result.ExitCode == 124) ||
                 (result.RequestedCompletion && !result.TimedOut && result.ForcedTermination && result.ExitCode == 0) ||
                 (!result.TimedOut && !result.ForcedTermination && result.ExitCode == 0));
            return result.CleanupConfirmed && result.FatalExceptions == 0 && result.Error is null &&
                expectedExit && observer.ReadySamples > 0 && observer.ModeSucceeded ? 0 : 4;
        }
        catch (Exception exception)
        {
            Console.Error.WriteLine(exception);
            return 3;
        }
    }
}
