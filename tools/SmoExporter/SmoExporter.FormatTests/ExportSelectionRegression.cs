using System.Buffers.Binary;
using System.Globalization;
using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using SmoExporter.Core;
using SmoViewer.Core;

internal static class ExportSelectionRegression
{
    public static int Run(string source, string skinSource, string output)
    {
        Directory.CreateDirectory(output);
        int checks = 0;
        void Check(bool condition, string message)
        { if (!condition) throw new InvalidDataException(message); ++checks; }
        void Reject(Action action, string fragment)
        {
            try { action(); }
            catch (Exception error) when (error.Message.Contains(fragment, StringComparison.OrdinalIgnoreCase))
            { ++checks; return; }
            throw new InvalidDataException("Expected rejection: " + fragment);
        }
        var scene = SmoSceneBuilder.Build(SmoDocument.Load(source));
        var repeated = scene.MeshPlacements.GroupBy(p => (p.MeshObjectIndex, p.SceneObjectIndex))
            .First(group => group.Count() > 1).ToArray();
        var shared = scene.Meshes.GroupBy(mesh => mesh.ObjectIndex).First(group => group.Count() > 1).ToArray();
        Reject(() => SmoExportSceneSelection.Create(scene,
            [new(repeated[0].MeshObjectIndex, repeated[0].SceneObjectIndex, Matrix4x4.Identity)]), "explicit container/member slot");
        var selected = SmoExportSceneSelection.Create(scene, repeated.Select(p =>
            new SmoExportPlacementSelection(p.MeshObjectIndex, p.SceneObjectIndex,
                SmoExportCoordinateSystem.ToExportMatrix(p.WorldMatrix)) { OccurrenceKey = p.OccurrenceKey }));
        Check(selected.MeshPlacements.Count == repeated.Length && selected.Meshes.Count == 1,
            "Selecting explicit repeated slots preserves every occurrence with one variant");
        for (int i = 0; i < repeated.Length; ++i)
            Check(selected.MeshPlacements[i].WorldMatrix == repeated[i].WorldMatrix, "Selection preserves native-derived placement matrix");
        Reject(() => SmoExportSceneSplitter.CreateSingleMeshScene(scene, shared[0].ObjectIndex), "concrete Mesh/Model key");
        foreach (var mesh in shared)
        {
            var split = SmoExportSceneSplitter.CreateSingleMeshScene(scene, mesh.VariantKey);
            Check(split.Meshes.Single().VariantKey == mesh.VariantKey && split.MeshPlacements.Single().EffectiveMeshKey == mesh.VariantKey,
                "Independent split retains selected material variant");
            Check(split.MeshPlacements[0].WorldMatrix == Matrix4x4.Identity && ReferenceEquals(split.Meshes[0].Positions, mesh.Positions),
                "Independent rigid export changes placement only");
        }
        var keys = shared.Select(mesh => mesh.VariantKey).Append(selected.Meshes[0].VariantKey).ToHashSet();
        var placements = scene.MeshPlacements.Where(p => keys.Contains(p.EffectiveMeshKey)).ToArray();
        var subset = SmoExportSceneSelection.Create(scene, placements.Select(p =>
            new SmoExportPlacementSelection(p.MeshObjectIndex, p.SceneObjectIndex,
                SmoExportCoordinateSystem.ToExportMatrix(p.WorldMatrix)) { OccurrenceKey = p.OccurrenceKey }));
        string bakedPath = Path.Combine(output, "baked.glb");
        GlbExporter.Export(subset with { SceneMode = SmoExportSceneMode.LevelWithBakedObjects }, bakedPath);
        var bytes = File.ReadAllBytes(bakedPath);
        using (var json = JsonDocument.Parse(bytes.AsMemory(20, checked((int)BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(12))))))
        {
            var meshes = json.RootElement.GetProperty("meshes").EnumerateArray().ToArray();
            var nodes = json.RootElement.GetProperty("nodes").EnumerateArray().Where(n => n.TryGetProperty("mesh", out _)).ToArray();
            Check(meshes.Length == placements.Length && nodes.Length == placements.Length, "Baked GLB expands every slot");
            Check(nodes.Select(n => n.GetProperty("mesh").GetInt32()).Distinct().Count() == placements.Length, "Baked GLB does not collapse repeated Model indices");
            Check(meshes.Select(m => m.GetProperty("primitives")[0].GetProperty("attributes").GetProperty("POSITION").GetInt32()).Distinct().Count() == placements.Length,
                "Baked GLB has separately editable geometry buffers");
            foreach (var node in nodes)
            {
                var extras = node.GetProperty("extras");
                var key = new SmoRenderOccurrenceKey(extras.GetProperty("sparkplugContainerObjectIndex").GetInt32(), extras.GetProperty("sparkplugMemberSlot").GetInt32());
                Check(placements.Any(p => p.OccurrenceKey == key), "Baked GLB preserves real slot provenance");
            }
        }
        string objPath = Path.Combine(output, "selected.obj");
        ObjExporter.Export(subset, objPath);
        var lines = File.ReadAllLines(objPath); var mtl = File.ReadAllLines(Path.ChangeExtension(objPath, ".mtl"));
        Check(lines.Count(line => line.StartsWith("o ")) == placements.Length, "OBJ includes all selected occurrences");
        Check(mtl.Count(line => line.StartsWith("newmtl ")) == subset.Meshes.Count, "OBJ retains every material variant");
        var expectedPositions = subset.MeshPlacements.SelectMany(p => subset.Meshes.Single(m => m.VariantKey == p.EffectiveMeshKey).Positions.Select(v => Vector3.Transform(v, p.WorldMatrix))).ToArray();
        var actualPositions = lines.Where(line => line.StartsWith("v ")).Select(line => line[2..].Split(' ').Select(v => float.Parse(v, CultureInfo.InvariantCulture)).ToArray()).ToArray();
        Check(actualPositions.Length == expectedPositions.Length, "OBJ expanded vertex count");
        for (int i = 0; i < actualPositions.Length; ++i)
            Check(new Vector3(actualPositions[i][0], actualPositions[i][1], actualPositions[i][2]) == expectedPositions[i], "OBJ vertex uses its own occurrence world");
        var skinScene = SmoSceneBuilder.Build(SmoDocument.Load(skinSource));
        var skin = skinScene.Meshes.First(m => m.SkinObjectIndex is not null);
        var skinPlacement = skinScene.MeshPlacements.First(p => p.EffectiveMeshKey == skin.VariantKey);
        Reject(() => SmoExportSceneSelection.Create(skinScene,
            [new(skin.ObjectIndex, skinPlacement.SceneObjectIndex, Matrix4x4.Identity) { OccurrenceKey = skinPlacement.OccurrenceKey }]), "EXPORT_SKIN_SELECTION");
        Reject(() => SmoExportSceneSplitter.CreateSingleMeshScene(skinScene, skin.VariantKey), "skinned");
        // Deliberately corrupt one alias in a small host DTO: the native bridge
        // must reject the conflict instead of silently aliasing different data.
        var changed = shared[1] with { Positions = shared[1].Positions.ToArray() };
        changed.Positions[0] += Vector3.UnitX;
        var corrupt = subset with { Meshes = subset.Meshes.Select(m => m.VariantKey == changed.VariantKey ? changed : m).ToArray() };
        Reject(() => FbxExporter.Export(corrupt, Path.Combine(output, "rejected.fbx")), "geometry");
        var report = new { status = "passed", checks, source, source_sha256 = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(source))),
            selected_placements = subset.MeshPlacements.Count, selected_variants = subset.Meshes.Count, repeated_slots = repeated.Length,
            baked_sha256 = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(bakedPath))),
            obj_sha256 = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(objPath))) };
        File.WriteAllText(Path.Combine(output, "report.json"), JsonSerializer.Serialize(report, new JsonSerializerOptions { WriteIndented = true }) + "\n");
        Console.WriteLine($"Selection/OBJ/baked/FBX guards: {checks} checks");
        return 0;
    }
}
