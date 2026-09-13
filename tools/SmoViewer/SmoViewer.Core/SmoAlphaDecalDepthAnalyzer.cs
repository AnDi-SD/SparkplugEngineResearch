using System.Numerics;

namespace SmoViewer.Core;

/// <summary>
/// One opaque sibling surface used as bind-pose depth evidence for a
/// princess-style partial-alpha draw unit.
/// </summary>
public readonly record struct SmoAlphaDecalOpaqueSurface(
    SmoMesh Mesh,
    Matrix4x4 WorldTransform);

/// <summary>
/// Conservative bind-pose evidence that a partial-alpha draw unit behaves
/// like a decal placed almost directly on an opaque sibling surface.
/// </summary>
public sealed record SmoAlphaDecalDepthInfo(
    bool IsApplicable,
    int OpaqueSurfaceCount,
    int OpaqueTriangleCount,
    int CandidateVertexCount,
    float OpaqueBoundsDiagonal,
    float CandidateBoundsDiagonalFraction,
    float NearSurfaceThreshold,
    float MedianNearestSurfaceDistance,
    int NearParallelVertexCount,
    float NearParallelVertexFraction,
    bool HasNearCoplanarDepthRisk,
    string? Diagnostic);

/// <summary>
/// Detects a narrow native-rendering risk that an isolated source-alpha
/// preview can conceal: a confirmed princess-style partial-alpha mesh placed
/// almost coplanar with opaque character geometry.
/// </summary>
public static class SmoAlphaDecalDepthAnalyzer
{
    // Relative thresholds avoid character-unit assumptions. The current Layla
    // eyes/mouth sit about 0.05% of body height above the face; authored tiara,
    // wings and ornaments are materially farther away.
    private const float NearSurfaceDiagonalFraction = 0.001f;
    private const float MaximumCandidateDiagonalFraction = 0.35f;
    private const float MinimumNearParallelVertexFraction = 0.50f;
    private const float MinimumNormalAlignment = 0.85f;
    private const float MinimumFiniteScale = 0.000001f;

    /// <summary>
    /// Applies the explicitly labelled Viewer failure-mode simulation to a
    /// BGRA32 pixel buffer. RGB is preserved so disabling the simulation is
    /// lossless; only sampled alpha is hidden for the flagged draw unit.
    /// </summary>
    public static void ApplyOverlayLossSimulation(Span<byte> bgraPixels)
    {
        if (bgraPixels.Length % 4 != 0)
        {
            throw new ArgumentException(
                "A BGRA32 pixel buffer must contain complete four-byte pixels.",
                nameof(bgraPixels));
        }

        for (int offset = 3; offset < bgraPixels.Length; offset += 4)
            bgraPixels[offset] = 0;
    }

    public static SmoAlphaDecalDepthInfo Analyze(
        SmoMesh candidate,
        SmoMaterialRenderStateInfo renderState,
        Matrix4x4 candidateWorldTransform,
        IReadOnlyList<SmoAlphaDecalOpaqueSurface> opaqueSurfaces)
    {
        ArgumentNullException.ThrowIfNull(candidate);
        ArgumentNullException.ThrowIfNull(renderState);
        ArgumentNullException.ThrowIfNull(opaqueSurfaces);

        bool applicable =
            renderState.HasConfirmedPrincessTransparentSurfaceState &&
            candidate.HasSkinningData &&
            candidate.HasNormals &&
            candidate.VertexCount >= 3;
        if (!applicable)
            return Empty(isApplicable: false, candidate.VertexCount);

        Triangle[] opaqueTriangles = BuildOpaqueTriangles(
            opaqueSurfaces, out Bounds3 opaqueBounds, out int surfaceCount);
        float opaqueDiagonal = opaqueBounds.Diagonal;
        if (opaqueTriangles.Length == 0 ||
            !float.IsFinite(opaqueDiagonal) ||
            opaqueDiagonal <= MinimumFiniteScale)
        {
            return Empty(
                isApplicable: true,
                candidate.VertexCount,
                surfaceCount,
                opaqueTriangles.Length,
                opaqueDiagonal);
        }

        float threshold = opaqueDiagonal * NearSurfaceDiagonalFraction;
        Bounds3 candidateBounds = Bounds3.Empty;
        Matrix4x4 candidateNormalTransform =
            CreateNormalTransform(candidateWorldTransform);
        float[] nearestDistances = new float[candidate.VertexCount];
        int nearParallelCount = 0;
        int finiteCandidateCount = 0;

        for (int vertexIndex = 0;
             vertexIndex < candidate.VertexCount;
             vertexIndex++)
        {
            Vector3 position = Vector3.Transform(
                candidate.Positions[vertexIndex], candidateWorldTransform);
            if (!IsFinite(position))
            {
                nearestDistances[vertexIndex] = float.PositiveInfinity;
                continue;
            }

            candidateBounds = candidateBounds.Include(position);
            finiteCandidateCount++;
            float nearestDistanceSquared = float.PositiveInfinity;
            Vector3 nearestSurfaceNormal = Vector3.Zero;
            foreach (Triangle triangle in opaqueTriangles)
            {
                float boundsDistanceSquared = triangle.Bounds.DistanceSquaredTo(position);
                if (boundsDistanceSquared > nearestDistanceSquared)
                    continue;

                float distanceSquared = PointTriangleDistanceSquared(
                    position, triangle.A, triangle.B, triangle.C);
                if (distanceSquared < nearestDistanceSquared)
                {
                    nearestDistanceSquared = distanceSquared;
                    nearestSurfaceNormal = triangle.Normal;
                }
            }

            float nearestDistance = MathF.Sqrt(nearestDistanceSquared);
            nearestDistances[vertexIndex] = nearestDistance;
            Vector3 candidateNormal = Vector3.TransformNormal(
                candidate.Normals[vertexIndex], candidateNormalTransform);
            bool aligned = TryNormalize(ref candidateNormal) &&
                           TryNormalize(ref nearestSurfaceNormal) &&
                           MathF.Abs(Vector3.Dot(
                               candidateNormal, nearestSurfaceNormal)) >=
                           MinimumNormalAlignment;
            if (nearestDistance <= threshold && aligned)
                nearParallelCount++;
        }

        float[] finiteDistances = nearestDistances
            .Where(float.IsFinite)
            .Order()
            .ToArray();
        float medianDistance = finiteDistances.Length == 0
            ? float.PositiveInfinity
            : finiteDistances[finiteDistances.Length / 2];
        float nearParallelFraction = finiteCandidateCount == 0
            ? 0.0f
            : (float)nearParallelCount / finiteCandidateCount;
        float candidateDiagonalFraction = candidateBounds.Diagonal /
                                          opaqueDiagonal;
        bool risk =
            finiteCandidateCount >= 3 &&
            candidateDiagonalFraction <= MaximumCandidateDiagonalFraction &&
            nearParallelFraction >= MinimumNearParallelVertexFraction &&
            medianDistance <= threshold;

        string? diagnostic = risk
            ? "NATIVE_ALPHA_DECAL_DEPTH_UNCONFIRMED: this confirmed " +
              "princess-style FinalBlendOp 2 partial-alpha run lies almost " +
              "coplanar with an opaque sibling surface in bind pose " +
              $"({nearParallelCount}/{finiteCandidateCount} vertices within " +
              $"{threshold:G4}; median nearest distance " +
              $"{medianDistance:G4}). The OpenGL preview draws this run in its " +
              "transparent pass with depth writes disabled, which can conceal " +
              "native depth rejection or z-fighting. This is not proof of the " +
              "game's depth state or native-frame parity."
            : null;

        return new SmoAlphaDecalDepthInfo(
            true,
            surfaceCount,
            opaqueTriangles.Length,
            finiteCandidateCount,
            opaqueDiagonal,
            candidateDiagonalFraction,
            threshold,
            medianDistance,
            nearParallelCount,
            nearParallelFraction,
            risk,
            diagnostic);
    }

    private static SmoAlphaDecalDepthInfo Empty(
        bool isApplicable,
        int candidateVertexCount,
        int opaqueSurfaceCount = 0,
        int opaqueTriangleCount = 0,
        float opaqueBoundsDiagonal = 0.0f) =>
        new(
            isApplicable,
            opaqueSurfaceCount,
            opaqueTriangleCount,
            candidateVertexCount,
            opaqueBoundsDiagonal,
            0.0f,
            0.0f,
            float.PositiveInfinity,
            0,
            0.0f,
            false,
            null);

    private static Triangle[] BuildOpaqueTriangles(
        IReadOnlyList<SmoAlphaDecalOpaqueSurface> opaqueSurfaces,
        out Bounds3 bounds,
        out int surfaceCount)
    {
        var triangles = new List<Triangle>();
        bounds = Bounds3.Empty;
        surfaceCount = 0;

        foreach (SmoAlphaDecalOpaqueSurface surface in opaqueSurfaces)
        {
            SmoMesh mesh = surface.Mesh;
            if (!mesh.HasSkinningData || mesh.TriangleIndices.Length < 3)
                continue;

            int before = triangles.Count;
            for (int offset = 0;
                 offset + 2 < mesh.TriangleIndices.Length;
                 offset += 3)
            {
                int ia = checked((int)mesh.TriangleIndices[offset]);
                int ib = checked((int)mesh.TriangleIndices[offset + 1]);
                int ic = checked((int)mesh.TriangleIndices[offset + 2]);
                if ((uint)ia >= (uint)mesh.VertexCount ||
                    (uint)ib >= (uint)mesh.VertexCount ||
                    (uint)ic >= (uint)mesh.VertexCount)
                {
                    continue;
                }

                Vector3 a = Vector3.Transform(
                    mesh.Positions[ia], surface.WorldTransform);
                Vector3 b = Vector3.Transform(
                    mesh.Positions[ib], surface.WorldTransform);
                Vector3 c = Vector3.Transform(
                    mesh.Positions[ic], surface.WorldTransform);
                if (!IsFinite(a) || !IsFinite(b) || !IsFinite(c))
                    continue;

                Vector3 normal = Vector3.Cross(b - a, c - a);
                if (!TryNormalize(ref normal))
                    continue;

                Bounds3 triangleBounds = Bounds3.Empty
                    .Include(a)
                    .Include(b)
                    .Include(c);
                triangles.Add(new Triangle(a, b, c, normal, triangleBounds));
                bounds = bounds.Include(a).Include(b).Include(c);
            }

            if (triangles.Count > before)
                surfaceCount++;
        }

        return triangles.ToArray();
    }

    private static Matrix4x4 CreateNormalTransform(Matrix4x4 worldTransform) =>
        Matrix4x4.Invert(worldTransform, out Matrix4x4 inverse)
            ? Matrix4x4.Transpose(inverse)
            : worldTransform;

    private static float PointTriangleDistanceSquared(
        Vector3 point,
        Vector3 a,
        Vector3 b,
        Vector3 c)
    {
        // Closest-point region tests from Real-Time Collision Detection.
        Vector3 ab = b - a;
        Vector3 ac = c - a;
        Vector3 ap = point - a;
        float d1 = Vector3.Dot(ab, ap);
        float d2 = Vector3.Dot(ac, ap);
        if (d1 <= 0.0f && d2 <= 0.0f)
            return Vector3.DistanceSquared(point, a);

        Vector3 bp = point - b;
        float d3 = Vector3.Dot(ab, bp);
        float d4 = Vector3.Dot(ac, bp);
        if (d3 >= 0.0f && d4 <= d3)
            return Vector3.DistanceSquared(point, b);

        float vc = d1 * d4 - d3 * d2;
        if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
        {
            float v = d1 / (d1 - d3);
            return Vector3.DistanceSquared(point, a + v * ab);
        }

        Vector3 cp = point - c;
        float d5 = Vector3.Dot(ab, cp);
        float d6 = Vector3.Dot(ac, cp);
        if (d6 >= 0.0f && d5 <= d6)
            return Vector3.DistanceSquared(point, c);

        float vb = d5 * d2 - d1 * d6;
        if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
        {
            float w = d2 / (d2 - d6);
            return Vector3.DistanceSquared(point, a + w * ac);
        }

        float va = d3 * d6 - d5 * d4;
        if (va <= 0.0f && d4 - d3 >= 0.0f && d5 - d6 >= 0.0f)
        {
            float w = (d4 - d3) /
                      ((d4 - d3) + (d5 - d6));
            return Vector3.DistanceSquared(point, b + w * (c - b));
        }

        float denominator = 1.0f / (va + vb + vc);
        float faceV = vb * denominator;
        float faceW = vc * denominator;
        Vector3 closest = a + ab * faceV + ac * faceW;
        return Vector3.DistanceSquared(point, closest);
    }

    private static bool TryNormalize(ref Vector3 value)
    {
        float lengthSquared = value.LengthSquared();
        if (!float.IsFinite(lengthSquared) ||
            lengthSquared <= MinimumFiniteScale * MinimumFiniteScale)
        {
            return false;
        }

        value /= MathF.Sqrt(lengthSquared);
        return IsFinite(value);
    }

    private static bool IsFinite(Vector3 value) =>
        float.IsFinite(value.X) &&
        float.IsFinite(value.Y) &&
        float.IsFinite(value.Z);

    private readonly record struct Triangle(
        Vector3 A,
        Vector3 B,
        Vector3 C,
        Vector3 Normal,
        Bounds3 Bounds);

    private readonly record struct Bounds3(Vector3 Minimum, Vector3 Maximum)
    {
        public static Bounds3 Empty => new(
            new Vector3(float.PositiveInfinity),
            new Vector3(float.NegativeInfinity));

        public float Diagonal => IsValid
            ? Vector3.Distance(Minimum, Maximum)
            : 0.0f;

        private bool IsValid =>
            IsFinite(Minimum) &&
            IsFinite(Maximum) &&
            Minimum.X <= Maximum.X &&
            Minimum.Y <= Maximum.Y &&
            Minimum.Z <= Maximum.Z;

        public Bounds3 Include(Vector3 point) => !IsValid
            ? new Bounds3(point, point)
            : new Bounds3(
                Vector3.Min(Minimum, point),
                Vector3.Max(Maximum, point));

        public float DistanceSquaredTo(Vector3 point)
        {
            if (!IsValid)
                return 0.0f;
            float dx = AxisDistance(point.X, Minimum.X, Maximum.X);
            float dy = AxisDistance(point.Y, Minimum.Y, Maximum.Y);
            float dz = AxisDistance(point.Z, Minimum.Z, Maximum.Z);
            return dx * dx + dy * dy + dz * dz;
        }

        private static float AxisDistance(
            float value,
            float minimum,
            float maximum) =>
            value < minimum
                ? minimum - value
                : value > maximum
                    ? value - maximum
                    : 0.0f;
    }
}
