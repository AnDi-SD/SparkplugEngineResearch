using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using OpenTK.Graphics.OpenGL4;
using OpenTK.Mathematics;
using OpenTK.Windowing.Common;
using OpenTK.Windowing.Desktop;
using SmoViewer.Rendering.Wpf;

// Runs the production vertex shader on the actual GL device. Fixed numeric
// expectations describe the four-weight branch of original Fixed.rfx; this
// is neither another production skin evaluator nor whole-game pixel parity.
internal static class GpuSkinningRegression
{
    public static int Run(string output, string? oldVertexSource = null)
    {
        output = Path.GetFullPath(output);
        Directory.CreateDirectory(output);
        string source = oldVertexSource is null
            ? (string)typeof(SmoGpuSceneRenderer).GetField("VertexShaderSource",
                BindingFlags.Static | BindingFlags.NonPublic)!.GetRawConstantValue()!
            : File.ReadAllText(oldVertexSource);
        if (source.Length > 65536) throw new InvalidDataException("Shader fixture exceeds 64 KiB.");
        var timer = Stopwatch.StartNew();
        string status = "failed", device = "", version = "", failure = "";
        var observations = new List<object>();
        int checks = 0;
        void Check(bool condition, string message)
        {
            ++checks;
            if (!condition) throw new InvalidDataException(message);
        }
        try
        {
            using var window = new NativeWindow(new NativeWindowSettings
            {
                ClientSize = new Vector2i(16, 16), StartVisible = false,
                StartFocused = false, API = ContextAPI.OpenGL,
                APIVersion = new Version(3, 3), Profile = ContextProfile.Core,
                Flags = ContextFlags.ForwardCompatible, AutoLoadBindings = true
            });
            window.MakeCurrent();
            device = GL.GetString(StringName.Renderer);
            version = GL.GetString(StringName.Version);
            int shader = GL.CreateShader(ShaderType.VertexShader);
            int program = GL.CreateProgram(), vao = GL.GenVertexArray(), buffer = GL.GenBuffer();
            try
            {
                GL.ShaderSource(shader, source);
                GL.CompileShader(shader);
                GL.GetShader(shader, ShaderParameter.CompileStatus, out int compiled);
                Check(compiled != 0, "Vertex compilation: " + GL.GetShaderInfoLog(shader));
                GL.AttachShader(program, shader);
                GL.TransformFeedbackVaryings(program, 1, ["gl_Position"], TransformFeedbackMode.InterleavedAttribs);
                GL.LinkProgram(program);
                GL.GetProgram(program, GetProgramParameterName.LinkStatus, out int linked);
                Check(linked != 0, "Transform-feedback link: " + GL.GetProgramInfoLog(program));
                GL.UseProgram(program);
                float[] identity = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1];
                foreach (string name in new[] { "uModel", "uView", "uProjection" })
                    GL.UniformMatrix4(GL.GetUniformLocation(program, name), 1, true, identity);
                var bones = new float[32 * 16];
                for (int i = 0; i < 32; ++i) identity.CopyTo(bones, i * 16);
                bones[12] = 1;                  // (x+1, y, z)
                bones[16 + 13] = 2;             // (x, y+2, z)
                bones[32 + 14] = 3;             // (x, y, z+3)
                bones[48] = 2; bones[48 + 5] = 3; bones[48 + 10] = 4;
                bones[48 + 12] = -1; bones[48 + 13] = -2; bones[48 + 14] = -3;
                GL.UniformMatrix4(GL.GetUniformLocation(program, "uBones[0]"), 32, true, bones);
                GL.Uniform1(GL.GetUniformLocation(program, "uSkinned"), 1);
                GL.BindVertexArray(vao);
                GL.VertexAttrib3(0, 2f, 3f, 4f);
                GL.VertexAttrib4(2, 1f, 1f, 1f, 1f);
                GL.VertexAttrib4(5, 0f, 1f, 2f, 3f);
                GL.VertexAttrib3(6, 0f, 0f, 1f);
                GL.BindBuffer(BufferTarget.TransformFeedbackBuffer, buffer);
                GL.BufferData(BufferTarget.TransformFeedbackBuffer, 16, IntPtr.Zero, BufferUsageHint.StreamRead);
                GL.BindBufferBase(BufferRangeTarget.TransformFeedbackBuffer, 0, buffer);
                GL.Enable(EnableCap.RasterizerDiscard);
                (string Name, float[] Weights, float[] Expected)[] cases =
                [
                    ("unit", [1, 0, 0, 0], [3, 3, 4, 1]),
                    ("half-sum", [.5f, 0, 0, 0], [1.5f, 1.5f, 2, 1]),
                    ("sum-above-one", [.5f, .5f, .5f, 0], [3.5f, 5.5f, 7.5f, 1]),
                    ("signed", [-.5f, 1.5f, 0, 0], [1.5f, 6, 4, 1]),
                    ("tiny", [5e-7f, 0, 0, 0], [1.5e-6f, 1.5e-6f, 2e-6f, 1]),
                    ("zero-sum", [0, 0, 0, 0], [0, 0, 0, 1]),
                    ("four-affine", [.1f, .2f, .3f, .4f], [2.5f, 5, 8.5f, 1])
                ];
                foreach (var sample in cases)
                {
                    GL.VertexAttrib4(4, sample.Weights);
                    GL.BeginTransformFeedback(TransformFeedbackPrimitiveType.Points);
                    GL.DrawArrays(PrimitiveType.Points, 0, 1);
                    GL.EndTransformFeedback();
                    var actual = new float[4];
                    GL.GetBufferSubData(BufferTarget.TransformFeedbackBuffer, IntPtr.Zero, 16, actual);
                    float maximum = actual.Zip(sample.Expected).Max(pair => MathF.Abs(pair.First - pair.Second));
                    observations.Add(new { sample.Name, sample.Weights, sample.Expected, actual, maximum });
                    float tolerance = sample.Name == "tiny" ? 1e-11f : 3e-6f;
                    Check(actual.All(float.IsFinite) && maximum <= tolerance,
                        $"{sample.Name}: actual vertex differs from original shader arithmetic by {maximum:G9}.");
                }
                GL.Uniform1(GL.GetUniformLocation(program, "uSkinned"), 0);
                GL.BeginTransformFeedback(TransformFeedbackPrimitiveType.Points);
                GL.DrawArrays(PrimitiveType.Points, 0, 1);
                GL.EndTransformFeedback();
                var unskinned = new float[4];
                GL.GetBufferSubData(BufferTarget.TransformFeedbackBuffer, IntPtr.Zero, 16, unskinned);
                Check(unskinned.SequenceEqual(new float[] { 2, 3, 4, 1 }), "Unskinned vertex is unchanged.");
                Check(GL.GetError() == ErrorCode.NoError, "GL completed without errors.");
                status = "passed";
            }
            finally
            {
                GL.Disable(EnableCap.RasterizerDiscard);
                GL.UseProgram(0);
                GL.BindVertexArray(0);
                GL.DeleteBuffer(buffer);
                GL.DeleteVertexArray(vao);
                GL.DeleteProgram(program);
                GL.DeleteShader(shader);
            }
        }
        catch (Exception error) { failure = error.ToString(); throw; }
        finally
        {
            var report = new { status, checks, device, version, failure,
                seconds = timer.Elapsed.TotalSeconds,
                peak_working_set_bytes = Process.GetCurrentProcess().PeakWorkingSet64,
                source = oldVertexSource is null ? "production VertexShaderSource" : Path.GetFullPath(oldVertexSource),
                shader_sha256 = Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(source))),
                assembly_sha256 = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(typeof(SmoGpuSceneRenderer).Assembly.Location))),
                observations,
                scope = "Actual hidden OpenGL transform feedback of the vertex shader; fixed four-weight numeric fixtures, no whole-game shader/light/pixel parity." };
            File.WriteAllText(Path.Combine(output, "report.json"), JsonSerializer.Serialize(report,
                new JsonSerializerOptions { WriteIndented = true }) + "\n");
        }
        Console.WriteLine($"GPU skinning: {checks} checks on {device}; hidden transform feedback PASS.");
        return 0;
    }
}
