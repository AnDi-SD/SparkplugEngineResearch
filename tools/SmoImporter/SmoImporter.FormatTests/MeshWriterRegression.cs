using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using SmoImporter.Core;
using SmoViewer.Core;

internal static class MeshWriterRegression
{
    public static void Run(string output, string[] files)
    {
        Directory.CreateDirectory(output);
        var cases = new List<object>(); int checks = 0;
        void Check(bool value, string message) { checks++; if (!value) throw new InvalidDataException(message); }
        foreach (string file in files)
        {
            byte[] input = File.ReadAllBytes(file);
            var document = SmoDocument.ParseOwned(input, file);
            var formats = new HashSet<uint>(); int selected = 0;
            foreach (var entry in document.Objects.Where(e => e.TypeHash == SmoClassIds.MeshData))
            {
                if (!SmoMeshDecoder.TryDecode(document, entry, out var template, out _) ||
                    !SmoVertexLayoutRegistry.TryGet(template.VertexFormat, out _) || !formats.Add(template.VertexFormat)) continue;
                var donor = new ImportedMesh("writer_probe",
                    [new(1,2,3),new(4,2,3),new(1,6,3)],
                    [Vector3.UnitZ,Vector3.UnitZ,Vector3.UnitZ],
                    [new(.1f,.2f),new(.9f,.2f),new(.1f,.8f)], [0,1,2],
                    [0x11223344,0x55667788,0x99aabbcc])
                    { SecondaryTextureCoordinates = [new(.3f,.4f),new(.7f,.4f),new(.3f,.6f)] };
                var scene = new ImportedScene([donor], [], []);
                var expected = SmoMeshResourceReplacer.CreatePreviewMesh(document, entry.Index, scene, ReplacementTransform.Identity);
                var replacement = SmoMeshResourceReplacer.Replace(document, entry.Index, scene, ReplacementTransform.Identity);
                var verified = SmoDocument.ParseOwned(replacement.Data, file);
                var actual = SmoMeshDecoder.Decode(verified, verified.Objects[entry.Index]);
                Check(!verified.HasErrors && verified.Objects.Count == document.Objects.Count, "Repacked catalog differs");
                Check(actual.Positions.SequenceEqual(expected.Positions), "Position bytes differ from prepared input");
                Check(actual.TriangleIndices.SequenceEqual(expected.TriangleIndices), "Topology differs from prepared input");
                Check(actual.Normals.SequenceEqual(expected.Normals), "Normals differ from prepared input");
                Check(actual.TextureCoordinates.SequenceEqual(expected.TextureCoordinates), "UV0 differs from prepared input");
                Check(actual.TextureCoordinates1.SequenceEqual(expected.TextureCoordinates1), "UV1 differs from prepared input");
                Check(actual.DiffuseColorsArgb.SequenceEqual(expected.DiffuseColorsArgb), "Colors differ from prepared input");
                Check(actual.BlendWeights.SequenceEqual(expected.BlendWeights), "Weights differ from prepared input");
                Check(actual.BlendIndices.SequenceEqual(expected.BlendIndices), "Bones differ from prepared input");
                string name = Path.GetFileNameWithoutExtension(file) + "-" + entry.Index + ".smo";
                File.WriteAllBytes(Path.Combine(output, name), replacement.Data);
                cases.Add(new {file=Path.GetFullPath(file), input_sha256=Convert.ToHexString(SHA256.HashData(input)),
                    index=entry.Index,format=template.VertexFormat,marker=template.Marker,output=name,
                    output_sha256=Convert.ToHexString(SHA256.HashData(replacement.Data))});
                if (++selected == 3) break;
            }
            Check(selected > 0, "No representative writable mesh");
            Check(File.ReadAllBytes(file).SequenceEqual(input), "Source file changed");
        }
        File.WriteAllText(Path.Combine(output,"report.json"),JsonSerializer.Serialize(new {checks,cases},new JsonSerializerOptions{WriteIndented=true}));
        Console.WriteLine($"PASS mesh writer: {cases.Count} replacements, {checks} checks");
    }
}
