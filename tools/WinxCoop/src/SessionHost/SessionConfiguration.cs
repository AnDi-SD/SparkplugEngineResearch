using System.Net;
using System.Text.Json;

namespace WinxCoop.SessionHost;

internal sealed record SessionConfiguration
{
    public uint Version { get; init; } = 1;
    public bool Worker { get; init; }
    public string BundleDirectory { get; init; } = "";
    public string GameRoot { get; init; } = "";
    public string CaseDirectory { get; init; } = "";
    public string Role { get; init; } = "host";
    public string BindAddress { get; init; } = "0.0.0.0";
    public string ServerAddress { get; init; } = "127.0.0.1";
    public ushort Port { get; init; } = 27045;
    public string Profile { get; init; } = "alfea-28";
    public int Seconds { get; init; } = 600;
    public int SmoothingMilliseconds { get; init; } = 50;
    public string JobName { get; init; } = "";
    public string UiCommandPath { get; init; } = "";
    public string UiCapability { get; init; } = "";
    public uint UiOwnerPid { get; init; }
    public ulong UiOwnerCreationFileTime { get; init; }

    internal bool Host => Role == "host";

    internal void Validate()
    {
        Require(Version == 1 && Role is "host" or "guest", "Unsupported session configuration or role.");
        Require(Profile == "alfea-28", "This runtime profile currently admits Alfea 28 only.");
        Require(Seconds is >= 60 and <= 600 && SmoothingMilliseconds is >= 0 and <= 200,
            "Session duration must be60..600s; smoothing must be0..200ms.");
        Require(Port != 0 && IPAddress.TryParse(BindAddress, out var bind) &&
            bind.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork &&
            IPAddress.TryParse(ServerAddress, out var server) &&
            server.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork && !server.Equals(IPAddress.Any),
            "Explicit IPv4 bind and server addresses are required.");
        foreach (string path in new[] { BundleDirectory, GameRoot, CaseDirectory }) AbsoluteAscii(path);
    }

    internal static string AbsoluteAscii(string value)
    {
        Require(!string.IsNullOrWhiteSpace(value) && Path.IsPathFullyQualified(value), "An absolute local path is required.");
        string full = Path.GetFullPath(value).TrimEnd(Path.DirectorySeparatorChar);
        Require(full.Length is > 3 and <= 180 && full[1] == ':' && full[2] == '\\' &&
            full.All(c => c is >= ' ' and <= '~' && c != '"'), "Use a short local ASCII path without quotes.");
        return full;
    }

    internal static void Require(bool value, string message)
    { if (!value) throw new InvalidDataException(message); }

    internal static readonly JsonSerializerOptions Json = new() { WriteIndented = true, PropertyNameCaseInsensitive = false, IncludeFields = true };
}
