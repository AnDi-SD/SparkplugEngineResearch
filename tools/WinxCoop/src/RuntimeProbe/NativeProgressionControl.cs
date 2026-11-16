using System.Diagnostics;
using System.Security.Cryptography;
using System.Text.Json;
using System.Text.Json.Serialization;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

// Local parent-to-Actor control only. These records are not the network codec.
internal sealed record ProgressionControlPermit(string CommandPath, uint OwnerPid, ulong OwnerCreationFileTime,
    string CapabilityNonce, uint NextLevel = 28);
internal sealed record ProgressionActorCommand(uint Version, string CapabilityNonce, uint LevelEpoch,
    uint Generation, ulong Sequence, ulong Operation, string Action, long IssuedAtMilliseconds);

internal sealed class NativeProgressionControl : IDisposable
{
    private readonly ProgressionControlPermit _permit;
    private readonly WorldStateIdentity _identity;
    private readonly Process _owner;
    private readonly DebuggerHost _host;
    private readonly Action<object> _log;
    private readonly byte[] _capability;
    private readonly bool _saveAllowed;
    private readonly long _maximumCommandJournalBytes;
    private readonly bool _overlayAllowed;
    private readonly bool _reloadPreparationAllowed;
    private readonly uint _sceneLevel;
    private readonly CampaignSceneProfile? _campaign;
    private readonly int _thread = Environment.CurrentManagedThreadId;
    private ProgressionActorCommand? _command;
    private string _commandHash = "";
    private long _lastPoll;
    private ulong _neutralReturns, _neutralWrites;
    private ulong _totalNeutralReturns;
    private uint _lastNeutralInputDebugStop;
    private ulong _unfocusedNeutralCapturePermits;
    private bool _retired;
    private static readonly JsonSerializerOptions Json = RuntimeControlJson.CreateOptions(4);

    internal NativeProgressionControl(ProbeConfiguration config, DebuggerHost host, Action<object> log)
    {
        _maximumCommandJournalBytes = config.InteractiveSession ? InteractiveSessionLimits.CommandJournalBytes : 2097152;
        _permit = config.ProgressionControl ?? throw new InvalidDataException("Missing progression control permit.");
        _identity = config.DiagnosticWorldState ?? throw new InvalidDataException("Progression requires its World identity.");
        bool story3 = config.ProgressionSceneLevel == 3;
        Require(!story3 || ProbeMain.IsClosedStory3Host(config) || ProbeMain.IsClosedStory3Guest(config),
            "Story3 primary input control requires its closed Host outcome or Guest presentation cohort.");
        _sceneLevel = config.CampaignScene?.Level ?? (story3 ? 3u : 28u); _campaign = config.CampaignScene;
        Require(config.EnableProgressionAuthority && config.UseProcessLocalMediaPath && config.EnableWorldEntry &&
            (_permit.NextLevel == 28 || ProbeMain.IsCampaign(config) && _permit.NextLevel == config.CampaignScene!.Level) && _permit.OwnerPid != 0 && _permit.OwnerCreationFileTime != 0 &&
            _permit.CapabilityNonce.Length == 32 && _permit.CapabilityNonce.All(Uri.IsHexDigit) &&
            Path.IsPathFullyQualified(_permit.CommandPath) && File.Exists(_permit.CommandPath) &&
            (File.GetAttributes(_permit.CommandPath) & FileAttributes.ReparsePoint) == 0,
            "Progression control requires an explicit bounded Alfea parent permit and existing ordinary command.");
        _host = host; _log = log; _capability = Convert.FromHexString(_permit.CapabilityNonce);
        _saveAllowed = config.ProgressionSavedReload && _identity.Role == WorldStateRole.Host;
        _overlayAllowed = config.ProgressionOverlayFixture;
        _reloadPreparationAllowed = config.ProgressionPreWaitingReloadPreparation && _saveAllowed && !story3;
        _owner = Process.GetProcessById(checked((int)_permit.OwnerPid));
        Require(!_owner.HasExited && Creation(_owner) == _permit.OwnerCreationFileTime,
            "Progression coordinator identity changed.");
        PollAtWorld();
    }
    internal bool InputAllowed => !_retired && _command?.Action == "Resume";
    internal bool TransitionRequested => !_retired && _command?.Action == "Transition";
    internal bool SaveRequested => !_retired && _saveAllowed && _command?.Action == "Save";
    internal bool ReloadPreparationRequested => !_retired && _reloadPreparationAllowed && _command?.Action == "PrepareReload";
    internal bool OverlayPauseRequested => !_retired && _overlayAllowed && _command?.Action == "Pause";
    internal bool OverlayContinueRequested => !_retired && _overlayAllowed && _command?.Action == "Continue";
    internal bool RetireRequested => !_retired && _command?.Action == "Retire";
    internal bool AwaitingRetirement => !_retired && _command is { Action: "Hold", Operation: > 0 };
    internal ulong Operation => _command?.Operation ?? 0;
    internal bool FreshNeutralObserved => _neutralReturns > 0;
    internal object Summary => new { enabled = true, inputAllowed = InputAllowed, retired = _retired,
        sourceSceneLevel = _sceneLevel, destinationLevel = _permit.NextLevel,
        sequence = _command?.Sequence, operation = _command?.Operation, action = _command?.Action,
        neutralOriginalInputReturns = _neutralReturns, commandFieldWrites = _neutralWrites,
        totalNeutralOriginalInputReturns = _totalNeutralReturns,
        lastNeutralInputDebugStop = _lastNeutralInputDebugStop, unfocusedNeutralCapturePermits = _unfocusedNeutralCapturePermits,
        barrierOwnsOnlyMovementJumpOrdinaryAttack = true, pauseAndCameraFieldsPreserved = true,
        commandSha256 = _commandHash };

    internal void PollAtWorld()
    {
        Require(Environment.CurrentManagedThreadId == _thread, "Progression command owner thread changed.");
        if (_retired || Environment.TickCount64 - _lastPoll < 25) return;
        _lastPoll = Environment.TickCount64;
        Require(!_owner.HasExited && Creation(_owner) == _permit.OwnerCreationFileTime,
            "Progression parent closed or changed.");
        var file = new FileInfo(_permit.CommandPath);
        Require(file.Exists && file.Length > 0 && file.Length <= _maximumCommandJournalBytes &&
            (file.Attributes & FileAttributes.ReparsePoint) == 0, "Progression command disappeared or exceeded its bound.");
        byte[] bytes = ReadAtomicCommand(file.FullName, _maximumCommandJournalBytes);
        string hash = Convert.ToHexString(SHA256.HashData(bytes));
        var next = RuntimeControlJson.Deserialize<ProgressionActorCommand>(bytes, Json) ??
            throw new InvalidDataException("Missing progression command.");
        Require(next.Version == 1 && next.Sequence > 0 && next.LevelEpoch == _identity.LevelEpoch &&
            next.Generation == _identity.Generation && next.CapabilityNonce.Length == 32 &&
            next.CapabilityNonce.All(Uri.IsHexDigit) &&
            CryptographicOperations.FixedTimeEquals(Convert.FromHexString(next.CapabilityNonce), _capability) &&
            (next.Action is "Hold" or "Resume" or "Transition" or "Retire" || _saveAllowed && next.Action == "Save" ||
                _reloadPreparationAllowed && next.Action == "PrepareReload" ||
                _overlayAllowed && (next.Action is "Pause" or "Continue")) &&
            next.IssuedAtMilliseconds >= 0 && next.IssuedAtMilliseconds <= Environment.TickCount64 &&
            Environment.TickCount64 - next.IssuedAtMilliseconds <= 5000 &&
            (next.Action is "Transition" or "Retire" or "Save" or "Pause" or "Continue" or "PrepareReload" ? next.Operation > 0 : true),
            "Progression command identity, action or local lease differs.");
        if (_command is { } previous)
        {
            Require(next.Sequence >= previous.Sequence, "Progression command sequence rolled back.");
            if (next.Sequence == previous.Sequence)
            { Require(hash == _commandHash, "Same progression sequence changed payload."); return; }
            Require(previous.Action is not ("Transition" or "Retire") ||
                next.Action == previous.Action && next.Operation == previous.Operation,
                "Terminal native generation command cannot be revoked.");
            Require(previous is not { Action: "Hold", Operation: > 0 } ||
                next.Operation == previous.Operation && next.Action is "Hold" or "Retire",
                "A prepared old Guest generation cannot reopen gameplay.");
            Require(previous.Action != "PrepareReload" || next.Operation == previous.Operation &&
                next.Action is "PrepareReload" or "Transition",
                "A pre-Waiting reload preparation cannot change operation or reopen gameplay.");
        }
        if (_command?.Action == "Resume" && next.Action != "Resume") _neutralReturns = 0;
        _command = next; _commandHash = hash;
        _log(new { @event = "progression-actor-command", sequence = next.Sequence, operation = next.Operation,
            action = next.Action, levelEpoch = next.LevelEpoch, generation = next.Generation,
            inputAllowed = InputAllowed, authority = "pinned-local-coordinator-capability" });
    }

    internal void RearmNeutralAfterOverlayAtWorld(uint tid, in Win32Native.X86Context context)
    {
        Require(_overlayAllowed && !_retired && _command is { Action: "Continue", Operation: 1 } &&
            Environment.CurrentManagedThreadId == _thread && _host.IsAtDebugStop &&
            tid == _host.MainThreadId && context.Eip == NativeCallDispatcher.WorldReturn,
            "Overlay neutral rearm requires its explicitly held owned native resumed World.");
        _neutralReturns = 0;
        _log(new { @event = "progression-overlay-input-fence-rearmed", operation = _command!.Operation,
            levelEpoch = _identity.LevelEpoch, generation = _identity.Generation,
            actualWorldStop = _host.DebugStopSequence, receiptCounterOnly = true,
            nativeContextWrites = 0, nativeApplyAcknowledgmentAdded = false,
            requiresSubsequentActualOriginalInputReturn = true });
    }

    internal void ObserveInputReturn(uint tid, ref Win32Native.X86Context context)
    {
        if (_retired || InputAllowed) return;
        Require(_host.IsAtDebugStop && tid == _host.MainThreadId && context.Eip == NativeInputCapture.InputReturn,
            "Progression input barrier left original InputRET.");
        uint flow = U(0x755294); if (flow < 0x10000 || U(flow) != 0x702D2C ||
            U(flow + 0x1B0) != _sceneLevel || U(flow + 0x1B8) != 0) return;
        uint depth = U(flow + 0x1AC); Require(depth < 20, "Progression input Flow exceeds bound.");
        uint playing = U(flow + 0x15C + depth * 4);
        if (playing < 0x10000 || U(playing) != 0x710874 || U(playing + 0x10) != _sceneLevel) return;
        uint input = U(0x7552A0); Require(input >= 0x10000 && U(input) == 0x6F5508, "Progression input owner differs.");
        uint device = U(input + 0x1C), command = U(input + 0x18);
        Require(device >= 0x10000 && U(device) == 0x7116CC && command >= 0x10000 && U(command) == 0x703090,
            "Progression input command/device differs.");
        if (U(device + 0x14) != 3) return;
        foreach (var field in new[] { (Offset: 4u, Bytes: 4), (Offset: 0x1Au, Bytes: 1), (Offset: 0x20u, Bytes: 1) })
        {
            byte[] before = Win32Native.ReadMemory(_host.ProcessHandle, command + field.Offset, field.Bytes);
            if (before.Any(b => b != 0))
            {
                Win32Native.WriteMemory(_host.ProcessHandle, command + field.Offset, new byte[field.Bytes], executable: false);
                ++_neutralWrites;
            }
            Require(Win32Native.ReadMemory(_host.ProcessHandle, command + field.Offset, field.Bytes).All(b => b == 0),
                "Progression neutral command readback differs.");
        }
        ++_neutralReturns; ++_totalNeutralReturns; _lastNeutralInputDebugStop = _host.DebugStopSequence;
        if (_neutralReturns == 1)
            _log(new { @event = "progression-input-native-neutral", levelEpoch = _identity.LevelEpoch,
                generation = _identity.Generation, actualOriginalInputReturn = true, commandFields = new[] { 4, 0x1A, 0x20 },
                commandSequence = _command?.Sequence,
                pauseAndCameraFieldsChanged = false });
    }

    // A narrow E7 caller allowance, not a device acquisition or source record.
    // The preceding callback must have read back the stock neutral fields at
    // this exact genuine InputRET. Positive Hold operations are retirement,
    // and Resume never grants this allowance while the window is inactive.
    internal bool CanCaptureUnfocusedNeutralAtInputReturn(uint tid, in Win32Native.X86Context context)
    {
        if (_retired || _sceneLevel != 28 && _campaign?.Admits(_sceneLevel) != true || _command is not { Action: "Hold", Operation: 0 } || _neutralReturns == 0 ||
            _lastNeutralInputDebugStop == 0 || _lastNeutralInputDebugStop != _host.DebugStopSequence) return false;
        Require(Environment.CurrentManagedThreadId == _thread && _host.IsAtDebugStop && tid == _host.MainThreadId &&
            context.Eip == NativeInputCapture.InputReturn, "Unfocused neutral allowance left its actual same InputRET.");
        long now = Environment.TickCount64;
        Require(!_owner.HasExited && Creation(_owner) == _permit.OwnerCreationFileTime &&
            _command.IssuedAtMilliseconds >= 0 && _command.IssuedAtMilliseconds <= now && now - _command.IssuedAtMilliseconds <= 5000,
            "Unfocused neutral allowance lost its fresh exact local command owner.");
        ++_unfocusedNeutralCapturePermits;
        return true;
    }
    public void Dispose() { _retired = true; _owner.Dispose(); }
    private static byte[] ReadAtomicCommand(string path, long maximumBytes)
    {
        for (int attempt = 0; ; ++attempt)
        {
            try
            {
                using var file = new FileStream(path, FileMode.Open, FileAccess.Read,
                    FileShare.ReadWrite | FileShare.Delete);
                long length = file.Length;
                Require(length > 0 && length <= maximumBytes, "Progression command journal exceeded its bound.");
                int count = checked((int)Math.Min(length, 8192));
                file.Position = length - count;
                byte[] bytes = new byte[count];
                file.ReadExactly(bytes);
                int end = Array.LastIndexOf(bytes, (byte)'\n');
                Require(end >= 0, "Progression command journal has no completed record.");
                int start = end == 0 ? -1 : Array.LastIndexOf(bytes, (byte)'\n', end - 1);
                Require(start >= 0 || length == count, "Progression journal record exceeded its bounded tail.");
                byte[] record = bytes.AsSpan(start + 1, end - start - 1).ToArray();
                Require(record.Length is > 0 and <= 4096, "Progression completed command length differs.");
                return record;
            }
            catch (IOException error) when (attempt < 3 && (error.HResult & 0xFFFF) is 32 or 33)
            { Thread.Sleep(1); } // Retry a fresh handle; never reuse a cached command as new proof.
        }
    }
    private uint U(uint address) => Win32Native.ReadUInt32(_host.ProcessHandle, address);
    private static ulong Creation(Process process) => checked((ulong)process.StartTime.ToUniversalTime().ToFileTimeUtc());
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidDataException(message); }
}
