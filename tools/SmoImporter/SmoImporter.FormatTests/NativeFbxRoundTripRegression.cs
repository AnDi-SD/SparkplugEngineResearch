using System.Numerics;
using SmoExporter.Core;
using SmoImporter.Core;
using SmoViewer.Core;

internal static class NativeFbxRoundTripRegression
{
    public static void Run(string sourcePath)
    {
        sourcePath = Path.GetFullPath(sourcePath);
        string directory = Path.Combine(
            Path.GetTempPath(), "smo-native-fbx-roundtrip-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(directory);
        try
        {
            SmoExportScene source = SmoSceneBuilder.Build(
                SmoDocument.Load(sourcePath),
                new SmoExportOptions(AnimationPaths: null));
            string glbPath = Path.Combine(directory, "reference.glb");
            string fbxPath = Path.Combine(directory, "native.fbx");
            GlbExporter.Export(source, glbPath);
            FbxExporter.Export(source, fbxPath);
            ImportedScene glb = GlbModelReader.Read(glbPath);
            ImportedScene fbx = FbxModelReader.ReadRigid(fbxPath);

            int glbTriangles = glb.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3);
            int fbxTriangles = fbx.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3);
            if (fbxTriangles != glbTriangles)
                throw new InvalidOperationException(
                    $"Native FBX round-trip changed triangle count: GLB={glbTriangles}, FBX={fbxTriangles}.");
            Bounds glbBounds = Bounds.From(glb);
            Bounds fbxBounds = Bounds.From(fbx);
            AssertClose(glbBounds.Minimum, fbxBounds.Minimum, "minimum bounds");
            AssertClose(glbBounds.Maximum, fbxBounds.Maximum, "maximum bounds");
            if (fbx.Meshes.Count(mesh => mesh.Skinning is not null) !=
                glb.Meshes.Count(mesh => mesh.Skinning is not null))
                throw new InvalidOperationException("Native FBX round-trip changed skinned mesh count.");
            if (fbx.Materials.Count != glb.Materials.Count)
                throw new InvalidOperationException(
                    $"Native FBX round-trip changed material count: " +
                    $"GLB={glb.Materials.Count}, FBX={fbx.Materials.Count}.");
            if (fbx.Textures.Count != glb.Textures.Count)
                throw new InvalidOperationException(
                    $"Native FBX round-trip changed texture count: " +
                    $"GLB={glb.Textures.Count}, FBX={fbx.Textures.Count}.");

            Console.WriteLine(
                $"NATIVE FBX ROUNDTRIP PASS: triangles={fbxTriangles}; " +
                $"meshes={fbx.Meshes.Count}; materials={fbx.Materials.Count}; " +
                $"textures={fbx.Textures.Count}; skinned=" +
                fbx.Meshes.Count(mesh => mesh.Skinning is not null));
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    private static void AssertClose(Vector3 expected, Vector3 actual, string owner)
    {
        float scale = MathF.Max(1f, MathF.Max(expected.Length(), actual.Length()));
        float distance = Vector3.Distance(expected, actual);
        if (!float.IsFinite(distance) || distance > scale * 0.0005f)
            throw new InvalidOperationException(
                $"Native FBX round-trip changed {owner}: expected={expected}, actual={actual}.");
    }

    private readonly record struct Bounds(Vector3 Minimum, Vector3 Maximum)
    {
        public static Bounds From(ImportedScene scene)
        {
            Vector3[] positions = scene.Meshes.SelectMany(mesh => mesh.Positions).ToArray();
            if (positions.Length == 0) throw new InvalidOperationException("Imported scene is empty.");
            Vector3 minimum = positions[0];
            Vector3 maximum = positions[0];
            foreach (Vector3 position in positions.Skip(1))
            {
                minimum = Vector3.Min(minimum, position);
                maximum = Vector3.Max(maximum, position);
            }
            return new Bounds(minimum, maximum);
        }
    }
}
