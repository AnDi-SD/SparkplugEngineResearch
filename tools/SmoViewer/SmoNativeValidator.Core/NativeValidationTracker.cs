namespace SmoNativeValidator.Core;

public sealed class NativeValidationTracker
{
    private readonly List<NativeValidationEvent> _events = [];

    public IReadOnlyList<NativeValidationEvent> Events => _events;
    public NativeValidationEvent? LastEvent => _events.Count == 0 ? null : _events[^1];
    public string? LastCheckpoint { get; private set; }
    public string? LastTargetCheckpoint { get; private set; }
    public DateTimeOffset? LastProgressUtc { get; private set; }
    public bool TargetRedirected { get; private set; }
    public bool TargetLoadEntered { get; private set; }
    public bool TargetLoadReturned { get; private set; }
    public bool TargetLoadAccepted { get; private set; }
    public bool TargetFfpsHeaderEntered { get; private set; }
    public bool TargetFfpsMagicAccepted { get; private set; }
    public bool TargetFfpsVersionAccepted { get; private set; }
    public bool SceneReadyReached { get; private set; }
    public int RedirectCount { get; private set; }
    public NativeExceptionSnapshot? LastException { get; private set; }

    public void Apply(NativeValidationEvent validationEvent)
    {
        ArgumentNullException.ThrowIfNull(validationEvent);
        _events.Add(validationEvent);
        LastProgressUtc = validationEvent.TimestampUtc;
        if (!string.IsNullOrWhiteSpace(validationEvent.Checkpoint))
            LastCheckpoint = validationEvent.Checkpoint;

        if (validationEvent.Kind == NativeValidationEventKind.PathRedirected)
        {
            TargetRedirected = true;
            RedirectCount++;
            LastTargetCheckpoint = validationEvent.Checkpoint;
        }

        if (validationEvent.Kind == NativeValidationEventKind.SceneReady)
            SceneReadyReached = true;

        bool targetContext = IsTrue(validationEvent.Data, "target") ||
            IsTrue(validationEvent.Data, "targetContext");

        if (validationEvent.Kind == NativeValidationEventKind.CheckpointEnter &&
            targetContext)
        {
            if (!string.IsNullOrWhiteSpace(validationEvent.Checkpoint))
                LastTargetCheckpoint = validationEvent.Checkpoint;

            switch (validationEvent.Checkpoint)
            {
                case "CP03":
                    TargetLoadEntered = true;
                    break;
                case "FFPS01":
                    TargetFfpsHeaderEntered = true;
                    break;
                case "FFPS02":
                    TargetFfpsMagicAccepted = true;
                    break;
                case "FFPS03":
                    TargetFfpsVersionAccepted = true;
                    break;
            }
        }

        if (validationEvent.Kind == NativeValidationEventKind.CheckpointReturn &&
            validationEvent.Data.TryGetValue("target", out string? returnTarget) &&
            string.Equals(returnTarget, "true", StringComparison.OrdinalIgnoreCase))
        {
            TargetLoadReturned = true;
            TargetLoadAccepted = validationEvent.Data.TryGetValue("accepted", out string? accepted) &&
                string.Equals(accepted, "true", StringComparison.OrdinalIgnoreCase);
        }
    }

    public void SetException(NativeExceptionSnapshot snapshot) => LastException = snapshot;

    private static bool IsTrue(
        IReadOnlyDictionary<string, string?> data,
        string key) =>
        data.TryGetValue(key, out string? value) &&
        string.Equals(value, "true", StringComparison.OrdinalIgnoreCase);
}
