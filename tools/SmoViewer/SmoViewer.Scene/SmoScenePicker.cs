using SmoViewer.Core;
using System.Numerics;

namespace SmoViewer.Scene;

/// <summary>A normalized ray expressed in native SMO world coordinates.</summary>
public readonly record struct SmoPickRay
{
    public SmoPickRay(Vector3 origin, Vector3 direction)
    {
        if (!IsFinite(origin) || !IsFinite(direction) ||
            direction.LengthSquared() < 1e-12f)
        {
            throw new ArgumentException("A picking ray needs finite coordinates and a direction.");
        }

        Origin = origin;
        Direction = Vector3.Normalize(direction);
    }

    public Vector3 Origin { get; }
    public Vector3 Direction { get; }

    private static bool IsFinite(Vector3 value) =>
        float.IsFinite(value.X) &&
        float.IsFinite(value.Y) &&
        float.IsFinite(value.Z);
}

public sealed record SmoSceneHit(
    SmoSceneMesh SceneMesh,
    float Distance,
    Vector3 Position,
    int TriangleIndex,
    Vector3 BarycentricCoordinates);

/// <summary>
/// Reusable CPU acceleration data for editor/viewer selection. Geometry stays
/// in native SMO coordinates; viewport adapters are responsible for converting
/// their camera convention to an <see cref="SmoPickRay"/>.
/// </summary>
public sealed class SmoScenePickIndex
{
    private readonly PickEntry[] _entries;

    /// <param name="positionProvider">
    /// Current local positions produced by the rendering backend for a skinned
    /// occurrence. Null means unavailable. The index captures a fixed snapshot;
    /// callers rebuild it when the rendered pose changes.
    /// </param>
    public SmoScenePickIndex(
        IEnumerable<SmoSceneMesh> meshes,
        Func<SmoSceneMesh, Vector3[]?>? positionProvider = null)
    {
        ArgumentNullException.ThrowIfNull(meshes);
        var issues = new List<string>();
        _entries = meshes
            .Select(mesh => CreateEntry(mesh, positionProvider, issues))
            .Where(entry => entry is not null)
            .Cast<PickEntry>()
            .ToArray();
        Issues = issues.AsReadOnly();
    }

    public int Count => _entries.Length;
    public IReadOnlyList<string> Issues { get; }

    public SmoSceneHit? Pick(
        SmoPickRay ray,
        Predicate<SmoSceneMesh>? include = null)
    {
        PickEntry? closestEntry = null;
        float closestDistance = float.PositiveInfinity;
        int closestTriangle = -1;
        Vector3 closestBarycentric = default;

        foreach (PickEntry entry in _entries)
        {
            if (include is not null && !include(entry.SceneMesh))
                continue;

            Vector3 localOrigin = Vector3.Transform(ray.Origin, entry.InverseWorld);
            Vector3 localDirection = Vector3.TransformNormal(
                ray.Direction, entry.InverseWorld);
            if (!SmoPickingMath.IntersectsBounds(
                    localOrigin,
                    localDirection,
                    entry.Minimum,
                    entry.Maximum,
                    closestDistance))
            {
                continue;
            }

            uint[] indices = entry.SceneMesh.Mesh.TriangleIndices;
            for (int offset = 0; offset + 2 < indices.Length; offset += 3)
            {
                int ia = checked((int)indices[offset]);
                int ib = checked((int)indices[offset + 1]);
                int ic = checked((int)indices[offset + 2]);
                if ((uint)ia >= (uint)entry.Positions.Length ||
                    (uint)ib >= (uint)entry.Positions.Length ||
                    (uint)ic >= (uint)entry.Positions.Length ||
                    !SmoPickingMath.TryIntersectTriangle(
                        localOrigin,
                        localDirection,
                        entry.Positions[ia],
                        entry.Positions[ib],
                        entry.Positions[ic],
                        out float distance,
                        out Vector3 barycentric) ||
                    distance >= closestDistance)
                {
                    continue;
                }

                closestEntry = entry;
                closestDistance = distance;
                closestTriangle = offset / 3;
                closestBarycentric = barycentric;
            }
        }

        return closestEntry is null
            ? null
            : new SmoSceneHit(
                closestEntry.SceneMesh,
                closestDistance,
                ray.Origin + ray.Direction * closestDistance,
                closestTriangle,
                closestBarycentric);
    }

    private static PickEntry? CreateEntry(
        SmoSceneMesh sceneMesh,
        Func<SmoSceneMesh, Vector3[]?>? positionProvider,
        List<string> issues)
    {
        if (sceneMesh.Mesh.Positions.Length == 0 ||
            sceneMesh.Mesh.TriangleIndices.Length < 3 ||
            !Matrix4x4.Invert(sceneMesh.WorldTransform, out Matrix4x4 inverseWorld))
        {
            return null;
        }

        Vector3[] positions = sceneMesh.Mesh.Positions;
        if (sceneMesh.Mesh.HasSkinningData)
        {
            Vector3[]? provided = positionProvider?.Invoke(sceneMesh);
            if (provided is null || provided.Length != sceneMesh.Mesh.VertexCount ||
                provided.Any(value => !float.IsFinite(value.X) ||
                    !float.IsFinite(value.Y) || !float.IsFinite(value.Z)))
            {
                issues.Add($"SKIN_PICKING_POSITIONS_UNAVAILABLE: scene object " +
                    $"[{sceneMesh.SceneObjectIndex}], mesh [{sceneMesh.Mesh.ObjectIndex}] " +
                    "requires a complete finite position snapshot from the rendering backend.");
                return null;
            }
            // Own the snapshot so later provider writes cannot invalidate bounds.
            positions = (Vector3[])provided.Clone();
        }
        Vector3 minimum = new(float.PositiveInfinity);
        Vector3 maximum = new(float.NegativeInfinity);
        foreach (Vector3 position in positions)
        {
            minimum = Vector3.Min(minimum, position);
            maximum = Vector3.Max(maximum, position);
        }

        return new PickEntry(
            sceneMesh,
            positions,
            inverseWorld,
            minimum,
            maximum);
    }

    private sealed record PickEntry(
        SmoSceneMesh SceneMesh,
        Vector3[] Positions,
        Matrix4x4 InverseWorld,
        Vector3 Minimum,
        Vector3 Maximum);
}

/// <summary>Low-level intersection helpers shared with future gizmo picking.</summary>
public static class SmoPickingMath
{
    public static bool TryIntersectTriangle(
        SmoPickRay ray,
        Vector3 a,
        Vector3 b,
        Vector3 c,
        out float distance,
        out Vector3 barycentric) =>
        TryIntersectTriangle(
            ray.Origin,
            ray.Direction,
            a,
            b,
            c,
            out distance,
            out barycentric);

    internal static bool TryIntersectTriangle(
        Vector3 origin,
        Vector3 direction,
        Vector3 a,
        Vector3 b,
        Vector3 c,
        out float distance,
        out Vector3 barycentric)
    {
        const float epsilon = 1e-7f;
        Vector3 edge1 = b - a;
        Vector3 edge2 = c - a;
        Vector3 p = Vector3.Cross(direction, edge2);
        float determinant = Vector3.Dot(edge1, p);
        if (MathF.Abs(determinant) <= epsilon)
        {
            distance = 0;
            barycentric = default;
            return false;
        }

        float inverse = 1 / determinant;
        Vector3 t = origin - a;
        float v = Vector3.Dot(t, p) * inverse;
        if (v < 0 || v > 1)
        {
            distance = 0;
            barycentric = default;
            return false;
        }

        Vector3 q = Vector3.Cross(t, edge1);
        float w = Vector3.Dot(direction, q) * inverse;
        if (w < 0 || v + w > 1)
        {
            distance = 0;
            barycentric = default;
            return false;
        }

        distance = Vector3.Dot(edge2, q) * inverse;
        barycentric = new Vector3(1 - v - w, v, w);
        return distance >= 0;
    }

    internal static bool IntersectsBounds(
        Vector3 origin,
        Vector3 direction,
        Vector3 minimum,
        Vector3 maximum,
        float maximumDistance)
    {
        float near = 0;
        float far = maximumDistance;
        for (int axis = 0; axis < 3; axis++)
        {
            float rayOrigin = GetAxis(origin, axis);
            float rayDirection = GetAxis(direction, axis);
            float min = GetAxis(minimum, axis);
            float max = GetAxis(maximum, axis);
            if (MathF.Abs(rayDirection) < 1e-12f)
            {
                if (rayOrigin < min || rayOrigin > max)
                    return false;
                continue;
            }

            float inverse = 1 / rayDirection;
            float first = (min - rayOrigin) * inverse;
            float second = (max - rayOrigin) * inverse;
            if (first > second)
                (first, second) = (second, first);
            near = MathF.Max(near, first);
            far = MathF.Min(far, second);
            if (near > far)
                return false;
        }
        return far >= 0;
    }

    private static float GetAxis(Vector3 value, int axis) => axis switch
    {
        0 => value.X,
        1 => value.Y,
        _ => value.Z
    };
}
