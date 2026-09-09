using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using SmoImporter.Core;
using SmoViewer.Core;

internal static class CollisionWriterRegression
{
    public static void Run(string output, string[] files)
    {
        Directory.CreateDirectory(output); var rows = new List<object>();
        foreach (string file in files)
        {
            byte[] original = File.ReadAllBytes(file); var document = SmoDocument.ParseOwned(original, file);
            var result = SmoCollisionBranchAppender.Append(document,
                [new Vector3(0,0,0),new Vector3(2,0,0),new Vector3(0,4,0),new Vector3(0,0,6)],
                [0,2,1,0,1,3,0,3,2,1,2,3], "shared_collision_probe");
            var after = SmoDocument.ParseOwned(result.Data, file);
            if (after.HasErrors || after.Objects.Count != document.Objects.Count + result.AddedObjectCount ||
                !File.ReadAllBytes(file).SequenceEqual(original)) throw new InvalidDataException("Collision output/catalog/source verification failed");
            var entry = after.Objects[result.MeshBoundingVolumeObjectIndex];
            byte[] leaf = after.Data.Span.Slice(checked((int)entry.PhysicalOffset),checked((int)entry.SerializedSize)).ToArray();
            string name = Path.GetFileNameWithoutExtension(file);
            File.WriteAllBytes(Path.Combine(output,name+".smo"),result.Data);
            File.WriteAllBytes(Path.Combine(output,name+".meshbv"),leaf);
            rows.Add(new {file=Path.GetFullPath(file),input_sha256=Hash(original),output_sha256=Hash(result.Data),
                mesh_bv_sha256=Hash(leaf),result.AddedObjectCount});
        }
        File.WriteAllText(Path.Combine(output,"report.json"),JsonSerializer.Serialize(new {status="passed",rows},new JsonSerializerOptions{WriteIndented=true}));
        Console.WriteLine($"PASS collision writers: {rows.Count} complete appended branches");
    }
    private static string Hash(byte[] bytes)=>Convert.ToHexString(SHA256.HashData(bytes));
}
