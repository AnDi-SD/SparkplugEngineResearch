using Microsoft.Win32;
using System.Runtime.Versioning;

namespace SmoNativeValidator.Core;

public enum WinxClubLocationSource
{
    Preferred,
    EnvironmentVariable,
    SavedManualSelection,
    RegistryWorkingDirectory,
    RegistryMediaPath,
    AdditionalDirectory,
    CommonInstallDirectory
}

public sealed record WinxClubLocationCandidate(
    string Path,
    WinxClubLocationSource Source,
    bool Exists);

public sealed record WinxClubLocatorOptions
{
    public string? PreferredExecutablePath { get; init; }
    public string? SavedExecutablePath { get; init; }
    public IReadOnlyList<string> AdditionalSearchDirectories { get; init; } = [];
    public bool IncludeEnvironmentVariable { get; init; } = true;
    public bool IncludeRegistry { get; init; } = true;
    public bool IncludeCommonDirectories { get; init; } = true;
}

public sealed record WinxClubLocationResult
{
    public string? SelectedPath { get; init; }
    public IReadOnlyList<WinxClubLocationCandidate> Candidates { get; init; } = [];
    public IReadOnlyList<string> Diagnostics { get; init; } = [];
    public bool Found => SelectedPath is not null;
}

public static class WinxClubLocator
{
    private const string ExecutableName = "WinxClub.exe";
    private const string RegistryPath = @"Software\Konami\Winx Club";

    public static WinxClubLocationResult Locate(WinxClubLocatorOptions? options = null)
    {
        options ??= new WinxClubLocatorOptions();
        List<(string? Path, WinxClubLocationSource Source)> raw = [];
        List<string> diagnostics = [];

        raw.Add((options.PreferredExecutablePath, WinxClubLocationSource.Preferred));
        if (options.IncludeEnvironmentVariable)
        {
            raw.Add((Environment.GetEnvironmentVariable("WINXCLUB_EXE"),
                WinxClubLocationSource.EnvironmentVariable));
        }

        raw.Add((options.SavedExecutablePath, WinxClubLocationSource.SavedManualSelection));

        if (options.IncludeRegistry && OperatingSystem.IsWindows())
        {
            try
            {
                foreach ((string path, WinxClubLocationSource source) in ReadRegistryCandidates())
                    raw.Add((path, source));
            }
            catch (Exception exception) when (
                exception is UnauthorizedAccessException or IOException or System.Security.SecurityException)
            {
                diagnostics.Add($"32-bit registry lookup failed: {exception.Message}");
            }
        }

        foreach (string directory in options.AdditionalSearchDirectories)
            raw.Add((FromDirectory(directory), WinxClubLocationSource.AdditionalDirectory));

        if (options.IncludeCommonDirectories)
        {
            foreach (string directory in GetCommonDirectories())
                raw.Add((FromDirectory(directory), WinxClubLocationSource.CommonInstallDirectory));
        }

        return ResolveCandidates(raw, diagnostics);
    }

    public static WinxClubLocationResult ResolveCandidates(
        IEnumerable<(string? Path, WinxClubLocationSource Source)> candidates,
        IEnumerable<string>? diagnostics = null,
        Func<string, bool>? fileExists = null)
    {
        ArgumentNullException.ThrowIfNull(candidates);
        fileExists ??= File.Exists;
        HashSet<string> seen = new(StringComparer.OrdinalIgnoreCase);
        List<WinxClubLocationCandidate> resolved = [];

        foreach ((string? candidatePath, WinxClubLocationSource source) in candidates)
        {
            if (string.IsNullOrWhiteSpace(candidatePath))
                continue;

            string path;
            try
            {
                path = Path.GetFullPath(candidatePath.Trim().Trim('"'));
                if (Directory.Exists(path))
                    path = Path.Combine(path, ExecutableName);
            }
            catch (Exception exception) when (exception is ArgumentException or NotSupportedException)
            {
                continue;
            }

            if (!seen.Add(path))
                continue;

            resolved.Add(new WinxClubLocationCandidate(path, source, fileExists(path)));
        }

        return new WinxClubLocationResult
        {
            SelectedPath = resolved.FirstOrDefault(candidate => candidate.Exists)?.Path,
            Candidates = resolved,
            Diagnostics = diagnostics?.ToArray() ?? []
        };
    }

    [SupportedOSPlatform("windows")]
    private static IEnumerable<(string Path, WinxClubLocationSource Source)> ReadRegistryCandidates()
    {
        using RegistryKey baseKey = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry32);
        using RegistryKey? key = baseKey.OpenSubKey(RegistryPath, writable: false);
        if (key is null)
            yield break;

        string? workingDirectory = key.GetValue("Working Directory") as string;
        if (!string.IsNullOrWhiteSpace(workingDirectory))
            yield return (FromDirectory(workingDirectory), WinxClubLocationSource.RegistryWorkingDirectory);

        string? mediaPath = key.GetValue("MediaPath") as string;
        if (!string.IsNullOrWhiteSpace(mediaPath))
        {
            string trimmed = mediaPath.Trim().Trim('"').TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
            string? parent = Directory.GetParent(trimmed)?.FullName;
            if (parent is not null)
                yield return (FromDirectory(parent), WinxClubLocationSource.RegistryMediaPath);
            yield return (FromDirectory(trimmed), WinxClubLocationSource.RegistryMediaPath);
        }
    }

    private static IEnumerable<string> GetCommonDirectories()
    {
        string? programFilesX86 = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86);
        string? programFiles = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles);
        string[] roots = [programFilesX86, programFiles];
        string[] suffixes =
        [
            Path.Combine("Konami", "Winx Club"),
            "Winx Club",
            Path.Combine("Ubisoft", "Winx Club")
        ];

        foreach (string? root in roots.Distinct(StringComparer.OrdinalIgnoreCase))
        {
            if (string.IsNullOrWhiteSpace(root))
                continue;
            foreach (string suffix in suffixes)
                yield return Path.Combine(root, suffix);
        }
    }

    private static string FromDirectory(string directory)
    {
        string value = directory.Trim().Trim('"');
        return string.Equals(Path.GetFileName(value), ExecutableName, StringComparison.OrdinalIgnoreCase)
            ? value
            : Path.Combine(value, ExecutableName);
    }
}
