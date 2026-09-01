using System.Numerics;
using SmoImporter.Core;

internal static class GeneratedSkinningQualityGuardRegression
{
    public static void Run()
    {
        GlbSkinTransferPlan healthyPlan = Plan(activeJointCount: 4);
        GlbSkinTransferPlan healthy = GeneratedSkinningQualityGuard.Apply(
            Analysis(targetJointCount: 52, preparedVertexCount: 1000,
                headVertexCount: 700),
            healthyPlan);
        Require(ReferenceEquals(healthy, healthyPlan),
            "the exact permitted boundary must not rewrite a healthy plan");

        GlbSkinTransferPlan rigidHead = GeneratedSkinningQualityGuard.Apply(
            Analysis(targetJointCount: 52, preparedVertexCount: 1000,
                headVertexCount: 701),
            Plan(activeJointCount: 52));
        Require(!rigidHead.CanReplace && rigidHead.Messages.Any(message =>
                message.Contains("Head", StringComparison.Ordinal)),
            "a greater-than-70-percent rigid Head result must be rejected");

        GlbSkinTransferPlan collapsedJoints = GeneratedSkinningQualityGuard.Apply(
            Analysis(targetJointCount: 52, preparedVertexCount: 1000,
                headVertexCount: 0),
            Plan(activeJointCount: 2));
        Require(!collapsedJoints.CanReplace && collapsedJoints.Messages.Any(message =>
                message.Contains("only 2", StringComparison.Ordinal)),
            "a two-joint humanoid result must be rejected");

        GlbSkinTransferPlan smallRigidAsset = GeneratedSkinningQualityGuard.Apply(
            Analysis(targetJointCount: 4, preparedVertexCount: 80,
                headVertexCount: 0),
            Plan(activeJointCount: 1));
        Require(smallRigidAsset.CanReplace,
            "the humanoid guard must not reject small/non-humanoid assets");

        Console.WriteLine("GENERATED QUALITY GUARD REGRESSION PASS");
    }

    private static GeneratedSkinningAnalysis Analysis(
        int targetJointCount,
        int preparedVertexCount,
        int headVertexCount) =>
        new(
            new GeneratedSkinningAlignment(1, Vector3.Zero),
            targetJointCount,
            0,
            0,
            0,
            0,
            preparedVertexCount,
            4,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            Array.Empty<GeneratedSkinningAttachment>(),
            Array.Empty<string>(),
            RequiresConfirmation: false)
        {
            HeadProtectedComponentVertexCount = headVertexCount
        };

    private static GlbSkinTransferPlan Plan(int activeJointCount) =>
        new(
            SmoSkeletonCompatibility.Exact,
            MeshCount: 1,
            MaterialGroupCount: 1,
            JointCount: 52,
            ActiveJointCount: activeJointCount,
            DifferentBindPoseJointCount: 0,
            MatchedBoneNames: Array.Empty<string>(),
            RemappedBones: Array.Empty<GlbBoneRemap>(),
            UnusedGlbJoints: Array.Empty<string>(),
            TargetBonesWithoutWeights: Array.Empty<string>(),
            Messages: Array.Empty<string>());

    private static void Require(bool condition, string message)
    {
        if (!condition)
            throw new InvalidOperationException(message);
    }
}
