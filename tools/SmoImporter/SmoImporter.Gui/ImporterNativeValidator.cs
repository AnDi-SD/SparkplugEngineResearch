using System.IO;
using SmoNativeValidator.Core;

namespace SmoImporter.Gui;

internal enum ImporterNativeVerdict
{
    Suitable,
    Unsuitable,
    Indeterminate
}

internal sealed record ImporterNativeValidationResult(
    ImporterNativeVerdict Verdict,
    string Message);

/// <summary>
/// Minimal Importer-facing adapter around the reusable native validator core.
/// The Importer intentionally exposes no routing, debugger or log settings.
/// </summary>
internal sealed class ImporterNativeValidator
{
    private readonly WinxClubNativeValidator _validator = new();
    private readonly NativeValidatorSettingsStore _settingsStore = new();
    private NativeValidatorSettings _settings;

    public ImporterNativeValidator()
    {
        _settings = _settingsStore.Load();
    }

    public string? SavedExecutablePath => _settings.ManualExecutablePath;

    public Task<string?> LocateExecutableAsync(
        string? sourceModelPath,
        CancellationToken cancellationToken = default) => Task.Run(() =>
    {
        cancellationToken.ThrowIfCancellationRequested();
        string? besideSourceModel = FindGameBesideModel(sourceModelPath);
        string? validManualPath = !string.IsNullOrWhiteSpace(
            _settings.ManualExecutablePath) &&
            File.Exists(_settings.ManualExecutablePath)
            ? _settings.ManualExecutablePath
            : null;
        string? preferred = validManualPath is not null
            ? validManualPath
            : besideSourceModel;
        WinxClubLocationResult location = WinxClubLocator.Locate(
            new WinxClubLocatorOptions
            {
                PreferredExecutablePath = preferred,
                SavedExecutablePath = _settings.ManualExecutablePath
            });
        cancellationToken.ThrowIfCancellationRequested();
        return location.SelectedPath;
    }, cancellationToken);

    public void SaveManualExecutablePath(string executablePath)
    {
        try
        {
            string normalized = Path.GetFullPath(
                executablePath.Trim().Trim('"'));
            _settings = _settings with { ManualExecutablePath = normalized };
            _settingsStore.Save(_settings);
        }
        catch (Exception exception) when (
            exception is IOException or UnauthorizedAccessException or
                System.Security.SecurityException or ArgumentException or
                NotSupportedException or PathTooLongException)
        {
            // A read-only profile must not block model creation or validation.
        }
    }

    public async Task<ImporterNativeValidationResult> ValidateAsync(
        string executablePath,
        string assetPath,
        string logicalGameAssetPath,
        CancellationToken cancellationToken = default)
    {
        NativeValidationRequest request = new()
        {
            ExecutablePath = executablePath,
            AssetPath = assetPath,
            LogicalGameAssetPath = logicalGameAssetPath,
            Route = NativeValidationRoute.FastGeneric,
            UseIsolatedLaunchWorkspace = true,
            OverallTimeout = TimeSpan.FromSeconds(60),
            NoProgressTimeout = TimeSpan.FromSeconds(15),
            SurvivalWindow = TimeSpan.FromSeconds(2),
            CollectFirstChanceExceptions = false,
            StageAsset = true,
            AllowFileNameOnlyLogicalPath = true
        };

        NativeValidationReport report = await _validator.ValidateAsync(
            request,
            progress: null,
            cancellationToken).ConfigureAwait(false);

        bool engineRejectedTarget = report.Status == NativeValidationStatus.EngineRejected &&
            report.Events.Any(validationEvent =>
                validationEvent.Checkpoint == "CP03.return" &&
                validationEvent.Data.TryGetValue("target", out string? target) &&
                string.Equals(target, "true", StringComparison.OrdinalIgnoreCase) &&
                validationEvent.Data.TryGetValue("accepted", out string? accepted) &&
                string.Equals(accepted, "false", StringComparison.OrdinalIgnoreCase));
        bool directTargetCrash = report.Status == NativeValidationStatus.Crash &&
            report.CrashAttributionConfidence == NativeCrashAttributionConfidence.Direct;

        if (engineRejectedTarget)
        {
            return new ImporterNativeValidationResult(
                ImporterNativeVerdict.Unsuitable,
                "✕ Модель не подходит — нативный загрузчик отклонил SMO.");
        }
        if (directTargetCrash)
        {
            return new ImporterNativeValidationResult(
                ImporterNativeVerdict.Unsuitable,
                "✕ Модель не подходит — игра упала при загрузке SMO.");
        }

        return report.Status switch
        {
            NativeValidationStatus.Passed => new(
                ImporterNativeVerdict.Suitable,
                "✓ Модель подходит для нативной загрузки — игра приняла созданный SMO."),
            NativeValidationStatus.Crash or NativeValidationStatus.EngineRejected => new(
                ImporterNativeVerdict.Indeterminate,
                "Совместимость не определена — сбой не удалось напрямую связать с моделью."),
            NativeValidationStatus.Cancelled => new(
                ImporterNativeVerdict.Indeterminate,
                "Проверка отменена."),
            NativeValidationStatus.InstrumentationUnavailable => new(
                ImporterNativeVerdict.Indeterminate,
                "Проверка не выполнена — загрузчик этой версии игры пока не распознан."),
            NativeValidationStatus.LaunchFailed => new(
                ImporterNativeVerdict.Indeterminate,
                "Проверка не выполнена — WinxClub.exe не удалось запустить."),
            NativeValidationStatus.PathError => new(
                ImporterNativeVerdict.Indeterminate,
                "Проверка не выполнена — не удалось подготовить модель для игры."),
            _ => new(
                ImporterNativeVerdict.Indeterminate,
                "Совместимость модели определить не удалось.")
        };
    }

    private static string? FindGameBesideModel(string? modelPath)
    {
        if (string.IsNullOrWhiteSpace(modelPath))
            return null;

        DirectoryInfo? directory = new(Path.GetDirectoryName(
            Path.GetFullPath(modelPath))!);
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
}
