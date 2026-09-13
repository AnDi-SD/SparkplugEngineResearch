using System.Diagnostics;
using System.Buffers.Binary;
using System.Text;
using System.Security.Cryptography;
using System.Text.Json;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;
using SmoViewer.Core;
using TextureDocument = SMOTextureTool.Core.SmoDocument;

internal static class FocusedTextureRegression
{
    private static int checks;

    public static void Run(string[] args)
    {
        bool previewOnly = args.FirstOrDefault() == "--preview-only";
        if (previewOnly) args = args.Skip(1).ToArray();
        if (args.Length < 2 || args.Length > 16)
            throw new ArgumentException("--focused OUTPUT_DIRECTORY SMO [SMO ...], at most 15 specimens.");
        string outputDirectory = Path.GetFullPath(args[0]);
        Directory.CreateDirectory(outputDirectory);
        var watch = Stopwatch.StartNew();
        VerifyCompactContainers();
        var files = new List<object>();
        foreach (string path in args.Skip(1))
        {
            byte[] original = File.ReadAllBytes(path);
            SmoDocument before = SmoDocument.ParseOwned(original);
            TextureDocument tool = TextureDocument.Parse(original);
            Check(!before.HasErrors && tool.Textures.Count > 0, "source catalog and embedded textures");
            string prefix = Path.GetFileNameWithoutExtension(path) + "-" + Hash(original)[..8];
            string exportDirectory = Path.Combine(outputDirectory, prefix + "-png");
            tool.ExportAll(exportDirectory);
            foreach (var texture in tool.Textures)
            {
                Check(SmoTextureDataDecoder.TryDecode(before, before.Objects[texture.ObjectIndex],
                    out var decoded, out _), "shared structural texture decode");
                var representation = decoded!.SelectedRepresentation!;
                var expected = representation.MipLevels[0];
                using Image<Rgba32> png = Image.Load<Rgba32>(Path.Combine(exportDirectory, texture.FileName));
                Check(png.Width == expected.Width && png.Height == expected.Height, "exported PNG dimensions");
                byte[] actualPixels = ToBgra(png);
                if (representation.Kind == SmoTextureRepresentationKind.CrossPlatformBgrx32)
                {
                    bool matches = actualPixels.Length == expected.PixelData.Length;
                    for (int pixel = 0; matches && pixel < actualPixels.Length; pixel += 4)
                        matches = actualPixels.AsSpan(pixel, 3).SequenceEqual(expected.PixelData.Span.Slice(pixel, 3)) && actualPixels[pixel + 3] == 255;
                    Check(matches, "exported XRGB preserves BGR and uses opaque alpha");
                    Check(texture.Channels.AlphaMin == 255 && texture.Channels.AlphaMax == 255, "XRGB channel analysis uses displayed alpha");
                }
                else Check(actualPixels.AsSpan().SequenceEqual(expected.PixelData.Span), "exported PNG exact BGRA/alpha");

                var textureEntry = before.Objects[texture.ObjectIndex];
                bool hasInlineMaterial = before.Objects.Any(item => item.TypeHash == SmoClassIds.MaterialData &&
                    item.PhysicalOffset < textureEntry.PhysicalOffset && item.PhysicalEnd >= textureEntry.PhysicalEnd);
                Check(texture.Material is not null || !hasInlineMaterial, "catalogued inline texture retains its material metadata");
                if (texture.Material is { } material)
                {
                    Check(material.PassIndex > 0 && material.LayerIndex > 0 &&
                        material.MaterialRenderStates.Count is 0 or 11 && material.LayerTextureStates.Count is 0 or 9,
                        $"material metadata retains its pass/layer and bounded optional state observations: file={path}, " +
                        $"textureObject={texture.ObjectIndex}, pass={material.PassIndex}, layer={material.LayerIndex}, " +
                        $"renderStates={material.MaterialRenderStates.Count}, textureStates={material.LayerTextureStates.Count}, " +
                        $"materialOffset=0x{material.BlockOffset:X}, field10Offset=0x{material.TextureContainerOffset:X}");
                    Check(before.Objects.Any(item => item.TypeHash == SmoClassIds.MaterialData &&
                        item.PhysicalOffset == material.BlockOffset && item.PhysicalOffset < textureEntry.PhysicalOffset &&
                        item.PhysicalEnd >= textureEntry.PhysicalEnd), "material metadata binds to the actual containing catalog extent");
                    Check(SmoDataBlockReader.TryReadHeader(before.Data.Span, material.TextureContainerOffset, out var container) &&
                        container.FieldType == 10 && container.PayloadSize == material.TextureContainerSize &&
                        container.PayloadOffset <= textureEntry.PhysicalOffset && container.PayloadEnd >= textureEntry.PhysicalEnd,
                        "material field10 metadata selects the actual field containing this texture");
                }
            }
            // Keep raw observations in the report. The shared material reader
            // handles field8/17 and defaults; metadata diagnostics may still
            // leave an unbound texture without a material DTO.
            var materialSnapshots = tool.Textures.Select(texture => new
            {
                objectIndex = texture.ObjectIndex,
                material = texture.Material is { } value ? new
                {
                    value.Index, value.BlockOffset, value.TextureContainerOffset, value.TextureContainerSize,
                    value.PassIndex, value.LayerIndex, value.LayerClassId, value.LayerClassName,
                    value.FinalBlendOperation, value.MaterialRenderStates, value.LayerTextureStates
                } : null
            }).ToArray();
            var materialMetadata = new
            {
                scope = "Current-run invariants and DTO snapshot; no baseline DTO equality is asserted",
                sha256 = Hash(JsonSerializer.SerializeToUtf8Bytes(materialSnapshots)),
                textures = materialSnapshots
            };
            if (previewOnly)
            {
                Check(File.ReadAllBytes(path).AsSpan().SequenceEqual(original), "preview preserved source bytes");
                files.Add(new { path = Path.GetFullPath(path), sha256 = Hash(original),
                    objects = before.Objects.Count, textures = tool.Textures.Count,
                    exported = tool.Textures.Count, unsupported = tool.UnsupportedTextures, materialMetadata,
                    cases = Array.Empty<object>() });
                Console.WriteLine($"PASS {prefix}: {tool.Textures.Count} PNG exports, preview only");
                continue;
            }
            var writable = tool.Textures.Where(texture => texture.CanReplace).ToArray();
            Check(writable.Length > 0, "writable source texture");
            var first = writable[0];
            byte[] noOp = tool.Repack(new Dictionary<int, string>
                { [first.Index] = Path.Combine(exportDirectory, first.FileName) });
            Check(noOp.AsSpan().SequenceEqual(original), "PNG roundtrip preserves the entire original SMO byte for byte");
            Check(tool.Repack(new Dictionary<int, string>()).AsSpan().SequenceEqual(original), "empty rewrite is byte exact");
            Throws<ArgumentOutOfRangeException>(() => tool.RepackEncodedImages(
                new Dictionary<int, ReadOnlyMemory<byte>> { [tool.Textures.Count + 1] = new byte[] { 1 } }),
                "unknown replacement index is rejected before image decoding");

            var cases = new List<object>();
            foreach (var size in new[] { (first.Width, first.Height), (13, 7), (128, 64), (1, 1), (64, 128) }.Distinct())
            {
                using var replacement = Pattern(size.Item1, size.Item2);
                byte[] expected = ToBgra(replacement);
                byte[] output = tool.RepackEncodedImages(new Dictionary<int, ReadOnlyMemory<byte>>
                    { [first.Index] = Encode(replacement) });
                VerifyUnchanged(before, output, new HashSet<int> { first.ObjectIndex });
                TextureDocument reparsed = TextureDocument.Parse(output);
                Check(reparsed.Textures.Count == tool.Textures.Count, "replacement preserves visible slot count");
                var actual = reparsed.Textures.Single(texture => texture.ObjectIndex == first.ObjectIndex);
                using Image<Rgba32> image = reparsed.Decode(actual);
                Check(image.Width == replacement.Width && image.Height == replacement.Height &&
                    ToBgra(image).AsSpan().SequenceEqual(expected), "replacement keeps exact supplied dimensions and RGBA");
                string outputPath = Path.Combine(outputDirectory, $"{prefix}-{size.Item1}x{size.Item2}.smo");
                File.WriteAllBytes(outputPath, output);
                cases.Add(new { width = size.Item1, height = size.Item2,
                    objectIndex = first.ObjectIndex, bytes = output.Length,
                    sha256 = Hash(output), path = outputPath, pixelSha256 = Hash(expected) });
            }
            if (writable.Length >= 2)
            {
                using var imageA = Pattern(23, 11);
                using var imageB = Pattern(17, 29);
                byte[] output = tool.RepackEncodedImages(new Dictionary<int, ReadOnlyMemory<byte>>
                {
                    [writable[1].Index] = Encode(imageB),
                    [writable[0].Index] = Encode(imageA)
                });
                VerifyUnchanged(before, output, writable.Take(2).Select(texture => texture.ObjectIndex).ToHashSet());
                TextureDocument after = TextureDocument.Parse(output);
                foreach (var item in new[] { (writable[0], imageA), (writable[1], imageB) })
                {
                    var texture = after.Textures.Single(texture => texture.ObjectIndex == item.Item1.ObjectIndex);
                    using Image<Rgba32> actual = after.Decode(texture);
                    Check(actual.Width == item.Item2.Width && actual.Height == item.Item2.Height &&
                        ToBgra(actual).AsSpan().SequenceEqual(ToBgra(item.Item2)), "multiple resized textures retain object identity");
                }
                string outputPath = Path.Combine(outputDirectory, prefix + "-multi.smo");
                File.WriteAllBytes(outputPath, output);
                cases.Add(new { multiple = true, sha256 = Hash(output), path = outputPath });
            }
            Check(File.ReadAllBytes(path).AsSpan().SequenceEqual(original), "original source was never written");
            files.Add(new { path = Path.GetFullPath(path), sha256 = Hash(original),
                objects = before.Objects.Count, textures = tool.Textures.Count,
                exported = tool.Textures.Count, unsupported = tool.UnsupportedTextures, materialMetadata, cases });
            Console.WriteLine($"PASS {prefix}: {tool.Textures.Count} PNG exports, {cases.Count} replacements");
        }
        var report = new { kind = "texture-tool-focused-regression", schemaVersion = 1,
            timestampUtc = DateTimeOffset.UtcNow, checks, elapsedSeconds = watch.Elapsed.TotalSeconds,
            peakWorkingSetBytes = Process.GetCurrentProcess().PeakWorkingSet64, files };
        File.WriteAllText(Path.Combine(outputDirectory, "report.json"),
            JsonSerializer.Serialize(report, new JsonSerializerOptions { WriteIndented = true }));
        Console.WriteLine($"PASS {checks} assertions in {watch.Elapsed.TotalSeconds:F3}s; {files.Count} specimens");
    }

    private static void VerifyUnchanged(SmoDocument before, byte[] output, HashSet<int> edited)
    {
        SmoDocument after = SmoDocument.ParseOwned(output);
        Check(!after.HasErrors && before.Objects.Count == after.Objects.Count, "strict catalog after rewrite");
        foreach (SmoObjectEntry old in before.Objects)
        {
            SmoObjectEntry current = after.Objects[old.Index];
            Check(old.Id == current.Id && old.TypeHash == current.TypeHash &&
                old.RawName.Span.SequenceEqual(current.RawName.Span) && old.ParentIndex == current.ParentIndex,
                $"catalog identity and ownership [{old.Index}]");
            if (edited.Contains(old.Index))
                continue;
            bool containsEdit = edited.Any(index => before.Objects[index].PhysicalOffset >= old.PhysicalOffset &&
                before.Objects[index].PhysicalEnd <= old.PhysicalEnd);
            if (!containsEdit)
            {
                Check(old.SerializedSize == current.SerializedSize &&
                    before.Data.Span.Slice(checked((int)old.PhysicalOffset), checked((int)old.SerializedSize))
                        .SequenceEqual(after.Data.Span.Slice(checked((int)current.PhysicalOffset), checked((int)current.SerializedSize))),
                    $"untouched object bytes [{old.Index}]");
                continue;
            }
            var oldFields = SmoObjectFieldReader.Read(before, old);
            var newFields = SmoObjectFieldReader.Read(after, current);
            Check(oldFields.Count == newFields.Count, $"enclosing field count [{old.Index}]");
            for (int i = 0; i < oldFields.Count; i++)
            {
                var field = oldFields[i];
                Check(field.FieldType == newFields[i].FieldType, "enclosing field order");
                if (edited.Any(index => before.Objects[index].PhysicalOffset >= field.AbsolutePayloadOffset &&
                    before.Objects[index].PhysicalEnd <= field.AbsoluteEnd))
                    continue;
                Check(before.Data.Span.Slice(field.AbsoluteHeaderOffset, field.EncodedSize)
                    .SequenceEqual(after.Data.Span.Slice(newFields[i].AbsoluteHeaderOffset, newFields[i].EncodedSize)),
                    $"untouched enclosing field [{old.Index}]/{i}");
            }
            if (old.TypeHash == SmoClassIds.Skin && SmoSkinDecoder.TryDecode(before, old, out var oldSkin, out _))
            {
                Check(SmoSkinDecoder.TryDecode(after, current, out var skin, out _) &&
                    oldSkin.Bones.Select(bone => bone.NodeObjectIndex).SequenceEqual(skin!.Bones.Select(bone => bone.NodeObjectIndex)),
                    "skin node-reference palette after resizing inline materials");
            }
        }
    }

    private static void VerifyCompactContainers()
    {
        // Synthetic structural fixture: a catalogued TextureData leaf, compact
        // enclosing headers, an extended unknown field and an inline size prefix.
        // It exercises relocation/header growth; it is not original-game evidence.
        static byte[] Join(params byte[][] parts) => parts.SelectMany(part => part).ToArray();
        static byte[] UInt(uint value) { byte[] bytes = new byte[4]; BinaryPrimitives.WriteUInt32LittleEndian(bytes, value); return bytes; }
        static byte[] Field(int type, byte[] payload) => SmoDataBlockWriter.BuildField(type, payload);
        byte[] raw = Join(new byte[] { 1 }, UInt(1), UInt(1), UInt(0), new byte[] { 1 },
            UInt(1), UInt(4), UInt(1), new byte[] { 3, 5, 7, 11 });
        byte[] local = Join(Field(2, new byte[] { 0 }), new byte[] { 0 }, Field(6, UInt(6)),
            Field(1, Join(Field(0, raw), new byte[] { 0 })), new byte[] { 0 });
        byte[] leaf = Join(UInt(SmoClassIds.TextureData), "SBOO"u8.ToArray(), Field(3, local), new byte[] { 0 });
        byte[] inline = Join(UInt(22), UInt((uint)leaf.Length), leaf);
        byte[] container = Field(0, inline);
        int childOffset = 8 + container.Length - inline.Length + 8;
        byte[] root = Join(UInt(SmoClassIds.Model), "SBOO"u8.ToArray(), container,
            Field(200, "opaque bytes survive exactly"u8.ToArray()), new byte[] { 0 });
        byte[] Entry(uint id, string name, uint type, int offset, int size)
        {
            byte[] nameBytes = Encoding.ASCII.GetBytes(name + "\0");
            byte[] length = new byte[2]; BinaryPrimitives.WriteUInt16LittleEndian(length, (ushort)nameBytes.Length);
            return Join(UInt(id), length, nameBytes, UInt(type), UInt((uint)offset), UInt((uint)size));
        }
        byte[] table = Join(Entry(11, "root", SmoClassIds.Model, 0, root.Length),
            Entry(22, "texture", SmoClassIds.TextureData, childOffset, leaf.Length), new byte[4]);
        int dataStart = 32 + table.Length;
        byte[] original = Join("FFPS"u8.ToArray(), UInt(1), UInt(0), UInt((uint)(dataStart + root.Length)),
            UInt(6), UInt((uint)dataStart), UInt((uint)root.Length), UInt(2), table, root);
        var before = SmoDocument.ParseOwned(original);
        foreach (var size in new[] { (13, 7), (257, 65) })
        {
            using var image = Pattern(size.Item1, size.Item2);
            byte[] output = SmoTextureDataWriter.ReplaceBgra(before, 1, image.Width, image.Height, ToBgra(image));
            VerifyUnchanged(before, output, new HashSet<int> { 1 });
            var after = SmoDocument.ParseOwned(output);
            var field = SmoObjectFieldReader.Read(after, after.Objects[0])[0];
            Check(field.SizeKind == (size.Item1 == 13 ? SmoDataBlockSizeCode.UInt16 : SmoDataBlockSizeCode.UInt32),
                "compact enclosing header expands across UInt8/16 limits");
            int prefix = checked((int)after.Objects[1].PhysicalOffset - 8);
            Check(BinaryPrimitives.ReadUInt32LittleEndian(output.AsSpan(prefix)) == 22 &&
                BinaryPrimitives.ReadUInt32LittleEndian(output.AsSpan(prefix + 4)) == after.Objects[1].SerializedSize,
                "inline ID/size prefix relocates with its resized field header");
            byte[] created = SmoTextureDataWriter.CreateBgraObject(image.Width, image.Height, ToBgra(image));
            byte[] inserted = SmoLeafObjectReplacer.Replace(before, 1, created);
            VerifyUnchanged(before, inserted, new HashSet<int> { 1 });
            var createdDocument = SmoDocument.ParseOwned(inserted);
            Check(SmoTextureDataDecoder.TryDecode(createdDocument, createdDocument.Objects[1], out var createdTexture, out _)
                && createdTexture.PlatformSpecific!.NativeField1C == 1
                && createdTexture.PlatformSpecific.MipLevels[0].PixelData.Span.SequenceEqual(ToBgra(image)),
                "native whole-object writer creates exact embedded BGRA bytes");
        }
        foreach (byte flag in new byte[] { 0, 2 })
        {
            byte[] changed = original.ToArray();int rawOffset = changed.AsSpan().IndexOf(raw);
            Check(rawOffset >= 0, "unique synthetic prefix exists");changed[rawOffset + 13] = flag;
            var flagged = SmoDocument.ParseOwned(changed);
            byte[] replaced = SmoTextureDataWriter.ReplaceBgra(flagged, 1, 1, 1, new byte[] { 7, 9, 11, 13 });
            var after = SmoDocument.ParseOwned(replaced);
            Check(SmoTextureDataDecoder.TryDecode(after, after.Objects[1], out var decoded, out _)
                && decoded.PlatformSpecific!.NativeField1C == flag
                && decoded.PlatformSpecific.PixelDataPresent
                && decoded.PlatformSpecific.MipLevels[0].PixelData.Span.SequenceEqual(new byte[] { 7, 9, 11, 13 }),
                "editor preserves raw native field1C without treating zero as missing stored pixels");
        }
    }

    private static Image<Rgba32> Pattern(int width, int height)
    {
        var image = new Image<Rgba32>(width, height);
        image.ProcessPixelRows(accessor =>
        {
            for (int y = 0; y < height; y++)
                for (int x = 0; x < width; x++)
                    accessor.GetRowSpan(y)[x] = new Rgba32((byte)(x * 13 + y * 7),
                        (byte)(x * 31 + y * 11), (byte)(x * 3 + y * 23), (byte)(x * 29 + y * 19));
        });
        return image;
    }

    private static byte[] Encode(Image<Rgba32> image)
    {
        using var stream = new MemoryStream();
        image.SaveAsPng(stream);
        return stream.ToArray();
    }

    private static byte[] ToBgra(Image<Rgba32> image)
    {
        byte[] data = new byte[checked(image.Width * image.Height * 4)];
        image.ProcessPixelRows(accessor =>
        {
            int offset = 0;
            for (int y = 0; y < image.Height; y++)
                foreach (Rgba32 pixel in accessor.GetRowSpan(y))
                {
                    data[offset++] = pixel.B; data[offset++] = pixel.G;
                    data[offset++] = pixel.R; data[offset++] = pixel.A;
                }
        });
        return data;
    }

    private static string Hash(byte[] data) => Convert.ToHexString(SHA256.HashData(data)).ToLowerInvariant();

    private static void Check(bool condition, string message)
    {
        checks++;
        if (!condition) throw new InvalidDataException(message);
    }

    private static void Throws<T>(Action action, string message) where T : Exception
    {
        try { action(); }
        catch (T) { checks++; return; }
        throw new InvalidDataException(message);
    }
}
