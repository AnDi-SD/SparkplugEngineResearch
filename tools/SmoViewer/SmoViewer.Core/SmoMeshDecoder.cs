using System.Buffers.Binary;
using System.Diagnostics.CodeAnalysis;
using System.Numerics;

namespace SmoViewer.Core;

/// <summary>
/// Strict decoder for the confirmed E0 and E1 <c>spMeshData</c> layouts.
/// </summary>
public static class SmoMeshDecoder
{
    public const byte E0Marker = 0xE0;
    public const byte E1Marker = 0xE1;
    public const uint TriangleListPrimitive = 2;
    public const uint TriangleStripPrimitive = 3;

    private const int ObjectSignatureSize = 8;
    private const int PositionSize = 3 * sizeof(float);
    private const uint E0Padding = 0xCDCDCDCD;

    private static ReadOnlySpan<byte> SbooSignature => "SBOO"u8;

    /// <summary>
    /// Decodes a mesh or throws <see cref="SmoFormatException"/> with the same
    /// diagnostic returned by <see cref="TryDecode"/>.
    /// </summary>
    public static SmoMesh Decode(SmoDocument document, SmoObjectEntry entry)
    {
        if (TryDecode(document, entry, out SmoMesh? mesh, out string error))
            return mesh;

        throw new SmoFormatException(error);
    }

    /// <summary>
    /// Decodes an object-directory entry without scanning for signatures or
    /// vertex positions. A stale/patched directory entry is rejected rather
    /// than silently redirected to nearby bytes.
    /// </summary>
    public static bool TryDecode(
        SmoDocument document,
        SmoObjectEntry entry,
        [NotNullWhen(true)] out SmoMesh? mesh,
        out string error)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(entry);

        mesh = null;
        error = string.Empty;
        string context = FormatContext(entry);

        if (entry.TypeHash != SmoClassIds.MeshData)
            return Fail(
                out mesh,
                out error,
                $"{context} is {SmoClassRegistry.GetDisplayName(entry.TypeHash)}, not spMeshData.");

        if (!entry.IsWithinDataSection)
            return Fail(
                out mesh,
                out error,
                $"{context} lies outside the declared SMO data section.");

        if (!entry.SignatureMatches)
            return Fail(
                out mesh,
                out error,
                $"{context} does not point at its declared classId/SBOO signature; " +
                "the directory offset may be stale after patching.");

        ReadOnlySpan<byte> data = document.Data.Span;
        if (entry.PhysicalOffset < 0 ||
            entry.PhysicalEnd < entry.PhysicalOffset ||
            entry.PhysicalEnd > data.Length ||
            entry.SerializedSize > int.MaxValue ||
            entry.PhysicalOffset > int.MaxValue)
        {
            return Fail(
                out mesh,
                out error,
                $"{context} has an invalid physical range " +
                $"[0x{entry.PhysicalOffset:X}, 0x{entry.PhysicalEnd:X}).");
        }

        int objectOffset = (int)entry.PhysicalOffset;
        int objectSize = (int)entry.SerializedSize;
        if (objectSize < ObjectSignatureSize + 6)
            return Fail(out mesh, out error, $"{context} is too small to contain a serialized object.");

        ReadOnlySpan<byte> serialized = data.Slice(objectOffset, objectSize);
        if (ReadUInt32(serialized, 0) != SmoClassIds.MeshData ||
            !serialized.Slice(sizeof(uint), SbooSignature.Length).SequenceEqual(SbooSignature))
        {
            return Fail(
                out mesh,
                out error,
                $"{context} failed the classId/SBOO signature check at its exact physical offset.");
        }

        if (!SmoDataBlockReader.TryReadHeader(
                serialized,
                ObjectSignatureSize,
                out SmoDataBlockHeader outer))
        {
            return Fail(out mesh, out error, $"{context} has a truncated outer data-block header.");
        }

        byte marker = outer.RawHeader;
        if (marker is not E0Marker and not E1Marker)
            return Fail(
                out mesh,
                out error,
                $"{context} uses unsupported mesh marker 0x{marker:X2}; expected E0 or E1.");

        if (outer.SizeKind != SmoDataBlockSizeCode.UInt32 || outer.HeaderSize != 5)
            return Fail(
                out mesh,
                out error,
                $"{context} marker 0x{marker:X2} does not use the confirmed UInt32 size form.");

        long expectedObjectSize = (long)outer.PayloadEnd + 1;
        if (expectedObjectSize != serialized.Length)
        {
            return Fail(
                out mesh,
                out error,
                $"{context} outer payload ends at relative 0x{outer.PayloadEnd:X}, " +
                $"but SerializedSize ends at 0x{serialized.Length:X}; " +
                "the directory size or physical offset is inconsistent.");
        }

        if (serialized[^1] != 0)
            return Fail(
                out mesh,
                out error,
                $"{context} is missing the zero byte at its declared physical end.");

        if (outer.PayloadSize > int.MaxValue)
            return Fail(out mesh, out error, $"{context} outer payload is too large.");

        ReadOnlySpan<byte> payload = serialized.Slice(
            outer.PayloadOffset,
            (int)outer.PayloadSize);

        return marker == E1Marker
            ? TryDecodeE1(
                entry, payload, objectOffset + outer.PayloadOffset,
                out mesh, out error)
            : TryDecodeE0(
                entry, payload, objectOffset + outer.PayloadOffset,
                out mesh, out error);
    }

    private static bool TryDecodeE1(
        SmoObjectEntry entry,
        ReadOnlySpan<byte> payload,
        long payloadOffset,
        [NotNullWhen(true)] out SmoMesh? mesh,
        out string error)
    {
        mesh = null;
        error = string.Empty;
        string context = FormatContext(entry);

        // E1 payload:
        // format, vertexCount, expandedRuntimeBytes, serializedIndexBytes,
        // zero field marker; primitive, storedIndexCountMinusTwo, zero;
        // complete index buffer; repeated format/count/zero; disk vertices.
        const int preambleSize = 17;
        const int primitiveHeaderSize = 12;
        const int vertexHeaderSize = 12;
        const int indexOffset = preambleSize + primitiveHeaderSize;
        const int fixedPayloadSize =
            preambleSize + primitiveHeaderSize + vertexHeaderSize;

        if (payload.Length < fixedPayloadSize)
            return Fail(out mesh, out error, $"{context} E1 payload is truncated.");

        uint vertexFormat = ReadUInt32(payload, 0);
        uint vertexCountValue = ReadUInt32(payload, 4);
        uint runtimeVertexBytes = ReadUInt32(payload, 8);
        uint indexBytesValue = ReadUInt32(payload, 12);
        byte fieldSeparator = payload[16];

        uint primitiveType = ReadUInt32(payload, preambleSize);
        uint storedIndexCount = ReadUInt32(payload, preambleSize + 4);
        uint primitiveReserved = ReadUInt32(payload, preambleSize + 8);

        if (fieldSeparator != 0)
            return Fail(
                out mesh,
                out error,
                $"{context} E1 preamble separator is 0x{fieldSeparator:X2}, expected 00.");

        if (primitiveReserved != 0)
            return Fail(
                out mesh,
                out error,
                $"{context} E1 primitive reserved value is 0x{primitiveReserved:X8}, expected zero.");

        if (primitiveType is not TriangleListPrimitive and not TriangleStripPrimitive)
            return Fail(
                out mesh,
                out error,
                $"{context} uses unsupported primitive type {primitiveType}; " +
                $"expected triangle-list {TriangleListPrimitive} or " +
                $"triangle-strip {TriangleStripPrimitive}.");

        if (storedIndexCount > (int.MaxValue - 4) / sizeof(ushort))
            return Fail(out mesh, out error, $"{context} E1 stored index count is too large.");

        int expectedIndexBytes = primitiveType == TriangleListPrimitive
            ? checked((int)storedIndexCount * 3 * sizeof(ushort))
            : checked((int)storedIndexCount * sizeof(ushort) + 2 * sizeof(ushort));
        if (indexBytesValue != (uint)expectedIndexBytes)
        {
            string expectedDescription = primitiveType == TriangleListPrimitive
                ? $"triangle count {storedIndexCount} * 3 * 2"
                : $"(stored index count {storedIndexCount} + 2) * 2";
            return Fail(
                out mesh,
                out error,
                $"{context} E1 index byte size {indexBytesValue} does not equal " +
                $"{expectedDescription}.");
        }

        int vertexHeaderOffset = checked(indexOffset + expectedIndexBytes);
        if (!CanRead(payload, vertexHeaderOffset, vertexHeaderSize))
            return Fail(
                out mesh,
                out error,
                $"{context} E1 index buffer crosses the exact serialized boundary.");

        uint repeatedVertexFormat = ReadUInt32(payload, vertexHeaderOffset);
        uint repeatedVertexCount = ReadUInt32(payload, vertexHeaderOffset + 4);
        uint vertexReserved = ReadUInt32(payload, vertexHeaderOffset + 8);
        if (repeatedVertexFormat != vertexFormat)
            return Fail(
                out mesh,
                out error,
                $"{context} E1 vertex formats disagree " +
                $"(0x{vertexFormat:X} vs 0x{repeatedVertexFormat:X}).");

        if (repeatedVertexCount != vertexCountValue)
            return Fail(
                out mesh,
                out error,
                $"{context} E1 vertex counts disagree " +
                $"({vertexCountValue} vs {repeatedVertexCount}).");

        if (vertexReserved != 0)
            return Fail(
                out mesh,
                out error,
                $"{context} E1 vertex reserved value is 0x{vertexReserved:X8}, expected zero.");

        int vertexOffset = checked(vertexHeaderOffset + vertexHeaderSize);
        int serializedVertexBytes = payload.Length - vertexOffset;
        if (!TryGetStrides(
                context,
                vertexCountValue,
                serializedVertexBytes,
                runtimeVertexBytes,
                out int vertexCount,
                out int stride,
                out int runtimeStride,
                out error))
        {
            return false;
        }

        return TryCreateMesh(
            entry,
            E1Marker,
            primitiveType,
            vertexFormat,
            stride,
            runtimeStride,
            indexTrailerSize: 0,
            runtimeVertexBytes,
            payload,
            payloadOffset,
            indexOffset,
            expectedIndexBytes,
            vertexOffset,
            vertexCount,
            out mesh,
            out error);
    }

    private static bool TryDecodeE0(
        SmoObjectEntry entry,
        ReadOnlySpan<byte> payload,
        long payloadOffset,
        [NotNullWhen(true)] out SmoMesh? mesh,
        out string error)
    {
        mesh = null;
        error = string.Empty;
        string context = FormatContext(entry);

        // E0 has no expanded-buffer preamble. The count can either be complete
        // or omit two indices stored in the four bytes at q.
        const int primitiveHeaderSize = 12;
        const int vertexHeaderSize = 12;
        if (payload.Length < primitiveHeaderSize + vertexHeaderSize)
            return Fail(out mesh, out error, $"{context} E0 payload is truncated.");

        uint primitiveType = ReadUInt32(payload, 0);
        uint indexCountValue = ReadUInt32(payload, 4);
        uint primitiveReserved = ReadUInt32(payload, 8);

        if (primitiveReserved != 0)
            return Fail(
                out mesh,
                out error,
                $"{context} E0 primitive reserved value is 0x{primitiveReserved:X8}, expected zero.");

        if (primitiveType is not TriangleListPrimitive and not TriangleStripPrimitive)
            return Fail(
                out mesh,
                out error,
                $"{context} uses unsupported primitive type {primitiveType}; " +
                $"expected triangle-list {TriangleListPrimitive} or " +
                $"triangle-strip {TriangleStripPrimitive}.");

        if (indexCountValue > (int.MaxValue - primitiveHeaderSize) / sizeof(ushort))
            return Fail(out mesh, out error, $"{context} E0 index count is too large.");

        int indexBytes = primitiveType == TriangleListPrimitive
            ? checked((int)indexCountValue * 3 * sizeof(ushort))
            : checked((int)indexCountValue * sizeof(ushort));
        int q = checked(primitiveHeaderSize + indexBytes);
        if (q > payload.Length)
            return Fail(
                out mesh,
                out error,
                $"{context} E0 index buffer crosses the exact serialized boundary.");

        bool directValid = TryReadE0VertexCandidate(
            payload,
            q,
            headerShift: 0,
            out VertexCandidate direct);

        VertexCandidate padded = default;
        bool shiftedValid =
            CanRead(payload, q, sizeof(uint)) &&
            TryReadE0VertexCandidate(
                payload,
                q + sizeof(uint),
                headerShift: sizeof(uint),
                out padded);

        if (directValid == shiftedValid)
        {
            string detail = directValid
                ? "both q and q+4 vertex headers are structurally valid"
                : "neither q nor q+4 contains a structurally valid vertex header";
            return Fail(
                out mesh,
                out error,
                $"{context} E0 layout is ambiguous or malformed: {detail}.");
        }

        VertexCandidate candidate = directValid ? direct : padded;
        bool hasShiftedWord = candidate.HeaderShift == sizeof(uint);
        bool shiftedWordIsPadding =
            hasShiftedWord && ReadUInt32(payload, q) == E0Padding;
        int completeIndexBytes = checked(
            indexBytes + (hasShiftedWord && !shiftedWordIsPadding ? sizeof(uint) : 0));
        int indexTrailerSize = shiftedWordIsPadding ? sizeof(uint) : 0;
        uint runtimeVertexBytes = checked((uint)candidate.SerializedVertexBytes);

        return TryCreateMesh(
            entry,
            E0Marker,
            primitiveType,
            candidate.VertexFormat,
            candidate.Stride,
            candidate.Stride,
            indexTrailerSize,
            runtimeVertexBytes,
            payload,
            payloadOffset,
            primitiveHeaderSize,
            completeIndexBytes,
            candidate.VertexOffset,
            candidate.VertexCount,
            out mesh,
            out error);
    }

    private static bool TryReadE0VertexCandidate(
        ReadOnlySpan<byte> payload,
        int headerOffset,
        int headerShift,
        out VertexCandidate candidate)
    {
        candidate = default;
        const int vertexHeaderSize = 12;
        if (!CanRead(payload, headerOffset, vertexHeaderSize))
            return false;

        uint vertexFormat = ReadUInt32(payload, headerOffset);
        uint vertexCountValue = ReadUInt32(payload, headerOffset + 4);
        uint reserved = ReadUInt32(payload, headerOffset + 8);
        if (reserved != 0 || vertexCountValue > int.MaxValue)
            return false;

        int vertexOffset = headerOffset + vertexHeaderSize;
        int vertexBytes = payload.Length - vertexOffset;
        int vertexCount = (int)vertexCountValue;
        int stride;
        if (vertexCount == 0)
        {
            if (vertexBytes != 0)
                return false;
            stride = 0;
        }
        else
        {
            if (vertexBytes % vertexCount != 0)
                return false;
            stride = vertexBytes / vertexCount;
            if (stride < PositionSize)
                return false;
        }

        candidate = new VertexCandidate(
            vertexFormat,
            vertexCount,
            stride,
            headerShift,
            vertexOffset,
            vertexBytes);
        return true;
    }

    private static bool TryGetStrides(
        string context,
        uint vertexCountValue,
        int serializedVertexBytes,
        uint runtimeVertexBytes,
        out int vertexCount,
        out int stride,
        out int runtimeStride,
        out string error)
    {
        vertexCount = 0;
        stride = 0;
        runtimeStride = 0;
        error = string.Empty;

        if (vertexCountValue > int.MaxValue)
        {
            error = $"{context} vertex count is too large.";
            return false;
        }

        vertexCount = (int)vertexCountValue;
        if (vertexCount == 0)
        {
            if (serializedVertexBytes != 0 || runtimeVertexBytes != 0)
            {
                error = $"{context} has zero vertices but a non-empty vertex buffer.";
                return false;
            }

            return true;
        }

        if (serializedVertexBytes % vertexCount != 0)
        {
            error =
                $"{context} physical vertex bytes {serializedVertexBytes} are not divisible " +
                $"by vertex count {vertexCount}.";
            return false;
        }

        if (runtimeVertexBytes % vertexCountValue != 0)
        {
            error =
                $"{context} runtime vertex bytes {runtimeVertexBytes} are not divisible " +
                $"by vertex count {vertexCount}.";
            return false;
        }

        stride = serializedVertexBytes / vertexCount;
        uint runtimeStrideValue = runtimeVertexBytes / vertexCountValue;
        if (runtimeStrideValue > int.MaxValue)
        {
            error = $"{context} runtime stride {runtimeStrideValue} is too large.";
            return false;
        }

        runtimeStride = (int)runtimeStrideValue;
        if (stride < PositionSize)
        {
            error =
                $"{context} serialized stride {stride} is smaller than an XYZ position.";
            return false;
        }

        if (runtimeStride < PositionSize)
        {
            error =
                $"{context} runtime stride {runtimeStride} is smaller than an XYZ position.";
            return false;
        }

        return true;
    }

    private static bool TryCreateMesh(
        SmoObjectEntry entry,
        byte marker,
        uint primitiveType,
        uint vertexFormat,
        int stride,
        int runtimeStride,
        int indexTrailerSize,
        uint runtimeVertexBytes,
        ReadOnlySpan<byte> payload,
        long payloadOffset,
        int indexOffset,
        int indexBytes,
        int vertexOffset,
        int vertexCount,
        [NotNullWhen(true)] out SmoMesh? mesh,
        out string error)
    {
        mesh = null;
        error = string.Empty;
        string context = FormatContext(entry);

        if ((indexBytes & 1) != 0 || !CanRead(payload, indexOffset, indexBytes))
            return Fail(out mesh, out error, $"{context} has an invalid 16-bit index buffer.");

        int indexCount = indexBytes / sizeof(ushort);
        var stripIndices = new ushort[indexCount];
        for (int index = 0; index < indexCount; index++)
        {
            ushort value = BinaryPrimitives.ReadUInt16LittleEndian(
                payload.Slice(indexOffset + index * sizeof(ushort), sizeof(ushort)));
            if (value >= vertexCount)
            {
                return Fail(
                    out mesh,
                    out error,
                    $"{context} strip index {index} has value {value}, " +
                    $"outside vertex range 0..{Math.Max(vertexCount - 1, 0)}.");
            }

            stripIndices[index] = value;
        }

        if (vertexCount > 0 &&
            (!CanRead(payload, vertexOffset, checked(vertexCount * stride)) ||
             vertexOffset + vertexCount * stride != payload.Length))
        {
            return Fail(
                out mesh,
                out error,
                $"{context} vertex buffer does not end at the exact outer payload boundary.");
        }

        var positions = new Vector3[vertexCount];
        for (int vertex = 0; vertex < vertexCount; vertex++)
        {
            int offset = checked(vertexOffset + vertex * stride);
            float x = ReadSingle(payload, offset);
            float y = ReadSingle(payload, offset + sizeof(float));
            float z = ReadSingle(payload, offset + 2 * sizeof(float));
            if (!float.IsFinite(x) || !float.IsFinite(y) || !float.IsFinite(z))
            {
                return Fail(
                    out mesh,
                    out error,
                    $"{context} vertex {vertex} has a non-finite XYZ position at confirmed offset zero.");
            }

            positions[vertex] = new Vector3(x, y, z);
        }

        Vector2[] textureCoordinates = [];
        Vector2[] textureCoordinates1 = [];
        Vector3[] normals = [];
        uint[] diffuseColorsArgb = [];
        Vector4[] blendWeights = [];
        SmoBlendIndices[] blendIndices = [];
        if (SmoVertexLayoutRegistry.TryGet(vertexFormat, out SmoVertexLayout? layout) &&
            layout is not null &&
            stride == layout.SerializedStride)
        {
            if (layout.DiffuseArgbOffset is int diffuseOffset)
            {
                if (diffuseOffset < 0 || diffuseOffset > stride - sizeof(uint))
                {
                    return Fail(
                        out mesh,
                        out error,
                        $"{context} confirmed diffuse offset {diffuseOffset} " +
                        $"does not fit serialized stride {stride}.");
                }

                diffuseColorsArgb = new uint[vertexCount];
                for (int vertex = 0; vertex < vertexCount; vertex++)
                {
                    int offset = checked(vertexOffset + vertex * stride + diffuseOffset);
                    diffuseColorsArgb[vertex] = ReadUInt32(payload, offset);
                }
            }

            if (layout.TextureCoordinate0Offset is int uvOffset)
            {
                const int uvSize = 2 * sizeof(float);
                if (uvOffset < 0 || uvOffset > stride - uvSize)
                {
                    return Fail(
                        out mesh,
                        out error,
                        $"{context} confirmed UV offset {uvOffset} " +
                        $"does not fit serialized stride {stride}.");
                }

                textureCoordinates = new Vector2[vertexCount];
                for (int vertex = 0; vertex < vertexCount; vertex++)
                {
                    int offset = checked(vertexOffset + vertex * stride + uvOffset);
                    float u = ReadSingle(payload, offset);
                    float v = ReadSingle(payload, offset + sizeof(float));
                    if (!float.IsFinite(u) || !float.IsFinite(v))
                    {
                        return Fail(
                            out mesh,
                            out error,
                            $"{context} vertex {vertex} has non-finite UV0 values.");
                    }

                    textureCoordinates[vertex] = new Vector2(u, v);
                }
            }

            if (layout.TextureCoordinate1Offset is int uv1Offset)
            {
                const int uvSize = 2 * sizeof(float);
                if (uv1Offset < 0 || uv1Offset > stride - uvSize)
                {
                    return Fail(
                        out mesh,
                        out error,
                        $"{context} confirmed UV1 offset {uv1Offset} " +
                        $"does not fit serialized stride {stride}.");
                }

                textureCoordinates1 = new Vector2[vertexCount];
                for (int vertex = 0; vertex < vertexCount; vertex++)
                {
                    int offset = checked(vertexOffset + vertex * stride + uv1Offset);
                    float u = ReadSingle(payload, offset);
                    float v = ReadSingle(payload, offset + sizeof(float));
                    if (!float.IsFinite(u) || !float.IsFinite(v))
                    {
                        return Fail(
                            out mesh,
                            out error,
                            $"{context} vertex {vertex} has non-finite UV1 values.");
                    }
                    textureCoordinates1[vertex] = new Vector2(u, v);
                }
            }

            if (layout.NormalOffset is int normalOffset)
            {
                const int normalSize = 3 * sizeof(float);
                if (normalOffset < 0 || normalOffset > stride - normalSize)
                {
                    return Fail(out mesh, out error,
                        $"{context} confirmed normal offset {normalOffset} " +
                        $"does not fit serialized stride {stride}.");
                }

                normals = new Vector3[vertexCount];
                for (int vertex = 0; vertex < vertexCount; vertex++)
                {
                    int offset = checked(vertexOffset + vertex * stride + normalOffset);
                    Vector3 normal = new(
                        ReadSingle(payload, offset),
                        ReadSingle(payload, offset + 4),
                        ReadSingle(payload, offset + 8));
                    if (!float.IsFinite(normal.X) || !float.IsFinite(normal.Y) ||
                        !float.IsFinite(normal.Z))
                    {
                        return Fail(out mesh, out error,
                            $"{context} vertex {vertex} has an invalid stored normal.");
                    }
                    normals[vertex] = normal.LengthSquared() < 0.000001f
                        ? Vector3.Zero
                        : Vector3.Normalize(normal);
                }
            }

            if (layout.BlendWeightsOffset is int weightsOffset &&
                layout.BlendIndicesOffset is int indicesOffset)
            {
                const int weightsSize = 4 * sizeof(float);
                const int indicesSize = 4;
                if (weightsOffset < 0 || weightsOffset > stride - weightsSize ||
                    indicesOffset < 0 || indicesOffset > stride - indicesSize)
                {
                    return Fail(
                        out mesh,
                        out error,
                        $"{context} confirmed skinning offsets do not fit " +
                        $"serialized stride {stride}.");
                }

                blendWeights = new Vector4[vertexCount];
                blendIndices = new SmoBlendIndices[vertexCount];
                for (int vertex = 0; vertex < vertexCount; vertex++)
                {
                    int weights = checked(vertexOffset + vertex * stride + weightsOffset);
                    Vector4 value = new(
                        ReadSingle(payload, weights),
                        ReadSingle(payload, weights + 4),
                        ReadSingle(payload, weights + 8),
                        ReadSingle(payload, weights + 12));
                    if (!float.IsFinite(value.X) || !float.IsFinite(value.Y) ||
                        !float.IsFinite(value.Z) || !float.IsFinite(value.W))
                    {
                        return Fail(out mesh, out error,
                            $"{context} vertex {vertex} has non-finite blend weights.");
                    }

                    int indices = checked(vertexOffset + vertex * stride + indicesOffset);
                    blendWeights[vertex] = value;
                    blendIndices[vertex] = new SmoBlendIndices(
                        payload[indices], payload[indices + 1],
                        payload[indices + 2], payload[indices + 3]);
                }
            }
        }

        uint[] triangles = primitiveType == TriangleListPrimitive
            ? ConvertTriangleList(stripIndices)
            : ConvertTriangleStrip(stripIndices);
        mesh = new SmoMesh(
            entry.Index,
            entry.Name,
            marker,
            primitiveType,
            vertexFormat,
            stride,
            runtimeStride,
            indexTrailerSize,
            runtimeVertexBytes,
            entry.PhysicalOffset,
            entry.PhysicalEnd,
            payloadOffset + indexOffset,
            payloadOffset + vertexOffset,
            positions,
            normals,
            textureCoordinates,
            textureCoordinates1,
            diffuseColorsArgb,
            blendWeights,
            blendIndices,
            stripIndices,
            triangles);
        return true;
    }

    private static uint[] ConvertTriangleStrip(ReadOnlySpan<ushort> strip)
    {
        if (strip.Length < 3)
            return [];

        var triangles = new List<uint>((strip.Length - 2) * 3);
        for (int window = 0; window + 2 < strip.Length; window++)
        {
            ushort a = strip[window];
            ushort b = strip[window + 1];
            ushort c = strip[window + 2];
            if (a == b || b == c || a == c)
                continue;

            // Strip parity advances for every window, including degenerate
            // connector triangles.
            if ((window & 1) == 0)
            {
                triangles.Add(a);
                triangles.Add(b);
            }
            else
            {
                triangles.Add(b);
                triangles.Add(a);
            }

            triangles.Add(c);
        }

        return triangles.ToArray();
    }

    private static uint[] ConvertTriangleList(ReadOnlySpan<ushort> indices)
    {
        if (indices.Length % 3 != 0)
            throw new SmoFormatException(
                $"Triangle-list index count {indices.Length} is not divisible by three.");

        var triangles = new uint[indices.Length];
        for (int index = 0; index < indices.Length; index++)
            triangles[index] = indices[index];
        return triangles;
    }

    private static string FormatContext(SmoObjectEntry entry) =>
        $"Object [{entry.Index}] \"{entry.Name}\" at 0x{entry.PhysicalOffset:X}";

    private static bool Fail(
        out SmoMesh? mesh,
        out string error,
        string message)
    {
        mesh = null;
        error = message;
        return false;
    }

    private static bool CanRead(ReadOnlySpan<byte> data, int offset, int length) =>
        offset >= 0 && length >= 0 && offset <= data.Length - length;

    private static uint ReadUInt32(ReadOnlySpan<byte> data, int offset) =>
        BinaryPrimitives.ReadUInt32LittleEndian(data.Slice(offset, sizeof(uint)));

    private static float ReadSingle(ReadOnlySpan<byte> data, int offset) =>
        BitConverter.Int32BitsToSingle(
            BinaryPrimitives.ReadInt32LittleEndian(data.Slice(offset, sizeof(float))));

    private readonly record struct VertexCandidate(
        uint VertexFormat,
        int VertexCount,
        int Stride,
        int HeaderShift,
        int VertexOffset,
        int SerializedVertexBytes);
}
