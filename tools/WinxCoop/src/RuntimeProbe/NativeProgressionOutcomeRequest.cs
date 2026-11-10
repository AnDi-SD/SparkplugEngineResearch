using System.Reflection.PortableExecutable;
using System.Security.Cryptography;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

// Trusted local receipts supplied by the original scene/outcome owners. Neither
// this adapter nor its native calls manufacture GameOver or checkpoint proof.
internal sealed record NativeProgressionOutcomeWitness(DebuggerHost Owner, uint LevelEpoch,
    uint Generation, ulong Operation, bool ProgressionGameOverSettled,
    bool ActualOriginalPlayingExitReturned, bool SharedBattleLifetimeSucceeded);
internal sealed record NativeProgressionOutcomeCheckpoint(string FilePath, string Sha256,
    int Bytes, uint DestinationLevel = 28);

/// <summary>
/// Closed Story3 Host outcome: actual GameOver -> original registered MainMenu,
/// then qualified cleanup -> whole original Saved00 load to destination28.
/// No old player/scene graph is followed and no network/save codec is added.
/// </summary>
internal sealed class NativeProgressionOutcomeRequest
{
    private const uint Request = 0x595450, LoadSave = 0x4E7580, LoadRet4 = 0x4E7B88;
    private const uint NativeLoadRequestCaller = 0x4E779B;
    private const uint FlowTable = 0x702D2C, ProfileTable = 0x6F5A44;
    private const uint GameOverTable = 0x70F778, GameOverGetter = 0x5E0450, GameOverRecord = 0x768630;
    private const uint MenuTable = 0x7102A8, MenuGetter = 0x5EF690, MenuRecord = 0x768990;
    private readonly DebuggerHost _host;
    private readonly PEReader _image;
    private readonly Action<object> _log;
    private readonly NativeProgressionOutcomeCheckpoint _checkpoint;
    private readonly IntPtr _process;
    private readonly uint _pid, _thread, _epoch, _generation;
    private readonly int _owner;
    private readonly long _deadline;
    private NativeCallDispatcher? _calls;
    private NativeProgressionTransitionGate? _gate;
    private NativeWorldEntrySeed? _destinationEntry;
    private ulong _operation;
    private uint _stage, _worlds, _flow, _gameOver, _gameOverId, _menu, _menuId, _profile;
    private uint _menuReturnsBefore, _loadReturnsBefore, _menuReturnStop, _loadReturnStop;
    private long _lastNow;
    private bool _menuReturned, _loadReturned, _completed, _retired, _failed;
    private bool _actualOutcomeAccepted, _actualOldExitAccepted, _cleanupAccepted;
    private string _failure = "";

    internal NativeProgressionOutcomeRequest(ProbeConfiguration config, DebuggerHost host, PEReader image,
        Action<object> log, NativeProgressionOutcomeCheckpoint checkpoint)
    {
        ArgumentNullException.ThrowIfNull(host); ArgumentNullException.ThrowIfNull(image);
        ArgumentNullException.ThrowIfNull(log); ArgumentNullException.ThrowIfNull(checkpoint);
        Require(config.ProgressionControl is { NextLevel: 28 } && config.ProgressionSceneLevel == 3 &&
            config.EnableProgressionAuthority && config.UseProcessLocalMediaPath && config.EnableWorldEntry &&
            config.DiagnosticWorldState is { Role: WorldStateRole.Host } &&
            config.DiagnosticHostInput is { Role: DiagnosticInputRole.Host } && config.DiagnosticGuestInput is null &&
            config.TemplateSharedPlayerProfile == "gardenia-03-network-input" && config.TemplateNetworkInputFixture &&
            config.TemplateStoryDeathFixture && !config.TemplateProjectileManagerFixture &&
            !config.TemplateNetworkOrdinaryAttackFixture && !config.WorldBattlePresentation &&
            config.DurationSeconds is >= 5 and <= 600,
            "Native outcome request requires the closed Story3 Host seven-body progression cohort and checkpoint28.");
        Require(host.IsAtDebugStop && host.ProcessHandle != IntPtr.Zero && host.ProcessPid != 0 &&
            host.MainThreadId != 0 && host.ImageBase == 0x400000 &&
            Convert.ToHexString(SHA256.HashData(image.GetEntireImage().GetContent().ToArray())) == ProbeMain.OriginalSha256,
            "Native outcome request requires its exact owned original PC process.");
        string expectedPath = Path.GetFullPath(Path.Combine(config.MediaDirectory, "Saved", "Saved00.wxs"));
        Require(Path.IsPathFullyQualified(checkpoint.FilePath) &&
            Path.GetFullPath(checkpoint.FilePath).Equals(expectedPath, StringComparison.OrdinalIgnoreCase) &&
            checkpoint.DestinationLevel == 28 && checkpoint.Bytes is > 0 and <= 262144 &&
            checkpoint.Sha256.Length == 64 && checkpoint.Sha256.All(Uri.IsHexDigit),
            "Outcome checkpoint requires the exact isolated slot0 path, bounded bytes, pinned hash and declared destination28.");
        _checkpoint = checkpoint with { FilePath = expectedPath, Sha256 = checkpoint.Sha256.ToUpperInvariant() };
        _host = host; _image = image; _log = log; _process = host.ProcessHandle;
        _pid = host.ProcessPid; _thread = host.MainThreadId; _owner = Environment.CurrentManagedThreadId;
        _epoch = config.DiagnosticWorldState!.Value.LevelEpoch; _generation = config.DiagnosticWorldState.Value.Generation;
        Require(_epoch is > 0 and < uint.MaxValue && _generation is > 0 and < uint.MaxValue,
            "Outcome requires a nonwrapping explicit old scene identity.");
        _lastNow = Environment.TickCount64; _deadline = checked(_lastNow + (long)config.DurationSeconds * 1000);
        PinImage(LoadSave, "6AFFE9AF70F3FF"); PinImage(LoadRet4, "C20400");
        PinImage(0x4E778D, "8B4C24286A01518BC8E8B5DC0A008B54242C");
        PinImage(MenuGetter, "B890897600C3"); PinImage(GameOverGetter, "B830867600C3");
        Require(ImageWord(MenuTable + 0x10) == MenuGetter && ImageWord(GameOverTable + 0x10) == GameOverGetter &&
            Convert.ToHexString(SHA256.HashData(Image(Request, 0x65))) ==
                "C899657754981014FD38AFAC593DD67E79F6C0A659630AE4197457138421FE4A",
            "Outcome original Flow request or typed Menu/GameOver getter pins differ.");
        VerifyCheckpoint();
    }

    internal bool Started => _stage != 0;
    internal bool MenuRequestReturned => _menuReturned;
    internal bool SavedLoadReturned => _loadReturned;
    internal bool Completed => _completed && !_failed;
    internal bool HasPendingNativeCall => _stage == 1 && !_menuReturned || _stage == 3 && !_loadReturned;
    internal object Summary => new
    {
        sourceLevel = 3, destinationLevel = 28, levelEpoch = _epoch, generation = _generation,
        operation = _operation, stage = _stage, worlds = _worlds, retired = _retired, failed = _failed,
        failure = _failure, completed = Completed, originalMenuRequestReturned = _menuReturned,
        originalSavedLoadReturned = _loadReturned, pendingOriginalCall = HasPendingNativeCall,
        actualGameOverWitnessAccepted = _actualOutcomeAccepted, actualOldExitWitnessAccepted = _actualOldExitAccepted,
        actualSharedBattleLifetimeSucceededAccepted = _cleanupAccepted,
        nativeMenuId = _menuId, nativeGameOverId = _gameOverId,
        checkpointSha256 = _checkpoint.Sha256, checkpointBytes = _checkpoint.Bytes,
        nativeCallsRequested = (_stage >= 1 ? 1 : 0) + (_stage >= 3 ? 1 : 0),
        inputOrProfileOrFlowWrites = 0, savedCodecDuplicated = false,
        nativeRequestReturnIsActualNewEntry = false, actualBothPeersReadyClaimed = false,
        witnessAuthority = "trusted-local-scene-owner-with-exact-debugger-epoch-generation-operation"
    };

    internal void StepAtWorld(uint tid, ref Win32Native.X86Context context, NativeCallDispatcher calls,
        NativeProgressionTransitionGate gate, Action resume, NativeProgressionOutcomeWitness witness,
        NativeWorldEntrySeed? destinationEntry = null)
    {
        try
        {
            EnsureWorld(tid, context); Require(!_retired && !_failed, "Native outcome owner is retired or failed.");
            Require(++_worlds <= NativeObservationBudget.WorldLimit, "Native outcome owner exceeded its finite World budget.");
            Require(ReferenceEquals(witness.Owner, _host) && witness.LevelEpoch == _epoch && witness.Generation == _generation,
                "Native outcome receipt belongs to another debugger owner or old generation.");
            _calls ??= calls; _gate ??= gate;
            Require(ReferenceEquals(_calls, calls) && ReferenceEquals(_gate, gate), "Native outcome changed dispatcher or authority owner.");
            if (_completed) return;
            if (_operation != 0) Require(witness.Operation == _operation && witness.ProgressionGameOverSettled &&
                witness.ActualOriginalPlayingExitReturned, "Native outcome operation or accepted original witnesses changed.");
            if (_cleanupAccepted) Require(witness.SharedBattleLifetimeSucceeded, "Cached native outcome cleanup proof regressed.");
            if (calls.Pending) return;
            Require(!calls.HasRetiredPage, "Native outcome caller must release the previous dispatcher page at this later World.");
            if (_stage == 0)
            {
                if (!witness.ProgressionGameOverSettled || !witness.ActualOriginalPlayingExitReturned || witness.Operation == 0) return;
                _operation = witness.Operation; _actualOutcomeAccepted = _actualOldExitAccepted = true;
                CaptureFreshGameOverAndMenu();
                var authority = new NativeProgressionOutcomeMenuAuthority(_host, _epoch, _generation, _operation,
                    _flow, _gameOver, _gameOverId, _menu, _menuId, true, true);
                gate.PermitOwnedOutcomeMenuRequestAtWorld(tid, context, calls, authority);
                _menuReturnsBefore = gate.OriginalRequestReturns; _stage = 1;
                _log(new { @event = "progression-outcome-original-menu-request-begin", operation = _operation,
                    nativeMenuId = _menuId, actualGameOverId = _gameOverId, nativeFunction = "0x00595450",
                    actualGameOverAndOldExitWitnessRequired = true, inputWrites = 0, oldSceneOrPlayerReads = 0 });
                calls.Begin(_host, tid, ref context, Request, _flow, [_menuId, 0], false, value =>
                {
                    try
                    {
                    EnsureCallback();
                    var returned = gate.LastOriginalRequestReturn;
                    Require(!_menuReturned && gate.OriginalRequestReturns == checked(_menuReturnsBefore + 1) &&
                        returned is { } receipt && receipt.Receiver == _flow && receipt.Destination == _menuId &&
                        receipt.RawArgument == 0 && receipt.OwnedOutcomeOperation == _operation,
                        "Native outcome Menu request lost its exact dispatcher-authorized original Flow return.");
                    _menuReturned = true; _menuReturnStop = _host.DebugStopSequence; _stage = 2;
                    _log(new { @event = "progression-outcome-original-menu-request-returned", operation = _operation,
                        resultOpaque = value, actualGameOverExitStillRequired = true, originalRet8Paired = true,
                        oldSceneOrPlayerReads = 0 }); resume();
                    }
                    catch (Exception error) { _failed = true; _failure = error.Message; throw; }
                });
                return;
            }
            if (_stage == 2)
            {
                Require(_menuReturned && _host.DebugStopSequence > _menuReturnStop, "Outcome Menu continuation is not a later World.");
                if (!witness.SharedBattleLifetimeSucceeded) return;
                Require(FreshOriginalMenu(), "Completed native story cleanup has no fresh registered original MainMenu.");
                _cleanupAccepted = true;
                if (destinationEntry is null) return; // Root must arm the exact next-entry scope before the whole load.
                Require(destinationEntry is { Level: 28, Retired: false, Failed: false, EntryCompleted: false } &&
                    !destinationEntry.HasPendingOriginalEntry, "Outcome saved load needs a fresh routed destination28 entry owner.");
                _destinationEntry ??= destinationEntry;
                Require(ReferenceEquals(_destinationEntry, destinationEntry), "Outcome changed its authorized destination entry scope.");
                BeginSavedLoad(tid, ref context, calls, gate, resume);
                return;
            }
            if (_stage == 4)
            {
                Require(_loadReturned && _host.DebugStopSequence > _loadReturnStop,
                    "Outcome saved load completion is not a later World after the original return.");
                VerifyCheckpoint(); _completed = true;
                _log(new { @event = "progression-outcome-native-requests-completed", operation = _operation,
                    actualGameOverAndOldExitAndCleanupPreserved = true, originalMenuAndWholeLoadSaveReturned = true,
                    actualNativeSavedDestination28Observed = true, dispatcherPageReleasedAtLaterWorld = true,
                    nextNativeEntryAndBothPeersReadyRemainExternal = true, oldSceneOrPlayerReads = 0 });
            }
        }
        catch (Exception error) { _failed = true; _failure = error.Message; throw; }
    }

    private void BeginSavedLoad(uint tid, ref Win32Native.X86Context context, NativeCallDispatcher calls,
        NativeProgressionTransitionGate gate, Action resume)
    {
        VerifyCheckpoint(); PinLive(LoadSave, "6AFFE9AF70F3FF"); PinLive(LoadRet4, "C20400");
        _profile = P(0x765AD4); Require(U(_profile) == ProfileTable, "Outcome saved load lost its live canonical Profile singleton.");
        // Deliberately no Profile+2B4/Instance/root/old Playing or old scene read.
        _loadReturnsBefore = gate.OriginalRequestReturns; _stage = 3;
        _log(new { @event = "progression-outcome-whole-saved-load-begin", operation = _operation,
            nativeFunction = "0x004E7580", slot = 0, checkpointSha256 = _checkpoint.Sha256,
            originalMenuAndActualStoryCleanupRequired = true, nativeProfileOrHealthWrites = 0 });
        calls.Begin(_host, tid, ref context, LoadSave, _profile, [0], false, value =>
        {
            try
            {
            EnsureCallback(); VerifyCheckpoint();
            var receipt = gate.LastOriginalRequestReturn ??
                throw new InvalidDataException("Outcome LoadSave has no original Flow request-return receipt.");
            Require(!_loadReturned && (value & 255) == 1 && U(0x765AD4) == _profile && U(_profile) == ProfileTable &&
                gate.OriginalRequestReturns == checked(_loadReturnsBefore + 1) &&
                receipt.Receiver == _flow && receipt.Caller == NativeLoadRequestCaller && receipt.Destination == 28 &&
                receipt.RawArgument == 1 && receipt.OwnedOutcomeOperation == 0,
                "Original outcome LoadSave did not return AL1 with exactly one original destination28/argument1 request.");
            _loadReturned = true; _loadReturnStop = _host.DebugStopSequence; _stage = 4;
            _log(new { @event = "progression-outcome-whole-saved-load-returned", operation = _operation,
                originalLoadSucceededAlOne = true, actualOriginalDestination = receipt.Destination,
                originalRequestCaller = "0x004E779B", actualNewEntryStillRequired = true,
                unchangedCheckpointSha256 = _checkpoint.Sha256, dispatcherPageStillRetired = true }); resume();
            }
            catch (Exception error) { _failed = true; _failure = error.Message; throw; }
        });
    }

    private void CaptureFreshGameOverAndMenu()
    {
        _flow = P(0x755294); Require(U(_flow) == FlowTable && U(_flow + 0x1B8) == 0, "Outcome has no settled original Flow.");
        uint depth = U(_flow + 0x1AC); Require(depth < 20, "Outcome Flow exceeds its twenty native slots.");
        _gameOver = P(_flow + 0x15C + depth * 4); _gameOverId = U(_gameOver + 0x10);
        Require(U(_gameOver) == GameOverTable && U(_flow + 0x1B0) == _gameOverId &&
            U(GameOverRecord) == 0x331018ED && U(GameOverTable + 0x10) == GameOverGetter,
            "Outcome request lacks the current typed original GameOver.");
        PinLive(GameOverGetter, "B830867600C3"); PinLive(MenuGetter, "B890897600C3");
        uint matches = 0;
        for (uint index = 0; index < 81; ++index)
        {
            uint candidate = U(_flow + 0x18 + index * 4); if (candidate == 0) continue;
            Require(candidate >= 0x10000 && (candidate & 3) == 0, "Outcome Flow registry contains an invalid pointer.");
            if (U(candidate) != MenuTable) continue;
            Require(index is >= 51 and <= 80 && U(candidate + 0x10) == index &&
                U(MenuTable + 0x10) == MenuGetter && U(MenuRecord) == 0x284B3AA2,
                "Outcome registered MainMenu class/ID differs from the original native branch.");
            _menu = candidate; _menuId = index; ++matches;
        }
        Require(matches == 1, "Outcome requires exactly one registered original MainMenu.");
    }
    private bool FreshOriginalMenu()
    {
        uint flow = P(0x755294); Require(flow == _flow && U(flow) == FlowTable, "Outcome Menu changed its original Flow owner.");
        uint depth = U(flow + 0x1AC); Require(depth < 20, "Outcome Menu Flow exceeds its twenty native slots.");
        if (U(flow + 0x1B8) != 0) return false;
        uint top = P(flow + 0x15C + depth * 4);
        return top == _menu && U(top) == MenuTable && U(top + 0x10) == _menuId && U(flow + 0x1B0) == _menuId &&
            U(flow + 0x18 + _menuId * 4) == _menu && U(MenuRecord) == 0x284B3AA2;
    }
    private void VerifyCheckpoint()
    {
        Require(File.Exists(_checkpoint.FilePath) &&
            (File.GetAttributes(_checkpoint.FilePath) & FileAttributes.ReparsePoint) == 0,
            "Outcome checkpoint slot is missing or a reparse point.");
        byte[] bytes = File.ReadAllBytes(_checkpoint.FilePath);
        Require(bytes.Length == _checkpoint.Bytes && Convert.ToHexString(SHA256.HashData(bytes)) == _checkpoint.Sha256,
            "Outcome checkpoint bytes changed before or during the original load.");
    }
    internal void RetireCached()
    {
        Require(Environment.CurrentManagedThreadId == _owner, "Outcome retirement changed its managed owner thread.");
        _retired = true; // Pending status remains visible; an abort is never converted into a receipt.
    }
    private void EnsureWorld(uint tid, in Win32Native.X86Context context)
    {
        EnsureCallback(); Require(tid == _thread && context.Eip == NativeCallDispatcher.WorldReturn,
            "Outcome requires its actual original main-thread WorldRET.");
    }
    private void EnsureCallback()
    {
        long now = Environment.TickCount64;
        Require(!_retired && !_failed && Environment.CurrentManagedThreadId == _owner && _host.IsAtDebugStop &&
            _host.ProcessHandle == _process && _host.ProcessPid == _pid && _host.MainThreadId == _thread &&
            _host.ImageBase == 0x400000 && now >= _lastNow && now <= _deadline,
            "Outcome owner/process/thread or bounded monotonic lifetime changed.");
        _lastNow = now;
    }
    private void PinImage(uint at, string hex) => Require(Image(at, hex.Length / 2).SequenceEqual(Convert.FromHexString(hex)),
        "Outcome original source pin differs.");
    private void PinLive(uint at, string hex)
    {
        PinImage(at, hex); byte[] expected = Convert.FromHexString(hex), actual = Win32Native.ReadMemory(_process, at, expected.Length);
        for (int index = 0; index < actual.Length; ++index)
            if (actual[index] == 0xCC && expected[index] != 0xCC && _host.Breakpoints.Contains(checked(at + (uint)index)))
                actual[index] = expected[index];
        Require(actual.SequenceEqual(expected), "Outcome live source pin differs outside owned breakpoints.");
    }
    private uint P(uint at) { uint value = U(at); Require(value >= 0x10000 && (value & 3) == 0, "Outcome fresh pointer is invalid."); return value; }
    private uint U(uint at) => Win32Native.ReadUInt32(_process, at);
    private byte[] Image(uint at, int bytes) => _image.GetSectionData(checked((int)(at - 0x400000))).GetContent(0, bytes).ToArray();
    private uint ImageWord(uint at) => BitConverter.ToUInt32(Image(at, 4));
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidDataException(message); }
}
