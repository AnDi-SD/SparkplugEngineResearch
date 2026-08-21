using SmoViewer.Core;

namespace SmoImporter.Core;

/// <summary>
/// Describes how donor geometry is prepared for the immutable game skeleton.
/// The legacy single-bone rigid attachment path is intentionally separate from
/// these character-rigging modes.
/// </summary>
public enum ModelPortingMode
{
    PreparedGameSkeleton = 0,
    AdaptDonorWeights = 1,
    GenerateWeights = 2
}

public sealed record ModelPortingModeRecommendation(
    ModelPortingMode Mode,
    string Reason,
    GlbSkinTransferPlan? PreparedSkeletonPlan = null);

/// <summary>
/// Makes a conservative recommendation. It never treats a texture-resolution
/// problem as a skeleton problem: the target textures are preserved while the
/// strict prepared-skeleton path is probed.
/// </summary>
public static class ModelPortingModeAnalyzer
{
    public static ModelPortingModeRecommendation Recommend(
        SmoDocument target,
        ImportedScene donor)
    {
        ArgumentNullException.ThrowIfNull(target);
        ArgumentNullException.ThrowIfNull(donor);

        int skinnedMeshCount = donor.Meshes.Count(mesh => mesh.Skinning is not null);
        if (skinnedMeshCount == 0)
        {
            return new ModelPortingModeRecommendation(
                ModelPortingMode.GenerateWeights,
                "The donor has no usable skin weights.");
        }

        if (skinnedMeshCount != donor.Meshes.Count)
        {
            return new ModelPortingModeRecommendation(
                ModelPortingMode.GenerateWeights,
                $"The donor mixes {skinnedMeshCount} skinned and " +
                $"{donor.Meshes.Count - skinnedMeshCount} unskinned meshes. " +
                "A partial rig cannot be adapted without guessing how the " +
                "unskinned surfaces should follow it, so the safe default is to " +
                "ignore the donor rig and generate one coherent weight set.");
        }

        try
        {
            GlbSkinTransferPlan plan = SmoSkinnedGlbReplacer.Analyze(
                target,
                donor,
                SkinnedTextureTransferMode.PreserveTarget);
            if (plan.CanReplace)
            {
                return new ModelPortingModeRecommendation(
                    ModelPortingMode.PreparedGameSkeleton,
                    "All active donor joints have a verified game-skeleton mapping.",
                    plan);
            }

            return RecommendAfterStrictFailure(
                target,
                donor,
                "Skin weights are present, but the donor skeleton is not strictly " +
                "compatible with the game skeleton.",
                plan);
        }
        catch (Exception exception) when (exception is InvalidDataException or
                                          InvalidOperationException or
                                          OverflowException or
                                          ArgumentException)
        {
            return RecommendAfterStrictFailure(
                target,
                donor,
                "Skin weights are present, but strict skeleton analysis failed: " +
                exception.Message,
                strictPlan: null);
        }
    }

    private static ModelPortingModeRecommendation RecommendAfterStrictFailure(
        SmoDocument target,
        ImportedScene donor,
        string strictFailureReason,
        GlbSkinTransferPlan? strictPlan)
    {
        try
        {
            SkinnedModelPortingAnalysis adaptation =
                SkinnedModelPortingPreparer.AnalyzeAdaptDonorWeights(
                    target,
                    donor);
            if (adaptation.CanPrepare)
            {
                return new ModelPortingModeRecommendation(
                    ModelPortingMode.AdaptDonorWeights,
                    strictFailureReason + " Conservative mode-2 analysis can map " +
                    "every active donor weight to the game skeleton.",
                    strictPlan);
            }

            string adaptationFailure = adaptation.Errors.Count == 0
                ? "mode-2 analysis rejected the donor without a more specific error"
                : string.Join(" | ", adaptation.Errors);
            return new ModelPortingModeRecommendation(
                ModelPortingMode.GenerateWeights,
                strictFailureReason + " Mode 2 also cannot adapt the donor safely: " +
                adaptationFailure + ". The safe default is to ignore the donor rig " +
                "and generate a coherent weight set from geometry.",
                strictPlan);
        }
        catch (Exception exception) when (exception is InvalidDataException or
                                          InvalidOperationException or
                                          OverflowException or
                                          ArgumentException)
        {
            return new ModelPortingModeRecommendation(
                ModelPortingMode.GenerateWeights,
                strictFailureReason + " Mode-2 safety analysis failed: " +
                exception.Message + ". The safe default is to ignore the donor rig " +
                "and generate a coherent weight set from geometry.",
                strictPlan);
        }
    }
}
