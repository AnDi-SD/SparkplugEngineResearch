namespace SmoNativeValidator.Core;

internal sealed record ResolvedNativeValidationRoute(
    NativeValidationRoute Route,
    string TriggerGameAssetPath,
    int? RequestedStartLevel);

internal static class NativeValidationRouting
{
    internal static ResolvedNativeValidationRoute Resolve(NativeValidationRequest request)
    {
        ArgumentNullException.ThrowIfNull(request);
        if (!Enum.IsDefined(request.Route))
            throw new ArgumentOutOfRangeException(nameof(request), "Unknown native validation route.");
        if (!request.UseIsolatedLaunchWorkspace)
        {
            throw new ArgumentException(
                "Native validation requires an isolated windowed launch workspace; " +
                "non-isolated launches could inherit fullScreen=true from the game installation.",
                nameof(request));
        }

        string logicalPath = LogicalAssetMatcher.Normalize(request.LogicalGameAssetPath)
            .TrimStart('\\');
        string expectedTrigger = request.Route switch
        {
            NativeValidationRoute.FastGeneric =>
                NativeValidationDefaults.FastTriggerGameAssetPath,
            NativeValidationRoute.Contextual => logicalPath,
            _ => throw new ArgumentOutOfRangeException(nameof(request), "Unknown native validation route.")
        };
        string trigger = string.IsNullOrWhiteSpace(request.TriggerGameAssetPath)
            ? expectedTrigger
            : LogicalAssetMatcher.Normalize(request.TriggerGameAssetPath).TrimStart('\\');
        if (!string.Equals(trigger, expectedTrigger, StringComparison.OrdinalIgnoreCase))
        {
            throw new NativeAssetPathException(request.Route == NativeValidationRoute.FastGeneric
                ? $"FastGeneric TriggerGameAssetPath must be {NativeValidationDefaults.FastTriggerGameAssetPath}."
                : "Contextual TriggerGameAssetPath must equal LogicalGameAssetPath.");
        }

        bool fileNameOnlyAllowed = request.Route == NativeValidationRoute.Contextual &&
            request.AllowFileNameOnlyLogicalPath;
        if (!trigger.EndsWith(".smo", StringComparison.OrdinalIgnoreCase) ||
            (!trigger.Contains('\\') && !fileNameOnlyAllowed))
        {
            throw new NativeAssetPathException(
                "TriggerGameAssetPath must be a Media-relative .smo path with a game directory; file-name-only contextual matching requires an explicit opt-in.");
        }

        int? requestedStartLevel = request.StartLevel is null or 0
            ? null
            : request.StartLevel;
        if (request.UseIsolatedLaunchWorkspace &&
            !string.IsNullOrWhiteSpace(request.WorkingDirectory))
        {
            throw new ArgumentException(
                "WorkingDirectory cannot be combined with UseIsolatedLaunchWorkspace; the owned workspace supplies the launch directory.",
                nameof(request));
        }
        if (request.Route == NativeValidationRoute.FastGeneric && requestedStartLevel is not null)
        {
            throw new ArgumentException(
                "StartLevel is available only for the Contextual validation route.",
                nameof(request));
        }
        if (requestedStartLevel is int startLevel)
        {
            if (!WinxClubLevelCatalog.CanStart(startLevel))
            {
                throw new ArgumentOutOfRangeException(
                    nameof(request),
                    startLevel,
                    WinxClubLevelCatalog.GetWarning(startLevel));
            }
        }

        return new ResolvedNativeValidationRoute(request.Route, trigger, requestedStartLevel);
    }
}
