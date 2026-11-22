using SmoLVLcreator.Core;
using System.IO;
using System.Text.Json;

namespace SmoLVLcreator.Gui;

public sealed record SmoLVLcreatorSettings(
    string ExportDirectory,
    SmoLevelExportFormat ExportFormat)
{
    public string Language { get; init; } = LevelText.Auto;

    public static SmoLVLcreatorSettings Default => new(
        Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments),
            "SmoLVLcreator Exports"),
        SmoLevelExportFormat.Glb);
}

public static class SmoLVLcreatorSettingsStore
{
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        WriteIndented = true
    };

    public static string SettingsPath => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "SparkplugEngineResearch",
        "SmoLVLcreator",
        "settings.json");

    public static SmoLVLcreatorSettings Load() => Load(SettingsPath);

    internal static SmoLVLcreatorSettings Load(string path)
    {
        try
        {
            if (!File.Exists(path))
                return SmoLVLcreatorSettings.Default;
            SmoLVLcreatorSettings? settings = JsonSerializer.Deserialize<
                SmoLVLcreatorSettings>(File.ReadAllText(path), JsonOptions);
            return settings is null || string.IsNullOrWhiteSpace(settings.ExportDirectory)
                ? SmoLVLcreatorSettings.Default
                : settings with
                {
                    ExportDirectory = Path.GetFullPath(settings.ExportDirectory),
                    Language = LevelText.Normalize(settings.Language)
                };
        }
        catch
        {
            return SmoLVLcreatorSettings.Default;
        }
    }

    public static void Save(SmoLVLcreatorSettings settings) => Save(settings, SettingsPath);

    internal static void Save(SmoLVLcreatorSettings settings, string path)
    {
        ArgumentNullException.ThrowIfNull(settings);
        string directory = Path.GetDirectoryName(path)!;
        Directory.CreateDirectory(directory);
        string temporary = path + $".{Guid.NewGuid():N}.tmp";
        try
        {
            File.WriteAllText(
                temporary,
                JsonSerializer.Serialize(settings, JsonOptions));
            File.Move(temporary, path, overwrite: true);
        }
        finally
        {
            if (File.Exists(temporary))
                File.Delete(temporary);
        }
    }
}
