using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using SmoViewer.Core;

namespace SmoViewer.FormatTests;

internal static class SmoOcclusionBufferInspectionRegression
{
    internal static int Run(string source, string output)
    {
        int checks = 0;
        void Check(bool value, string message) { ++checks; if (!value) throw new InvalidDataException(message); }
        string Hash(ReadOnlySpan<byte> bytes) => Convert.ToHexString(SHA256.HashData(bytes));
        var document = SmoDocument.Load(source);
        string sourceHash = Hash(document.Data.Span);
        // One original-backed representative, not a full corpus sweep. The
        // original CPU readers returned success and cursors 24/60 for these
        // exact payloads in race02-shape-run1.json; the later shape call stopped.
        Check(sourceHash == "AD39C13B896718755CC98445D943FA191640636F940930086CB802629E268436",
            "selected pristine race_02 fixture identity");
        var entry = document.Objects[7];
        Check(entry.Id == 8 && entry.TypeHash == SmoClassIds.OcclusionVolume &&
            entry.SerializedSize == 138 && entry.Name.TrimEnd('\0') == "oclusion_wall02",
            "selected original occluder identity");
        var fields = SmoObjectFieldReader.Read(document, entry);
        var indexField = fields.Single(field => field.FieldType == 0 && field.PayloadSize == 24);
        var vertexField = fields.Single(field => field.FieldType == 1 && field.PayloadSize == 60);
        Check(indexField.AbsolutePayloadOffset == 190026 &&
            Hash(indexField.Payload.Span) == "7C6497660ACCB32E25668A6915529A86B7285E53DC4A6C977F1B3502BD3BEDF3",
            "exact original IndexBuffer input");
        Check(vertexField.AbsolutePayloadOffset == 190055 &&
            Hash(vertexField.Payload.Span) == "D594F9C576A576B18E62C6834E8676C750E502B51D09DA56D248FECFDCF726F9",
            "exact original VertexBuffer input");
        Check(SmoOcclusionVolumeDecoder.TryDecode(document, entry, out var value, out var error), error);
        Check(value!.IndexBuffer.PrimitiveType == 2 && value.IndexBuffer.PrimitiveCount == 2 &&
            value.IndexBuffer.IndexFormat == 0 &&
            value.IndexBuffer.TriangleIndices.SequenceEqual(new ushort[] { 2, 1, 0, 1, 2, 3 }),
            "original UInt16 triangle buffer values");
        Vector3[] expected = [
            new(-4958.6494140625f, -82.49378204345703f, 724.2744750976562f),
            new(-4958.6494140625f, -82.49374389648438f, -2728.739990234375f),
            new(4958.6494140625f, 650.8276977539062f, 724.2744750976562f),
            new(4958.6494140625f, 650.8276977539062f, -2728.739990234375f)
        ];
        Check(value.VertexBuffer.VertexDeclaration == 0 && value.VertexBuffer.VertexCount == 4 &&
            value.VertexBuffer.Flags == 0 && value.VertexBuffer.Positions.SequenceEqual(expected),
            "original position buffer metadata and values");
        var inspected = SmoSerializedFieldInspector.Inspect(document, entry);
        Check(inspected.Any(field => field.Descriptor.Key == "occlusion_volume.index_buffer" && field.IsDecoded &&
            field.DisplayValue.Contains("triangles=2", StringComparison.Ordinal)), "serialized field inspector consumes shared index reader");
        Check(inspected.Any(field => field.Descriptor.Key == "occlusion_volume.vertex_buffer" && field.IsDecoded &&
            field.DisplayValue.Contains("vertices=4", StringComparison.Ordinal)), "serialized field inspector consumes shared vertex reader");
        Check(Hash(document.Data.Span) == sourceHash && Hash(File.ReadAllBytes(source)) == sourceHash,
            "inspection preserves both document and source bytes");
        File.WriteAllText(output, JsonSerializer.Serialize(new {
            status = "passed", checks, metadataOnly = true, fullOcclusionInitCompleted = false,
            source, sourceSha256 = sourceHash, entry.Index, entry.Id,
            indexPayloadOffset = indexField.AbsolutePayloadOffset, indexPayloadSize = indexField.PayloadSize,
            vertexPayloadOffset = vertexField.AbsolutePayloadOffset, vertexPayloadSize = vertexField.PayloadSize,
            value.IndexBuffer.PrimitiveCount, value.VertexBuffer.VertexCount,
            nativeSha256 = Hash(File.ReadAllBytes(Path.Combine(AppContext.BaseDirectory, "SparkplugViewerNative.dll")))
        }, new JsonSerializerOptions { WriteIndented = true }) + "\n");
        Console.WriteLine($"PASS original occlusion buffer metadata: {checks} checks");
        return 0;
    }
}
