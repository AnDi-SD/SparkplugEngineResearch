using System.Buffers.Binary;
using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using SmoViewer.Core;

internal static class StaticMatrixWriterRegression
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
        Matrix4x4 world = new(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        Matrix4x4 independentInverse = Matrix4x4.CreateTranslation(-17, 18, -19);
        Matrix4x4 rawFinite = Matrix4x4.Identity;
        rawFinite.M12 = BitConverter.UInt32BitsToSingle(0x80000000);
        rawFinite.M23 = BitConverter.UInt32BitsToSingle(1);
        rawFinite.M34 = float.MaxValue;
        rawFinite.M41 = -float.MaxValue;
        var cases = new List<object>();
        foreach (var requested in new[]
        {
            (Name: "defaults", World: Matrix4x4.Identity, Inverse: Matrix4x4.Identity),
            (Name: "independent-non-affine", World: world, Inverse: independentInverse),
            (Name: "finite-bit-patterns", World: rawFinite, Inverse: world)
        })
        {
            SmoStaticMatrixPayloads encoded = SmoStaticMatrixWriter.Encode(requested.World, requested.Inverse);
            Check(encoded.World.Span.SequenceEqual(MatrixBytes(requested.World)),
                "The actual writer preserves all sixteen world float words without affine conversion.");
            Check(encoded.Inverse.Span.SequenceEqual(MatrixBytes(requested.Inverse)),
                "The inverse is its supplied independent value; no inversion or normalization is applied.");
            byte[] matrices = [..encoded.World.Span, ..encoded.Inverse.Span];
            File.WriteAllBytes(Path.Combine(output, requested.Name + ".bin"), matrices);
            cases.Add(new { name = requested.Name, matrices_sha256 = Hash(matrices) });
        }

        foreach (bool inverseFirst in new[] { false, true })
        {
            Fixture fixture = CreateFixture(inverseFirst);
            byte[] patched = fixture.Bytes.ToArray();
            SmoStaticMatrixWriter.PatchPayloads(patched, fixture.WorldOffset, fixture.InverseOffset, world, independentInverse);
            byte[] expected = Expected(fixture, world, independentInverse);
            Check(patched.AsSpan().SequenceEqual(expected),
                "In-place matrix authoring preserves object/header/unknown bytes and honors both selected offsets.");
            Check(fixture.Bytes.AsSpan().SequenceEqual(CreateFixture(inverseFirst).Bytes),
                "Editing a destination copy does not modify the template.");

            byte[] source = Container(fixture.Bytes);
            byte[] originalSource = source.ToArray();
            var document = SmoDocument.ParseOwned(source, "static-matrix-authoring");
            Check(!document.HasErrors && document.Objects.Single().SignatureMatches,
                "Directed placement fixture is a complete catalogued StaticRenderObject with an empty renderable list.");
            Matrix4x4 desired = Matrix4x4.CreateTranslation(7, -8, 9);
            Matrix4x4 expectedInverse = Matrix4x4.CreateTranslation(-7, 8, -9);
            SmoPlacementTransformPatchResult result = SmoPlacementTransformWriter.Patch(document,
                [new SmoPlacementTransformEdit(0, Matrix4x4.Identity, desired)]);
            Check(result.EditedSceneObjectCount == 1 && result.StaticObjectIndices.SequenceEqual(new[] { 0 }),
                "The real placement consumer reports the selected StaticRenderObject.");
            Check(result.Data.AsSpan().SequenceEqual(Container(Expected(fixture, desired, expectedInverse))),
                "Placement authoring uses the shared writer while preserving UInt16/UInt32 headers, FAT and unknown fields.");
            Check(source.AsSpan().SequenceEqual(originalSource), "Successful placement authoring leaves source bytes immutable.");
            var decoded = SmoDocument.ParseOwned(result.Data, "static-matrix-authoring-result");
            Check(SmoStaticRenderObjectDecoder.TryDecode(decoded, decoded.Objects.Single(), out var value, out _) &&
                value.Transform == desired && value.EngineInverseTransform == expectedInverse,
                "The actual reader observes both separately authored placement matrices.");
            Matrix4x4 invalid = desired; invalid.M22 = float.NaN;
            Reject(() => SmoPlacementTransformWriter.Patch(document,
                [new SmoPlacementTransformEdit(0, Matrix4x4.Identity, desired),
                 new SmoPlacementTransformEdit(0, Matrix4x4.Identity, invalid)]),
                "A valid edit followed by an invalid edit rejects the entire placement request.");
            Check(source.AsSpan().SequenceEqual(originalSource), "A rejected placement batch does not partially change its input.");
            File.WriteAllBytes(Path.Combine(output, inverseFirst ? "inverse-first.smo" : "world-first.smo"), result.Data);
        }

        Fixture valid = CreateFixture(false);
        foreach (float invalid in new[] { float.NaN, float.PositiveInfinity, float.NegativeInfinity })
        {
            foreach (bool invalidInverse in new[] { false, true })
            {
                Matrix4x4 bad = Matrix4x4.Identity; bad.M44 = invalid;
                byte[] target = valid.Bytes.ToArray();
                Reject(() => SmoStaticMatrixWriter.PatchPayloads(target, valid.WorldOffset, valid.InverseOffset,
                    invalidInverse ? world : bad, invalidInverse ? bad : independentInverse),
                    "The editor finite-input policy rejects a non-finite word in either independent matrix.");
                Check(target.AsSpan().SequenceEqual(valid.Bytes),
                    "Native validation finishes before copying either payload, including a bad inverse after a valid world.");
            }
        }
        foreach (var offsets in new[]
        {
            (World: -1, Inverse: valid.InverseOffset),
            (World: int.MaxValue, Inverse: valid.InverseOffset),
            (World: valid.WorldOffset, Inverse: -1),
            (World: valid.WorldOffset, Inverse: int.MaxValue),
            (World: valid.WorldOffset, Inverse: valid.Bytes.Length - 63),
            (World: valid.WorldOffset, Inverse: valid.WorldOffset),
            (World: valid.WorldOffset, Inverse: valid.WorldOffset + 63)
        })
        {
            byte[] target = valid.Bytes.ToArray();
            Reject(() => SmoStaticMatrixWriter.PatchPayloads(target, offsets.World, offsets.Inverse, world, independentInverse),
                "Both complete, non-overlapping destination ranges are required before patching.");
            Check(target.AsSpan().SequenceEqual(valid.Bytes), "Invalid destination ranges never partially mutate the target.");
        }
        File.WriteAllText(Path.Combine(output, "report.json"), JsonSerializer.Serialize(new
        {
            status = "passed", checks,
            scope = "Actual StaticRenderObject writer through managed editing; independent matrices and atomic host guards; no new original execution",
            cases
        }, new JsonSerializerOptions { WriteIndented = true }) + "\n");
        Console.WriteLine($"PASS shared StaticRenderObject matrix authoring: {checks} checks");
        return 0;
    }

    // Directed fixture construction and expected bytes are test-only. Production
    // uses the existing original-backed StaticRenderObject and field writers.
    private sealed record Fixture(byte[] Bytes, int WorldOffset, int InverseOffset);
    private static Fixture CreateFixture(bool inverseFirst)
    {
        var bytes = new List<byte>([..BitConverter.GetBytes(SmoClassIds.StaticRenderObject), .."SBOO"u8]);
        int world = 0, inverse = 0;
        void World()
        { bytes.AddRange(new byte[] { 0xc1, 64, 0 }); world = bytes.Count; bytes.AddRange(MatrixBytes(Matrix4x4.Identity)); }
        void Inverse()
        { bytes.AddRange(new byte[] { 0xe2, 64, 0, 0, 0 }); inverse = bytes.Count; bytes.AddRange(MatrixBytes(Matrix4x4.Identity)); }
        void Unknown() => bytes.AddRange(new byte[] { 0xa9, 8, 1, 0, 0, 0, 0x53, 0x42, 0x4f, 0x4f });
        if (inverseFirst) { Inverse(); Unknown(); World(); }
        else { World(); Unknown(); Inverse(); }
        bytes.Add(0);
        return new(bytes.ToArray(), world, inverse);
    }
    private static byte[] Expected(Fixture fixture, Matrix4x4 world, Matrix4x4 inverse)
    {
        byte[] bytes = fixture.Bytes.ToArray();
        MatrixBytes(world).CopyTo(bytes, fixture.WorldOffset);
        MatrixBytes(inverse).CopyTo(bytes, fixture.InverseOffset);
        return bytes;
    }
    private static byte[] MatrixBytes(Matrix4x4 matrix)
    {
        float[] values = [matrix.M11, matrix.M12, matrix.M13, matrix.M14,
            matrix.M21, matrix.M22, matrix.M23, matrix.M24,
            matrix.M31, matrix.M32, matrix.M33, matrix.M34,
            matrix.M41, matrix.M42, matrix.M43, matrix.M44];
        byte[] bytes = new byte[64];
        for (int index = 0; index < values.Length; ++index)
            BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(index * 4), BitConverter.SingleToUInt32Bits(values[index]));
        return bytes;
    }
    private static string Hash(byte[] bytes) => Convert.ToHexString(SHA256.HashData(bytes));
    private static byte[] Container(byte[] body)
    {
        byte[] name = "static-matrix-authoring\0"u8.ToArray();
        int dataStart = SmoHeader.Size + 18 + name.Length + 4;
        byte[] bytes = new byte[dataStart + body.Length];
        "FFPS"u8.CopyTo(bytes);
        void Word(int offset, uint value) => BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(offset), value);
        Word(4, 0x26); Word(12, checked((uint)bytes.Length)); Word(16, 2);
        Word(20, checked((uint)dataStart)); Word(24, checked((uint)body.Length)); Word(28, 1);
        int cursor = SmoHeader.Size; Word(cursor, 1); cursor += 4;
        BinaryPrimitives.WriteUInt16LittleEndian(bytes.AsSpan(cursor), checked((ushort)name.Length)); cursor += 2;
        name.CopyTo(bytes, cursor); cursor += name.Length;
        Word(cursor, SmoClassIds.StaticRenderObject); Word(cursor + 4, 0); Word(cursor + 8, checked((uint)body.Length));
        body.CopyTo(bytes, dataStart); return bytes;
    }
}
