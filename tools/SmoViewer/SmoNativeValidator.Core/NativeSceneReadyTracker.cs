namespace SmoNativeValidator.Core;

internal readonly record struct NativeGameFlowSnapshot(
    uint CurrentStateId,
    uint PendingStateId,
    int StackIndex,
    uint ActiveStateId);

/// <summary>
/// Recognizes completion of a requested native level state without relying on
/// the game's buffered GameStateLog.txt stream.
/// </summary>
internal sealed class NativeSceneReadyTracker
{
    private readonly uint _expectedStateId;

    internal NativeSceneReadyTracker(int expectedStateId)
    {
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(expectedStateId);
        _expectedStateId = checked((uint)expectedStateId);
    }

    internal bool TargetStateObserved { get; private set; }
    internal bool TransitionQueueIdle { get; private set; }
    internal bool ActiveStackStateMatched { get; private set; }
    internal bool SceneReady { get; private set; }

    internal bool Observe(NativeGameFlowSnapshot snapshot, bool targetAccepted)
    {
        if (SceneReady)
            return true;

        TargetStateObserved = snapshot.CurrentStateId == _expectedStateId;
        TransitionQueueIdle = snapshot.PendingStateId == 0;
        ActiveStackStateMatched = snapshot.StackIndex >= 0 &&
            snapshot.ActiveStateId == _expectedStateId;
        SceneReady = targetAccepted && TargetStateObserved &&
            TransitionQueueIdle && ActiveStackStateMatched;
        return SceneReady;
    }
}
