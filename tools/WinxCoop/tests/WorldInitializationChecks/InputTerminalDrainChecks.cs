using System.Diagnostics;
using System.IO.MemoryMappedFiles;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.Loader;
using System.Security.Cryptography;
using System.Text.Json;

namespace WinxCoop.WorldInitializationChecks;

// Real managed writer processes and retained local mappings, never game input
// or a native apply receipt. Root alone builds/runs this separate CLI mode.
internal static class InputTerminalDrainChecks
{
    private const BindingFlags Members = BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic;
    private const int ExpectedChecks = 18;
    private sealed record Check(string Name, bool Passed);
    private static readonly List<Check> Results = [];

    internal static bool TryRun(string[] args, out int result)
    {
        result = 0;
        if (args.Length == 0) return false;
        if (args[0] == "--input-terminal-owner-child") { result = RunOwner(args); return true; }
        if (args[0] != "--input-terminal-checks") return false;
        result = Execute(args[1..]); return true;
    }

    private static int Execute(string[] args)
    {
        string? output = null, failure = null;
        try
        {
            Require(OperatingSystem.IsWindows(), "Input owner/mapping checks require Windows.");
            Require(args.Length == 8, "Use --input-terminal-checks --actor-directory PATH --actor-sha256 SHA --core-sha256 SHA --output FRESH.");
            Dictionary<string, string> options = new(StringComparer.Ordinal);
            for (int index = 0; index < args.Length; index += 2) Require(options.TryAdd(args[index], args[index + 1]), "Repeated option.");
            output = options["--output"]; string directory = options["--actor-directory"];
            Require(options.Count == 4 && Path.IsPathFullyQualified(directory) && Path.IsPathFullyQualified(output) &&
                !Directory.Exists(output) && !File.Exists(output), "Pinned absolute paths and fresh output required.");
            Directory.CreateDirectory(output);
            using var core = OpenPinned(Path.Combine(directory, "SmoNativeValidator.Core.dll"), options["--core-sha256"]);
            using var actorFile = OpenPinned(Path.Combine(directory, "WinxCoop.ActorProbe.dll"), options["--actor-sha256"]);
            AssemblyLoadContext.Default.LoadFromStream(core);
            Assembly actor = AssemblyLoadContext.Default.LoadFromStream(actorFile);
            using (var fixture = new OwnerFixture(actor))
            {
                CheckThat(fixture.Read() && fixture.StatusState == 3 && fixture.RetainedOwner is { IsClosed: false, IsInvalid: false },
                    "live exact owner admits its current Ready status and retains one process handle");
                SafeHandle retained = fixture.RetainedOwner!;
                fixture.Finish("closed");
                CheckThat(fixture.OwnerExitedZero && !retained.IsClosed,
                    "real writer exited0 while admitted owner handle and mapping remain retained");
                CheckThat(!fixture.Read() && fixture.Terminal == "ActorClosed" && fixture.Drains == 1 && fixture.StatusState == 5,
                    "current coherent Closed7 and producer7 survive owner exit without cached Ready");
                CheckThat(fixture.U(192) == 0 && fixture.U(196) == 0 && fixture.U(148) == 0 && fixture.U(152) == 0 && fixture.U(132) == 0 &&
                    fixture.GrantHistoryCount == 1,
                    "terminal drain advances no ring cursor native acknowledgment or actor Ready");
                CheckThat(fixture.TerminalCannotAdvance(),
                    "terminal endpoint cannot peek publish refresh Ready or invoke an applied verifier");
                fixture.DisposeClient();
                CheckThat(retained.IsClosed && fixture.RetainedOwner is null,
                    "client Dispose closes retained owner handle after mapping release");
            }
            Negative(actor, "ready", "dead Ready remains PeerClosed rather than a retained grant");
            Negative(actor, "ready-terminal", "producer7 cannot authorize a dead current Ready status");
            Negative(actor, "odd", "dead odd terminal publication cannot use cached Ready or terminal data");
            Negative(actor, "zero", "zero terminal publication is not a published final status");
            Negative(actor, "mixed", "Closed7 with a different producer terminal remains fail closed");
            Negative(actor, "unknown", "unknown status or producer terminal cannot become ActorClosed");
            Negative(actor, "malformed", "malformed final status cannot become ActorClosed");
            using (var fixture = new OwnerFixture(actor))
            {
                Require(fixture.Read(), "Live header-drift baseline unavailable."); fixture.Finish("header");
                bool rejected = false;
                try { fixture.Read(); } catch (InvalidDataException) { rejected = true; }
                CheckThat(rejected && fixture.Drains == 0 && fixture.U(148) == 0 && fixture.U(152) == 0,
                    "changed immutable nonce rejects terminal drain before any acknowledgment");
            }
            Negative(actor, "nonzero", "same owner with nonzero exit is not the exit0 terminal-drain contract");
            Negative(actor, "closed", "Guest keeps its existing failure policy and cannot use Host producer drain", role: 2);
            using (var fixture = new OwnerFixture(actor))
            {
                Require(fixture.Read(), "Live query-failure baseline unavailable.");
                fixture.RetainedOwner!.Dispose(); fixture.Finish("closed");
                CheckThat(!fixture.Read() && fixture.Terminal == "PeerClosed" && fixture.Drains == 0 &&
                    fixture.U(148) == 0 && fixture.U(152) == 0,
                    "unavailable retained owner query fails closed even with Closed7 bytes");
            }
            using (var fixture = new OwnerFixture(actor, badHeader: true, claim: false))
            {
                Require(fixture.RejectConstructor(), "Constructor failure warmup unexpectedly succeeded.");
                uint before = HandleCount();
                for (int index = 0; index < 12; ++index) Require(fixture.RejectConstructor(), "Invalid header accepted on retry.");
                uint after = HandleCount();
                CheckThat(after <= before + 1,
                    "repeated constructor failures release owner and mapping handles without cumulative leakage");
            }
        }
        catch (Exception error) { failure = error.ToString(); }
        bool passed = failure is null && Results.Count == ExpectedChecks && Results.All(row => row.Passed);
        if (output is not null && Directory.Exists(output) && !File.Exists(Path.Combine(output, "report.json")))
        {
            using var file = new FileStream(Path.Combine(output, "report.json"), FileMode.CreateNew, FileAccess.Write, FileShare.Read);
            JsonSerializer.Serialize(file, new { schema = 1, passed, failure, expectedChecks = ExpectedChecks, checks = Results,
                fixture = true, actualManagedOwnerExitAndRetainedMapping = true, syntheticBridge = true,
                ownerQueryUnavailableSimulatedByClosingOwnedHandle = true, actualPidReuseOrAccessDeniedForced = false,
                gameStarted = false, nativeApplyObserved = false, nativeAcknowledgmentAdded = false,
                gameplayQualified = false }, new JsonSerializerOptions { WriteIndented = true });
        }
        Console.WriteLine(passed ? "Input terminal drain mapping checks PASS (fixture only)." : "Input terminal drain checks FAIL: " + failure);
        return passed ? 0 : 1;
    }

    private static void Negative(Assembly actor, string mode, string name, uint role = 1)
    {
        using var fixture = new OwnerFixture(actor, role);
        Require(fixture.Read(), "Live negative-case baseline unavailable."); fixture.Finish(mode);
        CheckThat(!fixture.Read() && fixture.Terminal == "PeerClosed" && fixture.Drains == 0 &&
            fixture.U(192) == 0 && fixture.U(196) == 0 && fixture.U(148) == 0 && fixture.U(152) == 0, name);
    }

    private sealed class OwnerFixture : IDisposable
    {
        private readonly Process _writer;
        private readonly MemoryMappedFile? _mapping;
        private readonly MemoryMappedViewAccessor? _view;
        private readonly Type _type, _identity;
        private readonly object _id;
        private object? _client;
        internal OwnerFixture(Assembly actor, uint role = 1, bool badHeader = false, bool claim = true)
        {
            if (!OperatingSystem.IsWindows()) throw new PlatformNotSupportedException("Input owner fixtures require Windows.");
            string name = "Local\\WinxCoop.InputTerminalCheck." + Guid.NewGuid().ToString("N");
            string executable = Environment.ProcessPath ?? throw new InvalidOperationException("Current checker executable unavailable.");
            var start = new ProcessStartInfo(executable) { UseShellExecute = false, CreateNoWindow = true,
                RedirectStandardInput = true, RedirectStandardOutput = true, RedirectStandardError = true };
            if (string.Equals(Path.GetFileNameWithoutExtension(executable), "dotnet", StringComparison.OrdinalIgnoreCase))
                start.ArgumentList.Add(typeof(InputTerminalDrainChecks).Assembly.Location);
            foreach (string argument in new[] { "--input-terminal-owner-child", name, role.ToString(System.Globalization.CultureInfo.InvariantCulture), badHeader ? "1" : "0" })
                start.ArgumentList.Add(argument);
            _writer = Process.Start(start) ?? throw new InvalidOperationException("Owned fixture writer did not start.");
            try
            {
                var line = _writer.StandardOutput.ReadLineAsync();
                Require(line.Wait(5000) && line.Result is not null, "Owned writer did not publish its bounded identity.");
                using JsonDocument ready = JsonDocument.Parse(line.Result!);
                uint pid = ready.RootElement.GetProperty("pid").GetUInt32();
                ulong creation = ready.RootElement.GetProperty("creation").GetUInt64();
                Require(pid == _writer.Id && ready.RootElement.GetProperty("mapping").GetString() == name,
                    "Fixture writer identity does not match its owned process and mapping.");
                _mapping = MemoryMappedFile.OpenExisting(name, MemoryMappedFileRights.ReadWrite);
                _view = _mapping.CreateViewAccessor(0, 4352, MemoryMappedFileAccess.ReadWrite);
                _identity = actor.GetType("WinxCoop.RuntimeProbe.DiagnosticInputIdentity", true)!;
                Type roleType = actor.GetType("WinxCoop.RuntimeProbe.DiagnosticInputRole", true)!;
                _type = actor.GetType("WinxCoop.RuntimeProbe.DiagnosticInputClient", true)!;
                _id = Activator.CreateInstance(_identity, Members, null,
                    [name, Enum.ToObject(roleType, role), pid, creation, 44UL, 11UL, 22UL, 1u, 1u], null)!;
                if (claim) _client = NewClient();
            }
            catch
            {
                _view?.Dispose(); _mapping?.Dispose();
                try
                {
                    _writer.StandardInput.Close();
                    if (!_writer.WaitForExit(2000)) { _writer.Kill(); _writer.WaitForExit(2000); }
                }
                finally { _writer.Dispose(); }
                throw;
            }
        }
        private object NewClient()
        {
            try { return Activator.CreateInstance(_type, Members, null, [_id, null], null)!; }
            catch (TargetInvocationException error) when (error.InnerException is not null)
            { System.Runtime.ExceptionServices.ExceptionDispatchInfo.Capture(error.InnerException).Throw(); throw; }
        }
        internal uint U(int offset) => _view!.ReadUInt32(offset);
        internal SafeHandle? RetainedOwner => _client is null ? null : (SafeHandle?)_type.GetField("_bridgeOwnerProcess", Members)!.GetValue(_client);
        internal string Terminal => _type.GetProperty("Terminal", Members)!.GetValue(_client)!.ToString()!;
        internal uint Drains => (uint)_type.GetField("_closedOwnerTerminalDrains", Members)!.GetValue(_client)!;
        internal int GrantHistoryCount
        {
            get
            {
                object grants = _type.GetField("_grants", Members)!.GetValue(_client)!;
                return (int)grants.GetType().GetProperty("Count")!.GetValue(grants)!;
            }
        }
        internal uint StatusState
        {
            get
            {
                object status = _type.GetField("_bridge", Members)!.GetValue(_client)!;
                return Convert.ToUInt32(status.GetType().GetField("State", Members)!.GetValue(status));
            }
        }
        private object? Call(string name, params object?[] arguments)
        {
            try { return _type.GetMethod(name, Members)!.Invoke(_client, arguments); }
            catch (TargetInvocationException error) when (error.InnerException is not null)
            { System.Runtime.ExceptionServices.ExceptionDispatchInfo.Capture(error.InnerException).Throw(); throw; }
        }
        internal bool Read() => (bool)Call("TryReadBridgeStatus", [null])!;
        internal bool OwnerExitedZero => _writer.HasExited && _writer.ExitCode == 0;
        internal void Finish(string mode)
        {
            _writer.StandardInput.WriteLine(mode); _writer.StandardInput.Flush();
            Require(_writer.WaitForExit(5000) && _writer.ExitCode == (mode == "nonzero" ? 4 : 0), "Owned fixture writer did not complete its requested state.");
            Thread.Sleep(260); // Expire the unchanged ordinary250ms owner-query cache.
        }
        internal bool TerminalCannotAdvance()
        {
            bool verified = false;
            bool acknowledged = (bool)Call("AcknowledgeApplied", 1u, 1u, (Action)(() => verified = true))!;
            bool delivered = (bool)Call("TryPeekDelivery", [null])!;
            Type record = _type.Assembly.GetType("WinxCoop.RuntimeProbe.DiagnosticInputRecord", true)!;
            string published = Call("TryPublishCapture", Activator.CreateInstance(record))!.ToString()!;
            Type availability = _type.Assembly.GetType("WinxCoop.RuntimeProbe.DiagnosticInputAvailability", true)!;
            bool ready = (bool)Call("SetAvailability", Enum.ToObject(availability, 1), false)!;
            return !acknowledged && !verified && !delivered && !ready && published == "Terminal" &&
                U(192) == 0 && U(196) == 0 && U(148) == 0 && U(152) == 0 && U(132) == 0;
        }
        internal bool RejectConstructor()
        {
            try { using var unexpected = (IDisposable)NewClient(); return false; }
            catch (InvalidDataException) { return true; }
        }
        internal void DisposeClient() => (_client as IDisposable)?.Dispose();
        public void Dispose()
        {
            try { DisposeClient(); }
            finally
            {
                _view?.Dispose(); _mapping?.Dispose();
                if (!_writer.HasExited)
                {
                    _writer.StandardInput.Close();
                    if (!_writer.WaitForExit(2000)) { _writer.Kill(); _writer.WaitForExit(2000); }
                }
                _writer.Dispose();
            }
        }
    }

    private static int RunOwner(string[] args)
    {
        Require(OperatingSystem.IsWindows() && args.Length == 4 &&
            args[1].StartsWith("Local\\WinxCoop.InputTerminalCheck.", StringComparison.Ordinal), "Exact owned writer arguments required.");
        Require(uint.TryParse(args[2], out uint role) && role is 1 or 2 && args[3] is "0" or "1",
            "Exact owned writer role and mode required.");
        string name = args[1];
        Require(GetProcessTimes(GetCurrentProcess(), out long creation, out _, out _, out _), "Writer creation identity unavailable.");
        using var mapping = MemoryMappedFile.CreateNew(name, 4352, MemoryMappedFileAccess.ReadWrite);
        using var view = mapping.CreateViewAccessor(0, 4352, MemoryMappedFileAccess.ReadWrite);
        view.Write(0, 0x35494357u); view.Write(4, 5u); view.Write(8, 4352u); view.Write(12, 64u);
        view.Write(16, args[3] == "1" ? 45UL : 44UL); view.Write(24, (uint)Environment.ProcessId); view.Write(28, role);
        view.Write(32, (ulong)creation); view.Write(40, 11UL); view.Write(48, 22UL); view.Write(56, 1u); view.Write(60, 1u);
        view.Write(64, 1u); view.Write(68, 3u); view.Write(72, 33UL); view.Write(80, 1u); view.Write(84, 1u);
        view.Write(88, (ulong)Environment.TickCount64 + 2000); view.Write(96, 8u); view.Write(100, 0u);
        view.Write(104, 1u); view.Write(108, 0u); view.Write(112, (ulong)Environment.TickCount64); view.Write(120, 0UL);
        Thread.MemoryBarrier(); view.Write(64, 2u);
        Console.WriteLine(JsonSerializer.Serialize(new { mapping = name, pid = (uint)Environment.ProcessId, creation = (ulong)creation }));
        Console.Out.Flush();
        string? mode = Console.ReadLine();
        if (mode is null || mode == "ready") return 0;
        view.Write(64, 3u); Thread.MemoryBarrier();
        view.Write(68, mode == "ready-terminal" ? 3u : 5u); view.Write(104, 1u);
        view.Write(108, mode == "ready-terminal" ? 0u : mode == "unknown" ? 13u : 7u);
        view.Write(112, (ulong)Environment.TickCount64); view.Write(120, mode == "malformed" ? 1UL : 0UL);
        view.Write(200, role == 1 ? mode == "mixed" ? 6u : mode == "unknown" ? 13u : 7u : 0u);
        if (mode == "header") view.Write(16, 45UL);
        Thread.MemoryBarrier(); view.Write(64, mode == "odd" ? 5u : mode == "zero" ? 0u : 4u);
        return mode == "nonzero" ? 4 : 0;
    }

    private static uint HandleCount()
    {
        Require(GetProcessHandleCount(GetCurrentProcess(), out uint count), "Current fixture handle count unavailable."); return count;
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
    private static void Require(bool value, string reason) { if (!value) throw new InvalidDataException(reason); }
    private static void CheckThat(bool value, string name) { Require(value, name); Results.Add(new(name, true)); }
    [DllImport("kernel32.dll")] private static extern IntPtr GetCurrentProcess();
    [DllImport("kernel32.dll", SetLastError = true)] [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool GetProcessTimes(IntPtr process, out long creation, out long exit, out long kernel, out long user);
    [DllImport("kernel32.dll", SetLastError = true)] [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool GetProcessHandleCount(IntPtr process, out uint count);
}
