namespace SmoNativeValidator.Core;

internal enum ResourceLoadTargetRoute
{
    None,
    AlreadyRedirected,
    DirectLogicalPath
}

/// <summary>
/// Identifies the target when Winx bypasses BuildAssetPath and calls
/// ResourceLoad with an already expanded Media-relative path.
/// </summary>
internal static class ResourceLoadTargetClassifier
{
    internal static ResourceLoadTargetRoute Classify(
        string observedArgument,
        string redirectAssetPath,
        string triggerGameAssetPath)
    {
        if (LogicalAssetMatcher.PathsEqual(observedArgument, redirectAssetPath))
            return ResourceLoadTargetRoute.AlreadyRedirected;

        return LogicalAssetMatcher.IsMatch(observedArgument, triggerGameAssetPath)
            ? ResourceLoadTargetRoute.DirectLogicalPath
            : ResourceLoadTargetRoute.None;
    }

}
