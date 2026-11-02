namespace WinxCoop.RuntimeProbe;

/// <summary>Finite diagnostic work budget; wall-clock ownership remains independent.</summary>
internal static class NativeObservationBudget
{
    private static bool _configured;
    internal static uint WorldLimit { get; private set; } = 36000;

    internal static void Configure(ProbeConfiguration config)
    {
        if (_configured || config.DurationSeconds is < 5 or > 600)
            throw new InvalidOperationException("Observation budget must be configured once before the owned run.");
        // Original diagnostics keep their established bound. WCS1 may span a
        // bounded manual session; uncapped original games can exceed 36k
        // updates well before that session's wall-clock deadline.
        if (config.DiagnosticWorldState is not null)
            WorldLimit = checked((uint)Math.Max(36000, config.DurationSeconds * 1000));
        _configured = true;
    }
}
