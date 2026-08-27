using System.Buffers.Binary;
using System.Numerics;
using System.Text;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;
using SmoViewer.Core;

namespace SmoImporter.Core;

public sealed record SmoLevelModelGraphComponent(
    uint ModelObjectId,
    uint? MaterialObjectId,
    uint MeshObjectId,
    int MeshObjectIndex);

public sealed record SmoLevelModelGraphPlan(
    uint RootObjectId,
    IReadOnlyList<SmoLevelModelGraphComponent> Components);

public sealed record SmoLevelModelGraphReplacementResult(
    byte[] Data,
    int AddedObjectCount,
    IReadOnlyDictionary<uint, uint> PrimaryModelObjectIds,
    IReadOnlyDictionary<uint, uint> MeshObjectIds,
    IReadOnlyDictionary<uint, uint> TextureObjectIds,
    int VertexCount,
    int TriangleCount)
{
    public IReadOnlyDictionary<int, uint> ImportedTextureObjectIds { get; init; } =
        new Dictionary<int, uint>();
}

/// <summary>
/// Adds a complete rigid level model as new mesh/texture resources and redirects
/// the selected authoring model plus every reference-only placement to them.
/// The original scene graph stays in place. Replaced leaf resources are omitted
/// from the saved container once no reference uses them; shared textures are
/// retained under one of their remaining consumers.
/// </summary>
public static class SmoLevelModelGraphReplacer
{
    private const int ObjectReferenceSize = 8;
    private const int SerializedTexturePixelOffset = 0x3D;
    private const int SerializedTextureMarkerOffset = 0x3C;
    private static readonly uint[] RigidTextureAlphaMaterialRenderStates =
        [0, 0, 1, 0, 1, 0, 3, 0, 2, 0, 6];

    public static SmoTexture CreatePreviewTexture(
        ImportedTexture imported,
        int objectIndex = -1)
    {
        ArgumentNullException.ThrowIfNull(imported);
        using Image<Rgba32> image = Image.Load<Rgba32>(imported.Data);
        if (image.Width != imported.Width || image.Height != imported.Height)
            throw new InvalidDataException(
                $"Texture {imported.Name} dimensions do not match its image payload.");
        byte[] pixels = EncodeBgra(image);
        return SmoTexture.CreateTransient(
            imported.Name,
            image.Width,
            image.Height,
            pixels,
            objectIndex);
    }

    public static SmoLevelModelGraphPlan ResolvePlan(
        SmoDocument document,
        int selectedMeshObjectIndex)
    {
        ArgumentNullException.ThrowIfNull(document);
        if ((uint)selectedMeshObjectIndex >= (uint)document.Objects.Count ||
            document.Objects[selectedMeshObjectIndex].TypeHash != SmoClassIds.MeshData)
        {
            throw new ArgumentOutOfRangeException(
                nameof(selectedMeshObjectIndex),
                "Selected object is not spMeshData.");
        }

        SmoObjectEntry selected = document.Objects[selectedMeshObjectIndex];
        SmoObjectEntry model = FindAncestor(document.Objects, selected, SmoClassIds.Model) ??
            throw new InvalidOperationException(
                $"Mesh [{selectedMeshObjectIndex}] has no owning spModel.");
        SmoObjectEntry root = FindAncestor(document.Objects, model, SmoClassIds.RenderNode) ?? model;

        SmoLevelModelGraphComponent[] components = document.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.MeshData)
            .Select(entry => (Mesh: entry, Model: FindAncestor(
                document.Objects, entry, SmoClassIds.Model)))
            .Where(pair => pair.Model is not null &&
                (root.TypeHash == SmoClassIds.RenderNode
                    ? FindAncestor(document.Objects, pair.Model!, SmoClassIds.RenderNode)?.Id == root.Id
                    : pair.Model!.Id == root.Id))
            .OrderBy(pair => pair.Mesh.LogicalOffset)
            .Select(pair =>
            {
                SmoObjectEntry owner = pair.Model!;
                SmoObjectEntry? material = document.Objects.FirstOrDefault(entry =>
                    entry.ParentIndex == owner.Index &&
                    entry.TypeHash == SmoClassIds.MaterialData);
                return new SmoLevelModelGraphComponent(
                    owner.Id,
                    material?.Id,
                    pair.Mesh.Id,
                    pair.Mesh.Index);
            })
            .ToArray();

        if (components.Length == 0 ||
            components.All(component => component.MeshObjectId != selected.Id))
        {
            throw new InvalidDataException(
                "The selected model graph contains no confirmed mesh components.");
        }
        return new SmoLevelModelGraphPlan(root.Id, components);
    }

    /// <summary>
    /// Resolves a graph that can be replaced in-place. Every mesh part needs a
    /// reference-only placement to become the owner of its newly serialized
    /// inline resource; checking this while the edit is created prevents an
    /// authoring command that can only fail much later during Save.
    /// </summary>
    public static SmoLevelModelGraphPlan ResolveWritablePlan(
        SmoDocument document,
        int selectedMeshObjectIndex)
    {
        SmoLevelModelGraphPlan plan = ResolvePlan(document, selectedMeshObjectIndex);
        foreach (SmoLevelModelGraphComponent component in plan.Components)
        {
            bool hasWritablePlacement = document.Objects.Any(entry =>
                entry.TypeHash == SmoClassIds.Model &&
                HasReferenceOnly(document, entry, 0, component.MeshObjectId));
            if (!hasWritablePlacement)
            {
                throw new InvalidOperationException(
                    $"Mesh resource {component.MeshObjectId} has no reference-only " +
                    "placement that can own the new inline resource. Use complete " +
                    "composite replacement for this model.");
            }
        }
        return plan;
    }

    /// <summary>
    /// Returns the authored glTF alpha policy for one imported mesh. MASK and
    /// BLEND both require the level renderer's texture-alpha material path;
    /// the legacy engine has no separately confirmed generated MASK contract.
    /// </summary>
    public static bool RequiresTextureAlpha(
        ImportedScene scene,
        ImportedMesh mesh)
    {
        ArgumentNullException.ThrowIfNull(scene);
        ArgumentNullException.ThrowIfNull(mesh);
        return mesh.MaterialIndex >= 0 &&
               mesh.MaterialIndex < scene.Materials.Count &&
               scene.Materials[mesh.MaterialIndex].UsesTextureAlpha;
    }

    public static SmoMaterialRenderStateInfo? CreatePreviewMaterialState(
        ImportedScene scene,
        ImportedMesh importedMesh,
        SmoMesh previewMesh,
        SmoTexture? previewTexture)
    {
        if (!RequiresTextureAlpha(scene, importedMesh))
            return null;
        SmoTextureUvAlphaCoverage coverage = previewTexture is null
            ? default
            : SmoTextureUvAlphaAnalyzer.Analyze(previewMesh, previewTexture);
        return SmoMaterialRenderState.Classify(
                2,
                RigidTextureAlphaMaterialRenderStates)
            .ForConsumer(
                SmoMaterialConsumerKind.RigidOrEffect,
                textureUvAlphaCoverage: coverage) with
        {
            // Explicit glTF MASK/BLEND is authoritative even when its texture
            // happens to contain only binary or currently opaque texels.
            BlendMode = SmoMaterialBlendMode.RigidTextureAlphaSurfaceFinalBlend2
        };
    }

    public static SmoLevelModelGraphReplacementResult Replace(
        SmoDocument document,
        int selectedMeshObjectIndex,
        ImportedScene replacement,
        ReplacementTransform transform,
        Matrix4x4? referenceWorldTransform = null,
        IReadOnlyDictionary<int, uint>? reusableImportedTextureObjectIds = null)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(replacement);
        ArgumentNullException.ThrowIfNull(transform);
        SmoLevelModelGraphPlan plan = ResolveWritablePlan(
            document,
            selectedMeshObjectIndex);
        if (replacement.Meshes.Count != plan.Components.Count)
        {
            throw new InvalidOperationException(
                $"The complete SMO model contains {plan.Components.Count} mesh parts, " +
                $"but the imported model contains {replacement.Meshes.Count}. " +
                "Replacement was cancelled without changing the level.");
        }
        replacement = SmoLevelEmbeddedTextureBudget.Prepare(replacement);
        var reusableTextureIds = reusableImportedTextureObjectIds is null
            ? new Dictionary<int, uint>()
            : new Dictionary<int, uint>(reusableImportedTextureObjectIds);
        foreach ((int textureIndex, uint objectId) in reusableTextureIds)
        {
            if ((uint)textureIndex >= (uint)replacement.Textures.Count ||
                !document.Objects.Any(entry =>
                    entry.Id == objectId &&
                    entry.TypeHash == SmoClassIds.TextureData))
            {
                throw new InvalidDataException(
                    $"Reusable imported texture {textureIndex}->{objectId} " +
                    "does not exist in the current level container.");
            }
        }

        Dictionary<uint, uint[]> originalPlacementModelIds = plan.Components.ToDictionary(
            component => component.MeshObjectId,
            component => document.Objects
                .Where(entry => entry.TypeHash == SmoClassIds.Model &&
                    HasReferenceOnly(document, entry, 0, component.MeshObjectId))
                .Select(entry => entry.Id)
                .ToArray());

        Dictionary<uint, uint[]> oldTextureIdsByMaterial = plan.Components
            .Where(component => component.MaterialObjectId is not null)
            .GroupBy(component => component.MaterialObjectId!.Value)
            .ToDictionary(
                group => group.Key,
                group => ResolveTextureIds(
                    document,
                    document.Objects.Single(entry => entry.Id == group.Key)));
        int[] importedTextureIndices = replacement.Meshes
            .Select(mesh => ResolveImportedTextureIndex(replacement, mesh))
            .Where(index => index >= 0)
            .Distinct()
            .OrderBy(index => index)
            .ToArray();
        int[] texturesToInject = importedTextureIndices
            .Where(index => !reusableTextureIds.ContainsKey(index))
            .ToArray();
        var serializedImportedTextures = new Dictionary<int, byte[]>();
        if (texturesToInject.Length > 0)
        {
            SmoObjectEntry textureTemplate = FindTextureTemplate(
                document, oldTextureIdsByMaterial);
            foreach (int textureIndex in texturesToInject)
            {
                serializedImportedTextures.Add(
                    textureIndex,
                    BuildTextureObject(
                        document,
                        textureTemplate,
                        replacement.Textures[textureIndex]));
            }
        }
        byte[] output = document.Data.ToArray();
        Dictionary<uint, uint[]> placementModelIdsByMesh =
            originalPlacementModelIds.ToDictionary(pair => pair.Key, pair => pair.Value);
        Dictionary<uint, uint[]> placementMaterialIdsByMesh =
            plan.Components.ToDictionary(
                component => component.MeshObjectId,
                component => placementModelIdsByMesh[component.MeshObjectId]
                    .Select(modelId => FindMaterialChildId(document, modelId))
                    .Where(id => id.HasValue)
                    .Select(id => id!.Value)
                    .Concat(component.MaterialObjectId is uint primaryMaterialId
                        ? [primaryMaterialId]
                        : [])
                    .Distinct().ToArray());

        HashSet<uint> changingMaterialIds = placementMaterialIdsByMesh.Values
            .SelectMany(ids => ids)
            .ToHashSet();
        uint[] replacedOldTextureIds = oldTextureIdsByMaterial.Values
            .SelectMany(ids => ids)
            .Distinct()
            .ToArray();
        var retainedOldTextureIds = new HashSet<uint>();
        foreach (uint oldTextureId in replacedOldTextureIds)
        {
            SmoObjectEntry oldTexture = document.Objects.Single(entry =>
                entry.Id == oldTextureId);
            if (oldTexture.ParentIndex is not int ownerIndex)
                throw new InvalidDataException(
                    $"Texture resource {oldTextureId} has no inline owner.");
            uint ownerId = document.Objects[ownerIndex].Id;
            uint[] externalConsumers = document.Objects
                .Where(entry => entry.TypeHash == SmoClassIds.MaterialData &&
                    !changingMaterialIds.Contains(entry.Id) &&
                    HasFieldReference(document, entry, 10, oldTextureId))
                .Select(entry => entry.Id)
                .ToArray();
            if (!changingMaterialIds.Contains(ownerId))
            {
                retainedOldTextureIds.Add(oldTextureId);
                continue;
            }

            byte[] oldBytes = ObjectBytes(document, oldTexture).ToArray();
            byte[] oldName = oldTexture.RawName.ToArray();
            output = SmoVisualForestInjector.DemoteInlineLeafToReference(
                SmoDocument.ParseOwned(output, document.SourcePath),
                ownerId,
                10,
                oldTextureId);
            SmoLargeContainerMemory.ReleaseIntermediates(output.Length);
            if (externalConsumers.Length > 0)
            {
                output = SmoVisualForestInjector.PromoteReferenceToInline(
                    SmoDocument.ParseOwned(output, document.SourcePath),
                    externalConsumers[0],
                    10,
                    oldTextureId,
                    oldTextureId,
                    oldName,
                    oldTexture.TypeHash,
                    oldBytes);
                SmoLargeContainerMemory.ReleaseIntermediates(output.Length);
                retainedOldTextureIds.Add(oldTextureId);
            }
        }

        Dictionary<uint, uint[]> placementTextureIdsByMaterial =
            placementMaterialIdsByMesh.Values
                .SelectMany(ids => ids)
                .Distinct()
                .ToDictionary(
                    materialId => materialId,
                    materialId => ResolveTextureIds(
                        document,
                        document.Objects.Single(entry => entry.Id == materialId)));

        uint[] alphaMaterialIds = plan.Components
            .SelectMany((component, componentIndex) =>
                RequiresTextureAlpha(
                    replacement,
                    replacement.Meshes[componentIndex])
                    ? placementMaterialIdsByMesh[component.MeshObjectId]
                    : Array.Empty<uint>())
            .Distinct()
            .ToArray();
        if (alphaMaterialIds.Length > 0)
        {
            output = PatchRigidTextureAlphaMaterials(
                SmoDocument.ParseOwned(output, document.SourcePath),
                alphaMaterialIds);
            SmoLargeContainerMemory.ReleaseIntermediates(output.Length);
        }

        uint nextId = document.Objects.Max(entry => entry.Id) + 1;
        var newTextureIds = new Dictionary<int, uint>();
        var oldToNewTextureIds = new Dictionary<uint, uint>();
        foreach (int textureIndex in importedTextureIndices)
        {
            if (reusableTextureIds.TryGetValue(textureIndex, out uint reusedId))
            {
                newTextureIds.Add(textureIndex, reusedId);
                continue;
            }
            SmoDocument current = SmoDocument.ParseOwned(output, document.SourcePath);
            ImportedTexture importedTexture = replacement.Textures[textureIndex];
            byte[] textureObject = serializedImportedTextures[textureIndex];
            int hostIndex = Enumerable.Range(0, plan.Components.Count).First(index =>
                plan.Components[index].MaterialObjectId.HasValue &&
                ResolveImportedTextureIndex(replacement, replacement.Meshes[index]) ==
                    textureIndex);
            SmoLevelModelGraphComponent host = plan.Components[hostIndex];
            uint hostMaterialId = host.MaterialObjectId ??
                throw new InvalidOperationException(
                    $"Primary model for mesh {host.MeshObjectId} has no material.");
            uint[] hostTextureReferences = ResolveReferenceOnlyObjectIds(
                current,
                hostMaterialId,
                10,
                SmoClassIds.TextureData,
                requireCatalogEntry: false);
            uint newId = nextId++;
            output = hostTextureReferences.Length switch
            {
                0 => InjectInlineObject(
                    current,
                    hostMaterialId,
                    10,
                    newId,
                    $"{importedTexture.Name}_lvl",
                    SmoClassIds.TextureData,
                    textureObject),
                1 => SmoVisualForestInjector.PromoteReferenceToInline(
                    current,
                    hostMaterialId,
                    10,
                    hostTextureReferences[0],
                    newId,
                    Encoding.UTF8.GetBytes($"{importedTexture.Name}_lvl\0"),
                    SmoClassIds.TextureData,
                    textureObject),
                _ => throw new InvalidOperationException(
                    $"Replacement material {hostMaterialId} has multiple texture references.")
            };
            SmoLargeContainerMemory.ReleaseIntermediates(output.Length);
            newTextureIds.Add(textureIndex, newId);
            reusableTextureIds.Add(textureIndex, newId);

            for (int componentIndex = 0;
                 componentIndex < plan.Components.Count;
                 componentIndex++)
            {
                SmoLevelModelGraphComponent component = plan.Components[componentIndex];
                if (component.MaterialObjectId is not uint materialId ||
                    ResolveImportedTextureIndex(
                        replacement, replacement.Meshes[componentIndex]) != textureIndex)
                    continue;
                foreach (uint oldId in oldTextureIdsByMaterial[materialId])
                    oldToNewTextureIds.TryAdd(oldId, newId);
                foreach (uint placementMaterialId in
                         placementMaterialIdsByMesh[component.MeshObjectId])
                foreach (uint oldId in placementTextureIdsByMaterial[placementMaterialId])
                    oldToNewTextureIds.TryAdd(oldId, newId);
            }
        }

        for (int componentIndex = 0;
             componentIndex < plan.Components.Count;
             componentIndex++)
        {
            SmoLevelModelGraphComponent component = plan.Components[componentIndex];
            if (component.MaterialObjectId is null)
                continue;
            int importedTextureIndex = ResolveImportedTextureIndex(
                replacement, replacement.Meshes[componentIndex]);
            foreach (uint placementMaterialId in
                     placementMaterialIdsByMesh[component.MeshObjectId])
            {
                foreach (uint oldTextureId in
                         placementTextureIdsByMaterial[placementMaterialId])
                {
                    SmoDocument current = SmoDocument.ParseOwned(output, document.SourcePath);
                    SmoObjectEntry material = current.Objects.Single(entry =>
                        entry.Id == placementMaterialId);
                    if (!HasReferenceOnly(current, material, 10, oldTextureId))
                        continue;
                    if (importedTextureIndex >= 0)
                    {
                        // Reference IDs have a fixed-size payload. The container
                        // is already an owned mutable buffer, so copying the
                        // complete level for every material reference is both
                        // unnecessary and catastrophic for large levels.
                        PatchReferenceId(
                            current,
                            output,
                            placementMaterialId,
                            10,
                            oldTextureId,
                            newTextureIds[importedTextureIndex]);
                    }
                    else
                    {
                        output = SmoVisualForestInjector.RemoveReference(
                            current,
                            placementMaterialId,
                            10,
                            oldTextureId);
                        SmoLargeContainerMemory.ReleaseIntermediates(output.Length);
                    }
                }
                if (importedTextureIndex >= 0)
                {
                    uint newTextureId = newTextureIds[importedTextureIndex];
                    SmoDocument current = SmoDocument.ParseOwned(output, document.SourcePath);
                    SmoObjectEntry material = current.Objects.Single(entry =>
                        entry.Id == placementMaterialId);
                    if (!HasFieldReference(current, material, 10, newTextureId))
                    {
                        output = InjectReference(
                            current,
                            placementMaterialId,
                            10,
                            newTextureId);
                        SmoLargeContainerMemory.ReleaseIntermediates(output.Length);
                    }
                }
            }
        }

        var oldToNewMeshIds = new Dictionary<uint, uint>();
        for (int index = 0; index < plan.Components.Count; index++)
        {
            SmoLevelModelGraphComponent component = plan.Components[index];
            SmoDocument current = SmoDocument.ParseOwned(output, document.SourcePath);
            SmoObjectEntry target = current.Objects.Single(entry =>
                entry.Id == component.MeshObjectId);
            SmoMesh template = SmoMeshDecoder.Decode(current, target);
            SmoVertexLayout layout = SmoMeshResourceReplacer.ValidateWritableLayout(
                target, template);
            int boneSlot = SmoMeshResourceReplacer.ResolveBoneSlot(
                current, target, layout, requestedSlot: null);
            byte[] meshObject = SmoMeshResourceReplacer.BuildMeshObject(
                current,
                target,
                template,
                layout,
                replacement.Meshes[index],
                transform,
                boneSlot,
                referenceWorldTransform);
            uint newId = nextId++;
            output = SmoVisualForestInjector.ReplaceInlineLeaf(
                current,
                component.ModelObjectId,
                0,
                component.MeshObjectId,
                newId,
                Encoding.UTF8.GetBytes($"{replacement.Meshes[index].Name}_lvl\0"),
                SmoClassIds.MeshData,
                meshObject);
            SmoLargeContainerMemory.ReleaseIntermediates(output.Length);
            oldToNewMeshIds.Add(component.MeshObjectId, newId);

            uint[] placementModelIds =
                placementModelIdsByMesh[component.MeshObjectId];
            if (placementModelIds.Length > 0)
            {
                SmoDocument patchedCurrent = SmoDocument.ParseOwned(
                    output, document.SourcePath);
                foreach (uint modelId in placementModelIds)
                {
                    // Patch every fixed-size reference against the same parsed
                    // container and the same byte array. Previously this loop
                    // cloned the whole 80+ MiB level once per reference.
                    PatchReferenceId(
                        patchedCurrent,
                        output,
                        modelId,
                        0,
                        component.MeshObjectId,
                        newId);
                }
            }
        }

        SmoDocument verified = SmoDocument.ParseOwned(output, document.SourcePath);
        int removedTextureCount = replacedOldTextureIds.Count(id =>
            !retainedOldTextureIds.Contains(id));
        int expectedObjectCount = document.Objects.Count
            - oldToNewMeshIds.Count
            - removedTextureCount
            + oldToNewMeshIds.Count
            + texturesToInject.Length;
        if (verified.HasErrors ||
            verified.Objects.Count != expectedObjectCount)
        {
            throw new InvalidDataException(
                "The complete model graph failed strict structural verification.");
        }
        SmoObjectEntry sourceRoot = document.Objects.Single(entry =>
            entry.Id == plan.RootObjectId);
        SmoObjectEntry verifiedRoot = verified.Objects.Single(entry =>
            entry.Id == plan.RootObjectId);
        uint? sourceParentId = sourceRoot.ParentIndex is int sourceParentIndex
            ? document.Objects[sourceParentIndex].Id
            : null;
        uint? verifiedParentId = verifiedRoot.ParentIndex is int verifiedParentIndex
            ? verified.Objects[verifiedParentIndex].Id
            : null;
        if (verifiedParentId != sourceParentId)
        {
            throw new InvalidDataException(
                "The primary model root moved to another scene branch during replacement.");
        }
        VerifyOldResourcesUnchanged(document, verified, retainedOldTextureIds);
        foreach (uint removedTextureId in replacedOldTextureIds.Where(id =>
                     !retainedOldTextureIds.Contains(id)))
        {
            if (verified.Objects.Any(entry => entry.Id == removedTextureId) ||
                verified.Objects.Any(entry => HasFieldReference(
                    verified, entry, 10, removedTextureId)))
            {
                throw new InvalidDataException(
                    $"Unused texture resource ID {removedTextureId} survived graph pruning.");
            }
        }

        foreach ((uint oldId, uint newId) in oldToNewMeshIds)
        {
            SmoMeshDecoder.Decode(
                verified, verified.Objects.Single(entry => entry.Id == newId));
            if (verified.Objects.Any(entry => entry.Id == oldId) ||
                verified.Objects.Any(entry => HasFieldReference(
                    verified, entry, 0, oldId)))
            {
                throw new InvalidDataException(
                    $"Unused mesh resource ID {oldId} survived graph pruning.");
            }
            uint hostModelId = plan.Components.Single(component =>
                component.MeshObjectId == oldId).ModelObjectId;
            SmoObjectEntry hostModel = verified.Objects.Single(entry =>
                entry.Id == hostModelId);
            if (!HasFieldReference(verified, hostModel, 0, newId))
            {
                throw new InvalidDataException(
                    $"New primary model {hostModelId} does not own mesh {newId}.");
            }
            foreach (uint referenceModelId in placementModelIdsByMesh[oldId])
            {
                SmoObjectEntry referenceModel = verified.Objects.Single(entry =>
                    entry.Id == referenceModelId);
                if (!HasReferenceOnly(verified, referenceModel, 0, newId) ||
                    HasReferenceOnly(verified, referenceModel, 0, oldId))
                {
                    throw new InvalidDataException(
                        $"Model {referenceModelId} was not redirected from " +
                        $"mesh {oldId} to {newId}.");
                }
            }
        }

        return new SmoLevelModelGraphReplacementResult(
            output,
            verified.Objects.Count - document.Objects.Count,
            plan.Components.ToDictionary(
                component => component.ModelObjectId,
                component => component.ModelObjectId),
            oldToNewMeshIds,
            oldToNewTextureIds,
            replacement.Meshes.Sum(mesh => mesh.Positions.Length),
            replacement.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3))
        {
            ImportedTextureObjectIds = reusableTextureIds
        };
    }

    private static int ResolveImportedTextureIndex(
        ImportedScene scene,
        ImportedMesh mesh)
    {
        if (mesh.MaterialIndex < 0 || mesh.MaterialIndex >= scene.Materials.Count)
            return -1;
        int index = scene.Materials[mesh.MaterialIndex].BaseColorTextureIndex;
        if (index < 0)
            return -1;
        if (index >= scene.Textures.Count)
            throw new InvalidDataException(
                $"Imported material references missing texture {index}.");
        return index;
    }

    private static byte[] PatchRigidTextureAlphaMaterials(
        SmoDocument document,
        IReadOnlyCollection<uint> materialIds)
    {
        byte[] result = document.Data.ToArray();
        foreach (uint materialId in materialIds)
        {
            SmoObjectEntry material = document.Objects.Single(entry =>
                entry.Id == materialId &&
                entry.TypeHash == SmoClassIds.MaterialData);
            bool patchedStates = false;
            bool patchedBlend = false;
            foreach (SmoDataBlockHeader field in Fields(document, material))
            {
                int payload = checked(
                    (int)material.PhysicalOffset + field.PayloadOffset);
                if (field.FieldType == 0 &&
                    field.PayloadSize ==
                        RigidTextureAlphaMaterialRenderStates.Length * sizeof(uint))
                {
                    for (int index = 0;
                         index < RigidTextureAlphaMaterialRenderStates.Length;
                         index++)
                    {
                        WriteUInt32(
                            result,
                            payload + index * sizeof(uint),
                            RigidTextureAlphaMaterialRenderStates[index]);
                    }
                    patchedStates = true;
                }
                else if (field.FieldType == 3 &&
                         field.PayloadSize == sizeof(uint))
                {
                    // FinalBlendOp=2 plus companion RS[8]=2 is the confirmed
                    // rigid texture-alpha surface contract in shipped levels.
                    WriteUInt32(result, payload, 2);
                    patchedBlend = true;
                }
            }
            if (!patchedStates || !patchedBlend)
            {
                throw new InvalidDataException(
                    $"Material {materialId} has no writable rigid alpha render state.");
            }
        }

        SmoDocument verified = SmoDocument.ParseOwned(result, document.SourcePath);
        foreach (uint materialId in materialIds)
        {
            SmoObjectEntry material = verified.Objects.Single(entry =>
                entry.Id == materialId &&
                entry.TypeHash == SmoClassIds.MaterialData);
            if (!SmoMaterialRenderState.TryDecode(
                    verified,
                    material,
                    out SmoMaterialRenderStateInfo? state) ||
                state is null ||
                state.FinalBlendOperation != 2 ||
                !state.MaterialRenderStates.SequenceEqual(
                    RigidTextureAlphaMaterialRenderStates))
            {
                throw new InvalidDataException(
                    $"Material {materialId} did not retain the rigid alpha render state.");
            }
        }
        return result;
    }

    private static uint? FindMaterialChildId(
        SmoDocument document,
        uint modelId)
    {
        SmoObjectEntry model = document.Objects.Single(entry => entry.Id == modelId);
        return document.Objects.FirstOrDefault(entry =>
            entry.ParentIndex == model.Index &&
            entry.TypeHash == SmoClassIds.MaterialData)?.Id;
    }

    private static uint[] ResolveReferenceOnlyObjectIds(
        SmoDocument document,
        uint ownerId,
        byte fieldType,
        uint referencedTypeHash,
        bool requireCatalogEntry = true)
    {
        SmoObjectEntry owner = document.Objects.Single(entry => entry.Id == ownerId);
        HashSet<uint> validIds = document.Objects
            .Where(entry => entry.TypeHash == referencedTypeHash)
            .Select(entry => entry.Id)
            .ToHashSet();
        ReadOnlySpan<byte> bytes = ObjectBytes(document, owner);
        var result = new HashSet<uint>();
        foreach (SmoDataBlockHeader field in Fields(document, owner))
        {
            if (field.FieldType != fieldType ||
                field.PayloadSize != ObjectReferenceSize)
                continue;
            uint id = BinaryPrimitives.ReadUInt32LittleEndian(
                bytes[field.PayloadOffset..]);
            uint inlineSize = BinaryPrimitives.ReadUInt32LittleEndian(
                bytes[(field.PayloadOffset + sizeof(uint))..]);
            if (inlineSize == 0 &&
                (!requireCatalogEntry || validIds.Contains(id)))
                result.Add(id);
        }
        return result.ToArray();
    }

    private static SmoObjectEntry FindTextureTemplate(
        SmoDocument document,
        IReadOnlyDictionary<uint, uint[]> oldTextureIdsByMaterial)
    {
        HashSet<uint> preferred = oldTextureIdsByMaterial.Values
            .SelectMany(ids => ids).ToHashSet();
        foreach (SmoObjectEntry entry in document.Objects
                     .Where(entry => entry.TypeHash == SmoClassIds.TextureData)
                     .OrderByDescending(entry => preferred.Contains(entry.Id)))
        {
            if (SmoTextureDecoder.TryDecode(
                    document, entry, out SmoTexture? texture, out _) &&
                texture is not null &&
                texture.FormatCode is 0x32E3 or 0x43E3 &&
                texture.SourceLayout == SmoTextureLayout.Bgra)
            {
                return entry;
            }
        }
        throw new NotSupportedException(
            "The level has no confirmed BGRA texture template for a new resource.");
    }

    private static byte[] BuildTextureObject(
        SmoDocument document,
        SmoObjectEntry templateEntry,
        ImportedTexture imported)
    {
        if (!SmoTextureDecoder.TryDecode(
                document, templateEntry, out SmoTexture? template, out string error) ||
            template is null)
            throw new InvalidDataException(error);
        if (!RigidGlbTextureBundleReader.IsSerializedTextureSizeRepresentable(
                imported.Width, imported.Height) ||
            imported.Width is < 1 or > RigidGlbTextureBundleReader.AbsoluteMaximumTextureDimension ||
            imported.Height is < 1 or > RigidGlbTextureBundleReader.AbsoluteMaximumTextureDimension)
        {
            throw new InvalidDataException(
                $"Texture {imported.Name} has unsupported dimensions " +
                $"{imported.Width}x{imported.Height}.");
        }
        using Image<Rgba32> image = Image.Load<Rgba32>(imported.Data);
        if (image.Width != imported.Width || image.Height != imported.Height)
            throw new InvalidDataException(
                $"Texture {imported.Name} dimensions do not match its image payload.");
        byte[] pixels = EncodeBgra(image);

        ReadOnlySpan<byte> source = ObjectBytes(document, templateEntry);
        int oldPixelSize = checked(template.Width * template.Height * 4);
        int oldPixelEnd = checked(SerializedTexturePixelOffset + oldPixelSize);
        if (oldPixelEnd > source.Length ||
            source[SerializedTextureMarkerOffset] != 0)
            throw new InvalidDataException("Texture template has an unsupported layout.");
        byte[] result = new byte[checked(source.Length - oldPixelSize + pixels.Length)];
        source[..SerializedTexturePixelOffset].CopyTo(result);
        pixels.CopyTo(result.AsSpan(SerializedTexturePixelOffset));
        source[oldPixelEnd..].CopyTo(result.AsSpan(
            SerializedTexturePixelOffset + pixels.Length));
        int delta = pixels.Length - oldPixelSize;
        AddUInt32(result, 0x09, delta);
        AddUInt32(result, 0x1A, delta);
        AddUInt32(result, 0x1F, delta);
        WriteUInt32(result, 0x24, checked((uint)image.Width));
        WriteUInt32(result, 0x28, checked((uint)image.Height));
        WriteUInt32(result, 0x2C, 0);
        WriteUInt32(result, 0x30, checked(((uint)image.Width << 8) | 1));
        WriteUInt32(result, 0x34, checked((uint)image.Width << 10));
        WriteUInt32(result, 0x38, checked((uint)image.Height << 8));
        return result;
    }

    private static byte[] InjectInlineObject(
        SmoDocument document,
        uint ownerId,
        byte fieldType,
        uint objectId,
        string name,
        uint typeHash,
        byte[] objectData)
    {
        byte[] field = new byte[checked(5 + ObjectReferenceSize + objectData.Length)];
        field[0] = checked((byte)(0xE0 | fieldType));
        WriteUInt32(field, 1, checked((uint)(ObjectReferenceSize + objectData.Length)));
        WriteUInt32(field, 5, objectId);
        WriteUInt32(field, 9, checked((uint)objectData.Length));
        objectData.CopyTo(field, 13);
        var attachment = new SmoVisualForestAttachment(
            ownerId,
            field,
            [new SmoVisualForestEntry(
                objectId,
                Encoding.UTF8.GetBytes(name + '\0'),
                typeHash,
                13,
                checked((uint)objectData.Length))]);
        return SmoVisualForestInjector.Inject(document, ownerId, [attachment]);
    }

    private static byte[] EncodeBgra(Image<Rgba32> image)
    {
        byte[] pixels = new byte[checked(image.Width * image.Height * 4)];
        int pixelOffset = 0;
        image.ProcessPixelRows(accessor =>
        {
            for (int y = 0; y < image.Height; y++)
            foreach (Rgba32 pixel in accessor.GetRowSpan(y))
            {
                pixels[pixelOffset++] = pixel.B;
                pixels[pixelOffset++] = pixel.G;
                pixels[pixelOffset++] = pixel.R;
                pixels[pixelOffset++] = pixel.A;
            }
        });
        return pixels;
    }

    private static byte[] InjectReference(
        SmoDocument document,
        uint ownerId,
        byte fieldType,
        uint referenceId)
    {
        byte[] field = new byte[13];
        field[0] = checked((byte)(0xE0 | fieldType));
        WriteUInt32(field, 1, ObjectReferenceSize);
        WriteUInt32(field, 5, referenceId);
        WriteUInt32(field, 9, 0);
        return SmoVisualForestInjector.Inject(
            document,
            ownerId,
            [new SmoVisualForestAttachment(ownerId, field, [])]);
    }

    private static uint[] ResolveTextureIds(
        SmoDocument document,
        SmoObjectEntry material)
    {
        HashSet<uint> textureIds = document.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.TextureData &&
                entry.LogicalOffset >= material.LogicalOffset &&
                entry.LogicalEnd <= material.LogicalEnd)
            .Select(entry => entry.Id)
            .ToHashSet();
        foreach (SmoDataBlockHeader field in Fields(document, material))
        {
            if (field.FieldType != 10 || field.PayloadSize < 8)
                continue;
            uint id = BinaryPrimitives.ReadUInt32LittleEndian(
                ObjectBytes(document, material)[field.PayloadOffset..]);
            if (document.Objects.Any(entry =>
                    entry.Id == id && entry.TypeHash == SmoClassIds.TextureData))
                textureIds.Add(id);
        }
        return textureIds.ToArray();
    }

    private static bool HasFieldReference(
        SmoDocument document,
        SmoObjectEntry owner,
        byte fieldType,
        uint referenceId) => Fields(document, owner).Any(field =>
            field.FieldType == fieldType &&
            field.PayloadSize >= 4 &&
            BinaryPrimitives.ReadUInt32LittleEndian(
                ObjectBytes(document, owner)[field.PayloadOffset..]) == referenceId);

    private static bool HasReferenceOnly(
        SmoDocument document,
        SmoObjectEntry owner,
        byte fieldType,
        uint referenceId)
    {
        ReadOnlySpan<byte> bytes = ObjectBytes(document, owner);
        foreach (SmoDataBlockHeader field in Fields(document, owner))
        {
            if (field.FieldType == fieldType && field.PayloadSize == 8 &&
                BinaryPrimitives.ReadUInt32LittleEndian(bytes[field.PayloadOffset..]) ==
                    referenceId &&
                BinaryPrimitives.ReadUInt32LittleEndian(
                    bytes[(field.PayloadOffset + 4)..]) == 0)
                return true;
        }
        return false;
    }

    private static void PatchReferenceId(
        SmoDocument document,
        byte[] data,
        uint ownerId,
        byte fieldType,
        uint oldReferenceId,
        uint newReferenceId)
    {
        SmoObjectEntry owner = document.Objects.Single(entry => entry.Id == ownerId);
        ReadOnlySpan<byte> bytes = ObjectBytes(document, owner);
        foreach (SmoDataBlockHeader field in Fields(document, owner))
        {
            if (field.FieldType != fieldType || field.PayloadSize != 8 ||
                BinaryPrimitives.ReadUInt32LittleEndian(bytes[field.PayloadOffset..]) != oldReferenceId ||
                BinaryPrimitives.ReadUInt32LittleEndian(bytes[(field.PayloadOffset + 4)..]) != 0)
                continue;
            WriteUInt32(data, checked((int)owner.PhysicalOffset + field.PayloadOffset), newReferenceId);
        }
    }

    private static IReadOnlyList<SmoDataBlockHeader> Fields(
        SmoDocument document,
        SmoObjectEntry owner)
    {
        ReadOnlySpan<byte> bytes = ObjectBytes(document, owner);
        var result = new List<SmoDataBlockHeader>();
        int offset = 8;
        while (offset < bytes.Length &&
               SmoDataBlockReader.TryReadHeader(bytes, offset, out SmoDataBlockHeader field))
        {
            result.Add(field);
            offset = checked((int)field.PayloadEnd);
        }
        return result;
    }

    private static SmoObjectEntry? FindAncestor(
        IReadOnlyList<SmoObjectEntry> objects,
        SmoObjectEntry entry,
        uint typeHash)
    {
        int? cursor = entry.ParentIndex;
        while (cursor is int index && (uint)index < (uint)objects.Count)
        {
            SmoObjectEntry candidate = objects[index];
            if (candidate.TypeHash == typeHash)
                return candidate;
            cursor = candidate.ParentIndex;
        }
        return null;
    }

    private static void VerifyOldResourcesUnchanged(
        SmoDocument before,
        SmoDocument after,
        IEnumerable<uint> ids)
    {
        foreach (uint id in ids.Distinct())
        {
            SmoObjectEntry oldEntry = before.Objects.Single(entry => entry.Id == id);
            SmoObjectEntry newEntry = after.Objects.Single(entry => entry.Id == id);
            if (oldEntry.TypeHash != newEntry.TypeHash ||
                oldEntry.SerializedSize != newEntry.SerializedSize ||
                !ObjectBytes(before, oldEntry).SequenceEqual(ObjectBytes(after, newEntry)))
            {
                throw new InvalidDataException(
                    $"Preserved resource ID {id} changed during model replacement.");
            }
        }
    }

    private static ReadOnlySpan<byte> ObjectBytes(
        SmoDocument document,
        SmoObjectEntry entry) => document.Data.Span.Slice(
            checked((int)entry.PhysicalOffset), checked((int)entry.SerializedSize));

    private static void AddUInt32(Span<byte> data, int offset, int delta)
    {
        long value = checked((long)BinaryPrimitives.ReadUInt32LittleEndian(data[offset..]) + delta);
        WriteUInt32(data, offset, checked((uint)value));
    }

    private static void WriteUInt32(Span<byte> data, int offset, uint value) =>
        BinaryPrimitives.WriteUInt32LittleEndian(data[offset..], value);
}
