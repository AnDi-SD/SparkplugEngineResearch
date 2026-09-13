namespace SmoViewer.Core;

/// <summary>
/// Classifies decoded vertex-diffuse streams before they are used to modulate
/// a texture. Some character exporters leave an almost entirely black,
/// two-colour placeholder in layouts that always reserve diffuse bytes.
/// </summary>
public static class SmoVertexColorUsage
{
    private const uint RgbMask = 0x00FFFFFF;
    private const uint OpaqueBlack = 0xFF000000;
    private const int DominantBlackNumerator = 95;
    private const int DominantBlackDenominator = 100;

    public static bool ShouldModulateTexture(SmoMesh mesh)
    {
        ArgumentNullException.ThrowIfNull(mesh);
        return mesh.HasDiffuseColors &&
               ShouldModulateTexture(mesh.VertexFormat, mesh.DiffuseColorsArgb);
    }

    public static bool ShouldModulateTexture(
        uint vertexFormat,
        ReadOnlySpan<uint> diffuseColorsArgb)
    {
        if (diffuseColorsArgb.IsEmpty ||
            vertexFormat is not (0x0100 or 0x0900 or 0x093E or 0x0940 or
                0x097E or 0x1940 or 0x197E))
        {
            return false;
        }

        uint firstRgb = diffuseColorsArgb[0] & RgbMask;
        uint secondRgb = 0;
        bool hasSecondRgb = false;
        bool hasMoreThanTwoRgbValues = false;
        int opaqueBlackCount = diffuseColorsArgb[0] == OpaqueBlack ? 1 : 0;

        for (int index = 1; index < diffuseColorsArgb.Length; index++)
        {
            uint rgb = diffuseColorsArgb[index] & RgbMask;
            if (diffuseColorsArgb[index] == OpaqueBlack)
                opaqueBlackCount++;

            if (rgb == firstRgb)
                continue;

            if (!hasSecondRgb)
            {
                secondRgb = rgb;
                hasSecondRgb = true;
            }
            else if (rgb != secondRgb)
            {
                hasMoreThanTwoRgbValues = true;
            }
        }

        if (!hasSecondRgb)
            return false;

        // PC character layouts can contain a disabled diffuse stream with a
        // handful of stale/default vertices. Troll mesh [86] is 425 black plus
        // 10 white vertices; treating that as lighting turns a valid colour
        // atlas black. Keep this exception narrow so real face/eyelash
        // gradients, which contain more than two RGB values, remain active.
        bool isCharacterLayout = vertexFormat is 0x097E or 0x197E;
        bool isDominantBlackTwoColourPlaceholder =
            isCharacterLayout &&
            !hasMoreThanTwoRgbValues &&
            (long)opaqueBlackCount * DominantBlackDenominator >
            (long)diffuseColorsArgb.Length * DominantBlackNumerator;
        return !isDominantBlackTwoColourPlaceholder;
    }
}
