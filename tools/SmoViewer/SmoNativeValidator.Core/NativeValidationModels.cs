namespace SmoNativeValidator.Core;

public enum NativeValidationRoute
{
    Contextual = 0,
    FastGeneric = 1
}

public static class NativeValidationDefaults
{
    public const string FastTriggerGameAssetPath = @"Menus\mousecursor.smo";
    public const int ContextualStartLevel = 2;
}

public enum NativeValidationStatus
{
    Passed,
    Crash,
    Timeout,
    EngineRejected,
    PathError,
    InstrumentationUnavailable,
    Cancelled,
    LaunchFailed,
    TargetNotRequested,
    Inconclusive
}

public enum NativeCrashPhase
{
    BeforeTrigger,
    DuringTargetLoad,
    TargetLoadBackgroundOrUnwind,
    PostReturnSurvivalWindow,
    AfterTriggerUnattributed
}

public enum NativeCrashAttributionConfidence
{
    None,
    Possible,
    Direct
}

public enum NativeValidationStage
{
    Preparing,
    LocatingExecutable,
    Fingerprinting,
    StagingAsset,
    LaunchingEngine,
    MediaPathInitialization,
    AssetPathBuild,
    ResourceLoad,
    FfpsHeader,
    FfpsVersion,
    SceneInitialization,
    Runtime,
    Cleanup,
    Completed
}

public enum NativeValidationSeverity
{
    Trace = 0,
    Information = 1,
    Success = 2,
    Warning = 3,
    Error = 4
}

public enum NativeValidationEventKind
{
    SessionStarted,
    Diagnostic,
    ExecutableFingerprint,
    AssetStaged,
    ProcessStarted,
    CheckpointEnter,
    CheckpointReturn,
    PathObserved,
    PathRedirected,
    Exception,
    ProcessExited,
    Timeout,
    Cancelled,
    Cleanup,
    SessionCompleted
}

public sealed class NativeAssetPathException : IOException
{
    public NativeAssetPathException(string message) : base(message)
    {
    }

    public NativeAssetPathException(string message, Exception innerException) : base(message, innerException)
    {
    }
}

public sealed record NativeValidationEvent
{
    public required long Sequence { get; init; }
    public required DateTimeOffset TimestampUtc { get; init; }
    public required TimeSpan Elapsed { get; init; }
    public required NativeValidationEventKind Kind { get; init; }
    public required NativeValidationStage Stage { get; init; }
    public required NativeValidationSeverity Severity { get; init; }
    public required string Message { get; init; }
    public double? ProgressPercent { get; init; }
    public ulong? Address { get; init; }
    public uint? ThreadId { get; init; }
    public string? Checkpoint { get; init; }
    public IReadOnlyDictionary<string, string?> Data { get; init; } =
        new Dictionary<string, string?>();
}

public sealed record NativeExceptionSnapshot
{
    public required uint Code { get; init; }
    public required bool FirstChance { get; init; }
    public required uint ThreadId { get; init; }
    public required uint InstructionPointer { get; init; }
    public IReadOnlyDictionary<string, uint> Registers { get; init; } =
        new Dictionary<string, uint>();
    public IReadOnlyList<uint> StackWords { get; init; } = [];
}

public sealed record NativeValidationRequest
{
    public required string ExecutablePath { get; init; }
    public required string AssetPath { get; init; }
    /// <summary>
    /// The model's original Media-relative game path. This remains context metadata
    /// even when the fast route redirects an earlier, generic resource request.
    /// </summary>
    public required string LogicalGameAssetPath { get; init; }
    // Keep existing API/CLI callers contextual; new Viewer/settings presets explicitly
    // select the recommended FastGeneric route.
    public NativeValidationRoute Route { get; init; } = NativeValidationRoute.Contextual;
    /// <summary>
    /// Optional explicit assertion of the route's trigger path. FastGeneric accepts
    /// only Menus\mousecursor.smo; Contextual accepts only LogicalGameAssetPath.
    /// Leave null to let the route select its safe trigger.
    /// </summary>
    public string? TriggerGameAssetPath { get; init; }
    /// <summary>
    /// Optional native startLevel value. It is applied only to an isolated contextual
    /// launch workspace. Null preserves the game's normal startup sequence.
    /// </summary>
    public int? StartLevel { get; init; }
    /// <summary>
    /// Creates an owned working directory with windowed/non-interactive INI files and
    /// a private shader copy. Native validation is windowed-only, so disabling this
    /// option is rejected before the game process can start.
    /// </summary>
    public bool UseIsolatedLaunchWorkspace { get; init; } = true;
    public string? Arguments { get; init; }
    public string? WorkingDirectory { get; init; }
    public string? LogFilePath { get; init; }
    public TimeSpan OverallTimeout { get; init; } = TimeSpan.FromMinutes(2);
    public TimeSpan NoProgressTimeout { get; init; } = TimeSpan.FromSeconds(30);
    public TimeSpan SurvivalWindow { get; init; } = TimeSpan.FromSeconds(2);
    public bool IncludeBloomCheckpoints { get; init; }
    public bool CollectFirstChanceExceptions { get; init; } = true;
    public bool StageAsset { get; init; } = true;
    public bool AllowFileNameOnlyLogicalPath { get; init; }
}

public sealed record NativeValidationReport
{
    public required NativeValidationStatus Status { get; init; }
    public required string Summary { get; init; }
    public required DateTimeOffset StartedUtc { get; init; }
    public required DateTimeOffset FinishedUtc { get; init; }
    public required string ExecutablePath { get; init; }
    public string? ExecutableSha256 { get; init; }
    public ExecutableProfile? Profile { get; init; }
    public required string AssetPath { get; init; }
    public required string LogicalGameAssetPath { get; init; }
    public required NativeValidationRoute Route { get; init; }
    public required string TriggerGameAssetPath { get; init; }
    public int? StartLevel { get; init; }
    public bool UsedIsolatedLaunchWorkspace { get; init; }
    public string? LaunchWorkingDirectory { get; init; }
    public string? RedirectedAssetPath { get; init; }
    public string? LogFilePath { get; init; }
    public string? LastCheckpoint { get; init; }
    public string? LastTargetCheckpoint { get; init; }
    public bool TargetFfpsHeaderEntered { get; init; }
    public bool TargetFfpsMagicAccepted { get; init; }
    public bool TargetFfpsVersionAccepted { get; init; }
    public NativeExceptionSnapshot? Exception { get; init; }
    /// <summary>
    /// Loader phase in which the fatal exception was observed. Null when no
    /// second-chance exception was captured.
    /// </summary>
    public NativeCrashPhase? CrashPhase { get; init; }
    /// <summary>
    /// Strength of the selected-model correlation. A post-return fault is only
    /// Possible: temporal proximity during the survival window is not proof that
    /// the redirected model caused the fault.
    /// </summary>
    public NativeCrashAttributionConfidence? CrashAttributionConfidence { get; init; }
    public int? ExitCode { get; init; }
    public int RedirectCount { get; init; }
    public IReadOnlyList<NativeValidationEvent> Events { get; init; } = [];
    public TimeSpan Duration => FinishedUtc - StartedUtc;
}
