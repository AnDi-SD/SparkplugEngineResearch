using System.Buffers.Binary;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;

namespace SMOTextureTool.Core;

public sealed class SmoDocument
{
    private static ReadOnlySpan<byte> TextureSignature =>
        [0x2B, 0x08, 0xEA, 0x78, 0x53, 0x42, 0x4F, 0x4F];
    private static ReadOnlySpan<byte> MaterialSignature =>
        [0x8B, 0x34, 0x60, 0x61, 0x53, 0x42, 0x4F, 0x4F];
    private static ReadOnlySpan<byte> MeshSignature =>
        [0xF0, 0x4C, 0xC3, 0x33, 0x53, 0x42, 0x4F, 0x4F];
    private static ReadOnlySpan<byte> ModelSignature =>
        [0xDB, 0x77, 0x32, 0x76, 0x53, 0x42, 0x4F, 0x4F];

    private const int FileSizeOffset = 0x0C;
    private const int DataStartOffset = 0x14;
    private const int DataSizeOffset = 0x18;
    private const int MaximumDimension = TextureInfo.MaximumCurrentHeaderDimension;
    private const int SerializedTextureDataMarkerOffset = 0x3C;
    private const byte SerializedTextureDataMarker = 0;

    private readonly byte[] _data;

    private SmoDocument(byte[] data, IReadOnlyList<TextureInfo> textures)
    {
        _data = data;
        Textures = textures;
    }

    public IReadOnlyList<TextureInfo> Textures { get; }
    public int Length => _data.Length;

    public static SmoDocument Load(string path) => Parse(File.ReadAllBytes(path));

    public static SmoDocument Parse(ReadOnlySpan<byte> source)
    {
        if (source.Length < 0x1C)
            throw new SmoFormatException("Файл слишком мал для заголовка SMO.");

        uint declaredFileSize = ReadUInt32(source, FileSizeOffset);
        uint dataStart = ReadUInt32(source, DataStartOffset);
        uint declaredDataSize = ReadUInt32(source, DataSizeOffset);

        if (declaredFileSize != source.Length)
            throw new SmoFormatException(
                $"Размер в заголовке ({declaredFileSize}) не совпадает с фактическим ({source.Length}).");

        if (dataStart > source.Length || declaredDataSize != source.Length - dataStart)
            throw new SmoFormatException("Некорректны границы секции данных SMO.");

        byte[] data = source.ToArray();
        var textures = FindTextures(data);
        return new SmoDocument(data, textures);
    }

    public Image<Rgba32> Decode(TextureInfo texture)
    {
        EnsureOwnedTexture(texture);
        var image = new Image<Rgba32>(texture.Width, texture.Height);
        int offset = texture.PixelDataOffset;

        image.ProcessPixelRows(accessor =>
        {
            for (int y = 0; y < texture.Height; y++)
            {
                Span<Rgba32> row = accessor.GetRowSpan(y);
                for (int x = 0; x < texture.Width; x++, offset += 4)
                {
                    row[x] = texture.Layout switch
                    {
                        TextureLayout.Abgr => new Rgba32(
                            _data[offset + 3], _data[offset + 2],
                            _data[offset + 1], _data[offset]),
                        TextureLayout.Bgra => new Rgba32(
                            _data[offset + 2], _data[offset + 1],
                            _data[offset], _data[offset + 3]),
                        _ => throw new SmoFormatException("Неизвестная раскладка пикселей.")
                    };
                }
            }
        });

        return image;
    }

    public bool TryApplyVertexColors(
        TextureInfo texture, Image<Rgba32> image)
        => TryApplyVertexColors(texture, image, out _);

    public bool TryApplyVertexColors(
        TextureInfo texture,
        Image<Rgba32> image,
        out VertexColorBindingInfo? binding)
    {
        binding = null;
        EnsureOwnedTexture(texture);
        if (texture.ContentKind != TextureContentKind.Monochrome ||
            texture.Material?.UsesColorModulation != true ||
            image.Width < 1 || image.Height < 1 ||
            !TryFindOwningModelMesh(
                texture, out int modelOffset, out int meshOffset))
            return false;

        return TryRasterizeVertexColors(
            modelOffset, meshOffset, image, out binding);
    }

    private bool TryFindOwningModelMesh(
        TextureInfo texture, out int modelOffset, out int meshOffset)
    {
        modelOffset = -1;
        meshOffset = -1;
        MaterialReferenceInfo material = texture.Material!;
        if (material.BlockOffset <= 0)
            return false;

        modelOffset = _data.AsSpan(0, material.BlockOffset)
            .LastIndexOf(ModelSignature);
        if (modelOffset < 0)
            return false;

        bool containsMaterial = false;
        int offset = modelOffset + ModelSignature.Length;
        while (offset < _data.Length)
        {
            if (!TryReadDataBlockHeader(
                    _data, offset, out int fieldType,
                    out int headerSize, out uint payloadSize))
                return false;

            int payloadOffset = offset + headerSize;
            int payloadLength = checked((int)payloadSize);
            int payloadEnd = checked(payloadOffset + payloadLength);
            if (fieldType == 0)
            {
                if (material.BlockOffset >= payloadOffset &&
                    material.BlockOffset < payloadEnd &&
                    _data.AsSpan(payloadOffset, payloadLength)
                        .IndexOf(MaterialSignature) >= 0)
                {
                    containsMaterial = true;
                }
                else if (containsMaterial)
                {
                    int relativeMesh = _data.AsSpan(payloadOffset, payloadLength)
                        .IndexOf(MeshSignature);
                    if (relativeMesh >= 0)
                    {
                        meshOffset = payloadOffset + relativeMesh;
                        return true;
                    }
                }
            }

            if (payloadEnd <= offset ||
                payloadEnd > _data.Length ||
                (offset > material.BlockOffset &&
                 _data.AsSpan(offset).StartsWith(ModelSignature)))
                return false;
            offset = payloadEnd;
        }

        return false;
    }

    private bool TryRasterizeVertexColors(
        int modelOffset,
        int meshOffset,
        Image<Rgba32> image,
        out VertexColorBindingInfo? binding)
    {
        binding = null;
        int offset = meshOffset + MeshSignature.Length;
        if (!CanRead(_data, offset, 1))
            return false;

        byte field = _data[offset++];
        offset += field switch
        {
            0xE1 => 21,
            0xE0 => 4,
            _ => int.MaxValue
        };
        if (offset < 0 || !CanRead(_data, offset, 12))
            return false;

        uint primitiveType = ReadUInt32(_data, offset);
        uint storedIndexCount = ReadUInt32(_data, offset + 4);
        offset += 12;
        if (primitiveType != 3 || storedIndexCount > int.MaxValue / 2 ||
            !CanRead(_data, offset, checked((int)storedIndexCount * 2)))
            return false;

        var indices = new List<ushort>(checked((int)storedIndexCount + 2));
        for (int index = 0; index < storedIndexCount; index++)
            indices.Add(BinaryPrimitives.ReadUInt16LittleEndian(
                _data.AsSpan(offset + index * 2, 2)));
        offset += checked((int)storedIndexCount * 2);

        if (!CanRead(_data, offset, 4))
            return false;
        ushort extra0 = BinaryPrimitives.ReadUInt16LittleEndian(
            _data.AsSpan(offset, 2));
        ushort extra1 = BinaryPrimitives.ReadUInt16LittleEndian(
            _data.AsSpan(offset + 2, 2));
        if (extra0 != 52685 && extra1 != 52685)
        {
            indices.Add(extra0);
            indices.Add(extra1);
            offset += 4;
        }

        if (!CanRead(_data, offset, 12))
            return false;
        uint vertexFormat = ReadUInt32(_data, offset);
        uint vertexCountValue = ReadUInt32(_data, offset + 4);
        offset += 12;

        // Known layout 0x940: XYZ + normal, ARGB diffuse at +24, UV at +28.
        const int stride = 36;
        const int diffuseOffset = 24;
        const int uvOffset = 28;
        if (vertexFormat != 0x940 || vertexCountValue > int.MaxValue ||
            !CanRead(_data, offset, checked((int)vertexCountValue * stride)))
            return false;

        int vertexCount = (int)vertexCountValue;
        var vertices = new TintVertex[vertexCount];
        for (int index = 0; index < vertexCount; index++)
        {
            int vertexOffset = offset + index * stride;
            uint color = ReadUInt32(_data, vertexOffset + diffuseOffset);
            vertices[index] = new TintVertex(
                ReadSingle(_data, vertexOffset + uvOffset),
                ReadSingle(_data, vertexOffset + uvOffset + 4),
                (byte)(color >> 16),
                (byte)(color >> 8),
                (byte)color);
        }

        var tint = new Rgba32[checked(image.Width * image.Height)];
        var usedVertices = new bool[vertexCount];
        bool painted = false;
        int triangleCount = 0;
        int overlappingPixelWrites = 0;
        int conflictingPixelWrites = 0;
        for (int index = 0; index + 2 < indices.Count; index++)
        {
            ushort ia = indices[index];
            ushort ib = indices[index + 1];
            ushort ic = indices[index + 2];
            if (ia == ib || ib == ic || ia == ic ||
                ia >= vertexCount || ib >= vertexCount || ic >= vertexCount)
                continue;

            if ((index & 1) != 0)
                (ia, ib) = (ib, ia);
            if (IsDegenerateUvTriangle(
                    vertices[ia], vertices[ib], vertices[ic]))
                continue;
            triangleCount++;
            usedVertices[ia] = true;
            usedVertices[ib] = true;
            usedVertices[ic] = true;
            bool trianglePainted = RasterizeTintTriangle(
                vertices[ia], vertices[ib], vertices[ic],
                tint, image.Width, image.Height,
                ref overlappingPixelWrites,
                ref conflictingPixelWrites);
            if (!trianglePainted)
                continue;
            painted = true;
        }

        if (!painted)
            return false;

        image.ProcessPixelRows(accessor =>
        {
            for (int y = 0; y < image.Height; y++)
            {
                Span<Rgba32> row = accessor.GetRowSpan(y);
                for (int x = 0; x < image.Width; x++)
                {
                    Rgba32 color = tint[y * image.Width + x];
                    if (color.A == 0)
                        continue;
                    Rgba32 pixel = row[x];
                    row[x] = new Rgba32(
                        (byte)(pixel.R * color.R / 255),
                        (byte)(pixel.G * color.G / 255),
                        (byte)(pixel.B * color.B / 255),
                        pixel.A);
                }
            }
        });
        int[] influencingVertices = Enumerable.Range(0, vertexCount)
            .Where(index => usedVertices[index])
            .ToArray();
        binding = new VertexColorBindingInfo(
            modelOffset,
            meshOffset,
            vertexCount,
            triangleCount,
            influencingVertices,
            overlappingPixelWrites,
            conflictingPixelWrites);
        return true;
    }

    private static bool RasterizeTintTriangle(
        TintVertex a, TintVertex b, TintVertex c,
        Rgba32[] tint, int width, int height,
        ref int overlappingPixelWrites,
        ref int conflictingPixelWrites)
    {
        float ax = a.U * (width - 1);
        float ay = a.V * (height - 1);
        float bx = b.U * (width - 1);
        float by = b.V * (height - 1);
        float cx = c.U * (width - 1);
        float cy = c.V * (height - 1);
        float area = Edge(ax, ay, bx, by, cx, cy);
        if (MathF.Abs(area) < 0.000001f)
            return false;

        int minX = Math.Clamp((int)MathF.Floor(MathF.Min(ax, MathF.Min(bx, cx))), 0, width - 1);
        int maxX = Math.Clamp((int)MathF.Ceiling(MathF.Max(ax, MathF.Max(bx, cx))), 0, width - 1);
        int minY = Math.Clamp((int)MathF.Floor(MathF.Min(ay, MathF.Min(by, cy))), 0, height - 1);
        int maxY = Math.Clamp((int)MathF.Ceiling(MathF.Max(ay, MathF.Max(by, cy))), 0, height - 1);
        bool painted = false;
        for (int y = minY; y <= maxY; y++)
        {
            for (int x = minX; x <= maxX; x++)
            {
                float px = x + 0.5f;
                float py = y + 0.5f;
                float wa = Edge(bx, by, cx, cy, px, py) / area;
                float wb = Edge(cx, cy, ax, ay, px, py) / area;
                float wc = 1f - wa - wb;
                if (MathF.Min(wa, MathF.Min(wb, wc)) < -0.00001f)
                    continue;

                var color = new Rgba32(
                    ClampByte(wa * a.Red + wb * b.Red + wc * c.Red),
                    ClampByte(wa * a.Green + wb * b.Green + wc * c.Green),
                    ClampByte(wa * a.Blue + wb * b.Blue + wc * c.Blue),
                    255);
                int pixelIndex = y * width + x;
                Rgba32 previous = tint[pixelIndex];
                if (previous.A != 0)
                {
                    overlappingPixelWrites++;
                    if (Math.Max(
                            Math.Abs(previous.R - color.R),
                            Math.Max(
                                Math.Abs(previous.G - color.G),
                                Math.Abs(previous.B - color.B))) > 8)
                        conflictingPixelWrites++;
                }
                tint[pixelIndex] = color;
                painted = true;
            }
        }
        return painted;
    }

    private static bool IsDegenerateUvTriangle(
        TintVertex a, TintVertex b, TintVertex c) =>
        MathF.Abs(Edge(a.U, a.V, b.U, b.V, c.U, c.V)) < 0.0000001f;

    private static float Edge(
        float ax, float ay, float bx, float by, float px, float py) =>
        (px - ax) * (by - ay) - (py - ay) * (bx - ax);

    private static byte ClampByte(float value) =>
        (byte)Math.Clamp((int)MathF.Round(value), 0, 255);

    private static float ReadSingle(ReadOnlySpan<byte> data, int offset) =>
        BitConverter.Int32BitsToSingle(
            BinaryPrimitives.ReadInt32LittleEndian(data.Slice(offset, 4)));

    private readonly record struct TintVertex(
        float U, float V, byte Red, byte Green, byte Blue);

    public void ExportTexture(TextureInfo texture, string path)
    {
        using Image<Rgba32> image = Decode(texture);
        image.SaveAsPng(path);
    }

    public void ExportAll(string directory)
    {
        Directory.CreateDirectory(directory);
        foreach (TextureInfo texture in Textures)
            ExportTexture(texture, Path.Combine(directory, texture.FileName));
    }

    public byte[] Repack(IReadOnlyDictionary<int, string> replacementFiles) =>
        Repack(replacementFiles, allowNonPowerOfTwoResize: false);

    public byte[] Repack(
        IReadOnlyDictionary<int, string> replacementFiles,
        bool allowNonPowerOfTwoResize)
    {
        var replacements = new List<(TextureInfo Texture, Image<Rgba32> Image)>();
        try
        {
            foreach (TextureInfo texture in Textures)
            {
                if (!replacementFiles.TryGetValue(texture.Index, out string? path))
                    continue;

                Image<Rgba32> image = Image.Load<Rgba32>(path);
                ValidateReplacement(texture, image, allowNonPowerOfTwoResize);
                replacements.Add((texture, image));
            }
            return RepackLoadedImages(replacements);
        }
        finally
        {
            foreach (var replacement in replacements)
                replacement.Image.Dispose();
        }
    }

    public byte[] RepackEncodedImages(
        IReadOnlyDictionary<int, ReadOnlyMemory<byte>> replacementImages,
        bool allowNonPowerOfTwoResize = false)
    {
        var replacements = new List<(TextureInfo Texture, Image<Rgba32> Image)>();
        try
        {
            foreach (TextureInfo texture in Textures)
            {
                if (!replacementImages.TryGetValue(
                        texture.Index, out ReadOnlyMemory<byte> encoded))
                    continue;

                Image<Rgba32> image = Image.Load<Rgba32>(encoded.Span);
                ValidateReplacement(texture, image, allowNonPowerOfTwoResize);
                replacements.Add((texture, image));
            }
            return RepackLoadedImages(replacements);
        }
        finally
        {
            foreach (var replacement in replacements)
                replacement.Image.Dispose();
        }
    }

    private byte[] RepackLoadedImages(
        IReadOnlyCollection<(TextureInfo Texture, Image<Rgba32> Image)> replacements)
    {
        byte[] result = _data.ToArray();
        foreach ((TextureInfo texture, Image<Rgba32> image) in
                 replacements.OrderByDescending(item => item.Texture.PixelDataOffset))
        {
            result = ReplaceOne(result, texture, image);
        }

        PatchFileHeader(result);
        ValidateRepackedFile(result, Textures.Count, replacements);
        return result;
    }

    private static List<TextureInfo> FindTextures(byte[] data)
    {
        var result = new List<TextureInfo>();
        ReadOnlySpan<byte> span = data;
        List<int> materialOffsets = FindSignatureOffsets(span, MaterialSignature);
        int searchFrom = 0;

        while (searchFrom <= span.Length - TextureSignature.Length)
        {
            int relative = span[searchFrom..].IndexOf(TextureSignature);
            if (relative < 0)
                break;

            int blockOffset = searchFrom + relative;
            TextureInfo? texture = TryParseTexture(span, blockOffset, result.Count + 1);
            if (texture is not null)
            {
                texture = texture with
                {
                    Material = FindMaterialReference(span, materialOffsets, texture)
                };
                result.Add(texture);
            }

            searchFrom = blockOffset + TextureSignature.Length;
        }

        return result;
    }

    private static List<int> FindSignatureOffsets(
        ReadOnlySpan<byte> data, ReadOnlySpan<byte> signature)
    {
        var result = new List<int>();
        int searchFrom = 0;
        while (searchFrom <= data.Length - signature.Length)
        {
            int relative = data[searchFrom..].IndexOf(signature);
            if (relative < 0)
                break;
            int offset = searchFrom + relative;
            result.Add(offset);
            searchFrom = offset + signature.Length;
        }
        return result;
    }

    private static MaterialReferenceInfo? FindMaterialReference(
        ReadOnlySpan<byte> data,
        IReadOnlyList<int> materialOffsets,
        TextureInfo texture)
    {
        int materialListIndex = -1;
        for (int index = 0; index < materialOffsets.Count; index++)
        {
            if (materialOffsets[index] >= texture.BlockOffset)
                break;
            materialListIndex = index;
        }
        if (materialListIndex < 0)
            return null;

        int materialOffset = materialOffsets[materialListIndex];
        long textureEnd = (long)texture.PixelDataOffset + texture.PixelDataSize;
        int bestContainerOffset = -1;
        uint bestContainerSize = 0;
        long bestTail = long.MaxValue;

        // 0xEA = field type 10 with serializer size code 7 (UInt32).
        for (int offset = materialOffset + MaterialSignature.Length;
             offset < texture.BlockOffset;
             offset++)
        {
            if (data[offset] != 0xEA || !CanRead(data, offset + 1, 4))
                continue;

            uint containerSize = ReadUInt32(data, offset + 1);
            long containerEnd = (long)offset + 5 + containerSize;
            long tail = containerEnd - textureEnd;
            if (tail < 0 || tail >= bestTail)
                continue;

            bestTail = tail;
            bestContainerOffset = offset;
            bestContainerSize = containerSize;
        }

        if (bestContainerOffset < 0 ||
            !TryReadMaterialPath(
                data, materialOffset, bestContainerOffset,
                out int passIndex, out int layerIndex,
                out uint layerClassId, out uint finalBlendOperation,
                out uint[] materialRenderStates,
                out uint[] layerTextureStates))
            return null;

        string layerClassName = layerClassId switch
        {
            0x234C576B => "spStdLayer",
            _ => $"class 0x{layerClassId:X8}"
        };
        return new MaterialReferenceInfo(
            materialListIndex + 1,
            materialOffset,
            bestContainerOffset,
            bestContainerSize,
            passIndex,
            layerIndex,
            layerClassId,
            layerClassName,
            finalBlendOperation,
            materialRenderStates,
            layerTextureStates);
    }

    private static bool TryReadMaterialPath(
        ReadOnlySpan<byte> data,
        int materialOffset,
        int textureContainerOffset,
        out int passIndex,
        out int layerIndex,
        out uint layerClassId,
        out uint finalBlendOperation,
        out uint[] materialRenderStates,
        out uint[] layerTextureStates)
    {
        passIndex = 0;
        layerIndex = 0;
        layerClassId = 0;
        finalBlendOperation = 0;
        materialRenderStates = [];
        layerTextureStates = [];
        int offset = materialOffset + MaterialSignature.Length;

        while (offset <= textureContainerOffset)
        {
            if (!TryReadDataBlockHeader(
                    data, offset, out int fieldType,
                    out int headerSize, out uint payloadSize))
                return false;

            int payloadOffset = offset + headerSize;
            if (fieldType == 0 && payloadSize == 11 * sizeof(uint))
            {
                materialRenderStates = ReadUInt32Array(data, payloadOffset, 11);
            }
            else if (fieldType == 3 && payloadSize == 4)
            {
                passIndex++;
                layerIndex = 0;
                finalBlendOperation = ReadUInt32(data, payloadOffset);
            }
            else if (fieldType == 4 && payloadSize == 4)
            {
                layerIndex++;
                layerClassId = ReadUInt32(data, payloadOffset);
                layerTextureStates = [];
            }
            else if (fieldType == 17 && payloadSize == 9 * sizeof(uint))
            {
                layerTextureStates = ReadUInt32Array(data, payloadOffset, 9);
            }

            if (offset == textureContainerOffset)
                return fieldType == 10 && passIndex > 0 && layerIndex > 0;

            long next = (long)payloadOffset + payloadSize;
            if (next <= offset || next > textureContainerOffset)
                return false;
            offset = (int)next;
        }

        return false;
    }

    private static uint[] ReadUInt32Array(
        ReadOnlySpan<byte> data, int offset, int count)
    {
        var values = new uint[count];
        for (int index = 0; index < count; index++)
            values[index] = ReadUInt32(data, offset + index * sizeof(uint));
        return values;
    }

    private static bool TryReadDataBlockHeader(
        ReadOnlySpan<byte> data,
        int offset,
        out int fieldType,
        out int headerSize,
        out uint payloadSize)
    {
        fieldType = 0;
        headerSize = 0;
        payloadSize = 0;
        if (!CanRead(data, offset, 1))
            return false;

        byte header = data[offset];
        fieldType = header & 0x1F;
        int sizeCode = header >> 5;
        headerSize = 1;
        if (fieldType == 0x1F)
        {
            if (!CanRead(data, offset + headerSize, 1))
                return false;
            fieldType = data[offset + headerSize];
            headerSize++;
        }

        switch (sizeCode)
        {
            case 0: payloadSize = 0; break;
            case 1: payloadSize = 1; break;
            case 2: payloadSize = 2; break;
            case 3: payloadSize = 4; break;
            case 4: payloadSize = 8; break;
            case 5:
                if (!CanRead(data, offset + headerSize, 1)) return false;
                payloadSize = data[offset + headerSize];
                headerSize++;
                break;
            case 6:
                if (!CanRead(data, offset + headerSize, 2)) return false;
                payloadSize = BinaryPrimitives.ReadUInt16LittleEndian(
                    data.Slice(offset + headerSize, 2));
                headerSize += 2;
                break;
            case 7:
                if (!CanRead(data, offset + headerSize, 4)) return false;
                payloadSize = ReadUInt32(data, offset + headerSize);
                headerSize += 4;
                break;
        }

        return CanRead(data, offset + headerSize, checked((int)payloadSize));
    }

    private static TextureInfo? TryParseTexture(
        ReadOnlySpan<byte> data, int blockOffset, int index)
    {
        if (!CanRead(data, blockOffset + 0x08, 4))
            return null;

        ushort format = (ushort)(ReadUInt32(data, blockOffset + 0x08) & 0xFFFF);
        return format switch
        {
            0x32E3 or 0x43E3 => TryCreateSerializedBgraTexture(
                data, index, blockOffset, format),
            0x29E3 => TryCreateTexture(
                data, index, blockOffset, blockOffset + 0x34,
                blockOffset + 0x28, blockOffset + 0x30, format, TextureLayout.Bgra),
            _ => null
        };
    }

    private static TextureInfo? TryCreateSerializedBgraTexture(
        ReadOnlySpan<byte> data,
        int index,
        int blockOffset,
        ushort format)
    {
        int markerOffset = checked(blockOffset + SerializedTextureDataMarkerOffset);
        if (!CanRead(data, markerOffset, 1))
            return null;
        if (data[markerOffset] != SerializedTextureDataMarker)
            throw new SmoFormatException(
                $"Текстура 0x{format:X4} по смещению 0x{blockOffset:X} содержит " +
                $"некорректный marker 0x{data[markerOffset]:X2} на +0x3C; ожидался 00.");

        return TryCreateTexture(
            data, index, blockOffset, blockOffset + 0x3D,
            blockOffset + 0x24, blockOffset + 0x28, format, TextureLayout.Bgra);
    }

    private static TextureInfo? TryCreateTexture(
        ReadOnlySpan<byte> data,
        int index,
        int blockOffset,
        int pixelOffset,
        int widthOffset,
        int heightOffset,
        ushort format,
        TextureLayout layout)
    {
        if (!CanRead(data, widthOffset, 4) || !CanRead(data, heightOffset, 4))
            return null;

        int width = ReadInt32(data, widthOffset);
        int height = ReadInt32(data, heightOffset);
        if (width is <= 0 or > MaximumDimension ||
            height is <= 0 or > MaximumDimension)
            return null;

        long size = (long)width * height * 4;
        if (pixelOffset < 0 || pixelOffset + size > data.Length)
            return null;
        if (size > uint.MaxValue ||
            !HasExpectedTextureBlockSizes(data, blockOffset, (uint)size, format))
            return null;

        TextureChannelInfo channels =
            AnalyzeChannels(data.Slice(pixelOffset, checked((int)size)), layout);
        return new TextureInfo(
            index, blockOffset, pixelOffset, width, height, format, layout, channels);
    }

    private static TextureChannelInfo AnalyzeChannels(
        ReadOnlySpan<byte> pixels, TextureLayout layout)
    {
        byte redMin = 255, redMax = 0;
        byte greenMin = 255, greenMax = 0;
        byte blueMin = 255, blueMax = 0;
        byte alphaMin = 255, alphaMax = 0;
        bool rgbChannelsIdentical = true;
        Span<bool> alphaValues = stackalloc bool[256];
        int alphaValueCount = 0;

        for (int offset = 0; offset < pixels.Length; offset += 4)
        {
            byte red, green, blue, alpha;
            if (layout == TextureLayout.Abgr)
            {
                alpha = pixels[offset];
                blue = pixels[offset + 1];
                green = pixels[offset + 2];
                red = pixels[offset + 3];
            }
            else
            {
                blue = pixels[offset];
                green = pixels[offset + 1];
                red = pixels[offset + 2];
                alpha = pixels[offset + 3];
            }

            redMin = Math.Min(redMin, red);
            redMax = Math.Max(redMax, red);
            greenMin = Math.Min(greenMin, green);
            greenMax = Math.Max(greenMax, green);
            blueMin = Math.Min(blueMin, blue);
            blueMax = Math.Max(blueMax, blue);
            alphaMin = Math.Min(alphaMin, alpha);
            alphaMax = Math.Max(alphaMax, alpha);
            rgbChannelsIdentical &= red == green && green == blue;
            if (!alphaValues[alpha])
            {
                alphaValues[alpha] = true;
                alphaValueCount++;
            }
        }

        return new TextureChannelInfo(
            redMin, redMax, greenMin, greenMax, blueMin, blueMax,
            alphaMin, alphaMax, alphaValueCount, rgbChannelsIdentical);
    }

    private static bool HasExpectedTextureBlockSizes(
        ReadOnlySpan<byte> data,
        int blockOffset,
        uint pixelBytes,
        ushort format)
    {
        (int Marker, int Size, uint Tail)[] fields = format switch
        {
            0x32E3 or 0x43E3 =>
            [
                (0x08, 0x09, 0x32),
                (0x19, 0x1A, 0x20),
                (0x1E, 0x1F, 0x1A)
            ],
            0x29E3 =>
            [
                (0x08, 0x09, 0x29),
                (0x10, 0x11, 0x20),
                (0x15, 0x16, 0x1A)
            ],
            _ => []
        };

        foreach ((int marker, int size, uint tail) in fields)
        {
            int markerOffset = blockOffset + marker;
            int sizeOffset = blockOffset + size;
            if (!CanRead(data, markerOffset, 1) ||
                !CanRead(data, sizeOffset, 4) ||
                (data[markerOffset] >> 5) != 7 ||
                ReadUInt32(data, sizeOffset) < checked(pixelBytes + tail))
                return false;
        }

        return true;
    }

    private static byte[] ReplaceOne(
        byte[] data, TextureInfo texture, Image<Rgba32> image)
    {
        byte[] pixels = EncodePixels(image, texture.Layout);
        int oldEnd = texture.PixelDataOffset + texture.PixelDataSize;
        byte[] result = new byte[
            checked(data.Length - texture.PixelDataSize + pixels.Length)];

        data.AsSpan(0, texture.PixelDataOffset).CopyTo(result);
        pixels.CopyTo(result.AsSpan(texture.PixelDataOffset));
        data.AsSpan(oldEnd).CopyTo(
            result.AsSpan(texture.PixelDataOffset + pixels.Length));

        if (image.Width != texture.Width || image.Height != texture.Height)
        {
            switch (texture.FormatCode)
            {
                case 0x32E3:
                case 0x43E3:
                    PatchSerializedBgraHeader(result, texture, image.Width, image.Height);
                    break;
                case 0x29E3:
                    PatchLegacyBgraHeader(result, texture, image.Width, image.Height);
                    break;
                default:
                    throw new SmoFormatException(
                        $"Неподдерживаемый формат текстуры 0x{texture.FormatCode:X4}.");
            }
        }

        return result;
    }

    private static byte[] EncodePixels(Image<Rgba32> image, TextureLayout layout)
    {
        byte[] result = new byte[checked(image.Width * image.Height * 4)];
        int offset = 0;

        image.ProcessPixelRows(accessor =>
        {
            for (int y = 0; y < image.Height; y++)
            {
                ReadOnlySpan<Rgba32> row = accessor.GetRowSpan(y);
                foreach (Rgba32 pixel in row)
                {
                    if (layout == TextureLayout.Abgr)
                    {
                        result[offset] = pixel.A;
                        result[offset + 1] = pixel.B;
                        result[offset + 2] = pixel.G;
                        result[offset + 3] = pixel.R;
                    }
                    else
                    {
                        result[offset] = pixel.B;
                        result[offset + 1] = pixel.G;
                        result[offset + 2] = pixel.R;
                        result[offset + 3] = pixel.A;
                    }

                    offset += 4;
                }
            }
        });

        return result;
    }

    private static void ValidateReplacement(
        TextureInfo texture,
        Image<Rgba32> image,
        bool allowNonPowerOfTwoResize)
    {
        if (image.Width is <= 0 or > MaximumDimension ||
            image.Height is <= 0 or > MaximumDimension)
            throw new SmoFormatException(
                $"Размер изображения находится вне диапазона 1–{MaximumDimension}.");

        bool resized = image.Width != texture.Width || image.Height != texture.Height;
        if (resized && !texture.CanResize)
            throw new SmoFormatException(
                $"Текстуру {texture.Index} формата 0x{texture.FormatCode:X4} " +
                "можно заменить только изображением исходного размера.");

        if (resized && !allowNonPowerOfTwoResize &&
            (!IsPowerOfTwo(image.Width) || !IsPowerOfTwo(image.Height)))
            throw new SmoFormatException(
                $"Размер текстуры {texture.Index} должен состоять из степеней двойки.");

        if (resized &&
            (image.Width > TextureInfo.MaximumCurrentHeaderDimension ||
             image.Height > TextureInfo.MaximumCurrentHeaderDimension))
            throw new SmoFormatException(
                $"Максимальная сторона для текущей схемы — " +
                $"{TextureInfo.MaximumCurrentHeaderDimension} пикселей.");

    }

    private static void PatchSerializedBgraHeader(
        Span<byte> data, TextureInfo texture, int width, int height)
    {
        int block = texture.BlockOffset;

        // E3/E1/E0 use serializer size code 7, so each marker is followed
        // by a complete UInt32 block size.
        AddPixelSizeDelta(data, block + 0x09, texture, width, height);
        AddPixelSizeDelta(data, block + 0x1A, texture, width, height);
        AddPixelSizeDelta(data, block + 0x1F, texture, width, height);
        WriteUInt32(data, block + 0x24, (uint)width);
        WriteUInt32(data, block + 0x28, (uint)height);
        WriteUInt32(data, block + 0x2C, 0);
        WriteUInt32(data, block + 0x30, ((uint)width << 8) | 1);
        WriteUInt32(data, block + 0x34, (uint)width << 10);
        WriteUInt32(data, block + 0x38, (uint)height << 8);
    }

    private static void PatchLegacyBgraHeader(
        Span<byte> data, TextureInfo texture, int width, int height)
    {
        int block = texture.BlockOffset;

        AddPixelSizeDelta(data, block + 0x09, texture, width, height);
        AddPixelSizeDelta(data, block + 0x11, texture, width, height);
        AddPixelSizeDelta(data, block + 0x16, texture, width, height);
        WriteUInt32(data, block + 0x1B, (uint)width);
        WriteUInt32(data, block + 0x1F, (uint)height);
        WriteUInt32(data, block + 0x28, (uint)width);
        WriteUInt32(data, block + 0x2C, checked((uint)width * 4));
        WriteUInt32(data, block + 0x30, (uint)height);
    }

    private static void AddPixelSizeDelta(
        Span<byte> data,
        int sizeOffset,
        TextureInfo texture,
        int width,
        int height)
    {
        long newPixelBytes = checked((long)width * height * 4);
        long updatedSize = checked(
            (long)ReadUInt32(data, sizeOffset) +
            newPixelBytes -
            texture.PixelDataSize);
        WriteUInt32(data, sizeOffset, checked((uint)updatedSize));
    }

    private static void PatchFileHeader(Span<byte> data)
    {
        uint dataStart = ReadUInt32(data, DataStartOffset);
        WriteUInt32(data, FileSizeOffset, checked((uint)data.Length));
        WriteUInt32(data, DataSizeOffset, checked((uint)data.Length) - dataStart);
    }

    private static void ValidateRepackedFile(
        byte[] data,
        int expectedTextureCount,
        IReadOnlyCollection<(TextureInfo Texture, Image<Rgba32> Image)> replacements)
    {
        SmoDocument reparsed = Parse(data);
        if (reparsed.Textures.Count != expectedTextureCount)
            throw new SmoFormatException(
                $"После пересборки изменилась структура текстур: ожидалось " +
                $"{expectedTextureCount}, найдено {reparsed.Textures.Count} " +
                $"({string.Join(", ", reparsed.Textures.Select(texture =>
                    $"0x{texture.BlockOffset:X}/{texture.Width}x{texture.Height}"))}).");

        foreach (var replacement in replacements)
        {
            TextureInfo? actual = reparsed.Textures.FirstOrDefault(
                item => item.Index == replacement.Texture.Index);
            if (actual is null ||
                actual.Width != replacement.Image.Width ||
                actual.Height != replacement.Image.Height)
                throw new SmoFormatException(
                    $"Не удалось проверить текстуру {replacement.Texture.Index} после пересборки.");
        }
    }

    private void EnsureOwnedTexture(TextureInfo texture)
    {
        if (texture.Index < 1 || texture.Index > Textures.Count ||
            Textures[texture.Index - 1] != texture)
            throw new ArgumentException("Текстура не принадлежит этому документу.", nameof(texture));
    }

    private static bool CanRead(ReadOnlySpan<byte> data, int offset, int length) =>
        offset >= 0 && length >= 0 && offset <= data.Length - length;

    private static bool IsPowerOfTwo(int value) => (value & (value - 1)) == 0;

    private static int ReadInt32(ReadOnlySpan<byte> data, int offset) =>
        BinaryPrimitives.ReadInt32LittleEndian(data.Slice(offset, 4));

    private static uint ReadUInt32(ReadOnlySpan<byte> data, int offset) =>
        BinaryPrimitives.ReadUInt32LittleEndian(data.Slice(offset, 4));

    private static void WriteUInt32(Span<byte> data, int offset, uint value) =>
        BinaryPrimitives.WriteUInt32LittleEndian(data.Slice(offset, 4), value);

}
