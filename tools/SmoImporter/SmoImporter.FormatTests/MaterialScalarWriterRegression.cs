using System.Buffers.Binary;
using System.Security.Cryptography;
using System.Text.Json;
using SmoImporter.Core;
using SmoViewer.Core;

internal static class MaterialScalarWriterRegression
{
    private const uint StandardLayerClass = 0x234C576B;
    private static readonly uint[] States = [9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0xffffffff];
    private static readonly uint[] TextureStates = [0, 3, 3, 0, 0, 0xff000000, 2, 0, 0];
    private static readonly uint[] RigidStates = [0, 0, 1, 0, 1, 0, 3, 0, 2, 0, 6];

    public static int Run(string templatePath, string output)
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
        Directory.CreateDirectory(output);
        var cases = new List<object>();
        foreach (string mode in new[] { "single", "multipass", "orphan", "repeated", "legacy", "two-layers" })
        {
            Fixture fixture = CreateFixture(mode);
            byte[] input = Container(fixture.Bytes);
            byte[] originalInput = input.ToArray();
            var document = SmoDocument.ParseOwned(input, "material-scalars-" + mode);
            var entry = document.Objects.Single();
            Check(!document.HasErrors && entry.SignatureMatches, "Directed material fixture has a complete catalogued object.");
            byte[] all = SmoMaterialScalarWriter.PatchAllPasses(document, entry, States, 6);
            byte[] expected = Expected(fixture.Bytes, fixture.Fields, States, 6, null);
            Check(all.AsSpan().SequenceEqual(expected),
                "All-pass edit changes every MRS/pass assignment and preserves LTS, sentinels and original headers.");
            byte[] rigid = SmoLevelModelGraphReplacer.PatchRigidTextureAlphaMaterials(document, [entry.Id]);
            byte[] expectedRigid = input.ToArray();
            Expected(fixture.Bytes, fixture.Fields, RigidStates, 2, null)
                .CopyTo(expectedRigid, checked((int)entry.PhysicalOffset));
            Check(rigid.AsSpan().SequenceEqual(expectedRigid),
                "Importer rigid policy uses the shared all-pass edit without changing any other container bytes.");
            Check(input.AsSpan().SequenceEqual(originalInput), "Scalar authoring leaves the source document unchanged.");
            if (mode == "single")
            {
                byte[] single = SmoMaterialScalarWriter.PatchSingleStandardLayer(document, entry, States, 2, TextureStates);
                Check(single.AsSpan().SequenceEqual(Expected(fixture.Bytes, fixture.Fields, States, 2, TextureStates)),
                    "Single-layer edit changes exactly the three confirmed scalar payloads.");
                Check(single.AsSpan(0, 8).SequenceEqual(fixture.Bytes.AsSpan(0, 8)),
                    "The whole-object result retains the original class and SBOO header.");
                var patchedDocument = SmoDocument.ParseOwned(Container(single), "material-scalars-single-patched");
                Check(SmoMaterialRenderState.TryDecode(patchedDocument, patchedDocument.Objects.Single(), out var state) &&
                    state!.FinalBlendOperation == 2 && state.MaterialRenderStates.SequenceEqual(States),
                    "The actual material inspector reads the requested scalar state after editing.");
                Reject(() => SmoMaterialScalarWriter.PatchAllPasses(document, entry, States[..10], 2),
                    "Render-state inputs must contain eleven words.");
                Reject(() => SmoMaterialScalarWriter.PatchSingleStandardLayer(document, entry, States, 2, TextureStates[..8]),
                    "Texture-state inputs must contain nine words.");
                var otherDocument = SmoDocument.ParseOwned(input.ToArray(), "material-scalars-other-owner");
                Reject(() => SmoMaterialScalarWriter.PatchAllPasses(otherDocument, entry, States, 2),
                    "An entry from another document cannot select the patch extent.");
                File.WriteAllBytes(Path.Combine(output, "single-patched.smo"), Container(single));
            }
            else
            {
                Reject(() => SmoMaterialScalarWriter.PatchSingleStandardLayer(document, entry, States, 2, TextureStates),
                    $"Single-layer policy explicitly rejects {mode} instead of guessing a texture-state target.");
            }
            if (mode == "multipass")
                Check(fixture.Fields.Count(field => field.Type == 0) == 2 && fixture.Fields.Count(field => field.Type == 3) == 2,
                    "The all-pass fixture covers repeated MRS and two distinct actual pass assignments.");
            cases.Add(new { mode, bytes = input.Length, input_sha256 = Hash(input), all_object_sha256 = Hash(all), rigid_sha256 = Hash(rigid) });
            File.WriteAllBytes(Path.Combine(output, mode + "-input.smo"), input);
            File.WriteAllBytes(Path.Combine(output, mode + "-rigid.smo"), rigid);
        }
        foreach (string mode in new[] { "missing-states", "missing-pass" })
        {
            var document = SmoDocument.ParseOwned(Container(CreateFixture(mode).Bytes), "material-scalars-" + mode);
            Reject(() => SmoMaterialScalarWriter.PatchAllPasses(document, document.Objects.Single(), States, 2),
                $"All-pass editing rejects {mode} without inserting new fields.");
            Reject(() => SmoMaterialScalarWriter.PatchSingleStandardLayer(document, document.Objects.Single(), States, 2, TextureStates),
                $"Single-layer editing rejects {mode} without inserting new fields.");
        }

        byte[] realBytes = File.ReadAllBytes(templatePath);
        var real = SmoDocument.ParseOwned(realBytes, templatePath);
        var material = real.Objects.First(entry => entry.TypeHash == SmoClassIds.MaterialData);
        byte[] materialBytes = real.Data.Span.Slice(checked((int)material.PhysicalOffset), checked((int)material.SerializedSize)).ToArray();
        var realFields = ReadDirectFields(materialBytes);
        Check(realFields.Count(field => field.Type == 0) == 1 && realFields.Count(field => field.Type == 3) == 1 &&
            realFields.Count(field => field.Type == 17) == 1 && realFields.Count(field => field.Type == 4) == 1 &&
            realFields.All(field => field.Type != 8),
            "The selected shipped template is one explicit standard-layer scalar case.");
        byte[] realSingle = SmoMaterialScalarWriter.PatchSingleStandardLayer(real, material, States, 2, TextureStates);
        Check(realSingle.AsSpan().SequenceEqual(Expected(materialBytes, realFields, States, 2, TextureStates)),
            "Shipped material preserves every unrelated byte, including its inline texture and references.");
        byte[] realRigid = SmoLevelModelGraphReplacer.PatchRigidTextureAlphaMaterials(real, [material.Id]);
        byte[] realExpected = realBytes.ToArray();
        Expected(materialBytes, realFields, RigidStates, 2, null).CopyTo(realExpected, checked((int)material.PhysicalOffset));
        Check(realRigid.AsSpan().SequenceEqual(realExpected), "Shipped rigid import changes only the selected scalar assignments.");
        Check(File.ReadAllBytes(templatePath).AsSpan().SequenceEqual(realBytes), "The shipped template file remains unchanged.");
        File.WriteAllBytes(Path.Combine(output, "shipped-rigid.smo"), realRigid);
        File.WriteAllText(Path.Combine(output, "report.json"), JsonSerializer.Serialize(new
        {
            status = "passed", checks,
            scope = "Managed importer/writer integration; shared reader and scalar writer, not a new original-game execution",
            template = Path.GetFullPath(templatePath), template_sha256 = Hash(realBytes), material_id = material.Id,
            material_sha256 = Hash(materialBytes), single_object_sha256 = Hash(realSingle), rigid_sha256 = Hash(realRigid), cases
        }, new JsonSerializerOptions { WriteIndented = true }) + "\n");
        Console.WriteLine($"PASS shared material scalar authoring: {checks} checks");
        return 0;
    }

    // Explicit directed wire fixtures/expectations only. Production assignment
    // scope and serialization belong to the shared actual material classes.
    private sealed record Field(int Type, int PayloadOffset, int Length);
    private sealed record Fixture(byte[] Bytes, IReadOnlyList<Field> Fields);
    private static Fixture CreateFixture(string mode)
    {
        var bytes = new List<byte>([..BitConverter.GetBytes(SmoClassIds.MaterialData), .."SBOO"u8]);
        var fields = new List<Field>();
        void Add(int type, byte[] payload)
        {
            // Deliberately retain a wider UInt16 reservation even for 4/36/44B.
            bytes.Add(checked((byte)(0xc0 | type)));
            bytes.AddRange(BitConverter.GetBytes(checked((ushort)payload.Length)));
            fields.Add(new(type, bytes.Count, payload.Length)); bytes.AddRange(payload);
        }
        uint[] initialStates = Enumerable.Range(0, 11).Select(index => (uint)(index + 20)).ToArray();
        uint[] initialTexture = Enumerable.Range(0, 9).Select(index => (uint)(index + 40)).ToArray();
        Add(1, [0x7e]);
        Add(2, Words([0x11223344, 0x55667788, 0x99aabbcc, 0xddeeff00, 0x7fc01234]));
        Add(18, Words([7, 0]));
        if (mode == "orphan") Add(17, [0x7f]);
        if (mode != "missing-states") Add(0, Words(initialStates));
        if (mode != "missing-pass")
        {
            Add(3, Words([4])); Add(4, Words([StandardLayerClass]));
            Add(17, Words(initialTexture));
            Add(10, Words([0])); // A real null Texture reference, left untouched.
            if (mode == "repeated") Add(17, Words(initialTexture.Reverse().ToArray()));
            if (mode == "legacy") Add(8, Words(initialTexture.Reverse().ToArray()));
            if (mode == "two-layers") Add(4, Words([StandardLayerClass]));
            if (mode == "multipass")
            {
                Add(0, Words(initialStates.Reverse().ToArray()));
                Add(3, Words([5])); Add(4, Words([StandardLayerClass]));
                Add(17, Words(initialTexture.Reverse().ToArray()));
            }
        }
        Add(30, [0x5a, 0xa5, 0x33, 0x66, 0, 0xff, 0x42]);
        bytes.Add(0);
        return new(bytes.ToArray(), fields);
    }
    private static byte[] Expected(byte[] input, IReadOnlyList<Field> fields,
        uint[] states, uint blend, uint[]? texture)
    {
        byte[] result = input.ToArray();
        foreach (var field in fields)
        {
            byte[]? payload = field.Type switch { 0 => Words(states), 3 => Words([blend]), 17 when texture is not null => Words(texture), _ => null };
            if (payload is null) continue;
            if (payload.Length != field.Length) throw new InvalidDataException("Directed expected scalar extent differs from its fixture.");
            payload.CopyTo(result, field.PayloadOffset);
        }
        return result;
    }
    private static IReadOnlyList<Field> ReadDirectFields(byte[] bytes)
    {
        var fields = new List<Field>();
        int offset = 8;
        while (offset < bytes.Length && SmoDataBlockReader.TryReadHeader(bytes, offset, out var field))
        {
            if (field.PayloadSize != 0) fields.Add(new(field.FieldType, field.PayloadOffset, checked((int)field.PayloadSize)));
            offset = checked((int)field.PayloadEnd);
        }
        if (offset != bytes.Length) throw new InvalidDataException("Shipped fixture field stream is incomplete.");
        return fields;
    }
    private static byte[] Words(uint[] values)
    {
        byte[] bytes = new byte[values.Length * 4];
        for (int index = 0; index < values.Length; ++index)
            BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(index * 4), values[index]);
        return bytes;
    }
    private static string Hash(byte[] bytes) => Convert.ToHexString(SHA256.HashData(bytes));
    private static byte[] Container(byte[] body)
    {
        byte[] name = "material-scalars\0"u8.ToArray();
        int dataStart = SmoHeader.Size + 18 + name.Length + 4;
        byte[] bytes = new byte[dataStart + body.Length];
        "FFPS"u8.CopyTo(bytes);
        void Word(int offset, uint value) => BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(offset), value);
        Word(4, 0x26); Word(12, checked((uint)bytes.Length)); Word(16, 2);
        Word(20, checked((uint)dataStart)); Word(24, checked((uint)body.Length)); Word(28, 1);
        int cursor = SmoHeader.Size; Word(cursor, 1); cursor += 4;
        BinaryPrimitives.WriteUInt16LittleEndian(bytes.AsSpan(cursor), checked((ushort)name.Length)); cursor += 2;
        name.CopyTo(bytes, cursor); cursor += name.Length;
        Word(cursor, SmoClassIds.MaterialData); Word(cursor + 4, 0); Word(cursor + 8, checked((uint)body.Length));
        body.CopyTo(bytes, dataStart); return bytes;
    }
}
