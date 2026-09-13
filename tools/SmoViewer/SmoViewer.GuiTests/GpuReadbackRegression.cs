using System.Diagnostics;
using System.IO;
using System.Numerics;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text.Json;
using System.Windows.Media;
using System.Windows.Media.Media3D;
using OpenTK.Graphics.OpenGL4;
using OpenTK.Mathematics;
using OpenTK.Windowing.Common;
using OpenTK.Windowing.Desktop;
using SmoViewer.Core;
using SmoViewer.Rendering.Wpf;
using SmoViewer.Scene;
using Vector2 = System.Numerics.Vector2;
using Vector3 = System.Numerics.Vector3;
using Vector4 = System.Numerics.Vector4;

// Tests the shared renderer's actual uploaded geometry and shader readback.
// Fixed synthetic coordinates cover non-unit weights; no CPU skin evaluator.
internal static class GpuReadbackRegression
{
    private static int Handle(SmoGpuSceneRenderer renderer, string name) =>
        (int)typeof(SmoGpuSceneRenderer).GetField(name, BindingFlags.Instance | BindingFlags.NonPublic)!.GetValue(renderer)!;

    private static string PositionHash(Vector3[] values) =>
        Convert.ToHexString(SHA256.HashData(MemoryMarshal.AsBytes(values.AsSpan())));

    private static (bool Visible, bool Highlighted, float Opacity) Appearance(
        SmoGpuSceneRenderer renderer, SmoRenderObjectKey key)
    {
        var values = (System.Collections.IDictionary)typeof(SmoGpuSceneRenderer)
            .GetField("_appearances", BindingFlags.Instance | BindingFlags.NonPublic)!.GetValue(renderer)!;
        object value = values[key]!;
        Type type = value.GetType();
        return ((bool)type.GetProperty("Visible")!.GetValue(value)!,
            (bool)type.GetProperty("Highlighted")!.GetValue(value)!,
            (float)type.GetProperty("Opacity")!.GetValue(value)!);
    }

    private sealed record Bindings(int Program, int VertexArray, int ArrayBuffer,
        int Elements, int FeedbackBuffer, int IndexedBuffer, long Start, long Size, bool Discard)
    {
        public static Bindings Read()
        {
            GL.GetInteger(GetPName.CurrentProgram, out int program);
            GL.GetInteger(GetPName.VertexArrayBinding, out int vao);
            GL.GetInteger(GetPName.ArrayBufferBinding, out int array);
            GL.GetInteger(GetPName.ElementArrayBufferBinding, out int elements);
            GL.GetInteger(GetPName.TransformFeedbackBufferBinding, out int buffer);
            GL.GetInteger(GetIndexedPName.TransformFeedbackBufferBinding, 0, out int indexed);
            GL.GetInteger64(GetIndexedPName.TransformFeedbackBufferStart, 0, out long start);
            GL.GetInteger64(GetIndexedPName.TransformFeedbackBufferSize, 0, out long size);
            return new(program, vao, array, elements, buffer, indexed, start, size,
                GL.IsEnabled(EnableCap.RasterizerDiscard));
        }
    }

    public static int Run(string source, string output)
    {
        source = Path.GetFullPath(source);
        output = Path.GetFullPath(output);
        Directory.CreateDirectory(output);
        string sourceHash = "", device = "", version = "", failure = "";
        string[] decodeIssues = [], textureIssues = [];
        var observations = new List<object>();
        int checks = 0;
        bool passed = false;
        var timer = Stopwatch.StartNew();
        void Check(bool value, string message)
        {
            ++checks;
            if (!value) throw new InvalidDataException(message);
        }
        void Reject<T>(Action action, string label) where T : Exception
        {
            try { action(); }
            catch (T) { Check(true, label); return; }
            Check(false, label);
        }
        try
        {
            Check(new FileInfo(source).Length <= 4 * 1024 * 1024, "Selected source is bounded to 4 MiB.");
            byte[] bytes = File.ReadAllBytes(source);
            sourceHash = Convert.ToHexString(SHA256.HashData(bytes));
            var document = SmoDocument.Parse(bytes, source);
            var prepared = SmoSceneBuilder.Build(document);
            decodeIssues = prepared.DecodeErrors.ToArray();
            textureIssues = prepared.TextureIssues.ToArray();
            SmoSceneMesh? candidate = prepared.Meshes.FirstOrDefault(item =>
                item.ContainerKind != SmoRenderContainerKind.SkyBox &&
                item.Mesh.HasSkinningData && item.InitialSkinMatrices is { Count: > 0 and <= 32 });
            Check(candidate is not null, "Actual prepared scene contains a skinned mesh with a native initial palette.");
            SmoSceneMesh actualMesh = candidate!;
            Check(actualMesh.Mesh.VertexCount <= 65536, "Selected actual mesh stays within the small GL test budget.");
            var key = new SmoRenderObjectKey(0, actualMesh.SceneObjectIndex, actualMesh.OccurrenceKey);
            int meshIndex = actualMesh.Mesh.ObjectIndex;

            using var window = new NativeWindow(new NativeWindowSettings
            {
                ClientSize = new Vector2i(16, 16), StartVisible = false, StartFocused = false,
                API = ContextAPI.OpenGL, APIVersion = new Version(3, 3),
                Profile = ContextProfile.Core, Flags = ContextFlags.ForwardCompatible,
                AutoLoadBindings = true
            });
            window.MakeCurrent();
            device = GL.GetString(StringName.Renderer);
            version = GL.GetString(StringName.Version);
            var renderer = new SmoGpuSceneRenderer();
            renderer.SetAntialiasingSamples(0);
            var camera = new PerspectiveCamera(new Point3D(0, 0, 5),
                new Vector3D(0, 0, -1), new Vector3D(0, 1, 0), 45)
                { NearPlaneDistance = .01, FarPlaneDistance = 1000 };
            void Render() => renderer.Render(16, 16, camera, Colors.Black, 0,
                new Point3D(), false, Colors.Gray, 0, 10, 10);
            renderer.Add(actualMesh, key, Colors.White);
            Reject<InvalidOperationException>(() => renderer.ReadPositionsForPicking(key, meshIndex),
                "Readback rejects geometry which has not been uploaded.");
            Render();
            Bindings before = Bindings.Read();
            Vector3[] initial = renderer.ReadPositionsForPicking(key, meshIndex);
            Check(initial.Length == actualMesh.Mesh.VertexCount && initial.All(p =>
                float.IsFinite(p.X) && float.IsFinite(p.Y) && float.IsFinite(p.Z)),
                "Actual palette readback returns all finite uploaded vertices.");
            Check(Bindings.Read() == before, "Readback restores the render callback's GL state.");
            int program = Handle(renderer, "_pickingProgram"), buffer = Handle(renderer, "_pickingBuffer");
            Check(program != 0 && buffer != 0, "Picking program and buffer exist on the actual context.");
            Vector3[] repeated = renderer.ReadPositionsForPicking(key, meshIndex);
            Check(repeated.SequenceEqual(initial) && Handle(renderer, "_pickingProgram") == program &&
                Handle(renderer, "_pickingBuffer") == buffer, "Repeated readback reuses resources and preserves actual positions.");

            renderer.SetModelTransform(key, Matrix4x4.CreateTranslation(20, 30, 40));
            camera.Position = new Point3D(1, 2, 7);
            Render();
            Check(renderer.ReadPositionsForPicking(key, meshIndex).SequenceEqual(initial),
                "Picking output stays local when the placement and camera change.");
            renderer.SetBoneMatrices(key, []);
            Vector3[] unskinned = renderer.ReadPositionsForPicking(key, meshIndex);
            Check(unskinned.SequenceEqual(actualMesh.Mesh.Positions),
                "Disabling the palette exposes every original uploaded position without coordinate reflection.");
            renderer.SetBoneMatrices(key, actualMesh.InitialSkinMatrices!);
            Check(renderer.ReadPositionsForPicking(key, meshIndex).SequenceEqual(initial),
                "Restoring the actual palette restores its GPU positions.");
            observations.Add(new { kind = "actual-prepared-scene", key.FileIndex, key.ObjectIndex,
                meshIndex, vertices = initial.Length, paletteCount = actualMesh.InitialSkinMatrices!.Count,
                position_sha256 = PositionHash(initial), raw_position_sha256 = PositionHash(unskinned),
                first = initial.Take(3).Select(p => new[] { p.X, p.Y, p.Z }).ToArray() });

            int sentinelVao = GL.GenVertexArray(), sentinelRange = GL.GenBuffer(), sentinelGeneric = GL.GenBuffer();
            try
            {
                GL.BindVertexArray(sentinelVao);
                GL.BindBuffer(BufferTarget.TransformFeedbackBuffer, sentinelRange);
                GL.BufferData(BufferTarget.TransformFeedbackBuffer, 256, IntPtr.Zero, BufferUsageHint.StreamRead);
                GL.BindBufferRange(BufferRangeTarget.TransformFeedbackBuffer, 0, sentinelRange, new IntPtr(16), 128);
                GL.BindBuffer(BufferTarget.TransformFeedbackBuffer, sentinelGeneric);
                GL.BufferData(BufferTarget.TransformFeedbackBuffer, 64, IntPtr.Zero, BufferUsageHint.StreamRead);
                GL.BindBuffer(BufferTarget.ArrayBuffer, sentinelGeneric);
                GL.UseProgram(Handle(renderer, "_program"));
                GL.Enable(EnableCap.RasterizerDiscard);
                Bindings sentinels = Bindings.Read();
                Check(renderer.ReadPositionsForPicking(key, meshIndex).SequenceEqual(initial) &&
                    Bindings.Read() == sentinels,
                    "Nonzero program/VAO/generic+indexed range bindings and enabled raster discard survive capture.");
            }
            finally
            {
                GL.Disable(EnableCap.RasterizerDiscard);
                GL.UseProgram(0);
                GL.BindVertexArray(0);
                GL.BindBuffer(BufferTarget.TransformFeedbackBuffer, 0);
                GL.BindBufferBase(BufferRangeTarget.TransformFeedbackBuffer, 0, 0);
                GL.BindBuffer(BufferTarget.ArrayBuffer, 0);
                GL.DeleteVertexArray(sentinelVao);
                GL.DeleteBuffer(sentinelRange);
                GL.DeleteBuffer(sentinelGeneric);
            }
            Reject<KeyNotFoundException>(() => renderer.ReadPositionsForPicking(new(1, key.ObjectIndex), meshIndex),
                "File identity cannot select another file's uploaded geometry.");
            Reject<KeyNotFoundException>(() => renderer.ReadPositionsForPicking(key, -1),
                "Physical mesh identity must exist for the selected item.");
            renderer.Add(actualMesh, key, Colors.White);
            Reject<InvalidOperationException>(() => renderer.ReadPositionsForPicking(key, meshIndex),
                "Duplicate matching placements are explicitly ambiguous.");
            renderer.Clear();
            Render();
            Check(!GL.IsBuffer(buffer) && Handle(renderer, "_pickingBuffer") == 0 &&
                Handle(renderer, "_pickingProgram") == program && GL.IsProgram(program),
                "Clear/render deletes scene capture storage and retains one linked program.");

            Vector3[] points = [new(2, 3, 4), new(4, 3, 4), new(2, 5, 4), new(4, 5, 6)];
            var transient = SmoMesh.CreateTransient(actualMesh.Mesh, points,
                Enumerable.Repeat(Vector3.UnitZ, 4).ToArray(), Array.Empty<Vector2>(), [],
                Enumerable.Repeat(new Vector4(.5f, 0, 0, 0), 4).ToArray(),
                Enumerable.Repeat(new SmoBlendIndices(0, 0, 0, 0), 4).ToArray(), [0, 1, 2],
                objectIndex: meshIndex);
            SmoSceneMesh fixture = actualMesh with { Mesh = transient, Texture = null, BaseTexture = null,
                InitialSkinMatrices = new[] { Matrix4x4.CreateTranslation(1, 0, 0) } };
            // Explicit host fixture: two support slots share both the renderable
            // and physical mesh. These slots do not claim source-file membership.
            var fixtureKey = new SmoRenderObjectKey(1, key.ObjectIndex, new SmoRenderOccurrenceKey(0, 0));
            var secondKey = new SmoRenderObjectKey(1, key.ObjectIndex, new SmoRenderOccurrenceKey(0, 1));
            renderer.Add(fixture, fixtureKey, Colors.White);
            renderer.Add(fixture with { InitialSkinMatrices = new[] { Matrix4x4.CreateTranslation(9, 0, 0) } },
                secondKey, Colors.White);
            Reject<InvalidOperationException>(() => renderer.ReadPositionsForPicking(fixtureKey, meshIndex),
                "Clear/re-add requires fresh uploads.");
            Render();
            Check(renderer.ConsumeUploadReport() is { GeometryCount: 1, PlacementCount: 2 },
                "Distinct support slots share one physical geometry upload.");
            Reject<KeyNotFoundException>(() => renderer.ReadPositionsForPicking(new(1, key.ObjectIndex), meshIndex),
                "An occurrence-free key cannot select a specific support slot.");
            Vector3[] expected = [new(1.5f, 1.5f, 2), new(2.5f, 1.5f, 2), new(1.5f, 2.5f, 2), new(2.5f, 2.5f, 3)];
            Vector3[] half = renderer.ReadPositionsForPicking(fixtureKey, meshIndex);
            Check(half.SequenceEqual(expected), "Half-weight capture preserves raw shader arithmetic, including an unindexed vertex.");
            Vector3[] second = renderer.ReadPositionsForPicking(secondKey, meshIndex);
            Check(second.SequenceEqual(new Vector3[] { new(5.5f, 1.5f, 2), new(6.5f, 1.5f, 2),
                new(5.5f, 2.5f, 2), new(6.5f, 2.5f, 3) }) &&
                renderer.ReadPositionsForPicking(fixtureKey, meshIndex).SequenceEqual(half),
                "Two support slots sharing the renderable and physical geometry retain their own palettes.");
            renderer.SetBoneMatrices(secondKey, [Matrix4x4.CreateTranslation(17, 0, 0)]);
            Check(renderer.ReadPositionsForPicking(secondKey, meshIndex).SequenceEqual(new Vector3[] {
                    new(9.5f, 1.5f, 2), new(10.5f, 1.5f, 2), new(9.5f, 2.5f, 2), new(10.5f, 2.5f, 3) }) &&
                renderer.ReadPositionsForPicking(fixtureKey, meshIndex).SequenceEqual(half),
                "Updating one support slot's palette leaves the other slot's palette unchanged.");
            renderer.SetAppearance(fixtureKey, false, true, .25f);
            Check(Appearance(renderer, fixtureKey) == (false, true, .25f) &&
                Appearance(renderer, secondKey) == (true, false, 1f),
                "Visibility, highlight and opacity stay independent between support slots.");
            renderer.SetAppearance(secondKey, true, true, .75f);
            Check(Appearance(renderer, fixtureKey) == (false, true, .25f) &&
                Appearance(renderer, secondKey) == (true, true, .75f),
                "Updating the second slot's appearance preserves the first slot's appearance.");
            Check(Handle(renderer, "_pickingProgram") == program, "Clear/re-add does not compile another picking program.");
            observations.Add(new { kind = "explicit-half-weight-two-occurrence-fixture", vertices = half.Length,
                fixtureKey, secondKey,
                triangleIndices = transient.TriangleIndices.Length, position_sha256 = PositionHash(half),
                first = half.Select(p => new[] { p.X, p.Y, p.Z }).ToArray() });
            renderer.Clear();
            Render();
            Check(Handle(renderer, "_pickingBuffer") == 0 && GL.GetError() == ErrorCode.NoError,
                "Final scene cleanup leaves no capture buffer or GL error.");
            Check(sourceHash == Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(source))),
                "Source file bytes remain immutable.");
            passed = true;
        }
        catch (Exception error) { failure = error.ToString(); }
        finally
        {
            File.WriteAllText(Path.Combine(output, "report.json"), JsonSerializer.Serialize(new
            {
                status = passed ? "passed" : "failed", checks, source, source_sha256 = sourceHash,
                device, version, failure, decodeIssues, textureIssues, observations,
                seconds = timer.Elapsed.TotalSeconds,
                peak_working_set_bytes = Process.GetCurrentProcess().PeakWorkingSet64,
                renderer_sha256 = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(typeof(SmoGpuSceneRenderer).Assembly.Location))),
                scope = "Actual hidden OpenGL production-shader readback; one real prepared skinned mesh and an explicit nonunit fixture; no CPU skin evaluator or whole-game pixel parity."
            }, new JsonSerializerOptions { WriteIndented = true }) + "\n");
        }
        Console.WriteLine($"GPU picking readback: {checks} checks; {(passed ? "PASS" : failure)}");
        return passed ? 0 : 1;
    }
}
