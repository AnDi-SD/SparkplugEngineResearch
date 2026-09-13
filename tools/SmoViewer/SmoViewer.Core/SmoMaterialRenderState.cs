using System.Buffers.Binary;
using System.Collections.ObjectModel;

namespace SmoViewer.Core;

/// <summary>Decodes the render flags stored by <c>spMaterialData</c>.</summary>
public static class SmoMaterialRenderState
{
    private const int ObjectSignatureSize = 8;
    private const uint AlphaBlendFlag = 0x4;

    public static IReadOnlyDictionary<int, uint> ResolveAll(SmoDocument document)
    {
        ArgumentNullException.ThrowIfNull(document);
        Dictionary<int, List<SmoObjectEntry>> children = document.Objects
            .Where(entry => entry.ParentIndex.HasValue)
            .GroupBy(entry => entry.ParentIndex!.Value)
            .ToDictionary(group => group.Key, group => group.ToList());
        var result = new Dictionary<int, uint>();

        foreach (List<SmoObjectEntry> siblings in children.Values)
        {
            SmoObjectEntry[] meshes = siblings
                .Where(entry => entry.TypeHash == SmoClassIds.MeshData)
                .ToArray();
            SmoObjectEntry[] materials = siblings
                .Where(entry => entry.TypeHash == SmoClassIds.MaterialData)
                .ToArray();
            if (meshes.Length == 0 || materials.Length == 0)
                continue;

            for (int meshIndex = 0; meshIndex < meshes.Length; meshIndex++)
            {
                int materialIndex = materials.Length == 1
                    ? 0
                    : materials.Length == meshes.Length
                        ? meshIndex
                        : Array.FindLastIndex(
                            materials,
                            material => material.LogicalOffset < meshes[meshIndex].LogicalOffset);
                if (materialIndex < 0 ||
                    !TryDecodeFlags(document, materials[materialIndex], out uint flags))
                {
                    continue;
                }

                result[meshes[meshIndex].Index] = flags;
            }
        }

        return new ReadOnlyDictionary<int, uint>(result);
    }

    public static bool TryDecodeFlags(
        SmoDocument document,
        SmoObjectEntry material,
        out uint flags)
    {
        flags = 0;
        if (material.TypeHash != SmoClassIds.MaterialData ||
            !material.IsWithinDataSection || !material.SignatureMatches ||
            material.PhysicalOffset < 0 || material.PhysicalOffset > int.MaxValue ||
            material.SerializedSize > int.MaxValue ||
            material.PhysicalEnd > document.Data.Length)
        {
            return false;
        }

        ReadOnlySpan<byte> data = document.Data.Span.Slice(
            (int)material.PhysicalOffset, (int)material.SerializedSize);
        int offset = ObjectSignatureSize;
        while (SmoDataBlockReader.TryReadHeader(data, offset, out SmoDataBlockHeader header))
        {
            if (header.FieldType == 3 && header.PayloadSize == sizeof(uint))
            {
                flags = BinaryPrimitives.ReadUInt32LittleEndian(
                    data.Slice(header.PayloadOffset, sizeof(uint)));
                return true;
            }

            if (header.PayloadEnd <= offset || header.PayloadEnd > data.Length)
                break;
            offset = checked((int)header.PayloadEnd);
        }

        return false;
    }

    public static bool UsesAlphaBlend(uint flags) =>
        (flags & AlphaBlendFlag) != 0;
}
