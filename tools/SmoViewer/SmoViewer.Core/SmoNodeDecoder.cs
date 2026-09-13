using SmoViewer.Sparkplug;
using System.Numerics;
using System.Runtime.CompilerServices;
using System.Diagnostics.CodeAnalysis;

namespace SmoViewer.Core;

public enum SmoNodeRelationshipEncoding
{
    IdOnly,
    SizedReference,
    InlineObject
}

/// <summary>
/// One serialized object relationship used by node and render-node fields. A
/// null reference is a four-byte zero ID; a nonnull ID adds an inline-size
/// word and may contain the complete target SBOO. Parsing uses spSerializer.
/// </summary>
public sealed record SmoNodeRelationship(
    uint ObjectId,
    uint InlineSerializedSize,
    SmoNodeRelationshipEncoding Encoding,
    int? TargetObjectIndex,
    uint? TargetTypeHash,
    string? TargetName);

/// <summary>
/// Authored Node scalar fields and serialized relationships. Flag booleans are
/// the last authored byte (false if omitted); EffectiveFlags reports the real
/// fresh spNode after the same scalar reader. References are not materialized.
/// Quaternion is retained without normalization; the original converts it to
/// a matrix as-is. SerializedFieldMask distinguishes omission from presence.
/// </summary>
public sealed record SmoNodeData(
    Vector3 Position,
    Quaternion Rotation,
    Vector3 Scale,
    bool IsBone,
    bool IsStatic,
    bool IsAnimated,
    uint BillboardAxis,
    IReadOnlyList<SmoNodeRelationship> Children,
    IReadOnlyList<SmoNodeRelationship> Collisions,
    ushort SerializedFieldMask)
{
    public uint EffectiveFlags { get; init; }

    public bool IsFieldSerialized(int fieldType) =>
        fieldType is >= 0 and <= 8 &&
        (SerializedFieldMask & (1 << fieldType)) != 0;
}

/// <summary>Strict read-only decoder for the concrete <c>spNode</c> class.</summary>
public static class SmoNodeDecoder
{
    // Catalog objects are immutable within a document. One host identity index
    // avoids rebuilding a file-sized dictionary for each decoded Node/field.
    private static readonly ConditionalWeakTable<SmoDocument, IReadOnlyDictionary<uint,SmoObjectEntry>> ObjectIndices = new();
    private static IReadOnlyDictionary<uint,SmoObjectEntry> ObjectsById(SmoDocument document) =>
        ObjectIndices.GetValue(document, static value => value.Objects.GroupBy(item => item.Id)
            .Where(group => group.Count() == 1).ToDictionary(group => group.Key, group => group.Single()));
    internal static bool TryGetCataloguedObject(SmoDocument document, uint id,
        [NotNullWhen(true)] out SmoObjectEntry? entry) => ObjectsById(document).TryGetValue(id, out entry);
    // One shared field-to-object binding check for typed leaf decoders.
    public static bool TryDecodeTypedRelationship(
        SmoDocument document, SmoObjectField field,
        IReadOnlyDictionary<uint, SmoObjectEntry> objectsById, uint type,
        [NotNullWhen(true)] out SmoNodeRelationship? value)
    {
        value = null;
        if (!TryDecodeRelationship(field.Payload.Span, objectsById, out value) ||
            value is null || value.TargetObjectIndex is null || value.TargetTypeHash != type)
            return false;
        if (value.Encoding != SmoNodeRelationshipEncoding.InlineObject) return true;
        SmoObjectEntry target = document.Objects[value.TargetObjectIndex.Value];
        return target.PhysicalOffset == field.AbsolutePayloadOffset + SizedRelationshipPrefixSize &&
               target.SerializedSize == value.InlineSerializedSize;
    }

    public const int VectorPayloadSize = 3 * sizeof(float);
    public const int QuaternionPayloadSize = 4 * sizeof(float);
    public const int BooleanPayloadSize = sizeof(byte);
    public const int IdOnlyRelationshipPayloadSize = sizeof(uint);
    public const int SizedRelationshipPrefixSize = 2 * sizeof(uint);

    public static bool TryDecode(
        SmoDocument document,
        SmoObjectEntry entry,
        out SmoNodeData? node)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(entry);
        node = null;
        return entry.TypeHash == SmoClassIds.Node &&
               TryDecodeNodeSection(document,entry,out node);
    }

    internal static unsafe bool TryDecodeNodeSection(
        SmoDocument document, SmoObjectEntry entry, out SmoNodeData? node)
    {
        node = null;
        if (!SmoSerializedFieldRegistry.HasNodeSection(entry.TypeHash) ||
            !SmoObjectFieldReader.TryRead(document, entry, out var fields, out _) ||
            !SmoSerializedFieldRegistry.TryGetNodeSectionRange(entry.TypeHash, fields, out int first, out int terminal))
            return false;
        ushort mask = 0;
        var scalars = new List<NativeMethods.NodeField>();
        var children = new List<SmoNodeRelationship>();
        var collisions = new List<SmoNodeRelationship>();
        var objectsById = ObjectsById(document);
        for (int index = first; index < terminal; index++)
        {
            var field = fields[index];
            if (field.FieldType is < 0 or > 8) continue;
            if (field.FieldType is 5 or 7)
            {
                if (!TryDecodeRelationship(field.Payload.Span, objectsById, out var relationship) || relationship is null)
                    return false;
                (field.FieldType == 5 ? children : collisions).Add(relationship);
            }
            else scalars.Add(new NativeMethods.NodeField {
                Field = (uint)field.FieldType, Offset = (uint)field.AbsolutePayloadOffset, Size = field.PayloadSize });
            mask |= (ushort)(1 << field.FieldType);
        }
        var scalarArray = scalars.ToArray();
        NativeMethods.NodeValues values;
        fixed (byte* bytes = document.Data.Span)
        fixed (NativeMethods.NodeField* selected = scalarArray)
            if (NativeMethods.spv_node_values(bytes, checked((uint)document.Data.Length), selected,
                    (uint)scalarArray.Length, out values) == 0) return false;
        node = new SmoNodeData(values.Position, values.Rotation, values.Scale,
            values.Bone != 0, values.Static != 0, values.Animated != 0, values.Billboard,
            children.AsReadOnly(), collisions.AsReadOnly(), mask) { EffectiveFlags = values.Flags };
        return true;
    }

    public static bool TryDecodeRelationship(
        SmoDocument document,
        ReadOnlySpan<byte> payload,
        out SmoNodeRelationship? relationship)
    {
        ArgumentNullException.ThrowIfNull(document);
        var objectsById = ObjectsById(document);
        return TryDecodeRelationship(payload,objectsById,out relationship);
    }

    public static bool TryDecodeRelationship(
        ReadOnlySpan<byte> payload, out uint objectId,
        out uint inlineSerializedSize, out SmoNodeRelationshipEncoding encoding)
    {
        bool success = TryReadReference(payload, checked((uint)payload.Length), 1, out var prefix);
        objectId = prefix.Id; inlineSerializedSize = prefix.InlineSize;
        encoding = (SmoNodeRelationshipEncoding)prefix.Encoding;
        return success;
    }

    /// <summary>Inspect a retained corpus prefix without loading its target.</summary>
    public static bool TryDecodeRelationshipPrefix(
        ReadOnlySpan<byte> prefix, uint payloadSize, out uint objectId,
        out uint inlineSerializedSize, out SmoNodeRelationshipEncoding encoding)
    {
        bool success = TryReadReference(prefix, payloadSize, 0, out var result);
        objectId = result.Id; inlineSerializedSize = result.InlineSize;
        encoding = (SmoNodeRelationshipEncoding)result.Encoding;
        return success;
    }

    internal static unsafe bool TryReadReference(ReadOnlySpan<byte> bytes,
        uint payloadSize, uint kind, out NativeMethods.ReferencePrefix prefix)
    {
        fixed (byte* data = bytes)
        {
            if (NativeMethods.spv_reference_prefix(data, checked((uint)bytes.Length), payloadSize, kind, out prefix) != 0)
                return true;
            prefix = default;
            return false;
        }
    }

    /// <summary>
    /// Resolves a relationship through a caller-provided unique-ID index. This
    /// avoids rebuilding the object lookup for serializers with repeated
    /// relationship fields.
    /// </summary>
    public static bool TryDecodeRelationship(
        ReadOnlySpan<byte> payload,
        IReadOnlyDictionary<uint,SmoObjectEntry> objectsById,
        out SmoNodeRelationship? relationship)
    {
        relationship = null;
        if (!TryReadReference(payload, checked((uint)payload.Length), 1, out var prefix))
            return false;
        uint objectId = prefix.Id, inlineSize = prefix.InlineSize;
        var encoding = (SmoNodeRelationshipEncoding)prefix.Encoding;
        objectsById.TryGetValue(objectId, out SmoObjectEntry? target);
        if (target is not null && inlineSize > 0 &&
            (target.SerializedSize != inlineSize || prefix.ClassId != target.TypeHash))
            return false;
        relationship = new SmoNodeRelationship(
            objectId, inlineSize, encoding, target?.Index, target?.TypeHash,
            target?.Name.TrimEnd('\0'));
        return true;
    }

}
