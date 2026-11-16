using System.IO.MemoryMappedFiles;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.Loader;
using System.Security.Cryptography;
using System.Text.Json;

namespace WinxCoop.WorldInitializationChecks;

// Real local mapping/identity contract against SHA-pinned Actor bytes. The
// synthetic bridge and synthetic caller receipt are fixtures, never gameplay
// or evidence that the original native apply was executed by this checker.
internal static class ChecksProgram
{
    private const BindingFlags Instance = BindingFlags.Instance | BindingFlags.NonPublic | BindingFlags.Public;
    private const BindingFlags Static = BindingFlags.Static | BindingFlags.NonPublic | BindingFlags.Public;
    private sealed record Check(string Name, bool Passed);
    private static readonly List<Check> Results = [];
    private static bool _hostPreparationDelayObserved, _hostSnapshotPublicationObserved;
    private static int Main(string[] args)
    {
        if (InputTerminalDrainChecks.TryRun(args, out int terminalResult)) return terminalResult;
        string? output = null, failure = null;
        try
        {
            Require(OperatingSystem.IsWindows(), "These exact local-mapping checks require Windows.");
            Require(args.Length == 8, "Use --actor-directory PATH --actor-sha256 SHA64 --core-sha256 SHA64 --output FRESH_PATH.");
            Dictionary<string, string> options = new(StringComparer.Ordinal);
            for (int i = 0; i < args.Length; i += 2) Require(options.TryAdd(args[i], args[i + 1]), "Repeated option.");
            output = options["--output"]; string directory = options["--actor-directory"];
            Require(options.Count == 4 && Path.IsPathFullyQualified(directory) && Path.IsPathFullyQualified(output) &&
                !Directory.Exists(output) && !File.Exists(output), "Pinned absolute paths and fresh output required.");
            using var coreFile = OpenPinned(Path.Combine(directory, "SmoNativeValidator.Core.dll"), options["--core-sha256"]);
            using var actorFile = OpenPinned(Path.Combine(directory, "WinxCoop.ActorProbe.dll"), options["--actor-sha256"]);
            AssemblyLoadContext.Default.LoadFromStream(coreFile);
            Assembly actor = AssemblyLoadContext.Default.LoadFromStream(actorFile);
            using (var fixture = new MappingFixture(actor))
            {
                CheckThat(fixture.U(128 + 4) == 0 && fixture.U(128 + 8) == Environment.ProcessId,
                    "fresh endpoint claims only its exact current-process owner in Waiting");
                Require((bool)fixture.Call("SetReady", true, false)!, "initial transport observation failed");
                uint revision = fixture.U(128 + 12), publication = fixture.U(128);
                CheckThat(fixture.U(132) == 3 && fixture.U(160) == 0 && fixture.L(168) == 0,
                    "first scene observation publishes Initializing with no native receipt");
                Thread.Sleep(2101); // Model the observed long construction interval; no bridge/native/game call is repeated.
                fixture.PublishBridge();
                Require((bool)fixture.Call("SetReady", true, false)!, "later initial observation failed");
                CheckThat(fixture.U(132) == 3 && fixture.U(140) == revision && fixture.U(128) > publication && fixture.L(168) == 0,
                    "initial observations spanning the ordinary2s lease never promote Ready or an ACK");
                fixture.PublishSnapshot();
                object delivery = fixture.ReadDelivery();
                CheckThat(fixture.U(132) == 3 && fixture.U(160) == 0 && fixture.L(168) == 0,
                    "reading a complete synthetic delivery does not publish native apply");
                Require((bool)fixture.Call("Acknowledge", delivery)!, "synthetic caller receipt rejected exact pending delivery");
                CheckThat(fixture.U(132) == 1 && fixture.U(140) == revision + 1 && fixture.U(160) == 1 && fixture.L(168) == 7,
                    "explicit synthetic complete caller receipt atomically publishes first Ready and exact epoch/revision");
                fixture.PublishBridge(7);
                Require((bool)fixture.Call("SetReady", true, false)!, "later actual-observation API failed");
                CheckThat(fixture.U(132) == 1 && fixture.U(140) == revision + 1 && fixture.L(168) == 7,
                    "later Ready heartbeat keeps its exact existing ACK and never re-enters Initializing");
            }
            using (var fixture = new MappingFixture(actor))
            {
                Require((bool)fixture.Call("SetReady", true, false)!, "negative initialization failed");
                fixture.PublishSnapshot(); object delivery = fixture.ReadDelivery();
                fixture.CorruptDeliveredPayload();
                CheckThat(!(bool)fixture.Call("Acknowledge", delivery)! && fixture.U(132) == 3 && fixture.U(164) == 11 && fixture.L(168) == 0,
                    "changed delivered payload fences initial owner without adding Ready or an ACK");
            }
            using (var fixture = new MappingFixture(actor, role: 1))
            {
                CheckThat(fixture.U(132) == 0 && fixture.U(136) == Environment.ProcessId && fixture.U(192) == 0 &&
                    fixture.U(160) == 0 && fixture.L(168) == 0,
                    "fresh Host endpoint claims its exact owner only in Waiting with no snapshot or apply ACK");
                CheckThat(fixture.RejectsMalformedSnapshot() && fixture.U(132) == 0 && fixture.U(192) == 0,
                    "invalid full snapshot constructor leaves Host Waiting and its slot unpublished");
                CheckThat(!fixture.PublishHostSnapshot(fixture.CaptureSnapshot()) && fixture.U(132) == 0 && fixture.U(192) == 0,
                    "Host cannot publish even a valid snapshot while its Actor remains Waiting");
                Require((bool)fixture.Call("SetReady", false, false)!, "Host preparation observation failed");
                uint revision = fixture.U(140), publication = fixture.U(128);
                ulong observed = fixture.L(152);
                Thread.Sleep(3101); // Synthetic cold Host capture interval; no game/native capture is performed.
                fixture.PublishBridge(phase: 1, epoch: 0);
                Require((bool)fixture.Call("SetReady", false, false)!, "later Host preparation observation failed");
                CheckThat(fixture.U(132) == 0 && fixture.U(140) == revision && fixture.U(128) > publication &&
                    fixture.L(152) - observed >= 3000 && fixture.U(192) == 0 && fixture.U(160) == 0 && fixture.L(168) == 0,
                    "Host preparation spanning more than3s refreshes Waiting without Ready, snapshot or native apply");
                _hostPreparationDelayObserved = true;
                object snapshot = fixture.CaptureSnapshot(); // Copy-owned and validated by the actual Actor constructor.
                Require((bool)fixture.Call("SetReady", true, false)!, "validated Host readiness failed");
                CheckThat(fixture.U(132) == 1 && fixture.U(140) == revision + 1 && fixture.U(192) == 0 &&
                    fixture.U(160) == 0 && fixture.L(168) == 0,
                    "complete validated Host snapshot permits Ready1 exactly once without inventing an apply ACK");
                CheckThat(fixture.PublishHostSnapshot(snapshot) && fixture.HostSnapshotMatches(snapshot) &&
                    fixture.U(160) == 0 && fixture.L(168) == 0,
                    "Ready Host publishes the exact full local slot even before peer readiness and native apply");
                _hostSnapshotPublicationObserved = true;
                uint readyRevision = fixture.U(140), snapshotPublication = fixture.U(192);
                Require((bool)fixture.Call("SetReady", true, false)!, "later Host Ready observation failed");
                CheckThat(fixture.U(132) == 1 && fixture.U(140) == readyRevision && fixture.U(192) == snapshotPublication &&
                    fixture.HostSnapshotMatches(snapshot) && fixture.L(168) == 0,
                    "later Host heartbeat keeps Ready revision and the complete published slot without an ACK");
            }
            using (var fixture = new MappingFixture(actor, role: 1))
                CheckThat(fixture.U(132) == 0 && fixture.U(192) == 0 && fixture.U(160) == 0 && fixture.L(168) == 0,
                    "a fresh Host transport endpoint starts Waiting without inheriting the previous snapshot or ACK");
        }
        catch (Exception error) { failure = error.ToString(); }
        bool passed = failure is null && Results.Count == 15 && Results.All(check => check.Passed);
        if (output is not null && !Directory.Exists(output) && !File.Exists(output))
        {
            Directory.CreateDirectory(output);
            using var file = new FileStream(Path.Combine(output, "report.json"), FileMode.CreateNew, FileAccess.Write, FileShare.Read);
            JsonSerializer.Serialize(file, new { schema = 1, passed, failure, checks = Results,
                fixture = true, actualMappingAndExactOwner = true, syntheticBridge = true,
                nativeApplyCallerReceiptSimulated = true, nativeGameApplyObserved = false,
                hostColdCaptureDelaySimulated = true, hostPreparationDelayObserved = _hostPreparationDelayObserved,
                hostLocalSnapshotPublicationObserved = _hostSnapshotPublicationObserved,
                hostNativeCaptureObserved = false, hostNativeApplyAcknowledgmentClaimed = false,
                hostSessionAdmissionOrderingExecuted = false,
                gameStarted = false, nativeTickAdded = false, gameplayQualified = false }, new JsonSerializerOptions { WriteIndented = true });
        }
        Console.WriteLine(passed ? "World initialization mapping checks PASS (fixture only)." : "World initialization mapping checks FAIL: " + failure);
        return passed ? 0 : 1;
    }

    private sealed class MappingFixture : IDisposable
    {
        private readonly MemoryMappedFile _file;
        private readonly MemoryMappedViewAccessor _view;
        private readonly object _client;
        private readonly Type _clientType;
        private readonly Assembly _actor;
        private readonly byte[] _payload;
        private uint _bridgePublication;
        internal MappingFixture(Assembly actor, uint role = 2)
        {
            _actor = actor;
            string mapping = "Local\\WinxCoop.WorldState1.InitializationCheck." + Guid.NewGuid().ToString("N");
            _file = MemoryMappedFile.CreateNew(mapping, 16640, MemoryMappedFileAccess.ReadWrite);
            _view = _file.CreateViewAccessor(0, 16640, MemoryMappedFileAccess.ReadWrite);
            try
            {
                Require(GetProcessTimes(GetCurrentProcess(), out long created, out _, out _, out _), "Current fixture process identity unavailable.");
                W(0, 0x31534357); W(4, 1); W(8, 16640); W(12, 16384); W(16, 44UL);
                W(24, (uint)Environment.ProcessId); W(28, role); W(32, (ulong)created);
                W(40, 11UL); W(48, 22UL); W(56, 1); W(60, 1);
                if (role == 1) PublishBridge(phase: 1, epoch: 0); else PublishBridge();
                Type roleType = Type(actor, "WorldStateRole"), identity = Type(actor, "WorldStateIdentity");
                object id = Activator.CreateInstance(identity, Instance, null,
                    [mapping, Enum.ToObject(roleType, role), (uint)Environment.ProcessId, (ulong)created, 44UL, 11UL, 22UL, 1u, 1u], null)!;
                _clientType = Type(actor, "WorldStateClient");
                _client = Activator.CreateInstance(_clientType, Instance, null, [id, null], null)!;
                _payload = Pack(CaptureSnapshot());
            }
            catch { _view.Dispose(); _file.Dispose(); throw; }
        }
        internal uint U(long offset) => _view.ReadUInt32(offset);
        internal ulong L(long offset) => _view.ReadUInt64(offset);
        private void W(long offset, uint value) => _view.Write(offset, value);
        private void W(long offset, ulong value) => _view.Write(offset, value);
        internal object? Call(string method, params object?[] arguments) => Invoke(_clientType.GetMethod(method, Instance)!, _client, arguments);
        internal void PublishBridge(ulong received = 0, uint phase = 2, uint epoch = 1)
        {
            W(64, _bridgePublication + 1); Thread.MemoryBarrier(); W(68, phase); W(72, 33UL); W(80, epoch); W(84, 0);
            W(88, 0UL); W(96, 0UL); W(104, (ulong)Environment.TickCount64); W(112, received); W(120, 0UL);
            Thread.MemoryBarrier(); _bridgePublication += 2; W(64, _bridgePublication);
        }
        internal void PublishSnapshot()
        {
            W(192, 1); Thread.MemoryBarrier(); W(196, (uint)_payload.Length); W(200, 7UL); W(208, 100UL);
            _view.WriteArray(256, _payload, 0, _payload.Length); Thread.MemoryBarrier(); W(192, 2); PublishBridge(7);
        }
        internal object ReadDelivery()
        {
            object?[] arguments = [null];
            Require((bool)Invoke(_clientType.GetMethod("TryRead", Instance)!, _client, arguments)!, "synthetic full delivery unavailable");
            return arguments[0]!;
        }
        internal void CorruptDeliveredPayload() => _view.Write(256 + 40 + 20, 1.0f);
        internal object CaptureSnapshot() => CreateSnapshot(new byte[411]);
        private object CreateSnapshot(byte[] profile)
        {
            Type body = Type(_actor, "WorldPresentationActorState"), snapshot = Type(_actor, "WorldStateSnapshot");
            Array bodies = Array.CreateInstance(body, 2), entities = Array.CreateInstance(body, 0);
            for (uint i = 0; i < 2; ++i)
                bodies.SetValue(Activator.CreateInstance(body, Instance, null, [i + 1, 0x37810000u, 0u, 0u, 17u,
                    new float[] { 0, 0, 0 }, new float[] { 1, 1, 1 }, new float[] { 1, 0, 0, 0, 1, 0, 0, 0, 1 },
                    new float[] { 0, 0, 0 }, -1, -1], null), i);
            return Activator.CreateInstance(snapshot, Instance, null,
                [7UL, 100UL, 0u, bodies, entities, profile, null, null, null], null)!;
        }
        private byte[] Pack(object snapshot) => (byte[])Invoke(Type(_actor, "WorldStateContract")
            .GetMethod("PackLocalSnapshot", Static)!, null, [snapshot])!;
        internal bool RejectsMalformedSnapshot()
        {
            try { _ = CreateSnapshot(new byte[410]); return false; }
            catch (TargetInvocationException error) when (error.InnerException is InvalidDataException) { return true; }
        }
        internal bool PublishHostSnapshot(object snapshot) => (bool)Call("TryPublish", snapshot)!;
        internal bool HostSnapshotMatches(object snapshot)
        {
            byte[] expected = Pack(snapshot), actual = new byte[expected.Length];
            _view.ReadArray(256, actual, 0, actual.Length);
            return U(192) > 0 && (U(192) & 1) == 0 && U(196) == expected.Length && L(200) == 7 &&
                L(208) == 100 && actual.AsSpan().SequenceEqual(expected);
        }
        public void Dispose() { try { ((IDisposable)_client).Dispose(); } finally { _view.Dispose(); _file.Dispose(); } }
    }

    private static Type Type(Assembly actor, string name) => actor.GetType("WinxCoop.RuntimeProbe." + name, true)!;
    private static object? Invoke(MethodInfo method, object? target, object?[] arguments)
    {
        try { return method.Invoke(target, arguments); }
        catch (TargetInvocationException error) when (error.InnerException is not null)
        { System.Runtime.ExceptionServices.ExceptionDispatchInfo.Capture(error.InnerException).Throw(); throw; }
    }
    private static FileStream OpenPinned(string path, string expected)
    {
        Require(expected.Length == 64 && expected.All(Uri.IsHexDigit), "Explicit SHA256 pin required.");
        var file = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read);
        try
        {
            using var hash = SHA256.Create();
            Require(Convert.ToHexString(hash.ComputeHash(file)).Equals(expected, StringComparison.OrdinalIgnoreCase), "Pinned managed bytes changed.");
            file.Position = 0; return file;
        }
        catch { file.Dispose(); throw; }
    }
    private static void Require(bool value, string why) { if (!value) throw new InvalidDataException(why); }
    private static void CheckThat(bool value, string why) { Require(value, why); Results.Add(new(why, true)); }
    [DllImport("kernel32.dll")] private static extern IntPtr GetCurrentProcess();
    [DllImport("kernel32.dll", SetLastError = true)] [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool GetProcessTimes(IntPtr process, out long created, out long exited, out long kernel, out long user);
}
