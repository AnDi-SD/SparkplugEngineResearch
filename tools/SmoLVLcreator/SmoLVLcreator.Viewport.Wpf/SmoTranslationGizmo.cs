using OpenTK.Graphics.OpenGL4;
using System.Numerics;
using System.Windows;
using System.Windows.Media.Media3D;
using Quaternion = System.Numerics.Quaternion;
using WpfVector = System.Windows.Vector;

namespace SmoLVLcreator.Viewport.Wpf;

public enum SmoGizmoAxis
{
    None,
    X,
    Y,
    Z,
    Uniform
}

public sealed record SmoGizmoScreenAxis(
    SmoGizmoAxis Axis,
    Point Start,
    Point End,
    Vector3 NativeDirection);

public sealed record SmoTranslationGizmoLayout(
    Point3D Pivot,
    Point PivotScreen,
    double WorldScale,
    Quaternion NativeOrientation,
    IReadOnlyList<SmoGizmoScreenAxis> Axes)
{
    public SmoGizmoAxis HitTest(Point point, double tolerance = 9)
    {
        SmoGizmoScreenAxis? uniform = Axes.FirstOrDefault(candidate =>
            candidate.Axis == SmoGizmoAxis.Uniform);
        if (uniform is not null &&
            DistanceToSegment(point, uniform.Start, uniform.End) <= tolerance)
            return SmoGizmoAxis.Uniform;

        SmoGizmoAxis closest = SmoGizmoAxis.None;
        double closestDistance = tolerance;
        foreach (SmoGizmoScreenAxis axis in Axes)
        {
            double distance = DistanceToSegment(point, axis.Start, axis.End);
            if (distance < closestDistance)
            {
                closest = axis.Axis;
                closestDistance = distance;
            }
        }
        return closest;
    }

    public float CalculateScaleFactor(
        SmoGizmoAxis axis,
        Point dragStart,
        Point current)
    {
        if (axis == SmoGizmoAxis.Uniform)
        {
            double pixels = dragStart.Y - current.Y;
            return MathF.Max(0.01f, 1f + (float)(pixels / 92.0));
        }

        SmoGizmoScreenAxis? screenAxis = Axes.FirstOrDefault(candidate =>
            candidate.Axis == axis);
        if (screenAxis is null)
            return 1;
        WpfVector direction = screenAxis.End - screenAxis.Start;
        double length = direction.Length;
        if (length < 1e-6)
            return 1;
        direction.Normalize();
        double pixelsAlongAxis = WpfVector.Multiply(current - dragStart, direction);
        return MathF.Max(0.01f, 1f + (float)(pixelsAlongAxis / length));
    }

    public Vector3 CalculateWorldDelta(
        SmoGizmoAxis axis,
        Point dragStart,
        Point current)
    {
        SmoGizmoScreenAxis? screenAxis = Axes.FirstOrDefault(candidate =>
            candidate.Axis == axis);
        if (screenAxis is null)
            return Vector3.Zero;

        WpfVector screenDirection = screenAxis.End - screenAxis.Start;
        double screenLength = screenDirection.Length;
        if (screenLength < 1e-6)
            return Vector3.Zero;
        screenDirection.Normalize();
        WpfVector mouseDelta = current - dragStart;
        double pixels = WpfVector.Multiply(mouseDelta, screenDirection);
        float worldDistance = (float)(pixels * WorldScale / screenLength);
        return screenAxis.NativeDirection * worldDistance;
    }

    private static double DistanceToSegment(Point point, Point a, Point b)
    {
        WpfVector segment = b - a;
        double lengthSquared = segment.LengthSquared;
        if (lengthSquared < 1e-9)
            return (point - a).Length;
        double t = Math.Clamp(
            WpfVector.Multiply(point - a, segment) / lengthSquared,
            0,
            1);
        Point closest = a + segment * t;
        return (point - closest).Length;
    }
}

public sealed class SmoTranslationGizmoRenderer
{
    private const double DesiredAxisPixels = 92;
    private int _program;
    private int _vertexArray;
    private int _vertexBuffer;

    public SmoTranslationGizmoLayout? CreateLayout(
        ProjectionCamera camera,
        double viewportWidth,
        double viewportHeight,
        Point3D pivot,
        Quaternion nativeOrientation,
        bool includeUniform = false)
    {
        if (viewportWidth <= 0 || viewportHeight <= 0)
            return null;
        GetCameraBasis(camera, out Vector3 forward, out Vector3 right, out Vector3 up);
        Vector3 cameraPosition = ToVector(camera.Position);
        Vector3 pivotVector = ToVector(pivot);
        float depth = Vector3.Dot(pivotVector - cameraPosition, forward);
        if (depth <= Math.Max(camera.NearPlaneDistance, 0.0001))
            return null;

        double scale = camera switch
        {
            OrthographicCamera orthographic =>
                orthographic.Width * DesiredAxisPixels / viewportWidth,
            PerspectiveCamera perspective =>
                2 * depth * Math.Tan(perspective.FieldOfView * Math.PI / 360) *
                DesiredAxisPixels / viewportWidth,
            _ => 2 * depth * Math.Tan(45 * Math.PI / 360) *
                 DesiredAxisPixels / viewportWidth
        };
        scale = Math.Max(scale, 0.0001);

        if (!TryProject(
                camera,
                viewportWidth,
                viewportHeight,
                pivotVector,
                forward,
                right,
                up,
                out Point pivotScreen))
        {
            return null;
        }

        if (!float.IsFinite(nativeOrientation.X) ||
            !float.IsFinite(nativeOrientation.Y) ||
            !float.IsFinite(nativeOrientation.Z) ||
            !float.IsFinite(nativeOrientation.W) ||
            nativeOrientation.LengthSquared() < 1e-12f)
        {
            nativeOrientation = Quaternion.Identity;
        }
        nativeOrientation = Quaternion.Normalize(nativeOrientation);
        (SmoGizmoAxis Axis, Vector3 NativeDirection)[] axes =
        [
            (SmoGizmoAxis.X, Vector3.Transform(Vector3.UnitX, nativeOrientation)),
            (SmoGizmoAxis.Y, Vector3.Transform(Vector3.UnitY, nativeOrientation)),
            (SmoGizmoAxis.Z, Vector3.Transform(Vector3.UnitZ, nativeOrientation))
        ];
        var screenAxes = new List<SmoGizmoScreenAxis>(includeUniform ? 4 : 3);
        foreach (var axis in axes)
        {
            Vector3 renderDirection = ToRenderDirection(axis.NativeDirection);
            Vector3 endpoint = pivotVector + renderDirection * (float)scale;
            if (TryProject(
                    camera,
                    viewportWidth,
                    viewportHeight,
                    endpoint,
                    forward,
                    right,
                    up,
                    out Point endpointScreen) &&
                (endpointScreen - pivotScreen).Length >= 4)
            {
                screenAxes.Add(new SmoGizmoScreenAxis(
                    axis.Axis,
                    pivotScreen,
                    endpointScreen,
                    axis.NativeDirection));
            }
        }
        if (includeUniform)
        {
            double diagonal = DesiredAxisPixels * 0.68;
            screenAxes.Add(new SmoGizmoScreenAxis(
                SmoGizmoAxis.Uniform,
                pivotScreen,
                new Point(
                    pivotScreen.X + diagonal,
                    pivotScreen.Y - diagonal),
                Vector3.One));
        }

        return screenAxes.Count == 0
            ? null
            : new SmoTranslationGizmoLayout(
                pivot,
                pivotScreen,
                scale,
                nativeOrientation,
                screenAxes);
    }

    public void Render(
        int width,
        int height,
        SmoTranslationGizmoLayout? layout,
        SmoGizmoAxis hoverAxis,
        SmoGizmoAxis activeAxis)
    {
        if (layout is null || layout.Axes.Count == 0)
            return;
        EnsureInitialized();

        float[] vertices = new float[layout.Axes.Count * 2 * 6];
        int cursor = 0;
        foreach (SmoGizmoScreenAxis axis in layout.Axes)
        {
            Vector4 color = AxisColor(axis.Axis, hoverAxis, activeAxis);
            WriteVertex(vertices, ref cursor, axis.Start, width, height, color);
            WriteVertex(vertices, ref cursor, axis.End, width, height, color);
        }

        GL.Disable(EnableCap.DepthTest);
        GL.Enable(EnableCap.Blend);
        GL.BlendFunc(
            BlendingFactor.SrcAlpha,
            BlendingFactor.OneMinusSrcAlpha);
        GL.UseProgram(_program);
        GL.BindVertexArray(_vertexArray);
        GL.BindBuffer(BufferTarget.ArrayBuffer, _vertexBuffer);
        GL.BufferData(
            BufferTarget.ArrayBuffer,
            vertices.Length * sizeof(float),
            vertices,
            BufferUsageHint.StreamDraw);
        GL.LineWidth(4);
        GL.DrawArrays(PrimitiveType.Lines, 0, layout.Axes.Count * 2);
        GL.PointSize(11);
        for (int endpoint = 1; endpoint < layout.Axes.Count * 2; endpoint += 2)
            GL.DrawArrays(PrimitiveType.Points, endpoint, 1);
        GL.BindVertexArray(0);
        GL.UseProgram(0);
        GL.Disable(EnableCap.Blend);
        GL.Enable(EnableCap.DepthTest);
    }

    private void EnsureInitialized()
    {
        if (_program != 0)
            return;
        const string vertexShader = """
            #version 330 core
            layout(location = 0) in vec2 aPosition;
            layout(location = 1) in vec4 aColor;
            out vec4 vColor;
            void main()
            {
                gl_Position = vec4(aPosition, 0.0, 1.0);
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
        _vertexArray = GL.GenVertexArray();
        _vertexBuffer = GL.GenBuffer();
        GL.BindVertexArray(_vertexArray);
        GL.BindBuffer(BufferTarget.ArrayBuffer, _vertexBuffer);
        const int stride = 6 * sizeof(float);
        GL.VertexAttribPointer(0, 2, VertexAttribPointerType.Float, false, stride, 0);
        GL.EnableVertexAttribArray(0);
        GL.VertexAttribPointer(
            1,
            4,
            VertexAttribPointerType.Float,
            false,
            stride,
            2 * sizeof(float));
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
        {
            GL.DeleteProgram(program);
            throw new InvalidOperationException($"Move gizmo program failed: {log}");
        }
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
        throw new InvalidOperationException($"Move gizmo shader failed: {log}");
    }

    private static void WriteVertex(
        float[] destination,
        ref int cursor,
        Point point,
        int width,
        int height,
        Vector4 color)
    {
        destination[cursor++] = (float)(point.X / width * 2 - 1);
        destination[cursor++] = (float)(1 - point.Y / height * 2);
        destination[cursor++] = color.X;
        destination[cursor++] = color.Y;
        destination[cursor++] = color.Z;
        destination[cursor++] = color.W;
    }

    private static Vector4 AxisColor(
        SmoGizmoAxis axis,
        SmoGizmoAxis hover,
        SmoGizmoAxis active)
    {
        if (axis == active)
            return new Vector4(1, 0.75f, 0.18f, 1);
        if (axis == hover)
            return new Vector4(1, 0.95f, 0.42f, 1);
        return axis switch
        {
            SmoGizmoAxis.X => new Vector4(0.93f, 0.25f, 0.22f, 1),
            SmoGizmoAxis.Y => new Vector4(0.28f, 0.82f, 0.35f, 1),
            SmoGizmoAxis.Z => new Vector4(0.30f, 0.52f, 0.95f, 1),
            SmoGizmoAxis.Uniform => new Vector4(0.95f, 0.78f, 0.28f, 1),
            _ => Vector4.One
        };
    }

    private static Vector3 ToRenderDirection(Vector3 nativeDirection) =>
        new(nativeDirection.X, nativeDirection.Y, -nativeDirection.Z);

    private static bool TryProject(
        ProjectionCamera camera,
        double width,
        double height,
        Vector3 point,
        Vector3 forward,
        Vector3 right,
        Vector3 up,
        out Point screen)
    {
        Vector3 relative = point - ToVector(camera.Position);
        float depth = Vector3.Dot(relative, forward);
        if (depth <= Math.Max(camera.NearPlaneDistance, 0.0001))
        {
            screen = default;
            return false;
        }

        float aspect = (float)(width / height);
        if (camera is OrthographicCamera orthographic)
        {
            float halfWidth = (float)(orthographic.Width * 0.5);
            float halfHeight = halfWidth / Math.Max(aspect, 0.0001f);
            float ndcX = Vector3.Dot(relative, right) / halfWidth;
            float ndcY = Vector3.Dot(relative, up) / halfHeight;
            screen = ToScreen(ndcX, ndcY, width, height);
            return true;
        }

        double fov = camera is PerspectiveCamera perspective
            ? perspective.FieldOfView
            : 45;
        float tanHorizontal = MathF.Tan((float)(fov * Math.PI / 360));
        float tanVertical = tanHorizontal / Math.Max(aspect, 0.0001f);
        float x = Vector3.Dot(relative, right) / (depth * tanHorizontal);
        float y = Vector3.Dot(relative, up) / (depth * tanVertical);
        screen = ToScreen(x, y, width, height);
        return true;
    }

    private static Point ToScreen(
        float ndcX,
        float ndcY,
        double width,
        double height) =>
        new((ndcX + 1) * width * 0.5, (1 - ndcY) * height * 0.5);

    private static void GetCameraBasis(
        ProjectionCamera camera,
        out Vector3 forward,
        out Vector3 right,
        out Vector3 up)
    {
        forward = ToVector(camera.LookDirection);
        Vector3 upHint = ToVector(camera.UpDirection);
        if (forward.LengthSquared() < 1e-12f)
            forward = -Vector3.UnitZ;
        if (upHint.LengthSquared() < 1e-12f)
            upHint = Vector3.UnitY;
        forward = Vector3.Normalize(forward);
        upHint = Vector3.Normalize(upHint);
        right = Vector3.Cross(forward, upHint);
        if (right.LengthSquared() < 1e-12f)
            right = Vector3.UnitX;
        right = Vector3.Normalize(right);
        up = Vector3.Normalize(Vector3.Cross(right, forward));
    }

    private static Vector3 ToVector(Point3D point) =>
        new((float)point.X, (float)point.Y, (float)point.Z);

    private static Vector3 ToVector(Vector3D vector) =>
        new((float)vector.X, (float)vector.Y, (float)vector.Z);
}
