using System.Text;

namespace SmoNativeValidator.Core;

internal sealed class NativeLaunchWorkspace : IDisposable
{
    private static readonly EnumerationOptions RecursiveFiles = new()
    {
        RecurseSubdirectories = true,
        IgnoreInaccessible = false,
        AttributesToSkip = FileAttributes.ReparsePoint
    };
    private bool _disposed;

    private NativeLaunchWorkspace(
        string sessionDirectory,
        string shaderSourceDirectory,
        IReadOnlyList<string> copiedShaderFiles,
        int? startLevel)
    {
        SessionDirectory = sessionDirectory;
        ShaderSourceDirectory = shaderSourceDirectory;
        CopiedShaderFiles = copiedShaderFiles;
        StartLevel = startLevel;
    }

    internal string SessionDirectory { get; }
    internal string WorkingDirectory => SessionDirectory;
    internal string ShaderSourceDirectory { get; }
    internal IReadOnlyList<string> CopiedShaderFiles { get; }
    internal int? StartLevel { get; }
    internal string WinxIniPath => Path.Combine(SessionDirectory, "winx.ini");
    internal string ConfigIniPath => Path.Combine(SessionDirectory, "config.ini");

    internal static NativeLaunchWorkspace Create(
        string executablePath,
        string? shaderSearchDirectory,
        int? startLevel)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(executablePath);
        string executable = Path.GetFullPath(executablePath);
        if (!File.Exists(executable))
            throw new FileNotFoundException("WinxClub.exe was not found.", executable);
        if (startLevel == 0)
            startLevel = null;
        if (startLevel is int levelId && !WinxClubLevelCatalog.CanStart(levelId))
        {
            throw new ArgumentOutOfRangeException(
                nameof(startLevel),
                levelId,
                WinxClubLevelCatalog.GetWarning(levelId));
        }

        string shaderSource = ResolveShaderSource(executable, shaderSearchDirectory);
        string root = GetWorkspaceRoot();
        Directory.CreateDirectory(root);
        string directory = Path.Combine(root, $"w-{Guid.NewGuid():N}"[..14]);
        Directory.CreateDirectory(directory);

        try
        {
            if (!StagedAssetSession.IsAscii(directory))
            {
                throw new NativeAssetPathException(
                    "The isolated launch path is not ASCII. Configure TEMP to an ASCII path for the ANSI game runtime.");
            }

            WriteConfiguration(directory, startLevel);
            IReadOnlyList<string> copiedShaders = CopyShaders(
                shaderSource,
                Path.Combine(directory, "Shaders"));
            return new NativeLaunchWorkspace(
                directory,
                shaderSource,
                copiedShaders,
                startLevel);
        }
        catch
        {
            TryDeleteOwnedDirectory(directory, root);
            throw;
        }
    }

    public void Dispose()
    {
        if (_disposed)
            return;
        _disposed = true;
        TryDeleteOwnedDirectory(SessionDirectory, GetWorkspaceRoot());
    }

    private static string ResolveShaderSource(string executable, string? searchDirectory)
    {
        List<string> candidates = [];
        AddCandidate(candidates, Path.GetDirectoryName(executable));
        AddCandidate(candidates, searchDirectory);

        WinxClubLocationResult located = WinxClubLocator.Locate(new WinxClubLocatorOptions
        {
            PreferredExecutablePath = executable,
            AdditionalSearchDirectories = string.IsNullOrWhiteSpace(searchDirectory)
                ? []
                : [searchDirectory]
        });
        foreach (WinxClubLocationCandidate candidate in located.Candidates)
            AddCandidate(candidates, Path.GetDirectoryName(candidate.Path));

        foreach (string candidate in candidates.Distinct(StringComparer.OrdinalIgnoreCase))
        {
            string shaders = Path.Combine(candidate, "Shaders");
            if (Directory.Exists(shaders) && Directory.EnumerateFiles(
                    shaders, "*", RecursiveFiles).Any())
            {
                return Path.GetFullPath(shaders);
            }
        }

        throw new DirectoryNotFoundException(
            "The game Shaders directory was not found next to WinxClub.exe, in the configured working directory, or through the Winx Club locator.");
    }

    private static void AddCandidate(List<string> candidates, string? directory)
    {
        if (string.IsNullOrWhiteSpace(directory))
            return;
        try
        {
            candidates.Add(Path.GetFullPath(directory.Trim().Trim('"')));
        }
        catch (Exception exception) when (exception is ArgumentException or NotSupportedException)
        {
            // Another candidate (including registry-backed game locations) may be usable.
        }
    }

    private static void WriteConfiguration(string directory, int? startLevel)
    {
        List<string> winxLines =
        [
            "showCinematics=false",
            "useGamePad=false",
            "fullScreen=false",
            "enableSound=false",
            "enableShadows=false",
            "language=en"
        ];
        if (startLevel is int levelId)
            winxLines.Add($"startLevel={levelId}");
        winxLines.Add("testCinematic=false");
        winxLines.Add("cinematicToTest=0");

        Encoding encoding = new UTF8Encoding(encoderShouldEmitUTF8Identifier: false);
        File.WriteAllText(
            Path.Combine(directory, "winx.ini"),
            string.Join("\r\n", winxLines) + "\r\n",
            encoding);
        File.WriteAllText(
            Path.Combine(directory, "config.ini"),
            "[Input]\r\nmouse_exclusive = false\r\n",
            encoding);
    }

    private static IReadOnlyList<string> CopyShaders(string source, string destination)
    {
        Directory.CreateDirectory(destination);
        List<string> copied = [];
        foreach (string sourceFile in Directory.EnumerateFiles(
                     source, "*", RecursiveFiles))
        {
            string relative = Path.GetRelativePath(source, sourceFile);
            if (relative.StartsWith(".." + Path.DirectorySeparatorChar, StringComparison.Ordinal) ||
                Path.IsPathFullyQualified(relative))
            {
                throw new IOException($"Shader path escaped its source directory: {sourceFile}");
            }

            string destinationFile = Path.Combine(destination, relative);
            string? destinationDirectory = Path.GetDirectoryName(destinationFile);
            if (destinationDirectory is not null)
                Directory.CreateDirectory(destinationDirectory);
            File.Copy(sourceFile, destinationFile, overwrite: false);
            copied.Add(destinationFile);
        }

        if (copied.Count == 0)
            throw new DirectoryNotFoundException($"The Shaders directory is empty: {source}");
        return copied.AsReadOnly();
    }

    private static string GetWorkspaceRoot() =>
        Path.GetFullPath(Path.Combine(Path.GetTempPath(), "SmoNV"));

    private static void TryDeleteOwnedDirectory(string directory, string expectedRoot)
    {
        try
        {
            string resolvedDirectory = Path.GetFullPath(directory)
                .TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
            string resolvedRoot = Path.GetFullPath(expectedRoot)
                .TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
            if (!resolvedDirectory.StartsWith(resolvedRoot, StringComparison.OrdinalIgnoreCase) ||
                !Directory.Exists(directory))
            {
                return;
            }

            foreach (string file in Directory.EnumerateFiles(
                         directory, "*", SearchOption.AllDirectories))
            {
                File.SetAttributes(file, FileAttributes.Normal);
            }
            Directory.Delete(directory, recursive: true);
        }
        catch (IOException)
        {
            // Cleanup is best-effort and its result is surfaced by the validator.
        }
        catch (UnauthorizedAccessException)
        {
            // Cleanup is best-effort and its result is surfaced by the validator.
        }
    }
}
