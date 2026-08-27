using System.Collections.ObjectModel;
using System.Numerics;

namespace SmoLVLcreator.Core;

public sealed record SmoGeneratedCollisionMesh(
    IReadOnlyList<Vector3> Positions,
    IReadOnlyList<int> TriangleIndices)
{
    public int TriangleCount => TriangleIndices.Count / 3;
}

/// <summary>
/// Builds a bounded convex k-DOP around render geometry. Unlike an AABB it
/// follows the broad silhouette, while the number of support planes gives a
/// strict upper bound for the triangulated collision mesh.
/// </summary>
public static class SmoCollisionHullGenerator
{
    public const int DefaultTriangleBudget = 64;
    public const int MinimumTriangleBudget = 12;

    public static SmoGeneratedCollisionMesh Generate(
        IEnumerable<Vector3> sourcePositions,
        int triangleBudget = DefaultTriangleBudget,
        float? padding = null)
    {
        ArgumentNullException.ThrowIfNull(sourcePositions);
        if (triangleBudget < MinimumTriangleBudget)
        {
            throw new ArgumentOutOfRangeException(
                nameof(triangleBudget),
                triangleBudget,
                $"Collision budget must be at least {MinimumTriangleBudget} triangles.");
        }

        Vector3[] points = sourcePositions
            .Where(IsFinite)
            .Distinct()
            .ToArray();
        if (points.Length == 0)
            throw new ArgumentException("Collision source contains no finite vertices.");

        Vector3 minimum = points.Aggregate(Vector3.Min);
        Vector3 maximum = points.Aggregate(Vector3.Max);
        float diagonal = MathF.Max(Vector3.Distance(minimum, maximum), 0.001f);
        float resolvedPadding = padding ?? MathF.Max(diagonal * 0.005f, 0.05f);
        if (!float.IsFinite(resolvedPadding) || resolvedPadding < 0)
        {
            throw new ArgumentOutOfRangeException(
                nameof(padding),
                padding,
                "Collision padding must be a finite non-negative number.");
        }

        // A convex polyhedron with F planes has at most 4F-12 triangles after
        // its polygonal faces are triangulated. Keeping the plane count below
        // this value makes the requested budget a hard limit.
        int planeCount = Math.Clamp((triangleBudget + 12) / 4, 6, 62);
        if ((planeCount & 1) != 0)
            planeCount--;

        while (planeCount >= 6)
        {
            Vector3[] normals = CreateBalancedNormals(planeCount);
            SmoGeneratedCollisionMesh mesh = BuildKDop(
                points,
                normals,
                resolvedPadding);
            if (mesh.TriangleCount <= triangleBudget)
                return mesh;
            planeCount -= 2;
        }

        throw new InvalidOperationException(
            "Could not construct a collision hull inside the triangle budget.");
    }

    private static SmoGeneratedCollisionMesh BuildKDop(
        IReadOnlyList<Vector3> points,
        IReadOnlyList<Vector3> normals,
        float padding)
    {
        float[] distances = normals
            .Select(normal => points.Max(point => Vector3.Dot(point, normal)) + padding)
            .ToArray();
        float scale = MathF.Max(
            distances.Select(MathF.Abs).DefaultIfEmpty(1).Max(),
            padding);
        float insideTolerance = MathF.Max(scale * 0.00002f, 0.0001f);
        float mergeTolerance = MathF.Max(scale * 0.00001f, 0.00005f);

        var vertices = new List<Vector3>();
        for (int a = 0; a < normals.Count - 2; a++)
        for (int b = a + 1; b < normals.Count - 1; b++)
        for (int c = b + 1; c < normals.Count; c++)
        {
            if (!TryIntersectPlanes(
                    normals[a], distances[a],
                    normals[b], distances[b],
                    normals[c], distances[c],
                    out Vector3 intersection))
            {
                continue;
            }
            if (normals.Where((normal, index) =>
                    Vector3.Dot(normal, intersection) >
                    distances[index] + insideTolerance).Any())
            {
                continue;
            }
            if (vertices.Any(vertex =>
                    Vector3.DistanceSquared(vertex, intersection) <=
                    mergeTolerance * mergeTolerance))
            {
                continue;
            }
            vertices.Add(intersection);
        }

        if (vertices.Count < 4)
            throw new InvalidOperationException("Collision hull is degenerate.");

        var indices = new List<int>();
        for (int plane = 0; plane < normals.Count; plane++)
        {
            Vector3 normal = normals[plane];
            int[] face = vertices
                .Select((vertex, index) => (vertex, index))
                .Where(item => MathF.Abs(
                    Vector3.Dot(normal, item.vertex) - distances[plane]) <=
                    insideTolerance * 2)
                .Select(item => item.index)
                .ToArray();
            if (face.Length < 3)
                continue;

            Vector3 center = face
                .Select(index => vertices[index])
                .Aggregate(Vector3.Zero, (sum, value) => sum + value) / face.Length;
            Vector3 reference = MathF.Abs(normal.Y) < 0.8f
                ? Vector3.UnitY
                : Vector3.UnitX;
            Vector3 axisU = Vector3.Normalize(Vector3.Cross(reference, normal));
            Vector3 axisV = Vector3.Cross(normal, axisU);
            int[] ordered = face.OrderBy(index =>
            {
                Vector3 relative = vertices[index] - center;
                return MathF.Atan2(
                    Vector3.Dot(relative, axisV),
                    Vector3.Dot(relative, axisU));
            }).ToArray();
            ordered = RemoveCollinearVertices(ordered, vertices, mergeTolerance);
            for (int index = 1; index + 1 < ordered.Length; index++)
            {
                Vector3 ab = vertices[ordered[index]] - vertices[ordered[0]];
                Vector3 ac = vertices[ordered[index + 1]] - vertices[ordered[0]];
                if (Vector3.Cross(ab, ac).LengthSquared() <=
                    mergeTolerance * mergeTolerance)
                {
                    continue;
                }
                indices.Add(ordered[0]);
                indices.Add(ordered[index]);
                indices.Add(ordered[index + 1]);
            }
        }

        if (indices.Count < 12 || indices.Count % 3 != 0)
            throw new InvalidOperationException("Collision hull has no closed triangle surface.");
        return new SmoGeneratedCollisionMesh(
            new ReadOnlyCollection<Vector3>(vertices),
            new ReadOnlyCollection<int>(indices));
    }

    private static Vector3[] CreateBalancedNormals(int planeCount)
    {
        var pairs = new List<Vector3>
        {
            Vector3.UnitX,
            Vector3.UnitY,
            Vector3.UnitZ
        };
        var candidates = new List<Vector3>();
        for (int x = -3; x <= 3; x++)
        for (int y = -3; y <= 3; y++)
        for (int z = -3; z <= 3; z++)
        {
            if (x == 0 && y == 0 && z == 0)
                continue;
            var value = new Vector3(x, y, z);
            value = Canonicalize(Vector3.Normalize(value));
            if (pairs.Concat(candidates).Any(candidate =>
                    MathF.Abs(Vector3.Dot(candidate, value)) > 0.9999f))
            {
                continue;
            }
            candidates.Add(value);
        }

        int requestedPairs = planeCount / 2;
        while (pairs.Count < requestedPairs)
        {
            Vector3 selected = candidates
                .OrderBy(candidate => pairs.Max(existing =>
                    MathF.Abs(Vector3.Dot(existing, candidate))))
                .ThenBy(candidate => candidate.X)
                .ThenBy(candidate => candidate.Y)
                .ThenBy(candidate => candidate.Z)
                .First();
            pairs.Add(selected);
            candidates.Remove(selected);
        }

        return pairs.SelectMany(normal => new[] { normal, -normal }).ToArray();
    }

    private static int[] RemoveCollinearVertices(
        int[] face,
        IReadOnlyList<Vector3> vertices,
        float tolerance)
    {
        var result = face.ToList();
        bool changed = true;
        while (changed && result.Count > 3)
        {
            changed = false;
            for (int index = 0; index < result.Count; index++)
            {
                Vector3 previous = vertices[result[(index + result.Count - 1) % result.Count]];
                Vector3 current = vertices[result[index]];
                Vector3 next = vertices[result[(index + 1) % result.Count]];
                if (Vector3.Cross(current - previous, next - current).Length() > tolerance)
                    continue;
                result.RemoveAt(index);
                changed = true;
                break;
            }
        }
        return result.ToArray();
    }

    private static bool TryIntersectPlanes(
        Vector3 firstNormal,
        float firstDistance,
        Vector3 secondNormal,
        float secondDistance,
        Vector3 thirdNormal,
        float thirdDistance,
        out Vector3 intersection)
    {
        Vector3 secondCrossThird = Vector3.Cross(secondNormal, thirdNormal);
        float denominator = Vector3.Dot(firstNormal, secondCrossThird);
        if (MathF.Abs(denominator) < 0.000001f)
        {
            intersection = default;
            return false;
        }
        intersection = (
            firstDistance * secondCrossThird +
            secondDistance * Vector3.Cross(thirdNormal, firstNormal) +
            thirdDistance * Vector3.Cross(firstNormal, secondNormal)) / denominator;
        return IsFinite(intersection);
    }

    private static Vector3 Canonicalize(Vector3 value)
    {
        if (value.X < -0.00001f ||
            MathF.Abs(value.X) <= 0.00001f && value.Y < -0.00001f ||
            MathF.Abs(value.X) <= 0.00001f &&
            MathF.Abs(value.Y) <= 0.00001f && value.Z < 0)
        {
            return -value;
        }
        return value;
    }

    private static bool IsFinite(Vector3 value) =>
        float.IsFinite(value.X) &&
        float.IsFinite(value.Y) &&
        float.IsFinite(value.Z);
}
