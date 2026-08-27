using OpenTK.Graphics.OpenGL4;
using SmoViewer.Rendering.Wpf;
using System.Numerics;
using System.Windows.Media.Media3D;

namespace SmoLVLcreator.Viewport.Wpf;

public sealed record SmoCollisionOverlayItem(
    IReadOnlyList<Vector3> Positions,
    IReadOnlyList<int> TriangleIndices,
    Matrix4x4 WorldTransform,
    bool Selected);

/// <summary>Editor overlay for decoded collision triangle meshes.</summary>
public sealed class SmoCollisionOverlayRenderer
{
    private int _program;
    private int _vertexArray;
    private int _vertexBuffer;
    private int _viewLocation;
    private int _projectionLocation;

    public void Render(
        int width,
        int height,
        ProjectionCamera camera,
        IEnumerable<SmoCollisionOverlayItem> items)
    {
        ArgumentNullException.ThrowIfNull(camera);
        ArgumentNullException.ThrowIfNull(items);
        SmoCollisionOverlayItem[] drawItems = items.ToArray();
        float[] ordinaryVertices = BuildVertices(drawItems.Where(item => !item.Selected));
        float[] selectedVertices = BuildVertices(drawItems.Where(item => item.Selected));
        if (ordinaryVertices.Length == 0 && selectedVertices.Length == 0)
            return;

        EnsureInitialized();
        GL.Enable(EnableCap.Blend);
        GL.BlendFunc(BlendingFactor.SrcAlpha, BlendingFactor.OneMinusSrcAlpha);
        GL.DepthMask(false);
        GL.UseProgram(_program);
        GL.BindVertexArray(_vertexArray);
        SetMatrix(_viewLocation, SmoViewportMath.CreateViewMatrix(camera));
        SetMatrix(
            _projectionLocation,
            SmoViewportMath.CreateProjectionMatrix(
                camera,
                width / (float)Math.Max(height, 1)));

        if (ordinaryVertices.Length > 0)
        {
            // Ordinary room colliders obey the resolved scene depth. A small
            // line offset avoids flicker where collision and render surfaces coincide.
            GL.Enable(EnableCap.DepthTest);
            GL.DepthFunc(DepthFunction.Lequal);
            GL.Enable(EnableCap.PolygonOffsetLine);
            GL.PolygonOffset(-2, -2);
            DrawVertices(ordinaryVertices, 1.6f);
            GL.Disable(EnableCap.PolygonOffsetLine);
        }
        if (selectedVertices.Length > 0)
        {
            // Explicit selection stays readable as an editor overlay.
            GL.Disable(EnableCap.DepthTest);
            DrawVertices(selectedVertices, 2.2f);
        }
        GL.BindVertexArray(0);
        GL.UseProgram(0);
        GL.DepthMask(true);
        GL.Enable(EnableCap.DepthTest);
        GL.Disable(EnableCap.Blend);
    }

    private void DrawVertices(float[] vertices, float lineWidth)
    {
        GL.BindBuffer(BufferTarget.ArrayBuffer, _vertexBuffer);
        GL.BufferData(
            BufferTarget.ArrayBuffer,
            vertices.Length * sizeof(float),
            vertices,
            BufferUsageHint.StreamDraw);
        GL.LineWidth(lineWidth);
        GL.DrawArrays(PrimitiveType.Lines, 0, vertices.Length / 7);
    }

    private static float[] BuildVertices(IEnumerable<SmoCollisionOverlayItem> items)
    {
        var values = new List<float>();
        Matrix4x4 reflection = Matrix4x4.CreateScale(1, 1, -1);
        foreach (SmoCollisionOverlayItem item in items)
        {
            Vector4 color = item.Selected
                ? new Vector4(1.0f, 0.48f, 0.14f, 0.98f)
                : new Vector4(0.18f, 0.9f, 0.63f, 0.55f);
            Matrix4x4 world = item.WorldTransform * reflection;
            for (int offset = 0; offset + 2 < item.TriangleIndices.Count; offset += 3)
            {
                int a = item.TriangleIndices[offset];
                int b = item.TriangleIndices[offset + 1];
                int c = item.TriangleIndices[offset + 2];
                if ((uint)a >= (uint)item.Positions.Count ||
                    (uint)b >= (uint)item.Positions.Count ||
                    (uint)c >= (uint)item.Positions.Count)
                {
                    continue;
                }
                WriteEdge(values, item.Positions[a], item.Positions[b], world, color);
                WriteEdge(values, item.Positions[b], item.Positions[c], world, color);
                WriteEdge(values, item.Positions[c], item.Positions[a], world, color);
            }
        }
        return values.ToArray();
    }

    private static void WriteEdge(
        List<float> values,
        Vector3 a,
        Vector3 b,
        Matrix4x4 world,
        Vector4 color)
    {
        WriteVertex(values, Vector3.Transform(a, world), color);
        WriteVertex(values, Vector3.Transform(b, world), color);
    }

    private static void WriteVertex(List<float> values, Vector3 p, Vector4 c)
    {
        values.Add(p.X); values.Add(p.Y); values.Add(p.Z);
        values.Add(c.X); values.Add(c.Y); values.Add(c.Z); values.Add(c.W);
    }

    private void EnsureInitialized()
    {
        if (_program != 0)
            return;
        const string vertexShader = """
            #version 330 core
            layout(location = 0) in vec3 aPosition;
            layout(location = 1) in vec4 aColor;
            uniform mat4 uView;
            uniform mat4 uProjection;
            out vec4 vColor;
            void main()
            {
                gl_Position = vec4(aPosition, 1.0) * uView * uProjection;
                vColor = aColor;
            }
            """;
        const string fragmentShader = """
            #version 330 core
            in vec4 vColor;
            out vec4 FragColor;
            void main() { FragColor = vColor; }
            """;
        _program = CreateProgram(vertexShader, fragmentShader);
        _viewLocation = GL.GetUniformLocation(_program, "uView");
        _projectionLocation = GL.GetUniformLocation(_program, "uProjection");
        _vertexArray = GL.GenVertexArray();
        _vertexBuffer = GL.GenBuffer();
        GL.BindVertexArray(_vertexArray);
        GL.BindBuffer(BufferTarget.ArrayBuffer, _vertexBuffer);
        const int stride = 7 * sizeof(float);
        GL.VertexAttribPointer(0, 3, VertexAttribPointerType.Float, false, stride, 0);
        GL.EnableVertexAttribArray(0);
        GL.VertexAttribPointer(1, 4, VertexAttribPointerType.Float, false, stride, 3 * sizeof(float));
        GL.EnableVertexAttribArray(1);
        GL.BindVertexArray(0);
    }

    private static int CreateProgram(string vertexSource, string fragmentSource)
    {
        int vertex = CompileShader(ShaderType.VertexShader, vertexSource);
        int fragment = CompileShader(ShaderType.FragmentShader, fragmentSource);
        int program = GL.CreateProgram();
        GL.AttachShader(program, vertex);
        GL.AttachShader(program, fragment);
        GL.LinkProgram(program);
        GL.GetProgram(program, GetProgramParameterName.LinkStatus, out int linked);
        string log = GL.GetProgramInfoLog(program);
        GL.DeleteShader(vertex);
        GL.DeleteShader(fragment);
        if (linked == 0)
            throw new InvalidOperationException($"Collision overlay shader link failed: {log}");
        return program;
    }

    private static int CompileShader(ShaderType type, string source)
    {
        int shader = GL.CreateShader(type);
        GL.ShaderSource(shader, source);
        GL.CompileShader(shader);
        GL.GetShader(shader, ShaderParameter.CompileStatus, out int compiled);
        string log = GL.GetShaderInfoLog(shader);
        if (compiled == 0)
        {
            GL.DeleteShader(shader);
            throw new InvalidOperationException($"Collision overlay shader compile failed: {log}");
        }
        return shader;
    }

    private static void SetMatrix(int location, Matrix4x4 matrix)
    {
        float[] values =
        [
            matrix.M11, matrix.M12, matrix.M13, matrix.M14,
            matrix.M21, matrix.M22, matrix.M23, matrix.M24,
            matrix.M31, matrix.M32, matrix.M33, matrix.M34,
            matrix.M41, matrix.M42, matrix.M43, matrix.M44
        ];
        GL.UniformMatrix4(location, 1, true, values);
    }
}
