using System.Reflection.PortableExecutable;
using System.Security.Cryptography;
using System.Text.Json;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

internal sealed record ProbeConfiguration(
    string Executable, string WorkingDirectory, string MediaDirectory,
    string OutputDirectory, int DurationSeconds = 90, int SampleEvery = 15, string Mode = "observe",
    string? TemplateFile = null, string? TemplateSha256 = null);

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
            if (config.Mode is not ("observe" or "control-observe" or "native-call-check" or "visual-actor" or "visual-unload" or "template-create" or "template-init" or "controller-reset")) throw new InvalidDataException("Unknown probe mode");
            foreach (string path in new[] { config.Executable, config.WorkingDirectory, config.MediaDirectory, config.OutputDirectory })
                if (!Path.IsPathFullyQualified(path) || path.Any(c => c > 127))
                    throw new InvalidDataException("Explicit absolute ASCII paths required");
            if (!Directory.Exists(config.MediaDirectory) || !Directory.Exists(config.WorkingDirectory))
                throw new InvalidDataException("Prepared isolated game corpus required");
            if (config.Mode is "template-create" or "template-init") _ = NativeTemplateProbe.ReadVerifiedPayload(config);
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
                    "controller-reset" => "owned-debugger-native-movement-diagnostic", _ => "owned-debugger-stage-a-experiment" },
                gameWorldWrites = config.Mode.StartsWith("visual-") || config.Mode is "template-create" or "template-init" or "controller-reset",
                instrumentation = "verified software breakpoints and bounded main-thread native calls in owned process" });
            var host = new DebuggerHost(config.Executable, config.WorkingDirectory, Emit);
            var observer = new ActorObserver(config, profile, pe, Emit);
            DebuggerRunResult result = host.Run(TimeSpan.FromSeconds(config.DurationSeconds), observer.Install, runner =>
            {
                observer.CheckTimeout();
                if (config.Mode is "visual-actor" or "visual-unload" or "template-create" or "template-init" or "controller-reset" && observer.ModeSucceeded)
                    runner.RequestSuccessfulCompletion();
            });
            var report = new { kind = "WinxCoop-original-game-actor-observer", configuration = config,
                executableSha256 = hash, result, observer = observer.Summary,
                stageAComplete = false, gameWorldWrites = config.Mode.StartsWith("visual-") || config.Mode is "template-create" or "template-init" or "controller-reset", remoteActorCreated = observer.RemoteCreated };
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
