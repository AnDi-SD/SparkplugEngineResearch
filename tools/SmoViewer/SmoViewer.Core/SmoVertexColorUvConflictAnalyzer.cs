namespace SmoViewer.Core;

public sealed record SmoTriangleAtlasResolutionInfo(
    int DesiredCellSize,
    int CellSize,
    int Width,
    int Height,
    bool IsCapacityLimited);

/// <summary>
/// Detects vertices that intentionally share UV coordinates while carrying
/// different diffuse RGB values. Such data cannot be represented by baking
/// vertex colours back into one copy of the source texture.
/// </summary>
public static class SmoVertexColorUvConflictAnalyzer
{
    private const float QuantizationScale = 100_000f;
    private const int TriangleAtlasTargetSpan = 768;
    private const int MinimumTriangleAtlasCellSize = 12;
    private const int MaximumTriangleAtlasDimension = 2_048;
    private const int MaximumTriangleAtlasPixels = 1_048_576;

    /// <summary>
    /// Empty border around every private-atlas triangle. The required cell span
    /// includes both borders plus one endpoint so N source-texel intervals are
    /// represented by N+1 destination samples without bleeding from neighbours.
    /// </summary>
    public const int TriangleAtlasPadding = 2;

    /// <summary>
    /// Upper bound for the Viewer's private per-triangle texture atlas. The
    /// 2,048-triangle limit covers the authored H_DoorB meshes in Alfea01 while
    /// keeping the duplicated WPF preview geometry and atlas work bounded.
    /// </summary>
    public const int MaximumTriangleAtlasTriangles = 2_048;

    /// <summary>
    /// Chooses the private WPF triangle-atlas resolution from authored data, not
    /// object semantics. UV span multiplied by source texture dimensions gives
    /// the texel density requested by the widest triangle. A common WPF bitmap
    /// budget is then applied uniformly to every mesh; small and medium meshes
    /// retain the requested density exactly, while extreme multi-tile geometry
    /// cannot allocate an unbounded per-mesh bitmap.
    /// </summary>
    public static int GetPreviewTriangleAtlasCellSize(
        SmoMesh mesh,
        SmoTexture? texture)
    {
        return GetPreviewTriangleAtlasResolution(mesh, texture).CellSize;
    }

    /// <summary>
    /// Reports authored texel demand separately from the bitmap size that the
    /// WPF preview can allocate. This keeps a resource limit observable instead
    /// of presenting a reduced preview as a property of the SMO content.
    /// </summary>
    public static SmoTriangleAtlasResolutionInfo
        GetPreviewTriangleAtlasResolution(
            SmoMesh mesh,
            SmoTexture? texture)
    {
        ArgumentNullException.ThrowIfNull(mesh);
        if (mesh.TriangleCount <= 0)
        {
            return new SmoTriangleAtlasResolutionInfo(
                MinimumTriangleAtlasCellSize,
                MinimumTriangleAtlasCellSize,
                0,
                0,
                false);
        }

        int columns = (int)Math.Ceiling(Math.Sqrt(mesh.TriangleCount));
        int rows = (mesh.TriangleCount + columns - 1) / columns;
        int desired = texture is not null && mesh.HasTextureCoordinates
            ? checked((int)MathF.Ceiling(
                  MaximumTriangleSourceSpan(mesh, texture))) +
              TriangleAtlasPadding * 2 + 1
            : TriangleAtlasTargetSpan / columns;
        int dimensionCapacity = Math.Min(
            MaximumTriangleAtlasDimension / columns,
            MaximumTriangleAtlasDimension / rows);
        int pixelCapacity = (int)Math.Floor(Math.Sqrt(
            MaximumTriangleAtlasPixels / (double)(columns * rows)));
        int capacity = Math.Max(
            MinimumTriangleAtlasCellSize,
            Math.Min(dimensionCapacity, pixelCapacity));
        desired = Math.Max(desired, MinimumTriangleAtlasCellSize);
        int cellSize = Math.Min(desired, capacity);
        return new SmoTriangleAtlasResolutionInfo(
            desired,
            cellSize,
            checked(columns * cellSize),
            checked(rows * cellSize),
            cellSize < desired);
    }

    public static bool HasConflictingSharedCoordinates(SmoMesh mesh)
    {
        ArgumentNullException.ThrowIfNull(mesh);
        if (!mesh.HasTextureCoordinates || !mesh.HasDiffuseColors)
            return false;

        var rgbByCoordinate = new Dictionary<(int U, int V), uint>();
        for (int vertex = 0; vertex < mesh.VertexCount; vertex++)
        {
            var uv = mesh.TextureCoordinates[vertex];
            var key = (
                Quantize(uv.X),
                Quantize(uv.Y));
            uint rgb = mesh.DiffuseColorsArgb[vertex] & 0x00FFFFFF;
            if (rgbByCoordinate.TryGetValue(key, out uint previousRgb) &&
                previousRgb != rgb)
            {
                return true;
            }
            rgbByCoordinate.TryAdd(key, rgb);
        }
        return false;
    }

    private static int Quantize(float value)
    {
        float scaled = MathF.Round(value * QuantizationScale);
        if (scaled <= int.MinValue)
            return int.MinValue;
        if (scaled >= int.MaxValue)
            return int.MaxValue;
        return (int)scaled;
    }

    private static float MaximumTriangleSourceSpan(
        SmoMesh mesh,
        SmoTexture texture)
    {
        float maximum = 0;
        for (int triangle = 0; triangle < mesh.TriangleCount; triangle++)
        {
            int offset = triangle * 3;
            System.Numerics.Vector2 a = mesh.TextureCoordinates[
                checked((int)mesh.TriangleIndices[offset])];
            System.Numerics.Vector2 b = mesh.TextureCoordinates[
                checked((int)mesh.TriangleIndices[offset + 1])];
            System.Numerics.Vector2 c = mesh.TextureCoordinates[
                checked((int)mesh.TriangleIndices[offset + 2])];
            float horizontal = (MathF.Max(a.X, MathF.Max(b.X, c.X)) -
                                MathF.Min(a.X, MathF.Min(b.X, c.X))) *
                               texture.Width;
            float vertical = (MathF.Max(a.Y, MathF.Max(b.Y, c.Y)) -
                              MathF.Min(a.Y, MathF.Min(b.Y, c.Y))) *
                             texture.Height;
            maximum = MathF.Max(maximum, MathF.Max(horizontal, vertical));
        }
        return maximum;
    }
}
