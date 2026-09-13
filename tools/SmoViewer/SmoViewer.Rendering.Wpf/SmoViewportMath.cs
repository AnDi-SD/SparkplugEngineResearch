using SmoViewer.Scene;
using System.Numerics;
using System.Windows.Media.Media3D;

namespace SmoViewer.Rendering.Wpf;

/// <summary>Shared viewport calculations used by GPU and fallback hosts.</summary>
public static class SmoViewportMath
{
    public static Matrix4x4 CreateViewMatrix(ProjectionCamera camera)
    {
        ArgumentNullException.ThrowIfNull(camera);
        Vector3 position = ToVector(camera.Position);
        Vector3 look = ToVector(camera.LookDirection);
        Vector3 up = ToVector(camera.UpDirection);
        if (look.LengthSquared() < 1e-6f)
            look = -Vector3.UnitZ;
        if (up.LengthSquared() < 1e-6f)
            up = Vector3.UnitY;
        return Matrix4x4.CreateLookAt(position, position + look, up);
    }

    public static Matrix4x4 CreateProjectionMatrix(
        ProjectionCamera camera,
        float aspect)
    {
        ArgumentNullException.ThrowIfNull(camera);
        double near = Math.Max(camera.NearPlaneDistance, 0.0001);
        double far = Math.Max(camera.FarPlaneDistance, near + 1);
        return camera switch
        {
            OrthographicCamera orthographic => CreateOrthographicProjection(
                orthographic.Width, aspect, near, far),
            PerspectiveCamera perspective => CreatePerspectiveProjection(
                perspective.FieldOfView, aspect, near, far),
            _ => CreatePerspectiveProjection(45, aspect, near, far)
        };
    }

    public static double NiceGridStep(double value)
    {
        double power = Math.Pow(10, Math.Floor(Math.Log10(value)));
        double normalized = value / power;
        double factor = normalized <= 1 ? 1
            : normalized <= 2 ? 2
            : normalized <= 5 ? 5
            : 10;
        return factor * power;
    }

    /// <summary>
    /// Creates a native-SMO picking ray from the WPF camera and a viewport
    /// position. The Z reflection used by the OpenGL backend is reversed here.
    /// </summary>
    public static SmoPickRay CreateSmoPickRay(
        ProjectionCamera camera,
        double viewportX,
        double viewportY,
        double viewportWidth,
        double viewportHeight)
    {
        ArgumentNullException.ThrowIfNull(camera);
        if (viewportWidth <= 0 || viewportHeight <= 0)
            throw new ArgumentOutOfRangeException(
                nameof(viewportWidth),
                "Viewport dimensions must be positive.");

        Vector3 position = ToVector(camera.Position);
        Vector3 forward = ToVector(camera.LookDirection);
        Vector3 upHint = ToVector(camera.UpDirection);
        if (forward.LengthSquared() < 1e-12f)
            forward = -Vector3.UnitZ;
        if (upHint.LengthSquared() < 1e-12f)
            upHint = Vector3.UnitY;
        forward = Vector3.Normalize(forward);
        upHint = Vector3.Normalize(upHint);
        Vector3 right = Vector3.Cross(forward, upHint);
        if (right.LengthSquared() < 1e-12f)
            right = Vector3.UnitX;
        right = Vector3.Normalize(right);
        Vector3 up = Vector3.Normalize(Vector3.Cross(right, forward));

        float normalizedX = (float)(viewportX / viewportWidth * 2 - 1);
        float normalizedY = (float)(1 - viewportY / viewportHeight * 2);
        float aspect = (float)(viewportWidth / viewportHeight);
        Vector3 origin;
        Vector3 direction;
        if (camera is OrthographicCamera orthographic)
        {
            float halfWidth = (float)(orthographic.Width * 0.5);
            float halfHeight = halfWidth / Math.Max(aspect, 0.0001f);
            origin = position + right * (normalizedX * halfWidth) +
                up * (normalizedY * halfHeight);
            direction = forward;
        }
        else
        {
            double fieldOfView = camera is PerspectiveCamera perspective
                ? perspective.FieldOfView
                : 45;
            float horizontalTangent = MathF.Tan(
                (float)(fieldOfView * Math.PI / 360));
            float verticalTangent = horizontalTangent /
                Math.Max(aspect, 0.0001f);
            origin = position;
            direction = Vector3.Normalize(
                forward +
                right * (normalizedX * horizontalTangent) +
                up * (normalizedY * verticalTangent));
        }

        origin.Z = -origin.Z;
        direction.Z = -direction.Z;
        return new SmoPickRay(origin, direction);
    }

    private static Vector3 ToVector(Point3D value) =>
        new((float)value.X, (float)value.Y, (float)value.Z);

    private static Vector3 ToVector(Vector3D value) =>
        new((float)value.X, (float)value.Y, (float)value.Z);

    private static Matrix4x4 CreatePerspectiveProjection(
        double horizontalFieldOfViewDegrees,
        float aspect,
        double near,
        double far)
    {
        float radians = (float)(horizontalFieldOfViewDegrees * Math.PI / 180.0);
        float horizontalScale = 1f / MathF.Tan(radians * 0.5f);
        float verticalScale = horizontalScale * Math.Max(aspect, 0.0001f);
        float nearValue = (float)near;
        float farValue = (float)far;
        return new Matrix4x4(
            horizontalScale, 0, 0, 0,
            0, verticalScale, 0, 0,
            0, 0, (farValue + nearValue) / (nearValue - farValue), -1,
            0, 0, 2 * farValue * nearValue / (nearValue - farValue), 0);
    }

    private static Matrix4x4 CreateOrthographicProjection(
        double width,
        float aspect,
        double near,
        double far)
    {
        float widthValue = (float)Math.Max(width, 0.0001);
        float heightValue = widthValue / Math.Max(aspect, 0.0001f);
        float nearValue = (float)near;
        float farValue = (float)far;
        return new Matrix4x4(
            2 / widthValue, 0, 0, 0,
            0, 2 / heightValue, 0, 0,
            0, 0, -2 / (farValue - nearValue), 0,
            0, 0, -(farValue + nearValue) / (farValue - nearValue), 1);
    }
}
