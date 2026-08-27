namespace SmoLVLcreator.Core;

/// <summary>
/// Stable stages reported by the level save pipeline. One completed step is
/// one bounded mutation of the current SMO container, not an arbitrary timer
/// tick, so UI and stress tools can display honest progress.
/// </summary>
public enum SmoLevelSaveStage
{
    Preparing,
    ReplacingModel,
    PatchingTransforms,
    ReplacingTexture,
    AddingExternalModel,
    RemovingEntity,
    AddingPlacement,
    AddingCollision,
    RepairingCollisions,
    WritingTemporaryFile,
    Installing,
    Complete
}

public sealed record SmoLevelSaveProgress(
    SmoLevelSaveStage Stage,
    int CompletedSteps,
    int TotalSteps,
    string Message,
    long CurrentContainerBytes)
{
    public double Fraction => TotalSteps <= 0
        ? 0
        : Math.Clamp((double)CompletedSteps / TotalSteps, 0, 1);
}
