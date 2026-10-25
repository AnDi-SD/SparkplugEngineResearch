namespace SmoImporter.Core;

/// <summary>
/// Rejects generated humanoid skinning which is structurally writable but has
/// collapsed to a non-articulating result. Static or deliberately rigid imports
/// use their own modes and do not pass through this guard.
/// </summary>
public static class GeneratedSkinningQualityGuard
{
    public const int MinimumHumanoidActiveJointCount = 4;
    public const float MaximumRigidHeadVertexFraction = 0.70f;

    public static GlbSkinTransferPlan Apply(
        GeneratedSkinningAnalysis analysis,
        GlbSkinTransferPlan plan)
    {
        ArgumentNullException.ThrowIfNull(analysis);
        ArgumentNullException.ThrowIfNull(plan);

        var blockers = new List<string>();
        if (analysis.PreparedVertexCount > 0 &&
            analysis.HeadProtectedComponentVertexCount >
                analysis.PreparedVertexCount * MaximumRigidHeadVertexFraction)
        {
            blockers.Add(
                "Generated humanoid skinning is pathological: rigid Head " +
                $"ownership captured {analysis.HeadProtectedComponentVertexCount}/" +
                $"{analysis.PreparedVertexCount} donor vertices. Adjust or " +
                "disable Head protection before writing SMO.");
        }

        if (analysis.TargetDeformJointCount >= 12 &&
            analysis.PreparedVertexCount >= 100 &&
            plan.ActiveJointCount < MinimumHumanoidActiveJointCount)
        {
            blockers.Add(
                "Generated humanoid skinning is pathological: only " +
                $"{plan.ActiveJointCount} target joints have active weights; " +
                $"at least {MinimumHumanoidActiveJointCount} are required. " +
                "Use static mode for a deliberately rigid model or correct the " +
                "semantic-region/alignment result.");
        }

        if (blockers.Count == 0)
            return plan;

        return plan with
        {
            Compatibility = SmoSkeletonCompatibility.Incompatible,
            Messages = plan.Messages.Concat(blockers).ToArray()
        };
    }
}
