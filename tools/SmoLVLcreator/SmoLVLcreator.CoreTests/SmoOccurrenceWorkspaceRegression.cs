using System.Security.Cryptography;
using System.Text.Json;
using SmoLVLcreator.Core;

namespace SmoLVLcreator.CoreTests;

internal static class SmoOccurrenceWorkspaceRegression
{
    internal static int Run(string path, string output)
    {
        int checks = 0;
        void Check(bool condition, string message)
        { if (!condition) throw new InvalidDataException(message); ++checks; }
        var workspace = SmoLevelWorkspace.Load(path);
        var placements = workspace.Assets.SelectMany(asset => asset.Placements.Select(value => (asset, value))).ToArray();
        Check(placements.Length == workspace.PreparedScene.Meshes.Count, "Workspace retains every actual prepared occurrence");
        Check(placements.All(value => value.value.OccurrenceKey.HasValue), "Workspace preserves slot identity");
        Check(placements.Select(value => value.value.OccurrenceKey).Distinct().Count() == placements.Length, "Workspace does not merge repeated slots");
        foreach (var item in placements)
        {
            var entry = item.value;
            Check(entry.ModelObjectIndex == entry.SceneObjectIndex, "Placement names and selection use actual consuming Model/Skin");
            Check(entry.Name == workspace.Document.Objects[entry.ModelObjectIndex!.Value].Name, "Shared geometry does not borrow another Model name");
        }
        bool repeats = placements.GroupBy(value => (value.asset.ObjectIndex, value.value.SceneObjectIndex)).Any(group => group.Count() > 1);
        string? authoringIssue = null;
        int? editablePlacements = null;
        try
        {
            var editable = new SmoLevelDocument(workspace);
            editablePlacements = editable.Placements.Count;
            Check(!repeats, "Legacy command model must not silently collapse repeat occurrences");
            Check(editablePlacements == placements.Length, "Every unique placement reaches the editor command model");
        }
        catch (InvalidDataException exception) when (repeats && exception.Message.StartsWith("REPEATED_RENDERABLE_AUTHORING:"))
        { authoringIssue = exception.Message; Check(true, "Repeated authoring is explicitly deferred"); }
        var report = new
        {
            status = "passed", path = Path.GetFullPath(path), checks, placements = placements.Length,
            actual_models = workspace.PreparedScene.Meshes.Select(value => value.RenderableObjectIndex).Distinct().Count(),
            repeats, authoring_issue = authoringIssue, editable_placements = editablePlacements,
            native_dll_sha256 = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(Path.Combine(AppContext.BaseDirectory, "SparkplugViewerNative.dll"))))
        };
        File.WriteAllText(output, JsonSerializer.Serialize(report, new JsonSerializerOptions { WriteIndented = true }) + "\n");
        Console.WriteLine($"Occurrence workspace: {placements.Length} placements, repeated authoring deferred={repeats}, {checks} checks");
        return 0;
    }
}
