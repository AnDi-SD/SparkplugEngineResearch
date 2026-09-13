namespace SmoNativeValidator.Core;

internal readonly record struct NativeCrashContext(
    bool CurrentThreadTarget,
    bool TargetRedirected,
    bool TargetLoadEntered,
    bool TargetLoadReturned,
    bool TargetLoadAccepted,
    bool TargetAcceptanceObserved);

internal sealed record NativeCrashAttribution(
    NativeCrashPhase Phase,
    NativeCrashAttributionConfidence Confidence,
    string EventPhase,
    string Description)
{
    internal bool ModelDirectlyAttributed =>
        Confidence == NativeCrashAttributionConfidence.Direct;
}

internal static class NativeCrashAttributionClassifier
{
    internal static NativeCrashAttribution Classify(NativeCrashContext context)
    {
        if (context.CurrentThreadTarget)
        {
            return new NativeCrashAttribution(
                NativeCrashPhase.DuringTargetLoad,
                NativeCrashAttributionConfidence.Direct,
                "during-target-load",
                "The fault occurred in the active redirected model load.");
        }

        if (context.TargetLoadReturned &&
            context.TargetLoadAccepted &&
            context.TargetAcceptanceObserved)
        {
            return new NativeCrashAttribution(
                NativeCrashPhase.PostReturnSurvivalWindow,
                NativeCrashAttributionConfidence.Possible,
                "post-return-survival",
                "The redirected model returned successfully, then a temporally correlated fault occurred during the survival window; this does not prove model causation.");
        }

        if (!context.TargetRedirected)
        {
            return new NativeCrashAttribution(
                NativeCrashPhase.BeforeTrigger,
                NativeCrashAttributionConfidence.None,
                "before-trigger",
                "The fault occurred before the trigger resource was requested and is not attributable to the selected model.");
        }

        if (context.TargetLoadEntered && !context.TargetLoadReturned)
        {
            return new NativeCrashAttribution(
                NativeCrashPhase.TargetLoadBackgroundOrUnwind,
                NativeCrashAttributionConfidence.Possible,
                "target-load-background-or-unwind",
                "The fault followed target-load entry but was outside its directly correlated thread context; model attribution remains possible but inconclusive.");
        }

        return new NativeCrashAttribution(
            NativeCrashPhase.AfterTriggerUnattributed,
            NativeCrashAttributionConfidence.Possible,
            "after-trigger-unattributed",
            "The trigger was redirected, but the fault was outside a directly correlated target load; model attribution remains possible but inconclusive.");
    }
}
