using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

// E7-only local operation receipt. It preserves an actual private-controller
// neutral interval before the Guest deliberately enters prepared Waiting.
// Waiting is not source Ready and never adds an input/native World ACK.
internal sealed partial class NativeTemplateProbe
{
    private readonly bool _preWaitingReloadPreparationEnabled;
    private ulong _preWaitingReloadOperation, _preWaitingReloadInvalidatedOperation;
    private uint _preWaitingReloadStop, _preWaitingReloadPermissionChanges;
    private long _preWaitingReloadMilliseconds;
    private bool _preWaitingReloadActive;
    private uint _preWaitingReloadInvalidations, _preWaitingReloadConsumptions;

    private object PreWaitingReloadPreparationSummary => new
    {
        enabled = _preWaitingReloadPreparationEnabled, active = _preWaitingReloadActive,
        operation = _preWaitingReloadOperation, actualWorldStop = _preWaitingReloadStop,
        permissionChanges = _preWaitingReloadPermissionChanges,
        invalidatedOperation = _preWaitingReloadInvalidatedOperation,
        invalidations = _preWaitingReloadInvalidations, consumptions = _preWaitingReloadConsumptions,
        maximumRetainedMilliseconds = 60000, nativeGameplayOrHealthWrites = 0,
        sourceReadyOrNativeAcknowledgmentAdded = false
    };

    internal bool ObserveProgressionReloadPreparationAtWorld(uint tid, ref Win32Native.X86Context context,
        ulong operation, bool preparing, bool transitioning, bool currentWorldApplied,
        bool primaryNeutral, bool originalCheckpointBootstrapCompleted)
    {
        if (!_preWaitingReloadPreparationEnabled) return false;
        SharedBattleWorld(tid, ref context);
        if (!_sharedBattleCaptured || _sharedBattleUnloadStarted || _progressionSavedReloadPrepared) return false;
        bool sameCommand = operation != 0 && (preparing || transitioning);
        bool ready = ReadSharedBattleFlow(out _, out _, out _, out _, out _);
        bool liveHeld = _progressionSavedReloadCohort && _stage == 6 && _idleActive && !_dependentInterval &&
            _healthBattleHolding && ready && !ProgressionInputAllowed && !_progressionInputFencePending &&
            _networkInputStarted && !_networkInputRetired && !_networkInputDisconnected &&
            _networkInputFailure.Length == 0 && _networkInputLastAppliedNeutral &&
            _sharedHealthAlias is { Active: true, Retired: false } &&
            currentWorldApplied && primaryNeutral && Environment.TickCount64 <= _sharedBattleDeadline;
        if (_preWaitingReloadActive && (!sameCommand || operation != _preWaitingReloadOperation || !liveHeld ||
            _preWaitingReloadPermissionChanges != _progressionInputChanges ||
            Environment.TickCount64 - _preWaitingReloadMilliseconds > 60000))
        {
            _preWaitingReloadActive = false; _preWaitingReloadInvalidatedOperation = _preWaitingReloadOperation;
            ++_preWaitingReloadInvalidations;
            _log(new { @event = "template-progression-pre-waiting-reload-invalidated",
                operation = _preWaitingReloadOperation, actualWorldStop = _host.DebugStopSequence,
                sameCommand, liveHeld, originalReceiptStop = _preWaitingReloadStop,
                inputOrNativeAcknowledgmentAdded = false });
        }
        if (!_preWaitingReloadActive && preparing && sameCommand && liveHeld &&
            operation > _preWaitingReloadInvalidatedOperation && originalCheckpointBootstrapCompleted &&
            ProgressionNeutralNativeApplied)
        {
            VerifySharedBattleBorrowedOwners(fullLocal: true); VerifySharedBattleOwnGraph();
            VerifySharedBattleTemplateOwner(true); VerifyRetainedInputBodyOwners(); NetworkInputNeutralReadback();
            _preWaitingReloadOperation = operation; _preWaitingReloadStop = _host.DebugStopSequence;
            _preWaitingReloadPermissionChanges = _progressionInputChanges;
            _preWaitingReloadMilliseconds = Environment.TickCount64; _preWaitingReloadActive = true;
            _log(new { @event = "template-progression-pre-waiting-reload-prepared", operation,
                actualWorldStop = _preWaitingReloadStop, permissionChanges = _preWaitingReloadPermissionChanges,
                actualPrimaryNativeNeutral = primaryNeutral, actualPrivateNativeNeutral = true,
                actualCurrentConnectionNativeWorldApplied = currentWorldApplied,
                originalCheckpointBootstrapCompleted, actualReadyGameplayWorld = true,
                inputRemainsClosed = true, sharedHealthAliasStillActive = true,
                nativeGameplayOrHealthWrites = 0, sourceReadyOrNativeAcknowledgmentAdded = false });
        }
        return _preWaitingReloadActive && sameCommand && liveHeld && operation == _preWaitingReloadOperation &&
            _preWaitingReloadPermissionChanges == _progressionInputChanges &&
            Environment.TickCount64 - _preWaitingReloadMilliseconds <= 60000;
    }

    private bool HasPreWaitingReloadPreparation(ulong operation) => _preWaitingReloadPreparationEnabled &&
        _preWaitingReloadActive && operation != 0 && operation == _preWaitingReloadOperation &&
        _preWaitingReloadStop != 0 && _host.DebugStopSequence >= _preWaitingReloadStop &&
        _preWaitingReloadPermissionChanges == _progressionInputChanges &&
        Environment.TickCount64 - _preWaitingReloadMilliseconds <= 60000;
}
