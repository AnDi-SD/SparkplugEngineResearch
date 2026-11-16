using WinxCoop.RuntimeProbe;
using System.ComponentModel;
using System.Diagnostics;
using System.Globalization;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using Microsoft.Win32.SafeHandles;

namespace WinxCoop.SessionHost;

// UI-owned native supervisor. It never terminates Outer or configures a Job.
// Final cleanup is an exact retained-process/final-native-report observation,
// independent of whether the managed worker itself succeeded.
internal sealed class SessionParent : IDisposable
{
    private readonly SessionConfiguration _config;
    private readonly string _outerPath, _outerReport, _workerReport;
    private readonly List<FileStream> _pins = [];
    private readonly FileStream _commands;
    private readonly object _commandGate = new();
    private System.Threading.Timer? _heartbeat;
    private Exception? _heartbeatFailure;
    private bool _heartbeatStopped;
    private readonly SessionStatusJournal.Reader _statuses;
    private Process? _outer;
    private uint _outerPid;
    private ulong _outerCreation, _sequence, _operation;
    private long _lastHeartbeat;
    private string _action = "Start";
    private bool _disposed, _finalRead, _outerImageConfirmed, _statusReadFailed;
    internal string CaseDirectory => _config.CaseDirectory;
    internal JsonObject? Status { get; private set; }
    internal bool OuterExited { get; private set; }
    internal bool CleanupConfirmed { get; private set; }
    internal bool NoOwnedTreeCreated { get; private set; }
    // Native cleanup is independent of this bounded advisory-stream drain.
    // A rejected stream remains a failure and cannot prevent safe disposal of
    // already confirmed native owners forever.
    internal bool FinalStatusObserved => _statusReadFailed || _statuses.FinalComplete;
    internal bool ReadyToDispose => OuterExited && CleanupConfirmed && FinalStatusObserved;
    internal string? Failure { get; private set; }
    internal long LastStatusAtMilliseconds { get; private set; }
    internal bool Host => _config.Host;

    private SessionParent(SessionConfiguration configuration, List<FileStream> pins)
    {
        _config = configuration; _pins.AddRange(pins);
        _outerPath = Path.Combine(CaseDirectory, "Outer.exe");
        _outerReport = Path.Combine(CaseDirectory, "outer.json");
        _statuses = new SessionStatusJournal.Reader(CaseDirectory, _config.Interactive);
        _workerReport = Path.Combine(CaseDirectory, "worker-report.json");
        _commands = new FileStream(_config.UiCommandPath, FileMode.CreateNew, FileAccess.Write,
            FileShare.ReadWrite | FileShare.Delete);
        Status = new JsonObject { ["phase"] = "starting", ["running"] = false,
            ["message"] = configuration.Host ? "Создаём лобби…" : "Подключаемся к серверу…" };
        try
        {
            // The first real command is visible before any worker is started.
            lock (_commandGate) WriteCommand();
            // A busy WinForms message loop must not starve the local owner lease.
            _heartbeat = new System.Threading.Timer(PublishHeartbeat, null, 250, 250);
        }
        catch { StopHeartbeat(); _commands.Dispose(); _statuses.Dispose(); throw; }
    }

    internal static SessionParent Start(SessionConfiguration configuration, RuntimeBundle bundle, CancellationToken cancellation = default)
    {
        cancellation.ThrowIfCancellationRequested();
        configuration.Validate();
        var game = new GameSelection(configuration.GameRoot, configuration.InitialLevel);
        string output = SessionConfiguration.AbsoluteAscii(configuration.CaseDirectory);
        Require(!Directory.Exists(output) && !File.Exists(output), "Session output must be fresh.");
        string? parent = Path.GetDirectoryName(output);
        Require(parent is not null && Directory.Exists(parent), "Session output requires its selected existing parent.");
        RuntimeBundle.Ordinary(parent!, true);
        Require(SamePath(configuration.BundleDirectory, bundle.DirectoryPath),
            "Session bundle differs from the qualified local deployment.");
        Directory.CreateDirectory(output); RuntimeBundle.Ordinary(output, true);
        string app = Path.Combine(output, "app"); Directory.CreateDirectory(app);
        var pins = new List<FileStream>();
        SessionParent? owner = null;
        try
        {
            foreach (RuntimeFile file in bundle.Manifest.Files)
            {
                cancellation.ThrowIfCancellationRequested();
                pins.Add(CopyPinned(bundle.File(file.Name), Path.Combine(app, file.Name), file.Sha256));
            }
            string manifestSource = Path.Combine(bundle.DirectoryPath, "WinxCoop.SessionBundle.json");
            using (var source = new FileStream(manifestSource, FileMode.Open, FileAccess.Read, FileShare.Read))
            {
                Require(source.Length is > 0 and <= 262144, "Bundle manifest exceeds its bound.");
                byte[] bytes = ReadComplete(source, checked((int)source.Length));
                RuntimeBundleManifest actual = JsonSerializer.Deserialize<RuntimeBundleManifest>(bytes) ??
                    throw new InvalidDataException("Bundle manifest disappeared.");
                Require(actual.Version == bundle.Manifest.Version && actual.Architecture == bundle.Manifest.Architecture &&
                    actual.SourceProfileSha256 == bundle.Manifest.SourceProfileSha256 && actual.Files is not null &&
                    actual.Files.Length == bundle.Manifest.Files.Length && actual.Files.Zip(bundle.Manifest.Files).All(pair => pair.First == pair.Second),
                    "Bundle manifest changed after qualification.");
                WriteFresh(Path.Combine(app, "WinxCoop.SessionBundle.json"), bytes);
            }
            pins.Add(CopyPinned(bundle.File("XpGameProbe.exe"), Path.Combine(output, "Outer.exe"), bundle.Hash("XpGameProbe.exe")));
            using Process self = Process.GetCurrentProcess();
            string nonce = Convert.ToHexString(RandomNumberGenerator.GetBytes(16));
            SessionConfiguration worker = configuration with
            {
                Worker = true, BundleDirectory = app, CaseDirectory = output,
                JobName = @"Local\WinxCoop.E7." + Guid.NewGuid().ToString("N"),
                UiOwnerPid = checked((uint)self.Id), UiOwnerCreationFileTime = Creation(self.SafeHandle),
                UiCapability = nonce, UiCommandPath = Path.Combine(output, "ui-commands.jsonl")
            };
            worker.Validate();
            string executable = Path.Combine(app, "WinxCoop.SessionHost.exe");
            WriteFresh(executable + ".json", JsonSerializer.SerializeToUtf8Bytes(worker, SessionConfiguration.Json));
            string arguments = "--game " + Quote(executable) + " --media " + Quote(game.Media) +
                " --report " + Quote(Path.Combine(output, "outer.json")) + " --observe-ms " +
                new SessionLifecycleBudget(worker.Seconds, 0, worker.Interactive).NativeSupervisorMilliseconds.ToString(CultureInfo.InvariantCulture) +
                " --process-local-media --job-name " + Quote(worker.JobName) +
                (worker.Interactive ? " --interactive-session 1" : "");
            Require(arguments.Length is > 0 and <= 1024 && arguments.All(character => character is >= ' ' and <= '~'),
                "Native supervisor arguments exceed their ASCII bound.");
            WriteFresh(Path.Combine(output, "Outer.exe.args"), Encoding.ASCII.GetBytes(arguments));
            owner = new(worker, pins); pins.Clear();
            cancellation.ThrowIfCancellationRequested();
            owner._outer = Process.Start(new ProcessStartInfo
            {
                FileName = owner._outerPath, WorkingDirectory = output, UseShellExecute = false,
                CreateNoWindow = true, WindowStyle = ProcessWindowStyle.Hidden
            }) ?? throw new InvalidDataException("Native session supervisor did not start.");
            owner._outerPid = checked((uint)owner._outer.Id);
            owner._outerCreation = Creation(owner._outer.SafeHandle);
            owner.VerifyOuter();
            return owner;
        }
        catch (Exception error)
        {
            if (owner?._outer is not null)
            {
                // The retained native supervisor must remain observable even
                // if admission after Process.Start fails. Never lose or kill it.
                owner.SetFailure(error); return owner;
            }
            owner?.Dispose(); foreach (FileStream pin in pins) pin.Dispose(); throw;
        }
    }

    internal void Command(string action)
    {
        lock (_commandGate)
        {
            Require(!_disposed && !OuterExited && action is "Start" or "Reload" or "Save" or "Load" or "Stop",
                "Session command is unsupported or the native supervisor has exited.");
            Require(_config.Host || action == "Stop", "Only the server controls game start and progression.");
            if (_action == "Stop") return;
            _action = action; _operation = checked(_operation + 1); WriteCommand();
        }
    }

    internal void Pump()
    {
        if (_disposed) throw new ObjectDisposedException(GetType().FullName);
        Exception? heartbeatFailure;
        lock (_commandGate) heartbeatFailure = _heartbeatFailure;
        if (heartbeatFailure is not null) SetFailure(heartbeatFailure);
        if (_outer is null) return;
        try
        {
            OuterExited = _outer.HasExited;
            if (OuterExited) StopHeartbeat();
            if (_outerPid == 0) _outerPid = checked((uint)_outer.Id);
            if (_outerCreation == 0) _outerCreation = Creation(_outer.SafeHandle);
            VerifyOuter();
            if (!_statusReadFailed) try
            {
                JsonObject? status = _statuses.ReadLatest(final: OuterExited);
                if (status is not null)
                {
                    if (!_finalRead) Status = status;
                    LastStatusAtMilliseconds = Environment.TickCount64;
                    if (status["error"] is JsonValue error && error.TryGetValue<string>(out string? message) && !string.IsNullOrWhiteSpace(message))
                        SetFailure(new InvalidDataException(message));
                }
            }
            catch (Exception error) { _statusReadFailed = true; SetFailure(error); }
            // Optional UI status must not hide the independently authoritative
            // final native Job receipt. Its failure remains latched above.
            if (OuterExited && !_finalRead) ReadFinal();
        }
        catch (Exception error) { SetFailure(error); }
    }

    private void ReadFinal()
    {
        JsonObject? report = ReadObject(_outerReport, 65536, final: true);
        Require(report is not null, "Native supervisor exited without its final cleanup report.");
        Require(U(report!, "pid") == _outerPid && S(report!, "jobName") == _config.JobName &&
            SamePath(S(report!, "game"), Path.Combine(_config.BundleDirectory, "WinxCoop.SessionHost.exe")) &&
            SamePath(S(report!, "media"), Path.Combine(_config.GameRoot, "Media")) &&
            B(report!, "processLocalMedia") && S(report!, "registryPolicy") == "not-opened",
            "Native supervisor report identity differs from this retained session.");
        JsonObject job = report!["job"]?.AsObject() ?? throw new InvalidDataException("Missing native Job report.");
        JsonObject child = report["child"]?.AsObject() ?? throw new InvalidDataException("Missing native child report.");
        if (B(job, "created"))
        {
            JsonObject named = report["namedJob"]?.AsObject() ?? throw new InvalidDataException("Missing named Job receipt.");
            Require(B(named, "requested") && B(named, "freshCreated") && S(named, "name") == _config.JobName &&
                !B(named, "existingNameRejected") && B(job, "activeProcessesZeroVerified") &&
                B(job, "ownedTreeStopped") && B(job, "handleClosed") && U(job, "lastActiveProcesses") == 0,
                "Native session tree cleanup has not been confirmed.");
            CleanupConfirmed = true;
        }
        else
        {
            Require(!B(child, "created"), "A child was created without a confirmed owned Job.");
            NoOwnedTreeCreated = true; CleanupConfirmed = true;
            SetFailure(new InvalidDataException("Native supervisor failed before creating its owned session tree."));
        }
        if (!B(report, "probeCompleted") || S(report, "failure") != "none" || _outer!.ExitCode != 0)
            SetFailure(new InvalidDataException("Native supervisor did not complete successfully: " + S(report, "failure")));
        JsonObject? worker = ReadObject(_workerReport, 2097152, final: true);
        if (worker is null || !B(worker, "success"))
            SetFailure(new InvalidDataException(worker is null ? "Session worker has no final report." :
                "Session worker failed: " + (worker["error"]?.ToString() ?? "no detail")));
        _finalRead = true;
        SetFinalStatus();
    }

    private void SetFinalStatus()
    {
        Status = new JsonObject
        {
            ["phase"] = Failure is null ? "completed" : "failed", ["running"] = false,
            ["canStart"] = false, ["canReload"] = false, ["canSave"] = false, ["canLoad"] = false,
            ["message"] = Failure is null ? "Сеанс завершён." : "Сеанс завершён с ошибкой. Подробности сохранены в папке сеанса.",
            ["error"] = Failure
        };
    }

    private void PublishHeartbeat(object? state)
    {
        lock (_commandGate)
        {
            if (_heartbeatStopped || _disposed || _heartbeatFailure is not null) return;
            try
            {
                if (Environment.TickCount64 - _lastHeartbeat >= 250) WriteCommand();
            }
            catch (Exception error) { _heartbeatFailure = error; }
        }
    }

    private void StopHeartbeat()
    {
        System.Threading.Timer? heartbeat;
        lock (_commandGate)
        {
            _heartbeatStopped = true; heartbeat = _heartbeat; _heartbeat = null;
        }
        // Never wait while holding the writer gate: an already queued callback
        // must acquire it, observe the stop, and return before the file closes.
        if (heartbeat is null) return;
        using var drained = new ManualResetEvent(false);
        if (heartbeat.Dispose(drained)) drained.WaitOne();
    }

    // Constructor, UI commands and the timer all hold _commandGate here.
    private void WriteCommand()
    {
        long now = Environment.TickCount64;
        byte[] bytes = Encoding.UTF8.GetBytes(JsonSerializer.Serialize(new
        {
            Version = 1, CapabilityNonce = _config.UiCapability, Sequence = checked(++_sequence),
            Operation = _operation, Action = _action, IssuedAtMilliseconds = now
        }) + "\n");
        if (_config.Interactive)
            Require(_commands.Length <= InteractiveSessionLimits.CommandJournalBytes - bytes.Length,
                "Interactive UI command journal exceeded its32MiB bound.");
        _commands.Write(bytes, 0, bytes.Length); _commands.Flush(); _lastHeartbeat = now;
    }

    private void VerifyOuter()
    {
        Require(_outer is not null && !_outer.SafeHandle.IsInvalid && _outerPid != 0 && _outerCreation != 0 &&
            GetProcessId(_outer.SafeHandle) == _outerPid && Creation(_outer.SafeHandle) == _outerCreation,
            "Retained native supervisor identity changed.");
        if (OuterExited && _outerImageConfirmed) return;
        var path = new StringBuilder(1024); uint length = 1024;
        if (!QueryFullProcessImageName(_outer!.SafeHandle, 0, path, ref length))
            throw new Win32Exception(Marshal.GetLastWin32Error(), "Query retained native supervisor image");
        Require(SamePath(path.ToString(), _outerPath), "Retained native supervisor image differs from its pinned copy.");
        _outerImageConfirmed = true;
    }

    private void SetFailure(Exception error)
    {
        if (Failure is not null) return;
        Failure = error.ToString();
        if (_finalRead) SetFinalStatus();
        try { WriteFresh(Path.Combine(CaseDirectory, "ui-error.txt"), Encoding.UTF8.GetBytes(Failure + "\n")); }
        catch (Exception) { /* Do not hide or replace the original failure. */ }
    }

    private static JsonObject? ReadObject(string path, int maximumBytes, bool final)
    {
        try
        {
            if (!File.Exists(path)) return null;
            RuntimeBundle.Ordinary(path, false);
            using var file = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
            if (file.Length == 0 && !final) return null;
            Require(file.Length is > 0 && file.Length <= maximumBytes, "Session report exceeds its bounded size.");
            byte[] bytes = ReadComplete(file, checked((int)file.Length));
            return JsonNode.Parse(bytes, documentOptions: new JsonDocumentOptions { MaxDepth = 32 })?.AsObject() ??
                throw new InvalidDataException("Session report is not an object.");
        }
        catch (FileNotFoundException) when (!final) { return null; }
        catch (IOException) when (!final) { return null; }
        catch (JsonException) when (!final) { return null; }
    }

    private static FileStream CopyPinned(string source, string destination, string expected)
    {
        Require(destination.Length < 260 && !File.Exists(destination) && !Directory.Exists(destination), "Fresh bounded session file required.");
        RuntimeBundle.Ordinary(source, false);
        using (var input = new FileStream(source, FileMode.Open, FileAccess.Read, FileShare.Read))
        using (var output = new FileStream(destination, FileMode.CreateNew, FileAccess.Write, FileShare.Read))
        { input.CopyTo(output, 65536); output.Flush(flushToDisk: true); }
        var pin = new FileStream(destination, FileMode.Open, FileAccess.Read, FileShare.Read);
        try
        {
            using var sha = SHA256.Create();
            Require(Convert.ToHexString(sha.ComputeHash(pin)).Equals(expected, StringComparison.OrdinalIgnoreCase), "Copied runtime file hash differs.");
            pin.Position = 0; return pin;
        }
        catch { pin.Dispose(); throw; }
    }
    private static void WriteFresh(string path, byte[] bytes)
    {
        using var file = new FileStream(path, FileMode.CreateNew, FileAccess.Write, FileShare.Read);
        file.Write(bytes, 0, bytes.Length); file.Flush(flushToDisk: true);
    }
    private static byte[] ReadComplete(FileStream file, int length)
    {
        byte[] bytes = new byte[length]; int offset = 0;
        while (offset < length)
        {
            int read = file.Read(bytes, offset, length - offset);
            if (read == 0) throw new EndOfStreamException("Session file changed during its bounded read.");
            offset = checked(offset + read);
        }
        return bytes;
    }
    private static string Quote(string value)
    { Require(value.All(character => character is >= ' ' and <= '~' && character != '"'), "Plain ASCII argument required."); return '"' + value + '"'; }
    private static bool SamePath(string left, string right) =>
        Path.TrimEndingDirectorySeparator(Path.GetFullPath(left)).Equals(Path.TrimEndingDirectorySeparator(Path.GetFullPath(right)), StringComparison.OrdinalIgnoreCase);
    private static bool B(JsonObject value, string name) => value[name]?.GetValue<bool>() ??
        throw new InvalidDataException("Missing boolean native report field: " + name);
    private static uint U(JsonObject value, string name) => value[name]?.GetValue<uint>() ?? throw new InvalidDataException("Missing numeric native report field: " + name);
    private static string S(JsonObject value, string name) => value[name]?.GetValue<string>() ?? throw new InvalidDataException("Missing native report field: " + name);
    private static ulong Creation(SafeProcessHandle process)
    {
        if (!GetProcessTimes(process, out NativeFileTime created, out _, out _, out _))
            throw new Win32Exception(Marshal.GetLastWin32Error(), "Query retained process creation");
        return ((ulong)created.High << 32) | created.Low;
    }
    private static void Require(bool condition, string message) => SessionConfiguration.Require(condition, message);

    public void Dispose()
    {
        lock (_commandGate)
        {
            if (_disposed) return;
            Require(_outer is null || ReadyToDispose,
                "Wait for the exact native cleanup and bounded final status observation before disposing the parent.");
            _disposed = true;
        }
        StopHeartbeat();
        _statuses.Dispose(); _commands.Dispose(); _outer?.Dispose(); foreach (FileStream pin in _pins) pin.Dispose();
        _pins.Clear();
    }
    [StructLayout(LayoutKind.Sequential)] private struct NativeFileTime { internal uint Low, High; }
    [DllImport("kernel32.dll", SetLastError = true)] [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool GetProcessTimes(SafeProcessHandle process, out NativeFileTime creation, out NativeFileTime exit, out NativeFileTime kernel, out NativeFileTime user);
    [DllImport("kernel32.dll", SetLastError = true)] private static extern uint GetProcessId(SafeProcessHandle process);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)] [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool QueryFullProcessImageName(SafeProcessHandle process, uint flags, StringBuilder imageName, ref uint size);
}
