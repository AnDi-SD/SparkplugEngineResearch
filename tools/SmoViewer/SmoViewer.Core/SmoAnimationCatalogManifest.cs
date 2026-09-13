namespace SmoViewer.Core;

/// <summary>
/// Versioned catalog passed from SmoViewer to tools that consume the same
/// discovered animation names and ANM group assignments.
/// </summary>
public sealed record SmoAnimationCatalogManifest(
    int Version,
    IReadOnlyList<SmoAnimationCatalogEntry> Animations)
{
    public const int CurrentVersion = 1;
}

public sealed record SmoAnimationCatalogEntry(
    string Path,
    string Display,
    IReadOnlyList<string> Groups);
