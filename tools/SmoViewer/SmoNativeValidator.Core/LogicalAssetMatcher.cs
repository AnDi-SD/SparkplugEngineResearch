namespace SmoNativeValidator.Core;

public static class LogicalAssetMatcher
{
    public static bool IsMatch(string observedPath, string logicalGameAssetPath)
    {
        if (string.IsNullOrWhiteSpace(observedPath) || string.IsNullOrWhiteSpace(logicalGameAssetPath))
            return false;

        string observed = Normalize(observedPath);
        string logical = Normalize(logicalGameAssetPath).TrimStart('\\');
        if (!logical.EndsWith(".smo", StringComparison.OrdinalIgnoreCase) ||
            !observed.EndsWith(".smo", StringComparison.OrdinalIgnoreCase))
        {
            return false;
        }

        if (logical.Contains('\\'))
        {
            return observed.Equals(logical, StringComparison.OrdinalIgnoreCase) ||
                observed.EndsWith("\\" + logical, StringComparison.OrdinalIgnoreCase);
        }

        int separator = observed.LastIndexOf('\\');
        string observedName = separator >= 0 ? observed[(separator + 1)..] : observed;
        return observedName.Equals(logical, StringComparison.OrdinalIgnoreCase);
    }

    public static string Normalize(string path) =>
        path.Trim().Trim('"').Replace('/', '\\').TrimEnd('\\');
}
