using System.Numerics;
using SmoImporter.Core;
using SmoViewer.Core;

namespace SmoLVLcreator.Core;

/// <summary>
/// Renderer-independent input for the serializer. Keeping this projection
/// separate is what lets the save worker avoid constructing the full prepared
/// scene, texture catalogue, thumbnails and collision-link analysis.
/// </summary>
internal sealed class SmoLevelSaveState
{
    public required SmoDocument Source { get; init; }
    public required string SourcePath { get; init; }
    public int TotalEntityCount { get; init; }
    public bool IsModified { get; init; }
    public string? BatchWorkerExecutable { get; init; }
    public string? BatchWorkerAssembly { get; init; }
    public string? NativeFbxBridgePath { get; init; }
    public IReadOnlyList<SmoLevelSaveTransform> Transforms { get; init; } = [];
    public IReadOnlyList<SmoLevelModelReplacement> ModelReplacements { get; init; } = [];
    public IReadOnlyList<SmoLevelTextureReplacement> TextureReplacements { get; init; } = [];
    public IReadOnlyList<SmoLevelPlacementAddition> PlacementAdditions { get; init; } = [];
    public IReadOnlyDictionary<Guid, SmoLevelExternalModel> ExternalModels { get; init; } =
        new Dictionary<Guid, SmoLevelExternalModel>();
    public IReadOnlyList<SmoLevelExternalPlacement> ExternalPlacements { get; init; } = [];
    public IReadOnlyList<SmoLevelSaveRemoval> Removals { get; init; } = [];
    public IReadOnlyList<SmoLevelSaveCollision> GeneratedCollisions { get; init; } = [];

    public static SmoLevelSaveState FromDocument(SmoLevelDocument level)
    {
        ArgumentNullException.ThrowIfNull(level);
        if (level.HasActiveTransformSession)
            throw new InvalidOperationException(
                "Finish or cancel the active transform before saving.");
        return new SmoLevelSaveState
        {
            Source = level.Workspace.Document,
            SourcePath = level.Workspace.SourcePath,
            TotalEntityCount = level.Entities.Count,
            IsModified = level.IsModified,
            Transforms = level.Entities
                .Where(entity => entity.IsModified &&
                    !level.GeneratedCollisions.ContainsKey(entity.Id) &&
                    !level.RemovedEntityIds.Contains(entity.Id))
                .Select(entity => new SmoLevelSaveTransform(
                    entity.Id.SceneObjectIndex,
                    entity.Name,
                    entity.Kind,
                    entity.OriginalWorldTransform,
                    entity.WorldTransform,
                    string.Join(",", entity.Parts.Select(part =>
                        $"mesh:{part.Asset.ObjectIndex}/scene:{part.Source.SceneObjectIndex}")),
                    string.Join(",", entity.Collisions.Select(shape =>
                        $"info:{shape.Source.CollisionInfoObjectIndex}/" +
                        $"meshBV:{shape.Source.MeshBoundingVolumeObjectIndex}"))))
                .ToArray(),
            ModelReplacements = level.ModelReplacements.Values.ToArray(),
            TextureReplacements = level.TextureReplacements.Values.ToArray(),
            PlacementAdditions = level.PlacementAdditions.Values.ToArray(),
            ExternalModels = level.ExternalModels.ToDictionary(pair => pair.Key, pair => pair.Value),
            ExternalPlacements = level.ExternalPlacements.Values.ToArray(),
            Removals = level.RemovedEntityIds.Select(id =>
                new SmoLevelSaveRemoval(
                    id.SceneObjectIndex,
                    level.GetEntity(id).Kind)).ToArray(),
            GeneratedCollisions = level.GeneratedCollisions.Values.Select(item =>
                new SmoLevelSaveCollision(
                    item.EntityId.SceneObjectIndex,
                    item.Name,
                    item.Positions,
                    item.TriangleIndices,
                    level.GetEntity(item.EntityId).WorldTransform)).ToArray()
        };
    }
}

internal sealed record SmoLevelSaveTransform(
    int SceneObjectIndex,
    string Name,
    SmoLevelEntityKind Kind,
    Matrix4x4 OriginalWorldTransform,
    Matrix4x4 WorldTransform,
    string Parts,
    string Collisions);

internal sealed record SmoLevelSaveRemoval(
    int SceneObjectIndex,
    SmoLevelEntityKind Kind);

internal sealed record SmoLevelSaveCollision(
    int EntityIndex,
    string Name,
    IReadOnlyList<Vector3> Positions,
    IReadOnlyList<int> TriangleIndices,
    Matrix4x4 WorldTransform);
