namespace SmoNativeValidator.Core;

internal static class NativeMediaPathMapper
{
    internal static bool TryMap(
        string observedPath,
        string requestedFileName,
        string mediaRoot,
        out string mappedPath)
    {
        mappedPath = string.Empty;
        if (string.IsNullOrWhiteSpace(mediaRoot))
            return false;

        string root = Path.GetFullPath(mediaRoot)
            .TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
        string? relative = TryGetMediaRelativePath(observedPath);
        if (string.IsNullOrWhiteSpace(relative))
        {
            string requested = requestedFileName.Trim()
                .Replace(Path.AltDirectorySeparatorChar, Path.DirectorySeparatorChar)
                .TrimStart(Path.DirectorySeparatorChar);
            if (string.IsNullOrWhiteSpace(requested) || Path.IsPathFullyQualified(requested))
                return false;
            relative = requested;
        }

        string candidate = Path.GetFullPath(Path.Combine(root, relative));
        string rootPrefix = root + Path.DirectorySeparatorChar;
        if (!candidate.StartsWith(rootPrefix, StringComparison.OrdinalIgnoreCase))
            return false;
        if (!StagedAssetSession.IsAscii(candidate))
            throw new NativeAssetPathException(
                "The selected Media root produces a non-ASCII native asset path.");

        mappedPath = candidate;
        return true;
    }

    private static string? TryGetMediaRelativePath(string observedPath)
    {
        if (string.IsNullOrWhiteSpace(observedPath))
            return null;
        string normalized = observedPath.Trim().Trim('"')
            .Replace(Path.AltDirectorySeparatorChar, Path.DirectorySeparatorChar);
        string marker = Path.DirectorySeparatorChar + "Media" + Path.DirectorySeparatorChar;
        int markerIndex = normalized.LastIndexOf(marker, StringComparison.OrdinalIgnoreCase);
        return markerIndex >= 0
            ? normalized[(markerIndex + marker.Length)..]
            : null;
    }
}
