namespace SmoNativeValidator.Core;

public sealed class StagedAssetSession : IDisposable
{
    private static readonly string[] SidecarExtensions = [".stx", ".spt"];
    private bool _disposed;

    private StagedAssetSession(string sessionDirectory, string assetPath, IReadOnlyList<string> stagedFiles)
    {
        SessionDirectory = sessionDirectory;
        AssetPath = assetPath;
        StagedFiles = stagedFiles;
    }

    public string SessionDirectory { get; }
    public string AssetPath { get; }
    public IReadOnlyList<string> StagedFiles { get; }

    public static StagedAssetSession Create(string sourceAssetPath, string logicalGameAssetPath)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(sourceAssetPath);
        ArgumentException.ThrowIfNullOrWhiteSpace(logicalGameAssetPath);
        string source = Path.GetFullPath(sourceAssetPath);
        if (!File.Exists(source))
            throw new FileNotFoundException("The selected SMO does not exist.", source);
        if (!string.Equals(Path.GetExtension(source), ".smo", StringComparison.OrdinalIgnoreCase))
            throw new ArgumentException("The selected asset must have the .smo extension.", nameof(sourceAssetPath));

        string logicalFileName = GetLogicalFileName(logicalGameAssetPath);
        if (!string.Equals(Path.GetExtension(logicalFileName), ".smo", StringComparison.OrdinalIgnoreCase))
            throw new ArgumentException("The logical game asset path must end in .smo.", nameof(logicalGameAssetPath));

        string root = GetStagingRoot();
        Directory.CreateDirectory(root);
        string directory = Path.Combine(root, $"s-{Guid.NewGuid():N}"[..14]);
        Directory.CreateDirectory(directory);
        List<string> copied = [];

        try
        {
            string destination = Path.Combine(directory, logicalFileName);
            File.Copy(source, destination, overwrite: false);
            copied.Add(destination);

            string sourceWithoutExtension = Path.Combine(
                Path.GetDirectoryName(source) ?? string.Empty,
                Path.GetFileNameWithoutExtension(source));
            string destinationWithoutExtension = Path.Combine(
                directory,
                Path.GetFileNameWithoutExtension(logicalFileName));
            foreach (string extension in SidecarExtensions)
            {
                string sidecar = sourceWithoutExtension + extension;
                if (!File.Exists(sidecar))
                    continue;
                string sidecarDestination = destinationWithoutExtension + extension;
                File.Copy(sidecar, sidecarDestination, overwrite: false);
                copied.Add(sidecarDestination);
            }

            if (!IsAscii(destination))
            {
                throw new NativeAssetPathException(
                    "The staging path is not ASCII. Configure TEMP to an ASCII path for the ANSI game runtime.");
            }

            return new StagedAssetSession(directory, destination, copied);
        }
        catch
        {
            TryDeleteOwnedDirectory(directory, root);
            throw;
        }
    }

    public static bool IsAscii(string value) => value.All(character => character <= 0x7F);

    public void Dispose()
    {
        if (_disposed)
            return;
        _disposed = true;
        TryDeleteOwnedDirectory(SessionDirectory, GetStagingRoot());
    }

    private static string GetLogicalFileName(string logicalGameAssetPath)
    {
        string normalized = logicalGameAssetPath.Replace('/', '\\').TrimEnd('\\');
        int separator = normalized.LastIndexOf('\\');
        string name = separator >= 0 ? normalized[(separator + 1)..] : normalized;
        if (string.IsNullOrWhiteSpace(name) || name.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0)
            throw new ArgumentException("The logical asset path has an invalid file name.", nameof(logicalGameAssetPath));
        return name;
    }

    private static string GetStagingRoot() =>
        Path.GetFullPath(Path.Combine(Path.GetTempPath(), "SmoNV"));

    private static void TryDeleteOwnedDirectory(string directory, string expectedRoot)
    {
        try
        {
            string resolvedDirectory = Path.GetFullPath(directory).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
            string resolvedRoot = Path.GetFullPath(expectedRoot).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
            if (resolvedDirectory.StartsWith(resolvedRoot, StringComparison.OrdinalIgnoreCase) &&
                Directory.Exists(directory))
            {
                NormalizeOwnedAttributes(directory);
                Directory.Delete(directory, recursive: true);
            }
        }
        catch (IOException)
        {
            // The owned process may still be releasing a sidecar. Cleanup is best-effort.
        }
        catch (UnauthorizedAccessException)
        {
            // Cleanup failure is surfaced by the validator as a diagnostic event.
        }
    }

    private static void NormalizeOwnedAttributes(string directory)
    {
        foreach (string file in Directory.EnumerateFiles(
                     directory,
                     "*",
                     SearchOption.AllDirectories))
        {
            ClearRestrictiveAttributes(file);
        }

        foreach (string childDirectory in Directory.EnumerateDirectories(
                     directory,
                     "*",
                     SearchOption.AllDirectories)
                 .OrderByDescending(path => path.Length))
        {
            ClearRestrictiveAttributes(childDirectory);
        }
        ClearRestrictiveAttributes(directory);
    }

    private static void ClearRestrictiveAttributes(string path)
    {
        FileAttributes attributes = File.GetAttributes(path);
        FileAttributes normalized = attributes &
            ~(FileAttributes.ReadOnly | FileAttributes.Hidden | FileAttributes.System);
        File.SetAttributes(path, normalized == 0 ? FileAttributes.Normal : normalized);
    }
}
