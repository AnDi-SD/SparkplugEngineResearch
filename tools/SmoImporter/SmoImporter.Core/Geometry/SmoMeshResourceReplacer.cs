using System.Buffers.Binary;
using System.Numerics;
using SmoViewer.Core;

namespace SmoImporter.Core;

/// <summary>
/// Replaces one physical spMeshData resource while preserving every model,
/// material and placement that references it. This is the shared low-level
/// writer used by both the importer and level-editor workflows.
/// </summary>
public static class SmoMeshResourceReplacer
{
    /// <summary>
    /// Returns the model transform that lets a renderer preview a fitting
    /// adjustment on an identity-prepared transient mesh. The renderer can
    /// update only this matrix while dragging; no SMO repack or vertex rebuild
    /// is required.
    /// </summary>
    public static Matrix4x4 CreatePreviewModelTransform(
        Matrix4x4 referenceWorldTransform,
        ReplacementTransform transform)
    {
        ArgumentNullException.ThrowIfNull(transform);
        Matrix4x4 reflection = Matrix4x4.CreateScale(1, 1, -1);
        return referenceWorldTransform * reflection * transform.Matrix * reflection;
    }

    public static SmoMesh CreatePreviewMesh(
        SmoDocument document,
        int meshObjectIndex,
        ImportedScene replacement,
        ReplacementTransform transform,
        int? rigidBoneSlot = null,
        Matrix4x4? referenceWorldTransform = null,
        int? transientObjectIndex = null)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(replacement);
        ArgumentNullException.ThrowIfNull(transform);
        if ((uint)meshObjectIndex >= (uint)document.Objects.Count)
            throw new ArgumentOutOfRangeException(nameof(meshObjectIndex));
        SmoObjectEntry target = document.Objects[meshObjectIndex];
        if (target.TypeHash != SmoClassIds.MeshData)
            throw new InvalidOperationException(
                $"Object [{meshObjectIndex}] is not spMeshData.");

        ImportedMesh combined = ImportedMeshCombiner.Combine(replacement);
        SmoMesh template = SmoMeshDecoder.Decode(document, target);
        SmoVertexLayout layout = ValidateWritableLayout(target, template);
        int boneSlot = ResolveBoneSlot(
            document, target, layout, rigidBoneSlot);
        SmoPreparedReplacementGeometry prepared = PrepareGeometry(
            document,
            target,
            layout,
            combined,
            transform,
            boneSlot,
            referenceWorldTransform);
        return SmoMesh.CreateTransient(
            template,
            prepared.Positions,
            prepared.Normals,
            prepared.TextureCoordinates,
            prepared.SecondaryTextureCoordinates,
            prepared.DiffuseColorsArgb,
            prepared.BlendWeights,
            prepared.BlendIndices,
            prepared.TriangleIndices,
            transientObjectIndex);
    }

    public static SmoMeshResourceReplacement Replace(
        SmoDocument document,
        int meshObjectIndex,
        ImportedScene replacement,
        ReplacementTransform transform,
        int? rigidBoneSlot = null,
        Matrix4x4? referenceWorldTransform = null)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(replacement);
        ArgumentNullException.ThrowIfNull(transform);
        if ((uint)meshObjectIndex >= (uint)document.Objects.Count)
            throw new ArgumentOutOfRangeException(nameof(meshObjectIndex));

        SmoObjectEntry target = document.Objects[meshObjectIndex];
        if (target.TypeHash != SmoClassIds.MeshData)
            throw new InvalidOperationException(
                $"Object [{meshObjectIndex}] is not spMeshData.");

        ImportedMesh combined = ImportedMeshCombiner.Combine(replacement);
        SmoMesh template = SmoMeshDecoder.Decode(document, target);
        SmoVertexLayout layout = ValidateWritableLayout(target, template);
        int boneSlot = ResolveBoneSlot(
            document, target, layout, rigidBoneSlot);
        byte[] payload = BuildMeshObject(
            document,
            target,
            template,
            layout,
            combined,
            transform,
            boneSlot,
            referenceWorldTransform);
        byte[] output = Repack(
            document,
            [new SmoMeshObjectReplacement(target, payload)]);

        SmoDocument verified = SmoDocument.ParseOwned(output, document.SourcePath);
        SmoMesh verifiedMesh = SmoMeshDecoder.Decode(
            verified, verified.Objects[meshObjectIndex]);
        int triangleCount = combined.TriangleIndices.Length / 3;
        if (verified.HasErrors || verified.Objects.Count != document.Objects.Count ||
            verifiedMesh.VertexCount != combined.Positions.Length ||
            verifiedMesh.TriangleCount != triangleCount)
        {
            throw new InvalidDataException(
                "The replaced mesh resource failed strict post-write verification.");
        }

        return new SmoMeshResourceReplacement(
            output,
            meshObjectIndex,
            combined.Positions.Length,
            triangleCount,
            boneSlot);
    }

    internal static SmoVertexLayout ValidateWritableLayout(
        SmoObjectEntry target,
        SmoMesh template)
    {
        if (template.Marker is not (
                SmoMeshDecoder.E0Marker or SmoMeshDecoder.E1Marker) ||
            !SmoVertexLayoutRegistry.TryGet(
                template.VertexFormat,
                out SmoVertexLayout? layout) ||
            layout is null || layout.SerializedStride != template.Stride)
        {
            throw new InvalidOperationException(
                $"Mesh [{target.Index}] is not a confirmed writable E0/E1 layout.");
        }

        return layout;
    }

    internal static int ResolveBoneSlot(
        SmoDocument document,
        SmoObjectEntry target,
        SmoVertexLayout layout,
        int? requestedSlot)
    {
        if (!layout.BlendWeightsOffset.HasValue)
            return 0;

        IReadOnlyList<SmoBoneSlot> slots =
            SmoMeshReplacer.GetBoneSlots(document, target);
        if (requestedSlot.HasValue &&
            slots.All(choice => choice.Slot != requestedSlot.Value))
        {
            throw new InvalidOperationException(
                $"Bone palette slot {requestedSlot.Value} is not used by " +
                $"mesh [{target.Index}] and is unsafe.");
        }

        int slot = requestedSlot ?? slots.FirstOrDefault()?.Slot ?? 0;
        return slot is >= 0 and <= byte.MaxValue
            ? slot
            : throw new ArgumentOutOfRangeException(
                nameof(requestedSlot),
                "Bone palette slot must be in range 0..255.");
    }

    internal static byte[] BuildMeshObject(
        SmoDocument document,
        SmoObjectEntry target,
        SmoMesh template,
        SmoVertexLayout layout,
        ImportedMesh mesh,
        ReplacementTransform transform,
        int boneSlot,
        Matrix4x4? referenceWorldTransform = null)
    {
        int vertexCount = mesh.Positions.Length;
        int indexCount = mesh.TriangleIndices.Length;
        if (vertexCount > ushort.MaxValue || indexCount % 3 != 0)
            throw new InvalidOperationException(
                "A replacement chunk exceeds UInt16 limits or has incomplete triangles.");
        foreach (uint index in mesh.TriangleIndices)
        {
            if (index >= vertexCount || index > ushort.MaxValue)
                throw new InvalidDataException(
                    $"Replacement index {index} is outside its chunk.");
        }
        SmoPreparedReplacementGeometry prepared = PrepareGeometry(
            document,
            target,
            layout,
            mesh,
            transform,
            boneSlot,
            referenceWorldTransform);

        return SmoMeshDataWriter.CreateTriangleList(SmoMesh.CreateTransient(
            template, prepared.Positions, prepared.Normals, prepared.TextureCoordinates,
            prepared.SecondaryTextureCoordinates, prepared.DiffuseColorsArgb,
            prepared.BlendWeights, prepared.BlendIndices, prepared.TriangleIndices));
    }

    private static SmoPreparedReplacementGeometry PrepareGeometry(
        SmoDocument document,
        SmoObjectEntry target,
        SmoVertexLayout layout,
        ImportedMesh mesh,
        ReplacementTransform transform,
        int boneSlot,
        Matrix4x4? referenceWorldTransform)
    {
        int vertexCount = mesh.Positions.Length;
        if (vertexCount > ushort.MaxValue ||
            mesh.TriangleIndices.Length % 3 != 0 ||
            mesh.TriangleIndices.Any(index => index >= vertexCount))
        {
            throw new InvalidDataException(
                "Replacement geometry exceeds UInt16 limits or has invalid triangles.");
        }

        Matrix4x4 world = referenceWorldTransform ??
            SmoNodeTransformDecoder.ResolveModelWorldMatrix(document, target);
        if (!Matrix4x4.Invert(world, out Matrix4x4 inverseWorld))
            throw new InvalidOperationException(
                $"Mesh [{target.Index}] has a singular world transform.");
        Matrix4x4 adjustment = transform.Matrix;
        Matrix4x4 adjustmentNormal =
            Matrix4x4.Invert(adjustment, out Matrix4x4 inverseAdjustment)
                ? Matrix4x4.Transpose(inverseAdjustment)
                : adjustment;
        Matrix4x4 worldToLocalNormal = Matrix4x4.Transpose(world);

        var positions = new Vector3[vertexCount];
        Vector3[] sourceNormals = layout.NormalOffset.HasValue
            ? GlbModelReader.RepairInvalidNormals(
                mesh.Positions,
                mesh.Normals.Length == vertexCount
                    ? mesh.Normals
                    : new Vector3[vertexCount],
                mesh.TriangleIndices,
                out _,
                out _)
            : [];
        Vector3[] normals = layout.NormalOffset.HasValue
            ? new Vector3[vertexCount]
            : [];
        Vector2[] textureCoordinates = layout.TextureCoordinate0Offset.HasValue &&
                                       mesh.TextureCoordinates.Length == vertexCount
            ? mesh.TextureCoordinates.ToArray()
            : [];
        Vector2[] secondaryTextureCoordinates = layout.TextureCoordinate1Offset.HasValue
            ? mesh.SecondaryTextureCoordinates.Length == vertexCount
                ? mesh.SecondaryTextureCoordinates.ToArray()
                : mesh.TextureCoordinates.Length == vertexCount
                    ? mesh.TextureCoordinates.ToArray()
                    : []
            : [];
        uint[] diffuseColors = layout.DiffuseArgbOffset.HasValue
            ? new uint[vertexCount]
            : [];
        Vector4[] blendWeights = layout.BlendWeightsOffset.HasValue
            ? new Vector4[vertexCount]
            : [];
        SmoBlendIndices[] blendIndices = layout.BlendIndicesOffset.HasValue
            ? new SmoBlendIndices[vertexCount]
            : [];
        for (int vertex = 0; vertex < vertexCount; vertex++)
        {
            Vector3 adjusted = Vector3.Transform(
                mesh.Positions[vertex], adjustment);
            positions[vertex] = Vector3.Transform(
                new Vector3(adjusted.X, adjusted.Y, -adjusted.Z),
                inverseWorld);
            if (normals.Length == vertexCount)
            {
                Vector3 normal = Vector3.TransformNormal(
                    sourceNormals[vertex], adjustmentNormal);
                normal.Z = -normal.Z;
                normal = Vector3.TransformNormal(normal, worldToLocalNormal);
                normals[vertex] = normal.LengthSquared() > 0.000001f
                    ? Vector3.Normalize(normal)
                    : Vector3.UnitY;
            }
            if (diffuseColors.Length == vertexCount)
            {
                diffuseColors[vertex] = mesh.DiffuseColors.Length == vertexCount
                    ? mesh.DiffuseColors[vertex]
                    : 0xFFFFFFFF;
            }
            if (blendWeights.Length == vertexCount)
            {
                blendWeights[vertex] = new Vector4(1, 0, 0, 0);
                blendIndices[vertex] = new SmoBlendIndices(
                    checked((byte)boneSlot), 0, 0, 0);
            }
        }

        uint[] triangles = new uint[mesh.TriangleIndices.Length];
        for (int triangle = 0; triangle < triangles.Length; triangle += 3)
        {
            triangles[triangle] = mesh.TriangleIndices[triangle];
            triangles[triangle + 1] = mesh.TriangleIndices[triangle + 2];
            triangles[triangle + 2] = mesh.TriangleIndices[triangle + 1];
        }
        return new SmoPreparedReplacementGeometry(
            positions,
            normals,
            textureCoordinates,
            secondaryTextureCoordinates,
            diffuseColors,
            blendWeights,
            blendIndices,
            triangles);
    }

    internal static byte[] Repack(
        SmoDocument document,
        IReadOnlyList<SmoMeshObjectReplacement> replacements)
    {
        SmoMeshObjectReplacement[] ordered = replacements
            .OrderBy(item => item.Entry.PhysicalOffset)
            .ToArray();
        for (int index = 1; index < ordered.Length; index++)
        {
            if (ordered[index].Entry.PhysicalOffset <
                ordered[index - 1].Entry.PhysicalEnd)
            {
                throw new InvalidOperationException(
                    "Replacement mesh intervals overlap.");
            }
        }

        byte[] source = document.Data.ToArray();
        long finalLength = source.LongLength + ordered.Sum(item =>
            (long)item.Data.Length - item.Entry.SerializedSize);
        byte[] result = new byte[checked((int)finalLength)];
        int sourceCursor = 0;
        int targetCursor = 0;
        foreach (SmoMeshObjectReplacement replacement in ordered)
        {
            int start = checked((int)replacement.Entry.PhysicalOffset);
            int end = checked((int)replacement.Entry.PhysicalEnd);
            source.AsSpan(sourceCursor, start - sourceCursor)
                .CopyTo(result.AsSpan(targetCursor));
            targetCursor += start - sourceCursor;
            replacement.Data.CopyTo(result.AsSpan(targetCursor));
            targetCursor += replacement.Data.Length;
            sourceCursor = end;
        }
        source.AsSpan(sourceCursor).CopyTo(result.AsSpan(targetCursor));

        long Map(long oldOffset) => oldOffset + ordered
            .Where(item => item.Entry.PhysicalEnd <= oldOffset)
            .Sum(item => (long)item.Data.Length - item.Entry.SerializedSize);
        var replacementByIndex = ordered.ToDictionary(item => item.Entry.Index);
        foreach (SmoObjectEntry entry in document.Objects)
        {
            long newStart = Map(entry.PhysicalOffset);
            long newEnd = Map(entry.PhysicalEnd);
            uint newSize = checked((uint)(newEnd - newStart));
            int logicalOffsetField = entry.TableOffset + sizeof(uint) +
                sizeof(ushort) + entry.NameLength + sizeof(uint);
            WriteUInt32(
                result,
                logicalOffsetField,
                checked((uint)(newStart - document.Header.DataStart)));
            WriteUInt32(result, logicalOffsetField + sizeof(uint), newSize);

            if (!replacementByIndex.ContainsKey(entry.Index) &&
                newSize != entry.SerializedSize)
            {
                int objectStart = checked((int)newStart);
                ReadOnlySpan<byte> originalObject = source.AsSpan(
                    checked((int)entry.PhysicalOffset),
                    checked((int)entry.SerializedSize));
                if (SmoDataBlockReader.TryReadHeader(
                        originalObject,
                        8,
                        out SmoDataBlockHeader outer) &&
                    outer.PayloadEnd + 1 == entry.SerializedSize)
                {
                    if (outer.SizeKind != SmoDataBlockSizeCode.UInt32)
                    {
                        throw new InvalidOperationException(
                            $"Resized wrapping object [{entry.Index}] has a " +
                            "non-writable outer size field.");
                    }
                    WriteUInt32(
                        result,
                        objectStart + outer.Offset + outer.HeaderSize - sizeof(uint),
                        checked(newSize - (uint)(8 + outer.HeaderSize + 1)));
                }
            }
        }
        WriteUInt32(result, 0x0C, checked((uint)result.Length));
        WriteUInt32(
            result,
            0x18,
            checked((uint)result.Length - document.Header.DataStart));
        return result;
    }

    private static void WriteUInt32(Span<byte> data, int offset, uint value) =>
        BinaryPrimitives.WriteUInt32LittleEndian(data[offset..], value);
}

public sealed record SmoMeshResourceReplacement(
    byte[] Data,
    int MeshObjectIndex,
    int VertexCount,
    int TriangleCount,
    int BoneSlot);

internal sealed record SmoMeshObjectReplacement(
    SmoObjectEntry Entry,
    byte[] Data);

internal sealed record SmoPreparedReplacementGeometry(
    Vector3[] Positions,
    Vector3[] Normals,
    Vector2[] TextureCoordinates,
    Vector2[] SecondaryTextureCoordinates,
    uint[] DiffuseColorsArgb,
    Vector4[] BlendWeights,
    SmoBlendIndices[] BlendIndices,
    uint[] TriangleIndices);
