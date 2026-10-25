namespace SmoImporter.Core;

/// <summary>
/// Canonical, deterministic preparation shared by direct and isolated-worker
/// level imports. It keeps SMO part indices stable across process boundaries.
/// </summary>
public static class SmoLevelRigidImportPreparer
{
    public static ImportedScene Prepare(ImportedScene scene)
    {
        ArgumentNullException.ThrowIfNull(scene);
        ImportedScene split = MeshSplitter.SplitRigidScene(scene);
        return SmoLevelEmbeddedTextureBudget.Prepare(split);
    }
}
