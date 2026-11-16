namespace WinxCoop.RuntimeProbe;

/// <summary>Finite work budget; owner leases and operation deadlines remain independent.</summary>
internal static class NativeObservationBudget
{
    private static bool _configured, _interactive;
    private static int _duration;
    internal static uint WorldLimit { get; private set; } = 36000;
    internal static uint InputReturnLimit { get; private set; } = 1000000;
    internal static uint SnapshotLimit { get; private set; } = 100000;

    internal static bool DurationAllowed(ProbeConfiguration config)
    {
        if (!InteractiveSessionLimits.ActorDurationAllowed(config.DurationSeconds, config.InteractiveSession)) return false;
        if (!config.InteractiveSession) return true;
        // An isolated diagnostic cannot acquire the longer lifetime by raising
        // DurationSeconds. It needs the same explicit process-local role cohort.
        if (!config.EnableProgressionAuthority || !config.EnableWorldEntry || !config.UseProcessLocalMediaPath ||
            config.ProgressionControl is not { OwnerPid: > 0, OwnerCreationFileTime: > 0 } ||
            config.DiagnosticWorldState is not { LevelEpoch: > 0, Generation: > 0 } world) return false;
        bool host = world.Role == WorldStateRole.Host;
        DiagnosticInputIdentity? input = host ? config.DiagnosticHostInput : config.DiagnosticGuestInput;
        return (host ? config.DiagnosticGuestInput is null && config.TemplateNetworkInputFixture &&
                config.Mode == "template-bloom-body-hair-idle"
            : world.Role == WorldStateRole.Guest && config.DiagnosticHostInput is null &&
                config.DiagnosticGuestCaptureFixture && config.Mode == "client-presentation-live") &&
            input is { } admitted && admitted.Role == (host ? DiagnosticInputRole.Host : DiagnosticInputRole.Guest) &&
            admitted.SessionId == world.SessionId && admitted.ScenarioId == world.ScenarioId &&
            admitted.LevelEpoch == world.LevelEpoch && admitted.BodyGeneration == world.Generation &&
            (config.ProgressionAuthorityRole is null || config.ProgressionAuthorityRole == world.Role);
    }

    // Scalar-only lifetime helpers remain tied to this once-admitted Actor.
    internal static bool DurationAllowed(int seconds) => seconds is >= 5 and <= 600 ||
        _configured && _interactive && seconds == _duration;
    internal static int OperationSeconds(ProbeConfiguration config) =>
        config.InteractiveSession ? Math.Min(config.DurationSeconds, InteractiveSessionLimits.LegacyMaximumSeconds) : config.DurationSeconds;

    internal static void Configure(ProbeConfiguration config)
    {
        if (_configured || !DurationAllowed(config))
            throw new InvalidOperationException("Observation budget must be configured once before the owned run.");
        if (config.DiagnosticWorldState is not null)
            WorldLimit = checked((uint)Math.Max(36000, config.DurationSeconds * 1000));
        if (config.InteractiveSession)
        {
            InputReturnLimit = Math.Max(1000000u, InteractiveSessionLimits.InputSamples(config.DurationSeconds));
            SnapshotLimit = Math.Max(100000u, InteractiveSessionLimits.Snapshots(config.DurationSeconds));
        }
        _duration = config.DurationSeconds; _interactive = config.InteractiveSession; _configured = true;
    }
}
