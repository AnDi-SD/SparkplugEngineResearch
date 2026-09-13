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
        if (!mesh.HasDiffuseColors)
            return false;
        if (ShouldModulateTexture(mesh.VertexFormat, mesh.DiffuseColorsArgb))
            return true;

        // Rigid level effects and foliage commonly store one authored tint in
        // every vertex. Character layouts also carry uniform placeholder
        // streams, so keep this fallback restricted to non-skinned layouts.
        uint uniformRgb = mesh.DiffuseColorsArgb[0] & RgbMask;
        return !mesh.HasSkinningData &&
               HasUniformRgb(mesh) &&
               uniformRgb != RgbMask &&
               mesh.VertexFormat is 0x0900 or 0x093E or 0x0940 or 0x1900 or
                   0x1940;
    }

    public static bool HasUniformRgb(SmoMesh mesh)
    {
        ArgumentNullException.ThrowIfNull(mesh);
        if (!mesh.HasDiffuseColors)
            return false;

        uint firstRgb = mesh.DiffuseColorsArgb[0] & RgbMask;
        for (int index = 1; index < mesh.DiffuseColorsArgb.Length; index++)
        {
            if ((mesh.DiffuseColorsArgb[index] & RgbMask) != firstRgb)
                return false;
        }
        return true;
    }

    public static bool HasAuthoredAlphaGradient(SmoMesh mesh)
    {
        ArgumentNullException.ThrowIfNull(mesh);
        if (!HasUniformRgb(mesh))
            return false;

        byte firstAlpha = (byte)(mesh.DiffuseColorsArgb[0] >> 24);
        bool hasNonOpaqueAlpha = firstAlpha < byte.MaxValue;
        bool hasAlphaVariation = false;
        for (int index = 1; index < mesh.DiffuseColorsArgb.Length; index++)
        {
            byte alpha = (byte)(mesh.DiffuseColorsArgb[index] >> 24);
            hasNonOpaqueAlpha |= alpha < byte.MaxValue;
            hasAlphaVariation |= alpha != firstAlpha;
        }
        return hasNonOpaqueAlpha && hasAlphaVariation;
    }

    /// <summary>
    /// Detects the second observed rigid-light form: mixed RGB together with
    /// an explicit zero-to-visible alpha ramp under the FinalBlend 2,
    /// companion-state 2 material family. Ordinary baked level lighting often
    /// contains small non-255 alpha variations, so mixed RGB alone is not
    /// sufficient.
    /// </summary>
    public static bool HasAuthoredAlphaGradient(
        SmoMesh mesh,
        SmoMaterialRenderStateInfo? renderState)
    {
        ArgumentNullException.ThrowIfNull(mesh);
        if (HasAuthoredAlphaGradient(mesh))
            return true;
        if (mesh.HasSkinningData || !mesh.HasDiffuseColors ||
            renderState?.FinalBlendOperation != 0x2 ||
            renderState.CompanionBlendState != 2)
        {
            return false;
        }

        bool hasZero = false;
        bool hasVisible = false;
        foreach (uint argb in mesh.DiffuseColorsArgb)
        {
            byte alpha = (byte)(argb >> 24);
            hasZero |= alpha == 0;
            hasVisible |= alpha > 0;
        }
        return hasZero && hasVisible;
    }

    /// <summary>
    /// Detects a rigid surface whose complete vertex stream carries one
    /// deliberate, visible translucency value. Zero is excluded because it is
    /// commonly a disabled/default channel; opaque 255 needs no blend pass.
    /// </summary>
    public static bool HasUniformPartialAlpha(SmoMesh mesh)
    {
        ArgumentNullException.ThrowIfNull(mesh);
        if (!mesh.HasDiffuseColors)
            return false;

        byte alpha = (byte)(mesh.DiffuseColorsArgb[0] >> 24);
        if (alpha is 0 or byte.MaxValue)
            return false;
        for (int index = 1; index < mesh.DiffuseColorsArgb.Length; index++)
        {
            if ((byte)(mesh.DiffuseColorsArgb[index] >> 24) != alpha)
                return false;
        }
        return true;
    }

    public static bool ShouldUseVertexAlphaInPreview(
        SmoMesh mesh,
        SmoMaterialRenderStateInfo? renderState) =>
        (!mesh.HasSkinningData && HasUniformPartialAlpha(mesh)) ||
        renderState is null ||
        renderState.UsesAlphaBlend ||
        HasAuthoredAlphaGradient(mesh, renderState);

    public static bool ShouldModulateTexture(
        uint vertexFormat,
        ReadOnlySpan<uint> diffuseColorsArgb)
    {
        if (diffuseColorsArgb.IsEmpty ||
            vertexFormat is not (0x0100 or 0x0900 or 0x093E or 0x0940 or
                0x097E or 0x1900 or 0x1940 or 0x197E))
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
