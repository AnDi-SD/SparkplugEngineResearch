using SmoViewer.Sparkplug;
using System.Diagnostics.CodeAnalysis;
using System.Numerics;

namespace SmoViewer.Core;

/// <summary>
/// The world transform serialized by <c>spCollisionInfo</c> as
/// <c>esfCollisionInfoTransform</c>: position, rotation and scale.
/// </summary>
public sealed record SmoCollisionInfoTransform(
    Vector3 Position,
    Quaternion Rotation,
    Vector3 Scale)
{
    public Matrix4x4 WorldMatrix =>
        SparkplugNode.LocalMatrix(Position, Rotation, Scale);
}

/// <summary>
/// Authored scalar state and inspected primitive of one CollisionInfo section.
/// Missing fields keep native defaults; SerializedFieldMask preserves presence.
/// This metadata view does not instantiate an attached collision or its primitive.
/// </summary>
public sealed record SmoCollisionInfoData(
    SmoNodeRelationship Primitive,
    uint? CollisionGroup,
    SmoCollisionInfoTransform? Transform,
    byte SerializedFieldMask)
{
    /// <summary>Fresh native CollisionInfo group after the observed scalar fields.</summary>
    public uint EffectiveCollisionGroup { get; init; }

    public bool IsFieldSerialized(int fieldType) =>
        fieldType is >= 0 and <= 2 &&
        (SerializedFieldMask & (1 << fieldType)) != 0;
}

/// <summary>Shared original scalar reader with a metadata-only primitive resolver.</summary>
public static class SmoCollisionInfoDecoder
{
    public const int GroupPayloadSize = sizeof(uint);
    public const int TransformPayloadSize =
        3 * sizeof(float) + 4 * sizeof(float) + 3 * sizeof(float);

    public static bool TryDecode(
        SmoDocument document,
        SmoObjectEntry entry,
        [NotNullWhen(true)] out SmoCollisionInfoData? value,
        out string error)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(entry);
        value = null;
        error = string.Empty;
        if (entry.TypeHash != SmoClassIds.CollisionInfo)
        {
            error = $"Object [{entry.Index}] is not spCollisionInfo.";
            return false;
        }
        if (!SmoObjectFieldReader.TryRead(document,entry,out var fields,out error))
            return false;
        if (fields.Count == 0 || fields[^1].FieldType != 0 || fields[^1].PayloadSize != 0)
        {
            error = "CollisionInfo requires one complete serializer section.";
            return false;
        }
        var scalars = new List<NativeMethods.NodeField>();
        SmoNodeRelationship primitive = new(0,0,SmoNodeRelationshipEncoding.IdOnly,null,null,null);
        byte referenceMask = 0;
        foreach (var field in fields.Take(fields.Count - 1))
        {
            if (field.PayloadSize == 0)
            {
                error = "CollisionInfo contains more than one serializer section.";
                return false;
            }
            if (field.FieldType == 0)
            {
                if (!SmoNodeDecoder.TryDecodeRelationship(document,field.Payload.Span,out var link) || link is null ||
                    (link.ObjectId != 0 && (link.TargetObjectIndex is not int ||
                        link.TargetTypeHash is not uint kind || !IsBoundingVolumeClass(kind))))
                {
                    error = "CollisionInfo inspector cannot resolve the primitive to a supported bounding volume.";
                    return false;
                }
                if (link.Encoding == SmoNodeRelationshipEncoding.InlineObject &&
                    document.Objects[link.TargetObjectIndex!.Value].ParentIndex != entry.Index)
                {
                    error = "Inline CollisionInfo primitive is not its physical child.";
                    return false;
                }
                primitive = link; referenceMask = 1;
            }
            else if (field.FieldType is 1 or 2)
                scalars.Add(new NativeMethods.NodeField {Field=(uint)field.FieldType,
                    Offset=checked((uint)field.AbsolutePayloadOffset),Size=field.PayloadSize});
            // Unknown bounded fields are skipped by the original serializer.
        }
        if (!TryReadScalars(document.Data.Span,scalars.ToArray(),out var native))
        {
            error = "Invalid bounded CollisionInfo scalar payload.";
            return false;
        }
        byte mask = (byte)(native.FieldMask | referenceMask);
        value = new SmoCollisionInfoData(primitive,(mask & 2) != 0 ? native.Group : null,
            (mask & 4) != 0 ? new SmoCollisionInfoTransform(native.Position,native.Rotation,native.Scale) : null,mask)
            { EffectiveCollisionGroup = native.Group };
        return true;
    }

    public static bool IsBoundingVolumeClass(uint typeHash) => typeHash is
        SmoClassIds.BoundingVolume or
        SmoClassIds.MeshBoundingVolume or
        SmoClassIds.OrientedBoxBoundingVolume or
        SmoClassIds.BoxBoundingVolume or
        SmoClassIds.SphereBoundingVolume or
        SmoClassIds.CapsuleBoundingVolume or
        SmoClassIds.ConvexBoundingVolume;

    internal static bool TryDecodeTransformPayload(
        ReadOnlySpan<byte> payload,
        [NotNullWhen(true)] out SmoCollisionInfoTransform? transform)
    {
        transform = null;
        if (!TryReadScalars(payload,[new NativeMethods.NodeField {Field=2,Size=checked((uint)payload.Length)}],out var native))
            return false;
        transform = new SmoCollisionInfoTransform(native.Position,native.Rotation,native.Scale);
        return true;
    }

    private static unsafe bool TryReadScalars(ReadOnlySpan<byte> bytes,ReadOnlySpan<NativeMethods.NodeField> fields,
        out NativeMethods.CollisionInfoValues values)
    {
        fixed (byte* input = bytes)
        fixed (NativeMethods.NodeField* selected = fields)
            return NativeMethods.spv_collision_info_values(input,checked((uint)bytes.Length),selected,
                checked((uint)fields.Length),out values) != 0;
    }
}

/// <summary>Reads the transform used by scene and placement code.</summary>
public static class SmoCollisionInfoTransformDecoder
{
    public static bool TryDecode(
        SmoDocument document,
        SmoObjectEntry collisionInfo,
        out SmoCollisionInfoTransform? transform)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(collisionInfo);
        transform = null;
        if (!SmoCollisionInfoDecoder.TryDecode(document,collisionInfo,out var data,out _) || data.Transform is null)
            return false;
        transform = data.Transform;
        return true;
    }
}
