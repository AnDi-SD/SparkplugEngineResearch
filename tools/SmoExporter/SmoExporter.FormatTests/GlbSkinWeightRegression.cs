using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using SmoExporter.Core;
using SmoViewer.Core;

internal static class GlbSkinWeightRegression
{
    public static int Run(string source, string output)
    {
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
            SmoExportScene WithWeight(Vector4 weight)
            {
                var modified = originalWeights.ToArray();
                modified[0] = weight;
                var joints = target.JointIndices.ToArray();
                joints[0] = Vector4.Zero; // A declared valid source palette slot for all four inputs.
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
                string path = Path.Combine(output, sample.Name + ".glb");
                GlbExporter.Export(WithWeight(sample.Weight), path);
                byte[] bytes = File.ReadAllBytes(path);
                Check(bytes.Length > 20 && bytes.AsSpan(0, 4).SequenceEqual("glTF"u8), "Supported GLB output exists.");
                observations.Add(new { sample.Name, authoredWeight = new[] { sample.Weight.X, sample.Weight.Y, sample.Weight.Z, sample.Weight.W },
                    bytes = bytes.Length, sha256 = Convert.ToHexString(SHA256.HashData(bytes)) });
            }
            foreach (var sample in new[] { (Name: "half-sum", Sum: .5f), (Name: "double-sum", Sum: 2f), (Name: "tiny-sum", Sum: 5e-7f) })
            {
                string path = Path.Combine(output, sample.Name + ".glb");
                byte[] previous = "Existing user output must survive an unsupported export."u8.ToArray();
                File.WriteAllBytes(path, previous);
                string? issue = null;
                try { GlbExporter.Export(WithWeight(new Vector4(sample.Sum, 0, 0, 0)), path); }
                catch (InvalidDataException error) { issue = error.Message; }
                observations.Add(new { sample.Name, sample.Sum, issue, outputPreserved = File.ReadAllBytes(path).SequenceEqual(previous) });
                Check(issue?.StartsWith("GLB_SKIN_WEIGHT_SUM:", StringComparison.Ordinal) == true,
                    $"{sample.Name}: materially non-unit weights must be refused instead of normalized.");
                Check(File.ReadAllBytes(path).SequenceEqual(previous), "Unsupported export preserves existing output bytes.");
            }
            Check(target.BlendWeights.SequenceEqual(originalWeights), "Export leaves the source DTO weights unchanged.");
            status = "passed";
            Console.WriteLine($"GLB skin weight boundary: {checks} checks; three supported outputs and three early refusals.");
            return 0;
        }
        catch (Exception error) { failure = error.ToString(); Console.Error.WriteLine(error); return 1; }
        finally
        {
            var report = new { status, checks, source,
                source_sha256 = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(source))),
                core_sha256 = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(typeof(GlbExporter).Assembly.Location))),
                seconds = timer.Elapsed.TotalSeconds, failure, observations,
                scope = "Real source scene with one transient weight edit; no SMO/asset writes, no GLB conversion of non-unit sums." };
            File.WriteAllText(Path.Combine(output, "report.json"), JsonSerializer.Serialize(report,
                new JsonSerializerOptions { WriteIndented = true }) + "\n");
        }
    }
}
