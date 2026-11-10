using System.Reflection.PortableExecutable;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// One authorized Host capture/write lifecycle while the two-body generation
/// remains live and held. The existing SavePoint probe owns every native seam;
/// this adapter neither reloads the file nor completes the Actor experiment.
/// </summary>
internal sealed class NativeProgressionSavedState
{
    private readonly DebuggerHost _host;
    private readonly Action<object> _log;
    private readonly NativeSaveAuthorityGate _gate;
    private readonly NativeSavePointProbe _probe;
    private readonly IntPtr _process;
    private readonly uint _pid, _thread;
    private readonly int _owner, _durationSeconds;
    private uint _worlds, _startWorld, _completeWorld, _hostReturnsBefore, _hostSuccessfulReturnsBefore, _guestRejectionsBefore;
    private uint _authorizationStartStop, _completionStop;
    private long _startedMilliseconds;
    private bool _started, _completed, _failed;
    private string _failure = "";

    internal NativeProgressionSavedState(ProbeConfiguration config, DebuggerHost host, PEReader image,
        Action<object> log, NativeSaveAuthorityGate gate)
    {
        Require(config.ProgressionControl is not null && config.EnableProgressionAuthority &&
            config.UseProcessLocalMediaPath && config.EnableWorldEntry &&
            config.DiagnosticWorldState is { Role: WorldStateRole.Host } &&
            config.DiagnosticHostInput is { Role: DiagnosticInputRole.Host } && config.DiagnosticGuestInput is null &&
            config.TemplateNetworkInputFixture && config.TemplateSharedPlayerProfile == "alfea-28-network-input" &&
            !config.WorldBattlePresentation && !config.TemplateNetworkOrdinaryAttackFixture &&
            !config.TemplateDeathBattleFixture && !config.TemplateStoryDeathFixture &&
            !config.TemplateMosquitoPlayerTargets && config.DurationSeconds is >= 5 and <= 600 &&
            gate.Role == WorldStateRole.Host,
            "Progression saved state requires the explicit held Alfea28 Host two-body cohort.");
        _host = host; _log = log; _gate = gate; _process = host.ProcessHandle;
        _pid = host.ProcessPid; _thread = host.MainThreadId; _owner = Environment.CurrentManagedThreadId;
        _durationSeconds = config.DurationSeconds;
        // Its original registry-to-primary check remains strict: a second-body
        // registry cache is a failure, never permission to choose another actor.
        _probe = new(host, image, log, WorldStateRole.Host, gate, captureBeforeWriter: true);
    }

    internal bool Completed => _completed && !_failed && _gate.Healthy &&
        _gate.HostNativeReturns == checked(_hostReturnsBefore + 1) &&
        _gate.HostSuccessfulReturns == checked(_hostSuccessfulReturnsBefore + 1) &&
        _gate.GuestRejections == _guestRejectionsBefore;
    internal bool SaveAuthorizedStarted => _started;
    internal uint AuthorizationStartStop => _started ? _authorizationStartStop : 0;
    internal bool IsOwnedCompletedAtWorld(DebuggerHost host, uint tid, in Win32Native.X86Context context) =>
        Completed && ReferenceEquals(host, _host) && host.IsAtDebugStop && host.ProcessHandle == _process &&
        host.ProcessPid == _pid && host.MainThreadId == _thread && tid == _thread &&
        Environment.CurrentManagedThreadId == _owner && context.Eip == NativeCallDispatcher.WorldReturn &&
        _completionStop > _authorizationStartStop && host.DebugStopSequence >= _completionStop;
    internal object Summary => new
    {
        enabled = true, role = "Host", level = 28, authorizedLifecycleStarted = _started,
        completed = Completed, failed = _failed, failure = _failure,
        worlds = _worlds, startWorld = _startWorld, completedWorld = _completeWorld,
        authorizationStartStop = _authorizationStartStop, completionStop = _completionStop,
        hostNativeReturnsBefore = _hostReturnsBefore, hostNativeReturnsAfter = _gate.HostNativeReturns,
        hostSuccessfulReturnsBefore = _hostSuccessfulReturnsBefore, hostSuccessfulReturnsAfter = _gate.HostSuccessfulReturns,
        originalWriterSuccessAlOneRequired = true,
        guestRejectionsBefore = _guestRejectionsBefore, guestRejectionsAfter = _gate.GuestRejections,
        originalSavePoint = _probe.Summary, twoBodyCanonicalRegistryCheckRelaxed = false,
        requiresAuthorizedHeldBodyAndWorldApplyAndFreshNeutralAtStart = true,
        nativeOverlayMaySuspendGameplayAfterCapture = true, actorCompletionRequested = false,
        nativeEntryRetired = false, nativeReloadCallsAdded = 0, saveCodecDuplicated = false,
        profileOrPhysicsWritesByThisAdapter = 0, callerOwnsRetiredDispatcherPageRelease = true,
        nativeWriterReturnIsFileDurabilityProof = false, allSavedFieldsOrCoopProgressionAccepted = false
    };

    internal void StepAtWorld(uint tid, ref Win32Native.X86Context context, NativeCallDispatcher calls,
        Action resume, bool mediaVerified, bool saveAuthorized, bool nativeSharedBodyReady,
        bool currentWorldApplied, bool freshNeutral)
    {
        try
        {
            Require(!_failed && _host.IsAtDebugStop && _host.ProcessHandle == _process &&
                _host.ProcessPid == _pid && _host.MainThreadId == _thread && tid == _thread &&
                Environment.CurrentManagedThreadId == _owner && context.Eip == NativeCallDispatcher.WorldReturn,
                "Progression save left its owned main-thread WorldRET.");
            if (_completed) return;
            Require(++_worlds <= NativeObservationBudget.WorldLimit,
                "Progression save exceeded its bounded World observation budget.");
            if (!_started)
            {
                if (!mediaVerified || !saveAuthorized || !nativeSharedBodyReady || !currentWorldApplied ||
                    !freshNeutral || calls.Pending || calls.HasRetiredPage) return;
                Require(_gate.Healthy, "Progression save lost its active unpaired Host writer authority.");
                _hostReturnsBefore = _gate.HostNativeReturns; _hostSuccessfulReturnsBefore = _gate.HostSuccessfulReturns;
                _guestRejectionsBefore = _gate.GuestRejections;
                _started = true; _startWorld = _worlds; _startedMilliseconds = Environment.TickCount64;
                _authorizationStartStop = _host.DebugStopSequence;
                _log(new { @event = "progression-save-authorized-lifecycle-started", actualWorld = _worlds,
                    sharedNativeBodyReady = true, currentWorldNativeApplied = true, freshNeutralRequired = true,
                    originalCaptureAndWriterRequested = true, nativeCaptureAlreadyReturned = false,
                    existingCanonicalPrimaryRegistryGuardRequired = true });
            }
            Require(saveAuthorized && mediaVerified &&
                Environment.TickCount64 - _startedMilliseconds <= (long)_durationSeconds * 1000,
                "Progression save authorization, media admission or finite lifecycle bound was lost.");
            if (calls.Pending) return;
            // The full original capture may open a save overlay. The existing
            // probe validates its retained canonical scene through that pause;
            // startup-only body/readiness receipts are not fabricated anew.
            _probe.StepAtWorld(tid, ref context, calls, resume, mediaVerified);
            if (!_probe.ModeCompleted) return;
            Require(_probe.ModeSucceeded && _gate.Healthy && !calls.Pending && !calls.HasRetiredPage &&
                _gate.HostNativeReturns == checked(_hostReturnsBefore + 1) &&
                _gate.HostSuccessfulReturns == checked(_hostSuccessfulReturnsBefore + 1) &&
                _gate.GuestRejections == _guestRejectionsBefore,
                "Progression save lacks its single original successful AL1 Host return or later dispatcher-page release.");
            _completionStop = _host.DebugStopSequence; _completed = true; _completeWorld = _worlds;
            _log(new { @event = "progression-save-capture-writer-completed", actualWorld = _worlds,
                originalSelectedCurrentStateCaptureVerified = true, originalWriterReturnPaired = true,
                originalWriterSuccessAlOneObserved = true,
                dispatcherPageReleasedAtLaterWorld = true, nativeEntryStillOwnedByCaller = true,
                actorCompletionRequested = false, nativeReloadRequested = false,
                freshFileAndNativeLoadQualificationOwnedByCaller = true });
        }
        catch (Exception exception)
        {
            _failed = true; _failure = exception.Message;
            _log(new { @event = "progression-save-lifecycle-failed", failure = _failure,
                nativeReloadRequested = false, actorCompletionRequested = false });
            throw;
        }
    }

    private static void Require(bool condition, string message)
    { if (!condition) throw new InvalidDataException(message); }
}
