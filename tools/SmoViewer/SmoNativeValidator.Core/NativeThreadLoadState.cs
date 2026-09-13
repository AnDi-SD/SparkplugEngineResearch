namespace SmoNativeValidator.Core;

internal sealed record ResourceLoadFrame(long Id, uint ThreadId, bool Target);

internal readonly record struct ResourceLoadFramePopResult(
    bool Found,
    int AbandonedNestedFrames);

internal readonly record struct NativeThreadLoadCleanup(
    int RemovedFrames,
    bool RemovedPendingRedirect);

/// <summary>
/// Tracks nested ResourceLoad calls independently for each debuggee thread.
/// The top frame is the only authoritative context for generic serializer
/// checkpoints; an outer target load must not lend its identity to a nested
/// non-target resource.
/// </summary>
internal sealed class NativeThreadLoadState
{
    private readonly Dictionary<uint, List<ResourceLoadFrame>> _framesByThread = [];
    private readonly HashSet<uint> _pendingRedirectThreads = [];
    private long _nextFrameId;

    internal void MarkTargetPathRedirected(uint threadId) =>
        _pendingRedirectThreads.Add(threadId);

    internal bool ConsumePendingTargetRedirect(uint threadId) =>
        _pendingRedirectThreads.Remove(threadId);

    internal ResourceLoadFrame Push(uint threadId, bool target)
    {
        if (!_framesByThread.TryGetValue(threadId, out List<ResourceLoadFrame>? frames))
        {
            frames = [];
            _framesByThread.Add(threadId, frames);
        }

        ResourceLoadFrame frame = new(
            Interlocked.Increment(ref _nextFrameId),
            threadId,
            target);
        frames.Add(frame);
        return frame;
    }

    internal bool IsCurrentTarget(uint threadId) =>
        _framesByThread.TryGetValue(threadId, out List<ResourceLoadFrame>? frames) &&
        frames.Count > 0 &&
        frames[^1].Target;

    internal ResourceLoadFramePopResult Pop(uint threadId, long frameId)
    {
        if (!_framesByThread.TryGetValue(threadId, out List<ResourceLoadFrame>? frames))
            return default;

        int index = frames.FindLastIndex(frame => frame.Id == frameId);
        if (index < 0)
            return default;

        int abandonedNestedFrames = frames.Count - index - 1;
        frames.RemoveRange(index, frames.Count - index);
        if (frames.Count == 0)
            _framesByThread.Remove(threadId);

        return new ResourceLoadFramePopResult(true, abandonedNestedFrames);
    }

    internal NativeThreadLoadCleanup RemoveThread(uint threadId)
    {
        int removedFrames = _framesByThread.Remove(
            threadId,
            out List<ResourceLoadFrame>? frames)
            ? frames.Count
            : 0;
        bool removedPendingRedirect = _pendingRedirectThreads.Remove(threadId);
        return new NativeThreadLoadCleanup(removedFrames, removedPendingRedirect);
    }
}
