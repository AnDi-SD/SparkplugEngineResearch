using System.Buffers.Binary;
using System.Numerics;
using System.Text;
using SmoViewer.Core;

namespace SmoImporter.Core;

public sealed record SmoCollisionBranchAppendResult(
    byte[] Data,
    int AddedObjectCount,
    int CollisionInfoObjectIndex,
    int MeshBoundingVolumeObjectIndex);

public sealed record SmoCollisionRegistrationRepairResult(
    byte[] Data,
    IReadOnlyList<int> RegisteredCollisionInfoObjectIndices);

public sealed record SmoCollisionMeshCompatibilityRepairResult(
    byte[] Data,
    IReadOnlyList<int> RebuiltCollisionInfoObjectIndices);

public sealed record SmoCollisionBranchPlan(
    IReadOnlyList<SmoVisualForestOperation> Operations,
    uint CollisionInfoObjectId,
    uint MeshBoundingVolumeObjectId);

/// <summary>
/// Adds a native spCollisionInfo/spMeshBV branch beside the closest existing
/// collision. The template supplies the game's class metadata and transform;
/// only the generated triangle payload and fresh object IDs are substituted.
/// </summary>
public static class SmoCollisionBranchAppender
{
    private const int ObjectSignatureSize = 8;
    private const int ObjectReferenceSize = 8;

    public static SmoCollisionBranchAppendResult Append(
        SmoDocument document,
        IReadOnlyList<Vector3> worldPositions,
        IReadOnlyList<int> triangleIndices,
        string? name = null)
    {
        ArgumentNullException.ThrowIfNull(document);
        ValidateGeometry(worldPositions, triangleIndices);

        uint nextId = document.Objects.Max(entry => entry.Id);
        uint collisionId = NextId(document, ref nextId);
        uint meshId = NextId(document, ref nextId);
        SmoCollisionBranchPlan plan = CreatePlan(
            document,
            worldPositions,
            triangleIndices,
            collisionId,
            meshId,
            name);
        SmoDocument current = document;
        foreach (SmoVisualForestOperation operation in plan.Operations)
        {
            byte[] rewritten = ApplyOperation(current, operation);
            current = SmoDocument.Parse(rewritten, document.SourcePath);
        }
        SmoDocument verified = current;
        SmoObjectEntry verifiedCollision = verified.Objects.Single(entry =>
            entry.Id == collisionId);
        SmoObjectEntry verifiedMesh = verified.Objects.Single(entry => entry.Id == meshId);
        SmoCollisionMesh generated = SmoCollisionMeshDecoder.DecodeAll(verified)
            .Single(candidate =>
                candidate.CollisionInfoObjectIndex == verifiedCollision.Index);
        if (generated.TriangleIndices.Count != triangleIndices.Count ||
            generated.Positions.Count != worldPositions.Count)
        {
            throw new InvalidDataException(
                "Generated collision failed geometry verification.");
        }
        for (int index = 0; index < worldPositions.Count; index++)
        {
            Vector3 actual = Vector3.Transform(
                generated.Positions[index],
                generated.WorldTransform);
            if (Vector3.Distance(actual, worldPositions[index]) > 0.01f)
            {
                throw new InvalidDataException(
                    "Generated collision changed position during serialization.");
            }
        }

        return new SmoCollisionBranchAppendResult(
            verified.Data.ToArray(),
            2,
            verifiedCollision.Index,
            verifiedMesh.Index);
    }

    /// <summary>
    /// Builds the collision branch and its physics-registry reference without
    /// rewriting the source container. Callers may apply the operations
    /// immediately or persist them as editor project assets.
    /// </summary>
    public static SmoCollisionBranchPlan CreatePlan(
        SmoDocument document,
        IReadOnlyList<Vector3> worldPositions,
        IReadOnlyList<int> triangleIndices,
        uint collisionId,
        uint meshId,
        string? name = null)
    {
        ArgumentNullException.ThrowIfNull(document);
        ValidateGeometry(worldPositions, triangleIndices);
        if (collisionId == 0 || meshId == 0 || collisionId == meshId ||
            document.Objects.Any(entry => entry.Id == collisionId || entry.Id == meshId))
        {
            throw new ArgumentException(
                "Planned collision object IDs must be non-zero, distinct and unused.");
        }

        IReadOnlyList<SmoCollisionMesh> templates =
            SmoCollisionMeshDecoder.DecodeAll(document);
        if (templates.Count == 0)
        {
            throw new NotSupportedException(
                "The level has no native collision branch that can be used as a template.");
        }
        Vector3 center = worldPositions.Aggregate(Vector3.Zero, (sum, point) =>
            sum + point) / worldPositions.Count;
        int generatedMeshSize = BuildMeshObject(
            worldPositions,
            triangleIndices).Length;
        SmoCollisionMesh template = templates
            .OrderBy(candidate => Vector3.DistanceSquared(
                center,
                CalculateCenter(candidate)))
            .ThenBy(candidate => candidate.CollisionInfoObjectIndex)
            .FirstOrDefault(candidate => CanResizeTemplate(
                document,
                candidate,
                generatedMeshSize) &&
                TryFindRegistration(
                    document,
                    document.Objects[candidate.CollisionInfoObjectIndex],
                    out _))
            ?? throw new NotSupportedException(
                "The level has no registered collision template with a resizable geometry field.");
        if (!Matrix4x4.Invert(template.WorldTransform, out Matrix4x4 inverseTemplate))
            throw new InvalidDataException("Collision template has a singular transform.");

        Vector3[] localPositions = worldPositions
            .Select(position => Vector3.Transform(position, inverseTemplate))
            .ToArray();
        byte[] meshObject = BuildMeshObject(localPositions, triangleIndices);

        SmoObjectEntry owner = document.Objects[template.NodeObjectIndex];
        SmoObjectEntry collisionInfo =
            document.Objects[template.CollisionInfoObjectIndex];
        SmoObjectEntry oldMesh =
            document.Objects[template.MeshBoundingVolumeObjectIndex];
        (int outerPhysical, int outerLength, SmoDataBlockHeader outerField) =
            FindInlineField(document, owner, collisionInfo);
        (_, _, SmoDataBlockHeader meshField) =
            FindInlineField(document, collisionInfo, oldMesh);

        byte[] oldField = document.Data.Span.Slice(outerPhysical, outerLength).ToArray();
        int collisionOffset = checked((int)(collisionInfo.PhysicalOffset - outerPhysical));
        int meshOffset = checked((int)(oldMesh.PhysicalOffset - outerPhysical));
        int meshEnd = checked(meshOffset + (int)oldMesh.SerializedSize);
        int delta = checked(meshObject.Length - (int)oldMesh.SerializedSize);
        byte[] fieldData = new byte[checked(oldField.Length + delta)];
        oldField.AsSpan(0, meshOffset).CopyTo(fieldData);
        meshObject.CopyTo(fieldData, meshOffset);
        oldField.AsSpan(meshEnd).CopyTo(fieldData.AsSpan(meshOffset + meshObject.Length));

        WriteUInt32(fieldData, collisionOffset - ObjectReferenceSize, collisionId);
        WriteUInt32(
            fieldData,
            collisionOffset - sizeof(uint),
            checked((uint)((int)collisionInfo.SerializedSize + delta)));
        WriteUInt32(fieldData, meshOffset - ObjectReferenceSize, meshId);
        WriteUInt32(fieldData, meshOffset - sizeof(uint), checked((uint)meshObject.Length));
        WritePayloadSize(
            fieldData,
            0,
            outerField,
            checked((uint)((int)outerField.PayloadSize + delta)));
        WritePayloadSize(
            fieldData,
            collisionOffset + meshField.Offset,
            meshField,
            checked((uint)((int)meshField.PayloadSize + delta)));

        string safeName = string.IsNullOrWhiteSpace(name)
            ? "GeneratedCollision"
            : name.Trim();
        var attachment = new SmoVisualForestAttachment(
            owner.Id,
            fieldData,
            [
                new SmoVisualForestEntry(
                    collisionId,
                    Encoding.UTF8.GetBytes(safeName + "\0"),
                    SmoClassIds.CollisionInfo,
                    collisionOffset,
                    checked((uint)((int)collisionInfo.SerializedSize + delta))),
                new SmoVisualForestEntry(
                    meshId,
                    oldMesh.RawName.ToArray(),
                    SmoClassIds.MeshBoundingVolume,
                    meshOffset,
                    checked((uint)meshObject.Length))
            ]);
        SmoObjectEntry partitionSystem = FindPartitionSystemAncestor(
            document,
            collisionInfo);
        RegistrationField registrationTemplate = FindRegistrationTemplate(
            document,
            partitionSystem);
        byte[] registrationField = registrationTemplate.FieldData.ToArray();
        WriteUInt32(
            registrationField,
            registrationTemplate.TargetOffset,
            collisionId);
        var registrationAttachment = new SmoVisualForestAttachment(
            partitionSystem.Id,
            registrationField,
            []);
        return new SmoCollisionBranchPlan(
            [
                new SmoVisualForestOperation(
                    attachment,
                    SmoVisualForestInsertionKind.BeforeTerminal),
                new SmoVisualForestOperation(
                    registrationAttachment,
                    SmoVisualForestInsertionKind.AfterLastFieldType,
                    registrationTemplate.FieldType)
            ],
            collisionId,
            meshId);
    }

    private static byte[] ApplyOperation(
        SmoDocument current,
        SmoVisualForestOperation operation) => operation.InsertionKind switch
    {
        SmoVisualForestInsertionKind.BeforeTerminal =>
            SmoVisualForestInjector.Inject(
                current,
                operation.Attachment.TargetOwnerId,
                [operation.Attachment]),
        SmoVisualForestInsertionKind.AfterLastFieldType =>
            SmoVisualForestInjector.InjectAfterLastFieldType(
                current,
                operation.Attachment.TargetOwnerId,
                operation.AnchorFieldType,
                [operation.Attachment]),
        _ => throw new ArgumentOutOfRangeException(
            nameof(operation),
            operation.InsertionKind,
            "Unknown visual-forest insertion kind.")
    };

    /// <summary>
    /// Repairs collision branches which are present inline but absent from the
    /// spPartitionSystem field-7 registry consumed by the native physics loader.
    /// </summary>
    public static SmoCollisionRegistrationRepairResult EnsureRegistrations(
        SmoDocument document)
    {
        ArgumentNullException.ThrowIfNull(document);
        byte[] data = document.Data.ToArray();
        var registeredIds = new List<uint>();
        uint[] orphanIds = FindUnregisteredCollisionInfoObjectIndices(document)
            .Select(index => document.Objects[index].Id)
            .ToArray();
        foreach (uint collisionId in orphanIds)
        {
            SmoDocument current = SmoDocument.Parse(data, document.SourcePath);
            SmoObjectEntry collision = current.Objects.Single(entry =>
                entry.Id == collisionId);
            SmoCollisionRegistrationRepairResult repair =
                EnsureRegistration(current, collision);
            data = repair.Data;
            registeredIds.Add(collisionId);
        }
        SmoDocument verified = SmoDocument.Parse(data, document.SourcePath);
        return new SmoCollisionRegistrationRepairResult(
            data,
            registeredIds.Select(id => verified.Objects.Single(entry =>
                entry.Id == id).Index).ToArray());
    }

    public static IReadOnlyList<int> FindUnregisteredCollisionInfoObjectIndices(
        SmoDocument document)
    {
        ArgumentNullException.ThrowIfNull(document);
        return SmoCollisionMeshDecoder.DecodeAll(document)
            .Select(collision =>
                document.Objects[collision.CollisionInfoObjectIndex])
            .DistinctBy(collision => collision.Id)
            .Where(collision =>
                TryFindPartitionSystemAncestor(document, collision, out _) &&
                !TryFindRegistration(document, collision, out _))
            .Select(collision => collision.Index)
            .ToArray();
    }

    /// <summary>
    /// Rebuilds editor-generated collision branches whose spMeshBV ends at the
    /// geometry payload instead of the mandatory terminal empty FFPS field.
    /// Native Sparkplug rejects that shape even though the tolerant decoder can
    /// still recover its triangles.
    /// </summary>
    public static SmoCollisionMeshCompatibilityRepairResult
        EnsureMeshCompatibility(SmoDocument document)
    {
        ArgumentNullException.ThrowIfNull(document);
        SmoCollisionRegistrationRepairResult registration =
            EnsureRegistrations(document);
        byte[] data = registration.Data;
        SmoDocument registered = SmoDocument.Parse(data, document.SourcePath);
        uint[] malformedCollisionIds = SmoCollisionMeshDecoder.DecodeAll(registered)
            .Where(collision =>
                TryFindPartitionSystemAncestor(
                    registered,
                    registered.Objects[collision.CollisionInfoObjectIndex],
                    out _) &&
                !HasTerminalEmptyField(
                    registered,
                    registered.Objects[collision.MeshBoundingVolumeObjectIndex]))
            .Select(collision =>
                registered.Objects[collision.CollisionInfoObjectIndex].Id)
            .Distinct()
            .ToArray();
        var rebuiltIds = new List<uint>();
        foreach (uint collisionId in malformedCollisionIds)
        {
            SmoDocument current = SmoDocument.Parse(data, document.SourcePath);
            SmoObjectEntry collisionInfo = current.Objects.Single(entry =>
                entry.Id == collisionId);
            SmoCollisionMesh collision = SmoCollisionMeshDecoder.DecodeAll(current)
                .Single(candidate =>
                    candidate.CollisionInfoObjectIndex == collisionInfo.Index);
            Vector3[] worldPositions = collision.Positions
                .Select(position => Vector3.Transform(
                    position,
                    collision.WorldTransform))
                .ToArray();
            int[] triangleIndices = collision.TriangleIndices.ToArray();
            string name = string.IsNullOrWhiteSpace(collisionInfo.Name.TrimEnd('\0'))
                ? $"Collision_{collisionInfo.Index}"
                : collisionInfo.Name.TrimEnd('\0');
            SmoObjectEntry owner = current.Objects[collision.NodeObjectIndex];
            SmoObjectEntry partitionSystem = FindPartitionSystemAncestor(
                current, collisionInfo);

            data = SmoVisualForestInjector.RemoveReference(
                current,
                partitionSystem.Id,
                7,
                collisionInfo.Id);
            current = SmoDocument.Parse(data, document.SourcePath);
            data = SmoVisualForestInjector.RemoveInlineBranch(
                current,
                owner.Id,
                collisionInfo.Id);
            current = SmoDocument.Parse(data, document.SourcePath);
            SmoCollisionBranchAppendResult rebuilt = Append(
                current,
                worldPositions,
                triangleIndices,
                name);
            data = rebuilt.Data;
            SmoDocument after = SmoDocument.Parse(data, document.SourcePath);
            rebuiltIds.Add(after.Objects[rebuilt.CollisionInfoObjectIndex].Id);
        }

        SmoDocument verified = SmoDocument.Parse(data, document.SourcePath);
        foreach (uint rebuiltId in rebuiltIds)
        {
            SmoObjectEntry collisionInfo = verified.Objects.Single(entry =>
                entry.Id == rebuiltId);
            SmoObjectEntry mesh = verified.Objects.Single(entry =>
                entry.ParentIndex == collisionInfo.Index &&
                entry.TypeHash == SmoClassIds.MeshBoundingVolume);
            if (!HasTerminalEmptyField(verified, mesh))
            {
                throw new InvalidDataException(
                    $"Collision [{collisionInfo.Index}] still has an unterminated spMeshBV.");
            }
        }
        return new SmoCollisionMeshCompatibilityRepairResult(
            data,
            rebuiltIds.Select(id => verified.Objects.Single(entry =>
                entry.Id == id).Index).ToArray());
    }

    public static IReadOnlyList<int> FindUnterminatedCollisionMeshObjectIndices(
        SmoDocument document)
    {
        ArgumentNullException.ThrowIfNull(document);
        return SmoCollisionMeshDecoder.DecodeAll(document)
            .Where(collision =>
                TryFindPartitionSystemAncestor(
                    document,
                    document.Objects[collision.CollisionInfoObjectIndex],
                    out _) &&
                !HasTerminalEmptyField(
                    document,
                    document.Objects[collision.MeshBoundingVolumeObjectIndex]))
            .Select(collision => collision.MeshBoundingVolumeObjectIndex)
            .Distinct()
            .ToArray();
    }

    private static SmoCollisionRegistrationRepairResult EnsureRegistration(
        SmoDocument document,
        SmoObjectEntry collision)
    {
        if (TryFindRegistration(document, collision, out _))
        {
            return new SmoCollisionRegistrationRepairResult(
                document.Data.ToArray(), [collision.Index]);
        }

        SmoObjectEntry partitionSystem = FindPartitionSystemAncestor(
            document, collision);
        RegistrationField template = FindRegistrationTemplate(
            document, partitionSystem);
        byte[] referenceField = template.FieldData.ToArray();
        WriteUInt32(referenceField, template.TargetOffset, collision.Id);
        var attachment = new SmoVisualForestAttachment(
            partitionSystem.Id,
            referenceField,
            []);
        byte[] output = SmoVisualForestInjector.InjectAfterLastFieldType(
            document,
            partitionSystem.Id,
            template.FieldType,
            [attachment]);
        SmoDocument verified = SmoDocument.Parse(output, document.SourcePath);
        SmoObjectEntry verifiedCollision = verified.Objects.Single(entry =>
            entry.Id == collision.Id);
        if (!TryFindRegistration(verified, verifiedCollision, out _))
        {
            throw new InvalidDataException(
                $"Collision [{verifiedCollision.Index}] was not registered in spPartitionSystem.");
        }
        return new SmoCollisionRegistrationRepairResult(
            output, [verifiedCollision.Index]);
    }

    private static bool TryFindRegistration(
        SmoDocument document,
        SmoObjectEntry collision,
        out RegistrationField registration)
    {
        registration = default;
        if (!TryFindPartitionSystemAncestor(
                document, collision, out SmoObjectEntry partitionSystem))
            return false;
        ReadOnlySpan<byte> bytes = ObjectBytes(document, partitionSystem);
        int offset = ObjectSignatureSize;
        int matches = 0;
        while (offset < bytes.Length &&
               SmoDataBlockReader.TryReadHeader(
                   bytes, offset, out SmoDataBlockHeader field))
        {
            if (field.FieldType == 7 && field.PayloadSize == ObjectReferenceSize &&
                BinaryPrimitives.ReadUInt32LittleEndian(
                    bytes[field.PayloadOffset..]) == collision.Id &&
                BinaryPrimitives.ReadUInt32LittleEndian(
                    bytes[(field.PayloadOffset + sizeof(uint))..]) == 0)
            {
                registration = CreateRegistrationField(bytes, field);
                matches++;
            }
            offset = checked((int)field.PayloadEnd);
        }
        if (matches > 1)
        {
            throw new InvalidDataException(
                $"Collision [{collision.Index}] has {matches} spPartitionSystem registrations.");
        }
        return matches == 1;
    }

    private static RegistrationField FindRegistrationTemplate(
        SmoDocument document,
        SmoObjectEntry partitionSystem)
    {
        ReadOnlySpan<byte> bytes = ObjectBytes(document, partitionSystem);
        int offset = ObjectSignatureSize;
        while (offset < bytes.Length &&
               SmoDataBlockReader.TryReadHeader(
                   bytes, offset, out SmoDataBlockHeader field))
        {
            if (field.FieldType == 7 && field.PayloadSize == ObjectReferenceSize &&
                BinaryPrimitives.ReadUInt32LittleEndian(
                    bytes[(field.PayloadOffset + sizeof(uint))..]) == 0)
            {
                return CreateRegistrationField(bytes, field);
            }
            offset = checked((int)field.PayloadEnd);
        }
        throw new InvalidDataException(
            $"spPartitionSystem [{partitionSystem.Index}] has no collision registry template.");
    }

    private static RegistrationField CreateRegistrationField(
        ReadOnlySpan<byte> ownerBytes,
        SmoDataBlockHeader field) => new(
            field.FieldType,
            ownerBytes.Slice(
                field.Offset,
                checked((int)(field.PayloadEnd - field.Offset))).ToArray(),
            field.PayloadOffset - field.Offset);

    private static SmoObjectEntry FindPartitionSystemAncestor(
        SmoDocument document,
        SmoObjectEntry collision)
    {
        if (TryFindPartitionSystemAncestor(
                document, collision, out SmoObjectEntry partitionSystem))
        {
            return partitionSystem;
        }
        throw new InvalidOperationException(
            $"Collision [{collision.Index}] is not inside an spPartitionSystem.");
    }

    private static bool TryFindPartitionSystemAncestor(
        SmoDocument document,
        SmoObjectEntry collision,
        out SmoObjectEntry partitionSystem)
    {
        SmoObjectEntry? cursor = collision;
        while (cursor is not null)
        {
            if (cursor.TypeHash == SmoClassIds.PartitionSystem)
            {
                partitionSystem = cursor;
                return true;
            }
            cursor = cursor.ParentIndex is int parentIndex
                ? document.Objects[parentIndex]
                : null;
        }
        partitionSystem = null!;
        return false;
    }

    private static ReadOnlySpan<byte> ObjectBytes(
        SmoDocument document,
        SmoObjectEntry entry) => document.Data.Span.Slice(
            checked((int)entry.PhysicalOffset),
            checked((int)entry.SerializedSize));

    private static byte[] BuildMeshObject(
        IReadOnlyList<Vector3> positions,
        IReadOnlyList<int> indices)
    {
        int payloadSize = checked(
            3 * sizeof(uint) +
            indices.Count * sizeof(ushort) +
            3 * sizeof(uint) +
            positions.Count * 3 * sizeof(float));
        byte[] result = new byte[checked(ObjectSignatureSize + 5 + payloadSize + 1)];
        WriteUInt32(result, 0, SmoClassIds.MeshBoundingVolume);
        "SBOO"u8.CopyTo(result.AsSpan(sizeof(uint)));
        result[ObjectSignatureSize] = 0xE0; // field 0, UInt32 payload size
        WriteUInt32(result, ObjectSignatureSize + 1, checked((uint)payloadSize));
        int offset = ObjectSignatureSize + 5;
        WriteUInt32(result, offset, 2);
        WriteUInt32(result, offset + 4, checked((uint)(indices.Count / 3)));
        WriteUInt32(result, offset + 8, 0);
        offset += 3 * sizeof(uint);
        foreach (int index in indices)
        {
            BinaryPrimitives.WriteUInt16LittleEndian(
                result.AsSpan(offset), checked((ushort)index));
            offset += sizeof(ushort);
        }
        WriteUInt32(result, offset, 0);
        WriteUInt32(result, offset + 4, checked((uint)positions.Count));
        WriteUInt32(result, offset + 8, 0);
        offset += 3 * sizeof(uint);
        foreach (Vector3 position in positions)
        {
            WriteSingle(result, offset, position.X);
            WriteSingle(result, offset + 4, position.Y);
            WriteSingle(result, offset + 8, position.Z);
            offset += 3 * sizeof(float);
        }
        // Native spDataBlockSerializer objects end with an empty field. The
        // array is zero-initialized, so the final byte is the canonical 0x00.
        return result;
    }

    private static bool HasTerminalEmptyField(
        SmoDocument document,
        SmoObjectEntry entry)
    {
        ReadOnlySpan<byte> bytes = ObjectBytes(document, entry);
        int offset = ObjectSignatureSize;
        SmoDataBlockHeader last = default;
        bool hasField = false;
        while (offset < bytes.Length &&
               SmoDataBlockReader.TryReadHeader(
                   bytes, offset, out SmoDataBlockHeader field))
        {
            last = field;
            hasField = true;
            offset = checked((int)field.PayloadEnd);
        }
        return hasField && offset == bytes.Length &&
               last.RawHeader == 0 && last.PayloadSize == 0;
    }

    private static (int PhysicalOffset, int Length, SmoDataBlockHeader Header)
        FindInlineField(
            SmoDocument document,
            SmoObjectEntry parent,
            SmoObjectEntry child)
    {
        ReadOnlySpan<byte> serialized = document.Data.Span.Slice(
            checked((int)parent.PhysicalOffset),
            checked((int)parent.SerializedSize));
        int offset = ObjectSignatureSize;
        while (offset < serialized.Length &&
               SmoDataBlockReader.TryReadHeader(
                   serialized,
                   offset,
                   out SmoDataBlockHeader field))
        {
            long payloadPhysical = parent.PhysicalOffset + field.PayloadOffset;
            if (payloadPhysical == child.PhysicalOffset - ObjectReferenceSize &&
                field.PayloadSize == child.SerializedSize + ObjectReferenceSize)
            {
                return (
                    checked((int)(parent.PhysicalOffset + field.Offset)),
                    checked((int)(field.PayloadEnd - field.Offset)),
                    field);
            }
            offset = checked((int)field.PayloadEnd);
        }
        throw new InvalidOperationException(
            $"Inline field for collision object [{child.Index}] was not found.");
    }

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
                throw new NotSupportedException(
                    $"Collision template uses non-resizable {original.SizeKind} field.");
        }
    }

    private static bool CanResizeTemplate(
        SmoDocument document,
        SmoCollisionMesh template,
        int generatedMeshSize)
    {
        try
        {
            SmoObjectEntry owner = document.Objects[template.NodeObjectIndex];
            SmoObjectEntry collision =
                document.Objects[template.CollisionInfoObjectIndex];
            SmoObjectEntry mesh =
                document.Objects[template.MeshBoundingVolumeObjectIndex];
            (_, _, SmoDataBlockHeader outer) =
                FindInlineField(document, owner, collision);
            (_, _, SmoDataBlockHeader inner) =
                FindInlineField(document, collision, mesh);
            int delta = generatedMeshSize - checked((int)mesh.SerializedSize);
            return CanStoreSize(
                       outer.SizeKind,
                       checked((int)outer.PayloadSize + delta)) &&
                   CanStoreSize(
                       inner.SizeKind,
                       checked((int)inner.PayloadSize + delta));
        }
        catch (Exception exception) when (
            exception is InvalidOperationException or OverflowException)
        {
            return false;
        }
    }

    private static bool CanStoreSize(SmoDataBlockSizeCode kind, int value) =>
        value >= 0 && kind switch
        {
            SmoDataBlockSizeCode.UInt8 => value <= byte.MaxValue,
            SmoDataBlockSizeCode.UInt16 => value <= ushort.MaxValue,
            SmoDataBlockSizeCode.UInt32 => true,
            _ => false
        };

    private static uint NextId(SmoDocument document, ref uint candidate)
    {
        bool occupied;
        do
        {
            candidate = checked(candidate + 1);
            occupied = false;
            foreach (SmoObjectEntry entry in document.Objects)
            {
                if (entry.Id != candidate)
                    continue;
                occupied = true;
                break;
            }
        }
        while (occupied);
        return candidate;
    }

    private static Vector3 CalculateCenter(SmoCollisionMesh collision)
    {
        Vector3 sum = Vector3.Zero;
        foreach (Vector3 position in collision.Positions)
            sum += Vector3.Transform(position, collision.WorldTransform);
        return sum / Math.Max(collision.Positions.Count, 1);
    }

    private static void ValidateGeometry(
        IReadOnlyList<Vector3> positions,
        IReadOnlyList<int> indices)
    {
        ArgumentNullException.ThrowIfNull(positions);
        ArgumentNullException.ThrowIfNull(indices);
        if (positions.Count < 4 || positions.Count > ushort.MaxValue)
            throw new ArgumentException("Collision needs 4..65535 vertices.");
        if (indices.Count < 12 || indices.Count % 3 != 0)
            throw new ArgumentException("Collision indices do not form triangles.");
        if (positions.Any(position =>
                !float.IsFinite(position.X) ||
                !float.IsFinite(position.Y) ||
                !float.IsFinite(position.Z)))
            throw new ArgumentException("Collision contains non-finite vertices.");
        if (indices.Any(index => (uint)index >= (uint)positions.Count))
            throw new ArgumentException("Collision contains an invalid vertex index.");
    }

    private static void WriteUInt32(Span<byte> data, int offset, uint value) =>
        BinaryPrimitives.WriteUInt32LittleEndian(data[offset..], value);

    private static void WriteSingle(Span<byte> data, int offset, float value) =>
        BinaryPrimitives.WriteInt32LittleEndian(
            data[offset..], BitConverter.SingleToInt32Bits(value));

    private readonly record struct RegistrationField(
        int FieldType,
        byte[] FieldData,
        int TargetOffset);
}
