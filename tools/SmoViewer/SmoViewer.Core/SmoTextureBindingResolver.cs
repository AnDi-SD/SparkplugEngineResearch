using System.Collections.ObjectModel;

namespace SmoViewer.Core;

public sealed record SmoTextureBinding(
    SmoTexture? Texture,
    string? Issue,
    IReadOnlyList<SmoTexture>? AnimationFrames = null,
    TimeSpan? FrameDuration = null,
    SmoTexture? BaseTexture = null,
    bool UsesAlphaBlend = false,
    uint? DiffuseArgb = null,
    SmoMaterialRenderStateInfo? MaterialRenderState = null);

/// <summary>
/// Resolves material textures without signature scans by following catalog
/// ownership and render-node material reuse used by character mesh chunks.
/// </summary>
public static class SmoTextureBindingResolver
{
    public static IReadOnlyDictionary<int, SmoTextureBinding> ResolveAll(
        SmoDocument document)
    {
        ArgumentNullException.ThrowIfNull(document);

        Dictionary<int, SmoObjectEntry> entriesByIndex = document.Objects
            .ToDictionary(entry => entry.Index);
        Dictionary<uint, SmoObjectEntry> uniqueEntriesById = document.Objects
            .GroupBy(entry => entry.Id)
            .Where(group => group.Count() == 1)
            .ToDictionary(group => group.Key, group => group.Single());
        Dictionary<int, List<SmoObjectEntry>> childrenByParent = document.Objects
            .Where(entry => entry.ParentIndex.HasValue)
            .GroupBy(entry => entry.ParentIndex!.Value)
            .ToDictionary(group => group.Key, group => group.ToList());
        var result = new Dictionary<int, SmoTextureBinding>();
        bool isLargeLevelGraph = document.Objects.Count(entry =>
            entry.TypeHash == SmoClassIds.MeshData) > 100;

        foreach ((int ownerIndex, List<SmoObjectEntry> ownerChildren) in childrenByParent)
        {
            SmoObjectEntry[] meshes = ownerChildren
                .Where(entry => entry.TypeHash == SmoClassIds.MeshData)
                .ToArray();
            if (meshes.Length == 0)
                continue;

            if (!entriesByIndex.TryGetValue(ownerIndex, out SmoObjectEntry? owner) ||
                !IsConfirmedEntry(owner))
            {
                string issue =
                    $"INVALID_TEXTURE_OWNER: Mesh owner [{ownerIndex}] does not have " +
                    "a confirmed catalog interval and typeHash/SBOO signature.";
                foreach (SmoObjectEntry mesh in meshes)
                    result[mesh.Index] = new SmoTextureBinding(null, issue);
                continue;
            }

            SmoObjectEntry[] materials = ownerChildren
                .Where(entry => entry.TypeHash == SmoClassIds.MaterialData)
                .ToArray();
            if (materials.Length == 0)
                continue;

            if (materials.Any(material => !IsConfirmedEntry(material)))
            {
                string issue =
                    $"INVALID_MATERIAL_ENTRY: Object [{ownerIndex}] contains a material " +
                    "without a confirmed catalog interval and typeHash/SBOO signature.";
                foreach (SmoObjectEntry mesh in meshes)
                    result[mesh.Index] = new SmoTextureBinding(null, issue);
                continue;
            }

            SmoObjectEntry[][] materialTextures = materials
                .Select(material =>
                {
                    SmoObjectEntry[] contained = document.Objects
                        .Where(entry =>
                            entry.TypeHash == SmoClassIds.TextureData &&
                            entry.LogicalOffset >= material.LogicalOffset &&
                            entry.LogicalEnd <= material.LogicalEnd)
                        .OrderBy(entry => entry.LogicalOffset)
                        .ToArray();
                    if (contained.Length == 0 && TryResolveExplicitTextureReference(
                            document,
                            material,
                            uniqueEntriesById,
                            out SmoObjectEntry explicitTexture))
                    {
                        return new[] { explicitTexture };
                    }
                    return contained;
                })
                .ToArray();
            int textureCount = materialTextures.Sum(entries => entries.Length);
            if (textureCount == 0)
            {
                if (isLargeLevelGraph)
                {
                    for (int meshIndex = 0; meshIndex < meshes.Length; meshIndex++)
                    {
                        SmoObjectEntry targetMesh = meshes[meshIndex];
                        int materialIndex = materials.Length == 1
                            ? 0
                            : materials.Length == meshes.Length
                                ? meshIndex
                                : Array.FindLastIndex(
                                    materials,
                                    material => material.LogicalOffset <
                                                targetMesh.LogicalOffset);
                        if (materialIndex < 0)
                            continue;
                        SmoMaterialRenderStateInfo? materialRenderState =
                            MaterialRenderState(document, materials[materialIndex]);
                        result[targetMesh.Index] = new SmoTextureBinding(
                            null,
                            null,
                            UsesAlphaBlend:
                                materialRenderState?.UsesAlphaBlend == true,
                            DiffuseArgb: MaterialDiffuse(
                                document, materials[materialIndex]),
                            MaterialRenderState: materialRenderState);
                    }
                }
                continue;
            }

            if (materials.Length == 1 &&
                TryDecodeLayeredTextureSequence(
                    document,
                    materials[0],
                    materialTextures[0],
                    out SmoTexture? baseTexture,
                    out IReadOnlyList<SmoTexture>? layeredFrames,
                    out TimeSpan layeredFrameDuration))
            {
                SmoMaterialRenderStateInfo? materialRenderState =
                    MaterialRenderState(document, materials[0]);
                SmoTextureBinding layered = new(
                    layeredFrames[0],
                    null,
                    layeredFrames,
                    layeredFrameDuration,
                    baseTexture,
                    materialRenderState?.UsesAlphaBlend == true,
                    MaterialDiffuse(document, materials[0]),
                    materialRenderState);
                foreach (SmoObjectEntry mesh in meshes)
                    result[mesh.Index] = layered;
                continue;
            }

            if (materials.Length == 1 &&
                materialTextures[0].Length > 1 &&
                TryDecodeTextureSequence(
                    document,
                    materials[0],
                    materialTextures[0],
                    out IReadOnlyList<SmoTexture>? frames,
                    out TimeSpan frameDuration))
            {
                SmoMaterialRenderStateInfo? materialRenderState =
                    MaterialRenderState(document, materials[0]);
                SmoTextureBinding animated = new(
                    frames[0], null, frames, frameDuration, null,
                    materialRenderState?.UsesAlphaBlend == true,
                    MaterialDiffuse(document, materials[0]),
                    materialRenderState);
                foreach (SmoObjectEntry mesh in meshes)
                    result[mesh.Index] = animated;
                continue;
            }

            if (materials.Length == 1 &&
                TryDecodeStaticLayeredTextures(
                    document,
                    materials[0],
                    materialTextures[0],
                    out SmoTexture? staticBaseTexture,
                    out SmoTexture? staticEffectTexture))
            {
                SmoMaterialRenderStateInfo? materialRenderState =
                    MaterialRenderState(document, materials[0]);
                SmoTextureBinding layered = new(
                    staticEffectTexture,
                    null,
                    BaseTexture: staticBaseTexture,
                    UsesAlphaBlend: materialRenderState?.UsesAlphaBlend == true,
                    DiffuseArgb: MaterialDiffuse(document, materials[0]),
                    MaterialRenderState: materialRenderState);
                foreach (SmoObjectEntry mesh in meshes)
                    result[mesh.Index] = layered;
                continue;
            }

            if (materialTextures.Any(entries => entries.Length != 1))
            {
                string issue =
                    $"AMBIGUOUS_TEXTURE_LAYERS: Object [{ownerIndex}] contains " +
                    $"{materials.Length} materials and {textureCount} texture objects; " +
                    "at least one material does not have exactly one texture.";
                foreach (SmoObjectEntry mesh in meshes)
                    result[mesh.Index] = new SmoTextureBinding(null, issue);
                continue;
            }

            for (int meshIndex = 0; meshIndex < meshes.Length; meshIndex++)
            {
                SmoObjectEntry targetMesh = meshes[meshIndex];
                int materialIndex;
                if (materials.Length == 1)
                {
                    materialIndex = 0;
                }
                else if (materials.Length == meshes.Length)
                {
                    materialIndex = meshIndex;
                }
                else
                {
                    materialIndex = Array.FindLastIndex(
                        materials,
                        material => material.LogicalOffset < targetMesh.LogicalOffset);
                    if (materialIndex < 0)
                    {
                        result[targetMesh.Index] = new SmoTextureBinding(
                            null,
                            $"AMBIGUOUS_MATERIAL_BINDING: Mesh [{targetMesh.Index}] " +
                            $"\"{targetMesh.Name}\" appears before all {materials.Length} " +
                            $"materials in owner [{ownerIndex}].");
                        continue;
                    }
                }

                SmoObjectEntry textureEntry = materialTextures[materialIndex][0];
                if (SmoTextureDecoder.TryDecode(
                        document,
                        textureEntry,
                        out SmoTexture? texture,
                        out string error))
                {
                    SmoMaterialRenderStateInfo? materialRenderState =
                        MaterialRenderState(document, materials[materialIndex]);
                    result[targetMesh.Index] = new SmoTextureBinding(
                        texture,
                        null,
                        UsesAlphaBlend: materialRenderState?.UsesAlphaBlend == true,
                        DiffuseArgb: MaterialDiffuse(
                            document, materials[materialIndex]),
                        MaterialRenderState: materialRenderState);
                }
                else
                {
                    result[targetMesh.Index] = new SmoTextureBinding(null, error);
                }
            }
        }

        // Partitioned levels reuse materials through a compact spModel field:
        // field type 0, payload { materialObjectId, 0 }. The referenced
        // spMaterialData remains in an earlier sector and owns the actual
        // texture. Treating a model without an inline material as vertex-color
        // only loses floor/tile detail (Alfea01 centerhallL01-001 [3795]).
        foreach (SmoObjectEntry model in document.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.Model))
        {
            if (!childrenByParent.TryGetValue(
                    model.Index, out List<SmoObjectEntry>? modelChildren) ||
                !modelChildren.Any(entry =>
                    entry.TypeHash == SmoClassIds.MeshData) ||
                modelChildren.Any(entry =>
                    entry.TypeHash == SmoClassIds.MaterialData) ||
                !TryResolveExplicitMaterialReference(
                    document,
                    model,
                    uniqueEntriesById,
                    out SmoObjectEntry referencedMaterial) ||
                !TryCreateReferencedMaterialBinding(
                    document,
                    referencedMaterial,
                    uniqueEntriesById,
                    out SmoTextureBinding referencedBinding))
            {
                continue;
            }

            foreach (SmoObjectEntry mesh in modelChildren.Where(entry =>
                         entry.TypeHash == SmoClassIds.MeshData))
            {
                result.TryAdd(mesh.Index, referencedBinding);
            }
        }

        SmoObjectEntry[] allMeshes = document.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.MeshData)
            .ToArray();

        // Material reuse below is a confirmed character-export convention.
        // Large level graphs need their explicit relation fields decoded; using
        // the character primary-atlas fallback assigns unrelated room textures.
        if (isLargeLevelGraph)
            return new ReadOnlyDictionary<int, SmoTextureBinding>(result);

        Dictionary<int, (int MeshIndex, SmoTextureBinding Binding)[]>
            renderNodeLayeredBindings = allMeshes
                .Select(mesh => (
                    RenderNode: FindNearestAncestor(
                        mesh, SmoClassIds.RenderNode, entriesByIndex),
                    MeshIndex: mesh.Index,
                    Binding: result.GetValueOrDefault(mesh.Index)))
                .Where(item =>
                    item.RenderNode is not null &&
                    item.Binding?.BaseTexture is not null &&
                    item.Binding.AnimationFrames is { Count: > 1 })
                .GroupBy(item => item.RenderNode!.Index)
                .ToDictionary(
                    group => group.Key,
                    group => group.Select(item =>
                            (item.MeshIndex, item.Binding!))
                        .OrderBy(item => item.MeshIndex)
                        .ToArray());

        Dictionary<int, (int MeshIndex, SmoTextureBinding Binding)[]> renderNodeBindings = allMeshes
            .Select(mesh => (
                RenderNode: FindNearestAncestor(
                    mesh, SmoClassIds.RenderNode, entriesByIndex),
                MeshIndex: mesh.Index,
                Binding: result.GetValueOrDefault(mesh.Index)))
            // Animated/effect materials are local to their owner and must not
            // become the inherited character atlas for following skin chunks.
            .Where(item => item.Binding?.AnimationFrames is null)
            .Where(item => item.RenderNode is not null && item.Binding?.Texture is not null)
            .GroupBy(item => item.RenderNode!.Index)
            .ToDictionary(
                group => group.Key,
                group => group.Select(item =>
                        (item.MeshIndex, item.Binding!))
                    .OrderBy(item => item.MeshIndex)
                    .ToArray());
        foreach (int meshIndex in result
                     .Where(item =>
                         item.Value.AnimationFrames is { Count: > 1 } &&
                         item.Value.BaseTexture is null)
                     .Select(item => item.Key)
                     .ToArray())
        {
            SmoObjectEntry mesh = entriesByIndex[meshIndex];
            SmoObjectEntry? renderNode = FindNearestAncestor(
                mesh, SmoClassIds.RenderNode, entriesByIndex);
            if (renderNode is null ||
                !renderNodeBindings.TryGetValue(
                    renderNode.Index,
                    out (int MeshIndex, SmoTextureBinding Binding)[]? staticBindings))
                continue;
            SmoTexture? baseTexture = staticBindings
                .Where(binding => binding.MeshIndex < meshIndex)
                .OrderByDescending(binding => binding.MeshIndex)
                .Select(binding => binding.Binding.Texture)
                .FirstOrDefault();
            if (baseTexture is not null)
                result[meshIndex] = result[meshIndex] with { BaseTexture = baseTexture };
        }

        // A render node commonly contains several spSkin chunks but serializes
        // its material only with the first mesh chunk.
        foreach (SmoObjectEntry mesh in allMeshes)
        {
            if (result.ContainsKey(mesh.Index))
                continue;

            // Cross-render-node inheritance is a skin-container convention.
            // Rigid spModel meshes (mouth pieces and eye caps) have their own
            // material semantics and must not borrow the character atlas.
            if (mesh.ParentIndex is not int parentIndex ||
                !entriesByIndex.TryGetValue(parentIndex, out SmoObjectEntry? parent) ||
                parent.TypeHash != SmoClassIds.Skin)
            {
                continue;
            }

            SmoObjectEntry? renderNode = FindNearestAncestor(
                mesh, SmoClassIds.RenderNode, entriesByIndex);
            while (renderNode is not null)
            {
                if (renderNodeLayeredBindings.TryGetValue(
                        renderNode.Index,
                        out (int MeshIndex, SmoTextureBinding Binding)[]? layeredBindings))
                {
                    string targetFamily = GetNumberedObjectFamily(parent.Name);
                    SmoTextureBinding? inheritedLayered = layeredBindings
                        .Where(binding => binding.MeshIndex < mesh.Index)
                        .Where(binding =>
                        {
                            SmoObjectEntry sourceMesh = entriesByIndex[binding.MeshIndex];
                            return sourceMesh.ParentIndex is int sourceParentIndex &&
                                   entriesByIndex.TryGetValue(
                                       sourceParentIndex, out SmoObjectEntry? sourceParent) &&
                                   sourceParent.TypeHash == SmoClassIds.Skin &&
                                   GetNumberedObjectFamily(sourceParent.Name).Equals(
                                       targetFamily, StringComparison.OrdinalIgnoreCase);
                        })
                        .OrderByDescending(binding => binding.MeshIndex)
                        .Select(binding => binding.Binding)
                        .FirstOrDefault();
                    if (inheritedLayered is not null)
                    {
                        result[mesh.Index] = inheritedLayered;
                        break;
                    }
                }

                if (renderNodeBindings.TryGetValue(
                        renderNode.Index,
                        out (int MeshIndex, SmoTextureBinding Binding)[]? bindings))
                {
                    SmoTextureBinding? inheritedBinding = bindings
                        .Where(binding => binding.MeshIndex < mesh.Index)
                        .OrderByDescending(binding => binding.MeshIndex)
                        .Select(binding => binding.Binding)
                        .FirstOrDefault();
                    if (inheritedBinding is not null)
                    {
                        result[mesh.Index] = inheritedBinding;
                        break;
                    }
                }

                renderNode = FindNearestAncestor(
                    renderNode, SmoClassIds.RenderNode, entriesByIndex);
            }
        }

        // Nested WingL/WingR render nodes reuse the surrounding character atlas
        // without serializing another texture object. Keep this naming-backed
        // fallback narrow: other neutral rigid parts include eye caps that must
        // not inherit the clothing atlas.
        foreach (SmoObjectEntry meshEntry in allMeshes)
        {
            if (result.ContainsKey(meshEntry.Index) ||
                meshEntry.ParentIndex is not int modelIndex ||
                !entriesByIndex.TryGetValue(modelIndex, out SmoObjectEntry? model) ||
                model.TypeHash != SmoClassIds.Model ||
                !HasNeutralTexturedMaterial(document, model, childrenByParent) ||
                !SmoMeshDecoder.TryDecode(
                    document, meshEntry, out SmoMesh? mesh, out _) ||
                mesh is null ||
                !HasRenderableNeutralUvSurface(mesh))
            {
                continue;
            }

            SmoObjectEntry? localRenderNode = FindNearestAncestor(
                meshEntry, SmoClassIds.RenderNode, entriesByIndex);
            if (localRenderNode is null ||
                !IsWingRenderNode(localRenderNode.Name))
            {
                continue;
            }

            SmoObjectEntry? ancestorRenderNode = FindNearestAncestor(
                localRenderNode, SmoClassIds.RenderNode, entriesByIndex);
            while (ancestorRenderNode is not null)
            {
                if (renderNodeBindings.TryGetValue(
                        ancestorRenderNode.Index,
                        out (int MeshIndex, SmoTextureBinding Binding)[]? bindings))
                {
                    SmoTextureBinding? inheritedBinding = bindings
                        .Where(binding => binding.MeshIndex < meshEntry.Index)
                        .OrderByDescending(binding => binding.MeshIndex)
                        .Select(binding => binding.Binding)
                        .FirstOrDefault();
                    if (inheritedBinding is not null)
                    {
                        result[meshEntry.Index] = inheritedBinding;
                        break;
                    }
                }

                ancestorRenderNode = FindNearestAncestor(
                    ancestorRenderNode, SmoClassIds.RenderNode, entriesByIndex);
            }
        }

        // Numbered sibling render nodes such as Goopeye01/02/03 share one
        // material. Only propagate when the family points to one unique texture;
        // if several source materials use it with different states, inherit the
        // nearest complete binding instead of combining their flags globally.
        Dictionary<string, (int MeshIndex, SmoTextureBinding Binding)[]> familyBindings =
            renderNodeBindings
                .SelectMany(item => item.Value.Select(binding => (
                    Family: GetRenderNodeFamily(entriesByIndex[item.Key].Name),
                    binding.MeshIndex,
                    binding.Binding)))
                .GroupBy(item => item.Family, StringComparer.OrdinalIgnoreCase)
                .ToDictionary(
                    group => group.Key,
                    group => group.Select(item =>
                            (item.MeshIndex, item.Binding))
                        .OrderBy(item => item.MeshIndex)
                        .ToArray(),
                    StringComparer.OrdinalIgnoreCase);
        foreach (SmoObjectEntry mesh in allMeshes)
        {
            if (result.ContainsKey(mesh.Index))
                continue;

            SmoObjectEntry? renderNode = FindNearestAncestor(
                mesh, SmoClassIds.RenderNode, entriesByIndex);
            if (renderNode is null ||
                !familyBindings.TryGetValue(
                    GetRenderNodeFamily(renderNode.Name),
                    out (int MeshIndex, SmoTextureBinding Binding)[]? sources) ||
                sources.Select(source => source.Binding.Texture!.ObjectIndex)
                    .Distinct().Take(2).Count() != 1)
            {
                continue;
            }

            SmoTextureBinding inheritedBinding = sources
                .OrderBy(source => source.MeshIndex < mesh.Index ? 0 : 1)
                .ThenBy(source => Math.Abs((long)source.MeshIndex - mesh.Index))
                .ThenByDescending(source => source.MeshIndex)
                .Select(source => source.Binding)
                .First();
            result[mesh.Index] = inheritedBinding;
        }

        // The primary character atlas is the largest confirmed texture; detached
        // render nodes (for example Legs or Object01) reference that material.
        (int MeshIndex, SmoTextureBinding Binding)[] primarySources = allMeshes
            .Select(mesh => (
                MeshIndex: mesh.Index,
                Binding: result.GetValueOrDefault(mesh.Index)))
            .Where(source => source.Binding?.Texture is not null)
            .Select(source => (source.MeshIndex, source.Binding!))
            .ToArray();
        SmoTexture[] primaryTextures = primarySources
            .Select(source => source.Binding.Texture!)
            .DistinctBy(texture => texture!.ObjectIndex)
            .OrderByDescending(texture => TextureArea(texture!))
            .ToArray();
        if (primaryTextures.Length > 0)
        {
            long largestArea = TextureArea(primaryTextures[0]);
            SmoTexture[] largestTextures = primaryTextures
                .TakeWhile(texture => TextureArea(texture) == largestArea)
                .ToArray();
            foreach (SmoObjectEntry mesh in allMeshes)
            {
                if (!result.ContainsKey(mesh.Index) &&
                    FindNearestAncestor(
                        mesh, SmoClassIds.Skin, entriesByIndex) is not null &&
                    FindNearestAncestor(
                        mesh, SmoClassIds.Model, entriesByIndex) is null)
                {
                    // Repeated spSkin chunks can omit their texture object.
                    // Rigid spModel objects instead keep their own material or
                    // vertex color and must not receive the character atlas.
                    SmoTexture primaryTexture = largestTextures
                        .Where(texture => texture.ObjectIndex < mesh.Index)
                        .OrderByDescending(texture => texture.ObjectIndex)
                        .FirstOrDefault() ?? largestTextures
                        .OrderBy(texture => Math.Abs(texture.ObjectIndex - mesh.Index))
                        .First();
                    SmoTextureBinding primaryBinding = primarySources
                        .Where(source => source.Binding.Texture!.ObjectIndex ==
                                         primaryTexture.ObjectIndex)
                        .OrderBy(source => source.MeshIndex < mesh.Index ? 0 : 1)
                        .ThenBy(source =>
                            Math.Abs((long)source.MeshIndex - mesh.Index))
                        .ThenByDescending(source => source.MeshIndex)
                        .Select(source => source.Binding)
                        .First();
                    result[mesh.Index] = primaryBinding;
                }
            }

        }

        return new ReadOnlyDictionary<int, SmoTextureBinding>(result);
    }

    private static bool TryResolveExplicitMaterialReference(
        SmoDocument document,
        SmoObjectEntry model,
        IReadOnlyDictionary<uint, SmoObjectEntry> uniqueEntriesById,
        out SmoObjectEntry material)
    {
        material = null!;
        if (!IsConfirmedEntry(model) || model.PhysicalOffset > int.MaxValue ||
            model.SerializedSize > int.MaxValue)
        {
            return false;
        }

        ReadOnlySpan<byte> serialized = document.Data.Span.Slice(
            checked((int)model.PhysicalOffset),
            checked((int)model.SerializedSize));
        SmoObjectEntry? resolved = null;
        int offset = 8;
        while (offset < serialized.Length &&
               SmoDataBlockReader.TryReadHeader(
                   serialized, offset, out SmoDataBlockHeader field))
        {
            if (field.FieldType == 0 && field.PayloadSize == 8)
            {
                ReadOnlySpan<byte> payload = serialized.Slice(
                    field.PayloadOffset, 8);
                uint objectId = System.Buffers.Binary.BinaryPrimitives
                    .ReadUInt32LittleEndian(payload);
                uint inlineSize = System.Buffers.Binary.BinaryPrimitives
                    .ReadUInt32LittleEndian(payload[4..]);
                if (objectId != 0 && inlineSize == 0 &&
                    uniqueEntriesById.TryGetValue(
                        objectId, out SmoObjectEntry? candidate) &&
                    candidate.TypeHash == SmoClassIds.MaterialData &&
                    IsConfirmedEntry(candidate))
                {
                    if (resolved is not null && resolved.Index != candidate.Index)
                        return false;
                    resolved = candidate;
                }
            }
            offset = checked((int)field.PayloadEnd);
        }

        if (offset != serialized.Length || resolved is null)
            return false;
        material = resolved;
        return true;
    }

    private static bool TryCreateReferencedMaterialBinding(
        SmoDocument document,
        SmoObjectEntry material,
        IReadOnlyDictionary<uint, SmoObjectEntry> uniqueEntriesById,
        out SmoTextureBinding binding)
    {
        binding = null!;
        if (!IsConfirmedEntry(material))
            return false;

        SmoObjectEntry[] textureEntries = document.Objects
            .Where(entry =>
                entry.TypeHash == SmoClassIds.TextureData &&
                entry.LogicalOffset >= material.LogicalOffset &&
                entry.LogicalEnd <= material.LogicalEnd)
            .OrderBy(entry => entry.LogicalOffset)
            .ToArray();
        if (textureEntries.Length == 0 && TryResolveExplicitTextureReference(
                document,
                material,
                uniqueEntriesById,
                out SmoObjectEntry explicitTexture))
        {
            textureEntries = [explicitTexture];
        }

        SmoMaterialRenderStateInfo? renderState =
            MaterialRenderState(document, material);
        uint? diffuse = MaterialDiffuse(document, material);
        if (textureEntries.Length == 0)
        {
            binding = new SmoTextureBinding(
                null,
                null,
                UsesAlphaBlend: renderState?.UsesAlphaBlend == true,
                DiffuseArgb: diffuse,
                MaterialRenderState: renderState);
            return true;
        }

        if (TryDecodeLayeredTextureSequence(
                document,
                material,
                textureEntries,
                out SmoTexture? baseTexture,
                out IReadOnlyList<SmoTexture>? layeredFrames,
                out TimeSpan layeredFrameDuration))
        {
            binding = new SmoTextureBinding(
                layeredFrames[0],
                null,
                layeredFrames,
                layeredFrameDuration,
                baseTexture,
                renderState?.UsesAlphaBlend == true,
                diffuse,
                renderState);
            return true;
        }

        if (textureEntries.Length > 1 && TryDecodeTextureSequence(
                document,
                material,
                textureEntries,
                out IReadOnlyList<SmoTexture>? frames,
                out TimeSpan frameDuration))
        {
            binding = new SmoTextureBinding(
                frames[0],
                null,
                frames,
                frameDuration,
                null,
                renderState?.UsesAlphaBlend == true,
                diffuse,
                renderState);
            return true;
        }

        if (TryDecodeStaticLayeredTextures(
                document,
                material,
                textureEntries,
                out SmoTexture? staticBaseTexture,
                out SmoTexture? staticEffectTexture))
        {
            binding = new SmoTextureBinding(
                staticEffectTexture,
                null,
                BaseTexture: staticBaseTexture,
                UsesAlphaBlend: renderState?.UsesAlphaBlend == true,
                DiffuseArgb: diffuse,
                MaterialRenderState: renderState);
            return true;
        }

        if (textureEntries.Length != 1)
        {
            binding = new SmoTextureBinding(
                null,
                $"AMBIGUOUS_REFERENCED_MATERIAL_TEXTURES: Material " +
                $"[{material.Index}] contains {textureEntries.Length} textures.",
                UsesAlphaBlend: renderState?.UsesAlphaBlend == true,
                DiffuseArgb: diffuse,
                MaterialRenderState: renderState);
            return true;
        }

        if (!SmoTextureDecoder.TryDecode(
                document,
                textureEntries[0],
                out SmoTexture? texture,
                out string error) || texture is null)
        {
            binding = new SmoTextureBinding(
                null,
                error,
                UsesAlphaBlend: renderState?.UsesAlphaBlend == true,
                DiffuseArgb: diffuse,
                MaterialRenderState: renderState);
            return true;
        }

        binding = new SmoTextureBinding(
            texture,
            null,
            UsesAlphaBlend: renderState?.UsesAlphaBlend == true,
            DiffuseArgb: diffuse,
            MaterialRenderState: renderState);
        return true;
    }

    private static SmoMaterialRenderStateInfo? MaterialRenderState(
        SmoDocument document,
        SmoObjectEntry material) =>
        SmoMaterialRenderState.TryDecode(
            document, material, out SmoMaterialRenderStateInfo? state)
                ? state
                : null;

    private static uint? MaterialDiffuse(
        SmoDocument document,
        SmoObjectEntry material) =>
        SmoMaterialColorResolver.TryDecodeDiffuse(document, material, out uint diffuse)
            ? diffuse
            : null;

    private static bool TryResolveExplicitTextureReference(
        SmoDocument document,
        SmoObjectEntry material,
        IReadOnlyDictionary<uint, SmoObjectEntry> uniqueEntriesById,
        out SmoObjectEntry texture)
    {
        texture = null!;
        if (!IsConfirmedEntry(material) || material.PhysicalOffset > int.MaxValue ||
            material.SerializedSize > int.MaxValue)
        {
            return false;
        }

        ReadOnlySpan<byte> serialized = document.Data.Span.Slice(
            checked((int)material.PhysicalOffset),
            checked((int)material.SerializedSize));
        SmoObjectEntry? resolved = null;
        int offset = 8;
        while (offset < serialized.Length &&
               SmoDataBlockReader.TryReadHeader(
                   serialized, offset, out SmoDataBlockHeader field))
        {
            if (field.FieldType == 10 && field.PayloadSize == 8)
            {
                ReadOnlySpan<byte> payload = serialized.Slice(field.PayloadOffset, 8);
                uint objectId = System.Buffers.Binary.BinaryPrimitives
                    .ReadUInt32LittleEndian(payload);
                uint inlineSize = System.Buffers.Binary.BinaryPrimitives
                    .ReadUInt32LittleEndian(payload[4..]);
                if (inlineSize == 0 &&
                    uniqueEntriesById.TryGetValue(objectId, out SmoObjectEntry? candidate) &&
                    candidate.TypeHash == SmoClassIds.TextureData &&
                    IsConfirmedEntry(candidate))
                {
                    if (resolved is not null && resolved.Index != candidate.Index)
                        return false;
                    resolved = candidate;
                }
            }
            offset = checked((int)field.PayloadEnd);
        }
        if (offset != serialized.Length || resolved is null)
            return false;
        texture = resolved;
        return true;
    }

    private static SmoObjectEntry? FindNearestAncestor(
        SmoObjectEntry entry,
        uint typeHash,
        IReadOnlyDictionary<int, SmoObjectEntry> entriesByIndex)
    {
        SmoObjectEntry? cursor = entry;
        while (cursor.ParentIndex is int parentIndex &&
               entriesByIndex.TryGetValue(parentIndex, out cursor))
        {
            if (cursor.TypeHash == typeHash)
                return cursor;
        }

        return null;
    }

    private static string GetRenderNodeFamily(string name)
    {
        string family = name.TrimEnd(
            '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '_', '-', ' ');
        if (family.EndsWith("_L", StringComparison.OrdinalIgnoreCase) ||
            family.EndsWith("_R", StringComparison.OrdinalIgnoreCase) ||
            family.EndsWith(".L", StringComparison.OrdinalIgnoreCase) ||
            family.EndsWith(".R", StringComparison.OrdinalIgnoreCase))
            family = family[..^2];
        return family;
    }

    private static string GetNumberedObjectFamily(string name) => name
        .TrimEnd('0', '1', '2', '3', '4', '5', '6', '7', '8', '9')
        .TrimEnd('_', '-', ' ');

    private static bool HasNeutralTexturedMaterial(
        SmoDocument document,
        SmoObjectEntry model,
        IReadOnlyDictionary<int, List<SmoObjectEntry>> childrenByParent)
    {
        if (!childrenByParent.TryGetValue(
                model.Index, out List<SmoObjectEntry>? children))
            return false;

        SmoObjectEntry[] materials = children
            .Where(entry => entry.TypeHash == SmoClassIds.MaterialData)
            .ToArray();
        return materials.Length == 1 &&
               SmoMaterialColorResolver.TryDecodeDiffuse(
                   document, materials[0], out uint diffuse) &&
               diffuse == 0xFFFFFFFF;
    }

    private static bool HasRenderableNeutralUvSurface(SmoMesh mesh)
    {
        if (!mesh.HasTextureCoordinates ||
            (mesh.HasDiffuseColors &&
             mesh.DiffuseColorsArgb.Any(color => color != 0xFFFFFFFF)))
        {
            return false;
        }

        for (int triangle = 0;
             triangle + 2 < mesh.TriangleIndices.Length;
             triangle += 3)
        {
            int a = checked((int)mesh.TriangleIndices[triangle]);
            int b = checked((int)mesh.TriangleIndices[triangle + 1]);
            int c = checked((int)mesh.TriangleIndices[triangle + 2]);
            if ((uint)a >= (uint)mesh.TextureCoordinates.Length ||
                (uint)b >= (uint)mesh.TextureCoordinates.Length ||
                (uint)c >= (uint)mesh.TextureCoordinates.Length)
            {
                return false;
            }

            System.Numerics.Vector2 ab =
                mesh.TextureCoordinates[b] - mesh.TextureCoordinates[a];
            System.Numerics.Vector2 ac =
                mesh.TextureCoordinates[c] - mesh.TextureCoordinates[a];
            if (MathF.Abs(ab.X * ac.Y - ab.Y * ac.X) > 0.0000001f)
                return true;
        }

        return false;
    }

    private static bool IsWingRenderNode(string name)
    {
        string normalized = name.Replace("_", string.Empty)
            .Replace("-", string.Empty)
            .Replace(".", string.Empty)
            .Replace(" ", string.Empty);
        return normalized.StartsWith("Wing", StringComparison.OrdinalIgnoreCase) &&
               (normalized.EndsWith('L') || normalized.EndsWith('R'));
    }

    private static bool TryDecodeStaticLayeredTextures(
        SmoDocument document,
        SmoObjectEntry material,
        IReadOnlyList<SmoObjectEntry> textureEntries,
        out SmoTexture baseTexture,
        out SmoTexture effectTexture)
    {
        baseTexture = null!;
        effectTexture = null!;
        if (textureEntries.Count != 2 ||
            !document.Objects.Any(entry =>
                entry.TypeHash == SmoClassIds.UvController &&
                entry.LogicalOffset >= material.LogicalOffset &&
                entry.LogicalEnd <= material.LogicalEnd))
        {
            return false;
        }

        // Confirmed by Alfea03/Pcrystal12: the serialized order is UV0 base,
        // followed by the UV1 highlight controlled by spUVController.
        if (!SmoTextureDecoder.TryDecode(
                document, textureEntries[0], out SmoTexture? decodedBase, out _) ||
            decodedBase is null ||
            !SmoTextureDecoder.TryDecode(
                document, textureEntries[1], out SmoTexture? decodedEffect, out _) ||
            decodedEffect is null)
        {
            return false;
        }

        baseTexture = decodedBase;
        effectTexture = decodedEffect;
        return true;
    }

    private static bool TryDecodeLayeredTextureSequence(
        SmoDocument document,
        SmoObjectEntry material,
        IReadOnlyList<SmoObjectEntry> textureEntries,
        out SmoTexture baseTexture,
        out IReadOnlyList<SmoTexture> frames,
        out TimeSpan frameDuration)
    {
        baseTexture = null!;
        frames = [];
        frameDuration = TimeSpan.Zero;
        if (textureEntries.Count < 3)
            return false;

        SmoObjectEntry[][] sequences = textureEntries
            .GroupBy(
                entry => entry.Name.TrimEnd(
                    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'),
                StringComparer.OrdinalIgnoreCase)
            .Where(group => !string.IsNullOrWhiteSpace(group.Key) && group.Count() > 1)
            .Select(group => group.OrderBy(entry => entry.LogicalOffset).ToArray())
            .ToArray();
        if (sequences.Length != 1)
            return false;

        HashSet<int> sequenceIndices = sequences[0]
            .Select(entry => entry.Index)
            .ToHashSet();
        SmoObjectEntry[] baseEntries = textureEntries
            .Where(entry => !sequenceIndices.Contains(entry.Index))
            .ToArray();
        if (baseEntries.Length != 1 ||
            !SmoTextureDecoder.TryDecode(
                document, baseEntries[0], out SmoTexture? decodedBase, out _) ||
            decodedBase is null ||
            !TryDecodeTextureSequence(
                document,
                material,
                sequences[0],
                out IReadOnlyList<SmoTexture>? decodedFrames,
                out frameDuration))
        {
            return false;
        }

        baseTexture = decodedBase;
        frames = decodedFrames;
        return true;
    }

    private static bool TryDecodeTextureSequence(
        SmoDocument document,
        SmoObjectEntry material,
        IReadOnlyList<SmoObjectEntry> textureEntries,
        out IReadOnlyList<SmoTexture> frames,
        out TimeSpan frameDuration)
    {
        frames = [];
        frameDuration = TimeSpan.Zero;
        string[] prefixes = textureEntries
            .Select(entry => entry.Name.TrimEnd('0', '1', '2', '3', '4', '5', '6', '7', '8', '9'))
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .ToArray();
        if (textureEntries.Count < 2 || prefixes.Length != 1 ||
            string.IsNullOrWhiteSpace(prefixes[0]))
            return false;

        var decoded = new List<SmoTexture>(textureEntries.Count);
        foreach (SmoObjectEntry entry in textureEntries)
        {
            if (!SmoTextureDecoder.TryDecode(
                    document, entry, out SmoTexture? texture, out _) || texture is null)
                return false;
            if (decoded.Count > 0 &&
                (texture.Width != decoded[0].Width || texture.Height != decoded[0].Height))
                return false;
            decoded.Add(texture);
        }

        const uint textureSequenceController = 0x16FB0E47;
        SmoObjectEntry? controller = document.Objects.FirstOrDefault(entry =>
            entry.TypeHash == textureSequenceController &&
            entry.LogicalOffset >= material.LogicalOffset &&
            entry.LogicalEnd <= material.LogicalEnd);
        float durationSeconds = 0;
        if (controller is not null && controller.PhysicalOffset <= int.MaxValue &&
            controller.SerializedSize >= 21)
        {
            ReadOnlySpan<byte> bytes = document.Data.Span.Slice(
                (int)controller.PhysicalOffset, (int)controller.SerializedSize);
            if (bytes[8] == 0xE0 && bytes[9] == 0x0C &&
                bytes[10] == 0x44 && bytes[11] == 0x02)
            {
                int keyCount = System.Buffers.Binary.BinaryPrimitives.ReadInt32LittleEndian(bytes[13..17]);
                int lastKeyOffset = 17 + (keyCount - 1) * sizeof(float);
                if (keyCount > 0 && lastKeyOffset <= bytes.Length - sizeof(float))
                    durationSeconds = BitConverter.Int32BitsToSingle(
                        System.Buffers.Binary.BinaryPrimitives.ReadInt32LittleEndian(
                            bytes.Slice(lastKeyOffset, sizeof(float))));
            }
        }

        if (!float.IsFinite(durationSeconds) || durationSeconds <= 0)
            durationSeconds = decoded.Count / 8f;
        frames = decoded;
        frameDuration = TimeSpan.FromSeconds(durationSeconds / decoded.Count);
        return true;
    }

    private static long TextureArea(SmoTexture texture) =>
        (long)texture.Width * texture.Height;

    private static bool IsConfirmedEntry(SmoObjectEntry entry) =>
        entry.IsWithinDataSection && entry.SignatureMatches;
}
