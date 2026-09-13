using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text.Json;
using SmoViewer.Core;

namespace SmoViewer.FormatTests;

internal static class SmoTextureSourceRegression
{
    private sealed record ExpectedMip(int Index, int PayloadOffset, int PixelOffset, int PixelBytes,
        int Width, uint RuntimeMips, string Sha256);
    private sealed record Fixture(string Name, byte[] Fields, byte[] Pixels, int Representations = 1,
        bool Writable = false, bool Opaque = false, bool Local = false);
    private static string Hash(ReadOnlySpan<byte> bytes) => Convert.ToHexString(SHA256.HashData(bytes)).ToLowerInvariant();
    private static byte[] Word(uint value) => BitConverter.GetBytes(value);
    private static byte[] Field(int id, byte[] payload) =>
        [.. SmoDataBlockWriter.BuildHeader(id, checked((uint)payload.Length)), .. payload];
    // Test-only one-object FFPS envelope, following the existing Text fixture.
    // Production texture payloads below come from the shared native writer.
    private static SmoDocument Document(byte[] fields, uint platformMask = 2)
    {
        uint type = SmoClassIds.TextureData;
        byte[] body = [.. Word(type), .. Word(0x4F4F4253), .. fields];
        using var stream = new MemoryStream(); using var writer = new BinaryWriter(stream);
        foreach (uint word in new uint[] { 0x53504646, 0x26, 0, (uint)(54 + body.Length), platformMask, 54, (uint)body.Length, 1, 1 }) writer.Write(word);
        writer.Write((ushort)0); writer.Write(type); writer.Write(0u); writer.Write((uint)body.Length); writer.Write(0u); writer.Write(body);
        return SmoDocument.ParseOwned(stream.ToArray(), "synthetic-texture-source");
    }
    private static byte[] SeedNativeSection(byte[] serialized)
    {
        var document = Document(serialized[8..]);
        var embedded = SmoObjectFieldReader.Read(document, document.Objects[0]).Single(field => field.FieldType == 3).Payload;
        // Generic header traversal only; the seed has already been serialized
        // by the common writer. No texture/source grammar is recreated here.
        var native = new List<byte[]>();
        for (int offset = 0; offset < embedded.Length;)
        {
            if (!SmoDataBlockReader.TryReadHeader(embedded.Span, offset, out var header))
                throw new InvalidDataException("Shared writer seed has an invalid field extent.");
            if (header.FieldType == 1) native.Add(embedded.Slice(header.PayloadOffset, checked((int)header.PayloadSize)).ToArray());
            offset = checked((int)header.PayloadEnd);
        }
        return native.Single();
    }
    private static byte[] Local(byte[] native, byte[]? none = null) =>
        [.. Field(2, none ?? [0]), 0, .. Field(1, native), 0];
    private static byte[] Embedded(byte[] local) => [.. Field(3, local), 0];

    internal static int Run(string book, string bloom, string icebat, string output)
    {
        if (Directory.Exists(output)) throw new InvalidOperationException("Use a fresh output directory to retain earlier evidence.");
        Directory.CreateDirectory(output);
        int checks = 0;
        void Check(bool condition, string message) { ++checks; if (!condition) throw new InvalidDataException(message); }
        SmoTextureDataInfo Decode(SmoDocument document, SmoObjectEntry entry)
        {
            Check(SmoTextureDataDecoder.TryDecode(document, entry, out var decoded, out string error), error);
            return decoded!;
        }
        var realRows = new List<object>();
        var inputPaths = new[] { book, bloom, icebat }.Select(Path.GetFullPath).ToArray();
        string[] beforeHashes = inputPaths.Select(path => Hash(File.ReadAllBytes(path))).ToArray();
        Check(beforeHashes.SequenceEqual(new[] {
            "f8fc402e787a7ff1a7426a403074c40a454131886e931ac423c2a1742e168932",
            "3316bce73a9f4098a4bc813e1231a81c4beec46fab9c5bc34e6e9b5d1817e8f2",
            "6aec9ca21eb50fd93e89955551c3557ff19bd21260038ff6e7cf94eef178bf72" }),
            "Three selected inputs match the sealed source audit hashes.");
        var legacyDocument = SmoDocument.Load(book);
        var legacyEntries = legacyDocument.Objects.Where(entry => entry.TypeHash == SmoClassIds.TextureData).ToArray();
        Check(legacyDocument.Header.PlatformMask == 1 && legacyEntries.Length == 1, "Book is the one-texture legacy-common metadata input.");
        var legacy = Decode(legacyDocument, legacyEntries.Single());
        var legacyMip = legacy.SelectedRepresentation!.MipLevels.Single();
        Check(legacy.SourceKind == SmoTextureSourceKind.LegacyCrossPlatform && !legacy.UsesRuntimeSourceSelection && legacy.RuntimeMipLevelCount is null
            && legacy.SelectedRepresentation.Kind == SmoTextureRepresentationKind.CrossPlatformBgra32,
            "Book remains stored metadata; it does not claim a PC runtime load.");
        Check(MemoryMarshal.TryGetArray(legacyMip.PixelData, out var pixels) && MemoryMarshal.TryGetArray(legacyDocument.Data, out var fileBytes)
            && ReferenceEquals(pixels.Array, fileBytes.Array) && pixels.Count == 128 * 128 * 4
            && legacyMip.PixelData.Span.SequenceEqual(legacyDocument.Data.Span.Slice(pixels.Offset - fileBytes.Offset, pixels.Count)),
            "Legacy pixels are an exact bounded slice of the original source.");
        Check(!SmoTextureDataWriter.CanReplace(legacy, out _), "Legacy metadata does not become writable.");
        realRows.Add(new { file = Path.GetFullPath(book), entry = legacyEntries[0].Index, runtime = false,
            stored_mips = legacyMip.Index + 1, raw_pixel_sha256 = Hash(legacyMip.PixelData.Span) });

        // Exact offsets, stored bytes and runtime mip counts from
        // local-data/results/tools-core-cycle-20260910-0730/texture-source/pc-source-audit.json.
        var pcInputs = new[] {
            (Path: bloom, Expected: new[] {
                new ExpectedMip(4, 4006, 44, 262144, 256, 9, "ab089570239043e108bc3ed3213a5ad94d108a99c421b0b862c793919896946f"),
                new ExpectedMip(25, 313467, 44, 16384, 64, 7, "57dc1aba21e86937408e176a74d2618c4c999a7a282af98978859973ca8a1b8b") }),
            (Path: icebat, Expected: new[] {
                new ExpectedMip(5, 1344, 53, 4096, 32, 6, "2896b60b01a45e02e1291a77a8367b4147c4b0cc594c739a1181fe550b06b60e") }) };
        foreach (var item in pcInputs)
        {
            var document = SmoDocument.Load(item.Path);
            Check(document.Objects.Count(entry => entry.TypeHash == SmoClassIds.TextureData) == item.Expected.Length,
                "Every texture in the selected PC input is checked.");
            foreach (var wanted in item.Expected)
            {
                var entry = document.Objects[wanted.Index];
                var data = Decode(document, entry);
                var selected = data.SelectedRepresentation!;
                Check(selected.Kind == SmoTextureRepresentationKind.Direct3DBgra32 && selected.Width == wanted.Width
                    && selected.Height == wanted.Width && selected.MipLevels.Count == 1 && data.RuntimeMipLevelCount == wanted.RuntimeMips
                    && data.UsesRuntimeSourceSelection && selected.NativeDataFlag is not null
                    && data.ObservedRepresentations.Count == 1 && !data.HasOpaqueRepresentations,
                    "Actual source selection distinguishes one stored mip from the complete runtime chain.");
                var mip = selected.MipLevels.Single();
                Check(mip.DimensionsKnown && entry.PhysicalOffset + 8 == wanted.PayloadOffset && mip.PixelData.Length == wanted.PixelBytes
                    && Hash(mip.PixelData.Span) == wanted.Sha256
                    && mip.PixelData.Span.SequenceEqual(document.Data.Span.Slice(wanted.PayloadOffset + wanted.PixelOffset, wanted.PixelBytes)),
                    "Selected raw mip bytes and absolute source extent match the native audit.");
                Check(data.EditingIssue is null && SmoTextureDataWriter.CanReplace(data, out _), "Existing embedded PC single-mip replacement remains supported.");
                realRows.Add(new { file = Path.GetFullPath(item.Path), entry = entry.Index, runtime = true,
                    stored_mips = selected.MipLevels.Count, runtime_mips = data.RuntimeMipLevelCount, raw_pixel_sha256 = Hash(mip.PixelData.Span) });
            }
        }

        byte[] pixelsA = [1, 2, 3, 255, 4, 5, 6, 255, 7, 8, 9, 255, 10, 11, 12, 255];
        byte[] pixelsB = [90, 80, 70, 255, 60, 50, 40, 255, 30, 20, 10, 255, 9, 8, 7, 255];
        byte[] seedA = SmoTextureDataWriter.CreateBgraObject(2, 2, pixelsA);
        byte[] seedB = SmoTextureDataWriter.CreateBgraObject(2, 2, pixelsB);
        byte[] nativeA = SeedNativeSection(seedA), nativeB = SeedNativeSection(seedB);
        Fixture[] fixtures = [
            new("canonical-shared-writer", seedA[8..], pixelsA, Writable: true),
            new("source-none-local", Local(nativeA), pixelsA, Local: true),
            new("unknown-source-field", [.. Field(7, [0xFE]), .. seedA[8..]], pixelsA),
            new("repeated-native", Embedded([.. Field(2, [0]), 0, .. Field(1, nativeA), .. Field(1, nativeB), 0]), pixelsB, 2),
            new("reordered-platform", Embedded([.. Field(2, [0]), 0, .. Field(1, nativeA), .. Field(6, Word(2)), 0]), pixelsA),
            new("repeated-platform", Embedded([.. Field(2, [0]), 0, .. Field(6, Word(2)), .. Field(6, Word(2)), .. Field(1, nativeA), 0]), pixelsA),
            new("repeated-embedded-source", [.. Field(3, Local(nativeA)), .. Field(3, Local(nativeB)), 0], pixelsB, 2),
            new("opaque-skipped-cross", Embedded([.. Field(2, [0]), 0, .. Field(1, nativeA), .. Field(0, [0xFF, 0xEE]), 0]), pixelsA, Opaque: true),
            new("arbitrary-source-none-payload", Embedded(Local(nativeA, [0xFE, 0x80, 0x17])), pixelsA)
        ];
        var syntheticRows = new List<object>();
        SmoDocument? opaqueDocument = null;
        foreach (var fixture in fixtures)
        {
            var document = Document(fixture.Fields);
            var data = Decode(document, document.Objects[0]);
            Check(data.ObservedRepresentations.Count == fixture.Representations
                && ReferenceEquals(data.SelectedRepresentation, data.ObservedRepresentations[^1])
                && data.SelectedRepresentation!.MipLevels.Single().PixelData.Span.SequenceEqual(fixture.Pixels)
                && data.RuntimeMipLevelCount == 2 && data.HasOpaqueRepresentations == fixture.Opaque
                && data.SourceKind == (fixture.Local ? SmoTextureSourceKind.SourceNone : SmoTextureSourceKind.Embedded),
                $"{fixture.Name}: final successful initialization controls selection; skipped bytes remain opaque.");
            Check(SmoTextureDataWriter.CanReplace(data, out string reason) == fixture.Writable
                && (fixture.Writable ? data.EditingIssue is null : !string.IsNullOrEmpty(data.EditingIssue) && !string.IsNullOrEmpty(reason)),
                $"{fixture.Name}: newly readable forms do not silently enable the old editor path.");
            if (fixture.Opaque) opaqueDocument = document;
            syntheticRows.Add(new { fixture.Name, stored = data.ObservedRepresentations.Count, selected_sha256 = Hash(fixture.Pixels),
                data.HasOpaqueRepresentations, data.EditingIssue, writable = fixture.Writable });
        }
        // Proven cross field5 layout: width, height, format, pixel size, then
        // stored bytes. Two records in one section must retain separate codec projections.
        byte[][] xrgbPixels = [[11, 22, 33, 17], [91, 82, 73, 34]];
        byte[] xrgbSection = [
            .. Field(5, [.. Word(1), .. Word(1), .. Word(1), .. Word(4), .. xrgbPixels[0]]),
            .. Field(5, [.. Word(1), .. Word(1), .. Word(1), .. Word(4), .. xrgbPixels[1]]), 0];
        var xrgbDocument = Document([.. Field(2, [0]), 0, .. Field(6, Word(1)), .. Field(0, xrgbSection), 0]);
        var xrgb = Decode(xrgbDocument, xrgbDocument.Objects[0]);
        Check(xrgb.SourceKind == SmoTextureSourceKind.SourceNone && xrgb.PlatformType == 1
            && xrgb.UsesRuntimeSourceSelection && xrgb.ObservedRepresentations.Count == 2
            && ReferenceEquals(xrgb.SelectedRepresentation, xrgb.ObservedRepresentations[^1])
            // Original NormalizeDimension(1) is 2: stored 1x1, runtime 2x2 + 1x1.
            && xrgb.RuntimeMipLevelCount == 2 && !xrgb.HasOpaqueRepresentations,
            "Repeated XRGB field5 records remain individually observed and the last initialization is selected.");
        for (int i = 0; i < xrgbPixels.Length; ++i)
        {
            var representation = xrgb.ObservedRepresentations[i];
            byte[] expectedPreview = [xrgbPixels[i][0], xrgbPixels[i][1], xrgbPixels[i][2], 255];
            Check(representation.Kind == SmoTextureRepresentationKind.CrossPlatformBgrx32
                && representation.Width == 1 && representation.Height == 1 && representation.FormatValue == 1
                && representation.MipLevels.Count == 1
                && representation.MipLevels[0].PixelData.Span.SequenceEqual(xrgbPixels[i])
                && representation.BgraPreview.Span.SequenceEqual(expectedPreview),
                $"XRGB record {i} retains its own stored BGR/unused alpha and produces its own opaque BGRA preview.");
        }
        Check(!SmoTextureDataWriter.CanReplace(xrgb, out string xrgbEditingIssue)
            && !string.IsNullOrEmpty(xrgb.EditingIssue) && !string.IsNullOrEmpty(xrgbEditingIssue),
            "Repeated cross-platform records remain read-only.");
        syntheticRows.Add(new { Name = "repeated-xrgb-field5", stored = xrgb.ObservedRepresentations.Count,
            stored_sha256 = xrgb.ObservedRepresentations.Select(value => Hash(value.MipLevels[0].PixelData.Span)).ToArray(),
            preview_sha256 = xrgb.ObservedRepresentations.Select(value => Hash(value.BgraPreview.Span)).ToArray(),
            xrgb.RuntimeMipLevelCount, xrgb.EditingIssue, writable = false });
        var opaque = Decode(opaqueDocument!, opaqueDocument!.Objects[0]);
        Check(opaque.CrossPlatform is null && opaque.HasOpaqueRepresentations
            && SmoTextureDecoder.TryDecode(opaqueDocument, opaqueDocument.Objects[0], out var preview, out _)
            && preview!.Bgra32Pixels.Span.SequenceEqual(pixelsA), "Preview uses actual native selection and does not decode skipped cross bytes.");
        bool refused = false;
        try { SmoTextureDataWriter.BuildReplacementObjectBgra(opaqueDocument, 0, 2, 2, pixelsB); }
        catch (NotSupportedException) { refused = true; }
        Check(refused, "The editor refuses an opaque second representation before rewriting fields.");
        var foreign = Document(seedB[8..]);
        Check(!SmoTextureDataDecoder.TryDecode(opaqueDocument, foreign.Objects[0], out _, out string foreignError)
            && foreignError.StartsWith("TEXTURE_INPUT_INVALID", StringComparison.Ordinal), "Foreign entries are rejected at the shared source boundary.");
        var malformed = Document([.. SmoDataBlockWriter.BuildHeader(3, checked((uint)seedA.Length + 32)), 0]);
        Check(!SmoTextureDataDecoder.TryDecode(malformed, malformed.Objects[0], out _, out string malformedError)
            && malformedError.StartsWith("TEXTURE_PC_SOURCE_INVALID", StringComparison.Ordinal), "Malformed source extents fail explicitly.");
        Check(inputPaths.Select(path => Hash(File.ReadAllBytes(path))).SequenceEqual(beforeHashes), "All three original source files remain byte-identical.");
        File.WriteAllText(Path.Combine(output, "report.json"), JsonSerializer.Serialize(new {
            status = "passed", checks, real = realRows, synthetic = syntheticRows, foreignError, malformedError,
            source_sha256 = beforeHashes,
            native_sha256 = Hash(File.ReadAllBytes(Path.Combine(AppContext.BaseDirectory, "SparkplugViewerNative.dll"))),
            core_assembly_sha256 = Hash(File.ReadAllBytes(typeof(SmoTextureDataDecoder).Assembly.Location)),
            test_assembly_sha256 = Hash(File.ReadAllBytes(typeof(SmoTextureSourceRegression).Assembly.Location)),
            scope = "PC actual source reader metadata and selected stored pixels; book remains legacy-common metadata, not a PC runtime result."
        }, new JsonSerializerOptions { WriteIndented = true }) + "\n");
        Console.WriteLine($"Texture source regression: {checks} checks, four real textures, {fixtures.Length + 1} synthetic reader shapes.");
        return 0;
    }

    internal static int RunPlatformDispatch(string bloomingFlower, string outputDirectory, string? beforeDll = null)
    {
        if (Directory.Exists(outputDirectory)) throw new InvalidOperationException("Use a fresh output directory to retain earlier evidence.");
        Directory.CreateDirectory(outputDirectory);
        int checks = 0;
        void Check(bool condition, string message) { ++checks; if (!condition) throw new InvalidDataException(message); }
        string currentDll = Path.Combine(AppContext.BaseDirectory, "SparkplugViewerNative.dll");
        using var current = new WholeGraphProbe(currentDll);
        using var before = beforeDll is null ? null : new WholeGraphProbe(Path.GetFullPath(beforeDll));
        byte[] pixels = [1, 2, 3, 255, 4, 5, 6, 255, 7, 8, 9, 255, 10, 11, 12, 255];
        byte[] native = SeedNativeSection(SmoTextureDataWriter.CreateBgraObject(2, 2, pixels));
        // The same original-backed cross field5 layout already exercised above.
        byte[] cross = [.. Field(5, [.. Word(2), .. Word(2), .. Word(0), .. Word(4), .. pixels]), 0];
        byte[] source = [.. Field(2, [0]), 0];
        var fixtures = new[] {
            (Name: "common-source-none-cross", Document: Document([.. source, .. Field(0, cross), 0], 1), OldLoads: true),
            // Original common42F180 skips field6 as unknown. The old host DX
            // registration interprets it as a native-platform override and skips
            // cross field0. This is a declared synthetic dispatch discriminator.
            (Name: "common-unknown-field6-cross", Document: Document([.. source, .. Field(6, Word(2)), .. Field(0, cross), 0], 1), OldLoads: false),
            (Name: "pc-source-none-native", Document: Document(Local(native), 2), OldLoads: true)
        };
        var rows = new List<object>();
        foreach (var fixture in fixtures)
        {
            var document = fixture.Document;
            string inputHash = Hash(document.Data.Span);
            File.WriteAllBytes(Path.Combine(outputDirectory, fixture.Name + ".smo"), document.Data.ToArray());
            var loaded = SmoLoadedResources.Get(document);
            Check(loaded.LoadIssue is null, $"{fixture.Name}: whole SmoLoadedResources load: {loaded.LoadIssue}");
            Check(loaded.ReferenceTraceIssue is null && loaded.ReferenceTrace?.CoveredExtentCount == 1,
                $"{fixture.Name}: the actual whole graph reader covers its one TextureData extent.");
            var actual = current.Capture(document);
            Check(actual.Success && actual.Objects == 1 && actual.Nodes == 0 && actual.RootId == 1,
                $"{fixture.Name}: whole graph identity, not a leaf texture inspector: {actual.Error}");
            Check(actual.Width == 2 && actual.Height == 2 && actual.SurfaceFormat == 3 && actual.Mips == 2
                && actual.PixelSha256 == Hash(pixels), $"{fixture.Name}: actual initialized DXTexture retains expected base pixels and runtime mip count.");
            WholeGraphCapture? previous = before?.Capture(document);
            if (previous is not null)
            {
                Check(previous.Success == fixture.OldLoads, $"{fixture.Name}: the saved old DLL exposes the expected registration behavior.");
                if (fixture.OldLoads)
                    Check(previous == actual, $"{fixture.Name}: supported old/new whole graph results stay identical.");
                else
                    Check(previous.Error?.Contains("No restored native DX mip payload", StringComparison.Ordinal) == true,
                        "The old DX255 registration skips cross pixels after field6=2; failure is not an unrelated loader error.");
            }
            Check(Hash(document.Data.Span) == inputHash, $"{fixture.Name}: whole graph reads preserve the input bytes.");
            rows.Add(new { fixture.Name, platform = document.Header.PlatformMask, input_sha256 = inputHash,
                coverage = loaded.ReferenceTrace!.CoveredExtentCount, loaded.SceneIssue, current = actual, before = previous });
        }

        byte[] original = File.ReadAllBytes(bloomingFlower);
        string originalHash = Hash(original);
        Check(original.Length == 110101 && originalHash == "28ff90a536e40fbc84fbd8bf06150a3118d25b1cb7b4aafb9641d8afdf19c9a1",
            "The real negative is the sealed blooming_flower input from the original caller probe.");
        var legacy = SmoDocument.ParseOwned(original, Path.GetFullPath(bloomingFlower));
        var rejected = SmoLoadedResources.Get(legacy);
        Check(rejected.LoadIssue?.Contains("Invalid bounded derived section", StringComparison.Ordinal) == true
            && rejected.LoadIssue.Contains("[inline ID 6, class 1060524470]", StringComparison.Ordinal),
            "The real bare texture fails at the known bounded section before parent bytes can be consumed.");
        Check(rejected.ReferenceTrace is null && rejected.Models.Count == 0,
            "A failed whole graph does not become an empty successful model/coverage snapshot.");
        var negative = current.Capture(legacy, inspectTexture: false);
        Check(!negative.Success && negative.Error?.Contains("Invalid bounded derived section", StringComparison.Ordinal) == true,
            "The whole graph C ABI retains the original barelegacy refusal.");
        WholeGraphCapture? oldNegative = before?.Capture(legacy, inspectTexture: false);
        if (oldNegative is not null)
            Check(!oldNegative.Success && oldNegative.Error == negative.Error,
                "The dispatch correction preserves the saved old DLL's explicit barelegacy rejection.");
        Check(Hash(original) == originalHash && Hash(File.ReadAllBytes(bloomingFlower)) == originalHash,
            "The real original and its loaded byte array remain unchanged.");
        File.WriteAllText(Path.Combine(outputDirectory, "report.json"), JsonSerializer.Serialize(new {
            status = "passed", checks, synthetic = rows,
            real = new { file = Path.GetFullPath(bloomingFlower), source_sha256 = originalHash, rejected.LoadIssue,
                current = negative, before = oldNegative },
            native_sha256 = Hash(File.ReadAllBytes(currentDll)),
            before_native_sha256 = beforeDll is null ? null : Hash(File.ReadAllBytes(beforeDll)),
            core_assembly_sha256 = Hash(File.ReadAllBytes(typeof(SmoLoadedResources).Assembly.Location)),
            test_assembly_sha256 = Hash(File.ReadAllBytes(typeof(SmoTextureSourceRegression).Assembly.Location)),
            scope = "Whole ResourceGraph dispatch and actual runtime pixels: common1/DX6; one real barelegacy negative. No original whole-game acceptance claim."
        }, new JsonSerializerOptions { WriteIndented = true }) + "\n");
        Console.WriteLine($"Texture platform dispatch: {checks} checks; three whole graph positives and one original barelegacy refusal.");
        return 0;
    }

    private sealed record WholeGraphCapture(bool Success, string? Error, uint Objects = 0, uint Nodes = 0,
        uint RootId = 0, uint Width = 0, uint Height = 0, uint SurfaceFormat = 0, uint Mips = 0, string? PixelSha256 = null);

    // Test-only direct C ABI binding permits comparison with an explicitly
    // supplied saved DLL. Every pointer remains inside its owning module.
    private sealed class WholeGraphProbe : IDisposable
    {
        [StructLayout(LayoutKind.Sequential)] private struct TextureInfo { public uint Width, Height, Format, Mips; }
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate IntPtr Load([In] byte[] bytes, uint count);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate void Destroy(IntPtr graph);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate IntPtr LastError();
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int Info(IntPtr graph, out uint objects, out uint nodes, out uint root);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int Texture(IntPtr graph, uint id, out TextureInfo info);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int Pixels(IntPtr graph, uint id, [Out] byte[] bytes, uint count);
        private readonly IntPtr library;
        private readonly Load load;
        private readonly Destroy destroy;
        private readonly LastError lastError;
        private readonly Info info;
        private readonly Texture texture;
        private readonly Pixels pixels;
        internal WholeGraphProbe(string path)
        {
            library = NativeLibrary.Load(Path.GetFullPath(path));
            try
            {
                T Bind<T>(string name) where T : Delegate => Marshal.GetDelegateForFunctionPointer<T>(NativeLibrary.GetExport(library, name));
                load = Bind<Load>("spv_graph_load"); destroy = Bind<Destroy>("spv_graph_destroy");
                lastError = Bind<LastError>("spv_last_error"); info = Bind<Info>("spv_graph_info");
                texture = Bind<Texture>("spv_graph_texture"); pixels = Bind<Pixels>("spv_graph_texture_bgra");
            }
            catch { NativeLibrary.Free(library); throw; }
        }
        private string Error() => Marshal.PtrToStringUTF8(lastError()) ?? "Missing native diagnostic";
        private void Require(int result) { if (result == 0) throw new InvalidDataException(Error()); }
        internal WholeGraphCapture Capture(SmoDocument document, bool inspectTexture = true)
        {
            byte[] bytes = document.Data.ToArray();
            IntPtr graph = load(bytes, checked((uint)bytes.Length));
            if (graph == IntPtr.Zero) return new(false, Error());
            try
            {
                Require(info(graph, out uint count, out uint nodes, out uint root));
                if (!inspectTexture) return new(true, null, count, nodes, root);
                Require(texture(graph, root, out var state));
                if (state.Width != 2 || state.Height != 2) throw new InvalidDataException("The graph regression accepts only the declared 2x2 texture fixture.");
                byte[] bgra = new byte[16];
                Require(pixels(graph, root, bgra, checked((uint)bgra.Length)));
                return new(true, null, count, nodes, root, state.Width, state.Height, state.Format, state.Mips, Hash(bgra));
            }
            finally { destroy(graph); }
        }
        public void Dispose() => NativeLibrary.Free(library);
    }

    internal static int RunPs2(string objectFile, string outputDirectory)
    {
        if (Directory.Exists(outputDirectory)) throw new InvalidOperationException("Use a fresh output directory to retain earlier evidence.");
        Directory.CreateDirectory(outputDirectory);
        int checks = 0;
        void Check(bool condition, string message) { ++checks; if (!condition) throw new InvalidDataException(message); }
        byte[] raw = File.ReadAllBytes(objectFile);
        string sourceHash = Hash(raw);
        Check(raw.Length == 267 && sourceHash == "01dd193e83066de03f839870e486e1105a49eddeed480ba409c7c418e0cd98c1",
            "Selected noisesm object matches the sealed PS2 specimen.");
        var document = Document(raw[8..], 8);
        Check(document.Header.PlatformMask == 8 && document.Data.Span[54..].SequenceEqual(raw),
            "Only a test FFPS envelope is added; the complete original object and its eight-byte prefix remain exact.");
        Check(SmoDataBlockReader.TryReadHeader(raw, 30, out var nativeHeader) && nativeHeader.FieldType == 0
            && nativeHeader.HeaderSize == 5 && nativeHeader.PayloadOffset == 35 && nativeHeader.PayloadSize == 229 && raw[35] == 1,
            "The flag perturbation targets the actual native field0 byte after its five-byte header.");
        Check(SmoTextureDataDecoder.TryDecode(document, document.Objects[0], out var decoded, out string error), error);
        var representation = decoded!.PlatformSpecific!;
        Check(representation.Kind == SmoTextureRepresentationKind.Ps2Indexed4 && representation.Width == 16
            && representation.Height == 16 && representation.FormatValue == 0 && representation.AuxiliaryValue == 99590
            && representation.NativeDataFlag == 1 && representation.MipLevels.Count == 1,
            "PS2 image header and raw native flag match the original stored metadata.");
        Check(representation.Palette.Length == 64 && representation.Palette.Span.SequenceEqual(raw.AsSpan(30 + 26, 64)),
            "The 64 palette bytes preserve the native inspector's exact source extent.");
        var mip = representation.MipLevels.Single();
        Check(!mip.DimensionsKnown && mip.Width == 0 && mip.Height == 0
            && mip.Descriptor0 == 0 && mip.Descriptor1 == 16 && mip.Descriptor2 == 16,
            "Raw mip descriptors are retained; unproven per-mip dimensions remain explicitly unknown.");
        Check(mip.PixelData.Length == 128 && mip.PixelData.Span.SequenceEqual(raw.AsSpan(30 + 106, 128)),
            "Wire dataSize determines the exact 128-byte raw mip slice.");
        Check(!decoded.UsesRuntimeSourceSelection && decoded.RuntimeMipLevelCount is null,
            "PS2 metadata does not claim a runtime source selection or generated mip chain.");
        Check(!SmoTextureDataWriter.CanReplace(decoded, out string editingIssue) && !string.IsNullOrEmpty(editingIssue),
            "PS2 metadata does not become writable through the PC replacement path.");
        Check(!SmoTextureDecoder.TryDecode(document, document.Objects[0], out _, out string previewError)
            && previewError.StartsWith("PS2_TEXTURE_PREVIEW_UNSUPPORTED", StringComparison.Ordinal),
            "Native PS2 metadata is not guessed into a BGRA preview.");

        byte[] zeroFlag = raw.ToArray(); zeroFlag[35] = 0;
        var zeroDocument = Document(zeroFlag[8..], 8);
        Check(SmoTextureDataDecoder.TryDecode(zeroDocument, zeroDocument.Objects[0], out var zero, out string zeroError), zeroError);
        var zeroRepresentation = zero!.PlatformSpecific!;
        Check(zeroRepresentation.NativeDataFlag == 0 && zeroRepresentation.PixelDataPresent
            && zeroRepresentation.MipLevels.Single().PixelData.Span.SequenceEqual(mip.PixelData.Span)
            && zeroRepresentation.MipLevels.Single().PixelData.Length == 128,
            "A zero raw flag does not suppress the subsequently stored mip bytes.");

        byte[] malformed = raw.ToArray();
        // Keep the outer object/section extents intact. Expanding native field0
        // by one byte consumes its section terminator and must fail bounded inspection.
        BitConverter.GetBytes(nativeHeader.PayloadSize + 1).CopyTo(malformed, 31);
        var malformedDocument = Document(malformed[8..], 8);
        Check(!SmoTextureDataDecoder.TryDecode(malformedDocument, malformedDocument.Objects[0], out _, out string malformedError)
            && !string.IsNullOrEmpty(malformedError), "Malformed native field bounds are rejected explicitly.");
        Check(Hash(File.ReadAllBytes(objectFile)) == sourceHash && Hash(raw) == sourceHash,
            "The original extracted object and input array remain unchanged.");
        File.WriteAllText(Path.Combine(outputDirectory, "report.json"), JsonSerializer.Serialize(new
        {
            status = "passed", checks, object_file = Path.GetFullPath(objectFile), source_sha256 = sourceHash,
            image_width = representation.Width, image_height = representation.Height, representation.FormatValue,
            representation.AuxiliaryValue, native_flag = representation.NativeDataFlag,
            zero_native_flag = zeroRepresentation.NativeDataFlag, mip.DimensionsKnown, mip.Width, mip.Height,
            descriptors = new[] { mip.Descriptor0, mip.Descriptor1, mip.Descriptor2 },
            palette_sha256 = Hash(representation.Palette.Span), raw_mip_sha256 = Hash(mip.PixelData.Span),
            editingIssue, previewError, malformedError,
            native_sha256 = Hash(File.ReadAllBytes(Path.Combine(AppContext.BaseDirectory, "SparkplugViewerNative.dll"))),
            core_assembly_sha256 = Hash(File.ReadAllBytes(typeof(SmoTextureDataDecoder).Assembly.Location)),
            test_assembly_sha256 = Hash(File.ReadAllBytes(typeof(SmoTextureSourceRegression).Assembly.Location)),
            scope = "One real PS2 object plus zero-flag and malformed-field variants; metadata only, no PS2 runtime or pixel preview."
        }, new JsonSerializerOptions { WriteIndented = true }) + "\n");
        Console.WriteLine($"PS2 texture metadata: {checks} checks; no PS2 runtime or BGRA preview.");
        return 0;
    }
}
