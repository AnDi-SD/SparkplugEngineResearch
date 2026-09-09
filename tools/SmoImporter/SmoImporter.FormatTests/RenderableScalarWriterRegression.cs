using System.Buffers.Binary;
using System.Security.Cryptography;
using System.Text.Json;
using SmoViewer.Core;

internal static class RenderableScalarWriterRegression
{
    public static int Run(string output)
    {
        int checks = 0;
        void Check(bool condition, string message)
        { if (!condition) throw new InvalidDataException(message); ++checks; }
        void Reject(Action action, string message)
        {
            bool rejected = false;
            try { action(); }
            catch (Exception error) when (error is ArgumentException or InvalidDataException or NotSupportedException)
            { rejected = true; }
            Check(rejected, message);
        }
        if (Directory.Exists(output)) throw new InvalidOperationException("Use a new output directory to preserve prior evidence.");
        Directory.CreateDirectory(output);
        var cases = new List<object>();
        foreach (string mode in new[] { "ordered", "reordered", "foreign-repeat" })
        {
            Fixture fixture = CreateFixture(mode);
            byte[] input = Container(fixture.Bytes);
            byte[] originalInput = input.ToArray();
            var document = SmoDocument.ParseOwned(input, "renderable-scalars-" + mode);
            var entry = document.Objects.Single();
            Check(!document.HasErrors && entry.SignatureMatches, "Directed Skin has a complete catalogued three-section object.");
            foreach (var requested in new[] { (Alpha: false, Priority: 0u), (Alpha: true, Priority: uint.MaxValue) })
            {
                byte[] patched = SmoRenderableScalarWriter.PatchSkinSort(document, entry, requested.Alpha, requested.Priority);
                byte[] expected = Expected(fixture, requested.Alpha, requested.Priority);
                Check(patched.AsSpan().SequenceEqual(expected),
                    "Only the actual Renderable alpha/priority payloads change across the three inherited sections.");
                Check(patched.AsSpan(0, 8).SequenceEqual(fixture.Bytes.AsSpan(0, 8)) &&
                    fixture.Fields.All(field => patched.AsSpan(field.HeaderOffset, 3)
                        .SequenceEqual(fixture.Bytes.AsSpan(field.HeaderOffset, 3))),
                    "The whole-object result retains SBOO and every original UInt16-reserved field header.");
                Check(fixture.Fields.Where(field => field.Section != 0).All(field =>
                    patched.AsSpan(field.PayloadOffset, field.Length)
                        .SequenceEqual(fixture.Bytes.AsSpan(field.PayloadOffset, field.Length))),
                    "Model projection/unknown fields and Skin palette/unknown fields remain byte-identical.");
                var repeated = SmoDocument.ParseOwned(Container(patched), "renderable-scalars-idempotent");
                Check(SmoRenderableScalarWriter.PatchSkinSort(repeated, repeated.Objects.Single(), requested.Alpha, requested.Priority)
                    .AsSpan().SequenceEqual(patched), "Applying the same explicit sort policy twice is byte-idempotent.");
                string name = mode + (requested.Alpha ? "-on" : "-off");
                File.WriteAllBytes(Path.Combine(output, name + ".smo"), Container(patched));
                cases.Add(new { name, alpha = requested.Alpha, priority = requested.Priority,
                    input_sha256 = Hash(input), object_sha256 = Hash(patched) });
            }
            Check(input.AsSpan().SequenceEqual(originalInput), "Successful scalar edits do not mutate the source document.");
            File.WriteAllBytes(Path.Combine(output, mode + "-input.smo"), input);
        }
        foreach (string mode in new[] { "missing-alpha", "missing-priority", "repeated-alpha", "repeated-priority", "truncated", "short-alpha" })
        {
            byte[] input = Container(CreateFixture(mode).Bytes);
            byte[] originalInput = input.ToArray();
            var document = SmoDocument.ParseOwned(input, "renderable-scalars-" + mode);
            Reject(() => SmoRenderableScalarWriter.PatchSkinSort(document, document.Objects.Single(), false, 1),
                $"Skin sort editing rejects {mode}; fields 2/3 in later sections cannot substitute for Renderable assignments.");
            Check(input.AsSpan().SequenceEqual(originalInput), "A rejected scalar edit leaves all source bytes untouched.");
            File.WriteAllBytes(Path.Combine(output, mode + "-input.smo"), input);
        }
        byte[] validInput = Container(CreateFixture("ordered").Bytes);
        var first = SmoDocument.ParseOwned(validInput, "renderable-scalars-first-owner");
        var second = SmoDocument.ParseOwned(validInput.ToArray(), "renderable-scalars-second-owner");
        Reject(() => SmoRenderableScalarWriter.PatchSkinSort(second, first.Objects.Single(), false, 1),
            "An entry from another document cannot select a Skin patch extent.");
        byte[] otherBody = [..BitConverter.GetBytes(SmoClassIds.Model), .."SBOO"u8, 0, 0];
        var other = SmoDocument.ParseOwned(Container(otherBody, SmoClassIds.Model), "renderable-scalars-other-class");
        Reject(() => SmoRenderableScalarWriter.PatchSkinSort(other, other.Objects.Single(), false, 1),
            "The Skin-specific API rejects a different class instead of choosing a section layout.");
        File.WriteAllText(Path.Combine(output, "report.json"), JsonSerializer.Serialize(new
        {
            status = "passed", checks,
            scope = "Managed Skin sort-authoring integration across Renderable/Model/Skin sections; no new original-game execution",
            cases
        }, new JsonSerializerOptions { WriteIndented = true }) + "\n");
        Console.WriteLine($"PASS shared Renderable scalar authoring: {checks} checks");
        return 0;
    }

    // Explicit test construction and independent byte expectations only.
    // Production field scope is observed by the actual inherited readers.
    private sealed record Field(int Section, int Type, int HeaderOffset, int PayloadOffset, int Length);
    private sealed record Fixture(byte[] Bytes, IReadOnlyList<Field> Fields);
    private static Fixture CreateFixture(string mode)
    {
        var bytes = new List<byte>([..BitConverter.GetBytes(SmoClassIds.Skin), .."SBOO"u8]);
        var fields = new List<Field>();
        int section = 0;
        void Add(int type, byte[] payload)
        {
            int headerOffset = bytes.Count;
            bytes.Add(checked((byte)(0xc0 | type)));
            bytes.AddRange(BitConverter.GetBytes(checked((ushort)payload.Length)));
            fields.Add(new(section, type, headerOffset, bytes.Count, payload.Length));
            bytes.AddRange(payload);
        }
        void End() { bytes.Add(0); ++section; }
        void Alpha()
        {
            if (mode != "missing-alpha") Add(2, mode == "short-alpha" ? [1] : Words([0x80000000]));
            if (mode == "repeated-alpha") Add(2, Words([0]));
        }
        void Priority()
        {
            if (mode != "missing-priority") Add(3, Words([0xdeadbeef]));
            if (mode == "repeated-priority") Add(3, Words([0]));
        }
        Add(0, Words([0])); Add(1, Words([0])); // Explicit null Material/Fog references.
        if (mode == "reordered") { Priority(); Add(30, [0x5a, 0xa5, 0x42]); Alpha(); }
        else { Alpha(); Add(30, [0x5a, 0xa5, 0x42]); Priority(); }
        End();
        Add(1, Words([0xf00dbaad])); // Actual Model projection word.
        Add(2, Words([0x12345678])); Add(3, Words([0x87654321])); // Unknown in Model.
        if (mode == "foreign-repeat") Add(2, Words([0x13572468]));
        End();
        Add(0, Words([0xffffffff, 0])); // Raw weight word, empty palette; no substitute bones.
        Add(2, Words([0x11223344])); Add(3, Words([0x55667788])); // Unknown in Skin.
        if (mode == "foreign-repeat") Add(3, Words([0x24681357]));
        if (mode != "truncated") End();
        return new(bytes.ToArray(), fields);
    }
    private static byte[] Expected(Fixture fixture, bool alpha, uint priority)
    {
        byte[] bytes = fixture.Bytes.ToArray();
        foreach (var field in fixture.Fields.Where(field => field.Section == 0 && field.Type is 2 or 3))
            BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(field.PayloadOffset), field.Type == 2 ? (alpha ? 1u : 0u) : priority);
        return bytes;
    }
    private static byte[] Words(uint[] values)
    {
        byte[] bytes = new byte[values.Length * 4];
        for (int index = 0; index < values.Length; ++index)
            BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(index * 4), values[index]);
        return bytes;
    }
    private static string Hash(byte[] bytes) => Convert.ToHexString(SHA256.HashData(bytes));
    private static byte[] Container(byte[] body, uint type = SmoClassIds.Skin)
    {
        byte[] name = "renderable-scalars\0"u8.ToArray();
        int dataStart = SmoHeader.Size + 18 + name.Length + 4;
        byte[] bytes = new byte[dataStart + body.Length];
        "FFPS"u8.CopyTo(bytes);
        void Word(int offset, uint value) => BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(offset), value);
        Word(4, 0x26); Word(12, checked((uint)bytes.Length)); Word(16, 2);
        Word(20, checked((uint)dataStart)); Word(24, checked((uint)body.Length)); Word(28, 1);
        int cursor = SmoHeader.Size; Word(cursor, 1); cursor += 4;
        BinaryPrimitives.WriteUInt16LittleEndian(bytes.AsSpan(cursor), checked((ushort)name.Length)); cursor += 2;
        name.CopyTo(bytes, cursor); cursor += name.Length;
        Word(cursor, type); Word(cursor + 4, 0); Word(cursor + 8, checked((uint)body.Length));
        body.CopyTo(bytes, dataStart); return bytes;
    }
}
