using System.Diagnostics;
using System.IO;
using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using OpenTK.Graphics.OpenGL4;
using OpenTK.Mathematics;
using OpenTK.Windowing.Common;
using OpenTK.Windowing.Desktop;
using SmoExporter.Core;
using SmoImporter.Core;
using SmoImporter.Gui;
using SmoViewer.Core;
using Vector3 = System.Numerics.Vector3;
using Vector4 = System.Numerics.Vector4;
using Quaternion = System.Numerics.Quaternion;

internal static class ImporterFittingGpuRegression
{
    public static int Run(string source, string output)
    {
        source = Path.GetFullPath(source);
        output = Path.GetFullPath(output);
        Directory.CreateDirectory(output);
        var timer = Stopwatch.StartNew();
        int checks = 0;
        string failure = "", sourceHash = "", device = "", version = "";
        int contexts = 0, uploads = 0, readbacks = 0;
        bool passed = false;
        var samples = new List<object>();
        void Check(bool valid, string label)
        {
            ++checks;
            if (!valid) throw new InvalidDataException(label);
        }
        try
        {
            Check(new FileInfo(source).Length <= 4 * 1024 * 1024, "Selected target is bounded to 4 MiB.");
            byte[] bytes = File.ReadAllBytes(source);
            sourceHash = Convert.ToHexString(SHA256.HashData(bytes));
            using var sentinel = new NativeWindow(new NativeWindowSettings
            {
                ClientSize = new Vector2i(16, 16), StartVisible = false, StartFocused = false,
                API = ContextAPI.OpenGL, APIVersion = new Version(3, 3), Profile = ContextProfile.Core,
                Flags = ContextFlags.ForwardCompatible, AutoLoadBindings = true
            });
            sentinel.MakeCurrent();
            using var host = new TargetRigFittingGpuPreview();
            SmoDocument? document = null;
            SmoExportScene? scene = null;
            int builds = 0;
            // Reuse the existing independent identity/root-translation/local-rotation
            // assertions; all vertex production now comes from the actual GUI host.
            TargetRigFittingPreviewRegression.Run(source, (target, targetScene, pose) =>
            {
                document = target;
                scene = targetScene;
                ++builds;
                TargetRigFittingPreviewResult result = host.Build(target, targetScene, pose);
                Check(sentinel.Context.IsCurrent, "Importer host restores the caller's current GLFW context.");
                Check(result.Scene.Meshes.All(mesh => mesh.Normals.Length == 0),
                    "The original-target overlay exposes positions only, with no fabricated posed normals.");
                return result;
            });
            Check(builds >= 3 && host.ContextCreationCount == 1 && host.SceneUploadCount == 1,
                "Identity, root translation and local fitting edits reuse one context and one uploaded source scene.");
            Check(host.ReadbackCount == builds * scene!.Meshes.Count(mesh => mesh.SkinObjectIndex is not null),
                "Every actual skinned target mesh is captured on every fitting edit.");
            device = host.Device;
            version = host.Version;
            samples.Add(new { kind = "actual-target", builds, meshes = host.CachedMeshCount,
                host.ContextCreationCount, host.SceneUploadCount, host.ReadbackCount });

            SmoExportMesh template = scene.Meshes.First(mesh => mesh.SkinObjectIndex is not null);
            // Explicit host fixture, retaining the real target's skin and byte-index
            // layout. Fixed coordinates check half/zero/negative/tiny weights, with
            // no CPU evaluator or assumption that identity may bypass the shader.
            var fixture = template with
            {
                Positions = Enumerable.Repeat(new Vector3(2, 3, 4), 4).ToArray(),
                Normals = [], TextureCoordinates0 = [], TextureCoordinates1 = [], Colors = [],
                BlendWeights = [new(.5f, 0, 0, 0), Vector4.Zero, new(-.5f, 0, 0, 0), new(1e-7f, 0, 0, 0)],
                JointIndices = new Vector4[4], TriangleIndices = [0, 1, 2]
            };
            SmoExportScene fixtureScene = scene with { Meshes = new[] { fixture } };
            TargetRigDefinition rig = TargetRigDefinition.FromSmoDocument(document!);
            TargetRigFittingPoseSnapshot identity = rig.CreateFittingPose().Capture();
            Vector3[] actual = host.Build(document!, fixtureScene, identity).Scene.Meshes.Single().Positions;
            Check(Vector3.Distance(actual[0], new Vector3(1, 1.5f, 2)) < .0001f,
                "Identity fitting uses raw half weights through the shared shader.");
            Check(actual[1] == Vector3.Zero, "Zero total weight produces the shader's zero position.");
            Check(Vector3.Distance(actual[2], new Vector3(-1, -1.5f, -2)) < .0001f,
                "Negative original weights reach the shader unchanged.");
            Check(Vector3.Distance(actual[3], new Vector3(2e-7f, 3e-7f, 4e-7f)) < 1e-10f,
                "Tiny original weights are not discarded at the old CPU epsilon.");
            TargetRigFittingPose translated = rig.CreateFittingPose();
            translated.SetRootTransform(Quaternion.Identity, new Vector3(2, 0, 0));
            Vector3[] moved = host.Build(document!, fixtureScene, translated.Capture()).Scene.Meshes.Single().Positions;
            Check(Vector3.Distance(moved[0], new Vector3(2, 1.5f, 2)) < .0001f &&
                Vector3.Distance(moved[2], new Vector3(-2, -1.5f, -2)) < .0001f,
                "Authored translation and raw weight multiplication use the same production GPU path.");
            Check(host.ContextCreationCount == 1 && host.SceneUploadCount == 2 && host.CachedMeshCount == 1,
                "Changed source scene rebuilds once; subsequent palette edits retain the new geometry.");
            samples.Add(new { kind = "explicit-host-weights", identity = actual.Select(p => new[] { p.X, p.Y, p.Z }),
                translated = moved.Select(p => new[] { p.X, p.Y, p.Z }) });
            int paletteCount = scene.Skins.Single(skin => skin.ObjectIndex == fixture.SkinObjectIndex).JointObjectIndices.Count;
            Check(paletteCount < 32, "The selected actual palette leaves an unbound slot inside the shader's 32-entry array.");
            Vector4[] invalidJoints = fixture.JointIndices.ToArray();
            invalidJoints[0] = new Vector4(paletteCount, 0, 0, 0);
            SmoExportScene invalidScene = fixtureScene with { Meshes = new[] { fixture with { JointIndices = invalidJoints } } };
            bool activeJointRejected = false;
            try { host.Build(document!, invalidScene, identity); }
            catch (InvalidOperationException error) when (error.Message.Contains("active joint", StringComparison.Ordinal))
                { activeJointRejected = true; }
            Check(activeJointRejected, "An active influence cannot read an unbound shader slot outside its actual palette.");
            Vector4[] unusedJoints = fixture.JointIndices.ToArray();
            unusedJoints[0] = new Vector4(0, byte.MaxValue, paletteCount, 0);
            SmoExportScene unusedScene = fixtureScene with { Meshes = new[] { fixture with { JointIndices = unusedJoints } } };
            Check(host.Build(document!, unusedScene, identity).Scene.Meshes.Single().Positions.SequenceEqual(actual),
                "Zero-weight unused byte indices outside the palette remain accepted and preserve GPU positions.");
            int uploadsBeforeClear = host.SceneUploadCount;
            host.Clear();
            Check(host.CachedMeshCount == 0 && sentinel.Context.IsCurrent, "Clear releases source cache and restores the caller context.");
            Check(host.Build(document!, fixtureScene, identity).Scene.Meshes.Single().Positions.SequenceEqual(actual) &&
                host.SceneUploadCount == uploadsBeforeClear + 1 && host.ContextCreationCount == 1,
                "Clear/re-add uploads fresh geometry and retains one context.");
            contexts = host.ContextCreationCount;
            uploads = host.SceneUploadCount;
            readbacks = host.ReadbackCount;
            host.Dispose();
            Check(host.IsDisposed && host.CachedMeshCount == 0 && sentinel.Context.IsCurrent,
                "Dispose destroys the owned hidden context and releases source references.");
            bool rejected = false;
            try { host.Build(document!, fixtureScene, identity); }
            catch (ObjectDisposedException) { rejected = true; }
            Check(rejected, "Disposed hosts reject further GPU work.");
            Check(GL.GetError() == ErrorCode.NoError, "The preserved caller context has no GL error.");
            Check(File.ReadAllBytes(source).SequenceEqual(bytes), "Original SMO bytes remain immutable.");
            passed = true;
        }
        catch (Exception error) { failure = error.ToString(); }
        File.WriteAllText(Path.Combine(output, "report.json"), JsonSerializer.Serialize(new
        {
            status = passed ? "passed" : "failed", checks, source, source_sha256 = sourceHash,
            failure, device, version, contexts, uploads, readbacks, samples,
            seconds = timer.Elapsed.TotalSeconds, peak_working_set_bytes = Process.GetCurrentProcess().PeakWorkingSet64,
            scope = "Original target SMO in an authored fitting pose, using the Importer hidden host and shared production GPU shader; positions only. Authoring optimizer and donor inverse-bake are unchanged."
        }, new JsonSerializerOptions { WriteIndented = true }) + "\n");
        Console.WriteLine($"IMPORTER GPU FITTING: {checks} checks; {(passed ? "PASS" : failure)}");
        return passed ? 0 : 1;
    }
}
