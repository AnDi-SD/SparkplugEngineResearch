using System.Buffers.Binary;
using System.Numerics;
using SmoViewer.Core;

namespace SmoImporter.Core;

public static class SmoMeshReplacer
{
    public static IReadOnlyList<SmoBoneSlot> GetBoneSlots(
        SmoDocument document, SmoObjectEntry meshEntry)
    {
        SmoObjectEntry? skinEntry = FindAncestor(document, meshEntry, SmoClassIds.Skin);
        if (skinEntry is null || !SmoSkinDecoder.TryDecode(
                document, skinEntry, out SmoSkin? skin, out _) || skin is null)
            return [];
        IReadOnlyDictionary<int, SmoObjectEntry> entries = document.Objects.ToDictionary(item => item.Index);
        return skin.Bones.Select(bone => new SmoBoneSlot(
            bone.PaletteIndex,
            bone.NodeObjectId,
            entries.TryGetValue(bone.NodeObjectIndex, out SmoObjectEntry? node)
                ? node.Name : $"node_{bone.NodeObjectId}")).ToArray();
    }

    public static ReplacementResult Replace(
        SmoDocument document,
        SmoObjectEntry meshEntry,
        ImportedMesh replacement,
        ReplacementTransform transform,
        int boneSlot,
        string outputPath)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(meshEntry);
        ArgumentNullException.ThrowIfNull(replacement);
        SmoMesh source = SmoMeshDecoder.Decode(document, meshEntry);
        if (!SmoVertexLayoutRegistry.TryGet(source.VertexFormat, out SmoVertexLayout? layout) ||
            layout is null || layout.SerializedStride != source.Stride)
            throw new InvalidOperationException("Target mesh does not use a writable confirmed vertex layout.");
        if (replacement.Positions.Length != source.VertexCount)
            throw new InvalidOperationException(
                $"Topology-safe replacement requires {source.VertexCount} vertices; imported mesh has {replacement.Positions.Length}.");
        uint[] expectedTriangles = source.TriangleIndices.ToArray();
        for (int i = 0; i < expectedTriangles.Length; i += 3)
            (expectedTriangles[i + 1], expectedTriangles[i + 2]) = (expectedTriangles[i + 2], expectedTriangles[i + 1]);
        if (!replacement.TriangleIndices.SequenceEqual(expectedTriangles))
            throw new InvalidOperationException(
                "Triangle topology/order differs from the exported source mesh. " +
                "The first safe writer keeps the original strip and cannot replace topology yet.");
        IReadOnlyList<SmoBoneSlot> slots = GetBoneSlots(document, meshEntry);
        SmoBoneSlot? selectedBone = slots.FirstOrDefault(item => item.Slot == boneSlot);
        if (layout.BlendWeightsOffset.HasValue && slots.Count > 0 && selectedBone is null)
            throw new InvalidOperationException($"Bone palette slot {boneSlot} is not available for this mesh.");

        byte[] output = document.Data.ToArray();
        Matrix4x4 world = SmoNodeTransformDecoder.ResolveModelWorldMatrix(document, meshEntry);
        if (!Matrix4x4.Invert(world, out Matrix4x4 inverseWorld))
            throw new InvalidOperationException("Target mesh world transform is singular.");
        Matrix4x4 adjustment = transform.Matrix;
        Matrix4x4 adjustmentNormal = adjustment;
        if (Matrix4x4.Invert(adjustment, out Matrix4x4 inverseAdjustment))
            adjustmentNormal = Matrix4x4.Transpose(inverseAdjustment);
        Matrix4x4 worldToLocalNormal = Matrix4x4.Transpose(world);

        for (int vertex = 0; vertex < source.VertexCount; vertex++)
        {
            int offset = checked((int)source.VertexDataOffset + vertex * source.Stride);
            Vector3 gltf = Vector3.Transform(replacement.Positions[vertex], adjustment);
            Vector3 local = Vector3.Transform(new Vector3(gltf.X, gltf.Y, -gltf.Z), inverseWorld);
            WriteVector3(output, offset, local);

            if (layout.NormalOffset is int normalOffset && replacement.Normals.Length == source.VertexCount)
            {
                Vector3 normal = Vector3.TransformNormal(replacement.Normals[vertex], adjustmentNormal);
                normal.Z = -normal.Z;
                normal = Vector3.TransformNormal(normal, worldToLocalNormal);
                if (normal.LengthSquared() > 0.000001f) normal = Vector3.Normalize(normal);
                WriteVector3(output, offset + normalOffset, normal);
            }
            if (layout.TextureCoordinate0Offset is int uvOffset &&
                replacement.TextureCoordinates.Length == source.VertexCount)
            {
                WriteSingle(output, offset + uvOffset, replacement.TextureCoordinates[vertex].X);
                WriteSingle(output, offset + uvOffset + 4, replacement.TextureCoordinates[vertex].Y);
            }
            if (layout.BlendWeightsOffset is int weightsOffset &&
                layout.BlendIndicesOffset is int indicesOffset)
            {
                WriteSingle(output, offset + weightsOffset, 1f);
                WriteSingle(output, offset + weightsOffset + 4, 0f);
                WriteSingle(output, offset + weightsOffset + 8, 0f);
                WriteSingle(output, offset + weightsOffset + 12, 0f);
                output[offset + indicesOffset] = checked((byte)boneSlot);
                output[offset + indicesOffset + 1] = 0;
                output[offset + indicesOffset + 2] = 0;
                output[offset + indicesOffset + 3] = 0;
            }
        }

        string fullOutput = Path.GetFullPath(outputPath);
        Directory.CreateDirectory(Path.GetDirectoryName(fullOutput)!);
        File.WriteAllBytes(fullOutput, output);
        SmoDocument verification = SmoDocument.Load(fullOutput);
        SmoObjectEntry verifiedEntry = verification.Objects[meshEntry.Index];
        SmoMesh verified = SmoMeshDecoder.Decode(verification, verifiedEntry);
        if (verified.VertexCount != source.VertexCount || verified.TriangleCount != source.TriangleCount)
            throw new InvalidDataException("Written SMO failed post-write mesh verification.");
        return new ReplacementResult(
            fullOutput, verified.VertexCount, verified.TriangleCount,
            boneSlot, selectedBone?.Name ?? "not skinned");
    }

    private static SmoObjectEntry? FindAncestor(
        SmoDocument document, SmoObjectEntry entry, uint classId)
    {
        IReadOnlyDictionary<int, SmoObjectEntry> entries = document.Objects.ToDictionary(item => item.Index);
        SmoObjectEntry? cursor = entry;
        while (cursor.ParentIndex is int parent && entries.TryGetValue(parent, out cursor))
            if (cursor.TypeHash == classId) return cursor;
        return null;
    }

    private static void WriteVector3(Span<byte> data, int offset, Vector3 value)
    {
        WriteSingle(data, offset, value.X);
        WriteSingle(data, offset + 4, value.Y);
        WriteSingle(data, offset + 8, value.Z);
    }

    private static void WriteSingle(Span<byte> data, int offset, float value) =>
        BinaryPrimitives.WriteInt32LittleEndian(data[offset..], BitConverter.SingleToInt32Bits(value));
}
