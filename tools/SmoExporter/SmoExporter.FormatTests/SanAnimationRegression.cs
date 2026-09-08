using System.Diagnostics;
using System.Numerics;
using System.Reflection;
using System.Security.Cryptography;
using System.Text.Json;
using SmoExporter.Core;
using SmoViewer.Core;

internal static class SanAnimationRegression
{
    internal static int Run(string output, string repository, string nativeReport, bool includeReal = true)
    {
        string directory = Path.GetFullPath(output), root = Path.GetFullPath(repository);
        Directory.CreateDirectory(directory);
        string Hash(string path) => Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path)));
        using JsonDocument native = JsonDocument.Parse(File.ReadAllBytes(nativeReport));
        var cases = new List<(string Name, string Smo, string San, JsonElement? Native)>();
        string icy = Path.Combine(root, "local-data/pc-pristine/Media/Characters/Icy/Icy.smo");
        foreach (JsonElement fixture in native.RootElement.GetProperty("vmd_fixtures").EnumerateArray())
        {
            string san = Path.Combine(Path.GetDirectoryName(nativeReport)!, "vmd-fixtures", fixture.GetProperty("san").GetString()!);
            if (Hash(san) != fixture.GetProperty("sha256").GetString()) throw new InvalidDataException("Native fixture changed.");
            cases.Add((Path.GetFileNameWithoutExtension(san), icy, san, fixture));
        }
        if (includeReal)
        {
        cases.Add(("bbush", Path.Combine(root, "local-data/pc-pristine/Media/SFX/bbush.smo"),
                   Path.Combine(root, "local-data/pc-pristine/Media/Animations/bbush.san"), null));
        cases.Add(("icy-walk", icy, Path.Combine(Path.GetDirectoryName(icy)!, "xiwa.san"), null));
        string knut = Path.Combine(root, "local-data/pc-pristine/Media/Characters/Knut/knut.smo");
        cases.Add(("knut-walk", knut, Path.Combine(Path.GetDirectoryName(knut)!, "Knwa.san"), null));
        }
        var results = new List<object>();
        int bindingChecks = VerifyDuplicateTargets(cases[0].San);
        var references = new Dictionary<string, string>();
        var timer = Stopwatch.StartNew();
        foreach (var test in cases)
        {
            SmoExportScene scene = SmoSceneBuilder.Build(SmoDocument.Load(test.Smo), new SmoExportOptions(AnimationPaths: [test.San]));
            if (scene.Animations.Count != 1) throw new InvalidDataException("Selected SAN was not exported.");
            // Stable probe names only; geometry, transforms, skin and animation
            // all pass through unchanged production exporters.
            scene = scene with
            {
                Meshes = scene.Meshes.Select(mesh => mesh with { Name = "mesh_"+mesh.ObjectIndex }).ToArray(),
                MeshPlacements = scene.MeshPlacements.Select(placement => placement with
                    { Name = "placement_"+placement.SceneObjectIndex }).ToArray()
            };
            string fbx = Path.Combine(directory, test.Name+".fbx"), glb = Path.Combine(directory, test.Name+".glb");
            FbxExporter.Export(scene, fbx);
            GlbExporter.Export(scene, glb);
            if (!references.TryGetValue(test.Smo, out string? reference))
            {
                reference = "source-"+references.Count+".json";
                references.Add(test.Smo, reference);
                File.WriteAllText(Path.Combine(directory, reference), JsonSerializer.Serialize(new
                {
                    smo = Path.GetFullPath(test.Smo), smoSha256 = Hash(test.Smo),
                    nodes = scene.Nodes.Select(node =>
                    {
                        if (!Matrix4x4.Decompose(node.BindLocalMatrix, out Vector3 scale, out Quaternion rotation, out Vector3 position))
                            throw new InvalidDataException("Reference bind local matrix cannot be decomposed.");
                        return new { index = node.ObjectIndex, name = node.Name, parent = node.ParentObjectIndex,
                            position = Vector(position), rotation = QuaternionValues(rotation), scale = Vector(scale),
                            bindWorld = Matrix(node.BindWorldMatrix) };
                    }).ToArray(),
                    placements = scene.MeshPlacements.Select(item => new { name = item.Name, mesh = item.MeshObjectIndex,
                        parent = item.ParentNodeObjectIndex, world = Matrix(item.WorldMatrix) }).ToArray(),
                    skins = scene.Skins.Select(item => new { index = item.ObjectIndex, joints = item.JointObjectIndices,
                        inverseBinds = item.InverseBindMatrices.Select(Matrix).ToArray() }).ToArray(),
                    meshes = scene.Meshes.Select(item => new { index = item.ObjectIndex, skin = item.SkinObjectIndex,
                        positions = item.Positions.Select(Vector).ToArray(), weights = item.BlendWeights.Select(QuaternionValues).ToArray(),
                        joints = item.JointIndices.Select(QuaternionValues).ToArray() }).ToArray()
                }));
            }
            SmoAnimationDecoder.TryDecode(test.San, out SmoAnimationClip? clip, out _);
            var warnings = new List<string>();
            var bound = SmoAnimationBinding.BindByName(clip!.Tracks, warnings);
            float duration = scene.Animations[0].Duration;
            float[] times = test.Native is JsonElement nativeFixture
                ? nativeFixture.GetProperty("samples").EnumerateArray().Select(sample => sample.GetProperty("seconds").GetSingle()).ToArray() :
                test.Name == "bbush" ? Enumerable.Range(0, 31).Select(frame => frame/30f).ToArray() :
                [0, 1f/30, MathF.Floor(duration*15)/30, duration];
            var samples = times.Select((seconds, frame) => new
            {
                seconds,
                receiverSubframeDiagnostic = test.Native is JsonElement diagnosticFixture &&
                    diagnosticFixture.GetProperty("samples")[frame].TryGetProperty("receiverSubframeDiagnostic", out var diagnostic) && diagnostic.GetBoolean(),
                // Rare references are original-PC data, copied without the
                // changed C# sampler. Ordinary/real-scale references exercise
                // the shared sampler already compared independently to PC.
                tracks = test.Native is JsonElement fixture
                    ? (object)fixture.GetProperty("samples")[frame].GetProperty("tracks")
                    : bound.Values.ToDictionary(track => track.NodeName, track =>
                    {
                        SmoAnimationPose pose = track.Sample(seconds, Vector3.Zero, Quaternion.Identity, Vector3.One);
                        var values = new Dictionary<string, float[]>();
                        if (track.Positions.Count > 0) values["2"] = Vector(pose.Position);
                        if (track.Rotations.Count > 0) values["3"] = QuaternionValues(pose.Rotation);
                        if (track.Scales.Count > 0) values["4"] = Vector(pose.Scale);
                        return values;
                    })
            }).ToArray();
            results.Add(new { name = test.Name, fbx = Path.GetFileName(fbx), fbxSha256 = Hash(fbx),
                glb = Path.GetFileName(glb), glbSha256 = Hash(glb), reference,
                referenceSha256 = Hash(Path.Combine(directory, reference)), san = Path.GetFullPath(test.San), sanSha256 = Hash(test.San),
                nativePrsReference = test.Native is not null, samples, warnings = scene.Warnings,
                outputKeys = scene.Animations.Sum(animation => animation.Tracks.Sum(track =>
                    track.Positions.Count+track.Rotations.Count+track.Scales.Count)) });
            Console.WriteLine($"Prepared {test.Name}: production GLB+FBX, {times.Length} poses.");
        }
        File.WriteAllText(Path.Combine(directory, "input.json"), JsonSerializer.Serialize(new
        {
            nativeReportSha256 = Hash(nativeReport), cases = results, framesPerSecond = SmoAnimationBaker.FramesPerSecond,
            bindingChecks,
            elapsedSeconds = timer.Elapsed.TotalSeconds, peakWorkingSet = Process.GetCurrentProcess().PeakWorkingSet64
        }, new JsonSerializerOptions { WriteIndented = true }));
        return 0;
    }

    private static int VerifyDuplicateTargets(string san)
    {
        // Invoke the actual integration method with a deliberately ambiguous
        // node namespace. Both exact targets must survive; case-only must not.
        var nodes = new SmoExportNode[] {
            new(1, "Pelvis", null, Matrix4x4.Identity, Matrix4x4.Identity),
            new(2, "Pelvis", null, Matrix4x4.Identity, Matrix4x4.Identity),
            new(3, "pelvis", null, Matrix4x4.Identity, Matrix4x4.Identity) };
        var warnings = new List<string>();
        var method = typeof(SmoSceneBuilder).GetMethod("BuildAnimations", BindingFlags.Static | BindingFlags.NonPublic)!;
        var animations = (List<SmoExportAnimation>)method.Invoke(null, new object[] { new[] { san }, nodes, warnings })!;
        if (animations.Count != 1 || !animations[0].Tracks.Select(track => track.NodeObjectIndex).SequenceEqual(new[] { 1, 2 }))
            throw new InvalidDataException("SAN duplicate-name binding lost an exact target or matched case-only.");
        if (!ReferenceEquals(animations[0].Tracks[0].Rotations, animations[0].Tracks[1].Rotations))
            throw new InvalidDataException("Duplicate nodes must reuse one baked source curve.");
        if (!warnings.Any(warning => warning.Contains("all 2 matching nodes")))
            throw new InvalidDataException("Duplicate target binding must be reported.");
        return 3;
    }

    private static float[] Vector(Vector3 v) => [v.X, v.Y, v.Z];
    private static float[] QuaternionValues(Quaternion v) => [v.X, v.Y, v.Z, v.W];
    private static float[] QuaternionValues(Vector4 v) => [v.X, v.Y, v.Z, v.W];
    private static float[] Matrix(Matrix4x4 m) => [m.M11,m.M12,m.M13,m.M14,m.M21,m.M22,m.M23,m.M24,
        m.M31,m.M32,m.M33,m.M34,m.M41,m.M42,m.M43,m.M44];
}
