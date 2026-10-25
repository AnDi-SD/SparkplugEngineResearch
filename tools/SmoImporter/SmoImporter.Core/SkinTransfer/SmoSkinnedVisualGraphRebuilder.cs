using System.Numerics;
using SmoViewer.Core;

namespace SmoImporter.Core;

internal static partial class SmoSkinnedVisualGraphPipeline
{
    private sealed record RebuiltSkinnedVisualGraph(
        byte[] Data,
        int VertexCount,
        int TriangleCount,
        int PaletteCount,
        IReadOnlySet<uint> AddedObjectIds,
        IReadOnlySet<uint> AddedSkinIds,
        IReadOnlySet<uint> AddedMeshIds,
        IReadOnlySet<uint> AddedTextureIds,
        IReadOnlySet<uint> SkeletonCarrierMeshIds,
        IReadOnlySet<uint> RemovedTargetObjectIds,
        IReadOnlyDictionary<uint, ImportedTexture> ImportedTexturesByObjectId);

    private sealed record OriginalVisualRemovalResult(
        byte[] Data,
        IReadOnlySet<uint> RemovedTargetObjectIds,
        IReadOnlySet<uint> AddedSkeletonCarrierMeshIds);

    /// <summary>
    /// Rebuilds the character render graph from donor groups instead of
    /// patching target mesh/texture slots. The target is used only as a native
    /// layout template and as the owner of the animated skeleton. Every donor
    /// image becomes its own TextureData object. Original target meshes,
    /// materials and textures are physically removed from the FFPS container.
    /// </summary>
    private static GlbSkinTransferResult RebuildSkinnedVisualGraph(
        SmoDocument target,
        ImportedScene donor,
        GlbPlanContext context,
        IReadOnlyList<ImportedTransferMesh> transferMeshes,
        SkinnedRenderableMaterialProfile materialProfile,
        SkinnedGeometryTransferMode transferMode,
        string outputPath,
        CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        ImportedGroup[] sourceGroups = context.SourceGroups?
            .OrderByDescending(group => group.TriangleCount)
            .ThenBy(group => group.MaterialIndex)
            .ToArray() ?? [];
        if (sourceGroups.Length == 0)
        {
            throw new InvalidOperationException(
                "The skinned visual rebuild has no donor material groups.");
        }
        int sourceTriangleCount = donor.Meshes.Sum(mesh =>
            mesh.TriangleIndices.Length / 3);
        if (sourceGroups.Sum(group => group.TriangleCount) != sourceTriangleCount)
        {
            throw new InvalidDataException(
                "Donor material groups do not cover the complete imported geometry.");
        }

        TargetVisualGroup[] targetGroups = ReadTargetVisualGroups(
            target,
            SmoTextureBindingResolver.ResolveAll(target));
        if (targetGroups.Length == 0)
        {
            throw new InvalidOperationException(
                "The target has no native skinned visual group to use as a layout template.");
        }
        TargetVisualGroup templateGroup = OrderTargetGroupsByPreference(
            target,
            targetGroups)[0];
        uint textureTemplateObjectId =
            target.Objects[templateGroup.TextureObjectIndex].Id;
        SmoDocument current = target;
        var addedObjectIds = new HashSet<uint>();
        var addedSkinIds = new HashSet<uint>();
        var addedMeshIds = new HashSet<uint>();
        var addedTextureIds = new HashSet<uint>();
        var importedTexturesByObjectId = new Dictionary<uint, ImportedTexture>();
        int vertices = 0;
        int triangles = 0;
        int palettes = 0;

        foreach (ImportedGroup group in sourceGroups)
        {
            cancellationToken.ThrowIfCancellationRequested();
            ImportedTexture branchTexture =
                ResolveImportedGroupTexture(donor, group) ??
                throw new InvalidDataException(
                    $"Material group {group.MaterialIndex} has no donor texture. " +
                    "A clean character rebuild cannot fall back to a target image.");
            SmoSkinnedBranchSourceMesh[] branchMeshes = group.Meshes
                .Select(mesh => transferMeshes.Single(value => value.Key ==
                    GetImportedMeshIndex(donor.Meshes, mesh)))
                .Select(ToSkinnedBranchSourceMesh)
                .ToArray();
            SmoSkinnedRenderableOpacityPlan opacity =
                SmoSkinnedBranchSplitBuilder.ClassifyRenderables(
                    branchMeshes,
                    branchTexture,
                    materialProfile);
            branchMeshes = SmoSkinnedBranchSplitBuilder
                .ConformCloseSurfaceNormals(branchMeshes, opacity)
                .Meshes;

            SmoSkinnedBranchSplitResult result =
                SmoSkinnedBranchSplitBuilder.InjectImportedTexture(
                    current,
                    textureTemplateObjectId,
                    branchTexture,
                    branchMeshes,
                    opacity,
                    context.BoneRemap,
                    context.TargetInverseBind);
            // This fresh builder buffer belongs to the current operation.
            current = SmoDocument.ParseOwned(result.Data, target.SourcePath);
            if (current.HasErrors)
            {
                throw new InvalidDataException(
                    $"Material group {group.MaterialIndex} produced an invalid " +
                    "intermediate SMO graph.");
            }

            uint[] groupTextureIds = current.Objects
                .Where(entry => result.AddedObjectIds.Contains(entry.Id) &&
                                entry.TypeHash == SmoClassIds.TextureData)
                .Select(entry => entry.Id)
                .ToArray();
            if (groupTextureIds.Length != 1)
            {
                throw new InvalidDataException(
                    $"Material group {group.MaterialIndex} generated " +
                    $"{groupTextureIds.Length} TextureData objects instead of one.");
            }
            uint generatedTextureId = groupTextureIds[0];
            importedTexturesByObjectId.Add(generatedTextureId, branchTexture);
            addedTextureIds.Add(generatedTextureId);
            addedObjectIds.UnionWith(result.AddedObjectIds);
            addedSkinIds.UnionWith(result.AddedSkinIds);
            addedMeshIds.UnionWith(result.AddedMeshIds);
            vertices += result.VertexCount;
            triangles += result.TriangleCount;
            palettes += result.BranchCount;
        }

        if (triangles != sourceTriangleCount)
        {
            throw new InvalidDataException(
                $"Fresh visual branches contain {triangles} triangles, expected " +
                $"the donor's complete {sourceTriangleCount} triangles.");
        }

        OriginalVisualRemovalResult removal = RemoveOriginalCharacterVisuals(
                target,
                current,
                cancellationToken);
        addedObjectIds.UnionWith(removal.AddedSkeletonCarrierMeshIds);
        addedMeshIds.UnionWith(removal.AddedSkeletonCarrierMeshIds);
        var rebuilt = new RebuiltSkinnedVisualGraph(
            removal.Data,
            vertices,
            triangles,
            palettes,
            addedObjectIds,
            addedSkinIds,
            addedMeshIds,
            addedTextureIds,
            removal.AddedSkeletonCarrierMeshIds,
            removal.RemovedTargetObjectIds,
            importedTexturesByObjectId);

        SmoDocument? verified = null;
        int verifiedTriangles = 0;
        SmoVerifiedOutputInstallResult installed =
            SmoVerifiedOutputInstaller.Install(
                outputPath,
                rebuilt.Data,
                temporary =>
                {
                    cancellationToken.ThrowIfCancellationRequested();
                    verified = SmoDocument.Load(temporary);
                    var errors = new List<string>();
                    VerifyFreshSkinnedVisualGraph(
                        target,
                        donor,
                        context,
                        rebuilt,
                        verified,
                        transferMode,
                        errors,
                        out verifiedTriangles);
                    if (errors.Count > 0)
                    {
                        throw new InvalidDataException(
                            "Rebuilt skinned character failed verification: " +
                            string.Join("; ", errors.Distinct()) + ".");
                    }
                    cancellationToken.ThrowIfCancellationRequested();
                },
                cancellationToken,
                target.SourcePath);

        return new GlbSkinTransferResult(
            installed.OutputPath,
            verified!.Objects.Count(entry => entry.TypeHash == SmoClassIds.MeshData),
            rebuilt.VertexCount,
            verifiedTriangles,
            rebuilt.PaletteCount,
            verified.Data.Length,
            installed.Sha256);
    }

    private static OriginalVisualRemovalResult
        RemoveOriginalCharacterVisuals(
            SmoDocument target,
            SmoDocument withGeneratedVisuals,
            CancellationToken cancellationToken)
    {
        static bool Contains(SmoObjectEntry owner, SmoObjectEntry child) =>
            child.PhysicalOffset >= owner.PhysicalOffset &&
            child.PhysicalEnd <= owner.PhysicalEnd;

        SmoObjectEntry[] targetSkins = target.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.Skin)
            .ToArray();
        HashSet<uint> skeletonCarrierSkinIds = targetSkins
            .Where(skin => target.Objects.Any(entry =>
                entry.TypeHash == SmoClassIds.Node && Contains(skin, entry)))
            .Select(skin => skin.Id)
            .ToHashSet();
        if (skeletonCarrierSkinIds.Count == 0)
        {
            throw new InvalidDataException(
                "The target has no spSkin branch owning the inline animated skeleton. " +
                "A clean rebuild cannot remove its old visuals without losing bones.");
        }

        SmoDocument current = withGeneratedVisuals;
        var skeletonCarrierMeshIds = new HashSet<uint>();
        foreach (SmoObjectEntry carrier in targetSkins
                     .Where(entry => skeletonCarrierSkinIds.Contains(entry.Id))
                     .OrderBy(entry => entry.PhysicalOffset))
        {
            cancellationToken.ThrowIfCancellationRequested();
            current = RebuildSkeletonCarrierVisuals(
                target,
                current,
                carrier,
                skeletonCarrierMeshIds);
        }

        SmoObjectEntry[] removableRenderRoots = target.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.RenderNode &&
                            entry.ParentIndex is not null &&
                            target.Objects.Any(visual =>
                                (visual.TypeHash is SmoClassIds.MeshData or
                                    SmoClassIds.MaterialData or
                                    SmoClassIds.TextureData) &&
                                Contains(entry, visual)) &&
                            !targetSkins.Any(skin =>
                                skeletonCarrierSkinIds.Contains(skin.Id) &&
                                Contains(entry, skin)))
            .OrderByDescending(entry => entry.PhysicalOffset)
            .ToArray();

        uint[] originalVisualLeafIds = target.Objects
            .Where(entry => entry.TypeHash is SmoClassIds.MaterialData or
                SmoClassIds.MeshData or SmoClassIds.TextureData)
            .OrderBy(entry => entry.TypeHash == SmoClassIds.MaterialData ? 0 :
                entry.TypeHash == SmoClassIds.MeshData ? 1 : 2)
            .ThenByDescending(entry => entry.PhysicalOffset)
            .Select(entry => entry.Id)
            .ToArray();
        HashSet<uint> requestedRemovals = removableRenderRoots.Select(entry => entry.Id)
            .Concat(targetSkins.Where(entry => !skeletonCarrierSkinIds.Contains(entry.Id))
                .Select(entry => entry.Id))
            .Concat(originalVisualLeafIds).ToHashSet();
        uint[] presentRemovals = current.Objects.Where(entry => requestedRemovals.Contains(entry.Id))
            .Select(entry => entry.Id).ToArray();
        cancellationToken.ThrowIfCancellationRequested();
        current = ParseCleanRebuildStep(
            SmoVisualForestInjector.RemoveInlineBranches(current, presentRemovals),
            current.SourcePath,
            "remove old visual resources");

        HashSet<uint> retainedIds = current.Objects.Select(entry => entry.Id).ToHashSet();
        HashSet<uint> removedIds = target.Objects
            .Select(entry => entry.Id)
            .Where(id => !retainedIds.Contains(id))
            .ToHashSet();
        uint[] leakedVisuals = originalVisualLeafIds
            .Where(retainedIds.Contains)
            .ToArray();
        if (leakedVisuals.Length > 0)
        {
            throw new InvalidDataException(
                "Old character visual resources survived clean rebuild: " +
                string.Join(", ", leakedVisuals) + ".");
        }
        return new OriginalVisualRemovalResult(
            current.Data.ToArray(),
            removedIds,
            skeletonCarrierMeshIds);
    }

    /// <summary>
    /// A target spSkin may physically own the inline bone nodes that every new
    /// donor palette references. The skin object therefore remains as a pure
    /// skeleton carrier, but none of its old visual resources do. It becomes a
    /// material-less continuation-style skin and its old base mesh is replaced
    /// by a newly allocated one-vertex degenerate skinned mesh. Keeping it
    /// material-less also avoids a native-unsafe forward reference to an
    /// appended donor material.
    /// </summary>
    private static SmoDocument RebuildSkeletonCarrierVisuals(
        SmoDocument target,
        SmoDocument current,
        SmoObjectEntry targetCarrier,
        ISet<uint> skeletonCarrierMeshIds)
    {
        if (!SmoSkinDecoder.TryDecode(
                target,
                targetCarrier,
                out SmoSkin? targetSkin,
                out string skinError) ||
            targetSkin is null)
        {
            throw new InvalidDataException(
                $"Skeleton-carrier skin [{targetCarrier.Index}] is invalid: {skinError}");
        }
        if (targetSkin.Renderable.Material is { } materialRelationship)
        {
            if (materialRelationship.TargetObjectIndex is not int materialIndex)
            {
                throw new InvalidDataException(
                    $"Skeleton-carrier skin [{targetCarrier.Index}] has an " +
                    "unresolved material.");
            }
            SmoObjectEntry targetMaterial = target.Objects[materialIndex];
            SmoObjectEntry? currentMaterial = current.Objects.SingleOrDefault(entry =>
                entry.Id == targetMaterial.Id);
            bool materialIsInline = currentMaterial is not null &&
                currentMaterial.ParentIndex is int currentMaterialParent &&
                current.Objects[currentMaterialParent].Id == targetCarrier.Id;
            byte[] materialRemoved = materialIsInline
                ? SmoVisualForestInjector.RemoveInlineBranch(
                    current,
                    targetCarrier.Id,
                    targetMaterial.Id)
                : SmoVisualForestInjector.RemoveReference(
                    current,
                    targetCarrier.Id,
                    fieldType: 0,
                    targetMaterial.Id);
            current = ParseCleanRebuildStep(
                materialRemoved,
                current.SourcePath,
                $"remove old material {targetMaterial.Id}");
        }

        SmoNodeRelationship meshRelationship = targetSkin.BaseMesh;
        if (meshRelationship.TargetObjectIndex is not int meshIndex)
        {
            throw new InvalidDataException(
                $"Skeleton-carrier skin [{targetCarrier.Index}] has an unresolved base mesh.");
        }
        SmoObjectEntry targetMesh = target.Objects[meshIndex];
        byte[] carrierMesh = BuildSkeletonCarrierMesh(target, targetMesh);
        uint carrierMeshId = AllocateFreshObjectId(current);
        byte[] carrierName = BuildCatalogName("skeleton_carrier_mesh");
        SmoObjectEntry? currentMesh = current.Objects.SingleOrDefault(entry =>
            entry.Id == targetMesh.Id);
        bool meshIsInline = currentMesh is not null &&
            currentMesh.ParentIndex is int currentMeshParent &&
            current.Objects[currentMeshParent].Id == targetCarrier.Id;
        byte[] rebuiltCarrier = meshIsInline
            ? SmoVisualForestInjector.ReplaceInlineLeaf(
                current,
                targetCarrier.Id,
                fieldType: 0,
                targetMesh.Id,
                carrierMeshId,
                carrierName,
                SmoClassIds.MeshData,
                carrierMesh)
            : SmoVisualForestInjector.PromoteReferenceToInline(
                current,
                targetCarrier.Id,
                fieldType: 0,
                targetMesh.Id,
                carrierMeshId,
                carrierName,
                SmoClassIds.MeshData,
                carrierMesh);
        current = ParseCleanRebuildStep(
            rebuiltCarrier,
            current.SourcePath,
            $"replace old skeleton-carrier mesh {targetMesh.Id}");
        skeletonCarrierMeshIds.Add(carrierMeshId);
        return current;
    }

    private static SmoDocument ParseCleanRebuildStep(
        byte[] data,
        string? sourcePath,
        string operation)
    {
        SmoDocument parsed = SmoDocument.ParseOwned(data, sourcePath);
        if (parsed.HasErrors)
        {
            throw new InvalidDataException(
                $"Clean character rebuild produced an invalid SMO while trying to " +
                $"{operation}.");
        }
        return parsed;
    }

    private static uint AllocateFreshObjectId(SmoDocument document) =>
        document.Objects.Count == 0
            ? 1
            : checked(document.Objects.Max(entry => entry.Id) + 1);

    private static byte[] BuildCatalogName(string value)
    {
        byte[] encoded = System.Text.Encoding.UTF8.GetBytes(value);
        if (encoded.Length > 31)
        {
            throw new InvalidOperationException(
                $"Generated SMO name '{value}' exceeds the 31-byte catalog limit.");
        }
        byte[] result = new byte[encoded.Length + 1];
        encoded.CopyTo(result, 0);
        return result;
    }

    private static byte[] BuildSkeletonCarrierMesh(
        SmoDocument target,
        SmoObjectEntry targetMeshEntry)
    {
        SmoMesh template = SmoMeshDecoder.Decode(target, targetMeshEntry);
        if (template.Marker != SmoMeshDecoder.E1Marker ||
            !SmoVertexLayoutRegistry.TryGet(
                template.VertexFormat,
                out SmoVertexLayout? layout) ||
            layout is null ||
            layout.SerializedStride != template.Stride ||
            layout.BlendWeightsOffset is null ||
            layout.BlendIndicesOffset is null)
        {
            throw new NotSupportedException(
                $"Skeleton-carrier mesh [{targetMeshEntry.Index}] does not use a " +
                "confirmed skinned E1 vertex layout.");
        }

        return SmoMeshDataWriter.CreateTriangleList(SmoMesh.CreateTransient(
            template, [Vector3.Zero],
            layout.NormalOffset.HasValue ? [Vector3.UnitY] : [],
            layout.TextureCoordinate0Offset.HasValue ? [Vector2.Zero] : [],
            layout.TextureCoordinate1Offset.HasValue ? [Vector2.Zero] : [],
            layout.DiffuseArgbOffset.HasValue ? [0x00FFFFFFu] : [],
            [Vector4.UnitX], [new SmoBlendIndices(0,0,0,0)], [0,0,0]));
    }

    private static void VerifyFreshSkinnedVisualGraph(
        SmoDocument target,
        ImportedScene donor,
        GlbPlanContext context,
        RebuiltSkinnedVisualGraph rebuilt,
        SmoDocument output,
        SkinnedGeometryTransferMode transferMode,
        ICollection<string> errors,
        out int verifiedTriangles)
    {
        verifiedTriangles = 0;
        if (output.HasErrors)
            errors.Add("strict parser reported errors");
        HashSet<uint> expectedIds = target.Objects
            .Select(entry => entry.Id)
            .Where(id => !rebuilt.RemovedTargetObjectIds.Contains(id))
            .Concat(rebuilt.AddedObjectIds)
            .ToHashSet();
        HashSet<uint> actualIds = output.Objects.Select(entry => entry.Id).ToHashSet();
        if (!actualIds.SetEquals(expectedIds) ||
            output.Objects.Count != expectedIds.Count)
        {
            errors.Add("output object identities do not match clean rebuild plan");
        }
        if (target.Objects.Where(entry => entry.TypeHash is
                    SmoClassIds.MeshData or SmoClassIds.MaterialData or
                    SmoClassIds.TextureData)
                .Any(entry => actualIds.Contains(entry.Id)))
        {
            errors.Add("one or more original target mesh/material/texture IDs survived");
        }

        SmoObjectEntry[] outputMeshes = output.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.MeshData)
            .ToArray();
        SmoObjectEntry[] outputTextures = output.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.TextureData)
            .ToArray();
        SmoObjectEntry[] outputMaterials = output.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.MaterialData)
            .ToArray();
        if (outputMeshes.Any(entry => !rebuilt.AddedMeshIds.Contains(entry.Id)) ||
            outputTextures.Any(entry => !rebuilt.AddedTextureIds.Contains(entry.Id)) ||
            outputMaterials.Any(entry => !rebuilt.AddedObjectIds.Contains(entry.Id)))
        {
            errors.Add("output contains a visual resource outside generated donor branches");
        }
        if (outputTextures.Length != rebuilt.ImportedTexturesByObjectId.Count)
        {
            errors.Add(
                $"generated TextureData count {outputTextures.Length} != donor image " +
                $"group count {rebuilt.ImportedTexturesByObjectId.Count}");
        }

        IReadOnlyDictionary<int, SmoTextureBinding> bindings =
            SmoTextureBindingResolver.ResolveAll(output);
        foreach (SmoObjectEntry meshEntry in outputMeshes)
        {
            verifiedTriangles += CountImportedNonDegenerateTriangles(
                SmoMeshDecoder.Decode(output, meshEntry));
            SmoObjectEntry skinEntry = FindParentSkin(output, meshEntry);
            bool skeletonCarrier =
                rebuilt.SkeletonCarrierMeshIds.Contains(meshEntry.Id);
            if (!rebuilt.AddedSkinIds.Contains(skinEntry.Id) && !skeletonCarrier)
            {
                errors.Add(
                    $"mesh ID {meshEntry.Id} is outside a generated donor spSkin");
            }
            if (!skeletonCarrier &&
                (!bindings.TryGetValue(
                     meshEntry.Index, out SmoTextureBinding? binding) ||
                 binding.Issue is not null || binding.Texture is null ||
                 !rebuilt.AddedTextureIds.Contains(
                     output.Objects[binding.Texture.ObjectIndex].Id)))
            {
                errors.Add(
                    $"mesh ID {meshEntry.Id} is not bound to a generated donor texture");
            }
        }
        int expectedTriangles = donor.Meshes.Sum(mesh =>
            mesh.TriangleIndices.Length / 3);
        if (verifiedTriangles != expectedTriangles ||
            rebuilt.TriangleCount != expectedTriangles)
        {
            errors.Add(
                $"triangles {verifiedTriangles}/{rebuilt.TriangleCount} != donor " +
                expectedTriangles);
        }

        foreach ((uint textureId, ImportedTexture imported) in
                 rebuilt.ImportedTexturesByObjectId)
        {
            SmoObjectEntry? entry = output.Objects.SingleOrDefault(candidate =>
                candidate.Id == textureId &&
                candidate.TypeHash == SmoClassIds.TextureData);
            string textureError = "object is absent";
            if (entry is null ||
                !SmoTextureDecoder.TryDecode(
                    output,
                    entry,
                    out SmoTexture? decoded,
                    out textureError) ||
                decoded is null)
            {
                errors.Add(
                    $"generated donor texture ID {textureId} is invalid: " +
                    textureError);
                continue;
            }
            if (!ImportedTextureImageTools.SerializedBgraMatches(
                    imported.Data,
                    decoded.Width,
                    decoded.Height,
                    decoded.Bgra32Pixels.Span,
                    out string mismatch,
                    resizeToExpected: false))
            {
                errors.Add(
                    $"generated donor texture ID {textureId} pixel mismatch: " +
                    mismatch);
            }
        }

        VerifyTargetNodeInvariants(
            target,
            output,
            errors,
            rebuilt.RemovedTargetObjectIds);
        VerifyTargetPaletteBindMatrices(target, output, errors);
        VerifyTargetBindFrameIdentity(target, output, errors);
        if (transferMode == SkinnedGeometryTransferMode.PreservePreparedGeometry)
        {
            VerifyPreparedGeometryFingerprint(
                donor,
                output,
                context.BoneRemap,
                errors);
        }
    }
}
