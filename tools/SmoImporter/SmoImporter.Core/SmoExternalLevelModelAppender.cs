using System.Numerics;
using System.Text;
using SmoViewer.Core;

namespace SmoImporter.Core;

public sealed record SmoExternalLevelModelAppendResult(
    byte[] Data,
    int AddedObjectCount,
    IReadOnlyList<uint> MeshObjectIds,
    int PlacementCount)
{
    public IReadOnlyDictionary<int, uint> ImportedTextureObjectIds { get; init; } =
        new Dictionary<int, uint>();
}

/// <summary>
/// Serializes a new rigid model resource and all of its level placements in
/// one pass. Geometry and textures are written once per imported mesh part;
/// later placements are ordinary reference-only static branches.
/// </summary>
public static class SmoExternalLevelModelAppender
{
    public static int FindTemplateMeshObjectIndex(
        SmoDocument document,
        bool requireMaterial = false)
    {
        ArgumentNullException.ThrowIfNull(document);
        foreach (int meshIndex in SmoSharedMeshInstanceResolver.ResolveAll(document)
                     .Select(instance => instance.SourceMeshObjectIndex)
                     .Distinct())
        {
            try
            {
                SmoLevelModelGraphPlan plan =
                    SmoLevelModelGraphReplacer.ResolvePlan(document, meshIndex);
                if (plan.Components.Count != 1 ||
                    requireMaterial && plan.Components[0].MaterialObjectId is null)
                    continue;
                SmoObjectEntry entry = document.Objects[meshIndex];
                SmoMesh mesh = SmoMeshDecoder.Decode(document, entry);
                SmoMeshResourceReplacer.ValidateWritableLayout(entry, mesh);
                return meshIndex;
            }
            catch (Exception)
            {
                // Try the next confirmed shared rigid resource.
            }
        }
        throw new NotSupportedException(
            "The level has no writable shared rigid model template.");
    }

    internal static SmoExternalLevelModelAppendResult AppendPartRange(
        SmoDocument document,
        int templateMeshObjectIndex,
        ImportedScene importedScene,
        IReadOnlyList<Matrix4x4> worldTransforms,
        string? modelName,
        int firstPartIndex,
        int partCount,
        IReadOnlyDictionary<int, uint>? reusableImportedTextureObjectIds = null)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(importedScene);
        ArgumentNullException.ThrowIfNull(worldTransforms);
        importedScene = SmoLevelRigidImportPreparer.Prepare(importedScene);
        if (firstPartIndex < 0 || partCount <= 0 ||
            firstPartIndex > importedScene.Meshes.Count - partCount)
        {
            throw new ArgumentOutOfRangeException(
                nameof(firstPartIndex),
                "Requested external-model part range is outside the imported scene.");
        }
        if (worldTransforms.Count == 0)
            return new(document.Data.ToArray(), 0, [], 0);
        uint templateMeshId = document.Objects[templateMeshObjectIndex].Id;
        string safeName = string.IsNullOrWhiteSpace(modelName)
            ? "ExternalModel"
            : modelName.Trim();
        byte[]? output = null;
        var generatedMeshIds = new List<uint>(partCount);
        IReadOnlyDictionary<int, uint> importedTextureObjectIds =
            reusableImportedTextureObjectIds ?? new Dictionary<int, uint>();
        int end = checked(firstPartIndex + partCount);
        for (int partIndex = firstPartIndex; partIndex < end; partIndex++)
        {
            SmoDocument current = partIndex == firstPartIndex
                ? document
                : SmoDocument.ParseOwned(output!, document.SourcePath);
            PartAppendResult part = AppendPart(
                current,
                templateMeshId,
                importedScene,
                worldTransforms,
                safeName,
                partIndex,
                importedTextureObjectIds);
            output = part.Data;
            generatedMeshIds.Add(part.MeshObjectId);
            importedTextureObjectIds = part.ImportedTextureObjectIds;
            part = null!;
            current = null!;
            SmoLargeContainerMemory.ReleaseIntermediates(
                output.Length,
                compact: true);
        }

        SmoDocument verified = SmoDocument.ParseOwned(output!, document.SourcePath);
        if (verified.HasErrors)
            throw new InvalidDataException(
                "The external-model part batch failed structural verification.");
        foreach (uint meshId in generatedMeshIds)
            SmoMeshDecoder.Decode(verified, verified.Objects.Single(entry => entry.Id == meshId));
        return new(
            output!,
            verified.Objects.Count - document.Objects.Count,
            generatedMeshIds,
            worldTransforms.Count)
        {
            ImportedTextureObjectIds = importedTextureObjectIds
        };
    }

    private static uint NextObjectId(SmoDocument document)
    {
        uint result = document.Objects.Max(entry => entry.Id);
        do result = checked(result + 1);
        while (document.Objects.Any(entry => entry.Id == result));
        return result;
    }

    private static PartAppendResult AppendPart(
        SmoDocument current,
        uint templateMeshId,
        ImportedScene importedScene,
        IReadOnlyList<Matrix4x4> worldTransforms,
        string safeName,
        int partIndex,
        IReadOnlyDictionary<int, uint> reusableImportedTextureObjectIds)
    {
        string? sourcePath = current.SourcePath;
        int currentTemplateIndex = current.Objects.Single(entry =>
            entry.Id == templateMeshId).Index;
        SmoSharedPlacementCloneResult host = SmoSharedPlacementCloner.Clone(
            current,
            currentTemplateIndex,
            worldTransforms[0],
            $"{safeName}_{partIndex + 1}");
        int hostModelObjectIndex = host.ModelObjectIndex;
        byte[] output = host.Data;
        host = null!;
        current = null!;
        SmoLargeContainerMemory.ReleaseIntermediates(output.Length);

        current = SmoDocument.ParseOwned(output, sourcePath);
        SmoObjectEntry templateMesh = current.Objects.Single(entry =>
            entry.Id == templateMeshId);
        SmoObjectEntry hostModel = current.Objects[hostModelObjectIndex];
        uint temporaryMeshId = NextObjectId(current);
        byte[] templateBytes = current.Data.Span.Slice(
            checked((int)templateMesh.PhysicalOffset),
            checked((int)templateMesh.SerializedSize)).ToArray();
        output = SmoVisualForestInjector.PromoteReferenceToInline(
            current,
            hostModel.Id,
            0,
            templateMeshId,
            temporaryMeshId,
            Encoding.UTF8.GetBytes($"{safeName}_{partIndex + 1}_template\0"),
            SmoClassIds.MeshData,
            templateBytes);
        current = null!;
        SmoLargeContainerMemory.ReleaseIntermediates(output.Length);

        current = SmoDocument.ParseOwned(output, sourcePath);
        currentTemplateIndex = current.Objects.Single(entry =>
            entry.Id == templateMeshId).Index;
        SmoSharedPlacementCloneResult helper = SmoSharedPlacementCloner.Clone(
            current,
            currentTemplateIndex,
            worldTransforms[0],
            $"{safeName}_{partIndex + 1}_resource_helper");
        int helperStaticObjectIndex = helper.StaticObjectIndex;
        int helperModelObjectIndex = helper.ModelObjectIndex;
        output = helper.Data;
        helper = null!;
        current = null!;
        SmoLargeContainerMemory.ReleaseIntermediates(output.Length);
        current = SmoDocument.ParseOwned(output, sourcePath);
        SmoObjectEntry helperStatic = current.Objects[helperStaticObjectIndex];
        uint helperStaticId = helperStatic.Id;
        uint helperParentId = current.Objects[helperStatic.ParentIndex!.Value].Id;
        uint helperModelId = current.Objects[helperModelObjectIndex].Id;
        output = SmoVisualForestInjector.ReplaceReferenceId(
            current,
            helperModelId,
            0,
            templateMeshId,
            temporaryMeshId);
        current = null!;
        SmoLargeContainerMemory.ReleaseIntermediates(output.Length);

        ImportedMesh importedMesh = importedScene.Meshes[partIndex];
        var onePart = new ImportedScene(
            [importedMesh],
            importedScene.Textures,
            importedScene.Materials)
        {
            ImportWarnings = importedScene.ImportWarnings
        };
        current = SmoDocument.ParseOwned(output, sourcePath);
        int temporaryMeshIndex = current.Objects.Single(entry =>
            entry.Id == temporaryMeshId).Index;
        SmoLevelModelGraphReplacementResult replaced =
            SmoLevelModelGraphReplacer.Replace(
                current,
                temporaryMeshIndex,
                onePart,
                ReplacementTransform.Identity,
                Matrix4x4.Identity,
                reusableImportedTextureObjectIds);
        output = replaced.Data;
        uint generatedMeshId = replaced.MeshObjectIds[temporaryMeshId];
        IReadOnlyDictionary<int, uint> importedTextureObjectIds =
            replaced.ImportedTextureObjectIds;
        replaced = null!;
        current = null!;
        SmoLargeContainerMemory.ReleaseIntermediates(output.Length);

        for (int placementIndex = 1;
             placementIndex < worldTransforms.Count;
             placementIndex++)
        {
            current = SmoDocument.ParseOwned(output, sourcePath);
            int generatedMeshIndex = current.Objects.Single(entry =>
                entry.Id == generatedMeshId).Index;
            SmoSharedPlacementCloneResult clone = SmoSharedPlacementCloner.Clone(
                current,
                generatedMeshIndex,
                worldTransforms[placementIndex],
                $"{safeName}_{partIndex + 1}");
            output = clone.Data;
            clone = null!;
            current = null!;
            SmoLargeContainerMemory.ReleaseIntermediates(output.Length);
        }

        current = SmoDocument.ParseOwned(output, sourcePath);
        output = SmoVisualForestInjector.RemoveInlineBranch(
            current,
            helperParentId,
            helperStaticId);
        current = null!;
        SmoLargeContainerMemory.ReleaseIntermediates(output.Length);
        return new PartAppendResult(
            output,
            generatedMeshId,
            importedTextureObjectIds);
    }

    private sealed record PartAppendResult(
        byte[] Data,
        uint MeshObjectId,
        IReadOnlyDictionary<int, uint> ImportedTextureObjectIds);
}
