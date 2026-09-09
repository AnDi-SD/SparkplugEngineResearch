using System.Buffers.Binary;
using System.Diagnostics;
using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using SmoViewer.Core;

internal static class SkinPaletteWriterRegression
{
    public static int Run(string sourcePath, string outputFolder)
    {
        if (Directory.Exists(outputFolder))
            throw new InvalidOperationException("Use a new output directory to preserve prior evidence.");
        Directory.CreateDirectory(outputFolder);
        var watch = Stopwatch.StartNew();
        var checks = new List<string>();
        void Check(bool condition, string message)
        {
            if (!condition) throw new InvalidDataException(message);
            checks.Add(message);
        }
        void Reject<T>(Action action, string message, string? requiredMessage = null) where T : Exception
        {
            bool rejected = false;
            try { action(); }
            catch (T error)
            {
                rejected = requiredMessage is null || error.Message.Contains(requiredMessage, StringComparison.Ordinal);
            }
            Check(rejected, message);
        }

        string fullSourcePath = Path.GetFullPath(sourcePath);
        var document = SmoDocument.Load(fullSourcePath);
        string sourceHash = Hash(document.Data.Span);
        Check(!document.HasErrors, "The real source has a complete catalog.");
        var entry = document.Objects.First(item => item.TypeHash == SmoClassIds.Skin);
        Check(SmoSkinDecoder.TryDecode(document, entry, out var sourceSkin, out string decodeError),
            "The actual Skin metadata reader accepts the source: " + decodeError);
        uint[] nodeIds = sourceSkin!.Bones.Select(bone => bone.NodeObjectId).Distinct()
            .Where(id => document.Objects.Any(item => item.Id == id && item.TypeHash == SmoClassIds.Node))
            .Take(2).ToArray();
        Check(nodeIds.Length == 2 && nodeIds[0] != nodeIds[1],
            "Two distinct palette IDs bind to exact catalogued Node classes.");

        uint[] identity = [0x3f800000, 0, 0, 0, 0, 0x3f800000, 0, 0,
            0, 0, 0x3f800000, 0, 0, 0, 0, 0x3f800000];
        uint[] translated = [0x3f800000, 0, 0, 0, 0, 0x3f800000, 0, 0,
            0, 0, 0x3f800000, 0, 0x3f000000, 0xbf800000, 0x40000000, 0x3f800000];
        uint[] rawFirst = [0x80000000, 0x7fc12345, 0xffc54321, 0x7f800000,
            0xff800000, 1, 0x80000001, 0x00800000, 0x80800000, 0x3f800000,
            0xbf800000, 0x7f7fffff, 0xff7fffff, 0, 0x3eaaaaab, 0x3f800000];
        uint[] rawSecond = [0x7fcabcde, 0x80000000, 0, 0xffc13579,
            0x3f800000, 0xbf000000, 0x40000000, 0xc0000000, 0, 0, 0x3f800000,
            0, 0x80000000, 0x00000002, 0x80000002, 0x3f800000];
        var cases = new[]
        {
            new PaletteCase("two-real-nodes-zero-weights", 0,
                [new(nodeIds[0], identity), new(nodeIds[1], translated)]),
            new PaletteCase("repeated-node-raw-matrices-max-weights", uint.MaxValue,
                [new(nodeIds[0], rawFirst), new(nodeIds[0], rawSecond)]),
            new PaletteCase("empty-zero-weights", 0, []),
            new PaletteCase("empty-max-weights", uint.MaxValue, [])
        };
        var observations = new List<object>();
        byte[]? retainedOutput = null;
        byte[]? retainedExpected = null;
        using var writer = new SmoSkinPaletteWriter(document); // The only whole ResourceGraph load in this test.
        foreach (var item in cases)
        {
            var bindings = item.Bones.Select(bone => new SmoSkinPaletteBinding(bone.Id, Matrix(bone.Bits))).ToArray();
            byte[] expected = ExpectedField(item.Weights, item.Bones);
            byte[] actual = writer.WriteField(item.Weights, bindings);
            Check(actual.AsSpan().SequenceEqual(expected),
                item.Name + ": shared writer bytes equal an independent fixed wire-format expectation.");
            Check(SmoDataBlockReader.TryReadHeader(actual, out var header) && header.FieldType == 0 &&
                header.SizeKind == SmoDataBlockSizeCode.UInt32 && !header.HasExtendedFieldType &&
                header.PayloadSize == 8u + 72u * (uint)item.Bones.Length && header.PayloadEnd == actual.Length,
                item.Name + ": one UInt32-reserved palette field has no section terminator or inline Node payload.");
            Check(writer.WriteField(item.Weights, bindings).AsSpan().SequenceEqual(expected),
                item.Name + ": another call retains the original IDs and has no prior-call FAT state.");
            File.WriteAllBytes(Path.Combine(outputFolder, item.Name + ".actual.bin"), actual);
            File.WriteAllBytes(Path.Combine(outputFolder, item.Name + ".expected.bin"), expected);
            observations.Add(new { name = item.Name, weights = item.Weights, boneCount = item.Bones.Length,
                bones = item.Bones.Select(bone => new { id = bone.Id, matrixBits = bone.Bits }),
                length = actual.Length, actualSha256 = Hash(actual), expectedSha256 = Hash(expected) });
            retainedOutput = actual;
            retainedExpected = expected;
        }

        var catalogIds = document.Objects.Select(item => item.Id).ToHashSet();
        uint unknownId = uint.MaxValue - 1;
        while (catalogIds.Contains(unknownId) && unknownId > 0) --unknownId;
        Reject<InvalidDataException>(() => writer.WriteField(0, [new(unknownId, Matrix4x4.Identity)]),
            "A missing catalog ID cannot create a replacement Node.");
        Reject<InvalidDataException>(() => writer.WriteField(0, [new(entry.Id, Matrix4x4.Identity)]),
            "A real loaded Skin ID cannot substitute for a Node.");
        Reject<InvalidDataException>(() => writer.WriteField(0, [new(0, Matrix4x4.Identity)]),
            "A null Node ID is rejected before authoring a palette.");
        Reject<InvalidDataException>(() => writer.WriteField(0, [new(uint.MaxValue, Matrix4x4.Identity)]),
            "The reserved all-ones Node ID is rejected.");
        Reject<ArgumentNullException>(() => writer.WriteField(0, null!),
            "A null binding list is rejected.");
        Reject<ArgumentOutOfRangeException>(() => writer.WriteField(0,
                Enumerable.Repeat(new SmoSkinPaletteBinding(nodeIds[0], Matrix4x4.Identity), 1025).ToArray()),
            "The bounded managed input rejects 1025 bindings.");

        var sourceHeader = SmoSkinPaletteWriter.GetSinglePaletteField(document, entry);
        var observed = sourceSkin.PaletteFields.Single();
        Check(sourceHeader.Offset == 8L + observed.HeaderOffset &&
            sourceHeader.PayloadOffset == 8L + observed.PayloadOffset &&
            sourceHeader.PayloadSize == observed.PayloadSize,
            "GetSinglePaletteField selects the actual reader's exact Icy field offsets and size.");
        byte[] patchedBytes = document.Data.ToArray();
        int weightOffset = checked((int)entry.PhysicalOffset + sourceHeader.PayloadOffset);
        BinaryPrimitives.WriteUInt32LittleEndian(patchedBytes.AsSpan(weightOffset), uint.MaxValue);
        var patched = SmoDocument.ParseOwned(patchedBytes, "skin-palette-max-weight-metadata-only");
        var patchedEntry = patched.Objects.Single(item => item.Id == entry.Id);
        Check(SmoSkinDecoder.TryDecode(patched, patchedEntry, out var patchedSkin, out string patchedError),
            "The metadata reader accepts the same real Skin with an all-ones weight word: " + patchedError);
        Check(patchedSkin!.BlendInfluenceCountHint == uint.MaxValue,
            "The raw all-ones weight word remains UInt32 metadata without narrowing.");
        Check(SmoSkinPaletteWriter.GetSinglePaletteField(patched, patchedEntry) == sourceHeader &&
            patchedSkin.PaletteFields.SequenceEqual(sourceSkin.PaletteFields),
            "Changing only the weight word preserves all observed palette field locations.");

        var shapes = new List<object>();
        foreach (string mode in new[] { "repeated", "uint16", "extended" })
        {
            // A catalog-only fixture exercises inherited metadata readers. Its Mesh
            // entry is a reference target, never passed to ResourceGraph or a writer.
            byte[] fixture = MetadataContainer(mode);
            var metadata = SmoDocument.ParseOwned(fixture, "skin-palette-header-" + mode);
            var skinEntry = metadata.Objects.Single(item => item.TypeHash == SmoClassIds.Skin);
            Check(!metadata.HasErrors && SmoSkinDecoder.TryDecode(metadata, skinEntry, out var shape, out _) &&
                shape!.PaletteFields.Count == (mode == "repeated" ? 2 : 1),
                mode + ": the actual metadata reader observes the deliberately unsupported header shape.");
            Reject<NotSupportedException>(() => SmoSkinPaletteWriter.GetSinglePaletteField(metadata, skinEntry),
                mode + ": the editing API refuses to choose or normalize the unsupported shape.", "SKIN_PALETTE_SHAPE:");
            File.WriteAllBytes(Path.Combine(outputFolder, "metadata-only-" + mode + ".smo"), fixture);
            shapes.Add(new { mode, sha256 = Hash(fixture), wholeGraphLoaded = false });
        }

        writer.Dispose();
        writer.Dispose();
        Reject<ObjectDisposedException>(() => writer.WriteField(0, []),
            "Disposed native graph ownership cannot be used for another field write.");
        Check(retainedOutput!.AsSpan().SequenceEqual(retainedExpected),
            "The copied managed field remains valid after native graph disposal.");
        Check(Hash(document.Data.Span) == sourceHash,
            "Successful writes, rejected writes, metadata inspection and disposal leave the source document unchanged.");
        Check(HashFile(fullSourcePath) == sourceHash,
            "The pristine source file is unchanged on disk.");

        string nativePath = Path.Combine(AppContext.BaseDirectory, "SparkplugViewerNative.dll");
        using var process = Process.GetCurrentProcess();
        process.Refresh();
        File.WriteAllText(Path.Combine(outputFolder, "report.json"), JsonSerializer.Serialize(new
        {
            status = "passed", checks = checks.Count, checkDescriptions = checks,
            scope = "Managed actual-Node Skin palette field authoring and metadata field selection; no whole-container rewrite or new original-game execution",
            source = new { path = fullSourcePath, bytes = document.Data.Length, sha256 = sourceHash,
                skinId = entry.Id, skinObjectOffset = entry.PhysicalOffset, nodeIds,
                paletteHeaderOffsetInObject = sourceHeader.Offset, palettePayloadOffsetInObject = sourceHeader.PayloadOffset,
                palettePayloadSize = sourceHeader.PayloadSize, patchedWeightAbsoluteOffset = weightOffset },
            nativeDll = new { path = nativePath, sha256 = HashFile(nativePath) },
            coreAssemblySha256 = HashFile(typeof(SmoDocument).Assembly.Location),
            editingAssemblySha256 = HashFile(typeof(SmoSkinPaletteWriter).Assembly.Location),
            testAssemblySha256 = HashFile(typeof(SkinPaletteWriterRegression).Assembly.Location),
            actualGraphLoads = 1, elapsedSeconds = watch.Elapsed.TotalSeconds,
            processPeakWorkingSetBytes = process.PeakWorkingSet64,
            processPeakPagedMemoryBytes = process.PeakPagedMemorySize64,
            cases = observations, metadataOnlyShapes = shapes
        }, new JsonSerializerOptions { WriteIndented = true }) + "\n");
        Console.WriteLine($"PASS shared Skin palette authoring: {checks.Count} checks");
        return 0;
    }

    private sealed record Bone(uint Id, uint[] Bits);
    private sealed record PaletteCase(string Name, uint Weights, Bone[] Bones);

    // Independent test-only wire expectation. No production serializer calls.
    private static byte[] ExpectedField(uint weights, IReadOnlyList<Bone> bones)
    {
        byte[] bytes = new byte[13 + 72 * bones.Count];
        bytes[0] = 0xe0;
        WriteWord(bytes, 1, checked((uint)bytes.Length - 5));
        WriteWord(bytes, 5, weights);
        WriteWord(bytes, 9, checked((uint)bones.Count));
        for (int i = 0; i < bones.Count; ++i)
        {
            int offset = 13 + i * 72;
            WriteWord(bytes, offset, bones[i].Id);
            WriteWord(bytes, offset + 4, 0);
            for (int component = 0; component < 16; ++component)
                WriteWord(bytes, offset + 8 + component * 4, bones[i].Bits[component]);
        }
        return bytes;
    }

    private static Matrix4x4 Matrix(uint[] bits)
    {
        float F(int index) => BitConverter.Int32BitsToSingle(unchecked((int)bits[index]));
        return new(F(0), F(1), F(2), F(3), F(4), F(5), F(6), F(7),
            F(8), F(9), F(10), F(11), F(12), F(13), F(14), F(15));
    }

    private static byte[] MetadataContainer(string mode)
    {
        byte[] emptyPalette = ExpectedField(uint.MaxValue, []);
        byte[] palette = mode switch
        {
            "repeated" => [..emptyPalette, ..emptyPalette],
            "uint16" => [0xc0, 8, 0, ..emptyPalette.AsSpan(5).ToArray()],
            "extended" => [0xff, 0, ..emptyPalette.AsSpan(1).ToArray()],
            _ => throw new ArgumentOutOfRangeException(nameof(mode))
        };
        byte[] skin = [..BitConverter.GetBytes(SmoClassIds.Skin), .."SBOO"u8,
            0, // Renderable terminator.
            0xe0, 8, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, // Model field0: reference-only Mesh ID2.
            0, ..palette, 0]; // Model terminator, Skin fields and terminator.
        byte[] mesh = [..BitConverter.GetBytes(SmoClassIds.MeshData), .."SBOO"u8, 0];
        byte[][] names = ["skin-metadata\0"u8.ToArray(), "mesh-catalog-only\0"u8.ToArray()];
        byte[][] bodies = [skin, mesh];
        uint[] types = [SmoClassIds.Skin, SmoClassIds.MeshData];
        int dataStart = SmoHeader.Size + names.Sum(name => 18 + name.Length) + 4;
        byte[] bytes = new byte[dataStart + skin.Length + mesh.Length];
        "FFPS"u8.CopyTo(bytes);
        WriteWord(bytes, 4, 0x26); WriteWord(bytes, 12, checked((uint)bytes.Length));
        WriteWord(bytes, 16, 2); WriteWord(bytes, 20, checked((uint)dataStart));
        WriteWord(bytes, 24, checked((uint)(skin.Length + mesh.Length))); WriteWord(bytes, 28, 2);
        int cursor = SmoHeader.Size;
        int logicalOffset = 0;
        for (int i = 0; i < bodies.Length; ++i)
        {
            WriteWord(bytes, cursor, checked((uint)i + 1)); cursor += 4;
            BinaryPrimitives.WriteUInt16LittleEndian(bytes.AsSpan(cursor), checked((ushort)names[i].Length)); cursor += 2;
            names[i].CopyTo(bytes, cursor); cursor += names[i].Length;
            WriteWord(bytes, cursor, types[i]);
            WriteWord(bytes, cursor + 4, checked((uint)logicalOffset));
            WriteWord(bytes, cursor + 8, checked((uint)bodies[i].Length)); cursor += 12;
            bodies[i].CopyTo(bytes, dataStart + logicalOffset);
            logicalOffset += bodies[i].Length;
        }
        return bytes;
    }

    private static void WriteWord(byte[] bytes, int offset, uint value) =>
        BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(offset), value);
    private static string Hash(ReadOnlySpan<byte> bytes) => Convert.ToHexString(SHA256.HashData(bytes));
    private static string HashFile(string path)
    {
        using var stream = File.OpenRead(path);
        return Convert.ToHexString(SHA256.HashData(stream));
    }
}
