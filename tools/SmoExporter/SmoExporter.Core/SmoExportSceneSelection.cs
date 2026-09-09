using System.Numerics;
using SmoViewer.Core;

namespace SmoExporter.Core;

/// <summary>A concrete rendered occurrence selected by an editor host.</summary>
public sealed record SmoExportPlacementSelection(
    int MeshObjectIndex,
    int SceneObjectIndex,
    Matrix4x4 NativeWorldMatrix)
{
    public SmoRenderOccurrenceKey? OccurrenceKey { get; init; }
}

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
                (selection.MeshObjectIndex, selection.SceneObjectIndex, selection.OccurrenceKey))
            .ToArray();
        if (requested.Length == 0)
            throw new ArgumentException(
                "At least one visual placement must be selected.",
                nameof(selections));

        var available = scene.MeshPlacements.ToLookup(
                placement =>
                    (placement.MeshObjectIndex, placement.SceneObjectIndex));
        var placements = new List<SmoExportMeshPlacement>(requested.Length);
        foreach (SmoExportPlacementSelection selection in requested)
        {
            var matches = available[(selection.MeshObjectIndex, selection.SceneObjectIndex)]
                .Where(value => selection.OccurrenceKey is null || value.OccurrenceKey == selection.OccurrenceKey).ToArray();
            if (matches.Length != 1)
            {
                throw new InvalidDataException(
                    $"Export selection resolves {matches.Length} placements; select an explicit container/member slot for mesh " +
                    $"[{selection.MeshObjectIndex}] / scene " +
                    $"[{selection.SceneObjectIndex}].");
            }
            var placement = matches[0];
            Matrix4x4 world = SmoExportCoordinateSystem.ToExportMatrix(
                selection.NativeWorldMatrix);
            placements.Add(placement with
            {
                ParentNodeObjectIndex = null,
                WorldMatrix = world,
                LocalMatrix = world
            });
        }

        HashSet<SmoExportMeshKey> meshIndices = placements
            .Select(placement => placement.EffectiveMeshKey)
            .ToHashSet();
        SmoExportMesh[] meshes = scene.Meshes
            .Where(mesh => meshIndices.Contains(mesh.VariantKey))
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
            var missing = meshIndices
                .Except(meshes.Select(mesh => mesh.VariantKey))
                .ToArray();
            throw new InvalidDataException(
                $"Selected placements reference unavailable meshes: " +
                string.Join(", ", missing));
        }
        if (scene.Meshes.Any(mesh => meshIndices.Contains(mesh.VariantKey) && mesh.SkinObjectIndex is not null))
            throw new InvalidDataException("EXPORT_SKIN_SELECTION: a static placement selection cannot discard an actual Skin; use skeleton export or an explicit posed-mesh conversion.");

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
