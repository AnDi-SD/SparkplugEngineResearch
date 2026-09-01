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
        int transparentMaterials = 0;
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
            if (!ImportedTextureImageTools.TextureContainsTransparency(texture))
                continue;
            transparentTextures++;
            if (group.Select(item => item.Mesh.MaterialIndex)
                .Distinct()
                .Any(materialIndex =>
                    donor.Materials[materialIndex].AlphaMode ==
                    ImportedMaterialAlphaMode.Blend))
            {
                transparentMaterials++;
            }
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

        if (transparentTextures == 0 ||
            transparentMaterials != transparentTextures ||
            alphaTriangles == 0)
        {
            throw new InvalidOperationException(
                "Automatic Alpha classification did not mark every transparent " +
                "FBX texture material as Blend or find UV geometry sampling its texels.");
        }
        if (!File.ReadAllBytes(donorPath).SequenceEqual(donorBefore))
            throw new InvalidOperationException("Automatic Alpha regression modified the FBX.");
        Console.WriteLine(
            $"AUTO ALPHA FBX REGRESSION PASS: textures={donor.Textures.Count}; " +
            $"transparentTextures={transparentTextures}; alphaTriangles={alphaTriangles}; " +
            $"opaqueTriangles={opaqueTriangles}; blendMaterials={transparentMaterials}");
    }
}
