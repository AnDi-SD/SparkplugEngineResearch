using System.Diagnostics;
using System.Globalization;
using System.Text;
using System.Text.Json;
using SmoNativeValidator.Core;

namespace SmoNativeValidator.Cli;

internal static class Program
{
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNameCaseInsensitive = true,
        WriteIndented = true
    };

    public static async Task<int> Main(string[] args)
    {
        Console.OutputEncoding = Encoding.UTF8;

        CliOptions options;
        try
        {
            options = CliOptions.Parse(args);
            if (options.ShowHelp)
            {
                PrintUsage();
                return 0;
            }
        }
        catch (CliUsageException exception)
        {
            Console.Error.WriteLine($"Ошибка: {exception.Message}");
            Console.Error.WriteLine("Используйте --help для справки.");
            return 2;
        }

        using CancellationTokenSource cancellation = new();
        ConsoleCancelEventHandler cancelHandler = (_, eventArgs) =>
        {
            eventArgs.Cancel = true;
            if (!cancellation.IsCancellationRequested)
            {
                Console.Error.WriteLine("Получен Ctrl+C; завершаю текущую проверку безопасно...");
                cancellation.Cancel();
            }
        };
        Console.CancelKeyPress += cancelHandler;

        try
        {
            return await RunAsync(options, cancellation.Token).ConfigureAwait(false);
        }
        catch (OperationCanceledException)
        {
            Console.Error.WriteLine("Пакетная проверка отменена.");
            return 130;
        }
        catch (CliUsageException exception)
        {
            Console.Error.WriteLine($"Ошибка: {exception.Message}");
            return 2;
        }
        catch (Exception exception)
        {
            Console.Error.WriteLine($"Неожиданная ошибка CLI: {exception}");
            return 1;
        }
        finally
        {
            Console.CancelKeyPress -= cancelHandler;
        }
    }

    private static async Task<int> RunAsync(CliOptions options, CancellationToken cancellationToken)
    {
        string executablePath = Path.GetFullPath(options.ExecutablePath!);
        if (!File.Exists(executablePath))
            throw new CliUsageException($"WinxClub.exe не найден: {executablePath}");

        IReadOnlyList<ValidationCase> cases = options.ManifestPath is not null
            ? await LoadManifestAsync(options.ManifestPath, cancellationToken).ConfigureAwait(false)
            :
            [
                new ValidationCase
                {
                    Name = Path.GetFileNameWithoutExtension(options.AssetPath!),
                    Asset = Path.GetFullPath(options.AssetPath!),
                    Logical = options.LogicalPath!,
                    Arguments = options.Arguments,
                    WorkingDirectory = options.WorkingDirectory
                }
            ];

        if (cases.Count == 0)
            throw new CliUsageException("Манифест не содержит ни одного кейса.");

        string outputRoot = Path.GetFullPath(options.OutputDirectory);
        string runDirectory = CreateRunDirectory(outputRoot);
        DateTimeOffset startedUtc = DateTimeOffset.UtcNow;
        List<CaseSummary> summaries = new(cases.Count);
        WinxClubNativeValidator validator = new();

        Console.WriteLine($"Executable: {executablePath}");
        Console.WriteLine($"Кейсов:     {cases.Count}");
        Console.WriteLine($"Результаты: {runDirectory}");

        bool wasCancelled = false;
        for (int index = 0; index < cases.Count; index++)
        {
            if (cancellationToken.IsCancellationRequested)
            {
                wasCancelled = true;
                break;
            }
            ValidationCase testCase = cases[index];
            ValidateCase(testCase, index + 1);

            string assetPath = Path.GetFullPath(testCase.Asset);
            string displayName = string.IsNullOrWhiteSpace(testCase.Name)
                ? Path.GetFileNameWithoutExtension(assetPath)
                : testCase.Name.Trim();
            string fileStem = $"{index + 1:D3}_{SanitizeFileName(displayName)}";
            string logFilePath = Path.Combine(runDirectory, fileStem + ".jsonl");
            TimeSpan timeout = SecondsOrDefault(testCase.TimeoutSeconds, options.Timeout);
            TimeSpan noProgressTimeout = SecondsOrDefault(
                testCase.NoProgressTimeoutSeconds,
                options.NoProgressTimeout);
            TimeSpan survivalWindow = SecondsOrDefault(
                testCase.SurvivalWindowSeconds,
                options.SurvivalWindow);
            NativeValidationRoute route = ResolveRoute(
                testCase.Route,
                options.Route,
                index + 1);
            int? requestedStartLevel = testCase.StartLevel ?? options.StartLevel;
            bool useIsolatedLaunchWorkspace =
                testCase.UseIsolatedLaunchWorkspace ?? options.UseIsolatedLaunchWorkspace;
            bool requireSceneReady =
                testCase.RequireSceneReady ?? options.RequireSceneReady;
            string? configuredWorkingDirectory = testCase.WorkingDirectory ?? options.WorkingDirectory;
            string triggerGameAssetPath = ResolveTriggerGameAssetPath(
                testCase.Trigger,
                options.TriggerGameAssetPath,
                route,
                testCase.Logical);
            ValidateRouteOptions(
                index + 1,
                route,
                requestedStartLevel,
                useIsolatedLaunchWorkspace,
                configuredWorkingDirectory);
            if (requireSceneReady && route != NativeValidationRoute.Contextual)
            {
                throw new CliUsageException(
                    $"Кейс {index + 1}: requireSceneReady доступен только для contextual route.");
            }
            if (requireSceneReady && requestedStartLevel is not > 0)
            {
                throw new CliUsageException(
                    $"Case {index + 1}: requireSceneReady requires a positive startLevel.");
            }

            Console.WriteLine();
            Console.WriteLine($"[{index + 1}/{cases.Count}] {displayName}");
            Console.WriteLine($"  SMO:     {assetPath}");
            Console.WriteLine($"  Logical: {testCase.Logical}");
            Console.WriteLine($"  Route:   {route}");
            Console.WriteLine($"  Trigger: {triggerGameAssetPath}");
            if (requestedStartLevel is > 0)
                Console.WriteLine($"  startLevel: {requestedStartLevel} (изолированный оконный запуск)");
            if (requireSceneReady)
                Console.WriteLine($"  Scene:   required (active native level state {requestedStartLevel}, transition queue idle)");
            Stopwatch stopwatch = Stopwatch.StartNew();

            NativeValidationReport? report = null;
            Exception? runnerException = null;
            try
            {
                NativeValidationRequest request = new()
                {
                    ExecutablePath = executablePath,
                    AssetPath = assetPath,
                    LogicalGameAssetPath = testCase.Logical,
                    Route = route,
                    TriggerGameAssetPath = triggerGameAssetPath,
                    StartLevel = requestedStartLevel,
                    UseIsolatedLaunchWorkspace = useIsolatedLaunchWorkspace,
                    Arguments = testCase.Arguments ?? options.Arguments,
                    WorkingDirectory = ResolveOptionalPath(configuredWorkingDirectory),
                    LogFilePath = logFilePath,
                    OverallTimeout = timeout,
                    NoProgressTimeout = noProgressTimeout,
                    SurvivalWindow = survivalWindow,
                    RequireSceneReady = requireSceneReady,
                    IncludeBloomCheckpoints = testCase.IncludeBloomCheckpoints ?? options.IncludeBloomCheckpoints,
                    StringComparisonProbe = testCase.StringComparisonProbe ?? options.StringComparisonProbe,
                    CollectFirstChanceExceptions = testCase.CollectFirstChanceExceptions ?? options.CollectFirstChanceExceptions,
                    StageAsset = testCase.StageAsset ?? options.StageAsset
                };
                ConsoleProgress progress = new(
                    index + 1,
                    cases.Count,
                    options.VerboseConsole);
                report = await validator.ValidateAsync(request, progress, cancellationToken)
                    .ConfigureAwait(false);
            }
            catch (OperationCanceledException exception)
            {
                runnerException = exception;
                wasCancelled = true;
            }
            catch (Exception exception)
            {
                runnerException = exception;
                Console.Error.WriteLine($"  RUNNER ERROR: {exception.Message}");
            }
            stopwatch.Stop();

            CaseSummary summary = CaseSummary.Create(
                index + 1,
                displayName,
                assetPath,
                testCase.Logical,
                route,
                triggerGameAssetPath,
                stopwatch.Elapsed,
                logFilePath,
                report,
                runnerException);
            summaries.Add(summary);
            Console.WriteLine($"  => {summary.Status}: {summary.Summary}");
            Console.WriteLine(
                $"  Native: {summary.Route}, trigger {summary.TriggerGameAssetPath}, " +
                $"isolated={summary.UsedIsolatedLaunchWorkspace.ToString().ToLowerInvariant()}, " +
                $"media={summary.MediaSourceDirectory ?? "none"}, " +
                $"startLevel={summary.AppliedStartLevel?.ToString(CultureInfo.InvariantCulture) ?? "none"}, " +
                $"duration={FormatDuration(summary.ReportDurationMilliseconds)}, " +
                $"crash={summary.CrashPhase ?? "none"}/{summary.CrashAttributionConfidence ?? "none"}");
            Console.WriteLine($"  Log: {summary.LogFilePath}");
            if (wasCancelled || report?.Status == NativeValidationStatus.Cancelled)
            {
                wasCancelled = true;
                break;
            }
        }

        RunSummary runSummary = new()
        {
            StartedUtc = startedUtc,
            FinishedUtc = DateTimeOffset.UtcNow,
            ExecutablePath = executablePath,
            OutputDirectory = runDirectory,
            CaseCount = cases.Count,
            CompletedCaseCount = summaries.Count,
            WasCancelled = wasCancelled,
            StatusCounts = summaries
                .GroupBy(item => item.Status, StringComparer.Ordinal)
                .ToDictionary(group => group.Key, group => group.Count(), StringComparer.Ordinal),
            Cases = summaries
        };

        string summaryJsonPath = Path.Combine(runDirectory, "summary.json");
        string summaryTsvPath = Path.Combine(runDirectory, "summary.tsv");
        await File.WriteAllTextAsync(
            summaryJsonPath,
            JsonSerializer.Serialize(runSummary, JsonOptions) + Environment.NewLine,
            new UTF8Encoding(encoderShouldEmitUTF8Identifier: false),
            CancellationToken.None).ConfigureAwait(false);
        await File.WriteAllTextAsync(
            summaryTsvPath,
            BuildTsv(summaries),
            new UTF8Encoding(encoderShouldEmitUTF8Identifier: false),
            CancellationToken.None).ConfigureAwait(false);

        Console.WriteLine();
        Console.WriteLine("Пакет завершён:");
        foreach ((string status, int count) in runSummary.StatusCounts.OrderBy(pair => pair.Key))
            Console.WriteLine($"  {status}: {count}");
        Console.WriteLine($"JSON: {summaryJsonPath}");
        Console.WriteLine($"TSV:  {summaryTsvPath}");

        if (wasCancelled)
            return 130;
        return summaries.All(item => item.Status == NativeValidationStatus.Passed.ToString()) ? 0 : 1;
    }

    private static async Task<IReadOnlyList<ValidationCase>> LoadManifestAsync(
        string manifestPath,
        CancellationToken cancellationToken)
    {
        string fullPath = Path.GetFullPath(manifestPath);
        if (!File.Exists(fullPath))
            throw new CliUsageException($"Манифест не найден: {fullPath}");

        await using FileStream stream = File.OpenRead(fullPath);
        JsonDocument document;
        try
        {
            document = await JsonDocument.ParseAsync(
                stream,
                new JsonDocumentOptions
                {
                    AllowTrailingCommas = true,
                    CommentHandling = JsonCommentHandling.Skip
                },
                cancellationToken).ConfigureAwait(false);
        }
        catch (JsonException exception)
        {
            throw new CliUsageException($"Некорректный JSON-манифест: {exception.Message}");
        }

        using (document)
        {
            List<ValidationCase>? cases;
            try
            {
                cases = document.RootElement.ValueKind switch
                {
                    JsonValueKind.Array => document.RootElement.Deserialize<List<ValidationCase>>(JsonOptions),
                    JsonValueKind.Object => document.RootElement
                        .Deserialize<ValidationManifest>(JsonOptions)?.Cases,
                    _ => throw new CliUsageException("Корень манифеста должен быть объектом с cases или JSON-массивом.")
                };
            }
            catch (JsonException exception)
            {
                throw new CliUsageException($"Некорректный манифест: {exception.Message}");
            }

            cases ??= [];
            string baseDirectory = Path.GetDirectoryName(fullPath)!;
            foreach (ValidationCase testCase in cases)
            {
                if (!string.IsNullOrWhiteSpace(testCase.Asset) && !Path.IsPathFullyQualified(testCase.Asset))
                    testCase.Asset = Path.GetFullPath(testCase.Asset, baseDirectory);
                if (!string.IsNullOrWhiteSpace(testCase.WorkingDirectory) &&
                    !Path.IsPathFullyQualified(testCase.WorkingDirectory))
                {
                    testCase.WorkingDirectory = Path.GetFullPath(testCase.WorkingDirectory, baseDirectory);
                }
            }
            return cases;
        }
    }

    private static void ValidateCase(ValidationCase testCase, int index)
    {
        if (string.IsNullOrWhiteSpace(testCase.Asset))
            throw new CliUsageException($"Кейс {index}: поле asset обязательно.");
        if (string.IsNullOrWhiteSpace(testCase.Logical))
            throw new CliUsageException($"Кейс {index}: поле logical обязательно.");
        if (testCase.StartLevel is < 0 or > 50)
            throw new CliUsageException($"Кейс {index}: startLevel должен быть в диапазоне 0..50.");
        ValidateOptionalSeconds(testCase.TimeoutSeconds, index, "timeoutSeconds", allowZero: false);
        ValidateOptionalSeconds(testCase.NoProgressTimeoutSeconds, index, "noProgressTimeoutSeconds", allowZero: true);
        ValidateOptionalSeconds(testCase.SurvivalWindowSeconds, index, "survivalWindowSeconds", allowZero: true);
    }

    private static NativeValidationRoute ResolveRoute(
        string? caseRoute,
        NativeValidationRoute fallback,
        int caseIndex)
    {
        if (string.IsNullOrWhiteSpace(caseRoute))
            return fallback;

        return ParseRoute(caseRoute, $"Кейс {caseIndex}: route");
    }

    private static NativeValidationRoute ParseRoute(string value, string source)
    {
        return value.Trim().ToLowerInvariant() switch
        {
            "fast" or "fastgeneric" or "fast-generic" => NativeValidationRoute.FastGeneric,
            "context" or "contextual" => NativeValidationRoute.Contextual,
            _ => throw new CliUsageException($"{source} ожидает fast или contextual.")
        };
    }

    private static string ResolveTriggerGameAssetPath(
        string? caseTrigger,
        string? fallbackTrigger,
        NativeValidationRoute route,
        string logicalPath)
    {
        string? configured = !string.IsNullOrWhiteSpace(caseTrigger)
            ? caseTrigger
            : fallbackTrigger;
        if (!string.IsNullOrWhiteSpace(configured))
            return configured.Trim();

        return route == NativeValidationRoute.FastGeneric
            ? NativeValidationDefaults.FastTriggerGameAssetPath
            : logicalPath;
    }

    private static void ValidateRouteOptions(
        int caseIndex,
        NativeValidationRoute route,
        int? requestedStartLevel,
        bool useIsolatedLaunchWorkspace,
        string? workingDirectory)
    {
        if (!useIsolatedLaunchWorkspace)
        {
            throw new CliUsageException(
                $"Кейс {caseIndex}: нативная проверка допускает только изолированный " +
                "оконный запуск (useIsolatedLaunchWorkspace=true).");
        }
        if (requestedStartLevel is < 0 or > 50)
            throw new CliUsageException($"Кейс {caseIndex}: startLevel должен быть в диапазоне 0..50.");
        if (requestedStartLevel is > 0 && route != NativeValidationRoute.Contextual)
        {
            throw new CliUsageException(
                $"Кейс {caseIndex}: startLevel доступен только для route=contextual.");
        }
        if (useIsolatedLaunchWorkspace && !string.IsNullOrWhiteSpace(workingDirectory))
        {
            throw new CliUsageException(
                $"Кейс {caseIndex}: workingDirectory нельзя сочетать с изолированным запуском.");
        }
    }

    private static void ValidateOptionalSeconds(double? value, int index, string name, bool allowZero)
    {
        if (value is not double seconds)
            return;
        bool valid = double.IsFinite(seconds) && (allowZero ? seconds >= 0 : seconds > 0);
        if (!valid)
            throw new CliUsageException($"Кейс {index}: {name} имеет недопустимое значение.");
    }

    private static string CreateRunDirectory(string outputRoot)
    {
        Directory.CreateDirectory(outputRoot);
        string timestamp = DateTimeOffset.Now.ToString("yyyyMMdd-HHmmss-fff", CultureInfo.InvariantCulture);
        for (int suffix = 0; ; suffix++)
        {
            string name = suffix == 0 ? $"run-{timestamp}" : $"run-{timestamp}-{suffix:D2}";
            string candidate = Path.Combine(outputRoot, name);
            if (Directory.Exists(candidate))
                continue;
            Directory.CreateDirectory(candidate);
            return candidate;
        }
    }

    private static TimeSpan SecondsOrDefault(double? seconds, TimeSpan fallback) =>
        seconds is double value ? TimeSpan.FromSeconds(value) : fallback;

    private static string? ResolveOptionalPath(string? path) =>
        string.IsNullOrWhiteSpace(path) ? null : Path.GetFullPath(path);

    private static string FormatDuration(long? milliseconds) =>
        milliseconds is long value
            ? (value / 1000d).ToString("0.000", CultureInfo.InvariantCulture) + "s"
            : "n/a";

    private static string SanitizeFileName(string value)
    {
        HashSet<char> invalid = Path.GetInvalidFileNameChars().ToHashSet();
        StringBuilder result = new(value.Length);
        foreach (char character in value)
            result.Append(invalid.Contains(character) || char.IsControl(character) ? '_' : character);
        string sanitized = result.ToString().Trim().TrimEnd('.');
        return string.IsNullOrEmpty(sanitized) ? "case" : sanitized;
    }

    private static string BuildTsv(IReadOnlyList<CaseSummary> summaries)
    {
        StringBuilder result = new();
        result.AppendLine(
            "index\tname\tstatus\tduration_ms\tasset\tlogical\tlast_target_checkpoint\t" +
            "ffps_header\tffps_magic\tffps_version\tscene_ready\texception_code\texception_eip\t" +
            "exit_code\tredirect_count\tlog\tsummary\troute\ttrigger\tapplied_start_level\t" +
            "isolated\tmedia_source\treport_duration_ms\tcrash_phase\tcrash_attribution_confidence");
        foreach (CaseSummary item in summaries)
        {
            string[] values =
            [
                item.Index.ToString(CultureInfo.InvariantCulture),
                item.Name,
                item.Status,
                item.DurationMilliseconds.ToString(CultureInfo.InvariantCulture),
                item.AssetPath,
                item.LogicalGameAssetPath,
                item.LastTargetCheckpoint ?? string.Empty,
                item.TargetFfpsHeaderEntered.ToString(CultureInfo.InvariantCulture),
                item.TargetFfpsMagicAccepted.ToString(CultureInfo.InvariantCulture),
                item.TargetFfpsVersionAccepted.ToString(CultureInfo.InvariantCulture),
                item.SceneReadyReached.ToString(CultureInfo.InvariantCulture),
                item.Exception is null ? string.Empty : $"0x{item.Exception.Code:X8}",
                item.Exception is null ? string.Empty : $"0x{item.Exception.InstructionPointer:X8}",
                item.ExitCode?.ToString(CultureInfo.InvariantCulture) ?? string.Empty,
                item.RedirectCount.ToString(CultureInfo.InvariantCulture),
                item.LogFilePath,
                item.Summary,
                item.Route,
                item.TriggerGameAssetPath,
                item.AppliedStartLevel?.ToString(CultureInfo.InvariantCulture) ?? string.Empty,
                item.UsedIsolatedLaunchWorkspace.ToString().ToLowerInvariant(),
                item.MediaSourceDirectory ?? string.Empty,
                item.ReportDurationMilliseconds?.ToString(CultureInfo.InvariantCulture) ?? string.Empty,
                item.CrashPhase ?? string.Empty,
                item.CrashAttributionConfidence ?? string.Empty
            ];
            result.AppendLine(string.Join('\t', values.Select(EscapeTsv)));
        }
        return result.ToString();
    }

    private static string EscapeTsv(string value) =>
        value.Replace('\t', ' ').Replace('\r', ' ').Replace('\n', ' ');

    private static void PrintUsage()
    {
        Console.WriteLine("""
SmoNativeValidator.Cli — последовательная проверка SMO нативным кодом WinxClub.exe

Маршруты:
  contextual (по умолчанию)       Ждёт исходный logical-путь в реальном игровом контексте.
  fast                            Быстрый smoke-test через ранний Menus\mousecursor.smo:
                                  проверяет нативный загрузчик/FFPS/создание ресурсов,
                                  но не заменяет контекстную проверку скриптов и анимаций.

Один файл:
  SmoNativeValidator.Cli --exe <WinxClub.exe> --asset <model.smo> --logical <Media-relative.smo> [options]

Пакет:
  SmoNativeValidator.Cli --exe <WinxClub.exe> --manifest <cases.json> [options]

Обязательные параметры:
  --exe <path>                    Путь к WinxClub.exe.
  --asset <path>                  SMO для одиночного запуска.
  --logical <path>                Игровой путь ресурса, например Characters\Troll\Troll.smo.
  --manifest <path>               JSON-манифест; заменяет --asset и --logical.

Параметры:
  --route <fast|contextual>        Маршрут проверки (по умолчанию contextual).
  --trigger <Media-relative.smo>   Явно подтвердить trigger маршрута; обычно не требуется.
  --isolated-windowed              Совместимый alias; оконный workspace теперь обязателен.
  --start-level <0..50>            Нативный startLevel для contextual в изолированном workspace;
                                  0 означает обычный старт, недоступные уровни отвергает Core.
  --output-dir <path>             Каталог результатов (по умолчанию native-validation-results).
  --timeout <seconds>             Общий timeout кейса (по умолчанию 120).
  --no-progress-timeout <seconds> Timeout без целевого прогресса (по умолчанию 30; 0 отключает).
  --survival-window <seconds>     Окно стабильности после загрузки (по умолчанию 2).
  --require-scene-ready           Требовать активный startLevel и пустую очередь переходов.
  --arguments <text>              Аргументы WinxClub.exe.
  --working-directory <path>      Рабочий каталог игры.
  --include-bloom                 Включить дополнительные Bloom checkpoints.
  --probe-string <text>           Логировать выбранные native-сравнения, где аргумент
                                  содержит text; без параметра probes не устанавливаются.
  --no-first-chance               Не собирать first-chance exceptions.
  --no-stage                      Не копировать SMO во временный ASCII-путь.
  --verbose                       Выводить в консоль все фоновые checkpoints и debug output.
  -h, --help                      Показать эту справку.

Пути и logical-имена передаются как обычный текст; символ подчёркивания не требует экранирования.
Манифест может переопределять route, trigger, startLevel, requireSceneReady и useIsolatedLaunchWorkspace для кейса.
""");
    }

    private sealed class ConsoleProgress(
        int caseIndex,
        int caseCount,
        bool verbose) : IProgress<NativeValidationEvent>
    {
        private readonly HashSet<string> _shownHighFrequencyCheckpoints =
            new(StringComparer.OrdinalIgnoreCase);

        public void Report(NativeValidationEvent value)
        {
            if (!verbose && !ShouldShow(value))
                return;
            if (!verbose &&
                value.Kind == NativeValidationEventKind.CheckpointEnter &&
                (value.Checkpoint is "CP08" or "CP09") &&
                !_shownHighFrequencyCheckpoints.Add(value.Checkpoint))
            {
                return;
            }

            string percent = value.ProgressPercent is double progress
                ? $" {progress,6:0.0}%"
                : "       ";
            string checkpoint = string.IsNullOrWhiteSpace(value.Checkpoint)
                ? string.Empty
                : $" [{value.Checkpoint}]";
            Console.WriteLine(
                $"  [{caseIndex}/{caseCount}]{percent} {value.Stage}/{value.Kind}{checkpoint}: {value.Message}");
        }

        private static bool ShouldShow(NativeValidationEvent value)
        {
            if ((int)value.Severity >= (int)NativeValidationSeverity.Warning)
                return true;
            if (value.Kind is NativeValidationEventKind.SessionStarted or
                NativeValidationEventKind.ExecutableFingerprint or
                NativeValidationEventKind.AssetStaged or
                NativeValidationEventKind.ProcessStarted or
                NativeValidationEventKind.PathRedirected or
                NativeValidationEventKind.ProcessExited or
                NativeValidationEventKind.Cleanup or
                NativeValidationEventKind.SessionCompleted)
            {
                return true;
            }

            foreach (string key in new[]
            {
                "target",
                "targetContext",
                "matchesLogicalTarget",
                "targetCandidate"
            })
            {
                if (value.Data.TryGetValue(key, out string? text) &&
                    bool.TryParse(text, out bool isTarget) &&
                    isTarget)
                {
                    return true;
                }
            }
            return false;
        }
    }

    private sealed class CliUsageException(string message) : Exception(message);

    private sealed class CliOptions
    {
        public string? ExecutablePath { get; private set; }
        public string? AssetPath { get; private set; }
        public string? LogicalPath { get; private set; }
        public string? ManifestPath { get; private set; }
        public NativeValidationRoute Route { get; private set; } = NativeValidationRoute.Contextual;
        public string? TriggerGameAssetPath { get; private set; }
        public int? StartLevel { get; private set; }
        public bool UseIsolatedLaunchWorkspace { get; private set; } = true;
        public string OutputDirectory { get; private set; } = "native-validation-results";
        public TimeSpan Timeout { get; private set; } = TimeSpan.FromSeconds(120);
        public TimeSpan NoProgressTimeout { get; private set; } = TimeSpan.FromSeconds(30);
        public TimeSpan SurvivalWindow { get; private set; } = TimeSpan.FromSeconds(2);
        public bool RequireSceneReady { get; private set; }
        public string? Arguments { get; private set; }
        public string? WorkingDirectory { get; private set; }
        public bool IncludeBloomCheckpoints { get; private set; }
        public string? StringComparisonProbe { get; private set; }
        public bool CollectFirstChanceExceptions { get; private set; } = true;
        public bool StageAsset { get; private set; } = true;
        public bool VerboseConsole { get; private set; }
        public bool ShowHelp { get; private set; }

        public static CliOptions Parse(string[] args)
        {
            CliOptions result = new();
            for (int index = 0; index < args.Length; index++)
            {
                string argument = args[index];
                switch (argument.ToLowerInvariant())
                {
                    case "-h":
                    case "--help":
                        result.ShowHelp = true;
                        break;
                    case "--exe":
                        result.ExecutablePath = ReadValue(args, ref index, argument);
                        break;
                    case "--asset":
                        result.AssetPath = ReadValue(args, ref index, argument);
                        break;
                    case "--logical":
                        result.LogicalPath = ReadValue(args, ref index, argument);
                        break;
                    case "--manifest":
                        result.ManifestPath = ReadValue(args, ref index, argument);
                        break;
                    case "--route":
                        result.Route = ParseRoute(
                            ReadValue(args, ref index, argument),
                            argument);
                        break;
                    case "--trigger":
                        result.TriggerGameAssetPath = ReadValue(args, ref index, argument);
                        break;
                    case "--start-level":
                        result.StartLevel = ReadStartLevel(args, ref index, argument);
                        break;
                    case "--isolated-windowed":
                        result.UseIsolatedLaunchWorkspace = true;
                        break;
                    case "--output-dir":
                        result.OutputDirectory = ReadValue(args, ref index, argument);
                        break;
                    case "--timeout":
                        result.Timeout = ReadSeconds(args, ref index, argument, allowZero: false);
                        break;
                    case "--no-progress-timeout":
                        result.NoProgressTimeout = ReadSeconds(args, ref index, argument, allowZero: true);
                        break;
                    case "--survival-window":
                        result.SurvivalWindow = ReadSeconds(args, ref index, argument, allowZero: true);
                        break;
                    case "--require-scene-ready":
                        result.RequireSceneReady = true;
                        break;
                    case "--arguments":
                        result.Arguments = ReadValue(args, ref index, argument);
                        break;
                    case "--working-directory":
                        result.WorkingDirectory = ReadValue(args, ref index, argument);
                        break;
                    case "--include-bloom":
                        result.IncludeBloomCheckpoints = true;
                        break;
                    case "--probe-string":
                        result.StringComparisonProbe = ReadValue(args, ref index, argument);
                        break;
                    case "--no-first-chance":
                        result.CollectFirstChanceExceptions = false;
                        break;
                    case "--no-stage":
                        result.StageAsset = false;
                        break;
                    case "--verbose":
                        result.VerboseConsole = true;
                        break;
                    default:
                        throw new CliUsageException($"Неизвестный параметр: {argument}");
                }
            }

            if (result.ShowHelp)
                return result;
            if (string.IsNullOrWhiteSpace(result.ExecutablePath))
                throw new CliUsageException("Параметр --exe обязателен.");

            bool hasManifest = !string.IsNullOrWhiteSpace(result.ManifestPath);
            bool hasAsset = !string.IsNullOrWhiteSpace(result.AssetPath);
            bool hasLogical = !string.IsNullOrWhiteSpace(result.LogicalPath);
            if (hasManifest && (hasAsset || hasLogical))
                throw new CliUsageException("Используйте либо --manifest, либо пару --asset/--logical.");
            if (!hasManifest && (!hasAsset || !hasLogical))
                throw new CliUsageException("Без --manifest обязательны оба параметра --asset и --logical.");
            if (string.IsNullOrWhiteSpace(result.OutputDirectory))
                throw new CliUsageException("Параметр --output-dir не может быть пустым.");

            return result;
        }

        private static string ReadValue(string[] args, ref int index, string option)
        {
            if (++index >= args.Length)
                throw new CliUsageException($"После {option} ожидается значение.");
            return args[index];
        }

        private static TimeSpan ReadSeconds(
            string[] args,
            ref int index,
            string option,
            bool allowZero)
        {
            string text = ReadValue(args, ref index, option);
            if (!double.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out double seconds) ||
                !double.IsFinite(seconds) ||
                (allowZero ? seconds < 0 : seconds <= 0))
            {
                throw new CliUsageException($"{option} ожидает число секунд {(allowZero ? ">= 0" : "> 0")}.");
            }
            return TimeSpan.FromSeconds(seconds);
        }

        private static int ReadStartLevel(string[] args, ref int index, string option)
        {
            string text = ReadValue(args, ref index, option);
            if (!int.TryParse(text, NumberStyles.Integer, CultureInfo.InvariantCulture, out int level) ||
                level is < 0 or > 50)
            {
                throw new CliUsageException($"{option} ожидает целое число в диапазоне 0..50.");
            }
            return level;
        }
    }

    private sealed class ValidationManifest
    {
        public List<ValidationCase> Cases { get; set; } = [];
    }

    private sealed class ValidationCase
    {
        public string? Name { get; set; }
        public string Asset { get; set; } = string.Empty;
        public string Logical { get; set; } = string.Empty;
        public string? Route { get; set; }
        public string? Trigger { get; set; }
        public int? StartLevel { get; set; }
        public bool? UseIsolatedLaunchWorkspace { get; set; }
        public string? Arguments { get; set; }
        public string? WorkingDirectory { get; set; }
        public double? TimeoutSeconds { get; set; }
        public double? NoProgressTimeoutSeconds { get; set; }
        public double? SurvivalWindowSeconds { get; set; }
        public bool? RequireSceneReady { get; set; }
        public bool? IncludeBloomCheckpoints { get; set; }
        public string? StringComparisonProbe { get; set; }
        public bool? CollectFirstChanceExceptions { get; set; }
        public bool? StageAsset { get; set; }
    }

    private sealed record CaseSummary
    {
        public required int Index { get; init; }
        public required string Name { get; init; }
        public required string Status { get; init; }
        public required string Summary { get; init; }
        public required long DurationMilliseconds { get; init; }
        public required string AssetPath { get; init; }
        public required string LogicalGameAssetPath { get; init; }
        public required string Route { get; init; }
        public required string TriggerGameAssetPath { get; init; }
        public int? AppliedStartLevel { get; init; }
        public bool UsedIsolatedLaunchWorkspace { get; init; }
        public string? MediaSourceDirectory { get; init; }
        public long? ReportDurationMilliseconds { get; init; }
        public string? CrashPhase { get; init; }
        public string? CrashAttributionConfidence { get; init; }
        public required string LogFilePath { get; init; }
        public string? LastCheckpoint { get; init; }
        public string? LastTargetCheckpoint { get; init; }
        public bool TargetFfpsHeaderEntered { get; init; }
        public bool TargetFfpsMagicAccepted { get; init; }
        public bool TargetFfpsVersionAccepted { get; init; }
        public bool SceneReadyReached { get; init; }
        public int? ExitCode { get; init; }
        public int RedirectCount { get; init; }
        public NativeExceptionSnapshot? Exception { get; init; }
        public string? RunnerException { get; init; }

        public static CaseSummary Create(
            int index,
            string name,
            string assetPath,
            string logicalPath,
            NativeValidationRoute route,
            string triggerGameAssetPath,
            TimeSpan duration,
            string requestedLogPath,
            NativeValidationReport? report,
            Exception? runnerException)
        {
            if (report is null)
            {
                return new CaseSummary
                {
                    Index = index,
                    Name = name,
                    Status = "RunnerError",
                    Summary = runnerException?.Message ?? "Validator did not return a report.",
                    DurationMilliseconds = (long)duration.TotalMilliseconds,
                    AssetPath = assetPath,
                    LogicalGameAssetPath = logicalPath,
                    Route = route.ToString(),
                    TriggerGameAssetPath = triggerGameAssetPath,
                    LogFilePath = requestedLogPath,
                    RunnerException = runnerException?.ToString()
                };
            }

            return new CaseSummary
            {
                Index = index,
                Name = name,
                Status = report.Status.ToString(),
                Summary = report.Summary,
                DurationMilliseconds = (long)duration.TotalMilliseconds,
                AssetPath = report.AssetPath,
                LogicalGameAssetPath = report.LogicalGameAssetPath,
                Route = report.Route.ToString(),
                TriggerGameAssetPath = report.TriggerGameAssetPath,
                AppliedStartLevel = report.StartLevel,
                UsedIsolatedLaunchWorkspace = report.UsedIsolatedLaunchWorkspace,
                MediaSourceDirectory = report.MediaSourceDirectory,
                ReportDurationMilliseconds = (long)report.Duration.TotalMilliseconds,
                CrashPhase = report.CrashPhase?.ToString(),
                CrashAttributionConfidence = report.CrashAttributionConfidence?.ToString(),
                LogFilePath = report.LogFilePath ?? requestedLogPath,
                LastCheckpoint = report.LastCheckpoint,
                LastTargetCheckpoint = report.LastTargetCheckpoint,
                TargetFfpsHeaderEntered = report.TargetFfpsHeaderEntered,
                TargetFfpsMagicAccepted = report.TargetFfpsMagicAccepted,
                TargetFfpsVersionAccepted = report.TargetFfpsVersionAccepted,
                SceneReadyReached = report.SceneReadyReached,
                ExitCode = report.ExitCode,
                RedirectCount = report.RedirectCount,
                Exception = report.Exception
            };
        }
    }

    private sealed record RunSummary
    {
        public required DateTimeOffset StartedUtc { get; init; }
        public required DateTimeOffset FinishedUtc { get; init; }
        public required string ExecutablePath { get; init; }
        public required string OutputDirectory { get; init; }
        public required int CaseCount { get; init; }
        public required int CompletedCaseCount { get; init; }
        public required bool WasCancelled { get; init; }
        public required IReadOnlyDictionary<string, int> StatusCounts { get; init; }
        public required IReadOnlyList<CaseSummary> Cases { get; init; }
    }
}
