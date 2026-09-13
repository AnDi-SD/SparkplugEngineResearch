using System.Numerics;

namespace SmoViewer.Core;

/// <summary>
/// Relative-size evidence for a partial-alpha FinalBlendOp 2 surface using the
/// observed RS[5]=0 profile. The field itself is not claimed to be a decoded
/// native depth-write switch; the diagnostic is intentionally conservative.
/// </summary>
public sealed record SmoLargeAlphaNoDepthInfo(
    bool IsApplicable,
    float ReferenceBoundsDiagonal,
    float CandidateBoundsDiagonal,
    float CandidateDiagonalFraction,
    float WarningFraction,
    bool HasLargeSurfaceOrderingRisk,
    string? Diagnostic);

public static class SmoLargeAlphaNoDepthAnalyzer
{
    private const float LargeSurfaceDiagonalFraction = 0.35f;
    private const float MinimumFiniteScale = 0.000001f;

    public static SmoLargeAlphaNoDepthInfo Analyze(
        SmoMesh candidate,
        SmoMaterialRenderStateInfo renderState,
        Matrix4x4 candidateWorldTransform,
        IReadOnlyList<SmoAlphaDecalOpaqueSurface> opaqueSurfaces)
    {
        ArgumentNullException.ThrowIfNull(candidate);
        ArgumentNullException.ThrowIfNull(renderState);
        ArgumentNullException.ThrowIfNull(opaqueSurfaces);

        bool applicable =
            renderState.HasRs5ZeroTransparentSurfaceProfile &&
            candidate.HasSkinningData &&
            candidate.HasNormals &&
            candidate.VertexCount >= 3;
        if (!applicable)
            return Empty(false);

        Bounds3 candidateBounds = IncludeMesh(
            Bounds3.Empty, candidate, candidateWorldTransform);
        Bounds3 referenceBounds = Bounds3.Empty;
        foreach (SmoAlphaDecalOpaqueSurface surface in opaqueSurfaces)
        {
            referenceBounds = IncludeMesh(
                referenceBounds, surface.Mesh, surface.WorldTransform);
        }

        float candidateDiagonal = candidateBounds.Diagonal;
        float referenceDiagonal = referenceBounds.Diagonal;
        if (candidateDiagonal <= MinimumFiniteScale ||
            referenceDiagonal <= MinimumFiniteScale)
        {
            return Empty(
                true,
                referenceDiagonal,
                candidateDiagonal);
        }

        float fraction = candidateDiagonal / referenceDiagonal;
        bool risk = float.IsFinite(fraction) &&
                    fraction >= LargeSurfaceDiagonalFraction;
        string? diagnostic = risk
            ? "LARGE_ALPHA_RS5_ZERO_ORDERING_UNCONFIRMED: this partial-alpha " +
              "FinalBlendOp 2 draw uses the observed RS[5]=0 profile and spans " +
              $"{fraction:P0} of the opaque character bounds diagonal " +
              $"({candidateDiagonal:G4}/{referenceDiagonal:G4}). The isolated " +
              "OpenGL preview uses explicit opaque/transparent passes but " +
              "cannot prove how this large " +
              "surface orders against alpha-tested foliage or other level " +
              "geometry in the native renderer. Validate it in game."
            : null;

        return new SmoLargeAlphaNoDepthInfo(
            true,
            referenceDiagonal,
            candidateDiagonal,
            fraction,
            LargeSurfaceDiagonalFraction,
            risk,
            diagnostic);
    }

    private static SmoLargeAlphaNoDepthInfo Empty(
        bool isApplicable,
        float referenceDiagonal = 0.0f,
        float candidateDiagonal = 0.0f) =>
        new(
            isApplicable,
            referenceDiagonal,
            candidateDiagonal,
            0.0f,
            LargeSurfaceDiagonalFraction,
            false,
            null);

    private static Bounds3 IncludeMesh(
        Bounds3 bounds,
        SmoMesh mesh,
        Matrix4x4 worldTransform)
    {
        foreach (Vector3 source in mesh.Positions)
        {
            Vector3 position = Vector3.Transform(source, worldTransform);
            if (IsFinite(position))
                bounds = bounds.Include(position);
        }
        return bounds;
    }

    private static bool IsFinite(Vector3 value) =>
        float.IsFinite(value.X) &&
        float.IsFinite(value.Y) &&
        float.IsFinite(value.Z);

    private readonly record struct Bounds3(Vector3 Minimum, Vector3 Maximum)
    {
        public static Bounds3 Empty => new(
            new Vector3(float.PositiveInfinity),
            new Vector3(float.NegativeInfinity));

        private bool IsValid =>
            IsFinite(Minimum) && IsFinite(Maximum) &&
            Minimum.X <= Maximum.X &&
            Minimum.Y <= Maximum.Y &&
            Minimum.Z <= Maximum.Z;

        public float Diagonal => IsValid
            ? Vector3.Distance(Minimum, Maximum)
            : 0.0f;

        public Bounds3 Include(Vector3 point) => !IsValid
            ? new Bounds3(point, point)
            : new Bounds3(
                Vector3.Min(Minimum, point),
                Vector3.Max(Maximum, point));
    }
}
