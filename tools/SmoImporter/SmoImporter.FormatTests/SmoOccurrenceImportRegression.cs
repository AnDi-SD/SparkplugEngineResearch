using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using SmoExporter.Core;
using SmoImporter.Core;
using SmoViewer.Core;

internal static class SmoOccurrenceImportRegression
{
    public static int Run(string source, string layeredSource, string output)
    {
        Directory.CreateDirectory(output);
        int checks = 0;
        void Check(bool condition, string message)
        { if (!condition) throw new InvalidDataException(message); ++checks; }
        var shared = SmoSceneBuilder.Build(SmoDocument.Load(source));
        var imported = SmoModelReader.Convert(shared);
        Check(imported.Meshes.Count == shared.MeshPlacements.Count, "Importer preserves every common scene occurrence");
        var variants = shared.Meshes.ToDictionary(mesh => mesh.VariantKey);
        for (int index = 0; index < shared.MeshPlacements.Count; ++index)
        {
            var placement = shared.MeshPlacements[index];
            var sourceMesh = variants[placement.EffectiveMeshKey];
            var mesh = imported.Meshes[index];
            Check(mesh.Name == placement.Name && mesh.Positions.Length == sourceMesh.Positions.Length, "Imported occurrence identity and geometry count");
            for (int vertex = 0; vertex < sourceMesh.Positions.Length; ++vertex)
            {
                var expected = sourceMesh.SkinObjectIndex is null
                    ? Vector3.Transform(sourceMesh.Positions[vertex], placement.WorldMatrix)
                    : sourceMesh.Positions[vertex];
                Check(Vector3.Distance(mesh.Positions[vertex], expected) < .0001f, "Imported geometry uses its own native-derived placement");
            }
        }
        var publicRead = SmoModelReader.Read(source);
        Check(publicRead.Meshes.Count == imported.Meshes.Count, "Public SMO import retains all active support slots");
        bool rejected = false;
        try { SmoModelReader.Read(layeredSource); }
        catch (InvalidDataException error) when (error.Message.Contains("MATERIAL_IMPORT_SHAPE")) { rejected = true; }
        Check(rejected, "Importer explicitly rejects unsupported multipass projection");
        var report = new { status = "passed", checks, placements = imported.Meshes.Count, source,
            source_sha256 = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(source))), layered_source = layeredSource };
        File.WriteAllText(Path.Combine(output, "report.json"), JsonSerializer.Serialize(report, new JsonSerializerOptions { WriteIndented = true }) + "\n");
        Console.WriteLine($"SMO import occurrences: {imported.Meshes.Count} placements, {checks} checks");
        return 0;
    }
}
