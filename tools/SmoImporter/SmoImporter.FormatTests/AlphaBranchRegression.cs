using System.Numerics;
using SmoImporter.Core;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;

internal static class AlphaBranchRegression
{
public static void RunConnectedComponentClassification()
    {
        const int textureWidth = 32;
        const int textureHeight = 8;
        using var image = new Image<Rgba32>(
            textureWidth,
            textureHeight,
            new Rgba32(byte.MaxValue, byte.MaxValue, byte.MaxValue, byte.MaxValue));
        for (int y = 0; y < 4; y++)
            for (int x = 0; x < 5; x++)
                image[x, y] = new Rgba32(byte.MaxValue, byte.MaxValue, byte.MaxValue, 0);
        using var encoded = new MemoryStream();
        image.SaveAsPng(encoded);
        var texture = new ImportedTexture(
            "component-alpha.png",
            "image/png",
            textureWidth,
            textureHeight,
            encoded.ToArray());

        // Triangles 0 and 1 are one geometric quad, but its shared edge uses
        // duplicated vertices to model a normal OBJ/glTF attribute seam. Only
        // triangle 0 has UVs in the alpha area. Triangles 2..9 form a separate,
        // larger opaque grid in the same imported mesh.
        Vector3[] positions =
        [
            new(-2, 0, 0), new(-1, 0, 0), new(-1, 1, 0),
            new(-1, 0, 0), new(-1, 1, 0), new(-2, 1, 0),
            new(1, 0, 0), new(2, 0, 0), new(3, 0, 0),
            new(1, 1, 0), new(2, 1, 0), new(3, 1, 0),
            new(1, 2, 0), new(2, 2, 0), new(3, 2, 0)
        ];
        Vector2[] textureCoordinates =
        [
            new(0.02f, 0.02f), new(0.08f, 0.02f), new(0.08f, 0.20f),
            new(0.72f, 0.55f), new(0.78f, 0.75f), new(0.72f, 0.75f),
            new(0.72f, 0.55f), new(0.80f, 0.55f), new(0.88f, 0.55f),
            new(0.72f, 0.70f), new(0.80f, 0.70f), new(0.88f, 0.70f),
            new(0.72f, 0.85f), new(0.80f, 0.85f), new(0.88f, 0.85f)
        ];
        uint[] triangleIndices =
        [
            0, 1, 2,
            3, 4, 5,
            6, 7, 10, 6, 10, 9,
            7, 8, 11, 7, 11, 10,
            9, 10, 13, 9, 13, 12,
            10, 11, 14, 10, 14, 13
        ];
        Vector3[] normals = Enumerable.Repeat(Vector3.UnitZ, positions.Length).ToArray();
        uint[] colors = Enumerable.Repeat(uint.MaxValue, positions.Length).ToArray();
        var skeleton = new ImportedSkeleton(
            "component-test",
            ["Root"],
            [Matrix4x4.Identity]);
        var skinning = new ImportedSkinning(
            skeleton,
            Enumerable.Repeat(
                new ImportedJointIndices(0, 0, 0, 0), positions.Length).ToArray(),
            Enumerable.Repeat(Vector4.UnitX, positions.Length).ToArray());
        var source = new SmoSkinnedBranchSourceMesh(
            0,
            "mixed-disconnected",
            positions,
            normals,
            textureCoordinates,
            colors,
            triangleIndices,
            skinning);

        SmoSkinnedBranchSourceMesh isolatedOpaqueTriangle = source with
        {
            Key = 1,
            Name = "isolated-opaque-side-of-seam",
            TriangleIndices = [3, 4, 5]
        };
        SmoSkinnedRenderableOpacityPlan isolated =
            SmoSkinnedBranchSplitBuilder.ClassifyRenderables(
                [isolatedOpaqueTriangle], texture);
        if (isolated.OpaqueBodyTriangleCount != 1 || isolated.IsAlpha(1, 0))
        {
            throw new InvalidOperationException(
                "The opaque side of the synthetic attribute seam unexpectedly " +
                "samples alpha on its own.");
        }

        SmoSkinnedRenderableOpacityPlan automatic =
            SmoSkinnedBranchSplitBuilder.ClassifyRenderables([source], texture);
        if (automatic.AlphaTriangleCount != 2 ||
            automatic.OpaqueBodyTriangleCount != 8 ||
            automatic.OpaqueOverlayTriangleCount != 0 ||
            !automatic.IsAlpha(0, 0) ||
            !automatic.IsAlpha(0, 1) ||
            Enumerable.Range(2, 8).Any(triangle => automatic.GetFamily(0, triangle) !=
                SmoSkinnedRenderableMaterialFamily.OpaqueBody))
        {
            throw new InvalidOperationException(
                "Connected-component alpha classification did not keep the " +
                "large disconnected surface opaque.");
        }

        var importedMesh = new ImportedMesh(
            source.Name,
            positions,
            normals,
            textureCoordinates,
            triangleIndices,
            colors,
            MaterialIndex: 0,
            skinning);
        var scene = new ImportedScene(
            [importedMesh],
            [texture],
            [new ImportedMaterial("component-test", texture.Name, 0)]);
        var opaqueProfile = new SkinnedRenderableMaterialProfile(
            scene,
            [new(0, SkinnedRenderableMaterialMode.OpaqueOverlay)]);
        SmoSkinnedRenderableOpacityPlan opaque =
            SmoSkinnedBranchSplitBuilder.ClassifyRenderables(
                [source], texture, opaqueProfile);
        if (opaque.OpaqueOverlayTriangleCount != 10 ||
            Enumerable.Range(0, 10).Any(triangle => !opaque.IsOpaqueOverlay(0, triangle)))
        {
            throw new InvalidOperationException(
                "Explicit opaque-overlay classification stopped being mesh-level.");
        }

        var transparentProfile = new SkinnedRenderableMaterialProfile(
            scene,
            [new(0, SkinnedRenderableMaterialMode.TransparentSurface)]);
        SmoSkinnedRenderableOpacityPlan transparent =
            SmoSkinnedBranchSplitBuilder.ClassifyRenderables(
                [source], texture, transparentProfile);
        if (transparent.AlphaTriangleCount != 10 ||
            Enumerable.Range(0, 10).Any(triangle => !transparent.IsAlpha(0, triangle)))
        {
            throw new InvalidOperationException(
                "Explicit transparent-surface classification stopped being mesh-level.");
        }

        Vector2[] opaqueUvs =
            [new(0.72f, 0.55f), new(0.88f, 0.55f), new(0.88f, 0.85f), new(0.72f, 0.85f)];
        Vector2[] alphaUvs =
            [new(0.02f, 0.02f), new(0.08f, 0.02f), new(0.08f, 0.20f), new(0.02f, 0.20f)];
        uint[] quad = [0, 1, 2, 0, 2, 3];
        Vector3[] basePositions =
            [new(0, 0, 0), new(1, 0, 0), new(1, 1, 0), new(0, 1, 0)];
        Vector3[] closePositions =
            [new(.2f, .2f, .001f), new(.8f, .2f, .001f),
             new(.8f, .8f, .001f), new(.2f, .8f, .001f)];
        Vector3[] farPositions = closePositions
            .Select(value => value + new Vector3(3, 0, 0)).ToArray();
        Vector3 tilted = Vector3.Normalize(new Vector3(.3f, 0, 1));
        SmoSkinnedBranchSourceMesh Base(int key, string name, Vector3[] meshPositions,
            Vector3[] meshNormals, Vector2[] uvs) => new(
            key,
            name,
            meshPositions,
            meshNormals,
            uvs,
            Enumerable.Repeat(uint.MaxValue, 4).ToArray(),
            quad,
            new ImportedSkinning(
                skeleton,
                Enumerable.Repeat(new ImportedJointIndices(0, 0, 0, 0), 4).ToArray(),
                Enumerable.Repeat(Vector4.UnitX, 4).ToArray()));
        SmoSkinnedBranchSourceMesh baseSurface = Base(
            2, "opaque-reference", basePositions,
            Enumerable.Repeat(Vector3.UnitZ, 4).ToArray(), opaqueUvs);
        SmoSkinnedBranchSourceMesh closeOverlay = Base(
            3, "close-alpha-overlay", closePositions,
            Enumerable.Repeat(tilted, 4).ToArray(), alphaUvs);
        SmoSkinnedBranchSourceMesh farSurface = Base(
            4, "far-alpha-surface", farPositions,
            Enumerable.Repeat(tilted, 4).ToArray(), alphaUvs);
        SmoSkinnedBranchSourceMesh[] normalMeshes =
            [baseSurface, closeOverlay, farSurface];
        SmoSkinnedRenderableOpacityPlan normalOpacity =
            SmoSkinnedBranchSplitBuilder.ClassifyRenderables(normalMeshes, texture);
        SmoSurfaceOverlayNormalConformResult conformed =
            SmoSkinnedBranchSplitBuilder.ConformCloseSurfaceNormals(
                normalMeshes, normalOpacity);
        if (conformed.ConformedComponentCount != 1 ||
            conformed.ConformedVertexCount != 4 ||
            conformed.Meshes[1].Normals.Any(value =>
                Vector3.DistanceSquared(value, Vector3.UnitZ) > 0.000001f) ||
            !conformed.Meshes[0].Normals.SequenceEqual(baseSurface.Normals) ||
            !conformed.Meshes[2].Normals.SequenceEqual(farSurface.Normals) ||
            !closeOverlay.Normals.All(value => value == tilted))
        {
            throw new InvalidOperationException(
                "Close-surface normal conformity changed an unrelated surface or " +
                "failed to inherit the opaque reference normals without mutating input.");
        }
    }
}
