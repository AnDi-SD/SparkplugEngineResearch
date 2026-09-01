using System.Numerics;
using SmoImporter.Core;
using SmoViewer.Core;

namespace SmoLVLcreator.Core;

public sealed record SmoProjectCollisionAddition(
    uint CollisionInfoObjectId,
    uint MeshBoundingVolumeObjectId,
    IReadOnlyList<Guid> AssetIds);

public sealed record SmoProjectExternalModelAddition(
    IReadOnlyList<uint> MeshObjectIds,
    IReadOnlyDictionary<int, uint> ImportedTextureObjectIds,
    IReadOnlyList<uint> PlacementRootObjectIds,
    int PlacementCount,
    IReadOnlyList<Guid> AssetIds);

/// <summary>
/// Adapts serializer-owned importer plans to the immutable project
/// journal. It does not parse source model formats and does not rewrite an SMO.
/// </summary>
public static class SmoProjectImporterBridge
{
    public static SmoProjectExternalModelAddition AddExternalModelBatched(
        SmoProject project,
        ImportedScene importedScene,
        IReadOnlyList<Matrix4x4> worldTransforms,
        string modelSourcePath,
        string modelName,
        string workerExecutable,
        string? workerAssembly,
        string? nativeFbxBridgePath = null,
        Action<int, int>? progress = null,
        CancellationToken cancellationToken = default)
    {
        ArgumentNullException.ThrowIfNull(project);
        ArgumentNullException.ThrowIfNull(importedScene);
        ArgumentNullException.ThrowIfNull(worldTransforms);
        ArgumentException.ThrowIfNullOrWhiteSpace(modelSourcePath);
        ArgumentException.ThrowIfNullOrWhiteSpace(modelName);
        ArgumentException.ThrowIfNullOrWhiteSpace(workerExecutable);
        if (worldTransforms.Count == 0)
        {
            throw new ArgumentException(
                "An project external model needs at least one placement.",
                nameof(worldTransforms));
        }

        SmoDocument importedSource =
            SmoProjectSerializer.CreateImportedSourceDocument(project);
        int templateIndex = SmoExternalLevelModelAppender.FindTemplateMeshObjectIndex(
            importedSource,
            requireMaterial: importedScene.Textures.Count > 0);
        SmoExternalModelForestPlanResult planned =
            SmoExternalModelBatchPipeline.CreatePlan(
            importedSource,
            templateIndex,
            importedScene,
            worldTransforms,
            modelName,
            modelSourcePath,
            workerExecutable,
            workerAssembly,
            nativeFbxBridgePath,
            progress,
            cancellationToken);
        return AddExternalModelPlanResult(project, planned);
    }

    private static SmoProjectExternalModelAddition AddExternalModelPlanResult(
        SmoProject project,
        SmoExternalModelForestPlanResult planned)
    {
        SmoAdditiveForestPlan plan = planned.Plan;
        uint[] allocated = project.AreObjectIdsAvailable(plan.GeneratedObjectIds)
            ? plan.GeneratedObjectIds.ToArray()
            : project.AllocateObjectIds(plan.GeneratedObjectIds.Count);
        Dictionary<uint, uint> idMap = plan.GeneratedObjectIds
            .Select((id, index) => (id, NewId: allocated[index]))
            .ToDictionary(item => item.id, item => item.NewId);
        SmoAdditiveForestPlan remapped = plan.GeneratedObjectIds
            .Where((id, index) => id != allocated[index])
            .Any()
                ? SmoAdditiveForestPlanner.RemapObjectIds(plan, idMap)
                : plan;
        IReadOnlyList<Guid> assets = AddOperations(project, remapped.Operations);
        return new SmoProjectExternalModelAddition(
            planned.MeshObjectIds.Select(id => idMap[id]).ToArray(),
            planned.ImportedTextureObjectIds.ToDictionary(
                pair => pair.Key,
                pair => idMap[pair.Value]),
            remapped.Operations
                .SelectMany(operation => operation.Attachment.Entries)
                .Where(entry => entry.TypeHash == SmoClassIds.StaticRenderObject)
                .Select(entry => entry.Id)
                .ToArray(),
            planned.PlacementCount,
            assets);
    }

    public static SmoProjectCollisionAddition AddCollision(
        SmoProject project,
        IReadOnlyList<Vector3> worldPositions,
        IReadOnlyList<int> triangleIndices,
        string? name = null)
    {
        SmoDocument importedSource =
            SmoProjectSerializer.CreateImportedSourceDocument(project);
        return AddCollision(
            project,
            importedSource,
            worldPositions,
            triangleIndices,
            name);
    }

    public static SmoProjectCollisionAddition AddCollision(
        SmoProject project,
        SmoDocument importedSource,
        IReadOnlyList<Vector3> worldPositions,
        IReadOnlyList<int> triangleIndices,
        string? name = null)
    {
        ArgumentNullException.ThrowIfNull(project);
        ArgumentNullException.ThrowIfNull(importedSource);
        EnsureMatchingImportedSource(project, importedSource);
        uint[] ids = project.AllocateObjectIds(2);
        SmoCollisionBranchPlan plan = SmoCollisionBranchAppender.CreatePlan(
            importedSource,
            worldPositions,
            triangleIndices,
            ids[0],
            ids[1],
            name);
        IReadOnlyList<Guid> assets = AddOperations(project, plan.Operations);
        return new SmoProjectCollisionAddition(
            plan.CollisionInfoObjectId,
            plan.MeshBoundingVolumeObjectId,
            assets);
    }

    public static IReadOnlyList<Guid> AddOperations(
        SmoProject project,
        IReadOnlyList<SmoVisualForestOperation> operations)
    {
        ArgumentNullException.ThrowIfNull(project);
        ArgumentNullException.ThrowIfNull(operations);
        if (operations.Count == 0)
            return [];

        PreparedOperation[] prepared = operations
            .Select(operation => Prepare(project, operation))
            .ToArray();
        var assetIds = new List<Guid>(prepared.Length);
        try
        {
            foreach (PreparedOperation operation in prepared)
            {
                assetIds.Add(project.AddInlineForest(
                    operation.TargetOwnerIndex,
                    operation.InsertionRelativeOffset,
                    operation.FieldData,
                    operation.Objects));
            }
            return assetIds.AsReadOnly();
        }
        catch
        {
            project.DiscardAddedForests(assetIds);
            throw;
        }
    }

    private static PreparedOperation Prepare(
        SmoProject project,
        SmoVisualForestOperation operation)
    {
        ArgumentNullException.ThrowIfNull(operation);
        ArgumentNullException.ThrowIfNull(operation.Attachment);
        SmoVisualForestAttachment attachment = operation.Attachment;
        SmoProjectObject owner = project.Objects.SingleOrDefault(item =>
                item.Id == attachment.TargetOwnerId)
            ?? throw new InvalidDataException(
                $"Importer attachment owner {attachment.TargetOwnerId} is missing.");
        if (!owner.FieldStreamDecoded)
        {
            throw new InvalidDataException(
                $"Importer attachment owner {owner.Index} has no decoded fields.");
        }
        int insertion = operation.InsertionKind switch
        {
            SmoVisualForestInsertionKind.BeforeTerminal => owner.Fields
                .Where(field => field.PayloadSize == 0 &&
                                field.RelativeHeaderOffset + field.HeaderSize ==
                                owner.SerializedSize)
                .Select(field => field.RelativeHeaderOffset)
                .SingleOrDefault(-1),
            SmoVisualForestInsertionKind.AfterLastFieldType => ResolveAfterLastField(
                owner,
                operation.AnchorFieldType),
            _ => throw new ArgumentOutOfRangeException(
                nameof(operation),
                operation.InsertionKind,
                "Unknown importer attachment insertion kind.")
        };
        if (insertion < 0)
        {
            throw new InvalidDataException(
                $"Importer attachment anchor is missing from owner {owner.Index}.");
        }

        var stack = new List<SmoVisualForestEntry>();
        var parentIds = new Dictionary<uint, uint>();
        foreach (SmoVisualForestEntry entry in attachment.Entries
                     .OrderBy(item => item.RelativeOffset)
                     .ThenByDescending(item => item.SerializedSize))
        {
            ulong start = checked((uint)entry.RelativeOffset);
            ulong end = start + entry.SerializedSize;
            while (stack.Count > 0 && start >=
                   (ulong)stack[^1].RelativeOffset + stack[^1].SerializedSize)
            {
                stack.RemoveAt(stack.Count - 1);
            }
            if (stack.Count > 0 && end >
                (ulong)stack[^1].RelativeOffset + stack[^1].SerializedSize)
            {
                throw new InvalidDataException(
                    $"Importer object {entry.Id} partially overlaps another object.");
            }
            parentIds.Add(entry.Id, stack.Count == 0 ? owner.Id : stack[^1].Id);
            stack.Add(entry);
        }
        SmoProjectAddedObject[] objects = attachment.Entries
            .Select(entry => new SmoProjectAddedObject
            {
                Id = entry.Id,
                RawName = entry.RawName.ToArray(),
                TypeHash = entry.TypeHash,
                ObjectRelativeOffset = checked((uint)entry.RelativeOffset),
                SerializedSize = entry.SerializedSize,
                ParentObjectId = parentIds[entry.Id]
            })
            .ToArray();
        return new PreparedOperation(
            owner.Index,
            insertion,
            attachment.FieldData,
            objects);
    }

    private static int ResolveAfterLastField(
        SmoProjectObject owner,
        int fieldType)
    {
        SmoProjectField? anchor = owner.Fields
            .Where(field => field.FieldType == fieldType)
            .OrderBy(field => field.RelativeHeaderOffset)
            .LastOrDefault();
        if (anchor is null)
            return -1;
        int insertion = checked(
            anchor.RelativePayloadOffset + (int)anchor.PayloadSize);
        return owner.Fields.Any(field => field.RelativeHeaderOffset == insertion)
            ? insertion
            : -1;
    }

    private static void EnsureMatchingImportedSource(
        SmoProject project,
        SmoDocument source)
    {
        if (source.HasErrors || source.Objects.Count != project.Objects.Count ||
            source.Header.DataSize != project.DataSection.Length)
        {
            throw new InvalidDataException(
                "The importer plan source does not match the project base.");
        }
        int dataStart = checked((int)source.Header.DataStart);
        if (!source.Data.Span.Slice(dataStart).SequenceEqual(project.DataSection.Span))
        {
            throw new InvalidDataException(
                "The importer plan source data differs from immutable project data.bin.");
        }
        for (int index = 0; index < project.Objects.Count; index++)
        {
            SmoProjectObject expected = project.Objects[index];
            SmoObjectEntry actual = source.Objects[index];
            if (actual.Id != expected.Id ||
                actual.TypeHash != expected.TypeHash ||
                actual.LogicalOffset != expected.LogicalOffset ||
                actual.SerializedSize != expected.SerializedSize ||
                !actual.RawName.Span.SequenceEqual(expected.RawName))
            {
                throw new InvalidDataException(
                    $"Importer plan source catalog differs at object {index}.");
            }
        }
    }

    private sealed record PreparedOperation(
        int TargetOwnerIndex,
        int InsertionRelativeOffset,
        byte[] FieldData,
        IReadOnlyList<SmoProjectAddedObject> Objects);
}
