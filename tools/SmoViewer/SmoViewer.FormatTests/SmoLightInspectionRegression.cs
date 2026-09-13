using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using System.Text.Json.Nodes;
using SmoViewer.Core;

namespace SmoViewer.FormatTests;

internal static class SmoLightInspectionRegression
{
    private static byte[] State(SmoLightData light)
    {
        using var stream = new MemoryStream();
        using var writer = new BinaryWriter(stream);
        writer.Write(light.Type);
        writer.Write(light.ProjectShadowVolume ? 1u : 0u);
        writer.Write(light.AttenuationEnabled ? 1u : 0u);
        writer.Write(light.Enabled ? 1u : 0u);
        writer.Write(light.ColorRgba.X); writer.Write(light.ColorRgba.Y);
        writer.Write(light.ColorRgba.Z); writer.Write(light.ColorRgba.W);
        writer.Write(light.Intensity); writer.Write(light.Range);
        writer.Write(light.HotspotAngle); writer.Write(light.FalloffAngle);
        return stream.ToArray();
    }

    private static IReadOnlyList<SmoObjectField> Fields(byte[] ownSection)
    {
        // Exercise the database-style field-list transport with no usable
        // source offsets. Header interpretation still calls the shared reader.
        byte[] bytes = new byte[ownSection.Length + 1];
        ownSection.CopyTo(bytes, 1);
        var fields = new List<SmoObjectField>();
        int offset = 0;
        while (offset < bytes.Length)
        {
            if (!SmoDataBlockReader.TryReadHeader(bytes, offset, out var header) ||
                header.PayloadEnd > bytes.Length)
                throw new InvalidDataException("Invalid test field observation.");
            fields.Add(new SmoObjectField(0, 1, header.FieldType, 0,
                header.RawHeader, header.SizeKind, header.HeaderSize, header.PayloadSize,
                0, 0, 0, 0, bytes.AsMemory(header.PayloadOffset, checked((int)header.PayloadSize))));
            offset = checked((int)header.PayloadEnd);
        }
        return fields;
    }

    // abi-1.json rows: { case, own_section_hex, state_hex }. state_hex is the
    // 48-byte inspection projection compared against original PC captures.
    internal static int Run(string folder, string output)
    {
        int checks = 0;
        void Check(bool value, string label)
        {
            if (!value) throw new InvalidDataException(label);
            ++checks;
        }
        var rows = JsonNode.Parse(File.ReadAllText(Path.Combine(folder, "abi-1.json")))!["rows"]!.AsArray();
        foreach (var row in rows)
        {
            byte[] section = Convert.FromHexString(row!["own_section_hex"]!.GetValue<string>());
            string expected = row["state_hex"]!.GetValue<string>();
            Check(SmoLightDataDecoder.TryDecodeOwnSection(section, out var direct, out var error), error);
            Check(Convert.ToHexString(State(direct!)).Equals(expected, StringComparison.OrdinalIgnoreCase),
                "direct managed projection matches original-PC/ABI state bits");
            Check(SmoLightDataDecoder.TryDecode(Fields(section), out var transported, out error), error);
            Check(Convert.ToHexString(State(transported!)).Equals(expected, StringComparison.OrdinalIgnoreCase),
                "database field-list transport preserves original-PC/ABI state bits");
            Check(direct!.SerializedFieldMask == transported!.SerializedFieldMask,
                "physical presence agrees across the two transport paths");
        }
        Check(SmoLightDataDecoder.TryDecodeOwnSection([0], out var defaults, out _), "empty own section accepted");
        Check(defaults!.Enabled && defaults.ColorRgba == Vector4.One && defaults.Range == 200 &&
            defaults.Intensity == 1 && defaults.SerializedFieldMask == 0,
            "actual constructor defaults include enabled true");
        Check(SmoLightDataDecoder.TryDecodeOwnSection([0x28, 0], out _, out _) == false,
            "known flag without final terminator rejected by host boundary");
        Check(SmoLightDataDecoder.TryDecodeOwnSection([0x28, 3, 0], out var flag, out _) && flag!.Enabled,
            "raw nonzero flag reaches the shared host-bool canonicalization");
        Check(SmoLightDataDecoder.TryDecodeOwnSection([0x28, 0, 0], out var disabled, out _) && !disabled!.Enabled,
            "explicit serialized false overrides constructor true");
        foreach (string hex in new[] { "", "6001000000", "6201", "0000", "A00301020300" })
            Check(!SmoLightDataDecoder.TryDecodeOwnSection(Convert.FromHexString(hex), out _, out _),
                "truncated, missing-terminator, wrong-extent or trailing input rejected");

        byte[] id31 = Convert.FromHexString("7F1F0102030400");
        Check(SmoLightDataDecoder.TryDecodeOwnSection(id31, out var unknown, out _) &&
            unknown!.Enabled && unknown.SerializedFieldMask == 0,
            "direct reader accepts and skips original reader-valid unknown ID31");
        Check(!SmoLightDataDecoder.TryDecode(Fields(id31), out _, out var writerError) &&
            writerError.Contains("FIELD_HEADER_ID31_UNSUPPORTED", StringComparison.Ordinal),
            "field-list writer limitation is explicit instead of silently changing input");

        string root = Path.GetFullPath(Path.Combine(folder, "../../../.."));
        var document = SmoDocument.Load(Path.Combine(root, "local-data/pc-pristine/Media/Characters/Bloom/bloom_projectile.smo"));
        var entry = document.Objects.Single(item => item.TypeHash == SmoClassIds.LightData);
        Check(SmoObjectFieldReader.TryRead(document, entry, out var fields, out var fieldError), fieldError);
        Check(SmoLightDataDecoder.TryDecode(fields, out var actual, out var actualError), actualError);
        Check(SmoLightDataDecoder.TryDecode(document,entry,out var documentLight,out var documentError),documentError);
        Check(State(actual!).SequenceEqual(State(documentLight!)),"direct document bytes preserve the same original light state");
        Check(actual!.Type == (uint)SmoLightType.Ambient && actual.Enabled,
            "whole-original-proven Bloom projectile ambient light inspected");
        var inspection = SmoSerializedFieldInspector.Inspect(document, entry);
        Check(inspection.Any(item => item.Descriptor.Key == "light.type" && item.IsDecoded &&
            item.DisplayValue.Contains("ambient", StringComparison.Ordinal)),
            "existing inspector uses actual light state");
        var report = new
        {
            status = "passed", checks, originalCases = rows.Count, realFiles = 1,
            native_dll_sha256 = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(Path.Combine(AppContext.BaseDirectory, "SparkplugViewerNative.dll"))))
        };
        File.WriteAllText(output, JsonSerializer.Serialize(report, new JsonSerializerOptions { WriteIndented = true }) + "\n");
        Console.WriteLine($"Light inspection: {checks} checks");
        return 0;
    }
}
