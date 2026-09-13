using System.Diagnostics;

namespace SmoNativeValidator.Core;

public sealed class WinxClubNativeValidator
{
    public async Task<NativeValidationReport> ValidateAsync(
        NativeValidationRequest request,
        IProgress<NativeValidationEvent>? progress = null,
        CancellationToken cancellationToken = default)
    {
        ArgumentNullException.ThrowIfNull(request);
        DateTimeOffset startedUtc = DateTimeOffset.UtcNow;
        Stopwatch stopwatch = Stopwatch.StartNew();
        NativeValidationTracker tracker = new();
        NativeSessionLog log = CreateSessionLogWithFallback(request.LogFilePath);
        using (log)
        {
            ValidationEventSink events = new(startedUtc, stopwatch, log, tracker, progress);
            events.Emit(
                NativeValidationEventKind.SessionStarted,
                NativeValidationStage.Preparing,
                NativeValidationSeverity.Information,
                "Native WinxClub.exe validation session started.",
                progressPercent: 0,
                data: new Dictionary<string, string?>
                {
                    ["executablePath"] = request.ExecutablePath,
                    ["assetPath"] = request.AssetPath,
                    ["logicalGameAssetPath"] = request.LogicalGameAssetPath,
                    ["requestedRoute"] = request.Route.ToString(),
                    ["requestedTriggerGameAssetPath"] = request.TriggerGameAssetPath,
                    ["requestedStartLevel"] = request.StartLevel?.ToString(),
                    ["requireSceneReady"] = request.RequireSceneReady
                        .ToString().ToLowerInvariant(),
                    ["isolatedLaunchWorkspace"] = request.UseIsolatedLaunchWorkspace
                        .ToString().ToLowerInvariant(),
                    ["safety"] = "Registry, game media and executable files are never modified."
                });

            NativeValidationStatus status = NativeValidationStatus.LaunchFailed;
            string summary = "Validation did not start.";
            ExecutableIdentification? identification = null;
            NativeDebuggerRunResult? debugResult = null;
            StagedAssetSession? stagedAsset = null;
            NativeLaunchWorkspace? launchWorkspace = null;
            ResolvedNativeValidationRoute? resolvedRoute = null;
            string? redirectPath = null;
            string? launchWorkingDirectory = null;
            string? mediaRootPath = null;
            int? appliedStartLevel = null;

            try
            {
                ValidateRequest(request);
                resolvedRoute = NativeValidationRouting.Resolve(request);
                cancellationToken.ThrowIfCancellationRequested();

                events.Emit(
                    NativeValidationEventKind.Diagnostic,
                    NativeValidationStage.Preparing,
                    NativeValidationSeverity.Information,
                    resolvedRoute.Route == NativeValidationRoute.FastGeneric
                        ? $"Fast native route will use early trigger '{resolvedRoute.TriggerGameAssetPath}'."
                        : $"Contextual native route will wait for '{resolvedRoute.TriggerGameAssetPath}'.",
                    progressPercent: 2,
                    data: new Dictionary<string, string?>
                    {
                        ["route"] = resolvedRoute.Route.ToString(),
                        ["logicalGameAssetPath"] = request.LogicalGameAssetPath,
                        ["triggerGameAssetPath"] = resolvedRoute.TriggerGameAssetPath,
                        ["requestedStartLevel"] = resolvedRoute.RequestedStartLevel?.ToString()
                    });

                events.Emit(
                    NativeValidationEventKind.Diagnostic,
                    NativeValidationStage.Fingerprinting,
                    NativeValidationSeverity.Information,
                    "Analyzing executable PE sections and resolving internal loader code signatures.",
                    progressPercent: 5);
                identification = await ExecutableProfileCatalog.IdentifyAsync(
                    request.ExecutablePath,
                    cancellationToken).ConfigureAwait(false);
                events.Emit(
                    NativeValidationEventKind.ExecutableFingerprint,
                    NativeValidationStage.Fingerprinting,
                    identification.IsSupported
                        ? NativeValidationSeverity.Success
                        : NativeValidationSeverity.Error,
                    identification.IsSupported
                        ? $"{identification.Profile!.DisplayName}."
                        : identification.Error ?? "Internal loader instrumentation could not be resolved.",
                    progressPercent: 10,
                    data: new Dictionary<string, string?>
                    {
                        ["diagnosticSha256"] = identification.Sha256,
                        ["fileLength"] = identification.FileLength.ToString(),
                        ["peMachine"] = identification.PeMachine is ushort machine
                            ? $"0x{machine:X4}"
                            : null,
                        ["profile"] = identification.Profile?.Id,
                        ["supported"] = identification.IsSupported.ToString().ToLowerInvariant(),
                        ["supportDecision"] = "pe32-x86-and-internal-code-signatures",
                        ["resolvedSignatures"] = identification.SignatureResolutions.Count(
                            resolution => resolution.ResolvedRva is not null).ToString(),
                        ["signatureDiagnostics"] = string.Join("; ", identification.SignatureResolutions
                            .Where(resolution => resolution.Diagnostic is not null)
                            .Select(resolution => $"{resolution.Id}: {resolution.Diagnostic}"))
                    });

                if (!identification.IsSupported || identification.Profile is null)
                {
                    status = NativeValidationStatus.InstrumentationUnavailable;
                    summary = identification.Error ??
                        "Required internal loader signatures could not be resolved safely.";
                }
                else
                {
                    cancellationToken.ThrowIfCancellationRequested();
                    if (request.StageAsset)
                    {
                        stagedAsset = StagedAssetSession.Create(
                            request.AssetPath,
                            resolvedRoute.TriggerGameAssetPath);
                        redirectPath = stagedAsset.AssetPath;
                        events.Emit(
                            NativeValidationEventKind.AssetStaged,
                            NativeValidationStage.StagingAsset,
                            NativeValidationSeverity.Success,
                            $"Staged SMO and {stagedAsset.StagedFiles.Count - 1} sidecar(s) in a short session path.",
                            progressPercent: 15,
                            data: new Dictionary<string, string?>
                            {
                                ["stagedAssetPath"] = stagedAsset.AssetPath,
                                ["stagedFiles"] = string.Join(";", stagedAsset.StagedFiles),
                                ["cleanup"] = "session-finally"
                            });
                    }
                    else
                    {
                        redirectPath = Path.GetFullPath(request.AssetPath);
                        if (!StagedAssetSession.IsAscii(redirectPath))
                            throw new NativeAssetPathException("The direct redirect path is not ASCII; enable asset staging.");
                    }

                    if (request.UseIsolatedLaunchWorkspace)
                    {
                        launchWorkspace = NativeLaunchWorkspace.Create(
                            request.ExecutablePath,
                            request.WorkingDirectory,
                            resolvedRoute.Route == NativeValidationRoute.Contextual
                                ? resolvedRoute.RequestedStartLevel
                                : null);
                        launchWorkingDirectory = launchWorkspace.WorkingDirectory;
                        mediaRootPath = launchWorkspace.MediaSourceDirectory;
                        appliedStartLevel = launchWorkspace.StartLevel;
                        events.Emit(
                            NativeValidationEventKind.Diagnostic,
                            NativeValidationStage.LaunchingEngine,
                            NativeValidationSeverity.Success,
                            "Prepared an isolated windowed launch workspace with a private shader copy.",
                            progressPercent: 20,
                            data: new Dictionary<string, string?>
                            {
                                ["workingDirectory"] = launchWorkspace.WorkingDirectory,
                                ["mediaSourceDirectory"] = launchWorkspace.MediaSourceDirectory,
                                ["shaderSourceDirectory"] = launchWorkspace.ShaderSourceDirectory,
                                ["copiedShaderFiles"] = launchWorkspace.CopiedShaderFiles.Count.ToString(),
                                ["fullScreen"] = "false",
                                ["showCinematics"] = "false",
                                ["mouseExclusive"] = "false",
                                ["appliedStartLevel"] = appliedStartLevel?.ToString(),
                                ["cleanup"] = "session-finally"
                            });
                    }
                    else
                    {
                        launchWorkingDirectory = ResolveWorkingDirectory(request);
                    }

                    NativeDebuggerHarness harness = new(
                        request,
                        identification.Profile,
                        redirectPath,
                        resolvedRoute.TriggerGameAssetPath,
                        launchWorkingDirectory,
                        mediaRootPath ?? throw new InvalidOperationException(
                            "The isolated Media source was not resolved."),
                        events,
                        stopwatch,
                        cancellationToken);
                    // Win32 debugging is thread-affine. A dedicated thread also makes
                    // DebugSetProcessKillOnExit a real last-resort guard if detach fails.
                    debugResult = await Task.Factory.StartNew(
                        harness.Run,
                        CancellationToken.None,
                        TaskCreationOptions.LongRunning,
                        TaskScheduler.Default).ConfigureAwait(false);
                    status = debugResult.Status;
                    summary = debugResult.Summary;
                }
            }
            catch (OperationCanceledException)
            {
                status = NativeValidationStatus.Cancelled;
                summary = "Validation was cancelled before or during native execution.";
                events.Emit(
                    NativeValidationEventKind.Cancelled,
                    NativeValidationStage.Cleanup,
                    NativeValidationSeverity.Warning,
                    summary);
            }
            catch (Exception exception) when (
                exception is ArgumentException or IOException or UnauthorizedAccessException)
            {
                status = exception is NativeAssetPathException
                    ? NativeValidationStatus.PathError
                    : NativeValidationStatus.LaunchFailed;
                summary = exception.Message;
                events.Emit(
                    NativeValidationEventKind.Diagnostic,
                    NativeValidationStage.Preparing,
                    NativeValidationSeverity.Error,
                    exception.ToString());
            }
            finally
            {
                if (launchWorkspace is not null)
                {
                    string workspaceDirectory = launchWorkspace.SessionDirectory;
                    launchWorkspace.Dispose();
                    bool cleanupFailed = Directory.Exists(workspaceDirectory);
                    events.Emit(
                        NativeValidationEventKind.Cleanup,
                        NativeValidationStage.Cleanup,
                        cleanupFailed
                            ? NativeValidationSeverity.Warning
                            : NativeValidationSeverity.Information,
                        cleanupFailed
                            ? "The owned launch workspace is still present; no game installation file was modified."
                            : "Removed the owned isolated launch workspace.",
                        data: new Dictionary<string, string?>
                        {
                            ["sessionDirectory"] = workspaceDirectory,
                            ["existsAfterCleanup"] = cleanupFailed.ToString().ToLowerInvariant()
                        });
                }
                if (stagedAsset is not null)
                {
                    string stagedDirectory = stagedAsset.SessionDirectory;
                    stagedAsset.Dispose();
                    bool cleanupFailed = Directory.Exists(stagedDirectory);
                    events.Emit(
                        NativeValidationEventKind.Cleanup,
                        NativeValidationStage.Cleanup,
                        cleanupFailed
                            ? NativeValidationSeverity.Warning
                            : NativeValidationSeverity.Information,
                        cleanupFailed
                            ? "The owned staging directory is still present; no game or source file was modified."
                            : "Removed the owned staging session directory.",
                        data: new Dictionary<string, string?>
                        {
                            ["sessionDirectory"] = stagedDirectory,
                            ["existsAfterCleanup"] = cleanupFailed.ToString().ToLowerInvariant()
                        });
                }
            }

            events.Emit(
                NativeValidationEventKind.SessionCompleted,
                NativeValidationStage.Completed,
                status == NativeValidationStatus.Passed
                    ? NativeValidationSeverity.Success
                    : status is NativeValidationStatus.Cancelled or
                        NativeValidationStatus.TargetNotRequested or
                        NativeValidationStatus.Inconclusive
                        ? NativeValidationSeverity.Warning
                        : NativeValidationSeverity.Error,
                summary,
                progressPercent: 100,
                checkpoint: tracker.LastTargetCheckpoint ?? tracker.LastCheckpoint,
                data: new Dictionary<string, string?>
                {
                    ["status"] = status.ToString(),
                    ["route"] = resolvedRoute?.Route.ToString() ?? request.Route.ToString(),
                    ["triggerGameAssetPath"] = resolvedRoute?.TriggerGameAssetPath,
                    ["appliedStartLevel"] = appliedStartLevel?.ToString(),
                    ["mediaSourceDirectory"] = mediaRootPath,
                    ["redirectCount"] = tracker.RedirectCount.ToString(),
                    ["sceneReadyReached"] = tracker.SceneReadyReached
                        .ToString().ToLowerInvariant(),
                    ["logFilePath"] = log.FilePath
                });

            return new NativeValidationReport
            {
                Status = status,
                Summary = summary,
                StartedUtc = startedUtc,
                FinishedUtc = DateTimeOffset.UtcNow,
                ExecutablePath = Path.GetFullPath(request.ExecutablePath),
                ExecutableSha256 = identification?.Sha256,
                Profile = identification?.Profile,
                AssetPath = Path.GetFullPath(request.AssetPath),
                LogicalGameAssetPath = request.LogicalGameAssetPath,
                Route = resolvedRoute?.Route ?? request.Route,
                TriggerGameAssetPath = resolvedRoute?.TriggerGameAssetPath ??
                    request.TriggerGameAssetPath ??
                    (request.Route == NativeValidationRoute.FastGeneric
                        ? NativeValidationDefaults.FastTriggerGameAssetPath
                        : request.LogicalGameAssetPath),
                StartLevel = appliedStartLevel,
                UsedIsolatedLaunchWorkspace = launchWorkspace is not null,
                LaunchWorkingDirectory = launchWorkingDirectory,
                MediaSourceDirectory = mediaRootPath,
                RedirectedAssetPath = debugResult?.RedirectedAssetPath,
                LogFilePath = log.FilePath,
                LastCheckpoint = tracker.LastCheckpoint,
                LastTargetCheckpoint = tracker.LastTargetCheckpoint,
                TargetFfpsHeaderEntered = tracker.TargetFfpsHeaderEntered,
                TargetFfpsMagicAccepted = tracker.TargetFfpsMagicAccepted,
                TargetFfpsVersionAccepted = tracker.TargetFfpsVersionAccepted,
                SceneReadyReached = tracker.SceneReadyReached,
                Exception = debugResult?.FatalException,
                CrashPhase = debugResult?.CrashPhase,
                CrashAttributionConfidence = debugResult?.CrashAttributionConfidence,
                ExitCode = debugResult?.ExitCode,
                RedirectCount = tracker.RedirectCount,
                Events = tracker.Events.ToArray()
            };
        }
    }

    private static NativeSessionLog CreateSessionLogWithFallback(string? requestedPath)
    {
        try
        {
            return NativeSessionLog.Create(requestedPath);
        }
        catch (Exception exception) when (
            requestedPath is not null && exception is IOException or UnauthorizedAccessException)
        {
            return NativeSessionLog.Create();
        }
    }

    private static string ResolveWorkingDirectory(NativeValidationRequest request)
    {
        string executable = Path.GetFullPath(request.ExecutablePath);
        return string.IsNullOrWhiteSpace(request.WorkingDirectory)
            ? Path.GetDirectoryName(executable) ?? Environment.CurrentDirectory
            : Path.GetFullPath(request.WorkingDirectory);
    }

    private static void ValidateRequest(NativeValidationRequest request)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(request.ExecutablePath);
        ArgumentException.ThrowIfNullOrWhiteSpace(request.AssetPath);
        ArgumentException.ThrowIfNullOrWhiteSpace(request.LogicalGameAssetPath);
        if (!request.UseIsolatedLaunchWorkspace)
        {
            throw new ArgumentException(
                "Native validation requires an isolated windowed launch workspace; " +
                "the game must never inherit a full-screen installation configuration.",
                nameof(request));
        }
        if (request.RequireSceneReady && request.Route != NativeValidationRoute.Contextual)
        {
            throw new ArgumentException(
                "Scene-ready validation is available only for the contextual route.",
                nameof(request));
        }
        if (request.RequireSceneReady && request.StartLevel is not > 0)
        {
            throw new ArgumentException(
                "Scene-ready validation requires a positive native startLevel so the active level state can be identified.",
                nameof(request));
        }
        if (!File.Exists(request.ExecutablePath))
            throw new FileNotFoundException("WinxClub.exe was not found.", request.ExecutablePath);
        if (!File.Exists(request.AssetPath))
            throw new FileNotFoundException("The selected SMO file was not found.", request.AssetPath);
        if (!string.IsNullOrWhiteSpace(request.WorkingDirectory) &&
            !Directory.Exists(request.WorkingDirectory))
        {
            throw new DirectoryNotFoundException(
                $"The configured working directory was not found: {Path.GetFullPath(request.WorkingDirectory)}");
        }
        if (!string.Equals(Path.GetExtension(request.AssetPath), ".smo", StringComparison.OrdinalIgnoreCase))
            throw new NativeAssetPathException("AssetPath must point to an .smo file.");
        string logicalPath = LogicalAssetMatcher.Normalize(request.LogicalGameAssetPath);
        if (!logicalPath.EndsWith(".smo", StringComparison.OrdinalIgnoreCase))
        {
            throw new NativeAssetPathException("LogicalGameAssetPath must end in .smo.");
        }
        if (!logicalPath.Contains('\\'))
        {
            throw new NativeAssetPathException(
                "LogicalGameAssetPath must include the game directory (for example Characters\\Troll\\Troll.smo).");
        }
        if (request.OverallTimeout <= TimeSpan.Zero)
            throw new ArgumentOutOfRangeException(nameof(request), "OverallTimeout must be positive.");
        if (request.NoProgressTimeout < TimeSpan.Zero)
            throw new ArgumentOutOfRangeException(nameof(request), "NoProgressTimeout cannot be negative.");
        if (request.SurvivalWindow < TimeSpan.Zero)
            throw new ArgumentOutOfRangeException(nameof(request), "SurvivalWindow cannot be negative.");
    }
}
