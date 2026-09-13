using System.Buffers.Binary;
using System.Numerics;

namespace SmoViewer.Core;

public enum SmoPropertyValueKind
{
    RawBytes,
    Vector3,
    Quaternion,
    Matrix4x4
}

/// <summary>A confirmed semantic view over a slice of one serialized field.</summary>
public sealed record SmoPropertyDescriptor(
    string Key,
    string DisplayName,
    SmoFieldSelector Field,
    int PayloadOffset,
    int ValueSize,
    SmoPropertyValueKind ValueKind,
    bool Required,
    SmoFieldInsertion Insertion,
    ReadOnlyMemory<byte> DefaultFieldPayload)
{
    public bool CanMaterialize => !Required && !DefaultFieldPayload.IsEmpty;
}

public sealed record SmoClassSchema(
    uint TypeHash,
    string ClassName,
    IReadOnlyList<SmoPropertyDescriptor> Properties);

public sealed record SmoPropertyCapability(
    SmoPropertyDescriptor Descriptor,
    bool IsPresent,
    bool CanWrite);

/// <summary>
/// Raw object fields plus the semantic properties confirmed for its class.
/// Unknown fields remain visible to research tools without becoming editor UI.
/// </summary>
public sealed record SmoObjectCapabilities(
    int ObjectIndex,
    uint TypeHash,
    string ClassName,
    IReadOnlyList<SmoObjectField> RawFields,
    IReadOnlyList<SmoPropertyCapability> Properties)
{
    public bool Supports(string propertyKey) => Properties.Any(property =>
        property.Descriptor.Key == propertyKey && property.CanWrite);
}

public static class SmoPropertyKeys
{
    public const string Position = "transform.position";
    public const string Rotation = "transform.rotation";
    public const string Scale = "transform.scale";
    public const string WorldMatrix = "transform.world_matrix";
    public const string InverseWorldMatrix = "transform.inverse_world_matrix";
    public const string CollisionGeometry = "collision.geometry";
}

/// <summary>Registry of format facts confirmed by decoders and corpus tests.</summary>
public static class SmoSchemaRegistry
{
    private static readonly IReadOnlyDictionary<uint, SmoClassSchema> Schemas =
        BuildSchemas();

    public static bool TryGet(uint typeHash, out SmoClassSchema? schema) =>
        Schemas.TryGetValue(typeHash, out schema);

    public static SmoObjectCapabilities Describe(
        SmoDocument document,
        SmoObjectEntry entry)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(entry);
        IReadOnlyList<SmoObjectField> fields = SmoObjectFieldReader.Read(document, entry);
        if (!TryGet(entry.TypeHash, out SmoClassSchema? schema) || schema is null)
        {
            return new SmoObjectCapabilities(
                entry.Index,
                entry.TypeHash,
                SmoClassRegistry.GetDisplayName(entry.TypeHash),
                fields,
                Array.Empty<SmoPropertyCapability>());
        }

        SmoPropertyCapability[] properties = schema.Properties.Select(property =>
        {
            SmoObjectField? field = fields.FirstOrDefault(property.Field.Matches);
            bool present = field is not null &&
                property.PayloadOffset <= field.Payload.Length - property.ValueSize;
            return new SmoPropertyCapability(
                property,
                present,
                present || property.CanMaterialize);
        }).ToArray();
        return new SmoObjectCapabilities(
            entry.Index,
            entry.TypeHash,
            schema.ClassName,
            fields,
            properties);
    }

    private static IReadOnlyDictionary<uint, SmoClassSchema> BuildSchemas()
    {
        SmoPropertyDescriptor[] nodeProperties =
        [
            new(
                SmoPropertyKeys.Position,
                "Position",
                new SmoFieldSelector(0, 0, 12),
                0,
                12,
                SmoPropertyValueKind.Vector3,
                true,
                SmoFieldInsertion.BeforeTerminal,
                ReadOnlyMemory<byte>.Empty),
            new(
                SmoPropertyKeys.Rotation,
                "Rotation",
                new SmoFieldSelector(1, 0, 16),
                0,
                16,
                SmoPropertyValueKind.Quaternion,
                false,
                SmoFieldInsertion.After(new SmoFieldSelector(0, 0, 12)),
                IdentityQuaternion()),
            new(
                SmoPropertyKeys.Scale,
                "Scale",
                new SmoFieldSelector(2, 0, 12),
                0,
                12,
                SmoPropertyValueKind.Vector3,
                false,
                SmoFieldInsertion.After(new SmoFieldSelector(1, 0, 16)),
                OneVector3())
        ];

        var result = new Dictionary<uint, SmoClassSchema>();
        foreach ((uint typeHash, string name) in new[]
                 {
                     (SmoClassIds.Node, "spNode"),
                     (SmoClassIds.RenderNode, "spRenderNode"),
                     (SmoClassIds.Model, "spModel")
                 })
        {
            result[typeHash] = new SmoClassSchema(typeHash, name, nodeProperties);
        }

        result[SmoClassIds.StaticRenderObject] = new SmoClassSchema(
            SmoClassIds.StaticRenderObject,
            "spStaticRenderObject",
            [
                new(
                    SmoPropertyKeys.WorldMatrix,
                    "World matrix",
                    new SmoFieldSelector(1, 0, 64),
                    0,
                    64,
                    SmoPropertyValueKind.Matrix4x4,
                    true,
                    SmoFieldInsertion.BeforeTerminal,
                    ReadOnlyMemory<byte>.Empty),
                new(
                    SmoPropertyKeys.InverseWorldMatrix,
                    "Inverse world matrix",
                    new SmoFieldSelector(2, 0, 64),
                    0,
                    64,
                    SmoPropertyValueKind.Matrix4x4,
                    true,
                    SmoFieldInsertion.BeforeTerminal,
                    ReadOnlyMemory<byte>.Empty)
            ]);

        result[SmoClassIds.CollisionInfo] = new SmoClassSchema(
            SmoClassIds.CollisionInfo,
            "spCollisionInfo",
            [
                CollisionTransformProperty(
                    SmoPropertyKeys.Position, "Position", 0, 12,
                    SmoPropertyValueKind.Vector3),
                CollisionTransformProperty(
                    SmoPropertyKeys.Rotation, "Rotation", 12, 16,
                    SmoPropertyValueKind.Quaternion),
                CollisionTransformProperty(
                    SmoPropertyKeys.Scale, "Scale", 28, 12,
                    SmoPropertyValueKind.Vector3)
            ]);

        result[SmoClassIds.MeshBoundingVolume] = new SmoClassSchema(
            SmoClassIds.MeshBoundingVolume,
            "spMeshBV",
            [
                new(
                    SmoPropertyKeys.CollisionGeometry,
                    "Collision geometry",
                    new SmoFieldSelector(0),
                    0,
                    0,
                    SmoPropertyValueKind.RawBytes,
                    true,
                    SmoFieldInsertion.BeforeTerminal,
                    ReadOnlyMemory<byte>.Empty)
            ]);
        return result;
    }

    private static SmoPropertyDescriptor CollisionTransformProperty(
        string key,
        string displayName,
        int offset,
        int size,
        SmoPropertyValueKind kind) => new(
            key,
            displayName,
            new SmoFieldSelector(2, 0, 40),
            offset,
            size,
            kind,
            true,
            SmoFieldInsertion.BeforeTerminal,
            ReadOnlyMemory<byte>.Empty);

    private static byte[] IdentityQuaternion()
    {
        byte[] value = new byte[16];
        WriteSingle(value, 12, 1);
        return value;
    }

    private static byte[] OneVector3()
    {
        byte[] value = new byte[12];
        WriteSingle(value, 0, 1);
        WriteSingle(value, 4, 1);
        WriteSingle(value, 8, 1);
        return value;
    }

    private static void WriteSingle(Span<byte> data, int offset, float value) =>
        BinaryPrimitives.WriteInt32LittleEndian(
            data[offset..], BitConverter.SingleToInt32Bits(value));
}

/// <summary>Queues schema-backed writes through the same raw transaction.</summary>
public static class SmoPropertyMutation
{
    public static void Set(
        SmoMutationTransaction transaction,
        int objectIndex,
        string propertyKey,
        Vector3 value) => SetTyped(
            transaction,
            objectIndex,
            propertyKey,
            SmoPropertyValueKind.Vector3,
            SmoPropertyValueCodec.Encode(value));

    public static void Set(
        SmoMutationTransaction transaction,
        int objectIndex,
        string propertyKey,
        Quaternion value) => SetTyped(
            transaction,
            objectIndex,
            propertyKey,
            SmoPropertyValueKind.Quaternion,
            SmoPropertyValueCodec.Encode(value));

    public static void Set(
        SmoMutationTransaction transaction,
        int objectIndex,
        string propertyKey,
        Matrix4x4 value) => SetTyped(
            transaction,
            objectIndex,
            propertyKey,
            SmoPropertyValueKind.Matrix4x4,
            SmoPropertyValueCodec.Encode(value));

    public static void Set(
        SmoMutationTransaction transaction,
        int objectIndex,
        string propertyKey,
        ReadOnlyMemory<byte> value)
    {
        ArgumentNullException.ThrowIfNull(transaction);
        ArgumentException.ThrowIfNullOrWhiteSpace(propertyKey);
        if ((uint)objectIndex >= (uint)transaction.Source.Objects.Count)
            throw new ArgumentOutOfRangeException(nameof(objectIndex));
        SmoObjectEntry entry = transaction.Source.Objects[objectIndex];
        if (!SmoSchemaRegistry.TryGet(entry.TypeHash, out SmoClassSchema? schema) ||
            schema is null)
        {
            throw new NotSupportedException(
                $"Class 0x{entry.TypeHash:X8} has no confirmed property schema.");
        }
        SmoPropertyDescriptor descriptor = schema.Properties.SingleOrDefault(
                property => property.Key == propertyKey)
            ?? throw new NotSupportedException(
                $"Property '{propertyKey}' is not confirmed for {schema.ClassName}.");
        if (value.Length != descriptor.ValueSize)
        {
            throw new ArgumentException(
                $"Property '{propertyKey}' needs {descriptor.ValueSize} bytes, " +
                $"but received {value.Length}.", nameof(value));
        }

        if (transaction.WillContainField(objectIndex, descriptor.Field))
        {
            transaction.SetFieldPayloadSlice(
                objectIndex,
                descriptor.Field,
                descriptor.PayloadOffset,
                value);
            return;
        }
        if (!descriptor.CanMaterialize)
        {
            throw new InvalidDataException(
                $"Required property '{propertyKey}' is absent from object [{objectIndex}].");
        }

        byte[] payload = descriptor.DefaultFieldPayload.ToArray();
        value.CopyTo(payload.AsMemory(descriptor.PayloadOffset, descriptor.ValueSize));
        EnsureInsertionAnchor(transaction, objectIndex, schema, descriptor.Insertion);
        transaction.UpsertFieldPayload(
            objectIndex,
            descriptor.Field,
            payload,
            descriptor.Insertion);
    }

    private static void SetTyped(
        SmoMutationTransaction transaction,
        int objectIndex,
        string propertyKey,
        SmoPropertyValueKind expectedKind,
        ReadOnlyMemory<byte> value)
    {
        SmoPropertyDescriptor descriptor = GetDescriptor(
            transaction, objectIndex, propertyKey);
        if (descriptor.ValueKind != expectedKind)
        {
            throw new ArgumentException(
                $"Property '{propertyKey}' has kind {descriptor.ValueKind}, not " +
                $"{expectedKind}.", nameof(propertyKey));
        }
        Set(transaction, objectIndex, propertyKey, value);
    }

    private static SmoPropertyDescriptor GetDescriptor(
        SmoMutationTransaction transaction,
        int objectIndex,
        string propertyKey)
    {
        if ((uint)objectIndex >= (uint)transaction.Source.Objects.Count)
            throw new ArgumentOutOfRangeException(nameof(objectIndex));
        SmoObjectEntry entry = transaction.Source.Objects[objectIndex];
        if (!SmoSchemaRegistry.TryGet(entry.TypeHash, out SmoClassSchema? schema) ||
            schema is null)
        {
            throw new NotSupportedException(
                $"Class 0x{entry.TypeHash:X8} has no confirmed property schema.");
        }
        return schema.Properties.SingleOrDefault(property => property.Key == propertyKey)
            ?? throw new NotSupportedException(
                $"Property '{propertyKey}' is not confirmed for {schema.ClassName}.");
    }

    private static void EnsureInsertionAnchor(
        SmoMutationTransaction transaction,
        int objectIndex,
        SmoClassSchema schema,
        SmoFieldInsertion insertion)
    {
        if (insertion.Kind == SmoFieldInsertionKind.BeforeTerminal ||
            insertion.Anchor is not SmoFieldSelector anchor ||
            transaction.WillContainField(objectIndex, anchor))
        {
            return;
        }

        SmoPropertyDescriptor? anchorProperty = schema.Properties.FirstOrDefault(
            property => property.Field == anchor && property.CanMaterialize);
        if (anchorProperty is null)
        {
            throw new InvalidDataException(
                $"Insertion anchor field {anchor.FieldType} is absent and cannot " +
                "be materialized by the confirmed schema.");
        }
        EnsureInsertionAnchor(
            transaction, objectIndex, schema, anchorProperty.Insertion);
        transaction.UpsertFieldPayload(
            objectIndex,
            anchorProperty.Field,
            anchorProperty.DefaultFieldPayload,
            anchorProperty.Insertion);
    }
}
