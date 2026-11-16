using WinxCoop.RuntimeProbe;
namespace WinxCoop.SessionHost;

// Local monotonic limits, independent of native readiness and peer authority.
// No resource digest or UI phase can start the native-game clock by itself.
internal sealed class SessionLifecycleBudget
{
    internal const int QualificationSeconds = 1200, PreGameSeconds = 1500;
    internal const int CleanupReserveSeconds = 90, OuterReserveSeconds = 120;
    private readonly int _seconds;
    private readonly long _started;
    private long _lastObserved;
    internal long? QualifiedAt { get; private set; }
    internal long? NativeGameStartedAt { get; private set; }
    internal int NativeSupervisorMilliseconds => checked((QualificationSeconds + PreGameSeconds + _seconds + OuterReserveSeconds) * 1000);
    internal int NativeGameStopAfterMilliseconds => checked(Math.Max(15, _seconds - CleanupReserveSeconds) * 1000);

    internal SessionLifecycleBudget(int seconds, long started, bool interactive = false)
    {
        SessionConfiguration.Require(InteractiveSessionLimits.SessionDurationAllowed(seconds, interactive) && started >= 0, "Invalid local lifecycle budget.");
        _seconds = seconds; _started = started; _lastObserved = started;
    }

    internal void CompleteQualification(long now)
    {
        Check(now);
        SessionConfiguration.Require(QualifiedAt is null && NativeGameStartedAt is null, "Qualification completed more than once.");
        QualifiedAt = now;
    }

    internal void BeginNativeGame(long now)
    {
        Check(now);
        SessionConfiguration.Require(QualifiedAt is not null && NativeGameStartedAt is null,
            "Native game requires one completed local qualification and one initial start.");
        NativeGameStartedAt = now;
    }

    internal bool StopDue(long now)
    {
        Check(now);
        return NativeGameStartedAt is { } began && now - began >= NativeGameStopAfterMilliseconds;
    }

    internal void Check(long now)
    {
        SessionConfiguration.Require(now >= _lastObserved, "Local lifecycle clock rolled back.");
        _lastObserved = now;
        if (now - _started >= NativeSupervisorMilliseconds - 30000L)
            throw new TimeoutException("Local lifecycle exhausted its native-supervisor cleanup reserve.");
        if (NativeGameStartedAt is { } began)
        {
            if (now - began >= (_seconds + CleanupReserveSeconds) * 1000L)
                throw new TimeoutException("Owned native game exhausted its cleanup reserve.");
        }
        else if (QualifiedAt is { } qualified)
        {
            if (now - qualified >= PreGameSeconds * 1000L)
                throw new TimeoutException("Timed out waiting for the initial qualified peer/server start.");
        }
        else if (now - _started >= QualificationSeconds * 1000L)
            throw new TimeoutException("Complete game qualification exceeded its bounded preparation time.");
    }
}
