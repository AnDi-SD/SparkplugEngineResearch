using SmoExporter.Core;
using ExportSceneBuilder = SmoExporter.Core.SmoSceneBuilder;

namespace SmoLVLcreator.Core;

public enum SmoLevelExportFormat
{
    Glb,
    Fbx,
    Obj
}

public sealed record SmoLevelExportResult(
    string OutputPath,
    SmoLevelExportFormat Format,
    int EntityCount,
    int MeshCount,
    int PlacementCount,
    IReadOnlyList<string> Warnings);

/// <summary>
/// Adapts mutable level-editor selection to the shared SmoExporter.Core scene.
/// The GUI owns dialogs only; geometry and format writing stay in exporter core.
/// </summary>
public static class SmoLevelExportService
{
    public static SmoExportScene BuildSelectionScene(
        SmoLevelDocument document,
        IEnumerable<SmoLevelEntityId> entityIds)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(entityIds);
        SmoLevelEntity[] entities = entityIds
            .Distinct()
            .Select(document.GetEntity)
            .Where(entity => entity.Parts.Count > 0)
            .ToArray();
        if (entities.Length == 0)
        {
            throw new InvalidOperationException(
                "Выберите хотя бы один визуальный объект. " +
                "Коллизии экспортируются отдельно от моделей.");
        }

        SmoExportScene source = ExportSceneBuilder.Build(
            document.Workspace.Document,
            new SmoExportOptions(
                Resources: SmoExportResourceTypes.Meshes |
                           SmoExportResourceTypes.Materials |
                           SmoExportResourceTypes.Textures,
                SceneMode: SmoExportSceneMode.LevelWithInstances));
        SmoExportPlacementSelection[] placements = entities
            .SelectMany(entity => entity.Parts)
            .Select(part => new SmoExportPlacementSelection(
                part.Asset.ObjectIndex,
                part.Source.SceneObjectIndex,
                part.WorldTransform))
            .ToArray();
        return SmoExportSceneSelection.Create(source, placements);
    }

    public static SmoLevelExportResult Export(
        SmoLevelDocument document,
        IEnumerable<SmoLevelEntityId> entityIds,
        string outputPath,
        SmoLevelExportFormat format)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(outputPath);
        SmoLevelEntityId[] requested = entityIds.Distinct().ToArray();
        SmoExportScene scene = BuildSelectionScene(document, requested);
        string fullPath = Path.GetFullPath(outputPath);
        string expectedExtension = GetExtension(format);
        if (!Path.GetExtension(fullPath).Equals(
                expectedExtension,
                StringComparison.OrdinalIgnoreCase))
        {
            fullPath = Path.ChangeExtension(fullPath, expectedExtension);
        }

        switch (format)
        {
            case SmoLevelExportFormat.Glb:
                GlbExporter.Export(scene, fullPath);
                break;
            case SmoLevelExportFormat.Fbx:
                FbxExporter.Export(scene, fullPath);
                break;
            case SmoLevelExportFormat.Obj:
                ObjExporter.Export(scene, fullPath);
                break;
            default:
                throw new ArgumentOutOfRangeException(nameof(format), format, null);
        }
        return new SmoLevelExportResult(
            fullPath,
            format,
            requested.Count(id => document.GetEntity(id).Parts.Count > 0),
            scene.Meshes.Count,
            scene.MeshPlacements.Count,
            scene.Warnings);
    }

    public static string GetExtension(SmoLevelExportFormat format) => format switch
    {
        SmoLevelExportFormat.Glb => ".glb",
        SmoLevelExportFormat.Fbx => ".fbx",
        SmoLevelExportFormat.Obj => ".obj",
        _ => throw new ArgumentOutOfRangeException(nameof(format), format, null)
    };
}
