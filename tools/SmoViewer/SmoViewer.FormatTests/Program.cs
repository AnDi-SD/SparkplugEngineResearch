using System.Buffers.Binary;
using System.Numerics;
using System.Text;
using SmoViewer.Core;

namespace SmoViewer.FormatTests;

internal static class Program
{
    private static int _assertionCount;

    public static int Main(string[] args)
    {
        try
        {
            TestSyntheticDocument();
            TestDataBlockHeaders();
            TestSyntheticTextures();
            TestVertexColorUsage();

            CorpusOptions options = ParseCorpusOptions(args);
            string corpusPath = options.Path is not null
                ? Path.GetFullPath(options.Path)
                : Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", "..", "Samples"));

            if (Directory.Exists(corpusPath) || File.Exists(corpusPath))
                TestLocalCorpus(corpusPath, options.SampleCount, options.Seed);
            else
                Console.WriteLine($"Corpus not present; local sample checks skipped: {corpusPath}");

            Console.WriteLine($"PASS: {_assertionCount} assertions");
            return 0;
        }
        catch (Exception exception)
        {
            Console.Error.WriteLine($"FAIL: {exception.Message}");
            Console.Error.WriteLine(exception);
            return 1;
        }
    }

    private static void TestSyntheticDocument()
    {
        byte[] data = CreateSyntheticDocument();
        SmoDocument document = SmoDocument.Parse(data, "synthetic.smo");

        Equal("FFPS", document.Header.Signature, "synthetic signature");
        Equal((uint)data.Length, document.Header.FileSize, "synthetic declared length");
        Equal(1, document.Objects.Count, "synthetic object count");
        Equal("root", document.Objects[0].Name, "synthetic object name");
        Equal(SmoClassIds.Node, document.Objects[0].TypeHash, "synthetic class");
        True(document.Objects[0].SignatureMatches, "synthetic SBOO signature");
        True(document.Objects[0].IsWithinDataSection, "synthetic object bounds");
        Equal(0, document.Diagnostics.Count(
            item => item.Severity == SmoDiagnosticSeverity.Error),
            "synthetic errors");
    }

    private static byte[] CreateSyntheticDocument()
    {
        const int objectSize = sizeof(uint) + 4 + 1;
        byte[] body = new byte[objectSize];
        WriteUInt32(body, 0, SmoClassIds.Node);
        Encoding.ASCII.GetBytes("SBOO").CopyTo(body, sizeof(uint));
        body[^1] = 0;
        return CreateSingleObjectDocument("root", SmoClassIds.Node, body);
    }

    private static byte[] CreateSingleObjectDocument(
        string objectName,
        uint typeHash,
        byte[] body,
        int? serializedSize = null)
    {
        byte[] name = Encoding.ASCII.GetBytes(objectName + "\0");
        int tableLength = sizeof(uint) + sizeof(ushort) + name.Length + 3 * sizeof(uint);
        int dataStart = SmoHeader.Size + tableLength + sizeof(uint);
        int declaredObjectSize = serializedSize ?? body.Length;
        if (declaredObjectSize < 0 || declaredObjectSize > body.Length)
            throw new ArgumentOutOfRangeException(nameof(serializedSize));

        byte[] data = new byte[dataStart + body.Length];
        Encoding.ASCII.GetBytes("FFPS").CopyTo(data, 0);
        WriteUInt32(data, 0x04, 0x26);
        WriteUInt32(data, 0x08, 0);
        WriteUInt32(data, 0x0C, (uint)data.Length);
        WriteUInt32(data, 0x10, 2);
        WriteUInt32(data, 0x14, (uint)dataStart);
        WriteUInt32(data, 0x18, (uint)body.Length);
        WriteUInt32(data, 0x1C, 1);

        int offset = SmoHeader.ObjectTableOffset;
        WriteUInt32(data, offset, 1);
        offset += sizeof(uint);
        BinaryPrimitives.WriteUInt16LittleEndian(
            data.AsSpan(offset, sizeof(ushort)), (ushort)name.Length);
        offset += sizeof(ushort);
        name.CopyTo(data, offset);
        offset += name.Length;
        WriteUInt32(data, offset, typeHash);
        offset += sizeof(uint);
        WriteUInt32(data, offset, 0);
        offset += sizeof(uint);
        WriteUInt32(data, offset, (uint)declaredObjectSize);

        body.CopyTo(data, dataStart);
        return data;
    }

    private static byte[] CreateSyntheticTextureObject(ushort formatCode = 0x32E3)
    {
        const int width = 8;
        const int height = 8;
        const int pixelBytes = width * height * 4;
        bool hasPixelMarker = formatCode is 0x32E3 or 0x43E3;
        int pixelOffset = hasPixelMarker ? 0x3D : 0x34;
        uint outerTail = formatCode switch
        {
            0x32E3 => 0x32,
            0x43E3 => 0x43,
            0x29E3 => 0x29,
            _ => throw new ArgumentOutOfRangeException(nameof(formatCode))
        };
        int minimumSize = pixelOffset + pixelBytes + sizeof(uint);
        int outerBlockEnd = checked(0x0D + pixelBytes + (int)outerTail);
        byte[] body = new byte[Math.Max(minimumSize, outerBlockEnd)];

        WriteUInt32(body, 0, SmoClassIds.TextureData);
        Encoding.ASCII.GetBytes("SBOO").CopyTo(body, sizeof(uint));
        if (hasPixelMarker)
        {
            body[0x08] = 0xE3;
            WriteUInt32(body, 0x09, pixelBytes + outerTail);
            body[0x19] = 0xE1;
            WriteUInt32(body, 0x1A, pixelBytes + 0x20u);
            body[0x1E] = 0xE0;
            WriteUInt32(body, 0x1F, pixelBytes + 0x1Au);
            WriteUInt32(body, 0x24, width);
            WriteUInt32(body, 0x28, height);
            body[0x3C] = 0x00;
        }
        else
        {
            body[0x08] = 0xE3;
            WriteUInt32(body, 0x09, pixelBytes + 0x29u);
            body[0x10] = 0xE1;
            WriteUInt32(body, 0x11, pixelBytes + 0x20u);
            body[0x15] = 0xE0;
            WriteUInt32(body, 0x16, pixelBytes + 0x1Au);
            WriteUInt32(body, 0x1B, width);
            WriteUInt32(body, 0x1F, height);
            WriteUInt32(body, 0x28, width);
            WriteUInt32(body, 0x2C, width * 4u);
            WriteUInt32(body, 0x30, height);
        }

        for (int offset = pixelOffset; offset < pixelOffset + pixelBytes; offset += 4)
        {
            body[offset] = 0x33;
            body[offset + 1] = 0x22;
            body[offset + 2] = 0x11;
            body[offset + 3] = 0x44;
        }

        int lastPixelOffset = pixelOffset + pixelBytes - 4;
        body[lastPixelOffset] = 0xB6;
        body[lastPixelOffset + 1] = 0xC7;
        body[lastPixelOffset + 2] = 0xD8;
        body[lastPixelOffset + 3] = 0xA5;

        return body;
    }

    private static void TestDataBlockHeaders()
    {
        byte[] fixedFour = [0x63, 1, 2, 3, 4];
        True(
            SmoDataBlockReader.TryReadHeader(fixedFour, out SmoDataBlockHeader first),
            "fixed-size data block");
        Equal(3, first.FieldType, "fixed-size field type");
        Equal((uint)4, first.PayloadSize, "fixed-size payload");
        Equal(1, first.HeaderSize, "fixed-size header");

        byte[] extended = [0xDF, 0x2A, 0x03, 0x00, 9, 8, 7];
        True(
            SmoDataBlockReader.TryReadHeader(extended, out SmoDataBlockHeader second),
            "extended data block");
        Equal(0x2A, second.FieldType, "extended field type");
        Equal((uint)3, second.PayloadSize, "extended payload");
        Equal(4, second.HeaderSize, "extended header");
    }

    private static void TestSyntheticTextures()
    {
        (ushort FormatCode, string Name)[] cases =
        [
            (0x32E3, "BGRA 0x32E3"),
            (0x43E3, "BGRA 0x43E3"),
            (0x29E3, "BGRA 0x29E3")
        ];

        foreach ((ushort formatCode, string name) in cases)
        {
            byte[] body = CreateSyntheticTextureObject(formatCode);
            byte[] data = CreateSingleObjectDocument(
                $"texture_{formatCode:X4}",
                SmoClassIds.TextureData,
                body);
            SmoDocument document = SmoDocument.Parse(data, $"texture_{formatCode:X4}.smo");

            True(
                SmoTextureDecoder.TryDecode(
                    document,
                    document.Objects.Single(),
                    out SmoTexture? texture,
                    out string error),
                $"synthetic {name} texture decodes: {error}");
            Equal(8, texture!.Width, $"synthetic {name} width");
            Equal(8, texture.Height, $"synthetic {name} height");
            Equal(formatCode, texture.FormatCode, $"synthetic {name} format code");
            Equal(SmoTextureLayout.Bgra, texture.SourceLayout,
                $"synthetic {name} source layout");
            Equal(32, texture.PixelStride, $"synthetic {name} pixel stride");
            Equal(8 * 8 * 4, texture.Bgra32Pixels.Length, $"synthetic {name} size");

            ReadOnlySpan<byte> pixels = texture.Bgra32Pixels.Span;
            Equal((byte)0x33, pixels[0], $"synthetic {name} first blue");
            Equal((byte)0x22, pixels[1], $"synthetic {name} first green");
            Equal((byte)0x11, pixels[2], $"synthetic {name} first red");
            Equal((byte)0x44, pixels[3], $"synthetic {name} first alpha");

            if (formatCode is 0x32E3 or 0x43E3)
            {
                Equal((byte)0x00, body[0x3C], $"synthetic {name} serializer marker");
                True(pixels[3] != body[0x3C],
                    $"synthetic {name} serializer marker is not decoded as alpha");
                int last = pixels.Length - 4;
                Equal((byte)0xB6, pixels[last], $"synthetic {name} last blue");
                Equal((byte)0xC7, pixels[last + 1], $"synthetic {name} last green");
                Equal((byte)0xD8, pixels[last + 2], $"synthetic {name} last red");
                Equal((byte)0xA5, pixels[last + 3], $"synthetic {name} last alpha");
            }
        }

        byte[] invalidPixelMarkerBody =
            CreateSyntheticTextureObject();
        invalidPixelMarkerBody[0x3C] = 0x01;
        SmoDocument invalidPixelMarkerDocument = SmoDocument.Parse(
            CreateSingleObjectDocument(
                "invalid_pixel_marker",
                SmoClassIds.TextureData,
                invalidPixelMarkerBody));
        True(
            !SmoTextureDecoder.TryDecode(
                invalidPixelMarkerDocument,
                invalidPixelMarkerDocument.Objects.Single(),
                out _,
                out string invalidPixelMarkerError),
            "wrong 0x32E3 pixel serializer marker is rejected");
        True(
            invalidPixelMarkerError.StartsWith("TEXTURE_PIXEL_MARKER_MISMATCH:"),
            "wrong 0x32E3 pixel serializer marker has a stable diagnostic code");

        byte[] invalidMarkerBody = CreateSyntheticTextureObject();
        invalidMarkerBody[0x19] = 0xE0;
        SmoDocument invalidMarkerDocument = SmoDocument.Parse(
            CreateSingleObjectDocument(
                "invalid_marker",
                SmoClassIds.TextureData,
                invalidMarkerBody));
        True(
            !SmoTextureDecoder.TryDecode(
                invalidMarkerDocument,
                invalidMarkerDocument.Objects.Single(),
                out _,
                out string invalidMarkerError),
            "wrong nested texture marker is rejected");
        True(
            invalidMarkerError.StartsWith("TEXTURE_BLOCK_SIZE_MISMATCH:"),
            "wrong nested marker has a stable diagnostic code");

        byte[] oversizedBlockBody =
            CreateSyntheticTextureObject();
        WriteUInt32(oversizedBlockBody, 0x1A, uint.MaxValue);
        SmoDocument oversizedBlockDocument = SmoDocument.Parse(
            CreateSingleObjectDocument(
                "oversized_block",
                SmoClassIds.TextureData,
                oversizedBlockBody));
        True(
            !SmoTextureDecoder.TryDecode(
                oversizedBlockDocument,
                oversizedBlockDocument.Objects.Single(),
                out _,
                out string oversizedBlockError),
            "nested texture payload cannot escape the object interval");
        True(
            oversizedBlockError.StartsWith("TEXTURE_BLOCK_SIZE_MISMATCH:"),
            "oversized nested block has a stable diagnostic code");

        byte[] mismatchedDimensionsBody =
            CreateSyntheticTextureObject(0x29E3);
        WriteUInt32(mismatchedDimensionsBody, 0x1B, 4);
        SmoDocument mismatchedDimensionsDocument = SmoDocument.Parse(
            CreateSingleObjectDocument(
                "mismatched_dimensions",
                SmoClassIds.TextureData,
                mismatchedDimensionsBody));
        True(
            !SmoTextureDecoder.TryDecode(
                mismatchedDimensionsDocument,
                mismatchedDimensionsDocument.Objects.Single(),
                out _,
                out string mismatchedDimensionsError),
            "inconsistent BGRA dimensions are rejected");
        True(
            mismatchedDimensionsError.StartsWith("TEXTURE_DIMENSION_MISMATCH:"),
            "inconsistent BGRA dimensions have a stable diagnostic code");

        byte[] mismatchedStrideBody =
            CreateSyntheticTextureObject(0x29E3);
        WriteUInt32(mismatchedStrideBody, 0x2C, 4);
        SmoDocument mismatchedStrideDocument = SmoDocument.Parse(
            CreateSingleObjectDocument(
                "mismatched_stride",
                SmoClassIds.TextureData,
                mismatchedStrideBody));
        True(
            !SmoTextureDecoder.TryDecode(
                mismatchedStrideDocument,
                mismatchedStrideDocument.Objects.Single(),
                out _,
                out string mismatchedStrideError),
            "inconsistent BGRA row stride is rejected");
        True(
            mismatchedStrideError.StartsWith("TEXTURE_ROW_STRIDE_MISMATCH:"),
            "inconsistent BGRA row stride has a stable diagnostic code");

        byte[] unsupportedBody = CreateSyntheticTextureObject();
        unsupportedBody[0x09] = 0xFD;
        SmoDocument unsupportedDocument = SmoDocument.Parse(
            CreateSingleObjectDocument(
                "unsupported",
                SmoClassIds.TextureData,
                unsupportedBody));
        True(
            !SmoTextureDecoder.TryDecode(
                unsupportedDocument,
                unsupportedDocument.Objects.Single(),
                out _,
                out string unsupportedError),
            "unknown texture format is rejected");
        True(
            unsupportedError.StartsWith("UNSUPPORTED_TEXTURE_FORMAT:"),
            "unknown texture format has a stable diagnostic code");

        byte[] truncatedBody = CreateSyntheticTextureObject();
        SmoDocument truncatedDocument = SmoDocument.Parse(
            CreateSingleObjectDocument(
                "truncated",
                SmoClassIds.TextureData,
                truncatedBody,
                serializedSize: 0x40));
        True(
            !SmoTextureDecoder.TryDecode(
                truncatedDocument,
                truncatedDocument.Objects.Single(),
                out _,
                out string truncatedError),
            "texture pixels cannot escape the object interval");
        True(
            truncatedError.StartsWith("TEXTURE_PIXEL_BUFFER_OUTSIDE_OBJECT:"),
            "texture boundary failure has a stable diagnostic code");

        SmoDocument nodeDocument = SmoDocument.Parse(CreateSyntheticDocument());
        True(
            !SmoTextureDecoder.TryDecode(
                nodeDocument,
                nodeDocument.Objects.Single(),
                out _,
                out string classError),
            "non-texture object is rejected by texture decoder");
        True(
            classError.StartsWith("NOT_TEXTURE_DATA:"),
            "non-texture failure has a stable diagnostic code");
    }

    private static void TestVertexColorUsage()
    {
        const uint black = 0xFF000000;
        const uint white = 0xFFFFFFFF;

        uint[] trollPlaceholder = Enumerable.Repeat(black, 425)
            .Concat(Enumerable.Repeat(white, 10))
            .ToArray();
        True(
            !SmoVertexColorUsage.ShouldModulateTexture(
                0x097E, trollPlaceholder),
            "dominant-black two-colour character diffuse is a placeholder");

        uint[] boundaryTwoColour = Enumerable.Repeat(black, 95)
            .Concat(Enumerable.Repeat(white, 5))
            .ToArray();
        True(
            SmoVertexColorUsage.ShouldModulateTexture(
                0x097E, boundaryTwoColour),
            "exactly 95-percent black character diffuse remains renderable");

        uint[] aboveBoundaryPlaceholder = Enumerable.Repeat(black, 96)
            .Concat(Enumerable.Repeat(white, 4))
            .ToArray();
        True(
            !SmoVertexColorUsage.ShouldModulateTexture(
                0x097E, aboveBoundaryPlaceholder),
            "more than 95-percent black character diffuse is a placeholder");

        uint[] transparentBlack = Enumerable.Repeat(0x00000000u, 98)
            .Concat(Enumerable.Repeat(white, 2))
            .ToArray();
        True(
            SmoVertexColorUsage.ShouldModulateTexture(
                0x097E, transparentBlack),
            "non-opaque black is not classified as the exporter placeholder");

        uint[] genuineGradient = Enumerable.Repeat(black, 98)
            .Concat([0xFF0D132E, 0xFF808080])
            .ToArray();
        True(
            SmoVertexColorUsage.ShouldModulateTexture(
                0x097E, genuineGradient),
            "multi-colour character gradient remains renderable");
        True(
            SmoVertexColorUsage.ShouldModulateTexture(
                0x093E, trollPlaceholder),
            "dominant-black exception stays scoped to character layouts");
        True(
            !SmoVertexColorUsage.ShouldModulateTexture(
                0x097E, Enumerable.Repeat(black, 8).ToArray()),
            "uniform character diffuse remains a placeholder");
    }

    private static CorpusOptions ParseCorpusOptions(string[] args)
    {
        string? path = null;
        int? sampleCount = null;
        int seed = 20260808;

        for (int index = 0; index < args.Length; index++)
        {
            switch (args[index])
            {
                case "--sample-count":
                    sampleCount = int.Parse(args[++index]);
                    if (sampleCount <= 0)
                        throw new ArgumentOutOfRangeException(nameof(sampleCount));
                    break;
                case "--seed":
                    seed = int.Parse(args[++index]);
                    break;
                default:
                    if (args[index].StartsWith("--", StringComparison.Ordinal))
                        throw new ArgumentException($"Unknown option: {args[index]}");
                    if (path is not null)
                        throw new ArgumentException("Only one corpus path can be specified.");
                    path = args[index];
                    break;
            }
        }

        return new CorpusOptions(path, sampleCount, seed);
    }

    private static void TestLocalCorpus(string corpusPath, int? sampleCount, int seed)
    {
        bool singleFile = File.Exists(corpusPath);
        string[] allFiles = singleFile
            ? [corpusPath]
            : Directory
                .EnumerateFiles(corpusPath, "*.smo", SearchOption.AllDirectories)
                .OrderBy(path => path, StringComparer.OrdinalIgnoreCase)
                .ToArray();

        True(allFiles.Length > 0, "corpus contains SMO files");

        string[] files = allFiles;

        if (sampleCount.HasValue)
        {
            var random = new Random(seed);
            files = allFiles
                .Select(path => (Path: path, Key: random.NextInt64()))
                .OrderBy(item => item.Key)
                .Take(Math.Min(sampleCount.Value, allFiles.Length))
                .Select(item => item.Path)
                .ToArray();
            Equal(Math.Min(sampleCount.Value, allFiles.Length), files.Length, "sample size");
        }

        int parsed = 0;
        long objects = 0;
        int decodedTextures = 0;
        int resolvedBindings = 0;
        int decodedMeshes = 0;
        int uvMeshes = 0;
        int renderableTexturedMeshes = 0;
        int transformedModelMeshes = 0;
        int decodedStaticTransforms = 0;
        int resolvedMaterialColors = 0;
        int vertexColoredMeshes = 0;
        int meshesUnderStaticObjects = 0;
        int meshesWithoutStaticObjects = 0;
        var unplacedSamples = new List<string>();
        foreach (string file in files)
        {
            SmoDocument document = SmoDocument.Load(file);
            bool verboseFile = singleFile && document.Objects.Count <= 500;
            Equal(new FileInfo(file).Length, (long)document.Data.Length, $"length: {file}");
            Equal("FFPS", document.Header.Signature, $"signature: {file}");
            Equal(
                document.Header.DeclaredDataEnd,
                (ulong)document.Data.Length,
                $"data boundary: {file}");
            Equal(
                (int)document.Header.ObjectCount,
                document.Objects.Count,
                $"object count: {file}");
            parsed++;
            objects += document.Objects.Count;

            foreach (SmoObjectEntry entry in document.Objects.Where(
                         entry => entry.TypeHash == SmoClassIds.TextureData))
            {
                if (!SmoTextureDecoder.TryDecode(
                        document, entry, out SmoTexture? texture, out string textureError))
                {
                    if (verboseFile)
                        Console.WriteLine($"Texture [{entry.Index}] decode failed: {textureError}");
                    continue;
                }

                Equal(
                    checked(texture!.Width * texture.Height * 4),
                    texture.Bgra32Pixels.Length,
                    $"texture byte count: {file} [{entry.Index}]");
                Equal(texture.Width * 4, texture.PixelStride, $"texture stride: {file} [{entry.Index}]");
                if (texture.FormatCode is 0x32E3 or 0x43E3)
                {
                    Equal(SmoTextureLayout.Bgra, texture.SourceLayout,
                        $"newer texture source layout: {file} [{entry.Index}]");
                    ReadOnlySpan<byte> serialized = document.Data.Span.Slice(
                        checked((int)entry.PhysicalOffset),
                        checked((int)entry.SerializedSize));
                    Equal((byte)0x00, serialized[0x3C],
                        $"newer texture serializer marker: {file} [{entry.Index}]");

                    ReadOnlySpan<byte> sourcePixels = serialized.Slice(
                        0x3D, texture.Bgra32Pixels.Length);
                    ReadOnlySpan<byte> decodedPixels = texture.Bgra32Pixels.Span;
                    for (int channel = 0; channel < 4; channel++)
                    {
                        Equal(sourcePixels[channel], decodedPixels[channel],
                            $"newer texture first BGRA[{channel}]: {file} [{entry.Index}]");
                        int lastChannel = sourcePixels.Length - 4 + channel;
                        Equal(sourcePixels[lastChannel], decodedPixels[lastChannel],
                            $"newer texture last BGRA[{channel}]: {file} [{entry.Index}]");
                    }
                }
                decodedTextures++;
                if (verboseFile)
                {
                    ReadOnlySpan<byte> pixels = texture.Bgra32Pixels.Span;
                    int transparentPixels = 0;
                    byte minAlpha = byte.MaxValue;
                    byte maxAlpha = byte.MinValue;
                    for (int offset = 3; offset < pixels.Length; offset += 4)
                    {
                        byte alpha = pixels[offset];
                        minAlpha = Math.Min(minAlpha, alpha);
                        maxAlpha = Math.Max(maxAlpha, alpha);
                        if (alpha == 0)
                            transparentPixels++;
                    }
                    Console.WriteLine(
                        $"Texture [{entry.Index}] {texture.Name}: {texture.Width}x{texture.Height}, " +
                        $"alpha={minAlpha}..{maxAlpha}, transparent={transparentPixels}");
                }
            }

            IReadOnlyDictionary<int, SmoTextureBinding> bindings =
                SmoTextureBindingResolver.ResolveAll(document);
            IReadOnlyDictionary<int, uint> materialColors =
                SmoMaterialColorResolver.ResolveAll(document);
            SmoNodeHierarchy nodeHierarchy = SmoNodeHierarchy.Decode(document);
            foreach (SmoNodeChildLink link in nodeHierarchy.Links)
            {
                True(
                    document.Objects[link.ChildObjectIndex].Id == link.ChildObjectId,
                    $"node child ID resolves: {file} [{link.ParentObjectIndex}] -> " +
                    $"[{link.ChildObjectIndex}]");
                True(
                    link.InlineSerializedSize == 0 ||
                    link.InlineSerializedSize ==
                    document.Objects[link.ChildObjectIndex].SerializedSize,
                    $"node child inline size: {file} [{link.ParentObjectIndex}] -> " +
                    $"[{link.ChildObjectIndex}]");
            }

            if (verboseFile)
            {
                foreach (SmoObjectEntry skin in document.Objects.Where(entry =>
                             entry.TypeHash == SmoClassIds.Skin))
                {
                    Console.WriteLine(
                        $"Skin [{skin.Index}] id={skin.Id} {skin.Name}: " +
                        $"parent={skin.ParentIndex}, off=0x{skin.LogicalOffset:X}, " +
                        $"size=0x{skin.SerializedSize:X}");
                    if (SmoSkinDecoder.TryDecode(
                            document, skin, out SmoSkin? decodedSkin, out string skinError) &&
                        decodedSkin is not null)
                    {
                        Console.WriteLine(
                            $"  palette={string.Join(", ", decodedSkin.Bones.Select(bone =>
                                $"{bone.PaletteIndex}:[{bone.NodeObjectIndex}]" +
                                document.Objects[bone.NodeObjectIndex].Name))}");
                        foreach (SmoSkinBone bone in decodedSkin.Bones.Where(bone =>
                                     document.Objects[bone.NodeObjectIndex].Name is
                                         "Head" or "Bloom_head_geometry"))
                        {
                            if (System.Numerics.Matrix4x4.Invert(
                                    bone.InverseBindMatrix, out var bindMatrix))
                            {
                                Console.WriteLine(
                                    $"  bind {document.Objects[bone.NodeObjectIndex].Name}: " +
                                    $"T=({bindMatrix.M41:G6},{bindMatrix.M42:G6}," +
                                    $"{bindMatrix.M43:G6})");
                            }
                        }
                    }
                    else
                    {
                        Console.WriteLine($"  palette decode failed: {skinError}");
                    }
                }
                foreach (SmoNodeChildLink link in nodeHierarchy.Links)
                {
                    Console.WriteLine(
                        $"Child [{link.ParentObjectIndex}] -> [{link.ChildObjectIndex}] " +
                        $"id={link.ChildObjectId}, inline=0x{link.InlineSerializedSize:X}");
                }
                foreach (SmoObjectEntry node in document.Objects.Where(entry =>
                             entry.TypeHash is SmoClassIds.Node or
                                 SmoClassIds.RenderNode or SmoClassIds.Model))
                {
                    if (SmoNodeTransformDecoder.TryDecode(
                            document, node, out SmoNodeTransform? nodeTransform) &&
                        nodeTransform is not null)
                    {
                        Console.WriteLine(
                            $"Node [{node.Index}] id={node.Id} {node.Name}: " +
                            $"parent={node.ParentIndex}, off=0x{node.LogicalOffset:X}, " +
                            $"size=0x{node.SerializedSize:X}, " +
                            $"P={nodeTransform.Position}, Q={nodeTransform.Rotation}, " +
                            $"S={nodeTransform.Scale}");
                    }
                }
            }
            foreach ((int meshIndex, uint argb) in materialColors)
            {
                True(document.Objects.Any(entry =>
                        entry.Index == meshIndex && entry.TypeHash == SmoClassIds.MeshData),
                    $"material color references a mesh: {file} [{meshIndex}]");
                True((argb & 0x00FFFFFF) != 0,
                    $"material color is visible: {file} [{meshIndex}]");
                resolvedMaterialColors++;
            }
            foreach ((int meshIndex, SmoTextureBinding binding) in bindings)
            {
                True(
                    document.Objects.Any(entry =>
                        entry.Index == meshIndex && entry.TypeHash == SmoClassIds.MeshData),
                    $"binding references a mesh: {file} [{meshIndex}]");
                if (binding.Texture is not null)
                    resolvedBindings++;
            }

            foreach (SmoObjectEntry entry in document.Objects.Where(
                         entry => entry.TypeHash == SmoClassIds.StaticRenderObject))
            {
                if (!SmoStaticRenderObjectTransformDecoder.TryDecode(
                        document, entry, out Matrix4x4 staticTransform))
                    continue;

                True(IsFinite(staticTransform),
                    $"finite static render transform: {file} [{entry.Index}]");
                decodedStaticTransforms++;
            }

            foreach (SmoObjectEntry entry in document.Objects.Where(
                         entry => entry.TypeHash == SmoClassIds.MeshData))
            {
                if (!SmoMeshDecoder.TryDecode(document, entry, out SmoMesh? mesh, out _))
                    continue;
                decodedMeshes++;
                if (mesh!.VertexFormat == 0x093E)
                {
                    True(mesh.HasSkinningData,
                        $"0x093E skinning attributes: {file} [{entry.Index}]");
                    True(
                        mesh.BlendWeights.All(weight =>
                            weight.X >= 0 && weight.Y >= 0 &&
                            weight.Z >= 0 && weight.W >= 0 &&
                            weight.X + weight.Y + weight.Z + weight.W <= 1.001f),
                        $"0x093E normalized blend weights: {file} [{entry.Index}]");
                    True(
                        mesh.BlendWeights.Zip(mesh.BlendIndices).All(item =>
                            (item.First.X <= 0.00001f || item.Second.X < 16) &&
                            (item.First.Y <= 0.00001f || item.Second.Y < 16) &&
                            (item.First.Z <= 0.00001f || item.Second.Z < 16) &&
                            (item.First.W <= 0.00001f || item.Second.W < 16)),
                        $"0x093E active blend indices: {file} [{entry.Index}]");
                }
                Dictionary<int, SmoObjectEntry> entriesByIndex = document.Objects
                    .ToDictionary(item => item.Index);
                SmoObjectEntry? ancestor = entry;
                bool underStaticObject = false;
                while (ancestor.ParentIndex is int ancestorIndex &&
                       entriesByIndex.TryGetValue(ancestorIndex, out ancestor))
                {
                    if (ancestor.TypeHash == SmoClassIds.StaticRenderObject)
                    {
                        underStaticObject = true;
                        break;
                    }
                }
                if (underStaticObject)
                    meshesUnderStaticObjects++;
                else
                    meshesWithoutStaticObjects++;
                if (mesh!.HasDiffuseColors && mesh.DiffuseColorsArgb.Any(
                        color => (color & 0x00FFFFFF) != 0x00FFFFFF))
                    vertexColoredMeshes++;
                Matrix4x4 worldTransform =
                    SmoNodeTransformDecoder.ResolveModelWorldMatrix(document, entry);
                if (singleFile && !underStaticObject && worldTransform == Matrix4x4.Identity &&
                    unplacedSamples.Count < 30)
                {
                    var chain = new List<string>();
                    SmoObjectEntry? chainEntry = entry;
                    while (chainEntry is not null)
                    {
                        chain.Add(
                            $"[{chainEntry.Index}]" +
                            $"{SmoClassRegistry.GetDisplayName(chainEntry.TypeHash)}:" +
                            chainEntry.Name);
                        chainEntry = chainEntry.ParentIndex is int chainParent &&
                                     entriesByIndex.TryGetValue(chainParent, out SmoObjectEntry? parent)
                            ? parent
                            : null;
                    }
                    System.Numerics.Vector3 minimum = new(
                        mesh.Positions.Min(position => position.X),
                        mesh.Positions.Min(position => position.Y),
                        mesh.Positions.Min(position => position.Z));
                    System.Numerics.Vector3 maximum = new(
                        mesh.Positions.Max(position => position.X),
                        mesh.Positions.Max(position => position.Y),
                        mesh.Positions.Max(position => position.Z));
                    unplacedSamples.Add(
                        $"bounds center={(minimum + maximum) * 0.5f}, size={maximum - minimum}; " +
                        string.Join(" <- ", chain));
                }
                if (worldTransform != Matrix4x4.Identity)
                {
                    True(IsFinite(worldTransform), $"finite model transform: {file} [{entry.Index}]");
                    transformedModelMeshes++;
                }
                if (!mesh.HasTextureCoordinates)
                    continue;
                uvMeshes++;
                if (bindings.TryGetValue(entry.Index, out SmoTextureBinding? binding) &&
                    binding.Texture is not null)
                {
                    renderableTexturedMeshes++;
                }

                if (verboseFile)
                {
                    string textureName = bindings.TryGetValue(
                            entry.Index, out SmoTextureBinding? selected) &&
                        selected.Texture is not null
                            ? selected.Texture.Name
                            : "<fallback>";
                    float minU = mesh.TextureCoordinates.Min(uv => uv.X);
                    float maxU = mesh.TextureCoordinates.Max(uv => uv.X);
                    float minV = mesh.TextureCoordinates.Min(uv => uv.Y);
                    float maxV = mesh.TextureCoordinates.Max(uv => uv.Y);
                    System.Numerics.Vector3 rawCenter = new(
                        (mesh.Positions.Min(position => position.X) +
                         mesh.Positions.Max(position => position.X)) * 0.5f,
                        (mesh.Positions.Min(position => position.Y) +
                         mesh.Positions.Max(position => position.Y)) * 0.5f,
                        (mesh.Positions.Min(position => position.Z) +
                         mesh.Positions.Max(position => position.Z)) * 0.5f);
                    System.Numerics.Vector3 transformedCenter =
                        System.Numerics.Vector3.Transform(rawCenter, worldTransform);
                    Console.WriteLine(
                        $"Mesh [{entry.Index}] {entry.Name}: format=0x{mesh.VertexFormat:X}, " +
                        $"stride={mesh.Stride}/{mesh.RuntimeStride}, skin={mesh.HasSkinningData}, " +
                        $"texture={textureName}, UV=({minU:G5}..{maxU:G5}, {minV:G5}..{maxV:G5}), " +
                        $"worldT=({worldTransform.M41:G5},{worldTransform.M42:G5}," +
                        $"{worldTransform.M43:G5}), center={rawCenter}->{transformedCenter}");
                    int degenerateUvTriangles = 0;
                    for (int index = 0; index < mesh.TriangleIndices.Length; index += 3)
                    {
                        var a = mesh.TextureCoordinates[checked((int)mesh.TriangleIndices[index])];
                        var b = mesh.TextureCoordinates[checked((int)mesh.TriangleIndices[index + 1])];
                        var c = mesh.TextureCoordinates[checked((int)mesh.TriangleIndices[index + 2])];
                        float area = (b.X - a.X) * (c.Y - a.Y) -
                                     (b.Y - a.Y) * (c.X - a.X);
                        if (MathF.Abs(area) < 0.0000001f)
                            degenerateUvTriangles++;
                    }
                    Console.WriteLine(
                        $"  triangles={mesh.TriangleCount}, degenerate UV={degenerateUvTriangles}");
                    var chain = new List<string>();
                    SmoObjectEntry? cursor = entry;
                    while (cursor is not null)
                    {
                        chain.Add(
                            $"[{cursor.Index}]{SmoClassRegistry.GetDisplayName(cursor.TypeHash)}:{cursor.Name}");
                        cursor = cursor.ParentIndex is int parentIndex
                            ? document.Objects[parentIndex]
                            : null;
                    }
                    Console.WriteLine($"  owners={string.Join(" <- ", chain)}");
                }
            }
        }

        if (!sampleCount.HasValue && !singleFile)
        {
            CheckKnownFile(corpusPath, "fish.smo", expectedObjects: 18, expectedMeshes: 1);
            CheckKnownFile(corpusPath, "bloom_ball.smo", expectedObjects: 123, expectedMeshes: 6);
            CheckKnownFile(corpusPath, "loading.smo", expectedObjects: 11, expectedMeshes: 2);
            CheckKnownFile(corpusPath, "menu.smo", expectedObjects: 238, expectedMeshes: 41);
            CheckE0FinalIndices(corpusPath);
        }

        Console.WriteLine(
            $"Corpus: {parsed}/{allFiles.Length} SMO files, {objects} object-directory entries, " +
            $"decoded textures: {decodedTextures}, resolved bindings: {resolvedBindings}, " +
            $"meshes: {decodedMeshes}, UV meshes: {uvMeshes}, " +
            $"renderable textured meshes: {renderableTexturedMeshes}, " +
            $"transformed model meshes: {transformedModelMeshes}, " +
            $"static transforms: {decodedStaticTransforms}, " +
            $"material colors: {resolvedMaterialColors}, " +
            $"vertex-colored meshes: {vertexColoredMeshes}, " +
            $"meshes under static objects: {meshesUnderStaticObjects}, " +
            $"without static objects: {meshesWithoutStaticObjects}");
        foreach (string sample in unplacedSamples)
            Console.WriteLine($"  unplaced: {sample}");
        foreach (string file in files)
            Console.WriteLine($"  {(singleFile ? Path.GetFileName(file) : Path.GetRelativePath(corpusPath, file))}");

        if (singleFile && Path.GetFileName(corpusPath).Equals(
                "bloom_school.smo", StringComparison.OrdinalIgnoreCase))
        {
            CheckBloomSchool(corpusPath);
        }
        else if (singleFile && Path.GetFileName(corpusPath).Equals(
                     "knutBoss.smo", StringComparison.OrdinalIgnoreCase))
        {
            CheckKnutBossShieldTransparency(corpusPath);
        }
        else if (singleFile)
        {
            CheckSelectedCharacterBindings(corpusPath);
        }
    }

    private static void CheckKnutBossShieldTransparency(string path)
    {
        SmoDocument document = SmoDocument.Load(path);
        SmoObjectEntry shieldEntry = document.Objects[13];
        Equal(SmoClassIds.MeshData, shieldEntry.TypeHash,
            "knutBoss shield mesh object");

        SmoMesh shield = SmoMeshDecoder.Decode(document, shieldEntry);
        True(shield.HasTextureCoordinates,
            "knutBoss shield UV channel");

        IReadOnlyDictionary<int, SmoTextureBinding> bindings =
            SmoTextureBindingResolver.ResolveAll(document);
        True(bindings.TryGetValue(13, out SmoTextureBinding? binding),
            "knutBoss shield texture binding");
        Equal("gr_01", binding!.Texture!.Name,
            "knutBoss shield texture name");

        ReadOnlySpan<byte> pixels = binding.Texture.Bgra32Pixels.Span;
        int transparent = 0;
        int translucent = 0;
        int opaque = 0;
        for (int offset = 3; offset < pixels.Length; offset += 4)
        {
            switch (pixels[offset])
            {
                case 0:
                    transparent++;
                    break;
                case byte.MaxValue:
                    opaque++;
                    break;
                default:
                    translucent++;
                    break;
            }
        }

        Console.WriteLine(
            $"knutBoss shield [13] gr_01 alpha: transparent={transparent}, " +
            $"translucent={translucent}, opaque={opaque}");
        True(transparent > 0,
            "knutBoss shield texture has transparent pixels");
        True(translucent > 0,
            "knutBoss shield texture has translucent pixels");

        True(SmoMaterialRenderState.TryDecodeFlags(
                document, document.Objects[10], out uint shieldFlags) &&
             SmoMaterialRenderState.UsesAlphaBlend(shieldFlags),
            "knutBoss shield material enables alpha blending");
        True(SmoMaterialRenderState.TryDecodeFlags(
                document, document.Objects[26], out uint bodyFlags) &&
             !SmoMaterialRenderState.UsesAlphaBlend(bodyFlags),
            "knutBoss body material keeps alpha blending disabled");
        True(binding.UsesAlphaBlend,
            "knutBoss shield binding carries alpha-blend render state");
        True(bindings.TryGetValue(28, out SmoTextureBinding? bodyBinding) &&
             !bodyBinding.UsesAlphaBlend,
            "knutBoss body binding stays in the opaque render pass");
    }

    private static void CheckBloomSchool(string path)
    {
        SmoDocument document = SmoDocument.Load(path);
        SmoMesh[] meshes = document.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.MeshData)
            .Select(entry => SmoMeshDecoder.Decode(document, entry))
            .ToArray();
        Equal(6, meshes.Length, "bloom_school mesh count");
        Equal(5, meshes.Count(mesh => mesh.VertexFormat == 0x197E), "bloom_school 0x197E meshes");
        Equal(1, meshes.Count(mesh => mesh.VertexFormat == 0x097E), "bloom_school 0x097E meshes");
        Equal(6, meshes.Count(mesh => mesh.HasTextureCoordinates), "bloom_school UV meshes");
        Equal(6, meshes.Count(mesh => mesh.HasDiffuseColors), "bloom_school diffuse meshes");

        IReadOnlyDictionary<int, SmoTextureBinding> bindings =
            SmoTextureBindingResolver.ResolveAll(document);
        SmoTextureBinding[] textured = bindings.Values
            .Where(binding => binding.Texture is not null)
            .ToArray();
        Equal(6, textured.Length, "bloom_school textured meshes");
        Equal(
            5,
            textured.Count(binding => binding.Texture!.Name == "bloom_gilet"),
            "bloom_school body texture bindings");
        Equal(
            1,
            textured.Count(binding => binding.Texture!.Name == "bloomeye"),
            "bloom_school eye texture binding");
        True(
            textured.Select(binding => binding.Texture!.Name)
                .All(name => name is "bloom_gilet" or "bloomeye"),
            "bloom_school exact texture names");
    }

    private static void CheckSelectedCharacterBindings(string path)
    {
        IReadOnlyDictionary<string, IReadOnlyDictionary<string, int>> expected =
            new Dictionary<string, IReadOnlyDictionary<string, int>>(
                StringComparer.OrdinalIgnoreCase)
            {
                ["Goopmonster.smo"] = new Dictionary<string, int>
                {
                    ["gooptex"] = 6,
                    ["gooptex2"] = 3
                },
                ["Darcy.smo"] = new Dictionary<string, int> { ["darcy"] = 6 },
                ["knut.smo"] = new Dictionary<string, int> { ["knut"] = 6 },
                ["bloom_silk.smo"] = new Dictionary<string, int>
                {
                    ["bloom_lotus"] = 5,
                    ["bloomeye"] = 1
                },
                ["Generic.smo"] = new Dictionary<string, int>
                {
                    ["spe_body"] = 7,
                    ["spe_g_ey"] = 1,
                    ["spe_g_he"] = 1
                },
                ["Grizelda.smo"] = new Dictionary<string, int>
                {
                    ["grizelda"] = 6,
                    ["grizel_e"] = 1,
                    ["grizel_g"] = 1
                },
                ["fish.smo"] = new Dictionary<string, int> { ["fish"] = 1 },
                ["butterfly.smo"] = new Dictionary<string, int> { ["butter_g"] = 1 },
                ["Griffin.smo"] = new Dictionary<string, int>
                {
                    ["griffin"] = 6,
                    ["griffi_e"] = 1
                },
                ["Bloom_body.smo"] = new Dictionary<string, int>
                {
                    ["bloom_jeans"] = 6,
                    ["bloomeye"] = 2
                },
                ["Droid.smo"] = new Dictionary<string, int> { ["xj5"] = 4 },
                ["Amaryl.smo"] = new Dictionary<string, int> { ["amaryl"] = 6 },
                ["Troll.smo"] = new Dictionary<string, int> { ["troll"] = 6 },
                ["bloom_bike.smo"] = new Dictionary<string, int>
                {
                    ["b_bike"] = 5,
                    ["bloomeye"] = 1
                },
                ["bloomx.smo"] = new Dictionary<string, int>
                {
                    ["bloom"] = 8,
                    ["bloomeye"] = 1,
                    ["sparkles0001"] = 2
                },
                ["bloom_crystal.smo"] = new Dictionary<string, int>
                {
                    ["b_crysta"] = 7,
                    ["bloomeye"] = 1,
                    ["sparkles0001"] = 1
                },
                ["bloom_dating_outfit_02.smo"] = new Dictionary<string, int>
                {
                    ["bloom_jeansd"] = 8,
                    ["bloomeye_testd"] = 2
                },
                ["bloom_dating_outfit_03.smo"] = new Dictionary<string, int>
                {
                    ["b_biked"] = 5,
                    ["bloomeye_testd"] = 2,
                    ["bloom_jeansd"] = 4
                },
                ["bloom_dating_outfit_01.smo"] = new Dictionary<string, int>
                {
                    ["b_balld"] = 6,
                    ["bloomeye_testd"] = 2,
                    ["bloom_jeansd"] = 3
                },
                ["prince_dating_outfit_01.smo"] = new Dictionary<string, int>
                {
                    ["spe_body"] = 6,
                    ["bloomeye_testd"] = 2,
                    ["sky"] = 2
                },
                ["prince_dating_outfit_02.smo"] = new Dictionary<string, int>
                {
                    ["sky_jean"] = 5,
                    ["bloomeye_testd"] = 2,
                    ["sky"] = 2,
                    ["spe_body"] = 2
                },
                ["prince_dating_outfit_03.smo"] = new Dictionary<string, int>
                {
                    ["sky"] = 5,
                    ["bloomeye_testd"] = 2,
                    ["spe_body"] = 5
                }
            };
        if (!expected.TryGetValue(Path.GetFileName(path), out var expectedTextures))
            return;

        SmoDocument document = SmoDocument.Load(path);
        SmoMesh[] decodedMeshes = document.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.MeshData)
            .Select(entry => SmoMeshDecoder.Decode(document, entry))
            .ToArray();
        int expectedMeshCount = Path.GetFileName(path).ToLowerInvariant() switch
        {
            "bloom_dating_outfit_02.smo" => 14,
            "bloom_dating_outfit_03.smo" => 12,
            "bloom_dating_outfit_01.smo" => 12,
            "prince_dating_outfit_01.smo" => 13,
            "bloomx.smo" => 11,
            "knut.smo" => 7,
            "bloom_crystal.smo" => 9,
            "prince_dating_outfit_02.smo" => 14,
            "prince_dating_outfit_03.smo" => 15,
            _ => expectedTextures.Values.Sum()
        };
        Equal(expectedMeshCount, decodedMeshes.Length,
            $"{Path.GetFileName(path)} decoded meshes");
        Equal(decodedMeshes.Length, decodedMeshes.Count(mesh => mesh.HasTextureCoordinates),
            $"{Path.GetFileName(path)} UV meshes");
        if (Path.GetFileName(path).Equals(
                "butterfly.smo", StringComparison.OrdinalIgnoreCase))
        {
            SmoMesh butterfly = decodedMeshes.Single();
            True(butterfly.HasDiffuseColors, "butterfly vertex diffuse colors");
            True(
                butterfly.DiffuseColorsArgb.Distinct().Count() > 1,
                "butterfly varying vertex diffuse colors");
        }
        else if (Path.GetFileName(path).Equals(
                     "Amaryl.smo", StringComparison.OrdinalIgnoreCase))
        {
            foreach (SmoMesh mesh in decodedMeshes)
            {
                True(mesh.HasDiffuseColors, $"Amaryl [{mesh.ObjectIndex}] vertex diffuse");
                int black = mesh.DiffuseColorsArgb.Count(color => (color & 0x00FFFFFF) == 0);
                int transparent = mesh.DiffuseColorsArgb.Count(color => (color >> 24) == 0);
                Console.WriteLine(
                    $"Amaryl [{mesh.ObjectIndex}]: colors={mesh.DiffuseColorsArgb.Distinct().Count()}, " +
                    $"black={black}/{mesh.VertexCount}, alpha0={transparent}/{mesh.VertexCount}, " +
                    $"first={string.Join(",", mesh.DiffuseColorsArgb.Distinct().Take(8).Select(color => $"0x{color:X8}"))}");
                Equal(mesh.VertexCount, black,
                    $"Amaryl [{mesh.ObjectIndex}] uniform black diffuse sentinel");
            }
        }
        else if (Path.GetFileName(path).Equals(
                     "Troll.smo", StringComparison.OrdinalIgnoreCase))
        {
            SmoMesh head = decodedMeshes.Single(mesh => mesh.ObjectIndex == 86);
            Equal(425, head.DiffuseColorsArgb.Count(color => color == 0xFF000000),
                "Troll mesh 86 black placeholder vertices");
            Equal(10, head.DiffuseColorsArgb.Count(color => color == 0xFFFFFFFF),
                "Troll mesh 86 white default vertices");
            True(
                !SmoVertexColorUsage.ShouldModulateTexture(head),
                "Troll mesh 86 keeps its colour atlas instead of black tint");
            IReadOnlyDictionary<int, SmoTextureBinding> trollBindings =
                SmoTextureBindingResolver.ResolveAll(document);
            True(
                trollBindings.TryGetValue(86, out SmoTextureBinding? trollBinding) &&
                trollBinding.Texture?.Name == "troll",
                "Troll mesh 86 resolves its embedded troll atlas");
        }
        else if (Path.GetFileName(path).Equals(
                     "bloom_bike.smo", StringComparison.OrdinalIgnoreCase))
        {
            foreach (SmoMesh part in decodedMeshes)
            {
                True(part.HasDiffuseColors, $"bloom_bike [{part.ObjectIndex}] vertex diffuse");
                Console.WriteLine(
                    $"bloom_bike [{part.ObjectIndex}]: colors={part.DiffuseColorsArgb.Distinct().Count()}, " +
                    $"values={string.Join(",", part.DiffuseColorsArgb.Distinct().Take(12).Select(color => $"0x{color:X8}"))}");
            }
            Equal((uint)0xFF202020, decodedMeshes.Single(mesh => mesh.ObjectIndex == 23)
                    .DiffuseColorsArgb.Distinct().Single(),
                "bloom_bike eyes uniform diffuse sentinel");
        }
        else if (Path.GetFileName(path).Equals(
                     "bloomx.smo", StringComparison.OrdinalIgnoreCase))
        {
            IReadOnlyDictionary<int, SmoTextureBinding> bloomXBindings =
                SmoTextureBindingResolver.ResolveAll(document);
            foreach (int wingMeshIndex in new[] { 25, 29 })
            {
                SmoMesh wing = decodedMeshes.Single(mesh =>
                    mesh.ObjectIndex == wingMeshIndex);
                True(bloomXBindings.TryGetValue(
                        wingMeshIndex, out SmoTextureBinding? wingBinding),
                    $"bloomx wing [{wingMeshIndex}] inherited atlas binding");
                Equal("bloom", wingBinding!.Texture!.Name,
                    $"bloomx wing [{wingMeshIndex}] inherited bloom atlas");
                True(wing.HasTextureCoordinates,
                    $"bloomx wing [{wingMeshIndex}] UV channel");
                True(wing.DiffuseColorsArgb.All(color => color == 0xFFFFFFFF),
                    $"bloomx wing [{wingMeshIndex}] neutral vertex diffuse");
            }
            foreach (int meshIndex in new[] { 103, 105 })
            {
                SmoMesh part = decodedMeshes.Single(mesh => mesh.ObjectIndex == meshIndex);
                True(bloomXBindings.TryGetValue(
                        meshIndex, out SmoTextureBinding? binding),
                    $"bloomx mesh [{meshIndex}] layered binding");
                Equal("sparkles0001", binding!.Texture!.Name,
                    $"bloomx mesh [{meshIndex}] first animated layer frame");
                Equal("bloom_xc", binding.BaseTexture!.Name,
                    $"bloomx mesh [{meshIndex}] base texture layer");
                Equal(10, binding.AnimationFrames!.Count,
                    $"bloomx mesh [{meshIndex}] sparkle frame count");
                True(binding.FrameDuration > TimeSpan.Zero,
                    $"bloomx mesh [{meshIndex}] positive frame duration");
                True(part.HasTextureCoordinates1,
                    $"bloomx mesh [{meshIndex}] second UV channel");
                True(part.TextureCoordinates.Any(uv =>
                        uv.X < 0 || uv.X > 1 || uv.Y < 0 || uv.Y > 1),
                    $"bloomx mesh [{meshIndex}] tiled base UV channel");
            }

            SmoTexture firstFrame = bloomXBindings[103].AnimationFrames![0];
            SmoTexture bloomXBase = bloomXBindings[103].BaseTexture!;
            True(Enumerable.Range(0, bloomXBase.Width * bloomXBase.Height)
                    .Select(pixel => BitConverter.ToUInt32(
                        bloomXBase.Bgra32Pixels.Span.Slice(pixel * 4, 4)))
                    .All(color => (color & 0x00FFFFFF) == 0x0067CBDF),
                "bloomx base layer preserves its cyan RGB under alpha");
            True(Enumerable.Range(0, firstFrame.Width * firstFrame.Height)
                    .Count(pixel => BitConverter.ToUInt32(
                        firstFrame.Bgra32Pixels.Span.Slice(pixel * 4, 4)) ==
                        0xFF000000) > firstFrame.Width * firstFrame.Height / 2,
                "bloomx sparkle frame uses opaque black as additive transparency");
            True(firstFrame.Bgra32Pixels.ToArray()
                    .Where((_, index) => index % 4 == 3)
                    .Distinct().Count() > 1,
                "bloomx animated overlay uses varying alpha");
        }
        else if (Path.GetFileName(path).Equals(
                     "bloom_crystal.smo", StringComparison.OrdinalIgnoreCase))
        {
            foreach (SmoObjectEntry entry in document.Objects.Where(entry => entry.Index is >= 75 and <= 95))
            {
                Console.WriteLine(
                    $"crystal object [{entry.Index}] type=0x{entry.TypeHash:X8} " +
                    $"{SmoClassRegistry.GetDisplayName(entry.TypeHash)} name={entry.Name} " +
                    $"parent={entry.ParentIndex} offset=0x{entry.PhysicalOffset:X} size=0x{entry.SerializedSize:X}");
            }
            IReadOnlyDictionary<int, SmoTextureBinding> crystalBindings =
                SmoTextureBindingResolver.ResolveAll(document);
            True(crystalBindings.TryGetValue(93, out SmoTextureBinding? crystalBinding),
                "bloom_crystal mesh 93 animated binding");
            Equal("sparkles0001", crystalBinding!.Texture!.Name,
                "bloom_crystal mesh 93 first frame");
            Equal(10, crystalBinding.AnimationFrames!.Count,
                "bloom_crystal sparkle frame count");
            Equal("b_crysta", crystalBinding.BaseTexture!.Name,
                "bloom_crystal mesh 93 base texture stage");
            True(
                Math.Abs(crystalBinding.FrameDuration!.Value.TotalSeconds - 0.1266667) < 0.0001,
                "bloom_crystal sequence timing from controller keys");
            IReadOnlyDictionary<int, uint> crystalColors =
                SmoMaterialColorResolver.ResolveAll(document);
            Console.WriteLine(
                $"bloom_crystal mesh 93 material color=" +
                (crystalColors.TryGetValue(93, out uint crystalColor)
                    ? $"0x{crystalColor:X8}"
                    : "<none>"));
            SmoMesh crystalMesh = decodedMeshes.Single(mesh => mesh.ObjectIndex == 93);
            True(crystalMesh.HasTextureCoordinates1,
                "bloom_crystal mesh 93 second UV channel");
            Console.WriteLine(
                $"bloom_crystal mesh 93 diffuse=" +
                string.Join(",", crystalMesh.DiffuseColorsArgb.Distinct()
                    .Take(20).Select(color => $"0x{color:X8}")) +
                $", UV1=({crystalMesh.TextureCoordinates1.Min(uv => uv.X):G5}.." +
                $"{crystalMesh.TextureCoordinates1.Max(uv => uv.X):G5}, " +
                $"{crystalMesh.TextureCoordinates1.Min(uv => uv.Y):G5}.." +
                $"{crystalMesh.TextureCoordinates1.Max(uv => uv.Y):G5})");
            foreach (SmoTexture frame in crystalBinding.AnimationFrames)
            {
                int alphaZero = 0;
                long visibleRed = 0;
                long visibleGreen = 0;
                long visibleBlue = 0;
                int visible = 0;
                for (int pixel = 0; pixel < frame.Bgra32Pixels.Length; pixel += 4)
                {
                    byte alpha = frame.Bgra32Pixels.Span[pixel + 3];
                    if (alpha == 0)
                    {
                        alphaZero++;
                        continue;
                    }
                    visibleBlue += frame.Bgra32Pixels.Span[pixel];
                    visibleGreen += frame.Bgra32Pixels.Span[pixel + 1];
                    visibleRed += frame.Bgra32Pixels.Span[pixel + 2];
                    visible++;
                }
                Console.WriteLine(
                    $"  {frame.Name}: alpha0={alphaZero}/{frame.Width * frame.Height}, " +
                    $"visible avg RGB=({visibleRed / Math.Max(1, visible)}," +
                    $"{visibleGreen / Math.Max(1, visible)},{visibleBlue / Math.Max(1, visible)}), " +
                    $"UV1 sample BGRA=" + GetTextureSample(frame, crystalMesh.TextureCoordinates1[0]));
            }
        }
        IReadOnlyDictionary<int, SmoTextureBinding> bindings =
            SmoTextureBindingResolver.ResolveAll(document);
        string[] actualTextures = bindings.Values
            .Where(binding => binding.Texture is not null)
            .Select(binding => binding.Texture!.Name)
            .ToArray();
        Equal(expectedTextures.Values.Sum(), actualTextures.Length,
            $"{Path.GetFileName(path)} textured meshes");
        foreach ((string textureName, int expectedCount) in expectedTextures)
        {
            Equal(expectedCount, actualTextures.Count(name => name == textureName),
                $"{Path.GetFileName(path)} {textureName} bindings");
        }

        if (Path.GetFileName(path).Equals(
                "bloom_dating_outfit_02.smo", StringComparison.OrdinalIgnoreCase))
        {
            foreach (SmoTexture texture in bindings.Values
                         .Select(binding => binding.Texture)
                         .OfType<SmoTexture>()
                         .DistinctBy(texture => texture.ObjectIndex))
            {
                True(
                    texture.Bgra32Pixels.Span[3..].ToArray()
                        .Where((_, index) => index % 4 == 0)
                        .All(alpha => alpha == byte.MaxValue),
                    $"{texture.Name} opaque BGRA alpha");
            }
            foreach (int propMeshIndex in new[] { 6, 10, 38 })
            {
                True(
                    !bindings.TryGetValue(
                        propMeshIndex, out SmoTextureBinding? propBinding) ||
                    propBinding.Texture is null,
                    $"outfit_02 rigid prop [{propMeshIndex}] keeps material color");
            }
            IReadOnlyDictionary<int, uint> datingMaterialColors =
                SmoMaterialColorResolver.ResolveAll(document);
            foreach (int propMeshIndex in new[] { 6, 10, 38 })
            {
                True(
                    datingMaterialColors.ContainsKey(propMeshIndex),
                    $"outfit_02 rigid prop [{propMeshIndex}] has material color");
            }

            SmoDocument datingDocument = document;
            SmoObjectEntry[] skinEntries = datingDocument.Objects
                .Where(entry => entry.TypeHash == SmoClassIds.Skin)
                .ToArray();
            Equal(8, skinEntries.Length, "outfit_02 skin count");
            foreach (SmoObjectEntry skinEntry in skinEntries)
            {
                True(
                    SmoSkinDecoder.TryDecode(
                        datingDocument, skinEntry, out SmoSkin? skin, out _),
                    $"outfit_02 skin palette [{skinEntry.Index}]");
                Equal(16, skin!.Bones.Count,
                    $"outfit_02 16-bone palette [{skinEntry.Index}]");

                SmoObjectEntry meshEntry = datingDocument.Objects.Single(entry =>
                    entry.ParentIndex == skinEntry.Index &&
                    entry.TypeHash == SmoClassIds.MeshData);
                SmoMesh skinnedMesh = SmoMeshDecoder.Decode(datingDocument, meshEntry);
                True(skinnedMesh.HasSkinningData,
                    $"outfit_02 skinning attributes [{meshEntry.Index}]");
                True(
                    skinnedMesh.BlendWeights.All(weight =>
                        weight.X >= 0 && weight.Y >= 0 &&
                        weight.Z >= 0 && weight.W >= 0 &&
                        weight.X + weight.Y + weight.Z + weight.W <= 1.001f),
                    $"outfit_02 normalized blend weights [{meshEntry.Index}]");
                True(
                    skinnedMesh.BlendWeights.Zip(skinnedMesh.BlendIndices).All(item =>
                        (item.First.X <= 0.00001f || item.Second.X < 16) &&
                        (item.First.Y <= 0.00001f || item.Second.Y < 16) &&
                        (item.First.Z <= 0.00001f || item.Second.Z < 16) &&
                        (item.First.W <= 0.00001f || item.Second.W < 16)),
                    $"outfit_02 active blend indices [{meshEntry.Index}]");
            }

            Matrix4x4 leftEye = SmoNodeTransformDecoder.ResolveModelWorldMatrix(
                datingDocument, datingDocument.Objects[82]);
            Matrix4x4 rightEye = SmoNodeTransformDecoder.ResolveModelWorldMatrix(
                datingDocument, datingDocument.Objects[86]);
            True(leftEye.M41 > 0 && rightEye.M41 < 0,
                "outfit_02 eyes preserve left/right bind placement");
            True(
                MathF.Abs(leftEye.M42 - 144.20456f) < 0.01f &&
                MathF.Abs(rightEye.M42 - 144.20456f) < 0.01f,
                "outfit_02 eyes use inverse-bind world height");
        }
        else if (Path.GetFileName(path).Equals(
                     "bloom_dating_outfit_03.smo", StringComparison.OrdinalIgnoreCase))
        {
            foreach (int faceMeshIndex in new[] { 116, 118 })
            {
                SmoMesh faceMesh = SmoMeshDecoder.Decode(document, document.Objects[faceMeshIndex]);
                True(faceMesh.HasDiffuseColors,
                    $"outfit_03 face [{faceMeshIndex}] vertex diffuse");
                int darkVertices = faceMesh.DiffuseColorsArgb.Count(color =>
                    ((color >> 16) & 0xFF) +
                    ((color >> 8) & 0xFF) +
                    (color & 0xFF) <= 96);
                Console.WriteLine(
                    $"outfit_03 face [{faceMeshIndex}]: " +
                    $"diffuse colors={faceMesh.DiffuseColorsArgb.Distinct().Count()}, " +
                    $"dark vertices={darkVertices}/{faceMesh.VertexCount}, " +
                    $"darkest={string.Join(",", faceMesh.DiffuseColorsArgb
                        .Distinct()
                        .OrderBy(color => ((color >> 16) & 0xFF) +
                                          ((color >> 8) & 0xFF) + (color & 0xFF))
                        .Take(5).Select(color => $"0x{color:X8}"))}");
                True(darkVertices > 0,
                    $"outfit_03 face [{faceMeshIndex}] dark eyelash diffuse");
                True(
                    SmoVertexColorUsage.ShouldModulateTexture(faceMesh),
                    $"outfit_03 face [{faceMeshIndex}] vertex diffuse remains renderable");
            }
        }
        else if (Path.GetFileName(path).Equals(
                     "knut.smo", StringComparison.OrdinalIgnoreCase))
        {
            SmoObjectEntry glassesMesh = document.Objects[6];
            Equal(SmoClassIds.MeshData, glassesMesh.TypeHash,
                "Knut glasses mesh object");
            True(
                SmoRigidBindingResolver.ResolveAnimationNodeObjectIndex(
                    document, glassesMesh) == 2,
                "Knut glasses bind to their animated render node");
            Equal("Knut_TEMP_glasses", document.Objects[2].Name,
                "Knut glasses animation target name");

            IReadOnlyDictionary<int, SmoTextureBinding> knutBindings =
                SmoTextureBindingResolver.ResolveAll(document);
            SmoMesh decodedGlasses = decodedMeshes.Single(mesh =>
                mesh.ObjectIndex == 6);
            True(!knutBindings.TryGetValue(
                     6, out SmoTextureBinding? glassesBinding) ||
                 glassesBinding.Texture is null,
                "Knut glasses keep their solid-color material");
            True(decodedGlasses.HasTextureCoordinates,
                "Knut glasses UV channel");
            True(decodedGlasses.DiffuseColorsArgb.All(color =>
                    (color & 0x00FFFFFF) == 0),
                "Knut glasses black vertex-color sentinel");

            string animationPath = Path.Combine(
                Path.GetDirectoryName(path)!, "Knid.san");
            True(File.Exists(animationPath), "Knut regression animation is present");
            True(
                SmoAnimationDecoder.TryDecode(
                    animationPath, out SmoAnimationClip? animation, out _),
                "Knut regression animation decodes");
            True(
                animation!.Tracks.Any(track =>
                    track.NodeName.Equals(
                        "Knut_TEMP_glasses", StringComparison.OrdinalIgnoreCase) &&
                    track.Positions.Count > 1 && track.Rotations.Count > 1),
                "Knut glasses have an animated transform track");
        }
        else if (Path.GetFileName(path).Equals(
                     "bloom_dating_outfit_01.smo", StringComparison.OrdinalIgnoreCase))
        {
            foreach (int faceMeshIndex in new[] { 120, 124 })
            {
                SmoMesh faceMesh = SmoMeshDecoder.Decode(document, document.Objects[faceMeshIndex]);
                True(faceMesh.HasNormals,
                    $"outfit_01 face [{faceMeshIndex}] stored normals");
                True(
                    faceMesh.Normals.All(normal =>
                        MathF.Abs(normal.LengthSquared() - 1) < 0.001f),
                    $"outfit_01 face [{faceMeshIndex}] normalized normals");
            }
        }
        else if (Path.GetFileName(path).Equals(
                     "prince_dating_outfit_01.smo", StringComparison.OrdinalIgnoreCase))
        {
            foreach (int faceMeshIndex in new[] { 131, 133 })
            {
                SmoMesh faceMesh = SmoMeshDecoder.Decode(document, document.Objects[faceMeshIndex]);
                True(faceMesh.HasDiffuseColors,
                    $"prince outfit_01 face [{faceMeshIndex}] vertex diffuse");
                True(
                    SmoVertexColorUsage.ShouldModulateTexture(faceMesh),
                    $"prince outfit_01 face [{faceMeshIndex}] vertex diffuse remains renderable");
                True(faceMesh.HasNormals,
                    $"prince outfit_01 face [{faceMeshIndex}] stored normals");
                int conflictingUvVertices = faceMesh.TextureCoordinates
                    .Select((uv, index) => (Uv: uv, Color: faceMesh.DiffuseColorsArgb[index]))
                    .GroupBy(item => item.Uv)
                    .Where(group => group.Select(item => item.Color).Distinct().Count() > 1)
                    .Sum(group => group.Count());
                Console.WriteLine(
                    $"prince outfit_01 face [{faceMeshIndex}]: " +
                    $"vertices={faceMesh.VertexCount}, " +
                    $"diffuse colors={faceMesh.DiffuseColorsArgb.Distinct().Count()}, " +
                    $"normals={faceMesh.Normals.Distinct().Count()}, " +
                    $"conflicting UV vertices={conflictingUvVertices}, " +
                    $"normal lengths={faceMesh.Normals.Min(normal => normal.Length()):G5}.." +
                    $"{faceMesh.Normals.Max(normal => normal.Length()):G5}, " +
                    $"darkest={string.Join(",", faceMesh.DiffuseColorsArgb
                        .Distinct()
                        .OrderBy(color => ((color >> 16) & 0xFF) +
                                          ((color >> 8) & 0xFF) + (color & 0xFF))
                        .Take(5).Select(color => $"0x{color:X8}"))}");
                for (int triangle = 0; triangle < faceMesh.TriangleIndices.Length; triangle += 3)
                {
                    int ia = checked((int)faceMesh.TriangleIndices[triangle]);
                    int ib = checked((int)faceMesh.TriangleIndices[triangle + 1]);
                    int ic = checked((int)faceMesh.TriangleIndices[triangle + 2]);
                    Vector2 a = faceMesh.TextureCoordinates[ia];
                    Vector2 b = faceMesh.TextureCoordinates[ib];
                    Vector2 c = faceMesh.TextureCoordinates[ic];
                    float uvArea = MathF.Abs(
                        (b.X - a.X) * (c.Y - a.Y) - (b.Y - a.Y) * (c.X - a.X));
                    if (uvArea > 0.0000001f)
                        continue;
                    Console.WriteLine(
                        $"  degenerate triangle {triangle / 3}: indices={ia},{ib},{ic}; " +
                        $"uv={a},{b},{c}; colors=0x{faceMesh.DiffuseColorsArgb[ia]:X8}," +
                        $"0x{faceMesh.DiffuseColorsArgb[ib]:X8},0x{faceMesh.DiffuseColorsArgb[ic]:X8}; " +
                        $"positions={faceMesh.Positions[ia]},{faceMesh.Positions[ib]}," +
                        $"{faceMesh.Positions[ic]}");
                }
            }

            SmoMesh body = SmoMeshDecoder.Decode(document, document.Objects[107]);
            True(
                SmoVertexColorUsage.ShouldModulateTexture(body),
                "prince outfit_01 body [107] black-ended gradient remains renderable");
        }
        else if (Path.GetFileName(path).Equals(
                     "prince_dating_outfit_02.smo", StringComparison.OrdinalIgnoreCase))
        {
            True(
                bindings.TryGetValue(108, out SmoTextureBinding? headBinding) &&
                headBinding.Texture?.Name == "sky",
                "prince outfit_02 mesh 108 inherits preceding sky material");
        }
        else if (Path.GetFileName(path).Equals(
                     "prince_dating_outfit_03.smo", StringComparison.OrdinalIgnoreCase))
        {
            foreach (int headMeshIndex in new[] { 66, 68 })
            {
                True(
                    bindings.TryGetValue(
                        headMeshIndex, out SmoTextureBinding? headBinding) &&
                    headBinding.Texture?.Name == "sky",
                    $"prince outfit_03 mesh {headMeshIndex} inherits parent sky material");
            }
        }
    }

    private static void CheckKnownFile(
        string corpusPath,
        string fileName,
        int expectedObjects,
        int expectedMeshes)
    {
        string path = Path.Combine(corpusPath, fileName);
        if (!File.Exists(path))
        {
            Console.WriteLine($"Known sample not present; skipped: {path}");
            return;
        }

        SmoDocument document = SmoDocument.Load(path);
        Equal(expectedObjects, document.Objects.Count, $"{fileName} objects");
        SmoObjectEntry[] meshEntries = document.Objects
            .Where(item => item.TypeHash == SmoClassIds.MeshData)
            .ToArray();
        Equal(
            expectedMeshes,
            meshEntries.Length,
            $"{fileName} meshes");

        var decodedMeshes = new List<SmoMesh>(meshEntries.Length);
        foreach (SmoObjectEntry entry in meshEntries)
        {
            True(
                SmoMeshDecoder.TryDecode(
                    document,
                    entry,
                    out SmoMesh? mesh,
                    out string error),
                $"{fileName} mesh [{entry.Index}] decodes exactly: {error}");
            decodedMeshes.Add(mesh!);
        }

        Equal(expectedMeshes, decodedMeshes.Count, $"{fileName} decoded meshes");

        if (fileName.Equals("fish.smo", StringComparison.OrdinalIgnoreCase))
        {
            SmoMesh mesh = decodedMeshes.Single();
            Equal(SmoMeshDecoder.E1Marker, mesh.Marker, "fish marker");
            Equal(0x093Eu, mesh.VertexFormat, "fish vertex format");
            Equal(44, mesh.Stride, "fish serialized stride");
            Equal(56, mesh.RuntimeStride, "fish runtime stride");
            True(mesh.HasSkinningData, "fish compressed skinning attributes");
        }

        if (fileName.Equals("loading.smo", StringComparison.OrdinalIgnoreCase))
        {
            Equal(
                2,
                decodedMeshes.Count(mesh => mesh.HasTextureCoordinates),
                "loading confirmed UV meshes");
            True(
                decodedMeshes
                    .SelectMany(mesh => mesh.TextureCoordinates)
                    .All(uv => float.IsFinite(uv.X) && float.IsFinite(uv.Y)),
                "loading UV coordinates are finite");

            IReadOnlyDictionary<int, SmoTextureBinding> bindings =
                SmoTextureBindingResolver.ResolveAll(document);
            SmoTextureBinding[] textured = bindings.Values
                .Where(binding => binding.Texture is not null)
                .ToArray();
            Equal(1, textured.Length, "loading resolved texture bindings");
            SmoTexture texture = textured.Single().Texture!;
            Equal("load_default", texture.Name, "loading texture name");
            Equal((ushort)0x32E3, texture.FormatCode, "loading texture format");
            Equal(
                checked(texture.Width * texture.Height * 4),
                texture.Bgra32Pixels.Length,
                "loading decoded texture size");
        }
    }

    private static void CheckE0FinalIndices(string corpusPath)
    {
        string path = Path.Combine(
            corpusPath,
            "Winx Club",
            "Media",
            "Levels",
            "Gardenia",
            "kt.smo");
        if (!File.Exists(path))
        {
            Console.WriteLine($"Known E0 sample not present; skipped: {path}");
            return;
        }

        SmoDocument document = SmoDocument.Load(path);
        SmoMesh[] meshes = document.Objects
            .Where(item => item.TypeHash == SmoClassIds.MeshData)
            .Select(item => SmoMeshDecoder.Decode(document, item))
            .ToArray();

        True(
            meshes.Any(mesh => mesh.StripIndices.SequenceEqual(
                new ushort[] { 2, 0, 1, 3 })),
            "E0 q+4 word is preserved when it contains two final strip indices");
    }

    private static void WriteUInt32(byte[] data, int offset, uint value) =>
        BinaryPrimitives.WriteUInt32LittleEndian(data.AsSpan(offset, sizeof(uint)), value);

    private static bool IsFinite(Matrix4x4 matrix) =>
        float.IsFinite(matrix.M11) && float.IsFinite(matrix.M12) &&
        float.IsFinite(matrix.M13) && float.IsFinite(matrix.M14) &&
        float.IsFinite(matrix.M21) && float.IsFinite(matrix.M22) &&
        float.IsFinite(matrix.M23) && float.IsFinite(matrix.M24) &&
        float.IsFinite(matrix.M31) && float.IsFinite(matrix.M32) &&
        float.IsFinite(matrix.M33) && float.IsFinite(matrix.M34) &&
        float.IsFinite(matrix.M41) && float.IsFinite(matrix.M42) &&
        float.IsFinite(matrix.M43) && float.IsFinite(matrix.M44);

    private static string GetTextureSample(SmoTexture texture, Vector2 uv)
    {
        int x = Math.Clamp((int)MathF.Round(uv.X * (texture.Width - 1)), 0, texture.Width - 1);
        int y = Math.Clamp((int)MathF.Round(uv.Y * (texture.Height - 1)), 0, texture.Height - 1);
        int offset = (y * texture.Width + x) * 4;
        ReadOnlySpan<byte> pixels = texture.Bgra32Pixels.Span;
        return $"({pixels[offset]},{pixels[offset + 1]},{pixels[offset + 2]},{pixels[offset + 3]})";
    }

    private sealed record CorpusOptions(string? Path, int? SampleCount, int Seed);

    private static void True(bool condition, string name)
    {
        _assertionCount++;
        if (!condition)
            throw new InvalidOperationException($"Assertion failed: {name}");
    }

    private static void Equal<T>(T expected, T actual, string name)
        where T : notnull
    {
        _assertionCount++;
        if (!EqualityComparer<T>.Default.Equals(expected, actual))
            throw new InvalidOperationException(
                $"Assertion failed: {name}; expected {expected}, actual {actual}");
    }
}
