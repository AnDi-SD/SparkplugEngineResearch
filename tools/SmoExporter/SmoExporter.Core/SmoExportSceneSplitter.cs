using System.Numerics;

namespace SmoExporter.Core;

public static class SmoExportSceneSplitter
{
    public static SmoExportScene CreateSingleMeshScene(
        SmoExportScene scene, int meshObjectIndex)
    {
        ArgumentNullException.ThrowIfNull(scene);
        SmoExportMesh source = scene.Meshes.SingleOrDefault(mesh =>
            mesh.ObjectIndex == meshObjectIndex) ?? throw new ArgumentException(
                $"Scene does not contain mesh [{meshObjectIndex}].",
                nameof(meshObjectIndex));
        if (source.SkinObjectIndex is not null)
        {
            throw new InvalidOperationException(
                $"Mesh [{source.ObjectIndex}] {source.Name} is skinned and cannot be " +
                "exported as an independent rigid level element.");
        }

        SmoExportMesh mesh = source with
        {
            ParentNodeObjectIndex = null,
            BindWorldMatrix = Matrix4x4.Identity,
            BindLocalMatrix = Matrix4x4.Identity
        };
        var placement = new SmoExportMeshPlacement(
            mesh.ObjectIndex,
            mesh.Name,
            mesh.ObjectIndex,
            IsSharedInstance: false,
            StaticObjectIndex: null,
            MaterialObjectIndex: null,
            ParentNodeObjectIndex: null,
            Matrix4x4.Identity,
            Matrix4x4.Identity);
        SmoExportResourceTypes resources = scene.Resources &
            (SmoExportResourceTypes.Meshes |
             SmoExportResourceTypes.Materials |
             SmoExportResourceTypes.Textures);
        return scene with
        {
            Resources = resources,
            SceneMode = SmoExportSceneMode.SeparateMeshes,
            Meshes = [mesh],
            MeshPlacements = [placement],
            Nodes = [],
            Skins = [],
            Animations = []
        };
    }
}
