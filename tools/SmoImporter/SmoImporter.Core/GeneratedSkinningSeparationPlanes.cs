using System.Numerics;

namespace SmoImporter.Core;

/// <summary>
/// Editable infinite planes used to partition generated character weights and
/// detached rigid components. Shoulder planes stay anchored to their target
/// shoulder joints; Head and Back additionally allow translation along their
/// calibrated normal/vertical axis.
/// </summary>
public enum GeneratedSkinningSeparationPlaneKind
{
    LeftShoulder,
    RightShoulder,
    Head,
    Back
}

public sealed record GeneratedSkinningSeparationPlaneAdjustment(
    GeneratedSkinningSeparationPlaneKind Kind,
    bool Enabled,
    float Offset,
    float AngleDegrees)
{
    public static GeneratedSkinningSeparationPlaneAdjustment Automatic(
        GeneratedSkinningSeparationPlaneKind kind)
    {
        float angleDegrees = kind is
            GeneratedSkinningSeparationPlaneKind.LeftShoulder or
            GeneratedSkinningSeparationPlaneKind.RightShoulder
                ? 7
                : 0;
        return new(kind, Enabled: true, Offset: 0, AngleDegrees: angleDegrees);
    }
}

public sealed record GeneratedSkinningSeparationPlaneLimits(
    float MinimumOffset,
    float MaximumOffset,
    float MinimumAngleDegrees,
    float MaximumAngleDegrees);

/// <summary>
/// A resolved fitting-space plane. Positive signed distance points toward the
/// arm for shoulder planes, above the neck for Head, and toward the front for
/// Back. AxisU/AxisV and the half extents are preview-only finite bounds; all
/// classification uses the infinite plane.
/// </summary>
public sealed record GeneratedSkinningSeparationPlaneResolution(
    GeneratedSkinningSeparationPlaneKind Kind,
    bool IsEnabled,
    bool IsAvailable,
    string AnchorBoneName,
    Vector3 Point,
    Vector3 Normal,
    Vector3 AxisU,
    Vector3 AxisV,
    float PreviewHalfExtentU,
    float PreviewHalfExtentV,
    GeneratedSkinningSeparationPlaneAdjustment Adjustment,
    GeneratedSkinningSeparationPlaneLimits Limits,
    IReadOnlyList<string> Warnings)
{
    /// <summary>Unedited target-joint anchor in fitting space.</summary>
    public Vector3 AutomaticPoint { get; init; } = Point;

    /// <summary>Normal at zero angle, before applying the draft rotation.</summary>
    public Vector3 AutomaticNormal { get; init; } = Normal;

    /// <summary>Second unit axis spanning the allowed rotation plane.</summary>
    public Vector3 RotationSecondaryAxis { get; init; } = AxisV;

    /// <summary>Axis used by Offset; zero for fixed shoulder anchors.</summary>
    public Vector3 OffsetAxis { get; init; }

    /// <summary>
    /// Center of the finite editor preview. Shoulder walls use their joint as
    /// the upper edge, so their preview extends only down the rotated AxisV;
    /// classification continues to use the exact joint-anchored Point.
    /// </summary>
    public Vector3 PreviewCenter => Kind is
        GeneratedSkinningSeparationPlaneKind.LeftShoulder or
        GeneratedSkinningSeparationPlaneKind.RightShoulder
            ? Point + AxisV * PreviewHalfExtentV
            : Point;
}

internal static class GeneratedSkinningSeparationPlaneMath
{
    public static float SignedDistance(
        Vector3 position,
        GeneratedSkinningSeparationPlaneResolution plane) =>
        Vector3.Dot(position - plane.Point, plane.Normal);

    public static Vector3 RotateInPlane(
        Vector3 primary,
        Vector3 secondary,
        float angleDegrees)
    {
        float radians = angleDegrees * MathF.PI / 180f;
        Vector3 result = primary * MathF.Cos(radians) +
                         secondary * MathF.Sin(radians);
        float lengthSquared = result.LengthSquared();
        if (!float.IsFinite(lengthSquared) || lengthSquared <= 1e-12f)
            throw new InvalidDataException(
                "Separation-plane rotation produced a degenerate axis.");
        return Vector3.Normalize(result);
    }
}
