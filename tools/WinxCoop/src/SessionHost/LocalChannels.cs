using WinxCoop.RuntimeProbe;
using System.Buffers.Binary;
using System.Globalization;
using System.IO.MemoryMappedFiles;
using System.Text.Json;
using System.Text.Json.Nodes;
using WinxCoop.SessionRuntime;
using P = WinxCoop.SessionRuntime.ProgressionNative;

namespace WinxCoop.SessionHost;

// Exactly the local role's two bridge owners. No remote process, MMF, Actor
// or filesystem is opened. All peer authority arrives through native transport.
internal sealed class LocalChannels : IDisposable
{
    private const int ActorLeaseMilliseconds = 2000;
    private readonly SessionConfiguration _config;
    private readonly RuntimeBundle _bundle;
    private readonly RuntimeProcessPolicy _policy;
    private readonly BootstrapPeer.Assignment _assignment;
    private readonly string _directory;
    private readonly Dictionary<bool, Channel> _channels = [];
    private readonly int _thread = Environment.CurrentManagedThreadId;
    private OwnedChild? _actor;
    private bool _started, _waited, _disposed;
    private sealed record Channel(bool World, string Mapping, ulong Nonce, OwnedChild Child, string ReadyPath, string ReportPath, JsonObject? Ready);
    private sealed record WorldRead(uint SyncEpoch, ulong Sent, ulong Applied, ulong Received,
        uint ActorEpoch, ulong ActorApplied, ulong SnapshotRevision, ulong SnapshotTick, uint SnapshotFlags);

    internal int InputPort { get; private set; }
    internal int WorldPort { get; private set; }
    internal IReadOnlyList<OwnedChild> Children => _channels.Values.Select(value => value.Child).ToArray();

    internal LocalChannels(SessionConfiguration config, RuntimeBundle bundle, RuntimeProcessPolicy policy,
        BootstrapPeer.Assignment assignment, string generationDirectory)
    {
        Require(assignment.SessionId != 0 && assignment.ScenarioId != 0 && assignment.InputToken is > 0 and <= ushort.MaxValue &&
            assignment.WorldToken != 0 && assignment.LevelEpoch != 0 && assignment.Generation != 0,
            "Local bridge assignment has invalid session/channel/scene identities.");
        _config = config; _bundle = bundle; _policy = policy; _assignment = assignment;
        _directory = policy.ValidateOutput(Path.Combine(generationDirectory, "channels"));
        Require(!Directory.Exists(_directory) && !File.Exists(_directory), "Local channel output must be fresh.");
    }

    internal void Start(Action? pump = null)
    {
        Owner(); Require(!_started && !_disposed, "Local channels are fresh and single-use.");
        _started = true; Directory.CreateDirectory(_directory);
        Channel input = StartOne(false, pump); InputPort = Port(input.Ready!);
        Channel world = StartOne(true, pump); WorldPort = Port(world.Ready!);
        if (_config.Host) Require(InputPort != WorldPort, "Input and World must have distinct actual listeners.");
    }

    private Channel StartOne(bool world, Action? pump)
    {
        string role = _config.Role, label = (world ? "world-" : "input-") + role;
        string mapping = @"Local\WinxCoop." + (world ? "WorldState1." : "DiagnosticInput5.") + Guid.NewGuid().ToString("N") + "." + role;
        ulong nonce = _config.Host ? (world ? _assignment.HostWorldNonce : _assignment.HostInputNonce) :
            (world ? _assignment.GuestWorldNonce : _assignment.GuestInputNonce);
        Require(nonce != 0, "Host assignment must provide the exact local bridge nonce.");
        string readyPath = Path.Combine(_directory, label + "-ready.json"), reportPath = Path.Combine(_directory, label + "-report.json");
        var args = new List<string>
        {
            "--role", role, "--mapping", mapping, "--nonce", Number(nonce), "--session", Number(_assignment.SessionId),
            "--scenario", Number(_assignment.ScenarioId), "--level-epoch", Number(_assignment.LevelEpoch),
            "--generation", Number(_assignment.Generation), "--bind", _config.BindAddress + ":0",
            "--actor-lease-ms", Number(ActorLeaseMilliseconds), "--run-ms", Number(checked((ulong)_config.Seconds * 1000)),
            "--token", Number(_assignment.InputToken), "--ready-file", readyPath, "--report", reportPath
        };
        if (_config.Interactive) args.AddRange(["--interactive-session", "1"]);
        if (world) args.AddRange(["--connection-token", Number(_assignment.WorldToken)]);
        else args.AddRange(["--credits", "8", "--max-samples",
            Number(_config.Interactive ? InteractiveSessionLimits.InputSamples(_config.Seconds) : 65536u)]);
        if (!_config.Host)
        {
            uint serverPort = world ? _assignment.WorldPort : _assignment.InputPort;
            Require(serverPort is > 0 and <= ushort.MaxValue, "Guest requires the Host's actual bridge listener ports.");
            args.AddRange(["--server", _config.ServerAddress + ":" + Number(serverPort)]);
        }
        OwnedChild child;
        try { child = OwnedChild.Start(label, _bundle.File(world ? "WinxCoopWorldBridge.exe" : "WinxCoopInputBridge.exe"), args, _directory, _policy); }
        catch (OwnedChildStartException error)
        { _channels.Add(world, new(world, mapping, nonce, error.Child, readyPath, reportPath, null)); throw; }
        var pending = new Channel(world, mapping, nonce, child, readyPath, reportPath, null); _channels.Add(world, pending);
        JsonObject ready = ReadReady(readyPath, child, pump);
        Require(Exact(ready, "creationFileTimeDecimal") == child.CreationFileTime && Exact(ready, "nonceDecimal") == nonce &&
            ready["ipcBytes"]?.GetValue<uint>() == (world ? 16640u : 4352u) &&
            (!world || ready["worldStateVersion"]?.GetValue<uint>() == 1), "Local bridge Ready exact identity/ABI differs.");
        var complete = pending with { Ready = ready }; _channels[world] = complete; return complete;
    }

    private static JsonObject ReadReady(string path, OwnedChild child, Action? pump)
    {
        long deadline = Environment.TickCount64 + 15000;
        while (Environment.TickCount64 < deadline)
        {
            child.VerifyAlive(); pump?.Invoke();
            if (File.Exists(path))
            {
                try
                {
                    using var file = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read);
                    Require(file.Length is > 0 and <= 65536, "Local bridge Ready size is invalid.");
                    var ready = JsonNode.Parse(file) as JsonObject;
                    Require(ready is not null && ready["pid"]?.GetValue<int>() == child.Process.Id,
                        "Local bridge Ready belongs to another process.");
                    return ready!;
                }
                catch (JsonException) { }
                catch (IOException error) when ((error.HResult & 0xFFFF) is 32 or 33) { }
            }
            Thread.Sleep(10);
        }
        throw new TimeoutException("Owned local bridge Ready exceeded15s: " + child.Role);
    }

    internal JsonObject Endpoint(bool world)
    {
        Owner(); Channel channel = Get(world); channel.Child.VerifyAlive();
        var value = new JsonObject
        {
            ["MappingName"] = channel.Mapping, ["Role"] = _config.Host ? 1 : 2,
            ["OwnerPid"] = checked((uint)channel.Child.Process.Id), ["OwnerCreationFileTime"] = channel.Child.CreationFileTime,
            ["Nonce"] = channel.Nonce, ["SessionId"] = _assignment.SessionId, ["ScenarioId"] = _assignment.ScenarioId,
            ["LevelEpoch"] = _assignment.LevelEpoch
        };
        value[world ? "Generation" : "BodyGeneration"] = _assignment.Generation; return value;
    }

    internal void BindActor(OwnedChild actor)
    {
        Owner(); Require(_started && !_disposed && (_actor is null || ReferenceEquals(_actor, actor)), "Local Actor can be bound only once.");
        actor.VerifyAlive(); Require(actor.ExpectedJobName == _policy.JobName, "Local Actor belongs to another exact native-parent Job.");
        _actor = actor;
    }

    internal EntrySnapshotIdentity CaptureIdentity(uint level, uint ordinal)
    {
        Owner(); Require(_config.Host && _actor is not null && level != 0 && ordinal != 0, "Host capture requires its exact local Actor and ordinal.");
        OwnedChild actorOwner = _actor!;
        actorOwner.VerifyAlive(); Channel channel = Get(true); channel.Child.VerifyAlive();
        return new(channel.Mapping, channel.Nonce, checked((uint)channel.Child.Process.Id), channel.Child.CreationFileTime,
            _assignment.SessionId, _assignment.ScenarioId, _assignment.LevelEpoch, _assignment.Generation,
            checked((uint)actorOwner.Process.Id), actorOwner.CreationFileTime, _assignment.WorldToken, ActorLeaseMilliseconds, level, ordinal);
    }

    internal bool TryReadGuestApplied(out P.AppliedReceipt receipt)
    {
        Owner(); Require(!_config.Host, "Guest native apply can be observed only in its local Guest owner."); receipt = default;
        WorldRead? value = ReadWorld();
        if (value is null || value.Applied == 0 || value.ActorEpoch != value.SyncEpoch || value.ActorApplied == 0 ||
            value.ActorApplied != value.SnapshotRevision || value.Applied < value.ActorApplied ||
            value.SnapshotTick == 0 || value.SnapshotFlags != 0) return false;
        Require(value.Applied <= value.Received && value.ActorApplied <= value.Received, "Guest native/bridge ACK exceeds delivered revision.");
        receipt = new()
        {
            Scene = _assignment.Scene, Channels = _assignment.Channels, Revision = value.ActorApplied, NativeTick = value.SnapshotTick,
            CompleteNativeSnapshotApplied = 1,
            // Only the Actor event owner can attest the separate real neutral
            // InputRET boundary. Reading a World snapshot does not create it.
            FreshNeutralInputBoundaryObserved = 0
        };
        return true;
    }

    internal ulong HostPeerAppliedRevision
    {
        get
        {
            Owner(); Require(_config.Host, "Peer apply revision is observed through the local Host bridge only.");
            WorldRead? value = ReadWorld();
            if (value is null) return 0;
            Require(value.Applied <= value.Sent, "Host peer ACK exceeds actual sent revision."); return value.Applied;
        }
    }

    private WorldRead? ReadWorld()
    {
        Require(_actor is not null && !_disposed, "Bind the exact local Actor before reading native World readiness.");
        OwnedChild actorOwner = _actor!;
        actorOwner.VerifyAlive(); Channel channel = Get(true); channel.Child.VerifyAlive();
        using var mapping = MemoryMappedFile.OpenExisting(channel.Mapping, MemoryMappedFileRights.Read);
        using var view = mapping.CreateViewAccessor(0, 256, MemoryMappedFileAccess.Read);
        byte[] header = Read(view, 0, 64); ValidateHeader(header, channel);
        for (int attempt = 0; attempt < 3; ++attempt)
        {
            uint bp = view.ReadUInt32(64), ap = view.ReadUInt32(128), sp = view.ReadUInt32(192);
            if (((bp | ap | sp) & 1) != 0) continue;
            Thread.MemoryBarrier();
            byte[] bridge = Read(view, 64, 64), actor = Read(view, 128, 64), prefix = Read(view, 192, 64);
            Thread.MemoryBarrier();
            if (view.ReadUInt32(64) != bp || view.ReadUInt32(128) != ap || view.ReadUInt32(192) != sp ||
                U(bridge, 0) != bp || U(actor, 0) != ap || U(prefix, 0) != sp) continue;
            Require(Read(view, 0, 64).AsSpan().SequenceEqual(header), "Local World immutable header changed during read.");
            ValidateHeader(header, channel);
            Require(U(bridge, 20) == 0 && U(bridge, 4) is >= 1 and <= 3 && L(bridge, 56) == 0 &&
                (L(bridge, 8) == 0 || L(bridge, 8) == _assignment.WorldToken), "Local World bridge terminal or connection token differs.");
            if (U(bridge, 4) != 3 || L(bridge, 8) == 0 || ap == 0 || sp == 0) return null;
            Require(U(actor, 8) == actorOwner.Process.Id && L(actor, 16) == actorOwner.CreationFileTime && U(actor, 12) != 0 &&
                U(actor, 36) == 0 && L(actor, 48) == 0 && L(actor, 56) == 0 &&
                (U(actor, 4) is 0 or 1 || !_config.Host && U(actor, 4) == 3),
                "Local World Actor status is not this retained native owner.");
            // Initializing is transport admission only. Its fixed native
            // budget belongs to the bridge; this reader never promotes it.
            if (U(actor, 4) == 3) return null;
            ulong now = checked((ulong)Environment.TickCount64), bridgeTick = L(bridge, 40), actorTick = L(actor, 24);
            Require(bridgeTick != 0 && bridgeTick <= now && now - bridgeTick <= 500 && actorTick != 0 &&
                actorTick <= now && now - actorTick <= ActorLeaseMilliseconds, "Local bridge/Actor heartbeat is stale.");
            if (U(actor, 4) != 1) return null;
            Require(U(bridge, 16) != 0 && U(prefix, 4) is >= 40 and <= 16384 && L(prefix, 8) != 0 && L(prefix, 16) != 0 &&
                (U(prefix, 24) & ~1u) == 0 && prefix.AsSpan(28).IndexOfAnyExcept((byte)0) < 0,
                "Local same-publication World snapshot prefix is invalid.");
            return new(U(bridge, 16), L(bridge, 24), L(bridge, 32), L(bridge, 48),
                U(actor, 32), L(actor, 40), L(prefix, 8), L(prefix, 16), U(prefix, 24));
        }
        return null; // Contention does not become cached Ready.
    }

    private void ValidateHeader(byte[] header, Channel channel) => Require(
        U(header, 0) == 0x31534357 && U(header, 4) == 1 && U(header, 8) == 16640 && U(header, 12) == 16384 &&
        L(header, 16) == channel.Nonce && U(header, 24) == channel.Child.Process.Id && U(header, 28) == (_config.Host ? 1u : 2u) &&
        L(header, 32) == channel.Child.CreationFileTime && L(header, 40) == _assignment.SessionId && L(header, 48) == _assignment.ScenarioId &&
        U(header, 56) == _assignment.LevelEpoch && U(header, 60) == _assignment.Generation,
        "Local World header process/nonce/session/scene differs.");

    internal object Report() => new
    {
        role = _config.Role, localBridgesOnly = true, inputPort = InputPort, worldPort = WorldPort,
        levelEpoch = _assignment.LevelEpoch, generation = _assignment.Generation, waited = _waited,
        channels = _channels.Values.Select(channel => new { channel.World, channel.Mapping, nonceDecimal = Number(channel.Nonce),
            ready = channel.Ready, reportPath = channel.ReportPath, child = channel.Child.Report() }).ToArray()
    };

    internal void WaitStop(Action? pump = null)
    {
        Owner(); Require(_started && !_disposed && _channels.Count == 2 && _actor is not null && _actor.Process.HasExited,
            "Only the retired local Actor may release its two local bridges.");
        foreach (Channel channel in _channels.Values)
        {
            Wait(channel.Child, 20000, pump);
            using var file = new FileStream(channel.ReportPath, FileMode.Open, FileAccess.Read, FileShare.Read);
            Require(file.Length is > 0 and <= 1048576, "Local final bridge report size is invalid.");
            var report = JsonNode.Parse(file)!.AsObject(); uint terminal = report["terminalReason"]!.GetValue<uint>();
            Require(channel.Child.Process.ExitCode is 0 or 2 && !channel.Child.KilledByCoordinator &&
                report["socketsOpened"]!.GetValue<uint>() == report["socketsClosed"]!.GetValue<uint>() &&
                (channel.World ? terminal is 5 or 6 : terminal is 6 or 7) && report["pumpWait"]!["balanced"]!.GetValue<bool>(),
                "Local native bridge sockets/wait lifecycle did not complete naturally.");
            if (channel.World) Require(report["mappingClosed"]!.GetValue<bool>() && report["mappingCloseError"]!.GetValue<uint>() == 0 &&
                report["participantCloseError"]!.GetValue<uint>() == 0, "Local World mapping participants did not close.");
        }
        _waited = true;
    }

    internal void StopOwned(Action? pump = null)
    {
        Owner(); List<Exception> errors = [];
        foreach (Channel channel in _channels.Values)
        {
            try
            {
                if (!channel.Child.Process.HasExited)
                {
                    Task stop = channel.Child.StopAsync();
                    while (!stop.IsCompleted) { pump?.Invoke(); Thread.Sleep(10); }
                    stop.GetAwaiter().GetResult();
                }
                else channel.Child.FinishDrainAsync(TimeSpan.FromSeconds(5)).GetAwaiter().GetResult();
            }
            catch (Exception error) { errors.Add(error); }
        }
        if (errors.Count != 0) throw new AggregateException("Owned local bridge cleanup failed.", errors);
    }

    private static void Wait(OwnedChild child, int milliseconds, Action? pump)
    {
        long deadline = Environment.TickCount64 + milliseconds;
        while (!child.Process.HasExited)
        { Require(Environment.TickCount64 < deadline, "Local bridge exit exceeded its bounded wait."); pump?.Invoke(); Thread.Sleep(10); }
        child.WaitAsync(TimeSpan.FromSeconds(5)).GetAwaiter().GetResult();
    }
    public void Dispose()
    {
        Owner(); if (_disposed) return;
        Require(_channels.Values.All(channel => channel.Child.Process.HasExited), "Stop/wait every exact local bridge before Dispose.");
        foreach (Channel channel in _channels.Values) channel.Child.Dispose(); _disposed = true;
    }
    private Channel Get(bool world)
    { Require(_started && !_disposed && _channels.ContainsKey(world), "Missing exact local bridge."); return _channels[world]; }
    private void Owner() => Require(Environment.CurrentManagedThreadId == _thread, "Local bridge owner thread changed.");
    private static void Require(bool value, string message) => SessionConfiguration.Require(value, message);
    private static int Port(JsonObject value)
    { int port = value["port"]?.GetValue<int>() ?? 0; Require(port is > 0 and <= ushort.MaxValue, "Bridge actual bound port is invalid."); return port; }
    private static ulong Exact(JsonObject value, string key)
    {
        string text = value[key]?.GetValue<string>() ?? throw new InvalidDataException("Missing exact bridge decimal field: " + key);
        Require(ulong.TryParse(text, NumberStyles.None, CultureInfo.InvariantCulture, out ulong number) && number != 0 && Number(number) == text,
            "Invalid exact local bridge decimal identity."); return number;
    }
    private static string Number(ulong value) => value.ToString(CultureInfo.InvariantCulture);
    private static uint U(byte[] value, int offset) => BinaryPrimitives.ReadUInt32LittleEndian(value.AsSpan(offset, 4));
    private static ulong L(byte[] value, int offset) => BinaryPrimitives.ReadUInt64LittleEndian(value.AsSpan(offset, 8));
    private static byte[] Read(MemoryMappedViewAccessor view, int offset, int length)
    { byte[] value = new byte[length]; Require(view.ReadArray(offset, value, 0, length) == length, "Short local read-only World read."); return value; }
}
