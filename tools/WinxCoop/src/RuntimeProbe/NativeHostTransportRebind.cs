using System.ComponentModel;
using System.Diagnostics;
using System.Globalization;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text.Json;
using System.Text.Json.Serialization;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

// Diagnostic local orchestration only. These records are not network packets.
internal sealed record HostTransportRebindPermit(string MailboxPath, uint OwnerPid, ulong OwnerCreationFileTime,
    string CapabilityNonce, ulong OriginalSessionId, ulong ScenarioId, uint LevelEpoch, uint BodyGeneration,
    ulong PreviousInputConnectionToken, ulong PreviousWorldConnectionToken, long ExpiresAtMilliseconds);
internal sealed record HostTransportRebindRequest(uint Schema, uint RequestOrdinal, string CapabilityNonce,
    uint TargetActorPid, ulong TargetActorCreationFileTime, ulong PreviousSessionId,
    DiagnosticInputIdentity NextInput, WorldStateIdentity NextWorld, ulong NextInputConnectionToken,
    ulong NextWorldConnectionToken, long IssuedAtMilliseconds);
internal sealed record HostWorldTransportRebindWitness(WorldStateIdentity Previous, uint DebugStop, SharedProfileScene Scene,
    uint PrimaryInstance, uint PrimaryRoot, uint SecondaryInstance, uint SecondaryRoot, uint Parent);
internal sealed record HostInputTransportRebindWitness(DiagnosticInputIdentity Previous, uint DebugStop,
    uint Instance, uint Root, uint Command, uint Scene);

/// <summary>
/// One parent-pinned local request replaces terminal transport objects at an
/// original WorldRET. Existing native bodies/scene/readers remain owned by their
/// original components. No worker, native allocation or native call is added.
/// </summary>
internal sealed class NativeHostTransportRebind : IDisposable
{
    private const int MaximumRequestBytes = 8192;
    private readonly HostTransportRebindPermit _permit;
    private readonly DiagnosticInputIdentity _initialInput;
    private readonly WorldStateIdentity _initialWorld;
    private readonly DebuggerHost _host;
    private readonly IntPtr _nativeProcess;
    private readonly uint _nativePid, _nativeThread;
    private readonly Action<object> _log;
    private readonly int _ownerThread = Environment.CurrentManagedThreadId;
    private readonly Process _coordinator, _oldInputOwner, _oldWorldOwner;
    private readonly ulong _actorCreation;
    private readonly byte[] _capability;
    private Process? _newInputOwner, _newWorldOwner;
    private HostTransportRebindRequest? _request;
    private string? _requestHash;
    private long _lastPoll;
    private bool _consumed, _committed, _inputReady, _succeeded, _retired, _failed;
    private static readonly JsonSerializerOptions Json = new()
    { MaxDepth = 8, UnmappedMemberHandling = JsonUnmappedMemberHandling.Disallow };

    internal NativeHostTransportRebind(ProbeConfiguration config, DebuggerHost host, Action<object> log)
    {
        _permit = config.DiagnosticHostRebind ?? throw new InvalidDataException("Missing diagnostic Host rebind permit.");
        _initialInput = config.DiagnosticHostInput ?? throw new InvalidDataException("Missing original Host Input identity.");
        _initialWorld = config.DiagnosticWorldState ?? throw new InvalidDataException("Missing original Host World identity.");
        _host = host; _log = log;
        _nativeProcess = host.ProcessHandle; _nativePid = host.ProcessPid; _nativeThread = host.MainThreadId;
        EnsureStop(host.MainThreadId);
        long now = DiagnosticInputClient.LocalNowMilliseconds();
        Require(_initialInput.Role == DiagnosticInputRole.Host && _initialWorld.Role == WorldStateRole.Host &&
            config.DiagnosticGuestInput is null && _permit.OriginalSessionId != 0 &&
            _permit.OriginalSessionId == _initialInput.SessionId && _permit.OriginalSessionId == _initialWorld.SessionId &&
            _permit.ScenarioId == _initialInput.ScenarioId && _permit.ScenarioId == _initialWorld.ScenarioId &&
            _permit.LevelEpoch == _initialInput.LevelEpoch && _permit.LevelEpoch == _initialWorld.LevelEpoch &&
            _permit.BodyGeneration == _initialInput.BodyGeneration && _permit.BodyGeneration == _initialWorld.Generation &&
            _permit.PreviousInputConnectionToken != 0 && _permit.PreviousWorldConnectionToken != 0 &&
            _permit.PreviousInputConnectionToken != _permit.PreviousWorldConnectionToken &&
            _permit.ExpiresAtMilliseconds > now && _permit.ExpiresAtMilliseconds - now <= 600000,
            "Host rebind permit identity/tokens/deadline differ from its initial Host fixtures.");
        _capability = Capability(_permit.CapabilityNonce);
        ValidateMailboxPath(_permit.MailboxPath);
        Require(!File.Exists(_permit.MailboxPath) && !Directory.Exists(_permit.MailboxPath),
            "Host rebind requires a fresh one-command mailbox path at startup.");
        using Process actor = Process.GetCurrentProcess(); _actorCreation = Creation(actor);
        Require(_permit.OwnerPid != checked((uint)actor.Id) && Parent(checked((uint)actor.Id)) == _permit.OwnerPid,
            "Host rebind permit owner is not the actual Actor parent coordinator.");
        _coordinator = OpenLive(_permit.OwnerPid, _permit.OwnerCreationFileTime);
        try
        {
            _oldInputOwner = OpenOwnedBridge(_initialInput.OwnerPid, _initialInput.OwnerCreationFileTime);
            try { _oldWorldOwner = OpenOwnedBridge(_initialWorld.OwnerPid, _initialWorld.OwnerCreationFileTime); }
            catch { _oldInputOwner.Dispose(); throw; }
        }
        catch { _coordinator.Dispose(); throw; }
        _log(new { @event = "host-transport-rebind-permitted", requestOrdinal = 1, maximumAttempts = 1,
            actorPid = Environment.ProcessId, actorCreationFileTimeDecimal = Number(_actorCreation),
            coordinatorPid = _permit.OwnerPid, coordinatorCreationFileTimeDecimal = Number(_permit.OwnerCreationFileTime),
            previousSessionIdDecimal = Number(_permit.OriginalSessionId), mailboxPath = _permit.MailboxPath,
            capabilityLogged = false });
    }

    internal bool Succeeded => _succeeded && !_failed;
    internal object Summary => new { permitted = true, consumed = _consumed, committed = _committed,
        inputReadyAfterFreshNativeWorld = _inputReady, newConnectionNativeInputApplied = _succeeded,
        succeeded = Succeeded, retired = _retired, failed = _failed, requestSha256 = _requestHash,
        requestOrdinal = _consumed ? 1 : 0, maximumAttempts = 1, connectionAttempt = _committed ? 2 : 1,
        newSessionIdDecimal = _request is null ? null : Number(_request.NextWorld.SessionId),
        nativeAllocations = 0, nativeCallsAdded = 0, nativeOwnersReconstructed = false };

    internal void PollAtWorld(uint tid, ref Win32Native.X86Context context, NativeCallDispatcher calls,
        WorldStateSession world, NativeTemplateProbe template)
    {
        if (_retired || _failed) return;
        EnsureStop(tid);
        if (calls.Pending || context.Eip != NativeCallDispatcher.WorldReturn || !template.WorldStateBodyReady || world.Retired) return;
        try
        {
            Require(!_coordinator.HasExited && Creation(_coordinator) == _permit.OwnerCreationFileTime,
                "Host rebind parent coordinator ended or changed identity.");
            if (_committed)
            {
                if (!_inputReady && template.AllowReboundInputAtWorld(tid, ref context, world.CurrentConnectionNativeApplied))
                {
                    _inputReady = true;
                    _log(new { @event = "host-transport-rebind-input-ready", requestOrdinal = 1, connectionAttempt = 2,
                        requestSha256 = _requestHash, newSessionIdDecimal = Number(_request!.NextWorld.SessionId),
                        freshNativeWorldApplied = true, oldNativeAckReused = false });
                }
                if (!_succeeded && _inputReady && world.CurrentConnectionNativeApplied && template.ReboundInputNativeApplied)
                {
                    _succeeded = true;
                    _log(new { @event = "host-transport-rebind-native-qualified", requestOrdinal = 1, connectionAttempt = 2,
                        requestSha256 = _requestHash, newSessionIdDecimal = Number(_request!.NextWorld.SessionId),
                        newNativeWorldAndInputApplied = true, freshNativeMovementReadback = true, nativeOwnersSame = true });
                }
                return;
            }
            long now = DiagnosticInputClient.LocalNowMilliseconds();
            Require(now <= _permit.ExpiresAtMilliseconds, "One-command Host rebind deadline expired.");
            if (now - _lastPoll < 50) return; _lastPoll = now;
            if (!world.HostTransportDetachedForReconnect || !template.NetworkInputDetachedForReconnect) return;
            // These retained process handles prove exit even if a numeric PID
            // has since been reused. New live objects never reopen old clients.
            if (!_oldInputOwner.HasExited || !_oldWorldOwner.HasExited) return;
            byte[]? bytes = ReadRequest(); if (bytes is null) return;
            Require(!_consumed, "Host rebind request was already consumed.");
            HostTransportRebindRequest request = JsonSerializer.Deserialize<HostTransportRebindRequest>(bytes, Json)
                ?? throw new InvalidDataException("Missing complete Host rebind request object.");
            ValidateRequest(request, now);
            _consumed = true; _request = request; _requestHash = Convert.ToHexString(SHA256.HashData(bytes));
            var worldWitness = world.PrepareHostTransportRebindAtWorld(tid, ref context, template, calls);
            var inputWitness = template.PrepareHostInputRebindAtWorld(tid, ref context);
            Require(worldWitness.Scene.Scene == inputWitness.Scene && worldWitness.SecondaryInstance == inputWitness.Instance &&
                worldWitness.SecondaryRoot == inputWitness.Root, "Host World/Input rebind witnesses identify different retained native bodies.");
            _newInputOwner = OpenOwnedBridge(request.NextInput.OwnerPid, request.NextInput.OwnerCreationFileTime);
            _newWorldOwner = OpenOwnedBridge(request.NextWorld.OwnerPid, request.NextWorld.OwnerCreationFileTime);
            DiagnosticInputClient? input = null; WorldStateClient? nextWorld = null;
            try
            {
                // Both unchanged constructors validate owners/headers and claim
                // fresh Waiting storage before either current slot is swapped.
                input = new(request.NextInput, _log); nextWorld = new(request.NextWorld, _log);
                Require(input.TryReadBridgeStatus(out var inputStatus) && inputStatus.ConnectionToken == request.NextInputConnectionToken &&
                    nextWorld.TryReadBridgeStatus(out var worldStatus) && worldStatus.ConnectionToken == request.NextWorldConnectionToken,
                    "Fresh Host rebind bridge tokens/control do not match the parent request.");
                template.CommitHostInputRebindAtWorld(tid, ref context, request.NextInput, input, inputWitness);
                input = null; // Current template now owns this fresh endpoint.
                world.CommitHostTransportRebindAtWorld(tid, ref context, request.NextWorld, nextWorld, worldWitness, template, calls);
                nextWorld = null; _committed = true;
            }
            finally { input?.Dispose(); nextWorld?.Dispose(); }
            _log(new { @event = "host-transport-rebind-committed", requestOrdinal = 1, connectionAttempt = 2,
                requestSha256 = _requestHash, previousSessionIdDecimal = Number(_permit.OriginalSessionId),
                newSessionIdDecimal = Number(request.NextWorld.SessionId), actorPid = Environment.ProcessId,
                actorCreationFileTimeDecimal = Number(_actorCreation), inputMapping = request.NextInput.MappingName,
                worldMapping = request.NextWorld.MappingName, nativeOwnersSame = true, inputReady = false,
                currentNativeWorldApplied = false, worldRevisionContinues = true });
        }
        catch (Exception error)
        {
            _failed = true;
            _log(new { @event = "host-transport-rebind-failed", consumed = _consumed, committed = _committed,
                requestSha256 = _requestHash, error = error.ToString(), oldTransportNeverReopened = true });
            throw;
        }
    }

    private void ValidateRequest(HostTransportRebindRequest request, long now)
    {
        Require(request.Schema == 1 && request.RequestOrdinal == 1 &&
            CryptographicOperations.FixedTimeEquals(Capability(request.CapabilityNonce), _capability) &&
            request.TargetActorPid == checked((uint)Environment.ProcessId) && request.TargetActorCreationFileTime == _actorCreation &&
            request.PreviousSessionId == _permit.OriginalSessionId && request.IssuedAtMilliseconds > 0 &&
            request.IssuedAtMilliseconds <= now && now - request.IssuedAtMilliseconds <= 30000,
            "Host rebind request schema/nonce/ordinal/target/original session/age differs from its one-use permit.");
        var input = request.NextInput; var world = request.NextWorld;
        Require(input.Role == DiagnosticInputRole.Host && world.Role == WorldStateRole.Host &&
            input.SessionId != 0 && input.SessionId == world.SessionId && input.SessionId != _permit.OriginalSessionId &&
            input.ScenarioId == _permit.ScenarioId && world.ScenarioId == _permit.ScenarioId &&
            input.LevelEpoch == _permit.LevelEpoch && world.LevelEpoch == _permit.LevelEpoch &&
            input.BodyGeneration == _permit.BodyGeneration && world.Generation == _permit.BodyGeneration &&
            input.OwnerPid != world.OwnerPid && input.OwnerPid != _initialInput.OwnerPid && input.OwnerPid != _initialWorld.OwnerPid &&
            world.OwnerPid != _initialInput.OwnerPid && world.OwnerPid != _initialWorld.OwnerPid &&
            input.Nonce != 0 && world.Nonce != 0 && input.Nonce != world.Nonce &&
            input.Nonce != _initialInput.Nonce && input.Nonce != _initialWorld.Nonce &&
            world.Nonce != _initialInput.Nonce && world.Nonce != _initialWorld.Nonce &&
            FreshMapping(input.MappingName, @"Local\WinxCoop.DiagnosticInput5.") &&
            FreshMapping(world.MappingName, @"Local\WinxCoop.WorldState1.") &&
            request.NextInputConnectionToken != 0 && request.NextWorldConnectionToken != 0 &&
            request.NextInputConnectionToken != request.NextWorldConnectionToken &&
            request.NextInputConnectionToken != _permit.PreviousInputConnectionToken && request.NextInputConnectionToken != _permit.PreviousWorldConnectionToken &&
            request.NextWorldConnectionToken != _permit.PreviousInputConnectionToken && request.NextWorldConnectionToken != _permit.PreviousWorldConnectionToken,
            "Host rebind endpoint session/scene/body/mapping/nonce/token freshness is invalid.");
    }
    private bool FreshMapping(string? value, string prefix) => value is not null && value.StartsWith(prefix, StringComparison.Ordinal) &&
        value.Length > prefix.Length && value.Length <= 160 && value.Skip(prefix.Length).All(c => char.IsAsciiLetterOrDigit(c) || c is '.' or '-' or '_') &&
        value != _initialInput.MappingName && value != _initialWorld.MappingName;
    private byte[]? ReadRequest()
    {
        ValidateMailboxPath(_permit.MailboxPath);
        try
        {
            if (!File.Exists(_permit.MailboxPath)) return null;
            Require((File.GetAttributes(_permit.MailboxPath) & FileAttributes.ReparsePoint) == 0,
                "Host rebind mailbox is a reparse file.");
            using var file = new FileStream(_permit.MailboxPath, FileMode.Open, FileAccess.Read, FileShare.Read);
            Require(file.Length is > 0 and <= MaximumRequestBytes, "Host rebind mailbox exceeds its bounded request length.");
            byte[] bytes = new byte[checked((int)file.Length)]; file.ReadExactly(bytes);
            Require(file.ReadByte() == -1, "Host rebind mailbox changed during its immutable read."); return bytes;
        }
        catch (FileNotFoundException) { return null; }
        catch (IOException error) when (unchecked((uint)error.HResult) == 0x80070020) { return null; }
    }
    private static void ValidateMailboxPath(string path)
    {
        Require(Path.IsPathFullyQualified(path) && path.All(c => c <= 127) && Path.GetFullPath(path) == path &&
            Path.GetFileName(path).Length != 0, "Host rebind mailbox requires a normalized absolute ASCII path.");
        string? parent = Path.GetDirectoryName(path);
        for (string? current = parent; current is not null; current = Path.GetDirectoryName(Path.TrimEndingDirectorySeparator(current)))
        {
            Require(Directory.Exists(current) && (File.GetAttributes(current) & FileAttributes.ReparsePoint) == 0,
                "Host rebind mailbox parent is missing or a reparse point.");
            if (Path.GetPathRoot(current) == current) break;
        }
    }
    private Process OpenOwnedBridge(uint pid, ulong creation)
    {
        Process process = OpenLive(pid, creation);
        try { Require(Parent(pid) == _permit.OwnerPid && pid != checked((uint)Environment.ProcessId),
            "Rebind bridge is not a direct child of the pinned coordinator."); return process; }
        catch { process.Dispose(); throw; }
    }
    private static Process OpenLive(uint pid, ulong creation)
    {
        Require(pid != 0 && creation != 0, "Rebind process identity is zero.");
        Process process = Process.GetProcessById(checked((int)pid));
        try { _ = process.Handle; Require(!process.HasExited && Creation(process) == creation,
            "Rebind process PID/creation/liveness differs."); return process; }
        catch { process.Dispose(); throw; }
    }
    private static ulong Creation(Process process)
    {
        if (!GetProcessTimes(process.Handle, out NativeFileTime creation, out _, out _, out _))
            throw new Win32Exception(Marshal.GetLastWin32Error(), "Cannot verify rebind process creation FILETIME.");
        return ((ulong)creation.High << 32) | creation.Low;
    }
    private static uint Parent(uint pid)
    {
        IntPtr snapshot = CreateToolhelp32Snapshot(2, 0);
        if (snapshot == new IntPtr(-1)) throw new Win32Exception(Marshal.GetLastWin32Error(), "Cannot snapshot rebind parent identity.");
        try
        {
            ProcessEntry entry = new() { Size = checked((uint)Marshal.SizeOf<ProcessEntry>()), Image = "" };
            if (!Process32First(snapshot, ref entry)) throw new Win32Exception(Marshal.GetLastWin32Error(), "Cannot read rebind process snapshot.");
            do { if (entry.ProcessId == pid) return entry.ParentProcessId; } while (Process32Next(snapshot, ref entry));
            throw new InvalidDataException("Rebind process disappeared before parent admission.");
        }
        finally { _ = CloseHandle(snapshot); }
    }
    private static byte[] Capability(string value)
    { Require(value is { Length: 32 } && value.All(Uri.IsHexDigit), "Host rebind capability must contain128 cryptographic bits as32 hex characters."); return Convert.FromHexString(value); }
    private void EnsureStop(uint tid) => Require(_host.IsAtDebugStop && tid == _host.MainThreadId &&
        tid == _nativeThread && _host.ProcessPid == _nativePid && _nativePid != 0 &&
        _host.ProcessHandle == _nativeProcess && _nativeProcess != IntPtr.Zero &&
        Environment.CurrentManagedThreadId == _ownerThread, "Host rebind requires its pinned debugger-owner process/thread at a native stop.");
    internal void RetireCached()
    {
        if (_retired) return;
        Require(Environment.CurrentManagedThreadId == _ownerThread, "Host rebind retirement moved threads.");
        _retired = true; _coordinator.Dispose(); _oldInputOwner.Dispose(); _oldWorldOwner.Dispose();
        _newInputOwner?.Dispose(); _newWorldOwner?.Dispose();
    }
    public void Dispose() => RetireCached();
    private static string Number(ulong value) => value.ToString(CultureInfo.InvariantCulture);
    private static void Require(bool value, string message) { if (!value) throw new InvalidDataException(message); }
    [StructLayout(LayoutKind.Sequential)] private struct NativeFileTime { internal uint Low, High; }
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct ProcessEntry
    {
        internal uint Size, Usage, ProcessId; internal UIntPtr DefaultHeapId;
        internal uint ModuleId, Threads, ParentProcessId; internal int BasePriority; internal uint Flags;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)] internal string Image;
    }
    [DllImport("kernel32.dll", SetLastError = true)] [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool GetProcessTimes(IntPtr process, out NativeFileTime creation, out NativeFileTime exit, out NativeFileTime kernel, out NativeFileTime user);
    [DllImport("kernel32.dll", SetLastError = true)] private static extern IntPtr CreateToolhelp32Snapshot(uint flags, uint pid);
    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode, EntryPoint = "Process32FirstW")] [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool Process32First(IntPtr snapshot, ref ProcessEntry entry);
    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode, EntryPoint = "Process32NextW")] [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool Process32Next(IntPtr snapshot, ref ProcessEntry entry);
    [DllImport("kernel32.dll")] [return: MarshalAs(UnmanagedType.Bool)] private static extern bool CloseHandle(IntPtr handle);
}
