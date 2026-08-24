using System.Numerics;
using SmoImporter.Core;

internal static class GlbNormalRepairRegression
{
    public static void Run()
    {
        Vector3[] positions =
        [
            new(0, 0, 0),
            new(1, 0, 0),
            new(1, 1, 0),
            new(0, 1, 0),
            new(2, 2, 2)
        ];
        Vector3[] sourceNormals =
        [
            new(float.NaN, 0, 0),
            Vector3.UnitZ,
            Vector3.Zero,
            Vector3.UnitZ,
            new(float.PositiveInfinity, 0, 0)
        ];
        uint[] triangles = [0, 1, 2, 0, 2, 3];

        Vector3[] repaired = GlbModelReader.RepairInvalidNormals(
            positions,
            sourceNormals,
            triangles,
            out int reconstructed,
            out int fallback);

        if (reconstructed != 2 || fallback != 1 ||
            repaired.Length != sourceNormals.Length ||
            repaired.Any(normal =>
                !float.IsFinite(normal.X) ||
                !float.IsFinite(normal.Y) ||
                !float.IsFinite(normal.Z) ||
                MathF.Abs(normal.LengthSquared() - 1) > 0.000001f) ||
            repaired[0] != Vector3.UnitZ ||
            repaired[2] != Vector3.UnitZ ||
            repaired[4] != Vector3.UnitY ||
            repaired[1] != sourceNormals[1] ||
            repaired[3] != sourceNormals[3] ||
            !float.IsNaN(sourceNormals[0].X) ||
            sourceNormals[2] != Vector3.Zero ||
            !float.IsPositiveInfinity(sourceNormals[4].X))
        {
            throw new InvalidOperationException(
                "GLB invalid-normal repair did not preserve valid normals, " +
                "reconstruct referenced normals, and isolate the safe fallback.");
        }

        Console.WriteLine(
            "GLB NORMAL REPAIR REGRESSION PASS: 2 triangle-reconstructed, " +
            "1 unreferenced fallback, valid inputs preserved, source immutable.");
    }
}
