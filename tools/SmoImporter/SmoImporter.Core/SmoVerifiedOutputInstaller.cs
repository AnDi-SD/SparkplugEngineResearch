using System.Security.Cryptography;

namespace SmoImporter.Core;

internal sealed record SmoVerifiedOutputInstallResult(
    string OutputPath,
    string Sha256,
    string? BackupPath);

/// <summary>
/// Installs an already serialized SMO only after a same-directory temporary
/// file has passed the writer-specific verifier. Existing outputs are replaced
/// atomically and retained as timestamped backups.
/// </summary>
internal static class SmoVerifiedOutputInstaller
{
    public static SmoVerifiedOutputInstallResult Install(
        string outputPath,
        ReadOnlySpan<byte> data,
        Action<string> verifyTemporary,
        params string?[] protectedInputPaths) =>
        Install(
            outputPath,
            data,
            verifyTemporary,
            CancellationToken.None,
            protectedInputPaths);

    public static SmoVerifiedOutputInstallResult Install(
        string outputPath,
        ReadOnlySpan<byte> data,
        Action<string> verifyTemporary,
        CancellationToken cancellationToken,
        params string?[] protectedInputPaths)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(outputPath);
        ArgumentNullException.ThrowIfNull(verifyTemporary);
        cancellationToken.ThrowIfCancellationRequested();

        string output = Path.GetFullPath(outputPath);
        foreach (string? inputPath in protectedInputPaths)
        {
            if (string.IsNullOrWhiteSpace(inputPath))
                continue;
            if (Path.GetFullPath(inputPath).Equals(
                    output,
                    StringComparison.OrdinalIgnoreCase))
            {
                throw new InvalidOperationException(
                    "The output path must differ from every source/donor path.");
            }
        }

        string? directory = Path.GetDirectoryName(output);
        if (string.IsNullOrWhiteSpace(directory))
            throw new InvalidOperationException("Could not resolve the output directory.");
        Directory.CreateDirectory(directory);

        string temporary = Path.Combine(
            directory,
            $".{Path.GetFileName(output)}.{Guid.NewGuid():N}.tmp");
        try
        {
            cancellationToken.ThrowIfCancellationRequested();
            using (var stream = new FileStream(
                       temporary,
                       FileMode.CreateNew,
                       FileAccess.Write,
                       FileShare.None,
                       1024 * 1024,
                       FileOptions.SequentialScan))
            {
                stream.Write(data);
                stream.Flush(flushToDisk: true);
            }

            cancellationToken.ThrowIfCancellationRequested();
            verifyTemporary(temporary);
            cancellationToken.ThrowIfCancellationRequested();
            string expectedHash;
            using (var stream = new FileStream(
                       temporary,
                       FileMode.Open,
                       FileAccess.Read,
                       FileShare.Read,
                       1024 * 1024,
                       FileOptions.SequentialScan))
            {
                expectedHash = Convert.ToHexString(SHA256.HashData(stream));
            }

            // This is the final cancellation boundary: after it, the verified
            // temporary file is installed atomically. A cancellation request
            // can therefore never expose partially serialized output.
            cancellationToken.ThrowIfCancellationRequested();
            string? backupPath = null;
            if (File.Exists(output))
            {
                backupPath = output +
                    $".{DateTime.Now:yyyyMMdd-HHmmss-fff}.bak";
                File.Replace(
                    temporary,
                    output,
                    backupPath,
                    ignoreMetadataErrors: true);
            }
            else
            {
                File.Move(temporary, output);
            }

            string installedHash;
            using (var stream = new FileStream(
                       output,
                       FileMode.Open,
                       FileAccess.Read,
                       FileShare.Read,
                       1024 * 1024,
                       FileOptions.SequentialScan))
            {
                installedHash = Convert.ToHexString(SHA256.HashData(stream));
            }
            if (!installedHash.Equals(expectedHash, StringComparison.OrdinalIgnoreCase))
            {
                throw new InvalidDataException(
                    "Installed SMO differs from the verified temporary file.");
            }

            return new SmoVerifiedOutputInstallResult(
                output,
                installedHash,
                backupPath);
        }
        finally
        {
            if (File.Exists(temporary))
                File.Delete(temporary);
        }
    }
}
