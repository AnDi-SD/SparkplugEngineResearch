using Microsoft.Win32;
using SmoNativeValidator.Core;
using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Automation;
using System.Windows.Controls;
using System.Windows.Media;
using MediaColor = System.Windows.Media.Color;

namespace SmoViewer;

public partial class NativeValidationPanel : System.Windows.Controls.UserControl, IDisposable
{
    private const int MaximumVisibleEvents = 400;

    private readonly WinxClubNativeValidator _validator = new();
    private readonly NativeValidatorSettingsStore _settingsStore = new();
    private NativeValidatorSettings _settings = new();
    private CancellationTokenSource? _validationCancellation;
    private ExecutableIdentification? _executableIdentification;
    private string? _modelPath;
    private string? _gamePath;
    private string? _lastLogPath;
    private long _nextLocationRunId;
    private long _activeLocationRunId;
    private long _nextValidationRunId;
    private long _activeValidationRunId;
    private readonly System.Collections.Generic.HashSet<string>
        _shownHighFrequencyTargetCheckpoints =
            new(StringComparer.OrdinalIgnoreCase);
    private bool _initialized;
    private bool _applyingSettings;
    private bool _running;
    private bool _disposed;

    public NativeValidationPanel()
    {
        InitializeComponent();
    }

    public event EventHandler<NativeValidationPanelLogEventArgs>? LogMessage;

    public void SetModelPath(string? path)
    {
        string? fullPath = string.IsNullOrWhiteSpace(path)
            ? null
            : Path.GetFullPath(path);
        if (string.Equals(_modelPath, fullPath, StringComparison.OrdinalIgnoreCase))
            return;

        CancelActiveValidation(invalidateCallbacks: true);
        _modelPath = fullPath;
        ModelPathText.Text = fullPath is null
            ? "Сначала откройте SMO."
            : $"{Path.GetFileName(fullPath)}\n{Path.GetDirectoryName(fullPath)}";
        string logicalPath = fullPath is null
            ? string.Empty
            : InferLogicalGameAssetPath(fullPath);
        if (fullPath is not null && string.IsNullOrWhiteSpace(logicalPath) &&
            SettingsLogicalPathBelongsToModel(fullPath))
        {
            logicalPath = NormalizeLogicalPath(_settings.LogicalGameAssetPath!);
        }
        TargetAssetBox.Text = logicalPath;
        UpdateOriginalLogicalPathDisplay();
        ClearSessionDisplay();
        if (IsContextualMode && fullPath is not null &&
            string.IsNullOrWhiteSpace(logicalPath))
        {
            ValidationStatusText.Text =
                "Модель находится вне Media. Проверьте полный игровой путь ресурса перед контекстным запуском.";
        }
        UpdateStartState();

        if (_initialized && string.IsNullOrWhiteSpace(_settings.ManualExecutablePath))
            _ = LocateGameAsync(preferModelInstallation: true);
    }

    private async void NativeValidationPanel_Loaded(
        object sender,
        RoutedEventArgs e)
    {
        if (_initialized || _disposed)
            return;

        _initialized = true;
        try
        {
            _settings = _settingsStore.Load();
            bool migrateLegacyContextualSettings =
                IsLegacyContextualSettings(_settings);
            if (migrateLegacyContextualSettings)
            {
                _settings = _settings with
                {
                    Route = NativeValidationRoute.Contextual
                };
            }

            ApplySettingsToControls();
            if (migrateLegacyContextualSettings)
                TrySaveSettings();
        }
        catch (Exception exception)
        {
            EmitLog($"Native validator: настройки не прочитаны: {exception.Message}");
            ApplySettingsToControls();
        }

        await LocateGameAsync(preferModelInstallation: true);
    }

    private void NativeValidationPanel_Unloaded(
        object sender,
        RoutedEventArgs e)
    {
        if (Window.GetWindow(this)?.IsLoaded != true)
        {
            PersistUiSettings();
            CancelActiveValidation(invalidateCallbacks: true);
        }
    }

    private async void FindGameButton_Click(object sender, RoutedEventArgs e)
    {
        _settings = _settings with { ManualExecutablePath = null };
        TrySaveSettings();
        await LocateGameAsync(preferModelInstallation: true);
    }

    private async void ChooseGameButton_Click(object sender, RoutedEventArgs e)
    {
        Microsoft.Win32.OpenFileDialog dialog = new()
        {
            Title = "Выбрать оригинальный WinxClub.exe",
            Filter = "Winx Club (WinxClub.exe)|WinxClub.exe|Executable files (*.exe)|*.exe|All files (*.*)|*.*",
            CheckFileExists = true,
            Multiselect = false,
            InitialDirectory = _gamePath is null
                ? null
                : Path.GetDirectoryName(_gamePath)
        };
        if (dialog.ShowDialog(Window.GetWindow(this)) != true)
            return;

        long locationRunId = BeginLocationRun();
        await SetGameExecutableAsync(
            dialog.FileName,
            "выбран вручную",
            locationRunId);
        if (!IsCurrentLocationRun(locationRunId))
            return;

        if (_gamePath is not null)
        {
            _settings = _settings with { ManualExecutablePath = _gamePath };
            TrySaveSettings();
        }
    }

    private bool IsContextualMode =>
        ContextModeRadio?.IsChecked == true;

    private void ValidationMode_Changed(object sender, RoutedEventArgs e)
    {
        if (ContextModeSettingsPanel is null)
            return;

        ContextModeSettingsPanel.Visibility = IsContextualMode
            ? Visibility.Visible
            : Visibility.Collapsed;
        UpdateStartLevelHint();
        UpdateStartState();
        PersistUiSettings();
    }

    private void TargetAssetBox_TextChanged(object sender, TextChangedEventArgs e)
    {
        UpdateContextualPathHint();
        UpdateOriginalLogicalPathDisplay();
        UpdateStartState();
    }

    private void StartLevelBox_TextChanged(object sender, TextChangedEventArgs e)
    {
        UpdateStartLevelHint();
        UpdateStartState();
    }

    private void SettingsControl_LostFocus(object sender, RoutedEventArgs e) =>
        PersistUiSettings();

    private void TimeoutSelector_SelectionChanged(
        object sender,
        SelectionChangedEventArgs e) => PersistUiSettings();

    private async void StartValidationButton_Click(object sender, RoutedEventArgs e)
    {
        if (_running || _gamePath is null || _modelPath is null)
        {
            return;
        }

        NativeValidationRoute route = IsContextualMode
            ? NativeValidationRoute.Contextual
            : NativeValidationRoute.FastGeneric;
        string logicalGameAssetPath = GetOriginalLogicalGameAssetPath();
        if (route == NativeValidationRoute.Contextual &&
            (!IsContextualLogicalPathValid() ||
             !TryGetConfiguredStartLevel(out _)))
        {
            UpdateStartState();
            return;
        }

        TryGetConfiguredStartLevel(out int? contextualStartLevel);
        PersistUiSettings();

        ClearSessionDisplay();
        _running = true;
        SetConfigurationEnabled(false);
        StartValidationButton.IsEnabled = false;
        StopValidationButton.IsEnabled = true;
        ValidationProgress.Value = 0;
        ValidationStatusText.Text = "Подготовка нативной проверки…";
        CancellationTokenSource runCancellation = new();
        _validationCancellation = runCancellation;
        long runId = ++_nextValidationRunId;
        _activeValidationRunId = runId;

        TimeSpan overallTimeout = TimeSpan.FromSeconds(GetSelectedTimeoutSeconds());
        NativeValidationRequest request = new()
        {
            ExecutablePath = _gamePath,
            AssetPath = _modelPath,
            LogicalGameAssetPath = logicalGameAssetPath,
            Route = route,
            StartLevel = route == NativeValidationRoute.Contextual
                ? contextualStartLevel
                : null,
            UseIsolatedLaunchWorkspace = true,
            OverallTimeout = overallTimeout,
            NoProgressTimeout = TimeSpan.FromSeconds(
                Math.Clamp(overallTimeout.TotalSeconds / 4, 10, 30)),
            SurvivalWindow = TimeSpan.FromSeconds(3),
            IncludeBloomCheckpoints = logicalGameAssetPath.Contains(
                "Bloom", StringComparison.OrdinalIgnoreCase),
            CollectFirstChanceExceptions =
                FirstChanceExceptionsCheck.IsChecked == true,
            AllowFileNameOnlyLogicalPath = route == NativeValidationRoute.FastGeneric
        };

        string triggerPath = route == NativeValidationRoute.FastGeneric
            ? NativeValidationDefaults.FastTriggerGameAssetPath
            : logicalGameAssetPath;
        EmitLog(
            $"Native validator: {GetRouteDisplayName(route)}, " +
            $"trigger {triggerPath}, original {logicalGameAssetPath}, " +
            $"модель {Path.GetFileName(_modelPath)}; оконный изолированный запуск.");

        Progress<NativeValidationEvent> progress = new(validationEvent =>
        {
            if (IsCurrentValidationRun(runId))
                HandleValidationEvent(validationEvent);
        });
        try
        {
            NativeValidationReport report = await _validator.ValidateAsync(
                request,
                progress,
                runCancellation.Token);
            if (IsCurrentValidationRun(runId))
                ShowReport(report);
        }
        catch (OperationCanceledException)
        {
            if (IsCurrentValidationRun(runId))
            {
                ValidationStatusText.Text = "Проверка отменена.";
                ShowResultBanner(
                    new ValidationResultPresentation(
                        "■ ПРОВЕРКА ОТМЕНЕНА",
                        "Где: остановлен пользователем до получения результата.",
                        ValidationResultTone.Neutral),
                    $"{GetRouteDisplayName(route)} · результат модели не вынесен");
                EmitLog("Native validator: проверка отменена.");
            }
        }
        catch (Exception exception)
        {
            if (IsCurrentValidationRun(runId))
            {
                ValidationStatusText.Text =
                    "Проверка не запущена из-за внутренней ошибки инструмента.";
                ShowResultBanner(
                    new ValidationResultPresentation(
                        "⚠ ПРОВЕРКА НЕ ЗАПУЩЕНА",
                        "Где: внутренний сбой инструмента до получения результата модели.",
                        ValidationResultTone.SetupFailure),
                    $"{GetRouteDisplayName(route)} · подробности записаны в журнал Viewer");
                EmitLog(
                    $"ОШИБКА native validator: {exception.GetType().Name}: " +
                    exception.Message);
            }
        }
        finally
        {
            runCancellation.Dispose();
            if (ReferenceEquals(_validationCancellation, runCancellation))
            {
                _validationCancellation = null;
                _activeValidationRunId = 0;
                _running = false;
                if (!_disposed)
                {
                    StopValidationButton.IsEnabled = false;
                    SetConfigurationEnabled(true);
                    UpdateStartState();
                }
            }
        }
    }

    private void StopValidationButton_Click(object sender, RoutedEventArgs e)
    {
        ValidationStatusText.Text = "Остановка дочернего процесса игры…";
        StopValidationButton.IsEnabled = false;
        CancelActiveValidation();
    }

    private void OpenValidationLogButton_Click(object sender, RoutedEventArgs e)
    {
        if (_lastLogPath is null || !File.Exists(_lastLogPath))
            return;

        try
        {
            Process.Start(new ProcessStartInfo(_lastLogPath)
            {
                UseShellExecute = true
            });
        }
        catch (Exception exception)
        {
            EmitLog($"Native validator: не удалось открыть лог: {exception.Message}");
        }
    }

    private async Task LocateGameAsync(bool preferModelInstallation)
    {
        if (_running || _disposed)
            return;

        long locationRunId = BeginLocationRun();
        ValidationStatusText.Text = "Поиск WinxClub.exe…";
        FindGameButton.IsEnabled = false;
        ChooseGameButton.IsEnabled = false;
        try
        {
            string? besideModel = preferModelInstallation
                ? FindGameBesideModel(_modelPath)
                : null;
            string? preferred = !string.IsNullOrWhiteSpace(
                _settings.ManualExecutablePath)
                ? _settings.ManualExecutablePath
                : besideModel;
            WinxClubLocationResult location = await Task.Run(() =>
                WinxClubLocator.Locate(new WinxClubLocatorOptions
                {
                    PreferredExecutablePath = preferred,
                    SavedExecutablePath = _settings.ManualExecutablePath
                }));
            if (!IsCurrentLocationRun(locationRunId))
                return;

            if (location.SelectedPath is null)
            {
                _gamePath = null;
                _executableIdentification = null;
                GamePathBox.Text = string.Empty;
                GameProfileText.Text =
                    "WinxClub.exe автоматически не найден. Выберите его вручную.";
                ValidationStatusText.Text = "Игра не найдена.";
                foreach (string diagnostic in location.Diagnostics.Take(4))
                    EmitLog($"Native validator: {diagnostic}");
                UpdateStartState();
                return;
            }

            string source = string.Equals(
                location.SelectedPath,
                _settings.ManualExecutablePath,
                StringComparison.OrdinalIgnoreCase)
                ? "сохранённый ручной выбор"
                : string.Equals(
                    location.SelectedPath,
                    besideModel,
                    StringComparison.OrdinalIgnoreCase)
                    ? "рядом с Media открытой модели"
                    : "найден автоматически";
            await SetGameExecutableAsync(
                location.SelectedPath,
                source,
                locationRunId);
        }
        finally
        {
            if (IsCurrentLocationRun(locationRunId))
            {
                FindGameButton.IsEnabled = !_running;
                ChooseGameButton.IsEnabled = !_running;
            }
        }
    }

    private async Task SetGameExecutableAsync(
        string path,
        string source,
        long locationRunId)
    {
        if (!IsCurrentLocationRun(locationRunId))
            return;

        _gamePath = Path.GetFullPath(path);
        _executableIdentification = null;
        GamePathBox.Text = _gamePath;
        GameProfileText.Text = "Анализ внутреннего кода загрузчика…";
        ValidationStatusText.Text = "Проверка executable…";
        UpdateStartState();

        try
        {
            ExecutableIdentification identification =
                await ExecutableProfileCatalog.IdentifyAsync(
                    _gamePath, CancellationToken.None);
            if (!IsCurrentLocationRun(locationRunId))
                return;

            _executableIdentification = identification;
            GameProfileText.Text = identification.IsSupported
                ? $"{identification.Profile!.DisplayName} · {source}"
                : $"Не удалось безопасно найти точки загрузчика · {source}";
            ValidationStatusText.Text = identification.IsSupported
                ? "Игра готова; внутренние вызовы найдены независимо от патчей и хеша."
                : identification.Error ?? "Обязательные внутренние вызовы загрузчика не найдены.";
            EmitLog(
                identification.IsSupported
                    ? $"Native validator: {identification.Profile!.DisplayName}."
                    : $"Native validator: instrumentation unavailable: {identification.Error}");
        }
        catch (Exception exception)
        {
            if (!IsCurrentLocationRun(locationRunId))
                return;

            GameProfileText.Text = $"Executable не принят: {exception.Message}";
            ValidationStatusText.Text = "Выберите другой WinxClub.exe.";
            EmitLog($"Native validator: executable не принят: {exception.Message}");
        }

        UpdateStartState();
    }

    private long BeginLocationRun()
    {
        long runId = ++_nextLocationRunId;
        _activeLocationRunId = runId;
        return runId;
    }

    private bool IsCurrentLocationRun(long runId) =>
        !_disposed && runId != 0 && _activeLocationRunId == runId;

    private void HandleValidationEvent(NativeValidationEvent validationEvent)
    {
        bool? targetContext = GetTargetContext(validationEvent);
        string context = targetContext switch
        {
            true => " [цель]",
            false => " [фон]",
            null => string.Empty
        };
        string address = validationEvent.Address is ulong value
            ? $" · 0x{value:X8}"
            : string.Empty;
        string line =
            $"{validationEvent.TimestampUtc.ToLocalTime():HH:mm:ss.fff} " +
            $"{validationEvent.Stage}{context}{address} · {validationEvent.Message}";
        // Background resource loads are very noisy (thousands of CP02/CP08/CP09
        // hits before the requested model). They remain in the complete JSONL log;
        // the live panel is reserved for session events, target progress and errors.
        bool repeatedHighFrequencyTargetCheckpoint =
            targetContext == true &&
            validationEvent.Kind == NativeValidationEventKind.CheckpointEnter &&
            (validationEvent.Checkpoint is "CP08" or "CP09") &&
            !_shownHighFrequencyTargetCheckpoints.Add(validationEvent.Checkpoint);
        if (!repeatedHighFrequencyTargetCheckpoint &&
            (targetContext != false ||
             (int)validationEvent.Severity >= (int)NativeValidationSeverity.Warning))
        {
            ValidationEventsList.Items.Add(line);
            if (ValidationEventsList.Items.Count > MaximumVisibleEvents)
                ValidationEventsList.Items.RemoveAt(0);
            ValidationEventsList.ScrollIntoView(
                ValidationEventsList.Items[ValidationEventsList.Items.Count - 1]);
        }

        if (targetContext != false)
        {
            ValidationStatusText.Text = validationEvent.Message;
            ValidationProgress.IsIndeterminate = validationEvent.ProgressPercent is null;
            if (validationEvent.ProgressPercent is double progress)
                ValidationProgress.Value = Math.Clamp(progress, 0, 100);
        }

        if ((int)validationEvent.Severity >=
            (int)NativeValidationSeverity.Warning)
            EmitLog(
                $"Native validator [{validationEvent.Stage}]: " +
                validationEvent.Message);
    }

    private static bool? GetTargetContext(NativeValidationEvent validationEvent)
    {
        foreach (string key in new[]
        {
            "target",
            "targetContext",
            "matchesLogicalTarget",
            "targetCandidate"
        })
        {
            if (!validationEvent.Data.TryGetValue(key, out string? value) ||
                !bool.TryParse(value, out bool parsed))
            {
                continue;
            }

            return parsed;
        }

        return validationEvent.Kind == NativeValidationEventKind.PathRedirected
            ? true
            : null;
    }

    private void ShowReport(NativeValidationReport report)
    {
        _lastLogPath = report.LogFilePath;
        OpenValidationLogButton.IsEnabled =
            _lastLogPath is not null && File.Exists(_lastLogPath);
        ValidationProgress.IsIndeterminate = false;
        ValidationProgress.Value = report.Status == NativeValidationStatus.Passed
            ? 100
            : ValidationProgress.Value;
        ValidationResultPresentation presentation =
            CreateResultPresentation(report);
        ValidationStatusText.Text = "Проверка завершена.";
        ShowResultBanner(
            presentation,
            $"{GetRouteDisplayName(report.Route)} · {report.Duration.TotalSeconds:N1} с · подробности в полном логе");

        string lastTargetCheckpoint = report.LastTargetCheckpoint ?? "не достигнута";
        EmitLog(
            $"Native validator: {report.Status}; {report.Duration.TotalSeconds:N1} с; " +
            $"trigger {report.TriggerGameAssetPath}; original " +
            $"{report.LogicalGameAssetPath}; {report.Summary}; " +
            $"последняя целевая точка {lastTargetCheckpoint}.");
    }

    private static ValidationResultPresentation CreateResultPresentation(
        NativeValidationReport report) => report.Status switch
        {
            NativeValidationStatus.Passed => new(
                "✓ ТЕСТ ПРОЙДЕН",
                "Где: ошибок в нативной загрузке не обнаружено; FFPS принят, ResourceLoad вернул модель.",
                ValidationResultTone.Success),
            NativeValidationStatus.Crash => CreateCrashPresentation(report),
            NativeValidationStatus.EngineRejected =>
                CreateEngineRejectedPresentation(report),
            NativeValidationStatus.Timeout => new(
                "⚠ РЕЗУЛЬТАТ НЕ ОПРЕДЕЛЁН",
                $"Где: таймаут; {DescribeIncompleteLoadPosition(report)}.",
                ValidationResultTone.Warning),
            NativeValidationStatus.TargetNotRequested => new(
                "⚠ МОДЕЛЬ НЕ ПРОВЕРЕНА",
                $"Где: игра не запросила «{report.TriggerGameAssetPath}» в выбранном контексте.",
                ValidationResultTone.Warning),
            NativeValidationStatus.Inconclusive => new(
                "⚠ РЕЗУЛЬТАТ НЕ ОПРЕДЕЛЁН",
                $"Где: {DescribeIncompleteLoadPosition(report)}.",
                ValidationResultTone.Warning),
            NativeValidationStatus.PathError => new(
                "✕ ТЕСТ НЕ ЗАПУЩЕН",
                $"Где: не удалось подготовить игровой путь «{report.TriggerGameAssetPath}».",
                ValidationResultTone.SetupFailure),
            NativeValidationStatus.InstrumentationUnavailable => new(
                "✕ ТЕСТ НЕ ЗАПУЩЕН",
                "Где: в WinxClub.exe не найдены обязательные вызовы нативного загрузчика.",
                ValidationResultTone.SetupFailure),
            NativeValidationStatus.LaunchFailed => new(
                "✕ ТЕСТ НЕ ЗАПУЩЕН",
                "Где: WinxClub.exe не удалось запустить под наблюдением.",
                ValidationResultTone.SetupFailure),
            NativeValidationStatus.Cancelled => new(
                "■ ТЕСТ ОТМЕНЁН",
                "Где: остановлен пользователем до получения результата.",
                ValidationResultTone.Neutral),
            _ => new(
                "⚠ РЕЗУЛЬТАТ НЕ ОПРЕДЕЛЁН",
                $"Где: неизвестный итог «{report.Status}».",
                ValidationResultTone.Warning)
        };

    private static ValidationResultPresentation CreateCrashPresentation(
        NativeValidationReport report)
    {
        string phase = DescribeCrashPhaseShort(report.CrashPhase);
        if (report.CrashAttributionConfidence ==
            NativeCrashAttributionConfidence.Direct)
        {
            return new ValidationResultPresentation(
                "✕ ТЕСТ НЕ ПРОЙДЕН",
                $"Где: {phase}; последний этап — {DescribeLastTargetCheckpoint(report)}.",
                ValidationResultTone.Failure);
        }

        if (report.CrashAttributionConfidence ==
            NativeCrashAttributionConfidence.Possible)
        {
            return new ValidationResultPresentation(
                "⚠ КРАШ ВО ВРЕМЯ ТЕСТА",
                $"Где: {phase}; связь с моделью возможна, но не доказана.",
                ValidationResultTone.Warning);
        }

        return new ValidationResultPresentation(
            "⚠ МОДЕЛЬ НЕ ПРОВЕРЕНА",
            $"Где: {phase}; связь краша с моделью не установлена.",
            ValidationResultTone.Warning);
    }

    private static ValidationResultPresentation
        CreateEngineRejectedPresentation(NativeValidationReport report)
    {
        if (HasTargetResourceReturn(report, accepted: true))
        {
            return new ValidationResultPresentation(
                "⚠ РЕЗУЛЬТАТ НЕ ОПРЕДЕЛЁН",
                "Где: ResourceLoad вернул модель, но контрольное завершение теста не подтверждено.",
                ValidationResultTone.Warning);
        }

        string location = !report.TargetFfpsHeaderEntered
            ? "до чтения заголовка FFPS"
            : !report.TargetFfpsMagicAccepted
                ? "FFPS — сигнатура файла не принята"
                : !report.TargetFfpsVersionAccepted
                    ? "FFPS — версия сериализатора не принята"
                    : HasTargetResourceReturn(report, accepted: false)
                        ? "создание нативного объекта/сцены после FFPS 0x26; ResourceLoad вернул null"
                        : $"после FFPS 0x26; последний этап — {DescribeLastTargetCheckpoint(report)}";
        return new ValidationResultPresentation(
            "✕ ТЕСТ НЕ ПРОЙДЕН",
            $"Где: {location}.",
            ValidationResultTone.Failure);
    }

    private static string DescribeIncompleteLoadPosition(
        NativeValidationReport report)
    {
        if (HasTargetResourceReturn(report, accepted: true))
            return "модель вернулась из ResourceLoad, но контрольное окно не завершилось";
        if (report.TargetFfpsVersionAccepted)
            return "FFPS 0x26 принят, возврат ResourceLoad не подтверждён";
        if (report.TargetFfpsMagicAccepted)
            return "сигнатура FFPS принята, версия сериализатора ещё не подтверждена";
        if (report.TargetFfpsHeaderEntered)
            return "загрузчик вошёл в FFPS, принятие сигнатуры ещё не подтверждено";
        if (report.RedirectCount > 0)
            return "путь модели подменён, вход в FFPS ещё не подтверждён";
        return $"последний этап — {DescribeLastTargetCheckpoint(report)}";
    }

    private static bool HasTargetResourceReturn(
        NativeValidationReport report,
        bool accepted) => report.Events.Any(validationEvent =>
            validationEvent.Kind == NativeValidationEventKind.CheckpointReturn &&
            string.Equals(
                validationEvent.Checkpoint,
                "CP03.return",
                StringComparison.OrdinalIgnoreCase) &&
            EventFlagEquals(validationEvent, "target", expected: true) &&
            EventFlagEquals(validationEvent, "accepted", accepted));

    private static bool EventFlagEquals(
        NativeValidationEvent validationEvent,
        string key,
        bool expected) =>
        validationEvent.Data.TryGetValue(key, out string? value) &&
        bool.TryParse(value, out bool parsed) &&
        parsed == expected;

    private static string DescribeLastTargetCheckpoint(
        NativeValidationReport report) => report.LastTargetCheckpoint switch
        {
            "CP02" => "построение игрового пути ресурса",
            "CP02.return" => "готовый игровой путь ресурса",
            "CP03" => "вход в ResourceLoad",
            "CP03.return" => "возврат из ResourceLoad",
            "FFPS01" => "чтение заголовка FFPS",
            "FFPS02" => "принятие сигнатуры FFPS",
            "FFPS03" => "создание нативного объекта/сцены после FFPS 0x26",
            "CP04" => "загрузка тела Bloom",
            "CP05" => "ветка snow/hair Bloom",
            "CP06" => "загрузка волос Bloom",
            "CP07" => "настройка волос Bloom",
            "CP08" => "привязка узла сцены",
            "CP09" => "runtime-обновление волос Bloom",
            null or "" => "целевая загрузка ещё не началась",
            string checkpoint => checkpoint
        };

    private static string DescribeCrashPhaseShort(NativeCrashPhase? phase) =>
        phase switch
        {
            NativeCrashPhase.BeforeTrigger => "до запроса проверяемой модели",
            NativeCrashPhase.DuringTargetLoad => "внутри ResourceLoad модели",
            NativeCrashPhase.TargetLoadBackgroundOrUnwind =>
                "после входа в ResourceLoad, вне подтверждённого потока модели",
            NativeCrashPhase.PostReturnSurvivalWindow =>
                "после возврата модели, в контрольном окне",
            NativeCrashPhase.AfterTriggerUnattributed =>
                "после подмены пути, вне подтверждённой загрузки модели",
            _ => "фаза краша не определена"
        };

    private void ShowResultBanner(
        ValidationResultPresentation presentation,
        string metadata)
    {
        ValidationResultTitleText.Text = presentation.Title;
        ValidationResultLocationText.Text = presentation.Location;
        ValidationResultMetaText.Text = metadata;
        ApplyResultTone(presentation.Tone);
        AutomationProperties.SetName(
            ValidationResultBorder,
            $"{presentation.Title}. {presentation.Location}. {metadata}");
        AutomationProperties.SetHelpText(
            ValidationResultBorder,
            "Технические подробности доступны в списке этапов и полном JSONL-логе.");
        ValidationResultBorder.Visibility = Visibility.Visible;
        ValidationResultBorder.BringIntoView();
    }

    private void ApplyResultTone(ValidationResultTone tone)
    {
        (MediaColor background, MediaColor border, MediaColor title) = tone switch
        {
            ValidationResultTone.Success => (
                MediaColor.FromRgb(23, 53, 38),
                MediaColor.FromRgb(62, 173, 107),
                MediaColor.FromRgb(134, 226, 167)),
            ValidationResultTone.Failure => (
                MediaColor.FromRgb(59, 29, 34),
                MediaColor.FromRgb(223, 91, 104),
                MediaColor.FromRgb(255, 154, 165)),
            ValidationResultTone.SetupFailure => (
                MediaColor.FromRgb(59, 40, 27),
                MediaColor.FromRgb(201, 119, 58),
                MediaColor.FromRgb(255, 176, 107)),
            ValidationResultTone.Warning => (
                MediaColor.FromRgb(58, 49, 24),
                MediaColor.FromRgb(209, 163, 58),
                MediaColor.FromRgb(255, 209, 102)),
            _ => (
                MediaColor.FromRgb(37, 42, 51),
                MediaColor.FromRgb(109, 119, 136),
                MediaColor.FromRgb(216, 220, 228))
        };
        ValidationResultBorder.Background = new SolidColorBrush(background);
        ValidationResultBorder.BorderBrush = new SolidColorBrush(border);
        ValidationResultTitleText.Foreground = new SolidColorBrush(title);
    }

    private enum ValidationResultTone
    {
        Success,
        Failure,
        Warning,
        SetupFailure,
        Neutral
    }

    private readonly record struct ValidationResultPresentation(
        string Title,
        string Location,
        ValidationResultTone Tone);

    private void SetConfigurationEnabled(bool enabled)
    {
        FindGameButton.IsEnabled = enabled;
        ChooseGameButton.IsEnabled = enabled;
        FastModeRadio.IsEnabled = enabled;
        ContextModeRadio.IsEnabled = enabled;
        ContextModeSettingsPanel.IsEnabled = enabled;
        TargetAssetBox.IsEnabled = enabled;
        StartLevelBox.IsEnabled = enabled;
        TimeoutSelector.IsEnabled = enabled;
        FirstChanceExceptionsCheck.IsEnabled = enabled;
    }

    private void UpdateStartState()
    {
        if (StartValidationButton is null)
            return;

        bool routeConfigurationValid = !IsContextualMode ||
            (IsContextualLogicalPathValid() &&
             TryGetConfiguredStartLevel(out _));
        StartValidationButton.IsEnabled =
            !_running &&
            _executableIdentification?.IsSupported == true &&
            _gamePath is not null &&
            File.Exists(_gamePath) &&
            _modelPath is not null &&
            File.Exists(_modelPath) &&
            routeConfigurationValid;
    }

    private void ClearSessionDisplay()
    {
        if (ValidationEventsList is null)
            return;

        ValidationEventsList.Items.Clear();
        _shownHighFrequencyTargetCheckpoints.Clear();
        ValidationResultBorder.Visibility = Visibility.Collapsed;
        ValidationResultTitleText.Text = string.Empty;
        ValidationResultLocationText.Text = string.Empty;
        ValidationResultMetaText.Text = string.Empty;
        ValidationMenuScrollViewer.ScrollToTop();
        ValidationProgress.IsIndeterminate = false;
        ValidationProgress.Value = 0;
        _lastLogPath = null;
        OpenValidationLogButton.IsEnabled = false;
        if (!_running)
            ValidationStatusText.Text = "Ожидание запуска.";
    }

    private bool IsCurrentValidationRun(long runId) =>
        !_disposed && runId != 0 && _activeValidationRunId == runId;

    private void CancelActiveValidation(bool invalidateCallbacks = false)
    {
        if (invalidateCallbacks)
            _activeValidationRunId = 0;

        try
        {
            _validationCancellation?.Cancel();
        }
        catch (ObjectDisposedException)
        {
        }
    }

    private void ApplySettingsToControls()
    {
        _applyingSettings = true;
        try
        {
            bool contextual =
                _settings.Route == NativeValidationRoute.Contextual;
            ContextModeRadio.IsChecked = contextual;
            FastModeRadio.IsChecked = !contextual;

            int startLevel = _settings.ContextualStartLevel ?? 0;
            if (startLevel is < 0 or > 50)
                startLevel = NativeValidationDefaults.ContextualStartLevel;
            StartLevelBox.Text = startLevel.ToString();

            if (string.IsNullOrWhiteSpace(TargetAssetBox.Text) &&
                _modelPath is not null &&
                SettingsLogicalPathBelongsToModel(_modelPath))
            {
                TargetAssetBox.Text =
                    NormalizeLogicalPath(_settings.LogicalGameAssetPath!);
            }

            ComboBoxItem? timeoutItem = TimeoutSelector.Items
                .OfType<ComboBoxItem>()
                .FirstOrDefault(item =>
                    int.TryParse(item.Tag?.ToString(), out int seconds) &&
                    seconds == _settings.OverallTimeoutSeconds);
            if (timeoutItem is not null)
                TimeoutSelector.SelectedItem = timeoutItem;
        }
        finally
        {
            _applyingSettings = false;
        }

        ContextModeSettingsPanel.Visibility = IsContextualMode
            ? Visibility.Visible
            : Visibility.Collapsed;
        UpdateContextualPathHint();
        UpdateOriginalLogicalPathDisplay();
        UpdateStartLevelHint();
        UpdateStartState();
    }

    private bool IsLegacyContextualSettings(NativeValidatorSettings settings)
    {
        if (string.IsNullOrWhiteSpace(settings.LogicalGameAssetPath) ||
            !File.Exists(_settingsStore.SettingsFilePath))
        {
            return false;
        }

        try
        {
            using JsonDocument document = JsonDocument.Parse(
                File.ReadAllText(_settingsStore.SettingsFilePath));
            return !document.RootElement.EnumerateObject().Any(property =>
                property.Name.Equals("route", StringComparison.OrdinalIgnoreCase));
        }
        catch (Exception exception) when (
            exception is IOException or UnauthorizedAccessException or
                JsonException or InvalidOperationException)
        {
            return false;
        }
    }

    private void PersistUiSettings()
    {
        if (!_initialized || _applyingSettings || TargetAssetBox is null ||
            StartLevelBox is null || TimeoutSelector is null)
        {
            return;
        }

        int? contextualStartLevel = _settings.ContextualStartLevel;
        if (int.TryParse(StartLevelBox.Text, out int parsedStartLevel) &&
            parsedStartLevel is >= 0 and <= 50)
        {
            // Settings keep the UI's explicit 0 so it survives JSON round-trips;
            // the request maps 0 to null (normal game startup).
            contextualStartLevel = parsedStartLevel;
        }

        string logicalPath = NormalizeLogicalPath(TargetAssetBox.Text);
        _settings = _settings with
        {
            Route = IsContextualMode
                ? NativeValidationRoute.Contextual
                : NativeValidationRoute.FastGeneric,
            ContextualStartLevel = contextualStartLevel,
            LogicalGameAssetPath = string.IsNullOrWhiteSpace(logicalPath)
                ? null
                : logicalPath,
            LogicalGameAssetSourcePath = string.IsNullOrWhiteSpace(logicalPath) ||
                _modelPath is null
                ? null
                : Path.GetFullPath(_modelPath),
            OverallTimeoutSeconds = GetSelectedTimeoutSeconds()
        };
        TrySaveSettings();
    }

    private bool TryGetConfiguredStartLevel(out int? startLevel)
    {
        startLevel = null;
        if (StartLevelBox is null ||
            !int.TryParse(StartLevelBox.Text, out int value) ||
            value is < 0 or > 50)
        {
            return false;
        }

        if (value == 0)
            return true;

        if (!WinxClubLevelCatalog.TryGet(value, out WinxClubLevelInfo? level) ||
            level?.CanStart != true)
        {
            return false;
        }

        startLevel = value;
        return true;
    }

    private void UpdateStartLevelHint()
    {
        if (StartLevelHintText is null || StartLevelBox is null)
            return;

        if (!int.TryParse(StartLevelBox.Text, out int value) ||
            value is < 0 or > 50)
        {
            StartLevelHintText.Text =
                "Введите 0 или номер встроенного уровня от 1 до 50.";
            return;
        }

        if (value == 0)
        {
            StartLevelHintText.Text =
                "0 — штатный startup без принудительного выбора уровня.";
            return;
        }

        if (!WinxClubLevelCatalog.TryGet(value, out WinxClubLevelInfo? level) ||
            level is null)
        {
            StartLevelHintText.Text =
                $"⚠ Уровень {value} отсутствует в исследованном каталоге.";
            return;
        }

        if (value is >= 38 and <= 40)
        {
            StartLevelHintText.Text =
                $"⚠ {value} — пустой слот в таблице игры: путь SPL отсутствует. " +
                "Нативный запуск этого маршрута невозможен.";
            return;
        }

        if (value == 50)
        {
            StartLevelHintText.Text =
                "⚠ 50 — Gardenia04: есть SPL/SPT, но отсутствует SMO уровня. " +
                "Нативный запуск не завершится.";
            return;
        }

        string warning = string.IsNullOrWhiteSpace(level.Warning)
            ? string.Empty
            : $" {level.Warning}";
        StartLevelHintText.Text = level.CanStart
            ? $"{level.DisplayName}.{warning}"
            : $"⚠ {level.DisplayName}.{warning} Запуск этого маршрута отключён.";
    }

    private bool IsContextualLogicalPathValid()
    {
        string path = NormalizeLogicalPath(TargetAssetBox?.Text ?? string.Empty);
        return path.Contains('\\') &&
            path.EndsWith(".smo", StringComparison.OrdinalIgnoreCase);
    }

    private void UpdateContextualPathHint()
    {
        if (TargetAssetHintText is null || TargetAssetBox is null)
            return;

        TargetAssetHintText.Text = string.IsNullOrWhiteSpace(TargetAssetBox.Text) ||
            IsContextualLogicalPathValid()
            ? "Например Characters\\Troll\\Troll.smo. Символы _ и \\ вводятся напрямую. " +
                "Если уровень не использует этот путь, результатом будет «ресурс не запрошен»."
            : "Путь должен включать папку внутри Media и оканчиваться на .smo, " +
                "например Characters\\Bloom\\bloom_jeans.smo.";
    }

    private void UpdateOriginalLogicalPathDisplay()
    {
        if (OriginalLogicalPathBox is null)
            return;

        OriginalLogicalPathBox.Text = _modelPath is null
            ? "—"
            : GetOriginalLogicalGameAssetPath();
    }

    private bool SettingsLogicalPathBelongsToModel(string modelPath)
    {
        if (string.IsNullOrWhiteSpace(_settings.LogicalGameAssetPath) ||
            string.IsNullOrWhiteSpace(_settings.LogicalGameAssetSourcePath))
        {
            return false;
        }

        try
        {
            return string.Equals(
                Path.GetFullPath(_settings.LogicalGameAssetSourcePath),
                Path.GetFullPath(modelPath),
                StringComparison.OrdinalIgnoreCase);
        }
        catch (Exception exception) when (
            exception is ArgumentException or NotSupportedException or
                PathTooLongException)
        {
            return false;
        }
    }

    private string GetOriginalLogicalGameAssetPath()
    {
        string configured = NormalizeLogicalPath(TargetAssetBox.Text);
        if (!string.IsNullOrWhiteSpace(configured))
            return configured;

        return _modelPath is null
            ? "model.smo"
            : Path.GetFileName(_modelPath);
    }

    private static string GetRouteDisplayName(NativeValidationRoute route) =>
        route == NativeValidationRoute.FastGeneric
            ? "быстрая проверка"
            : "контекстная проверка";

    private void TrySaveSettings()
    {
        try
        {
            _settingsStore.Save(_settings);
        }
        catch (Exception exception)
        {
            EmitLog($"Native validator: настройки не сохранены: {exception.Message}");
        }
    }

    private int GetSelectedTimeoutSeconds()
    {
        if (TimeoutSelector.SelectedItem is ComboBoxItem item &&
            int.TryParse(item.Tag?.ToString(), out int seconds))
        {
            return seconds;
        }

        return 60;
    }

    private static string? FindGameBesideModel(string? modelPath)
    {
        if (string.IsNullOrWhiteSpace(modelPath))
            return null;

        DirectoryInfo? directory = new(Path.GetDirectoryName(modelPath)!);
        while (directory is not null)
        {
            if (directory.Name.Equals("Media", StringComparison.OrdinalIgnoreCase) &&
                directory.Parent is not null)
            {
                string candidate = Path.Combine(
                    directory.Parent.FullName, "WinxClub.exe");
                return File.Exists(candidate) ? candidate : null;
            }

            directory = directory.Parent;
        }

        return null;
    }

    private static string InferLogicalGameAssetPath(string modelPath)
    {
        string fullPath = Path.GetFullPath(modelPath);
        string separator = Path.DirectorySeparatorChar.ToString();
        string marker = $"{separator}Media{separator}";
        int media = fullPath.LastIndexOf(marker, StringComparison.OrdinalIgnoreCase);
        return media >= 0
            ? NormalizeLogicalPath(fullPath[(media + marker.Length)..])
            : string.Empty;
    }

    private static string NormalizeLogicalPath(string path) =>
        path.Trim().Trim('"').Replace('/', '\\').TrimStart('\\');

    private void EmitLog(string message) =>
        LogMessage?.Invoke(this, new NativeValidationPanelLogEventArgs(message));

    public void Dispose()
    {
        if (_disposed)
            return;

        PersistUiSettings();
        _disposed = true;
        _activeLocationRunId = 0;
        // ValidateAsync owns the active token until its finally block completes.
        // Cancelling is safe here; disposing it early can race debugger cleanup.
        CancelActiveValidation(invalidateCallbacks: true);
    }
}

public sealed class NativeValidationPanelLogEventArgs(string message) : EventArgs
{
    public string Message { get; } = message;
}
