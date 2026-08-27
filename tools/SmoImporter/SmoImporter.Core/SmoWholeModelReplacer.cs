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
        int maximumTextureDimension =
            SMOTextureTool.Core.TextureInfo.MaximumCurrentHeaderDimension)
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

        var replacements = new List<SmoMeshObjectReplacement>(targets.Length);
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
            byte[] payload = SmoMeshResourceReplacer.BuildMeshObject(
                document, target, template, layout, targetMesh, transform, boneSlot);
            replacements.Add(new SmoMeshObjectReplacement(target, payload));
        }

        byte[] output = SmoMeshResourceReplacer.Repack(document, replacements);
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
        using (Image<Rgba32> sourceImage = Image.Load<Rgba32>(imageData))
        {
            if (sourceImage.Width > maximumDimension ||
                sourceImage.Height > maximumDimension)
            {
                throw new InvalidDataException(
                    $"Texture is {sourceImage.Width}x{sourceImage.Height}; " +
                    $"maximum is {maximumDimension}x{maximumDimension}. " +
                    "Downscaling is forbidden.");
            }
        }
        byte[] replaced = FixedSizeTextureWriter.ReplaceRgbWithoutDownscaling(
            output, target.Index, imageData);
        SMOTextureTool.Core.TextureInfo verifiedTarget =
            SMOTextureTool.Core.SmoDocument.Parse(replaced).Textures
                .Single(texture => texture.Index == target.Index);
        if (!SameMaterialOwner(target.Material, verifiedTarget.Material))
            throw new InvalidDataException(
                "Texture replacement changed its material owner.");
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

    private static void WriteUInt32(Span<byte> data, int offset, uint value) =>
        BinaryPrimitives.WriteUInt32LittleEndian(data[offset..], value);
}
