using System.Numerics;

namespace SmoImporter.Core;

/// <summary>
/// Computes the same conservative height-and-center fit used by the importer UI.
/// The source proportions are never imposed on the replacement model.
/// </summary>
public static class ReplacementTransformFitter
{
    public static ReplacementTransform FitByHeightAndCenter(
        IEnumerable<Vector3> sourcePositions,
        IEnumerable<Vector3> replacementPositions)
    {
        ArgumentNullException.ThrowIfNull(sourcePositions);
        ArgumentNullException.ThrowIfNull(replacementPositions);

        Vector3[] source = sourcePositions.ToArray();
        Vector3[] replacement = replacementPositions.ToArray();
        if (source.Length == 0)
            throw new InvalidOperationException("The source model has no vertices to fit against.");
        if (replacement.Length == 0)
            throw new InvalidOperationException("The replacement model has no vertices to fit.");
        if (source.Any(value => !IsFinite(value)) || replacement.Any(value => !IsFinite(value)))
            throw new InvalidDataException("Model bounds contain NaN or infinity.");

        (Vector3 sourceMin, Vector3 sourceMax) = Bounds(source);
        (Vector3 replacementMin, Vector3 replacementMax) = Bounds(replacement);
        Vector3 sourceSize = sourceMax - sourceMin;
        Vector3 replacementSize = replacementMax - replacementMin;
        float sourceHeight = sourceSize.Y > 0.000001f
            ? sourceSize.Y
            : sourceSize.Length();
        float replacementHeight = replacementSize.Y > 0.000001f
            ? replacementSize.Y
            : replacementSize.Length();
        if (!float.IsFinite(sourceHeight) || sourceHeight <= 0.000001f)
            throw new InvalidOperationException("The source model has no measurable extent.");
        if (!float.IsFinite(replacementHeight) || replacementHeight <= 0.000001f)
            throw new InvalidOperationException("The replacement model has no measurable extent.");

        float scale = sourceHeight / replacementHeight;
        Vector3 sourceCenter = (sourceMin + sourceMax) * 0.5f;
        Vector3 replacementCenter = (replacementMin + replacementMax) * 0.5f;
        Vector3 translation = sourceCenter - replacementCenter * scale;
        return new ReplacementTransform(scale, Vector3.Zero, translation);
    }

    private static (Vector3 Min, Vector3 Max) Bounds(IReadOnlyList<Vector3> values)
    {
        Vector3 min = values[0];
        Vector3 max = values[0];
        for (int index = 1; index < values.Count; index++)
        {
            min = Vector3.Min(min, values[index]);
            max = Vector3.Max(max, values[index]);
        }
        return (min, max);
    }

    private static bool IsFinite(Vector3 value) =>
        float.IsFinite(value.X) &&
        float.IsFinite(value.Y) &&
        float.IsFinite(value.Z);
}
