using SmoLVLcreator.Core;
using System.IO;
using System.Text.Json;

namespace SmoLVLcreator.Gui;

public sealed record SmoLVLcreatorSettings(
    string ExportDirectory,
    SmoLevelExportFormat ExportFormat)
{
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

    public static SmoLVLcreatorSettings Load()
    {
        try
        {
            if (!File.Exists(SettingsPath))
                return SmoLVLcreatorSettings.Default;
            SmoLVLcreatorSettings? settings = JsonSerializer.Deserialize<
                SmoLVLcreatorSettings>(File.ReadAllText(SettingsPath), JsonOptions);
            return settings is null || string.IsNullOrWhiteSpace(settings.ExportDirectory)
                ? SmoLVLcreatorSettings.Default
                : settings with
                {
                    ExportDirectory = Path.GetFullPath(settings.ExportDirectory)
                };
        }
        catch
        {
            return SmoLVLcreatorSettings.Default;
        }
    }

    public static void Save(SmoLVLcreatorSettings settings)
    {
        ArgumentNullException.ThrowIfNull(settings);
        string path = SettingsPath;
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
