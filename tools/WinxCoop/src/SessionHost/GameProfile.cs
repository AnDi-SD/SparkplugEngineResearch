using System.Security.Cryptography;
using System.Buffers;
using System.Text;

namespace WinxCoop.SessionHost;

internal sealed class GameProfile : IDisposable
{
    internal const string OriginalExecutableSha256 = "3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F";
    private readonly FileStream _executablePin;
    internal string GameRoot { get; }
    internal string Executable => Path.Combine(GameRoot, "WinxClub.exe");
    internal string Media => Path.Combine(GameRoot, "Media");
    internal byte[] CompatibilityDigest { get; }
    internal string MediaDigest { get; }
    internal ulong FilesHashed { get; }
    internal ulong BytesHashed { get; }

    internal GameProfile(SessionConfiguration config, RuntimeBundle bundle, Action<string>? progress = null, Action? checkpoint = null)
    {
        checkpoint?.Invoke();
        GameRoot = new GameSelection(config.GameRoot, config.InitialLevel).GameRoot;
        _executablePin = new FileStream(Executable, FileMode.Open, FileAccess.Read, FileShare.Read);
        try
        {
            Require(Hash(_executablePin, checkpoint: checkpoint) == OriginalExecutableSha256, "Unsupported original WinxClub.exe.");
            _executablePin.Position = 0;
            progress?.Invoke("Проверяем игровые ресурсы…");
            (MediaDigest, FilesHashed, BytesHashed) = Fingerprint(GameRoot, progress, checkpoint);
            // Cross-architecture builds intentionally have different local
            // binary hashes. The common source/profile digest and original
            // game content define peer compatibility; each local deployment
            // is independently pinned by RuntimeBundle.
            using var signature = SHA256.Create();
            byte[] domain = Encoding.ASCII.GetBytes(config.Campaign
                ? "WinxCoop.CampaignPrototype.Gardenia1.v1\n" : "WinxCoop.E7.SharedPlayer.Alfea28.v1\n");
            byte[] source = Convert.FromHexString(bundle.Manifest.SourceProfileSha256);
            byte[] media = Convert.FromHexString(MediaDigest);
            byte[] executable = Convert.FromHexString(OriginalExecutableSha256);
            byte[] input = new byte[domain.Length + source.Length + media.Length + executable.Length];
            domain.CopyTo(input, 0); source.CopyTo(input, domain.Length);
            media.CopyTo(input, domain.Length + source.Length);
            executable.CopyTo(input, domain.Length + source.Length + media.Length);
            CompatibilityDigest = signature.ComputeHash(input);
        }
        catch { _executablePin.Dispose(); throw; }
    }

    private static (string Digest, ulong Count, ulong Bytes) Fingerprint(string gameRoot, Action<string>? progress, Action? checkpoint)
    {
        var files = new List<(string Relative, string Path)>();
        long lastProgress = Environment.TickCount64;
        void Report(string message, bool force = false)
        {
            long now = Environment.TickCount64;
            if (progress is null || !force && now - lastProgress < 250) return;
            lastProgress = now; progress(message);
        }
        foreach (string tree in new[] { "Media", "Shaders" })
        {
            string root = Path.Combine(gameRoot, tree); RuntimeBundle.Ordinary(root, true);
            var pending = new Stack<(string Directory, int Depth)>(); pending.Push((root, 0));
            while (pending.Count != 0)
            {
                checkpoint?.Invoke();
                var current = pending.Pop(); Require(current.Depth <= 64, "Game resource tree is too deep.");
                foreach (string path in Directory.EnumerateFileSystemEntries(current.Directory))
                {
                    checkpoint?.Invoke();
                    FileAttributes attributes = System.IO.File.GetAttributes(path);
                    Require((attributes & FileAttributes.ReparsePoint) == 0, "Game resources contain a reparse point.");
                    string name = Path.GetFileName(path);
                    if ((attributes & FileAttributes.Directory) != 0)
                    {
                        if (name.Equals("Saved", StringComparison.OrdinalIgnoreCase) || name.Equals("Logs", StringComparison.OrdinalIgnoreCase)) continue;
                        pending.Push((path, current.Depth + 1)); continue;
                    }
                    string extension = Path.GetExtension(path);
                    if (extension.Equals(".log", StringComparison.OrdinalIgnoreCase) || extension.Equals(".tmp", StringComparison.OrdinalIgnoreCase) ||
                        extension.Equals(".dmp", StringComparison.OrdinalIgnoreCase)) continue;
                    Require(files.Count < 200000, "Too many game resource files.");
                    string relative = Path.GetRelativePath(gameRoot, path).Replace('\\', '/').ToLowerInvariant();
                    Require(relative.All(c => c is >= ' ' and <= '~'), "Prepared resource names must be ASCII.");
                    files.Add((relative, path));
                    Report($"Ищем ресурсы: {files.Count}");
                }
            }
        }
        Require(files.Count != 0, "Game resources are empty.");
        files.Sort((a, b) => StringComparer.Ordinal.Compare(a.Relative, b.Relative));
        using var digest = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
        digest.AppendData(Encoding.ASCII.GetBytes("WinxCoop.GameResources.v1\n"));
        ulong total = 0; string previous = "";
        for (int index = 0; index < files.Count; ++index)
        {
            checkpoint?.Invoke();
            var row = files[index]; Require(row.Relative != previous, "Case-normalized game resource names alias."); previous = row.Relative;
            using var stream = new FileStream(row.Path, FileMode.Open, FileAccess.Read, FileShare.Read,
                bufferSize: 4096, FileOptions.SequentialScan);
            ulong bytes = checked((ulong)stream.Length); total = checked(total + bytes);
            string hash = Hash(stream, read => Report($"Проверяем ресурсы: {index}/{files.Count} ({(total - bytes + read) / (1024 * 1024)} МиБ)"), checkpoint);
            digest.AppendData(Encoding.ASCII.GetBytes(row.Relative + "\0" + bytes.ToString(System.Globalization.CultureInfo.InvariantCulture) + "\0" + hash + "\n"));
            Report($"Проверяем ресурсы: {index + 1}/{files.Count}", force: index + 1 == files.Count);
        }
        return (Convert.ToHexString(digest.GetHashAndReset()), (ulong)files.Count, total);
    }
    internal static string Hash(Stream stream, Action<ulong>? progress = null, Action? checkpoint = null)
    {
        // Keep the same complete SHA256 without issuing one kernel read per
        // 4KiB. One pooled buffer is reused across the resource cohort.
        byte[] buffer = ArrayPool<byte>.Shared.Rent(1024 * 1024);
        try
        {
            using var sha = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
            ulong read = 0;
            while (true)
            {
                checkpoint?.Invoke();
                int count = stream.Read(buffer, 0, buffer.Length);
                checkpoint?.Invoke();
                if (count == 0) break;
                sha.AppendData(buffer, 0, count); read = checked(read + (ulong)count); progress?.Invoke(read);
            }
            return Convert.ToHexString(sha.GetHashAndReset());
        }
        finally { ArrayPool<byte>.Shared.Return(buffer); }
    }
    private static void Require(bool value, string message) => SessionConfiguration.Require(value, message);
    public void Dispose() => _executablePin.Dispose();
}
