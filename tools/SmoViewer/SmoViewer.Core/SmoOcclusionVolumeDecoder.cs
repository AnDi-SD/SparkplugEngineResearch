using System.Diagnostics.CodeAnalysis;
using System.Numerics;
using SmoViewer.Sparkplug;

namespace SmoViewer.Core;

/// <summary>Portable triangle-list index buffer owned by an occluder.</summary>
public sealed record SmoOcclusionIndexBuffer(
    uint PrimitiveType,
    uint PrimitiveCount,
    uint IndexFormat,
    IReadOnlyList<ushort> TriangleIndices);

/// <summary>Position-only portable vertex buffer owned by an occluder.</summary>
public sealed record SmoOcclusionVertexBuffer(
    uint VertexDeclaration,
    uint VertexCount,
    uint Flags,
    IReadOnlyList<Vector3> Positions);

/// <summary>
/// Strict inspection projection of two sections of <c>spOcclusionVolume</c>.
/// This is not a loaded occluder or a completed engine Init operation.
/// </summary>
public sealed record SmoOcclusionVolumeData(
    SmoNodeData Node,
    SmoOcclusionIndexBuffer IndexBuffer,
    SmoOcclusionVertexBuffer VertexBuffer);

/// <summary>
/// Read-only host inspection profile for inherited node placement and portable
/// buffers read by the shared spIndexBuffer/spVertexBuffer implementations.
/// Field ordering, the position-only UInt16 triangle profile and geometry
/// acceptance below remain host restrictions, not reconstructed engine Init.
/// Geometry classification is a separate engine/corpus operation.
/// </summary>
public static class SmoOcclusionVolumeDecoder
{
    public const uint TriangleListPrimitiveType = 2;
    public const uint UInt16IndexFormat = 0;
    public const uint PositionOnlyVertexDeclaration = 0;
    public const int BufferHeaderSize = 3 * sizeof(uint);

    public static bool TryDecode(
        SmoDocument document,
        SmoObjectEntry entry,
        [NotNullWhen(true)] out SmoOcclusionVolumeData? value,
        out string error)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(entry);
        value = null;
        error = string.Empty;
        if (entry.TypeHash != SmoClassIds.OcclusionVolume)
        {
            error = $"Object [{entry.Index}] is not spOcclusionVolume.";
            return false;
        }
        if (!SmoObjectFieldReader.TryRead(document,entry,out var fields,out error) ||
            !SmoNodeDecoder.TryDecodeNodeSection(
                document,entry,out SmoNodeData? node) || node is null)
        {
            return false;
        }

        int[] terminators = fields.Select((field,index) => (field,index))
            .Where(item => item.field.FieldType == 0 &&
                           item.field.PayloadSize == 0)
            .Select(item => item.index).ToArray();
        if (terminators.Length != 2 || terminators[1] != fields.Count - 1 ||
            terminators[0] < 0 || terminators[1] != terminators[0] + 3 ||
            fields[terminators[0] + 1].FieldType != 0 ||
            fields[terminators[0] + 2].FieldType != 1)
        {
            error = "spOcclusionVolume must contain an inherited spNode section " +
                    "followed by one IndexBuffer and one VertexBuffer.";
            return false;
        }
        for (int index = 0;index < terminators[0];index++)
        {
            SmoObjectField field = fields[index];
            if (field.FieldType is < 0 or > 8 ||
                !SmoSerializedFieldRegistry.TryDescribeField(
                    entry.TypeHash,fields,index,out var descriptor) ||
                descriptor?.Key.StartsWith("node.",StringComparison.Ordinal) != true)
            {
                error = "spOcclusionVolume inherited section contains an " +
                        "unsupported spNode field.";
                return false;
            }
        }

        if (!TryReadIndexBuffer(fields[terminators[0] + 1].Payload.Span,
                out SmoOcclusionIndexBuffer? indexBuffer) || indexBuffer is null)
        {
            error = "spOcclusionVolume IndexBuffer must be a non-empty " +
                    "triangle list with UInt16 indices.";
            return false;
        }
        if (!TryReadVertexBuffer(fields[terminators[0] + 2].Payload.Span,
                out SmoOcclusionVertexBuffer? vertexBuffer) || vertexBuffer is null)
        {
            error = "spOcclusionVolume VertexBuffer must contain at least three " +
                    "finite position-only Vector3 vertices.";
            return false;
        }
        if (!ValidateGeometry(indexBuffer,vertexBuffer))
        {
            error = "spOcclusionVolume triangle indices must be in range, use " +
                    "every vertex and form non-degenerate triangles.";
            return false;
        }

        value = new SmoOcclusionVolumeData(node,indexBuffer,vertexBuffer);
        return true;
    }

    private static unsafe bool TryReadIndexBuffer(
        ReadOnlySpan<byte> payload,
        [NotNullWhen(true)] out SmoOcclusionIndexBuffer? value)
    {
        value = null;
        IndexBufferHandle handle;
        fixed (byte* bytes = payload)
            handle = new(NativeMethods.spv_index_buffer_read(bytes, checked((uint)payload.Length)));
        using (handle)
        {
            if (handle.IsInvalid || NativeMethods.spv_index_buffer_info(handle, out var info) == 0 ||
                info.PrimitiveType != TriangleListPrimitiveType || info.PrimitiveCount == 0 ||
                info.FormatFlags != UInt16IndexFormat)
                return false;
            var nativeIndices = new uint[checked((int)info.IndexCount)];
            fixed (uint* output = nativeIndices)
                if (NativeMethods.spv_index_buffer_indices(handle, output, info.IndexCount) == 0)
                    return false;
            // The shared reader has already expanded the topology and decoded
            // UInt16 values; this is only the existing managed DTO projection.
            var indices = new ushort[nativeIndices.Length];
            for (int index = 0; index < indices.Length; index++)
                indices[index] = checked((ushort)nativeIndices[index]);
            value = new SmoOcclusionIndexBuffer(
                info.PrimitiveType, info.PrimitiveCount, info.FormatFlags, indices);
            return true;
        }
    }

    private static unsafe bool TryReadVertexBuffer(
        ReadOnlySpan<byte> payload,
        [NotNullWhen(true)] out SmoOcclusionVertexBuffer? value)
    {
        value = null;
        VertexBufferHandle handle;
        fixed (byte* bytes = payload)
            handle = new(NativeMethods.spv_vertex_buffer_read(bytes, checked((uint)payload.Length)));
        using (handle)
        {
            if (handle.IsInvalid || NativeMethods.spv_vertex_buffer_info(handle, out var info) == 0 ||
                info.ComponentFlags != PositionOnlyVertexDeclaration || info.VertexCount < 3 || info.Flags != 0)
                return false;
            var positions = new Vector3[checked((int)info.VertexCount)];
            fixed (Vector3* output = positions)
                if (NativeMethods.spv_vertex_buffer_positions(handle, output, checked(info.VertexCount * 3)) == 0)
                    return false;
            if (!positions.All(IsFinite))
                return false;
            value = new SmoOcclusionVertexBuffer(
                info.ComponentFlags, info.VertexCount, info.Flags, positions);
            return true;
        }
    }

    private static bool ValidateGeometry(
        SmoOcclusionIndexBuffer indices,SmoOcclusionVertexBuffer vertices)
    {
        var used = new bool[vertices.Positions.Count];
        for (int offset = 0;offset < indices.TriangleIndices.Count;offset += 3)
        {
            int a = indices.TriangleIndices[offset];
            int b = indices.TriangleIndices[offset + 1];
            int c = indices.TriangleIndices[offset + 2];
            if (a >= vertices.Positions.Count || b >= vertices.Positions.Count ||
                c >= vertices.Positions.Count || a == b || b == c || a == c ||
                Vector3.Cross(vertices.Positions[b] - vertices.Positions[a],
                    vertices.Positions[c] - vertices.Positions[a]).LengthSquared() <=
                1e-12f)
            {
                return false;
            }
            used[a] = used[b] = used[c] = true;
        }
        return used.All(item => item);
    }

    private static bool IsFinite(Vector3 value) =>
        float.IsFinite(value.X) && float.IsFinite(value.Y) &&
        float.IsFinite(value.Z);
}
