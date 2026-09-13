using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using System.Text.Json.Nodes;
using SmoViewer.Core;

namespace SmoViewer.FormatTests;

internal static class SmoBoundingVolumeInspectionRegression
{
    // Transport only: expected scalar/radius bits come from archived original-PC
    // captures compared with the current native bridge, never from C# arithmetic.
    private static byte[] Bits(params float[] values) =>
        values.SelectMany(BitConverter.GetBytes).ToArray();

    private static bool Read(uint classId, uint field, byte[] payload, out byte[] values,
        out byte[] derived)
    {
        values = []; derived = [];
        if (field == 0)
        {
            Vector3 position = default;
            bool valid = classId switch
            {
                SmoClassIds.SphereBoundingVolume => SmoSphereBoundingVolumeDecoder.TryDecodePosition(payload, out position),
                SmoClassIds.BoxBoundingVolume => SmoBoxBoundingVolumeDecoder.TryDecodePosition(payload, out position),
                SmoClassIds.OrientedBoxBoundingVolume => SmoOrientedBoxBoundingVolumeDecoder.TryDecodePosition(payload, out position),
                _ => false
            };
            values = Bits(position.X, position.Y, position.Z);
            return valid;
        }
        if (field == 1 && classId == SmoClassIds.SphereBoundingVolume)
        {
            bool valid = SmoSphereBoundingVolumeDecoder.TryDecodeRadius(payload, out float radius);
            values = Bits(radius);
            return valid;
        }
        if (field == 1 && classId == SmoClassIds.BoxBoundingVolume)
        {
            if (!SmoBoxBoundingVolumeDecoder.TryDecodeSize(payload, out var box)) return false;
            values = Bits(box!.FullSize.X, box.FullSize.Y, box.FullSize.Z);
            derived = Bits(box.HalfExtents.X, box.HalfExtents.Y, box.HalfExtents.Z, box.BoundingSphereRadius);
            return true;
        }
        if (field == 1 && classId == SmoClassIds.OrientedBoxBoundingVolume)
        {
            if (!SmoOrientedBoxBoundingVolumeDecoder.TryDecodeSize(payload, out var box)) return false;
            values = Bits(box!.FullSize.X, box.FullSize.Y, box.FullSize.Z);
            derived = Bits(box.HalfExtents.X, box.HalfExtents.Y, box.HalfExtents.Z, box.BoundingSphereRadius);
            return true;
        }
        if (field == 2 && classId == SmoClassIds.OrientedBoxBoundingVolume)
        {
            bool valid = SmoOrientedBoxBoundingVolumeDecoder.TryDecodeRotation(payload, out var rotation);
            values = Bits(rotation.X, rotation.Y, rotation.Z, rotation.W);
            return valid;
        }
        return false;
    }

    internal static int Run(string folder, string output)
    {
        int checks = 0;
        void Check(bool value, string label)
        {
            if (!value) throw new InvalidDataException(label);
            ++checks;
        }
        var input = JsonNode.Parse(File.ReadAllText(Path.Combine(folder, "abi-1.json")))!;
        var rows = input["rows"]!.AsArray();
        foreach (var row in rows)
        {
            uint classId = row!["class_id"]!.GetValue<uint>();
            uint field = row["field"]!.GetValue<uint>();
            byte[] payload = Convert.FromHexString(row["payload_hex"]!.GetValue<string>());
            byte[] expected = Convert.FromHexString(row["state_hex"]!.GetValue<string>());
            Check(Read(classId, field, payload, out var values, out var derived), "original scalar accepted");
            Check(values.AsSpan().SequenceEqual(expected.AsSpan(0, values.Length)), "authored scalar bits match original/ABI");
            if (derived.Length != 0)
                Check(derived.AsSpan().SequenceEqual(expected.AsSpan(16, 16)), "derived half extents and original radius bits");
            Check(!Read(classId, field, payload[..^1], out _, out _), "truncated scalar rejected");
            Check(!Read(classId, field, [..payload, 0], out _, out _), "oversized scalar rejected");
        }
        // Existing field inspector must consume the three shared projections,
        // including a file whose unrelated full-graph load is still unsupported.
        string root = Path.GetFullPath(Path.Combine(folder, "../../../../.."));
        var specimens = new List<object>();
        foreach (var file in input["real_files"]!.AsArray())
        {
            string path = Path.Combine(root, file!["source"]!.GetValue<string>());
            Check(Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path))).Equals(
                file["sha256"]!.GetValue<string>(), StringComparison.OrdinalIgnoreCase), "same real specimen");
            var document = SmoDocument.Load(path);
            var volumes = document.Objects.Where(entry => entry.TypeHash is
                SmoClassIds.SphereBoundingVolume or SmoClassIds.BoxBoundingVolume or SmoClassIds.OrientedBoxBoundingVolume).ToArray();
            Check(volumes.Length > 0, "selected file contains a real volume");
            foreach (var volume in volumes)
            {
                string key = volume.TypeHash switch
                {
                    SmoClassIds.SphereBoundingVolume => "sphere_bv.radius",
                    SmoClassIds.BoxBoundingVolume => "box_bv.size",
                    _ => "obb.size"
                };
                Check(SmoSerializedFieldInspector.Inspect(document, volume)
                    .Any(item => item.Descriptor.Key == key && item.IsDecoded), "real volume inspected through shared reader");
            }
            string? loadIssue = SmoLoadedResources.Get(document).LoadIssue;
            if (file["status"]!.GetValue<string>() == "passed")
                Check(loadIssue is null, loadIssue ?? "actual graph loaded");
            specimens.Add(new { path = file["source"]!.GetValue<string>(), volumes = volumes.Length, loadIssue });
        }
        string hash = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(
            Path.Combine(AppContext.BaseDirectory, "SparkplugViewerNative.dll"))));
        Check(hash.Equals(input["native_dll_sha256"]!.GetValue<string>(), StringComparison.OrdinalIgnoreCase),
            "managed process loads the exact native DLL used for original/ABI comparison");
        File.WriteAllText(output, JsonSerializer.Serialize(new
        {
            status = "passed", checks, scalarRows = rows.Count, specimens, native_dll_sha256 = hash
        }, new JsonSerializerOptions { WriteIndented = true }) + "\n");
        Console.WriteLine($"Bounding volume inspection: {checks} checks, {rows.Count} original scalar rows");
        return 0;
    }
}
