using System.Collections.ObjectModel;
using System.Numerics;
using SmoExporter.Core;
using SmoViewer.Core;

namespace SmoImporter.Core;

/// <summary>
/// Uniform external-space alignment. The automatic path derives scale and
/// translation from connected surfaces; the explicit path may also carry a
/// user-authored XYZ rotation.
/// </summary>
public sealed record GeneratedSkinningAlignment(
    float Scale,
    Vector3 Translation)
{
    public Vector3 RotationDegrees { get; init; }

    public Matrix4x4 Matrix =>
        Matrix4x4.CreateScale(Scale) *
        Matrix4x4.CreateFromYawPitchRoll(
            Degrees(RotationDegrees.Y),
            Degrees(RotationDegrees.X),
            Degrees(RotationDegrees.Z)) *
        Matrix4x4.CreateTranslation(Translation);

    private static float Degrees(float value) => value * MathF.PI / 180f;
}

/// <summary>
/// Semantic targets supported by explicit detached-component assignments.
/// The values intentionally resolve to exact target deform-joint names:
/// <see cref="UpperBack"/> to <c>Spine_03</c> and <see cref="Head"/> to
/// <c>Head</c>.
/// </summary>
public enum GeneratedSkinningComponentAttachmentTarget
{
    UpperBack,
    Head
}

/// <summary>
/// An exact connected-surface assignment captured in original donor vertex
/// coordinates. Alignment is deliberately not part of this identity: changing
/// a finite rotation, positive uniform scale and translation cannot change
/// component topology.
/// </summary>
public sealed record GeneratedSkinningComponentOverride(
    int ComponentIndex,
    GeneratedSkinningComponentAttachmentTarget Target,
    IReadOnlyList<TargetRigBodyVertexMembership> VerticesByMesh);

/// <summary>
/// Immutable input contract for manual detached-component assignments. Both
/// fingerprints and every original vertex index are validated again by
/// <see cref="GeneratedSkinningPreparer.Prepare(SmoDocument, ImportedScene, ReplacementTransform, TargetRigBodySelection, GeneratedSkinningComponentOverrides)"/>.
/// </summary>
public sealed record GeneratedSkinningComponentOverrides(
    IReadOnlyList<GeneratedSkinningComponentOverride> Components,
    int TotalComponentCount,
    string TargetRigFingerprint,
    string DonorGeometryFingerprint);

/// <summary>
/// A disconnected donor surface that was kept rigid and assigned to one safe
/// target deform joint. The assignment is diagnostic until the caller confirms it.
/// </summary>
public sealed record GeneratedSkinningAttachment(
    int ComponentIndex,
    IReadOnlyList<int> MeshIndices,
    IReadOnlyList<string> MeshNames,
    int VertexCount,
    int TriangleCount,
    string TargetBoneName,
    int TargetSkeletonJointIndex,
    float DistanceToBone,
    Vector3 AlignedCenter)
{
    /// <summary>Exact original donor vertices in this connected surface.</summary>
    public IReadOnlyList<TargetRigBodyVertexMembership> VerticesByMesh { get; init; } =
        Array.Empty<TargetRigBodyVertexMembership>();

    /// <summary>
    /// Non-null when the rigid one-hot assignment was supplied explicitly.
    /// </summary>
    public GeneratedSkinningComponentAttachmentTarget? ManualAssignment { get; init; }

    /// <summary>
    /// Non-null when a detached component was proven to be a geometric companion
    /// of an applied semantic region. Explicit manual assignments take precedence.
    /// </summary>
    public GeneratedSkinningSemanticRegion? SemanticAssignment { get; init; }

    /// <summary>
    /// Non-null when a whole detached component was classified by the enabled
    /// protective Head rule or the strict-majority Back rule. This is still
    /// covered by the global confirmation gate; explicit manual assignments
    /// always take precedence.
    /// </summary>
    public GeneratedSkinningComponentAttachmentTarget? PlaneAssignment { get; init; }

    /// <summary>
    /// Planes crossing this connected surface without placing a strict majority
    /// on the attachment side. Such a component is never split and requires a
    /// whole-component manual choice.
    /// </summary>
    public IReadOnlyList<GeneratedSkinningSeparationPlaneKind> IntersectedPlanes
        { get; init; } = Array.Empty<GeneratedSkinningSeparationPlaneKind>();

    public bool RequiresManualPlaneAssignment =>
        ManualAssignment is null && IntersectedPlanes.Count > 0;
}

public enum GeneratedSkinningAutomaticComponentBinding
{
    GeneratedWeights,
    Head,
    UpperBack
}

/// <summary>
/// One complete connected donor surface exposed to the manual exception UI.
/// Unlike <see cref="GeneratedSkinningAttachment"/>, this list contains every
/// renderable component, including surfaces which currently use generated
/// deform weights.
/// </summary>
public sealed record GeneratedSkinningComponentInfo(
    int ComponentIndex,
    IReadOnlyList<int> MeshIndices,
    IReadOnlyList<string> MeshNames,
    int VertexCount,
    int TriangleCount,
    Vector3 AlignedCenter,
    IReadOnlyList<TargetRigBodyVertexMembership> VerticesByMesh,
    GeneratedSkinningAutomaticComponentBinding AutomaticBinding,
    GeneratedSkinningComponentAttachmentTarget? ManualAssignment);

public sealed record GeneratedSkinningAnalysis(
    GeneratedSkinningAlignment Alignment,
    int TargetDeformJointCount,
    int TargetMainComponentVertexCount,
    int TargetMainComponentTriangleCount,
    int DonorMainComponentVertexCount,
    int DonorMainComponentTriangleCount,
    int PreparedVertexCount,
    int MaximumInfluencesPerVertex,
    float MaximumDiscardedTopFourWeightMass,
    float MeanDiscardedTopFourWeightMass,
    float MaximumTopFourToFinalWeightL1Distance,
    float MeanTopFourToFinalWeightL1Distance,
    int FittingPoseComparisonVertexCount,
    float MaximumTopFourToFinalFittingPositionDelta,
    float RmsTopFourToFinalFittingPositionDelta,
    IReadOnlyList<GeneratedSkinningAttachment> Attachments,
    IReadOnlyList<string> Warnings,
    bool RequiresConfirmation)
{
    /// <summary>Identity needed to persist component overrides safely.</summary>
    public string TargetRigFingerprint { get; init; } = string.Empty;

    /// <summary>Identity needed to persist component overrides safely.</summary>
    public string DonorGeometryFingerprint { get; init; } = string.Empty;

    /// <summary>Total connected surfaces in the donor topology.</summary>
    public int DonorComponentCount { get; init; }

    /// <summary>Every renderable connected donor surface and its current binding.</summary>
    public IReadOnlyList<GeneratedSkinningComponentInfo> Components { get; init; } =
        Array.Empty<GeneratedSkinningComponentInfo>();

    /// <summary>Smooth vertices whose capsule score was reduced by a torso field.</summary>
    public int AnatomicalVolumeAffectedVertexCount { get; init; }

    /// <summary>
    /// Smooth vertices which use only the target-bone capsule field.
    /// </summary>
    public int CapsuleOnlyVertexCount { get; init; }

    /// <summary>Selected-body vertices handled by a semantic core/transition.</summary>
    public int SemanticRegionAppliedVertexCount { get; init; }

    /// <summary>
    /// Renderable donor vertices promoted from complete disconnected components
    /// to rigid one-hot Head ownership before smooth capsule weighting.
    /// </summary>
    public int HeadProtectedComponentVertexCount { get; init; }

    /// <summary>
    /// Smooth lower-body vertices for which the sagittal wall removed every
    /// opposite-side leg capsule before weight ranking.
    /// </summary>
    public int LowerBodyWallAffectedVertexCount { get; init; }

    /// <summary>
    /// Smooth vertices for which at least one enabled shoulder wall removed
    /// arm or central-body capsules before weight ranking.
    /// </summary>
    public int ShoulderWallAffectedVertexCount { get; init; }

    /// <summary>
    /// Target-calibrated Hand volumes, hard Head plane and their exact captured
    /// original-donor vertex memberships.
    /// </summary>
    public GeneratedSkinningRegionAnalysis SemanticRegions { get; init; } =
        new(
            Array.Empty<GeneratedSkinningRegionResolution>(),
            string.Empty,
            string.Empty,
            string.Empty,
            string.Empty);

    /// <summary>
    /// Internal instrumentation for the single preparation and semantic-region
    /// resolution performed by one public call.
    /// </summary>
    internal int InternalPreparationPassCount { get; init; }

    internal int SemanticResolutionPassCount { get; init; }
}

public sealed record GeneratedSkinningPreparationResult(
    GeneratedSkinningAnalysis Analysis,
    ImportedScene PreparedScene)
{
    /// <summary>
    /// Alignment-applied donor geometry with generated target weights, arranged
    /// around the temporary fitting pose. With the reset pose it is identical to
    /// <see cref="PreparedScene"/>.
    /// </summary>
    public ImportedScene FittingPreviewScene { get; init; } = PreparedScene;
}

public sealed record GeneratedSkinningProgress(
    double Fraction,
    string Stage);

/// <summary>
/// Conservative prototype for mode 3. It aligns an unskinned donor to the
/// target bind geometry, generates up to four normalized capsule weights per
/// body vertex, and reduces that limit only when the exact target palette plan
/// requires it. Every disconnected donor component is kept as a rigid
/// attachment which must be confirmed by a later UI layer. The donor is a
/// prealigned input: it must already be upright, Y-up, and face the same
/// direction as the target. An unskinned surface cannot prove front/back or
/// detect a mirrored character reliably, so this API never rotates or reflects it.
/// </summary>
public static partial class GeneratedSkinningPreparer
{
    // Start with the full nearest-four result and reduce it only when the exact
    // target palette plan proves that result cannot fit. Three is the highest
    // compatible limit observed for Bloom/Layla; two remains the preferred
    // conservative retry. One is an emergency, palette-proven fallback for an
    // otherwise unwritable pose and is selected only after 4/3/2 all fail.
    private const int TopFourComparisonInfluences = 4;
    private const float PositionEpsilon = 0.000001f;
    private const float WeightEpsilon = 0.000001f;
    private const float MainComponentAmbiguityRatio = 0.85f;
    private const float MinimumMainAreaCoverage = 0.25f;
    private const float RobustLowerQuantile = 0.05f;
    private const float RobustUpperQuantile = 0.95f;
    private const float MinimumVerticalAxisRatio = 0.65f;
    private const float MaximumAspectRatioDisagreement = 3f;
    private const float MinimumAlignmentScale = 0.0001f;
    private const float MaximumAlignmentScale = 10000f;
    private const float TargetEnvelopeWeightThreshold = 0.5f;
    private const int MaximumSafeGeneratedMeshes = 256;
    private const int MaximumSafeGeneratedVertices = 200_000;
    private const int MaximumSafeGeneratedTriangles = 600_000;
    private const int MaximumSafeGeneratedTargetBytes = 512 * 1024 * 1024;
    private const long MaximumSafeGeneratedManagedBytes =
        1536L * 1024 * 1024;

    private sealed class PreparationPassState(
        CancellationToken cancellationToken,
        IProgress<GeneratedSkinningProgress>? progress)
    {
        public int PreparationPassCount { get; private set; }

        public int SemanticResolutionCount { get; private set; }

        public CancellationToken CancellationToken => cancellationToken;

        public void BeginPass()
        {
            cancellationToken.ThrowIfCancellationRequested();
            ValidateRuntimeMemoryHeadroom();
            PreparationPassCount++;
            if (PreparationPassCount > 1)
            {
                throw new InvalidOperationException(
                    "Generated-skinning repeated its preparation pass.");
            }
            Report(
                0.06,
                "Расчёт весов и semantic regions");
        }

        public void RecordSemanticResolution()
        {
            SemanticResolutionCount++;
            if (SemanticResolutionCount > 1)
            {
                throw new InvalidOperationException(
                    "Generated-skinning attempted semantic-region resolution " +
                    "more than once in one public preparation.");
            }
        }

        public void ThrowIfCancellationRequested() =>
            cancellationToken.ThrowIfCancellationRequested();

        public void Report(double fraction, string stage) =>
            progress?.Report(new GeneratedSkinningProgress(
                Math.Clamp(fraction, 0, 1),
                stage));

        private static void ValidateRuntimeMemoryHeadroom()
        {
            long managedBytes = GC.GetTotalMemory(forceFullCollection: false);
            GCMemoryInfo memory = GC.GetGCMemoryInfo();
            bool systemMemoryCritical = memory.HighMemoryLoadThresholdBytes > 0 &&
                memory.MemoryLoadBytes >=
                memory.HighMemoryLoadThresholdBytes * 9 / 10;
            const long meaningfulImporterPressureBytes = 512L * 1024 * 1024;
            bool importerIsContributingToSystemPressure =
                managedBytes >= meaningfulImporterPressureBytes &&
                systemMemoryCritical;
            if (managedBytes <= MaximumSafeGeneratedManagedBytes &&
                !importerIsContributingToSystemPressure)
            {
                return;
            }

            throw new InvalidOperationException(
                "Расчёт автоматических весов остановлен до следующего прохода: " +
                "приложение или система приблизились к безопасному пределу памяти. " +
                $"Managed memory: {managedBytes / (1024 * 1024):N0} MiB. " +
                "Закройте другие тяжёлые программы либо упростите/разделите модель.");
        }
    }
    private const float EnvelopeCoreRatio = 1f;
    private const float EnvelopeFadeRatio = 1.25f;
    private const int MinimumEnvelopeSamples = 8;

    private enum BodySide
    {
        Center,
        Left,
        Right
    }

    private sealed record GeometrySource(
        int MeshIndex,
        string Name,
        Vector3[] Positions,
        uint[] TriangleIndices);

    private readonly record struct GeometryVertex(
        int MeshIndex,
        int VertexIndex);

    private sealed record GeometryComponent(
        int ComponentIndex,
        GeometryVertex[] Vertices,
        int TriangleCount,
        float Area,
        Vector3 Center);

    private sealed record SceneTopology(
        IReadOnlyList<GeometryComponent> Components,
        IReadOnlyList<GeometryVertex> UnreferencedVertices,
        IReadOnlyList<IReadOnlyList<uint>> RenderableTriangleIndicesByMesh,
        int RemovedDegenerateTriangleCount);

    private sealed record RobustBounds(
        Vector3 Lower,
        Vector3 Upper)
    {
        public Vector3 Center => (Lower + Upper) * 0.5f;
        public Vector3 Size => Upper - Lower;
    }

    private sealed record TargetSkeletonLayout(
        ImportedSkeleton Skeleton,
        IReadOnlyList<TargetRigJoint> DeformJoints,
        IReadOnlyDictionary<int, int> SkeletonIndexByRigJoint);

    private sealed record BoneCapsule(
        int SkeletonJointIndex,
        string BoneName,
        BodySide Side,
        Vector3 Start,
        Vector3 End,
        float Radius,
        bool SafeForAutomaticWeights);

    private sealed record AnatomicalVolume(
        int SkeletonJointIndex,
        string BoneName,
        Vector3 Start,
        Vector3 End,
        Vector3 LateralAxis,
        Vector3 ForwardAxis,
        float LateralRadius,
        float ForwardRadius,
        float AxialRadius,
        bool IsHead,
        int CalibrationSampleCount);

    private sealed record ManualComponentAssignment(
        GeneratedSkinningComponentAttachmentTarget Target,
        string BoneName,
        int SkeletonJointIndex);

    private sealed record ComponentPlaneClassification(
        GeneratedSkinningComponentAttachmentTarget? Assignment,
        IReadOnlyList<GeneratedSkinningSeparationPlaneKind> IntersectedPlanes);

    private readonly record struct ComponentPlaneCoverage(
        int PositionCount,
        int PositivePositionCount,
        int NegativePositionCount)
    {
        public bool CrossesPlane =>
            PositivePositionCount > 0 && NegativePositionCount > 0;

        public bool HasPositiveMajority =>
            (long)PositivePositionCount * 2 > PositionCount;

        public bool HasNegativeMajority =>
            (long)NegativePositionCount * 2 > PositionCount;
    }

    private sealed record SideCalibration(
        float CenterX,
        float LeftDirection,
        Vector3 Center,
        Vector3 LeftAxis,
        float DeadZone,
        bool UseVectorAxis)
    {
        public Vector3 LowerBodyWallOrigin { get; init; }

        public bool HasLowerBodyWall { get; init; }
    }

    private sealed record PackedInfluence(
        ushort Joint,
        float Weight);

    private sealed record GeneratedVertexInfluences(
        PackedInfluence[] Influences,
        PackedInfluence[] TopFourInfluences,
        float DiscardedTopFourWeightMass,
        float TopFourToFinalWeightL1Distance,
        bool AnatomicalVolumeAffected)
    {
        public bool LowerBodyWallAffected { get; init; }

        public bool ShoulderWallAffected { get; init; }
    }

    private sealed record FittingDeformationComparison(
        int VertexCount,
        float MaximumPositionDelta,
        float RmsPositionDelta);

    public static GeneratedSkinningPreparationResult Prepare(
        SmoDocument target,
        ImportedScene donor) =>
        PrepareCore(
            target,
            donor,
            fittingPose: null,
            alignmentOverride: null,
            bodySelection: null,
            componentOverrides: null);

    /// <summary>
    /// Generates target weights after applying an explicit final donor
    /// alignment in the importer's external row-vector space. This safe slice
    /// accepts only a positive uniform scale and a translation; orientation
    /// remains an explicit precondition of generated-skinning mode.
    /// </summary>
    public static GeneratedSkinningPreparationResult Prepare(
        SmoDocument target,
        ImportedScene donor,
        ReplacementTransform donorAlignment) =>
        PrepareCore(
            target,
            donor,
            fittingPose: null,
            ValidateExplicitAlignment(donorAlignment),
            bodySelection: null,
            componentOverrides: null);

    /// <summary>
    /// Applies validated whole-component rigid assignments after using the
    /// automatically selected dominant surface only as an alignment hint.
    /// </summary>
    public static GeneratedSkinningPreparationResult Prepare(
        SmoDocument target,
        ImportedScene donor,
        ReplacementTransform donorAlignment,
        GeneratedSkinningComponentOverrides componentOverrides)
    {
        ArgumentNullException.ThrowIfNull(componentOverrides);
        return PrepareCore(
            target,
            donor,
            fittingPose: null,
            ValidateExplicitAlignment(donorAlignment),
            bodySelection: null,
            componentOverrides);
    }

    /// <summary>
    /// Uses an explicitly selected multi-component donor body for robust
    /// alignment and pose fitting. The selection must have been produced for
    /// the exact target rig, donor scene, and alignment supplied here. It does
    /// not classify rigid details: after fitting, every donor component joins
    /// one logical deform body unless Back or a manual assignment extracts it.
    /// </summary>
    public static GeneratedSkinningPreparationResult Prepare(
        SmoDocument target,
        ImportedScene donor,
        ReplacementTransform donorAlignment,
        TargetRigBodySelection bodySelection)
    {
        ArgumentNullException.ThrowIfNull(bodySelection);
        return PrepareCore(
            target,
            donor,
            fittingPose: null,
            ValidateExplicitAlignment(donorAlignment),
            bodySelection,
            componentOverrides: null);
    }

    /// <summary>
    /// Uses the exact selected body as a fitting hint, then generates smooth
    /// weights for the logical body and applies validated one-hot assignments
    /// to explicitly extracted whole components. Manual assignments take
    /// precedence over automatic Back classification.
    /// </summary>
    public static GeneratedSkinningPreparationResult Prepare(
        SmoDocument target,
        ImportedScene donor,
        ReplacementTransform donorAlignment,
        TargetRigBodySelection bodySelection,
        GeneratedSkinningComponentOverrides componentOverrides)
    {
        ArgumentNullException.ThrowIfNull(bodySelection);
        ArgumentNullException.ThrowIfNull(componentOverrides);
        return PrepareCore(
            target,
            donor,
            fittingPose: null,
            ValidateExplicitAlignment(donorAlignment),
            bodySelection,
            componentOverrides);
    }

    /// <summary>
    /// Generates target weights around a validated temporary fitting pose and
    /// then bakes the fitting-space geometry back into the immutable canonical
    /// target bind pose. The reset pose reproduces the legacy result exactly.
    /// This safe mode-3 slice supports local bone rotations only: root rotation
    /// and translation are rejected because donor alignment is still expressed
    /// in canonical target space.
    /// </summary>
    public static GeneratedSkinningPreparationResult Prepare(
        SmoDocument target,
        ImportedScene donor,
        TargetRigFittingPoseSnapshot fittingPose) =>
        Prepare(
            target,
            donor,
            fittingPose,
            enableAutomaticBackExtraction: true);

    public static GeneratedSkinningPreparationResult Prepare(
        SmoDocument target,
        ImportedScene donor,
        TargetRigFittingPoseSnapshot fittingPose,
        bool enableAutomaticBackExtraction)
    {
        ArgumentNullException.ThrowIfNull(fittingPose);
        fittingPose.ValidateForTarget(target);
        if (fittingPose.RootRotation != Quaternion.Identity ||
            fittingPose.RootTranslation != Vector3.Zero)
        {
            throw new InvalidOperationException(
                "Generated-skinning fitting supports local bone rotations only; " +
                "root rotation and translation require an explicit donor-alignment " +
                "space contract.");
        }
        return PrepareCore(
            target,
            donor,
            fittingPose,
            alignmentOverride: null,
            bodySelection: null,
            componentOverrides: null,
            enableAutomaticBackExtraction: enableAutomaticBackExtraction);
    }

    /// <summary>
    /// Applies an explicit final donor alignment before generated weights,
    /// rigid attachments, and the temporary fitting-pose bake are calculated.
    /// The alignment replaces automatic height-and-center fitting; it is never
    /// composed with it. Target bind matrices and inverse binds remain
    /// canonical and unchanged.
    /// </summary>
    public static GeneratedSkinningPreparationResult Prepare(
        SmoDocument target,
        ImportedScene donor,
        TargetRigFittingPoseSnapshot fittingPose,
        ReplacementTransform donorAlignment)
    {
        ArgumentNullException.ThrowIfNull(fittingPose);
        fittingPose.ValidateForTarget(target);
        if (fittingPose.RootRotation != Quaternion.Identity ||
            fittingPose.RootTranslation != Vector3.Zero)
        {
            throw new InvalidOperationException(
                "Generated-skinning fitting supports local bone rotations only; " +
                "root rotation and translation require an explicit donor-alignment " +
                "space contract.");
        }
        return PrepareCore(
            target,
            donor,
            fittingPose,
            ValidateExplicitAlignment(donorAlignment),
            bodySelection: null,
            componentOverrides: null);
    }

    /// <summary>
    /// Fitting-pose counterpart of the manual whole-component overload. The
    /// automatic dominant surface remains a fitting hint, not a rigid filter.
    /// </summary>
    public static GeneratedSkinningPreparationResult Prepare(
        SmoDocument target,
        ImportedScene donor,
        TargetRigFittingPoseSnapshot fittingPose,
        ReplacementTransform donorAlignment,
        GeneratedSkinningComponentOverrides componentOverrides)
    {
        ArgumentNullException.ThrowIfNull(fittingPose);
        ArgumentNullException.ThrowIfNull(componentOverrides);
        fittingPose.ValidateForTarget(target);
        if (fittingPose.RootRotation != Quaternion.Identity ||
            fittingPose.RootTranslation != Vector3.Zero)
        {
            throw new InvalidOperationException(
                "Generated-skinning fitting supports local bone rotations only; " +
                "root rotation and translation require an explicit donor-alignment " +
                "space contract.");
        }
        return PrepareCore(
            target,
            donor,
            fittingPose,
            ValidateExplicitAlignment(donorAlignment),
            bodySelection: null,
            componentOverrides);
    }

    /// <summary>
    /// Applies a validated multi-component donor body selection to alignment
    /// and fitting-pose calibration. Smooth/rigid eligibility is evaluated only
    /// afterwards across all meshes by the universal Head/Back/manual rules.
    /// </summary>
    public static GeneratedSkinningPreparationResult Prepare(
        SmoDocument target,
        ImportedScene donor,
        TargetRigFittingPoseSnapshot fittingPose,
        ReplacementTransform donorAlignment,
        TargetRigBodySelection bodySelection) =>
        Prepare(
            target,
            donor,
            fittingPose,
            donorAlignment,
            bodySelection,
            enableAutomaticBackExtraction: true);

    public static GeneratedSkinningPreparationResult Prepare(
        SmoDocument target,
        ImportedScene donor,
        TargetRigFittingPoseSnapshot fittingPose,
        ReplacementTransform donorAlignment,
        TargetRigBodySelection bodySelection,
        bool enableAutomaticBackExtraction)
    {
        ArgumentNullException.ThrowIfNull(fittingPose);
        ArgumentNullException.ThrowIfNull(bodySelection);
        fittingPose.ValidateForTarget(target);
        if (fittingPose.RootRotation != Quaternion.Identity ||
            fittingPose.RootTranslation != Vector3.Zero)
        {
            throw new InvalidOperationException(
                "Generated-skinning fitting supports local bone rotations only; " +
                "root rotation and translation require an explicit donor-alignment " +
                "space contract.");
        }
        return PrepareCore(
            target,
            donor,
            fittingPose,
            ValidateExplicitAlignment(donorAlignment),
            bodySelection,
            componentOverrides: null,
            enableAutomaticBackExtraction: enableAutomaticBackExtraction);
    }

    /// <summary>
    /// Fitting-pose counterpart of the explicit body-and-component assignment
    /// overload. Component identity is revalidated from original donor vertex
    /// membership and does not depend on the current alignment.
    /// </summary>
    public static GeneratedSkinningPreparationResult Prepare(
        SmoDocument target,
        ImportedScene donor,
        TargetRigFittingPoseSnapshot fittingPose,
        ReplacementTransform donorAlignment,
        TargetRigBodySelection bodySelection,
        GeneratedSkinningComponentOverrides componentOverrides)
    {
        ArgumentNullException.ThrowIfNull(fittingPose);
        ArgumentNullException.ThrowIfNull(bodySelection);
        ArgumentNullException.ThrowIfNull(componentOverrides);
        fittingPose.ValidateForTarget(target);
        if (fittingPose.RootRotation != Quaternion.Identity ||
            fittingPose.RootTranslation != Vector3.Zero)
        {
            throw new InvalidOperationException(
                "Generated-skinning fitting supports local bone rotations only; " +
                "root rotation and translation require an explicit donor-alignment " +
                "space contract.");
        }
        return PrepareCore(
            target,
            donor,
            fittingPose,
            ValidateExplicitAlignment(donorAlignment),
            bodySelection,
            componentOverrides);
    }

    /// <summary>
    /// Applies validated semantic head/hand adjustments to the logical deform
    /// body before fitting-pose geometry is baked back to the canonical target
    /// bind pose. The explicit body selection is only the fitting hint.
    /// </summary>
    public static GeneratedSkinningPreparationResult Prepare(
        SmoDocument target,
        ImportedScene donor,
        TargetRigFittingPoseSnapshot fittingPose,
        ReplacementTransform donorAlignment,
        TargetRigBodySelection bodySelection,
        GeneratedSkinningRegionOverrides regionOverrides)
    {
        ArgumentNullException.ThrowIfNull(fittingPose);
        ArgumentNullException.ThrowIfNull(bodySelection);
        ArgumentNullException.ThrowIfNull(regionOverrides);
        fittingPose.ValidateForTarget(target);
        ValidateGeneratedFittingPoseRoot(fittingPose);
        return PrepareCore(
            target,
            donor,
            fittingPose,
            ValidateExplicitAlignment(donorAlignment),
            bodySelection,
            componentOverrides: null,
            regionOverrides);
    }

    /// <summary>
    /// Full generated-skinning preparation contract: explicit fitting-body
    /// selection, whole-component assignments, and semantic vertex-region edits
    /// are independently revalidated against their immutable source state.
    /// </summary>
    public static GeneratedSkinningPreparationResult Prepare(
        SmoDocument target,
        ImportedScene donor,
        TargetRigFittingPoseSnapshot fittingPose,
        ReplacementTransform donorAlignment,
        TargetRigBodySelection bodySelection,
        GeneratedSkinningComponentOverrides componentOverrides,
        GeneratedSkinningRegionOverrides regionOverrides)
    {
        ArgumentNullException.ThrowIfNull(fittingPose);
        ArgumentNullException.ThrowIfNull(bodySelection);
        ArgumentNullException.ThrowIfNull(componentOverrides);
        ArgumentNullException.ThrowIfNull(regionOverrides);
        fittingPose.ValidateForTarget(target);
        ValidateGeneratedFittingPoseRoot(fittingPose);
        return PrepareCore(
            target,
            donor,
            fittingPose,
            ValidateExplicitAlignment(donorAlignment),
            bodySelection,
            componentOverrides,
            regionOverrides);
    }

    /// <summary>
    /// Cancellable full-contract entry point used by interactive previews.
    /// Cancellation is observed before preparation, during bounded processing
    /// stages and before final plan analysis.
    /// </summary>
    public static GeneratedSkinningPreparationResult PrepareCancellable(
        SmoDocument target,
        ImportedScene donor,
        TargetRigFittingPoseSnapshot fittingPose,
        ReplacementTransform donorAlignment,
        TargetRigBodySelection? bodySelection,
        GeneratedSkinningComponentOverrides? componentOverrides,
        GeneratedSkinningRegionOverrides? regionOverrides,
        CancellationToken cancellationToken,
        IProgress<GeneratedSkinningProgress>? progress = null) =>
        PrepareCancellable(
            target,
            donor,
            fittingPose,
            donorAlignment,
            bodySelection,
            componentOverrides,
            regionOverrides,
            cancellationToken,
            progress,
            enableAutomaticBackExtraction: true);

    public static GeneratedSkinningPreparationResult PrepareCancellable(
        SmoDocument target,
        ImportedScene donor,
        TargetRigFittingPoseSnapshot fittingPose,
        ReplacementTransform donorAlignment,
        TargetRigBodySelection? bodySelection,
        GeneratedSkinningComponentOverrides? componentOverrides,
        GeneratedSkinningRegionOverrides? regionOverrides,
        CancellationToken cancellationToken,
        IProgress<GeneratedSkinningProgress>? progress,
        bool enableAutomaticBackExtraction)
    {
        ArgumentNullException.ThrowIfNull(fittingPose);
        fittingPose.ValidateForTarget(target);
        ValidateGeneratedFittingPoseRoot(fittingPose);
        cancellationToken.ThrowIfCancellationRequested();
        return PrepareCore(
            target,
            donor,
            fittingPose,
            ValidateExplicitAlignment(donorAlignment),
            bodySelection,
            componentOverrides,
            regionOverrides,
            cancellationToken,
            progress,
            enableAutomaticBackExtraction);
    }

    public static GeneratedSkinningPreparationResult PrepareCancellable(
        SmoDocument target,
        ImportedScene donor,
        TargetRigFittingPoseSnapshot fittingPose,
        CancellationToken cancellationToken,
        IProgress<GeneratedSkinningProgress>? progress = null)
    {
        ArgumentNullException.ThrowIfNull(fittingPose);
        fittingPose.ValidateForTarget(target);
        ValidateGeneratedFittingPoseRoot(fittingPose);
        cancellationToken.ThrowIfCancellationRequested();
        return PrepareCore(
            target,
            donor,
            fittingPose,
            alignmentOverride: null,
            bodySelection: null,
            componentOverrides: null,
            regionOverrides: null,
            cancellationToken,
            progress);
    }

    private static GeneratedSkinningPreparationResult PrepareCore(
        SmoDocument target,
        ImportedScene donor,
        TargetRigFittingPoseSnapshot? fittingPose,
        GeneratedSkinningAlignment? alignmentOverride,
        TargetRigBodySelection? bodySelection,
        GeneratedSkinningComponentOverrides? componentOverrides,
        GeneratedSkinningRegionOverrides? regionOverrides = null,
        CancellationToken cancellationToken = default,
        IProgress<GeneratedSkinningProgress>? progress = null,
        bool enableAutomaticBackExtraction = true)
    {
        ArgumentNullException.ThrowIfNull(target);
        ArgumentNullException.ThrowIfNull(donor);
        ValidatePreparationResourceBudget(target, donor);
        cancellationToken.ThrowIfCancellationRequested();

        // The clean writer allocates additional 16-slot palettes. A triangle
        // has at most 3 * 4 distinct generated influences, so the former search
        // against a fixed number of target palettes cannot constrain this path.
        // A fixed four-influence limit also keeps vertices outside semantic
        // regions independent of how those regions change palette pressure.
        if (3 * TopFourComparisonInfluences > SmoSkinnedBranchSplitBuilder.PaletteCapacity)
        {
            throw new InvalidOperationException(
                "Generated influences exceed the clean writer's per-triangle palette guarantee.");
        }
        progress?.Report(new GeneratedSkinningProgress(
            0.01, "Проверка модели и бюджета расчёта"));
        var passState = new PreparationPassState(cancellationToken, progress);
        GeneratedSkinningPreparationResult candidate = PrepareWithInfluenceLimit(
            target,
            donor,
            fittingPose,
            alignmentOverride,
            bodySelection,
            componentOverrides,
            regionOverrides,
            TopFourComparisonInfluences,
            applySemanticRegions: true,
            passState: passState,
            enableAutomaticBackExtraction);
        cancellationToken.ThrowIfCancellationRequested();
        passState.Report(0.88, "Финальная проверка semantic regions и palettes");
        // Preserve the final analysis and its validation/cancellation behavior.
        // The GUI and writer still decide whether the complete plan can be used.
        _ = SmoSkinnedGlbReplacer.Analyze(
            target,
            candidate.PreparedScene,
            cancellationToken: passState.CancellationToken);
        if (passState.SemanticResolutionCount != 1 || passState.PreparationPassCount != 1)
        {
            throw new InvalidOperationException(
                "Generated-skinning violated its single preparation/semantic pass contract.");
        }
        passState.Report(1, "Создание весов завершено");
        return candidate;
    }

    private static void ValidatePreparationResourceBudget(
        SmoDocument target,
        ImportedScene donor)
    {
        int meshCount = donor.Meshes.Count;
        long vertexCount = donor.Meshes.Sum(mesh => (long)mesh.Positions.Length);
        long triangleCount = donor.Meshes.Sum(
            mesh => (long)mesh.TriangleIndices.Length / 3);
        if (target.Data.Length > MaximumSafeGeneratedTargetBytes ||
            meshCount > MaximumSafeGeneratedMeshes ||
            vertexCount > MaximumSafeGeneratedVertices ||
            triangleCount > MaximumSafeGeneratedTriangles)
        {
            throw new InvalidDataException(
                "Generated-skinning was blocked by its interactive safety " +
                $"budget: target={target.Data.Length / (1024d * 1024d):N1} MiB, " +
                $"donor={meshCount:N0} meshes, {vertexCount:N0} vertices, " +
                $"{triangleCount:N0} triangles. Limits are " +
                $"{MaximumSafeGeneratedTargetBytes / (1024 * 1024)} MiB, " +
                $"{MaximumSafeGeneratedMeshes:N0} meshes, " +
                $"{MaximumSafeGeneratedVertices:N0} vertices and " +
                $"{MaximumSafeGeneratedTriangles:N0} triangles. Reduce or split " +
                "the model before creating weights.");
        }
    }

    private static GeneratedSkinningPreparationResult PrepareWithInfluenceLimit(
        SmoDocument target,
        ImportedScene donor,
        TargetRigFittingPoseSnapshot? fittingPose,
        GeneratedSkinningAlignment? alignmentOverride,
        TargetRigBodySelection? bodySelection,
        GeneratedSkinningComponentOverrides? componentOverrides,
        GeneratedSkinningRegionOverrides? regionOverrides,
        int maximumInfluences,
        bool applySemanticRegions,
        PreparationPassState passState,
        bool enableAutomaticBackExtraction)
    {
        ArgumentNullException.ThrowIfNull(target);
        ArgumentNullException.ThrowIfNull(donor);
        ArgumentNullException.ThrowIfNull(passState);
        passState.BeginPass();
        if (donor.Meshes.Count == 0)
            throw new InvalidDataException("The donor contains no meshes.");
        if (donor.Meshes.Any(mesh => mesh.Skinning is not null))
        {
            throw new InvalidDataException(
                "Generated skinning mode accepts only a completely unskinned donor scene.");
        }

        ValidateDonorAttributes(donor);
        TargetRigDefinition rig = fittingPose?.Definition ??
                                  TargetRigDefinition.FromSmoDocument(target);
        IReadOnlyList<Matrix4x4>? fittingWorldMatrices =
            fittingPose is { IsIdentityPose: false }
                ? fittingPose.WorldMatrices
                : null;
        TargetSkeletonLayout targetSkeleton = BuildTargetSkeleton(rig);

        SmoExportScene targetScene = SmoSceneBuilder.Build(
            target,
            new SmoExportOptions(
                ApplyWorldTransforms: true,
                AnimationPaths: null,
                Resources: SmoExportResourceTypes.Meshes |
                           SmoExportResourceTypes.Skeleton));
        if (targetScene.Warnings.Count > 0)
        {
            throw new InvalidDataException(
                "Target geometry could not be decoded completely enough for a " +
                "conservative fit: " + string.Join(" | ", targetScene.Warnings));
        }
        SmoExportMesh[] targetSkinnedMeshes = targetScene.Meshes
            .Where(mesh => mesh.SkinObjectIndex is not null)
            .ToArray();
        if (targetSkinnedMeshes.Any(mesh =>
                mesh.Positions.Length == 0 ||
                mesh.BlendWeights.Length != mesh.Positions.Length ||
                mesh.JointIndices.Length != mesh.Positions.Length))
        {
            throw new InvalidDataException(
                "Target SMO contains an incomplete decoded skinned mesh; using the " +
                "remaining surface would make body selection ambiguous.");
        }
        GeometrySource[] targetSources = targetSkinnedMeshes
            .Select((mesh, index) => new GeometrySource(
                index, mesh.Name, mesh.Positions, mesh.TriangleIndices))
            .ToArray();
        if (targetSources.Length == 0)
        {
            throw new InvalidDataException(
                "Target SMO has no decoded skinned geometry for robust fitting.");
        }

        GeometrySource[] donorSources = donor.Meshes
            .Select((mesh, index) => new GeometrySource(
                index, mesh.Name, mesh.Positions, mesh.TriangleIndices))
            .ToArray();
        SceneTopology targetTopology = BuildTopology(
            targetSources, "target SMO", passState.CancellationToken);
        passState.ThrowIfCancellationRequested();
        SceneTopology donorTopology = BuildTopology(
            donorSources, "donor", passState.CancellationToken);
        passState.ThrowIfCancellationRequested();
        GeometryComponent targetMain = SelectUnambiguousMainComponent(
            targetTopology.Components, targetSources, "target SMO");
        GeometryComponent[] donorBodyComponents;
        if (bodySelection is null)
        {
            donorBodyComponents =
            [SelectUnambiguousMainComponent(
                donorTopology.Components, donorSources, "donor")];
        }
        else
        {
            if (alignmentOverride is null)
            {
                throw new InvalidOperationException(
                    "An explicit donor body selection requires its exact explicit alignment.");
            }
            donorBodyComponents = ValidateExplicitBodySelection(
                target,
                donor,
                donorTopology,
                donorSources,
                bodySelection,
                alignmentOverride);
        }
        Vector3[] targetMainPositions = GetComponentPositions(
            targetSources, targetMain);
        Vector3[] donorMainPositions = donorBodyComponents
            .SelectMany(component => GetComponentPositions(donorSources, component))
            .Distinct()
            .ToArray();
        RobustBounds targetBounds = ComputeRobustBounds(
            targetMainPositions, "target main component");
        RobustBounds donorBounds = ComputeRobustBounds(
            donorMainPositions,
            bodySelection is null
                ? "donor main component"
                : "selected donor body components");
        GeneratedSkinningAlignment alignment;
        if (alignmentOverride is null)
        {
            alignment = BuildAlignment(targetBounds, donorBounds);
        }
        else
        {
            // Validate orientation after the complete explicit transform. This
            // lets a user-authored rotation stand up a sideways donor while the
            // automatic path remains conservative and never guesses orientation.
            ValidateVerticalAxis(targetBounds, "target main component");
            RobustBounds alignedDonorBounds = ComputeRobustBounds(
                donorMainPositions
                    .Select(position => Vector3.Transform(
                        position,
                        alignmentOverride.Matrix))
                    .ToArray(),
                bodySelection is null
                    ? "aligned donor main component"
                    : "aligned selected donor body components");
            ValidateVerticalAxis(
                alignedDonorBounds,
                bodySelection is null
                    ? "aligned donor main component"
                    : "aligned selected donor body components");
            alignment = alignmentOverride;
        }

        SideCalibration sideCalibration = CalibrateSides(
            targetSkeleton.DeformJoints,
            targetBounds,
            fittingWorldMatrices);
        ValidateTargetBodyOverlap(
            targetSkeleton.DeformJoints,
            targetBounds,
            fittingWorldMatrices);
        IReadOnlyList<BoneCapsule> capsules = BuildBoneCapsules(
            rig,
            targetSkeleton,
            targetBounds,
            fittingWorldMatrices);
        if (!capsules.Any(capsule => capsule.SafeForAutomaticWeights))
        {
            throw new InvalidDataException(
                "Target rig produced no safe non-service deform-bone capsules.");
        }
        IReadOnlyDictionary<int, AnatomicalVolume> anatomicalVolumes =
            BuildAnatomicalVolumes(
                rig,
                targetSkeleton,
                targetScene,
                targetSkinnedMeshes,
                targetBounds,
                fittingWorldMatrices,
                out IReadOnlyList<string> envelopeDiagnostics);
        SeparationPlanePreparation separationPlanePreparation =
            ResolveSeparationPlanes(
                rig,
                targetSkeleton,
                targetBounds,
                fittingWorldMatrices,
                sideCalibration,
                anatomicalVolumes,
                regionOverrides);

        Vector3[][] alignedPositionsByMesh = donor.Meshes
            .Select(mesh => mesh.Positions
                .Select(position => ApplyAlignment(position, alignment))
                .ToArray())
            .ToArray();
        passState.ThrowIfCancellationRequested();

        // Body selection is an alignment/pose-fitting hint only. It must never
        // decide which disconnected donor meshes receive deform weights. Treat
        // every renderable component from every source mesh as one logical body,
        // then apply the planes in their declared order. Shoulder and Head are
        // protective weight boundaries; only a strict whole-component majority
        // behind Back (or an explicit manual pin) extracts a rigid detail.
        int fittingBodyComponentCount = donorBodyComponents.Length;
        IReadOnlyList<GeneratedSkinningComponentOverride>? requestedOverrides =
            componentOverrides?.Components;
        HashSet<int> explicitlyPinnedIndices = requestedOverrides is null
            ? []
            : requestedOverrides
                .Where(component => component is not null)
                .Select(component => component.ComponentIndex)
                .ToHashSet();
        Dictionary<int, ComponentPlaneClassification> planeClassificationByComponent =
            donorTopology.Components.ToDictionary(
                component => component.ComponentIndex,
                component => ClassifyDetachedComponentAgainstPlanes(
                    component,
                    alignedPositionsByMesh,
                    separationPlanePreparation));
        int[] backSeparatedComponentIndices = enableAutomaticBackExtraction
            ? donorTopology.Components
                .Where(component =>
                    !explicitlyPinnedIndices.Contains(component.ComponentIndex) &&
                    planeClassificationByComponent[component.ComponentIndex].Assignment ==
                        GeneratedSkinningComponentAttachmentTarget.UpperBack)
                .Select(component => component.ComponentIndex)
                .Order()
                .ToArray()
            : [];
        HashSet<int> rigidDetailComponentIndices = backSeparatedComponentIndices
            .Concat(explicitlyPinnedIndices)
            .ToHashSet();
        donorBodyComponents = donorTopology.Components
            .Where(component =>
                !rigidDetailComponentIndices.Contains(component.ComponentIndex))
            .OrderBy(component => component.ComponentIndex)
            .ToArray();
        if (donorBodyComponents.Length == 0)
        {
            throw new InvalidDataException(
                "Rigid component assignments extracted every donor component. " +
                "The logical deform body would be empty; return at least one " +
                "component to automatic weights.");
        }

        HashSet<int> donorBodyComponentIndices = donorBodyComponents
            .Select(component => component.ComponentIndex)
            .ToHashSet();
        IReadOnlyDictionary<int, ManualComponentAssignment> manualAssignments =
            ValidateComponentOverrides(
                target,
                donor,
                donorTopology,
                donorSources,
                donorBodyComponentIndices,
                targetSkeleton,
                componentOverrides);

        var attachments = new List<GeneratedSkinningAttachment>();
        var components = new List<GeneratedSkinningComponentInfo>(
            donorTopology.Components.Count);
        var warnings = new List<string>
        {
            "Generated-skinning input must be upright and Y-up after explicit donor " +
            "alignment, unmirrored, and facing the same direction as the target; an " +
            "unskinned surface cannot prove mirror or facing automatically.",
            "Weights were generated heuristically from target bind-pose bone capsules, " +
            "finite target-weight-calibrated torso fields and hard separation planes; extreme animation " +
            "poses still require visual inspection."
        };
        foreach (string warning in donor.ImportWarnings)
            warnings.Add(warning);
        foreach (string diagnostic in envelopeDiagnostics)
            warnings.Add(diagnostic);
        if (anatomicalVolumes.Values.Any(volume =>
                IsTorsoFieldBone(volume.BoneName)))
        {
            warnings.Add(
                "Inside the finite torso field, non-central capsule candidates receive " +
                "a 16*alpha normalized-distance penalty so long thigh/biceps capsules " +
                "cannot capture the chest. At alpha zero this branch is bypassed exactly.");
        }
        int ignoredTargetComponents = targetTopology.Components.Count - 1;
        if (ignoredTargetComponents > 0)
        {
            warnings.Add(
                $"Robust alignment ignored {ignoredTargetComponents} disconnected target " +
                "surface component(s); only the largest target body component affected fit.");
        }
        warnings.Add(enableAutomaticBackExtraction
            ? $"Pose fitting used {fittingBodyComponentCount} provisional component(s), " +
              $"and the preliminary Back pass retained {donorBodyComponents.Length} " +
              "component(s). Semantic Head mesh protection is resolved next and may " +
              "return protected source-mesh components to the logical deform body."
            : $"Pose fitting used {fittingBodyComponentCount} provisional component(s). " +
              "Automatic Back extraction is disabled; every component remains on " +
              "generated weights unless Head protection or an explicit manual " +
              "Head/UpperBack exception owns it.");
        if (explicitlyPinnedIndices.Count > 0)
        {
            warnings.Add(
                $"Manual assignments extracted {explicitlyPinnedIndices.Count} whole " +
                "component(s) from the logical deform body: " +
                $"{string.Join(", ", explicitlyPinnedIndices.Order().Select(index => $"#{index}"))}.");
        }
        if (donorTopology.UnreferencedVertices.Count > 0 ||
            donorTopology.RemovedDegenerateTriangleCount > 0)
        {
            HashSet<int> affectedMeshes = donorTopology.UnreferencedVertices
                .Select(vertex => vertex.MeshIndex)
                .ToHashSet();
            foreach (int meshIndex in Enumerable.Range(0, donorSources.Length))
            {
                if (donorSources[meshIndex].TriangleIndices.Length !=
                    donorTopology.RenderableTriangleIndicesByMesh[meshIndex].Count)
                {
                    affectedMeshes.Add(meshIndex);
                }
            }
            warnings.Add(
                $"Removed {donorTopology.RemovedDegenerateTriangleCount} degenerate " +
                $"donor triangle(s) and excluded " +
                $"{donorTopology.UnreferencedVertices.Count} non-surface vertex/vertices " +
                $"across {affectedMeshes.Count} affected mesh(es). Source vertex indices " +
                "remain stable; excluded vertices receive inert placeholder weights " +
                "and cannot affect rendered geometry.");
        }

        SemanticRegionPreparation? semanticRegionPreparation = null;
        if (applySemanticRegions)
        {
            passState.RecordSemanticResolution();
            semanticRegionPreparation = ResolveSemanticRegions(
                target,
                donor,
                rig,
                targetSkeleton,
                targetScene,
                targetSkinnedMeshes,
                targetBounds,
                fittingWorldMatrices,
                donorBodyComponents,
                donorSources,
                donorTopology,
                alignedPositionsByMesh,
                sideCalibration,
                capsules,
                anatomicalVolumes,
                separationPlanePreparation,
                maximumInfluences,
                alignment,
                fittingPose,
                explicitlyPinnedIndices,
                regionOverrides);
            passState.ThrowIfCancellationRequested();
            foreach (GeneratedSkinningRegionResolution resolution in
                     semanticRegionPreparation.Analysis.Regions)
            {
                foreach (string diagnostic in resolution.Warnings)
                    warnings.Add(diagnostic);
            }
        }

        HashSet<int> semanticHeadComponentIndices = semanticRegionPreparation is null
            ? []
            : semanticRegionPreparation.ComponentAssignments
                .Where(pair => pair.Value.Region ==
                    GeneratedSkinningSemanticRegion.Head)
                .Select(pair => pair.Key)
                .ToHashSet();
        donorBodyComponentIndices.UnionWith(semanticHeadComponentIndices);
        donorBodyComponents = donorTopology.Components
            .Where(component => donorBodyComponentIndices.Contains(
                component.ComponentIndex))
            .OrderBy(component => component.ComponentIndex)
            .ToArray();
        int[] effectiveBackSeparatedComponentIndices =
            backSeparatedComponentIndices
                .Where(index => !semanticHeadComponentIndices.Contains(index))
                .ToArray();
        warnings.Add(
            $"Final generated weights treat {donorBodyComponents.Length} component(s) " +
            "from all donor meshes as one logical deform body. Imported mesh " +
            "boundaries only propagate protective Head ownership; they never create " +
            "a rigid detail by themselves.");
        if (effectiveBackSeparatedComponentIndices.Length > 0)
        {
            warnings.Add(
                $"After whole-mesh Head protection, the Back separation plane " +
                $"extracted {effectiveBackSeparatedComponentIndices.Length} whole " +
                $"component(s) by strict majority: " +
                $"{string.Join(", ", effectiveBackSeparatedComponentIndices.Select(index => $"#{index}"))}.");
        }
        ImportedJointIndices[][] jointsByMesh = donor.Meshes
            .Select(mesh => new ImportedJointIndices[mesh.Positions.Length])
            .ToArray();
        Vector4[][] weightsByMesh = donor.Meshes
            .Select(mesh => new Vector4[mesh.Positions.Length])
            .ToArray();
        ImportedJointIndices[][] topFourJointsByMesh = donor.Meshes
            .Select(mesh => new ImportedJointIndices[mesh.Positions.Length])
            .ToArray();
        Vector4[][] topFourWeightsByMesh = donor.Meshes
            .Select(mesh => new Vector4[mesh.Positions.Length])
            .ToArray();
        bool[][] assignedByMesh = donor.Meshes
            .Select(mesh => new bool[mesh.Positions.Length])
            .ToArray();
        bool[][] smoothByMesh = donor.Meshes
            .Select(mesh => new bool[mesh.Positions.Length])
            .ToArray();
        int smoothVertexCount = 0;
        int anatomicalVolumeAffectedVertexCount = 0;
        int lowerBodyWallAffectedVertexCount = 0;
        int shoulderWallAffectedVertexCount = 0;
        int semanticRegionAppliedVertexCount = 0;
        int smoothSemanticRegionAppliedVertexCount = 0;
        int headProtectedComponentVertexCount = 0;
        double discardedTopFourWeightMassSum = 0;
        double topFourToFinalWeightL1DistanceSum = 0;
        float maximumDiscardedTopFourWeightMass = 0;
        float maximumTopFourToFinalWeightL1Distance = 0;

        foreach (GeometryComponent component in donorTopology.Components)
        {
            passState.ThrowIfCancellationRequested();
            bool isMain = donorBodyComponentIndices.Contains(component.ComponentIndex);
            ComponentPlaneClassification planeClassification =
                planeClassificationByComponent[component.ComponentIndex];
            ManualComponentAssignment? manualAssignment = manualAssignments
                .GetValueOrDefault(component.ComponentIndex);
            SemanticComponentAssignment? semanticComponentAssignment = null;
            bool isWholeHeadComponent =
                semanticRegionPreparation?.ComponentAssignments.TryGetValue(
                        component.ComponentIndex,
                        out semanticComponentAssignment) == true &&
                semanticComponentAssignment.Region ==
                    GeneratedSkinningSemanticRegion.Head;
            int[] componentMeshIndices = component.Vertices
                .Select(vertex => vertex.MeshIndex)
                .Distinct()
                .Order()
                .ToArray();
            string[] componentMeshNames = componentMeshIndices
                .Select(index => donorSources[index].Name)
                .ToArray();
            IReadOnlyList<TargetRigBodyVertexMembership> componentMembership =
                BuildComponentMembership(component, donorSources);
            GeneratedSkinningAutomaticComponentBinding automaticBinding =
                isWholeHeadComponent
                    ? GeneratedSkinningAutomaticComponentBinding.Head
                    : !isMain && manualAssignment is null
                        ? GeneratedSkinningAutomaticComponentBinding.UpperBack
                        : GeneratedSkinningAutomaticComponentBinding.GeneratedWeights;
            components.Add(new GeneratedSkinningComponentInfo(
                component.ComponentIndex,
                Array.AsReadOnly(componentMeshIndices),
                Array.AsReadOnly(componentMeshNames),
                component.Vertices.Length,
                component.TriangleCount,
                ApplyAlignment(component.Center, alignment),
                componentMembership,
                automaticBinding,
                manualAssignment?.Target));
            if (isWholeHeadComponent)
            {
                int headJoint = semanticComponentAssignment?
                    .AnchorSkeletonJointIndex ??
                    separationPlanePreparation.HeadSkeletonJointIndex;
                if (headJoint < 0 ||
                    (uint)headJoint >=
                    (uint)targetSkeleton.Skeleton.JointNames.Count)
                {
                    throw new InvalidDataException(
                        $"Head protection classified whole component " +
                        $"#{component.ComponentIndex}, but the exact target " +
                        "Head joint is unavailable.");
                }
                var rigidHeadIndices = new ImportedJointIndices(
                    checked((ushort)headJoint), 0, 0, 0);
                foreach (GeometryVertex vertex in component.Vertices)
                {
                    if ((headProtectedComponentVertexCount & 0xFFF) == 0)
                        passState.ThrowIfCancellationRequested();
                    jointsByMesh[vertex.MeshIndex][vertex.VertexIndex] =
                        rigidHeadIndices;
                    weightsByMesh[vertex.MeshIndex][vertex.VertexIndex] =
                        Vector4.UnitX;
                    topFourJointsByMesh[vertex.MeshIndex][vertex.VertexIndex] =
                        rigidHeadIndices;
                    topFourWeightsByMesh[vertex.MeshIndex][vertex.VertexIndex] =
                        Vector4.UnitX;
                    assignedByMesh[vertex.MeshIndex][vertex.VertexIndex] = true;
                    if (applySemanticRegions &&
                        semanticRegionPreparation!.Assignments.ContainsKey(vertex))
                    {
                        semanticRegionAppliedVertexCount++;
                    }
                    headProtectedComponentVertexCount++;
                }
                continue;
            }
            if (isMain)
            {
                foreach (GeometryVertex vertex in component.Vertices)
                {
                    if ((smoothVertexCount & 0xFFF) == 0)
                        passState.ThrowIfCancellationRequested();
                    SemanticVertexAssignment? semanticAssignment = null;
                    bool usesSemanticRegion =
                        applySemanticRegions &&
                        semanticRegionPreparation!.Assignments.TryGetValue(
                            vertex,
                            out semanticAssignment);
                    GeneratedVertexInfluences generated;
                    if (usesSemanticRegion)
                    {
                        generated = BuildSemanticVertexInfluences(
                            semanticAssignment!);
                    }
                    else if (semanticRegionPreparation is not null)
                    {
                        generated =
                            semanticRegionPreparation.CapsuleInfluences[vertex];
                    }
                    else
                    {
                        generated = GenerateVertexInfluences(
                            alignedPositionsByMesh[vertex.MeshIndex]
                                [vertex.VertexIndex],
                            capsules,
                            anatomicalVolumes,
                            sideCalibration,
                            targetBounds.Size.Y,
                            maximumInfluences,
                            separationPlanePreparation);
                    }
                    WriteInfluences(
                        generated.Influences,
                        out jointsByMesh[vertex.MeshIndex][vertex.VertexIndex],
                        out weightsByMesh[vertex.MeshIndex][vertex.VertexIndex]);
                    WriteInfluences(
                        generated.TopFourInfluences,
                        out topFourJointsByMesh[vertex.MeshIndex][vertex.VertexIndex],
                        out topFourWeightsByMesh[vertex.MeshIndex][vertex.VertexIndex]);
                    assignedByMesh[vertex.MeshIndex][vertex.VertexIndex] = true;
                    smoothByMesh[vertex.MeshIndex][vertex.VertexIndex] = true;
                    smoothVertexCount++;
                    if (generated.AnatomicalVolumeAffected)
                        anatomicalVolumeAffectedVertexCount++;
                    if (generated.LowerBodyWallAffected)
                        lowerBodyWallAffectedVertexCount++;
                    if (generated.ShoulderWallAffected)
                        shoulderWallAffectedVertexCount++;
                    if (usesSemanticRegion)
                    {
                        semanticRegionAppliedVertexCount++;
                        smoothSemanticRegionAppliedVertexCount++;
                    }
                    discardedTopFourWeightMassSum +=
                        generated.DiscardedTopFourWeightMass;
                    topFourToFinalWeightL1DistanceSum +=
                        generated.TopFourToFinalWeightL1Distance;
                    maximumDiscardedTopFourWeightMass = MathF.Max(
                        maximumDiscardedTopFourWeightMass,
                        generated.DiscardedTopFourWeightMass);
                    maximumTopFourToFinalWeightL1Distance = MathF.Max(
                        maximumTopFourToFinalWeightL1Distance,
                        generated.TopFourToFinalWeightL1Distance);
                }
                continue;
            }

            Vector3 alignedCenter = ApplyAlignment(component.Center, alignment);
            string componentLabel = DescribeComponent(component, donorSources);
            BoneCapsule attachmentBone;
            if (manualAssignment is not null)
            {
                attachmentBone = capsules
                    .Where(capsule =>
                        capsule.SkeletonJointIndex == manualAssignment.SkeletonJointIndex)
                    .OrderBy(capsule => DistanceToCapsuleCenterline(
                        alignedCenter, capsule.Start, capsule.End))
                    .FirstOrDefault() ?? throw new InvalidDataException(
                        $"Manual target joint '{manualAssignment.BoneName}' has no " +
                        "generated capsule.");
            }
            else if (planeClassification.Assignment ==
                     GeneratedSkinningComponentAttachmentTarget.UpperBack)
            {
                attachmentBone = capsules
                    .Where(capsule => string.Equals(
                        capsule.BoneName,
                        "Spine_03",
                        StringComparison.Ordinal))
                    .OrderBy(capsule => DistanceToCapsuleCenterline(
                        alignedCenter, capsule.Start, capsule.End))
                    .FirstOrDefault() ?? throw new InvalidDataException(
                        "The Back plane classified a component, but exact target " +
                        "joint Spine_03 has no generated capsule.");
            }
            else
            {
                throw new InvalidDataException(
                    $"Internal logical-body invariant failed for component " +
                    $"#{component.ComponentIndex}: a component may leave the deform " +
                    "body only through an explicit manual assignment or a strict " +
                    "whole-component Back majority.");
            }
            foreach (GeometryVertex vertex in component.Vertices)
            {
                if ((vertex.VertexIndex & 0xFFF) == 0)
                    passState.ThrowIfCancellationRequested();
                jointsByMesh[vertex.MeshIndex][vertex.VertexIndex] =
                    new ImportedJointIndices(
                        checked((ushort)attachmentBone.SkeletonJointIndex), 0, 0, 0);
                weightsByMesh[vertex.MeshIndex][vertex.VertexIndex] = Vector4.UnitX;
                topFourJointsByMesh[vertex.MeshIndex][vertex.VertexIndex] =
                    jointsByMesh[vertex.MeshIndex][vertex.VertexIndex];
                topFourWeightsByMesh[vertex.MeshIndex][vertex.VertexIndex] = Vector4.UnitX;
                assignedByMesh[vertex.MeshIndex][vertex.VertexIndex] = true;
            }
            float distance = DistanceToCapsuleCenterline(
                alignedCenter, attachmentBone.Start, attachmentBone.End);
            attachments.Add(new GeneratedSkinningAttachment(
                component.ComponentIndex,
                Array.AsReadOnly(componentMeshIndices),
                Array.AsReadOnly(componentMeshNames),
                component.Vertices.Length,
                component.TriangleCount,
                attachmentBone.BoneName,
                attachmentBone.SkeletonJointIndex,
                distance,
                alignedCenter)
            {
                VerticesByMesh = componentMembership,
                ManualAssignment = manualAssignment?.Target,
                SemanticAssignment = null,
                PlaneAssignment = manualAssignment is null
                    ? planeClassification.Assignment
                    : null,
                IntersectedPlanes = planeClassification.IntersectedPlanes
            });
            warnings.Add(manualAssignment is not null
                ? $"Detached component {componentLabel}#{component.ComponentIndex} uses the " +
                  $"validated manual {manualAssignment.Target} assignment and is rigidly " +
                  $"one-hot weighted to exact joint {attachmentBone.BoneName}."
                : $"Detached whole component {componentLabel}" +
                  $"#{component.ComponentIndex} has a strict majority behind Back " +
                  $"after Head protection and is rigidly one-hot weighted to exact " +
                  $"joint {attachmentBone.BoneName}; confirm or change its whole-component " +
                  "assignment before writing SMO.");
        }

        // ImportedSkinning remains indexed exactly like every source vertex
        // attribute. FBX/glTF seam expansion can leave vertices which occur
        // only in degenerate faces; they cannot affect a rendered triangle but
        // still need a valid array entry. Use one deterministic safe deform
        // joint for all of them, without counting them as smooth body vertices
        // or detached components.
        if (donorTopology.UnreferencedVertices.Count > 0)
        {
            BoneCapsule placeholder = capsules
                .Where(capsule => capsule.SafeForAutomaticWeights)
                .OrderBy(capsule => capsule.SkeletonJointIndex)
                .ThenBy(capsule => capsule.BoneName, StringComparer.Ordinal)
                .FirstOrDefault() ?? throw new InvalidDataException(
                    "Target fitting rig has no safe deform joint for non-surface " +
                    "donor vertices.");
            ushort placeholderJoint = checked((ushort)placeholder.SkeletonJointIndex);
            foreach (GeometryVertex vertex in donorTopology.UnreferencedVertices)
            {
                if ((vertex.VertexIndex & 0xFFF) == 0)
                    passState.ThrowIfCancellationRequested();
                jointsByMesh[vertex.MeshIndex][vertex.VertexIndex] =
                    new ImportedJointIndices(placeholderJoint, 0, 0, 0);
                weightsByMesh[vertex.MeshIndex][vertex.VertexIndex] = Vector4.UnitX;
                topFourJointsByMesh[vertex.MeshIndex][vertex.VertexIndex] =
                    jointsByMesh[vertex.MeshIndex][vertex.VertexIndex];
                topFourWeightsByMesh[vertex.MeshIndex][vertex.VertexIndex] = Vector4.UnitX;
                assignedByMesh[vertex.MeshIndex][vertex.VertexIndex] = true;
            }
        }

        ValidateLowerBodyWallWeights(
            alignedPositionsByMesh,
            smoothByMesh,
            jointsByMesh,
            weightsByMesh,
            targetSkeleton.Skeleton,
            sideCalibration);
        if (semanticRegionPreparation is not null)
        {
            ValidateShoulderWallWeights(
                alignedPositionsByMesh,
                smoothByMesh,
                jointsByMesh,
                weightsByMesh,
                targetSkeleton.Skeleton,
                semanticRegionPreparation.SeparationPlanes);
            ValidateHeadPlaneWeights(
                alignedPositionsByMesh,
                smoothByMesh,
                jointsByMesh,
                weightsByMesh,
                semanticRegionPreparation.SeparationPlanes,
                semanticRegionPreparation.Analysis);
        }

        var preparedMeshes = new ImportedMesh[donor.Meshes.Count];
        int preparedVertices = 0;
        for (int meshIndex = 0; meshIndex < donor.Meshes.Count; meshIndex++)
        {
            passState.ThrowIfCancellationRequested();
            ImportedMesh source = donor.Meshes[meshIndex];
            if (assignedByMesh[meshIndex].Any(value => !value))
            {
                throw new InvalidDataException(
                    $"Donor mesh [{meshIndex}] {source.Name} contains vertices which could " +
                    "not be assigned without guessing topology.");
            }
            ValidatePackedWeights(
                source.Name,
                jointsByMesh[meshIndex],
                weightsByMesh[meshIndex],
                targetSkeleton.Skeleton);
            preparedMeshes[meshIndex] = source with
            {
                Positions = alignedPositionsByMesh[meshIndex],
                Normals = source.Normals
                    .Select(normal => ApplyNormalAlignment(normal, alignment))
                    .ToArray(),
                TextureCoordinates = source.TextureCoordinates.ToArray(),
                TriangleIndices = donorTopology
                    .RenderableTriangleIndicesByMesh[meshIndex]
                    .ToArray(),
                DiffuseColorsArgb = source.DiffuseColorsArgb?.ToArray(),
                Skinning = new ImportedSkinning(
                    targetSkeleton.Skeleton,
                    jointsByMesh[meshIndex],
                    weightsByMesh[meshIndex])
            };
            preparedVertices += source.Positions.Length;
        }

        float meanDiscardedTopFourWeightMass = smoothVertexCount == 0
            ? 0
            : (float)(discardedTopFourWeightMassSum / smoothVertexCount);
        float meanTopFourToFinalWeightL1Distance = smoothVertexCount == 0
            ? 0
            : (float)(topFourToFinalWeightL1DistanceSum / smoothVertexCount);
        warnings.Add(
            $"Palette-safe mode-3 skinning keeps at most " +
            $"{maximumInfluences} active influences per smooth vertex. " +
            $"Against the nearest-four baseline, discarded normalized weight mass " +
            $"is max {maximumDiscardedTopFourWeightMass:G6}, mean " +
            $"{meanDiscardedTopFourWeightMass:G6}; weight L1 distance is max " +
            $"{maximumTopFourToFinalWeightL1Distance:G6}, mean " +
            $"{meanTopFourToFinalWeightL1Distance:G6}.");
        int capsuleOnlyVertexCount = smoothVertexCount -
                                     anatomicalVolumeAffectedVertexCount -
                                     smoothSemanticRegionAppliedVertexCount;
        if (capsuleOnlyVertexCount < 0)
        {
            throw new InvalidDataException(
                "Generated-skinning accounting produced a negative capsule-only " +
                "vertex count.");
        }
        warnings.Add(
            $"Finite torso anatomical fields changed capsule scores for " +
            $"{anatomicalVolumeAffectedVertexCount} of {smoothVertexCount} smooth " +
            $"vertices; semantic rigid regions replaced weights for " +
            $"{semanticRegionAppliedVertexCount}; " +
            $"{capsuleOnlyVertexCount} smooth vertices used only the target-bone " +
            "capsule field.");
        warnings.Add(
            $"The target-rig sagittal lower-body wall excluded opposite-leg " +
            $"capsules for {lowerBodyWallAffectedVertexCount} smooth " +
            "vertex/vertices below Pelvis/Spine_01; close or crossed donor legs " +
            "therefore cannot exchange left/right leg weights across the wall.");
        warnings.Add(
            $"The two target-shoulder walls filtered arm/body capsule candidates " +
            $"for {shoulderWallAffectedVertexCount} smooth vertex/vertices below " +
            "their shoulder anchors; weights cannot cross either enabled wall.");
        warnings.Add(
            $"Head protection assigned {headProtectedComponentVertexCount} vertex/vertices " +
            "as whole one-hot Head components before any per-vertex capsule " +
            "weighting. Spatially coherent islands from the same imported mesh " +
            "share that ownership, so their lower portions cannot fall through " +
            "to Back/Spine; distant rear islands remain independent.");

        // Imported scenes are immutable by contract. Reuse encoded texture
        // resources while geometry and skin arrays remain independently owned.
        ImportedTexture[] preparedTextures = donor.Textures.ToArray();
        ImportedMaterial[] preparedMaterials = donor.Materials.ToArray();
        var fittingScene = new ImportedScene(
            Array.AsReadOnly(preparedMeshes),
            Array.AsReadOnly(preparedTextures),
            Array.AsReadOnly(preparedMaterials));
        ImportedScene preparedScene = fittingPose is null
            ? fittingScene
            : TargetRigFittingSkinBaker.BakeToCanonical(
                fittingScene,
                fittingPose);
        FittingDeformationComparison deformationComparison =
            CompareTopFourToFinalFittingDeformation(
                preparedScene,
                fittingPose,
                topFourJointsByMesh,
                topFourWeightsByMesh,
                smoothByMesh);
        if (deformationComparison.VertexCount > 0)
        {
            warnings.Add(
                $"In the selected fitting pose, nearest-four versus final weights " +
                $"produce vertex-position delta max " +
                $"{deformationComparison.MaximumPositionDelta:G6}, RMS " +
                $"{deformationComparison.RmsPositionDelta:G6} across " +
                $"{deformationComparison.VertexCount} smooth vertices.");
        }

        string targetRigFingerprint = TargetRigDefinition.ComputeSourceFingerprint(target);
        string donorGeometryFingerprint =
            TargetRigAutomaticPoseFitter.ComputeDonorGeometryFingerprint(donor);
        var analysis = new GeneratedSkinningAnalysis(
            alignment,
            targetSkeleton.DeformJoints.Count,
            targetMain.Vertices.Length,
            targetMain.TriangleCount,
            donorBodyComponents.Sum(component => component.Vertices.Length),
            donorBodyComponents.Sum(component => component.TriangleCount),
            preparedVertices,
            maximumInfluences,
            maximumDiscardedTopFourWeightMass,
            meanDiscardedTopFourWeightMass,
            maximumTopFourToFinalWeightL1Distance,
            meanTopFourToFinalWeightL1Distance,
            deformationComparison.VertexCount,
            deformationComparison.MaximumPositionDelta,
            deformationComparison.RmsPositionDelta,
            new ReadOnlyCollection<GeneratedSkinningAttachment>(attachments),
            new ReadOnlyCollection<string>(warnings),
            RequiresConfirmation: true)
        {
            TargetRigFingerprint = targetRigFingerprint,
            DonorGeometryFingerprint = donorGeometryFingerprint,
            DonorComponentCount = donorTopology.Components.Count,
            Components = new ReadOnlyCollection<GeneratedSkinningComponentInfo>(
                components),
            AnatomicalVolumeAffectedVertexCount = anatomicalVolumeAffectedVertexCount,
            CapsuleOnlyVertexCount = capsuleOnlyVertexCount,
            SemanticRegionAppliedVertexCount = semanticRegionAppliedVertexCount,
            HeadProtectedComponentVertexCount =
                headProtectedComponentVertexCount,
            LowerBodyWallAffectedVertexCount = lowerBodyWallAffectedVertexCount,
            ShoulderWallAffectedVertexCount = shoulderWallAffectedVertexCount,
            InternalPreparationPassCount = passState.PreparationPassCount,
            SemanticResolutionPassCount = passState.SemanticResolutionCount
        };
        if (semanticRegionPreparation is not null)
        {
            analysis = analysis with
            {
                SemanticRegions = semanticRegionPreparation.Analysis
            };
        }
        return new GeneratedSkinningPreparationResult(
            analysis,
            preparedScene)
        {
            FittingPreviewScene = fittingScene
        };
    }

    private static void ValidateGeneratedFittingPoseRoot(
        TargetRigFittingPoseSnapshot fittingPose)
    {
        if (fittingPose.RootRotation != Quaternion.Identity ||
            fittingPose.RootTranslation != Vector3.Zero)
        {
            throw new InvalidOperationException(
                "Generated-skinning fitting supports local bone rotations only; " +
                "root rotation and translation require an explicit donor-alignment " +
                "space contract.");
        }
    }

    private static FittingDeformationComparison
        CompareTopFourToFinalFittingDeformation(
            ImportedScene canonicalScene,
            TargetRigFittingPoseSnapshot? fittingPose,
            IReadOnlyList<ImportedJointIndices[]> topFourJointsByMesh,
            IReadOnlyList<Vector4[]> topFourWeightsByMesh,
            IReadOnlyList<bool[]> smoothByMesh)
    {
        if (fittingPose is null || fittingPose.IsIdentityPose)
            return new FittingDeformationComparison(0, 0, 0);
        if (canonicalScene.Meshes.Count != topFourJointsByMesh.Count ||
            canonicalScene.Meshes.Count != topFourWeightsByMesh.Count ||
            canonicalScene.Meshes.Count != smoothByMesh.Count)
        {
            throw new InvalidDataException(
                "Top-four fitting comparison arrays do not match the prepared scene.");
        }

        var transformsBySkeleton = new Dictionary<ImportedSkeleton, Matrix4x4[]>(
            ReferenceEqualityComparer.Instance);
        float maximumDelta = 0;
        double squaredDeltaSum = 0;
        int vertexCount = 0;
        for (int meshIndex = 0; meshIndex < canonicalScene.Meshes.Count; meshIndex++)
        {
            ImportedMesh mesh = canonicalScene.Meshes[meshIndex];
            ImportedSkinning skinning = mesh.Skinning ?? throw new InvalidDataException(
                $"Prepared mesh '{mesh.Name}' has no skinning for fitting comparison.");
            if (!transformsBySkeleton.TryGetValue(
                    skinning.Skeleton, out Matrix4x4[]? transforms))
            {
                transforms = new Matrix4x4[skinning.Skeleton.JointNames.Count];
                for (int joint = 0; joint < transforms.Length; joint++)
                {
                    string name = skinning.Skeleton.JointNames[joint];
                    int rigJoint = fittingPose.Definition.GetJointIndex(name);
                    transforms[joint] = skinning.Skeleton.InverseBindMatrices[joint] *
                        fittingPose.WorldMatrices[rigJoint];
                    if (!IsFinite(transforms[joint]))
                    {
                        throw new InvalidDataException(
                            $"Fitting comparison transform for '{name}' is non-finite.");
                    }
                }
                transformsBySkeleton.Add(skinning.Skeleton, transforms);
            }
            if (topFourJointsByMesh[meshIndex].Length != mesh.Positions.Length ||
                topFourWeightsByMesh[meshIndex].Length != mesh.Positions.Length ||
                smoothByMesh[meshIndex].Length != mesh.Positions.Length)
            {
                throw new InvalidDataException(
                    $"Top-four fitting comparison data for '{mesh.Name}' is incomplete.");
            }

            for (int vertex = 0; vertex < mesh.Positions.Length; vertex++)
            {
                if (!smoothByMesh[meshIndex][vertex])
                    continue;
                Vector3 finalPosition = ApplyWeightedFittingTransform(
                    mesh.Positions[vertex],
                    skinning.JointIndices[vertex],
                    skinning.Weights[vertex],
                    transforms);
                Vector3 topFourPosition = ApplyWeightedFittingTransform(
                    mesh.Positions[vertex],
                    topFourJointsByMesh[meshIndex][vertex],
                    topFourWeightsByMesh[meshIndex][vertex],
                    transforms);
                float delta = Vector3.Distance(finalPosition, topFourPosition);
                if (!float.IsFinite(delta))
                    throw new InvalidDataException(
                        "Top-four fitting comparison produced a non-finite delta.");
                maximumDelta = MathF.Max(maximumDelta, delta);
                squaredDeltaSum += (double)delta * delta;
                vertexCount++;
            }
        }

        return new FittingDeformationComparison(
            vertexCount,
            maximumDelta,
            vertexCount == 0
                ? 0
                : (float)Math.Sqrt(squaredDeltaSum / vertexCount));
    }

    private static Vector3 ApplyWeightedFittingTransform(
        Vector3 position,
        ImportedJointIndices joints,
        Vector4 weights,
        IReadOnlyList<Matrix4x4> transforms)
    {
        ushort[] jointValues = [joints.X, joints.Y, joints.Z, joints.W];
        float[] weightValues = [weights.X, weights.Y, weights.Z, weights.W];
        Vector3 result = Vector3.Zero;
        float total = 0;
        for (int influence = 0; influence < 4; influence++)
        {
            float weight = weightValues[influence];
            if (!float.IsFinite(weight) || weight < 0)
            {
                throw new InvalidDataException(
                    "Fitting comparison contains an invalid weight.");
            }
            if (weight <= WeightEpsilon)
                continue;
            if (jointValues[influence] >= transforms.Count)
            {
                throw new InvalidDataException(
                    "Fitting comparison contains an invalid weighted joint.");
            }
            result += Vector3.Transform(
                position, transforms[jointValues[influence]]) * weight;
            total += weight;
        }
        if (!float.IsFinite(total) || total <= WeightEpsilon || !IsFinite(result))
            throw new InvalidDataException("Fitting comparison cannot normalize a vertex.");
        return result / total;
    }

    private static TargetSkeletonLayout BuildTargetSkeleton(
        TargetRigDefinition rig)
    {
        TargetRigJoint[] deform = rig.Joints
            .Where(joint => joint.IsDeformJoint)
            .ToArray();
        if (deform.Length == 0)
            throw new InvalidDataException("Target fitting rig has no deform joints.");
        if (deform.Length > ushort.MaxValue)
            throw new InvalidDataException("Target fitting rig exceeds UInt16 joint indices.");

        Dictionary<int, int> skeletonIndexByRigJoint = deform
            .Select((joint, skeletonIndex) => (joint.JointIndex, skeletonIndex))
            .ToDictionary(pair => pair.JointIndex, pair => pair.skeletonIndex);
        var parents = new int[deform.Length];
        var bindWorld = new Matrix4x4[deform.Length];
        var inverseBind = new Matrix4x4[deform.Length];
        var bindLocal = new Matrix4x4[deform.Length];
        for (int skeletonIndex = 0; skeletonIndex < deform.Length; skeletonIndex++)
        {
            TargetRigJoint joint = deform[skeletonIndex];
            int parentRigJoint = FindNearestDeformParent(rig, joint.JointIndex);
            int parentSkeleton = parentRigJoint >= 0
                ? skeletonIndexByRigJoint[parentRigJoint]
                : -1;
            parents[skeletonIndex] = parentSkeleton;
            bindWorld[skeletonIndex] = joint.BindWorldMatrix;
            if (!Matrix4x4.Invert(joint.BindWorldMatrix, out inverseBind[skeletonIndex]) ||
                !IsFinite(inverseBind[skeletonIndex]))
            {
                throw new InvalidDataException(
                    $"Target deform joint {joint.Name} has no finite inverse bind matrix.");
            }
            bindLocal[skeletonIndex] = joint.BindWorldMatrix;
            if (parentSkeleton >= 0)
            {
                if (!Matrix4x4.Invert(
                        deform[parentSkeleton].BindWorldMatrix,
                        out Matrix4x4 inverseParent) || !IsFinite(inverseParent))
                {
                    throw new InvalidDataException(
                        $"Target deform parent of {joint.Name} is not invertible.");
                }
                bindLocal[skeletonIndex] = joint.BindWorldMatrix * inverseParent;
            }
        }

        var skeleton = new ImportedSkeleton(
            "generated_target_skeleton",
            deform.Select(joint => joint.Name).ToArray(),
            Array.AsReadOnly(inverseBind))
        {
            ParentJointIndices = Array.AsReadOnly(parents),
            BindWorldMatrices = Array.AsReadOnly(bindWorld),
            BindLocalMatrices = Array.AsReadOnly(bindLocal)
        };
        return new TargetSkeletonLayout(
            skeleton,
            Array.AsReadOnly(deform),
            new ReadOnlyDictionary<int, int>(skeletonIndexByRigJoint));
    }

    private static int FindNearestDeformParent(
        TargetRigDefinition rig,
        int jointIndex)
    {
        var visited = new HashSet<int> { jointIndex };
        int cursor = rig.Joints[jointIndex].ParentJointIndex;
        while (cursor >= 0)
        {
            if (!visited.Add(cursor))
                throw new InvalidDataException("Target fitting rig contains a parent cycle.");
            TargetRigJoint parent = rig.Joints[cursor];
            if (parent.IsDeformJoint)
                return cursor;
            cursor = parent.ParentJointIndex;
        }
        return -1;
    }

    private static IReadOnlyList<BoneCapsule> BuildBoneCapsules(
        TargetRigDefinition rig,
        TargetSkeletonLayout layout,
        RobustBounds targetBounds,
        IReadOnlyList<Matrix4x4>? fittingWorldMatrices)
    {
        float height = targetBounds.Size.Y;
        float minimumRadius = MathF.Max(height * 0.015f, PositionEpsilon);
        var children = layout.DeformJoints.ToDictionary(
            joint => joint.JointIndex,
            _ => new List<TargetRigJoint>());
        foreach (TargetRigJoint child in layout.DeformJoints)
        {
            int parent = FindNearestDeformParent(rig, child.JointIndex);
            if (parent >= 0 && children.TryGetValue(parent, out List<TargetRigJoint>? list))
                list.Add(child);
        }

        var result = new List<BoneCapsule>();
        foreach (TargetRigJoint joint in layout.DeformJoints)
        {
            int skeletonIndex = layout.SkeletonIndexByRigJoint[joint.JointIndex];
            Vector3 start = Translation(GetFittingWorldMatrix(
                joint,
                fittingWorldMatrices));
            BodySide side = ClassifyBoneSide(joint.Name);
            bool safeAutomaticWeights = IsSafeAutomaticWeightBone(joint.Name);
            IReadOnlyList<TargetRigJoint> deformChildren = children[joint.JointIndex];
            if (deformChildren.Count == 0)
            {
                result.Add(new BoneCapsule(
                    skeletonIndex,
                    joint.Name,
                    side,
                    start,
                    start,
                    minimumRadius * 1.5f,
                    safeAutomaticWeights));
                continue;
            }

            foreach (TargetRigJoint child in deformChildren.OrderBy(value => value.JointIndex))
            {
                Vector3 end = Translation(GetFittingWorldMatrix(
                    child,
                    fittingWorldMatrices));
                float length = Vector3.Distance(start, end);
                if (!float.IsFinite(length))
                    throw new InvalidDataException($"Target bone {joint.Name} has non-finite length.");
                float radius = MathF.Max(minimumRadius, length * 0.22f);
                result.Add(new BoneCapsule(
                    skeletonIndex,
                    joint.Name,
                    side,
                    start,
                    end,
                    radius,
                    safeAutomaticWeights));
            }
        }
        return new ReadOnlyCollection<BoneCapsule>(result);
    }

    private static IReadOnlyDictionary<int, AnatomicalVolume> BuildAnatomicalVolumes(
        TargetRigDefinition rig,
        TargetSkeletonLayout layout,
        SmoExportScene targetScene,
        IReadOnlyList<SmoExportMesh> targetSkinnedMeshes,
        RobustBounds targetBounds,
        IReadOnlyList<Matrix4x4>? fittingWorldMatrices,
        out IReadOnlyList<string> diagnostics)
    {
        string[] chain =
        [
            "Spine_01", "Spine_02", "Spine_03", "Neck", "Head"
        ];
        Dictionary<string, TargetRigJoint> jointsByName = layout.DeformJoints
            .Where(joint => chain.Contains(joint.Name, StringComparer.Ordinal))
            .GroupBy(joint => joint.Name, StringComparer.Ordinal)
            .Where(group => group.Count() == 1)
            .ToDictionary(group => group.Key, group => group.Single(), StringComparer.Ordinal);
        Dictionary<int, List<Vector3>> samplesBySkeleton =
            CollectTargetWeightSamples(
                rig,
                layout,
                targetScene,
                targetSkinnedMeshes,
                jointsByName.Values.Select(joint => joint.JointIndex).ToHashSet());
        SideCalibration bindSides = CalibrateSides(
            layout.DeformJoints,
            targetBounds,
            fittingWorldMatrices: null);
        SideCalibration fittingSides = fittingWorldMatrices is null
            ? bindSides
            : CalibrateSides(layout.DeformJoints, targetBounds, fittingWorldMatrices);
        Vector3 bindLateral = GetCalibratedLateralAxis(bindSides);
        Vector3 fittingLateral = GetCalibratedLateralAxis(fittingSides);
        float minimumRadius = MathF.Max(targetBounds.Size.Y * 0.015f, PositionEpsilon);
        var result = new Dictionary<int, AnatomicalVolume>();
        var messages = new List<string>();

        for (int index = 0; index < chain.Length - 1; index++)
        {
            string boneName = chain[index];
            string childName = chain[index + 1];
            if (!jointsByName.TryGetValue(boneName, out TargetRigJoint? joint) ||
                !jointsByName.TryGetValue(childName, out TargetRigJoint? child) ||
                !layout.SkeletonIndexByRigJoint.TryGetValue(
                    joint.JointIndex,
                    out int skeletonIndex) ||
                !samplesBySkeleton.TryGetValue(skeletonIndex, out List<Vector3>? samples) ||
                samples.Distinct().Count() < MinimumEnvelopeSamples)
            {
                messages.Add(
                    $"Target-weight anatomical field {boneName}->{childName} was skipped: " +
                    $"it requires exact deform-joint names and at least " +
                    $"{MinimumEnvelopeSamples} vertices carrying >= " +
                    $"{TargetEnvelopeWeightThreshold:G3} total {boneName} weight.");
                continue;
            }

            Vector3 bindStart = Translation(joint.BindWorldMatrix);
            Vector3 bindEnd = Translation(child.BindWorldMatrix);
            if (!IsFinite(bindStart) || !IsFinite(bindEnd) ||
                Vector3.DistanceSquared(bindStart, bindEnd) <= PositionEpsilon)
            {
                messages.Add(
                    $"Target-weight anatomical field {boneName}->{childName} was skipped: " +
                    "the exact target joints have a degenerate bind-space axis.");
                continue;
            }
            BuildVolumeFrame(
                bindStart,
                bindEnd,
                bindLateral,
                out _,
                out Vector3 bindFrameLateral,
                out Vector3 bindForward);
            Vector3[] uniqueSamples = samples.Distinct().ToArray();
            float[] initialForward = uniqueSamples
                .Select(position => Vector3.Dot(position - bindStart, bindForward))
                .Order()
                .ToArray();
            float positiveExtent = Quantile(initialForward, RobustUpperQuantile);
            float negativeExtent = -Quantile(initialForward, RobustLowerQuantile);
            float forwardSign = positiveExtent >= negativeExtent ? 1f : -1f;
            bindForward *= forwardSign;
            float[] lateralDistances = uniqueSamples
                .Select(position => MathF.Abs(Vector3.Dot(
                    position - bindStart, bindFrameLateral)))
                .Order()
                .ToArray();
            float[] forwardDistances = uniqueSamples
                .Select(position => MathF.Max(0, Vector3.Dot(
                    position - bindStart, bindForward)))
                .Order()
                .ToArray();
            (float lateralHeightFloor, float forwardHeightFloor) = boneName switch
            {
                // The shared 8.7% lateral floor approximates the pooled central
                // target-chain cross-section while keeping the 1.25 fade shell
                // inside the outer-arm threshold. Depth expands with height,
                // because the per-joint >=50% samples systematically undershoot
                // the visible front chest surface.
                "Spine_01" => (0.087f, 0.075f),
                "Spine_02" => (0.087f, 0.080f),
                "Spine_03" => (0.087f, 0.085f),
                "Neck" => (0.025f, 0.025f),
                _ => (0.025f, 0.025f)
            };
            float lateralRadius = MathF.Max(
                Quantile(lateralDistances, RobustUpperQuantile),
                MathF.Max(minimumRadius, targetBounds.Size.Y * lateralHeightFloor));
            // The centre is shifted forward by the semi-radius. Consequently
            // c - f*b is exactly the source spine line: the posterior wall is
            // invariantly tangent to the skeleton rather than centred on it.
            float forwardRadius = MathF.Max(
                Quantile(forwardDistances, RobustUpperQuantile) * 0.5f,
                MathF.Max(minimumRadius, targetBounds.Size.Y * forwardHeightFloor));

            Vector3 posedStart = Translation(GetFittingWorldMatrix(
                joint,
                fittingWorldMatrices));
            Vector3 posedEnd = Translation(GetFittingWorldMatrix(
                child,
                fittingWorldMatrices));
            BuildVolumeFrame(
                posedStart,
                posedEnd,
                fittingLateral,
                out _,
                out Vector3 posedFrameLateral,
                out Vector3 posedForward);
            posedForward *= forwardSign;
            result.Add(skeletonIndex, new AnatomicalVolume(
                skeletonIndex,
                boneName,
                posedStart,
                posedEnd,
                posedFrameLateral,
                posedForward,
                lateralRadius,
                forwardRadius,
                MathF.Max(minimumRadius, MathF.Min(lateralRadius, forwardRadius)),
                IsHead: false,
                uniqueSamples.Length));
            messages.Add(
                $"Target-weight shifted torso field {boneName}->{childName}: " +
                $"{uniqueSamples.Length} samples, lateral radius {lateralRadius:G6}, " +
                $"forward semi-radius {forwardRadius:G6}; its posterior wall exactly " +
                "touches the posed spine line.");
        }

        diagnostics = new ReadOnlyCollection<string>(messages);
        return new ReadOnlyDictionary<int, AnatomicalVolume>(result);
    }

    private static Dictionary<int, List<Vector3>> CollectTargetWeightSamples(
        TargetRigDefinition rig,
        TargetSkeletonLayout layout,
        SmoExportScene targetScene,
        IReadOnlyList<SmoExportMesh> meshes,
        IReadOnlySet<int> acceptedRigJoints)
    {
        var result = layout.DeformJoints
            .Where(joint => acceptedRigJoints.Contains(joint.JointIndex))
            .ToDictionary(
                joint => layout.SkeletonIndexByRigJoint[joint.JointIndex],
                _ => new List<Vector3>());
        Dictionary<int, SmoExportSkin> skins = targetScene.Skins
            .GroupBy(skin => skin.ObjectIndex)
            .Where(group => group.Count() == 1)
            .ToDictionary(group => group.Key, group => group.Single());
        foreach (SmoExportMesh mesh in meshes)
        {
            if (mesh.SkinObjectIndex is not int skinObjectIndex ||
                !skins.TryGetValue(skinObjectIndex, out SmoExportSkin? skin))
                continue;
            for (int vertex = 0; vertex < mesh.Positions.Length; vertex++)
            {
                var accumulated = new Dictionary<int, float>();
                for (int slot = 0; slot < 4; slot++)
                {
                    float weight = VectorComponent(mesh.BlendWeights[vertex], slot);
                    if (!float.IsFinite(weight) || weight < 0)
                    {
                        throw new InvalidDataException(
                            "Target SMO contains a non-finite or negative skin weight.");
                    }
                    if (weight <= WeightEpsilon)
                        continue;
                    float indexValue = VectorComponent(mesh.JointIndices[vertex], slot);
                    if (!float.IsFinite(indexValue) ||
                        indexValue < int.MinValue ||
                        indexValue > int.MaxValue)
                    {
                        throw new InvalidDataException(
                            "Target SMO contains an out-of-range weighted skin palette index.");
                    }
                    int paletteIndex = checked((int)MathF.Round(indexValue));
                    if (MathF.Abs(indexValue - paletteIndex) > 0.001f ||
                        (uint)paletteIndex >= (uint)skin.JointObjectIndices.Count)
                    {
                        throw new InvalidDataException(
                            "Target SMO contains an invalid weighted skin palette index.");
                    }
                    int rigJoint;
                    try
                    {
                        rigJoint = rig.GetJointIndexByObjectIndex(
                            skin.JointObjectIndices[paletteIndex]);
                    }
                    catch (KeyNotFoundException)
                    {
                        continue;
                    }
                    if (!acceptedRigJoints.Contains(rigJoint) ||
                        !layout.SkeletonIndexByRigJoint.TryGetValue(
                            rigJoint,
                            out int skeletonIndex))
                        continue;
                    accumulated[skeletonIndex] =
                        accumulated.GetValueOrDefault(skeletonIndex) + weight;
                }
                foreach ((int skeletonIndex, float weight) in accumulated)
                {
                    if (weight >= TargetEnvelopeWeightThreshold)
                        result[skeletonIndex].Add(mesh.Positions[vertex]);
                }
            }
        }
        return result;
    }

    private static Vector3 GetCalibratedLateralAxis(SideCalibration calibration) =>
        calibration.UseVectorAxis
            ? calibration.LeftAxis
            : new Vector3(calibration.LeftDirection, 0, 0);

    private static void BuildVolumeFrame(
        Vector3 start,
        Vector3 end,
        Vector3 lateralHint,
        out Vector3 up,
        out Vector3 lateral,
        out Vector3 forward)
    {
        Vector3 direction = end - start;
        if (!IsFinite(direction) || direction.LengthSquared() <= PositionEpsilon)
            throw new InvalidDataException("An anatomical volume has a degenerate axis.");
        up = Vector3.Normalize(direction);
        lateral = lateralHint - up * Vector3.Dot(lateralHint, up);
        if (!IsFinite(lateral) || lateral.LengthSquared() <= PositionEpsilon)
            throw new InvalidDataException(
                "The target side axis is parallel to an anatomical volume axis.");
        lateral = Vector3.Normalize(lateral);
        forward = Vector3.Cross(lateral, up);
        if (!IsFinite(forward) || forward.LengthSquared() <= PositionEpsilon)
            throw new InvalidDataException("An anatomical volume frame is degenerate.");
        forward = Vector3.Normalize(forward);
    }

    private static float VectorComponent(Vector4 value, int index) => index switch
    {
        0 => value.X,
        1 => value.Y,
        2 => value.Z,
        3 => value.W,
        _ => throw new ArgumentOutOfRangeException(nameof(index))
    };

    private static GeneratedVertexInfluences GenerateVertexInfluences(
        Vector3 position,
        IReadOnlyList<BoneCapsule> capsules,
        IReadOnlyDictionary<int, AnatomicalVolume> anatomicalVolumes,
        SideCalibration calibration,
        float targetHeight,
        int maximumInfluences,
        SeparationPlanePreparation? separationPlanes = null)
    {
        BodySide vertexSide = ClassifyPositionSide(position, calibration);
        BodySide lowerBodyWallSide = ClassifyLowerBodyWallSide(
            position,
            calibration);
        BodySide compatibilitySide = lowerBodyWallSide == BodySide.Center
            ? vertexSide
            : lowerBodyWallSide;
        bool lowerBodyWallAffected = lowerBodyWallSide != BodySide.Center &&
                                     lowerBodyWallSide != vertexSide;
        if (TryGetRigidHeadPlaneJoint(
                position,
                separationPlanes,
                out int rigidHeadJoint))
        {
            PackedInfluence[] rigidHead =
            [new PackedInfluence(checked((ushort)rigidHeadJoint), 1)];
            return new GeneratedVertexInfluences(
                rigidHead,
                rigidHead.ToArray(),
                DiscardedTopFourWeightMass: 0,
                TopFourToFinalWeightL1Distance: 0,
                AnatomicalVolumeAffected: false)
            {
                LowerBodyWallAffected = lowerBodyWallAffected,
                ShoulderWallAffected = false
            };
        }
        bool anatomicalVolumeAffected = false;
        float torsoFieldAlpha = anatomicalVolumes.Values
            .Where(volume => IsTorsoFieldBone(volume.BoneName))
            .Select(volume => AnatomicalVolumeAlpha(
                DistanceToAnatomicalVolume(position, volume)))
            .DefaultIfEmpty(0)
            .Max();
        float nearestAutomaticCapsuleDistance = capsules
            .Where(capsule => capsule.SafeForAutomaticWeights)
            .Select(capsule => DistanceToCapsuleCenterline(
                position,
                capsule.Start,
                capsule.End))
            .DefaultIfEmpty(float.PositiveInfinity)
            .Min();
        BoneCapsule[] sideCompatibleCapsules = capsules
            .Where(capsule => capsule.SafeForAutomaticWeights &&
                              !IsOpposite(compatibilitySide, capsule.Side))
            .ToArray();
        ShoulderPlaneClassification shoulderClassification =
            ClassifyShoulderPlanes(position, separationPlanes);
        BoneCapsule[] shoulderCompatibleCapsules = sideCompatibleCapsules
            .Where(capsule => IsShoulderCapsuleCompatible(
                capsule.SkeletonJointIndex,
                shoulderClassification,
                separationPlanes))
            .ToArray();
        bool shoulderWallAffected =
            shoulderCompatibleCapsules.Length != sideCompatibleCapsules.Length;
        BoneCapsule[] planeCompatibleCapsules = shoulderCompatibleCapsules
            .Where(capsule => IsHeadPlaneCapsuleCompatible(
                position,
                capsule.SkeletonJointIndex,
                separationPlanes))
            .ToArray();
        var distances = planeCompatibleCapsules
            .GroupBy(capsule => capsule.SkeletonJointIndex)
            .Select(group =>
            {
                BoneCapsule representative = group.First();
                float capsuleNormalizedDistance = group.Min(capsule =>
                    DistanceToCapsuleCenterline(position, capsule.Start, capsule.End) /
                    capsule.Radius);
                float normalizedDistance = capsuleNormalizedDistance;
                if (anatomicalVolumes.TryGetValue(
                        representative.SkeletonJointIndex,
                        out AnatomicalVolume? volume) &&
                    !volume.IsHead)
                {
                    float shapeDistance = DistanceToAnatomicalVolume(position, volume);
                    float alpha = AnatomicalVolumeAlpha(shapeDistance);
                    // The calibrated primitive is a solid anatomical volume,
                    // not another centreline. Every point inside it has zero
                    // distance to the volume; only the exterior shell grows a
                    // normalized distance. This prevents nearby arm/thigh
                    // capsules from winning across the chest interior.
                    float volumeDistance = MathF.Max(
                        0,
                        shapeDistance - EnvelopeCoreRatio);
                    if (alpha > 0 && volumeDistance < capsuleNormalizedDistance)
                    {
                        // Do not evaluate a lerp at alpha zero. This explicit
                        normalizedDistance = capsuleNormalizedDistance +
                            (volumeDistance - capsuleNormalizedDistance) * alpha;
                        anatomicalVolumeAffected |=
                            BitConverter.SingleToInt32Bits(normalizedDistance) !=
                            BitConverter.SingleToInt32Bits(capsuleNormalizedDistance);
                    }
                }
                if (torsoFieldAlpha > 0 &&
                    !IsCentralBodyBone(representative.BoneName))
                {
                    // Inside the finite torso envelope, limb capsules can be
                    // geometrically near-zero (especially long thighs and
                    // biceps) even though the vertex lies on the chest. Push
                    // only those non-central candidates out of the central
                    // competition. The additive term is bypassed exactly when
                    // alpha is zero, preserving all outer-limb float bits.
                    float penalized = normalizedDistance + 16 * torsoFieldAlpha;
                    anatomicalVolumeAffected |=
                        BitConverter.SingleToInt32Bits(penalized) !=
                        BitConverter.SingleToInt32Bits(capsuleNormalizedDistance);
                    normalizedDistance = penalized;
                }
                float absoluteDistance = group.Min(capsule =>
                    DistanceToCapsuleCenterline(position, capsule.Start, capsule.End));
                return (representative.SkeletonJointIndex, normalizedDistance, absoluteDistance);
            })
            .OrderBy(value => value.normalizedDistance)
            .ThenBy(value => value.SkeletonJointIndex)
            .Take(TopFourComparisonInfluences)
            .ToArray();
        if (distances.Length == 0 ||
            distances.Any(value => !float.IsFinite(value.normalizedDistance) ||
                                   !float.IsFinite(value.absoluteDistance)))
        {
            throw new InvalidDataException(
                "A donor body vertex has no finite target bone capsule compatible " +
                "with the side and shoulder separation walls.");
        }
        // This guard diagnoses a bad donor-to-target alignment. Separation
        // planes deliberately remove nearby but anatomically incompatible
        // capsules, so using the first surviving candidate here would reject
        // otherwise valid vertices near a wall. Measure against the complete
        // safe target skeleton while keeping the filtered list for weights.
        if (!float.IsFinite(nearestAutomaticCapsuleDistance) ||
            nearestAutomaticCapsuleDistance > targetHeight * 0.75f)
        {
            throw new InvalidDataException(
                "The aligned donor body extends too far from the target skeleton for " +
                "conservative automatic weighting.");
        }

        float[] raw = distances.Select(value =>
        {
            float squared = value.normalizedDistance * value.normalizedDistance;
            return 1f / (0.05f + squared);
        }).ToArray();
        float topFourTotal = raw.Sum();
        if (!float.IsFinite(topFourTotal) || topFourTotal <= WeightEpsilon)
            throw new InvalidDataException("Generated body weights cannot be normalized.");

        int retainedCount = Math.Min(maximumInfluences, distances.Length);
        float retainedTotal = raw.Take(retainedCount).Sum();
        if (!float.IsFinite(retainedTotal) || retainedTotal <= WeightEpsilon)
            throw new InvalidDataException("Palette-safe body weights cannot be normalized.");
        float discardedMass = MathF.Max(0, (topFourTotal - retainedTotal) / topFourTotal);
        float l1Distance = discardedMass;
        for (int index = 0; index < retainedCount; index++)
        {
            l1Distance += MathF.Abs(
                raw[index] / retainedTotal - raw[index] / topFourTotal);
        }
        if (!float.IsFinite(discardedMass) || !float.IsFinite(l1Distance))
            throw new InvalidDataException(
                "Palette-safe body-weight comparison produced non-finite diagnostics.");

        PackedInfluence[] topFourInfluences = distances
            .Select((value, index) => new PackedInfluence(
                checked((ushort)value.SkeletonJointIndex),
                raw[index] / topFourTotal))
            .ToArray();
        PackedInfluence[] influences = distances.Take(retainedCount)
            .Select((value, index) => new PackedInfluence(
                checked((ushort)value.SkeletonJointIndex),
                raw[index] / retainedTotal))
            .ToArray();
        return new GeneratedVertexInfluences(
            influences,
            topFourInfluences,
            discardedMass,
            l1Distance,
            anatomicalVolumeAffected)
        {
            LowerBodyWallAffected = lowerBodyWallAffected,
            ShoulderWallAffected = shoulderWallAffected
        };
    }

    private static bool IsTorsoFieldBone(string boneName) =>
        boneName is "Spine_01" or "Spine_02" or "Spine_03";

    private static bool IsCentralBodyBone(string boneName) =>
        boneName is "Pelvis" or "Spine_01" or "Spine_02" or "Spine_03" or
            "Neck" or "Head";

    private static float DistanceToAnatomicalVolume(
        Vector3 position,
        AnatomicalVolume volume)
    {
        if (volume.IsHead)
        {
            Vector3 up = Vector3.Normalize(volume.End - volume.Start);
            Vector3 relative = position - volume.Start;
            float headLateral = Vector3.Dot(relative, volume.LateralAxis) /
                                volume.LateralRadius;
            float headForward = Vector3.Dot(relative, volume.ForwardAxis) /
                                volume.ForwardRadius;
            float headAxial = Vector3.Dot(relative, up) / volume.AxialRadius;
            float headSquared = headLateral * headLateral +
                                headForward * headForward +
                                headAxial * headAxial;
            return MathF.Sqrt(MathF.Max(0, headSquared));
        }

        Vector3 axis = volume.End - volume.Start;
        float length = axis.Length();
        if (!float.IsFinite(length) || length <= PositionEpsilon)
            throw new InvalidDataException(
                $"Anatomical field {volume.BoneName} has a degenerate posed axis.");
        Vector3 upAxis = axis / length;
        float axialDistance = Vector3.Dot(position - volume.Start, upAxis);
        if (string.Equals(volume.BoneName, "Spine_01", StringComparison.Ordinal) &&
            axialDistance < 0)
        {
            // Hard lower torso boundary: the new field contributes exactly
            // zero below Spine_01, keeping pelvis/thigh/leg capsule scores on
            // their capsule-only path.
            return EnvelopeFadeRatio;
        }
        if (string.Equals(volume.BoneName, "Spine_03", StringComparison.Ordinal) &&
            axialDistance > length)
        {
            // The upper torso field ends at Neck. Neck/Head blending is owned
            // by their own calibrated fields, so the expanded chest cannot
            // compete inside the skull.
            return EnvelopeFadeRatio;
        }
        float clampedAxial = Math.Clamp(axialDistance, 0, length);
        float outsideAxial = axialDistance < 0
            ? -axialDistance
            : axialDistance > length
                ? axialDistance - length
                : 0;
        Vector3 shiftedCenterline = volume.Start +
                                    upAxis * clampedAxial +
                                    volume.ForwardAxis * volume.ForwardRadius;
        Vector3 relativeToCenterline = position - shiftedCenterline;
        float lateral = Vector3.Dot(
            relativeToCenterline,
            volume.LateralAxis) / volume.LateralRadius;
        float forward = Vector3.Dot(
            relativeToCenterline,
            volume.ForwardAxis) / volume.ForwardRadius;
        float axial = outsideAxial / volume.AxialRadius;
        float squared = lateral * lateral + forward * forward + axial * axial;
        return MathF.Sqrt(MathF.Max(0, squared));
    }

    private static float AnatomicalVolumeAlpha(float normalizedDistance)
    {
        if (!float.IsFinite(normalizedDistance) ||
            normalizedDistance >= EnvelopeFadeRatio)
            return 0;
        if (normalizedDistance <= EnvelopeCoreRatio)
            return 1;
        float amount = (EnvelopeFadeRatio - normalizedDistance) /
                       (EnvelopeFadeRatio - EnvelopeCoreRatio);
        // Smoothstep stays exactly zero/one at the field boundaries.
        return amount * amount * (3 - 2 * amount);
    }

    private static bool TrySelectElongatedHeadRoot(
        GeometryComponent component,
        IReadOnlyList<Vector3[]> alignedPositionsByMesh,
        IReadOnlyList<BoneCapsule> capsules,
        float targetHeight,
        out BoneCapsule rootedBone)
    {
        rootedBone = null!;
        BoneCapsule? head = capsules
            .Where(capsule => capsule.SafeForAutomaticWeights &&
                              string.Equals(
                                  capsule.BoneName,
                                  "Head",
                                  StringComparison.Ordinal))
            .OrderBy(capsule => capsule.SkeletonJointIndex)
            .FirstOrDefault();
        if (head is null)
            return false;

        Vector3[] positions = component.Vertices
            .Select(vertex => alignedPositionsByMesh[vertex.MeshIndex]
                [vertex.VertexIndex])
            .Distinct()
            .ToArray();
        if (positions.Length < 8)
            return false;
        float minimumY = positions.Min(position => position.Y);
        float maximumY = positions.Max(position => position.Y);
        float verticalExtent = maximumY - minimumY;
        if (!float.IsFinite(verticalExtent) ||
            verticalExtent < targetHeight * 0.45f ||
            maximumY < MathF.Max(head.Start.Y, head.End.Y) -
                       targetHeight * 0.12f)
        {
            return false;
        }

        float topBandHeight = MathF.Max(
            verticalExtent * 0.12f,
            targetHeight * 0.025f);
        Vector3[] topBand = positions
            .Where(position => position.Y >= maximumY - topBandHeight)
            .ToArray();
        if (topBand.Length < 4)
            return false;
        float rootRadius = targetHeight * 0.11f;
        int nearHead = topBand.Count(position =>
            DistanceToCapsuleCenterline(position, head.Start, head.End) <=
            rootRadius);
        int required = Math.Max(4, (int)MathF.Ceiling(topBand.Length * 0.03f));
        if (nearHead < required)
            return false;

        rootedBone = head;
        return true;
    }

    private static BoneCapsule SelectAttachmentBone(
        Vector3 center,
        IReadOnlyList<BoneCapsule> capsules,
        SideCalibration calibration,
        float targetHeight,
        ICollection<string> warnings,
        string meshName,
        int componentIndex)
    {
        BodySide side = ClassifyPositionSide(center, calibration);
        var candidates = capsules
            .Where(capsule => capsule.SafeForAutomaticWeights &&
                              !IsOpposite(side, capsule.Side))
            .GroupBy(capsule => capsule.SkeletonJointIndex)
            .Select(group =>
            {
                BoneCapsule representative = group.First();
                float distance = group.Min(capsule =>
                    DistanceToCapsuleCenterline(center, capsule.Start, capsule.End));
                return (Capsule: representative, Distance: distance);
            })
            .OrderBy(value => value.Distance)
            .ThenBy(value => value.Capsule.SkeletonJointIndex)
            .ToArray();
        if (candidates.Length == 0 || !float.IsFinite(candidates[0].Distance))
        {
            throw new InvalidDataException(
                $"Detached component {meshName}#{componentIndex} has no safe " +
                "side-compatible target deform bone.");
        }
        if (candidates.Length > 1)
        {
            float ambiguity = MathF.Max(targetHeight * 0.005f,
                candidates[0].Distance * 0.05f);
            if (candidates[1].Distance - candidates[0].Distance <= ambiguity)
            {
                warnings.Add(
                    $"Attachment {meshName}#{componentIndex} is almost equally close to " +
                    $"{candidates[0].Capsule.BoneName} and " +
                    $"{candidates[1].Capsule.BoneName}; the first deterministic choice " +
                    "requires explicit confirmation.");
            }
        }
        return candidates[0].Capsule;
    }

    private static void WriteInfluences(
        IReadOnlyList<PackedInfluence> influences,
        out ImportedJointIndices joints,
        out Vector4 weights)
    {
        if (influences.Count is < 1 or > 4)
            throw new InvalidDataException("Generated influence count must be between one and four.");
        ushort[] indices = new ushort[4];
        float[] values = new float[4];
        for (int index = 0; index < influences.Count; index++)
        {
            indices[index] = influences[index].Joint;
            values[index] = influences[index].Weight;
        }
        joints = new ImportedJointIndices(
            indices[0], indices[1], indices[2], indices[3]);
        weights = new Vector4(values[0], values[1], values[2], values[3]);
    }

    private static void ValidatePackedWeights(
        string meshName,
        IReadOnlyList<ImportedJointIndices> joints,
        IReadOnlyList<Vector4> weights,
        ImportedSkeleton skeleton)
    {
        if (joints.Count != weights.Count)
            throw new InvalidDataException($"Generated skin arrays differ for mesh {meshName}.");
        for (int vertex = 0; vertex < weights.Count; vertex++)
        {
            Vector4 value = weights[vertex];
            if (!IsFinite(value) || value.X < 0 || value.Y < 0 ||
                value.Z < 0 || value.W < 0)
            {
                throw new InvalidDataException(
                    $"Generated weights for {meshName} vertex {vertex} are invalid.");
            }
            float total = value.X + value.Y + value.Z + value.W;
            if (!float.IsFinite(total) || MathF.Abs(total - 1) > 0.0001f)
            {
                throw new InvalidDataException(
                    $"Generated weights for {meshName} vertex {vertex} are not normalized.");
            }
            ImportedJointIndices indices = joints[vertex];
            if ((value.X > WeightEpsilon && indices.X >= skeleton.JointNames.Count) ||
                (value.Y > WeightEpsilon && indices.Y >= skeleton.JointNames.Count) ||
                (value.Z > WeightEpsilon && indices.Z >= skeleton.JointNames.Count) ||
                (value.W > WeightEpsilon && indices.W >= skeleton.JointNames.Count))
            {
                throw new InvalidDataException(
                    $"Generated weights for {meshName} vertex {vertex} reference an invalid joint.");
            }
        }
    }

    private static void ValidateLowerBodyWallWeights(
        IReadOnlyList<Vector3[]> positionsByMesh,
        IReadOnlyList<bool[]> smoothByMesh,
        IReadOnlyList<ImportedJointIndices[]> jointsByMesh,
        IReadOnlyList<Vector4[]> weightsByMesh,
        ImportedSkeleton skeleton,
        SideCalibration calibration)
    {
        for (int meshIndex = 0; meshIndex < positionsByMesh.Count; meshIndex++)
        {
            for (int vertexIndex = 0;
                 vertexIndex < positionsByMesh[meshIndex].Length;
                 vertexIndex++)
            {
                if (!smoothByMesh[meshIndex][vertexIndex])
                    continue;
                BodySide side = ClassifyLowerBodyWallSide(
                    positionsByMesh[meshIndex][vertexIndex],
                    calibration);
                if (side == BodySide.Center)
                    continue;

                ImportedJointIndices joints = jointsByMesh[meshIndex][vertexIndex];
                Vector4 weights = weightsByMesh[meshIndex][vertexIndex];
                ushort[] indices = [joints.X, joints.Y, joints.Z, joints.W];
                for (int influence = 0; influence < indices.Length; influence++)
                {
                    float weight = VectorComponent(weights, influence);
                    if (weight <= WeightEpsilon)
                        continue;
                    int jointIndex = indices[influence];
                    if ((uint)jointIndex >= (uint)skeleton.JointNames.Count)
                        continue;
                    BodySide boneSide = ClassifyBoneSide(
                        skeleton.JointNames[jointIndex]);
                    if (IsOpposite(side, boneSide))
                    {
                        throw new InvalidDataException(
                            $"Sagittal lower-body wall validation found an " +
                            $"opposite-side influence on mesh {meshIndex}, vertex " +
                            $"{vertexIndex}: {skeleton.JointNames[jointIndex]}=" +
                            $"{weight:G6}.");
                    }
                }
            }
        }
    }

    private static void ValidateShoulderWallWeights(
        IReadOnlyList<Vector3[]> positionsByMesh,
        IReadOnlyList<bool[]> smoothByMesh,
        IReadOnlyList<ImportedJointIndices[]> jointsByMesh,
        IReadOnlyList<Vector4[]> weightsByMesh,
        ImportedSkeleton skeleton,
        SeparationPlanePreparation preparation)
    {
        for (int meshIndex = 0; meshIndex < positionsByMesh.Count; meshIndex++)
        {
            for (int vertexIndex = 0;
                 vertexIndex < positionsByMesh[meshIndex].Length;
                 vertexIndex++)
            {
                if (!smoothByMesh[meshIndex][vertexIndex])
                    continue;
                ShoulderPlaneClassification classification =
                    ClassifyShoulderPlanes(
                        positionsByMesh[meshIndex][vertexIndex],
                        preparation);
                if (!classification.LeftActive &&
                    !classification.RightActive)
                {
                    continue;
                }

                ImportedJointIndices joints = jointsByMesh[meshIndex][vertexIndex];
                Vector4 weights = weightsByMesh[meshIndex][vertexIndex];
                ushort[] indices = [joints.X, joints.Y, joints.Z, joints.W];
                for (int influence = 0; influence < indices.Length; influence++)
                {
                    float weight = VectorComponent(weights, influence);
                    if (weight <= WeightEpsilon)
                        continue;
                    int jointIndex = indices[influence];
                    bool compatible = IsShoulderCapsuleCompatible(
                        jointIndex,
                        classification,
                        preparation);
                    if (compatible)
                        continue;
                    string jointName = (uint)jointIndex <
                        (uint)skeleton.JointNames.Count
                            ? skeleton.JointNames[jointIndex]
                            : $"joint#{jointIndex}";
                    throw new InvalidDataException(
                        $"Shoulder-wall validation found a crossing influence " +
                        $"on mesh {meshIndex}, vertex {vertexIndex}: " +
                        $"{jointName}={weight:G6}.");
                }
            }
        }
    }

    private static void ValidateHeadPlaneWeights(
        IReadOnlyList<Vector3[]> positionsByMesh,
        IReadOnlyList<bool[]> smoothByMesh,
        IReadOnlyList<ImportedJointIndices[]> jointsByMesh,
        IReadOnlyList<Vector4[]> weightsByMesh,
        SeparationPlanePreparation preparation,
        GeneratedSkinningRegionAnalysis analysis)
    {
        if (!preparation.ByKind.TryGetValue(
                GeneratedSkinningSeparationPlaneKind.Head,
                out GeneratedSkinningSeparationPlaneResolution? plane) ||
            !plane.IsEnabled ||
            !plane.IsAvailable)
        {
            return;
        }
        GeneratedSkinningRegionResolution head = analysis.Regions.Single(
            value => value.Region == GeneratedSkinningSemanticRegion.Head);
        if (!head.IsApplied || head.AnchorSkeletonJointIndex < 0)
        {
            throw new InvalidDataException(
                "The enabled Head plane did not produce a validated rigid Head " +
                "region; adjust or disable the plane before writing SMO. " +
                $"Status={head.Status}; diagnostics=" +
                string.Join(" | ", head.Warnings));
        }

        for (int meshIndex = 0; meshIndex < positionsByMesh.Count; meshIndex++)
        {
            for (int vertexIndex = 0;
                 vertexIndex < positionsByMesh[meshIndex].Length;
                 vertexIndex++)
            {
                if (!smoothByMesh[meshIndex][vertexIndex])
                    continue;
                bool headSide = GeneratedSkinningSeparationPlaneMath.SignedDistance(
                    positionsByMesh[meshIndex][vertexIndex],
                    plane) >= -PositionEpsilon;
                ImportedJointIndices joints = jointsByMesh[meshIndex][vertexIndex];
                Vector4 weights = weightsByMesh[meshIndex][vertexIndex];
                ushort[] indices = [joints.X, joints.Y, joints.Z, joints.W];
                if (headSide)
                {
                    if (indices[0] != head.AnchorSkeletonJointIndex ||
                        MathF.Abs(weights.X - 1) > WeightEpsilon ||
                        weights.Y > WeightEpsilon ||
                        weights.Z > WeightEpsilon ||
                        weights.W > WeightEpsilon)
                    {
                        throw new InvalidDataException(
                            $"Head-plane validation found a deforming Head-side " +
                            $"vertex on mesh {meshIndex}, vertex {vertexIndex}.");
                    }
                    continue;
                }

                for (int influence = 0; influence < indices.Length; influence++)
                {
                    if (VectorComponent(weights, influence) > WeightEpsilon &&
                        preparation.HeadSkeletonJointIndices.Contains(
                            indices[influence]))
                    {
                        throw new InvalidDataException(
                            $"Head-plane validation found a Head-branch influence " +
                            $"below the plane on mesh {meshIndex}, vertex " +
                            $"{vertexIndex}.");
                    }
                }
            }
        }
    }

    private static GeneratedSkinningAlignment BuildAlignment(
        RobustBounds target,
        RobustBounds donor)
    {
        ValidateVerticalAxis(target, "target main component");
        ValidateVerticalAxis(donor, "donor main component");
        float scale = target.Size.Y / donor.Size.Y;
        if (!float.IsFinite(scale) || scale < MinimumAlignmentScale ||
            scale > MaximumAlignmentScale)
        {
            throw new InvalidDataException(
                $"Uniform fit scale {scale:G9} is outside the conservative range.");
        }

        ValidateAspectAgreement(target.Size, donor.Size, scale);
        Vector3 translation = target.Center - donor.Center * scale;
        if (!IsFinite(translation))
            throw new InvalidDataException("Uniform fit produced a non-finite translation.");
        return new GeneratedSkinningAlignment(scale, translation);
    }

    private static GeneratedSkinningAlignment ValidateExplicitAlignment(
        ReplacementTransform donorAlignment)
    {
        ArgumentNullException.ThrowIfNull(donorAlignment);
        if (!float.IsFinite(donorAlignment.Scale) || donorAlignment.Scale <= 0 ||
            !IsFinite(donorAlignment.RotationDegrees) ||
            !IsFinite(donorAlignment.Translation))
        {
            throw new ArgumentException(
                "Generated-skinning donor alignment requires a finite positive " +
                "uniform scale and a finite translation.",
                nameof(donorAlignment));
        }

        Matrix4x4 matrix = donorAlignment.Matrix;
        if (!IsFinite(matrix) ||
            !Matrix4x4.Invert(matrix, out Matrix4x4 inverse) ||
            !IsFinite(inverse))
        {
            throw new ArgumentException(
                "Generated-skinning donor alignment must be finite and invertible.",
                nameof(donorAlignment));
        }
        return new GeneratedSkinningAlignment(
            donorAlignment.Scale,
            donorAlignment.Translation)
        {
            RotationDegrees = donorAlignment.RotationDegrees
        };
    }

    private static void ValidateVerticalAxis(RobustBounds bounds, string label)
    {
        Vector3 size = bounds.Size;
        float largest = MathF.Max(size.X, MathF.Max(size.Y, size.Z));
        if (!IsFinite(size) || size.Y <= PositionEpsilon || largest <= PositionEpsilon)
            throw new InvalidDataException($"{label} has no stable three-dimensional extent.");
        if (size.Y / largest < MinimumVerticalAxisRatio)
        {
            throw new InvalidDataException(
                $"{label} is not clearly Y-up; automatic character alignment is ambiguous.");
        }
    }

    private static void ValidateAspectAgreement(
        Vector3 targetSize,
        Vector3 donorSize,
        float scale)
    {
        CheckAxis(targetSize.X, donorSize.X * scale, "width");
        CheckAxis(targetSize.Z, donorSize.Z * scale, "depth");

        static void CheckAxis(float target, float alignedDonor, string axis)
        {
            if (target <= PositionEpsilon || alignedDonor <= PositionEpsilon)
                return;
            float ratio = MathF.Max(target, alignedDonor) /
                          MathF.Min(target, alignedDonor);
            if (!float.IsFinite(ratio) || ratio > MaximumAspectRatioDisagreement)
            {
                throw new InvalidDataException(
                    $"Target and donor {axis} differ too much for an unambiguous uniform fit.");
            }
        }
    }

    private static SideCalibration CalibrateSides(
        IReadOnlyList<TargetRigJoint> deformJoints,
        RobustBounds targetBounds,
        IReadOnlyList<Matrix4x4>? fittingWorldMatrices)
    {
        TargetRigJoint? lowerBodyWallAnchor = deformJoints
            .Where(joint => string.Equals(
                joint.Name,
                "Pelvis",
                StringComparison.Ordinal))
            .Concat(deformJoints.Where(joint => string.Equals(
                joint.Name,
                "Spine_01",
                StringComparison.Ordinal)))
            .FirstOrDefault();
        Vector3 lowerBodyWallOrigin = lowerBodyWallAnchor is null
            ? Vector3.Zero
            : Translation(GetFittingWorldMatrix(
                lowerBodyWallAnchor,
                fittingWorldMatrices));
        bool hasLowerBodyWall = lowerBodyWallAnchor is not null &&
                                IsFinite(lowerBodyWallOrigin);
        if (!hasLowerBodyWall)
        {
            throw new InvalidDataException(
                "Target skeleton has no finite Pelvis/Spine_01 anchor for the " +
                "mandatory sagittal lower-body wall.");
        }
        if (fittingWorldMatrices is null)
        {
            float[] bindLeft = deformJoints
                .Where(joint => ClassifyBoneSide(joint.Name) == BodySide.Left)
                .Select(joint => Translation(joint.BindWorldMatrix).X)
                .ToArray();
            float[] bindRight = deformJoints
                .Where(joint => ClassifyBoneSide(joint.Name) == BodySide.Right)
                .Select(joint => Translation(joint.BindWorldMatrix).X)
                .ToArray();
            if (bindLeft.Length == 0 || bindRight.Length == 0)
            {
                throw new InvalidDataException(
                    "Target skeleton has no unambiguous named left/right " +
                    "deform-joint pairs.");
            }
            float bindLeftAverage = bindLeft.Average();
            float bindRightAverage = bindRight.Average();
            float bindSeparation = bindLeftAverage - bindRightAverage;
            float bindMinimumSeparation = MathF.Max(
                targetBounds.Size.X * 0.05f,
                targetBounds.Size.Y * 0.01f);
            if (!float.IsFinite(bindSeparation) ||
                MathF.Abs(bindSeparation) <= bindMinimumSeparation)
            {
                throw new InvalidDataException(
                    "Target left/right bone positions do not define a stable " +
                    "character side axis.");
            }
            return new SideCalibration(
                targetBounds.Center.X,
                MathF.Sign(bindSeparation),
                Vector3.Zero,
                Vector3.Zero,
                bindMinimumSeparation * 0.5f,
                UseVectorAxis: false)
            {
                LowerBodyWallOrigin = lowerBodyWallOrigin,
                HasLowerBodyWall = hasLowerBodyWall
            };
        }

        Vector3[] left = deformJoints
            .Where(joint => ClassifyBoneSide(joint.Name) == BodySide.Left)
            .Select(joint => Translation(GetFittingWorldMatrix(
                joint,
                fittingWorldMatrices)))
            .ToArray();
        Vector3[] right = deformJoints
            .Where(joint => ClassifyBoneSide(joint.Name) == BodySide.Right)
            .Select(joint => Translation(GetFittingWorldMatrix(
                joint,
                fittingWorldMatrices)))
            .ToArray();
        if (left.Length == 0 || right.Length == 0)
        {
            throw new InvalidDataException(
                "Target skeleton has no unambiguous named left/right deform-joint pairs.");
        }
        Vector3 leftAverage = Average(left);
        Vector3 rightAverage = Average(right);
        Vector3 separation = leftAverage - rightAverage;
        float separationLength = separation.Length();
        float minimumSeparation = MathF.Max(
            targetBounds.Size.X * 0.05f,
            targetBounds.Size.Y * 0.01f);
        if (!IsFinite(separation) || !float.IsFinite(separationLength) ||
            separationLength <= minimumSeparation)
        {
            throw new InvalidDataException(
                "Target left/right bone positions do not define a stable character side axis.");
        }
        return new SideCalibration(
            0,
            0,
            (leftAverage + rightAverage) * 0.5f,
            separation / separationLength,
            minimumSeparation * 0.5f,
            UseVectorAxis: true)
        {
            LowerBodyWallOrigin = lowerBodyWallOrigin,
            HasLowerBodyWall = hasLowerBodyWall
        };

        static Vector3 Average(IReadOnlyList<Vector3> values)
        {
            Vector3 sum = Vector3.Zero;
            foreach (Vector3 value in values)
                sum += value;
            return sum / values.Count;
        }
    }

    private static void ValidateTargetBodyOverlap(
        IReadOnlyList<TargetRigJoint> deformJoints,
        RobustBounds targetBounds,
        IReadOnlyList<Matrix4x4>? fittingWorldMatrices)
    {
        TargetRigJoint[] centralBodyJoints = deformJoints
            .Where(joint => IsSafeAutomaticWeightBone(joint.Name) &&
                            ClassifyBoneSide(joint.Name) == BodySide.Center)
            .ToArray();
        if (centralBodyJoints.Length < 2)
        {
            throw new InvalidDataException(
                "Target rig has too few safe central deform joints to corroborate " +
                "the selected target body surface.");
        }

        float height = targetBounds.Size.Y;
        Vector3 margin = new(
            MathF.Max(targetBounds.Size.X * 0.25f, height * 0.04f),
            height * 0.12f,
            MathF.Max(targetBounds.Size.Z * 0.35f, height * 0.04f));
        Vector3 lower = targetBounds.Lower - margin;
        Vector3 upper = targetBounds.Upper + margin;
        int inside = centralBodyJoints.Count(joint =>
        {
            Vector3 position = Translation(GetFittingWorldMatrix(
                joint,
                fittingWorldMatrices));
            return position.X >= lower.X && position.X <= upper.X &&
                   position.Y >= lower.Y && position.Y <= upper.Y &&
                   position.Z >= lower.Z && position.Z <= upper.Z;
        });
        int required = Math.Max(2, (int)MathF.Ceiling(centralBodyJoints.Length * 0.6f));
        if (inside < required)
        {
            throw new InvalidDataException(
                "The largest target surface does not overlap enough safe central " +
                "deform joints to identify it as the body without guessing.");
        }
    }

    private static BodySide ClassifyBoneSide(string name)
    {
        string trimmed = name.Trim();
        string lower = trimmed.ToLowerInvariant();
        if (lower.StartsWith("left", StringComparison.Ordinal) ||
            HasSidePrefix(trimmed, 'L') || HasSideSuffix(trimmed, 'L') ||
            lower.StartsWith("larm", StringComparison.Ordinal) ||
            lower.StartsWith("lhand", StringComparison.Ordinal) ||
            lower.StartsWith("lleg", StringComparison.Ordinal) ||
            lower.StartsWith("lfoot", StringComparison.Ordinal) ||
            lower.StartsWith("lthigh", StringComparison.Ordinal))
        {
            return BodySide.Left;
        }
        if (lower.StartsWith("right", StringComparison.Ordinal) ||
            HasSidePrefix(trimmed, 'R') || HasSideSuffix(trimmed, 'R') ||
            lower.StartsWith("rarm", StringComparison.Ordinal) ||
            lower.StartsWith("rhand", StringComparison.Ordinal) ||
            lower.StartsWith("rleg", StringComparison.Ordinal) ||
            lower.StartsWith("rfoot", StringComparison.Ordinal) ||
            lower.StartsWith("rthigh", StringComparison.Ordinal))
        {
            return BodySide.Right;
        }
        return BodySide.Center;

        static bool HasSidePrefix(string value, char side) =>
            value.Length >= 2 && char.ToUpperInvariant(value[0]) == side &&
            (value[1] is '_' or '-' or '.' || char.IsUpper(value[1]));

        static bool HasSideSuffix(string value, char side) =>
            value.Length >= 2 && char.ToUpperInvariant(value[^1]) == side &&
            value[^2] is '_' or '-' or '.';
    }

    private static BodySide ClassifyPositionSide(
        Vector3 position,
        SideCalibration calibration)
    {
        float signed = calibration.UseVectorAxis
            ? Vector3.Dot(
                position - calibration.Center,
                calibration.LeftAxis)
            : (position.X - calibration.CenterX) * calibration.LeftDirection;
        if (signed > calibration.DeadZone)
            return BodySide.Left;
        if (signed < -calibration.DeadZone)
            return BodySide.Right;
        return BodySide.Center;
    }

    private static BodySide ClassifyLowerBodyWallSide(
        Vector3 position,
        SideCalibration calibration)
    {
        if (!calibration.HasLowerBodyWall ||
            position.Y > calibration.LowerBodyWallOrigin.Y + PositionEpsilon)
        {
            return BodySide.Center;
        }

        float signed = calibration.UseVectorAxis
            ? Vector3.Dot(
                position - calibration.LowerBodyWallOrigin,
                calibration.LeftAxis)
            : (position.X - calibration.LowerBodyWallOrigin.X) *
              calibration.LeftDirection;
        if (signed > PositionEpsilon)
            return BodySide.Left;
        if (signed < -PositionEpsilon)
            return BodySide.Right;
        return BodySide.Center;
    }

    private static bool IsOpposite(BodySide position, BodySide bone) =>
        position == BodySide.Left && bone == BodySide.Right ||
        position == BodySide.Right && bone == BodySide.Left;

    private static bool IsSafeAutomaticWeightBone(string name)
    {
        string value = name.Trim().ToLowerInvariant();
        return !value.StartsWith("c-", StringComparison.Ordinal) &&
               !value.StartsWith("cc-", StringComparison.Ordinal) &&
               !value.StartsWith("up-", StringComparison.Ordinal) &&
               !value.Contains("control", StringComparison.Ordinal) &&
               !value.Contains("helper", StringComparison.Ordinal) &&
               !value.Contains("tracker", StringComparison.Ordinal) &&
               !value.Contains("attach", StringComparison.Ordinal) &&
               !value.Contains("socket", StringComparison.Ordinal) &&
               !value.Contains("neutral", StringComparison.Ordinal) &&
               !value.Contains("submaster", StringComparison.Ordinal);
    }

    private static SceneTopology BuildTopology(
        IReadOnlyList<GeometrySource> meshes,
        string label,
        CancellationToken cancellationToken = default)
    {
        cancellationToken.ThrowIfCancellationRequested();
        var offsets = new int[meshes.Count];
        int totalVertexCount = 0;
        for (int expectedIndex = 0; expectedIndex < meshes.Count; expectedIndex++)
        {
            GeometrySource mesh = meshes[expectedIndex];
            if (mesh.MeshIndex != expectedIndex)
            {
                throw new InvalidDataException(
                    $"{label} geometry sources do not have contiguous mesh indices.");
            }
            offsets[mesh.MeshIndex] = totalVertexCount;
            totalVertexCount = checked(totalVertexCount + mesh.Positions.Length);
        }
        var vertices = new GeometryVertex[totalVertexCount];
        foreach (GeometrySource mesh in meshes)
        {
            for (int vertex = 0; vertex < mesh.Positions.Length; vertex++)
            {
                if ((vertex & 0xFFF) == 0)
                    cancellationToken.ThrowIfCancellationRequested();
                vertices[offsets[mesh.MeshIndex] + vertex] =
                    new GeometryVertex(mesh.MeshIndex, vertex);
            }
        }

        var union = new UnionFind(totalVertexCount);
        var referenced = new bool[totalVertexCount];
        var triangleData = new List<(int First, int Second, int Third, float Area)>();
        List<uint>[] renderableTriangleIndices = meshes
            .Select(_ => new List<uint>())
            .ToArray();
        int removedDegenerateTriangleCount = 0;
        foreach (GeometrySource mesh in meshes)
        {
            if (mesh.Positions.Any(position => !IsFinite(position)))
                throw new InvalidDataException($"{label} mesh {mesh.Name} has non-finite positions.");
            if (mesh.TriangleIndices.Length % 3 != 0)
            {
                throw new InvalidDataException(
                    $"{label} mesh {mesh.Name} has an incomplete triangle index list.");
            }
            if (mesh.Positions.Length == 0)
            {
                if (mesh.TriangleIndices.Length != 0)
                {
                    throw new InvalidDataException(
                        $"{label} mesh {mesh.Name} has indices but no vertices.");
                }
                continue;
            }
            if (mesh.TriangleIndices.Length == 0)
            {
                continue;
            }

            for (int index = 0; index < mesh.TriangleIndices.Length; index += 3)
            {
                if ((index & 0x3FFF) == 0)
                    cancellationToken.ThrowIfCancellationRequested();
                int first = CheckedIndex(mesh, mesh.TriangleIndices[index], label);
                int second = CheckedIndex(mesh, mesh.TriangleIndices[index + 1], label);
                int third = CheckedIndex(mesh, mesh.TriangleIndices[index + 2], label);
                Vector3 cross = Vector3.Cross(
                    mesh.Positions[second] - mesh.Positions[first],
                    mesh.Positions[third] - mesh.Positions[first]);
                float area = cross.Length() * 0.5f;
                if (!float.IsFinite(area))
                {
                    throw new InvalidDataException(
                        $"{label} mesh {mesh.Name} has a triangle with non-finite " +
                        "surface area.");
                }
                if (first == second || second == third || first == third ||
                    area <= PositionEpsilon * PositionEpsilon)
                {
                    removedDegenerateTriangleCount++;
                    continue;
                }

                renderableTriangleIndices[mesh.MeshIndex].Add(mesh.TriangleIndices[index]);
                renderableTriangleIndices[mesh.MeshIndex].Add(mesh.TriangleIndices[index + 1]);
                renderableTriangleIndices[mesh.MeshIndex].Add(mesh.TriangleIndices[index + 2]);

                int firstGlobal = offsets[mesh.MeshIndex] + first;
                int secondGlobal = offsets[mesh.MeshIndex] + second;
                int thirdGlobal = offsets[mesh.MeshIndex] + third;
                referenced[firstGlobal] = referenced[secondGlobal] =
                    referenced[thirdGlobal] = true;
                union.Union(firstGlobal, secondGlobal);
                union.Union(firstGlobal, thirdGlobal);
                triangleData.Add((firstGlobal, secondGlobal, thirdGlobal, area));
            }
        }

        // Attribute seams and separate glTF primitives commonly duplicate the
        // exact same geometric edge. Merge raw components only when they share
        // at least two exactly equal positions. One coincident point is not
        // enough: it may merely be contact between clothing and the body. A
        // proximity epsilon is also deliberately avoided because it could join
        // wings or another surface which only lies close to the body.
        var verticesByPosition = new Dictionary<Vector3, List<int>>();
        for (int globalVertex = 0; globalVertex < vertices.Length; globalVertex++)
        {
            if ((globalVertex & 0xFFF) == 0)
                cancellationToken.ThrowIfCancellationRequested();
            if (!referenced[globalVertex])
                continue;
            GeometryVertex vertex = vertices[globalVertex];
            Vector3 position = meshes[vertex.MeshIndex].Positions[vertex.VertexIndex];
            if (!verticesByPosition.TryGetValue(position, out List<int>? equalVertices))
            {
                equalVertices = [];
                verticesByPosition.Add(position, equalVertices);
            }
            equalVertices.Add(globalVertex);
        }
        var sharedPositionCount = new Dictionary<(int First, int Second), int>();
        foreach (List<int> equalVertices in verticesByPosition.Values)
        {
            cancellationToken.ThrowIfCancellationRequested();
            int[] roots = equalVertices.Select(union.Find).Distinct().Order().ToArray();
            if (roots.Length > 32)
            {
                throw new InvalidDataException(
                    $"{label} has more than 32 overlapping raw components at one " +
                    "position; seam connectivity is ambiguous.");
            }
            for (int first = 0; first < roots.Length; first++)
            {
                for (int second = first + 1; second < roots.Length; second++)
                {
                    (int First, int Second) key = (roots[first], roots[second]);
                    sharedPositionCount[key] = sharedPositionCount.GetValueOrDefault(key) + 1;
                }
            }
        }
        foreach (KeyValuePair<(int First, int Second), int> pair in sharedPositionCount)
        {
            if (pair.Value >= 2)
                union.Union(pair.Key.First, pair.Key.Second);
        }

        Dictionary<int, List<int>> verticesByRoot = Enumerable.Range(0, vertices.Length)
            .Where(vertex => referenced[vertex])
            .GroupBy(union.Find)
            .ToDictionary(group => group.Key, group => group.ToList());
        Dictionary<int, List<(int First, int Second, int Third, float Area)>> trianglesByRoot =
            triangleData.GroupBy(triangle => union.Find(triangle.First))
                .ToDictionary(group => group.Key, group => group.ToList());
        var components = new List<GeometryComponent>();
        int componentIndex = 0;
        foreach (List<int> globalVertices in verticesByRoot.Values
                     .OrderBy(group => group.Min()))
        {
            int root = union.Find(globalVertices[0]);
            List<(int First, int Second, int Third, float Area)> triangles =
                trianglesByRoot.GetValueOrDefault(root) ?? [];
            double area = triangles.Sum(triangle => (double)triangle.Area);
            if (triangles.Count == 0 || !double.IsFinite(area) ||
                area <= 0 || area > float.MaxValue)
            {
                throw new InvalidDataException(
                    $"{label} component {componentIndex} has no finite non-degenerate surface.");
            }
            GeometryVertex[] componentVertices = globalVertices
                .Select(globalVertex => vertices[globalVertex])
                .ToArray();
            Vector3[] uniquePositions = componentVertices
                .Select(vertex => meshes[vertex.MeshIndex].Positions[vertex.VertexIndex])
                .Distinct()
                .ToArray();
            components.Add(new GeometryComponent(
                componentIndex,
                componentVertices,
                triangles.Count,
                (float)area,
                Average(uniquePositions)));
            componentIndex++;
        }
        if (components.Count == 0)
            throw new InvalidDataException($"{label} has no non-degenerate connected surface.");
        GeometryVertex[] unreferencedVertices = Enumerable.Range(0, vertices.Length)
            .Where(globalVertex => !referenced[globalVertex])
            .Select(globalVertex => vertices[globalVertex])
            .ToArray();
        IReadOnlyList<uint>[] readOnlyRenderableIndices = renderableTriangleIndices
            .Select(indices => (IReadOnlyList<uint>)indices.AsReadOnly())
            .ToArray();
        return new SceneTopology(
            new ReadOnlyCollection<GeometryComponent>(components),
            Array.AsReadOnly(unreferencedVertices),
            Array.AsReadOnly(readOnlyRenderableIndices),
            removedDegenerateTriangleCount);
    }

    private static GeometryComponent SelectUnambiguousMainComponent(
        IReadOnlyList<GeometryComponent> components,
        IReadOnlyList<GeometrySource> meshes,
        string label)
    {
        GeometryComponent[] ordered = components
            .OrderByDescending(component => component.Area)
            .ThenByDescending(component => ComponentExtent(component, meshes).Y)
            .ThenByDescending(component => component.TriangleCount)
            .ThenByDescending(component => component.Vertices.Length)
            .ThenBy(component => component.ComponentIndex)
            .ToArray();
        GeometryComponent main = ordered[0];
        double totalArea = ordered.Sum(component => (double)component.Area);
        if (main.TriangleCount < 4 ||
            main.Area < totalArea * MinimumMainAreaCoverage)
        {
            throw new InvalidDataException(
                $"{label} has no dominant connected surface by geometric area.");
        }
        if (ordered.Length > 1 &&
            ordered[1].Area >= main.Area * MainComponentAmbiguityRatio)
        {
            throw new InvalidDataException(
                $"{label} has two almost equally large disconnected surfaces " +
                $"({DescribeComponent(main, meshes)}#{main.ComponentIndex} and " +
                $"{DescribeComponent(ordered[1], meshes)}#{ordered[1].ComponentIndex}); " +
                "automatic body selection is ambiguous.");
        }
        float largestVerticalExtent = components.Max(component =>
            ComponentExtent(component, meshes).Y);
        float mainVerticalExtent = ComponentExtent(main, meshes).Y;
        if (!float.IsFinite(largestVerticalExtent) || largestVerticalExtent <= PositionEpsilon ||
            mainVerticalExtent < largestVerticalExtent * 0.8f)
        {
            throw new InvalidDataException(
                $"{label} largest-area surface is not also a dominant upright surface; " +
                "automatic body selection is ambiguous.");
        }
        return main;
    }

    private static GeometryComponent[] ValidateExplicitBodySelection(
        SmoDocument target,
        ImportedScene donor,
        SceneTopology topology,
        IReadOnlyList<GeometrySource> donorSources,
        TargetRigBodySelection selection,
        GeneratedSkinningAlignment alignment)
    {
        string targetFingerprint = TargetRigDefinition.ComputeSourceFingerprint(target);
        if (string.IsNullOrWhiteSpace(selection.TargetRigFingerprint) ||
            !string.Equals(
                selection.TargetRigFingerprint,
                targetFingerprint,
                StringComparison.Ordinal))
        {
            throw new InvalidDataException(
                "Explicit donor body selection belongs to a different target rig.");
        }
        string donorFingerprint =
            TargetRigAutomaticPoseFitter.ComputeDonorGeometryFingerprint(donor);
        if (string.IsNullOrWhiteSpace(selection.DonorGeometryFingerprint) ||
            !string.Equals(
                selection.DonorGeometryFingerprint,
                donorFingerprint,
                StringComparison.Ordinal))
        {
            throw new InvalidDataException(
                "Explicit donor body selection belongs to a different donor scene or revision.");
        }
        if (selection.DonorAlignment is null ||
            selection.DonorAlignment.Scale != alignment.Scale ||
            selection.DonorAlignment.RotationDegrees != alignment.RotationDegrees ||
            selection.DonorAlignment.Translation != alignment.Translation ||
            !IsFinite(selection.DonorAlignment.Matrix))
        {
            throw new InvalidDataException(
                "Explicit donor body selection belongs to a different donor alignment.");
        }
        if (selection.Components is null ||
            selection.Components.Any(component => component is null) ||
            selection.Components.Count < 1 ||
            selection.Components.Count > topology.Components.Count ||
            selection.TotalComponentCount != topology.Components.Count ||
            selection.ExcludedComponentCount !=
                topology.Components.Count - selection.Components.Count)
        {
            throw new InvalidDataException(
                "Explicit donor body selection component totals are inconsistent " +
                "with the current donor topology.");
        }
        int wholeCount = selection.Components.Count(component =>
            component.Role == TargetRigBodyComponentRole.WholeBody);
        int lowerCount = selection.Components.Count(component =>
            component.Role == TargetRigBodyComponentRole.LowerBody);
        int upperCount = selection.Components.Count(component =>
            component.Role == TargetRigBodyComponentRole.TorsoAndArms);
        int supplementalCount = selection.Components.Count(component =>
            component.Role == TargetRigBodyComponentRole.SupplementalBody);
        bool validRoles = supplementalCount ==
                              selection.Components.Count -
                              wholeCount - lowerCount - upperCount &&
                          ((wholeCount == 1 && lowerCount == 0 && upperCount == 0) ||
                           (wholeCount == 0 && lowerCount == 1 && upperCount == 1));
        if (!validRoles)
        {
            throw new InvalidDataException(
                "Explicit donor body selection roles must contain one whole-body " +
                "anchor or one lower/upper anchor pair; any remaining components " +
                "must be supplemental body surfaces.");
        }

        Dictionary<int, GeometryComponent> topologyByIndex = topology.Components
            .ToDictionary(component => component.ComponentIndex);
        var selectedIndices = new HashSet<int>();
        var selectedVertices = new HashSet<GeometryVertex>();
        var validated = new List<GeometryComponent>(selection.Components.Count);
        foreach (TargetRigSelectedBodyComponent descriptor in selection.Components)
        {
            if (!selectedIndices.Add(descriptor.ComponentIndex) ||
                !topologyByIndex.TryGetValue(
                    descriptor.ComponentIndex,
                    out GeometryComponent? component))
            {
                throw new InvalidDataException(
                    $"Explicit donor body component #{descriptor.ComponentIndex} " +
                    "is duplicate or absent from the current donor topology.");
            }
            Vector3[] uniqueAlignedPositions = GetComponentPositions(
                    donorSources,
                    component)
                .Select(position => ApplyAlignment(position, alignment))
                .Distinct()
                .ToArray();
            if (uniqueAlignedPositions.Length == 0 ||
                uniqueAlignedPositions.Any(position => !IsFinite(position)))
            {
                throw new InvalidDataException(
                    $"Explicit donor body component #{component.ComponentIndex} " +
                    "has no finite aligned positions.");
            }
            Vector3 alignedMinimum = uniqueAlignedPositions[0];
            Vector3 alignedMaximum = uniqueAlignedPositions[0];
            foreach (Vector3 position in uniqueAlignedPositions.AsSpan(1))
            {
                alignedMinimum = Vector3.Min(alignedMinimum, position);
                alignedMaximum = Vector3.Max(alignedMaximum, position);
            }
            float expectedArea = component.Area * alignment.Scale * alignment.Scale;
            if (descriptor.UniquePositionCount != uniqueAlignedPositions.Length ||
                descriptor.TriangleCount != component.TriangleCount ||
                !float.IsFinite(descriptor.SurfaceArea) || descriptor.SurfaceArea <= 0 ||
                !ApproximatelyEqual(descriptor.SurfaceArea, expectedArea) ||
                !IsFinite(descriptor.AlignedMinimum) ||
                !IsFinite(descriptor.AlignedMaximum) ||
                !ApproximatelyEqual(descriptor.AlignedMinimum, alignedMinimum) ||
                !ApproximatelyEqual(descriptor.AlignedMaximum, alignedMaximum))
            {
                throw new InvalidDataException(
                    $"Explicit donor body component #{component.ComponentIndex} " +
                    "counts, surface area, or aligned bounds no longer match the donor.");
            }

            if (descriptor.VerticesByMesh is null ||
                descriptor.VerticesByMesh.Any(membership => membership is null) ||
                descriptor.VerticesByMesh.Count == 0)
            {
                throw new InvalidDataException(
                    $"Explicit donor body component #{component.ComponentIndex} " +
                    "has no original donor vertex membership.");
            }
            var actualVertices = new HashSet<GeometryVertex>();
            var membershipMeshes = new HashSet<int>();
            foreach (TargetRigBodyVertexMembership membership in
                     descriptor.VerticesByMesh)
            {
                if ((uint)membership.MeshIndex >= (uint)donor.Meshes.Count ||
                    !membershipMeshes.Add(membership.MeshIndex) ||
                    !string.Equals(
                        membership.MeshName,
                        donor.Meshes[membership.MeshIndex].Name,
                        StringComparison.Ordinal) ||
                    membership.VertexIndices is null ||
                    membership.VertexIndices.Count == 0)
                {
                    throw new InvalidDataException(
                        $"Explicit donor body component #{component.ComponentIndex} " +
                        "contains invalid or duplicate mesh membership.");
                }
                var uniqueVertexIndices = new HashSet<int>();
                foreach (int vertexIndex in membership.VertexIndices)
                {
                    if ((uint)vertexIndex >=
                            (uint)donor.Meshes[membership.MeshIndex].Positions.Length ||
                        !uniqueVertexIndices.Add(vertexIndex) ||
                        !actualVertices.Add(new GeometryVertex(
                            membership.MeshIndex,
                            vertexIndex)))
                    {
                        throw new InvalidDataException(
                            $"Explicit donor body component #{component.ComponentIndex} " +
                            "contains an invalid, duplicate, or overlapping vertex.");
                    }
                }
            }
            HashSet<GeometryVertex> expectedVertices = component.Vertices.ToHashSet();
            if (!actualVertices.SetEquals(expectedVertices))
            {
                throw new InvalidDataException(
                    $"Explicit donor body component #{component.ComponentIndex} " +
                    "vertex membership does not exactly match its connected surface.");
            }
            if (actualVertices.Any(vertex => !selectedVertices.Add(vertex)))
            {
                throw new InvalidDataException(
                    "Explicit donor body components overlap in original donor vertices.");
            }
            validated.Add(component);
        }
        return validated
            .OrderBy(component => component.ComponentIndex)
            .ToArray();
    }

    private static IReadOnlyDictionary<int, ManualComponentAssignment>
        ValidateComponentOverrides(
            SmoDocument target,
            ImportedScene donor,
            SceneTopology topology,
            IReadOnlyList<GeometrySource> donorSources,
            IReadOnlySet<int> bodyComponentIndices,
            TargetSkeletonLayout targetSkeleton,
            GeneratedSkinningComponentOverrides? overrides)
    {
        if (overrides is null)
        {
            return new ReadOnlyDictionary<int, ManualComponentAssignment>(
                new Dictionary<int, ManualComponentAssignment>());
        }

        string targetFingerprint = TargetRigDefinition.ComputeSourceFingerprint(target);
        if (string.IsNullOrWhiteSpace(overrides.TargetRigFingerprint) ||
            !string.Equals(
                overrides.TargetRigFingerprint,
                targetFingerprint,
                StringComparison.Ordinal))
        {
            throw new InvalidDataException(
                "Component overrides belong to a different target rig or revision.");
        }
        string donorFingerprint =
            TargetRigAutomaticPoseFitter.ComputeDonorGeometryFingerprint(donor);
        if (string.IsNullOrWhiteSpace(overrides.DonorGeometryFingerprint) ||
            !string.Equals(
                overrides.DonorGeometryFingerprint,
                donorFingerprint,
                StringComparison.Ordinal))
        {
            throw new InvalidDataException(
                "Component overrides belong to a different donor scene or revision.");
        }
        if (overrides.TotalComponentCount != topology.Components.Count ||
            overrides.Components is null ||
            overrides.Components.Any(component => component is null))
        {
            throw new InvalidDataException(
                "Component override totals are inconsistent with the current donor topology.");
        }

        Dictionary<int, GeometryComponent> topologyByIndex = topology.Components
            .ToDictionary(component => component.ComponentIndex);
        var result = new Dictionary<int, ManualComponentAssignment>();
        foreach (GeneratedSkinningComponentOverride descriptor in overrides.Components)
        {
            if (!topologyByIndex.TryGetValue(
                    descriptor.ComponentIndex,
                    out GeometryComponent? component) ||
                !result.TryAdd(
                    descriptor.ComponentIndex,
                    ResolveManualTarget(descriptor.Target, targetSkeleton)))
            {
                throw new InvalidDataException(
                    $"Component override #{descriptor.ComponentIndex} is duplicate or " +
                    "absent from the current donor topology.");
            }
            if (bodyComponentIndices.Contains(component.ComponentIndex))
            {
                throw new InvalidDataException(
                    $"Component override #{component.ComponentIndex} targets a smooth body " +
                    "surface. Manual one-hot assignments are allowed only for detached " +
                    "components.");
            }
            ValidateExactComponentMembership(
                donor,
                component,
                descriptor.VerticesByMesh,
                $"Component override #{component.ComponentIndex}");
        }
        return new ReadOnlyDictionary<int, ManualComponentAssignment>(result);
    }

    private static ManualComponentAssignment ResolveManualTarget(
        GeneratedSkinningComponentAttachmentTarget target,
        TargetSkeletonLayout targetSkeleton)
    {
        string exactBoneName = target switch
        {
            GeneratedSkinningComponentAttachmentTarget.UpperBack => "Spine_03",
            GeneratedSkinningComponentAttachmentTarget.Head => "Head",
            _ => throw new InvalidDataException(
                $"Unsupported manual component target value {(int)target}.")
        };
        TargetRigJoint[] matches = targetSkeleton.DeformJoints
            .Where(joint => string.Equals(
                joint.Name,
                exactBoneName,
                StringComparison.Ordinal))
            .ToArray();
        if (matches.Length != 1 ||
            !targetSkeleton.SkeletonIndexByRigJoint.TryGetValue(
                matches[0].JointIndex,
                out int skeletonIndex))
        {
            throw new InvalidDataException(
                $"Manual component target requires exactly one deform joint named " +
                $"'{exactBoneName}', but the current target rig does not provide it.");
        }
        return new ManualComponentAssignment(target, exactBoneName, skeletonIndex);
    }

    private static void ValidateExactComponentMembership(
        ImportedScene donor,
        GeometryComponent component,
        IReadOnlyList<TargetRigBodyVertexMembership>? memberships,
        string label)
    {
        if (memberships is null || memberships.Count == 0 ||
            memberships.Any(membership => membership is null))
        {
            throw new InvalidDataException(
                $"{label} has no original donor vertex membership.");
        }
        var actual = new HashSet<GeometryVertex>();
        var membershipMeshes = new HashSet<int>();
        foreach (TargetRigBodyVertexMembership membership in memberships)
        {
            if ((uint)membership.MeshIndex >= (uint)donor.Meshes.Count ||
                !membershipMeshes.Add(membership.MeshIndex) ||
                !string.Equals(
                    membership.MeshName,
                    donor.Meshes[membership.MeshIndex].Name,
                    StringComparison.Ordinal) ||
                membership.VertexIndices is null ||
                membership.VertexIndices.Count == 0)
            {
                throw new InvalidDataException(
                    $"{label} contains invalid or duplicate mesh membership.");
            }
            var perMesh = new HashSet<int>();
            foreach (int vertexIndex in membership.VertexIndices)
            {
                if ((uint)vertexIndex >=
                        (uint)donor.Meshes[membership.MeshIndex].Positions.Length ||
                    !perMesh.Add(vertexIndex) ||
                    !actual.Add(new GeometryVertex(membership.MeshIndex, vertexIndex)))
                {
                    throw new InvalidDataException(
                        $"{label} contains an invalid or duplicate original donor vertex.");
                }
            }
        }
        if (!actual.SetEquals(component.Vertices))
        {
            throw new InvalidDataException(
                $"{label} vertex membership does not exactly match connected component " +
                $"#{component.ComponentIndex}.");
        }
    }

    private static IReadOnlyList<TargetRigBodyVertexMembership> BuildComponentMembership(
        GeometryComponent component,
        IReadOnlyList<GeometrySource> donorSources)
    {
        TargetRigBodyVertexMembership[] memberships = component.Vertices
            .GroupBy(vertex => vertex.MeshIndex)
            .OrderBy(group => group.Key)
            .Select(group => new TargetRigBodyVertexMembership(
                group.Key,
                donorSources[group.Key].Name,
                Array.AsReadOnly(group
                    .Select(vertex => vertex.VertexIndex)
                    .Distinct()
                    .Order()
                    .ToArray())))
            .ToArray();
        return new ReadOnlyCollection<TargetRigBodyVertexMembership>(memberships);
    }

    private static bool ApproximatelyEqual(float left, float right)
    {
        float tolerance = 0.0001f * MathF.Max(1, MathF.Max(MathF.Abs(left), MathF.Abs(right)));
        return float.IsFinite(left) && float.IsFinite(right) &&
               MathF.Abs(left - right) <= tolerance;
    }

    private static bool ApproximatelyEqual(Vector3 left, Vector3 right)
    {
        float scale = MathF.Max(1, MathF.Max(left.Length(), right.Length()));
        return IsFinite(left) && IsFinite(right) &&
               Vector3.Distance(left, right) <= 0.0001f * scale;
    }

    private static RobustBounds ComputeRobustBounds(
        IReadOnlyList<Vector3> positions,
        string label)
    {
        if (positions.Count < 8)
            throw new InvalidDataException($"{label} has too few vertices for robust fitting.");
        float[] x = positions.Select(value => value.X).Order().ToArray();
        float[] y = positions.Select(value => value.Y).Order().ToArray();
        float[] z = positions.Select(value => value.Z).Order().ToArray();
        var lower = new Vector3(
            Quantile(x, RobustLowerQuantile),
            Quantile(y, RobustLowerQuantile),
            Quantile(z, RobustLowerQuantile));
        var upper = new Vector3(
            Quantile(x, RobustUpperQuantile),
            Quantile(y, RobustUpperQuantile),
            Quantile(z, RobustUpperQuantile));
        if (!IsFinite(lower) || !IsFinite(upper) ||
            upper.X < lower.X || upper.Y < lower.Y || upper.Z < lower.Z)
        {
            throw new InvalidDataException($"{label} produced invalid robust bounds.");
        }
        return new RobustBounds(lower, upper);
    }

    private static float Quantile(IReadOnlyList<float> sorted, float probability)
    {
        float position = probability * (sorted.Count - 1);
        int lower = (int)MathF.Floor(position);
        int upper = Math.Min(lower + 1, sorted.Count - 1);
        float amount = position - lower;
        return sorted[lower] + (sorted[upper] - sorted[lower]) * amount;
    }

    private static Vector3[] GetComponentPositions(
        IReadOnlyList<GeometrySource> meshes,
        GeometryComponent component)
    {
        return component.Vertices
            .Select(vertex => meshes[vertex.MeshIndex].Positions[vertex.VertexIndex])
            .Distinct()
            .ToArray();
    }

    private static Vector3 ComponentExtent(
        GeometryComponent component,
        IReadOnlyList<GeometrySource> meshes)
    {
        Vector3[] positions = GetComponentPositions(meshes, component);
        if (positions.Length == 0)
            return Vector3.Zero;
        Vector3 lower = positions[0];
        Vector3 upper = positions[0];
        foreach (Vector3 position in positions.AsSpan(1))
        {
            lower = Vector3.Min(lower, position);
            upper = Vector3.Max(upper, position);
        }
        return upper - lower;
    }

    private static string DescribeComponent(
        GeometryComponent component,
        IReadOnlyList<GeometrySource> meshes) =>
        string.Join(", ", component.Vertices
            .Select(vertex => vertex.MeshIndex)
            .Distinct()
            .Order()
            .Select(index => $"[{index}] {meshes[index].Name}"));

    private static Vector3 ApplyAlignment(
        Vector3 value,
        GeneratedSkinningAlignment alignment)
    {
        Vector3 result = Vector3.Transform(value, alignment.Matrix);
        if (!IsFinite(result))
        {
            throw new InvalidDataException(
                "Donor alignment produced a non-finite geometry position.");
        }
        return result;
    }

    private static Vector3 ApplyNormalAlignment(
        Vector3 value,
        GeneratedSkinningAlignment alignment)
    {
        Vector3 result = Vector3.TransformNormal(value, alignment.Matrix);
        float lengthSquared = result.LengthSquared();
        if (!IsFinite(result) || !float.IsFinite(lengthSquared))
        {
            throw new InvalidDataException(
                "Donor alignment produced a non-finite geometry normal.");
        }
        return lengthSquared <= 0.000000000001f
            ? Vector3.Zero
            : Vector3.Normalize(result);
    }

    private static float DistanceToCapsuleCenterline(
        Vector3 point,
        Vector3 start,
        Vector3 end)
    {
        Vector3 segment = end - start;
        float lengthSquared = segment.LengthSquared();
        if (!float.IsFinite(lengthSquared))
            return float.PositiveInfinity;
        if (lengthSquared <= PositionEpsilon * PositionEpsilon)
            return Vector3.Distance(point, start);
        float amount = Vector3.Dot(point - start, segment) / lengthSquared;
        amount = Math.Clamp(amount, 0, 1);
        return Vector3.Distance(point, start + segment * amount);
    }

    private static void ValidateDonorAttributes(ImportedScene donor)
    {
        for (int meshIndex = 0; meshIndex < donor.Meshes.Count; meshIndex++)
        {
            ImportedMesh mesh = donor.Meshes[meshIndex];
            if (mesh.Positions.Any(value => !IsFinite(value)))
                throw new InvalidDataException($"Donor mesh [{meshIndex}] has non-finite positions.");
            if (mesh.Normals.Length != 0 && mesh.Normals.Length != mesh.Positions.Length)
                throw new InvalidDataException($"Donor mesh [{meshIndex}] has incomplete normals.");
            if (mesh.Normals.Any(value => !IsFinite(value)))
                throw new InvalidDataException($"Donor mesh [{meshIndex}] has non-finite normals.");
            if (mesh.TextureCoordinates.Length != 0 &&
                mesh.TextureCoordinates.Length != mesh.Positions.Length)
            {
                throw new InvalidDataException($"Donor mesh [{meshIndex}] has incomplete UVs.");
            }
            if (mesh.TextureCoordinates.Any(value =>
                    !float.IsFinite(value.X) || !float.IsFinite(value.Y)))
            {
                throw new InvalidDataException($"Donor mesh [{meshIndex}] has non-finite UVs.");
            }
            if (mesh.DiffuseColors.Length != 0 &&
                mesh.DiffuseColors.Length != mesh.Positions.Length)
            {
                throw new InvalidDataException(
                    $"Donor mesh [{meshIndex}] has incomplete diffuse colors.");
            }
        }
    }

    private static int CheckedIndex(
        GeometrySource mesh,
        uint index,
        string label)
    {
        if (index >= mesh.Positions.Length)
        {
            throw new InvalidDataException(
                $"{label} mesh {mesh.Name} references vertex {index} outside " +
                $"its {mesh.Positions.Length} positions.");
        }
        return checked((int)index);
    }

    private static Vector3 Average(IEnumerable<Vector3> values)
    {
        Vector3 total = Vector3.Zero;
        int count = 0;
        foreach (Vector3 value in values)
        {
            total += value;
            count++;
        }
        if (count == 0 || !IsFinite(total))
            throw new InvalidDataException("Cannot compute a finite component center.");
        return total / count;
    }

    private static Vector3 Translation(Matrix4x4 value) =>
        new(value.M41, value.M42, value.M43);

    private static Matrix4x4 GetFittingWorldMatrix(
        TargetRigJoint joint,
        IReadOnlyList<Matrix4x4>? fittingWorldMatrices)
    {
        if (fittingWorldMatrices is null)
            return joint.BindWorldMatrix;
        if ((uint)joint.JointIndex >= (uint)fittingWorldMatrices.Count)
        {
            throw new InvalidDataException(
                $"Fitting pose has no world matrix for target joint {joint.Name}.");
        }
        Matrix4x4 value = fittingWorldMatrices[joint.JointIndex];
        if (!IsFinite(value))
        {
            throw new InvalidDataException(
                $"Fitting pose world matrix for target joint {joint.Name} is non-finite.");
        }
        return value;
    }

    private static bool IsFinite(Vector3 value) =>
        float.IsFinite(value.X) && float.IsFinite(value.Y) && float.IsFinite(value.Z);

    private static bool IsFinite(Vector4 value) =>
        float.IsFinite(value.X) && float.IsFinite(value.Y) &&
        float.IsFinite(value.Z) && float.IsFinite(value.W);

    private static bool IsFinite(Matrix4x4 value) =>
        float.IsFinite(value.M11) && float.IsFinite(value.M12) &&
        float.IsFinite(value.M13) && float.IsFinite(value.M14) &&
        float.IsFinite(value.M21) && float.IsFinite(value.M22) &&
        float.IsFinite(value.M23) && float.IsFinite(value.M24) &&
        float.IsFinite(value.M31) && float.IsFinite(value.M32) &&
        float.IsFinite(value.M33) && float.IsFinite(value.M34) &&
        float.IsFinite(value.M41) && float.IsFinite(value.M42) &&
        float.IsFinite(value.M43) && float.IsFinite(value.M44);

    private sealed class UnionFind
    {
        private readonly int[] _parents;
        private readonly byte[] _ranks;

        public UnionFind(int count)
        {
            _parents = Enumerable.Range(0, count).ToArray();
            _ranks = new byte[count];
        }

        public int Find(int value)
        {
            int root = value;
            while (_parents[root] != root)
                root = _parents[root];
            while (_parents[value] != value)
            {
                int parent = _parents[value];
                _parents[value] = root;
                value = parent;
            }
            return root;
        }

        public void Union(int left, int right)
        {
            int leftRoot = Find(left);
            int rightRoot = Find(right);
            if (leftRoot == rightRoot)
                return;
            if (_ranks[leftRoot] < _ranks[rightRoot])
            {
                _parents[leftRoot] = rightRoot;
            }
            else if (_ranks[leftRoot] > _ranks[rightRoot])
            {
                _parents[rightRoot] = leftRoot;
            }
            else
            {
                _parents[rightRoot] = leftRoot;
                _ranks[leftRoot]++;
            }
        }
    }
}
