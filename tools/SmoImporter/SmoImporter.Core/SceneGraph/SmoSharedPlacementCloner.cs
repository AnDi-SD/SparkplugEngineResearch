using System.Buffers.Binary;
using System.Numerics;
using SmoViewer.Core;

namespace SmoImporter.Core;

/// <summary>
/// Adds a lightweight level placement by cloning a confirmed reference-only
/// spStaticRenderObject branch. The physical spMeshData is never copied.
/// New branches are appended beside their template inside the same partition
/// owner, preserving the native room/sector graph.
/// </summary>
public static class SmoSharedPlacementCloner
{
    public static SmoSharedPlacementCloneResult Clone(
        SmoDocument document,
        int sourceMeshObjectIndex,
        Matrix4x4 worldTransform,
        string? placementName = null)
    {
        ArgumentNullException.ThrowIfNull(document);
        if ((uint)sourceMeshObjectIndex >= (uint)document.Objects.Count ||
            document.Objects[sourceMeshObjectIndex].TypeHash != SmoClassIds.MeshData)
        {
            throw new ArgumentOutOfRangeException(
                nameof(sourceMeshObjectIndex),
                "Source object is not an spMeshData resource.");
        }
        uint sourceMeshObjectId = document.Objects[sourceMeshObjectIndex].Id;
        if (!Matrix4x4.Invert(worldTransform, out _))
            throw new ArgumentException(
                "A shared placement needs an invertible world transform.",
                nameof(worldTransform));
        Matrix4x4 inverse =
            SmoPlacementTransformWriter.CreateLegacyStaticInverseTransform(worldTransform);

        IReadOnlyList<SmoSharedMeshInstanceInfo> instances =
            SmoSharedMeshInstanceResolver.ResolveAll(document);
        SmoSharedMeshInstanceInfo? template = instances.FirstOrDefault(instance =>
            instance.SourceMeshObjectIndex == sourceMeshObjectIndex);
        if (template is null)
        {
            return CloneEmbeddedPlacement(
                document,
                sourceMeshObjectIndex,
                worldTransform,
                inverse,
                placementName);
        }
        SmoObjectEntry sourceStatic = document.Objects[template.StaticObjectIndex];
        SmoObjectEntry sourceParent = sourceStatic.ParentIndex is int parentIndex
            ? document.Objects[parentIndex]
            : throw new InvalidOperationException(
                "The shared placement template has no serialized parent field.");
        SmoSharedMeshInstanceInfo targetTemplate = SelectNearestTemplate(
            instances,
            worldTransform);
        SmoObjectEntry targetStatic = document.Objects[targetTemplate.StaticObjectIndex];
        SmoObjectEntry targetParent = targetStatic.ParentIndex is int targetParentIndex
            ? document.Objects[targetParentIndex]
            : throw new InvalidOperationException(
                "The nearest placement shell has no serialized parent field.");
        (int fieldPhysicalOffset, int fieldLength) = FindInlineField(
            document,
            sourceParent,
            sourceStatic);
        byte[] fieldData = document.Data.Span
            .Slice(fieldPhysicalOffset, fieldLength)
            .ToArray();
        SmoObjectEntry[] branchEntries = document.Objects
            .Where(entry => entry.PhysicalOffset >= sourceStatic.PhysicalOffset &&
                entry.PhysicalEnd <= sourceStatic.PhysicalEnd)
            .OrderBy(entry => entry.PhysicalOffset)
            .ToArray();
        if (branchEntries.Length == 0 || branchEntries[0].Index != sourceStatic.Index)
            throw new InvalidOperationException(
                "The shared placement branch could not be enumerated safely.");

        uint nextId = document.Objects.Max(entry => entry.Id);
        var idMap = new Dictionary<uint, uint>();
        foreach (SmoObjectEntry entry in branchEntries)
        {
            do
            {
                nextId = checked(nextId + 1);
            }
            while (document.Objects.Any(candidate => candidate.Id == nextId) ||
                   idMap.ContainsValue(nextId));
            idMap.Add(entry.Id, nextId);
        }

        var generatedEntries = new List<SmoVisualForestEntry>(branchEntries.Length);
        foreach (SmoObjectEntry entry in branchEntries)
        {
            int relativeObjectOffset = checked(
                (int)(entry.PhysicalOffset - fieldPhysicalOffset));
            int relativePrefixOffset = relativeObjectOffset - 8;
            if (relativePrefixOffset < 0 ||
                BinaryPrimitives.ReadUInt32LittleEndian(
                    fieldData.AsSpan(relativePrefixOffset)) != entry.Id)
            {
                throw new InvalidOperationException(
                    $"Object [{entry.Index}] has no confirmed inline prefix in " +
                    "the placement branch.");
            }
            WriteUInt32(fieldData, relativePrefixOffset, idMap[entry.Id]);
            byte[] rawName = entry.RawName.ToArray();
            if (entry.Index == sourceStatic.Index &&
                !string.IsNullOrWhiteSpace(placementName))
            {
                // Directory names are metadata and may differ in length from the
                // serialized branch without changing its object bytes.
                rawName = System.Text.Encoding.UTF8.GetBytes(
                    placementName.Trim() + "\0");
            }
            generatedEntries.Add(new SmoVisualForestEntry(
                idMap[entry.Id],
                rawName,
                entry.TypeHash,
                relativeObjectOffset,
                entry.SerializedSize));
        }

        PatchStaticMatrices(
            fieldData,
            checked((int)(sourceStatic.PhysicalOffset - fieldPhysicalOffset)),
            checked((int)sourceStatic.SerializedSize),
            worldTransform,
            inverse);
        var attachment = new SmoVisualForestAttachment(
            targetParent.Id,
            fieldData,
            generatedEntries);
        byte[] output = SmoVisualForestInjector.Inject(
            document,
            targetParent.Id,
            [attachment]);
        SmoDocument verified = SmoDocument.ParseOwned(output, document.SourcePath);
        int newStaticIndex = verified.Objects.Single(entry =>
            entry.Id == idMap[sourceStatic.Id]).Index;
        SmoSharedMeshInstanceInfo instance =
            SmoSharedMeshInstanceResolver.ResolveAll(verified).Single(candidate =>
                candidate.StaticObjectIndex == newStaticIndex);
        int verifiedSourceMeshObjectIndex = verified.Objects.Single(entry =>
            entry.Id == sourceMeshObjectId).Index;
        float matrixDifference = MatrixMaxDifference(
            instance.WorldTransform,
            worldTransform);
        if (instance.SourceMeshObjectIndex != verifiedSourceMeshObjectIndex ||
            matrixDifference > 0.01f)
        {
            throw new InvalidDataException(
                "The cloned placement failed reference or transform verification. " +
                $"Mesh {instance.SourceMeshObjectIndex}/{verifiedSourceMeshObjectIndex}, " +
                $"maximum matrix difference {matrixDifference:R}.");
        }

        return new SmoSharedPlacementCloneResult(
            output,
            newStaticIndex,
            instance.ModelObjectIndex,
            verifiedSourceMeshObjectIndex,
            instance.WorldTransform,
            branchEntries.Length);
    }

    public static bool CanClone(SmoDocument document, int sourceMeshObjectIndex)
    {
        if ((uint)sourceMeshObjectIndex >= (uint)document.Objects.Count ||
            document.Objects[sourceMeshObjectIndex].TypeHash != SmoClassIds.MeshData)
            return false;
        if (SmoSharedMeshInstanceResolver.ResolveAll(document).Any(instance =>
                instance.SourceMeshObjectIndex == sourceMeshObjectIndex))
            return true;
        return FindAncestor(
                   document,
                   document.Objects[sourceMeshObjectIndex],
                   SmoClassIds.StaticRenderObject) is not null ||
               FindAncestor(
                   document,
                   document.Objects[sourceMeshObjectIndex],
                   SmoClassIds.Model) is not null &&
               SmoSharedMeshInstanceResolver.ResolveAll(document).Count > 0;
    }

    private static SmoSharedPlacementCloneResult CloneEmbeddedPlacement(
        SmoDocument document,
        int sourceMeshObjectIndex,
        Matrix4x4 worldTransform,
        Matrix4x4 inverse,
        string? placementName)
    {
        SmoObjectEntry selectedMesh = document.Objects[sourceMeshObjectIndex];
        uint sourceMeshObjectId = selectedMesh.Id;
        SmoObjectEntry? sourceStatic = FindAncestor(
                document,
                selectedMesh,
                SmoClassIds.StaticRenderObject);
        if (sourceStatic is null)
        {
            return CloneNodeModelPlacement(
                document,
                sourceMeshObjectIndex,
                worldTransform,
                placementName);
        }
        SmoObjectEntry sourceParent = sourceStatic.ParentIndex is int parentIndex
            ? document.Objects[parentIndex]
            : throw new InvalidOperationException(
                "The embedded placement template has no serialized parent field.");
        (int fieldPhysicalOffset, int fieldLength) = FindInlineField(
            document,
            sourceParent,
            sourceStatic);
        byte[] fieldData = document.Data.Span
            .Slice(fieldPhysicalOffset, fieldLength)
            .ToArray();
        SmoObjectEntry[] branchEntries = document.Objects
            .Where(entry => entry.PhysicalOffset >= sourceStatic.PhysicalOffset &&
                            entry.PhysicalEnd <= sourceStatic.PhysicalEnd)
            .OrderBy(entry => entry.PhysicalOffset)
            .ToArray();
        var existingIds = document.Objects.Select(entry => entry.Id).ToHashSet();
        uint nextId = existingIds.Max();
        var idMap = new Dictionary<uint, uint>();
        foreach (SmoObjectEntry entry in branchEntries)
        {
            do nextId = checked(nextId + 1);
            while (existingIds.Contains(nextId) || idMap.ContainsValue(nextId));
            idMap.Add(entry.Id, nextId);
        }

        var generatedEntries = new List<SmoVisualForestEntry>(branchEntries.Length);
        foreach (SmoObjectEntry entry in branchEntries)
        {
            int relativeObjectOffset = checked(
                (int)(entry.PhysicalOffset - fieldPhysicalOffset));
            int relativePrefixOffset = relativeObjectOffset - 8;
            if (relativePrefixOffset < 0 ||
                BinaryPrimitives.ReadUInt32LittleEndian(
                    fieldData.AsSpan(relativePrefixOffset)) != entry.Id)
                throw new InvalidOperationException(
                    $"Object [{entry.Index}] has no confirmed inline prefix in the embedded branch.");
            WriteUInt32(fieldData, relativePrefixOffset, idMap[entry.Id]);
            byte[] rawName = entry.RawName.ToArray();
            if (entry.Index == sourceStatic.Index &&
                !string.IsNullOrWhiteSpace(placementName))
                rawName = System.Text.Encoding.UTF8.GetBytes(placementName.Trim() + "\0");
            generatedEntries.Add(new SmoVisualForestEntry(
                idMap[entry.Id],
                rawName,
                entry.TypeHash,
                relativeObjectOffset,
                entry.SerializedSize));
        }

        PatchStaticMatrices(
            fieldData,
            checked((int)(sourceStatic.PhysicalOffset - fieldPhysicalOffset)),
            checked((int)sourceStatic.SerializedSize),
            worldTransform,
            inverse);
        byte[] output = SmoVisualForestInjector.Inject(
            document,
            sourceParent.Id,
            [new SmoVisualForestAttachment(
                sourceParent.Id,
                fieldData,
                generatedEntries)]);

        foreach (SmoObjectEntry resource in branchEntries
                     .Where(entry => entry.TypeHash is
                         SmoClassIds.TextureData or SmoClassIds.MeshData)
                     .OrderBy(entry => entry.TypeHash == SmoClassIds.TextureData ? 0 : 1))
        {
            SmoDocument current = SmoDocument.ParseOwned(output, document.SourcePath);
            uint clonedId = idMap[resource.Id];
            SmoObjectEntry cloned = current.Objects.Single(entry => entry.Id == clonedId);
            if (cloned.ParentIndex is not int ownerIndex ||
                current.Objects.Any(entry => entry.ParentIndex == cloned.Index))
                continue;
            uint ownerId = current.Objects[ownerIndex].Id;
            byte fieldType = resource.TypeHash == SmoClassIds.TextureData
                ? (byte)10
                : (byte)0;
            output = SmoVisualForestInjector.DemoteInlineLeafToReference(
                current,
                ownerId,
                fieldType,
                clonedId);
            current = SmoDocument.ParseOwned(output, document.SourcePath);
            output = SmoVisualForestInjector.ReplaceReferenceId(
                current,
                ownerId,
                fieldType,
                clonedId,
                resource.Id);
        }

        SmoDocument verified = SmoDocument.ParseOwned(output, document.SourcePath);
        uint newStaticId = idMap[sourceStatic.Id];
        int newStaticIndex = verified.Objects.Single(entry =>
            entry.Id == newStaticId).Index;
        int verifiedSourceMeshObjectIndex = verified.Objects.Single(entry =>
            entry.Id == sourceMeshObjectId).Index;
        SmoSharedMeshInstanceInfo instance =
            SmoSharedMeshInstanceResolver.ResolveAll(verified).Single(candidate =>
                candidate.StaticObjectIndex == newStaticIndex &&
                candidate.SourceMeshObjectIndex == verifiedSourceMeshObjectIndex);
        return new SmoSharedPlacementCloneResult(
            output,
            newStaticIndex,
            instance.ModelObjectIndex,
            verifiedSourceMeshObjectIndex,
            instance.WorldTransform,
            verified.Objects.Count - document.Objects.Count);
    }

    private static SmoSharedPlacementCloneResult CloneNodeModelPlacement(
        SmoDocument document,
        int sourceMeshObjectIndex,
        Matrix4x4 worldTransform,
        string? placementName)
    {
        SmoObjectEntry sourceMesh = document.Objects[sourceMeshObjectIndex];
        uint sourceMeshObjectId = sourceMesh.Id;
        SmoObjectEntry sourceModel = FindAncestor(
                document,
                sourceMesh,
                SmoClassIds.Model)
            ?? throw new InvalidOperationException(
                $"Mesh [{sourceMeshObjectIndex}] has no owning spModel branch.");
        IReadOnlyList<SmoSharedMeshInstanceInfo> instances =
            SmoSharedMeshInstanceResolver.ResolveAll(document);
        if (instances.Count == 0)
            throw new InvalidOperationException(
                "The level has no static placement shell for node-authored models.");
        SmoSharedMeshInstanceInfo genericTemplate = SelectNearestTemplate(
            instances,
            worldTransform);

        SmoSharedPlacementCloneResult shell = Clone(
            document,
            genericTemplate.SourceMeshObjectIndex,
            worldTransform,
            placementName);
        byte[] output = shell.Data;
        SmoDocument current = SmoDocument.ParseOwned(output, document.SourcePath);
        SmoObjectEntry shellModel = current.Objects[shell.ModelObjectIndex];
        uint shellStaticId = current.Objects[shell.StaticObjectIndex].Id;
        uint modelOwnerId = shellModel.ParentIndex is int ownerIndex
            ? current.Objects[ownerIndex].Id
            : throw new InvalidOperationException(
                "The generated static shell has no model owner.");
        output = SmoVisualForestInjector.RemoveInlineBranch(
            current,
            modelOwnerId,
            shellModel.Id);

        current = SmoDocument.ParseOwned(output, document.SourcePath);
        SmoObjectEntry currentSourceModel = current.Objects.Single(entry =>
            entry.Id == sourceModel.Id);
        (output, Dictionary<uint, uint> idMap) = InjectClonedInlineBranch(
            current,
            currentSourceModel,
            modelOwnerId,
            placementName);

        SmoObjectEntry[] sourceBranch = document.Objects.Where(entry =>
                entry.PhysicalOffset >= sourceModel.PhysicalOffset &&
                entry.PhysicalEnd <= sourceModel.PhysicalEnd)
            .ToArray();
        foreach (SmoObjectEntry resource in sourceBranch
                     .Where(entry => entry.TypeHash is
                         SmoClassIds.TextureData or SmoClassIds.MeshData)
                     .OrderBy(entry => entry.TypeHash == SmoClassIds.TextureData ? 0 : 1))
        {
            current = SmoDocument.ParseOwned(output, document.SourcePath);
            uint clonedId = idMap[resource.Id];
            SmoObjectEntry cloned = current.Objects.Single(entry => entry.Id == clonedId);
            if (cloned.ParentIndex is not int clonedOwnerIndex ||
                current.Objects.Any(entry => entry.ParentIndex == cloned.Index))
                continue;
            uint clonedOwnerId = current.Objects[clonedOwnerIndex].Id;
            if (resource.TypeHash == SmoClassIds.MeshData &&
                resource.Id != sourceMeshObjectId &&
                resource.ParentIndex == sourceModel.Index)
            {
                // A reference-only spModel is a placement of exactly one mesh.
                // Some node-authored source models contain several inline mesh
                // leaves. Turning all of them into references makes the model
                // ambiguous and invisible to both the editor and the engine.
                // The catalog operation requested this particular mesh, so
                // omit its direct siblings from the generated placement shell.
                output = SmoVisualForestInjector.RemoveInlineBranch(
                    current,
                    clonedOwnerId,
                    clonedId);
                continue;
            }
            if (resource.TypeHash == SmoClassIds.MeshData &&
                resource.Id != sourceMeshObjectId)
            {
                // Meshes owned by nested models keep their authored inline
                // representation. Only the selected model is converted into
                // the lightweight one-mesh placement convention.
                continue;
            }
            byte fieldType = resource.TypeHash == SmoClassIds.TextureData
                ? (byte)10
                : (byte)0;
            output = SmoVisualForestInjector.DemoteInlineLeafToReference(
                current,
                clonedOwnerId,
                fieldType,
                clonedId);
            current = SmoDocument.ParseOwned(output, document.SourcePath);
            output = SmoVisualForestInjector.ReplaceReferenceId(
                current,
                clonedOwnerId,
                fieldType,
                clonedId,
                resource.Id);
        }

        SmoDocument verified = SmoDocument.ParseOwned(output, document.SourcePath);
        int newStaticIndex = verified.Objects.Single(entry =>
            entry.Id == shellStaticId).Index;
        int verifiedSourceMeshObjectIndex = verified.Objects.Single(entry =>
            entry.Id == sourceMeshObjectId).Index;
        SmoSharedMeshInstanceInfo? instance =
            SmoSharedMeshInstanceResolver.ResolveAll(verified).SingleOrDefault(candidate =>
                candidate.StaticObjectIndex == newStaticIndex &&
                candidate.SourceMeshObjectIndex == verifiedSourceMeshObjectIndex);
        if (instance is null)
        {
            uint clonedModelId = idMap[sourceModel.Id];
            SmoObjectEntry clonedModel = verified.Objects.Single(entry =>
                entry.Id == clonedModelId);
            string directChildren = string.Join(",", verified.Objects
                .Where(entry => entry.ParentIndex == clonedModel.Index)
                .Select(entry => $"{entry.Index}:{entry.TypeHash:X8}:{entry.Id}"));
            string fieldZeroReferences = string.Join(",", ResolveReferenceIds(
                verified,
                clonedModel,
                fieldType: 0));
            string staticInstances = string.Join(",", SmoSharedMeshInstanceResolver
                .ResolveAll(verified)
                .Where(candidate => candidate.StaticObjectIndex == newStaticIndex)
                .Select(candidate =>
                    $"model={candidate.ModelObjectIndex}/mesh={candidate.SourceMeshObjectIndex}"));
            throw new InvalidDataException(
                $"The node-authored placement shell [{newStaticIndex}] does not " +
                $"resolve its requested mesh [{verifiedSourceMeshObjectIndex}] " +
                $"(source ID {sourceMeshObjectId}); cloned model " +
                $"[{clonedModel.Index}] ID {clonedModelId}; children={directChildren}; " +
                $"field0={fieldZeroReferences}; instances={staticInstances}.");
        }
        return new SmoSharedPlacementCloneResult(
            output,
            newStaticIndex,
            instance.ModelObjectIndex,
            verifiedSourceMeshObjectIndex,
            instance.WorldTransform,
            verified.Objects.Count - document.Objects.Count);
    }

    private static SmoSharedMeshInstanceInfo SelectNearestTemplate(
        IReadOnlyList<SmoSharedMeshInstanceInfo> instances,
        Matrix4x4 worldTransform)
    {
        if (instances.Count == 0)
            throw new InvalidOperationException(
                "The level has no static placement shell.");
        Vector3 target = worldTransform.Translation;
        return instances
            .OrderBy(instance => Vector3.DistanceSquared(
                instance.WorldTransform.Translation,
                target))
            .ThenBy(instance => instance.StaticObjectIndex)
            .First();
    }

    private static IReadOnlyList<uint> ResolveReferenceIds(
        SmoDocument document,
        SmoObjectEntry owner,
        byte fieldType)
    {
        ReadOnlySpan<byte> serialized = document.Data.Span.Slice(
            checked((int)owner.PhysicalOffset),
            checked((int)owner.SerializedSize));
        var result = new List<uint>();
        int offset = 8;
        while (offset < serialized.Length &&
               SmoDataBlockReader.TryReadHeader(
                   serialized,
                   offset,
                   out SmoDataBlockHeader field))
        {
            if (field.FieldType == fieldType && field.PayloadSize == 8)
            {
                ReadOnlySpan<byte> payload = serialized.Slice(
                    field.PayloadOffset,
                    checked((int)field.PayloadSize));
                if (SmoNodeDecoder.TryDecodeRelationship(payload, out uint id,
                        out uint inlineSize, out var encoding) &&
                    encoding == SmoNodeRelationshipEncoding.SizedReference &&
                    id != 0 && inlineSize == 0)
                    result.Add(id);
            }
            offset = checked((int)field.PayloadEnd);
        }
        return result;
    }

    private static (byte[] Data, Dictionary<uint, uint> IdMap)
        InjectClonedInlineBranch(
            SmoDocument document,
            SmoObjectEntry sourceRoot,
            uint targetOwnerId,
            string? rootName)
    {
        SmoObjectEntry sourceParent = sourceRoot.ParentIndex is int parentIndex
            ? document.Objects[parentIndex]
            : throw new InvalidOperationException(
                "The source model has no serialized parent field.");
        (int fieldPhysicalOffset, int fieldLength) = FindInlineField(
            document,
            sourceParent,
            sourceRoot);
        byte[] fieldData = document.Data.Span
            .Slice(fieldPhysicalOffset, fieldLength)
            .ToArray();
        SmoObjectEntry[] branchEntries = document.Objects.Where(entry =>
                entry.PhysicalOffset >= sourceRoot.PhysicalOffset &&
                entry.PhysicalEnd <= sourceRoot.PhysicalEnd)
            .OrderBy(entry => entry.PhysicalOffset)
            .ToArray();
        HashSet<uint> existingIds = document.Objects.Select(entry => entry.Id).ToHashSet();
        uint nextId = existingIds.Max();
        var idMap = new Dictionary<uint, uint>();
        foreach (SmoObjectEntry entry in branchEntries)
        {
            do nextId = checked(nextId + 1);
            while (existingIds.Contains(nextId) || idMap.ContainsValue(nextId));
            idMap.Add(entry.Id, nextId);
        }
        var generatedEntries = new List<SmoVisualForestEntry>(branchEntries.Length);
        foreach (SmoObjectEntry entry in branchEntries)
        {
            int relativeObjectOffset = checked(
                (int)(entry.PhysicalOffset - fieldPhysicalOffset));
            int relativePrefixOffset = relativeObjectOffset - 8;
            if (relativePrefixOffset < 0 ||
                BinaryPrimitives.ReadUInt32LittleEndian(
                    fieldData.AsSpan(relativePrefixOffset)) != entry.Id)
                throw new InvalidOperationException(
                    $"Object [{entry.Index}] has no confirmed inline prefix in its model branch.");
            WriteUInt32(fieldData, relativePrefixOffset, idMap[entry.Id]);
            byte[] rawName = entry.RawName.ToArray();
            if (entry.Id == sourceRoot.Id && !string.IsNullOrWhiteSpace(rootName))
                rawName = System.Text.Encoding.UTF8.GetBytes(rootName.Trim() + "\0");
            generatedEntries.Add(new SmoVisualForestEntry(
                idMap[entry.Id],
                rawName,
                entry.TypeHash,
                relativeObjectOffset,
                entry.SerializedSize));
        }
        byte[] output = SmoVisualForestInjector.Inject(
            document,
            targetOwnerId,
            [new SmoVisualForestAttachment(targetOwnerId, fieldData, generatedEntries)]);
        return (output, idMap);
    }

    private static SmoObjectEntry? FindAncestor(
        SmoDocument document,
        SmoObjectEntry entry,
        uint typeHash)
    {
        SmoObjectEntry? current = entry;
        while (current is not null)
        {
            if (current.TypeHash == typeHash)
                return current;
            current = current.ParentIndex is int parentIndex
                ? document.Objects[parentIndex]
                : null;
        }
        return null;
    }

    private static (int PhysicalOffset, int Length) FindInlineField(
        SmoDocument document,
        SmoObjectEntry parent,
        SmoObjectEntry child)
    {
        ReadOnlySpan<byte> serialized = document.Data.Span.Slice(
            checked((int)parent.PhysicalOffset),
            checked((int)parent.SerializedSize));
        int offset = 8;
        while (offset < serialized.Length &&
               SmoDataBlockReader.TryReadHeader(
                   serialized,
                   offset,
                   out SmoDataBlockHeader field))
        {
            long payloadPhysical = parent.PhysicalOffset + field.PayloadOffset;
            if (payloadPhysical == child.PhysicalOffset - 8 &&
                field.PayloadSize == child.SerializedSize + 8)
            {
                return (
                    checked((int)(parent.PhysicalOffset + field.Offset)),
                    checked((int)(field.PayloadEnd - field.Offset)));
            }
            offset = checked((int)field.PayloadEnd);
        }
        throw new InvalidOperationException(
            $"The inline field for static object [{child.Index}] was not found.");
    }

    private static void PatchStaticMatrices(
        Span<byte> fieldData,
        int objectOffset,
        int serializedSize,
        Matrix4x4 forward,
        Matrix4x4 inverse)
    {
        Span<byte> serialized = fieldData.Slice(objectOffset, serializedSize);
        if (!SmoDataBlockReader.TryReadHeader(
                serialized,
                8,
                out SmoDataBlockHeader forwardField) ||
            forwardField.FieldType != 1 || forwardField.PayloadSize != 64 ||
            !SmoDataBlockReader.TryReadHeader(
                serialized,
                checked((int)forwardField.PayloadEnd),
                out SmoDataBlockHeader inverseField) ||
            inverseField.FieldType != 2 || inverseField.PayloadSize != 64)
        {
            throw new InvalidOperationException(
                "The shared placement template has no writable matrix pair.");
        }
        SmoStaticMatrixWriter.PatchPayloads(serialized,
            forwardField.PayloadOffset, inverseField.PayloadOffset, forward, inverse);
    }

    private static float MatrixMaxDifference(Matrix4x4 left, Matrix4x4 right)
    {
        ReadOnlySpan<float> a =
        [
            left.M11, left.M12, left.M13, left.M14,
            left.M21, left.M22, left.M23, left.M24,
            left.M31, left.M32, left.M33, left.M34,
            left.M41, left.M42, left.M43, left.M44
        ];
        ReadOnlySpan<float> b =
        [
            right.M11, right.M12, right.M13, right.M14,
            right.M21, right.M22, right.M23, right.M24,
            right.M31, right.M32, right.M33, right.M34,
            right.M41, right.M42, right.M43, right.M44
        ];
        float maximum = 0;
        for (int index = 0; index < a.Length; index++)
            maximum = MathF.Max(maximum, MathF.Abs(a[index] - b[index]));
        return maximum;
    }

    private static void WriteUInt32(Span<byte> data, int offset, uint value) =>
        BinaryPrimitives.WriteUInt32LittleEndian(data[offset..], value);
}

public sealed record SmoSharedPlacementCloneResult(
    byte[] Data,
    int StaticObjectIndex,
    int ModelObjectIndex,
    int SourceMeshObjectIndex,
    Matrix4x4 WorldTransform,
    int AddedObjectCount);
