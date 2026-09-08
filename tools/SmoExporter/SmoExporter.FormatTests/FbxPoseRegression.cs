using System.Security.Cryptography;
using System.Numerics;
using System.Text.Json;
using SmoExporter.Core;
using SmoViewer.Core;

internal static class FbxPoseRegression
{
    public static int Run(string output, string smo, string san)
    {
        string directory = Path.GetFullPath(output);
        Directory.CreateDirectory(directory);
        var scene = SmoSceneBuilder.Build(SmoDocument.Load(smo), new SmoExportOptions(AnimationPaths: [san]));
        if (scene.Animations.Count != 1 || scene.Skins.Count == 0)
            throw new InvalidDataException("Select one real skinned model and its SAN animation.");
        scene = scene with
        {
            Meshes = scene.Meshes.Select(mesh => mesh with { Name = "mesh_" + mesh.ObjectIndex }).ToArray(),
            MeshPlacements = scene.MeshPlacements.Select(placement => placement with
                { Name = "placement_" + placement.SceneObjectIndex }).ToArray()
        };
        string Hash(string path) => Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path)));
        var pairs = new List<object>();
        foreach (bool animated in new[] { false, true })
        {
            string name = animated ? "motion" : "bind";
            var variant = animated ? scene : scene with { Animations = [] };
            string fbx = Path.Combine(directory, name + ".fbx");
            string glb = Path.Combine(directory, name + ".glb");
            float duration = scene.Animations[0].Duration;
            float[] times = animated ? [0f, 1f / 30, MathF.Floor(duration * 15) / 30, duration] : [0f];
            FbxExporter.Export(variant, fbx);
            // Blender imports quaternion LINEAR as four component F-curves,
            // producing NLERP between keys. Insert independent .NET SLERP
            // values at the probe times in the test reference ONLY, so that
            // this comparison measures the FBX exporter, not that importer.
            var reference = variant with { Animations = variant.Animations.Select(animation =>
                animation with { Tracks = animation.Tracks.Select(track => track with
                    { Rotations = InsertProbeKeys(track.Rotations, times) }).ToArray() }).ToArray() };
            GlbExporter.Export(reference, glb);
            pairs.Add(new
            {
                fbx = Path.GetFileName(fbx), fbxSha256 = Hash(fbx),
                glb = Path.GetFileName(glb), glbSha256 = Hash(glb),
                times,
                referenceKind = "Test GLB with System.Numerics SLERP keys inserted at probe times; production GLB unchanged.",
                placements = scene.MeshPlacements.Select(item => item.Name).ToArray()
            });
        }
        File.WriteAllText(Path.Combine(directory, "pairs.json"), JsonSerializer.Serialize(new
        {
            schemaVersion = 1, smo = Path.GetFullPath(smo), smoSha256 = Hash(smo),
            san = Path.GetFullPath(san), sanSha256 = Hash(san),
            duration = scene.Animations[0].Duration, pairs
        }, new JsonSerializerOptions { WriteIndented = true }));
        Console.WriteLine("Prepared FBX/GLB bind and SAN motion; independent pose comparison pending.");
        return 0;
    }

    private static IReadOnlyList<SmoAnimationKey<Quaternion>> InsertProbeKeys(
        IReadOnlyList<SmoAnimationKey<Quaternion>> keys, float[] times)
    {
        if (keys.Count == 0) return keys;
        var result = keys.ToDictionary(key => key.Time, key => key.Value);
        foreach (float time in times)
        {
            if (result.ContainsKey(time)) continue;
            Quaternion value;
            if (time <= keys[0].Time) value = keys[0].Value;
            else if (time >= keys[^1].Time) value = keys[^1].Value;
            else
            {
                int right = 1;
                while (keys[right].Time < time) right++;
                var a = keys[right - 1];
                var b = keys[right];
                value = Quaternion.Normalize(Quaternion.Slerp(
                    Quaternion.Normalize(a.Value), Quaternion.Normalize(b.Value),
                    (time - a.Time) / (b.Time - a.Time)));
            }
            result.Add(time, value);
        }
        return result.OrderBy(item => item.Key)
            .Select(item => new SmoAnimationKey<Quaternion>(item.Key, item.Value)).ToArray();
    }
}
