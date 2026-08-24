using System.Numerics;
using SmoImporter.Core;

internal static class AutoAlphaTextureRegression
{
    public static void Run(string donorPath)
    {
        donorPath = Path.GetFullPath(donorPath);
        byte[] donorBefore = File.ReadAllBytes(donorPath);
        ImportedScene donor = ImportedModelReader.ReadGeometryOnly(donorPath);
        if (donor.Textures.Count == 0)
            throw new InvalidOperationException("Alpha FBX regression donor has no texture.");

        var skeleton = new ImportedSkeleton(
            "AutoAlphaRegression",
            ["Root"],
            [Matrix4x4.Identity]);
        int alphaTriangles = 0;
        int opaqueTriangles = 0;
        int transparentTextures = 0;
        foreach (IGrouping<int, (ImportedMesh Mesh, int Key)> group in donor.Meshes
                     .Select((mesh, key) => (Mesh: mesh, Key: key))
                     .Where(item => (uint)item.Mesh.MaterialIndex <
                                    (uint)donor.Materials.Count)
                     .GroupBy(item => donor.Materials[item.Mesh.MaterialIndex]
                         .BaseColorTextureIndex))
        {
            if ((uint)group.Key >= (uint)donor.Textures.Count)
                continue;
            ImportedTexture texture = donor.Textures[group.Key];
            if (!ImportedTextureAtlasRepacker.TextureContainsTransparency(texture))
                continue;
            transparentTextures++;
            SmoSkinnedBranchSourceMesh[] meshes = group.Select(item =>
            {
                ImportedMesh mesh = item.Mesh;
                int vertexCount = mesh.Positions.Length;
                var skinning = new ImportedSkinning(
                    skeleton,
                    Enumerable.Repeat(
                        new ImportedJointIndices(0, 0, 0, 0), vertexCount).ToArray(),
                    Enumerable.Repeat(Vector4.UnitX, vertexCount).ToArray());
                return new SmoSkinnedBranchSourceMesh(
                    item.Key,
                    mesh.Name,
                    mesh.Positions,
                    mesh.Normals,
                    mesh.TextureCoordinates,
                    mesh.DiffuseColors,
                    mesh.TriangleIndices,
                    skinning);
            }).ToArray();
            SmoSkinnedRenderableOpacityPlan opacity =
                SmoSkinnedBranchSplitBuilder.ClassifyRenderables(meshes, texture);
            alphaTriangles += opacity.AlphaTriangleCount;
            opaqueTriangles += opacity.OpaqueBodyTriangleCount;
        }

        if (transparentTextures == 0 || alphaTriangles == 0)
        {
            throw new InvalidOperationException(
                "Automatic Alpha classification did not find UV geometry sampling " +
                "transparent texels in the FBX donor.");
        }
        if (!File.ReadAllBytes(donorPath).SequenceEqual(donorBefore))
            throw new InvalidOperationException("Automatic Alpha regression modified the FBX.");
        Console.WriteLine(
            $"AUTO ALPHA FBX REGRESSION PASS: textures={donor.Textures.Count}; " +
            $"transparentTextures={transparentTextures}; alphaTriangles={alphaTriangles}; " +
            $"opaqueTriangles={opaqueTriangles}");
    }
}
