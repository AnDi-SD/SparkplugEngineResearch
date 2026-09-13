using System.Numerics;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text.Json;
using SmoViewer.Core;

namespace SmoViewer.FormatTests;

internal static class SmoOctreeReaderRegression
{
    internal static int Run(string source, string output)
    {
        var document = SmoDocument.Load(source);
        int checks = 0, count = 0;
        void Check(bool condition, string message)
        { if (!condition) throw new InvalidDataException(message); ++checks; }
        Check(SmoLoadedResources.Get(document).LoadIssue is null,
            "The actual PC ResourceGraph must load before spatial inspection");
        foreach (var entry in document.Objects.Where(value => value.TypeHash == SmoClassIds.OctreeNode))
        {
            Check(SmoOctreeNodeDecoder.TryDecode(document, entry, out var decoded, out var error), error);
            Check(SmoObjectFieldReader.TryRead(document, entry, out var fields, out error), error);
            var own = fields.SkipWhile(field => field.PayloadSize != 0).Skip(1).Where(field => field.PayloadSize != 0).ToArray();
            Vector3[] expected = [default, default, default];
            foreach (var field in own.Where(field => field.FieldType is >= 0 and <= 2))
                expected[field.FieldType] = MemoryMarshal.Read<Vector3>(field.Payload.Span);
            Check(decoded!.Pivot == expected[0] && decoded.Minimum == expected[1] && decoded.Maximum == expected[2], "Loaded Octree state preserves this fixture's authored vectors");
            Check(decoded.PartitionNode.Children.Select(value => value.SlotIndex).SequenceEqual(Enumerable.Range(0,8).Select(value => (uint)value)), "Selected real Octree has all eight observed child slots");
            ++count;
        }
        Check(count > 0, "Selected fixture must exercise Octree");
        var report = new { status = "passed", source = Path.GetFullPath(source), source_sha256 = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(source))),
            native_dll_sha256 = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(Path.Combine(AppContext.BaseDirectory,"SparkplugViewerNative.dll")))),
            octree_nodes = count, checks, scope = "Actual PC ResourceGraph Octree inspection for this selected fixture; not full-corpus or PS2 runtime coverage." };
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(output))!);
        File.WriteAllText(output, JsonSerializer.Serialize(report, new JsonSerializerOptions { WriteIndented = true }) + "\n");
        Console.WriteLine($"Octree metadata: {count} nodes, {checks} checks");
        return 0;
    }
}
