using OpenTK.Graphics.OpenGL4;
using System.Numerics;
using System.Windows;
using System.Windows.Media.Media3D;
using Quaternion = System.Numerics.Quaternion;
using WpfVector = System.Windows.Vector;

namespace SmoLVLcreator.Viewport.Wpf;

public sealed record SmoRotationGizmoRing(
    SmoGizmoAxis Axis,
    Vector3 NativeDirection,
    IReadOnlyList<Point> Points);

public sealed record SmoRotationGizmoLayout(
    Point3D Pivot,
    Point PivotScreen,
    Quaternion NativeOrientation,
    IReadOnlyList<SmoRotationGizmoRing> Rings)
{
    public SmoGizmoAxis HitTest(Point point, double tolerance = 9)
    {
        SmoGizmoAxis closest = SmoGizmoAxis.None;
        double closestDistance = tolerance;
        foreach (SmoRotationGizmoRing ring in Rings)
        {
            for (int index = 1; index < ring.Points.Count; index++)
            {
                double distance = DistanceToSegment(
                    point,
                    ring.Points[index - 1],
                    ring.Points[index]);
                if (distance < closestDistance)
                {
                    closest = ring.Axis;
                    closestDistance = distance;
                }
            }
        }
        return closest;
    }

    public float CalculateRadians(Point dragStart, Point current)
    {
        WpfVector start = dragStart - PivotScreen;
        WpfVector end = current - PivotScreen;
        if (start.LengthSquared < 1e-6 || end.LengthSquared < 1e-6)
            return 0;
        double startAngle = Math.Atan2(start.Y, start.X);
        double endAngle = Math.Atan2(end.Y, end.X);
        double delta = endAngle - startAngle;
        while (delta > Math.PI)
            delta -= Math.PI * 2;
        while (delta < -Math.PI)
            delta += Math.PI * 2;
        return (float)delta;
    }

    public Vector3 GetNativeAxis(SmoGizmoAxis axis) =>
        Rings.FirstOrDefault(ring => ring.Axis == axis)?.NativeDirection ??
        Vector3.UnitY;

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

public sealed class SmoRotationGizmoRenderer
{
    private const double DesiredRadiusPixels = 78;
    private const int RingSegments = 64;
    private int _program;
    private int _vertexArray;
    private int _vertexBuffer;

    public SmoRotationGizmoLayout? CreateLayout(
        ProjectionCamera camera,
        double viewportWidth,
        double viewportHeight,
        Point3D pivot,
        Quaternion nativeOrientation)
    {
        if (viewportWidth <= 0 || viewportHeight <= 0)
            return null;
        GetCameraBasis(camera, out Vector3 forward, out Vector3 right, out Vector3 up);
        Vector3 cameraPosition = ToVector(camera.Position);
        Vector3 pivotVector = ToVector(pivot);
        float depth = Vector3.Dot(pivotVector - cameraPosition, forward);
        if (depth <= Math.Max(camera.NearPlaneDistance, 0.0001))
            return null;
        if (!IsFinite(nativeOrientation) || nativeOrientation.LengthSquared() < 1e-12f)
            nativeOrientation = Quaternion.Identity;
        nativeOrientation = Quaternion.Normalize(nativeOrientation);

        double radius = camera switch
        {
            OrthographicCamera orthographic =>
                orthographic.Width * DesiredRadiusPixels / viewportWidth,
            PerspectiveCamera perspective =>
                2 * depth * Math.Tan(perspective.FieldOfView * Math.PI / 360) *
                DesiredRadiusPixels / viewportWidth,
            _ => 2 * depth * Math.Tan(45 * Math.PI / 360) *
                 DesiredRadiusPixels / viewportWidth
        };
        radius = Math.Max(radius, 0.0001);
        if (!TryProject(
                camera, viewportWidth, viewportHeight, pivotVector,
                forward, right, up, out Point pivotScreen))
        {
            return null;
        }

        (SmoGizmoAxis Axis, Vector3 Direction)[] axes =
        [
            (SmoGizmoAxis.X, Vector3.Transform(Vector3.UnitX, nativeOrientation)),
            (SmoGizmoAxis.Y, Vector3.Transform(Vector3.UnitY, nativeOrientation)),
            (SmoGizmoAxis.Z, Vector3.Transform(Vector3.UnitZ, nativeOrientation))
        ];
        var rings = new List<SmoRotationGizmoRing>(3);
        foreach ((SmoGizmoAxis axis, Vector3 nativeDirection) in axes)
        {
            Vector3 normal = ToRenderDirection(nativeDirection);
            normal = Vector3.Normalize(normal);
            Vector3 reference = MathF.Abs(normal.Y) < 0.85f
                ? Vector3.UnitY
                : Vector3.UnitX;
            Vector3 basisU = Vector3.Normalize(Vector3.Cross(normal, reference));
            Vector3 basisV = Vector3.Normalize(Vector3.Cross(normal, basisU));
            var points = new List<Point>(RingSegments + 1);
            for (int segment = 0; segment <= RingSegments; segment++)
            {
                float angle = segment * MathF.Tau / RingSegments;
                Vector3 point = pivotVector + (float)radius *
                    (MathF.Cos(angle) * basisU + MathF.Sin(angle) * basisV);
                if (TryProject(
                        camera, viewportWidth, viewportHeight, point,
                        forward, right, up, out Point projected))
                {
                    points.Add(projected);
                }
            }
            if (points.Count > 2)
                rings.Add(new SmoRotationGizmoRing(axis, nativeDirection, points));
        }
        return rings.Count == 0
            ? null
            : new SmoRotationGizmoLayout(
                pivot,
                pivotScreen,
                nativeOrientation,
                rings);
    }

    public void Render(
        int width,
        int height,
        SmoRotationGizmoLayout? layout,
        SmoGizmoAxis hoverAxis,
        SmoGizmoAxis activeAxis)
    {
        if (layout is null || layout.Rings.Count == 0)
            return;
        EnsureInitialized();

        int vertexCount = layout.Rings.Sum(ring => ring.Points.Count);
        float[] vertices = new float[vertexCount * 6];
        int cursor = 0;
        foreach (SmoRotationGizmoRing ring in layout.Rings)
        {
            Vector4 color = AxisColor(ring.Axis, hoverAxis, activeAxis);
            foreach (Point point in ring.Points)
                WriteVertex(vertices, ref cursor, point, width, height, color);
        }

        GL.Disable(EnableCap.DepthTest);
        GL.Enable(EnableCap.Blend);
        GL.BlendFunc(BlendingFactor.SrcAlpha, BlendingFactor.OneMinusSrcAlpha);
        GL.UseProgram(_program);
        GL.BindVertexArray(_vertexArray);
        GL.BindBuffer(BufferTarget.ArrayBuffer, _vertexBuffer);
        GL.BufferData(
            BufferTarget.ArrayBuffer,
            vertices.Length * sizeof(float),
            vertices,
            BufferUsageHint.StreamDraw);
        GL.LineWidth(3);
        int first = 0;
        foreach (SmoRotationGizmoRing ring in layout.Rings)
        {
            GL.DrawArrays(PrimitiveType.LineStrip, first, ring.Points.Count);
            first += ring.Points.Count;
        }
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
        int vertex = CompileShader(ShaderType.VertexShader, vertexShader);
        int fragment = CompileShader(ShaderType.FragmentShader, fragmentShader);
        _program = GL.CreateProgram();
        GL.AttachShader(_program, vertex);
        GL.AttachShader(_program, fragment);
        GL.LinkProgram(_program);
        GL.GetProgram(_program, GetProgramParameterName.LinkStatus, out int linked);
        string log = GL.GetProgramInfoLog(_program);
        GL.DeleteShader(vertex);
        GL.DeleteShader(fragment);
        if (linked == 0)
            throw new InvalidOperationException($"Rotate gizmo program failed: {log}");

        _vertexArray = GL.GenVertexArray();
        _vertexBuffer = GL.GenBuffer();
        GL.BindVertexArray(_vertexArray);
        GL.BindBuffer(BufferTarget.ArrayBuffer, _vertexBuffer);
        const int stride = 6 * sizeof(float);
        GL.VertexAttribPointer(0, 2, VertexAttribPointerType.Float, false, stride, 0);
        GL.EnableVertexAttribArray(0);
        GL.VertexAttribPointer(
            1, 4, VertexAttribPointerType.Float, false, stride, 2 * sizeof(float));
        GL.EnableVertexAttribArray(1);
        GL.BindVertexArray(0);
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
        throw new InvalidOperationException($"Rotate gizmo shader failed: {log}");
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
            _ => Vector4.One
        };
    }

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
            screen = ToScreen(
                Vector3.Dot(relative, right) / halfWidth,
                Vector3.Dot(relative, up) / halfHeight,
                width,
                height);
            return true;
        }
        double fov = camera is PerspectiveCamera perspective
            ? perspective.FieldOfView
            : 45;
        float tanHorizontal = MathF.Tan((float)(fov * Math.PI / 360));
        float tanVertical = tanHorizontal / Math.Max(aspect, 0.0001f);
        screen = ToScreen(
            Vector3.Dot(relative, right) / (depth * tanHorizontal),
            Vector3.Dot(relative, up) / (depth * tanVertical),
            width,
            height);
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

    private static bool IsFinite(Quaternion value) =>
        float.IsFinite(value.X) && float.IsFinite(value.Y) &&
        float.IsFinite(value.Z) && float.IsFinite(value.W);

    private static Vector3 ToRenderDirection(Vector3 nativeDirection) =>
        new(nativeDirection.X, nativeDirection.Y, -nativeDirection.Z);

    private static Vector3 ToVector(Point3D point) =>
        new((float)point.X, (float)point.Y, (float)point.Z);

    private static Vector3 ToVector(Vector3D vector) =>
        new((float)vector.X, (float)vector.Y, (float)vector.Z);
}
