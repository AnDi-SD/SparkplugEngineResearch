using System.Buffers.Binary;
using SmoViewer.Core;

namespace SmoImporter.Core;

/// <summary>
/// The additive part of an importer result expressed as independent inline
/// forests. Existing SMO bytes are deliberately not carried by this plan.
/// </summary>
public sealed record SmoAdditiveForestPlan(
    IReadOnlyList<SmoVisualForestOperation> Operations,
    IReadOnlyList<uint> GeneratedObjectIds);

/// <summary>
/// Converts an isolated serializer result to the shared forest-operation contract.
/// It accepts only results which preserve every existing object and
/// add complete inline branches immediately before an existing owner's
/// terminal field. Any other mutation is rejected.
/// </summary>
public static class SmoAdditiveForestPlanner
{
    private const int ObjectSignatureSize = 8;
    private const int ObjectReferenceSize = 8;

    public static SmoAdditiveForestPlan Create(
        SmoDocument source,
        SmoDocument additiveResult)
    {
        ArgumentNullException.ThrowIfNull(source);
        ArgumentNullException.ThrowIfNull(additiveResult);
        if (source.HasErrors)
            throw new InvalidDataException("The additive-plan source has parser errors.");
        if (additiveResult.HasErrors)
            throw new InvalidDataException("The additive writer result has parser errors.");

        Dictionary<uint, SmoObjectEntry> sourceById = source.Objects
            .ToDictionary(entry => entry.Id);
        Dictionary<uint, SmoObjectEntry> resultById = additiveResult.Objects
            .ToDictionary(entry => entry.Id);
        ValidateExistingCatalog(source, additiveResult, sourceById, resultById);

        SmoObjectEntry[] generated = additiveResult.Objects
            .Where(entry => !sourceById.ContainsKey(entry.Id))
            .OrderBy(entry => entry.LogicalOffset)
            .ThenByDescending(entry => entry.SerializedSize)
            .ToArray();
        if (generated.Length == 0)
            return new SmoAdditiveForestPlan([], []);

        HashSet<uint> generatedIds = generated.Select(entry => entry.Id).ToHashSet();
        SmoObjectEntry[] roots = generated.Where(entry =>
        {
            if (entry.ParentIndex is not int parentIndex)
                throw new InvalidDataException(
                    $"Generated object {entry.Id} has no inline parent.");
            return !generatedIds.Contains(additiveResult.Objects[parentIndex].Id);
        }).ToArray();
        if (roots.Length == 0)
            throw new InvalidDataException("The additive result has no generated forest roots.");

        var extracted = new List<ExtractedOperation>(roots.Length);
        var assignedIds = new HashSet<uint>();
        foreach (SmoObjectEntry root in roots)
        {
            SmoObjectEntry parent = additiveResult.Objects[root.ParentIndex!.Value];
            if (!sourceById.ContainsKey(parent.Id))
            {
                throw new InvalidDataException(
                    $"Generated forest root {root.Id} is not owned by an imported object.");
            }
            SmoDataBlockHeader field = FindExactInlineField(additiveResult, parent, root);
            int fieldPhysicalOffset = checked((int)parent.PhysicalOffset + field.Offset);
            int fieldLength = checked(field.HeaderSize + (int)field.PayloadSize);
            byte[] fieldData = additiveResult.Data.Span
                .Slice(fieldPhysicalOffset, fieldLength)
                .ToArray();
            SmoObjectEntry[] entries = generated.Where(entry =>
                    entry.PhysicalOffset >= root.PhysicalOffset &&
                    entry.PhysicalEnd <= root.PhysicalEnd)
                .OrderBy(entry => entry.PhysicalOffset)
                .ThenByDescending(entry => entry.SerializedSize)
                .ToArray();
            if (entries.Length == 0 || entries[0].Id != root.Id)
                throw new InvalidDataException($"Generated forest {root.Id} is incomplete.");
            if (additiveResult.Objects.Any(entry =>
                    sourceById.ContainsKey(entry.Id) &&
                    entry.PhysicalOffset >= root.PhysicalOffset &&
                    entry.PhysicalEnd <= root.PhysicalEnd))
            {
                throw new InvalidDataException(
                    $"Generated forest {root.Id} physically moves an imported object.");
            }

            var forestEntries = new List<SmoVisualForestEntry>(entries.Length);
            foreach (SmoObjectEntry entry in entries)
            {
                if (!assignedIds.Add(entry.Id))
                    throw new InvalidDataException($"Generated object {entry.Id} belongs to two forests.");
                forestEntries.Add(new SmoVisualForestEntry(
                    entry.Id,
                    entry.RawName.ToArray(),
                    entry.TypeHash,
                    checked((int)entry.PhysicalOffset - fieldPhysicalOffset),
                    entry.SerializedSize));
            }
            extracted.Add(new ExtractedOperation(
                parent.Id,
                field.Offset,
                fieldLength,
                new SmoVisualForestOperation(
                    new SmoVisualForestAttachment(parent.Id, fieldData, forestEntries),
                    SmoVisualForestInsertionKind.BeforeTerminal)));
        }

        if (!assignedIds.SetEquals(generatedIds))
        {
            uint missing = generatedIds.First(id => !assignedIds.Contains(id));
            throw new InvalidDataException(
                $"Generated object {missing} is outside every additive forest.");
        }

        ValidateOwnerSuffixes(source, additiveResult, sourceById, resultById, extracted);
        ValidateUnchangedObjects(source, additiveResult, sourceById, resultById, extracted);
        SmoVisualForestOperation[] operations = extracted
            .OrderBy(item => additiveResult.Objects.Single(entry =>
                entry.Id == item.TargetOwnerId).LogicalOffset)
            .ThenBy(item => item.FieldOffset)
            .Select(item => item.Operation)
            .ToArray();
        return new SmoAdditiveForestPlan(
            operations,
            generated.Select(entry => entry.Id).ToArray());
    }

    /// <summary>
    /// Reassigns every generated ID while preserving references between the
    /// generated objects. References to imported objects remain untouched.
    /// </summary>
    public static SmoAdditiveForestPlan RemapObjectIds(
        SmoAdditiveForestPlan plan,
        IReadOnlyDictionary<uint, uint> idMap)
    {
        ArgumentNullException.ThrowIfNull(plan);
        ArgumentNullException.ThrowIfNull(idMap);
        if (plan.GeneratedObjectIds.Count != idMap.Count ||
            plan.GeneratedObjectIds.Any(id => !idMap.ContainsKey(id)) ||
            idMap.Values.Distinct().Count() != idMap.Count)
        {
            throw new ArgumentException(
                "The generated-object ID map must be complete and one-to-one.",
                nameof(idMap));
        }

        var operations = new List<SmoVisualForestOperation>(plan.Operations.Count);
        foreach (SmoVisualForestOperation operation in plan.Operations)
        {
            byte[] data = operation.Attachment.FieldData.ToArray();
            SmoVisualForestEntry[] entries = operation.Attachment.Entries
                .Select(entry => entry with { Id = idMap[entry.Id] })
                .ToArray();
            foreach (SmoVisualForestEntry original in operation.Attachment.Entries)
            {
                int prefix = checked(original.RelativeOffset - ObjectReferenceSize);
                if (prefix < 0 || prefix > data.Length - ObjectReferenceSize ||
                    BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(prefix)) != original.Id)
                {
                    throw new InvalidDataException(
                        $"Generated object {original.Id} has no remappable inline prefix.");
                }
                BinaryPrimitives.WriteUInt32LittleEndian(
                    data.AsSpan(prefix),
                    idMap[original.Id]);

                ReadOnlySpan<byte> objectBytes = operation.Attachment.FieldData.AsSpan(
                    original.RelativeOffset,
                    checked((int)original.SerializedSize));
                int fieldOffset = ObjectSignatureSize;
                while (fieldOffset < objectBytes.Length &&
                       SmoDataBlockReader.TryReadHeader(
                           objectBytes,
                           fieldOffset,
                           out SmoDataBlockHeader field))
                {
                    if (field.PayloadSize == ObjectReferenceSize)
                    {
                        uint referencedId = BinaryPrimitives.ReadUInt32LittleEndian(
                            objectBytes[field.PayloadOffset..]);
                        uint inlineSize = BinaryPrimitives.ReadUInt32LittleEndian(
                            objectBytes[(field.PayloadOffset + sizeof(uint))..]);
                        if (inlineSize == 0 && idMap.TryGetValue(referencedId, out uint mapped))
                        {
                            int payload = checked(original.RelativeOffset + field.PayloadOffset);
                            BinaryPrimitives.WriteUInt32LittleEndian(data.AsSpan(payload), mapped);
                        }
                    }
                    fieldOffset = checked((int)field.PayloadEnd);
                }
            }
            operations.Add(operation with
            {
                Attachment = operation.Attachment with
                {
                    FieldData = data,
                    Entries = entries
                }
            });
        }
        return new SmoAdditiveForestPlan(
            operations,
            plan.GeneratedObjectIds.Select(id => idMap[id]).ToArray());
    }

    private static void ValidateExistingCatalog(
        SmoDocument source,
        SmoDocument result,
        IReadOnlyDictionary<uint, SmoObjectEntry> sourceById,
        IReadOnlyDictionary<uint, SmoObjectEntry> resultById)
    {
        foreach (SmoObjectEntry existing in source.Objects)
        {
            if (!resultById.TryGetValue(existing.Id, out SmoObjectEntry? current))
                throw new InvalidDataException($"Additive writer removed object {existing.Id}.");
            if (current.TypeHash != existing.TypeHash ||
                !current.RawName.Span.SequenceEqual(existing.RawName.Span))
            {
                throw new InvalidDataException(
                    $"Additive writer changed catalog identity for object {existing.Id}.");
            }
            uint? oldParentId = existing.ParentIndex is int oldParent
                ? source.Objects[oldParent].Id
                : null;
            uint? newParentId = current.ParentIndex is int newParent
                ? result.Objects[newParent].Id
                : null;
            if (oldParentId != newParentId)
            {
                throw new InvalidDataException(
                    $"Additive writer changed the physical owner of object {existing.Id}.");
            }
        }
        if (resultById.Count < sourceById.Count)
            throw new InvalidDataException("Additive writer reduced the object catalog.");
    }

    private static void ValidateOwnerSuffixes(
        SmoDocument source,
        SmoDocument result,
        IReadOnlyDictionary<uint, SmoObjectEntry> sourceById,
        IReadOnlyDictionary<uint, SmoObjectEntry> resultById,
        IReadOnlyList<ExtractedOperation> extracted)
    {
        foreach (IGrouping<uint, ExtractedOperation> group in extracted.GroupBy(item => item.TargetOwnerId))
        {
            SmoObjectEntry oldOwner = sourceById[group.Key];
            SmoObjectEntry newOwner = resultById[group.Key];
            byte[] stripped = ObjectBytes(result, newOwner).ToArray();
            foreach (ExtractedOperation field in group.OrderByDescending(item => item.FieldOffset))
            {
                int end = checked(field.FieldOffset + field.FieldLength);
                stripped = stripped.AsSpan(0, field.FieldOffset)
                    .ToArray()
                    .Concat(stripped.AsSpan(end).ToArray())
                    .ToArray();
            }
            if (!stripped.AsSpan().SequenceEqual(ObjectBytes(source, oldOwner)))
            {
                throw new InvalidDataException(
                    $"Additive writer changed owner {group.Key} outside generated fields.");
            }

            ReadOnlySpan<byte> ownerBytes = ObjectBytes(result, newOwner);
            int ownerLength = ownerBytes.Length;
            SmoDataBlockHeader[] fields = ReadDirectFields(ownerBytes);
            int terminalIndex = Array.FindLastIndex(fields, field =>
                field.PayloadSize == 0 && field.Offset + field.HeaderSize == ownerLength);
            if (terminalIndex < 0)
                throw new InvalidDataException($"Additive owner {group.Key} has no terminal field.");
            HashSet<int> generatedOffsets = group.Select(item => item.FieldOffset).ToHashSet();
            int first = Array.FindIndex(fields, field => generatedOffsets.Contains(field.Offset));
            if (first < 0 || fields.Skip(first).Take(terminalIndex - first)
                    .Any(field => !generatedOffsets.Contains(field.Offset)))
            {
                throw new InvalidDataException(
                    $"Generated fields in owner {group.Key} are not a terminal suffix.");
            }
        }
    }

    private static void ValidateUnchangedObjects(
        SmoDocument source,
        SmoDocument result,
        IReadOnlyDictionary<uint, SmoObjectEntry> sourceById,
        IReadOnlyDictionary<uint, SmoObjectEntry> resultById,
        IReadOnlyList<ExtractedOperation> extracted)
    {
        HashSet<uint> targets = extracted.Select(item => item.TargetOwnerId).ToHashSet();
        var affectedAncestors = new HashSet<uint>(targets);
        foreach (uint target in targets)
        {
            SmoObjectEntry cursor = sourceById[target];
            while (cursor.ParentIndex is int parentIndex)
            {
                cursor = source.Objects[parentIndex];
                affectedAncestors.Add(cursor.Id);
            }
        }
        foreach (SmoObjectEntry existing in source.Objects.Where(entry =>
                     !affectedAncestors.Contains(entry.Id)))
        {
            SmoObjectEntry current = resultById[existing.Id];
            if (existing.SerializedSize != current.SerializedSize ||
                !ObjectBytes(source, existing).SequenceEqual(ObjectBytes(result, current)))
            {
                throw new InvalidDataException(
                    $"Additive writer changed unrelated object {existing.Id}.");
            }
        }
    }

    private static SmoDataBlockHeader FindExactInlineField(
        SmoDocument document,
        SmoObjectEntry owner,
        SmoObjectEntry child)
    {
        ReadOnlySpan<byte> bytes = ObjectBytes(document, owner);
        int offset = ObjectSignatureSize;
        while (offset < bytes.Length &&
               SmoDataBlockReader.TryReadHeader(bytes, offset, out SmoDataBlockHeader field))
        {
            long payloadPhysical = owner.PhysicalOffset + field.PayloadOffset;
            if (field.PayloadSize == child.SerializedSize + ObjectReferenceSize &&
                payloadPhysical == child.PhysicalOffset - ObjectReferenceSize &&
                BinaryPrimitives.ReadUInt32LittleEndian(bytes[field.PayloadOffset..]) == child.Id &&
                BinaryPrimitives.ReadUInt32LittleEndian(
                    bytes[(field.PayloadOffset + sizeof(uint))..]) == child.SerializedSize)
            {
                return field;
            }
            offset = checked((int)field.PayloadEnd);
        }
        throw new InvalidDataException(
            $"Generated root {child.Id} has no exact inline field in owner {owner.Id}.");
    }

    private static SmoDataBlockHeader[] ReadDirectFields(ReadOnlySpan<byte> bytes)
    {
        var fields = new List<SmoDataBlockHeader>();
        int offset = ObjectSignatureSize;
        while (offset < bytes.Length &&
               SmoDataBlockReader.TryReadHeader(bytes, offset, out SmoDataBlockHeader field))
        {
            fields.Add(field);
            offset = checked((int)field.PayloadEnd);
        }
        if (offset != bytes.Length)
            throw new InvalidDataException("Generated owner field stream is not fully decodable.");
        return fields.ToArray();
    }

    private static ReadOnlySpan<byte> ObjectBytes(
        SmoDocument document,
        SmoObjectEntry entry) => document.Data.Span.Slice(
            checked((int)entry.PhysicalOffset),
            checked((int)entry.SerializedSize));

    private sealed record ExtractedOperation(
        uint TargetOwnerId,
        int FieldOffset,
        int FieldLength,
        SmoVisualForestOperation Operation);
}
