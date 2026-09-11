using System.Diagnostics;
using System.Security.Cryptography;
using System.Text.Json;
using SmoImporter.Core;
using SmoLVLcreator.Core;
using SmoViewer.Core;

namespace SmoLVLcreator.CoreTests;

// Directed migration captures, with the same operations usable before/after
// replacing the envelope encoder. Payload authoring is deliberately unchanged.
internal static class SmoContainerEnvelopeRegression
{
    public static int RunBoundaries()
    {
        int checks = 0;
        var empty = new SmoContainerEnvelope(0x01020304, 0x89ABCDEF, 0x11223344, [], 0);
        byte[] golden = Convert.FromHexString(
            "4646505304030201EFCDAB89240000004433221124000000000000000000000000000000");
        Check(empty.AllocateContainer().AsSpan().SequenceEqual(golden), "empty golden prefix");
        byte[] guarded = Enumerable.Repeat((byte)0xA5, 52).ToArray();
        empty.WritePrefix(guarded.AsSpan(8, 36));
        Check(guarded.AsSpan(8, 36).SequenceEqual(golden) &&
            guarded.Take(8).All(b => b == 0xA5) && guarded.Skip(44).All(b => b == 0xA5), "prefix guards");
        byte[] shortOutput = Enumerable.Repeat((byte)0x5A, 35).ToArray();
        Throws<ArgumentException>(() => empty.WritePrefix(shortOutput.AsSpan()), "short output");
        Check(shortOutput.All(b => b == 0x5A), "short output unchanged");
        Throws<ArgumentOutOfRangeException>(() => new SmoContainerEnvelope(1, 0, 1, [], -1), "negative data");
        Throws<OverflowException>(() => new SmoContainerEnvelope(1, 0, 1, [], int.MaxValue), "managed file size overflow");
        byte[][] names = [[], [0], [0x41, 0, 0x5A, 0], [0xFF, 0xFE, 0]];
        var entries = names.Select((name, i) => new SmoContainerEntry((uint)(100 + i), name,
            SmoClassIds.Node, (uint)(i * 9), 9)).ToArray();
        var envelope = new SmoContainerEnvelope(1, 0xAABBCCDD, 1, entries, entries.Length * 9);
        // Input ownership: later changes in the caller's name buffers must not
        // alter an already prepared envelope.
        names[2][2] = 0x42;
        byte[] container = envelope.AllocateContainer();
        for (int i = 0; i < entries.Length; ++i)
        {
            Span<byte> payload = container.AsSpan(envelope.DataStart + i * 9, 9);
            System.Buffers.Binary.BinaryPrimitives.WriteUInt32LittleEndian(payload, SmoClassIds.Node);
            "SBOO"u8.CopyTo(payload[4..]);
        }
        SmoDocument inspected = SmoDocument.Parse(container);
        Check(inspected.Objects.Count == 4, "shared reader entry count");
        Check(!inspected.HasErrors && inspected.Objects.All(e => e.SignatureMatches), "valid synthetic object signatures");
        Check(inspected.Objects[0].RawName.Length == 0, "null name remains length zero");
        Check(inspected.Objects[1].RawName.Span.SequenceEqual(new byte[] { 0 }), "nonnull empty name");
        Check(inspected.Objects[2].RawName.Span.SequenceEqual(new byte[] { 0x41, 0, 0x5A, 0 }), "embedded NUL/trailing bytes and owned copy");
        Check(inspected.Objects[3].RawName.Span.SequenceEqual(new byte[] { 0xFF, 0xFE, 0 }), "non-UTF8 raw bytes");
        Check(inspected.Header.Unknown08 == 0xAABBCCDD, "unknown header word preserved");
        byte[] longest = new byte[ushort.MaxValue];
        var maximum = new SmoContainerEnvelope(1, 0, 1,
            [new(1, longest, SmoClassIds.Node, 0, 0)], 0);
        Check(maximum.AllocateContainer().Length == 36 + 18 + ushort.MaxValue, "maximum raw name length");
        Throws<InvalidDataException>(() => new SmoContainerEnvelope(1, 0, 1,
            [new(1, new byte[ushort.MaxValue + 1], SmoClassIds.Node, 0, 0)], 0), "oversized raw name rejected");
        using var nonseekable = new NonSeekableOutput();
        empty.WritePrefix(nonseekable);
        Check(nonseekable.Bytes.AsSpan().SequenceEqual(golden), "nonseekable prefix stream");
        using var readonlyStream = new MemoryStream(new byte[36], writable: false);
        Throws<ArgumentException>(() => empty.WritePrefix(readonlyStream), "readonly stream rejected");
        Console.WriteLine(JsonSerializer.Serialize(new { status = "passed", checks, binaries = LoadedBinaries() }));
        return 0;

        void Check(bool valid, string message)
        { ++checks; if (!valid) throw new InvalidDataException(message); }
        void Throws<T>(Action action, string message) where T : Exception
        { try { action(); } catch (T) { ++checks; return; } throw new InvalidDataException(message); }
    }

    private sealed class NonSeekableOutput : Stream
    {
        private readonly MemoryStream _output = new();
        public byte[] Bytes => _output.ToArray();
        public override bool CanRead => false;
        public override bool CanSeek => false;
        public override bool CanWrite => true;
        public override long Length => throw new NotSupportedException();
        public override long Position { get => throw new NotSupportedException(); set => throw new NotSupportedException(); }
        public override void Flush() => _output.Flush();
        public override int Read(byte[] buffer, int offset, int count) => throw new NotSupportedException();
        public override long Seek(long offset, SeekOrigin origin) => throw new NotSupportedException();
        public override void SetLength(long length) => throw new NotSupportedException();
        public override void Write(byte[] buffer, int offset, int count) => _output.Write(buffer, offset, count);
        public override void Write(ReadOnlySpan<byte> buffer) => _output.Write(buffer);
        protected override void Dispose(bool disposing)
        { if (disposing) _output.Dispose(); base.Dispose(disposing); }
    }

    public static int Run(string sourcePath, string outputDirectory, bool leafOnly = false)
    {
        if (Directory.Exists(outputDirectory))
            throw new IOException("Use a fresh capture directory.");
        Directory.CreateDirectory(outputDirectory);
        SmoDocument source = SmoDocument.Load(sourcePath);
        if (source.HasErrors) throw new InvalidDataException("Invalid source container.");
        var records = new List<object>();
        if (leafOnly)
        {
            var leaf = source.Objects.Last(e => !source.Objects.Any(other =>
                other.Index != e.Index && other.PhysicalOffset >= e.PhysicalOffset && other.PhysicalEnd <= e.PhysicalEnd));
            var original = source.Data.Slice(checked((int)leaf.PhysicalOffset), checked((int)leaf.SerializedSize));
            if (original.Span[^1] != 0) throw new InvalidDataException("Expected terminated leaf fixture.");
            byte[] field = SmoDataBlockWriter.BuildField(30, new byte[] { 0xA5, 0, 0x5A });
            byte[] replacement = new byte[original.Length + field.Length];
            original.Span[..^1].CopyTo(replacement);
            field.CopyTo(replacement, original.Length - 1);
            Capture("leaf-replace-field", () => SmoLeafObjectReplacer.Replace(source, leaf.Index, replacement));
            return Finish();
        }
        byte[] edited = Capture("editing-add-field", () =>
        {
            var transaction = new SmoMutationTransaction(source);
            transaction.AddField(0, 30, new byte[] { 0xA5, 0, 0x5A }, SmoFieldInsertion.BeforeTerminal);
            return transaction.Commit().Data;
        });
        byte[] inserted = Capture("importer-insert-field", () =>
            SmoVisualForestInjector.Inject(source, source.Objects[0].Id,
                [new(source.Objects[0].Id,
                    SmoDataBlockWriter.BuildField(30, new byte[] { 0xA5, 0, 0x5A }), [])]));
        if (!edited.AsSpan().SequenceEqual(inserted))
            throw new InvalidDataException("Equivalent field insertion paths disagree.");
        SmoObjectEntry? child = source.Objects.FirstOrDefault(e => e.ParentIndex >= 0);
        if (child is not null)
            Capture("importer-remove-branch", () => SmoVisualForestInjector.RemoveInlineBranch(
                source, source.Objects[child.ParentIndex!.Value].Id, child.Id));
        SmoProject project = SmoProject.Import(source);
        byte[] preview = Capture("project-preview", () =>
            SmoProjectSerializer.CreateCurrentDocument(project).Data.ToArray());
        byte[] streamed = Capture("project-stream", () =>
        {
            using var stream = new MemoryStream();
            SmoProjectSerializer.Write(project, stream);
            return stream.ToArray();
        });
        if (!preview.AsSpan().SequenceEqual(source.Data.Span) ||
            !streamed.AsSpan().SequenceEqual(source.Data.Span))
            throw new InvalidDataException("Unchanged project did not preserve source bytes.");
        return Finish();

        int Finish()
        {
            var result = new { status = "passed", source = Path.GetFullPath(sourcePath),
                sourceSha256 = Convert.ToHexString(SHA256.HashData(source.Data.Span)),
                binaries = LoadedBinaries(), records };
            File.WriteAllText(Path.Combine(outputDirectory, "capture.json"),
                JsonSerializer.Serialize(result, new JsonSerializerOptions { WriteIndented = true }));
            Console.WriteLine(JsonSerializer.Serialize(new { status = "passed", operations = records.Count }));
            return 0;
        }

        byte[] Capture(string name, Func<byte[]> operation)
        {
            byte[] value = operation(); // warm the existing/native path
            double milliseconds = 0;
            long allocated = 0;
            const int repetitions = 7;
            for (int i = 0; i < repetitions; i++)
            {
                long before = GC.GetAllocatedBytesForCurrentThread();
                long start = Stopwatch.GetTimestamp();
                byte[] repeated = operation();
                milliseconds += Stopwatch.GetElapsedTime(start).TotalMilliseconds;
                allocated += GC.GetAllocatedBytesForCurrentThread() - before;
                if (!value.AsSpan().SequenceEqual(repeated))
                    throw new InvalidDataException($"Nondeterministic output: {name}");
            }
            SmoDocument parsed = SmoDocument.Parse(value);
            if (parsed.HasErrors) throw new InvalidDataException($"Invalid generated index: {name}");
            File.WriteAllBytes(Path.Combine(outputDirectory, name + ".smo"), value);
            records.Add(new { name, bytes = value.Length, objects = parsed.Objects.Count,
                sha256 = Convert.ToHexString(SHA256.HashData(value)), repetitions,
                milliseconds, managedAllocatedBytes = allocated });
            return value;
        }
    }

    private static object[] LoadedBinaries()
    {
        IEnumerable<string> paths = AppDomain.CurrentDomain.GetAssemblies()
            .Where(a => !a.IsDynamic && a.GetName().Name!.StartsWith("Smo", StringComparison.Ordinal))
            .Select(a => a.Location).Append(Path.Combine(AppContext.BaseDirectory, "SparkplugViewerNative.dll"));
        return paths.Distinct().Select(path => (object)new { path,
            sha256 = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path))) }).ToArray();
    }
}
