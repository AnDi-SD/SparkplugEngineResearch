using System.Numerics;

namespace SmoViewer.Core;

public readonly record struct SmoTextureTileBounds(
    int FirstX,
    int LastX,
    int FirstY,
    int LastY)
{
    public int TileCount => checked((LastX - FirstX + 1) * (LastY - FirstY + 1));
}

/// <summary>
/// Converts authored, repeating SMO texture coordinates into the unit tiles
/// needed by the viewer's software texture-composition paths.
/// </summary>
public static class SmoTextureCoordinateTiling
{
    private const int MaximumRasterizedTileCount = 4096;

    public static bool TryGetTriangleTileBounds(
        Vector2 a,
        Vector2 b,
        Vector2 c,
        out SmoTextureTileBounds bounds)
    {
        bounds = default;
        if (!IsFinite(a) || !IsFinite(b) || !IsFinite(c))
            return false;

        float minimumX = MathF.Min(a.X, MathF.Min(b.X, c.X));
        float maximumX = MathF.Max(a.X, MathF.Max(b.X, c.X));
        float minimumY = MathF.Min(a.Y, MathF.Min(b.Y, c.Y));
        float maximumY = MathF.Max(a.Y, MathF.Max(b.Y, c.Y));
        if (minimumX < int.MinValue || maximumX >= int.MaxValue ||
            minimumY < int.MinValue || maximumY >= int.MaxValue)
        {
            return false;
        }

        int firstX = (int)MathF.Floor(minimumX);
        int lastX = (int)MathF.Floor(maximumX);
        int firstY = (int)MathF.Floor(minimumY);
        int lastY = (int)MathF.Floor(maximumY);
        long tileCount =
            ((long)lastX - firstX + 1) * ((long)lastY - firstY + 1);
        if (tileCount > MaximumRasterizedTileCount)
            return false;

        bounds = new SmoTextureTileBounds(firstX, lastX, firstY, lastY);
        return true;
    }

    public static float Wrap(float value) => value - MathF.Floor(value);

    private static bool IsFinite(Vector2 value) =>
        float.IsFinite(value.X) && float.IsFinite(value.Y);
}
