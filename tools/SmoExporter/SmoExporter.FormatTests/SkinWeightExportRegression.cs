using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using SmoExporter.Core;
using SmoViewer.Core;

internal static class SkinWeightExportRegression
{
    public static int Run(string source, string output, string format = "GLB", string? nativeBridge = null)
    {
        bool fbx = format == "FBX";
        string extension = fbx ? ".fbx" : ".glb";
        void Export(SmoExportScene scene, string path)
        {
            if (fbx) FbxExporter.Export(scene, path, nativeBridge);
            else GlbExporter.Export(scene, path);
        }
        source = Path.GetFullPath(source);
        output = Path.GetFullPath(output);
        Directory.CreateDirectory(output);
        int checks = 0;
        var observations = new List<object>();
        string status = "failed", failure = "";
        void Check(bool condition, string message)
        {
            ++checks;
            if (!condition) throw new InvalidDataException(message);
        }
        var timer = System.Diagnostics.Stopwatch.StartNew();
        try
        {
            var scene = SmoExporter.Core.SmoSceneBuilder.Build(SmoDocument.Load(source),
                new SmoExportOptions(Resources: SmoExportResourceTypes.Meshes | SmoExportResourceTypes.Skeleton));
            var target = scene.Meshes.First(mesh => mesh.SkinObjectIndex.HasValue && mesh.BlendWeights.Length > 0);
            var originalWeights = target.BlendWeights.ToArray();
            SmoExportScene WithWeight(Vector4 weight, Vector4? jointOverride = null)
            {
                var modified = originalWeights.ToArray();
                modified[0] = weight;
                var joints = target.JointIndices.ToArray();
                joints[0] = jointOverride ?? Vector4.Zero;
                return scene with { Meshes = scene.Meshes.Select(mesh => ReferenceEquals(mesh, target)
                    ? mesh with { BlendWeights = modified, JointIndices = joints } : mesh).ToArray() };
            }
            foreach (var sample in new[]
            {
                (Name: "unit", Weight: new Vector4(1, 0, 0, 0)),
                (Name: "unit-pair", Weight: new Vector4(.25f, .75f, 0, 0)),
                (Name: "near-unit", Weight: new Vector4(1.00005f, 0, 0, 0))
            })
            {
                string path = Path.Combine(output, sample.Name + extension);
                Export(WithWeight(sample.Weight), path);
                byte[] bytes = File.ReadAllBytes(path);
                Check(bytes.Length > 27 && (fbx ? bytes.AsSpan(0, 18).SequenceEqual("Kaydara FBX Binary"u8) :
                    bytes.AsSpan(0, 4).SequenceEqual("glTF"u8)), "Supported output exists.");
                observations.Add(new { sample.Name, authoredWeight = new[] { sample.Weight.X, sample.Weight.Y, sample.Weight.Z, sample.Weight.W },
                    bytes = bytes.Length, sha256 = Convert.ToHexString(SHA256.HashData(bytes)) });
            }
            foreach (var sample in new[] { (Name: "half-sum", Sum: .5f), (Name: "double-sum", Sum: 2f), (Name: "tiny-sum", Sum: 5e-7f) })
            {
                string path = Path.Combine(output, sample.Name + extension);
                byte[] previous = "Existing user output must survive an unsupported export."u8.ToArray();
                File.WriteAllBytes(path, previous);
                string? issue = null;
                try { Export(WithWeight(new Vector4(sample.Sum, 0, 0, 0)), path); }
                catch (Exception error) when (error is InvalidDataException or InvalidOperationException) { issue = error.Message; }
                observations.Add(new { sample.Name, sample.Sum, issue, outputPreserved = File.ReadAllBytes(path).SequenceEqual(previous) });
                Check(issue?.Contains(format + "_SKIN_WEIGHT_SUM:", StringComparison.Ordinal) == true,
                    $"{sample.Name}: materially non-unit weights must be refused instead of normalized.");
                Check(File.ReadAllBytes(path).SequenceEqual(previous), "Unsupported export preserves existing output bytes.");
            }
            if (fbx)
            {
                foreach (var sample in new[]
                {
                    (Name: "zero-sum", Weight: Vector4.Zero, Joint: Vector4.Zero, Code: "FBX_SKIN_WEIGHT_SUM:"),
                    (Name: "negative-unit", Weight: new Vector4(1.1f, -.1f, 0, 0), Joint: Vector4.Zero, Code: "FBX_SKIN_INFLUENCE:"),
                    (Name: "nonfinite-weight", Weight: new Vector4(float.PositiveInfinity, 0, 0, 0), Joint: Vector4.Zero, Code: "FBX_SKIN_INFLUENCE:"),
                    (Name: "fractional-joint", Weight: Vector4.UnitX, Joint: new Vector4(.5f, 0, 0, 0), Code: "FBX_SKIN_INFLUENCE:")
                })
                {
                    string path = Path.Combine(output, sample.Name + extension);
                    byte[] previous = "Existing user output must survive an unsupported export."u8.ToArray();
                    File.WriteAllBytes(path, previous);
                    string? issue = null;
                    try { Export(WithWeight(sample.Weight, sample.Joint), path); }
                    catch (InvalidOperationException error) { issue = error.Message; }
                    Check(issue?.Contains(sample.Code, StringComparison.Ordinal) == true, sample.Name + " has an explicit target-format refusal.");
                    Check(File.ReadAllBytes(path).SequenceEqual(previous), "Invalid influence preserves destination.");
                    observations.Add(new { sample.Name, issue, outputPreserved = true });
                }
                var geometryOnly = WithWeight(new Vector4(.5f, 0, 0, 0)) with { Resources = SmoExportResourceTypes.Meshes };
                string geometryPath = Path.Combine(output, "geometry-only.fbx");
                Export(geometryOnly, geometryPath);
                Check(new FileInfo(geometryPath).Length > 27, "Disabled skeleton does not validate unused skin weights.");
            }
            Check(target.BlendWeights.SequenceEqual(originalWeights), "Export leaves the source DTO weights unchanged.");
            status = "passed";
            Console.WriteLine($"{format} skin weight boundary: {checks} checks; supported outputs and explicit early refusals.");
            return 0;
        }
        catch (Exception error) { failure = error.ToString(); Console.Error.WriteLine(error); return 1; }
        finally
        {
            var report = new { status, checks, source, format,
                source_sha256 = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(source))),
                core_sha256 = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(typeof(GlbExporter).Assembly.Location))),
                native_bridge_sha256 = fbx ? Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(
                    NativeFbxBridge.ResolveExecutable(nativeBridge)!))) : null,
                seconds = timer.Elapsed.TotalSeconds, failure, observations,
                scope = "Real source scene with one transient weight edit; no SMO/asset writes, no conversion of non-unit sums." };
            File.WriteAllText(Path.Combine(output, "report.json"), JsonSerializer.Serialize(report,
                new JsonSerializerOptions { WriteIndented = true }) + "\n");
        }
    }
}
