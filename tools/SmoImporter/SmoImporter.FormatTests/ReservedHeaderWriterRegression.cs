using System.Text.Json;
using SmoViewer.Core;

internal static class ReservedHeaderWriterRegression
{
    public static int Run(string output)
    {
        if (Directory.Exists(output)) throw new InvalidOperationException("Use a new output directory to preserve prior evidence.");
        Directory.CreateDirectory(output);
        int checks = 0;
        void Check(bool value, string message)
        { if (!value) throw new InvalidDataException(message); ++checks; }
        void Reject(Action action, string message)
        {
            bool rejected = false;
            try { action(); }
            catch (Exception error) when (error is ArgumentException or InvalidDataException or NotSupportedException or OverflowException)
            { rejected = true; }
            Check(rejected, message);
        }
        var rows = new List<object>();
        var fixtures = new[]
        {
            (Old: "a808", Size: 1u, Expected: "a801"),
            (Old: "a808", Size: 8u, Expected: "a808"),
            (Old: "a808", Size: 255u, Expected: "a8ff"),
            (Old: "c80800", Size: 8u, Expected: "c80800"),
            (Old: "c80800", Size: 256u, Expected: "c80001"),
            (Old: "c80800", Size: 65535u, Expected: "c8ffff"),
            (Old: "e808000000", Size: 8u, Expected: "e808000000"),
            (Old: "e808000000", Size: 65536u, Expected: "e800000100"),
            (Old: "e808000000", Size: uint.MaxValue, Expected: "e8ffffffff"),
            (Old: "bf2008", Size: 255u, Expected: "bf20ff"),
            (Old: "ffc808000000", Size: 65536u, Expected: "ffc800000100"),
            (Old: "dfff0800", Size: 256u, Expected: "dfff0001"),
            (Old: "e008000000", Size: 8u, Expected: "e008000000"),
            (Old: "be08", Size: 8u, Expected: "be08"),
            (Old: "df2a0800", Size: 3u, Expected: "df2a0300")
        };
        foreach (var fixture in fixtures)
        {
            var original = Observe(fixture.Old);
            byte[] expected = Convert.FromHexString(fixture.Expected);
            Check(SmoDataBlockWriter.BuildReservedHeader(original, fixture.Size).AsSpan().SequenceEqual(expected),
                "Observed reservation matches independent original-compatible header bytes.");
            Check(SmoDataBlockWriter.BuildReservedHeader(original.FieldType, fixture.Size, original.SizeKind)
                .AsSpan().SequenceEqual(expected), "Explicit reservation uses the same shared writer.");
            byte[] destination = Destination(fixture.Old);
            byte[] before = destination.ToArray();
            byte[] completeExpected = before.ToArray();
            expected.CopyTo(completeExpected, 2);
            SmoDataBlockWriter.PatchReservedHeader(destination, 2, original, fixture.Size);
            Check(destination.AsSpan().SequenceEqual(completeExpected),
                "Relocated patch changes only the bounded header, with prefix/suffix and payload sentinel intact.");
            Check(original.Offset == 3 && before.AsSpan(2, original.HeaderSize)
                .SequenceEqual(Convert.FromHexString(fixture.Old)), "Mapped target offset is independent of the original reader offset.");
            rows.Add(new { fixture.Old, fixture.Size, fixture.Expected });
        }

        void RejectPatch(string old, SmoDataBlockHeader original, uint size, int offset = 2, byte[]? destination = null)
        {
            byte[] target = destination ?? Destination(old);
            byte[] before = target.ToArray();
            Reject(() => SmoDataBlockWriter.PatchReservedHeader(target, offset, original, size),
                "Invalid or stale reserved patch refuses.");
            Check(target.AsSpan().SequenceEqual(before), "Rejected patch leaves the complete destination unchanged.");
        }
        foreach (var item in new[] { (Old: "a808", Size: 256u), (Old: "c80800", Size: 65536u) })
        {
            var original = Observe(item.Old);
            Reject(() => SmoDataBlockWriter.BuildReservedHeader(original, item.Size), "Observed reservation refuses widening.");
            Reject(() => SmoDataBlockWriter.BuildReservedHeader(original.FieldType, item.Size, original.SizeKind),
                "Explicit reservation refuses widening.");
            RejectPatch(item.Old, original, item.Size);
        }
        var u8 = Observe("a808");
        RejectPatch("a808", u8, 0);
        RejectPatch("bf1f08", Observe("bf1f08"), 8);
        RejectPatch("bf0708", Observe("bf0708"), 8);
        RejectPatch("bf0708", Observe("bf0708"), 256); // fallback would also be three bytes
        RejectPatch("88", Observe("88", 8), 8);
        RejectPatch("a800", Observe("a800", 0), 8);
        RejectPatch("a808", u8 with { HeaderSize = 3 }, 8);
        RejectPatch("a808", u8 with { RawHeader = 0xA9 }, 8);
        RejectPatch("a808", u8 with { FieldType = 9 }, 8);
        RejectPatch("a808", u8 with { SizeCode = 6 }, 8);
        RejectPatch("a808", u8 with { PayloadSize = 256 }, 8);
        RejectPatch("a808", u8 with { Offset = -1 }, 8);
        RejectPatch("a808", u8, 8, -1);
        RejectPatch("a808", u8, 8, int.MaxValue);
        RejectPatch("a808", u8, 8, destination: [0xD1, 0xD2, 0xA8]);
        RejectPatch("a809", u8, 8);
        RejectPatch("df2b0800", Observe("df2a0800"), 8);
        foreach (int id in new[] { -1, 31, 256 })
            Reject(() => SmoDataBlockWriter.BuildReservedHeader(id, 8), "Unsupported field ID refuses.");
        foreach (var code in new[] { SmoDataBlockSizeCode.Empty, SmoDataBlockSizeCode.Fixed1,
                     SmoDataBlockSizeCode.Fixed8, (SmoDataBlockSizeCode)8 })
            Reject(() => SmoDataBlockWriter.BuildReservedHeader(8, 8, code), "Only variable reservations are supported.");
        Reject(() => SmoDataBlockWriter.BuildReservedHeader(8, 0), "New real empty reserved field refuses.");
        Check(SmoDataBlockWriter.BuildReservedHeader(8, 8).AsSpan().SequenceEqual(Convert.FromHexString("e808000000")),
            "New relationship headers explicitly default to UInt32 rather than compact fixed8.");

        // A destination may already be shortened before its old header is
        // patched. Requiring the old full field to fit would reject valid shrink.
        byte[] shortDestination = [0xD1, 0xD2, 0xA8, 8, 0x44, 0xEE];
        SmoDataBlockWriter.PatchReservedHeader(shortDestination, 2, u8, 1);
        Check(shortDestination.AsSpan().SequenceEqual(new byte[] { 0xD1, 0xD2, 0xA8, 1, 0x44, 0xEE }),
            "Header-only provenance permits a valid shrink without allocating or re-reading the old payload.");
        RejectPatch("a808", u8, 2, destination: shortDestination);

        Check(SmoDataBlockWriter.BuildHeader(8, 256, u8).AsSpan().SequenceEqual(Convert.FromHexString("c80001")),
            "Existing BuildHeader keeps its widening fallback contract.");
        Check(SmoDataBlockWriter.BuildHeader(8, 8).AsSpan().SequenceEqual(new byte[] { 0x88 }),
            "Existing BuildHeader keeps its automatic compact form.");
        Check(SmoDataBlockWriter.BuildHeader(7, 0).AsSpan().SequenceEqual(new byte[] { 0 }),
            "Existing BuildHeader keeps its terminator shorthand.");
        Check(SmoDataBlockWriter.BuildHeader(7, 256, Observe("bf0708")).AsSpan().SequenceEqual(Convert.FromHexString("c70001")),
            "Legacy widening fallback still discards an incompatible forced escape only in BuildHeader.");
        File.WriteAllText(Path.Combine(output, "report.json"), JsonSerializer.Serialize(new
        {
            status = "passed", checks, cases = rows,
            scope = "Reserved header host guards over the existing original-backed ABI; no new game execution or payload allocation for large lengths"
        }, new JsonSerializerOptions { WriteIndented = true }) + "\n");
        Console.WriteLine($"PASS reserved field-header authoring: {checks} checks");
        return 0;
    }

    private static SmoDataBlockHeader Observe(string headerHex, int payloadSize = 8)
    {
        byte[] bytes = [0x11, 0x22, 0x33, ..Convert.FromHexString(headerHex), ..new byte[payloadSize]];
        if (!SmoDataBlockReader.TryReadHeader(bytes, 3, out var header))
            throw new InvalidDataException("Directed header fixture was rejected by the shared reader.");
        return header;
    }

    private static byte[] Destination(string headerHex) =>
        [0xD1, 0xD2, ..Convert.FromHexString(headerHex), 0x44, 0x55, 0x66, 0xEE];
}
