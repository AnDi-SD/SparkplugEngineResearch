using OpenTK.Graphics.OpenGL4;
using SmoViewer.Rendering.Wpf;
using System.Numerics;
using System.Windows.Media.Media3D;

namespace SmoLVLcreator.Viewport.Wpf;

/// <summary>Small editor-only AABB overlay drawn by the shared GL viewport.</summary>
public sealed class SmoBoundsOverlayRenderer
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
        Vector3 minimum,
        Vector3 maximum)
    {
        EnsureInitialized();
        float[] vertices = BuildVertices(minimum, maximum);
        GL.Enable(EnableCap.DepthTest);
        GL.DepthFunc(DepthFunction.Lequal);
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
        GL.BindBuffer(BufferTarget.ArrayBuffer, _vertexBuffer);
        GL.BufferData(
            BufferTarget.ArrayBuffer,
            vertices.Length * sizeof(float),
            vertices,
            BufferUsageHint.StreamDraw);
        GL.LineWidth(1.8f);
        GL.DrawArrays(PrimitiveType.Lines, 0, vertices.Length / 3);
        GL.BindVertexArray(0);
        GL.UseProgram(0);
        GL.DepthMask(true);
        GL.Disable(EnableCap.Blend);
    }

    private static float[] BuildVertices(Vector3 minimum, Vector3 maximum)
    {
        Vector3[] corners =
        [
            new(minimum.X, minimum.Y, minimum.Z),
            new(maximum.X, minimum.Y, minimum.Z),
            new(maximum.X, maximum.Y, minimum.Z),
            new(minimum.X, maximum.Y, minimum.Z),
            new(minimum.X, minimum.Y, maximum.Z),
            new(maximum.X, minimum.Y, maximum.Z),
            new(maximum.X, maximum.Y, maximum.Z),
            new(minimum.X, maximum.Y, maximum.Z)
        ];
        int[] edges =
        [
            0, 1, 1, 2, 2, 3, 3, 0,
            4, 5, 5, 6, 6, 7, 7, 4,
            0, 4, 1, 5, 2, 6, 3, 7
        ];
        var values = new float[edges.Length * 3];
        for (int index = 0; index < edges.Length; index++)
        {
            Vector3 point = corners[edges[index]];
            values[index * 3] = point.X;
            values[index * 3 + 1] = point.Y;
            values[index * 3 + 2] = point.Z;
        }
        return values;
    }

    private void EnsureInitialized()
    {
        if (_program != 0)
            return;
        const string vertexShader = """
            #version 330 core
            layout(location = 0) in vec3 aPosition;
            uniform mat4 uView;
            uniform mat4 uProjection;
            void main()
            {
                gl_Position = vec4(aPosition, 1.0) * uView * uProjection;
            }
            """;
        const string fragmentShader = """
            #version 330 core
            out vec4 FragColor;
            void main() { FragColor = vec4(1.0, 0.62, 0.16, 0.94); }
            """;
        _program = CreateProgram(vertexShader, fragmentShader);
        _viewLocation = GL.GetUniformLocation(_program, "uView");
        _projectionLocation = GL.GetUniformLocation(_program, "uProjection");
        _vertexArray = GL.GenVertexArray();
        _vertexBuffer = GL.GenBuffer();
        GL.BindVertexArray(_vertexArray);
        GL.BindBuffer(BufferTarget.ArrayBuffer, _vertexBuffer);
        GL.VertexAttribPointer(
            0, 3, VertexAttribPointerType.Float, false, 3 * sizeof(float), 0);
        GL.EnableVertexAttribArray(0);
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
            throw new InvalidOperationException($"Bounds overlay shader link failed: {log}");
        return program;
    }

    private static int CompileShader(ShaderType type, string source)
    {
        int shader = GL.CreateShader(type);
        GL.ShaderSource(shader, source);
        GL.CompileShader(shader);
        GL.GetShader(shader, ShaderParameter.CompileStatus, out int compiled);
        if (compiled != 0)
            return shader;
        string log = GL.GetShaderInfoLog(shader);
        GL.DeleteShader(shader);
        throw new InvalidOperationException(
            $"Bounds overlay {type} compile failed: {log}");
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
