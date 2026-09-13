using System.Buffers.Binary;
using System.Runtime.InteropServices;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;
using SmoViewer.Core;
using CatalogDocument = SmoViewer.Core.SmoDocument;

namespace SMOTextureTool.Core;

public sealed class SmoDocument
{
    private static ReadOnlySpan<byte> MaterialSignature =>
        [0x8B, 0x34, 0x60, 0x61, 0x53, 0x42, 0x4F, 0x4F];
    private static ReadOnlySpan<byte> MeshSignature =>
        [0xF0, 0x4C, 0xC3, 0x33, 0x53, 0x42, 0x4F, 0x4F];
    private static ReadOnlySpan<byte> ModelSignature =>
        [0xDB, 0x77, 0x32, 0x76, 0x53, 0x42, 0x4F, 0x4F];

    private const int MaximumDimension = TextureInfo.MaximumCurrentHeaderDimension;
    private readonly byte[] _data;
    private readonly CatalogDocument _catalog;

    private SmoDocument(byte[] data, CatalogDocument catalog,
        IReadOnlyList<TextureInfo> textures, IReadOnlyList<string> unsupportedTextures)
    {
        _data = data;
        _catalog = catalog;
        Textures = textures;
        UnsupportedTextures = unsupportedTextures;
    }

    public IReadOnlyList<TextureInfo> Textures { get; }
    public IReadOnlyList<string> UnsupportedTextures { get; }
    public int Length => _data.Length;

    public static SmoDocument Load(string path) => Parse(File.ReadAllBytes(path));

    public static SmoDocument Parse(ReadOnlySpan<byte> source)
    {
        byte[] data = source.ToArray();
        // Shared original FFPS/FAT readers provide both metadata and bounded
        // structural diagnostics. No second header parser in this application.
        CatalogDocument catalog = CatalogDocument.ParseOwned(data);
        if (catalog.HasErrors)
            throw new SmoFormatException("Некорректен каталог объектов SMO: " +
                string.Join(" | ", catalog.Diagnostics.Where(item =>
                    item.Severity == SmoDiagnosticSeverity.Error).Select(item => item.Message)));
        var textures = FindTextures(catalog, out var unsupported);
        return new SmoDocument(data, catalog, textures, unsupported);
    }

    public Image<Rgba32> Decode(TextureInfo texture)
    {
        EnsureOwnedTexture(texture);
        var image = new Image<Rgba32>(texture.Width, texture.Height);
        ReadOnlyMemory<byte> pixels = texture.BgraPreview.IsEmpty
            ? _data.AsMemory(texture.PixelDataOffset, texture.PixelDataSize) : texture.BgraPreview;
        int offset = 0;

        image.ProcessPixelRows(accessor =>
        {
            ReadOnlySpan<byte> source = pixels.Span;
            for (int y = 0; y < texture.Height; y++)
            {
                Span<Rgba32> row = accessor.GetRowSpan(y);
                for (int x = 0; x < texture.Width; x++, offset += 4)
                {
                    row[x] = texture.Layout switch
                    {
                        TextureLayout.Abgr => new Rgba32(
                            source[offset + 3], source[offset + 2],
                            source[offset + 1], source[offset]),
                        TextureLayout.Bgra => new Rgba32(
                            source[offset + 2], source[offset + 1],
                            source[offset], source[offset + 3]),
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
        Repack(replacementFiles, allowNonPowerOfTwoResize: true);

    public byte[] Repack(
        IReadOnlyDictionary<int, string> replacementFiles,
        bool allowNonPowerOfTwoResize) =>
        RepackImages(replacementFiles.ToDictionary(pair => pair.Key,
            pair => (Func<Image<Rgba32>>)(() => TextureImageLoader.Load(pair.Value))),
            allowNonPowerOfTwoResize);

    public byte[] RepackEncodedImages(
        IReadOnlyDictionary<int, ReadOnlyMemory<byte>> replacementImages,
        bool allowNonPowerOfTwoResize = true) =>
        RepackImages(replacementImages.ToDictionary(pair => pair.Key,
            pair => (Func<Image<Rgba32>>)(() => TextureImageLoader.Load(pair.Value.Span))),
            allowNonPowerOfTwoResize);

    private byte[] RepackImages(
        IReadOnlyDictionary<int, Func<Image<Rgba32>>> replacements,
        bool allowNonPowerOfTwoResize)
    {
        foreach (int index in replacements.Keys)
            if (index < 1 || index > Textures.Count)
                throw new ArgumentOutOfRangeException(nameof(replacements),
                    $"Texture index {index} does not exist.");
        byte[] result = _data;
        CatalogDocument current = _catalog;
        foreach (var pair in replacements.OrderBy(pair => pair.Key))
        {
            TextureInfo texture = Textures[pair.Key - 1];
            using Image<Rgba32> image = pair.Value();
            ValidateReplacement(texture, image, allowNonPowerOfTwoResize);
            result = SmoTextureDataWriter.ReplaceBgra(current, texture.ObjectIndex,
                image.Width, image.Height, EncodePixels(image, texture.Layout));
            current = CatalogDocument.ParseOwned(result);
        }
        return ReferenceEquals(result, _data) ? _data.ToArray() : result;
    }

    private static List<TextureInfo> FindTextures(
        CatalogDocument catalog, out IReadOnlyList<string> unsupported)
    {
        var result = new List<TextureInfo>();
        var issues = new List<string>();
        foreach (SmoObjectEntry entry in catalog.Objects
                     .Where(entry => entry.TypeHash == SmoClassIds.TextureData)
                     .OrderBy(entry => entry.PhysicalOffset))
        {
            if (!SmoTextureDataDecoder.TryDecode(catalog, entry, out var decoded, out string error))
            {
                issues.Add($"[{entry.Index}] {entry.Name}: {error}");
                continue;
            }
            var representation = decoded.SelectedRepresentation;
            if (representation is null || representation.Kind is not
                (SmoTextureRepresentationKind.CrossPlatformBgra32 or SmoTextureRepresentationKind.CrossPlatformBgrx32 or SmoTextureRepresentationKind.Direct3DBgra32))
            {
                issues.Add($"[{entry.Index}] {entry.Name}: native palette/swizzle preview is unsupported.");
                continue;
            }
            var mip = representation.MipLevels[0];
            if (!MemoryMarshal.TryGetArray(mip.PixelData, out ArraySegment<byte> segment) ||
                !MemoryMarshal.TryGetArray(catalog.Data, out ArraySegment<byte> fileSegment) ||
                !ReferenceEquals(segment.Array, fileSegment.Array))
                throw new InvalidDataException("Texture pixels are not a bounded slice of the SMO.");
            int block = checked((int)entry.PhysicalOffset);
            ushort headerBytes = BinaryPrimitives.ReadUInt16LittleEndian(catalog.Data.Span[(block + 8)..]);
            bool writable = SmoTextureDataWriter.CanReplace(decoded, out string reason);
            var material = FindMaterialReference(catalog, entry, out string? materialIssue);
            var texture = new TextureInfo(result.Count + 1, block,
                segment.Offset - fileSegment.Offset, mip.Width, mip.Height, headerBytes,
                TextureLayout.Bgra, AnalyzeChannels(representation.BgraPreview.IsEmpty ? mip.PixelData.Span : representation.BgraPreview.Span, TextureLayout.Bgra))
            {
                ObjectIndex = entry.Index,
                BgraPreview = representation.BgraPreview,
                RepresentationName = SmoTextureDataDecoder.GetRepresentationName(representation.Kind),
                MipLevelCount = representation.MipLevels.Count,
                ReplacementIssue = writable ? null : reason,
                Material = material,
                MaterialIssue = materialIssue
            };
            result.Add(texture);
        }
        unsupported = issues.AsReadOnly();
        return result;
    }

    private static MaterialReferenceInfo? FindMaterialReference(
        CatalogDocument catalog, SmoObjectEntry texture, out string? issue)
    {
        issue = null;
        var owners = catalog.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.MaterialData &&
                entry.PhysicalOffset < texture.PhysicalOffset &&
                entry.PhysicalEnd >= texture.PhysicalEnd)
            .OrderBy(entry => entry.SerializedSize).ToArray();
        if (owners.Length == 0)
        {
            issue = "MATERIAL_BINDING_UNLOCATED: This texture has no containing catalogued MaterialData.";
            return null;
        }
        if (owners.Length > 1 && owners[0].SerializedSize == owners[1].SerializedSize)
        {
            issue = "MATERIAL_BINDING_AMBIGUOUS: Multiple MaterialData entries share the nearest containing extent.";
            return null;
        }
        var material = owners[0];
        if (!SmoObjectFieldReader.TryRead(catalog, material, out var fields, out string fieldError))
        {
            issue = "MATERIAL_FIELDS: " + fieldError;
            return null;
        }
        // Keep authored byte ranges separate from the final state produced by
        // the actual material reader. A saved inline field may be overwritten.
        var containers = fields.Where(field =>
            field.FieldType == 10 && field.AbsolutePayloadOffset <= texture.PhysicalOffset &&
            field.AbsoluteEnd >= texture.PhysicalEnd).ToArray();
        if (containers.Length != 1)
        {
            issue = containers.Length == 0
                ? "MATERIAL_BINDING_UNLOCATED: No direct field10 contains this saved texture."
                : "MATERIAL_BINDING_AMBIGUOUS: Multiple direct field10 ranges contain this saved texture.";
            return null;
        }
        var container = containers[0];
        if (!SmoMaterialInspection.TryInspect(catalog, material, out var state, out string stateError))
        {
            issue = "MATERIAL_INSPECTION: " + stateError;
            return null;
        }
        var matches = state!.Passes
            .SelectMany(pass => pass.Layers.Select(layer => (Pass: pass, Layer: layer)))
            .Where(item => item.Layer.TextureReference is { } reference &&
                reference.AbsolutePayloadOffset == container.AbsolutePayloadOffset &&
                reference.Size == container.PayloadSize)
            .ToArray();
        if (matches.Length != 1)
        {
            issue = matches.Length == 0
                ? "MATERIAL_BINDING_NOT_CURRENT: This saved field10 is not a layer's final observed Texture reference."
                : "MATERIAL_BINDING_AMBIGUOUS: Multiple current layers match this saved Texture reference.";
            return null;
        }
        var match = matches[0];
        int materialIndex = catalog.Objects.Count(entry =>
            entry.TypeHash == SmoClassIds.MaterialData && entry.PhysicalOffset <= material.PhysicalOffset);
        return new MaterialReferenceInfo(materialIndex, checked((int)material.PhysicalOffset),
            container.AbsoluteHeaderOffset, container.PayloadSize, match.Pass.Index + 1, match.Layer.Index + 1,
            match.Layer.ClassId, match.Layer.ClassId == 0x234C576B ? "spStdLayer" : $"class 0x{match.Layer.ClassId:X8}",
            match.Pass.FinalBlendOperation, state.RenderStates, match.Layer.TextureStates);
    }

    private static bool TryReadDataBlockHeader(
        ReadOnlySpan<byte> data,
        int offset,
        out int fieldType,
        out int headerSize,
        out uint payloadSize)
        => SmoDataBlockReader.TryReadHeader(data, offset, out fieldType, out headerSize, out payloadSize);

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

        if (!texture.CanReplace)
            throw new NotSupportedException(texture.ReplacementIssue ?? "Unsupported texture slot.");
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
