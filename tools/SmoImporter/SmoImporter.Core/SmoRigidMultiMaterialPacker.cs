using System.Buffers.Binary;
using System.Numerics;
using System.Security.Cryptography;
using System.Text;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;
using SmoViewer.Core;

namespace SmoImporter.Core;

public sealed record SmoRigidPackedTexture(
    int MaterialNumber,
    int FrameNumber,
    string SourcePath,
    uint ObjectId,
    string ObjectName,
    int Width,
    int Height,
    bool UsesAlpha);

public sealed record SmoRigidMultiMaterialPackAnalysis(
    bool CanPack,
    int MaterialGroupCount,
    int MeshCount,
    int TextureCount,
    int SequenceCount,
    int VertexCount,
    int TriangleCount,
    int RigidBoneSlot,
    string RigidBoneName,
    IReadOnlyList<string> Messages);

public sealed record SmoRigidMultiMaterialPackResult(
    string OutputPath,
    int MaterialGroupCount,
    int AddedMeshCount,
    int AddedTextureCount,
    int AddedSequenceCount,
    int VertexCount,
    int TriangleCount,
    int RigidBoneSlot,
    string RigidBoneName,
    long FileSize,
    string Sha256,
    IReadOnlyList<SmoRigidPackedTexture> Textures);

/// <summary>
/// Builds independent rigid skin/material/texture branches for an external GLB
/// texture bundle. The target service graph and skeleton remain intact; every
/// generated vertex uses the target primary palette's Head slot.
/// </summary>
public static class SmoRigidMultiMaterialPacker
{
    private const int ObjectSignatureSize = 8;
    private const int ObjectReferenceSize = 8;
    private const int SerializedTexturePixelOffset = 0x3D;
    private const int SerializedTextureMarkerOffset = 0x3C;
    private const uint TextureSequenceClassId = 0x16FB0E47;
    private const uint SharedVisualHelperClassId = 0x7AC95AEC;
    private const uint AlphaBlendFlag = 0x4;
    private const int ExpectedPaletteSize = 16;
    private const int DefaultRigidBoneSlot = 8;
    private const int SequenceFrameStepBits = 0x3D088889;

    /// <summary>
    /// Performs the writer-specific compatibility checks used by <see cref="Pack"/>
    /// without constructing a visual forest or writing an output file.
    /// </summary>
    public static SmoRigidMultiMaterialPackAnalysis Analyze(
        SmoDocument target,
        RigidGlbTextureBundle bundle)
    {
        ArgumentNullException.ThrowIfNull(target);
        ArgumentNullException.ThrowIfNull(bundle);

        int materialGroups = bundle.MaterialGroups.Count;
        int meshes = bundle.MaterialGroups.Sum(group => group.Meshes.Count);
        int textures = bundle.MaterialGroups.Sum(group => group.Frames.Count);
        int sequences = bundle.MaterialGroups.Count(group => group.Frames.Count > 1);
        int vertices = bundle.MaterialGroups
            .SelectMany(group => group.Meshes)
            .Sum(mesh => mesh.Positions.Length);
        int triangles = bundle.MaterialGroups
            .SelectMany(group => group.Meshes)
            .Sum(mesh => mesh.TriangleIndices.Length / 3);
        int rigidSlot = DefaultRigidBoneSlot;
        string rigidName = string.Empty;
        var messages = new List<string>();
        try
        {
            ValidateBundleInput(bundle);
            BuildContext context = BuildContext.Create(target);
            rigidSlot = context.HeadSlot;
            rigidName = context.HeadName;
            ValidateTargetTemplates(target, context);
            foreach (RigidMaterialGroup group in bundle.MaterialGroups)
                ValidateGroupInput(group, validateTexturePayloads: true);

            int upscaled = bundle.MaterialGroups
                .SelectMany(group => group.Frames)
                .Count(frame => frame.WasUpscaled);
            messages.Add(
                $"Ready: {materialGroups} material groups, {meshes} meshes, " +
                $"{textures} textures and {sequences} texture sequences.");
            messages.Add($"Rigid binding: palette slot {rigidSlot} ({rigidName}).");
            if (upscaled > 0)
                messages.Add($"{upscaled} texture(s) were enlarged to power-of-two dimensions.");
            return new SmoRigidMultiMaterialPackAnalysis(
                true, materialGroups, meshes, textures, sequences, vertices,
                triangles, rigidSlot, rigidName, messages.AsReadOnly());
        }
        catch (Exception exception)
        {
            messages.Add(exception.Message);
            return new SmoRigidMultiMaterialPackAnalysis(
                false, materialGroups, meshes, textures, sequences, vertices,
                triangles, rigidSlot, rigidName, messages.AsReadOnly());
        }
    }

    public static SmoRigidMultiMaterialPackResult Pack(
        SmoDocument target,
        RigidGlbTextureBundle bundle,
        ReplacementTransform transform,
        string outputPath,
        bool includeTextureSequences = true)
    {
        ArgumentNullException.ThrowIfNull(target);
        ArgumentNullException.ThrowIfNull(bundle);
        ArgumentNullException.ThrowIfNull(transform);
        ArgumentException.ThrowIfNullOrWhiteSpace(outputPath);
        ValidateBundleInput(bundle);

        BuildContext context = BuildContext.Create(target);
        ValidateTargetTemplates(target, context);
        var allocator = new ObjectIdAllocator(target.Objects.Select(entry => entry.Id));
        var attachments = new List<SmoVisualForestAttachment>(bundle.MaterialGroups.Count);
        var packedTextures = new List<SmoRigidPackedTexture>();
        int vertices = 0;
        int triangles = 0;
        int sequences = 0;

        foreach (RigidMaterialGroup group in bundle.MaterialGroups.OrderBy(item => item.MaterialNumber))
        {
            BuiltSkinAttachment built = BuildGroupAttachment(
                target,
                context,
                group,
                transform,
                allocator,
                includeTextureSequences);
            attachments.Add(built.Attachment);
            packedTextures.AddRange(built.Textures);
            vertices = checked(vertices + built.VertexCount);
            triangles = checked(triangles + built.TriangleCount);
            sequences += built.SequenceCount;
        }

        byte[] disabled = SmoVisualTransplanter.DisableAllTargetMeshes(target);
        SmoDocument disabledTarget = SmoDocument.Parse(disabled, target.SourcePath);
        byte[] output = SmoVisualForestInjector.Inject(
            disabledTarget,
            context.Render.Id,
            attachments);

        string fullOutput = Path.GetFullPath(outputPath);
        string? directory = Path.GetDirectoryName(fullOutput);
        if (string.IsNullOrWhiteSpace(directory))
            throw new InvalidOperationException("Could not resolve the output directory.");
        Directory.CreateDirectory(directory);
        string temporary = Path.Combine(
            directory, $".{Path.GetFileName(fullOutput)}.{Guid.NewGuid():N}.tmp");
        try
        {
            File.WriteAllBytes(temporary, output);
            VerifyOutput(
                target,
                temporary,
                context,
                attachments,
                packedTextures,
                bundle.MaterialGroups,
                triangles,
                includeTextureSequences);
            File.Move(temporary, fullOutput, overwrite: true);
        }
        finally
        {
            if (File.Exists(temporary))
                File.Delete(temporary);
        }

        return new SmoRigidMultiMaterialPackResult(
            fullOutput,
            bundle.MaterialGroups.Count,
            bundle.MaterialGroups.Count,
            packedTextures.Count,
            sequences,
            vertices,
            triangles,
            context.HeadSlot,
            context.HeadName,
            output.LongLength,
            Convert.ToHexString(SHA256.HashData(output)),
            packedTextures.AsReadOnly());
    }

    private static void ValidateBundleInput(RigidGlbTextureBundle bundle)
    {
        if (bundle.MaterialGroups.Count == 0)
            throw new InvalidOperationException("Rigid GLB texture bundle has no material groups.");
        int[] duplicateNumbers = bundle.MaterialGroups
            .GroupBy(group => group.MaterialNumber)
            .Where(group => group.Count() > 1)
            .Select(group => group.Key)
            .ToArray();
        if (duplicateNumbers.Length > 0)
        {
            throw new InvalidDataException(
                "Rigid GLB texture bundle has duplicate material numbers: " +
                string.Join(", ", duplicateNumbers) + ".");
        }
    }

    private static void ValidateTargetTemplates(SmoDocument target, BuildContext context)
    {
        SmoObjectEntry textureEntry = FindInlineChild(
            target, context.OpaqueMaterial, SmoClassIds.TextureData);
        if (!SmoTextureDecoder.TryDecode(
                target, textureEntry, out SmoTexture? texture, out string textureError) ||
            texture is null)
            throw new InvalidOperationException(textureError);
        if (texture.FormatCode is not (0x32E3 or 0x43E3) ||
            texture.SourceLayout != SmoTextureLayout.Bgra)
        {
            throw new NotSupportedException(
                $"Primary texture [{textureEntry.Index}] is not writable BGRA 0x32E3/0x43E3.");
        }
        ReadOnlySpan<byte> textureBytes = ObjectBytes(target, textureEntry);
        int pixelSize = checked(texture.Width * texture.Height * 4);
        if (textureBytes.Length < SerializedTexturePixelOffset + pixelSize ||
            textureBytes[SerializedTextureMarkerOffset] != 0)
        {
            throw new InvalidDataException(
                $"Primary texture [{textureEntry.Index}] has an unsupported serialized layout.");
        }

        SmoMesh mesh = SmoMeshDecoder.Decode(target, context.Mesh);
        if (mesh.Marker != SmoMeshDecoder.E1Marker ||
            !SmoVertexLayoutRegistry.TryGet(mesh.VertexFormat, out SmoVertexLayout? layout) ||
            layout is null || layout.SerializedStride != mesh.Stride ||
            layout.BlendWeightsOffset is null || layout.BlendIndicesOffset is null)
        {
            throw new InvalidOperationException(
                $"Primary mesh [{context.Mesh.Index}] is not a writable skinned E1 layout.");
        }
        Matrix4x4 world = SmoNodeTransformDecoder.ResolveModelWorldMatrix(target, context.Mesh);
        if (!Matrix4x4.Invert(world, out _))
        {
            throw new InvalidOperationException(
                $"Primary mesh [{context.Mesh.Index}] has a singular world transform.");
        }
    }

    private static void ValidateGroupInput(
        RigidMaterialGroup group,
        bool validateTexturePayloads)
    {
        if (group.Meshes.Count == 0)
            throw new InvalidDataException($"Material {group.Name} has no meshes.");
        if (group.Frames.Count == 0)
            throw new InvalidDataException($"Material {group.Name} has no texture frames.");
        int[] frameNumbers = group.Frames.Select(frame => frame.FrameNumber).ToArray();
        if (!frameNumbers.SequenceEqual(Enumerable.Range(0, frameNumbers.Length)))
        {
            throw new InvalidDataException(
                $"Material {group.Name} texture frames are not contiguous from zero.");
        }

        ImportedMesh combined = ImportedMeshCombiner.Combine(
            new ImportedScene(group.Meshes), group.Name);
        if (combined.Positions.Length is < 1 or > ushort.MaxValue ||
            combined.TriangleIndices.Length < 3 ||
            combined.TriangleIndices.Length % 3 != 0)
        {
            throw new InvalidDataException(
                $"Material {group.Name} exceeds UInt16 mesh limits or has incomplete triangles.");
        }
        if (combined.Positions.Any(position =>
                !float.IsFinite(position.X) ||
                !float.IsFinite(position.Y) ||
                !float.IsFinite(position.Z)))
            throw new InvalidDataException($"Material {group.Name} contains a non-finite vertex.");
        if (combined.TextureCoordinates.Length != combined.Positions.Length)
            throw new InvalidDataException(
                $"Material {group.Name} has no complete TEXCOORD_0 stream.");
        if (combined.TextureCoordinates.Any(uv =>
                !float.IsFinite(uv.X) || !float.IsFinite(uv.Y)))
            throw new InvalidDataException(
                $"Material {group.Name} contains a non-finite texture coordinate.");

        foreach (RigidTextureFrame frame in group.Frames)
        {
            ImportedTexture texture = frame.Texture;
            if (!IsPowerOfTwo(texture.Width) || !IsPowerOfTwo(texture.Height) ||
                texture.Width is < 1 or > RigidGlbTextureBundleReader.AbsoluteMaximumTextureDimension ||
                texture.Height is < 1 or > RigidGlbTextureBundleReader.AbsoluteMaximumTextureDimension)
            {
                throw new InvalidDataException(
                    $"Texture {texture.Name} has unsupported dimensions " +
                    $"{texture.Width}x{texture.Height}.");
            }
            if (!validateTexturePayloads)
                continue;
            using Image<Rgba32> image = Image.Load<Rgba32>(texture.Data);
            if (image.Width != texture.Width || image.Height != texture.Height)
            {
                throw new InvalidDataException(
                    $"Texture {texture.Name} declares {texture.Width}x{texture.Height}, " +
                    $"but its image is {image.Width}x{image.Height}.");
            }
        }
    }

    private static BuiltSkinAttachment BuildGroupAttachment(
        SmoDocument target,
        BuildContext context,
        RigidMaterialGroup group,
        ReplacementTransform transform,
        ObjectIdAllocator allocator,
        bool includeTextureSequences)
    {
        ValidateGroupInput(group, validateTexturePayloads: false);

        int effectiveFrameCount = includeTextureSequences ? group.Frames.Count : 1;
        uint skinId = allocator.Take();
        uint materialId = allocator.Take();
        uint[] textureIds = Enumerable.Range(0, effectiveFrameCount)
            .Select(_ => allocator.Take()).ToArray();
        uint? sequenceId = includeTextureSequences && group.Frames.Count > 1
            ? allocator.Take()
            : null;
        uint meshId = allocator.Take();

        bool usesAlpha = group.Frames.Any(frame => TextureHasTransparency(frame.Texture));
        SmoObjectEntry materialTemplate = usesAlpha
            ? context.AlphaMaterial
            : context.OpaqueMaterial;
        SmoObjectEntry textureTemplate = FindInlineChild(
            target, materialTemplate, SmoClassIds.TextureData);

        BuiltObject[] textures = new BuiltObject[effectiveFrameCount];
        var textureResults = new List<SmoRigidPackedTexture>(effectiveFrameCount);
        for (int frameIndex = 0; frameIndex < effectiveFrameCount; frameIndex++)
        {
            RigidTextureFrame frame = group.Frames[frameIndex];
            string objectName = TextureObjectName(group.MaterialNumber, frameIndex);
            textures[frameIndex] = BuildTextureObject(
                target,
                textureTemplate,
                textureIds[frameIndex],
                objectName,
                frame.Texture);
            textureResults.Add(new SmoRigidPackedTexture(
                group.MaterialNumber,
                frame.FrameNumber,
                frame.SourcePath,
                textureIds[frameIndex],
                objectName,
                frame.Texture.Width,
                frame.Texture.Height,
                TextureHasTransparency(frame.Texture)));
        }

        BuiltObject? sequence = null;
        if (sequenceId.HasValue)
        {
            sequence = BuildTextureSequenceObject(
                sequenceId.Value,
                $"layla_mat{group.MaterialNumber}_sequence",
                textures);
        }

        BuiltObject material = BuildMaterialObject(
            target,
            materialTemplate,
            materialId,
            $"layla_mat{group.MaterialNumber}_material",
            textures[0],
            sequence,
            usesAlpha);

        ImportedMesh combined = ImportedMeshCombiner.Combine(
            new ImportedScene(group.Meshes),
            $"layla_mat{group.MaterialNumber}");
        BuiltObject mesh = BuildMeshObject(
            target,
            context.Mesh,
            meshId,
            $"layla_mat{group.MaterialNumber}_mesh",
            combined,
            transform,
            context.HeadSlot);
        BuiltObject skin = BuildSkinObject(
            target,
            context,
            skinId,
            $"layla_mat{group.MaterialNumber}_skin",
            material,
            mesh);
        SmoVisualForestAttachment attachment = WrapSkinAttachment(
            target,
            context,
            skin);
        return new BuiltSkinAttachment(
            attachment,
            textureResults,
            combined.Positions.Length,
            combined.TriangleIndices.Length / 3,
            sequence is null ? 0 : 1);
    }

    private static BuiltObject BuildTextureObject(
        SmoDocument document,
        SmoObjectEntry templateEntry,
        uint id,
        string name,
        ImportedTexture imported)
    {
        if (!SmoTextureDecoder.TryDecode(
                document, templateEntry, out SmoTexture? template, out string textureError) ||
            template is null)
            throw new InvalidDataException(textureError);
        if (template.FormatCode is not (0x32E3 or 0x43E3) ||
            template.SourceLayout != SmoTextureLayout.Bgra)
        {
            throw new NotSupportedException(
                $"Texture template [{templateEntry.Index}] must be BGRA 0x32E3/0x43E3.");
        }
        if (!IsPowerOfTwo(imported.Width) || !IsPowerOfTwo(imported.Height) ||
            imported.Width is < 1 or > RigidGlbTextureBundleReader.AbsoluteMaximumTextureDimension ||
            imported.Height is < 1 or > RigidGlbTextureBundleReader.AbsoluteMaximumTextureDimension)
        {
            throw new InvalidDataException(
                $"Texture {imported.Name} has unsupported dimensions {imported.Width}x{imported.Height}.");
        }

        using Image<Rgba32> image = Image.Load<Rgba32>(imported.Data);
        if (image.Width != imported.Width || image.Height != imported.Height)
        {
            throw new InvalidDataException(
                $"Texture {imported.Name} declares {imported.Width}x{imported.Height}, " +
                $"but its image is {image.Width}x{image.Height}.");
        }
        byte[] pixels = EncodeBgra(image);
        ReadOnlySpan<byte> source = ObjectBytes(document, templateEntry);
        int oldPixelSize = checked(template.Width * template.Height * 4);
        if (source.Length < SerializedTexturePixelOffset + oldPixelSize ||
            source[SerializedTextureMarkerOffset] != 0)
        {
            throw new InvalidDataException(
                $"Texture template [{templateEntry.Index}] has an unsupported serialized layout.");
        }
        int oldPixelEnd = checked(SerializedTexturePixelOffset + oldPixelSize);
        byte[] result = new byte[checked(
            source.Length - oldPixelSize + pixels.Length)];
        source[..SerializedTexturePixelOffset].CopyTo(result);
        pixels.CopyTo(result.AsSpan(SerializedTexturePixelOffset));
        source[oldPixelEnd..].CopyTo(
            result.AsSpan(SerializedTexturePixelOffset + pixels.Length));

        PatchTextureHeader(
            result,
            oldPixelSize,
            pixels.Length,
            imported.Width,
            imported.Height);
        if (result[SerializedTextureMarkerOffset] != 0)
            throw new InvalidDataException("Texture serializer marker was modified.");
        return BuiltObject.CreateRoot(
            id,
            RawName(name),
            SmoClassIds.TextureData,
            result);
    }

    private static BuiltObject BuildTextureSequenceObject(
        uint id,
        string name,
        IReadOnlyList<BuiltObject> textures)
    {
        if (textures.Count < 2)
            throw new ArgumentException("A texture sequence needs at least two frames.", nameof(textures));
        int[] schedule = BuildPingPongTextureSchedule(textures.Count);
        int payloadSize = checked(
            sizeof(uint) + schedule.Length * sizeof(float) +
            schedule.Length * ObjectReferenceSize +
            textures.Skip(1).Sum(texture => texture.Data.Length));
        using var stream = new MemoryStream(checked(
            ObjectSignatureSize + 5 + payloadSize + 1));
        WriteObjectSignature(stream, TextureSequenceClassId);
        WriteUInt32FieldHeader(stream, fieldType: 0, checked((uint)payloadSize));
        WriteUInt32(stream, checked((uint)schedule.Length));
        float frameStep = BitConverter.Int32BitsToSingle(SequenceFrameStepBits);
        float keyTime = 0;
        for (int index = 0; index < schedule.Length; index++)
        {
            keyTime += frameStep;
            WriteSingle(stream, keyTime);
        }

        var placements = new List<ObjectPlacement>
        {
            new(id, RawName(name), TextureSequenceClassId, 0, 0)
        };
        var definedInline = new HashSet<int> { 0 };
        foreach (int textureIndex in schedule)
        {
            BuiltObject texture = textures[textureIndex];
            WriteUInt32(stream, texture.Root.Id);
            if (!definedInline.Add(textureIndex))
            {
                WriteUInt32(stream, 0);
                continue;
            }
            WriteUInt32(stream, checked((uint)texture.Data.Length));
            int textureOffset = checked((int)stream.Position);
            stream.Write(texture.Data);
            placements.AddRange(texture.Placements.Select(placement => placement with
            {
                RelativeOffset = checked(textureOffset + placement.RelativeOffset)
            }));
        }
        stream.WriteByte(0);
        byte[] data = stream.ToArray();
        placements[0] = placements[0] with { SerializedSize = checked((uint)data.Length) };
        return new BuiltObject(data, placements);
    }

    private static int[] BuildPingPongTextureSchedule(int textureCount)
    {
        if (textureCount < 2)
            throw new ArgumentOutOfRangeException(nameof(textureCount));

        // Native Bloom/Stella sequences hold both turning points for three
        // keys and every intermediate frame for two.  This is the exact
        // generalized 4*N-2-key ping-pong graph used by the game serializer.
        var result = new List<int>(checked(4 * textureCount - 2));
        result.AddRange(Enumerable.Repeat(0, 3));
        for (int index = 1; index < textureCount - 1; index++)
            result.AddRange(Enumerable.Repeat(index, 2));
        result.AddRange(Enumerable.Repeat(textureCount - 1, 3));
        for (int index = textureCount - 2; index >= 1; index--)
            result.AddRange(Enumerable.Repeat(index, 2));

        if (result.Count != 4 * textureCount - 2)
            throw new InvalidOperationException("Texture sequence schedule size mismatch.");
        return result.ToArray();
    }

    private static BuiltObject BuildMaterialObject(
        SmoDocument document,
        SmoObjectEntry templateEntry,
        uint id,
        string name,
        BuiltObject firstTexture,
        BuiltObject? sequence,
        bool usesAlpha)
    {
        ReadOnlySpan<byte> source = ObjectBytes(document, templateEntry);
        SmoObjectEntry templateTexture = FindInlineChild(
            document, templateEntry, SmoClassIds.TextureData);
        SmoDataBlockHeader textureField = FindInlineChildField(
            document, templateEntry, templateTexture);
        using var stream = new MemoryStream(checked(
            source.Length - (int)templateTexture.SerializedSize +
            firstTexture.Data.Length + (sequence?.Data.Length ?? 0) + 16));
        stream.Write(source[..ObjectSignatureSize]);
        var placements = new List<ObjectPlacement>
        {
            new(id, RawName(name), SmoClassIds.MaterialData, 0, 0)
        };
        int offset = ObjectSignatureSize;
        while (offset < source.Length &&
               SmoDataBlockReader.TryReadHeader(source, offset, out SmoDataBlockHeader field))
        {
            if (field.Offset == textureField.Offset)
            {
                WriteResizedInlineField(stream, source, field, firstTexture, placements);
                if (sequence is not null)
                    WriteInlineField(stream, fieldType: 11, sequence, placements);
            }
            else
            {
                byte[] raw = source.Slice(
                    field.Offset,
                    checked((int)field.PayloadEnd - field.Offset)).ToArray();
                if (usesAlpha && field.FieldType == 3 &&
                    field.PayloadSize == sizeof(uint))
                {
                    uint flags = BinaryPrimitives.ReadUInt32LittleEndian(
                        raw.AsSpan(field.HeaderSize));
                    BinaryPrimitives.WriteUInt32LittleEndian(
                        raw.AsSpan(field.HeaderSize), flags | AlphaBlendFlag);
                }
                stream.Write(raw);
            }
            offset = checked((int)field.PayloadEnd);
        }
        if (offset != source.Length)
            throw new InvalidDataException(
                $"Material template [{templateEntry.Index}] has an invalid field stream.");
        byte[] data = stream.ToArray();
        placements[0] = placements[0] with { SerializedSize = checked((uint)data.Length) };
        return new BuiltObject(data, placements);
    }

    private static BuiltObject BuildMeshObject(
        SmoDocument document,
        SmoObjectEntry templateEntry,
        uint id,
        string name,
        ImportedMesh mesh,
        ReplacementTransform transform,
        int boneSlot)
    {
        SmoMesh template = SmoMeshDecoder.Decode(document, templateEntry);
        if (template.Marker != SmoMeshDecoder.E1Marker ||
            !SmoVertexLayoutRegistry.TryGet(template.VertexFormat, out SmoVertexLayout? layout) ||
            layout is null || layout.SerializedStride != template.Stride ||
            layout.BlendWeightsOffset is null || layout.BlendIndicesOffset is null)
        {
            throw new InvalidOperationException(
                $"Primary mesh [{templateEntry.Index}] is not a writable skinned E1 layout.");
        }
        int vertexCount = mesh.Positions.Length;
        int indexCount = mesh.TriangleIndices.Length;
        if (vertexCount is < 1 or > ushort.MaxValue || indexCount % 3 != 0)
            throw new InvalidDataException("Rigid material mesh exceeds UInt16 limits or has incomplete triangles.");
        foreach (uint index in mesh.TriangleIndices)
            if (index >= vertexCount || index > ushort.MaxValue)
                throw new InvalidDataException($"Rigid material mesh index {index} is outside its vertex array.");

        const int preambleSize = 17;
        const int primitiveHeaderSize = 12;
        const int vertexHeaderSize = 12;
        int indexBytes = checked(indexCount * sizeof(ushort));
        int vertexBytes = checked(vertexCount * template.Stride);
        int payloadSize = checked(
            preambleSize + primitiveHeaderSize + indexBytes + vertexHeaderSize + vertexBytes);
        byte[] result = new byte[checked(ObjectSignatureSize + 5 + payloadSize + 1)];
        WriteUInt32(result, 0, SmoClassIds.MeshData);
        "SBOO"u8.CopyTo(result.AsSpan(4));
        result[8] = SmoMeshDecoder.E1Marker;
        WriteUInt32(result, 9, checked((uint)payloadSize));
        int payload = 13;
        WriteUInt32(result, payload, template.VertexFormat);
        WriteUInt32(result, payload + 4, checked((uint)vertexCount));
        WriteUInt32(result, payload + 8, checked((uint)(vertexCount * template.RuntimeStride)));
        WriteUInt32(result, payload + 12, checked((uint)indexBytes));
        result[payload + 16] = 0;
        int primitive = payload + preambleSize;
        WriteUInt32(result, primitive, SmoMeshDecoder.TriangleListPrimitive);
        WriteUInt32(result, primitive + 4, checked((uint)(indexCount / 3)));
        WriteUInt32(result, primitive + 8, 0);
        int indices = primitive + primitiveHeaderSize;
        for (int triangle = 0; triangle < indexCount; triangle += 3)
        {
            WriteUInt16(result, indices + triangle * 2,
                checked((ushort)mesh.TriangleIndices[triangle]));
            WriteUInt16(result, indices + (triangle + 1) * 2,
                checked((ushort)mesh.TriangleIndices[triangle + 2]));
            WriteUInt16(result, indices + (triangle + 2) * 2,
                checked((ushort)mesh.TriangleIndices[triangle + 1]));
        }
        int vertexHeader = indices + indexBytes;
        WriteUInt32(result, vertexHeader, template.VertexFormat);
        WriteUInt32(result, vertexHeader + 4, checked((uint)vertexCount));
        WriteUInt32(result, vertexHeader + 8, 0);
        int vertices = vertexHeader + vertexHeaderSize;

        Matrix4x4 world = SmoNodeTransformDecoder.ResolveModelWorldMatrix(document, templateEntry);
        if (!Matrix4x4.Invert(world, out Matrix4x4 inverseWorld))
            throw new InvalidOperationException(
                $"Primary mesh [{templateEntry.Index}] has a singular world transform.");
        Matrix4x4 adjustment = transform.Matrix;
        Matrix4x4 adjustmentNormal = Matrix4x4.Invert(adjustment, out Matrix4x4 inverseAdjustment)
            ? Matrix4x4.Transpose(inverseAdjustment)
            : adjustment;
        Matrix4x4 worldToLocalNormal = Matrix4x4.Transpose(world);
        Vector3[] sourceNormals = HasUsableNormals(mesh.Normals, vertexCount)
            ? mesh.Normals
            : GenerateSmoothNormals(mesh);
        for (int vertex = 0; vertex < vertexCount; vertex++)
        {
            int record = checked(vertices + vertex * template.Stride);
            Vector3 adjusted = Vector3.Transform(mesh.Positions[vertex], adjustment);
            Vector3 local = Vector3.Transform(
                new Vector3(adjusted.X, adjusted.Y, -adjusted.Z), inverseWorld);
            WriteVector3(result, record, local);
            if (layout.NormalOffset is int normalOffset)
            {
                Vector3 normal = Vector3.TransformNormal(sourceNormals[vertex], adjustmentNormal);
                normal.Z = -normal.Z;
                normal = Vector3.TransformNormal(normal, worldToLocalNormal);
                if (normal.LengthSquared() > 0.000001f)
                    normal = Vector3.Normalize(normal);
                WriteVector3(result, record + normalOffset, normal);
            }
            if (layout.TextureCoordinate0Offset is int uvOffset &&
                mesh.TextureCoordinates.Length == vertexCount)
            {
                WriteSingle(result, record + uvOffset, mesh.TextureCoordinates[vertex].X);
                WriteSingle(result, record + uvOffset + sizeof(float), mesh.TextureCoordinates[vertex].Y);
            }
            if (layout.TextureCoordinate1Offset is int uv1Offset &&
                mesh.TextureCoordinates.Length == vertexCount)
            {
                WriteSingle(result, record + uv1Offset, mesh.TextureCoordinates[vertex].X);
                WriteSingle(result, record + uv1Offset + sizeof(float), mesh.TextureCoordinates[vertex].Y);
            }
            if (layout.DiffuseArgbOffset is int colorOffset)
            {
                uint color = mesh.DiffuseColors.Length == vertexCount
                    ? mesh.DiffuseColors[vertex]
                    : 0xFFFFFFFF;
                WriteUInt32(result, record + colorOffset, color);
            }
            WriteVector4(result, record + layout.BlendWeightsOffset.Value,
                new Vector4(1, 0, 0, 0));
            result[record + layout.BlendIndicesOffset.Value] = checked((byte)boneSlot);
        }
        return BuiltObject.CreateRoot(id, RawName(name), SmoClassIds.MeshData, result);
    }

    private static Vector3[] GenerateSmoothNormals(ImportedMesh mesh)
    {
        var normals = new Vector3[mesh.Positions.Length];
        for (int index = 0; index < mesh.TriangleIndices.Length; index += 3)
        {
            int first = checked((int)mesh.TriangleIndices[index]);
            int second = checked((int)mesh.TriangleIndices[index + 1]);
            int third = checked((int)mesh.TriangleIndices[index + 2]);
            Vector3 face = Vector3.Cross(
                mesh.Positions[second] - mesh.Positions[first],
                mesh.Positions[third] - mesh.Positions[first]);
            normals[first] += face;
            normals[second] += face;
            normals[third] += face;
        }
        for (int index = 0; index < normals.Length; index++)
            normals[index] = normals[index].LengthSquared() > 1e-20f
                ? Vector3.Normalize(normals[index])
                : Vector3.UnitY;
        return normals;
    }

    private static bool HasUsableNormals(
        IReadOnlyList<Vector3> normals,
        int vertexCount) =>
        normals.Count == vertexCount && normals.All(normal =>
            float.IsFinite(normal.X) &&
            float.IsFinite(normal.Y) &&
            float.IsFinite(normal.Z) &&
            normal.LengthSquared() > 1e-20f);

    private static BuiltObject BuildSkinObject(
        SmoDocument document,
        BuildContext context,
        uint id,
        string name,
        BuiltObject material,
        BuiltObject mesh)
    {
        ReadOnlySpan<byte> source = ObjectBytes(document, context.Skin);
        SmoDataBlockHeader materialField = FindInlineChildField(
            document, context.Skin, context.OpaqueMaterial);
        SmoDataBlockHeader helperField = FindInlineChildField(
            document, context.Skin, context.Helper);
        SmoDataBlockHeader meshField = FindInlineChildField(
            document, context.Skin, context.Mesh);
        SmoDataBlockHeader paletteField = FindPaletteField(source, context.Palette.Bones.Count);
        byte[] palette = BuildReferencePaletteField(
            source.Slice(paletteField.Offset, paletteField.HeaderSize),
            paletteField,
            context.Palette,
            document);
        using var stream = new MemoryStream(checked(
            source.Length - (int)context.OpaqueMaterial.SerializedSize -
            (int)context.Mesh.SerializedSize - (int)context.Helper.SerializedSize -
            context.Palette.Bones.Sum(bone => checked((int)bone.InlineSerializedSize)) +
            material.Data.Length + mesh.Data.Length));
        stream.Write(source[..ObjectSignatureSize]);
        var placements = new List<ObjectPlacement>
        {
            new(id, RawName(name), SmoClassIds.Skin, 0, 0)
        };
        int offset = ObjectSignatureSize;
        while (offset < source.Length &&
               SmoDataBlockReader.TryReadHeader(source, offset, out SmoDataBlockHeader field))
        {
            if (field.Offset == materialField.Offset)
            {
                WriteResizedInlineField(stream, source, field, material, placements);
            }
            else if (field.Offset == helperField.Offset)
            {
                WriteReferenceOnlyField(stream, source, field, context.Helper.Id);
            }
            else if (field.Offset == meshField.Offset)
            {
                WriteResizedInlineField(stream, source, field, mesh, placements);
            }
            else if (field.Offset == paletteField.Offset)
            {
                stream.Write(palette);
            }
            else
            {
                stream.Write(source.Slice(
                    field.Offset,
                    checked((int)field.PayloadEnd - field.Offset)));
            }
            offset = checked((int)field.PayloadEnd);
        }
        if (offset != source.Length)
            throw new InvalidDataException(
                $"Skin template [{context.Skin.Index}] has an invalid field stream.");
        byte[] data = stream.ToArray();
        placements[0] = placements[0] with { SerializedSize = checked((uint)data.Length) };
        return new BuiltObject(data, placements);
    }

    private static SmoVisualForestAttachment WrapSkinAttachment(
        SmoDocument document,
        BuildContext context,
        BuiltObject skin)
    {
        SmoDataBlockHeader wrapper = FindInlineChildField(
            document, context.Render, context.Skin);
        ReadOnlySpan<byte> renderBytes = ObjectBytes(document, context.Render);
        byte[] header = renderBytes.Slice(wrapper.Offset, wrapper.HeaderSize).ToArray();
        PatchPayloadSize(header, 0, wrapper, checked((uint)(ObjectReferenceSize + skin.Data.Length)));
        byte[] fieldData = new byte[checked(header.Length + ObjectReferenceSize + skin.Data.Length)];
        header.CopyTo(fieldData, 0);
        WriteUInt32(fieldData, header.Length, skin.Root.Id);
        WriteUInt32(fieldData, header.Length + sizeof(uint), checked((uint)skin.Data.Length));
        int rootOffset = checked(header.Length + ObjectReferenceSize);
        skin.Data.CopyTo(fieldData, rootOffset);
        SmoVisualForestEntry[] entries = skin.Placements.Select(placement =>
            new SmoVisualForestEntry(
                placement.Id,
                placement.RawName,
                placement.TypeHash,
                checked(rootOffset + placement.RelativeOffset),
                placement.SerializedSize)).ToArray();
        return new SmoVisualForestAttachment(context.Render.Id, fieldData, entries);
    }

    private static byte[] BuildReferencePaletteField(
        ReadOnlySpan<byte> originalHeader,
        SmoDataBlockHeader paletteField,
        SmoSkin palette,
        SmoDocument document)
    {
        if (paletteField.SizeKind != SmoDataBlockSizeCode.UInt32)
            throw new InvalidDataException("Skin palette does not use a writable UInt32 size.");
        int payloadSize = checked(8 + palette.Bones.Count * (ObjectReferenceSize + 16 * sizeof(float)));
        byte[] result = new byte[checked(originalHeader.Length + payloadSize)];
        originalHeader.CopyTo(result);
        WriteUInt32(result, originalHeader.Length - sizeof(uint), checked((uint)payloadSize));
        WriteUInt32(result, originalHeader.Length, 0);
        WriteUInt32(result, originalHeader.Length + sizeof(uint), checked((uint)palette.Bones.Count));
        int cursor = originalHeader.Length + 8;
        foreach (SmoSkinBone bone in palette.Bones)
        {
            SmoObjectEntry node = document.Objects[bone.NodeObjectIndex];
            WriteUInt32(result, cursor, node.Id);
            WriteUInt32(result, cursor + sizeof(uint), 0);
            WriteMatrix(result.AsSpan(cursor + ObjectReferenceSize), bone.InverseBindMatrix);
            cursor += ObjectReferenceSize + 16 * sizeof(float);
        }
        return result;
    }

    private static void VerifyOutput(
        SmoDocument target,
        string path,
        BuildContext context,
        IReadOnlyList<SmoVisualForestAttachment> attachments,
        IReadOnlyList<SmoRigidPackedTexture> packedTextures,
        IReadOnlyList<RigidMaterialGroup> groups,
        int expectedTriangles,
        bool includeTextureSequences)
    {
        SmoDocument output = SmoDocument.Load(path);
        var errors = new List<string>();
        if (output.HasErrors)
            errors.Add("strict parser reported errors");
        Dictionary<uint, SmoObjectEntry> outputById = output.Objects.ToDictionary(entry => entry.Id);
        foreach (SmoObjectEntry source in target.Objects)
        {
            if (!outputById.TryGetValue(source.Id, out SmoObjectEntry? retained) ||
                retained.TypeHash != source.TypeHash || retained.Name != source.Name)
            {
                errors.Add($"target object ID {source.Id} identity changed");
                continue;
            }
            uint? sourceParent = source.ParentIndex is int sourceParentIndex
                ? target.Objects[sourceParentIndex].Id
                : null;
            uint? retainedParent = retained.ParentIndex is int retainedParentIndex
                ? output.Objects[retainedParentIndex].Id
                : null;
            if (sourceParent != retainedParent)
                errors.Add($"target object ID {source.Id} parent changed");
        }

        HashSet<uint> addedIds = attachments
            .SelectMany(attachment => attachment.Entries)
            .Select(entry => entry.Id)
            .ToHashSet();
        if (addedIds.Count != attachments.Sum(attachment => attachment.Entries.Count))
            errors.Add("generated object IDs are not unique");
        if (packedTextures.Any(texture =>
                !outputById.TryGetValue(texture.ObjectId, out SmoObjectEntry? entry) ||
                entry.TypeHash != SmoClassIds.TextureData))
            errors.Add("one or more generated textures are absent from the catalog");
        foreach (SmoRigidPackedTexture packed in packedTextures)
        {
            if (!outputById.TryGetValue(packed.ObjectId, out SmoObjectEntry? entry) ||
                entry.TypeHash != SmoClassIds.TextureData)
                continue;
            ReadOnlySpan<byte> bytes = ObjectBytes(output, entry);
            uint header30 = BinaryPrimitives.ReadUInt32LittleEndian(bytes[0x30..]);
            uint header34 = BinaryPrimitives.ReadUInt32LittleEndian(bytes[0x34..]);
            uint header38 = BinaryPrimitives.ReadUInt32LittleEndian(bytes[0x38..]);
            if (header30 != (checked((uint)packed.Width << 8) | 1) ||
                header34 != checked((uint)packed.Width << 10) ||
                header38 != checked((uint)packed.Height << 8))
            {
                errors.Add(
                    $"generated texture ID {packed.ObjectId} has invalid native dimension metadata");
            }
        }

        SmoObjectEntry[] targetMeshes = target.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.MeshData)
            .ToArray();
        foreach (SmoObjectEntry targetMesh in targetMeshes)
        {
            SmoMesh disabled = SmoMeshDecoder.Decode(output, outputById[targetMesh.Id]);
            if (disabled.TriangleIndices.Length != 0)
                errors.Add($"target mesh ID {targetMesh.Id} is not disabled");
        }

        SmoObjectEntry[] addedMeshes = output.Objects.Where(entry =>
            addedIds.Contains(entry.Id) && entry.TypeHash == SmoClassIds.MeshData).ToArray();
        if (addedMeshes.Length != groups.Count)
            errors.Add($"generated mesh count {addedMeshes.Length} != groups {groups.Count}");
        int actualTriangles = 0;
        foreach (SmoObjectEntry meshEntry in addedMeshes)
        {
            SmoMesh mesh = SmoMeshDecoder.Decode(output, meshEntry);
            actualTriangles += mesh.TriangleCount;
            if (!mesh.BlendWeights.All(weight =>
                    weight.X == 1 && weight.Y == 0 && weight.Z == 0 && weight.W == 0) ||
                !mesh.BlendIndices.All(indices => indices.X == context.HeadSlot))
                errors.Add($"generated mesh ID {meshEntry.Id} is not rigid-bound to slot {context.HeadSlot}");
        }
        if (actualTriangles != expectedTriangles)
            errors.Add($"generated triangles {actualTriangles} != source {expectedTriangles}");

        int expectedSequences = includeTextureSequences
            ? groups.Count(group => group.Frames.Count > 1)
            : 0;
        int actualSequences = output.Objects.Count(entry =>
            addedIds.Contains(entry.Id) && entry.TypeHash == TextureSequenceClassId);
        if (actualSequences != expectedSequences)
            errors.Add($"generated sequence count {actualSequences} != expected {expectedSequences}");

        IReadOnlyDictionary<int, SmoTextureBinding> bindings =
            SmoTextureBindingResolver.ResolveAll(output);
        foreach (SmoObjectEntry meshEntry in addedMeshes)
        {
            if (!bindings.TryGetValue(meshEntry.Index, out SmoTextureBinding? binding) ||
                binding.Texture is null)
                errors.Add($"generated mesh ID {meshEntry.Id} has no texture binding");
        }
        foreach (SmoObjectEntry material in output.Objects.Where(entry =>
                     addedIds.Contains(entry.Id) && entry.TypeHash == SmoClassIds.MaterialData))
        {
            if (!SmoMaterialRenderState.TryDecodeFlags(output, material, out uint flags))
                errors.Add($"generated material ID {material.Id} has no render flags");
            bool expectedAlpha = output.Objects.Any(entry =>
                material.PhysicalOffset <= entry.PhysicalOffset &&
                entry.PhysicalEnd <= material.PhysicalEnd &&
                packedTextures.Any(texture => texture.ObjectId == entry.Id && texture.UsesAlpha));
            if (expectedAlpha != SmoMaterialRenderState.UsesAlphaBlend(flags))
                errors.Add($"generated material ID {material.Id} alpha-blend state is wrong");
        }

        if (errors.Count > 0)
            throw new InvalidDataException(
                "Synthetic rigid multi-material output failed verification: " +
                string.Join("; ", errors.Distinct()) + ".");
    }

    private static bool TextureHasTransparency(ImportedTexture texture)
    {
        using Image<Rgba32> image = Image.Load<Rgba32>(texture.Data);
        bool transparent = false;
        image.ProcessPixelRows(accessor =>
        {
            for (int y = 0; y < accessor.Height && !transparent; y++)
            {
                foreach (Rgba32 pixel in accessor.GetRowSpan(y))
                {
                    if (pixel.A < byte.MaxValue)
                    {
                        transparent = true;
                        break;
                    }
                }
            }
        });
        return transparent;
    }

    private static byte[] EncodeBgra(Image<Rgba32> image)
    {
        byte[] result = new byte[checked(image.Width * image.Height * 4)];
        int offset = 0;
        image.ProcessPixelRows(accessor =>
        {
            for (int y = 0; y < image.Height; y++)
            {
                foreach (Rgba32 pixel in accessor.GetRowSpan(y))
                {
                    result[offset] = pixel.B;
                    result[offset + 1] = pixel.G;
                    result[offset + 2] = pixel.R;
                    result[offset + 3] = pixel.A;
                    offset += 4;
                }
            }
        });
        return result;
    }

    private static void PatchTextureHeader(
        Span<byte> data,
        int oldPixelSize,
        int newPixelSize,
        int width,
        int height)
    {
        int delta = checked(newPixelSize - oldPixelSize);
        AddUInt32(data, 0x09, delta);
        AddUInt32(data, 0x1A, delta);
        AddUInt32(data, 0x1F, delta);
        WriteUInt32(data, 0x24, checked((uint)width));
        WriteUInt32(data, 0x28, checked((uint)height));
        WriteUInt32(data, 0x2C, 0);
        WriteUInt32(data, 0x30, checked(((uint)width << 8) | 1));
        WriteUInt32(data, 0x34, checked((uint)width << 10));
        // Confirmed on pristine rectangular 0x32E3 textures: +0x30 and
        // +0x34 are width-derived, while +0x38 is height-derived.  Using
        // width here happens to work for square images but makes the native
        // loader over-read every rectangular pixel buffer.
        WriteUInt32(data, 0x38, checked((uint)height << 8));
    }

    private static void AddUInt32(Span<byte> data, int offset, int delta)
    {
        long value = checked((long)BinaryPrimitives.ReadUInt32LittleEndian(data[offset..]) + delta);
        BinaryPrimitives.WriteUInt32LittleEndian(data[offset..], checked((uint)value));
    }

    private static SmoDataBlockHeader FindInlineChildField(
        SmoDocument document,
        SmoObjectEntry owner,
        SmoObjectEntry child)
    {
        ReadOnlySpan<byte> source = ObjectBytes(document, owner);
        int offset = ObjectSignatureSize;
        while (offset < source.Length &&
               SmoDataBlockReader.TryReadHeader(source, offset, out SmoDataBlockHeader field))
        {
            if (field.PayloadSize == child.SerializedSize + ObjectReferenceSize &&
                BinaryPrimitives.ReadUInt32LittleEndian(source[field.PayloadOffset..]) == child.Id &&
                BinaryPrimitives.ReadUInt32LittleEndian(source[(field.PayloadOffset + 4)..]) ==
                    child.SerializedSize &&
                owner.PhysicalOffset + field.PayloadOffset + ObjectReferenceSize == child.PhysicalOffset)
                return field;
            offset = checked((int)field.PayloadEnd);
        }
        throw new InvalidDataException(
            $"Object [{child.Index}] is not an inline field of [{owner.Index}].");
    }

    private static SmoDataBlockHeader FindPaletteField(
        ReadOnlySpan<byte> serialized,
        int expectedBoneCount)
    {
        int offset = ObjectSignatureSize;
        while (offset < serialized.Length &&
               SmoDataBlockReader.TryReadHeader(serialized, offset, out SmoDataBlockHeader field))
        {
            if (field.FieldType == 0 && field.PayloadSize >= 8)
            {
                ReadOnlySpan<byte> payload = serialized.Slice(
                    field.PayloadOffset, checked((int)field.PayloadSize));
                if (BinaryPrimitives.ReadUInt32LittleEndian(payload) == 0 &&
                    BinaryPrimitives.ReadUInt32LittleEndian(payload[4..]) == expectedBoneCount)
                    return field;
            }
            offset = checked((int)field.PayloadEnd);
        }
        throw new InvalidDataException("Skin palette field was not found.");
    }

    private static SmoObjectEntry FindInlineChild(
        SmoDocument document,
        SmoObjectEntry owner,
        uint typeHash)
    {
        SmoObjectEntry[] matches = document.Objects.Where(entry =>
            entry.ParentIndex == owner.Index && entry.TypeHash == typeHash).ToArray();
        if (matches.Length != 1)
            throw new InvalidDataException(
                $"Object [{owner.Index}] has {matches.Length} direct children of class 0x{typeHash:X8}.");
        _ = FindInlineChildField(document, owner, matches[0]);
        return matches[0];
    }

    private static void WriteResizedInlineField(
        MemoryStream stream,
        ReadOnlySpan<byte> source,
        SmoDataBlockHeader templateField,
        BuiltObject child,
        ICollection<ObjectPlacement> placements)
    {
        byte[] header = source.Slice(templateField.Offset, templateField.HeaderSize).ToArray();
        PatchPayloadSize(header, 0, templateField,
            checked((uint)(ObjectReferenceSize + child.Data.Length)));
        stream.Write(header);
        WriteUInt32(stream, child.Root.Id);
        WriteUInt32(stream, checked((uint)child.Data.Length));
        int childOffset = checked((int)stream.Position);
        stream.Write(child.Data);
        foreach (ObjectPlacement placement in child.Placements)
            placements.Add(placement with
            {
                RelativeOffset = checked(childOffset + placement.RelativeOffset)
            });
    }

    private static void WriteInlineField(
        MemoryStream stream,
        int fieldType,
        BuiltObject child,
        ICollection<ObjectPlacement> placements)
    {
        WriteUInt32FieldHeader(stream, fieldType,
            checked((uint)(ObjectReferenceSize + child.Data.Length)));
        WriteUInt32(stream, child.Root.Id);
        WriteUInt32(stream, checked((uint)child.Data.Length));
        int childOffset = checked((int)stream.Position);
        stream.Write(child.Data);
        foreach (ObjectPlacement placement in child.Placements)
            placements.Add(placement with
            {
                RelativeOffset = checked(childOffset + placement.RelativeOffset)
            });
    }

    private static void WriteReferenceOnlyField(
        MemoryStream stream,
        ReadOnlySpan<byte> source,
        SmoDataBlockHeader templateField,
        uint objectId)
    {
        byte[] header = source.Slice(templateField.Offset, templateField.HeaderSize).ToArray();
        PatchPayloadSize(header, 0, templateField, ObjectReferenceSize);
        stream.Write(header);
        WriteUInt32(stream, objectId);
        WriteUInt32(stream, 0);
    }

    private static void WriteUInt32FieldHeader(
        MemoryStream stream,
        int fieldType,
        uint payloadSize)
    {
        if ((uint)fieldType >= 0x1F)
            throw new ArgumentOutOfRangeException(nameof(fieldType));
        stream.WriteByte(checked((byte)(0xE0 | fieldType)));
        WriteUInt32(stream, payloadSize);
    }

    private static void PatchPayloadSize(
        Span<byte> header,
        int headerOffset,
        SmoDataBlockHeader template,
        uint payloadSize)
    {
        if (template.SizeKind != SmoDataBlockSizeCode.UInt32 ||
            template.HeaderSize < sizeof(uint) + 1)
            throw new InvalidDataException("Generated inline field needs a UInt32 size header.");
        WriteUInt32(header, headerOffset + template.HeaderSize - sizeof(uint), payloadSize);
    }

    private static void WriteObjectSignature(Stream stream, uint classId)
    {
        WriteUInt32(stream, classId);
        stream.Write("SBOO"u8);
    }

    private static ReadOnlySpan<byte> ObjectBytes(
        SmoDocument document,
        SmoObjectEntry entry) => document.Data.Span.Slice(
            checked((int)entry.PhysicalOffset),
            checked((int)entry.SerializedSize));

    private static byte[] RawName(string value) =>
        Encoding.UTF8.GetBytes(value + '\0');

    private static string TextureObjectName(int materialNumber, int frameIndex) =>
        $"layla_mat{materialNumber}_{frameIndex + 1:D4}";

    private static bool IsPowerOfTwo(int value) =>
        value > 0 && (value & (value - 1)) == 0;

    private static void WriteMatrix(Span<byte> data, Matrix4x4 value)
    {
        float[] values =
        [
            value.M11, value.M12, value.M13, value.M14,
            value.M21, value.M22, value.M23, value.M24,
            value.M31, value.M32, value.M33, value.M34,
            value.M41, value.M42, value.M43, value.M44
        ];
        for (int index = 0; index < values.Length; index++)
            WriteSingle(data, index * sizeof(float), values[index]);
    }

    private static void WriteVector3(Span<byte> data, int offset, Vector3 value)
    {
        WriteSingle(data, offset, value.X);
        WriteSingle(data, offset + sizeof(float), value.Y);
        WriteSingle(data, offset + 2 * sizeof(float), value.Z);
    }

    private static void WriteVector4(Span<byte> data, int offset, Vector4 value)
    {
        WriteSingle(data, offset, value.X);
        WriteSingle(data, offset + sizeof(float), value.Y);
        WriteSingle(data, offset + 2 * sizeof(float), value.Z);
        WriteSingle(data, offset + 3 * sizeof(float), value.W);
    }

    private static void WriteSingle(Span<byte> data, int offset, float value) =>
        BinaryPrimitives.WriteInt32LittleEndian(
            data[offset..], BitConverter.SingleToInt32Bits(value));

    private static void WriteSingle(Stream stream, float value) =>
        WriteUInt32(stream, unchecked((uint)BitConverter.SingleToInt32Bits(value)));

    private static void WriteUInt16(Span<byte> data, int offset, ushort value) =>
        BinaryPrimitives.WriteUInt16LittleEndian(data[offset..], value);

    private static void WriteUInt32(Span<byte> data, int offset, uint value) =>
        BinaryPrimitives.WriteUInt32LittleEndian(data[offset..], value);

    private static void WriteUInt32(Stream stream, uint value)
    {
        Span<byte> data = stackalloc byte[sizeof(uint)];
        BinaryPrimitives.WriteUInt32LittleEndian(data, value);
        stream.Write(data);
    }

    private sealed record ObjectPlacement(
        uint Id,
        byte[] RawName,
        uint TypeHash,
        int RelativeOffset,
        uint SerializedSize);

    private sealed record BuiltObject(
        byte[] Data,
        IReadOnlyList<ObjectPlacement> Placements)
    {
        public ObjectPlacement Root => Placements[0];

        public static BuiltObject CreateRoot(
            uint id,
            byte[] rawName,
            uint typeHash,
            byte[] data) => new(
                data,
                [new ObjectPlacement(id, rawName, typeHash, 0, checked((uint)data.Length))]);
    }

    private sealed record BuiltSkinAttachment(
        SmoVisualForestAttachment Attachment,
        IReadOnlyList<SmoRigidPackedTexture> Textures,
        int VertexCount,
        int TriangleCount,
        int SequenceCount);

    private sealed class ObjectIdAllocator
    {
        private uint _next;

        public ObjectIdAllocator(IEnumerable<uint> ids)
        {
            uint maximum = ids.DefaultIfEmpty().Max();
            _next = checked(maximum + 1);
        }

        public uint Take()
        {
            uint result = _next;
            _next = checked(_next + 1);
            return result;
        }
    }

    private sealed record BuildContext(
        SmoObjectEntry Render,
        SmoObjectEntry Skin,
        SmoObjectEntry OpaqueMaterial,
        SmoObjectEntry AlphaMaterial,
        SmoObjectEntry Helper,
        SmoObjectEntry Mesh,
        SmoSkin Palette,
        int HeadSlot,
        string HeadName)
    {
        public static BuildContext Create(SmoDocument target)
        {
            SmoObjectEntry render = target.Objects
                .Where(entry => entry.TypeHash == SmoClassIds.RenderNode)
                .OrderByDescending(entry => target.Objects.Count(candidate =>
                    candidate.TypeHash == SmoClassIds.MeshData && Contains(entry, candidate)))
                .ThenBy(entry => entry.Index)
                .FirstOrDefault() ?? throw new InvalidOperationException(
                    "Target contains no render node.");
            SmoObjectEntry skin = target.Objects
                .Where(entry => entry.ParentIndex == render.Index &&
                                entry.TypeHash == SmoClassIds.Skin)
                .OrderByDescending(entry => target.Objects
                    .Where(candidate => candidate.ParentIndex == entry.Index &&
                                        candidate.TypeHash == SmoClassIds.MeshData)
                    .Select(candidate => SmoMeshDecoder.Decode(target, candidate).VertexCount)
                    .DefaultIfEmpty()
                    .Max())
                .ThenBy(entry => entry.Index)
                .FirstOrDefault() ?? throw new InvalidOperationException(
                    "Primary render contains no skin.");
            SmoObjectEntry material = FindInlineChild(target, skin, SmoClassIds.MaterialData);
            SmoObjectEntry helper = FindInlineChild(target, skin, SharedVisualHelperClassId);
            SmoObjectEntry mesh = FindInlineChild(target, skin, SmoClassIds.MeshData);
            if (!SmoSkinDecoder.TryDecode(
                    target, skin, out SmoSkin? palette, out string error) || palette is null)
                throw new InvalidOperationException(error);
            if (palette.Bones.Count != ExpectedPaletteSize)
                throw new InvalidOperationException(
                    $"Primary palette has {palette.Bones.Count} bones, expected {ExpectedPaletteSize}.");
            if ((uint)DefaultRigidBoneSlot >= (uint)palette.Bones.Count)
                throw new InvalidOperationException("Primary palette has no slot 8.");
            SmoSkinBone head = palette.Bones[DefaultRigidBoneSlot];
            string headName = target.Objects[head.NodeObjectIndex].Name;
            if (!headName.Equals("Head", StringComparison.OrdinalIgnoreCase))
                throw new InvalidOperationException(
                    $"Primary palette slot 8 is '{headName}', not Head.");

            // Bloom's body/eye materials currently share the same opaque state.
            // Alpha is enabled explicitly on generated material copies.
            return new BuildContext(
                render,
                skin,
                material,
                material,
                helper,
                mesh,
                palette,
                DefaultRigidBoneSlot,
                headName);
        }

        private static bool Contains(SmoObjectEntry owner, SmoObjectEntry child) =>
            owner.PhysicalOffset <= child.PhysicalOffset &&
            child.PhysicalEnd <= owner.PhysicalEnd;
    }
}
