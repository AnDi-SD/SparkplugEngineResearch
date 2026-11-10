using System.Collections.ObjectModel;
using System.Security.Cryptography;
using WinxCoop.RuntimeProbe;

namespace WinxCoop.SessionRuntime;

// Immutable admission for one local runtime owner's bundle and case output.
// It does not create/configure/terminate a Job or grant a process-tree kill.
internal sealed class RuntimeProcessPolicy
{
    private readonly IReadOnlyDictionary<string, string> _imagePins;
    internal string BundleRoot { get; }
    internal string OutputRoot { get; }
    internal string JobName { get; }

    private RuntimeProcessPolicy(string bundleRoot, string outputRoot, string jobName,
        IReadOnlyDictionary<string, string> imagePins)
    { BundleRoot = bundleRoot; OutputRoot = outputRoot; JobName = jobName; _imagePins = imagePins; }

    internal static RuntimeProcessPolicy CreateE7(string bundleRoot, string outputRoot, string exactJobName,
        IReadOnlyDictionary<string, string> pinnedImageSha256)
    {
        ArgumentNullException.ThrowIfNull(pinnedImageSha256);
        string bundle = Absolute(bundleRoot), output = Absolute(outputRoot);
        OwnedChild.Require(Directory.Exists(bundle), "Runtime bundle root must already exist.");
        ValidateNoReparseAncestors(bundle); ValidateNoReparseAncestors(output);
        const string prefix = @"Local\WinxCoop.E7.";
        OwnedChild.Require(exactJobName.StartsWith(prefix, StringComparison.Ordinal) &&
            exactJobName.Length > prefix.Length && exactJobName.Length <= prefix.Length + 64 &&
            exactJobName[prefix.Length..].All(c => RuntimeFramework.IsAsciiLetterOrDigit(c) || c is '.' or '-' or '_'),
            "An exact bounded E7 native-parent Job name is required.");
        OwnedChild.Require(pinnedImageSha256.Count is > 0 and <= 1024, "A bounded explicit runtime image allowlist is required.");
        var copy = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        foreach (var pair in pinnedImageSha256)
        {
            string image = Absolute(pair.Key);
            OwnedChild.Require(Within(bundle, image) && !OwnedChild.SamePath(bundle, image) &&
                pair.Value is { Length: 64 } && pair.Value.All(Uri.IsHexDigit) &&
                copy.TryAdd(image, pair.Value.ToUpperInvariant()), "Runtime image pin is outside its bundle, duplicated or invalid.");
            ValidateNoReparseAncestors(image);
        }
        return new(bundle, output, exactJobName, new ReadOnlyDictionary<string, string>(copy));
    }

    internal string ValidateImage(string path)
    {
        string image = ValidateImagePath(path);
        using var file = OpenPinnedImage(image);
        return image;
    }

    internal FileStream OpenPinnedImage(string path)
    {
        string image = ValidateImagePath(path);
        var file = new FileStream(image, FileMode.Open, FileAccess.Read, FileShare.Read);
        try
        {
            OwnedChild.Require(Convert.ToHexString(RuntimeFramework.HashSha256(file)).Equals(_imagePins[image], StringComparison.OrdinalIgnoreCase),
                "Runtime child image differs from its explicit SHA256 pin.");
            file.Position = 0;
            return file;
        }
        catch { file.Dispose(); throw; }
    }

    // Identity verification after launch keeps its admitted image path and
    // retained process handle. It does not repeatedly rehash loaded executables
    // on the runtime pump; the exact bytes are pinned before Process.Start.
    internal string ValidateImagePath(string path)
    {
        string image = Absolute(path);
        OwnedChild.Require(Within(BundleRoot, image) && _imagePins.ContainsKey(image),
            "Runtime child image is not explicitly pinned in this bundle.");
        ValidateNoReparseAncestors(image);
        OwnedChild.Require(File.Exists(image) && (File.GetAttributes(image) & FileAttributes.Directory) == 0,
            "Pinned runtime child image is not a regular file.");
        return image;
    }

    internal string ValidateOutput(string path)
    {
        string output = Absolute(path);
        OwnedChild.Require(Within(OutputRoot, output), "Runtime child output is outside its owned case root.");
        ValidateNoReparseAncestors(output);
        return output;
    }

    private static string Absolute(string path)
    {
        OwnedChild.Require(Path.IsPathFullyQualified(path), "Runtime bundle/output/image path must be absolute.");
        return Path.TrimEndingDirectorySeparator(Path.GetFullPath(path));
    }

    private static bool Within(string root, string path)
    {
        if (OwnedChild.SamePath(root, path)) return true;
        string prefix = Path.EndsInDirectorySeparator(root) ? root : root + Path.DirectorySeparatorChar;
        return path.StartsWith(prefix, StringComparison.OrdinalIgnoreCase);
    }

    private static void ValidateNoReparseAncestors(string path)
    {
        string? current = path;
        while (!string.IsNullOrEmpty(current))
        {
            if (File.Exists(current) || Directory.Exists(current))
                OwnedChild.Require((File.GetAttributes(current) & FileAttributes.ReparsePoint) == 0,
                    "Runtime admission excludes reparse files/directories in its path.");
            current = Path.GetDirectoryName(current);
        }
    }
}
