using System.Diagnostics.CodeAnalysis;
using System.Numerics;
using SmoViewer.Sparkplug;

namespace SmoViewer.Core;

/// <summary>DTO/modern-backend adapter over the reconstructed mesh and buffer readers.</summary>
public static class SmoMeshDecoder
{
    public const byte E0Marker = 0xE0;
    public const byte E1Marker = 0xE1;
    public const uint TriangleListPrimitive = 2;
    public const uint TriangleStripPrimitive = 3;

    public static SmoMesh Decode(SmoDocument document, SmoObjectEntry entry)
    {
        if (TryDecode(document, entry, out var mesh, out var error)) return mesh;
        throw new SmoFormatException(error);
    }

    public static bool TryDecode(SmoDocument document, SmoObjectEntry entry,
        [NotNullWhen(true)] out SmoMesh? mesh, out string error)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(entry);
        mesh = null; error = string.Empty;
        if (entry.TypeHash != SmoClassIds.MeshData || !entry.IsWithinDataSection ||
            !entry.SignatureMatches || entry.PhysicalOffset < 0 || entry.SerializedSize < 8 ||
            entry.PhysicalEnd > document.Data.Length)
        {
            error = $"Object [{entry.Index}] is not a bounded spMeshData resource.";
            return false;
        }
        // The original selector chooses field1 for PC and field0 for the
        // portable reader. No trial parsing or fallback to a different format.
        return TryReadNative(entry, document.Data.Span.Slice(
            checked((int)entry.PhysicalOffset + 8), checked((int)entry.SerializedSize - 8)),
            entry.PhysicalOffset + 8, 0, document.Header.PlatformMask, out mesh, out error);
    }

    internal static bool TryDecodeRepresentation(SmoObjectEntry entry, SmoObjectField field,
        [NotNullWhen(true)] out SmoMesh? mesh, out string error)
    {
        ArgumentNullException.ThrowIfNull(entry);
        ArgumentNullException.ThrowIfNull(field);
        if (field.FieldType is not (0 or 1) || field.PayloadSize == 0)
        {
            mesh = null; error = "Field is not a mesh representation."; return false;
        }
        // Explicit inspector request for an independently serialized field.
        return TryReadNative(entry, field.Payload.Span, field.AbsolutePayloadOffset,
            field.FieldType == 0 ? 1u : 2u, 0, out mesh, out error);
    }

    internal static unsafe bool TryReadMetadata(SmoObjectField field,
        out NativeMethods.MeshInfo info, out string error)
    {
        info = default; error = string.Empty;
        try
        {
            fixed (byte* input = field.Payload.Span)
            {
                using var owner = new MeshHandle(NativeMethods.Check(NativeMethods.spv_mesh_read(
                    input, checked((uint)field.Payload.Length), field.FieldType == 0 ? 3u : 4u, 0)));
                NativeMethods.Check(NativeMethods.spv_mesh_info(owner, out info));
                return true;
            }
        }
        catch (InvalidDataException exception) { error = exception.Message; return false; }
    }

    private static unsafe bool TryReadNative(SmoObjectEntry entry, ReadOnlySpan<byte> payload,
        long inputOffset, uint kind, uint platformMask,
        [NotNullWhen(true)] out SmoMesh? mesh, out string error)
    {
        mesh = null; error = string.Empty;
        try
        {
            MeshHandle owner;
            fixed (byte* input = payload)
                owner = new MeshHandle(NativeMethods.Check(NativeMethods.spv_mesh_read(
                    input, checked((uint)payload.Length), kind, platformMask)));
            using (owner)
            {
                NativeMethods.Check(NativeMethods.spv_mesh_info(owner, out var info));
                // Existing editing DTOs hold UInt16 source indices. The native
                // reader preserves UInt32 streams; this consumer reports its limit.
                if (info.IndexElementSize != sizeof(ushort))
                    throw new InvalidDataException("Managed mesh editing DTO requires UInt16 source indices.");
                if (info.PrimitiveType is not (TriangleListPrimitive or TriangleStripPrimitive))
                    throw new InvalidDataException("Modern triangle backend does not display this primitive type.");
                var sourceVertices = new NativeMethods.MeshVertex[checked((int)info.Vertices)];
                var sourceIndices = new uint[checked((int)info.Indices)];
                fixed (NativeMethods.MeshVertex* output = sourceVertices)
                    NativeMethods.Check(NativeMethods.spv_mesh_vertices(owner, output, info.Vertices));
                fixed (uint* output = sourceIndices)
                    NativeMethods.Check(NativeMethods.spv_mesh_indices(owner, output, info.Indices));
                bool Has(uint bit) => (info.Attributes & bit) != 0;
                var positions = sourceVertices.Select(vertex => vertex.Position).ToArray();
                var normals = Has(1) ? sourceVertices.Select(vertex => vertex.Normal).ToArray() : [];
                var colors = Has(2) ? sourceVertices.Select(vertex => vertex.Color).ToArray() : [];
                var uv0 = Has(4) ? sourceVertices.Select(vertex => vertex.Uv0).ToArray() : [];
                var uv1 = Has(8) ? sourceVertices.Select(vertex => vertex.Uv1).ToArray() : [];
                var weights = Has(16) ? sourceVertices.Select(vertex => vertex.Weights).ToArray() : [];
                var bones = Has(32) ? sourceVertices.Select(vertex => new SmoBlendIndices(
                    (byte)vertex.Bones, (byte)(vertex.Bones >> 8),
                    (byte)(vertex.Bones >> 16), (byte)(vertex.Bones >> 24))).ToArray() : [];
                var indices = sourceIndices.Select(index => checked((ushort)index)).ToArray();
                // One shared host projection validates only emitted triangles.
                // Raw original strip indices, including unused tails, stay above.
                NativeMethods.Check(NativeMethods.spv_mesh_triangles(owner, null, 0, out uint triangleCount));
                var triangles = new uint[checked((int)triangleCount)];
                fixed (uint* output = triangles)
                    NativeMethods.Check(NativeMethods.spv_mesh_triangles(owner, output, triangleCount, out _));
                long fieldOffset = checked(inputOffset + info.FieldPayloadOffset);
                mesh = new SmoMesh(entry.Index, entry.Name,
                    info.FieldId == 0 ? E0Marker : E1Marker, info.PrimitiveType,
                    info.ComponentFlags, checked((int)info.SerializedStride), checked((int)info.RuntimeStride),
                    0, info.RuntimeVbSize, entry.PhysicalOffset, entry.PhysicalEnd,
                    checked(fieldOffset + info.IndexPayloadOffset), checked(fieldOffset + info.VertexPayloadOffset),
                    positions, normals, uv0, uv1, colors, weights, bones, indices, triangles)
                { PrimitiveCount = info.Primitives };
                return true;
            }
        }
        catch (InvalidDataException exception) { error = exception.Message; return false; }
    }

}
