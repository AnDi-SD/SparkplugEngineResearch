using System.IO;
using System.Security;

namespace SmoImporter.Gui;

internal sealed record WizardResultAnimationEntry(string Path, string DisplayName, string SourceFolder);

internal sealed record WizardResultAnimationDiscovery(
    IReadOnlyList<WizardResultAnimationEntry> Entries,
    IReadOnlyList<string> Folders,
    bool NeedsManualDirectory,
    string Status);

/// <summary>Finds nearby character clips without loading them or reproducing the SAN sampler.</summary>
internal static class WizardResultAnimationCatalog
{
    public static WizardResultAnimationDiscovery Discover(string? sourceModelPath, bool isHumanoid,
        bool hasWings, string? gameExecutablePath, string? manualDirectory = null)
    {
        var entries = new Dictionary<string, WizardResultAnimationEntry>(StringComparer.OrdinalIgnoreCase);
        var visited = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        bool inaccessible = false;
        bool manual = !string.IsNullOrWhiteSpace(manualDirectory);

        void AddFolder(string? directory)
        {
            string? full = NormalizeDirectory(directory);
            if (full is null || !visited.Add(full) || !Directory.Exists(full))
                return;
            try
            {
                // These are character folders, not a game-wide or recursive disk scan.
                foreach (string path in Directory.EnumerateFiles(full, "*", SearchOption.TopDirectoryOnly)
                             .Where(path => Path.GetExtension(path).Equals(".san", StringComparison.OrdinalIgnoreCase)))
                {
                    string normalized = Path.GetFullPath(path);
                    string display = Path.GetFileNameWithoutExtension(normalized) + " · " + new DirectoryInfo(full).Name;
                    entries.TryAdd(normalized, new WizardResultAnimationEntry(normalized, display, full));
                }
            }
            catch (Exception error) when (IsFolderError(error))
            {
                inaccessible = true;
            }
        }

        string? sourceDirectory = ParentDirectory(sourceModelPath);
        if (manual)
            AddFolder(manualDirectory);
        else
        {
            AddFolder(sourceDirectory);
            if (entries.Count == 0 && isHumanoid)
                AddFolder(FindCharacterFolder("Bloom", sourceDirectory, ParentDirectory(gameExecutablePath)));
        }

        if (isHumanoid && hasWings)
            AddFolder(FindCharacterFolder("BloomX", sourceDirectory, ParentDirectory(gameExecutablePath)));

        WizardResultAnimationEntry[] ordered = entries.Values
            .OrderBy(entry => entry.DisplayName, StringComparer.OrdinalIgnoreCase)
            .ThenBy(entry => entry.Path, StringComparer.OrdinalIgnoreCase)
            .ToArray();
        string[] folders = ordered.Select(entry => entry.SourceFolder).Distinct(StringComparer.OrdinalIgnoreCase).ToArray();
        string status = ordered.Length > 0
            ? $"Найдено SAN-анимаций: {ordered.Length}." + (inaccessible ? " Часть папок недоступна." : "")
            : inaccessible ? "Не удалось открыть папку анимаций. Выберите другую папку."
            : manual ? "В выбранной папке нет SAN-анимаций. Выберите другую папку."
            : isHumanoid ? "Анимации рядом с моделью и в папке Bloom не найдены. Выберите папку."
            : "Рядом с моделью нет SAN-анимаций. Выберите папку.";
        return new WizardResultAnimationDiscovery(ordered, folders, ordered.Length == 0, status);
    }

    private static string? FindCharacterFolder(string character, string? sourceDirectory, string? gameDirectory)
    {
        var visited = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (string? start in new[] { gameDirectory, sourceDirectory })
        {
            string? normalized = NormalizeDirectory(start);
            if (normalized is null)
                continue;
            DirectoryInfo? cursor;
            try { cursor = new DirectoryInfo(normalized); }
            catch (Exception error) when (IsFolderError(error)) { continue; }
            while (cursor is not null)
            {
                string candidate = cursor.Name.Equals("Characters", StringComparison.OrdinalIgnoreCase)
                    ? Path.Combine(cursor.FullName, character)
                    : cursor.Name.Equals("Media", StringComparison.OrdinalIgnoreCase)
                        ? Path.Combine(cursor.FullName, "Characters", character)
                        : Path.Combine(cursor.FullName, "Media", "Characters", character);
                if (visited.Add(candidate) && HasClips(candidate))
                    return candidate;
                try { cursor = cursor.Parent; }
                catch (Exception error) when (IsFolderError(error)) { break; }
            }
        }
        return null;
    }

    private static bool HasClips(string directory)
    {
        try
        {
            return Directory.Exists(directory) && Directory.EnumerateFiles(directory, "*", SearchOption.TopDirectoryOnly)
                .Any(path => Path.GetExtension(path).Equals(".san", StringComparison.OrdinalIgnoreCase));
        }
        catch (Exception error) when (IsFolderError(error)) { return false; }
    }

    private static string? ParentDirectory(string? file)
    {
        if (string.IsNullOrWhiteSpace(file))
            return null;
        try { return Path.GetDirectoryName(Path.GetFullPath(file)); }
        catch (Exception error) when (IsFolderError(error)) { return null; }
    }

    private static string? NormalizeDirectory(string? directory)
    {
        if (string.IsNullOrWhiteSpace(directory))
            return null;
        try { return Path.TrimEndingDirectorySeparator(Path.GetFullPath(directory)); }
        catch (Exception error) when (IsFolderError(error)) { return null; }
    }

    private static bool IsFolderError(Exception error) => error is IOException or UnauthorizedAccessException or
        SecurityException or ArgumentException or NotSupportedException;
}
