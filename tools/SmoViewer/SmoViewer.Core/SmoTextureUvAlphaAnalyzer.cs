using System.Numerics;

namespace SmoViewer.Core;

/// <summary>
/// Texture-alpha evidence sampled only from texels covered by decoded UV0
/// triangles. <see cref="IsReliable"/> is false when the mesh/texture layout is
/// incomplete, UVs cannot be reduced to a single repeated tile per triangle,
/// or no pixel centre is covered.
/// </summary>
public readonly record struct SmoTextureUvAlphaCoverage(
    bool IsReliable,
    int SampledTexelCount,
    int FullyTransparentTexelCount,
    int PartialAlphaTexelCount,
    int OpaqueTexelCount)
{
    public bool HasPartialAlpha =>
        IsReliable && PartialAlphaTexelCount > 0;

    public bool HasTransparentAndPartialAlpha =>
        IsReliable &&
        FullyTransparentTexelCount > 0 &&
        PartialAlphaTexelCount > 0;

    public override string ToString() => IsReliable
        ? $"sampled={SampledTexelCount},a0={FullyTransparentTexelCount}," +
          $"partial={PartialAlphaTexelCount},a255={OpaqueTexelCount}"
        : "<unavailable>";
}

/// <summary>
/// Produces conservative UV-aware alpha evidence. It intentionally does not
/// turn arbitrary textures containing transparent pixels into blended
/// materials; callers first establish a confirmed material/consumer context.
/// </summary>
public static class SmoTextureUvAlphaAnalyzer
{
    private const float EdgeEpsilon = 0.0000001f;

    public static SmoTextureUvAlphaCoverage Analyze(
        SmoMesh mesh,
        SmoTexture texture)
    {
        ArgumentNullException.ThrowIfNull(mesh);
        ArgumentNullException.ThrowIfNull(texture);
        if (!mesh.HasTextureCoordinates || mesh.TriangleIndices.Length < 3 ||
            texture.Width <= 0 || texture.Height <= 0)
        {
            return default;
        }

        long pixelCount64 = (long)texture.Width * texture.Height;
        if (pixelCount64 <= 0 || pixelCount64 > int.MaxValue ||
            texture.Bgra32Pixels.Length < pixelCount64 * 4)
        {
            return default;
        }

        int pixelCount = checked((int)pixelCount64);
        bool[] covered = new bool[pixelCount];
        for (int triangle = 0;
             triangle + 2 < mesh.TriangleIndices.Length;
             triangle += 3)
        {
            int ia = checked((int)mesh.TriangleIndices[triangle]);
            int ib = checked((int)mesh.TriangleIndices[triangle + 1]);
            int ic = checked((int)mesh.TriangleIndices[triangle + 2]);
            if ((uint)ia >= (uint)mesh.TextureCoordinates.Length ||
                (uint)ib >= (uint)mesh.TextureCoordinates.Length ||
                (uint)ic >= (uint)mesh.TextureCoordinates.Length)
            {
                return default;
            }

            Vector2 a = mesh.TextureCoordinates[ia];
            Vector2 b = mesh.TextureCoordinates[ib];
            Vector2 c = mesh.TextureCoordinates[ic];
            if (!TryNormalizeRepeatedTile(ref a, ref b, ref c))
            {
                return default;
            }

            float area = Cross(b - a, c - a);
            if (MathF.Abs(area) <= EdgeEpsilon)
                continue;

            int minX = Math.Clamp(
                (int)MathF.Ceiling(
                    MathF.Min(a.X, MathF.Min(b.X, c.X)) * texture.Width -
                    0.5f),
                0,
                texture.Width - 1);
            int maxX = Math.Clamp(
                (int)MathF.Floor(
                    MathF.Max(a.X, MathF.Max(b.X, c.X)) * texture.Width -
                    0.5f),
                0,
                texture.Width - 1);
            int minY = Math.Clamp(
                (int)MathF.Ceiling(
                    MathF.Min(a.Y, MathF.Min(b.Y, c.Y)) * texture.Height -
                    0.5f),
                0,
                texture.Height - 1);
            int maxY = Math.Clamp(
                (int)MathF.Floor(
                    MathF.Max(a.Y, MathF.Max(b.Y, c.Y)) * texture.Height -
                    0.5f),
                0,
                texture.Height - 1);

            for (int y = minY; y <= maxY; y++)
            for (int x = minX; x <= maxX; x++)
            {
                Vector2 point = new(
                    (x + 0.5f) / texture.Width,
                    (y + 0.5f) / texture.Height);
                if (ContainsPoint(a, b, c, point))
                    covered[y * texture.Width + x] = true;
            }
        }

        ReadOnlySpan<byte> pixels = texture.Bgra32Pixels.Span;
        int sampled = 0;
        int transparent = 0;
        int partial = 0;
        int opaque = 0;
        for (int pixel = 0; pixel < covered.Length; pixel++)
        {
            if (!covered[pixel])
                continue;
            sampled++;
            byte alpha = pixels[pixel * 4 + 3];
            if (alpha == 0)
                transparent++;
            else if (alpha == byte.MaxValue)
                opaque++;
            else
                partial++;
        }

        return sampled == 0
            ? default
            : new SmoTextureUvAlphaCoverage(
                true, sampled, transparent, partial, opaque);
    }

    private static bool TryNormalizeRepeatedTile(
        ref Vector2 a,
        ref Vector2 b,
        ref Vector2 c)
    {
        if (!float.IsFinite(a.X) || !float.IsFinite(a.Y) ||
            !float.IsFinite(b.X) || !float.IsFinite(b.Y) ||
            !float.IsFinite(c.X) || !float.IsFinite(c.Y))
        {
            return false;
        }

        a = new Vector2(SnapInteger(a.X), SnapInteger(a.Y));
        b = new Vector2(SnapInteger(b.X), SnapInteger(b.Y));
        c = new Vector2(SnapInteger(c.X), SnapInteger(c.Y));
        if (!TryNormalizeAxis(ref a.X, ref b.X, ref c.X) ||
            !TryNormalizeAxis(ref a.Y, ref b.Y, ref c.Y))
        {
            return false;
        }
        return true;
    }

    private static bool TryNormalizeAxis(ref float a, ref float b, ref float c)
    {
        const float tolerance = 0.00001f;
        float minimum = MathF.Min(a, MathF.Min(b, c));
        float tile = MathF.Floor(minimum);
        a -= tile;
        b -= tile;
        c -= tile;
        return a >= -tolerance && a <= 1 + tolerance &&
               b >= -tolerance && b <= 1 + tolerance &&
               c >= -tolerance && c <= 1 + tolerance;
    }

    private static float SnapInteger(float value)
    {
        float rounded = MathF.Round(value);
        return MathF.Abs(value - rounded) <= 0.00001f
            ? rounded
            : value;
    }

    private static bool ContainsPoint(
        Vector2 a,
        Vector2 b,
        Vector2 c,
        Vector2 point)
    {
        float edge0 = Cross(b - a, point - a);
        float edge1 = Cross(c - b, point - b);
        float edge2 = Cross(a - c, point - c);
        bool hasNegative =
            edge0 < -EdgeEpsilon || edge1 < -EdgeEpsilon ||
            edge2 < -EdgeEpsilon;
        bool hasPositive =
            edge0 > EdgeEpsilon || edge1 > EdgeEpsilon ||
            edge2 > EdgeEpsilon;
        return !(hasNegative && hasPositive);
    }

    private static float Cross(Vector2 left, Vector2 right) =>
        left.X * right.Y - left.Y * right.X;
}
