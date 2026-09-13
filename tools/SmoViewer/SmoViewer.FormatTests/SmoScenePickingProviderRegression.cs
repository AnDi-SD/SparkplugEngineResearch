using System.Numerics;
using System.Text.Json;
using SmoViewer.Core;
using SmoViewer.Scene;

namespace SmoViewer.FormatTests;

/// <summary>Host picking/provider contract; contains no skinning oracle.</summary>
internal static class SmoScenePickingProviderRegression
{
    internal static int Run(string path, string output)
    {
        int checks = 0;
        void Check(bool condition, string message)
        {
            if (!condition) throw new InvalidDataException(message);
            ++checks;
        }

        var document = SmoDocument.Load(path);
        SmoSceneMesh template = SmoSceneBuilder.Build(document).Meshes.First();
        Vector3[] triangle = [new(0, 0, 0), new(2, 0, 0), new(0, 2, 0)];
        SmoSceneMesh Make(bool skinned) => template with
        {
            Mesh = SmoMesh.CreateTransient(template.Mesh, triangle, [], [], [],
                skinned ? [Vector4.One, Vector4.One, Vector4.One] : [],
                skinned ? [new(0, 0, 0, 0), new(0, 0, 0, 0), new(0, 0, 0, 0)] : [],
                [0, 1, 2]),
            WorldTransform = Matrix4x4.CreateTranslation(5, 0, 0),
            SkinObjectIndex = skinned ? template.SceneObjectIndex : null,
            // Deliberately unrelated to provider positions: the picker must
            // never evaluate a palette or use it as an unavailable fallback.
            InitialSkinMatrices = skinned ? [Matrix4x4.CreateTranslation(999, 999, 999)] : null
        };
        SmoSceneMesh rigid = Make(false), skin = Make(true);
        var ray = new SmoPickRay(new Vector3(5.25f, .25f, 4), -Vector3.UnitZ);

        int providerCalls = 0;
        var rigidIndex = new SmoScenePickIndex([rigid], _ =>
        {
            ++providerCalls;
            throw new InvalidOperationException("Static picking must not call a skin provider");
        });
        Check(providerCalls == 0 && rigidIndex.Count == 1 && rigidIndex.Issues.Count == 0,
            "Static geometry is independent of the provider");
        Check(rigidIndex.Pick(ray) is { Distance: 4, TriangleIndex: 0 },
            "Static triangle/world picking remains unchanged");

        var unsupported = new SmoScenePickIndex([skin]);
        Check(unsupported.Count == 0 && unsupported.Issues.Count == 1 && unsupported.Pick(ray) is null,
            "Skin without a backend snapshot is an explicit unsupported entry");
        Check(unsupported.Issues[0].Contains("SKIN_PICKING_POSITIONS_UNAVAILABLE", StringComparison.Ordinal),
            "Unsupported skin carries a stable diagnostic code");
        foreach (Vector3[]? invalid in new Vector3[]?[]
        {
            null,
            [Vector3.Zero],
            [new(float.NaN, 0, 0), Vector3.UnitX, Vector3.UnitY],
            [new(float.PositiveInfinity, 0, 0), Vector3.UnitX, Vector3.UnitY]
        })
        {
            var invalidIndex = new SmoScenePickIndex([skin], _ => invalid);
            Check(invalidIndex.Count == 0 && invalidIndex.Issues.Count == 1 && invalidIndex.Pick(ray) is null,
                "Unavailable, wrong-count, and non-finite snapshots cannot produce hits");
        }

        Vector3[] supplied = triangle.Select(value => value + new Vector3(0, 0, 2)).ToArray();
        var suppliedIndex = new SmoScenePickIndex([rigid, skin], mesh =>
        {
            ++providerCalls;
            Check(ReferenceEquals(mesh, skin), "Provider receives the actual skinned occurrence");
            return supplied;
        });
        Check(providerCalls == 1 && suppliedIndex.Count == 2 && suppliedIndex.Issues.Count == 0,
            "Only the skinned occurrence requests one snapshot");
        Check(suppliedIndex.Pick(ray) is { Distance: 2, TriangleIndex: 0 } hit &&
            ReferenceEquals(hit.SceneMesh, skin) && hit.Position == new Vector3(5.25f, .25f, 2),
            "Picking uses supplied local positions and applies the occurrence world transform once");
        Array.Fill(supplied, new Vector3(1000, 1000, 1000));
        Check(suppliedIndex.Pick(ray) is { Distance: 2 },
            "Captured positions remain consistent with bounds after provider storage changes");
        Check(suppliedIndex.Pick(ray, mesh => ReferenceEquals(mesh, rigid)) is { Distance: 4 },
            "Selection predicates retain static geometry beside a supplied skin");
        var empty = new SmoScenePickIndex([]);
        Check(empty.Count == 0 && empty.Issues.Count == 0 && empty.Pick(ray) is null,
            "Empty selection is supported without requiring a provider");

        File.WriteAllText(output, JsonSerializer.Serialize(new
        {
            status = "passed", checks, source = Path.GetFullPath(path),
            boundary = "Host synthetic triangle/provider contract; actual GPU shader equivalence is tested separately."
        }, new JsonSerializerOptions { WriteIndented = true }) + "\n");
        Console.WriteLine($"Scene picking provider: {checks} checks");
        return 0;
    }
}
