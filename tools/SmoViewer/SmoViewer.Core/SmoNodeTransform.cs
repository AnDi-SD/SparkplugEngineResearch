using System.Buffers.Binary;
using System.Numerics;
using System.Runtime.CompilerServices;

namespace SmoViewer.Core;

/// <summary>Confirmed base-node position, rotation and scale fields.</summary>
public sealed record SmoNodeTransform(
    int ObjectIndex,
    Vector3 Position,
    Quaternion Rotation,
    Vector3 Scale)
{
    public Matrix4x4 LocalMatrix =>
        Matrix4x4.CreateScale(Scale) *
        Matrix4x4.CreateFromQuaternion(Rotation) *
        Matrix4x4.CreateTranslation(Position);
}

public static class SmoNodeTransformDecoder
{
    private const int ObjectSignatureSize = 8;
    private static readonly ConditionalWeakTable<SmoDocument, TransformContext>
        Contexts = new();

    private sealed record TransformContext(
        IReadOnlyDictionary<int, SmoObjectEntry> Entries,
        SmoNodeHierarchy Hierarchy,
        IReadOnlyDictionary<int, Matrix4x4> BindWorldMatrices);

    public static bool TryDecode(
        SmoDocument document,
        SmoObjectEntry entry,
        out SmoNodeTransform? transform)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(entry);
        transform = null;
        if (!entry.IsWithinDataSection || !entry.SignatureMatches ||
            entry.PhysicalOffset < 0 || entry.PhysicalOffset > int.MaxValue ||
            entry.SerializedSize > int.MaxValue ||
            entry.PhysicalEnd > document.Data.Length)
        {
            return false;
        }

        ReadOnlySpan<byte> data = document.Data.Span.Slice(
            (int)entry.PhysicalOffset, (int)entry.SerializedSize);
        if (!TryFindOwnTransformFields(
                document,
                entry,
                data,
                out ReadOnlySpan<byte> position,
                out ReadOnlySpan<byte> rotation,
                out ReadOnlySpan<byte> scale))
        {
            return false;
        }

        Vector3 positionValue = ReadVector3(position);
        Quaternion rotationValue = rotation.IsEmpty
            ? Quaternion.Identity
            : new Quaternion(
                ReadSingle(rotation, 0),
                ReadSingle(rotation, 4),
                ReadSingle(rotation, 8),
                ReadSingle(rotation, 12));
        Vector3 scaleValue = Vector3.One;
        if (!scale.IsEmpty)
            scaleValue = ReadVector3(scale);
        if (!IsFinite(positionValue) || !IsFinite(scaleValue) ||
            !float.IsFinite(rotationValue.X) || !float.IsFinite(rotationValue.Y) ||
            !float.IsFinite(rotationValue.Z) || !float.IsFinite(rotationValue.W) ||
            rotationValue.LengthSquared() < 0.000001f)
        {
            return false;
        }

        rotationValue = Quaternion.Normalize(rotationValue);
        transform = new SmoNodeTransform(
            entry.Index, positionValue, rotationValue, scaleValue);
        return true;
    }

    public static Matrix4x4 ResolveModelWorldMatrix(
        SmoDocument document,
        SmoObjectEntry meshEntry)
    {
        TransformContext context = Contexts.GetValue(document, static value =>
            new TransformContext(
                value.Objects.ToDictionary(entry => entry.Index),
                SmoNodeHierarchy.Decode(value),
                SmoSkinBindingResolver.ResolveBindWorldMatrices(value)));
        IReadOnlyDictionary<int, SmoObjectEntry> entries = context.Entries;
        SmoNodeHierarchy hierarchy = context.Hierarchy;
        IReadOnlyDictionary<int, Matrix4x4> bindWorldMatrices =
            context.BindWorldMatrices;
        SmoObjectEntry? cursor = meshEntry;
        SmoObjectEntry? model = meshEntry.TypeHash == SmoClassIds.Model
            ? meshEntry
            : null;
        while (model is null &&
               cursor.ParentIndex is int parentIndex &&
               entries.TryGetValue(parentIndex, out cursor))
        {
            if (cursor.TypeHash == SmoClassIds.Model)
            {
                model = cursor;
                break;
            }
        }

        if (model is null)
            return Matrix4x4.Identity;

        Matrix4x4 world = Matrix4x4.Identity;
        cursor = model;
        while (cursor is not null)
        {
            if (cursor != model &&
                bindWorldMatrices.TryGetValue(cursor.Index, out Matrix4x4 bindWorld))
            {
                world *= bindWorld;
                break;
            }
            else if (cursor.TypeHash == SmoClassIds.StaticRenderObject &&
                SmoStaticRenderObjectTransformDecoder.TryDecode(
                    document, cursor, out Matrix4x4 staticTransform))
            {
                world *= staticTransform;
                // This serializer field is explicitly a world transform. The
                // containing partition/portal intervals describe storage and
                // culling, not an additional transform hierarchy.
                break;
            }
            else if (CanOwnNodeTransform(cursor.TypeHash) &&
                TryDecode(document, cursor, out SmoNodeTransform? transform) &&
                transform is not null)
            {
                world *= transform.LocalMatrix;
            }

            int? logicalParent = hierarchy.ParentsByChild.TryGetValue(
                    cursor.Index, out IReadOnlyList<int>? graphParents) &&
                graphParents.Count == 1
                    ? graphParents[0]
                    : cursor.ParentIndex;
            cursor = logicalParent is int parentIndex &&
                     entries.TryGetValue(parentIndex, out SmoObjectEntry? parent)
                ? parent : null;
        }

        return world;
    }

    private static bool CanOwnNodeTransform(uint typeHash) => typeHash is
        SmoClassIds.Node or SmoClassIds.RenderNode or SmoClassIds.Model;

    private static bool TryFindOwnTransformFields(
        SmoDocument document,
        SmoObjectEntry entry,
        ReadOnlySpan<byte> data,
        out ReadOnlySpan<byte> position,
        out ReadOnlySpan<byte> rotation,
        out ReadOnlySpan<byte> scale)
    {
        position = default;
        rotation = default;
        scale = default;

        var excluded = document.Objects
            .Where(candidate => candidate.ParentIndex == entry.Index)
            .Select(candidate => (
                Start: checked((int)(candidate.LogicalOffset - entry.LogicalOffset)),
                End: checked((int)(candidate.LogicalEnd - entry.LogicalOffset))))
            .OrderBy(interval => interval.Start)
            .ToArray();

        int rangeStart = ObjectSignatureSize;
        foreach ((int childStart, int childEnd) in excluded.Append((data.Length, data.Length)))
        {
            if (TryFindTransformInRange(
                    data, rangeStart, childStart, out position, out rotation, out scale))
                return true;
            rangeStart = Math.Max(rangeStart, childEnd);
        }

        return false;
    }

    private static bool TryFindTransformInRange(
        ReadOnlySpan<byte> data,
        int start,
        int end,
        out ReadOnlySpan<byte> position,
        out ReadOnlySpan<byte> rotation,
        out ReadOnlySpan<byte> scale)
    {
        position = default;
        rotation = default;
        scale = default;
        start = Math.Clamp(start, ObjectSignatureSize, data.Length);
        end = Math.Clamp(end, start, data.Length);
        for (int candidate = start; candidate < end; candidate++)
        {
            int offset = candidate;
            if (!TryReadField(
                    data, ref offset, 0, 3 * sizeof(float), out position) ||
                offset > end)
            {
                position = default;
                rotation = default;
                continue;
            }

            int rotationOffset = offset;
            if (TryReadField(
                    data, ref rotationOffset, 1, 4 * sizeof(float), out rotation) &&
                rotationOffset <= end)
                offset = rotationOffset;
            else
                rotation = default;

            int scaleOffset = offset;
            if (TryReadField(
                    data, ref scaleOffset, 2, 3 * sizeof(float), out scale) &&
                scaleOffset > end)
                scale = default;
            return true;
        }

        return false;
    }

    private static bool TryReadField(
        ReadOnlySpan<byte> data,
        scoped ref int offset,
        int expectedType,
        int expectedSize,
        out ReadOnlySpan<byte> payload)
    {
        payload = default;
        if (!SmoDataBlockReader.TryReadHeader(data, offset, out SmoDataBlockHeader header) ||
            header.FieldType != expectedType || header.PayloadSize != expectedSize)
        {
            return false;
        }

        payload = data.Slice(header.PayloadOffset, expectedSize);
        offset = checked((int)header.PayloadEnd);
        return true;
    }

    private static Vector3 ReadVector3(ReadOnlySpan<byte> data) =>
        new(ReadSingle(data, 0), ReadSingle(data, 4), ReadSingle(data, 8));

    private static float ReadSingle(ReadOnlySpan<byte> data, int offset) =>
        BitConverter.Int32BitsToSingle(
            BinaryPrimitives.ReadInt32LittleEndian(data.Slice(offset, sizeof(float))));

    private static bool IsFinite(Vector3 value) =>
        float.IsFinite(value.X) && float.IsFinite(value.Y) && float.IsFinite(value.Z);
}

/// <summary>
/// Decodes the transform stored directly by spStaticRenderObject.  These
/// objects wrap placed level models; their first two fields are the world
/// transform (0x01) and its inverse (0x02), both serialized as row-major 4x4
/// matrices for Sparkplug's row-vector convention.
/// </summary>
public static class SmoStaticRenderObjectTransformDecoder
{
    private const int ObjectSignatureSize = 8;
    private const int MatrixByteSize = 16 * sizeof(float);

    public static bool TryDecode(
        SmoDocument document,
        SmoObjectEntry entry,
        out Matrix4x4 transform)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(entry);
        transform = Matrix4x4.Identity;
        if (entry.TypeHash != SmoClassIds.StaticRenderObject ||
            !entry.IsWithinDataSection || !entry.SignatureMatches ||
            entry.PhysicalOffset < 0 || entry.PhysicalOffset > int.MaxValue ||
            entry.SerializedSize > int.MaxValue ||
            entry.PhysicalEnd > document.Data.Length)
        {
            return false;
        }

        ReadOnlySpan<byte> data = document.Data.Span.Slice(
            (int)entry.PhysicalOffset, (int)entry.SerializedSize);
        if (!SmoDataBlockReader.TryReadHeader(
                data, ObjectSignatureSize, out SmoDataBlockHeader forward) ||
            forward.FieldType != 1 || forward.PayloadSize != MatrixByteSize ||
            !TryReadMatrix(data.Slice(forward.PayloadOffset, MatrixByteSize), out transform))
        {
            return false;
        }

        // The matching inverse field is structural validation for the pair. Some
        // level objects intentionally have singular transforms (for example a
        // zero scale used to hide a placed object), so mathematical invertibility
        // must not be required here.
        int inverseOffset = checked((int)forward.PayloadEnd);
        if (!SmoDataBlockReader.TryReadHeader(
                data, inverseOffset, out SmoDataBlockHeader inverseHeader) ||
            inverseHeader.FieldType != 2 || inverseHeader.PayloadSize != MatrixByteSize ||
            !TryReadMatrix(
                data.Slice(inverseHeader.PayloadOffset, MatrixByteSize), out _))
        {
            transform = Matrix4x4.Identity;
            return false;
        }

        return true;
    }

    private static bool TryReadMatrix(ReadOnlySpan<byte> data, out Matrix4x4 value)
    {
        Span<float> cells = stackalloc float[16];
        for (int index = 0; index < cells.Length; index++)
        {
            cells[index] = BitConverter.Int32BitsToSingle(
                BinaryPrimitives.ReadInt32LittleEndian(data.Slice(index * 4, 4)));
            if (!float.IsFinite(cells[index]))
            {
                value = Matrix4x4.Identity;
                return false;
            }
        }

        value = new Matrix4x4(
            cells[0], cells[1], cells[2], cells[3],
            cells[4], cells[5], cells[6], cells[7],
            cells[8], cells[9], cells[10], cells[11],
            cells[12], cells[13], cells[14], cells[15]);
        return true;
    }

}
