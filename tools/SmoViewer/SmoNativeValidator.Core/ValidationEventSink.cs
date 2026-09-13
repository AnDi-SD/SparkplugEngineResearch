using System.Collections.ObjectModel;
using System.Diagnostics;

namespace SmoNativeValidator.Core;

internal sealed class ValidationEventSink
{
    private readonly Stopwatch _stopwatch;
    private readonly NativeSessionLog _log;
    private readonly IProgress<NativeValidationEvent>? _progress;
    private long _sequence;

    internal ValidationEventSink(
        DateTimeOffset startedUtc,
        Stopwatch stopwatch,
        NativeSessionLog log,
        NativeValidationTracker tracker,
        IProgress<NativeValidationEvent>? progress)
    {
        StartedUtc = startedUtc;
        _stopwatch = stopwatch;
        _log = log;
        Tracker = tracker;
        _progress = progress;
    }

    internal DateTimeOffset StartedUtc { get; }
    internal NativeValidationTracker Tracker { get; }

    internal NativeValidationEvent Emit(
        NativeValidationEventKind kind,
        NativeValidationStage stage,
        NativeValidationSeverity severity,
        string message,
        double? progressPercent = null,
        uint? address = null,
        uint? threadId = null,
        string? checkpoint = null,
        IReadOnlyDictionary<string, string?>? data = null)
    {
        NativeValidationEvent validationEvent = new()
        {
            Sequence = Interlocked.Increment(ref _sequence),
            TimestampUtc = DateTimeOffset.UtcNow,
            Elapsed = _stopwatch.Elapsed,
            Kind = kind,
            Stage = stage,
            Severity = severity,
            Message = message,
            ProgressPercent = progressPercent,
            Address = address,
            ThreadId = threadId,
            Checkpoint = checkpoint,
            Data = data is null
                ? new Dictionary<string, string?>()
                : new ReadOnlyDictionary<string, string?>(
                    new Dictionary<string, string?>(data, StringComparer.OrdinalIgnoreCase))
        };
        Tracker.Apply(validationEvent);
        _log.Write(validationEvent);
        _progress?.Report(validationEvent);
        return validationEvent;
    }
}
