using System.Buffers.Binary;
using System.Numerics;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;
using SixLabors.ImageSharp.Processing;
using SmoViewer.Core;

namespace SmoImporter.Core;

public sealed record WholeModelReplacementResult(
    string OutputPath,
    int MeshCount,
    int VertexCount,
    int TriangleCount,
    long FileSize);

public static class SmoWholeModelReplacer
{
    public static WholeModelReplacementResult Replace(
        SmoDocument document,
        ImportedScene replacement,
        ReplacementTransform transform,
        string outputPath,
        int? rigidBoneSlot = null,
        string? texturePath = null,
        ImportedTexture? embeddedTexture = null,
        int maximumTextureDimension = 2048)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(replacement);

        SmoObjectEntry[] targets = document.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.MeshData)
            .ToArray();
        if (targets.Length == 0)
            throw new InvalidOperationException("The target SMO contains no mesh slots.");

        ImportedMesh combined = ImportedMeshCombiner.Combine(replacement);
        int triangleCount = combined.TriangleIndices.Length / 3;
        if (combined.Positions.Length > ushort.MaxValue)
            throw new InvalidOperationException(
                $"The imported model has {combined.Positions.Length} vertices. " +
                "Rigid single-slot mode currently supports at most 65,535 unique vertices.");

        SmoObjectEntry host = targets
            .OrderByDescending(entry => SmoMeshDecoder.Decode(document, entry).VertexCount)
            .First();
        IReadOnlyList<SmoBoneSlot> allowedRigidBones = GetRigidBoneChoices(document);
        if (rigidBoneSlot.HasValue && allowedRigidBones.All(choice => choice.Slot != rigidBoneSlot.Value))
            throw new InvalidOperationException(
                $"Bone palette slot {rigidBoneSlot.Value} is not used by the original host mesh and is unsafe.");
        var empty = new ImportedMesh(
            "disabled_mesh", [Vector3.Zero], [Vector3.UnitY], [Vector2.Zero], [0, 0, 0]);

        var replacements = new List<ObjectReplacement>(targets.Length);
        for (int index = 0; index < targets.Length; index++)
        {
            SmoObjectEntry target = targets[index];
            SmoMesh template = SmoMeshDecoder.Decode(document, target);
            if (template.Marker != SmoMeshDecoder.E1Marker ||
                !SmoVertexLayoutRegistry.TryGet(template.VertexFormat, out SmoVertexLayout? layout) ||
                layout is null || layout.SerializedStride != template.Stride)
                throw new InvalidOperationException($"Mesh [{target.Index}] is not a confirmed writable E1 layout.");

            IReadOnlyList<SmoBoneSlot> slots = SmoMeshReplacer.GetBoneSlots(document, target);
            int boneSlot = layout.BlendWeightsOffset.HasValue
                ? target.Index == host.Index && rigidBoneSlot.HasValue
                    ? rigidBoneSlot.Value
                    : slots.FirstOrDefault()?.Slot ?? 0
                : 0;
            if (boneSlot is < 0 or > byte.MaxValue)
                throw new ArgumentOutOfRangeException(nameof(rigidBoneSlot), "Bone palette slot must be in range 0..255.");
            ImportedMesh targetMesh = target.Index == host.Index ? combined : empty;
            byte[] payload = BuildMeshObject(
                document, target, template, layout, targetMesh, transform, boneSlot);
            replacements.Add(new ObjectReplacement(target, payload));
        }

        byte[] output = Repack(document, replacements);
        if (!string.IsNullOrWhiteSpace(texturePath))
        {
            output = ReplaceBodyTexture(
                document,
                host,
                output,
                File.ReadAllBytes(Path.GetFullPath(texturePath)),
                maximumTextureDimension);
        }
        else if (embeddedTexture is not null)
        {
            output = ReplaceBodyTexture(
                document,
                host,
                output,
                embeddedTexture.Data,
                maximumTextureDimension);
        }
        else
        {
            VerifyTargetTexturesUnchanged(document, output);
        }
        string fullOutput = Path.GetFullPath(outputPath);
        Directory.CreateDirectory(Path.GetDirectoryName(fullOutput)!);
        File.WriteAllBytes(fullOutput, output);

        SmoDocument verified = SmoDocument.Load(fullOutput);
        SmoMesh[] meshes = verified.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.MeshData)
            .Select(entry => SmoMeshDecoder.Decode(verified, entry))
            .ToArray();
        SmoMesh verifiedHost = meshes.Single(mesh => mesh.ObjectIndex == host.Index);
        bool disabledMeshesValid = meshes
            .Where(mesh => mesh.ObjectIndex != host.Index)
            .All(mesh => mesh.VertexCount == 1 && mesh.TriangleCount == 1 &&
                mesh.TriangleIndices.SequenceEqual(new uint[] { 0, 0, 0 }));
        if (verified.HasErrors || meshes.Length != targets.Length ||
            verifiedHost.TriangleCount != triangleCount || !disabledMeshesValid)
            throw new InvalidDataException("The repacked SMO failed strict post-write verification.");

        return new WholeModelReplacementResult(
            fullOutput, meshes.Length, meshes.Sum(mesh => mesh.VertexCount),
            triangleCount, output.LongLength);
    }

    public static IReadOnlyList<SmoBoneSlot> GetRigidBoneChoices(SmoDocument document)
    {
        SmoObjectEntry host = document.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.MeshData)
            .OrderByDescending(entry => SmoMeshDecoder.Decode(document, entry).VertexCount)
            .First();
        IReadOnlyList<SmoBoneSlot> direct = SmoMeshReplacer.GetBoneSlots(document, host);
        if (direct.Count > 0) return direct;

        var choices = new Dictionary<int, SmoBoneSlot>();
        foreach (SmoObjectEntry skinEntry in document.Objects.Where(entry => entry.TypeHash == SmoClassIds.Skin))
        {
            if (!SmoSkinDecoder.TryDecode(document, skinEntry, out SmoSkin? skin, out _) || skin is null)
                continue;
            IReadOnlyDictionary<int, SmoObjectEntry> entries = document.Objects.ToDictionary(entry => entry.Index);
            foreach (SmoSkinBone bone in skin.Bones)
                choices.TryAdd(bone.PaletteIndex, new SmoBoneSlot(
                    bone.PaletteIndex, bone.NodeObjectId, entries[bone.NodeObjectIndex].Name));
        }
        SmoMesh mesh = SmoMeshDecoder.Decode(document, host);
        var usedSlots = new SortedSet<int>();
        if (mesh.HasSkinningData)
        {
            for (int vertex = 0; vertex < mesh.VertexCount; vertex++)
            {
                Vector4 weights = mesh.BlendWeights[vertex];
                SmoBlendIndices indices = mesh.BlendIndices[vertex];
                if (weights.X > 0.000001f) usedSlots.Add(indices.X);
                if (weights.Y > 0.000001f) usedSlots.Add(indices.Y);
                if (weights.Z > 0.000001f) usedSlots.Add(indices.Z);
                if (weights.W > 0.000001f) usedSlots.Add(indices.W);
            }
        }
        if (usedSlots.Count == 0) usedSlots.Add(0);
        return usedSlots.Select(slot => choices.TryGetValue(slot, out SmoBoneSlot? named)
                ? named
                : new SmoBoneSlot(slot, 0, $"confirmed host palette slot {slot}"))
            .ToArray();
    }

    private static byte[] ReplaceBodyTexture(
        SmoDocument document,
        SmoObjectEntry host,
        byte[] output,
        ReadOnlySpan<byte> imageData,
        int maximumDimension)
    {
        IReadOnlyDictionary<int, SmoTextureBinding> bindings =
            SmoTextureBindingResolver.ResolveAll(document);
        if (!bindings.TryGetValue(host.Index, out SmoTextureBinding? binding) ||
            binding.Texture is null || binding.Issue is not null)
            throw new InvalidOperationException(
                "The main body mesh has no unambiguous writable texture binding.");
        int[] textureObjects = document.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.TextureData)
            .Select(entry => entry.Index)
            .ToArray();
        int textureOrdinal = Array.IndexOf(textureObjects, binding.Texture.ObjectIndex);
        if (textureOrdinal < 0)
            throw new InvalidOperationException("The bound body texture is absent from the texture catalog.");
        SMOTextureTool.Core.SmoDocument textureDocument =
            SMOTextureTool.Core.SmoDocument.Parse(output);
        if (textureOrdinal >= textureDocument.Textures.Count)
            throw new InvalidOperationException("Texture order changed during mesh repack.");
        SMOTextureTool.Core.TextureInfo target = textureDocument.Textures[textureOrdinal];
        byte[] replaced = FixedSizeTextureWriter.ReplaceRgb(
            output, target.Index, imageData);
        SMOTextureTool.Core.TextureInfo verifiedTarget =
            SMOTextureTool.Core.SmoDocument.Parse(replaced).Textures
                .Single(texture => texture.Index == target.Index);
        if (!SameMaterialOwner(target.Material, verifiedTarget.Material) ||
            replaced.Length != output.Length)
            throw new InvalidDataException(
                "Fixed-size texture replacement changed the SMO structure or material owner.");
        return replaced;
    }

    private static void VerifyTargetTexturesUnchanged(
        SmoDocument target,
        byte[] output)
    {
        SmoDocument verified = SmoDocument.Parse(output, target.SourcePath);
        SmoObjectEntry[] targetTextures = target.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.TextureData)
            .ToArray();
        if (verified.Objects.Count(entry => entry.TypeHash == SmoClassIds.TextureData) !=
            targetTextures.Length)
        {
            throw new InvalidDataException(
                "Preserving target textures changed the TextureData object count.");
        }

        foreach (SmoObjectEntry targetTexture in targetTextures)
        {
            if ((uint)targetTexture.Index >= (uint)verified.Objects.Count)
            {
                throw new InvalidDataException(
                    $"Preserving target texture [{targetTexture.Index}] changed the object catalog.");
            }
            SmoObjectEntry verifiedTexture = verified.Objects[targetTexture.Index];
            if (verifiedTexture.TypeHash != SmoClassIds.TextureData ||
                verifiedTexture.Id != targetTexture.Id ||
                verifiedTexture.SerializedSize != targetTexture.SerializedSize ||
                !target.Data.Span.Slice(
                        checked((int)targetTexture.PhysicalOffset),
                        checked((int)targetTexture.SerializedSize))
                    .SequenceEqual(verified.Data.Span.Slice(
                        checked((int)verifiedTexture.PhysicalOffset),
                        checked((int)verifiedTexture.SerializedSize))))
            {
                throw new InvalidDataException(
                    $"Target texture [{targetTexture.Index}] {targetTexture.Name} changed " +
                    "while texture preservation was enabled.");
            }
        }
    }

    private static bool SameMaterialOwner(
        SMOTextureTool.Core.MaterialReferenceInfo? before,
        SMOTextureTool.Core.MaterialReferenceInfo? after) =>
        before is null && after is null ||
        before is not null && after is not null &&
        before.Index == after.Index &&
        before.PassIndex == after.PassIndex &&
        before.LayerIndex == after.LayerIndex &&
        before.LayerClassId == after.LayerClassId;


    private static void PatchCatalogAfterTextureResize(
        Span<byte> output,
        SmoDocument before,
        int oldPixelOffset,
        int oldPixelSize,
        int delta)
    {
        long oldPixelEnd = checked((long)oldPixelOffset + oldPixelSize);
        long Map(long oldOffset) => oldOffset >= oldPixelEnd ? oldOffset + delta : oldOffset;
        foreach (SmoObjectEntry entry in before.Objects)
        {
            long newStart = Map(entry.PhysicalOffset);
            long newEnd = Map(entry.PhysicalEnd);
            int logicalOffsetField = entry.TableOffset + sizeof(uint) + sizeof(ushort) +
                entry.NameLength + sizeof(uint);
            WriteUInt32(output, logicalOffsetField,
                checked((uint)(newStart - before.Header.DataStart)));
            WriteUInt32(output, logicalOffsetField + sizeof(uint),
                checked((uint)(newEnd - newStart)));
        }
    }

    private static byte[] BuildMeshObject(
        SmoDocument document,
        SmoObjectEntry target,
        SmoMesh template,
        SmoVertexLayout layout,
        ImportedMesh mesh,
        ReplacementTransform transform,
        int boneSlot)
    {
        int vertexCount = mesh.Positions.Length;
        int indexCount = mesh.TriangleIndices.Length;
        if (vertexCount > ushort.MaxValue || indexCount % 3 != 0)
            throw new InvalidOperationException("A replacement chunk exceeds UInt16 limits or has incomplete triangles.");
        foreach (uint index in mesh.TriangleIndices)
            if (index >= vertexCount || index > ushort.MaxValue)
                throw new InvalidDataException($"Replacement index {index} is outside its chunk.");

        int indexBytes = checked(indexCount * sizeof(ushort));
        int vertexBytes = checked(vertexCount * template.Stride);
        const int preambleSize = 17;
        const int primitiveHeaderSize = 12;
        const int vertexHeaderSize = 12;
        int payloadSize = checked(preambleSize + primitiveHeaderSize + indexBytes + vertexHeaderSize + vertexBytes);
        byte[] result = new byte[checked(8 + 5 + payloadSize + 1)];
        WriteUInt32(result, 0, SmoClassIds.MeshData);
        "SBOO"u8.CopyTo(result.AsSpan(4));
        result[8] = SmoMeshDecoder.E1Marker;
        WriteUInt32(result, 9, (uint)payloadSize);
        int payload = 13;
        WriteUInt32(result, payload, template.VertexFormat);
        WriteUInt32(result, payload + 4, (uint)vertexCount);
        WriteUInt32(result, payload + 8, checked((uint)(vertexCount * template.RuntimeStride)));
        WriteUInt32(result, payload + 12, (uint)indexBytes);
        result[payload + 16] = 0;
        int primitive = payload + preambleSize;
        WriteUInt32(result, primitive, SmoMeshDecoder.TriangleListPrimitive);
        WriteUInt32(result, primitive + 4, checked((uint)(indexCount / 3)));
        WriteUInt32(result, primitive + 8, 0);
        int indices = primitive + primitiveHeaderSize;
        for (int triangle = 0; triangle < indexCount; triangle += 3)
        {
            WriteUInt16(result, indices + triangle * 2, checked((ushort)mesh.TriangleIndices[triangle]));
            WriteUInt16(result, indices + (triangle + 1) * 2, checked((ushort)mesh.TriangleIndices[triangle + 2]));
            WriteUInt16(result, indices + (triangle + 2) * 2, checked((ushort)mesh.TriangleIndices[triangle + 1]));
        }
        int vertexHeader = indices + indexBytes;
        WriteUInt32(result, vertexHeader, template.VertexFormat);
        WriteUInt32(result, vertexHeader + 4, (uint)vertexCount);
        WriteUInt32(result, vertexHeader + 8, 0);
        int vertices = vertexHeader + vertexHeaderSize;

        Matrix4x4 world = SmoNodeTransformDecoder.ResolveModelWorldMatrix(document, target);
        if (!Matrix4x4.Invert(world, out Matrix4x4 inverseWorld))
            throw new InvalidOperationException($"Mesh [{target.Index}] has a singular world transform.");
        Matrix4x4 adjustment = transform.Matrix;
        Matrix4x4 adjustmentNormal = Matrix4x4.Invert(adjustment, out Matrix4x4 inverseAdjustment)
            ? Matrix4x4.Transpose(inverseAdjustment) : adjustment;
        Matrix4x4 worldToLocalNormal = Matrix4x4.Transpose(world);

        for (int vertex = 0; vertex < vertexCount; vertex++)
        {
            int offset = vertices + vertex * template.Stride;
            Vector3 adjusted = Vector3.Transform(mesh.Positions[vertex], adjustment);
            Vector3 local = Vector3.Transform(new Vector3(adjusted.X, adjusted.Y, -adjusted.Z), inverseWorld);
            WriteVector3(result, offset, local);
            if (layout.NormalOffset is int normalOffset && mesh.Normals.Length == vertexCount)
            {
                Vector3 normal = Vector3.TransformNormal(mesh.Normals[vertex], adjustmentNormal);
                normal.Z = -normal.Z;
                normal = Vector3.TransformNormal(normal, worldToLocalNormal);
                if (normal.LengthSquared() > 0.000001f) normal = Vector3.Normalize(normal);
                WriteVector3(result, offset + normalOffset, normal);
            }
            if (layout.TextureCoordinate0Offset is int uvOffset && mesh.TextureCoordinates.Length == vertexCount)
            {
                WriteSingle(result, offset + uvOffset, mesh.TextureCoordinates[vertex].X);
                WriteSingle(result, offset + uvOffset + 4, mesh.TextureCoordinates[vertex].Y);
            }
            if (layout.DiffuseArgbOffset is int colorOffset)
                WriteUInt32(result, offset + colorOffset, 0xFFFFFFFF);
            if (layout.BlendWeightsOffset is int weightsOffset && layout.BlendIndicesOffset is int bonesOffset)
            {
                WriteSingle(result, offset + weightsOffset, 1f);
                result[offset + bonesOffset] = checked((byte)boneSlot);
            }
        }
        return result;
    }

    private static byte[] Repack(SmoDocument document, IReadOnlyList<ObjectReplacement> replacements)
    {
        ObjectReplacement[] ordered = replacements.OrderBy(item => item.Entry.PhysicalOffset).ToArray();
        for (int i = 1; i < ordered.Length; i++)
            if (ordered[i].Entry.PhysicalOffset < ordered[i - 1].Entry.PhysicalEnd)
                throw new InvalidOperationException("Replacement mesh intervals overlap.");

        byte[] source = document.Data.ToArray();
        long finalLength = source.LongLength + ordered.Sum(item => (long)item.Data.Length - item.Entry.SerializedSize);
        byte[] result = new byte[checked((int)finalLength)];
        int sourceCursor = 0, targetCursor = 0;
        foreach (ObjectReplacement replacement in ordered)
        {
            int start = checked((int)replacement.Entry.PhysicalOffset);
            int end = checked((int)replacement.Entry.PhysicalEnd);
            source.AsSpan(sourceCursor, start - sourceCursor).CopyTo(result.AsSpan(targetCursor));
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
            int logicalOffsetField = entry.TableOffset + sizeof(uint) + sizeof(ushort) + entry.NameLength + sizeof(uint);
            WriteUInt32(result, logicalOffsetField, checked((uint)(newStart - document.Header.DataStart)));
            WriteUInt32(result, logicalOffsetField + sizeof(uint), newSize);

            if (!replacementByIndex.ContainsKey(entry.Index) && newSize != entry.SerializedSize)
            {
                int objectStart = checked((int)newStart);
                ReadOnlySpan<byte> originalObject = source.AsSpan(
                    checked((int)entry.PhysicalOffset), checked((int)entry.SerializedSize));
                if (SmoDataBlockReader.TryReadHeader(originalObject, 8, out SmoDataBlockHeader outer) &&
                    outer.PayloadEnd + 1 == entry.SerializedSize)
                {
                    if (outer.SizeKind != SmoDataBlockSizeCode.UInt32)
                        throw new InvalidOperationException(
                            $"Resized wrapping object [{entry.Index}] has a non-writable outer size field.");
                    WriteUInt32(result, objectStart + outer.Offset + outer.HeaderSize - sizeof(uint),
                        checked(newSize - (uint)(8 + outer.HeaderSize + 1)));
                }
            }
        }
        WriteUInt32(result, 0x0C, checked((uint)result.Length));
        WriteUInt32(result, 0x18, checked((uint)result.Length - document.Header.DataStart));
        return result;
    }

    private static void WriteVector3(Span<byte> data, int offset, Vector3 value)
    { WriteSingle(data, offset, value.X); WriteSingle(data, offset + 4, value.Y); WriteSingle(data, offset + 8, value.Z); }
    private static void WriteSingle(Span<byte> data, int offset, float value) =>
        BinaryPrimitives.WriteInt32LittleEndian(data[offset..], BitConverter.SingleToInt32Bits(value));
    private static void WriteUInt16(Span<byte> data, int offset, ushort value) =>
        BinaryPrimitives.WriteUInt16LittleEndian(data[offset..], value);
    private static void WriteUInt32(Span<byte> data, int offset, uint value) =>
        BinaryPrimitives.WriteUInt32LittleEndian(data[offset..], value);

    private sealed record ObjectReplacement(SmoObjectEntry Entry, byte[] Data);
}
