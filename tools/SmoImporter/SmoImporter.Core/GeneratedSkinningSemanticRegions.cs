using System.Collections.ObjectModel;
using System.Numerics;

namespace SmoImporter.Core;

/// <summary>
/// Humanoid vertex regions which may replace heuristic smooth weights inside
/// an explicitly selected donor body surface. Region identity is semantic and
/// never depends on donor mesh, material, or node names.
/// </summary>
public enum GeneratedSkinningSemanticRegion
{
    Head,
    LeftHand,
    RightHand
}

/// <summary>Outcome of conservative target/donor region resolution.</summary>
public enum GeneratedSkinningRegionStatus
{
    Applied,
    Disabled,
    UnsafeCalibration
}

/// <summary>
/// User-authored changes relative to an automatically calibrated Hand volume.
/// Head is edited exclusively through
/// <see cref="GeneratedSkinningSeparationPlaneAdjustment"/>.
/// </summary>
public sealed record GeneratedSkinningRegionAdjustment(
    GeneratedSkinningSemanticRegion Region,
    bool Enabled,
    float AxialOffset,
    float AxialScale,
    float RadialScale)
{
    public static GeneratedSkinningRegionAdjustment Automatic(
        GeneratedSkinningSemanticRegion region) =>
        new(region, Enabled: true, AxialOffset: 0, AxialScale: 1, RadialScale: 1);
}

/// <summary>
/// Safe editor limits derived from the calibrated volume. Core validates these
/// values again; exposing them lets a UI avoid producing known-unsafe drafts.
/// </summary>
public sealed record GeneratedSkinningRegionAdjustmentLimits(
    float MinimumAxialOffset,
    float MaximumAxialOffset,
    float MinimumAxialScale,
    float MaximumAxialScale,
    float MinimumRadialScale,
    float MaximumRadialScale);

/// <summary>
/// Oriented finite superellipsoid in target fitting space. Axes are finite unit
/// vectors; radii are positive half-extents. Hand regions use exponent 4 so the
/// distal finger cap is not clipped by spherical taper; their transition
/// membership occupies the proximal axial slab. Head resolutions no longer
/// expose this volume and use a hard separation plane instead.
/// </summary>
public sealed record GeneratedSkinningRegionVolume(
    Vector3 Center,
    Vector3 AxialAxis,
    Vector3 LateralAxis,
    Vector3 ForwardAxis,
    float AxialRadius,
    float LateralRadius,
    float ForwardRadius,
    float ProximalTransitionLength)
{
    public float ShapeExponent { get; init; } = 2;
}

/// <summary>
/// Exact resolved result for one semantic region. Vertex indices always refer
/// to the immutable input donor scene, including seam-expanded duplicates.
/// </summary>
public sealed record GeneratedSkinningRegionResolution(
    GeneratedSkinningSemanticRegion Region,
    bool IsEnabled,
    bool IsApplied,
    GeneratedSkinningRegionStatus Status,
    GeneratedSkinningRegionAdjustment Adjustment,
    GeneratedSkinningRegionAdjustmentLimits AdjustmentLimits,
    string AnchorBoneName,
    int AnchorSkeletonJointIndex,
    string ProximalBoneName,
    int ProximalSkeletonJointIndex,
    GeneratedSkinningRegionVolume? AutomaticVolume,
    GeneratedSkinningRegionVolume? ResolvedVolume,
    int CalibrationSampleCount,
    IReadOnlyList<TargetRigBodyVertexMembership> CoreVerticesByMesh,
    IReadOnlyList<TargetRigBodyVertexMembership> TransitionVerticesByMesh,
    IReadOnlyList<string> Warnings)
{
    /// <summary>
    /// Head uses this hard cut instead of an editable containment ellipsoid.
    /// Hand regions leave it null.
    /// </summary>
    public GeneratedSkinningSeparationPlaneResolution? SeparationPlane { get; init; }

    /// <summary>
    /// Detached connected surfaces proven geometrically to move with this
    /// semantic region. These remain whole rigid components; no name or material
    /// participates in their classification.
    /// </summary>
    public IReadOnlyList<int> RigidCompanionComponentIndices { get; init; } =
        Array.Empty<int>();

    /// <summary>
    /// Connected donor component which owns the articulated semantic region.
    /// Head uses it to keep the actual body/head surface on the per-vertex
    /// plane path while promoting only other protected components as whole
    /// rigid companions.
    /// </summary>
    public int TopologyOwnerComponentIndex { get; init; } = -1;

    /// <summary>
    /// Target-derived direct branches below Hand used as approximate finger
    /// lanes. Donor finger names and identities are never required.
    /// </summary>
    public IReadOnlyList<string> MotionBranchBoneNames { get; init; } =
        Array.Empty<string>();

    /// <summary>Core vertices carrying non-zero approximate finger articulation.</summary>
    public int CoarseMotionVertexCount { get; init; }

    /// <summary>Largest resolved non-Hand articulation weight.</summary>
    public float MaximumMotionProxyWeight { get; init; }
}

/// <summary>
/// Region analysis and the complete identity token required to commit edited
/// adjustments safely. Alignment and fitting pose are included because exact
/// captured donor membership is resolved in fitting space.
/// </summary>
public sealed record GeneratedSkinningRegionAnalysis(
    IReadOnlyList<GeneratedSkinningRegionResolution> Regions,
    string TargetRigFingerprint,
    string DonorGeometryFingerprint,
    string AlignmentFingerprint,
    string FittingPoseFingerprint)
{
    /// <summary>
    /// Resolved shoulder, Head and Back separation planes sharing this exact
    /// target/donor/alignment/pose identity.
    /// </summary>
    public IReadOnlyList<GeneratedSkinningSeparationPlaneResolution>
        SeparationPlanes { get; init; } =
        Array.Empty<GeneratedSkinningSeparationPlaneResolution>();

    /// <summary>
    /// Captures adjustments with this exact analysis identity. Core still
    /// validates all fingerprints and values when the overrides are consumed.
    /// </summary>
    public GeneratedSkinningRegionOverrides CreateOverrides(
        IReadOnlyList<GeneratedSkinningRegionAdjustment> adjustments)
    {
        ArgumentNullException.ThrowIfNull(adjustments);
        return new GeneratedSkinningRegionOverrides(
            new ReadOnlyCollection<GeneratedSkinningRegionAdjustment>(
                adjustments.ToArray()),
            TargetRigFingerprint,
            DonorGeometryFingerprint,
            AlignmentFingerprint,
            FittingPoseFingerprint)
        {
            SeparationPlanes = new ReadOnlyCollection<
                GeneratedSkinningSeparationPlaneAdjustment>(
                SeparationPlanes.Select(value => value.Adjustment).ToArray())
        };
    }

    public GeneratedSkinningRegionOverrides CreateOverrides(
        IReadOnlyList<GeneratedSkinningRegionAdjustment> adjustments,
        IReadOnlyList<GeneratedSkinningSeparationPlaneAdjustment> planes)
    {
        ArgumentNullException.ThrowIfNull(adjustments);
        ArgumentNullException.ThrowIfNull(planes);
        return new GeneratedSkinningRegionOverrides(
            new ReadOnlyCollection<GeneratedSkinningRegionAdjustment>(
                adjustments.ToArray()),
            TargetRigFingerprint,
            DonorGeometryFingerprint,
            AlignmentFingerprint,
            FittingPoseFingerprint)
        {
            SeparationPlanes = new ReadOnlyCollection<
                GeneratedSkinningSeparationPlaneAdjustment>(planes.ToArray())
        };
    }
}

/// <summary>
/// Immutable semantic-region edit contract. A caller obtains the four identity
/// values from <see cref="GeneratedSkinningRegionAnalysis.CreateOverrides"/>;
/// stale target, donor, alignment, or fitting-pose tokens are rejected.
/// Omitted regions retain their automatic adjustment.
/// </summary>
public sealed record GeneratedSkinningRegionOverrides(
    IReadOnlyList<GeneratedSkinningRegionAdjustment> Adjustments,
    string TargetRigFingerprint,
    string DonorGeometryFingerprint,
    string AlignmentFingerprint,
    string FittingPoseFingerprint)
{
    public IReadOnlyList<GeneratedSkinningSeparationPlaneAdjustment>
        SeparationPlanes { get; init; } =
        Array.Empty<GeneratedSkinningSeparationPlaneAdjustment>();
}
