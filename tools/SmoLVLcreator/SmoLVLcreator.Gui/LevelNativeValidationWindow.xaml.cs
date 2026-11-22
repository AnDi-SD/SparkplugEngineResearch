using Microsoft.Win32;
using SmoLVLcreator.Core;
using SmoNativeValidator.Core;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

namespace SmoLVLcreator.Gui;

public partial class LevelNativeValidationWindow : Window
{
    private const int MaximumVisibleEvents = 400;
    private readonly SmoLevelDocument? _document;
    private readonly string _sourcePath;
    private readonly string? _prebuiltCandidatePath;
    private readonly string? _initialLogicalPath;
    private readonly WinxClubNativeValidator _validator = new();
    private readonly NativeValidatorSettingsStore _settingsStore = new();
    private NativeValidatorSettings _settings = new();
    private CancellationTokenSource? _cancellation;
    private string? _gamePath;
    private string? _lastLogPath;
    private bool _running;
    private bool _closeAfterCancellation;

    public LevelNativeValidationWindow(SmoLevelDocument document, string sourcePath)
    {
        _document = document ?? throw new ArgumentNullException(nameof(document));
        _sourcePath = Path.GetFullPath(sourcePath);
        InitializeComponent();
    }

    public LevelNativeValidationWindow(
        string prebuiltCandidatePath,
        string? logicalGameAssetPath,
        string? sourcePathHint = null)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(prebuiltCandidatePath);
        _prebuiltCandidatePath = Path.GetFullPath(prebuiltCandidatePath);
        if (!File.Exists(_prebuiltCandidatePath))
            throw new FileNotFoundException(
                LevelText.Get("Dialog.Validation.CandidateMissing"),
                _prebuiltCandidatePath);
        _sourcePath = !string.IsNullOrWhiteSpace(sourcePathHint) &&
                      File.Exists(sourcePathHint)
            ? Path.GetFullPath(sourcePathHint)
            : _prebuiltCandidatePath;
        _initialLogicalPath = logicalGameAssetPath;
        InitializeComponent();
    }

    private async void Window_Loaded(object sender, RoutedEventArgs e)
    {
        _settings = _settingsStore.Load();
        LogicalPathBox.Text = string.IsNullOrWhiteSpace(_initialLogicalPath)
            ? InferLogicalGameAssetPath(_sourcePath)
            : NormalizeLogicalPath(_initialLogicalPath);
        await LocateGameAsync(clearManualSelection: false);
    }

    private async void FindGame_Click(object sender, RoutedEventArgs e) =>
        await LocateGameAsync(clearManualSelection: true);

    private async Task LocateGameAsync(bool clearManualSelection)
    {
        if (_running)
            return;

        if (clearManualSelection)
        {
            _settings = _settings with { ManualExecutablePath = null };
            TrySaveSettings();
        }

        SetConfigurationEnabled(false);
        ResultTitleText.Text = LevelText.Get("Dialog.Validation.Searching");
        ResultDetailText.Text = LevelText.Get("Dialog.Validation.SearchDetails");
        string? besideSource = FindGameBesideSource(_sourcePath);
        WinxClubLocationResult location = await Task.Run(() =>
            WinxClubLocator.Locate(new WinxClubLocatorOptions
            {
                PreferredExecutablePath = _settings.ManualExecutablePath ?? besideSource,
                SavedExecutablePath = _settings.ManualExecutablePath is null
                    ? null
                    : besideSource
            }));
        _gamePath = location.SelectedPath;
        GamePathBox.Text = _gamePath ?? LevelText.Get("Dialog.Validation.GameMissing");
        ResultTitleText.Text = _gamePath is null
            ? LevelText.Get("Dialog.Validation.ExecutableMissing")
            : LevelText.Get("Dialog.Validation.GameLocated");
        ResultDetailText.Text = _gamePath is null
            ? LevelText.Get("Dialog.Validation.ChooseManually")
            : _gamePath;
        SetConfigurationEnabled(true);
    }

    private void ChooseGame_Click(object sender, RoutedEventArgs e)
    {
        if (_running)
            return;

        var dialog = new OpenFileDialog
        {
            Title = LevelText.Get("Dialog.Validation.ChooseGame"),
            Filter = LevelText.Get("Dialog.Validation.GameFileFilter"),
            CheckFileExists = true,
            Multiselect = false,
            InitialDirectory = _gamePath is null ? null : Path.GetDirectoryName(_gamePath)
        };
        if (dialog.ShowDialog(this) != true)
            return;

        _gamePath = Path.GetFullPath(dialog.FileName);
        GamePathBox.Text = _gamePath;
        _settings = _settings with { ManualExecutablePath = _gamePath };
        TrySaveSettings();
        UpdateStartState();
    }

    private void LogicalPathBox_TextChanged(object sender, TextChangedEventArgs e)
    {
        if (RouteHintText is not null)
        {
            string logicalPath = NormalizeLogicalPath(LogicalPathBox.Text);
            RouteHintText.Text = string.IsNullOrWhiteSpace(logicalPath)
                ? LevelText.Get("Dialog.Validation.ExactPathHint")
                : LevelText.Get("Dialog.Validation.RouteHint", logicalPath);
        }
        UpdateStartState();
    }

    private async void Start_Click(object sender, RoutedEventArgs e)
    {
        if (_running || _gamePath is null)
            return;

        string logicalPath = NormalizeLogicalPath(LogicalPathBox.Text);
        if (string.IsNullOrWhiteSpace(logicalPath))
        {
            ShowSetupFailure(
                LevelText.Get("Dialog.Validation.MediaPathMissing"),
                LevelText.Get("Dialog.Validation.MediaPathRequired"));
            return;
        }

        _settings = _settings with
        {
            ManualExecutablePath = _gamePath
        };
        TrySaveSettings();

        var runCancellation = new CancellationTokenSource();
        _cancellation = runCancellation;
        _running = true;
        SetConfigurationEnabled(false);
        StartButton.IsEnabled = false;
        StopButton.IsEnabled = true;
        OpenLogButton.IsEnabled = false;
        EventsList.Items.Clear();
        ValidationProgress.Value = 0;
        ProgressText.Text = LevelText.Get("Dialog.Validation.Percent", 0);
        ResultTitleText.Foreground = new SolidColorBrush(Color.FromRgb(216, 222, 233));
        ResultTitleText.Text = LevelText.Get("Dialog.Validation.BuildingTemporaryLevel");
        ResultDetailText.Text = LevelText.Get("Dialog.Validation.TemporaryLevelDetails");
        ResultDetailText.ToolTip = null;

        string temporaryDirectory = Path.Combine(
            Path.GetTempPath(),
            "SmoLVLcreator",
            "NativeValidation",
            Guid.NewGuid().ToString("N"));
        string candidateFileName = Path.GetFileName(
            _prebuiltCandidatePath ?? _sourcePath);
        string candidatePath = Path.Combine(temporaryDirectory, candidateFileName);
        bool candidateBuilt = false;
        try
        {
            Directory.CreateDirectory(temporaryDirectory);
            if (_prebuiltCandidatePath is not null)
            {
                await Task.Run(
                    () => File.Copy(
                        _prebuiltCandidatePath,
                        candidatePath,
                        overwrite: false),
                    runCancellation.Token);
                var candidate = new FileInfo(candidatePath);
                AddEvent(LevelText.Get("Dialog.Validation.PrebuiltEvent", candidate.Length));
            }
            else
            {
                SmoLevelSaveResult saved = await Task.Run(
                    () => SmoLevelSaveService.Save(
                        _document!,
                        candidatePath,
                        progress: null,
                        runCancellation.Token),
                    runCancellation.Token);
                AddEvent(LevelText.Get("Dialog.Validation.BuiltEvent", saved.FileSize, saved.ResourceEditCount));
                AddEvent(LevelText.Get("Dialog.Validation.BuildLog", saved.LogPath));
            }
            candidateBuilt = true;
            int? automaticStartLevel =
                WinxClubLevelCatalog.TryResolveStartLevelForLogicalSmo(
                    logicalPath,
                    out int inferredStartLevel)
                    ? inferredStartLevel
                    : null;
            bool requireSceneReady = automaticStartLevel is not null;
            if (automaticStartLevel is int startLevel)
            {
                AddEvent(LevelText.Get("Dialog.Validation.AutomaticRouteEvent", startLevel, logicalPath));
                ResultTitleText.Text = LevelText.Get("Dialog.Validation.LaunchingLevel");
                ResultDetailText.Text = LevelText.Get("Dialog.Validation.AutomaticRouteDetails", logicalPath, startLevel);
            }
            else
            {
                AddEvent(LevelText.Get("Dialog.Validation.ManualRouteEvent", logicalPath));
                ResultTitleText.Text = LevelText.Get("Dialog.Validation.LaunchingGame");
                ResultDetailText.Text = LevelText.Get("Dialog.Validation.ManualRouteDetails", logicalPath);
            }

            var request = new NativeValidationRequest
            {
                ExecutablePath = _gamePath,
                AssetPath = candidatePath,
                LogicalGameAssetPath = logicalPath,
                Route = NativeValidationRoute.Contextual,
                StartLevel = automaticStartLevel,
                RequireSceneReady = requireSceneReady,
                UseIsolatedLaunchWorkspace = true,
                OverallTimeout = TimeSpan.FromSeconds(
                    Math.Clamp(Math.Max(_settings.OverallTimeoutSeconds, 300), 300, 900)),
                NoProgressTimeout = TimeSpan.FromSeconds(
                    Math.Clamp(_settings.NoProgressTimeoutSeconds, 10, 60)),
                SurvivalWindow = TimeSpan.FromMilliseconds(
                    Math.Clamp(_settings.SurvivalWindowMilliseconds, 1000, 10000)),
                CollectFirstChanceExceptions = true,
                IncludeBloomCheckpoints = logicalPath.Contains(
                    "Bloom", StringComparison.OrdinalIgnoreCase),
                StageAsset = true
            };
            var progress = new Progress<NativeValidationEvent>(HandleValidationEvent);
            NativeValidationReport report = await _validator.ValidateAsync(
                request,
                progress,
                runCancellation.Token);
            ShowReport(report);
        }
        catch (OperationCanceledException)
        {
            ResultTitleText.Text = LevelText.Get("Dialog.Validation.Cancelled");
            ResultDetailText.Text = LevelText.Get("Dialog.Validation.CancelledDetails");
            AddEvent(LevelText.Get("Dialog.Validation.CancelledEvent"));
        }
        catch (Exception exception)
        {
            ResultTitleText.Foreground = new SolidColorBrush(Color.FromRgb(229, 107, 107));
            ResultTitleText.Text = candidateBuilt
                ? LevelText.Get("Dialog.Validation.StartError")
                : LevelText.Get("Dialog.Validation.BuildError");
            ResultDetailText.Text = LevelText.Error(exception.Message);
            if (exception is SmoLevelSaveException saveException)
            {
                _lastLogPath = saveException.LogPath;
                OpenLogButton.IsEnabled = File.Exists(_lastLogPath);
                AddEvent(LevelText.Get("Dialog.Validation.SaveLog", _lastLogPath));
            }
            AddEvent(LevelText.Get("Dialog.Validation.ExceptionEvent",
                exception.GetType().Name, LevelText.Error(exception.Message)));
        }
        finally
        {
            runCancellation.Dispose();
            if (ReferenceEquals(_cancellation, runCancellation))
                _cancellation = null;
            _running = false;
            StopButton.IsEnabled = false;
            SetConfigurationEnabled(true);
            if (candidateBuilt)
                TryDeleteValidationDirectory(temporaryDirectory);
            if (_closeAfterCancellation)
                _ = Dispatcher.BeginInvoke(Close);
        }
    }

    private void HandleValidationEvent(NativeValidationEvent validationEvent)
    {
        if (validationEvent.ProgressPercent is double progress)
        {
            ValidationProgress.Value = Math.Clamp(progress, 0, 100);
            ProgressText.Text = LevelText.Get("Dialog.Validation.Percent", ValidationProgress.Value);
        }

        string checkpoint = string.IsNullOrWhiteSpace(validationEvent.Checkpoint)
            ? string.Empty
            : $" · {validationEvent.Checkpoint}";
        string description = DescribeEvent(validationEvent);
        AddEvent(LevelText.Get("Dialog.Validation.ProgressEvent",
            validationEvent.Elapsed.TotalSeconds,
            LevelText.Get($"Dialog.Validation.Stage.{validationEvent.Stage}"),
            checkpoint, description), validationEvent.Message == description ? null : validationEvent.Message);
    }

    private static string DescribeEvent(NativeValidationEvent validationEvent)
    {
        IReadOnlyDictionary<string, string?> data = validationEvent.Data;
        return validationEvent.Kind switch
        {
            NativeValidationEventKind.SessionStarted => LevelText.Get("Dialog.Validation.Event.Started"),
            NativeValidationEventKind.Diagnostic when validationEvent.Stage == NativeValidationStage.Preparing &&
                data.GetValueOrDefault("triggerGameAssetPath") is string trigger =>
                LevelText.Get("Dialog.Validation.Event.ExpectedPath", trigger),
            NativeValidationEventKind.Diagnostic when validationEvent.Stage == NativeValidationStage.Fingerprinting &&
                validationEvent.Severity == NativeValidationSeverity.Information =>
                LevelText.Get("Dialog.Validation.Event.IdentifyExecutable"),
            NativeValidationEventKind.ExecutableFingerprint when validationEvent.Severity == NativeValidationSeverity.Success =>
                LevelText.Get("Dialog.Validation.Event.ExecutableRecognized"),
            NativeValidationEventKind.AssetStaged => LevelText.Get("Dialog.Validation.Event.AssetReady"),
            NativeValidationEventKind.Diagnostic when validationEvent.Stage == NativeValidationStage.LaunchingEngine &&
                data.ContainsKey("copiedShaderFiles") => LevelText.Get("Dialog.Validation.Event.WorkspaceReady"),
            NativeValidationEventKind.ProcessStarted when data.GetValueOrDefault("processId") is string pid =>
                LevelText.Get("Dialog.Validation.Event.ProcessStarted", pid),
            NativeValidationEventKind.ProcessExited when data.GetValueOrDefault("exitCode") is string exitCode =>
                LevelText.Get("Dialog.Validation.Event.ProcessExited", exitCode),
            NativeValidationEventKind.PathObserved when data.GetValueOrDefault("observedPath") is string observed =>
                LevelText.Get("Dialog.Validation.Event.PathObserved", observed),
            NativeValidationEventKind.PathRedirected when data.GetValueOrDefault("redirectedPath") is string redirected =>
                LevelText.Get("Dialog.Validation.Event.Redirected", redirected),
            NativeValidationEventKind.SceneReady when data.GetValueOrDefault("activeStateId") is string state =>
                LevelText.Get("Dialog.Validation.Event.SceneReady", state),
            NativeValidationEventKind.Timeout => LevelText.Get("Dialog.Validation.TimeoutTitle"),
            NativeValidationEventKind.Cancelled => LevelText.Get("Dialog.Validation.Cancelled"),
            NativeValidationEventKind.Cleanup when data.GetValueOrDefault("existsAfterCleanup") == "false" =>
                LevelText.Get("Dialog.Validation.Event.CleanupDone"),
            NativeValidationEventKind.Cleanup when data.GetValueOrDefault("existsAfterCleanup") == "true" =>
                LevelText.Get("Dialog.Validation.Event.CleanupPending", data.GetValueOrDefault("sessionDirectory")),
            NativeValidationEventKind.SessionCompleted when Enum.TryParse(
                data.GetValueOrDefault("status"), out NativeValidationStatus status) && Enum.IsDefined(status) =>
                LevelText.Get($"Dialog.Validation.Status.{status}"),
            _ => LevelText.Error(validationEvent.Message)
        };
    }

    private void ShowReport(NativeValidationReport report)
    {
        _lastLogPath = report.LogFilePath;
        OpenLogButton.IsEnabled = _lastLogPath is not null && File.Exists(_lastLogPath);
        if (report.Status == NativeValidationStatus.Passed)
            ValidationProgress.Value = 100;
        ProgressText.Text = LevelText.Get("Dialog.Validation.Percent", ValidationProgress.Value);

        string passedDetail = report.SceneReadyReached
            ? LevelText.Get("Dialog.Validation.SceneReadyDetails")
            : LevelText.Get("Dialog.Validation.AcceptedDetails");
        string lastCheckpoint = report.LastTargetCheckpoint ?? report.LastCheckpoint ??
            LevelText.Get("Dialog.Validation.CheckpointNotReached");
        string status = LevelText.Get($"Dialog.Validation.Status.{report.Status}");

        (string title, string detail, Color color) = report.Status switch
        {
            NativeValidationStatus.Passed => (
                LevelText.Get("Dialog.Validation.PassedTitle"),
                passedDetail,
                Color.FromRgb(113, 190, 132)),
            NativeValidationStatus.Crash => (
                LevelText.Get("Dialog.Validation.CrashTitle"),
                LevelText.Get("Dialog.Validation.CrashDetails",
                    report.CrashPhase is { } phase ? LevelText.Get($"Dialog.Validation.CrashPhase.{phase}") : "—",
                    report.CrashAttributionConfidence is { } confidence
                        ? LevelText.Get($"Dialog.Validation.CrashConfidence.{confidence}") : "—",
                    lastCheckpoint),
                Color.FromRgb(229, 107, 107)),
            NativeValidationStatus.EngineRejected => (
                LevelText.Get("Dialog.Validation.RejectedTitle"),
                LevelText.Get("Dialog.Validation.LastCheckpoint", lastCheckpoint),
                Color.FromRgb(229, 107, 107)),
            NativeValidationStatus.TargetNotRequested => (
                LevelText.Get("Dialog.Validation.NotRequestedTitle"),
                LevelText.Get("Dialog.Validation.NotRequestedDetails"),
                Color.FromRgb(230, 154, 70)),
            NativeValidationStatus.Timeout => (
                LevelText.Get("Dialog.Validation.TimeoutTitle"),
                LevelText.Get("Dialog.Validation.TimeoutDetails"),
                Color.FromRgb(230, 154, 70)),
            NativeValidationStatus.Cancelled => (
                LevelText.Get("Dialog.Validation.Cancelled"),
                LevelText.Get("Dialog.Validation.CancelledDetails"),
                Color.FromRgb(174, 184, 198)),
            _ => (
                LevelText.Get("Dialog.Validation.OtherStatusTitle", status),
                LevelText.Get($"Dialog.Validation.ResultDetails.{report.Status}"),
                Color.FromRgb(230, 154, 70))
        };

        ResultTitleText.Text = title;
        ResultTitleText.Foreground = new SolidColorBrush(color);
        ResultDetailText.Text = LevelText.Get("Dialog.Validation.ReportDetails",
            detail, report.Duration.TotalSeconds, report.ExitCode?.ToString() ?? "—");
        ResultDetailText.ToolTip = report.Summary;
        AddEvent(LevelText.Get("Dialog.Validation.ReportEvent", status, detail,
            report.LogFilePath ?? LevelText.Get("Dialog.Validation.NoLog")), report.Summary);
    }

    private void AddEvent(string text, string? diagnostic = null)
    {
        EventsList.Items.Add(diagnostic is null ? text : new ListBoxItem
        {
            Content = text,
            ToolTip = new TextBlock { Text = diagnostic, TextWrapping = TextWrapping.Wrap, MaxWidth = 560 }
        });
        while (EventsList.Items.Count > MaximumVisibleEvents)
            EventsList.Items.RemoveAt(0);
        if (EventsList.Items.Count > 0)
            EventsList.ScrollIntoView(EventsList.Items[EventsList.Items.Count - 1]);
    }

    private void ShowSetupFailure(string title, string detail)
    {
        ResultTitleText.Foreground = new SolidColorBrush(Color.FromRgb(229, 107, 107));
        ResultTitleText.Text = title;
        ResultDetailText.Text = detail;
    }

    private void Stop_Click(object sender, RoutedEventArgs e) => _cancellation?.Cancel();

    private void OpenLog_Click(object sender, RoutedEventArgs e)
    {
        if (_lastLogPath is null || !File.Exists(_lastLogPath))
            return;
        Process.Start(new ProcessStartInfo(_lastLogPath) { UseShellExecute = true });
    }

    private void Close_Click(object sender, RoutedEventArgs e) => Close();

    private void Window_Closing(object? sender, CancelEventArgs e)
    {
        if (!_running)
            return;
        e.Cancel = true;
        _closeAfterCancellation = true;
        ResultTitleText.Text = LevelText.Get("Dialog.Validation.Stopping");
        _cancellation?.Cancel();
    }

    private void SetConfigurationEnabled(bool enabled)
    {
        FindGameButton.IsEnabled = enabled;
        ChooseGameButton.IsEnabled = enabled;
        LogicalPathBox.IsEnabled = enabled;
        UpdateStartState();
    }

    private void UpdateStartState()
    {
        if (StartButton is null)
            return;
        StartButton.IsEnabled = !_running &&
            !string.IsNullOrWhiteSpace(_gamePath) &&
            !string.IsNullOrWhiteSpace(LogicalPathBox?.Text);
    }

    private void TrySaveSettings()
    {
        try
        {
            _settingsStore.Save(_settings);
        }
        catch
        {
            // Read-only user profiles must not block validation.
        }
    }

    private static string? FindGameBesideSource(string sourcePath)
    {
        DirectoryInfo? directory = new(Path.GetDirectoryName(sourcePath)!);
        while (directory is not null)
        {
            if (directory.Name.Equals("Media", StringComparison.OrdinalIgnoreCase) &&
                directory.Parent is not null)
            {
                string candidate = Path.Combine(directory.Parent.FullName, "WinxClub.exe");
                return File.Exists(candidate) ? candidate : null;
            }
            directory = directory.Parent;
        }
        return null;
    }

    private static string InferLogicalGameAssetPath(string sourcePath)
    {
        string fullPath = Path.GetFullPath(sourcePath);
        string marker = $"{Path.DirectorySeparatorChar}Media{Path.DirectorySeparatorChar}";
        int media = fullPath.LastIndexOf(marker, StringComparison.OrdinalIgnoreCase);
        return media >= 0
            ? NormalizeLogicalPath(fullPath[(media + marker.Length)..])
            : Path.GetFileName(fullPath);
    }

    private static string NormalizeLogicalPath(string path) =>
        path.Trim().Trim('"').Replace('/', '\\').TrimStart('\\');

    private static void TryDeleteValidationDirectory(string path)
    {
        try
        {
            string full = Path.GetFullPath(path);
            string root = Path.GetFullPath(Path.Combine(
                Path.GetTempPath(), "SmoLVLcreator", "NativeValidation"));
            if (full.StartsWith(
                    root + Path.DirectorySeparatorChar,
                    StringComparison.OrdinalIgnoreCase) &&
                Directory.Exists(full))
            {
                Directory.Delete(full, recursive: true);
            }
        }
        catch
        {
            // Temporary cleanup is best effort.
        }
    }
}
