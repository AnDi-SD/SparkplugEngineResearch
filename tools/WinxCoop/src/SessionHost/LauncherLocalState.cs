using System.Net;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using WinxCoop.RuntimeProbe;

namespace WinxCoop.SessionHost;

internal sealed record LauncherPreferences
{
    public uint Version { get; init; } = 1;
    public string GameRoot { get; init; } = "";
    public string ServerAddress { get; init; } = "127.0.0.1";
    public ushort Port { get; init; } = 27045;
    public string Profile { get; init; } = CampaignSceneProfile.Name;
    public int Seconds { get; init; } = InteractiveSessionLimits.DefaultSeconds;
}

// UI preferences and diagnostics have a per-user local owner, independent of
// the read-only distribution folder and of each retained native session case.
internal sealed class LauncherLocalState : IDisposable
{
    private const long MaximumLogBytes = 16 * 1024 * 1024;
    private readonly Mutex _logMutex;
    private readonly string _settingsPath;
    internal string ProgramName { get; } = Environment.Is64BitProcess ? "Windows-x64" : "Win7-x86";
    internal string RootDirectory { get; }
    internal string CasesDirectory { get; }
    internal string LogPath { get; }

    internal LauncherLocalState()
    {
        string appData = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        SessionConfiguration.Require(!string.IsNullOrWhiteSpace(appData), "Local application data is unavailable.");
        RootDirectory = Path.Combine(appData, "WinxCoop", ProgramName);
        CasesDirectory = Path.Combine(RootDirectory, "Sessions");
        EnsureLocalDirectory(RootDirectory); EnsureLocalDirectory(CasesDirectory);
        LogPath = Path.Combine(RootDirectory, "launcher.log");
        _settingsPath = Path.Combine(RootDirectory, "launcher-settings.json");
        string key = Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(RootDirectory.ToUpperInvariant())))[..24];
        _logMutex = new Mutex(false, @"Local\WinxCoop.LauncherLog." + key);
    }

    internal static void EnsureLocalDirectory(string path)
    {
        string full = Path.GetFullPath(path);
        string root = Path.GetPathRoot(full) ?? throw new InvalidDataException("Local data requires an absolute drive path.");
        SessionConfiguration.Require(!full.StartsWith(@"\\", StringComparison.Ordinal) &&
            new DriveInfo(root).DriveType is not (DriveType.Network or DriveType.Unknown or DriveType.NoRootDirectory),
            "Session data and logs must use a writable local drive, not the program share.");
        for (string? current = full; !string.IsNullOrEmpty(current); current = Path.GetDirectoryName(current))
            if (File.Exists(current) || Directory.Exists(current))
                SessionConfiguration.Require((File.GetAttributes(current) & FileAttributes.ReparsePoint) == 0,
                    "Local session data excludes reparse files/directories.");
        Directory.CreateDirectory(full); RuntimeBundle.Ordinary(full, true);
    }

    internal void Write(string name, object? data = null)
    {
        byte[] bytes = Encoding.UTF8.GetBytes(JsonSerializer.Serialize(new
        {
            utc = DateTime.UtcNow, pid = Environment.ProcessId, program = ProgramName, @event = name, data
        }) + "\n");
        SessionConfiguration.Require(bytes.Length <= 65536, "Launcher log record exceeds its bounded size.");
        bool acquired = false;
        try
        {
            try { acquired = _logMutex.WaitOne(2000); }
            catch (AbandonedMutexException) { acquired = true; }
            SessionConfiguration.Require(acquired, "Local launcher log is busy.");
            if (File.Exists(LogPath))
            {
                RuntimeBundle.Ordinary(LogPath, false);
                if (new FileInfo(LogPath).Length > MaximumLogBytes - bytes.Length)
                    File.Move(LogPath, Path.Combine(RootDirectory, "launcher-" + DateTime.UtcNow.ToString("yyyyMMdd-HHmmss") + "-" + Guid.NewGuid().ToString("N") + ".log"));
            }
            using var file = new FileStream(LogPath, FileMode.Append, FileAccess.Write, FileShare.ReadWrite | FileShare.Delete);
            file.Write(bytes, 0, bytes.Length); file.Flush();
        }
        finally { if (acquired) _logMutex.ReleaseMutex(); }
    }

    internal bool TryWrite(string name, object? data = null)
    {
        try { Write(name, data); return true; }
        catch (Exception) { return false; } // A diagnostic failure must never prevent UI Stop.
    }

    internal LauncherPreferences ReadPreferences()
    {
        try
        {
            if (!File.Exists(_settingsPath)) return new();
            RuntimeBundle.Ordinary(_settingsPath, false);
            using var file = new FileStream(_settingsPath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
            SessionConfiguration.Require(file.Length is > 0 and <= 16384, "Launcher settings exceed their size bound.");
            LauncherPreferences value = JsonSerializer.Deserialize<LauncherPreferences>(file) ?? throw new InvalidDataException("Launcher settings are empty.");
            SessionConfiguration.Require(value.Version == 1 && value.GameRoot.Length <= 180 && value.Port != 0 &&
                value.Profile is "campaign-prototype" or "alfea-28" &&
                InteractiveSessionLimits.SessionDurationAllowed(value.Seconds, value.Profile == CampaignSceneProfile.Name) &&
                IPAddress.TryParse(value.ServerAddress, out var address) &&
                address.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork && !address.Equals(IPAddress.Any),
                "Launcher settings contain unsupported values.");
            return value;
        }
        catch (Exception error)
        {
            TryWrite("settings-read-failed", new { error = error.ToString() }); return new();
        }
    }

    internal void SavePreferences(LauncherPreferences value)
    {
        try
        {
            string temporary = Path.Combine(RootDirectory, "launcher-settings-" + Guid.NewGuid().ToString("N") + ".tmp");
            using (var file = new FileStream(temporary, FileMode.CreateNew, FileAccess.Write, FileShare.Read))
            {
                byte[] bytes = JsonSerializer.SerializeToUtf8Bytes(value, SessionConfiguration.Json);
                file.Write(bytes, 0, bytes.Length); file.Flush();
            }
            if (File.Exists(_settingsPath)) RuntimeBundle.Ordinary(_settingsPath, false);
            File.Move(temporary, _settingsPath, overwrite: true);
        }
        catch (Exception error) { TryWrite("settings-save-failed", new { error = error.ToString() }); }
    }

    public void Dispose() => _logMutex.Dispose();
}
