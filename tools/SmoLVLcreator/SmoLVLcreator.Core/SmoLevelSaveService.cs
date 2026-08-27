using System.Numerics;
using System.Security.Cryptography;
using System.Diagnostics;
using SmoImporter.Core;
using SmoViewer.Core;

namespace SmoLVLcreator.Core;

public sealed record SmoLevelSaveResult(
    string OutputPath,
    string? BackupPath,
    string LogPath,
    int EditedEntityCount,
    int PatchedTransformCount,
    int AddedPlacementCount,
    int ResourceEditCount,
    long FileSize);

public sealed class SmoLevelSaveException(
    string message,
    string logPath,
    Exception innerException) : Exception(message, innerException)
{
    public string LogPath { get; } = logPath;
}

/// <summary>
/// Applies editor transforms, resource replacements and appended shared
/// placements, then atomically installs a structurally verified SMO file.
/// </summary>
public static class SmoLevelSaveService
{
    public static string GetLogPath(string outputPath) =>
        SmoLevelSaveLog.GetAdjacentPath(outputPath);

    public static bool NeedsCollisionRegistryRepair(SmoLevelDocument level)
    {
        ArgumentNullException.ThrowIfNull(level);
        return SmoCollisionBranchAppender
            .FindUnregisteredCollisionInfoObjectIndices(level.Workspace.Document)
            .Count > 0;
    }

    public static bool NeedsCollisionMeshCompatibilityRepair(SmoLevelDocument level)
    {
        ArgumentNullException.ThrowIfNull(level);
        return SmoCollisionBranchAppender
            .FindUnterminatedCollisionMeshObjectIndices(level.Workspace.Document)
            .Count > 0;
    }

    public static bool TryAppendDiagnostic(
        string logPath,
        string level,
        string category,
        string message)
    {
        try
        {
            SmoLevelSaveLog.Append(logPath, level, category, message);
            return true;
        }
        catch
        {
            return false;
        }
    }

    public static SmoLevelSaveResult Save(
        SmoLevelDocument level,
        string outputPath)
        => Save(level, outputPath, progress: null, CancellationToken.None);

    public static SmoLevelSaveResult Save(
        SmoLevelDocument level,
        string outputPath,
        IProgress<SmoLevelSaveProgress>? progress,
        CancellationToken cancellationToken = default)
        => SaveCore(
            SmoLevelSaveState.FromDocument(level),
            outputPath,
            enforceMemoryBudget: true,
            progress,
            cancellationToken);

    internal static SmoLevelSaveResult SaveWithoutMemoryGuard(
        SmoLevelDocument level,
        string outputPath)
        => SaveWithoutMemoryGuard(
            level,
            outputPath,
            progress: null,
            CancellationToken.None);

    internal static SmoLevelSaveResult SaveWithoutMemoryGuard(
        SmoLevelDocument level,
        string outputPath,
        IProgress<SmoLevelSaveProgress>? progress,
        CancellationToken cancellationToken = default)
        => SaveCore(
            SmoLevelSaveState.FromDocument(level),
            outputPath,
            enforceMemoryBudget: false,
            progress,
            cancellationToken);

    internal static SmoLevelSaveResult SaveStateWithoutMemoryGuard(
        SmoLevelSaveState state,
        string outputPath,
        IProgress<SmoLevelSaveProgress>? progress,
        CancellationToken cancellationToken = default)
        => SaveCore(
            state,
            outputPath,
            enforceMemoryBudget: false,
            progress,
            cancellationToken);

    private static SmoLevelSaveResult SaveCore(
        SmoLevelSaveState state,
        string outputPath,
        bool enforceMemoryBudget,
        IProgress<SmoLevelSaveProgress>? progress,
        CancellationToken cancellationToken)
    {
        ArgumentNullException.ThrowIfNull(state);
        ArgumentException.ThrowIfNullOrWhiteSpace(outputPath);

        string fullOutputPath = Path.GetFullPath(outputPath);
        string? directory = Path.GetDirectoryName(fullOutputPath);
        if (string.IsNullOrWhiteSpace(directory) || !Directory.Exists(directory))
        {
            throw new DirectoryNotFoundException(
                $"Output directory does not exist: {directory}");
        }

        using SmoLevelSaveLog log = SmoLevelSaveLog.Create(fullOutputPath);
        log.Start(state.SourcePath, fullOutputPath);
        string temporaryPath = Path.Combine(
            directory,
            $".{Path.GetFileName(fullOutputPath)}.{Guid.NewGuid():N}.tmp");
        string? backupPath = null;
        try
        {
            cancellationToken.ThrowIfCancellationRequested();
            SmoDocument source = state.Source;
            int externalModelStepCount = state.ExternalPlacements
                .Select(item => item.ModelId)
                .Distinct()
                .Count();
            int totalSteps = checked(
                6 +
                state.ModelReplacements.Count +
                state.TextureReplacements.Count +
                externalModelStepCount +
                state.Removals.Count +
                state.PlacementAdditions.Count +
                state.GeneratedCollisions.Count);
            var pipeline = new SavePipelineProgress(
                progress,
                totalSteps,
                source.Data.Length);
            pipeline.Report(
                SmoLevelSaveStage.Preparing,
                "Подготовка списка изменений");
            log.Info(
                "SOURCE",
                $"bytes={source.Data.Length}; objects={source.Objects.Count}; " +
                $"diagnostics={source.Diagnostics.Count}; errors={source.HasErrors}; " +
                $"sha256={Hash(source.Data.Span)}");

            IReadOnlyList<SmoLevelSaveTransform> editedEntities = state.Transforms;
            log.Info(
                "EDIT_SET",
                $"entities.total={state.TotalEntityCount}; " +
                $"entities.edited={editedEntities.Count}; " +
                $"history.modified={state.IsModified}");
            foreach (SmoLevelSaveTransform entity in editedEntities)
            {
                int? owner = SmoPlacementTransformWriter.FindStaticPlacementOwnerIndex(
                    source,
                    entity.SceneObjectIndex);
                bool nodeWritable = SmoPlacementTransformWriter.CanWriteNodeTransform(
                    source,
                    entity.SceneObjectIndex);
                bool collisionWritable =
                    SmoPlacementTransformWriter.CanWriteCollisionTransform(
                        source,
                        entity.SceneObjectIndex);
                Vector3 before = Translation(entity.OriginalWorldTransform);
                Vector3 after = Translation(entity.WorldTransform);
                log.Info(
                    "ENTITY",
                    $"id={entity.SceneObjectIndex}; name={entity.Name}; kind={entity.Kind}; " +
                    $"staticOwner={owner?.ToString() ?? "NONE"}; " +
                    $"nodeWritable={nodeWritable}; " +
                    $"collisionWritable={collisionWritable}; " +
                    $"parts={entity.Parts}; collisions={entity.Collisions}; " +
                    $"position.before={Format(before)}; position.after={Format(after)}; " +
                    $"delta={Format(after - before)}; " +
                    $"matrix.before={Format(entity.OriginalWorldTransform)}; " +
                    $"matrix.after={Format(entity.WorldTransform)}");
            }

            byte[] resourceData = source.Data.ToArray();
            pipeline.Complete(
                SmoLevelSaveStage.Preparing,
                "Список изменений подготовлен",
                resourceData.LongLength);
            int addedResourceObjectCount = 0;
            var replacementMeshIds = new Dictionary<uint, uint>();
            var replacementPrimaryModelIds = new Dictionary<uint, uint>();
            foreach (SmoLevelModelReplacement replacement in
                     state.ModelReplacements.OrderBy(item =>
                         item.MeshObjectIndex))
            {
                cancellationToken.ThrowIfCancellationRequested();
                pipeline.Report(
                    SmoLevelSaveStage.ReplacingModel,
                    $"Замена модели [{replacement.MeshObjectIndex}]");
                log.Info(
                    "RESOURCE_MODEL",
                    $"object={replacement.MeshObjectIndex}; " +
                    $"source={replacement.SourcePath}; " +
                    $"meshes={replacement.ImportedScene.Meshes.Count}; " +
                    $"transform={replacement.Transform}");
                SmoDocument current = SmoDocument.ParseOwned(
                    resourceData,
                    source.SourcePath);
                uint selectedSourceId = source.Objects[
                    replacement.MeshObjectIndex].Id;
                int currentMeshObjectIndex = current.Objects.Single(entry =>
                    entry.Id == selectedSourceId).Index;
                SmoLevelModelGraphReplacementResult result =
                    SmoLevelModelGraphReplacer.Replace(
                        current,
                        currentMeshObjectIndex,
                        replacement.ImportedScene,
                        replacement.Transform,
                        referenceWorldTransform:
                            replacement.ReferenceWorldTransform);
                resourceData = result.Data;
                addedResourceObjectCount += result.AddedObjectCount;
                foreach ((uint oldId, uint newId) in result.MeshObjectIds)
                    replacementMeshIds[oldId] = newId;
                foreach ((uint oldId, uint newId) in result.PrimaryModelObjectIds)
                    replacementPrimaryModelIds[oldId] = newId;
                log.Info(
                    "RESOURCE_MODEL",
                    $"selected={replacement.MeshObjectIndex}; " +
                    $"addedObjects={result.AddedObjectCount}; " +
                    $"primaryModelIds={string.Join(",", result.PrimaryModelObjectIds.Select(pair => $"{pair.Key}->{pair.Value}"))}; " +
                    $"meshIds={string.Join(",", result.MeshObjectIds.Select(pair => $"{pair.Key}->{pair.Value}"))}; " +
                    $"textureIds={string.Join(",", result.TextureObjectIds.Select(pair => $"{pair.Key}->{pair.Value}"))}; " +
                    $"vertices={result.VertexCount}; " +
                    $"triangles={result.TriangleCount}; bytes={resourceData.Length}");
                current = null!;
                result = null!;
                ReleasePipelineIntermediates(log, resourceData, "MODEL_REPLACED");
                pipeline.Complete(
                    SmoLevelSaveStage.ReplacingModel,
                    $"Модель [{replacement.MeshObjectIndex}] заменена",
                    resourceData.LongLength);
            }

            cancellationToken.ThrowIfCancellationRequested();
            pipeline.Report(
                SmoLevelSaveStage.PatchingTransforms,
                $"Применение трансформаций: {editedEntities.Count:N0}");
            SmoDocument resourceDocument = SmoDocument.ParseOwned(
                resourceData,
                source.SourcePath);
            SmoPlacementTransformEdit[] transformEdits = editedEntities
                .Select(entity => new SmoPlacementTransformEdit(
                    ResolveCurrentObjectIndex(
                        source,
                        resourceDocument,
                        entity.SceneObjectIndex,
                        replacementPrimaryModelIds),
                    entity.OriginalWorldTransform,
                    entity.WorldTransform))
                .ToArray();
            log.Info(
                "PATCH",
                "Writing schema-backed transforms through the shared field transaction.");
            SmoPlacementTransformPatchResult patch =
                SmoPlacementTransformWriter.Patch(
                    resourceDocument,
                    transformEdits);
            log.Info(
                "PATCH",
                $"complete; editedSceneObjects={patch.EditedSceneObjectCount}; " +
                $"staticOwners={string.Join(",", patch.StaticObjectIndices)}; " +
                $"nodeOwners={string.Join(",", patch.NodeObjectIndices)}; " +
                $"collisionMeshes={string.Join(",", patch.CollisionMeshObjectIndices)}; " +
                $"bytes={patch.Data.Length}; sha256={Hash(patch.Data)}");

            byte[] outputData = patch.Data;
            resourceDocument = null!;
            resourceData = null!;
            ReleasePipelineIntermediates(log, outputData, "TRANSFORMS_PATCHED");
            pipeline.Complete(
                SmoLevelSaveStage.PatchingTransforms,
                $"Трансформации применены: {editedEntities.Count:N0}",
                outputData.LongLength);
            uint[] textureObjectIds = source.Objects
                .Where(entry => entry.TypeHash == SmoClassIds.TextureData)
                .Select(entry => entry.Id)
                .ToArray();
            foreach (SmoLevelTextureReplacement replacement in
                     state.TextureReplacements.OrderBy(item =>
                         item.TextureObjectIndex))
            {
                cancellationToken.ThrowIfCancellationRequested();
                pipeline.Report(
                    SmoLevelSaveStage.ReplacingTexture,
                    $"Замена текстуры [{replacement.TextureObjectIndex}]");
                uint sourceTextureId = source.Objects[
                    replacement.TextureObjectIndex].Id;
                SmoDocument currentTextureDocument = SmoDocument.ParseOwned(
                    outputData,
                    source.SourcePath);
                int[] currentTextureObjectIndices = currentTextureDocument.Objects
                    .Where(entry => entry.TypeHash == SmoClassIds.TextureData)
                    .Select(entry => entry.Index)
                    .ToArray();
                SmoObjectEntry? currentTexture = currentTextureDocument.Objects
                    .SingleOrDefault(entry => entry.Id == sourceTextureId);
                int textureOrdinal = currentTexture is null
                    ? -1
                    : Array.IndexOf(currentTextureObjectIndices, currentTexture.Index);
                if (textureOrdinal < 0 || !textureObjectIds.Contains(sourceTextureId))
                {
                    throw new InvalidOperationException(
                        $"Texture object [{replacement.TextureObjectIndex}] disappeared " +
                        "from the source catalog.");
                }
                log.Info(
                    "RESOURCE_TEXTURE",
                    $"object={replacement.TextureObjectIndex}; index={textureOrdinal + 1}; " +
                    $"source={replacement.SourcePath}; alpha={replacement.ReplaceAlpha}; " +
                    $"inputBytes={replacement.EncodedImage.Length}");
                outputData = replacement.ReplaceAlpha
                    ? FixedSizeTextureWriter.ReplaceRgba(
                        outputData,
                        textureOrdinal + 1,
                        replacement.EncodedImage.Span)
                    : FixedSizeTextureWriter.ReplaceRgb(
                        outputData,
                        textureOrdinal + 1,
                        replacement.EncodedImage.Span);
                currentTextureDocument = null!;
                ReleasePipelineIntermediates(log, outputData, "TEXTURE_REPLACED");
                pipeline.Complete(
                    SmoLevelSaveStage.ReplacingTexture,
                    $"Текстура [{replacement.TextureObjectIndex}] заменена",
                    outputData.LongLength);
            }
            log.Info(
                "RESOURCE_PATCH",
                $"models={state.ModelReplacements.Count}; " +
                $"textures={state.TextureReplacements.Count}; " +
                $"bytes={outputData.Length}; sha256={Hash(outputData)}");

            int addedObjectCount = addedResourceObjectCount;
            // External resources are materialized before source placements are
            // removed. A full composite replacement may legitimately use one
            // of those source meshes as its serialization template; once the
            // new graph exists, ordinary removal can safely prune every old
            // placement and its now-unused resources.
            foreach (IGrouping<Guid, SmoLevelExternalPlacement> group in
                     state.ExternalPlacements
                         .OrderBy(item => item.Id)
                         .GroupBy(item => item.ModelId))
            {
                cancellationToken.ThrowIfCancellationRequested();
                SmoLevelExternalModel model = state.ExternalModels[group.Key];
                pipeline.Report(
                    SmoLevelSaveStage.AddingExternalModel,
                    $"Упаковка внешней модели {model.Name}");
                SmoDocument current = SmoDocument.ParseOwned(
                    outputData,
                    source.SourcePath);
                uint templateSourceId = source.Objects[
                    model.TemplateMeshObjectIndex].Id;
                uint activeTemplateId = replacementMeshIds.GetValueOrDefault(
                    templateSourceId,
                    templateSourceId);
                int currentTemplateIndex = current.Objects.Single(entry =>
                    entry.Id == activeTemplateId).Index;
                LogMemory(
                    log,
                    "EXTERNAL_MODEL_ADD_BEGIN",
                    $"model={model.Id}; parts={model.ImportedScene.Meshes.Count}; " +
                    $"containerBytes={outputData.Length}");
                Matrix4x4[] placementTransforms = group
                    .Select(item => item.WorldTransform)
                    .ToArray();
                SmoExternalLevelModelAppendResult appended;
                if (!string.IsNullOrWhiteSpace(state.BatchWorkerExecutable))
                {
                    appended = SmoExternalModelBatchPipeline.Append(
                        current,
                        currentTemplateIndex,
                        model.ImportedScene,
                        placementTransforms,
                        model.Name,
                        model.SourcePath,
                        state.BatchWorkerExecutable,
                        state.BatchWorkerAssembly,
                        state.NativeFbxBridgePath,
                        (completedParts, totalParts) => pipeline.Report(
                            SmoLevelSaveStage.AddingExternalModel,
                            $"Упаковка {model.Name}: детали " +
                            $"{completedParts:N0}/{totalParts:N0}"),
                        cancellationToken);
                }
                else
                {
                    appended = enforceMemoryBudget
                        ? SmoExternalLevelModelAppender.Append(
                            current,
                            currentTemplateIndex,
                            model.ImportedScene,
                            placementTransforms,
                            model.Name)
                        : SmoExternalLevelModelAppender.AppendWithoutMemoryGuard(
                            current,
                            currentTemplateIndex,
                            model.ImportedScene,
                            placementTransforms,
                            model.Name);
                }
                outputData = appended.Data;
                addedObjectCount += appended.AddedObjectCount;
                log.Info(
                    "EXTERNAL_MODEL_ADD",
                    $"model={model.Id}; name={model.Name}; source={model.SourcePath}; " +
                    $"parts={model.ImportedScene.Meshes.Count}; " +
                    $"placements={appended.PlacementCount}; " +
                    $"meshIds={string.Join(",", appended.MeshObjectIds)}; " +
                    $"objects={appended.AddedObjectCount}");
                LogMemory(
                    log,
                    "EXTERNAL_MODEL_ADD_END",
                    $"model={model.Id}; containerBytes={outputData.Length}");
                current = null!;
                appended = null!;
                ReleasePipelineIntermediates(log, outputData, "EXTERNAL_MODEL_ADDED");
                pipeline.Complete(
                    SmoLevelSaveStage.AddingExternalModel,
                    $"Внешняя модель {model.Name} упакована",
                    outputData.LongLength);
            }
            // Materialize pending clones before removing original placements.
            // A clone can be the remaining reference that keeps a shared mesh
            // resource alive; reversing this order may prune the resource and
            // make the later placement impossible to resolve.
            foreach (SmoLevelPlacementAddition addition in
                     state.PlacementAdditions.OrderBy(item => item.Id))
            {
                cancellationToken.ThrowIfCancellationRequested();
                pipeline.Report(
                    SmoLevelSaveStage.AddingPlacement,
                    $"Добавление размещения {addition.Name}");
                SmoDocument current = SmoDocument.ParseOwned(
                    outputData,
                    source.SourcePath);
                uint sourceMeshId = source.Objects[addition.MeshObjectIndex].Id;
                uint activeMeshId = replacementMeshIds.GetValueOrDefault(
                    sourceMeshId,
                    sourceMeshId);
                int currentMeshObjectIndex = current.Objects.Single(entry =>
                    entry.Id == activeMeshId).Index;
                log.Info(
                    "PLACEMENT_ADD_BEGIN",
                    $"edit={addition.Id}; sourceMesh={addition.MeshObjectIndex}; " +
                    $"sourceId={sourceMeshId}; activeId={activeMeshId}; " +
                    $"currentMesh={currentMeshObjectIndex}; name={addition.Name}");
                SmoSharedPlacementCloneResult added =
                    SmoSharedPlacementCloner.Clone(
                        current,
                        currentMeshObjectIndex,
                        addition.WorldTransform,
                        addition.Name);
                outputData = added.Data;
                addedObjectCount += added.AddedObjectCount;
                log.Info(
                    "PLACEMENT_ADD",
                    $"edit={addition.Id}; mesh={addition.MeshObjectIndex}; " +
                    $"static={added.StaticObjectIndex}; model={added.ModelObjectIndex}; " +
                    $"objects={added.AddedObjectCount}; matrix={Format(addition.WorldTransform)}");
                current = null!;
                added = null!;
                ReleasePipelineIntermediates(log, outputData, "PLACEMENT_ADDED");
                pipeline.Complete(
                    SmoLevelSaveStage.AddingPlacement,
                    $"Размещение {addition.Name} добавлено",
                    outputData.LongLength);
            }
            foreach (SmoLevelSaveRemoval removedEntity in
                     state.Removals.OrderBy(item => item.SceneObjectIndex))
            {
                var removedId = new SmoLevelEntityId(removedEntity.SceneObjectIndex);
                cancellationToken.ThrowIfCancellationRequested();
                pipeline.Report(
                    SmoLevelSaveStage.RemovingEntity,
                    $"Удаление объекта [{removedId.SceneObjectIndex}]");
                SmoDocument current = SmoDocument.ParseOwned(outputData, source.SourcePath);
                if (!TryResolveCurrentObjectIndex(
                    source,
                    current,
                    removedId.SceneObjectIndex,
                    replacementPrimaryModelIds,
                    out int currentSceneIndex))
                {
                    log.Info(
                        "PLACEMENT_REMOVE",
                        $"scene={removedId.SceneObjectIndex}; already removed with a shared branch");
                    pipeline.Complete(
                        SmoLevelSaveStage.RemovingEntity,
                        $"Объект [{removedId.SceneObjectIndex}] уже удалён общей веткой",
                        outputData.LongLength);
                    continue;
                }
                if (removedEntity.Kind == SmoLevelEntityKind.Collision)
                {
                    SmoCollisionBranchRemovalResult removed =
                        SmoCollisionBranchRemover.Remove(current, currentSceneIndex);
                    outputData = removed.Data;
                    addedObjectCount -= removed.RemovedObjectCount;
                    log.Info(
                        "COLLISION_REMOVE",
                        $"scene={removedId.SceneObjectIndex}; " +
                        $"objects={removed.RemovedObjectCount}");
                    removed = null!;
                }
                else
                {
                    SmoLevelPlacementRemovalResult removed =
                        SmoLevelPlacementRemover.Remove(current, currentSceneIndex);
                    outputData = removed.Data;
                    addedObjectCount -= removed.RemovedObjectCount;
                    log.Info(
                        "PLACEMENT_REMOVE",
                        $"scene={removedId.SceneObjectIndex}; " +
                        $"objects={removed.RemovedObjectCount}; " +
                        $"relocatedResources={string.Join(",", removed.RelocatedResourceIds)}");
                    removed = null!;
                }
                current = null!;
                ReleasePipelineIntermediates(log, outputData, "ENTITY_REMOVED");
                pipeline.Complete(
                    SmoLevelSaveStage.RemovingEntity,
                    $"Объект [{removedId.SceneObjectIndex}] удалён",
                    outputData.LongLength);
            }
            foreach (SmoLevelSaveCollision generated in
                     state.GeneratedCollisions.OrderBy(item => item.EntityIndex))
            {
                cancellationToken.ThrowIfCancellationRequested();
                pipeline.Report(
                    SmoLevelSaveStage.AddingCollision,
                    $"Добавление коллизии {generated.Name}");
                Vector3[] worldPositions = generated.Positions
                    .Select(position => Vector3.Transform(
                        position,
                        generated.WorldTransform))
                    .ToArray();
                SmoDocument current = SmoDocument.ParseOwned(
                    outputData,
                    source.SourcePath);
                SmoCollisionBranchAppendResult appended =
                    SmoCollisionBranchAppender.Append(
                        current,
                        worldPositions,
                        generated.TriangleIndices,
                        generated.Name);
                outputData = appended.Data;
                addedObjectCount += appended.AddedObjectCount;
                log.Info(
                    "COLLISION_ADD",
                    $"edit={generated.EntityIndex}; " +
                    $"name={generated.Name}; vertices={generated.Positions.Count}; " +
                    $"triangles={generated.TriangleIndices.Count / 3}; " +
                    $"collision={appended.CollisionInfoObjectIndex}; " +
                    $"meshBV={appended.MeshBoundingVolumeObjectIndex}; " +
                    $"objects={appended.AddedObjectCount}");
                current = null!;
                appended = null!;
                ReleasePipelineIntermediates(log, outputData, "COLLISION_ADDED");
                pipeline.Complete(
                    SmoLevelSaveStage.AddingCollision,
                    $"Коллизия {generated.Name} добавлена",
                    outputData.LongLength);
            }

            cancellationToken.ThrowIfCancellationRequested();
            pipeline.Report(
                SmoLevelSaveStage.RepairingCollisions,
                "Проверка реестра коллизий");
            SmoDocument collisionRegistrationDocument = SmoDocument.ParseOwned(
                outputData,
                source.SourcePath);
            SmoCollisionRegistrationRepairResult registrationRepair =
                SmoCollisionBranchAppender.EnsureRegistrations(
                    collisionRegistrationDocument);
            outputData = registrationRepair.Data;
            log.Info(
                "COLLISION_REGISTRY",
                registrationRepair.RegisteredCollisionInfoObjectIndices.Count == 0
                    ? "verified; repaired=0"
                    : $"repaired={registrationRepair.RegisteredCollisionInfoObjectIndices.Count}; " +
                      $"collisions={string.Join(",", registrationRepair.RegisteredCollisionInfoObjectIndices)}");
            collisionRegistrationDocument = null!;
            registrationRepair = null!;
            ReleasePipelineIntermediates(log, outputData, "COLLISION_REGISTRY_REPAIRED");
            pipeline.Complete(
                SmoLevelSaveStage.RepairingCollisions,
                "Реестр коллизий проверен",
                outputData.LongLength);

            cancellationToken.ThrowIfCancellationRequested();
            pipeline.Report(
                SmoLevelSaveStage.RepairingCollisions,
                "Проверка совместимости collision-мешей");
            SmoDocument collisionCompatibilityDocument = SmoDocument.ParseOwned(
                outputData,
                source.SourcePath);
            SmoCollisionMeshCompatibilityRepairResult compatibilityRepair =
                SmoCollisionBranchAppender.EnsureMeshCompatibility(
                    collisionCompatibilityDocument);
            outputData = compatibilityRepair.Data;
            log.Info(
                "COLLISION_MESH_COMPATIBILITY",
                compatibilityRepair.RebuiltCollisionInfoObjectIndices.Count == 0
                    ? "verified; rebuilt=0"
                    : $"rebuilt={compatibilityRepair.RebuiltCollisionInfoObjectIndices.Count}; " +
                      $"collisions={string.Join(",", compatibilityRepair.RebuiltCollisionInfoObjectIndices)}");
            collisionCompatibilityDocument = null!;
            compatibilityRepair = null!;
            ReleasePipelineIntermediates(log, outputData, "COLLISION_MESHES_REPAIRED");
            pipeline.Complete(
                SmoLevelSaveStage.RepairingCollisions,
                "Collision-меши проверены",
                outputData.LongLength);

            cancellationToken.ThrowIfCancellationRequested();
            pipeline.Report(
                SmoLevelSaveStage.WritingTemporaryFile,
                "Запись и проверка временного файла");
            log.Info("TEMP_WRITE", $"path={temporaryPath}");
            File.WriteAllBytes(temporaryPath, outputData);
            SmoDocument verification = SmoDocument.Load(temporaryPath);
            log.Info(
                "TEMP_VERIFY",
                $"objects={verification.Objects.Count}; bytes={verification.Data.Length}; " +
                $"diagnostics={verification.Diagnostics.Count}; errors={verification.HasErrors}");
            if (verification.Objects.Count != source.Objects.Count + addedObjectCount)
            {
                throw new InvalidDataException(
                    "Saved SMO failed structural verification.");
            }
            verification = null!;
            ReleasePipelineIntermediates(log, outputData, "TEMP_FILE_VERIFIED");
            pipeline.Complete(
                SmoLevelSaveStage.WritingTemporaryFile,
                "Временный файл проверен",
                outputData.LongLength);

            // Installation is deliberately non-cancellable: from this point the
            // pipeline must finish the atomic replace and verify its result.
            pipeline.Report(
                SmoLevelSaveStage.Installing,
                "Атомарная установка сохранённого уровня");
            if (File.Exists(fullOutputPath))
            {
                backupPath = fullOutputPath +
                    $".{DateTime.Now:yyyyMMdd-HHmmss-fff}.bak";
                log.Info(
                    "INSTALL",
                    $"Atomic replace requested; destination={fullOutputPath}; " +
                    $"backup={backupPath}");
                File.Replace(
                    temporaryPath,
                    fullOutputPath,
                    backupPath,
                    ignoreMetadataErrors: true);
            }
            else
            {
                log.Info(
                    "INSTALL",
                    $"New destination requested; moving temporary file to {fullOutputPath}");
                File.Move(temporaryPath, fullOutputPath);
            }

            SmoDocument finalVerification = SmoDocument.Load(fullOutputPath);
            log.Info(
                "FINAL_VERIFY",
                $"objects={finalVerification.Objects.Count}; " +
                $"bytes={finalVerification.Data.Length}; " +
                $"diagnostics={finalVerification.Diagnostics.Count}; " +
                $"errors={finalVerification.HasErrors}; " +
                $"sha256={Hash(finalVerification.Data.Span)}");
            log.Info(
                "SUCCESS",
                $"Save complete; output={fullOutputPath}; backup={backupPath ?? "NONE"}; " +
                $"log={log.FilePath}");
            finalVerification = null!;
            pipeline.Complete(
                SmoLevelSaveStage.Complete,
                "Уровень сохранён и проверен",
                outputData.LongLength);

            // The caller owns editor history and marks it saved on its UI thread.
            return new SmoLevelSaveResult(
                fullOutputPath,
                backupPath,
                log.FilePath,
                editedEntities.Count,
                patch.StaticObjectIndices.Count + patch.NodeObjectIndices.Count +
                    patch.CollisionMeshObjectIndices.Count,
                state.PlacementAdditions.Count + state.ExternalPlacements.Count,
                state.ModelReplacements.Count + state.TextureReplacements.Count +
                    state.GeneratedCollisions.Count,
                outputData.LongLength);
        }
        catch (OperationCanceledException)
        {
            log.Warning("SAVE_CANCELLED", "Save cancelled between pipeline steps.");
            throw;
        }
        catch (Exception exception)
        {
            log.Error("SAVE_FAILED", exception);
            throw new SmoLevelSaveException(
                exception.Message,
                log.FilePath,
                exception);
        }
        finally
        {
            if (File.Exists(temporaryPath))
            {
                try
                {
                    File.Delete(temporaryPath);
                    log.Info("TEMP_CLEANUP", $"Deleted {temporaryPath}");
                }
                catch (Exception cleanupException)
                {
                    log.Warning(
                        "TEMP_CLEANUP_FAILED",
                        $"path={temporaryPath}; error={cleanupException}");
                }
            }
        }
    }

    private static void ReleasePipelineIntermediates(
        SmoLevelSaveLog log,
        byte[] currentData,
        string stage)
    {
        SmoLargeContainerMemory.ReleaseIntermediates(
            currentData.Length,
            compact: true);
        LogMemory(
            log,
            $"PIPELINE_{stage}",
            $"containerBytes={currentData.Length}");
    }

    private sealed class SavePipelineProgress(
        IProgress<SmoLevelSaveProgress>? progress,
        int totalSteps,
        long initialContainerBytes)
    {
        private int _completedSteps;
        private long _currentContainerBytes = initialContainerBytes;

        public void Report(SmoLevelSaveStage stage, string message) =>
            progress?.Report(new SmoLevelSaveProgress(
                stage,
                _completedSteps,
                totalSteps,
                message,
                _currentContainerBytes));

        public void Complete(
            SmoLevelSaveStage stage,
            string message,
            long currentContainerBytes)
        {
            _currentContainerBytes = currentContainerBytes;
            _completedSteps = Math.Min(totalSteps, _completedSteps + 1);
            progress?.Report(new SmoLevelSaveProgress(
                stage,
                _completedSteps,
                totalSteps,
                message,
                currentContainerBytes));
        }
    }

    private static string Hash(ReadOnlySpan<byte> data) =>
        Convert.ToHexString(SHA256.HashData(data));

    private static void LogMemory(
        SmoLevelSaveLog log,
        string stage,
        string details)
    {
        using Process process = Process.GetCurrentProcess();
        GCMemoryInfo memory = GC.GetGCMemoryInfo();
        log.Info(
            stage,
            $"{details}; managed={GC.GetTotalMemory(forceFullCollection: false)}; " +
            $"workingSet={process.WorkingSet64}; private={process.PrivateMemorySize64}; " +
            $"memoryLoad={memory.MemoryLoadBytes}; " +
            $"highThreshold={memory.HighMemoryLoadThresholdBytes}");
    }

    private static Vector3 Translation(Matrix4x4 value) =>
        new(value.M41, value.M42, value.M43);

    private static int ResolveCurrentObjectIndex(
        SmoDocument source,
        SmoDocument current,
        int sourceObjectIndex,
        IReadOnlyDictionary<uint, uint>? replacementObjectIds = null)
    {
        if ((uint)sourceObjectIndex >= (uint)source.Objects.Count)
            throw new ArgumentOutOfRangeException(nameof(sourceObjectIndex));
        uint id = source.Objects[sourceObjectIndex].Id;
        if (replacementObjectIds is not null &&
            replacementObjectIds.TryGetValue(id, out uint replacementId))
        {
            id = replacementId;
        }
        return current.Objects.Single(entry => entry.Id == id).Index;
    }

    private static bool TryResolveCurrentObjectIndex(
        SmoDocument source,
        SmoDocument current,
        int sourceObjectIndex,
        IReadOnlyDictionary<uint, uint>? replacementObjectIds,
        out int currentObjectIndex)
    {
        if ((uint)sourceObjectIndex >= (uint)source.Objects.Count)
            throw new ArgumentOutOfRangeException(nameof(sourceObjectIndex));
        uint id = source.Objects[sourceObjectIndex].Id;
        if (replacementObjectIds is not null &&
            replacementObjectIds.TryGetValue(id, out uint replacementId))
        {
            id = replacementId;
        }
        SmoObjectEntry? entry = current.Objects.SingleOrDefault(candidate =>
            candidate.Id == id);
        currentObjectIndex = entry?.Index ?? -1;
        return entry is not null;
    }

    private static string Format(Vector3 value) =>
        FormattableString.Invariant($"({value.X:R},{value.Y:R},{value.Z:R})");

    private static string Format(Matrix4x4 value) => FormattableString.Invariant(
        $"[{value.M11:R},{value.M12:R},{value.M13:R},{value.M14:R};{value.M21:R},{value.M22:R},{value.M23:R},{value.M24:R};{value.M31:R},{value.M32:R},{value.M33:R},{value.M34:R};{value.M41:R},{value.M42:R},{value.M43:R},{value.M44:R}]");
}
