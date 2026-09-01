namespace SmoImporter.Core;

internal static class SmoTextureSerializationLimits
{
    public const int MaximumDimension = 16384;

    public static bool IsSizeRepresentable(int width, int height) =>
        width > 0 && height > 0 && ((long)width * height & 63) == 0;
}
