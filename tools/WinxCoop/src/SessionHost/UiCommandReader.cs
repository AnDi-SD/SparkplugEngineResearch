using WinxCoop.RuntimeProbe;
using System.Diagnostics;
using System.Security.Cryptography;
using System.Text.Json;
using WinxCoop.SessionRuntime;

namespace WinxCoop.SessionHost;

internal sealed record UiCommand(uint Version, string CapabilityNonce, ulong Sequence,
    ulong Operation, string Action, long IssuedAtMilliseconds);

// Local UI-owner lease only. No network commands or native receipts are
// decoded here; positive operation IDs are retained across heartbeat records.
internal sealed class UiCommandReader : IDisposable
{
    private readonly SessionConfiguration _config;
    private readonly Process _owner;
    private readonly byte[] _capability;
    private readonly int _thread = Environment.CurrentManagedThreadId;
    private UiCommand? _last;
    private string? _lastHash;
    private bool _disposed;
    internal UiCommand? Last => _last;

    internal UiCommandReader(SessionConfiguration config)
    {
        _config = config;
        string path = SessionConfiguration.AbsoluteAscii(config.UiCommandPath);
        string root = SessionConfiguration.AbsoluteAscii(config.CaseDirectory) + Path.DirectorySeparatorChar;
        Require(config.Worker && config.UiOwnerPid != 0 && config.UiOwnerPid != Environment.ProcessId &&
            config.UiOwnerCreationFileTime != 0 && config.UiCapability.Length == 32 && config.UiCapability.All(Uri.IsHexDigit) &&
            path.StartsWith(root, StringComparison.OrdinalIgnoreCase), "UI control must belong to this exact local worker case.");
        RuntimeBundle.Ordinary(path, false); _capability = Convert.FromHexString(config.UiCapability);
        _owner = Process.GetProcessById(checked((int)config.UiOwnerPid));
        try { VerifyOwner(); _ = Poll(); }
        catch { _owner.Dispose(); throw; }
    }

    internal UiCommand Poll()
    {
        Require(!_disposed && Environment.CurrentManagedThreadId == _thread, "UI control reader owner changed or closed.");
        UiCommand next; byte[] bytes;
        for (int attempt = 0; ; ++attempt)
        {
            VerifyOwner(); RuntimeBundle.Ordinary(_config.UiCommandPath, false);
            bytes = ReadLastComplete(_config.UiCommandPath,
                _config.Interactive ? InteractiveSessionLimits.CommandJournalBytes : 2097152);
            using var document = JsonDocument.Parse(bytes, new JsonDocumentOptions { MaxDepth = 4 });
            JsonElement value = document.RootElement;
            Require(value.ValueKind == JsonValueKind.Object, "UI command must be an object.");
            var names = new HashSet<string>(StringComparer.Ordinal);
            foreach (JsonProperty property in value.EnumerateObject()) Require(names.Add(property.Name), "Duplicate UI command property.");
            Require(names.SetEquals(new[] { "Version", "CapabilityNonce", "Sequence", "Operation", "Action", "IssuedAtMilliseconds" }),
                "UI command schema differs from the local control contract.");
            next = new UiCommand(value.GetProperty("Version").GetUInt32(), value.GetProperty("CapabilityNonce").GetString()!,
                value.GetProperty("Sequence").GetUInt64(), value.GetProperty("Operation").GetUInt64(),
                value.GetProperty("Action").GetString()!, value.GetProperty("IssuedAtMilliseconds").GetInt64());
            long now = Environment.TickCount64;
            long age = next.IssuedAtMilliseconds >= 0 ? now - next.IssuedAtMilliseconds : -1;
            bool capabilityMatch = next.CapabilityNonce is { Length: 32 } &&
                next.CapabilityNonce.All(Uri.IsHexDigit) &&
                CryptographicOperations.FixedTimeEquals(Convert.FromHexString(next.CapabilityNonce), _capability);
            bool valid = next.Version == 1 && next.Sequence != 0 && capabilityMatch &&
                next.Action is "Start" or "Reload" or "Save" or "Load" or "Stop" &&
                (next.Operation != 0 || next.Action == "Start") && next.IssuedAtMilliseconds >= 0 &&
                next.IssuedAtMilliseconds <= now;
            // Reading a real record can straddle a scheduling stall. Resnapshot
            // once immediately; never extend the lease or accept an invalid row.
            if (attempt == 0 && valid && age > 5000) continue;
            Require(valid && age <= 5000,
                $"UI command capability, action or fresh local lease is invalid. sequence={next.Sequence}, " +
                $"issued={next.IssuedAtMilliseconds}, now={now}, age={age}, capabilityMatch={capabilityMatch}.");
            break;
        }
        string hash = Convert.ToHexString(SHA256.HashData(bytes));
        if (_last is not null)
        {
            Require(next.Sequence >= _last.Sequence && next.Operation >= _last.Operation,
                "UI command sequence/operation rolled back.");
            if (next.Sequence == _last.Sequence)
            { Require(hash == _lastHash, "A repeated UI sequence changed its payload."); return _last; }
            Require(next.Operation != _last.Operation || next.Action == _last.Action,
                "The same UI operation changed its action.");
            Require(_last.Action != "Stop" || next.Action == "Stop" && next.Operation == _last.Operation,
                "A terminal UI Stop was revoked.");
        }
        _last = next; _lastHash = hash; return next;
    }

    private void VerifyOwner() => Require(!_owner.HasExited && OwnedChild.NativeCreation(_owner) == _config.UiOwnerCreationFileTime,
        "The exact local UI process closed or changed.");

    private static byte[] ReadLastComplete(string path, long maximumBytes)
    {
        for (int attempt = 0; ; ++attempt)
        {
            try
            {
                using var file = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
                Require(file.Length > 0 && file.Length <= maximumBytes, "UI command journal exceeds its selected byte bound.");
                int count = checked((int)Math.Min(file.Length, 8192)); long length = file.Length;
                file.Position = length - count; byte[] bytes = new byte[count]; file.ReadExactly(bytes);
                int end = Array.LastIndexOf(bytes, (byte)'\n');
                Require(end >= 0, "UI journal has no complete command.");
                int begin = end == 0 ? -1 : Array.LastIndexOf(bytes, (byte)'\n', end - 1);
                Require(begin >= 0 || count == length, "UI command exceeds the bounded tail.");
                int recordBytes = end - begin - 1; Require(recordBytes is > 0 and <= 4096, "UI command length is invalid.");
                return bytes.AsSpan(begin + 1, recordBytes).ToArray();
            }
            catch (IOException error) when (attempt < 3 && (error.HResult & 0xFFFF) is 32 or 33)
            { Thread.Sleep(1); }
        }
    }

    public void Dispose() { if (_disposed) return; _owner.Dispose(); _disposed = true; }
    private static void Require(bool value, string message) => SessionConfiguration.Require(value, message);
}
