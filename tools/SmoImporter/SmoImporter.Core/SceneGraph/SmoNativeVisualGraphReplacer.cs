using System.Buffers.Binary;
using System.Numerics;
using SmoViewer.Core;

namespace SmoImporter.Core;

public sealed record SmoNativeVisualGraphPlan(
    int VisualRootCount,
    int MeshCount,
    int MaterialCount,
    int TextureCount,
    int PreservedTargetNodeCount,
    IReadOnlyList<string> MatchedBoneNames,
    IReadOnlyList<string> IgnoredDonorBoneNames,
    IReadOnlyList<string> Errors,
    IReadOnlyList<string> Warnings)
{
    public bool CanReplace => Errors.Count == 0;
}

public sealed record SmoNativeVisualGraphReplaceResult(
    string OutputPath,
    int VisualRootCount,
    int MeshCount,
    int MaterialCount,
    int TextureCount,
    int CopiedObjectCount,
    long FileSize,
    string Sha256);

/// <summary>
/// Replaces a character's complete inline render forest with the donor's
/// already serialized render forest. Mesh and texture SBOO payloads are copied
/// byte-for-byte. Geometry, UV, weights, material runs and texture pixels are
/// not rebuilt. Donor-only palette bones are collapsed to the nearest shared
/// ancestor; only their node relationships and inverse binds are normalized.
/// </summary>
public static class SmoNativeVisualGraphReplacer
{
    private const int ObjectSignatureSize = 8;
    private const int ObjectReferenceSize = 8;
    private const int Matrix4x4Size = 16 * sizeof(float);

    public static SmoNativeVisualGraphPlan Analyze(
        SmoDocument target,
        SmoDocument donor,
        CancellationToken cancellationToken = default)
    {
        ArgumentNullException.ThrowIfNull(target);
        ArgumentNullException.ThrowIfNull(donor);
        cancellationToken.ThrowIfCancellationRequested();
        var errors = new List<string>();
        var warnings = new List<string>();
        NativeReplacementLayout? layout = null;
        try
        {
            layout = BuildLayout(target, donor, cancellationToken);
        }
        catch (Exception exception) when (exception is InvalidDataException or
                                          InvalidOperationException or
                                          NotSupportedException or
                                          OverflowException or
                                          ArgumentException)
        {
            errors.Add(exception.Message);
        }

        if (layout is null)
        {
            return new SmoNativeVisualGraphPlan(
                0, 0, 0, 0, 0, [], [], errors, warnings);
        }

        if (layout.IgnoredDonorBoneNames.Count > 0)
        {
            warnings.Add(
                "Donor-only skin bones were collapsed to their nearest shared " +
                "ancestors and removed: " +
                string.Join(", ", layout.IgnoredDonorBoneNames) + ".");
        }

        return new SmoNativeVisualGraphPlan(
            layout.DonorRoots.Count,
            layout.DonorVisualEntries.Count(entry =>
                entry.TypeHash == SmoClassIds.MeshData),
            layout.DonorVisualEntries.Count(entry =>
                entry.TypeHash == SmoClassIds.MaterialData),
            layout.DonorVisualEntries.Count(entry =>
                entry.TypeHash == SmoClassIds.TextureData),
            layout.TargetLeafGrafts.Count,
            layout.MatchedBoneNames,
            layout.IgnoredDonorBoneNames,
            errors,
            warnings);
    }

    public static SmoNativeVisualGraphReplaceResult Replace(
        SmoDocument target,
        SmoDocument donor,
        string outputPath,
        CancellationToken cancellationToken = default)
    {
        ArgumentNullException.ThrowIfNull(target);
        ArgumentNullException.ThrowIfNull(donor);
        ArgumentException.ThrowIfNullOrWhiteSpace(outputPath);
        cancellationToken.ThrowIfCancellationRequested();

        NativeReplacementLayout layout = BuildLayout(
            target, donor, cancellationToken);
        SmoDocument effectiveDonor = layout.Donor;
        // IDs start beyond the complete original catalog; monotonic allocation
        // needs no repeated catalog scans and cannot recycle a removed ID.
        uint nextId = target.Objects.Max(entry => entry.Id);
        var idMap = new Dictionary<uint, uint>();
        foreach (SmoObjectEntry entry in layout.DonorVisualEntries.OrderBy(entry => entry.LogicalOffset))
            idMap.Add(entry.Id, nextId = checked(nextId + 1));

        // Observe the intact graph before producing the transient, dangling-ID
        // authoring state. Only retained consumers' reference-only sites change.
        SmoDocument current = RemapRetainedTargetReferences(target, layout, idMap, cancellationToken);
        current = SmoDocument.Parse(SmoVisualForestInjector.RemoveInlineBranches(
            current, layout.TargetRoots.Select(root => root.Id)), target.SourcePath);
        if (current.HasErrors)
            throw new InvalidDataException("Removing target visual roots produced an invalid SMO.");

        cancellationToken.ThrowIfCancellationRequested();
        var branches = layout.DonorRoots.OrderBy(entry => entry.PhysicalOffset).Select(root =>
        {
            if (root.ParentIndex is not int parentIndex)
                throw new InvalidDataException($"Donor visual root {root.Id} has no owner.");
            var parent = effectiveDonor.Objects[parentIndex];
            var field = FindExactInlineField(effectiveDonor, parent, root);
            return (Root: root, PhysicalOffset: checked((int)parent.PhysicalOffset + field.Offset),
                Length: checked(field.HeaderSize + (int)field.PayloadSize));
        }).ToArray();
        var ranges = RequireReferenceTrace(effectiveDonor).CaptureRanges(effectiveDonor,
            branches.Select(branch => (branch.PhysicalOffset, branch.Length)).ToArray());
        var relocation = new Dictionary<uint, uint>(BuildExternalReferenceMap(
            target, effectiveDonor, layout, ranges, cancellationToken));
        foreach (var pair in idMap) relocation.Add(pair.Key, pair.Value);
        var destinationIds = current.Objects.Select(entry => entry.Id).Concat(idMap.Values).ToHashSet();
        SmoVisualForestAttachment[] attachments = branches.Select((branch, index) => BuildAttachment(
            effectiveDonor, branch.Root, branch.PhysicalOffset, branch.Length, ranges[index],
            layout.TargetOwnerId, layout.TargetFieldType, relocation, destinationIds)).ToArray();

        byte[] outputData = layout.AnchorFieldType is int anchorFieldType
            ? SmoVisualForestInjector.InjectAfterLastFieldType(
                current,
                layout.TargetOwnerId,
                anchorFieldType,
                attachments)
            : SmoVisualForestInjector.Inject(
                current,
                layout.TargetOwnerId,
                attachments);
        SmoDocument rebuilt = SmoDocument.Parse(outputData, target.SourcePath);
        rebuilt = RestoreMissingTargetLeafNodes(
            target,
            rebuilt,
            layout,
            ref nextId,
            cancellationToken);
        outputData = rebuilt.Data.ToArray();
        VerifyResult(target, effectiveDonor, rebuilt, layout, idMap);

        SmoVerifiedOutputInstallResult installed =
            SmoVerifiedOutputInstaller.Install(
                outputPath,
                outputData,
                temporary =>
                {
                    cancellationToken.ThrowIfCancellationRequested();
                    SmoDocument verified = SmoDocument.Load(temporary);
                    VerifyResult(target, effectiveDonor, verified, layout, idMap);
                },
                cancellationToken,
                target.SourcePath,
                donor.SourcePath);
        return new SmoNativeVisualGraphReplaceResult(
            installed.OutputPath,
            layout.DonorRoots.Count,
            layout.DonorVisualEntries.Count(entry =>
                entry.TypeHash == SmoClassIds.MeshData),
            layout.DonorVisualEntries.Count(entry =>
                entry.TypeHash == SmoClassIds.MaterialData),
            layout.DonorVisualEntries.Count(entry =>
                entry.TypeHash == SmoClassIds.TextureData),
            layout.DonorVisualEntries.Count,
            new FileInfo(installed.OutputPath).Length,
            installed.Sha256);
    }

    private static NativeReplacementLayout BuildLayout(
        SmoDocument target,
        SmoDocument donor,
        CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        if (target.HasErrors)
            throw new InvalidDataException("The target SMO has parser errors.");
        if (donor.HasErrors)
            throw new InvalidDataException("The donor SMO has parser errors.");
        SmoProductionPlatformGuard.EnsurePcWritable(target);
        SmoProductionPlatformGuard.EnsurePcWritable(donor);

        DonorSkeletonNormalization normalization = NormalizeDonorSkeleton(
            target, donor, cancellationToken);
        donor = normalization.Document;

        SmoObjectEntry[] targetRoots = FindVisualRoots(target);
        SmoObjectEntry[] donorRoots = FindVisualRoots(donor);
        if (targetRoots.Length == 0)
            throw new InvalidDataException("The target has no inline render forest.");
        if (donorRoots.Length == 0)
            throw new InvalidDataException("The donor has no inline render forest.");

        SmoObjectEntry primaryTarget = targetRoots
            .OrderByDescending(root => CountDescendantsOfType(
                target, root, SmoClassIds.MeshData))
            .ThenBy(root => root.PhysicalOffset)
            .First();
        if (primaryTarget.ParentIndex is not int targetParentIndex)
            throw new InvalidDataException("The target render forest has no owner.");
        SmoObjectEntry targetOwner = target.Objects[targetParentIndex];
        SmoDataBlockHeader targetField = FindExactInlineField(
            target, targetOwner, primaryTarget);
        int? anchorFieldType = FindStablePredecessorFieldType(
            target, targetOwner, targetField, targetRoots);

        SmoObjectEntry[] targetVisualEntries = EntriesInsideRoots(
            target, targetRoots);
        SmoObjectEntry[] donorVisualEntries = EntriesInsideRoots(
            donor, donorRoots);
        ValidateCompleteVisualCoverage(target, targetVisualEntries, "target");
        ValidateCompleteVisualCoverage(donor, donorVisualEntries, "donor");

        string[] donorBoneNames = FindSkinBoneNames(
            donor, donorVisualEntries, cancellationToken);
        HashSet<uint> removedTargetIds = targetVisualEntries
            .Select(entry => entry.Id)
            .ToHashSet();
        SmoObjectEntry[] targetLeafGrafts = FindTargetLeafGrafts(
            target,
            targetVisualEntries,
            donorVisualEntries,
            removedTargetIds);

        return new NativeReplacementLayout(
            donor,
            targetRoots,
            donorRoots,
            targetVisualEntries,
            donorVisualEntries,
            targetOwner.Id,
            targetField.FieldType,
            anchorFieldType,
            donorBoneNames,
            normalization.IgnoredBoneNames,
            removedTargetIds,
            targetLeafGrafts);
    }

    private static DonorSkeletonNormalization NormalizeDonorSkeleton(
        SmoDocument target,
        SmoDocument donor,
        CancellationToken cancellationToken)
    {
        HashSet<string> targetNodeNames = target.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.Node &&
                            !string.IsNullOrWhiteSpace(entry.Name))
            .Select(entry => entry.Name)
            .ToHashSet(StringComparer.Ordinal);
        SmoObjectEntry[] skinEntries = donor.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.Skin)
            .ToArray();
        var missingNodes = new Dictionary<uint, SmoObjectEntry>();
        foreach (SmoObjectEntry skinEntry in skinEntries)
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (!SmoSkinDecoder.TryDecode(
                    donor, skinEntry, out SmoSkin? skin, out string error) ||
                skin is null)
            {
                throw new InvalidDataException(error);
            }
            foreach (SmoSkinBone bone in skin.Bones)
            {
                SmoObjectEntry node = donor.Objects[bone.NodeObjectIndex];
                if (!targetNodeNames.Contains(node.Name))
                    missingNodes.TryAdd(node.Id, node);
            }
        }
        if (missingNodes.Count == 0)
            return new DonorSkeletonNormalization(donor, []);

        SmoNodeHierarchy hierarchy = SmoNodeHierarchy.Decode(donor);
        IReadOnlyDictionary<int, Matrix4x4> bindWorld =
            SmoSkinBindingResolver.ResolveBindWorldMatrices(donor);
        var collapses = new Dictionary<uint, DonorBoneCollapse>();
        foreach (SmoObjectEntry missing in missingNodes.Values)
        {
            cancellationToken.ThrowIfCancellationRequested();
            SmoObjectEntry fallback = FindNearestRetainedAncestor(
                donor, hierarchy, missing, targetNodeNames);
            if (!bindWorld.TryGetValue(
                    fallback.Index, out Matrix4x4 fallbackBindWorld) ||
                !Matrix4x4.Invert(
                    fallbackBindWorld, out Matrix4x4 fallbackInverseBind) ||
                !IsFinite(fallbackInverseBind))
            {
                throw new InvalidDataException(
                    $"Donor-only bone {missing.Name} falls back to " +
                    $"{fallback.Name}, but that node has no finite canonical " +
                    "inverse-bind matrix.");
            }
            collapses.Add(missing.Id, new DonorBoneCollapse(
                fallback.Id,
                missing.Name,
                fallbackInverseBind));
        }

        // Observe the intact graph before palette replacement temporarily adds
        // forward references; StripInlineCollapsedPaletteBones then relocates
        // their inline owners. The intermediate state is not a loadable file.
        SmoDocument current = RewriteDirectCollapsedBoneReferences(
            donor, collapses, cancellationToken);
        current = RewriteReferenceOnlyCollapsedPaletteEntries(
            current, collapses, cancellationToken);
        current = StripInlineCollapsedPaletteBones(
            current, collapses, cancellationToken);
        current = RemoveRemainingInlineCollapsedBones(
            current, collapses, cancellationToken);

        uint[] survivors = collapses.Keys
            .Where(id => current.Objects.Any(entry => entry.Id == id))
            .ToArray();
        if (survivors.Length > 0)
        {
            throw new InvalidDataException(
                "Donor-only bones survived skeleton normalization: " +
                string.Join(", ", survivors.Select(id => collapses[id].SourceName)) + ".");
        }
        foreach (SmoObjectEntry skinEntry in current.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.Skin))
        {
            if (!SmoSkinDecoder.TryDecode(
                    current, skinEntry, out SmoSkin? skin, out string error) ||
                skin is null)
            {
                throw new InvalidDataException(error);
            }
            foreach (SmoSkinBone bone in skin.Bones)
            {
                string name = current.Objects[bone.NodeObjectIndex].Name;
                if (!targetNodeNames.Contains(name))
                {
                    throw new InvalidDataException(
                        $"Normalized donor skin {skinEntry.Id} still references " +
                        $"bone {name}, which is absent from the target skeleton.");
                }
            }
        }

        string[] ignored = collapses.Values
            .Select(collapse => collapse.SourceName)
            .Distinct(StringComparer.Ordinal)
            .Order(StringComparer.Ordinal)
            .ToArray();
        return new DonorSkeletonNormalization(current, ignored);
    }

    private static SmoObjectEntry FindNearestRetainedAncestor(
        SmoDocument donor,
        SmoNodeHierarchy hierarchy,
        SmoObjectEntry missing,
        IReadOnlySet<string> targetNodeNames)
    {
        var visited = new HashSet<int> { missing.Index };
        int cursor = missing.Index;
        while (hierarchy.ParentsByChild.TryGetValue(
                   cursor, out IReadOnlyList<int>? parents) &&
               parents.Count == 1)
        {
            cursor = parents[0];
            if (!visited.Add(cursor))
            {
                throw new InvalidDataException(
                    $"Donor skeleton contains a cycle through {missing.Name}.");
            }
            SmoObjectEntry candidate = donor.Objects[cursor];
            if (candidate.TypeHash == SmoClassIds.Node &&
                targetNodeNames.Contains(candidate.Name))
            {
                return candidate;
            }
        }
        throw new InvalidDataException(
            $"Donor-only bone {missing.Name} has no unambiguous ancestor " +
            "present in the target skeleton.");
    }

    private static SmoDocument RewriteReferenceOnlyCollapsedPaletteEntries(
        SmoDocument donor,
        IReadOnlyDictionary<uint, DonorBoneCollapse> collapses,
        CancellationToken cancellationToken)
    {
        var transaction = new SmoMutationTransaction(donor);
        bool changed = false;
        foreach (SmoObjectEntry skinEntry in donor.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.Skin))
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (!SmoSkinDecoder.TryDecode(
                    donor, skinEntry, out SmoSkin? skin, out string error) ||
                skin is null)
            {
                throw new InvalidDataException(error);
            }
            SmoObjectField field = FindSkinPaletteField(donor, skinEntry);
            byte[] payload = field.Payload.ToArray();
            int cursor = 2 * sizeof(uint);
            bool skinChanged = false;
            foreach (SmoSkinBone bone in skin.Bones)
            {
                if (bone.InlineSerializedSize == 0 &&
                    collapses.TryGetValue(
                        bone.NodeObjectId, out DonorBoneCollapse? collapse))
                {
                    BinaryPrimitives.WriteUInt32LittleEndian(
                        payload.AsSpan(cursor), collapse.FallbackNodeId);
                    WriteMatrix(
                        payload.AsSpan(cursor + ObjectReferenceSize),
                        collapse.FallbackInverseBind);
                    skinChanged = true;
                }
                cursor = checked(cursor + ObjectReferenceSize +
                    (int)bone.InlineSerializedSize + Matrix4x4Size);
            }
            if (!skinChanged)
                continue;
            transaction.SetFieldPayload(skinEntry.Index, field.Selector, payload);
            changed = true;
        }
        if (!changed)
            return donor;
        SmoDocument result = SmoDocument.Parse(
            transaction.Commit().Data, donor.SourcePath);
        if (result.HasErrors)
            throw new InvalidDataException(
                "Remapping reference-only donor bones produced an invalid SMO.");
        return result;
    }

    private static SmoDocument RewriteDirectCollapsedBoneReferences(
        SmoDocument donor,
        IReadOnlyDictionary<uint, DonorBoneCollapse> collapses,
        CancellationToken cancellationToken)
    {
        var observed = RequireReferenceTrace(donor).CaptureRange(donor, 0, donor.Data.Length);
        var sites = observed.Sites.Where(site => site.InlineSize == 0 &&
            collapses.ContainsKey(site.ObjectId)).ToDictionary(site => site.Offset);
        var childFields = new List<(int OwnerIndex, SmoFieldSelector Selector, int Offset)>();
        var childSites = new HashSet<int>();
        var directSites = new HashSet<int>();
        foreach (var owner in donor.Objects)
        {
            cancellationToken.ThrowIfCancellationRequested();
            var fields = SmoObjectFieldReader.Read(donor, owner);
            for (int index = 0; index < fields.Count; ++index)
            {
                var field = fields[index];
                int offset = checked((int)owner.PhysicalOffset + field.RelativePayloadOffset);
                if (!sites.ContainsKey(offset)) continue;
                directSites.Add(offset);
                if (!SmoSerializedFieldRegistry.TryDescribeField(
                        owner.TypeHash, fields, index, out var descriptor) || descriptor?.Key != "node.child")
                    continue;
                childFields.Add((owner.Index, field.Selector, offset));
                childSites.Add(offset);
            }
        }
        byte[] data = donor.Data.ToArray();
        bool remapped = false;
        foreach (var site in sites.Values)
        {
            cancellationToken.ThrowIfCancellationRequested();
            // Palette IDs need their inverse-bind matrices changed together in
            // the following typed palette operation, never as direct fixups.
            if (!directSites.Contains(site.Offset) || childSites.Contains(site.Offset) || collapses.ContainsKey(site.ConsumerId)) continue;
            BinaryPrimitives.WriteUInt32LittleEndian(data.AsSpan(site.Offset), collapses[site.ObjectId].FallbackNodeId);
            remapped = true;
        }
        var current = remapped ? SmoDocument.Parse(data, donor.SourcePath) : donor;
        if (childFields.Count != 0)
        {
            // Remove the exact schema-backed child fields in one transaction.
            // Equal-sized opaque fields and nested non-child references survive.
            var transaction = new SmoMutationTransaction(current);
            foreach (var child in childFields.OrderByDescending(value => value.Offset))
                transaction.RemoveField(child.OwnerIndex, child.Selector);
            current = SmoDocument.Parse(transaction.Commit().Data, donor.SourcePath);
        }
        if (current.HasErrors)
            throw new InvalidDataException("Normalizing observed donor bone references produced an invalid SMO.");
        return current;
    }

    private static SmoDocument StripInlineCollapsedPaletteBones(
        SmoDocument donor,
        IReadOnlyDictionary<uint, DonorBoneCollapse> collapses,
        CancellationToken cancellationToken)
    {
        SmoDocument current = donor;
        uint[] skinIds = donor.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.Skin)
            .Select(entry => entry.Id)
            .ToArray();
        foreach (uint skinId in skinIds)
        {
            cancellationToken.ThrowIfCancellationRequested();
            SmoObjectEntry skinEntry = current.Objects.Single(entry =>
                entry.Id == skinId);
            if (!SmoSkinDecoder.TryDecode(
                    current, skinEntry, out SmoSkin? skin, out string error) ||
                skin is null)
            {
                throw new InvalidDataException(error);
            }
            SmoObjectField field = FindSkinPaletteField(current, skinEntry);
            ReadOnlySpan<byte> source = field.Payload.Span;
            var segments = new List<SkinPaletteSegment>(skin.Bones.Count);
            int cursor = 2 * sizeof(uint);
            foreach (SmoSkinBone bone in skin.Bones)
            {
                int length = checked(ObjectReferenceSize +
                    (int)bone.InlineSerializedSize + Matrix4x4Size);
                segments.Add(new SkinPaletteSegment(bone, cursor, length));
                cursor = checked(cursor + length);
            }
            if (cursor != source.Length)
                throw new InvalidDataException(
                    $"Skin {skinId} palette traversal is incomplete.");
            SkinPaletteSegment[] collapsedInline = segments.Where(segment =>
                    segment.Bone.InlineSerializedSize != 0 &&
                    collapses.ContainsKey(segment.Bone.NodeObjectId))
                .ToArray();
            if (collapsedInline.Length == 0)
                continue;

            var relocationByDestination = new Dictionary<int, SkinPaletteSegment>();
            var relocatedSourceStarts = new HashSet<int>();
            foreach (IGrouping<uint, SkinPaletteSegment> fallbackGroup in
                     collapsedInline.GroupBy(segment =>
                         collapses[segment.Bone.NodeObjectId].FallbackNodeId))
            {
                SkinPaletteSegment destination = fallbackGroup
                    .OrderBy(segment => segment.Start)
                    .First();
                SkinPaletteSegment? fallbackSource = segments.SingleOrDefault(segment =>
                    segment.Bone.NodeObjectId == fallbackGroup.Key &&
                    segment.Bone.InlineSerializedSize != 0);
                if (fallbackSource is null || fallbackSource.Start < destination.Start)
                    continue;
                relocationByDestination.Add(destination.Start, fallbackSource);
                relocatedSourceStarts.Add(fallbackSource.Start);
            }

            var removedIds = new HashSet<uint>();
            foreach (SkinPaletteSegment segment in collapsedInline)
            {
                SmoObjectEntry root = current.Objects[segment.Bone.NodeObjectIndex];
                SmoObjectEntry[] subtree = current.Objects.Where(entry =>
                        entry.PhysicalOffset >= root.PhysicalOffset &&
                        entry.PhysicalEnd <= root.PhysicalEnd)
                    .ToArray();
                SmoObjectEntry[] retained = subtree.Where(entry =>
                        !collapses.ContainsKey(entry.Id))
                    .ToArray();
                if (retained.Length > 0)
                {
                    throw new InvalidDataException(
                        $"Donor-only bone {root.Name} physically owns retained " +
                        "objects and cannot be discarded safely.");
                }
                removedIds.UnionWith(subtree.Select(entry => entry.Id));
            }

            using var payload = new MemoryStream(source.Length);
            payload.Write(source[..(2 * sizeof(uint))]);
            var relocatedOffsets = new Dictionary<uint, int>();
            foreach (SkinPaletteSegment segment in segments)
            {
                int destinationStart = checked((int)payload.Position);
                if (relocationByDestination.TryGetValue(
                        segment.Start, out SkinPaletteSegment? relocatedSource))
                {
                    CopyPaletteSegment(
                        current,
                        skinEntry,
                        field,
                        source,
                        relocatedSource,
                        payload,
                        relocatedOffsets,
                        destinationStart);
                }
                else if (collapses.TryGetValue(
                             segment.Bone.NodeObjectId,
                             out DonorBoneCollapse? collapse))
                {
                    WriteCollapsedPaletteReference(payload, collapse);
                }
                else if (relocatedSourceStarts.Contains(segment.Start))
                {
                    WriteCollapsedPaletteReference(
                        payload,
                        new DonorBoneCollapse(
                            segment.Bone.NodeObjectId,
                            current.Objects[segment.Bone.NodeObjectIndex].Name,
                            segment.Bone.InverseBindMatrix));
                }
                else
                {
                    CopyPaletteSegment(
                        current,
                        skinEntry,
                        field,
                        source,
                        segment,
                        payload,
                        relocatedOffsets,
                        destinationStart);
                }
            }
            current = SmoDocument.Parse(
                SmoVisualForestInjector.ReplaceDirectFieldPayloadAndRelocateInlineObjects(
                    current,
                    skinId,
                    field.Selector,
                    payload.ToArray(),
                    removedIds,
                    relocatedOffsets),
                donor.SourcePath);
            if (current.HasErrors)
                throw new InvalidDataException(
                    $"Removing inline donor-only bones from skin {skinId} " +
                    "produced an invalid SMO.");
        }
        return current;
    }

    private static void CopyPaletteSegment(
        SmoDocument document,
        SmoObjectEntry skinEntry,
        SmoObjectField field,
        ReadOnlySpan<byte> source,
        SkinPaletteSegment segment,
        Stream destination,
        IDictionary<uint, int> relocatedOffsets,
        int destinationStart)
    {
        destination.Write(source.Slice(segment.Start, segment.Length));
        if (segment.Bone.InlineSerializedSize == 0)
            return;
        long objectPhysicalStart =
            skinEntry.PhysicalOffset + field.RelativePayloadOffset +
            segment.Start + ObjectReferenceSize;
        long objectPhysicalEnd = objectPhysicalStart +
            segment.Bone.InlineSerializedSize;
        foreach (SmoObjectEntry entry in document.Objects.Where(entry =>
                     entry.PhysicalOffset >= objectPhysicalStart &&
                     entry.PhysicalEnd <= objectPhysicalEnd))
        {
            int relative = checked((int)(entry.PhysicalOffset - objectPhysicalStart));
            relocatedOffsets.Add(
                entry.Id,
                checked(destinationStart + ObjectReferenceSize + relative));
        }
    }

    private static void WriteCollapsedPaletteReference(
        Stream destination,
        DonorBoneCollapse collapse)
    {
        byte[] reference = new byte[ObjectReferenceSize + Matrix4x4Size];
        BinaryPrimitives.WriteUInt32LittleEndian(
            reference, collapse.FallbackNodeId);
        BinaryPrimitives.WriteUInt32LittleEndian(
            reference.AsSpan(sizeof(uint)), 0);
        WriteMatrix(
            reference.AsSpan(ObjectReferenceSize),
            collapse.FallbackInverseBind);
        destination.Write(reference);
    }

    private static SmoDocument RemoveRemainingInlineCollapsedBones(
        SmoDocument donor,
        IReadOnlyDictionary<uint, DonorBoneCollapse> collapses,
        CancellationToken cancellationToken)
    {
        SmoDocument current = donor;
        while (true)
        {
            SmoObjectEntry[] remaining = current.Objects
                .Where(entry => collapses.ContainsKey(entry.Id))
                .ToArray();
            if (remaining.Length == 0)
                return current;
            SmoObjectEntry[] roots = remaining
                .Where(entry => entry.ParentIndex is not int parentIndex ||
                    !collapses.ContainsKey(current.Objects[parentIndex].Id))
                .OrderByDescending(entry => entry.PhysicalOffset)
                .ToArray();
            if (roots.Length == 0)
                throw new InvalidDataException(
                    "Donor-only bone definitions form an unremovable cycle.");
            foreach (SmoObjectEntry sourceRoot in roots)
            {
                cancellationToken.ThrowIfCancellationRequested();
                SmoObjectEntry? currentRoot = current.Objects.SingleOrDefault(entry =>
                    entry.Id == sourceRoot.Id);
                if (currentRoot is null)
                    continue;
                SmoObjectEntry root = currentRoot;
                if (root.ParentIndex is not int parentIndex)
                    throw new InvalidDataException(
                        $"Donor-only bone {root.Name} has no inline owner.");
                SmoObjectEntry[] retainedDescendants = current.Objects.Where(entry =>
                        entry.Id != root.Id &&
                        entry.PhysicalOffset >= root.PhysicalOffset &&
                        entry.PhysicalEnd <= root.PhysicalEnd &&
                        !collapses.ContainsKey(entry.Id))
                    .ToArray();
                if (retainedDescendants.Length > 0)
                {
                    throw new InvalidDataException(
                        $"Donor-only bone {root.Name} physically owns retained " +
                        "objects and cannot be discarded safely.");
                }
                uint ownerId = current.Objects[parentIndex].Id;
                byte[] removedBranch;
                try
                {
                    removedBranch = SmoVisualForestInjector.RemoveInlineBranch(
                        current, ownerId, root.Id);
                }
                catch (InvalidOperationException exception)
                {
                    SmoObjectEntry owner = current.Objects[parentIndex];
                    ReadOnlySpan<byte> ownerBytes = ObjectBytes(current, owner);
                    var parsedFields = new List<string>();
                    int fieldCursor = ObjectSignatureSize;
                    while (fieldCursor < ownerBytes.Length &&
                           SmoDataBlockReader.TryReadHeader(
                               ownerBytes, fieldCursor, out SmoDataBlockHeader field))
                    {
                        parsedFields.Add(
                            $"{field.FieldType}:{field.PayloadSize}@{field.PayloadOffset}");
                        fieldCursor = checked((int)field.PayloadEnd);
                    }
                    string fields = string.Join(", ", parsedFields);
                    throw new InvalidDataException(
                        $"Cannot discard donor-only bone {root.Name}: owner " +
                        $"{owner.Name} fields are [{fields}], child size is " +
                        $"{root.SerializedSize} at 0x{root.PhysicalOffset:X}; " +
                        $"field parsing stopped at {fieldCursor}/{ownerBytes.Length}.",
                        exception);
                }
                current = SmoDocument.Parse(removedBranch, donor.SourcePath);
                if (current.HasErrors)
                    throw new InvalidDataException(
                        $"Removing donor-only bone {root.Name} produced an invalid SMO.");
            }
        }
    }

    private static void WriteMatrix(Span<byte> destination, Matrix4x4 value)
    {
        if (destination.Length < Matrix4x4Size)
            throw new ArgumentException("Matrix destination is truncated.");
        float[] values =
        [
            value.M11, value.M12, value.M13, value.M14,
            value.M21, value.M22, value.M23, value.M24,
            value.M31, value.M32, value.M33, value.M34,
            value.M41, value.M42, value.M43, value.M44
        ];
        for (int index = 0; index < values.Length; index++)
        {
            BinaryPrimitives.WriteSingleLittleEndian(
                destination[(index * sizeof(float))..], values[index]);
        }
    }

    private static bool IsFinite(Matrix4x4 value) =>
        float.IsFinite(value.M11) && float.IsFinite(value.M12) &&
        float.IsFinite(value.M13) && float.IsFinite(value.M14) &&
        float.IsFinite(value.M21) && float.IsFinite(value.M22) &&
        float.IsFinite(value.M23) && float.IsFinite(value.M24) &&
        float.IsFinite(value.M31) && float.IsFinite(value.M32) &&
        float.IsFinite(value.M33) && float.IsFinite(value.M34) &&
        float.IsFinite(value.M41) && float.IsFinite(value.M42) &&
        float.IsFinite(value.M43) && float.IsFinite(value.M44);

    private static SmoObjectEntry[] FindTargetLeafGrafts(
        SmoDocument target,
        IReadOnlyCollection<SmoObjectEntry> targetVisualEntries,
        IReadOnlyCollection<SmoObjectEntry> donorVisualEntries,
        IReadOnlySet<uint> removedTargetIds)
    {
        static string Key(SmoObjectEntry entry) =>
            $"{entry.TypeHash:X8}:{Convert.ToHexString(entry.RawName.Span)}";

        HashSet<string> available = donorVisualEntries
            .Where(entry => entry.TypeHash == SmoClassIds.Node)
            .Concat(target.Objects.Where(entry =>
                entry.TypeHash == SmoClassIds.Node &&
                !removedTargetIds.Contains(entry.Id)))
            .Select(Key)
            .ToHashSet(StringComparer.Ordinal);
        SmoObjectEntry[] allMissing = targetVisualEntries
            .Where(entry => entry.TypeHash == SmoClassIds.Node &&
                            !available.Contains(Key(entry)))
            .OrderBy(entry => entry.PhysicalOffset)
            .ToArray();
        if (allMissing.Length == 0)
            return [];

        SmoNodeHierarchy hierarchy = SmoNodeHierarchy.Decode(target);
        var targetDeformIndices = new HashSet<int>();
        foreach (SmoObjectEntry skinEntry in target.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.Skin))
        {
            if (!SmoSkinDecoder.TryDecode(
                    target, skinEntry, out SmoSkin? skin, out string error) ||
                skin is null)
            {
                throw new InvalidDataException(error);
            }
            targetDeformIndices.UnionWith(
                skin.Bones.Select(bone => bone.NodeObjectIndex));
        }
        HashSet<int> absentDeformIndices = allMissing
            .Where(entry => targetDeformIndices.Contains(entry.Index))
            .Select(entry => entry.Index)
            .ToHashSet();
        SmoObjectEntry[] missing = allMissing.Where(entry =>
                !targetDeformIndices.Contains(entry.Index) &&
                !HasAncestorInSet(hierarchy, entry.Index, absentDeformIndices))
            .ToArray();
        foreach (SmoObjectEntry leaf in missing)
        {
            if (hierarchy.ChildrenByParent.TryGetValue(
                    leaf.Index, out IReadOnlyList<int>? children) &&
                children.Count > 0)
            {
                throw new InvalidDataException(
                    $"Target-only animation node {leaf.Name} is not a leaf; " +
                    "native donor grafting cannot preserve its subtree safely.");
            }
            if (!hierarchy.ParentsByChild.TryGetValue(
                    leaf.Index, out IReadOnlyList<int>? parents) ||
                parents.Count != 1)
            {
                throw new InvalidDataException(
                    $"Target-only animation node {leaf.Name} has " +
                    $"{parents?.Count ?? 0} logical parents.");
            }
            SmoObjectEntry parent = target.Objects[parents[0]];
            if (!available.Contains(Key(parent)))
            {
                throw new InvalidDataException(
                    $"Target-only animation node {leaf.Name} has unavailable " +
                    $"parent {parent.Name} in the donor graph.");
            }
            if (leaf.ParentIndex != parent.Index)
            {
                throw new InvalidDataException(
                    $"Target-only animation node {leaf.Name} is not serialized " +
                    "inline by its logical parent.");
            }
            _ = FindExactInlineField(target, parent, leaf);
        }
        return missing;
    }

    private static bool HasAncestorInSet(
        SmoNodeHierarchy hierarchy,
        int objectIndex,
        IReadOnlySet<int> candidates)
    {
        var visited = new HashSet<int> { objectIndex };
        int cursor = objectIndex;
        while (hierarchy.ParentsByChild.TryGetValue(
                   cursor, out IReadOnlyList<int>? parents) &&
               parents.Count == 1)
        {
            cursor = parents[0];
            if (!visited.Add(cursor))
                throw new InvalidDataException(
                    "Target node hierarchy contains a cycle.");
            if (candidates.Contains(cursor))
                return true;
        }
        return false;
    }

    private static SmoDocument RestoreMissingTargetLeafNodes(
        SmoDocument target,
        SmoDocument current,
        NativeReplacementLayout layout,
        ref uint nextId,
        CancellationToken cancellationToken)
    {
        if (layout.TargetLeafGrafts.Count == 0)
            return current;
        SmoNodeHierarchy targetHierarchy = SmoNodeHierarchy.Decode(target);
        foreach (SmoObjectEntry sourceLeaf in layout.TargetLeafGrafts)
        {
            cancellationToken.ThrowIfCancellationRequested();
            int sourceParentIndex =
                targetHierarchy.ParentsByChild[sourceLeaf.Index].Single();
            SmoObjectEntry sourceParent = target.Objects[sourceParentIndex];
            SmoObjectEntry[] outputParents = current.Objects.Where(entry =>
                    entry.TypeHash == sourceParent.TypeHash &&
                    entry.RawName.Span.SequenceEqual(sourceParent.RawName.Span))
                .ToArray();
            if (outputParents.Length != 1)
            {
                throw new InvalidDataException(
                    $"Target-only animation node {sourceLeaf.Name} has " +
                    $"{outputParents.Length} exact output parents named " +
                    $"{sourceParent.Name}.");
            }

            nextId = checked(nextId + 1);
            SmoDataBlockHeader sourceField = FindExactInlineField(
                target, sourceParent, sourceLeaf);
            ReadOnlySpan<byte> parentBytes = ObjectBytes(target, sourceParent);
            byte[] fieldData = parentBytes.Slice(
                sourceField.Offset,
                checked(sourceField.HeaderSize + (int)sourceField.PayloadSize))
                .ToArray();
            int idOffset = sourceField.PayloadOffset - sourceField.Offset;
            BinaryPrimitives.WriteUInt32LittleEndian(
                fieldData.AsSpan(idOffset), nextId);
            var attachment = new SmoVisualForestAttachment(
                outputParents[0].Id,
                fieldData,
                [new SmoVisualForestEntry(
                    nextId,
                    sourceLeaf.RawName.ToArray(),
                    sourceLeaf.TypeHash,
                    checked(idOffset + ObjectReferenceSize),
                    sourceLeaf.SerializedSize)]);
            current = SmoDocument.Parse(
                SmoVisualForestInjector.Inject(
                    current, outputParents[0].Id, [attachment]),
                target.SourcePath);
            if (current.HasErrors)
            {
                throw new InvalidDataException(
                    $"Restoring target animation node {sourceLeaf.Name} " +
                    "produced an invalid SMO.");
            }
        }
        return current;
    }

    private static SmoFileReferenceTrace RequireReferenceTrace(SmoDocument document)
    {
        var loaded = SmoLoadedResources.Get(document);
        return loaded.ReferenceTrace ?? throw new InvalidDataException(
            loaded.ReferenceTraceIssue ?? loaded.LoadIssue ?? "Native visual transfer requires actual reader reference observations.");
    }

    private static SmoDocument RemapRetainedTargetReferences(
        SmoDocument target,
        NativeReplacementLayout layout,
        IReadOnlyDictionary<uint, uint> donorIdMap,
        CancellationToken cancellationToken)
    {
        var observed = RequireReferenceTrace(target).CaptureRange(target, 0, target.Data.Length);
        var sites = observed.Sites.Where(site => site.InlineSize == 0 &&
            layout.RemovedTargetIds.Contains(site.ObjectId) &&
            !layout.RemovedTargetIds.Contains(site.ConsumerId)).ToArray();
        if (sites.Length == 0) return target;
        var referenceMap = new Dictionary<uint, uint>();
        var targetById = target.Objects.ToDictionary(entry => entry.Id);
        foreach (var site in sites)
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (referenceMap.ContainsKey(site.ObjectId)) continue;
            var removed = targetById[site.ObjectId];
            var matches = layout.DonorVisualEntries.Where(entry => entry.TypeHash == removed.TypeHash &&
                entry.RawName.Span.SequenceEqual(removed.RawName.Span)).ToArray();
            if (matches.Length != 1)
                throw new InvalidDataException($"Retained target object {site.ConsumerId} references removed visual " +
                    $"object {site.ObjectId} ({removed.Name}), but the donor forest has {matches.Length} exact replacements.");
            referenceMap.Add(site.ObjectId, donorIdMap[matches[0].Id]);
        }
        byte[] data = target.Data.ToArray();
        // Host fixups at sealed reader offsets, never a field-size classifier.
        // Inline identities remain intact for the following bulk removal.
        foreach (var site in sites)
            BinaryPrimitives.WriteUInt32LittleEndian(data.AsSpan(site.Offset), referenceMap[site.ObjectId]);
        var remapped = SmoDocument.Parse(data, target.SourcePath);
        if (remapped.HasErrors)
            throw new InvalidDataException("Remapping retained target references produced an invalid SMO.");
        return remapped;
    }

    private static SmoObjectEntry[] FindVisualRoots(SmoDocument document)
    {
        SmoObjectEntry[] renderNodes = document.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.RenderNode &&
                            CountDescendantsOfType(
                                document, entry, SmoClassIds.MeshData) > 0)
            .ToArray();
        HashSet<int> renderIndices = renderNodes.Select(entry => entry.Index).ToHashSet();
        return renderNodes
            .Where(entry => !HasAncestor(entry, document, renderIndices))
            .OrderBy(entry => entry.PhysicalOffset)
            .ToArray();
    }

    private static bool HasAncestor(
        SmoObjectEntry entry,
        SmoDocument document,
        IReadOnlySet<int> candidates)
    {
        int? parent = entry.ParentIndex;
        while (parent is int index)
        {
            if (candidates.Contains(index))
                return true;
            parent = document.Objects[index].ParentIndex;
        }
        return false;
    }

    private static int CountDescendantsOfType(
        SmoDocument document,
        SmoObjectEntry root,
        uint typeHash) => document.Objects.Count(entry =>
            entry.TypeHash == typeHash && IsInside(root, entry));

    private static SmoObjectEntry[] EntriesInsideRoots(
        SmoDocument document,
        IReadOnlyList<SmoObjectEntry> roots) => document.Objects
        .Where(entry => roots.Any(root => IsInside(root, entry)))
        .OrderBy(entry => entry.PhysicalOffset)
        .ThenByDescending(entry => entry.SerializedSize)
        .ToArray();

    private static bool IsInside(SmoObjectEntry root, SmoObjectEntry entry) =>
        entry.PhysicalOffset >= root.PhysicalOffset &&
        entry.PhysicalEnd <= root.PhysicalEnd;

    private static void ValidateCompleteVisualCoverage(
        SmoDocument document,
        IReadOnlyCollection<SmoObjectEntry> entries,
        string role)
    {
        HashSet<int> covered = entries.Select(entry => entry.Index).ToHashSet();
        SmoObjectEntry[] outside = document.Objects.Where(entry =>
                entry.TypeHash is SmoClassIds.MeshData or
                    SmoClassIds.MaterialData or SmoClassIds.TextureData &&
                !covered.Contains(entry.Index))
            .ToArray();
        if (outside.Length > 0)
        {
            throw new InvalidDataException(
                $"The {role} has visual resources outside its render forest: " +
                string.Join(", ", outside.Select(entry =>
                    $"[{entry.Index}] {entry.Name}")) + ".");
        }
    }

    private static string[] FindSkinBoneNames(
        SmoDocument donor,
        IReadOnlyCollection<SmoObjectEntry> donorEntries,
        CancellationToken cancellationToken)
    {
        HashSet<int> included = donorEntries.Select(entry => entry.Index).ToHashSet();
        var names = new HashSet<string>(StringComparer.Ordinal);
        foreach (SmoObjectEntry skinEntry in donorEntries.Where(entry =>
                     entry.TypeHash == SmoClassIds.Skin))
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (!SmoSkinDecoder.TryDecode(
                    donor, skinEntry, out SmoSkin? skin, out string error) ||
                skin is null)
            {
                throw new InvalidDataException(error);
            }
            foreach (SmoSkinBone bone in skin.Bones)
            {
                SmoObjectEntry node = donor.Objects[bone.NodeObjectIndex];
                if (!included.Contains(node.Index))
                {
                    // External references are supported and remapped separately,
                    // but their names still participate in compatibility checks.
                }
                if (string.IsNullOrWhiteSpace(node.Name))
                    throw new InvalidDataException(
                        $"Donor skin [{skinEntry.Index}] has an unnamed bone.");
                names.Add(node.Name);
            }
        }
        return names.Order(StringComparer.Ordinal).ToArray();
    }

    private static IReadOnlyDictionary<uint, uint> BuildExternalReferenceMap(
        SmoDocument target,
        SmoDocument donor,
        NativeReplacementLayout layout,
        IReadOnlyList<SmoFileReferenceRange> ranges,
        CancellationToken cancellationToken)
    {
        var internalIds = layout.DonorVisualEntries.Select(entry => entry.Id).ToHashSet();
        var donorById = donor.Objects.ToDictionary(entry => entry.Id);
        var result = new Dictionary<uint, uint>();
        foreach (uint id in ranges.SelectMany(range => range.Sites).Select(site => site.ObjectId).Distinct())
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (id == 0 || internalIds.Contains(id)) continue;
            if (!donorById.TryGetValue(id, out var reference))
                throw new InvalidDataException($"Donor forest references unknown object {id}.");
            var candidates = target.Objects.Where(candidate => !layout.RemovedTargetIds.Contains(candidate.Id) &&
                candidate.TypeHash == reference.TypeHash && candidate.RawName.Span.SequenceEqual(reference.RawName.Span)).ToArray();
            if (candidates.Length != 1)
                throw new InvalidDataException($"External donor reference {id} ({reference.Name}) has " +
                    $"{candidates.Length} exact matches in the retained target graph.");
            result.Add(id, candidates[0].Id);
        }
        return result;
    }

    private static SmoVisualForestAttachment BuildAttachment(
        SmoDocument donor, SmoObjectEntry root, int fieldPhysical, int fieldLength,
        SmoFileReferenceRange range, uint targetOwnerId, int targetFieldType,
        IReadOnlyDictionary<uint, uint> relocation, IReadOnlySet<uint> destinationIds)
    {
        var entries = donor.Objects.Where(entry => IsInside(root, entry))
            .OrderBy(entry => entry.PhysicalOffset).ThenByDescending(entry => entry.SerializedSize).ToArray();
        foreach (var entry in entries)
        {
            int relative = checked((int)entry.PhysicalOffset - fieldPhysical);
            if (!range.Objects.Any(value => value.ObjectId == entry.Id && value.RelativeOffset == relative &&
                    value.ClassId == entry.TypeHash && value.SerializedSize == entry.SerializedSize) ||
                !range.Sites.Any(site => site.Offset == relative - ObjectReferenceSize &&
                    site.ObjectId == entry.Id && site.InlineSize == entry.SerializedSize))
                throw new InvalidDataException($"Donor visual object {entry.Id} lacks observed inline identity/coverage.");
        }
        byte[] data = range.Relocate(donor.Data.Span.Slice(fieldPhysical, fieldLength), relocation, destinationIds);
        if ((uint)targetFieldType > 0x1F)
            throw new InvalidDataException("Target visual field type is not representable.");
        data[0] = checked((byte)((data[0] & 0xE0) | targetFieldType));
        return new SmoVisualForestAttachment(targetOwnerId, data,
            entries.Select(entry => new SmoVisualForestEntry(relocation[entry.Id], entry.RawName.ToArray(),
                entry.TypeHash, checked((int)entry.PhysicalOffset - fieldPhysical), entry.SerializedSize)).ToArray());
    }

    private static SmoObjectField FindSkinPaletteField(
        SmoDocument document,
        SmoObjectEntry skinEntry)
    {
        IReadOnlyList<SmoObjectField> fields =
            SmoObjectFieldReader.Read(document, skinEntry);
        int firstTerminator = FindTerminator(fields, 0);
        int secondTerminator = firstTerminator < 0
            ? -1
            : FindTerminator(fields, firstTerminator + 1);
        if (secondTerminator < 0 || secondTerminator + 1 >= fields.Count)
            throw new InvalidDataException(
                $"Skin {skinEntry.Id} has no palette section.");
        SmoObjectField palette = fields[secondTerminator + 1];
        if (palette.FieldType != 0 || palette.PayloadSize < 2 * sizeof(uint))
            throw new InvalidDataException(
                $"Skin {skinEntry.Id} has an invalid palette field.");
        return palette;
    }

    private static int FindTerminator(
        IReadOnlyList<SmoObjectField> fields,
        int start)
    {
        for (int index = start; index < fields.Count; index++)
        {
            if (fields[index].FieldType == 0 && fields[index].PayloadSize == 0)
                return index;
        }
        return -1;
    }

    private static void VerifyResult(
        SmoDocument target,
        SmoDocument donor,
        SmoDocument output,
        NativeReplacementLayout layout,
        IReadOnlyDictionary<uint, uint> idMap)
    {
        if (output.HasErrors)
            throw new InvalidDataException("Native visual replacement has parser errors.");
        foreach (uint oldId in layout.RemovedTargetIds)
        {
            if (output.Objects.Any(entry => entry.Id == oldId))
            {
                throw new InvalidDataException(
                    $"Old target visual object ID {oldId} survived replacement.");
            }
        }
        VerifyNoReferencesToRemovedTargetObjects(output, layout.RemovedTargetIds);
        foreach (SmoObjectEntry donorEntry in layout.DonorVisualEntries)
        {
            SmoObjectEntry outputEntry = output.Objects.Single(entry =>
                entry.Id == idMap[donorEntry.Id]);
            if (outputEntry.TypeHash != donorEntry.TypeHash ||
                !outputEntry.RawName.Span.SequenceEqual(donorEntry.RawName.Span))
            {
                throw new InvalidDataException(
                    $"Copied donor object {donorEntry.Id} changed catalog identity.");
            }
            if (donorEntry.TypeHash is SmoClassIds.MeshData or
                    SmoClassIds.TextureData &&
                !ObjectBytes(output, outputEntry).SequenceEqual(
                    ObjectBytes(donor, donorEntry)))
            {
                throw new InvalidDataException(
                    $"Donor resource {donorEntry.Id} ({donorEntry.Name}) was modified.");
            }
        }
        int donorMeshes = layout.DonorVisualEntries.Count(entry =>
            entry.TypeHash == SmoClassIds.MeshData);
        int donorTextures = layout.DonorVisualEntries.Count(entry =>
            entry.TypeHash == SmoClassIds.TextureData);
        if (output.Objects.Count(entry => entry.TypeHash == SmoClassIds.MeshData) !=
                donorMeshes ||
            output.Objects.Count(entry => entry.TypeHash == SmoClassIds.TextureData) !=
                donorTextures)
        {
            throw new InvalidDataException(
                "Output mesh/texture counts differ from the donor render forest.");
        }
        foreach (SmoObjectEntry meshEntry in output.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.MeshData))
        {
            _ = SmoMeshDecoder.Decode(output, meshEntry);
        }
        foreach (SmoObjectEntry skinEntry in output.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.Skin))
        {
            if (!SmoSkinDecoder.TryDecode(
                    output,
                    skinEntry,
                    out SmoSkin? skin,
                    out string error) || skin is null)
            {
                throw new InvalidDataException(error);
            }
        }
        foreach (SmoObjectEntry textureEntry in output.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.TextureData))
        {
            if (!SmoTextureDecoder.TryDecode(
                    output,
                    textureEntry,
                    out SmoTexture? texture,
                    out string error) || texture is null)
            {
                throw new InvalidDataException(error);
            }
        }
    }

    private static void VerifyNoReferencesToRemovedTargetObjects(
        SmoDocument output, IReadOnlySet<uint> removedIds)
    {
        var observed = RequireReferenceTrace(output).CaptureRange(output, 0, output.Data.Length);
        foreach (var site in observed.Sites)
            if (removedIds.Contains(site.ObjectId))
                throw new InvalidDataException($"Output object {site.ConsumerId} still references removed target " +
                    $"visual object {site.ObjectId}.");
    }

    private static int? FindStablePredecessorFieldType(
        SmoDocument document,
        SmoObjectEntry owner,
        SmoDataBlockHeader visualField,
        IReadOnlyList<SmoObjectEntry> visualRoots)
    {
        SmoDataBlockHeader[] fields = ReadDirectFields(ObjectBytes(document, owner));
        int index = Array.FindIndex(fields, field => field.Offset == visualField.Offset);
        if (index <= 0)
            return null;
        HashSet<int> visualOffsets = visualRoots
            .Where(root => root.ParentIndex == owner.Index)
            .Select(root => FindExactInlineField(document, owner, root).Offset)
            .ToHashSet();
        for (int candidate = index - 1; candidate >= 0; candidate--)
        {
            if (!visualOffsets.Contains(fields[candidate].Offset))
                return fields[candidate].FieldType;
        }
        return null;
    }

    private static SmoDataBlockHeader FindExactInlineField(
        SmoDocument document,
        SmoObjectEntry owner,
        SmoObjectEntry child)
    {
        ReadOnlySpan<byte> bytes = ObjectBytes(document, owner);
        foreach (SmoDataBlockHeader field in ReadDirectFields(bytes))
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
            $"Object {child.Id} has no exact inline field in owner {owner.Id}.");
    }

    private static SmoDataBlockHeader[] ReadDirectFields(ReadOnlySpan<byte> bytes)
    {
        var fields = new List<SmoDataBlockHeader>();
        int offset = ObjectSignatureSize;
        while (offset < bytes.Length &&
               SmoDataBlockReader.TryReadHeader(
                   bytes, offset, out SmoDataBlockHeader field))
        {
            fields.Add(field);
            offset = checked((int)field.PayloadEnd);
        }
        if (offset != bytes.Length)
            throw new InvalidDataException("SMO object field stream is incomplete.");
        return fields.ToArray();
    }

    private static ReadOnlySpan<byte> ObjectBytes(
        SmoDocument document,
        SmoObjectEntry entry) => document.Data.Span.Slice(
            checked((int)entry.PhysicalOffset),
            checked((int)entry.SerializedSize));

    private sealed record NativeReplacementLayout(
        SmoDocument Donor,
        IReadOnlyList<SmoObjectEntry> TargetRoots,
        IReadOnlyList<SmoObjectEntry> DonorRoots,
        IReadOnlyList<SmoObjectEntry> TargetVisualEntries,
        IReadOnlyList<SmoObjectEntry> DonorVisualEntries,
        uint TargetOwnerId,
        int TargetFieldType,
        int? AnchorFieldType,
        IReadOnlyList<string> MatchedBoneNames,
        IReadOnlyList<string> IgnoredDonorBoneNames,
        IReadOnlySet<uint> RemovedTargetIds,
        IReadOnlyList<SmoObjectEntry> TargetLeafGrafts);

    private sealed record DonorSkeletonNormalization(
        SmoDocument Document,
        IReadOnlyList<string> IgnoredBoneNames);

    private sealed record DonorBoneCollapse(
        uint FallbackNodeId,
        string SourceName,
        Matrix4x4 FallbackInverseBind);

    private sealed record SkinPaletteSegment(
        SmoSkinBone Bone,
        int Start,
        int Length);
}
