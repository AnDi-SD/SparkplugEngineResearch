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
                "The prebuilt native-validation candidate was not found.",
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
        ResultTitleText.Text = "Поиск WinxClub.exe…";
        ResultDetailText.Text = "Проверяются путь уровня, общие каталоги и 32-битный реестр игры.";
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
        GamePathBox.Text = _gamePath ?? "Игра не найдена";
        ResultTitleText.Text = _gamePath is null ? "WinxClub.exe не найден" : "Игра найдена";
        ResultDetailText.Text = _gamePath is null
            ? "Укажите WinxClub.exe вручную."
            : _gamePath;
        SetConfigurationEnabled(true);
    }

    private void ChooseGame_Click(object sender, RoutedEventArgs e)
    {
        if (_running)
            return;

        var dialog = new OpenFileDialog
        {
            Title = "Выбрать WinxClub.exe",
            Filter = "Winx Club (WinxClub.exe)|WinxClub.exe|Executable (*.exe)|*.exe",
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
                ? "Укажите точный путь исходного уровня относительно Media."
                : $"Тестер будет ждать точный игровой запрос: {logicalPath}";
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
                "Не указан путь внутри Media",
                "Для проверки нужен исходный путь уровня относительно каталога Media.");
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
        ProgressText.Text = "0%";
        ResultTitleText.Foreground = new SolidColorBrush(Color.FromRgb(216, 222, 233));
        ResultTitleText.Text = "Сборка временного уровня…";
        ResultDetailText.Text = "Исходный SMO остаётся без изменений.";

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
                AddEvent(
                    $"BUILD · {candidate.Length:N0} bytes · " +
                    "готовый SMO проектного serializer'а");
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
                AddEvent(
                    $"BUILD · {saved.FileSize:N0} bytes · " +
                    $"{saved.ResourceEditCount} resource edits");
                AddEvent($"BUILD LOG · {saved.LogPath}");
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
                AddEvent(
                    $"ROUTE · startLevel={startLevel} · игра автоматически откроет {logicalPath}");
                ResultTitleText.Text = "Игра запускает проверяемый уровень";
                ResultDetailText.Text =
                    $"Для {logicalPath} найден нативный startLevel={startLevel}. " +
                    "Тест завершится только после выхода из загрузочного состояния и возврата управления игре.";
            }
            else
            {
                AddEvent($"WAIT · откройте в игре уровень, который запрашивает {logicalPath}");
                ResultTitleText.Text = "Игра запускается — затем откройте уровень";
                ResultDetailText.Text =
                    $"Автоматический маршрут для {logicalPath} неизвестен. " +
                    "В меню игры загрузите сохранение или перейдите в нужную комнату.";
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
            ResultTitleText.Text = "Проверка отменена";
            ResultDetailText.Text = "Дочерний процесс игры остановлен, временный уровень удаляется.";
            AddEvent("CANCELLED");
        }
        catch (Exception exception)
        {
            ResultTitleText.Foreground = new SolidColorBrush(Color.FromRgb(229, 107, 107));
            ResultTitleText.Text = candidateBuilt
                ? "Ошибка запуска проверки"
                : "Временный SMO не собран";
            ResultDetailText.Text = exception.Message;
            if (exception is SmoLevelSaveException saveException)
            {
                _lastLogPath = saveException.LogPath;
                OpenLogButton.IsEnabled = File.Exists(_lastLogPath);
                AddEvent($"SAVE LOG · {_lastLogPath}");
            }
            AddEvent($"ERROR · {exception.GetType().Name} · {exception.Message}");
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
            ProgressText.Text = $"{ValidationProgress.Value:N0}%";
        }

        string checkpoint = string.IsNullOrWhiteSpace(validationEvent.Checkpoint)
            ? string.Empty
            : $" · {validationEvent.Checkpoint}";
        AddEvent(
            $"{validationEvent.Elapsed.TotalSeconds,6:N1}s · " +
            $"{validationEvent.Stage}{checkpoint} · {validationEvent.Message}");
    }

    private void ShowReport(NativeValidationReport report)
    {
        _lastLogPath = report.LogFilePath;
        OpenLogButton.IsEnabled = _lastLogPath is not null && File.Exists(_lastLogPath);
        if (report.Status == NativeValidationStatus.Passed)
            ValidationProgress.Value = 100;
        ProgressText.Text = $"{ValidationProgress.Value:N0}%";

        string passedDetail = report.SceneReadyReached
            ? "Игра приняла временный SMO, завершила загрузку сцены, вернула управление игровому состоянию и пережила контрольное окно."
            : "Игра приняла временный SMO и пережила контрольное окно после загрузки.";

        (string title, string detail, Color color) = report.Status switch
        {
            NativeValidationStatus.Passed => (
                "✓ ТЕСТ ПРОЙДЕН",
                passedDetail,
                Color.FromRgb(113, 190, 132)),
            NativeValidationStatus.Crash => (
                "✕ ИГРА УПАЛА",
                $"Фаза: {report.CrashPhase}; связь с уровнем: " +
                $"{report.CrashAttributionConfidence}; последняя точка: " +
                $"{report.LastTargetCheckpoint ?? report.LastCheckpoint ?? "не достигнута"}.",
                Color.FromRgb(229, 107, 107)),
            NativeValidationStatus.EngineRejected => (
                "✕ УРОВЕНЬ ОТКЛОНЁН ЗАГРУЗЧИКОМ",
                $"Последняя точка: {report.LastTargetCheckpoint ?? report.LastCheckpoint ?? "не достигнута"}.",
                Color.FromRgb(229, 107, 107)),
            NativeValidationStatus.TargetNotRequested => (
                "⚠ УРОВЕНЬ НЕ БЫЛ ЗАПРОШЕН",
                "Игра была закрыта до входа в нужную комнату либо путь уровня внутри Media указан неверно.",
                Color.FromRgb(230, 154, 70)),
            NativeValidationStatus.Timeout => (
                "⚠ ТАЙМАУТ",
                report.Summary,
                Color.FromRgb(230, 154, 70)),
            NativeValidationStatus.Cancelled => (
                "ПРОВЕРКА ОТМЕНЕНА",
                report.Summary,
                Color.FromRgb(174, 184, 198)),
            _ => (
                $"⚠ {report.Status}",
                report.Summary,
                Color.FromRgb(230, 154, 70))
        };

        ResultTitleText.Text = title;
        ResultTitleText.Foreground = new SolidColorBrush(color);
        ResultDetailText.Text =
            $"{detail} Время: {report.Duration.TotalSeconds:N1} с. " +
            $"Exit code: {report.ExitCode?.ToString() ?? "—"}.";
        AddEvent(
            $"RESULT · {report.Status} · {report.Summary} · " +
            $"log={report.LogFilePath ?? "NONE"}");
    }

    private void AddEvent(string text)
    {
        EventsList.Items.Add(text);
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
        ResultTitleText.Text = "Останавливаю проверку…";
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
