namespace WinxCoop.RuntimeProbe;

// Local lifetime/storage arithmetic only. This never supplies native readiness,
// a scene admission, an IPC credit, or a longer freshness/owner lease.
internal static class InteractiveSessionLimits
{
    internal const int DefaultSeconds = 7200, MaximumSeconds = 14400;
    internal const int LegacyMaximumSeconds = 600;
    internal const int NativeParentMaximumMilliseconds = 17220000;
    internal const long CommandJournalBytes = 32L * 1024 * 1024;
    internal const int StatusJournalBytes = 256 * 1024 * 1024;
    internal const long ActorJournalBytes = 2L * 1024 * 1024 * 1024;
    internal const int ActorRecordBytes = 1024 * 1024;
    internal const int MaximumRetainedGenerations = 8;

    internal static bool SessionDurationAllowed(int seconds, bool interactive) =>
        seconds >= 60 && seconds <= (interactive ? MaximumSeconds : LegacyMaximumSeconds);
    internal static bool ActorDurationAllowed(int seconds, bool interactive) =>
        seconds >= (interactive ? 60 : 5) && seconds <= (interactive ? MaximumSeconds : LegacyMaximumSeconds);
    internal static uint InputSamples(int seconds) => checked((uint)seconds * 1000);
    internal static uint Snapshots(int seconds) => checked((uint)seconds * 20 + 128);
}
