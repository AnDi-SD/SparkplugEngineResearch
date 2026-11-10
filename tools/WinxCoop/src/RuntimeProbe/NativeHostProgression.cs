using System.Reflection.PortableExecutable;
using System.Security.Cryptography;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// Bounded Alfea28 Host retirement and next-entry receipt. The coordinator owns
/// authorization and the native request; this observer neither issues that
/// request nor recreates controllers in the retiring process.
/// </summary>
internal sealed class NativeHostProgression
{
    private readonly DebuggerHost _host;
    private readonly Action<object> _log;
    private readonly IntPtr _process;
    private readonly uint _pid, _thread, _oldEpoch, _oldGeneration;
    private readonly int _owner, _durationSeconds;
    private readonly uint _level, _destinationLevel;
    private readonly bool _storyOutcome;
    private NativeTemplateProbe? _template;
    private NativeWorldEntrySeed? _nextEntry;
    private byte[]? _seed, _witness;
    private uint _worlds, _unloadWorld, _retirementWorld, _completedWorld;
    private long _unloadStartedMilliseconds;
    private bool _actualExitReturned, _retirementReady, _completed, _failed;
    private string _failure = "";

    internal NativeHostProgression(ProbeConfiguration config, DebuggerHost host, PEReader image, Action<object> log)
    {
        bool storyOutcome = config.ProgressionControl is not null && config.ProgressionSceneLevel == 3 &&
            config.TemplateSharedPlayerProfile == "gardenia-03-network-input" && config.TemplateStoryDeathFixture &&
            !config.TemplateProjectileManagerFixture;
        bool alfea = config.ProgressionSceneLevel == 28 && config.TemplateSharedPlayerProfile == "alfea-28-network-input" &&
            !config.TemplateStoryDeathFixture;
        Require(config.EnableProgressionAuthority &&
            config.DiagnosticWorldState is { Role: WorldStateRole.Host } &&
            config.DiagnosticHostInput is { Role: DiagnosticInputRole.Host } &&
            config.DiagnosticGuestInput is null && config.TemplateNetworkInputFixture &&
            (alfea || storyOutcome) && !config.WorldBattlePresentation &&
            !config.TemplateNetworkOrdinaryAttackFixture && !config.TemplateDeathBattleFixture &&
            !config.TemplateMosquitoPlayerTargets &&
            config.DurationSeconds is >= 5 and <= 600,
            "Host progression requires its explicit bounded Alfea28 Host network-body cohort.");
        WorldStateIdentity world = config.DiagnosticWorldState!.Value;
        DiagnosticInputIdentity input = config.DiagnosticHostInput!.Value;
        Require(world.LevelEpoch is > 0 and < uint.MaxValue && world.Generation is > 0 and < uint.MaxValue &&
            input.SessionId == world.SessionId && input.ScenarioId == world.ScenarioId &&
            input.LevelEpoch == world.LevelEpoch && input.BodyGeneration == world.Generation &&
            config.ProgressionAuthorityRole is null or WorldStateRole.Host,
            "Host progression Input5/WCS1 identities or explicit authority role differ.");
        Require(host.IsAtDebugStop && host.ProcessHandle != IntPtr.Zero && host.ProcessPid != 0 &&
            host.MainThreadId != 0 && host.ImageBase == 0x400000 &&
            Convert.ToHexString(SHA256.HashData(image.GetEntireImage().GetContent().ToArray())) == ProbeMain.OriginalSha256,
            "Host progression requires the exact original image in its owned stopped process.");
        _host = host; _log = log; _process = host.ProcessHandle; _pid = host.ProcessPid;
        _thread = host.MainThreadId; _owner = Environment.CurrentManagedThreadId;
        _oldEpoch = world.LevelEpoch; _oldGeneration = world.Generation; _durationSeconds = config.DurationSeconds;
        _storyOutcome = storyOutcome; _level = storyOutcome ? 3u : 28u;
        _destinationLevel = config.ProgressionControl?.NextLevel ?? 28;
        Require(_destinationLevel == 28, "Host continuation currently requires its explicit checkpoint/reload28 destination.");
        log(new { @event = "host-progression-observer-created", level = _level, oldEpoch = _oldEpoch,
            oldGeneration = _oldGeneration, requestAuthorizationOwnedByCoordinator = true,
            nativeRequestCalls = 0, oldProcessBodyRecreation = false });
    }

    internal bool RetirementReady => _retirementReady && !_failed;
    internal bool Completed => _completed && !_failed;
    internal byte[]? CopiedSeed => !Completed || _seed is null ? null : (byte[])_seed.Clone();
    internal byte[]? CopiedWitness => !Completed || _witness is null ? null : (byte[])_witness.Clone();
    internal object Summary => new
    {
        role = "Host", level = _level, destinationLevel = _destinationLevel, storyOutcome = _storyOutcome,
        oldEpoch = _oldEpoch, oldGeneration = _oldGeneration,
        nextEpoch = _oldEpoch + 1, nextGeneration = _oldGeneration + 1,
        worlds = _worlds, unloadWorld = _unloadWorld, retirementWorld = _retirementWorld,
        completedWorld = _completedWorld, externallySuppliedPairedOriginalExitReturn = _actualExitReturned,
        retirementReady = RetirementReady, completed = Completed, failed = _failed, failure = _failure,
        copiedSeedBytes = _seed?.Length ?? 0, copiedWitnessBytes = _witness?.Length ?? 0,
        copiedSeedSha256 = _seed is null ? null : Convert.ToHexString(SHA256.HashData(_seed)),
        copiedWitnessSha256 = _witness is null ? null : Convert.ToHexString(SHA256.HashData(_witness)),
        requiresOldNativeDeleteBatchAndLaterWorld = true, requiresActualNextEntryWorld = true,
        coordinatorBarrierValidatedByThisObserver = false, gameplayMovementJumpGoalRequired = false,
        pointerInequalityUsedAsGenerationProof = false, historicalSceneOrOwnGraphReadAfterExit = false,
        nativeRequestsAdded = 0, nativeGameplayWrites = 0, inProcessBodyOrTransportRecreation = false,
        nextProcessOrBothPeersReadyClaimed = false, fullCoopProgressionAccepted = false
    };

    internal void ObserveAtWorld(uint tid, ref Win32Native.X86Context context, NativeCallDispatcher calls,
        NativeTemplateProbe template, bool actualOldExitReturned, NativeWorldEntrySeed nextEntry)
    {
        try
        {
            Require(!_failed && _host.IsAtDebugStop && _host.ProcessHandle == _process &&
                _host.ProcessPid == _pid && tid == _thread && _host.MainThreadId == _thread &&
                Environment.CurrentManagedThreadId == _owner && context.Eip == NativeCallDispatcher.WorldReturn &&
                !calls.Pending, "Host progression observation lost its owned main-thread WorldRET.");
            if (_completed) return;
            Require(++_worlds <= NativeObservationBudget.WorldLimit,
                "Host progression observation exceeded its finite World budget.");
            _template ??= template; _nextEntry ??= nextEntry;
            Require(ReferenceEquals(_template, template) && ReferenceEquals(_nextEntry, nextEntry),
                "Host progression observer changed its old template or next-entry scope.");
            if (!template.SharedBattleUnloadStarted) return;
            if (_unloadStartedMilliseconds == 0)
            {
                _unloadStartedMilliseconds = Environment.TickCount64; _unloadWorld = _worlds;
            }
            Require(Environment.TickCount64 - _unloadStartedMilliseconds <= (long)_durationSeconds * 1000,
                "Host progression did not retire and qualify its next entry within its finite unload bound.");
            if (!template.SharedBattleNativeDeleteBatchCompleted || !actualOldExitReturned) return;
            _actualExitReturned = true;
            if (_storyOutcome && !template.SharedBattleLifetimeSucceeded) return;
            if (!_storyOutcome && !template.NativeResourcesRetired)
                template.ObserveSharedBattleResourcesRetiredAtWorld(_host, tid, ref context, calls,
                    actualOriginalExitReturned: true);
            Require((_storyOutcome ? template.SharedBattleLifetimeSucceeded : template.NativeResourcesRetired) && !template.HasLiveOwnership &&
                !calls.Pending && !calls.HasRetiredPage,
                "Host progression retained old native resources or a dispatcher page after retirement.");
            if (!_retirementReady)
            {
                _retirementReady = true; _retirementWorld = _worlds;
                _log(new { @event = "host-progression-old-resources-retired", world = _worlds,
                    externallySuppliedPairedOriginalExitReturn = true, completedNativeDeleteBatch = true,
                    laterWorldResourceRelease = true, historicalSceneOrOwnGraphRead = false,
                    waitingForActualNextEntryWorld = !nextEntry.WorldQualified });
            }
            if (!nextEntry.WorldQualified) return;
            byte[] seed = nextEntry.GetSeed(), witness = nextEntry.GetWitness();
            WorldEntrySeedState decoded = WorldEntrySeedState.Unpack(seed);
            Require(decoded.Level == _destinationLevel && decoded.LevelEpoch == _oldEpoch + 1 &&
                decoded.Generation == _oldGeneration + 1 && witness.Length == NativeWorldEntrySeed.WitnessBytes,
                "Host progression next-entry payload is not the next Alfea28 epoch/generation.");
            _seed = (byte[])seed.Clone(); _witness = (byte[])witness.Clone();
            _completedWorld = _worlds; _completed = true;
            _log(new { @event = "host-progression-next-entry-copied", world = _worlds,
                oldEpoch = _oldEpoch, oldGeneration = _oldGeneration, nextEpoch = decoded.LevelEpoch,
                nextGeneration = decoded.Generation, actualHostEntryOrdinal = decoded.HostEntryOrdinal,
                seedSha256 = Convert.ToHexString(SHA256.HashData(_seed)),
                witnessSha256 = Convert.ToHexString(SHA256.HashData(_witness)),
                actualOriginalEntryPlacementAndWorldRequired = true, oldResourcesRetired = true,
                nativePointersInPayload = false, inProcessSecondBodyRecreated = false,
                newInputAndWorldSessionsRequiredInNextProcess = true, bothPeersReadyClaimed = false });
        }
        catch (Exception exception)
        {
            _failed = true; _failure = exception.Message;
            _log(new { @event = "host-progression-observation-failed", failure = _failure,
                nativeRequestsAdded = 0, nativeGameplayWrites = 0 });
            throw;
        }
    }

    private static void Require(bool condition, string message)
    { if (!condition) throw new InvalidDataException(message); }
}
