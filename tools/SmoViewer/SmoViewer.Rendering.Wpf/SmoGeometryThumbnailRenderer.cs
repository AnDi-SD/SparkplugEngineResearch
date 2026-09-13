using System.Numerics;
using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Media.Media3D;

namespace SmoViewer.Rendering.Wpf;

/// <summary>
/// Lightweight preview path for geometry which has not been serialized to an
/// SMO resource yet. It deliberately accepts renderer-neutral vertex arrays so
/// import tools do not need to repack and parse a complete container merely to
/// display a fitting preview.
/// </summary>
public static class SmoGeometryThumbnailRenderer
{
    public static Task<BitmapSource> CreateThumbnailAsync(
        IReadOnlyList<SmoGeometryPreviewMesh> meshes,
        Matrix4x4 transform,
        int width = 900,
        int height = 600,
        SmoGeometryPreviewBounds? framingBounds = null,
        CancellationToken cancellationToken = default)
    {
        var completion = new TaskCompletionSource<BitmapSource>(
            TaskCreationOptions.RunContinuationsAsynchronously);
        var thread = new Thread(() =>
        {
            try
            {
                cancellationToken.ThrowIfCancellationRequested();
                BitmapSource result = CreateThumbnail(
                    meshes,
                    transform,
                    width,
                    height,
                    framingBounds);
                cancellationToken.ThrowIfCancellationRequested();
                completion.TrySetResult(result);
            }
            catch (OperationCanceledException)
            {
                completion.TrySetCanceled(cancellationToken);
            }
            catch (Exception exception)
            {
                completion.TrySetException(exception);
            }
        })
        {
            IsBackground = true,
            Name = "SmoGeometryThumbnailRenderer"
        };
        thread.SetApartmentState(ApartmentState.STA);
        thread.Start();
        return completion.Task;
    }

    public static BitmapSource CreateThumbnail(
        IReadOnlyList<SmoGeometryPreviewMesh> meshes,
        Matrix4x4 transform,
        int width = 900,
        int height = 600,
        SmoGeometryPreviewBounds? framingBounds = null)
    {
        ArgumentNullException.ThrowIfNull(meshes);
        if (meshes.Count == 0)
            throw new ArgumentException("At least one preview mesh is required.", nameof(meshes));
        if (width <= 0 || height <= 0)
            throw new ArgumentOutOfRangeException(nameof(width));

        var group = new Model3DGroup();
        group.Children.Add(new AmbientLight(Color.FromRgb(78, 85, 94)));
        group.Children.Add(new DirectionalLight(
            Color.FromRgb(235, 238, 242),
            new Vector3D(-0.45, -0.7, -1)));
        group.Children.Add(new DirectionalLight(
            Color.FromRgb(105, 130, 160),
            new Vector3D(0.8, 0.25, 0.45)));

        Vector3 minimum = new(float.PositiveInfinity);
        Vector3 maximum = new(float.NegativeInfinity);
        int renderedTriangles = 0;
        foreach (SmoGeometryPreviewMesh mesh in meshes)
        {
            if (mesh.Positions.Length == 0 || mesh.TriangleIndices.Length < 3)
                continue;
            var positions = new Point3DCollection(mesh.Positions.Length);
            foreach (Vector3 position in mesh.Positions)
            {
                Vector3 adjusted = Vector3.Transform(position, transform);
                if (!IsFinite(adjusted))
                    throw new InvalidDataException("Preview transform produced a non-finite vertex.");
                positions.Add(new Point3D(adjusted.X, adjusted.Y, adjusted.Z));
                minimum = Vector3.Min(minimum, adjusted);
                maximum = Vector3.Max(maximum, adjusted);
            }

            var indices = new Int32Collection(mesh.TriangleIndices.Length);
            for (int offset = 0; offset + 2 < mesh.TriangleIndices.Length; offset += 3)
            {
                uint a = mesh.TriangleIndices[offset];
                uint b = mesh.TriangleIndices[offset + 1];
                uint c = mesh.TriangleIndices[offset + 2];
                if (a >= positions.Count || b >= positions.Count || c >= positions.Count)
                    continue;
                indices.Add((int)a);
                indices.Add((int)b);
                indices.Add((int)c);
                renderedTriangles++;
            }

            var geometry = new MeshGeometry3D
            {
                Positions = positions,
                TriangleIndices = indices
            };
            if (mesh.Normals.Length == mesh.Positions.Length)
            {
                geometry.Normals = new Vector3DCollection(mesh.Normals.Select(normal =>
                {
                    Vector3 adjusted = Vector3.TransformNormal(normal, transform);
                    if (adjusted.LengthSquared() > 1e-12f)
                        adjusted = Vector3.Normalize(adjusted);
                    return new Vector3D(adjusted.X, adjusted.Y, adjusted.Z);
                }));
            }
            if (mesh.TextureCoordinates.Length == mesh.Positions.Length)
            {
                geometry.TextureCoordinates = new PointCollection(
                    mesh.TextureCoordinates.Select(uv => new Point(uv.X, uv.Y)));
            }
            geometry.Freeze();

            var brush = new SolidColorBrush(ResolveColor(mesh));
            brush.Freeze();
            var material = new MaterialGroup();
            material.Children.Add(new DiffuseMaterial(brush));
            material.Children.Add(new SpecularMaterial(
                new SolidColorBrush(Color.FromArgb(80, 255, 255, 255)),
                22));
            material.Freeze();
            group.Children.Add(new GeometryModel3D(geometry, material)
            {
                BackMaterial = material
            });
        }
        if (renderedTriangles == 0 || !IsFinite(minimum) || !IsFinite(maximum))
            throw new InvalidDataException("Preview geometry contains no valid triangles.");

        if (framingBounds is SmoGeometryPreviewBounds frame)
        {
            if (!IsFinite(frame.Minimum) || !IsFinite(frame.Maximum) ||
                frame.Minimum.X >= frame.Maximum.X ||
                frame.Minimum.Y >= frame.Maximum.Y ||
                frame.Minimum.Z >= frame.Maximum.Z)
            {
                throw new ArgumentException(
                    "Preview framing bounds are invalid.",
                    nameof(framingBounds));
            }
            minimum = frame.Minimum;
            maximum = frame.Maximum;
        }

        Vector3 midpoint = (minimum + maximum) * 0.5f;
        var center = new Point3D(midpoint.X, midpoint.Y, midpoint.Z);
        double radius = Math.Max((maximum - minimum).Length() * 0.5, 0.001);
        var viewport = new Viewport3D
        {
            Width = width,
            Height = height,
            ClipToBounds = true
        };
        var cameraDirection = new Vector3D(1.25, 0.85, 1.5);
        cameraDirection.Normalize();
        double halfFov = 18 * Math.PI / 180;
        double distance = Math.Max(radius / Math.Tan(halfFov) * 1.18, 1);
        viewport.Camera = new PerspectiveCamera
        {
            Position = center + cameraDirection * distance,
            LookDirection = -cameraDirection * distance,
            UpDirection = new Vector3D(0, 1, 0),
            FieldOfView = 36,
            NearPlaneDistance = Math.Max(0.01, distance - radius * 2.5),
            FarPlaneDistance = distance + radius * 3.5 + 1
        };
        viewport.Children.Add(new ModelVisual3D { Content = group });
        viewport.Measure(new Size(width, height));
        viewport.Arrange(new Rect(0, 0, width, height));
        viewport.UpdateLayout();

        var bitmap = new RenderTargetBitmap(
            width,
            height,
            96,
            96,
            PixelFormats.Pbgra32);
        bitmap.Render(viewport);
        bitmap.Freeze();
        return bitmap;
    }

    private static Color ResolveColor(SmoGeometryPreviewMesh mesh)
    {
        if (mesh.DiffuseColorsArgb.Length == 0)
            return mesh.FallbackColor;
        long red = 0;
        long green = 0;
        long blue = 0;
        int step = Math.Max(1, mesh.DiffuseColorsArgb.Length / 256);
        int samples = 0;
        for (int index = 0; index < mesh.DiffuseColorsArgb.Length; index += step)
        {
            uint color = mesh.DiffuseColorsArgb[index];
            red += (color >> 16) & 0xFF;
            green += (color >> 8) & 0xFF;
            blue += color & 0xFF;
            samples++;
        }
        return Color.FromRgb(
            (byte)(red / samples),
            (byte)(green / samples),
            (byte)(blue / samples));
    }

    private static bool IsFinite(Vector3 value) =>
        float.IsFinite(value.X) &&
        float.IsFinite(value.Y) &&
        float.IsFinite(value.Z);
}

public sealed record SmoGeometryPreviewMesh(
    Vector3[] Positions,
    Vector3[] Normals,
    Vector2[] TextureCoordinates,
    uint[] TriangleIndices,
    uint[] DiffuseColorsArgb,
    Color FallbackColor);

public readonly record struct SmoGeometryPreviewBounds(
    Vector3 Minimum,
    Vector3 Maximum);
