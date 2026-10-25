namespace SmoImporter.Core;

internal static class SmoTextureSerializationLimits
{
    public const int MaximumDimension = 16384;

    public static bool IsSizeRepresentable(int width, int height) =>
        width is >= 1 and <= MaximumDimension && height is >= 1 and <= MaximumDimension;
}
