namespace SmoViewer.Core;

/// <summary>
/// Describes the name-based join between SAN tracks and SMO scene nodes.
/// Exact and case-folded results are kept separate because the serialized
/// resources contain names, not stable object IDs or catalog offsets.
/// </summary>
public sealed record SmoAnimationBindingAnalysis(
    int ModelNodeCount,
    int UniqueModelNodeNameCount,
    int TrackCount,
    int ExactMatchCount,
    int CaseFoldedOnlyMatchCount,
    int MissingMatchCount,
    IReadOnlyList<string> CaseFoldedOnlyTrackNames,
    IReadOnlyList<string> MissingTrackNames,
    IReadOnlyList<string> AmbiguousModelNodeNames);

public static class SmoAnimationBindingAnalyzer
{
    public static SmoAnimationBindingAnalysis Analyze(
        SmoDocument model,
        SmoAnimationClip animation)
    {
        ArgumentNullException.ThrowIfNull(model);
        ArgumentNullException.ThrowIfNull(animation);

        string[] modelNodeNames = model.Objects
            .Where(entry => entry.TypeHash is SmoClassIds.Node or SmoClassIds.RenderNode)
            .Select(entry => entry.Name.TrimEnd('\0'))
            .Where(name => name.Length > 0)
            .ToArray();
        HashSet<string> exactNames = modelNodeNames.ToHashSet(StringComparer.Ordinal);
        HashSet<string> foldedNames = modelNodeNames.ToHashSet(
            StringComparer.OrdinalIgnoreCase);
        string[] ambiguous = modelNodeNames
            .GroupBy(name => name, StringComparer.Ordinal)
            .Where(group => group.Count() > 1)
            .Select(group => group.Key)
            .Order(StringComparer.Ordinal)
            .ToArray();

        int exact = 0;
        int foldedOnly = 0;
        int missing = 0;
        var foldedOnlyNames = new SortedSet<string>(StringComparer.Ordinal);
        var missingNames = new SortedSet<string>(StringComparer.Ordinal);
        foreach (SmoAnimationTrack track in animation.Tracks)
        {
            if (exactNames.Contains(track.NodeName))
            {
                exact++;
            }
            else if (foldedNames.Contains(track.NodeName))
            {
                foldedOnly++;
                foldedOnlyNames.Add(track.NodeName);
            }
            else
            {
                missing++;
                missingNames.Add(track.NodeName);
            }
        }

        return new SmoAnimationBindingAnalysis(
            modelNodeNames.Length,
            exactNames.Count,
            animation.Tracks.Count,
            exact,
            foldedOnly,
            missing,
            foldedOnlyNames.ToArray(),
            missingNames.ToArray(),
            ambiguous);
    }
}
