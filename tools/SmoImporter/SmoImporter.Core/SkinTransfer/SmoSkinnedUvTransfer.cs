using System.Numerics;

namespace SmoImporter.Core;

internal static class SmoSkinnedUvTransfer
{
    public static Vector2 SecondaryOrPrimary(
        IReadOnlyList<Vector2> primary,
        IReadOnlyList<Vector2> secondary,
        int vertex,
        int vertexCount)
    {
        if ((uint)vertex >= (uint)vertexCount)
            throw new ArgumentOutOfRangeException(nameof(vertex));
        if (secondary.Count == vertexCount)
            return secondary[vertex];
        return primary.Count == vertexCount ? primary[vertex] : Vector2.Zero;
    }

    public static bool HasDistinctSecondary(
        IReadOnlyList<Vector2> primary,
        IReadOnlyList<Vector2> secondary,
        int vertexCount)
    {
        if (secondary.Count != vertexCount)
            return false;
        if (primary.Count != vertexCount)
            return true;
        for (int vertex = 0; vertex < vertexCount; vertex++)
        {
            if (secondary[vertex] != primary[vertex])
                return true;
        }
        return false;
    }
}
