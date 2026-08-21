using System.Collections.ObjectModel;
using System.Diagnostics;
using System.Numerics;
using SmoExporter.Core;
using SmoViewer.Core;

namespace SmoImporter.Core;

/// <summary>
/// Uniform external-space alignment derived only from the largest connected
/// target and donor surface components.
/// </summary>
public sealed record GeneratedSkinningAlignment(
    float Scale,
    Vector3 Translation)
{
    public Matrix4x4 Matrix =>
        Matrix4x4.CreateScale(Scale) *
        Matrix4x4.CreateTranslation(Translation);
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
/// a positive uniform scale/translation cannot change component topology.
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
}

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

    /// <summary>Smooth vertices whose capsule score was reduced by a torso/head field.</summary>
    public int AnatomicalVolumeAffectedVertexCount { get; init; }

    /// <summary>
    /// Smooth vertices for which every anatomical-field contribution was zero
    /// or could not improve the legacy capsule score; their score path remains
    /// bit-identical to the legacy implementation.
    /// </summary>
    public int AnatomicalVolumeLegacyVertexCount { get; init; }
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
public static class GeneratedSkinningPreparer
{
    // Start with the full nearest-four result and reduce it only when the exact
    // target palette plan proves that result cannot fit. Three is the highest
    // compatible limit observed for Bloom/Layla; two remains a conservative
    // final retry for targets with fewer writable hardware palettes.
    private const int TopFourComparisonInfluences = 4;
    private const int MinimumGeneratedInfluences = 2;
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

    private sealed record SideCalibration(
        float CenterX,
        float LeftDirection,
        Vector3 Center,
        Vector3 LeftAxis,
        float DeadZone,
        bool UseVectorAxis);

    private sealed record PackedInfluence(
        ushort Joint,
        float Weight);

    private sealed record GeneratedVertexInfluences(
        PackedInfluence[] Influences,
        PackedInfluence[] TopFourInfluences,
        float DiscardedTopFourWeightMass,
        float TopFourToFinalWeightL1Distance,
        bool AnatomicalVolumeAffected);

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
    /// Applies validated detached-component assignments while retaining the
    /// legacy single dominant donor-body selection.
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
    /// Generates weights for an explicitly selected multi-component donor body.
    /// The selection must have been produced for the exact target rig, donor
    /// scene, and alignment supplied here. Every selected connected component
    /// receives smooth generated weights; all remaining components retain the
    /// conservative rigid-attachment path.
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
    /// Generates smooth weights for the exact selected body and applies
    /// validated one-hot assignments to explicitly selected detached surfaces.
    /// Manual assignments cannot target body components and are independent of
    /// the donor alignment identity.
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
        TargetRigFittingPoseSnapshot fittingPose)
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
            componentOverrides: null);
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
    /// Fitting-pose counterpart of the manual detached-component overload,
    /// retaining the legacy single dominant donor-body selection.
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
    /// Applies a validated multi-component donor body selection before smooth
    /// generated weights and fitting-pose baking. This overload is deliberately
    /// explicit: legacy calls keep their single-dominant-surface safety policy.
    /// </summary>
    public static GeneratedSkinningPreparationResult Prepare(
        SmoDocument target,
        ImportedScene donor,
        TargetRigFittingPoseSnapshot fittingPose,
        ReplacementTransform donorAlignment,
        TargetRigBodySelection bodySelection)
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
            componentOverrides: null);
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

    private static GeneratedSkinningPreparationResult PrepareCore(
        SmoDocument target,
        ImportedScene donor,
        TargetRigFittingPoseSnapshot? fittingPose,
        GeneratedSkinningAlignment? alignmentOverride,
        TargetRigBodySelection? bodySelection,
        GeneratedSkinningComponentOverrides? componentOverrides)
    {
        var paletteFailures = new List<string>();
        for (int maximumInfluences = TopFourComparisonInfluences;
             maximumInfluences >= MinimumGeneratedInfluences;
             maximumInfluences--)
        {
            GeneratedSkinningPreparationResult candidate =
                PrepareWithInfluenceLimit(
                    target,
                    donor,
                    fittingPose,
                    alignmentOverride,
                    bodySelection,
                    componentOverrides,
                    maximumInfluences);
            GlbSkinTransferPlan plan = SmoSkinnedGlbReplacer.Analyze(
                target,
                candidate.PreparedScene,
                SkinnedTextureTransferMode.PreserveTarget);
            string[] capacityFailures = plan.Messages
                .Where(IsPaletteCapacityFailure)
                .ToArray();
            if (capacityFailures.Length == 0)
            {
                return maximumInfluences == TopFourComparisonInfluences
                    ? candidate
                    : AppendAnalysisWarning(
                        candidate,
                        $"The exact PreserveTarget palette plan rejected higher " +
                        $"influence limits and selected {maximumInfluences} as the " +
                        "highest compatible mode-3 limit. " +
                        string.Join(" | ", paletteFailures));
            }

            paletteFailures.Add(
                $"max {maximumInfluences}: {string.Join(" | ", capacityFailures)}");
            if (maximumInfluences == MinimumGeneratedInfluences)
            {
                return AppendAnalysisWarning(
                    candidate,
                    "Even the minimum generated influence limit remains incompatible " +
                    "with the exact PreserveTarget palette plan. " +
                    string.Join(" | ", paletteFailures));
            }
        }

        throw new UnreachableException();
    }

    private static GeneratedSkinningPreparationResult PrepareWithInfluenceLimit(
        SmoDocument target,
        ImportedScene donor,
        TargetRigFittingPoseSnapshot? fittingPose,
        GeneratedSkinningAlignment? alignmentOverride,
        TargetRigBodySelection? bodySelection,
        GeneratedSkinningComponentOverrides? componentOverrides,
        int maximumInfluences)
    {
        ArgumentNullException.ThrowIfNull(target);
        ArgumentNullException.ThrowIfNull(donor);
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
        SceneTopology targetTopology = BuildTopology(targetSources, "target SMO");
        SceneTopology donorTopology = BuildTopology(donorSources, "donor");
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
            // An explicit scale/translation cannot correct a non-Y-up model.
            // Keep this structural mode-3 precondition while deliberately not
            // replacing the requested alignment with the automatic fit.
            ValidateVerticalAxis(targetBounds, "target main component");
            ValidateVerticalAxis(
                donorBounds,
                bodySelection is null
                    ? "donor main component"
                    : "selected donor body components");
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

        var attachments = new List<GeneratedSkinningAttachment>();
        var warnings = new List<string>
        {
            "Generated-skinning input is required to be upright, Y-up, unmirrored, " +
            "and facing the same direction as the target; an unskinned surface " +
            "cannot prove this orientation automatically.",
            "Weights were generated heuristically from target bind-pose bone capsules " +
            "plus finite target-weight-calibrated torso/head fields; extreme animation " +
            "poses still require visual inspection."
        };
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
        if (bodySelection is not null)
        {
            warnings.Add(
                $"Explicit body selection combined {donorBodyComponents.Length} validated " +
                $"connected donor surface component(s) for smooth weights; the remaining " +
                $"{donorTopology.Components.Count - donorBodyComponents.Length} component(s) " +
                "stay on the rigid-attachment path.");
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

        Vector3[][] alignedPositionsByMesh = donor.Meshes
            .Select(mesh => mesh.Positions
                .Select(position => ApplyAlignment(position, alignment))
                .ToArray())
            .ToArray();
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
        double discardedTopFourWeightMassSum = 0;
        double topFourToFinalWeightL1DistanceSum = 0;
        float maximumDiscardedTopFourWeightMass = 0;
        float maximumTopFourToFinalWeightL1Distance = 0;

        foreach (GeometryComponent component in donorTopology.Components)
        {
            bool isMain = donorBodyComponentIndices.Contains(component.ComponentIndex);
            if (isMain)
            {
                foreach (GeometryVertex vertex in component.Vertices)
                {
                    GeneratedVertexInfluences generated = GenerateVertexInfluences(
                        alignedPositionsByMesh[vertex.MeshIndex][vertex.VertexIndex],
                        capsules,
                        anatomicalVolumes,
                        sideCalibration,
                        targetBounds.Size.Y,
                        maximumInfluences);
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
            ManualComponentAssignment? manualAssignment = manualAssignments
                .GetValueOrDefault(component.ComponentIndex);
            BoneCapsule attachmentBone;
            if (manualAssignment is null)
            {
                attachmentBone = SelectAttachmentBone(
                    alignedCenter,
                    capsules,
                    sideCalibration,
                    targetBounds.Size.Y,
                    warnings,
                    componentLabel,
                    component.ComponentIndex);
            }
            else
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
            foreach (GeometryVertex vertex in component.Vertices)
            {
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
            int[] meshIndices = component.Vertices
                .Select(vertex => vertex.MeshIndex)
                .Distinct()
                .Order()
                .ToArray();
            string[] meshNames = meshIndices
                .Select(index => donorSources[index].Name)
                .ToArray();
            IReadOnlyList<TargetRigBodyVertexMembership> membership =
                BuildComponentMembership(component, donorSources);
            attachments.Add(new GeneratedSkinningAttachment(
                component.ComponentIndex,
                Array.AsReadOnly(meshIndices),
                Array.AsReadOnly(meshNames),
                component.Vertices.Length,
                component.TriangleCount,
                attachmentBone.BoneName,
                attachmentBone.SkeletonJointIndex,
                distance,
                alignedCenter)
            {
                VerticesByMesh = membership,
                ManualAssignment = manualAssignment?.Target
            });
            warnings.Add(manualAssignment is null
                ? $"Detached component {componentLabel}#{component.ComponentIndex} was kept " +
                  $"rigid on {attachmentBone.BoneName}; confirm this attachment before writing SMO."
                : $"Detached component {componentLabel}#{component.ComponentIndex} uses the " +
                  $"validated manual {manualAssignment.Target} assignment and is rigidly " +
                  $"one-hot weighted to exact joint {attachmentBone.BoneName}.");
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
                jointsByMesh[vertex.MeshIndex][vertex.VertexIndex] =
                    new ImportedJointIndices(placeholderJoint, 0, 0, 0);
                weightsByMesh[vertex.MeshIndex][vertex.VertexIndex] = Vector4.UnitX;
                topFourJointsByMesh[vertex.MeshIndex][vertex.VertexIndex] =
                    jointsByMesh[vertex.MeshIndex][vertex.VertexIndex];
                topFourWeightsByMesh[vertex.MeshIndex][vertex.VertexIndex] = Vector4.UnitX;
                assignedByMesh[vertex.MeshIndex][vertex.VertexIndex] = true;
            }
        }

        var preparedMeshes = new ImportedMesh[donor.Meshes.Count];
        int preparedVertices = 0;
        for (int meshIndex = 0; meshIndex < donor.Meshes.Count; meshIndex++)
        {
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
                Normals = source.Normals.ToArray(),
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
        warnings.Add(
            $"Finite torso/head anatomical fields changed capsule scores for " +
            $"{anatomicalVolumeAffectedVertexCount} of {smoothVertexCount} smooth " +
            $"vertices; {smoothVertexCount - anatomicalVolumeAffectedVertexCount} " +
            "vertices retained the bit-identical legacy capsule score path.");

        ImportedTexture[] preparedTextures = donor.Textures
            .Select(texture => texture with { Data = texture.Data.ToArray() })
            .ToArray();
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
            AnatomicalVolumeAffectedVertexCount = anatomicalVolumeAffectedVertexCount,
            AnatomicalVolumeLegacyVertexCount =
                smoothVertexCount - anatomicalVolumeAffectedVertexCount
        };
        return new GeneratedSkinningPreparationResult(
            analysis,
            preparedScene)
        {
            FittingPreviewScene = fittingScene
        };
    }

    private static bool IsPaletteCapacityFailure(string message) =>
        message.Contains(
            "Material group needs more than",
            StringComparison.Ordinal);

    private static GeneratedSkinningPreparationResult AppendAnalysisWarning(
        GeneratedSkinningPreparationResult preparation,
        string warning)
    {
        string[] warnings = preparation.Analysis.Warnings
            .Append(warning)
            .ToArray();
        return preparation with
        {
            Analysis = preparation.Analysis with
            {
                Warnings = new ReadOnlyCollection<string>(warnings)
            }
        };
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

        if (jointsByName.TryGetValue("Neck", out TargetRigJoint? neck) &&
            jointsByName.TryGetValue("Head", out TargetRigJoint? head) &&
            layout.SkeletonIndexByRigJoint.TryGetValue(
                head.JointIndex,
                out int headSkeletonIndex) &&
            samplesBySkeleton.TryGetValue(
                headSkeletonIndex,
                out List<Vector3>? headSamples) &&
            headSamples.Distinct().Count() >= MinimumEnvelopeSamples)
        {
            Vector3 bindNeck = Translation(neck.BindWorldMatrix);
            Vector3 bindHead = Translation(head.BindWorldMatrix);
            BuildVolumeFrame(
                bindNeck,
                bindHead,
                bindLateral,
                out Vector3 bindUp,
                out Vector3 bindFrameLateral,
                out Vector3 bindForward);
            Vector3[] uniqueHeadSamples = headSamples.Distinct().ToArray();
            float[] initialForward = uniqueHeadSamples
                .Select(position => Vector3.Dot(position - bindHead, bindForward))
                .Order()
                .ToArray();
            float forwardSign = Quantile(initialForward, RobustUpperQuantile) >=
                                -Quantile(initialForward, RobustLowerQuantile)
                ? 1f
                : -1f;
            bindForward *= forwardSign;
            float[] lateral = uniqueHeadSamples
                .Select(position => MathF.Abs(Vector3.Dot(
                    position - bindHead, bindFrameLateral)))
                .Order()
                .ToArray();
            float[] forward = uniqueHeadSamples
                .Select(position => MathF.Max(0, Vector3.Dot(
                    position - bindHead, bindForward)))
                .Order()
                .ToArray();
            float[] axial = uniqueHeadSamples
                .Select(position => Vector3.Dot(position - bindHead, bindUp))
                .Order()
                .ToArray();
            float axialLower = Quantile(axial, RobustLowerQuantile);
            float axialUpper = Quantile(axial, RobustUpperQuantile);
            float axialCenter = (axialLower + axialUpper) * 0.5f;
            float lateralRadius = MathF.Max(
                Quantile(lateral, RobustUpperQuantile),
                minimumRadius);
            float forwardRadius = MathF.Max(
                Quantile(forward, RobustUpperQuantile) * 0.5f,
                minimumRadius);
            float axialRadius = MathF.Max(
                (axialUpper - axialLower) * 0.5f,
                minimumRadius);

            Vector3 posedNeck = Translation(GetFittingWorldMatrix(
                neck,
                fittingWorldMatrices));
            Vector3 posedHead = Translation(GetFittingWorldMatrix(
                head,
                fittingWorldMatrices));
            BuildVolumeFrame(
                posedNeck,
                posedHead,
                fittingLateral,
                out Vector3 posedUp,
                out Vector3 posedFrameLateral,
                out Vector3 posedForward);
            posedForward *= forwardSign;
            Vector3 center = posedHead +
                             posedUp * axialCenter +
                             posedForward * forwardRadius;
            result[headSkeletonIndex] = new AnatomicalVolume(
                headSkeletonIndex,
                "Head",
                center,
                center + posedUp * axialRadius,
                posedFrameLateral,
                posedForward,
                lateralRadius,
                forwardRadius,
                axialRadius,
                IsHead: true,
                uniqueHeadSamples.Length);
            messages.Add(
                $"Target-weight shifted Head ellipsoid: {uniqueHeadSamples.Length} samples, " +
                $"radii ({lateralRadius:G6}, {axialRadius:G6}, " +
                $"{forwardRadius:G6}); its posterior surface exactly touches the posed " +
                "head axis.");
        }
        else
        {
            messages.Add(
                "Target-weight Head ellipsoid was skipped: exact Neck/Head deform joints " +
                $"and at least {MinimumEnvelopeSamples} Head-weighted vertices are required.");
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
        int maximumInfluences)
    {
        BodySide vertexSide = ClassifyPositionSide(position, calibration);
        bool anatomicalVolumeAffected = false;
        float headFieldAlpha = anatomicalVolumes.Values
            .Where(volume => volume.IsHead)
            .Select(volume => AnatomicalVolumeAlpha(
                DistanceToAnatomicalVolume(position, volume)))
            .DefaultIfEmpty(0)
            .Max();
        float torsoFieldAlpha = anatomicalVolumes.Values
            .Where(volume => IsTorsoFieldBone(volume.BoneName))
            .Select(volume => AnatomicalVolumeAlpha(
                DistanceToAnatomicalVolume(position, volume)))
            .DefaultIfEmpty(0)
            .Max() * (1 - headFieldAlpha);
        var distances = capsules
            .Where(capsule => capsule.SafeForAutomaticWeights &&
                              !IsOpposite(vertexSide, capsule.Side))
            .GroupBy(capsule => capsule.SkeletonJointIndex)
            .Select(group =>
            {
                BoneCapsule representative = group.First();
                float legacyNormalizedDistance = group.Min(capsule =>
                    DistanceToCapsuleCenterline(position, capsule.Start, capsule.End) /
                    capsule.Radius);
                float normalizedDistance = legacyNormalizedDistance;
                if (anatomicalVolumes.TryGetValue(
                        representative.SkeletonJointIndex,
                        out AnatomicalVolume? volume))
                {
                    float shapeDistance = DistanceToAnatomicalVolume(position, volume);
                    float alpha = AnatomicalVolumeAlpha(shapeDistance);
                    if (!volume.IsHead)
                        alpha *= 1 - headFieldAlpha;
                    // The calibrated primitive is a solid anatomical volume,
                    // not another centreline. Every point inside it has zero
                    // distance to the volume; only the exterior shell grows a
                    // normalized distance. This prevents nearby arm/thigh
                    // capsules from winning across the chest interior.
                    float volumeDistance = MathF.Max(
                        0,
                        shapeDistance - EnvelopeCoreRatio);
                    if (alpha > 0 && volumeDistance < legacyNormalizedDistance)
                    {
                        // Do not evaluate a lerp at alpha zero. This explicit
                        // branch makes the score bit-identical to the old capsule
                        // path for every vertex outside the finite field.
                        normalizedDistance = legacyNormalizedDistance +
                            (volumeDistance - legacyNormalizedDistance) * alpha;
                        anatomicalVolumeAffected |=
                            BitConverter.SingleToInt32Bits(normalizedDistance) !=
                            BitConverter.SingleToInt32Bits(legacyNormalizedDistance);
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
                        BitConverter.SingleToInt32Bits(legacyNormalizedDistance);
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
                "A donor body vertex has no finite side-compatible target bone capsule.");
        }
        if (distances[0].absoluteDistance > targetHeight * 0.75f)
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
            anatomicalVolumeAffected);
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
            // their legacy bit path.
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
        if (donorAlignment.RotationDegrees != Vector3.Zero)
        {
            throw new ArgumentException(
                "Generated-skinning donor alignment does not support rotation; " +
                "RotationDegrees must be exactly zero.",
                nameof(donorAlignment));
        }
        if (!float.IsFinite(donorAlignment.Scale) || donorAlignment.Scale <= 0 ||
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
            donorAlignment.Translation);
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
        if (fittingWorldMatrices is null)
        {
            float[] legacyLeft = deformJoints
                .Where(joint => ClassifyBoneSide(joint.Name) == BodySide.Left)
                .Select(joint => Translation(joint.BindWorldMatrix).X)
                .ToArray();
            float[] legacyRight = deformJoints
                .Where(joint => ClassifyBoneSide(joint.Name) == BodySide.Right)
                .Select(joint => Translation(joint.BindWorldMatrix).X)
                .ToArray();
            if (legacyLeft.Length == 0 || legacyRight.Length == 0)
            {
                throw new InvalidDataException(
                    "Target skeleton has no unambiguous named left/right " +
                    "deform-joint pairs.");
            }
            float legacyLeftAverage = legacyLeft.Average();
            float legacyRightAverage = legacyRight.Average();
            float legacySeparation = legacyLeftAverage - legacyRightAverage;
            float legacyMinimumSeparation = MathF.Max(
                targetBounds.Size.X * 0.05f,
                targetBounds.Size.Y * 0.01f);
            if (!float.IsFinite(legacySeparation) ||
                MathF.Abs(legacySeparation) <= legacyMinimumSeparation)
            {
                throw new InvalidDataException(
                    "Target left/right bone positions do not define a stable " +
                    "character side axis.");
            }
            return new SideCalibration(
                targetBounds.Center.X,
                MathF.Sign(legacySeparation),
                Vector3.Zero,
                Vector3.Zero,
                legacyMinimumSeparation * 0.5f,
                UseVectorAxis: false);
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
            UseVectorAxis: true);

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
        string label)
    {
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
                vertices[offsets[mesh.MeshIndex] + vertex] =
                    new GeometryVertex(mesh.MeshIndex, vertex);
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
            selection.DonorAlignment.RotationDegrees != Vector3.Zero ||
            selection.DonorAlignment.Translation != alignment.Translation ||
            !IsFinite(selection.DonorAlignment.Matrix))
        {
            throw new InvalidDataException(
                "Explicit donor body selection belongs to a different donor alignment.");
        }
        if (selection.Components is null ||
            selection.Components.Any(component => component is null) ||
            selection.Components.Count is < 1 or > 2 ||
            selection.TotalComponentCount != topology.Components.Count ||
            selection.ExcludedComponentCount !=
                topology.Components.Count - selection.Components.Count)
        {
            throw new InvalidDataException(
                "Explicit donor body selection component totals are inconsistent " +
                "with the current donor topology.");
        }
        TargetRigBodyComponentRole[] roles = selection.Components
            .Select(component => component.Role)
            .Order()
            .ToArray();
        bool validRoles = roles.Length == 1
            ? roles[0] == TargetRigBodyComponentRole.WholeBody
            : roles.SequenceEqual(new[]
            {
                TargetRigBodyComponentRole.LowerBody,
                TargetRigBodyComponentRole.TorsoAndArms
            });
        if (!validRoles)
        {
            throw new InvalidDataException(
                "Explicit donor body selection roles must be one whole body or " +
                "one lower body plus one torso-and-arms component.");
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
        Vector3 result = value * alignment.Scale + alignment.Translation;
        if (!IsFinite(result))
        {
            throw new InvalidDataException(
                "Donor alignment produced a non-finite geometry position.");
        }
        return result;
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
