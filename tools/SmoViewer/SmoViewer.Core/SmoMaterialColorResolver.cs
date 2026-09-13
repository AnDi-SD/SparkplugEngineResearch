using System.Buffers.Binary;
using System.Collections.ObjectModel;

namespace SmoViewer.Core;

/// <summary>Resolves the packed diffuse color of non-textured spMaterialData.</summary>
public static class SmoMaterialColorResolver
{
    private const int ObjectSignatureSize = 8;

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
                    !TryDecodeDiffuse(document, materials[materialIndex], out uint color))
                    continue;

                result[meshes[meshIndex].Index] = color;
            }
        }

        return new ReadOnlyDictionary<int, uint>(result);
    }

    public static bool TryDecodeDiffuse(
        SmoDocument document,
        SmoObjectEntry material,
        out uint argb)
    {
        argb = 0;
        if (material.TypeHash != SmoClassIds.MaterialData ||
            !material.IsWithinDataSection || !material.SignatureMatches ||
            material.PhysicalOffset < 0 || material.PhysicalOffset > int.MaxValue ||
            material.SerializedSize > int.MaxValue ||
            material.PhysicalEnd > document.Data.Length)
            return false;

        ReadOnlySpan<byte> data = document.Data.Span.Slice(
            (int)material.PhysicalOffset, (int)material.SerializedSize);
        int offset = ObjectSignatureSize;
        while (SmoDataBlockReader.TryReadHeader(data, offset, out SmoDataBlockHeader header))
        {
            // esfMaterialColor: ambient, diffuse, specular and emissive ARGB,
            // followed by the material power. Diffuse is the second DWORD.
            if (header.FieldType == 2 && header.PayloadSize == 20)
            {
                argb = BinaryPrimitives.ReadUInt32LittleEndian(
                    data.Slice(header.PayloadOffset + sizeof(uint), sizeof(uint)));
                return (argb & 0x00FFFFFF) != 0;
            }

            if (header.PayloadEnd <= offset || header.PayloadEnd > data.Length)
                break;
            offset = checked((int)header.PayloadEnd);
        }

        return false;
    }
}
