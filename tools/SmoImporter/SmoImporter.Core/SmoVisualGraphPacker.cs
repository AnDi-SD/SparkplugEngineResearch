using System.Buffers.Binary;
using System.Numerics;
using SmoViewer.Core;

namespace SmoImporter.Core;

internal sealed record SmoPackedVisualGraph(
    byte[] Data,
    int PackedMeshCount,
    int PackedTextureCount,
    int DisabledTargetMeshCount,
    IReadOnlySet<uint> PackedObjectIds,
    IReadOnlySet<uint> PackedMeshIds,
    IReadOnlySet<uint> PackedTextureIds);

/// <summary>
/// Adds the donor's complete visual forest to compatible target render/node
/// anchors. Target object IDs and its unknown service graph stay intact. Donor
/// skin palettes become reference-only palettes which point at the target
/// skeleton by bone name, while donor render, material, texture, UV and mesh
/// objects keep their serialized payloads apart from confirmed FFPS references.
/// </summary>
internal static class SmoVisualGraphPacker
{
    private const int ObjectSignatureSize = 8;
    private const int ObjectReferenceSize = 8;
    private const uint SharedVisualHelperClassId = 0x7AC95AEC;
    private const uint TextureSequenceClassId = 0x16FB0E47;

    public static SmoPackedVisualGraph Pack(SmoDocument target, SmoDocument donor)
    {
        ArgumentNullException.ThrowIfNull(target);
        ArgumentNullException.ThrowIfNull(donor);

        SmoBoneRemapPlan boneRemap = SmoBoneMappingPlanner.Build(target, donor);
        if (boneRemap.Errors.Count > 0)
            throw new InvalidOperationException(string.Join(" ", boneRemap.Errors));

        SmoObjectEntry targetRender = FindPrimaryRenderNode(target);
        SmoObjectEntry donorPrimaryRender = FindPrimaryRenderNode(donor);
        SmoObjectEntry[] donorSkins = donor.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.Skin)
            .OrderBy(entry => entry.LogicalOffset)
            .ToArray();
        if (donorSkins.Length == 0)
            throw new InvalidOperationException("The donor has no spSkin branches.");

        SmoObjectEntry[] donorRigidRoots = FindRigidVisualRoots(donor);
        SmoObjectEntry[] donorSecondaryRenders = donor.Objects
            .Where(render => render.TypeHash == SmoClassIds.RenderNode &&
                             render.Index != donorPrimaryRender.Index &&
                             donor.Objects.Any(skin =>
                                 skin.TypeHash == SmoClassIds.Skin &&
                                 skin.ParentIndex == render.Index))
            .OrderBy(render => render.LogicalOffset)
            .ToArray();

        Dictionary<int, uint> reusedDonorObjects = donor.Objects
            .Where(entry => entry.TypeHash == SharedVisualHelperClassId)
            .Select(entry => (Donor: entry, Targets: target.Objects.Where(candidate =>
                    candidate.TypeHash == entry.TypeHash &&
                    candidate.Name.Equals(entry.Name, StringComparison.Ordinal) &&
                    ObjectBytesEqual(donor, entry, target, candidate))
                .ToArray()))
            .Where(item => item.Targets.Length == 1)
            .ToDictionary(item => item.Donor.Index, item => item.Targets[0].Id);

        Dictionary<int, SmoObjectEntry[]> branchEntries = donorSkins.ToDictionary(
            skin => skin.Index,
            skin => GetVisualBranchEntries(donor, skin)
                .Where(entry => !reusedDonorObjects.ContainsKey(entry.Index))
                .ToArray());
        Dictionary<int, SmoObjectEntry[]> rigidBranchEntries = donorRigidRoots.ToDictionary(
            render => render.Index,
            render => GetRigidVisualBranchEntries(donor, render)
                .Where(entry => !reusedDonorObjects.ContainsKey(entry.Index))
                .ToArray());
        SmoObjectEntry[] selected = branchEntries.Values
            .Concat(rigidBranchEntries.Values)
            .SelectMany(entries => entries)
            .Concat(donorSecondaryRenders)
            .DistinctBy(entry => entry.Index)
            .OrderBy(entry => entry.LogicalOffset)
            .ToArray();
        EnsureCompleteVisualSelection(donor, selected);

        Dictionary<uint, SmoObjectEntry> donorById = donor.Objects
            .GroupBy(entry => entry.Id)
            .Where(group => group.Count() == 1)
            .ToDictionary(group => group.Key, group => group.Single());
        InlineObjectRelocation[] inlineRelocations = FindRigidTextureRelocations(
            donor,
            donorRigidRoots,
            donorSkins,
            donorById);
        Dictionary<string, SmoObjectEntry> targetNodes = target.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.Node &&
                            !string.IsNullOrWhiteSpace(entry.Name))
            .GroupBy(entry => entry.Name, StringComparer.Ordinal)
            .Where(group => group.Count() == 1)
            .ToDictionary(group => group.Key, group => group.Single(), StringComparer.Ordinal);

        uint nextId = target.Objects.Max(entry => entry.Id);
        var newIdsByDonorIndex = new Dictionary<int, uint>();
        foreach (SmoObjectEntry entry in selected)
            newIdsByDonorIndex.Add(entry.Index, checked(++nextId));

        var referenceIds = new Dictionary<uint, uint>();
        foreach (SmoObjectEntry entry in selected)
            referenceIds.Add(entry.Id, newIdsByDonorIndex[entry.Index]);
        foreach ((int donorIndex, uint targetId) in reusedDonorObjects)
            referenceIds[donor.Objects[donorIndex].Id] = targetId;
        SmoObjectEntry donorRoot = donor.Objects.Single(entry => entry.ParentIndex is null);
        SmoObjectEntry targetRoot = target.Objects.Single(entry => entry.ParentIndex is null);
        referenceIds[donorRoot.Id] = targetRoot.Id;

        foreach (SmoObjectEntry donorRender in donor.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.RenderNode &&
                     !referenceIds.ContainsKey(entry.Id)))
        {
            referenceIds[donorRender.Id] = targetRender.Id;
        }

        foreach (SmoObjectEntry donorNode in donor.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.Node && !referenceIds.ContainsKey(entry.Id)))
        {
            string sourceName = donorNode.Name;
            string targetName = boneRemap.DonorToTarget.TryGetValue(sourceName, out string? mapped)
                ? mapped
                : sourceName;
            if (targetNodes.TryGetValue(targetName, out SmoObjectEntry? targetNode))
                referenceIds[donorNode.Id] = targetNode.Id;
        }

        foreach (SmoObjectEntry donorEntry in donor.Objects.Where(entry =>
                     !referenceIds.ContainsKey(entry.Id)))
        {
            SmoObjectEntry[] matches = target.Objects.Where(candidate =>
                    candidate.TypeHash == donorEntry.TypeHash &&
                    candidate.Name.Equals(donorEntry.Name, StringComparison.Ordinal))
                .ToArray();
            if (matches.Length == 1)
                referenceIds[donorEntry.Id] = matches[0].Id;
        }

        var builtSkinBranches = new List<BuiltBranch>(donorSkins.Length);
        foreach (SmoObjectEntry skin in donorSkins)
        {
            builtSkinBranches.Add(BuildSkinBranch(
                donor,
                skin,
                branchEntries[skin.Index],
                donorById,
                referenceIds,
                newIdsByDonorIndex,
                boneRemap.DonorToTarget,
                targetNodes));
        }

        var builtRigidBranches = new List<BuiltBranch>(donorRigidRoots.Length);
        foreach (SmoObjectEntry rigidRoot in donorRigidRoots)
        {
            builtRigidBranches.Add(BuildRigidBranch(
                donor,
                rigidRoot,
                rigidBranchEntries[rigidRoot.Index],
                donorById,
                referenceIds,
                newIdsByDonorIndex,
                reusedDonorObjects));
        }

        foreach (InlineObjectRelocation relocation in inlineRelocations)
        {
            int sourceBranchIndex = builtSkinBranches.FindIndex(branch =>
                branch.Root.Index == relocation.SourceSkin.Index);
            int destinationBranchIndex = builtRigidBranches.FindIndex(branch =>
                branch.Root.Index == relocation.DestinationRigidRoot.Index);
            if (sourceBranchIndex < 0 || destinationBranchIndex < 0)
                throw new InvalidOperationException(
                    "Inline visual-object relocation branch was not built.");
            (BuiltBranch source, BuiltBranch destination) = RelocateInlineObject(
                donor,
                builtSkinBranches[sourceBranchIndex],
                builtRigidBranches[destinationBranchIndex],
                relocation,
                newIdsByDonorIndex);
            builtSkinBranches[sourceBranchIndex] = source;
            builtRigidBranches[destinationBranchIndex] = destination;
        }

        Dictionary<int, BuiltBranch> builtSkinByIndex = builtSkinBranches
            .ToDictionary(branch => branch.Root.Index);
        var builtSecondaryBranches = new List<BuiltBranch>(donorSecondaryRenders.Length);
        foreach (SmoObjectEntry secondaryRender in donorSecondaryRenders)
        {
            SmoObjectEntry[] childSkins = donorSkins.Where(skin =>
                    skin.ParentIndex == secondaryRender.Index)
                .OrderBy(skin => skin.LogicalOffset)
                .ToArray();
            builtSecondaryBranches.Add(BuildSecondaryRenderBranch(
                donor,
                secondaryRender,
                childSkins.Select(skin => builtSkinByIndex[skin.Index]).ToArray(),
                donorById,
                referenceIds,
                newIdsByDonorIndex));
        }

        var forestAttachments = new List<SmoVisualForestAttachment>(
            donorRigidRoots.Length + donorSecondaryRenders.Length + donorSkins.Length);
        for (int index = 0; index < donorRigidRoots.Length; index++)
        {
            SmoObjectEntry rigidRoot = donorRigidRoots[index];
            if (rigidRoot.ParentIndex is not int donorParentIndex ||
                donor.Objects[donorParentIndex].TypeHash != SmoClassIds.Node)
            {
                throw new InvalidOperationException(
                    $"Rigid donor render [{rigidRoot.Index}] has no direct spNode parent.");
            }
            SmoObjectEntry donorParent = donor.Objects[donorParentIndex];
            string targetName = boneRemap.DonorToTarget.TryGetValue(
                donorParent.Name, out string? mappedParent)
                ? mappedParent
                : donorParent.Name;
            if (!targetNodes.TryGetValue(targetName, out SmoObjectEntry? targetParent))
            {
                throw new InvalidOperationException(
                    $"Rigid donor render [{rigidRoot.Index}] parent node \"{donorParent.Name}\" " +
                    "has no unique target node mapping.");
            }
            forestAttachments.Add(BuildAttachment(
                donor,
                donorParent,
                builtRigidBranches[index],
                targetParent.Id,
                newIdsByDonorIndex));
        }

        for (int index = 0; index < donorSecondaryRenders.Length; index++)
        {
            SmoObjectEntry secondaryRender = donorSecondaryRenders[index];
            if (secondaryRender.ParentIndex is not int donorParentIndex ||
                donor.Objects[donorParentIndex].TypeHash != SmoClassIds.Node)
            {
                throw new InvalidOperationException(
                    $"Secondary donor render [{secondaryRender.Index}] has no direct spNode parent.");
            }
            SmoObjectEntry donorParent = donor.Objects[donorParentIndex];
            uint targetParentId;
            if (donorParent.ParentIndex is null)
            {
                targetParentId = targetRoot.Id;
            }
            else
            {
                string targetName = boneRemap.DonorToTarget.TryGetValue(
                    donorParent.Name, out string? mappedParent)
                    ? mappedParent
                    : donorParent.Name;
                if (!targetNodes.TryGetValue(targetName, out SmoObjectEntry? targetParent))
                {
                    throw new InvalidOperationException(
                        $"Secondary donor render [{secondaryRender.Index}] parent node " +
                        $"\"{donorParent.Name}\" has no unique target node mapping.");
                }
                targetParentId = targetParent.Id;
            }
            forestAttachments.Add(BuildAttachment(
                donor,
                donorParent,
                builtSecondaryBranches[index],
                targetParentId,
                newIdsByDonorIndex));
        }

        var primarySkinAttachments = new List<PrimarySkinAttachment>(donorSkins.Length);
        for (int index = 0; index < donorSkins.Length; index++)
        {
            SmoObjectEntry skin = donorSkins[index];
            if (skin.ParentIndex is not int donorParentIndex ||
                donor.Objects[donorParentIndex].TypeHash != SmoClassIds.RenderNode)
            {
                throw new InvalidOperationException(
                    $"Donor skin [{skin.Index}] has no direct spRenderNode parent.");
            }
            SmoObjectEntry donorParent = donor.Objects[donorParentIndex];
            if (donorParent.Index != donorPrimaryRender.Index)
            {
                if (!donorSecondaryRenders.Any(render => render.Index == donorParent.Index))
                    throw new InvalidOperationException(
                        $"Donor skin [{skin.Index}] belongs to an unsupported render branch.");
                continue;
            }
            primarySkinAttachments.Add(new PrimarySkinAttachment(
                skin,
                BuildAttachment(
                    donor,
                    donorParent,
                    builtSkinBranches[index],
                    targetRender.Id,
                    newIdsByDonorIndex)));
        }

        byte[] disabledBytes = SmoVisualTransplanter.DisableAllTargetMeshes(target);
        SmoDocument current = SmoDocument.Parse(disabledBytes, target.SourcePath);
        foreach (IGrouping<uint, SmoVisualForestAttachment> group in forestAttachments
                     .GroupBy(attachment => attachment.TargetOwnerId))
        {
            current = SmoDocument.Parse(
                SmoVisualForestInjector.Inject(current, group.Key, group.ToArray()),
                target.SourcePath);
        }
        if (primarySkinAttachments.Count > 0)
        {
            current = SmoDocument.Parse(
                SmoVisualForestInjector.Inject(
                    current,
                    targetRender.Id,
                    OrderPrimarySkinAttachments(donor, primarySkinAttachments)),
                target.SourcePath);
        }
        byte[] output = current.Data.ToArray();

        var packedIds = selected.Select(entry => newIdsByDonorIndex[entry.Index]).ToHashSet();
        var relocatedIds = inlineRelocations
            .Select(relocation => newIdsByDonorIndex[relocation.Object.Index])
            .ToHashSet();
        var meshIds = selected.Where(entry => entry.TypeHash == SmoClassIds.MeshData)
            .Select(entry => newIdsByDonorIndex[entry.Index]).ToHashSet();
        var textureIds = selected.Where(entry => entry.TypeHash == SmoClassIds.TextureData)
            .Select(entry => newIdsByDonorIndex[entry.Index]).ToHashSet();
        var result = new SmoPackedVisualGraph(
            output,
            meshIds.Count,
            textureIds.Count,
            target.Objects.Count(entry => entry.TypeHash == SmoClassIds.MeshData),
            packedIds,
            meshIds,
            textureIds);
        HashSet<uint> modifiedTargetOwners = forestAttachments
            .Select(attachment => attachment.TargetOwnerId)
            .Append(targetRender.Id)
            .ToHashSet();
        Verify(
            target,
            donor,
            result,
            newIdsByDonorIndex,
            referenceIds,
            relocatedIds,
            modifiedTargetOwners);
        return result;
    }

    private static SmoObjectEntry FindPrimaryRenderNode(SmoDocument document)
    {
        SmoObjectEntry[] meshes = document.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.MeshData)
            .ToArray();
        SmoObjectEntry[] renderNodes = document.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.RenderNode)
            .OrderByDescending(render => meshes.Count(mesh => Contains(render, mesh)))
            .ThenBy(entry => entry.LogicalOffset)
            .ToArray();
        if (renderNodes.Length == 0 || meshes.Length == 0)
        {
            throw new InvalidOperationException(
                "A render node containing mesh data was not found.");
        }
        return renderNodes[0];
    }

    private static SmoVisualForestAttachment[] OrderPrimarySkinAttachments(
        SmoDocument donor,
        IReadOnlyList<PrimarySkinAttachment> attachments)
    {
        if (attachments.Count < 2)
            return attachments.Select(item => item.Attachment).ToArray();

        HashSet<int> primarySkinIndices = attachments
            .Select(item => item.Skin.Index)
            .ToHashSet();
        var definingSkinByTexture = new Dictionary<int, int>();
        foreach (SmoObjectEntry texture in donor.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.TextureData))
        {
            SmoObjectEntry[] owners = attachments
                .Select(item => item.Skin)
                .Where(skin => Contains(skin, texture))
                .ToArray();
            if (owners.Length == 1)
                definingSkinByTexture[texture.Index] = owners[0].Index;
        }

        IReadOnlyDictionary<int, SmoTextureBinding> bindings =
            SmoTextureBindingResolver.ResolveAll(donor);
        var dependencies = attachments.ToDictionary(
            item => item.Skin.Index,
            _ => new HashSet<int>());
        foreach (PrimarySkinAttachment attachment in attachments)
        {
            foreach (SmoObjectEntry mesh in donor.Objects.Where(entry =>
                         entry.TypeHash == SmoClassIds.MeshData &&
                         Contains(attachment.Skin, entry)))
            {
                if (!bindings.TryGetValue(mesh.Index, out SmoTextureBinding? binding) ||
                    binding.Issue is not null || binding.Texture is null ||
                    !definingSkinByTexture.TryGetValue(
                        binding.Texture.ObjectIndex, out int definingSkinIndex) ||
                    definingSkinIndex == attachment.Skin.Index ||
                    !primarySkinIndices.Contains(definingSkinIndex))
                {
                    continue;
                }
                dependencies[attachment.Skin.Index].Add(definingSkinIndex);
            }
        }

        // Once donor skins are appended to a target render node, a target atlas
        // already exists before them. A donor such as Icy defines its atlas in a
        // later skin and relies on the character-atlas fallback for earlier
        // chunks. Emit each unique texture-defining skin before its consumers so
        // those chunks keep the donor binding instead of inheriting the target
        // atlas. Unrelated branches retain their original logical order.
        var remaining = attachments.ToDictionary(item => item.Skin.Index);
        var ordered = new List<SmoVisualForestAttachment>(attachments.Count);
        while (remaining.Count > 0)
        {
            PrimarySkinAttachment? next = attachments
                .Where(item => remaining.ContainsKey(item.Skin.Index))
                .Where(item => dependencies[item.Skin.Index].All(
                    dependency => !remaining.ContainsKey(dependency)))
                .OrderBy(item => item.Skin.LogicalOffset)
                .FirstOrDefault();
            if (next is null)
            {
                string cycle = string.Join(", ", remaining.Keys.Order());
                throw new InvalidOperationException(
                    $"Primary donor skin texture dependencies contain a cycle: {cycle}.");
            }
            ordered.Add(next.Attachment);
            remaining.Remove(next.Skin.Index);
        }
        return ordered.ToArray();
    }

    private static SmoObjectEntry[] FindRigidVisualRoots(SmoDocument document) =>
        document.Objects
            .Where(render => render.TypeHash == SmoClassIds.RenderNode &&
                             render.ParentIndex is int parentIndex &&
                             document.Objects[parentIndex].TypeHash == SmoClassIds.Node &&
                             document.Objects.Any(mesh =>
                                 mesh.TypeHash == SmoClassIds.MeshData &&
                                 HasRigidPath(document, render, mesh)))
            .Where(render => !document.Objects.Any(ancestor =>
                ancestor.TypeHash == SmoClassIds.RenderNode &&
                ancestor.Index != render.Index &&
                HasRigidPath(document, ancestor, render)))
            .OrderBy(render => render.LogicalOffset)
            .ToArray();

    private static bool HasRigidPath(
        SmoDocument document,
        SmoObjectEntry ancestor,
        SmoObjectEntry descendant)
    {
        if (!Contains(ancestor, descendant))
            return false;
        SmoObjectEntry cursor = descendant;
        while (cursor.ParentIndex is int parentIndex)
        {
            if (parentIndex == ancestor.Index)
                return true;
            cursor = document.Objects[parentIndex];
            if (cursor.TypeHash is SmoClassIds.Node or SmoClassIds.Skin)
                return false;
        }
        return false;
    }

    private static SmoObjectEntry[] GetRigidVisualBranchEntries(
        SmoDocument document,
        SmoObjectEntry root)
    {
        var result = new List<SmoObjectEntry> { root };
        foreach (SmoObjectEntry entry in document.Objects.Where(candidate =>
                     candidate.Index != root.Index && Contains(root, candidate)))
        {
            SmoObjectEntry cursor = entry;
            bool include = true;
            while (cursor.ParentIndex is int parentIndex && parentIndex != root.Index)
            {
                cursor = document.Objects[parentIndex];
                if (cursor.TypeHash is SmoClassIds.Node or SmoClassIds.Skin)
                {
                    include = false;
                    break;
                }
            }
            if (include && cursor.ParentIndex == root.Index &&
                entry.TypeHash is not SmoClassIds.Node and not SmoClassIds.Skin)
            {
                result.Add(entry);
            }
        }
        return result.OrderBy(entry => entry.LogicalOffset).ToArray();
    }

    private static SmoObjectEntry[] GetVisualBranchEntries(
        SmoDocument document,
        SmoObjectEntry skin)
    {
        var result = new List<SmoObjectEntry> { skin };
        foreach (SmoObjectEntry entry in document.Objects.Where(candidate =>
                     candidate.Index != skin.Index && Contains(skin, candidate)))
        {
            bool reachesSkin = false;
            bool crossesNode = entry.TypeHash == SmoClassIds.Node;
            SmoObjectEntry cursor = entry;
            while (!crossesNode && cursor.ParentIndex is int parentIndex)
            {
                if (parentIndex == skin.Index)
                {
                    reachesSkin = true;
                    break;
                }
                cursor = document.Objects[parentIndex];
                crossesNode = cursor.TypeHash == SmoClassIds.Node;
            }
            if (reachesSkin && !crossesNode)
                result.Add(entry);
        }
        return result.OrderBy(entry => entry.LogicalOffset).ToArray();
    }

    private static void EnsureCompleteVisualSelection(
        SmoDocument donor,
        IReadOnlyCollection<SmoObjectEntry> selected)
    {
        HashSet<int> selectedIndices = selected.Select(entry => entry.Index).ToHashSet();
        foreach (SmoObjectEntry entry in donor.Objects.Where(entry =>
                     entry.TypeHash is SmoClassIds.MeshData or SmoClassIds.TextureData or
                         SmoClassIds.MaterialData))
        {
            if (!selectedIndices.Contains(entry.Index))
                throw new InvalidOperationException(
                    $"Donor visual object [{entry.Index}] {entry.Name} is outside the packed skin branches.");
        }
    }

    private static InlineObjectRelocation[] FindRigidTextureRelocations(
        SmoDocument donor,
        IReadOnlyList<SmoObjectEntry> rigidRoots,
        IReadOnlyList<SmoObjectEntry> skins,
        IReadOnlyDictionary<uint, SmoObjectEntry> donorById)
    {
        var candidates = new List<(SmoObjectEntry Root, SmoObjectEntry Owner,
            SmoObjectEntry Texture)>();
        foreach (SmoObjectEntry root in rigidRoots)
        {
            foreach (SmoObjectEntry owner in GetRigidVisualBranchEntries(donor, root).Where(
                         entry => entry.TypeHash == SmoClassIds.MaterialData))
            {
                ReadOnlySpan<byte> serialized = ObjectBytes(donor, owner);
                foreach (SmoDataBlockHeader field in ReadFields(serialized).Where(
                             item => item.FieldType == 10 &&
                                     item.PayloadSize == ObjectReferenceSize))
                {
                    ReadOnlySpan<byte> payload = serialized.Slice(
                        field.PayloadOffset, ObjectReferenceSize);
                    uint objectId = BinaryPrimitives.ReadUInt32LittleEndian(payload);
                    uint inlineSize = BinaryPrimitives.ReadUInt32LittleEndian(payload[4..]);
                    if (inlineSize == 0 &&
                        donorById.TryGetValue(objectId, out SmoObjectEntry? texture) &&
                        texture.TypeHash == SmoClassIds.TextureData)
                    {
                        candidates.Add((root, owner, texture));
                    }
                }
            }
        }

        var result = new List<InlineObjectRelocation>();
        foreach (IGrouping<int, (SmoObjectEntry Root, SmoObjectEntry Owner,
                     SmoObjectEntry Texture)> group in candidates
                     .GroupBy(candidate => candidate.Texture.Index))
        {
            (SmoObjectEntry Root, SmoObjectEntry Owner, SmoObjectEntry Texture) destination =
                group.OrderBy(candidate => candidate.Root.LogicalOffset)
                    .ThenBy(candidate => candidate.Owner.LogicalOffset)
                    .First();
            SmoObjectEntry texture = destination.Texture;
            SmoObjectEntry[] definitionOwners = donor.Objects.Where(owner =>
                    owner.TypeHash == SmoClassIds.MaterialData &&
                    ReadFields(ObjectBytes(donor, owner)).Any(field =>
                    {
                        if (field.FieldType != 10 ||
                            field.PayloadSize != texture.SerializedSize + ObjectReferenceSize)
                            return false;
                        ReadOnlySpan<byte> payload = ObjectBytes(donor, owner).Slice(
                            field.PayloadOffset, ObjectReferenceSize);
                        return BinaryPrimitives.ReadUInt32LittleEndian(payload) == texture.Id &&
                               BinaryPrimitives.ReadUInt32LittleEndian(payload[4..]) ==
                                   texture.SerializedSize &&
                               owner.PhysicalOffset + field.PayloadOffset + ObjectReferenceSize ==
                                   texture.PhysicalOffset;
                    }))
                .ToArray();
            if (definitionOwners.Length != 1)
                continue;
            SmoObjectEntry[] sourceSkins = skins.Where(skin => Contains(skin, texture)).ToArray();
            if (sourceSkins.Length != 1)
                continue;
            result.Add(new InlineObjectRelocation(
                texture,
                definitionOwners[0],
                sourceSkins[0],
                destination.Owner,
                destination.Root));
        }
        return result.ToArray();
    }

    private static (BuiltBranch Source, BuiltBranch Destination) RelocateInlineObject(
        SmoDocument donor,
        BuiltBranch source,
        BuiltBranch destination,
        InlineObjectRelocation relocation,
        IReadOnlyDictionary<int, uint> newIdsByDonorIndex)
    {
        if (!source.Entries.Any(entry => entry.Source.Index == relocation.Object.Index) ||
            destination.Entries.Any(entry => entry.Source.Index == relocation.Object.Index))
        {
            throw new InvalidOperationException(
                $"Visual object [{relocation.Object.Index}] has an invalid relocation ownership state.");
        }
        if (donor.Objects.Any(entry => entry.Index != relocation.Object.Index &&
                                       Contains(relocation.Object, entry)))
        {
            throw new InvalidOperationException(
                $"Visual object [{relocation.Object.Index}] has nested catalog descendants.");
        }

        uint newId = newIdsByDonorIndex[relocation.Object.Index];
        BuiltBranch rewrittenSource = RewriteInlineObjectField(
            donor,
            source,
            relocation.SourceOwner,
            relocation.Object,
            newId,
            makeInline: false,
            newIdsByDonorIndex);
        BuiltBranch rewrittenDestination = RewriteInlineObjectField(
            donor,
            destination,
            relocation.DestinationOwner,
            relocation.Object,
            newId,
            makeInline: true,
            newIdsByDonorIndex);
        return (rewrittenSource, rewrittenDestination);
    }

    private static BuiltBranch RewriteInlineObjectField(
        SmoDocument donor,
        BuiltBranch branch,
        SmoObjectEntry owner,
        SmoObjectEntry movedObject,
        uint newObjectId,
        bool makeInline,
        IReadOnlyDictionary<int, uint> newIdsByDonorIndex)
    {
        BranchEntry ownerPlacement = branch.Entries.Single(entry =>
            entry.Source.Index == owner.Index);
        ReadOnlySpan<byte> ownerData = branch.Data.AsSpan(
            ownerPlacement.RelativeOffset,
            checked((int)ownerPlacement.SerializedSize));
        uint expectedOldInlineSize = makeInline ? 0 : movedObject.SerializedSize;
        int expectedFieldType = owner.TypeHash == SmoClassIds.MaterialData &&
                                movedObject.TypeHash == SmoClassIds.TextureData
            ? 10
            : owner.TypeHash == SmoClassIds.Model &&
              movedObject.TypeHash == SharedVisualHelperClassId
                ? 1
                : throw new InvalidOperationException(
                    $"Inline rewrite [{owner.Index}] -> [{movedObject.Index}] uses an " +
                    "unsupported owner/object class pair.");
        var matches = new List<SmoDataBlockHeader>();
        foreach (SmoDataBlockHeader candidate in ReadFields(ownerData))
        {
            if (candidate.FieldType != expectedFieldType ||
                candidate.PayloadSize < ObjectReferenceSize)
                continue;
            ReadOnlySpan<byte> payload = ownerData.Slice(
                candidate.PayloadOffset, ObjectReferenceSize);
            if (BinaryPrimitives.ReadUInt32LittleEndian(payload) == newObjectId &&
                BinaryPrimitives.ReadUInt32LittleEndian(payload[4..]) ==
                    expectedOldInlineSize &&
                candidate.PayloadSize == expectedOldInlineSize + ObjectReferenceSize)
            {
                matches.Add(candidate);
            }
        }
        if (matches.Count != 1)
        {
            throw new InvalidOperationException(
                $"Inline rewrite owner [{owner.Index}] does not contain one exact " +
                $"field-{expectedFieldType} " +
                $"reference to visual object [{movedObject.Index}].");
        }

        SmoDataBlockHeader field = matches[0];
        uint newPayloadSize = checked(ObjectReferenceSize +
            (makeInline ? movedObject.SerializedSize : 0));
        byte[] header = ownerData.Slice(field.Offset, field.HeaderSize).ToArray();
        WritePayloadSize(header, 0, field, newPayloadSize);
        byte[] replacement = new byte[checked(header.Length + (int)newPayloadSize)];
        header.CopyTo(replacement, 0);
        WriteUInt32(replacement, header.Length, newObjectId);
        WriteUInt32(
            replacement,
            header.Length + sizeof(uint),
            makeInline ? movedObject.SerializedSize : 0);
        if (makeInline)
        {
            ObjectBytes(donor, movedObject).CopyTo(
                replacement.AsSpan(header.Length + ObjectReferenceSize));
        }

        int changeStart = checked(ownerPlacement.RelativeOffset + field.Offset);
        int oldEnd = checked(changeStart + field.HeaderSize + (int)field.PayloadSize);
        int delta = checked(replacement.Length - (oldEnd - changeStart));
        byte[] rewritten = new byte[checked(branch.Data.Length + delta)];
        branch.Data.AsSpan(0, changeStart).CopyTo(rewritten);
        replacement.CopyTo(rewritten, changeStart);
        branch.Data.AsSpan(oldEnd).CopyTo(rewritten.AsSpan(changeStart + replacement.Length));

        foreach (BranchEntry entry in branch.Entries)
        {
            ReadOnlySpan<byte> serialized = branch.Data.AsSpan(
                entry.RelativeOffset, checked((int)entry.SerializedSize));
            foreach (SmoDataBlockHeader ancestorField in ReadFields(serialized))
            {
                long payloadStart = (long)entry.RelativeOffset + ancestorField.PayloadOffset;
                long payloadEnd = (long)entry.RelativeOffset + ancestorField.PayloadEnd;
                if (payloadStart <= changeStart && changeStart < payloadEnd)
                {
                    int mappedHeader = checked(entry.RelativeOffset + ancestorField.Offset);
                    long resizedPayload = (long)ancestorField.PayloadSize + delta;
                    if (resizedPayload < 0 || resizedPayload > uint.MaxValue)
                        throw new InvalidOperationException(
                            "Relocated visual object produced an invalid ancestor field size.");
                    WritePayloadSize(
                        rewritten,
                        mappedHeader,
                        ancestorField,
                        checked((uint)resizedPayload));
                }
            }
        }

        foreach (BranchEntry entry in branch.Entries.Where(entry =>
                     entry.RelativeOffset <= changeStart &&
                     changeStart < (long)entry.RelativeOffset + entry.SerializedSize))
        {
            int prefix = entry.RelativeOffset - ObjectReferenceSize;
            if (prefix < 0)
                continue;
            uint expectedId = newIdsByDonorIndex[entry.Source.Index];
            if (BinaryPrimitives.ReadUInt32LittleEndian(branch.Data.AsSpan(prefix)) != expectedId ||
                BinaryPrimitives.ReadUInt32LittleEndian(branch.Data.AsSpan(prefix + 4)) !=
                    entry.SerializedSize)
            {
                throw new InvalidOperationException(
                    $"Relocation ancestor [{entry.Source.Index}] has a stale inline prefix.");
            }
            WriteUInt32(
                rewritten,
                prefix + sizeof(uint),
                checked((uint)((long)entry.SerializedSize + delta)));
        }

        var placements = new List<BranchEntry>(
            branch.Entries.Count + (makeInline ? 1 : -1));
        foreach (BranchEntry entry in branch.Entries)
        {
            if (entry.Source.Index == movedObject.Index)
            {
                if (makeInline)
                    throw new InvalidOperationException("Destination already contains relocated object.");
                continue;
            }
            long entryEnd = (long)entry.RelativeOffset + entry.SerializedSize;
            bool containsChange = entry.RelativeOffset <= changeStart && changeStart < entryEnd;
            if (!containsChange && entry.RelativeOffset > changeStart &&
                entry.RelativeOffset < oldEnd)
            {
                throw new InvalidOperationException(
                    $"Relocation would split catalog object [{entry.Source.Index}].");
            }
            int relativeOffset = entry.RelativeOffset >= oldEnd
                ? checked(entry.RelativeOffset + delta)
                : entry.RelativeOffset;
            uint serializedSize = containsChange
                ? checked((uint)((long)entry.SerializedSize + delta))
                : entry.SerializedSize;
            placements.Add(new BranchEntry(entry.Source, relativeOffset, serializedSize));
        }
        if (makeInline)
        {
            placements.Add(new BranchEntry(
                movedObject,
                checked(changeStart + header.Length + ObjectReferenceSize),
                movedObject.SerializedSize));
        }
        placements.Sort((left, right) => left.RelativeOffset.CompareTo(right.RelativeOffset));
        BranchEntry rebuiltRoot = placements.Single(entry =>
            entry.Source.Index == branch.Root.Index);
        if (rebuiltRoot.RelativeOffset != 0 || rebuiltRoot.SerializedSize != rewritten.Length)
            throw new InvalidOperationException("Relocated branch root size is inconsistent.");
        return new BuiltBranch(branch.Root, rewritten, placements);
    }

    private static BuiltBranch BuildSkinBranch(
        SmoDocument donor,
        SmoObjectEntry skinEntry,
        IReadOnlyList<SmoObjectEntry> branchEntries,
        IReadOnlyDictionary<uint, SmoObjectEntry> donorById,
        IReadOnlyDictionary<uint, uint> referenceIds,
        IReadOnlyDictionary<int, uint> newIdsByDonorIndex,
        IReadOnlyDictionary<string, string> boneRemap,
        IReadOnlyDictionary<string, SmoObjectEntry> targetNodes)
    {
        if (!SmoSkinDecoder.TryDecode(
                donor, skinEntry, out SmoSkin? skin, out string skinError) || skin is null)
            throw new InvalidOperationException(skinError);

        ReadOnlySpan<byte> source = donor.Data.Span.Slice(
            checked((int)skinEntry.PhysicalOffset), checked((int)skinEntry.SerializedSize));
        SmoDataBlockHeader palette = FindPaletteField(source, skin.Bones.Count);
        int paletteEnd = checked((int)palette.PayloadEnd);
        if (paletteEnd != source.Length - 1 || source[^1] != 0)
            throw new InvalidOperationException(
                $"Donor skin [{skinEntry.Index}] has serialized fields after its palette; " +
                "their shifted offsets are not supported by the visual graph packer.");
        byte[] paletteField = BuildPaletteField(
            source.Slice(palette.Offset, palette.HeaderSize),
            palette,
            skin,
            donor,
            boneRemap,
            targetNodes);

        byte[] working = source.ToArray();
        foreach (SmoObjectEntry descendant in branchEntries.Skip(1))
        {
            int prefix = checked((int)(descendant.PhysicalOffset - skinEntry.PhysicalOffset) - 8);
            if (prefix < 0 ||
                BinaryPrimitives.ReadUInt32LittleEndian(source[prefix..]) != descendant.Id ||
                BinaryPrimitives.ReadUInt32LittleEndian(source[(prefix + 4)..]) != descendant.SerializedSize)
            {
                throw new InvalidOperationException(
                    $"Donor visual object [{descendant.Index}] is not serialized through a confirmed inline prefix.");
            }
            WriteUInt32(working, prefix, newIdsByDonorIndex[descendant.Index]);
        }

        PatchConfirmedReferences(
            working,
            donor,
            skinEntry,
            branchEntries,
            donorById,
            referenceIds,
            palette.Offset);

        HashSet<int> selectedIndices = branchEntries.Select(entry => entry.Index).ToHashSet();
        SmoObjectEntry[] directChildren = donor.Objects
            .Where(entry => entry.ParentIndex == skinEntry.Index)
            .ToArray();
        var fieldMappings = new List<(int OldStart, int OldEnd, int NewStart)>();
        using var stream = new MemoryStream(source.Length);
        stream.Write(working.AsSpan(0, ObjectSignatureSize));
        int offset = ObjectSignatureSize;
        while (offset < source.Length &&
               SmoDataBlockReader.TryReadHeader(source, offset, out SmoDataBlockHeader field))
        {
            int newStart = checked((int)stream.Position);
            if (field.Offset == palette.Offset)
            {
                stream.Write(paletteField);
            }
            else
            {
                SmoObjectEntry? inlineChild = null;
                if (field.PayloadSize >= ObjectReferenceSize)
                {
                    ReadOnlySpan<byte> payload = source.Slice(
                        field.PayloadOffset, checked((int)field.PayloadSize));
                    uint childId = BinaryPrimitives.ReadUInt32LittleEndian(payload);
                    uint childSize = BinaryPrimitives.ReadUInt32LittleEndian(payload[4..]);
                    inlineChild = directChildren.SingleOrDefault(child =>
                        child.Id == childId && child.SerializedSize == childSize &&
                        skinEntry.PhysicalOffset + field.PayloadOffset + ObjectReferenceSize ==
                        child.PhysicalOffset && field.PayloadSize == childSize + ObjectReferenceSize);
                }

                if (inlineChild is not null && !selectedIndices.Contains(inlineChild.Index))
                {
                    if (!referenceIds.TryGetValue(inlineChild.Id, out uint replacementId))
                        throw new InvalidOperationException(
                            $"Excluded donor skin child [{inlineChild.Index}] has no target mapping.");
                    byte[] header = source.Slice(field.Offset, field.HeaderSize).ToArray();
                    WritePayloadSize(header, 0, field, ObjectReferenceSize);
                    stream.Write(header);
                    WriteUInt32(stream, replacementId);
                    WriteUInt32(stream, 0);
                }
                else
                {
                    stream.Write(working.AsSpan(
                        field.Offset, checked((int)field.PayloadEnd - field.Offset)));
                    fieldMappings.Add((
                        field.Offset,
                        checked((int)field.PayloadEnd),
                        newStart));
                }
            }
            offset = checked((int)field.PayloadEnd);
        }
        if (offset != source.Length)
            throw new InvalidOperationException(
                $"Donor skin [{skinEntry.Index}] has an invalid field stream.");

        byte[] rebuilt = stream.ToArray();
        var placements = new List<BranchEntry>
        {
            new(skinEntry, RelativeOffset: 0, checked((uint)rebuilt.Length))
        };
        foreach (SmoObjectEntry descendant in branchEntries.Skip(1))
        {
            int oldRelative = checked((int)(descendant.PhysicalOffset - skinEntry.PhysicalOffset));
            (int OldStart, int OldEnd, int NewStart) mapping = fieldMappings.Single(item =>
                item.OldStart <= oldRelative && oldRelative < item.OldEnd);
            placements.Add(new BranchEntry(
                descendant,
                checked(mapping.NewStart + oldRelative - mapping.OldStart),
                descendant.SerializedSize));
        }
        return new BuiltBranch(
            skinEntry,
            rebuilt,
            placements);
    }

    private static BuiltBranch BuildRigidBranch(
        SmoDocument donor,
        SmoObjectEntry rootEntry,
        IReadOnlyList<SmoObjectEntry> branchEntries,
        IReadOnlyDictionary<uint, SmoObjectEntry> donorById,
        IReadOnlyDictionary<uint, uint> referenceIds,
        IReadOnlyDictionary<int, uint> newIdsByDonorIndex,
        IReadOnlyDictionary<int, uint> reusedDonorObjects)
    {
        HashSet<int> selectedIndices = branchEntries
            .Select(entry => entry.Index)
            .ToHashSet();
        SmoObjectEntry[] omittedInlineObjects = donor.Objects.Where(entry =>
                entry.Index != rootEntry.Index &&
                Contains(rootEntry, entry) &&
                !selectedIndices.Contains(entry.Index) &&
                entry.TypeHash is SmoClassIds.Node or SmoClassIds.Skin)
            .ToArray();
        if (omittedInlineObjects.Length > 0)
        {
            throw new InvalidOperationException(
                $"Rigid donor render [{rootEntry.Index}] contains excluded inline object " +
                $"[{omittedInlineObjects[0].Index}] {omittedInlineObjects[0].Name}.");
        }

        byte[] rebuilt = ObjectBytes(donor, rootEntry).ToArray();
        foreach (SmoObjectEntry descendant in branchEntries.Skip(1))
        {
            int prefix = checked((int)(descendant.PhysicalOffset - rootEntry.PhysicalOffset) - 8);
            if (prefix < 0 ||
                BinaryPrimitives.ReadUInt32LittleEndian(rebuilt.AsSpan(prefix)) != descendant.Id ||
                BinaryPrimitives.ReadUInt32LittleEndian(rebuilt.AsSpan(prefix + 4)) !=
                descendant.SerializedSize)
            {
                throw new InvalidOperationException(
                    $"Rigid donor visual object [{descendant.Index}] is not serialized " +
                    "through a confirmed inline prefix.");
            }
            WriteUInt32(rebuilt, prefix, newIdsByDonorIndex[descendant.Index]);
        }

        PatchConfirmedReferences(
            rebuilt,
            donor,
            rootEntry,
            branchEntries,
            donorById,
            referenceIds,
            skippedFieldOffset: null);
        BuiltBranch result = new(
            rootEntry,
            rebuilt,
            BuildPreservedBranchEntries(rootEntry, rebuilt.Length, branchEntries));

        // A rigid spModel can inline the same byte-identical visual helper that
        // is already present in the target. Keeping the inline bytes while
        // remapping only their ID would define the target object twice. Collapse
        // every such confirmed inline definition to the serializer's observed
        // reference-only Model.field1 form, updating all enclosing sizes and
        // catalog placements transactionally.
        foreach ((int donorIndex, uint targetId) in reusedDonorObjects
                     .Where(item => Contains(rootEntry, donor.Objects[item.Key]))
                     .OrderByDescending(item => donor.Objects[item.Key].LogicalOffset))
        {
            SmoObjectEntry reused = donor.Objects[donorIndex];
            if (reused.ParentIndex is not int ownerIndex ||
                !branchEntries.Any(entry => entry.Index == ownerIndex))
            {
                throw new InvalidOperationException(
                    $"Reusable rigid helper [{reused.Index}] has no packed direct owner.");
            }
            SmoObjectEntry owner = donor.Objects[ownerIndex];
            if (owner.TypeHash != SmoClassIds.Model ||
                reused.TypeHash != SharedVisualHelperClassId)
            {
                throw new InvalidOperationException(
                    $"Reusable rigid object [{reused.Index}] is not a confirmed " +
                    "spModel visual-helper child.");
            }
            result = RewriteInlineObjectField(
                donor,
                result,
                owner,
                reused,
                targetId,
                makeInline: false,
                newIdsByDonorIndex);
        }
        return result;
    }

    private static BuiltBranch BuildSecondaryRenderBranch(
        SmoDocument donor,
        SmoObjectEntry renderEntry,
        IReadOnlyList<BuiltBranch> skinBranches,
        IReadOnlyDictionary<uint, SmoObjectEntry> donorById,
        IReadOnlyDictionary<uint, uint> referenceIds,
        IReadOnlyDictionary<int, uint> newIdsByDonorIndex)
    {
        Dictionary<int, BuiltBranch> skinsByIndex = skinBranches
            .ToDictionary(branch => branch.Root.Index);
        SmoObjectEntry[] directChildren = donor.Objects
            .Where(entry => entry.ParentIndex == renderEntry.Index)
            .ToArray();
        ReadOnlySpan<byte> source = ObjectBytes(donor, renderEntry);
        using var stream = new MemoryStream(source.Length);
        stream.Write(source[..ObjectSignatureSize]);
        var placements = new List<BranchEntry>();
        int offset = ObjectSignatureSize;
        while (offset < source.Length &&
               SmoDataBlockReader.TryReadHeader(source, offset, out SmoDataBlockHeader field))
        {
            SmoObjectEntry? inlineChild = null;
            if (field.PayloadSize >= ObjectReferenceSize)
            {
                ReadOnlySpan<byte> payload = source.Slice(
                    field.PayloadOffset, checked((int)field.PayloadSize));
                uint childId = BinaryPrimitives.ReadUInt32LittleEndian(payload);
                uint childSize = BinaryPrimitives.ReadUInt32LittleEndian(payload[4..]);
                inlineChild = directChildren.SingleOrDefault(child =>
                    child.Id == childId && child.SerializedSize == childSize &&
                    renderEntry.PhysicalOffset + field.PayloadOffset + ObjectReferenceSize ==
                    child.PhysicalOffset && field.PayloadSize == childSize + ObjectReferenceSize);
            }

            if (inlineChild is not null)
            {
                if (skinsByIndex.TryGetValue(inlineChild.Index, out BuiltBranch? skinBranch))
                {
                    byte[] header = source.Slice(field.Offset, field.HeaderSize).ToArray();
                    WritePayloadSize(
                        header,
                        0,
                        field,
                        checked((uint)(skinBranch.Data.Length + ObjectReferenceSize)));
                    stream.Write(header);
                    WriteUInt32(stream, newIdsByDonorIndex[inlineChild.Index]);
                    WriteUInt32(stream, checked((uint)skinBranch.Data.Length));
                    int childStart = checked((int)stream.Position);
                    stream.Write(skinBranch.Data);
                    placements.AddRange(skinBranch.Entries.Select(entry => entry with
                    {
                        RelativeOffset = checked(childStart + entry.RelativeOffset)
                    }));
                }
                else if (inlineChild.TypeHash != SmoClassIds.Node)
                {
                    throw new InvalidOperationException(
                        $"Secondary donor render [{renderEntry.Index}] contains unsupported " +
                        $"direct child [{inlineChild.Index}] {inlineChild.Name}.");
                }
            }
            else
            {
                byte[] rawField = source.Slice(
                    field.Offset, checked((int)field.PayloadEnd - field.Offset)).ToArray();
                if (field.PayloadSize >= ObjectReferenceSize)
                {
                    ReadOnlySpan<byte> payload = source.Slice(
                        field.PayloadOffset, checked((int)field.PayloadSize));
                    uint oldId = BinaryPrimitives.ReadUInt32LittleEndian(payload);
                    uint inlineSize = BinaryPrimitives.ReadUInt32LittleEndian(payload[4..]);
                    if (donorById.TryGetValue(oldId, out SmoObjectEntry? referenced) &&
                        IsConfirmedObjectReference(renderEntry, referenced, field, inlineSize))
                    {
                        if (!referenceIds.TryGetValue(oldId, out uint replacementId))
                            throw new InvalidOperationException(
                                $"Secondary render reference to donor ID {oldId} has no mapping.");
                        WriteUInt32(rawField, field.HeaderSize, replacementId);
                    }
                }
                stream.Write(rawField);
            }
            offset = checked((int)field.PayloadEnd);
        }
        if (offset != source.Length)
            throw new InvalidOperationException(
                $"Secondary donor render [{renderEntry.Index}] has an invalid field stream.");

        byte[] rebuilt = stream.ToArray();
        placements.Insert(0, new BranchEntry(
            renderEntry,
            RelativeOffset: 0,
            checked((uint)rebuilt.Length)));
        return new BuiltBranch(renderEntry, rebuilt, placements);
    }

    private static IReadOnlyList<BranchEntry> BuildPreservedBranchEntries(
        SmoObjectEntry root,
        int rebuiltRootSize,
        IReadOnlyList<SmoObjectEntry> sourceEntries) =>
        sourceEntries.Select(source => new BranchEntry(
                source,
                checked((int)(source.PhysicalOffset - root.PhysicalOffset)),
                source.Index == root.Index
                    ? checked((uint)rebuiltRootSize)
                    : source.SerializedSize))
            .ToArray();

    private static SmoDataBlockHeader FindPaletteField(
        ReadOnlySpan<byte> serialized,
        int expectedBoneCount)
    {
        int offset = ObjectSignatureSize;
        SmoDataBlockHeader found = default;
        bool hasPalette = false;
        while (offset < serialized.Length &&
               SmoDataBlockReader.TryReadHeader(serialized, offset, out SmoDataBlockHeader field))
        {
            if (field.FieldType == 0 && field.PayloadSize >= 8)
            {
                ReadOnlySpan<byte> payload = serialized.Slice(
                    field.PayloadOffset, checked((int)field.PayloadSize));
                if (BinaryPrimitives.ReadUInt32LittleEndian(payload) == 0 &&
                    BinaryPrimitives.ReadUInt32LittleEndian(payload[4..]) == expectedBoneCount)
                {
                    found = field;
                    hasPalette = true;
                }
            }
            offset = checked((int)field.PayloadEnd);
        }
        return hasPalette
            ? found
            : throw new InvalidOperationException("The donor skin palette field was not found.");
    }

    private static byte[] BuildPaletteField(
        ReadOnlySpan<byte> originalHeader,
        SmoDataBlockHeader palette,
        SmoSkin skin,
        SmoDocument donor,
        IReadOnlyDictionary<string, string> boneRemap,
        IReadOnlyDictionary<string, SmoObjectEntry> targetNodes)
    {
        if (palette.SizeKind != SmoDataBlockSizeCode.UInt32)
            throw new InvalidOperationException("The donor skin palette does not use a writable UInt32 size.");
        int payloadSize = checked(8 + skin.Bones.Count * (8 + 16 * sizeof(float)));
        byte[] result = new byte[checked(palette.HeaderSize + payloadSize)];
        originalHeader.CopyTo(result);
        WriteUInt32(result, palette.HeaderSize - sizeof(uint), checked((uint)payloadSize));
        WriteUInt32(result, palette.HeaderSize, 0);
        WriteUInt32(result, palette.HeaderSize + 4, checked((uint)skin.Bones.Count));
        int cursor = palette.HeaderSize + 8;
        foreach (SmoSkinBone bone in skin.Bones)
        {
            string donorName = donor.Objects[bone.NodeObjectIndex].Name;
            string targetName = boneRemap.TryGetValue(donorName, out string? mapped)
                ? mapped
                : donorName;
            if (!targetNodes.TryGetValue(targetName, out SmoObjectEntry? targetNode))
                throw new InvalidOperationException(
                    $"Donor palette bone \"{donorName}\" has no unique target node mapping.");
            WriteUInt32(result, cursor, targetNode.Id);
            WriteUInt32(result, cursor + 4, 0);
            WriteMatrix(result.AsSpan(cursor + 8, 16 * sizeof(float)), bone.InverseBindMatrix);
            cursor += 8 + 16 * sizeof(float);
        }
        return result;
    }

    private static void PatchConfirmedReferences(
        Span<byte> rebuiltRoot,
        SmoDocument donor,
        SmoObjectEntry rootEntry,
        IReadOnlyList<SmoObjectEntry> branchEntries,
        IReadOnlyDictionary<uint, SmoObjectEntry> donorById,
        IReadOnlyDictionary<uint, uint> referenceIds,
        int? skippedFieldOffset)
    {
        foreach (SmoObjectEntry owner in branchEntries)
        {
            ReadOnlySpan<byte> original = donor.Data.Span.Slice(
                checked((int)owner.PhysicalOffset), checked((int)owner.SerializedSize));
            int ownerRelative = checked((int)(owner.PhysicalOffset - rootEntry.PhysicalOffset));
            int offset = ObjectSignatureSize;
            while (offset < original.Length &&
                   SmoDataBlockReader.TryReadHeader(original, offset, out SmoDataBlockHeader field))
            {
                bool isSkippedField = owner.Index == rootEntry.Index &&
                                      field.Offset == skippedFieldOffset;
                if (!isSkippedField && owner.TypeHash == TextureSequenceClassId &&
                    field.FieldType == 0 && field.PayloadSize != 0)
                {
                    foreach (TextureSequenceReference textureReference in
                             ReadTextureSequenceReferences(
                                 donor,
                                 owner,
                                 original,
                                 field,
                                 donorById))
                    {
                        if (!referenceIds.TryGetValue(
                                textureReference.ObjectId, out uint replacementId))
                        {
                            throw new InvalidOperationException(
                                $"Texture-sequence reference [{owner.Index}]+" +
                                $"0x{textureReference.Offset:X} to donor ID " +
                                $"{textureReference.ObjectId} has no target/new-object mapping.");
                        }
                        WriteUInt32(
                            rebuiltRoot,
                            ownerRelative + textureReference.Offset,
                            replacementId);
                    }
                }
                else if (!isSkippedField && field.PayloadSize >= ObjectReferenceSize)
                {
                    ReadOnlySpan<byte> payload = original.Slice(
                        field.PayloadOffset, checked((int)field.PayloadSize));
                    uint oldId = BinaryPrimitives.ReadUInt32LittleEndian(payload);
                    uint inlineSize = BinaryPrimitives.ReadUInt32LittleEndian(payload[4..]);
                    if (donorById.TryGetValue(oldId, out SmoObjectEntry? referenced) &&
                        IsConfirmedObjectReference(owner, referenced, field, inlineSize))
                    {
                        if (!referenceIds.TryGetValue(oldId, out uint replacementId))
                        {
                            throw new InvalidOperationException(
                                $"Confirmed donor reference [{owner.Index}]+0x{field.PayloadOffset:X} " +
                                $"to [{referenced.Index}] {referenced.Name} has no target/new-object mapping.");
                        }
                        WriteUInt32(rebuiltRoot, ownerRelative + field.PayloadOffset, replacementId);
                    }
                }
                offset = checked((int)field.PayloadEnd);
            }
        }
    }

    private static TextureSequenceReference[] ReadTextureSequenceReferences(
        SmoDocument document,
        SmoObjectEntry owner,
        ReadOnlySpan<byte> serialized,
        SmoDataBlockHeader field,
        IReadOnlyDictionary<uint, SmoObjectEntry> byId)
    {
        if (owner.TypeHash != TextureSequenceClassId || field.FieldType != 0 ||
            field.PayloadSize < sizeof(uint))
        {
            throw new InvalidOperationException(
                $"Object [{owner.Index}] is not a supported texture-sequence field.");
        }

        int payloadEnd = checked((int)field.PayloadEnd);
        uint count = BinaryPrimitives.ReadUInt32LittleEndian(
            serialized[field.PayloadOffset..]);
        long referencesStart = (long)field.PayloadOffset + sizeof(uint) +
                               (long)count * sizeof(float);
        if (count == 0 || count > int.MaxValue ||
            referencesStart < field.PayloadOffset || referencesStart > payloadEnd)
        {
            throw new InvalidOperationException(
                $"Texture sequence [{owner.Index}] has an invalid sample count {count}.");
        }

        int cursor = checked((int)referencesStart);
        var result = new TextureSequenceReference[checked((int)count)];
        for (int index = 0; index < result.Length; index++)
        {
            if (cursor > payloadEnd - ObjectReferenceSize)
            {
                throw new InvalidOperationException(
                    $"Texture sequence [{owner.Index}] reference {index} is truncated.");
            }
            uint objectId = BinaryPrimitives.ReadUInt32LittleEndian(serialized[cursor..]);
            uint inlineSize = BinaryPrimitives.ReadUInt32LittleEndian(serialized[(cursor + 4)..]);
            if (!byId.TryGetValue(objectId, out SmoObjectEntry? texture) ||
                texture.TypeHash != SmoClassIds.TextureData)
            {
                throw new InvalidOperationException(
                    $"Texture sequence [{owner.Index}] reference {index} points to " +
                    $"non-texture donor ID {objectId}.");
            }

            if (inlineSize != 0 &&
                (inlineSize != texture.SerializedSize ||
                 owner.PhysicalOffset + cursor + ObjectReferenceSize != texture.PhysicalOffset))
            {
                throw new InvalidOperationException(
                    $"Texture sequence [{owner.Index}] reference {index} has an " +
                    "unconfirmed inline texture interval.");
            }
            long next = (long)cursor + ObjectReferenceSize + inlineSize;
            if (next > payloadEnd)
            {
                throw new InvalidOperationException(
                    $"Texture sequence [{owner.Index}] reference {index} exceeds its field.");
            }
            result[index] = new TextureSequenceReference(cursor, objectId, inlineSize);
            cursor = checked((int)next);
        }
        if (cursor != payloadEnd)
        {
            throw new InvalidOperationException(
                $"Texture sequence [{owner.Index}] has {payloadEnd - cursor} trailing bytes.");
        }
        return result;
    }

    private static SmoVisualForestAttachment BuildAttachment(
        SmoDocument donor,
        SmoObjectEntry donorParent,
        BuiltBranch branch,
        uint targetOwnerId,
        IReadOnlyDictionary<int, uint> newIdsByDonorIndex)
    {
        SmoObjectEntry root = branch.Root;
        ReadOnlySpan<byte> parentData = ObjectBytes(donor, donorParent);
        int oldPrefix = checked((int)(root.PhysicalOffset - donorParent.PhysicalOffset) - 8);
        int fieldOffset = FindContainingFieldOffset(parentData, oldPrefix);
        if (!SmoDataBlockReader.TryReadHeader(
                parentData, fieldOffset, out SmoDataBlockHeader wrapper) ||
            wrapper.PayloadOffset != oldPrefix || wrapper.PayloadSize != root.SerializedSize + 8)
        {
            throw new InvalidOperationException(
                $"Donor visual root [{root.Index}] has an unsupported parent wrapper.");
        }

        byte[] header = parentData.Slice(fieldOffset, wrapper.HeaderSize).ToArray();
        WritePayloadSize(header, 0, wrapper, checked((uint)(branch.Data.Length + 8)));
        byte[] fieldData = new byte[checked(header.Length + 8 + branch.Data.Length)];
        header.CopyTo(fieldData, 0);
        WriteUInt32(fieldData, header.Length, newIdsByDonorIndex[root.Index]);
        WriteUInt32(fieldData, header.Length + 4, checked((uint)branch.Data.Length));
        branch.Data.CopyTo(fieldData, header.Length + 8);

        int rootRelative = header.Length + 8;
        var entries = new List<SmoVisualForestEntry>(branch.Entries.Count);
        foreach (BranchEntry branchEntry in branch.Entries)
        {
            SmoObjectEntry source = branchEntry.Source;
            entries.Add(new SmoVisualForestEntry(
                newIdsByDonorIndex[source.Index],
                source.RawName.ToArray(),
                source.TypeHash,
                checked(rootRelative + branchEntry.RelativeOffset),
                branchEntry.SerializedSize));
        }
        return new SmoVisualForestAttachment(targetOwnerId, fieldData, entries);
    }

    private static int FindContainingFieldOffset(ReadOnlySpan<byte> owner, int payloadOffset)
    {
        int offset = ObjectSignatureSize;
        while (offset < owner.Length &&
               SmoDataBlockReader.TryReadHeader(owner, offset, out SmoDataBlockHeader field))
        {
            if (field.PayloadOffset == payloadOffset)
                return offset;
            offset = checked((int)field.PayloadEnd);
        }
        throw new InvalidOperationException("The inline child field wrapper was not found.");
    }

    private static void Verify(
        SmoDocument target,
        SmoDocument donor,
        SmoPackedVisualGraph packed,
        IReadOnlyDictionary<int, uint> newIdsByDonorIndex,
        IReadOnlyDictionary<uint, uint> referenceIds,
        IReadOnlySet<uint> relocatedObjectIds,
        IReadOnlySet<uint> modifiedTargetOwners)
    {
        SmoDocument output = SmoDocument.Parse(packed.Data);
        var errors = new List<string>();
        if (output.HasErrors)
            errors.Add("strict parser reported errors");
        if (output.Objects.Select(entry => entry.Id).Distinct().Count() != output.Objects.Count)
            errors.Add("object IDs are not unique");

        Dictionary<uint, SmoObjectEntry> outputById = output.Objects.ToDictionary(entry => entry.Id);
        foreach (SmoObjectEntry source in target.Objects)
        {
            if (!outputById.TryGetValue(source.Id, out SmoObjectEntry? retained) ||
                retained.TypeHash != source.TypeHash || retained.Name != source.Name)
            {
                errors.Add($"target object ID {source.Id} identity changed");
                continue;
            }
            uint? sourceParentId = source.ParentIndex is int sourceParent
                ? target.Objects[sourceParent].Id
                : null;
            uint? retainedParentId = retained.ParentIndex is int retainedParent
                ? output.Objects[retainedParent].Id
                : null;
            if (sourceParentId != retainedParentId)
                errors.Add($"target object ID {source.Id} parent changed");
        }

        foreach (SmoObjectEntry targetMesh in target.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.MeshData))
        {
            if (!outputById.TryGetValue(targetMesh.Id, out SmoObjectEntry? disabledMesh))
                continue;
            SmoMesh mesh = SmoMeshDecoder.Decode(output, disabledMesh);
            if (!IsStrictlyDegenerate(mesh))
                errors.Add($"legacy target mesh ID {targetMesh.Id} is still visible");
        }

        foreach (SmoObjectEntry donorMesh in donor.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.MeshData &&
                     newIdsByDonorIndex.ContainsKey(entry.Index)))
        {
            uint newId = newIdsByDonorIndex[donorMesh.Index];
            if (!outputById.TryGetValue(newId, out SmoObjectEntry? packedMesh) ||
                !ObjectBytes(output, packedMesh).SequenceEqual(ObjectBytes(donor, donorMesh)))
                errors.Add($"donor mesh [{donorMesh.Index}] payload changed");
        }
        foreach (SmoObjectEntry donorTexture in donor.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.TextureData &&
                     newIdsByDonorIndex.ContainsKey(entry.Index)))
        {
            uint newId = newIdsByDonorIndex[donorTexture.Index];
            if (!outputById.TryGetValue(newId, out SmoObjectEntry? packedTexture) ||
                !ObjectBytes(output, packedTexture).SequenceEqual(ObjectBytes(donor, donorTexture)))
                errors.Add($"donor texture [{donorTexture.Index}] payload changed");
        }

        HashSet<uint> targetNodeIds = target.Objects.Where(entry => entry.TypeHash == SmoClassIds.Node)
            .Select(entry => entry.Id).ToHashSet();
        foreach (uint packedId in packed.PackedObjectIds)
        {
            if (!outputById.TryGetValue(packedId, out SmoObjectEntry? entry) ||
                entry.TypeHash != SmoClassIds.Skin)
                continue;
            if (!SmoSkinDecoder.TryDecode(output, entry, out SmoSkin? skin, out string error) ||
                skin is null)
            {
                errors.Add($"packed skin ID {packedId} is invalid: {error}");
                continue;
            }
            if (skin.Bones.Any(bone => bone.InlineSerializedSize != 0 ||
                                       !targetNodeIds.Contains(bone.NodeObjectId)))
                errors.Add($"packed skin ID {packedId} does not use reference-only target nodes");
        }
        if (output.Objects.Any(entry => entry.TypeHash == SmoClassIds.Node &&
                                        packed.PackedObjectIds.Contains(entry.Id)))
            errors.Add("a donor node object was copied into the packed visual graph");

        IReadOnlyDictionary<int, SmoTextureBinding> donorBindings =
            SmoTextureBindingResolver.ResolveAll(donor);
        IReadOnlyDictionary<int, SmoTextureBinding> bindings =
            SmoTextureBindingResolver.ResolveAll(output);
        foreach (SmoObjectEntry donorMesh in donor.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.MeshData &&
                     newIdsByDonorIndex.ContainsKey(entry.Index)))
        {
            uint meshId = newIdsByDonorIndex[donorMesh.Index];
            SmoObjectEntry mesh = outputById[meshId];
            bindings.TryGetValue(mesh.Index, out SmoTextureBinding? binding);
            donorBindings.TryGetValue(donorMesh.Index, out SmoTextureBinding? donorBinding);
            if (donorBinding?.Texture is not null && donorBinding.Issue is null)
            {
                SmoObjectEntry donorTexture = donor.Objects[donorBinding.Texture.ObjectIndex];
                uint expectedTextureId = newIdsByDonorIndex[donorTexture.Index];
                uint? actualTextureId = binding?.Texture is not null
                    ? output.Objects[binding.Texture.ObjectIndex].Id
                    : null;
                if (binding?.Issue is not null || actualTextureId != expectedTextureId)
                {
                    string expectedTexture =
                        $"{expectedTextureId} ({donorTexture.Name})";
                    string actualTexture = binding?.Texture is not null
                        ? $"{actualTextureId} ({binding.Texture.Name})"
                        : "none";
                    errors.Add(
                        $"packed donor mesh ID {meshId} ({mesh.Name}) texture binding " +
                        $"differs from donor [{donorMesh.Index}]: expected " +
                        $"{expectedTexture}, actual {actualTexture}, " +
                        $"issue={binding?.Issue ?? "none"}");
                }
            }
            else if (binding?.Texture is not null &&
                     !packed.PackedTextureIds.Contains(
                         output.Objects[binding.Texture.ObjectIndex].Id))
            {
                errors.Add(
                    $"packed donor mesh ID {meshId} unexpectedly uses a legacy target texture");
            }
        }

        VerifyKnownReferences(
            donor,
            output,
            newIdsByDonorIndex,
            referenceIds,
            errors);
        VerifyRelocatedReferencesAreBackward(
            output,
            relocatedObjectIds,
            errors);
        VerifyNoForwardKnownReferences(
            output,
            packed.PackedObjectIds,
            errors);
        VerifyPackedInlineObjectsResolve(output, packed.PackedObjectIds, errors);
        VerifyCatalogInlinePrefixes(output, errors);
        VerifyStableTargetObjects(target, output, modifiedTargetOwners, errors);
        if (errors.Count > 0)
            throw new InvalidDataException(
                "Packed donor visual graph failed verification: " + string.Join("; ", errors.Distinct()) + ".");
    }

    private static void VerifyRelocatedReferencesAreBackward(
        SmoDocument output,
        IReadOnlySet<uint> relocatedObjectIds,
        ICollection<string> errors)
    {
        if (relocatedObjectIds.Count == 0)
            return;
        Dictionary<uint, SmoObjectEntry> outputById = output.Objects
            .ToDictionary(entry => entry.Id);
        var inlineDefinitions = relocatedObjectIds.ToDictionary(id => id, _ => 0);
        foreach (SmoObjectEntry material in output.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.MaterialData))
        {
            ReadOnlySpan<byte> serialized = ObjectBytes(output, material);
            foreach (SmoDataBlockHeader field in ReadFields(serialized).Where(field =>
                         field.FieldType == 10 && field.PayloadSize >= ObjectReferenceSize))
            {
                ReadOnlySpan<byte> payload = serialized.Slice(
                    field.PayloadOffset, ObjectReferenceSize);
                uint objectId = BinaryPrimitives.ReadUInt32LittleEndian(payload);
                uint inlineSize = BinaryPrimitives.ReadUInt32LittleEndian(payload[4..]);
                if (!relocatedObjectIds.Contains(objectId))
                    continue;
                if (!outputById.TryGetValue(objectId, out SmoObjectEntry? texture) ||
                    texture.TypeHash != SmoClassIds.TextureData)
                {
                    errors.Add($"relocated texture ID {objectId} does not resolve");
                    continue;
                }
                if (inlineSize == 0)
                {
                    if (field.PayloadSize != ObjectReferenceSize ||
                        texture.PhysicalOffset >=
                            material.PhysicalOffset + field.PayloadOffset)
                    {
                        errors.Add(
                            $"material ID {material.Id} has a forward relocated-texture " +
                            $"reference to ID {objectId}");
                    }
                }
                else if (inlineSize != texture.SerializedSize ||
                         field.PayloadSize != inlineSize + ObjectReferenceSize ||
                         material.PhysicalOffset + field.PayloadOffset + ObjectReferenceSize !=
                             texture.PhysicalOffset)
                {
                    errors.Add(
                        $"material ID {material.Id} has an invalid inline relocated texture " +
                        $"ID {objectId}");
                }
                else
                {
                    inlineDefinitions[objectId]++;
                }
            }
        }
        foreach ((uint objectId, int count) in inlineDefinitions)
        {
            if (count != 1)
                errors.Add($"relocated texture ID {objectId} has {count} inline definitions");
        }
    }

    private static void VerifyNoForwardKnownReferences(
        SmoDocument output,
        IReadOnlySet<uint> packedObjectIds,
        ICollection<string> errors)
    {
        Dictionary<uint, SmoObjectEntry> outputById = output.Objects
            .ToDictionary(entry => entry.Id);
        foreach (SmoObjectEntry owner in output.Objects.Where(entry =>
                     packedObjectIds.Contains(entry.Id)))
        {
            ReadOnlySpan<byte> serialized = ObjectBytes(output, owner);
            foreach (SmoDataBlockHeader field in ReadFields(serialized))
            {
                if (owner.TypeHash == TextureSequenceClassId && field.FieldType == 0 &&
                    field.PayloadSize != 0)
                {
                    foreach (TextureSequenceReference textureReference in
                             ReadTextureSequenceReferences(
                                 output,
                                 owner,
                                 serialized,
                                 field,
                                 outputById))
                    {
                        if (textureReference.InlineSize == 0 &&
                            outputById[textureReference.ObjectId].PhysicalOffset >=
                            owner.PhysicalOffset + textureReference.Offset)
                        {
                            errors.Add(
                                $"packed texture sequence ID {owner.Id} has a forward " +
                                $"reference to ID {textureReference.ObjectId}");
                        }
                    }
                    continue;
                }
                if (field.PayloadSize < ObjectReferenceSize)
                    continue;
                ReadOnlySpan<byte> payload = serialized.Slice(
                    field.PayloadOffset, ObjectReferenceSize);
                uint objectId = BinaryPrimitives.ReadUInt32LittleEndian(payload);
                uint inlineSize = BinaryPrimitives.ReadUInt32LittleEndian(payload[4..]);
                if (inlineSize != 0 ||
                    !outputById.TryGetValue(objectId, out SmoObjectEntry? referenced) ||
                    !IsConfirmedObjectReference(owner, referenced, field, inlineSize))
                {
                    continue;
                }
                if (referenced.PhysicalOffset >= owner.PhysicalOffset + field.PayloadOffset)
                {
                    errors.Add(
                        $"packed object ID {owner.Id} has a forward confirmed reference " +
                        $"to ID {objectId}");
                }
            }

            if (owner.TypeHash == SmoClassIds.Skin &&
                SmoSkinDecoder.TryDecode(
                    output, owner, out SmoSkin? skin, out _) && skin is not null)
            {
                foreach (SmoSkinBone bone in skin.Bones.Where(bone =>
                             bone.InlineSerializedSize == 0))
                {
                    SmoObjectEntry referenced = output.Objects[bone.NodeObjectIndex];
                    if (referenced.PhysicalOffset >= owner.PhysicalOffset)
                    {
                        errors.Add(
                            $"packed skin ID {owner.Id} has a forward palette reference " +
                            $"to node ID {referenced.Id}");
                    }
                }
            }
        }
    }

    private static void VerifyKnownReferences(
        SmoDocument donor,
        SmoDocument output,
        IReadOnlyDictionary<int, uint> newIdsByDonorIndex,
        IReadOnlyDictionary<uint, uint> referenceIds,
        ICollection<string> errors)
    {
        Dictionary<uint, SmoObjectEntry> donorById = donor.Objects
            .GroupBy(entry => entry.Id).Where(group => group.Count() == 1)
            .ToDictionary(group => group.Key, group => group.Single());
        Dictionary<uint, SmoObjectEntry> outputById = output.Objects.ToDictionary(entry => entry.Id);
        foreach ((int donorIndex, uint packedId) in newIdsByDonorIndex)
        {
            SmoObjectEntry source = donor.Objects[donorIndex];
            SmoObjectEntry packed = outputById[packedId];
            ReadOnlySpan<byte> sourceBytes = ObjectBytes(donor, source);
            ReadOnlySpan<byte> packedBytes = ObjectBytes(output, packed);
            int paletteOffset = -1;
            if (source.TypeHash == SmoClassIds.Skin &&
                SmoSkinDecoder.TryDecode(donor, source, out SmoSkin? sourceSkin, out _) &&
                sourceSkin is not null)
            {
                paletteOffset = FindPaletteField(sourceBytes, sourceSkin.Bones.Count).Offset;
            }
            SmoDataBlockHeader[] sourceFields = ReadFields(sourceBytes);
            SmoDataBlockHeader[] packedFields = ReadFields(packedBytes);
            if (sourceFields.Length != packedFields.Length)
            {
                errors.Add($"packed object ID {packedId} changed its field count");
                continue;
            }
            for (int fieldIndex = 0; fieldIndex < sourceFields.Length; fieldIndex++)
            {
                SmoDataBlockHeader field = sourceFields[fieldIndex];
                SmoDataBlockHeader packedField = packedFields[fieldIndex];
                if (source.TypeHash == TextureSequenceClassId && field.FieldType == 0 &&
                    field.PayloadSize != 0)
                {
                    TextureSequenceReference[] sourceReferences =
                        ReadTextureSequenceReferences(
                            donor,
                            source,
                            sourceBytes,
                            field,
                            donorById);
                    TextureSequenceReference[] packedReferences =
                        ReadTextureSequenceReferences(
                            output,
                            packed,
                            packedBytes,
                            packedField,
                            outputById);
                    if (sourceReferences.Length != packedReferences.Length)
                    {
                        errors.Add($"packed texture sequence ID {packedId} changed its count");
                        continue;
                    }
                    for (int referenceIndex = 0;
                         referenceIndex < sourceReferences.Length;
                         referenceIndex++)
                    {
                        TextureSequenceReference sourceReference =
                            sourceReferences[referenceIndex];
                        TextureSequenceReference packedReference =
                            packedReferences[referenceIndex];
                        if (!referenceIds.TryGetValue(
                                sourceReference.ObjectId, out uint expected) ||
                            packedReference.ObjectId != expected ||
                            packedReference.InlineSize != sourceReference.InlineSize)
                        {
                            errors.Add(
                                $"packed texture sequence ID {packedId} retains donor " +
                                $"reference {sourceReference.ObjectId} at sample {referenceIndex}");
                        }
                    }
                }
                else if (field.Offset != paletteOffset && field.PayloadSize >= 8)
                {
                    ReadOnlySpan<byte> payload = sourceBytes.Slice(
                        field.PayloadOffset, checked((int)field.PayloadSize));
                    uint oldId = BinaryPrimitives.ReadUInt32LittleEndian(payload);
                    uint inlineSize = BinaryPrimitives.ReadUInt32LittleEndian(payload[4..]);
                    if (donorById.TryGetValue(oldId, out SmoObjectEntry? referenced) &&
                        IsConfirmedObjectReference(source, referenced, field, inlineSize) &&
                        referenceIds.TryGetValue(oldId, out uint expected))
                    {
                        if (packedField.PayloadSize < ObjectReferenceSize ||
                            BinaryPrimitives.ReadUInt32LittleEndian(
                                packedBytes[packedField.PayloadOffset..]) != expected)
                        {
                            errors.Add(
                                $"packed object ID {packedId} retains donor reference {oldId}");
                        }
                    }
                }
            }
        }
    }

    private static SmoDataBlockHeader[] ReadFields(ReadOnlySpan<byte> serialized)
    {
        var result = new List<SmoDataBlockHeader>();
        int offset = ObjectSignatureSize;
        while (offset < serialized.Length &&
               SmoDataBlockReader.TryReadHeader(serialized, offset, out SmoDataBlockHeader field))
        {
            result.Add(field);
            offset = checked((int)field.PayloadEnd);
        }
        if (offset != serialized.Length)
            throw new InvalidDataException("Object field stream is truncated.");
        return result.ToArray();
    }

    private static bool IsConfirmedObjectReference(
        SmoObjectEntry owner,
        SmoObjectEntry referenced,
        SmoDataBlockHeader field,
        uint inlineSize)
    {
        // Inline objects are unambiguous only when the complete field payload is
        // [id][serializedSize][object bytes] and the catalog points at exactly
        // those bytes. Reference-only links are restricted to observed PC
        // serializer class/field pairs. Do not broaden
        // this to arbitrary [uint,uint] payloads: mesh data can contain the same
        // bit pattern by accident.
        bool inlineObject = inlineSize == referenced.SerializedSize &&
            field.PayloadSize == referenced.SerializedSize + ObjectReferenceSize &&
            owner.PhysicalOffset + field.PayloadOffset + ObjectReferenceSize ==
            referenced.PhysicalOffset;
        bool referenceOnly = inlineSize == 0 &&
            field.PayloadSize == ObjectReferenceSize &&
            (owner.TypeHash == SmoClassIds.Skin && field.FieldType == 0 &&
                 referenced.TypeHash == SmoClassIds.MaterialData ||
             owner.TypeHash == SmoClassIds.Skin && field.FieldType == 1 &&
                 referenced.TypeHash == SharedVisualHelperClassId ||
             owner.TypeHash == SmoClassIds.MaterialData && field.FieldType == 10 &&
                 referenced.TypeHash == SmoClassIds.TextureData ||
             owner.TypeHash == SmoClassIds.Model && field.FieldType == 1 &&
                 referenced.TypeHash == SharedVisualHelperClassId);
        return inlineObject || referenceOnly;
    }

    private static void VerifyPackedInlineObjectsResolve(
        SmoDocument output,
        IReadOnlySet<uint> packedIds,
        ICollection<string> errors)
    {
        Dictionary<uint, SmoObjectEntry> byId = output.Objects.ToDictionary(entry => entry.Id);
        foreach (SmoObjectEntry owner in output.Objects.Where(entry => packedIds.Contains(entry.Id)))
        {
            ReadOnlySpan<byte> serialized = ObjectBytes(output, owner);
            int offset = ObjectSignatureSize;
            while (offset < serialized.Length &&
                   SmoDataBlockReader.TryReadHeader(serialized, offset, out SmoDataBlockHeader field))
            {
                if (field.PayloadSize >= 16)
                {
                    ReadOnlySpan<byte> payload = serialized.Slice(
                        field.PayloadOffset, checked((int)field.PayloadSize));
                    uint childId = BinaryPrimitives.ReadUInt32LittleEndian(payload);
                    uint childSize = BinaryPrimitives.ReadUInt32LittleEndian(payload[4..]);
                    bool hasObjectSignature = payload.Length >= 16 &&
                        payload.Slice(12, 4).SequenceEqual("SBOO"u8);
                    if (hasObjectSignature && childSize + 8 == field.PayloadSize)
                    {
                        if (!byId.TryGetValue(childId, out SmoObjectEntry? child) ||
                            child.SerializedSize != childSize ||
                            child.PhysicalOffset != owner.PhysicalOffset +
                                field.PayloadOffset + ObjectReferenceSize)
                        {
                            errors.Add(
                                $"packed object ID {owner.Id} contains an unresolved inline child {childId}");
                        }
                    }
                }
                offset = checked((int)field.PayloadEnd);
            }
        }
    }

    private static void VerifyCatalogInlinePrefixes(
        SmoDocument output,
        ICollection<string> errors)
    {
        foreach (SmoObjectEntry entry in output.Objects.Where(item => item.ParentIndex is not null))
        {
            long prefixOffset = entry.PhysicalOffset - ObjectReferenceSize;
            if (prefixOffset < output.Header.DataStart || prefixOffset > int.MaxValue ||
                BinaryPrimitives.ReadUInt32LittleEndian(
                    output.Data.Span[checked((int)prefixOffset)..]) != entry.Id ||
                BinaryPrimitives.ReadUInt32LittleEndian(
                    output.Data.Span[checked((int)prefixOffset + 4)..]) != entry.SerializedSize)
            {
                errors.Add($"catalog object ID {entry.Id} has a stale inline prefix");
            }
        }
    }

    private static void VerifyStableTargetObjects(
        SmoDocument target,
        SmoDocument output,
        IReadOnlySet<uint> modifiedTargetOwners,
        ICollection<string> errors)
    {
        HashSet<int> mutable = target.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.MeshData)
            .SelectMany(entry => AncestorsAndSelf(target, entry.Index))
            .ToHashSet();
        foreach (uint ownerId in modifiedTargetOwners)
        {
            SmoObjectEntry owner = target.Objects.Single(entry => entry.Id == ownerId);
            mutable.UnionWith(AncestorsAndSelf(target, owner.Index));
        }
        Dictionary<uint, SmoObjectEntry> outputById = output.Objects.ToDictionary(entry => entry.Id);
        foreach (SmoObjectEntry source in target.Objects.Where(entry => !mutable.Contains(entry.Index)))
        {
            if (!ObjectBytes(target, source).SequenceEqual(ObjectBytes(output, outputById[source.Id])))
                errors.Add($"stable target object ID {source.Id} bytes changed");
        }
    }

    private static IEnumerable<int> AncestorsAndSelf(SmoDocument document, int index)
    {
        int? cursor = index;
        while (cursor is int value)
        {
            yield return value;
            cursor = document.Objects[value].ParentIndex;
        }
    }

    private static ReadOnlySpan<byte> ObjectBytes(SmoDocument document, SmoObjectEntry entry) =>
        document.Data.Span.Slice(
            checked((int)entry.PhysicalOffset), checked((int)entry.SerializedSize));

    private static bool ObjectBytesEqual(
        SmoDocument firstDocument,
        SmoObjectEntry first,
        SmoDocument secondDocument,
        SmoObjectEntry second) =>
        first.SerializedSize == second.SerializedSize &&
        ObjectBytes(firstDocument, first).SequenceEqual(ObjectBytes(secondDocument, second));

    private static bool IsStrictlyDegenerate(SmoMesh mesh)
        => mesh.StripIndices.Length >= 3 &&
           mesh.StripIndices.All(index => index == mesh.StripIndices[0]) &&
           mesh.TriangleIndices.Length == 0;

    private static bool Contains(SmoObjectEntry parent, SmoObjectEntry child) =>
        parent.LogicalOffset <= child.LogicalOffset && child.LogicalEnd <= parent.LogicalEnd;

    private static void WritePayloadSize(
        Span<byte> data,
        int headerOffset,
        SmoDataBlockHeader original,
        uint value)
    {
        int sizeEnd = headerOffset + original.HeaderSize;
        switch (original.SizeKind)
        {
            case SmoDataBlockSizeCode.UInt8:
                data[sizeEnd - 1] = checked((byte)value);
                break;
            case SmoDataBlockSizeCode.UInt16:
                BinaryPrimitives.WriteUInt16LittleEndian(
                    data[(sizeEnd - sizeof(ushort))..], checked((ushort)value));
                break;
            case SmoDataBlockSizeCode.UInt32:
                WriteUInt32(data, sizeEnd - sizeof(uint), value);
                break;
            default:
                throw new InvalidOperationException(
                    $"Resized ancestor field uses non-writable size form {original.SizeKind}.");
        }
    }

    private static void WriteMatrix(Span<byte> data, Matrix4x4 value)
    {
        float[] cells =
        [
            value.M11, value.M12, value.M13, value.M14,
            value.M21, value.M22, value.M23, value.M24,
            value.M31, value.M32, value.M33, value.M34,
            value.M41, value.M42, value.M43, value.M44
        ];
        for (int index = 0; index < cells.Length; index++)
        {
            BinaryPrimitives.WriteInt32LittleEndian(
                data[(index * sizeof(float))..],
                BitConverter.SingleToInt32Bits(cells[index]));
        }
    }

    private static void WriteUInt32(Stream stream, uint value)
    {
        Span<byte> data = stackalloc byte[sizeof(uint)];
        BinaryPrimitives.WriteUInt32LittleEndian(data, value);
        stream.Write(data);
    }

    private static void WriteUInt32(Span<byte> data, int offset, uint value) =>
        BinaryPrimitives.WriteUInt32LittleEndian(data[offset..], value);

    private sealed record BuiltBranch(
        SmoObjectEntry Root,
        byte[] Data,
        IReadOnlyList<BranchEntry> Entries);

    private sealed record BranchEntry(
        SmoObjectEntry Source,
        int RelativeOffset,
        uint SerializedSize);

    private sealed record PrimarySkinAttachment(
        SmoObjectEntry Skin,
        SmoVisualForestAttachment Attachment);

    private readonly record struct TextureSequenceReference(
        int Offset,
        uint ObjectId,
        uint InlineSize);

    private sealed record InlineObjectRelocation(
        SmoObjectEntry Object,
        SmoObjectEntry SourceOwner,
        SmoObjectEntry SourceSkin,
        SmoObjectEntry DestinationOwner,
        SmoObjectEntry DestinationRigidRoot);

}

internal static partial class SmoVisualTransplanter
{
    internal static byte[] DisableAllTargetMeshes(SmoDocument target)
    {
        byte[] result = target.Data.ToArray();
        SmoObjectEntry[] meshEntries = target.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.MeshData)
            .ToArray();
        foreach (SmoObjectEntry entry in meshEntries)
        {
            SmoMesh mesh = SmoMeshDecoder.Decode(target, entry);
            long byteCount = checked((long)mesh.IndexCount * sizeof(ushort));
            if (mesh.IndexCount < 3 || mesh.IndexDataOffset < entry.PhysicalOffset ||
                mesh.IndexDataOffset + byteCount > entry.PhysicalEnd ||
                mesh.IndexDataOffset > int.MaxValue || byteCount > int.MaxValue)
            {
                throw new InvalidOperationException(
                    $"Target mesh [{entry.Index}] has an unsafe index-buffer interval.");
            }
            result.AsSpan(checked((int)mesh.IndexDataOffset), checked((int)byteCount)).Clear();
        }

        SmoDocument verified = SmoDocument.Parse(result, target.SourcePath);
        if (verified.HasErrors)
            throw new InvalidDataException("Degenerate target mesh rewrite failed strict parsing.");
        foreach (SmoObjectEntry entry in verified.Objects.Where(item =>
                     item.TypeHash == SmoClassIds.MeshData))
        {
            SmoMesh mesh = SmoMeshDecoder.Decode(verified, entry);
            if (mesh.StripIndices.Length < 3 ||
                mesh.StripIndices.Any(index => index != mesh.StripIndices[0]) ||
                mesh.TriangleIndices.Length != 0)
            {
                throw new InvalidOperationException(
                    $"Target mesh [{entry.Index}] was not made strictly degenerate.");
            }
        }
        if (verified.Objects.Count(entry => entry.TypeHash == SmoClassIds.MeshData) !=
            meshEntries.Length)
            throw new InvalidOperationException(
                "The degenerate rewrite changed the target mesh catalog.");
        return result;
    }
}
