using System.Diagnostics;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using WinxCoop.SessionRuntime;

namespace WinxCoop.SessionHost;

// One local native Actor generation and its capability-bound command writer.
// Raw evidence stays in the Actor's journal; memory retains bounded last/count
// indexes only. Event names are observations, never fabricated Ready receipts.
internal sealed class RoleActor : IDisposable
{
    private const int MaximumRecordBytes = 1024 * 1024, MaximumEventKinds = 512;
    private const long MaximumJournalBytes = 256L * 1024 * 1024;
    private const int ReadBudgetPerTick = 256 * 1024;
    private readonly SessionConfiguration _config;
    private readonly RuntimeBundle _bundle;
    private readonly RuntimeProcessPolicy _policy;
    private readonly LocalChannels _channels;
    private readonly FileStream _commands;
    private readonly string _capability, _eventsPath;
    private readonly Dictionary<string, int> _counts = new(StringComparer.Ordinal);
    private readonly Dictionary<string, JsonObject> _last = new(StringComparer.Ordinal);
    private readonly byte[] _tail = new byte[MaximumRecordBytes];
    private readonly byte[] _readBuffer = new byte[65536];
    private readonly int _thread = Environment.CurrentManagedThreadId;
    private readonly bool _saveAllowed, _overlayAllowed, _reloadPreparationAllowed;
    private int _tailBytes;
    private long _eventOffset, _lastHeartbeat;
    private ulong _sequence, _operation;
    private string _action = "Hold";
    private bool _disposed;
    internal readonly uint Epoch, Generation;
    internal readonly string Role, Output, ConfigPath;
    internal OwnedChild Child { get; private set; } = null!;
    internal JsonObject? Report { get; private set; }
    internal bool Started => Child is not null;
    internal int NativeFailureCount { get; private set; }
    internal bool NativeReady => Count(_config.Host ? "template-network-input-begin" : "client-presentation-session-begin") > 0;
    internal bool HeldNeutral => Count("progression-input-native-neutral") > 0;
    internal bool PrivateNeutral => Count("template-progression-neutral-native-applied") > 0;
    internal bool Completed { get { Owner(); return Started && Child.Process.HasExited && TryLoadReport(); } }

    internal RoleActor(SessionConfiguration config, RuntimeBundle bundle, RuntimeProcessPolicy policy,
        LocalChannels channels, JsonObject stockActorConfig, uint epoch, uint generation, string directory)
    {
        Require(epoch != 0 && generation != 0, "Explicit local Actor epoch/generation required.");
        _config = config; _bundle = bundle; _policy = policy; _channels = channels;
        Role = config.Role; Epoch = epoch; Generation = generation;
        string owned = policy.ValidateOutput(directory); Directory.CreateDirectory(owned);
        string control = policy.ValidateOutput(Path.Combine(owned, "control"));
        Require(!Directory.Exists(control) && !File.Exists(control), "Local Actor command directory must be fresh.");
        Directory.CreateDirectory(control);
        string commandPath = Path.Combine(control, "commands.jsonl");
        ConfigPath = Path.Combine(owned, "actor-config.json"); Output = Path.Combine(owned, "actor-native");
        Require(!Directory.Exists(Output) && !File.Exists(Output) && !File.Exists(ConfigPath), "Local Actor output/config must be fresh.");
        _eventsPath = Path.Combine(Output, "events.jsonl");
        _capability = Convert.ToHexString(RandomNumberGenerator.GetBytes(16));
#if NET6_0
        JsonObject actor = JsonNode.Parse(stockActorConfig.ToJsonString())!.AsObject();
#else
        JsonObject actor = stockActorConfig.DeepClone().AsObject();
#endif
        _saveAllowed = config.Host && actor["ProgressionSavedReload"]?.GetValue<bool>() == true;
        _overlayAllowed = actor["ProgressionOverlayFixture"]?.GetValue<bool>() == true;
        _reloadPreparationAllowed = _saveAllowed && actor["ProgressionPreWaitingReloadPreparation"]?.GetValue<bool>() == true;
        actor["OutputDirectory"] = Output;
        actor[config.Host ? "DiagnosticHostInput" : "DiagnosticGuestInput"] = channels.Endpoint(false);
        actor["DiagnosticWorldState"] = channels.Endpoint(true);
        using (Process self = Process.GetCurrentProcess())
            actor["ProgressionControl"] = new JsonObject
            {
                ["CommandPath"] = commandPath, ["OwnerPid"] = Environment.ProcessId,
                ["OwnerCreationFileTime"] = OwnedChild.NativeCreation(self), ["CapabilityNonce"] = _capability,
                ["NextLevel"] = 28
            };
        // The caller supplies the admitted native profile and entry ordinal.
        // This owner adds local transport/control authority, never a ready bit.
        actor["EnableProgressionAuthority"] = true; actor["UseProcessLocalMediaPath"] = true;
        actor["EnableWorldEntry"] = true; actor["DurationSeconds"] = 600;
        actor["WorldPresentationSmoothingMilliseconds"] = config.Host ? 0 : config.SmoothingMilliseconds;
        _commands = new FileStream(commandPath, FileMode.CreateNew, FileAccess.Write, FileShare.ReadWrite | FileShare.Delete);
        try
        {
            WriteCommand();
            using var file = new FileStream(ConfigPath, FileMode.CreateNew, FileAccess.Write, FileShare.Read);
            JsonSerializer.Serialize(file, actor, SessionConfiguration.Json);
        }
        catch { _commands.Dispose(); throw; }
    }

    internal void Start()
    {
        Owner(); Require(!Started && !_disposed, "Local Actor start is single-use.");
        try { Child = OwnedChild.Start("actor-" + Role, _bundle.File("WinxCoop.ActorProbe.exe"), ["--config", ConfigPath], Path.GetDirectoryName(ConfigPath)!, _policy); }
        catch (OwnedChildStartException error) { Child = error.Child; throw; }
        _channels.BindActor(Child);
    }

    internal void Tick()
    {
        Owner(); Require(!_disposed, "Local Actor is disposed.");
        if (!Started) return;
        if (!Child.Process.HasExited && Environment.TickCount64 - _lastHeartbeat >= 250) WriteCommand();
        ReadEvents();
        if (Child.Process.HasExited) TryLoadReport();
        Require(NativeFailureCount == 0, "Local native Actor reported a fatal/scoped failure; see its preserved raw journal.");
    }

    internal void Command(string action, ulong operation)
    {
        Owner(); Require(!_disposed && (action is "Hold" or "Resume" or "Transition" or "Retire" ||
            _saveAllowed && action == "Save" || _reloadPreparationAllowed && action == "PrepareReload" ||
            _overlayAllowed && action is "Pause" or "Continue"), "Unqualified local Actor command.");
        Require(action is not ("Transition" or "Retire" or "Save" or "Pause" or "Continue" or "PrepareReload") || operation > 0,
            "Native generation operation must be positive.");
        Require(_action is not ("Transition" or "Retire") || action == _action && operation == _operation,
            "Terminal native generation command cannot be revoked.");
        Require(_action != "Hold" || _operation == 0 || operation == _operation && action is "Hold" or "Retire",
            "Prepared local Guest generation cannot reopen gameplay.");
        Require(_action != "PrepareReload" || operation == _operation && action is "PrepareReload" or "Transition",
            "Prepared local Host reload cannot reopen gameplay or change operation.");
        _action = action; _operation = operation; WriteCommand();
    }

    internal int Count(string name) { Owner(); return _counts.GetValueOrDefault(name); }
    internal JsonObject? Last(string name)
    {
        Owner();
        if (!_last.TryGetValue(name, out JsonObject? value)) return null;
#if NET6_0
        return JsonNode.Parse(value.ToJsonString())!.AsObject();
#else
        return value.DeepClone().AsObject();
#endif
    }

    internal JsonObject SuccessfulReport()
    {
        Owner(); Require(Completed && Child.Process.ExitCode == 0 && Report is not null, "Local Actor did not complete successfully.");
        Require(File.Exists(_eventsPath), "Completed local Actor has no preserved native journal.");
        // The Actor has stopped, so draining its finite disk journal cannot
        // starve an active command lease or promote a still-growing record.
        while (File.Exists(_eventsPath) && _eventOffset < new FileInfo(_eventsPath).Length) ReadEvents();
        Require(_tailBytes == 0 && NativeFailureCount == 0, "Final native journal is incomplete or contains a failure.");
        JsonObject result = Report!["result"]!.AsObject();
        Require(result["CleanupConfirmed"]!.GetValue<bool>() && result["RequestedCompletion"]!.GetValue<bool>() &&
            !result["TimedOut"]!.GetValue<bool>() && result["FatalExceptions"]!.GetValue<int>() == 0 && result["Error"] is null,
            "Local Actor native cleanup/exception result failed.");
        return Report!;
    }

    internal object JournalSummary() => new
    {
        path = _eventsPath, consumedBytes = _eventOffset, bufferedIncompleteBytes = _tailBytes,
        retainedEventKinds = _counts.Count, maximumEventKinds = MaximumEventKinds,
        maximumRecordBytes = MaximumRecordBytes, maximumJournalBytes = MaximumJournalBytes,
        perTickReadBudget = ReadBudgetPerTick, NativeFailureCount, counts = _counts
    };

    private bool TryLoadReport()
    {
        if (!Started || !Child.Process.HasExited) return false;
        string path = Path.Combine(Output, "report.json");
        if (Report is null && File.Exists(path))
        {
            using var file = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read);
            Require(file.Length is > 0 and <= 32 * 1024 * 1024, "Local native final report size is invalid.");
            Report = JsonNode.Parse(file)?.AsObject() ?? throw new InvalidDataException("Missing local Actor native report.");
        }
        return Report is not null;
    }

    private void ReadEvents()
    {
        if (!File.Exists(_eventsPath))
        { Require(_eventOffset == 0, "Local native event journal disappeared after observation."); return; }
        using var file = new FileStream(_eventsPath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
        Require(file.Length >= _eventOffset && file.Length <= MaximumJournalBytes, "Local native event journal truncated or exceeded256MiB.");
        file.Position = _eventOffset;
        byte[] buffer = _readBuffer; int budget = ReadBudgetPerTick;
        while (budget != 0)
        {
            int count = file.Read(buffer, 0, Math.Min(buffer.Length, budget)); if (count == 0) break;
            _eventOffset += count; budget -= count;
            int begin = 0;
            while (begin < count)
            {
                int end = Array.IndexOf(buffer, (byte)'\n', begin, count - begin);
                int length = (end < 0 ? count : end) - begin;
                Require(_tailBytes + length <= MaximumRecordBytes, "Local native event record exceeds1MiB.");
                buffer.AsSpan(begin, length).CopyTo(_tail.AsSpan(_tailBytes)); _tailBytes += length;
                if (end < 0) break;
                if (_tailBytes != 0) ObserveRecord(_tail.AsSpan(0, _tailBytes));
                _tailBytes = 0; begin = end + 1;
            }
        }
    }

    private void ObserveRecord(ReadOnlySpan<byte> bytes)
    {
        JsonObject? wrapper = JsonNode.Parse(bytes)?.AsObject();
        JsonObject? data = wrapper?["data"] as JsonObject;
        string? name = data?["event"]?.GetValue<string>();
        Require(data is not null && name is { Length: > 0 and <= 160 }, "Local native event shape is invalid.");
        if (!_counts.ContainsKey(name!)) Require(_counts.Count < MaximumEventKinds, "Local native event kind count exceeds its bound.");
        _counts[name!] = checked(_counts.GetValueOrDefault(name!) + 1); _last[name!] = data!;
        if (name is "fatal-exception" or "debugger-error" or "debug-event-error" || name!.EndsWith("-failed", StringComparison.Ordinal))
            ++NativeFailureCount;
    }

    private void WriteCommand()
    {
        _lastHeartbeat = Environment.TickCount64;
        var command = new { Version = 1, CapabilityNonce = _capability, LevelEpoch = Epoch, Generation,
            Sequence = checked(++_sequence), Operation = _operation, Action = _action, IssuedAtMilliseconds = _lastHeartbeat };
        byte[] bytes = Encoding.UTF8.GetBytes(JsonSerializer.Serialize(command) + "\n");
        Require(_commands.Length + bytes.Length <= 2097152, "Local Actor command journal exceeded its native2MiB bound.");
        _commands.Write(bytes); _commands.Flush();
    }

    public void Dispose()
    {
        Owner(); if (_disposed) return;
        Require(!Started || Child.Process.HasExited, "Stop/wait the exact local Actor before Dispose.");
        _commands.Dispose(); if (Started) Child.Dispose(); _disposed = true;
    }
    private void Owner() => Require(Environment.CurrentManagedThreadId == _thread, "Local Actor owner thread changed.");
    private static void Require(bool value, string message) => SessionConfiguration.Require(value, message);
}
