using SmoViewer.Core;

namespace SmoExporter.Core;

public enum SmoExportContentKind
{
    Character,
    Level
}

public sealed record SmoExportElementInfo(
    int MeshObjectIndex,
    string Name,
    string OwnerName,
    int PlacementCount,
    bool IsInstancedFamily)
{
    public string Display =>
        $"[{MeshObjectIndex}] " +
        (string.IsNullOrWhiteSpace(Name) ? $"mesh_{MeshObjectIndex}" : Name) +
        (string.IsNullOrWhiteSpace(OwnerName) ||
         OwnerName.Equals(Name, StringComparison.OrdinalIgnoreCase)
            ? string.Empty
            : $" · {OwnerName}") +
        (PlacementCount > 1 ? $" · размещений: {PlacementCount}" : string.Empty);
}

public sealed record SmoExportContentProfile(
    SmoExportContentKind Kind,
    int PhysicalMeshCount,
    int PlacementCount,
    int SharedInstanceCount,
    IReadOnlyList<SmoExportElementInfo> Elements,
    string Reason);

public static class SmoExportContentProfileAnalyzer
{
    public static SmoExportContentProfile Analyze(SmoDocument document)
    {
        ArgumentNullException.ThrowIfNull(document);

        SmoObjectEntry[] meshes = document.Objects.Where(entry =>
            entry.TypeHash == SmoClassIds.MeshData).ToArray();
        IReadOnlyList<SmoSharedMeshInstanceInfo> shared =
            SmoSharedMeshInstanceResolver.ResolveAll(document);
        int skinCount = document.Objects.Count(entry =>
            entry.TypeHash == SmoClassIds.Skin);
        int staticObjectCount = document.Objects.Count(entry =>
            entry.TypeHash == SmoClassIds.StaticRenderObject);

        bool isLevel = shared.Count > 0 ||
                       (skinCount == 0 && staticObjectCount >= 8 && meshes.Length >= 16);
        string reason = shared.Count > 0
            ? $"обнаружено {shared.Count} reference-only размещений"
            : isLevel
                ? $"обнаружено {staticObjectCount} статических объектов и {meshes.Length} мешей без skin"
                : skinCount > 0
                    ? $"обнаружено skin-объектов: {skinCount}"
                    : "компактный модельный граф";

        Dictionary<int, int> sharedCounts = shared
            .GroupBy(instance => instance.SourceMeshObjectIndex)
            .ToDictionary(group => group.Key, group => group.Count());
        SmoExportElementInfo[] elements = meshes.Select(mesh =>
        {
            string owner = FindOwnerName(document.Objects, mesh);
            int sharedCount = sharedCounts.GetValueOrDefault(mesh.Index);
            return new SmoExportElementInfo(
                mesh.Index,
                mesh.Name,
                owner,
                1 + sharedCount,
                sharedCount > 0);
        }).OrderByDescending(element => element.IsInstancedFamily)
          .ThenBy(element => string.IsNullOrWhiteSpace(element.Name))
          .ThenBy(element => element.Name, StringComparer.OrdinalIgnoreCase)
          .ThenBy(element => element.MeshObjectIndex)
          .ToArray();

        return new SmoExportContentProfile(
            isLevel ? SmoExportContentKind.Level : SmoExportContentKind.Character,
            meshes.Length,
            meshes.Length + shared.Count,
            shared.Count,
            elements,
            reason);
    }

    private static string FindOwnerName(
        IReadOnlyList<SmoObjectEntry> objects, SmoObjectEntry mesh)
    {
        SmoObjectEntry? cursor = mesh;
        var visited = new HashSet<int>();
        while (cursor.ParentIndex is int parentIndex &&
               (uint)parentIndex < (uint)objects.Count &&
               visited.Add(parentIndex))
        {
            cursor = objects[parentIndex];
            if (cursor.TypeHash == SmoClassIds.StaticRenderObject)
                return cursor.Name;
        }
        return string.Empty;
    }
}
