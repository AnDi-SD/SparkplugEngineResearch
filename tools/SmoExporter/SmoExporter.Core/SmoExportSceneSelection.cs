using System.Numerics;

namespace SmoExporter.Core;

/// <summary>A concrete rendered occurrence selected by an editor host.</summary>
public sealed record SmoExportPlacementSelection(
    int MeshObjectIndex,
    int SceneObjectIndex,
    Matrix4x4 NativeWorldMatrix);

/// <summary>
/// Projects a decoded level scene to an arbitrary set of visible placements.
/// This is shared by editor hosts so selection semantics are not reimplemented
/// in individual GUIs.
/// </summary>
public static class SmoExportSceneSelection
{
    public static SmoExportScene Create(
        SmoExportScene scene,
        IEnumerable<SmoExportPlacementSelection> selections)
    {
        ArgumentNullException.ThrowIfNull(scene);
        ArgumentNullException.ThrowIfNull(selections);
        SmoExportPlacementSelection[] requested = selections
            .DistinctBy(selection =>
                (selection.MeshObjectIndex, selection.SceneObjectIndex))
            .ToArray();
        if (requested.Length == 0)
            throw new ArgumentException(
                "At least one visual placement must be selected.",
                nameof(selections));

        Dictionary<(int Mesh, int Scene), SmoExportMeshPlacement> available =
            scene.MeshPlacements.ToDictionary(
                placement =>
                    (placement.MeshObjectIndex, placement.SceneObjectIndex));
        var placements = new List<SmoExportMeshPlacement>(requested.Length);
        foreach (SmoExportPlacementSelection selection in requested)
        {
            if (!available.TryGetValue(
                    (selection.MeshObjectIndex, selection.SceneObjectIndex),
                    out SmoExportMeshPlacement? placement))
            {
                throw new InvalidDataException(
                    $"Export scene has no placement for mesh " +
                    $"[{selection.MeshObjectIndex}] / scene " +
                    $"[{selection.SceneObjectIndex}].");
            }
            Matrix4x4 world = SmoExportCoordinateSystem.ToExportMatrix(
                selection.NativeWorldMatrix);
            placements.Add(placement with
            {
                ParentNodeObjectIndex = null,
                WorldMatrix = world,
                LocalMatrix = world
            });
        }

        HashSet<int> meshIndices = placements
            .Select(placement => placement.MeshObjectIndex)
            .ToHashSet();
        SmoExportMesh[] meshes = scene.Meshes
            .Where(mesh => meshIndices.Contains(mesh.ObjectIndex))
            .Select(mesh => mesh with
            {
                SkinObjectIndex = null,
                ParentNodeObjectIndex = null,
                BindWorldMatrix = Matrix4x4.Identity,
                BindLocalMatrix = Matrix4x4.Identity,
                BlendWeights = [],
                JointIndices = []
            })
            .ToArray();
        if (meshes.Length != meshIndices.Count)
        {
            int[] missing = meshIndices
                .Except(meshes.Select(mesh => mesh.ObjectIndex))
                .Order()
                .ToArray();
            throw new InvalidDataException(
                $"Selected placements reference unavailable meshes: " +
                string.Join(", ", missing));
        }

        SmoExportResourceTypes resources = scene.Resources &
            (SmoExportResourceTypes.Meshes |
             SmoExportResourceTypes.Materials |
             SmoExportResourceTypes.Textures);
        return scene with
        {
            Resources = resources,
            SceneMode = SmoExportSceneMode.LevelWithInstances,
            Meshes = meshes,
            MeshPlacements = placements,
            Nodes = [],
            Skins = [],
            Animations = []
        };
    }
}

public static class SmoExportCoordinateSystem
{
    public static Matrix4x4 ToExportMatrix(Matrix4x4 nativeMatrix)
    {
        Matrix4x4 reflection = Matrix4x4.CreateScale(1, 1, -1);
        return reflection * nativeMatrix * reflection;
    }
}
