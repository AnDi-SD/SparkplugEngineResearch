using SmoViewer.Core;
using SmoViewer.Scene;
using System.IO;
using System.Numerics;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Media.Media3D;

namespace SmoViewer.Rendering.Wpf;

/// <summary>
/// Creates small frozen WPF images from the same decoded scene resources used
/// by the shared viewport. The service owns presentation only; parsing and
/// material/texture resolution remain in Core and Scene.
/// </summary>
public static class SmoThumbnailRenderer
{
    public static BitmapSource CreateTextureThumbnail(
        int width,
        int height,
        ReadOnlyMemory<byte> bgra32Pixels,
        int maximumWidth = 320,
        int maximumHeight = 160)
    {
        if (width <= 0 || height <= 0)
            throw new ArgumentOutOfRangeException(nameof(width));
        if (bgra32Pixels.Length != checked(width * height * 4))
        {
            throw new ArgumentException(
                "The BGRA32 buffer does not match the declared dimensions.",
                nameof(bgra32Pixels));
        }

        double scale = Math.Min(
            1,
            Math.Min((double)maximumWidth / width, (double)maximumHeight / height));
        int targetWidth = Math.Max(1, (int)Math.Round(width * scale));
        int targetHeight = Math.Max(1, (int)Math.Round(height * scale));
        byte[] pixels = ResizeBgraNearest(
            bgra32Pixels.Span,
            width,
            height,
            targetWidth,
            targetHeight);
        BitmapSource bitmap = BitmapSource.Create(
            targetWidth,
            targetHeight,
            96,
            96,
            PixelFormats.Bgra32,
            null,
            pixels,
            targetWidth * 4);
        bitmap.Freeze();
        return bitmap;
    }

    public static BitmapSource CreateModelThumbnail(
        SmoSceneMesh sceneMesh,
        int width = 320,
        int height = 128) =>
        CreateModelThumbnail([sceneMesh], width, height);

    /// <summary>
    /// Renders scene occurrences in world space as a single thumbnail. This
    /// keeps both ordinary split models and mixed placed/baked assemblies
    /// aligned without duplicating rendering/material logic in editor UIs.
    /// </summary>
    public static BitmapSource CreateModelThumbnail(
        IReadOnlyList<SmoSceneMesh> sceneMeshes,
        int width = 320,
        int height = 128)
    {
        ArgumentNullException.ThrowIfNull(sceneMeshes);
        if (sceneMeshes.Count == 0)
            throw new ArgumentException("At least one mesh is required.", nameof(sceneMeshes));
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
        foreach (SmoSceneMesh sceneMesh in sceneMeshes)
        {
            ArgumentNullException.ThrowIfNull(sceneMesh);
            MeshGeometry3D geometry = CreateGeometry(
                sceneMesh.Mesh,
                sceneMesh.WorldTransform,
                out Vector3 meshMinimum,
                out Vector3 meshMaximum);
            minimum = Vector3.Min(minimum, meshMinimum);
            maximum = Vector3.Max(maximum, meshMaximum);
            if (sceneMesh.Texture is null && sceneMesh.Mesh.HasDiffuseColors)
            {
                foreach (GeometryModel3D colorModel in CreateVertexColorModels(
                             sceneMesh.Mesh,
                             geometry))
                {
                    group.Children.Add(colorModel);
                }
            }
            else
            {
                Material material = CreateMaterial(sceneMesh);
                group.Children.Add(new GeometryModel3D(geometry, material)
                {
                    BackMaterial = material
                });
            }
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

    private static MeshGeometry3D CreateGeometry(
        SmoMesh mesh,
        Matrix4x4 worldTransform,
        out Vector3 minimum,
        out Vector3 maximum)
    {
        if (mesh.Positions.Length == 0 || mesh.TriangleIndices.Length < 3)
            throw new InvalidDataException("A thumbnail requires non-empty triangle geometry.");

        var positions = new Point3DCollection(mesh.Positions.Length);
        minimum = new Vector3(float.PositiveInfinity);
        maximum = new Vector3(float.NegativeInfinity);
        foreach (Vector3 source in mesh.Positions)
        {
            Vector3 world = Vector3.Transform(source, worldTransform);
            var converted = new Vector3(world.X, world.Y, -world.Z);
            positions.Add(new Point3D(converted.X, converted.Y, converted.Z));
            minimum = Vector3.Min(minimum, converted);
            maximum = Vector3.Max(maximum, converted);
        }

        var indices = new Int32Collection(mesh.TriangleIndices.Length);
        for (int triangle = 0; triangle + 2 < mesh.TriangleIndices.Length; triangle += 3)
        {
            uint a = mesh.TriangleIndices[triangle];
            uint b = mesh.TriangleIndices[triangle + 1];
            uint c = mesh.TriangleIndices[triangle + 2];
            if (a >= positions.Count || b >= positions.Count || c >= positions.Count)
                continue;
            indices.Add((int)a);
            indices.Add((int)c);
            indices.Add((int)b);
        }

        var geometry = new MeshGeometry3D
        {
            Positions = positions,
            TriangleIndices = indices
        };
        if (mesh.HasNormals)
        {
            geometry.Normals = new Vector3DCollection(mesh.Normals.Select(normal =>
            {
                Vector3 world = Vector3.TransformNormal(normal, worldTransform);
                if (world.LengthSquared() > 1e-12f)
                    world = Vector3.Normalize(world);
                return new Vector3D(world.X, world.Y, -world.Z);
            }));
        }
        if (mesh.HasTextureCoordinates)
        {
            geometry.TextureCoordinates = new PointCollection(
                mesh.TextureCoordinates.Select(uv => new Point(uv.X, uv.Y)));
        }
        geometry.Freeze();

        return geometry;
    }

    private static IEnumerable<GeometryModel3D> CreateVertexColorModels(
        SmoMesh mesh,
        MeshGeometry3D sourceGeometry)
    {
        var groups = new Dictionary<int, VertexColorGroup>();
        for (int triangle = 0; triangle + 2 < mesh.TriangleIndices.Length; triangle += 3)
        {
            uint a = mesh.TriangleIndices[triangle];
            uint b = mesh.TriangleIndices[triangle + 1];
            uint c = mesh.TriangleIndices[triangle + 2];
            if (a >= mesh.DiffuseColorsArgb.Length ||
                b >= mesh.DiffuseColorsArgb.Length ||
                c >= mesh.DiffuseColorsArgb.Length)
            {
                continue;
            }

            uint colorA = mesh.DiffuseColorsArgb[a];
            uint colorB = mesh.DiffuseColorsArgb[b];
            uint colorC = mesh.DiffuseColorsArgb[c];
            int red = (int)(((colorA >> 16) & 0xFF) +
                            ((colorB >> 16) & 0xFF) +
                            ((colorC >> 16) & 0xFF)) / 3;
            int green = (int)(((colorA >> 8) & 0xFF) +
                              ((colorB >> 8) & 0xFF) +
                              ((colorC >> 8) & 0xFF)) / 3;
            int blue = (int)((colorA & 0xFF) +
                             (colorB & 0xFF) +
                             (colorC & 0xFF)) / 3;
            // Four levels per channel retain the authored palette while
            // bounding one thumbnail to at most 64 WPF material groups.
            int key = ((red >> 6) << 4) | ((green >> 6) << 2) | (blue >> 6);
            if (!groups.TryGetValue(key, out VertexColorGroup? colorGroup))
            {
                colorGroup = new VertexColorGroup();
                groups.Add(key, colorGroup);
            }
            colorGroup.Indices.Add((int)a);
            colorGroup.Indices.Add((int)c);
            colorGroup.Indices.Add((int)b);
            colorGroup.Red += red;
            colorGroup.Green += green;
            colorGroup.Blue += blue;
            colorGroup.TriangleCount++;
        }

        foreach (VertexColorGroup colorGroup in groups.Values)
        {
            var geometry = new MeshGeometry3D
            {
                Positions = sourceGeometry.Positions,
                Normals = sourceGeometry.Normals,
                TriangleIndices = new Int32Collection(colorGroup.Indices)
            };
            geometry.Freeze();
            int divisor = Math.Max(1, colorGroup.TriangleCount);
            Color color = Color.FromRgb(
                (byte)(colorGroup.Red / divisor),
                (byte)(colorGroup.Green / divisor),
                (byte)(colorGroup.Blue / divisor));
            Material material = CreateLitMaterial(new SolidColorBrush(color));
            yield return new GeometryModel3D(geometry, material)
            {
                BackMaterial = material
            };
        }
    }

    private static Material CreateMaterial(SmoSceneMesh sceneMesh)
    {
        Brush brush;
        if (sceneMesh.Texture is SmoTexture texture)
        {
            BitmapSource bitmap = CreateTextureThumbnail(
                texture.Width,
                texture.Height,
                texture.Bgra32Pixels,
                256,
                256);
            brush = new ImageBrush(bitmap)
            {
                Stretch = Stretch.Fill,
                TileMode = TileMode.None
            };
        }
        else
        {
            brush = new SolidColorBrush(ResolveDiffuseColor(sceneMesh));
        }
        return CreateLitMaterial(brush);
    }

    private static Material CreateLitMaterial(Brush brush)
    {
        brush.Freeze();
        var material = new MaterialGroup();
        material.Children.Add(new DiffuseMaterial(brush));
        material.Children.Add(new SpecularMaterial(
            new SolidColorBrush(Color.FromArgb(90, 255, 255, 255)),
            24));
        material.Freeze();
        return material;
    }

    private sealed class VertexColorGroup
    {
        public List<int> Indices { get; } = [];
        public long Red { get; set; }
        public long Green { get; set; }
        public long Blue { get; set; }
        public int TriangleCount { get; set; }
    }

    private static Color ResolveDiffuseColor(SmoSceneMesh sceneMesh)
    {
        if (sceneMesh.MaterialColorArgb is uint argb)
        {
            return Color.FromArgb(
                (byte)(argb >> 24),
                (byte)(argb >> 16),
                (byte)(argb >> 8),
                (byte)argb);
        }
        if (sceneMesh.Mesh.DiffuseColorsArgb.Length > 0)
        {
            long red = 0;
            long green = 0;
            long blue = 0;
            int step = Math.Max(1, sceneMesh.Mesh.DiffuseColorsArgb.Length / 128);
            int samples = 0;
            for (int index = 0; index < sceneMesh.Mesh.DiffuseColorsArgb.Length; index += step)
            {
                uint color = sceneMesh.Mesh.DiffuseColorsArgb[index];
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
        return Color.FromRgb(174, 188, 205);
    }

    private static byte[] ResizeBgraNearest(
        ReadOnlySpan<byte> source,
        int sourceWidth,
        int sourceHeight,
        int targetWidth,
        int targetHeight)
    {
        if (sourceWidth == targetWidth && sourceHeight == targetHeight)
            return source.ToArray();

        byte[] target = new byte[checked(targetWidth * targetHeight * 4)];
        for (int y = 0; y < targetHeight; y++)
        {
            int sourceY = Math.Min(sourceHeight - 1, y * sourceHeight / targetHeight);
            for (int x = 0; x < targetWidth; x++)
            {
                int sourceX = Math.Min(sourceWidth - 1, x * sourceWidth / targetWidth);
                int sourceOffset = (sourceY * sourceWidth + sourceX) * 4;
                int targetOffset = (y * targetWidth + x) * 4;
                source.Slice(sourceOffset, 4).CopyTo(target.AsSpan(targetOffset, 4));
            }
        }
        return target;
    }
}
