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

public sealed record SmoLevelModelGraphWriteOptions(
    bool RequireReferenceOnlyPlacements = true,
    bool ExpandSingleComponentTemplate = false,
    bool RequireUnskinnedModel = false,
    bool PrepareRigidImport = false)
{
    public static SmoLevelModelGraphWriteOptions StandaloneStatic { get; } = new(
        RequireReferenceOnlyPlacements: false,
        ExpandSingleComponentTemplate: true,
        RequireUnskinnedModel: true,
        PrepareRigidImport: true);
}

public sealed record SmoLevelModelGraphReplacementAnalysis(
    bool CanReplace,
    int TargetMeshCount,
    int ImportedMeshCount,
    int ImportedTextureCount,
    int VertexCount,
    int TriangleCount,
    IReadOnlyList<string> Messages);

public sealed record SmoLevelModelGraphFileResult(
    string OutputPath,
    int MeshCount,
    int TextureCount,
    int VertexCount,
    int TriangleCount,
    long FileSize,
    string Sha256,
    string? BackupPath);

/// <summary>
/// Replaces a complete rigid model graph with new mesh/texture resources. The
/// default level profile redirects the selected authoring model plus every
/// reference-only placement. The standalone static profile reuses existing
/// inline model branches or expands one native branch to the donor part count.
/// Replaced leaf resources are omitted once no reference uses them; shared donor
/// textures are serialized once and referenced by every matching consumer.
/// </summary>
public static class SmoLevelModelGraphReplacer
{
    private const int ObjectReferenceSize = 8;
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
        SmoLoadedResources loaded = RequireLoadedResources(document);

        // Components are physical byte carriers, one per inline mesh resource.
        // Additional Model consumers remain placements of that same resource;
        // their actual Mesh/Material links never follow physical ownership.
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
                SmoLoadedModel linked = RequireLoadedModel(loaded, owner);
                if (linked.MeshId != pair.Mesh.Id ||
                    linked.MeshObjectIndex is not int meshIndex || meshIndex != pair.Mesh.Index)
                    throw new InvalidOperationException(
                        $"Inline mesh {pair.Mesh.Id} is not the active mesh of its " +
                        $"physical spModel owner {owner.Id}; this byte carrier cannot be replaced.");
                return new SmoLevelModelGraphComponent(
                    owner.Id,
                    linked.MaterialId == 0 ? null : linked.MaterialId,
                    linked.MeshId,
                    meshIndex);
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

    public static SmoLevelModelGraphReplacementAnalysis Analyze(
        SmoDocument document,
        int selectedMeshObjectIndex,
        ImportedScene replacement,
        SmoLevelModelGraphWriteOptions? options = null)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(replacement);
        options ??= new SmoLevelModelGraphWriteOptions();
        try
        {
            PreparedGraphReplacement prepared = PrepareGraphReplacement(
                document,
                selectedMeshObjectIndex,
                replacement,
                options);
            ValidatePreparedGraphReplacement(prepared, options);
            var messages = new List<string>
            {
                $"Ready: {prepared.Replacement.Meshes.Count} model-graph part(s), " +
                $"{prepared.Replacement.Meshes.Sum(mesh => mesh.Positions.Length):N0} " +
                "vertices and " +
                $"{prepared.Replacement.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3):N0} " +
                "triangles.",
                "The shared model-graph writer will replace mesh/material/texture " +
                "resources and verify their native ownership and references."
            };
            messages.Add(prepared.ExpandedSingleTemplate
                ? "One native model branch will be expanded to the donor part count " +
                  "before the shared replacement pass."
                : "Existing native model branches will be replaced one-to-one.");
            if (options.RequireUnskinnedModel)
            {
                messages.Add(
                    "The selected graph and donor are unskinned; no spSkin or bone " +
                    "palette data will be created.");
            }
            return CreateAnalysis(prepared, true, messages);
        }
        catch (Exception exception) when (exception is InvalidDataException or
                                          InvalidOperationException or
                                          NotSupportedException or
                                          OverflowException or
                                          ArgumentException)
        {
            return new SmoLevelModelGraphReplacementAnalysis(
                false,
                CountSelectedGraphMeshes(document, selectedMeshObjectIndex),
                replacement.Meshes.Count,
                CountImportedTextures(replacement),
                replacement.Meshes.Sum(mesh => mesh.Positions.Length),
                replacement.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3),
                [exception.Message]);
        }
    }

    public static SmoLevelModelGraphFileResult ReplaceFile(
        SmoDocument document,
        int selectedMeshObjectIndex,
        ImportedScene replacement,
        ReplacementTransform transform,
        string outputPath,
        SmoLevelModelGraphWriteOptions? options = null,
        CancellationToken cancellationToken = default,
        params string?[] protectedInputPaths)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(outputPath);
        options ??= new SmoLevelModelGraphWriteOptions();
        SmoLevelModelGraphReplacementResult result = Replace(
            document,
            selectedMeshObjectIndex,
            replacement,
            transform,
            options: options,
            cancellationToken: cancellationToken);
        string?[] protectedPaths = [document.SourcePath, .. protectedInputPaths];
        SmoVerifiedOutputInstallResult installed =
            SmoVerifiedOutputInstaller.Install(
                outputPath,
                result.Data,
                temporary => VerifyInstalledGraph(
                    SmoDocument.Load(temporary),
                    result,
                    options),
                cancellationToken,
                protectedPaths);
        return new SmoLevelModelGraphFileResult(
            installed.OutputPath,
            result.MeshObjectIds.Count,
            result.ImportedTextureObjectIds.Count,
            result.VertexCount,
            result.TriangleCount,
            result.Data.LongLength,
            installed.Sha256,
            installed.BackupPath);
    }

    public static SmoLevelModelGraphReplacementResult Replace(
        SmoDocument document,
        int selectedMeshObjectIndex,
        ImportedScene replacement,
        ReplacementTransform transform,
        Matrix4x4? referenceWorldTransform = null,
        IReadOnlyDictionary<int, uint>? reusableImportedTextureObjectIds = null,
        SmoLevelModelGraphWriteOptions? options = null,
        CancellationToken cancellationToken = default)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(replacement);
        ArgumentNullException.ThrowIfNull(transform);
        cancellationToken.ThrowIfCancellationRequested();
        options ??= new SmoLevelModelGraphWriteOptions();
        PreparedGraphReplacement prepared = PrepareGraphReplacement(
            document,
            selectedMeshObjectIndex,
            replacement,
            options);
        ValidatePreparedGraphReplacement(
            prepared,
            options,
            reusableImportedTextureObjectIds);
        document = prepared.Document;
        replacement = prepared.Replacement;
        SmoLevelModelGraphPlan plan = prepared.Plan;
        var reusableTextureIds = reusableImportedTextureObjectIds is null
            ? new Dictionary<int, uint>()
            : new Dictionary<int, uint>(reusableImportedTextureObjectIds);
        foreach ((int textureIndex, uint objectId) in reusableTextureIds)
        {
            cancellationToken.ThrowIfCancellationRequested();
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
            SmoObjectEntry? textureTemplate = options.RequireUnskinnedModel
                ? null
                : FindTextureTemplate(document, oldTextureIdsByMaterial);
            foreach (int textureIndex in texturesToInject)
            {
                serializedImportedTextures.Add(
                    textureIndex,
                    textureTemplate is null
                        ? BuildCanonicalTextureObject(
                            replacement.Textures[textureIndex])
                        : BuildTextureObject(
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
                    .Select(modelId => FindPlacementMaterialId(
                        document, modelId, component.MeshObjectId))
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
            cancellationToken.ThrowIfCancellationRequested();
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
            cancellationToken.ThrowIfCancellationRequested();
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
            // The original resource reader resolves references in stream
            // order. Expand the existing first reference in both static and
            // level imports; appending an inline copy after it creates a
            // forward reference that a catalog-only parser would accept.
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
            cancellationToken.ThrowIfCancellationRequested();
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
            cancellationToken.ThrowIfCancellationRequested();
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

        cancellationToken.ThrowIfCancellationRequested();
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
            verified.Objects.Count - prepared.OriginalObjectCount,
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

    private static PreparedGraphReplacement PrepareGraphReplacement(
        SmoDocument document,
        int selectedMeshObjectIndex,
        ImportedScene replacement,
        SmoLevelModelGraphWriteOptions options)
    {
        if (document.HasErrors)
            throw new InvalidDataException("Target SMO has parser errors.");
        if (options.RequireUnskinnedModel)
            SmoProductionPlatformGuard.EnsurePcWritable(document);
        if (replacement.Meshes.Count == 0)
            throw new InvalidDataException("The imported model contains no meshes.");

        replacement = options.PrepareRigidImport
            ? SmoLevelRigidImportPreparer.Prepare(replacement)
            : SmoLevelEmbeddedTextureBudget.Prepare(replacement);
        if (replacement.Meshes.Count == 0)
            throw new InvalidDataException(
                "Model preparation produced no writable mesh parts.");

        int originalObjectCount = document.Objects.Count;
        int originalTargetMeshCount = document.Objects.Count(entry =>
            entry.TypeHash == SmoClassIds.MeshData);
        SmoLevelModelGraphPlan plan;
        if (options.RequireUnskinnedModel)
        {
            StandaloneTemplateNormalization normalized =
                NormalizeStandaloneVisualForest(
                    document,
                    selectedMeshObjectIndex,
                    replacement);
            document = normalized.Document;
            plan = normalized.Plan;
            selectedMeshObjectIndex = plan.Components[0].MeshObjectIndex;
        }
        else
        {
            plan = ResolveGraphPlan(
                document,
                selectedMeshObjectIndex,
                options);
            originalTargetMeshCount = plan.Components.Count;
        }
        uint selectedMeshId = plan.Components[0].MeshObjectId;
        bool expanded = false;
        if (replacement.Meshes.Count != plan.Components.Count &&
            options.ExpandSingleComponentTemplate &&
            plan.Components.Count == 1 &&
            replacement.Meshes.Count > 1)
        {
            document = ExpandSingleComponentTemplate(
                document,
                plan,
                replacement.Meshes.Count);
            selectedMeshObjectIndex = document.Objects.Single(entry =>
                entry.Id == selectedMeshId).Index;
            plan = ResolveGraphPlan(document, selectedMeshObjectIndex, options);
            expanded = true;
        }
        if (replacement.Meshes.Count != plan.Components.Count)
        {
            throw new InvalidOperationException(
                $"The complete SMO model contains {plan.Components.Count} mesh " +
                $"parts, but the imported model contains " +
                $"{replacement.Meshes.Count}. Replacement was cancelled without " +
                "changing the source file.");
        }
        return new PreparedGraphReplacement(
            document,
            plan,
            replacement,
            originalObjectCount,
            originalTargetMeshCount,
            expanded);
    }

    private static StandaloneTemplateNormalization NormalizeStandaloneVisualForest(
        SmoDocument document,
        int selectedMeshObjectIndex,
        ImportedScene replacement)
    {
        bool requireMaterial = replacement.Meshes.Any(mesh =>
            ResolveImportedTextureIndex(replacement, mesh) >= 0);
        SmoObjectEntry[] candidates = document.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.MeshData)
            .OrderBy(entry => entry.Index == selectedMeshObjectIndex ? 0 : 1)
            .ThenBy(entry => entry.LogicalOffset)
            .ToArray();
        SmoLevelModelGraphPlan? templatePlan = null;
        SmoLevelModelGraphComponent? templateComponent = null;
        foreach (SmoObjectEntry candidate in candidates)
        {
            try
            {
                SmoLevelModelGraphPlan candidatePlan = ResolvePlan(
                    document,
                    candidate.Index);
                SmoLevelModelGraphComponent component = candidatePlan.Components
                    .Single(item => item.MeshObjectId == candidate.Id);
                if (candidatePlan.Components.Count(item =>
                        item.ModelObjectId == component.ModelObjectId) != 1 ||
                    requireMaterial && component.MaterialObjectId is null)
                {
                    continue;
                }
                SmoObjectEntry root = document.Objects.Single(entry =>
                    entry.Id == candidatePlan.RootObjectId);
                SmoObjectEntry model = document.Objects.Single(entry =>
                    entry.Id == component.ModelObjectId);
                if (root.TypeHash != SmoClassIds.RenderNode ||
                    model.ParentIndex != root.Index)
                {
                    continue;
                }
                SmoMesh mesh = SmoMeshDecoder.Decode(document, candidate);
                SmoVertexLayout layout =
                    SmoMeshResourceReplacer.ValidateWritableLayout(candidate, mesh);
                if (layout.BlendWeightsOffset.HasValue ||
                    layout.BlendIndicesOffset.HasValue)
                {
                    continue;
                }
                _ = ExtractInlineBranch(document, root, model);
                templatePlan = new SmoLevelModelGraphPlan(
                    candidatePlan.RootObjectId,
                    [component]);
                templateComponent = component;
                break;
            }
            catch (Exception exception) when (exception is InvalidDataException or
                                              InvalidOperationException or
                                              NotSupportedException or
                                              SmoFormatException)
            {
                // Try another native rigid model branch. The standalone path
                // needs one safe branch only; every other visual branch is
                // removed before the shared graph writer expands this template.
            }
        }
        if (templatePlan is null || templateComponent is null)
        {
            throw new NotSupportedException(
                "The target SMO has no standalone rigid model branch that can " +
                "serve as the universal import template.");
        }

        uint templateModelId = templateComponent.ModelObjectId;
        uint templateMeshId = templateComponent.MeshObjectId;
        uint[] otherModelIds = document.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.MeshData &&
                            entry.Id != templateMeshId)
            .Select(entry => FindAncestor(
                document.Objects,
                entry,
                SmoClassIds.Model)?.Id)
            .Where(id => id.HasValue && id.Value != templateModelId)
            .Select(id => id!.Value)
            .Distinct()
            .ToArray();
        byte[] output = document.Data.ToArray();
        foreach (uint modelId in otherModelIds)
        {
            SmoDocument current = SmoDocument.ParseOwned(
                output,
                document.SourcePath);
            SmoObjectEntry? model = current.Objects.FirstOrDefault(entry =>
                entry.Id == modelId && entry.TypeHash == SmoClassIds.Model);
            if (model is null)
                continue;
            if (model.ParentIndex is not int ownerIndex)
                throw new InvalidDataException(
                    $"Standalone model branch {modelId} has no inline owner.");
            uint ownerId = current.Objects[ownerIndex].Id;
            output = SmoVisualForestInjector.RemoveInlineBranch(
                current,
                ownerId,
                modelId);
            SmoLargeContainerMemory.ReleaseIntermediates(output.Length);
        }

        SmoDocument normalized = SmoDocument.ParseOwned(output, document.SourcePath);
        int templateIndex = normalized.Objects.Single(entry =>
            entry.Id == templateMeshId).Index;
        SmoLevelModelGraphPlan normalizedPlan = ResolvePlan(
            normalized,
            templateIndex);
        SmoLevelModelGraphComponent[] retained = normalizedPlan.Components
            .Where(component => component.ModelObjectId == templateModelId &&
                                component.MeshObjectId == templateMeshId)
            .ToArray();
        if (retained.Length != 1 ||
            normalized.Objects.Count(entry =>
                entry.TypeHash == SmoClassIds.MeshData) != 1)
        {
            throw new InvalidDataException(
                "Standalone visual normalization did not leave exactly one " +
                "native rigid template mesh.");
        }
        return new StandaloneTemplateNormalization(
            normalized,
            new SmoLevelModelGraphPlan(normalizedPlan.RootObjectId, retained));
    }

    private static SmoLevelModelGraphPlan ResolveGraphPlan(
        SmoDocument document,
        int selectedMeshObjectIndex,
        SmoLevelModelGraphWriteOptions options) =>
        options.RequireReferenceOnlyPlacements
            ? ResolveWritablePlan(document, selectedMeshObjectIndex)
            : ResolvePlan(document, selectedMeshObjectIndex);

    private static void ValidatePreparedGraphReplacement(
        PreparedGraphReplacement prepared,
        SmoLevelModelGraphWriteOptions options,
        IReadOnlyDictionary<int, uint>? reusableImportedTextureObjectIds = null)
    {
        SmoDocument document = prepared.Document;
        ImportedScene replacement = prepared.Replacement;
        if (options.RequireUnskinnedModel &&
            document.Objects.Any(entry => entry.TypeHash == SmoClassIds.Skin))
        {
            throw new InvalidOperationException(
                "Standalone static replacement requires a target without spSkin " +
                "objects.");
        }
        if (options.RequireUnskinnedModel &&
            replacement.Meshes.Any(mesh => mesh.Skinning is not null))
        {
            throw new InvalidOperationException(
                "Standalone static replacement accepts only donor meshes without " +
                "skin weights.");
        }

        for (int index = 0; index < replacement.Meshes.Count; index++)
        {
            ImportedMesh imported = replacement.Meshes[index];
            if (imported.Positions.Length == 0 ||
                imported.TriangleIndices.Length == 0)
            {
                throw new InvalidDataException(
                    $"Imported mesh [{index}] '{imported.Name}' is empty.");
            }
            if (imported.MaterialIndex < -1 ||
                imported.MaterialIndex >= replacement.Materials.Count)
            {
                throw new InvalidDataException(
                    $"Imported mesh [{index}] references missing material " +
                    $"{imported.MaterialIndex}.");
            }
            SmoLevelModelGraphComponent component = prepared.Plan.Components[index];
            SmoObjectEntry targetMesh = document.Objects.Single(entry =>
                entry.Id == component.MeshObjectId);
            SmoMesh decoded = SmoMeshDecoder.Decode(document, targetMesh);
            SmoVertexLayout layout = SmoMeshResourceReplacer.ValidateWritableLayout(
                targetMesh,
                decoded);
            if (options.RequireUnskinnedModel &&
                (layout.BlendWeightsOffset.HasValue ||
                 layout.BlendIndicesOffset.HasValue))
            {
                throw new InvalidOperationException(
                    $"Target mesh [{targetMesh.Index}] uses a skinned layout; " +
                    "the static graph path never writes bone data.");
            }
            if (ResolveImportedTextureIndex(replacement, imported) >= 0 &&
                component.MaterialObjectId is null)
            {
                throw new InvalidOperationException(
                    $"Model component {component.ModelObjectId} has no native " +
                    "material that can own its imported texture.");
            }
        }

        int[] importedTextureIndices = replacement.Meshes
            .Select(mesh => ResolveImportedTextureIndex(replacement, mesh))
            .Where(index => index >= 0)
            .Distinct()
            .Where(index =>
                reusableImportedTextureObjectIds?.ContainsKey(index) != true)
            .ToArray();
        if (importedTextureIndices.Length > 0)
        {
            Dictionary<uint, uint[]> oldTextureIdsByMaterial = prepared.Plan.Components
                .Where(component => component.MaterialObjectId is not null)
                .GroupBy(component => component.MaterialObjectId!.Value)
                .ToDictionary(
                    group => group.Key,
                    group => ResolveTextureIds(
                        document,
                        document.Objects.Single(entry => entry.Id == group.Key)));
            SmoObjectEntry? textureTemplate = options.RequireUnskinnedModel
                ? null
                : FindTextureTemplate(document, oldTextureIdsByMaterial);
            foreach (int textureIndex in importedTextureIndices)
            {
                _ = textureTemplate is null
                    ? BuildCanonicalTextureObject(
                        replacement.Textures[textureIndex])
                    : BuildTextureObject(
                        document,
                        textureTemplate,
                        replacement.Textures[textureIndex]);
            }
        }

        uint[] alphaMaterialIds = prepared.Plan.Components
            .SelectMany((component, index) =>
                RequiresTextureAlpha(replacement, replacement.Meshes[index]) &&
                component.MaterialObjectId is uint materialId
                    ? [materialId]
                    : Array.Empty<uint>())
            .Distinct()
            .ToArray();
        if (alphaMaterialIds.Length > 0)
            _ = PatchRigidTextureAlphaMaterials(document, alphaMaterialIds);
    }

    private static SmoDocument ExpandSingleComponentTemplate(
        SmoDocument document,
        SmoLevelModelGraphPlan plan,
        int requiredPartCount)
    {
        if (plan.Components.Count != 1 || requiredPartCount <= 1)
            return document;
        SmoObjectEntry root = document.Objects.Single(entry =>
            entry.Id == plan.RootObjectId);
        SmoLevelModelGraphComponent component = plan.Components[0];
        SmoObjectEntry model = document.Objects.Single(entry =>
            entry.Id == component.ModelObjectId);
        if (root.TypeHash != SmoClassIds.RenderNode ||
            model.ParentIndex != root.Index)
        {
            throw new NotSupportedException(
                "Expanding a single model template requires a direct " +
                "spRenderNode -> spModel branch.");
        }
        SmoAdditiveForestPlan template = ExtractInlineBranch(
            document,
            root,
            model);
        uint nextId = document.Objects.Max(entry => entry.Id);
        var attachments = new List<SmoVisualForestAttachment>(requiredPartCount - 1);
        for (int index = 1; index < requiredPartCount; index++)
        {
            var idMap = new Dictionary<uint, uint>(
                template.GeneratedObjectIds.Count);
            foreach (uint id in template.GeneratedObjectIds)
                idMap.Add(id, checked(++nextId));
            SmoAdditiveForestPlan clone =
                SmoAdditiveForestPlanner.RemapObjectIds(template, idMap);
            if (clone.Operations.Count != 1)
            {
                throw new InvalidDataException(
                    "The native model template did not produce one inline branch.");
            }
            attachments.Add(clone.Operations[0].Attachment);
        }
        return SmoDocument.ParseOwned(
            SmoVisualForestInjector.Inject(document, root.Id, attachments),
            document.SourcePath);
    }

    private static SmoAdditiveForestPlan ExtractInlineBranch(
        SmoDocument document,
        SmoObjectEntry owner,
        SmoObjectEntry root)
    {
        SmoDataBlockHeader field = FindInlineField(document, owner, root);
        int fieldPhysical = checked((int)owner.PhysicalOffset + field.Offset);
        int fieldLength = checked(field.HeaderSize + (int)field.PayloadSize);
        byte[] fieldData = document.Data.Span
            .Slice(fieldPhysical, fieldLength)
            .ToArray();
        SmoObjectEntry[] entries = document.Objects.Where(entry =>
                entry.PhysicalOffset >= root.PhysicalOffset &&
                entry.PhysicalEnd <= root.PhysicalEnd)
            .OrderBy(entry => entry.PhysicalOffset)
            .ThenByDescending(entry => entry.SerializedSize)
            .ToArray();
        if (entries.Length == 0 || entries[0].Id != root.Id)
            throw new InvalidDataException("The native model template is incomplete.");
        var attachment = new SmoVisualForestAttachment(
            owner.Id,
            fieldData,
            entries.Select(entry => new SmoVisualForestEntry(
                entry.Id,
                entry.RawName.ToArray(),
                entry.TypeHash,
                checked((int)entry.PhysicalOffset - fieldPhysical),
                entry.SerializedSize)).ToArray());
        SmoLoadedResources loaded = SmoLoadedResources.Get(document);
        SmoFileReferenceTrace trace = loaded.ReferenceTrace ??
            throw new InvalidDataException(loaded.ReferenceTraceIssue ?? loaded.LoadIssue ??
                "The native model template has no actual-reader reference provenance.");
        if (loaded.ReferenceTraceIssue is not null)
            throw new InvalidDataException(loaded.ReferenceTraceIssue);
        return new SmoAdditiveForestPlan(
            [new SmoVisualForestOperation(
                attachment,
                SmoVisualForestInsertionKind.BeforeTerminal)],
            entries.Select(entry => entry.Id).ToArray())
        { ReferenceRanges = [trace.CaptureRange(document, fieldPhysical, fieldLength)] };
    }

    private static SmoDataBlockHeader FindInlineField(
        SmoDocument document,
        SmoObjectEntry owner,
        SmoObjectEntry child)
    {
        ReadOnlySpan<byte> bytes = ObjectBytes(document, owner);
        foreach (SmoDataBlockHeader field in Fields(document, owner))
        {
            long payloadPhysical = owner.PhysicalOffset + field.PayloadOffset;
            if (field.PayloadSize == child.SerializedSize + ObjectReferenceSize &&
                payloadPhysical == child.PhysicalOffset - ObjectReferenceSize &&
                BinaryPrimitives.ReadUInt32LittleEndian(
                    bytes[field.PayloadOffset..]) == child.Id &&
                BinaryPrimitives.ReadUInt32LittleEndian(
                    bytes[(field.PayloadOffset + sizeof(uint))..]) ==
                    child.SerializedSize)
            {
                return field;
            }
        }
        throw new InvalidDataException(
            $"Object {child.Id} is not an inline child of {owner.Id}.");
    }

    private static SmoLevelModelGraphReplacementAnalysis CreateAnalysis(
        PreparedGraphReplacement prepared,
        bool canReplace,
        IReadOnlyList<string> messages) => new(
            canReplace,
            prepared.OriginalTargetMeshCount,
            prepared.Replacement.Meshes.Count,
            CountImportedTextures(prepared.Replacement),
            prepared.Replacement.Meshes.Sum(mesh => mesh.Positions.Length),
            prepared.Replacement.Meshes.Sum(mesh =>
                mesh.TriangleIndices.Length / 3),
            messages);

    private static int CountSelectedGraphMeshes(
        SmoDocument document,
        int selectedMeshObjectIndex)
    {
        try
        {
            return ResolvePlan(document, selectedMeshObjectIndex).Components.Count;
        }
        catch
        {
            return document.Objects.Count(entry =>
                entry.TypeHash == SmoClassIds.MeshData);
        }
    }

    private static int CountImportedTextures(ImportedScene scene) =>
        scene.Meshes
            .Select(mesh => ResolveImportedTextureIndex(scene, mesh))
            .Where(index => index >= 0)
            .Distinct()
            .Count();

    private static void VerifyInstalledGraph(
        SmoDocument document,
        SmoLevelModelGraphReplacementResult result,
        SmoLevelModelGraphWriteOptions options)
    {
        if (document.HasErrors)
            throw new InvalidDataException(
                "Installed model graph has parser errors.");
        SmoMesh[] meshes = result.MeshObjectIds.Values
            .Select(id => document.Objects.Single(entry =>
                entry.Id == id && entry.TypeHash == SmoClassIds.MeshData))
            .Select(entry => SmoMeshDecoder.Decode(document, entry))
            .ToArray();
        if (meshes.Sum(mesh => mesh.VertexCount) != result.VertexCount ||
            meshes.Sum(mesh => mesh.TriangleCount) != result.TriangleCount)
        {
            throw new InvalidDataException(
                "Installed model graph geometry counts changed.");
        }
        if (options.RequireUnskinnedModel &&
            (document.Objects.Any(entry => entry.TypeHash == SmoClassIds.Skin) ||
             meshes.Any(mesh => mesh.HasSkinningData)))
        {
            throw new InvalidDataException(
                "Installed static model graph contains skinning data.");
        }
        foreach (uint textureId in result.ImportedTextureObjectIds.Values.Distinct())
        {
            SmoObjectEntry textureEntry = document.Objects.Single(entry =>
                entry.Id == textureId &&
                entry.TypeHash == SmoClassIds.TextureData);
            if (!SmoTextureDecoder.TryDecode(
                    document,
                    textureEntry,
                    out SmoTexture? texture,
                    out string error) ||
                texture is null)
            {
                throw new InvalidDataException(
                    $"Installed texture {textureId} is invalid: {error}");
            }
        }
        VerifyTextureReferenceOrder(document, result.ImportedTextureObjectIds.Values);
    }

    internal static void VerifyTextureReferenceOrder(SmoDocument document, IEnumerable<uint> textureIds)
    {
        HashSet<uint> selected = textureIds.ToHashSet();
        Dictionary<uint, long> starts = document.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.TextureData && selected.Contains(entry.Id))
            .ToDictionary(entry => entry.Id, entry => entry.PhysicalOffset);
        foreach (SmoObjectEntry material in document.Objects.Where(entry => entry.TypeHash == SmoClassIds.MaterialData))
        {
            foreach (SmoObjectField field in SmoObjectFieldReader.Read(document, material))
            {
                if (field.FieldType != 10 || field.PayloadSize < 8) continue;
                uint id = BinaryPrimitives.ReadUInt32LittleEndian(field.Payload.Span);
                uint inlineSize = BinaryPrimitives.ReadUInt32LittleEndian(field.Payload.Span[4..]);
                if (inlineSize == 0 && starts.TryGetValue(id, out long start) && field.AbsolutePayloadOffset < start)
                    throw new InvalidDataException(
                        $"Material [{material.Index}] references imported texture {id} before its inline definition.");
            }
        }
    }

    private sealed record PreparedGraphReplacement(
        SmoDocument Document,
        SmoLevelModelGraphPlan Plan,
        ImportedScene Replacement,
        int OriginalObjectCount,
        int OriginalTargetMeshCount,
        bool ExpandedSingleTemplate);

    private sealed record StandaloneTemplateNormalization(
        SmoDocument Document,
        SmoLevelModelGraphPlan Plan);

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

    internal static byte[] PatchRigidTextureAlphaMaterials(
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

    private static SmoLoadedResources RequireLoadedResources(SmoDocument document)
    {
        SmoLoadedResources loaded = SmoLoadedResources.Get(document);
        if (loaded.LoadIssue is not null)
            throw new InvalidDataException(loaded.LoadIssue);
        return loaded;
    }

    private static SmoLoadedModel RequireLoadedModel(
        SmoLoadedResources loaded,
        SmoObjectEntry model) => loaded.Models.TryGetValue(model.Index, out SmoLoadedModel? linked)
            ? linked
            : throw new InvalidDataException(
                $"SPARKPLUG_AUTHORING_GRAPH: spModel {model.Id} is absent from the loaded graph.");

    private static uint? FindPlacementMaterialId(
        SmoDocument document,
        uint modelId,
        uint meshId)
    {
        SmoObjectEntry model = document.Objects.Single(entry => entry.Id == modelId);
        SmoLoadedModel linked = RequireLoadedModel(RequireLoadedResources(document), model);
        // Earlier serialized references still require byte relocation, even if
        // a later assignment changes the Model's active mesh. Only an actual
        // consumer's material participates in the imported material edit.
        return linked.MeshId == meshId && linked.MaterialId != 0
            ? linked.MaterialId
            : null;
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

    private static SmoObjectEntry? FindTextureTemplate(
        SmoDocument document,
        IReadOnlyDictionary<uint, uint[]> oldTextureIdsByMaterial,
        bool allowMissing = false)
    {
        HashSet<uint> preferred = oldTextureIdsByMaterial.Values
            .SelectMany(ids => ids).ToHashSet();
        foreach (SmoObjectEntry entry in document.Objects
                     .Where(entry => entry.TypeHash == SmoClassIds.TextureData)
                     .OrderByDescending(entry => preferred.Contains(entry.Id)))
        {
            if (SmoTextureDataDecoder.TryDecode(
                    document, entry, out SmoTextureDataInfo? data, out _) &&
                data is
                {
                    SourceKind: SmoTextureSourceKind.LegacyCrossPlatform,
                    CrossPlatform.FormatValue: 0 or 1,
                    CrossPlatform.Kind:
                        SmoTextureRepresentationKind.CrossPlatformBgra32
                })
            {
                return entry;
            }
            if (data is not null && SmoTextureDataWriter.CanReplace(data, out _))
            {
                return entry;
            }
        }
        if (allowMissing)
            return null;
        throw new NotSupportedException(
            "The level has no confirmed BGRA texture template for a new resource.");
    }

    private static byte[] BuildCanonicalTextureObject(ImportedTexture imported)
    {
        if (!SmoTextureSerializationLimits.IsSizeRepresentable(imported.Width, imported.Height))
        {
            throw new InvalidDataException(
                $"Texture {imported.Name} has unsupported dimensions " +
                $"{imported.Width}x{imported.Height}.");
        }
        using Image<Rgba32> image = Image.Load<Rgba32>(imported.Data);
        if (image.Width != imported.Width || image.Height != imported.Height)
            throw new InvalidDataException(
                $"Texture {imported.Name} dimensions do not match its image payload.");
        return SmoTextureDataWriter.CreateBgraObject(image.Width, image.Height, EncodeBgra(image));
    }

    internal static byte[] BuildTextureObject(
        SmoDocument document,
        SmoObjectEntry templateEntry,
        ImportedTexture imported)
    {
        if (!SmoTextureDataDecoder.TryDecode(
                document, templateEntry, out SmoTextureDataInfo? data, out string error))
            throw new InvalidDataException(error);
        bool legacy = data is
        {
            SourceKind: SmoTextureSourceKind.LegacyCrossPlatform,
            CrossPlatform.Kind: SmoTextureRepresentationKind.CrossPlatformBgra32,
            CrossPlatform.FormatValue: 0 or 1
        };
        if (!legacy && !SmoTextureDataWriter.CanReplace(data, out string reason))
            throw new NotSupportedException(reason);
        if (!SmoTextureSerializationLimits.IsSizeRepresentable(imported.Width, imported.Height))
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

        if (legacy)
        {
            // A new imported resource uses the native PC wrapper. Copying the
            // old bare field0 section does not initialize the game's DX reader.
            return SmoTextureDataWriter.CreateBgraObject(image.Width, image.Height, pixels);
        }
        return SmoTextureDataWriter.BuildReplacementObjectBgra(
            document, templateEntry.Index, image.Width, image.Height, pixels);
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

    private static void WriteUInt32(Span<byte> data, int offset, uint value) =>
        BinaryPrimitives.WriteUInt32LittleEndian(data[offset..], value);
}
