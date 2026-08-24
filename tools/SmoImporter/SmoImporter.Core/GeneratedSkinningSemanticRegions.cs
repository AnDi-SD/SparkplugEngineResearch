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
/// User-authored changes relative to the automatically calibrated bone-local
/// volume. <see cref="AxialOffset"/> is expressed in target fitting-space
/// distance units; both scales are positive multipliers.
/// </summary>
public sealed record GeneratedSkinningRegionAdjustment(
    GeneratedSkinningSemanticRegion Region,
    bool Enabled,
    float AxialOffset,
    float AxialScale,
    float RadialScale)
{
    /// <summary>
    /// Head-only pitch in degrees around the volume lateral axis. Positive
    /// values move the proximal/lower pole toward the face and the distal/
    /// upper pole toward the back of the head. Hand regions must leave this at
    /// zero.
    /// </summary>
    public float ForwardTiltDegrees { get; init; }

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
/// vectors; radii are positive half-extents. The default exponent 2 is an
/// ellipsoid; hand regions use exponent 4 so the distal finger cap is not
/// clipped by spherical taper. Hand transition membership occupies the proximal
/// axial slab. For Head this volume is only a containment/editor guard: the
/// complete topology-proved lobe stays rigid, and its Head/Neck blend lives in a
/// separate disjoint geodesic collar on the external Neck side of the fixed cut.
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
    /// Detached connected surfaces proven geometrically to move with this
    /// semantic region. These remain whole rigid components; no name or material
    /// participates in their classification.
    /// </summary>
    public IReadOnlyList<int> RigidCompanionComponentIndices { get; init; } =
        Array.Empty<int>();

    /// <summary>
    /// Compatibility summary for older diagnostics: the central representative
    /// of the target-derived finger branches. The actual core weights may use
    /// every branch listed in <see cref="MotionBranchBoneNames"/> and its
    /// descendants. Empty/-1 means that the hand safely fell back to rigid Hand.
    /// </summary>
    public string MotionProxyBoneName { get; init; } = string.Empty;

    public int MotionProxySkeletonJointIndex { get; init; } = -1;

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
            FittingPoseFingerprint);
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
    string FittingPoseFingerprint);
