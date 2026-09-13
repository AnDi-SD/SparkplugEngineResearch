using System.Text.Json;
using System.Text.Json.Serialization;

namespace SmoNativeValidator.Core;

public sealed record NativeValidatorSettings
{
    public string? ManualExecutablePath { get; init; }
    public string? LogicalGameAssetPath { get; init; }
    public string? LogicalGameAssetSourcePath { get; init; }
    public NativeValidationRoute Route { get; init; } = NativeValidationRoute.FastGeneric;
    public int? ContextualStartLevel { get; init; } = NativeValidationDefaults.ContextualStartLevel;
    public int OverallTimeoutSeconds { get; init; } = 120;
    public int NoProgressTimeoutSeconds { get; init; } = 30;
    public int SurvivalWindowMilliseconds { get; init; } = 2000;
    public bool IncludeBloomCheckpoints { get; init; }
}

public sealed class NativeValidatorSettingsStore
{
    private const string ApplicationDirectory = "SparkplugEngineResearch";
    private const string ComponentDirectory = "SmoNativeValidator";
    private const string FileName = "settings.json";
    private static readonly JsonSerializerOptions SerializerOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        WriteIndented = true,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull
    };

    public NativeValidatorSettingsStore(string? settingsFilePath = null)
    {
        SettingsFilePath = settingsFilePath ?? GetDefaultSettingsFilePath();
    }

    public string SettingsFilePath { get; }

    public NativeValidatorSettings Load()
    {
        if (!File.Exists(SettingsFilePath))
            return new NativeValidatorSettings();

        try
        {
            return Deserialize(File.ReadAllText(SettingsFilePath));
        }
        catch (Exception exception) when (
            exception is IOException or UnauthorizedAccessException or JsonException)
        {
            return new NativeValidatorSettings();
        }
    }

    public void Save(NativeValidatorSettings settings)
    {
        ArgumentNullException.ThrowIfNull(settings);
        string? directory = Path.GetDirectoryName(SettingsFilePath);
        if (!string.IsNullOrEmpty(directory))
            Directory.CreateDirectory(directory);

        string temporaryPath = SettingsFilePath + ".tmp";
        File.WriteAllText(temporaryPath, Serialize(settings));
        File.Move(temporaryPath, SettingsFilePath, overwrite: true);
    }

    public static string Serialize(NativeValidatorSettings settings) =>
        JsonSerializer.Serialize(settings, SerializerOptions);

    public static NativeValidatorSettings Deserialize(string json) =>
        JsonSerializer.Deserialize<NativeValidatorSettings>(json, SerializerOptions)
        ?? new NativeValidatorSettings();

    public static string GetDefaultSettingsFilePath()
    {
        string localAppData = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        return Path.Combine(localAppData, ApplicationDirectory, ComponentDirectory, FileName);
    }
}
