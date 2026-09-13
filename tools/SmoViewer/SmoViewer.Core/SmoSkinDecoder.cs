using System.Buffers.Binary;
using System.Numerics;

namespace SmoViewer.Core;

public sealed record SmoSkinBone(
    int PaletteIndex,
    int NodeObjectIndex,
    uint NodeObjectId,
    uint InlineSerializedSize,
    Matrix4x4 InverseBindMatrix);

/// <summary>Confirmed bone palette serialized by <c>spSkinSerializer</c>.</summary>
public sealed record SmoSkin(
    int ObjectIndex,
    string Name,
    IReadOnlyList<SmoSkinBone> Bones,
    uint? AlphaSortEnable,
    uint? Priority);

public static class SmoSkinDecoder
{
    private const int ObjectSignatureSize = 8;
    private const int ReferenceSize = 2 * sizeof(uint);
    private const int MatrixSize = 16 * sizeof(float);

    public static bool TryDecode(
        SmoDocument document,
        SmoObjectEntry entry,
        out SmoSkin? skin,
        out string error)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(entry);
        skin = null;
        error = string.Empty;

        if (entry.TypeHash != SmoClassIds.Skin || !entry.IsWithinDataSection ||
            !entry.SignatureMatches || entry.PhysicalOffset < 0 ||
            entry.PhysicalOffset > int.MaxValue || entry.SerializedSize > int.MaxValue ||
            entry.PhysicalEnd > document.Data.Length)
        {
            error = $"INVALID_SKIN_ENTRY: Object [{entry.Index}] is not a confirmed spSkin.";
            return false;
        }

        Dictionary<uint, SmoObjectEntry> nodesById = document.Objects
            .Where(item => item.TypeHash == SmoClassIds.Node)
            .GroupBy(item => item.Id)
            .Where(group => group.Count() == 1)
            .ToDictionary(group => group.Key, group => group.Single());
        ReadOnlySpan<byte> serialized = document.Data.Span.Slice(
            (int)entry.PhysicalOffset, (int)entry.SerializedSize);

        int offset = ObjectSignatureSize;
        List<SmoSkinBone>? decodedPalette = null;
        uint? alphaSortEnable = null;
        uint? priority = null;
        while (offset < serialized.Length &&
               SmoDataBlockReader.TryReadHeader(
                   serialized, offset, out SmoDataBlockHeader header))
        {
            if (header.FieldType == 0 && header.PayloadSize >= 2 * sizeof(uint))
            {
                ReadOnlySpan<byte> payload = serialized.Slice(
                    header.PayloadOffset, checked((int)header.PayloadSize));
                if (TryDecodePalette(payload, nodesById, out List<SmoSkinBone>? palette))
                    decodedPalette = palette;
            }
            else if (header.PayloadSize == sizeof(uint) &&
                     header.FieldType == 2 &&
                     !alphaSortEnable.HasValue)
            {
                // Direct top-level spSkin field. Shipped alpha-sorted skin
                // consumers use value 1; generated regression fixtures use 0.
                alphaSortEnable = BinaryPrimitives.ReadUInt32LittleEndian(
                    serialized[header.PayloadOffset..]);
            }
            else if (header.PayloadSize == sizeof(uint) &&
                     header.FieldType == 3 &&
                     !priority.HasValue)
            {
                // Direct top-level spSkin render priority. Native ordering
                // semantics are not inferred from the observed numeric value.
                priority = BinaryPrimitives.ReadUInt32LittleEndian(
                    serialized[header.PayloadOffset..]);
            }

            int next = checked((int)header.PayloadEnd);
            if (next <= offset)
                break;
            offset = next;
        }

        if (decodedPalette is null)
        {
            error = $"SKIN_BONE_PALETTE_NOT_FOUND: Object [{entry.Index}] \"{entry.Name}\" " +
                    "does not contain a structurally valid node-reference palette.";
            return false;
        }

        skin = new SmoSkin(
            entry.Index,
            entry.Name,
            decodedPalette.AsReadOnly(),
            alphaSortEnable,
            priority);
        return true;
    }

    private static bool TryDecodePalette(
        ReadOnlySpan<byte> payload,
        IReadOnlyDictionary<uint, SmoObjectEntry> nodesById,
        out List<SmoSkinBone>? bones)
    {
        bones = null;
        uint reserved = BinaryPrimitives.ReadUInt32LittleEndian(payload);
        uint count = BinaryPrimitives.ReadUInt32LittleEndian(payload.Slice(sizeof(uint)));
        if (reserved != 0 || count is 0 or > 256)
            return false;

        int offset = 2 * sizeof(uint);
        var result = new List<SmoSkinBone>(checked((int)count));
        for (int paletteIndex = 0; paletteIndex < count; paletteIndex++)
        {
            if (offset > payload.Length - ReferenceSize)
                return false;
            uint nodeId = BinaryPrimitives.ReadUInt32LittleEndian(payload.Slice(offset));
            uint inlineSize = BinaryPrimitives.ReadUInt32LittleEndian(
                payload.Slice(offset + sizeof(uint)));
            offset += ReferenceSize;
            if (!nodesById.TryGetValue(nodeId, out SmoObjectEntry? node) ||
                (inlineSize != 0 && inlineSize != node.SerializedSize) ||
                inlineSize > int.MaxValue ||
                offset > payload.Length - checked((int)inlineSize) - MatrixSize)
            {
                return false;
            }

            offset += (int)inlineSize;
            if (!TryReadMatrix(payload.Slice(offset, MatrixSize), out Matrix4x4 inverseBind))
                return false;
            offset += MatrixSize;
            result.Add(new SmoSkinBone(
                paletteIndex, node.Index, nodeId, inlineSize, inverseBind));
        }

        if (offset != payload.Length)
            return false;
        bones = result;
        return true;
    }

    private static bool TryReadMatrix(ReadOnlySpan<byte> data, out Matrix4x4 matrix)
    {
        Span<float> values = stackalloc float[16];
        for (int index = 0; index < values.Length; index++)
        {
            int bits = BinaryPrimitives.ReadInt32LittleEndian(data.Slice(index * 4, 4));
            values[index] = BitConverter.Int32BitsToSingle(bits);
            if (!float.IsFinite(values[index]))
            {
                matrix = Matrix4x4.Identity;
                return false;
            }
        }

        matrix = new Matrix4x4(
            values[0], values[1], values[2], values[3],
            values[4], values[5], values[6], values[7],
            values[8], values[9], values[10], values[11],
            values[12], values[13], values[14], values[15]);
        return true;
    }
}
