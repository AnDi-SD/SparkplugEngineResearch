using System.Numerics;
using SmoImporter.Core;

internal static class RigidLevelImportPreparationRegression
{
    public static void Run()
    {
        const int vertexCount = 65_538;
        var positions = new Vector3[vertexCount];
        var normals = new Vector3[vertexCount];
        var uv0 = new Vector2[vertexCount];
        var uv1 = new Vector2[vertexCount];
        var colors = new uint[vertexCount];
        var indices = new uint[vertexCount];
        for (int index = 0; index < vertexCount; index++)
        {
            positions[index] = new Vector3(index, index % 7, index % 11);
            normals[index] = Vector3.Normalize(new Vector3(1, index % 3 + 1, 2));
            uv0[index] = new Vector2(index / (float)vertexCount, index % 13 / 13f);
            uv1[index] = new Vector2(index % 17 / 17f, index / (float)vertexCount);
            colors[index] = 0xFF000000u | checked((uint)index & 0x00FFFFFFu);
            indices[index] = checked((uint)index);
        }

        var sourceMesh = new ImportedMesh(
            "oversized",
            positions,
            normals,
            uv0,
            indices,
            colors,
            MaterialIndex: 0)
        {
            SecondaryTextureCoordinates = uv1
        };
        var source = new ImportedScene(
            [sourceMesh],
            [],
            [new ImportedMaterial("material")]);

        ImportedScene split = SmoLevelRigidImportPreparer.Prepare(source);
        Assert(split.Meshes.Count == 2, "oversized mesh was split into two chunks");
        Assert(split.Meshes.All(mesh => mesh.Positions.Length <= ushort.MaxValue),
            "every chunk stays within UInt16 vertex limits");
        Assert(split.Meshes.All(mesh => mesh.TriangleIndices.All(index =>
            index < mesh.Positions.Length && index <= ushort.MaxValue)),
            "every remapped index stays inside its chunk");
        Assert(split.Meshes.All(mesh => mesh.MaterialIndex == sourceMesh.MaterialIndex),
            "material index is preserved");
        Assert(split.Meshes.All(mesh =>
                mesh.Normals.Length == mesh.Positions.Length &&
                mesh.TextureCoordinates.Length == mesh.Positions.Length &&
                mesh.SecondaryTextureCoordinates.Length == mesh.Positions.Length &&
                mesh.DiffuseColors.Length == mesh.Positions.Length),
            "all supported vertex channels are preserved");
        Assert(split.Meshes.Sum(mesh => mesh.TriangleIndices.Length) == indices.Length,
            "all source triangles are preserved");

        int sourceCorner = 0;
        foreach (ImportedMesh chunk in split.Meshes)
        {
            foreach (uint localIndex in chunk.TriangleIndices)
            {
                int local = checked((int)localIndex);
                Assert(chunk.Positions[local] == positions[sourceCorner],
                    "triangle/position order is preserved");
                Assert(chunk.Normals[local] == normals[sourceCorner],
                    "normal order is preserved");
                Assert(chunk.TextureCoordinates[local] == uv0[sourceCorner],
                    "UV0 order is preserved");
                Assert(chunk.SecondaryTextureCoordinates[local] == uv1[sourceCorner],
                    "UV1 order is preserved");
                Assert(chunk.DiffuseColors[local] == colors[sourceCorner],
                    "vertex-color order is preserved");
                sourceCorner++;
            }
        }
        Assert(sourceCorner == vertexCount, "every source corner was compared");

        ImportedScene repeated = SmoLevelRigidImportPreparer.Prepare(source);
        Assert(Describe(split) == Describe(repeated), "split output is deterministic");
        Assert(split.ImportWarnings.SequenceEqual(repeated.ImportWarnings),
            "split diagnostics are deterministic");

        ImportedScene small = source with
        {
            Meshes = [new ImportedMesh(
                "small", positions[..3], normals[..3], uv0[..3], [0, 1, 2],
                colors[..3], MaterialIndex: 0)
            {
                SecondaryTextureCoordinates = uv1[..3]
            }]
        };
        Assert(ReferenceEquals(small, SmoLevelRigidImportPreparer.Prepare(small)),
            "already safe scenes remain byte-for-byte source objects");

        Console.WriteLine(
            $"RIGID LEVEL PREPARATION PASS: chunks={split.Meshes.Count}; " +
            $"vertices={split.Meshes.Sum(mesh => mesh.Positions.Length):N0}; " +
            $"triangles={split.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3):N0}; " +
            "material/normal/UV0/UV1/color/order/determinism preserved");
    }

    private static string Describe(ImportedScene scene) => string.Join(
        "|",
        scene.Meshes.Select(mesh =>
            $"{mesh.Name}:{mesh.MaterialIndex}:{mesh.Positions.Length}:" +
            string.Join(',', mesh.TriangleIndices)));

    private static void Assert(bool condition, string message)
    {
        if (!condition)
            throw new InvalidOperationException(message);
    }
}
