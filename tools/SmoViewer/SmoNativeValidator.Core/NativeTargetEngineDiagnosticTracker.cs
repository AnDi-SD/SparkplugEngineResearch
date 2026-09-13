namespace SmoNativeValidator.Core;

internal sealed record NativeTargetEngineDiagnostic(
    string Code,
    string Description);

/// <summary>
/// Latches high-confidence Sparkplug serializer diagnostics only while the
/// redirected ResourceLoad frame is the current frame on the emitting thread.
/// Background debug output must never turn an otherwise valid model into an
/// EngineRejected result.
/// </summary>
internal sealed class NativeTargetEngineDiagnosticTracker
{
    private readonly HashSet<string> _codes = new(StringComparer.Ordinal);
    private readonly List<NativeTargetEngineDiagnostic> _diagnostics = [];

    internal IReadOnlyList<NativeTargetEngineDiagnostic> Diagnostics => _diagnostics;
    internal bool HasDiagnostics => _diagnostics.Count > 0;

    internal IReadOnlyList<NativeTargetEngineDiagnostic> Observe(
        string value,
        bool targetContext,
        string redirectedAssetPath)
    {
        if (!targetContext || string.IsNullOrWhiteSpace(value))
            return [];

        var added = new List<NativeTargetEngineDiagnostic>();
        if (Contains(value, "WARNING: Stream reached EOF (") &&
            Contains(value, redirectedAssetPath))
        {
            Add(added, "target-stream-eof", "target stream reached EOF");
        }
        if ((Contains(value, "pStream->Read(") ||
             Contains(value, "pCurrentStream->Read(")) &&
            Contains(value, "ERROR:"))
        {
            Add(added, "target-stream-read-failed", "target stream read failed");
        }
        if (Contains(value, "ERROR: No empty ") &&
            Contains(value, "File corrupt?"))
        {
            Add(added, "invalid-null-relation", "invalid NULL object relation");
        }
        if (Contains(value, "ERROR: Resource load failed"))
        {
            Add(added, "resource-load-failed", "nested resource load failed");
        }
        if (Contains(value, "ERROR: Cannot create object of this type"))
        {
            Add(added, "object-creation-failed", "object construction failed");
        }
        return added;
    }

    internal string DescribeRejection(bool returnedNonNull)
    {
        if (!HasDiagnostics)
            throw new InvalidOperationException("No target engine diagnostics were recorded.");

        string returnClause = returnedNonNull
            ? " despite a non-null ResourceLoad return"
            : string.Empty;
        return "The native serializer reported target-scoped corruption" +
            returnClause + ": " +
            string.Join(", ", _diagnostics.Select(item => item.Description)) + ".";
    }

    private void Add(
        ICollection<NativeTargetEngineDiagnostic> added,
        string code,
        string description)
    {
        if (!_codes.Add(code))
            return;
        var diagnostic = new NativeTargetEngineDiagnostic(code, description);
        _diagnostics.Add(diagnostic);
        added.Add(diagnostic);
    }

    private static bool Contains(string value, string expected) =>
        !string.IsNullOrWhiteSpace(expected) &&
        value.Contains(expected, StringComparison.OrdinalIgnoreCase);
}
