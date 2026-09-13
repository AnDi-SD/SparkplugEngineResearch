namespace SmoViewer.Core;

/// <summary>
/// Observational consistency check for generated opaque face runs. Imported
/// run names are part of the importer/viewer diagnostic contract; no native
/// lighting equation is inferred from the mismatch.
/// </summary>
public sealed record SmoImportedFaceDiffuseInfo(
    int RetainedOpaqueWhiteSurfaceCount,
    int ImportedOpaqueFaceRunCount,
    int ImportedOpaqueBlackFaceRunCount,
    int ImportedOpaqueWhiteFaceRunCount,
    bool HasDiffuseMismatch,
    string? Diagnostic);

public static class SmoImportedFaceDiffuseAnalyzer
{
    public static SmoImportedFaceDiffuseInfo Analyze(
        IReadOnlyList<SmoMesh> meshes)
    {
        ArgumentNullException.ThrowIfNull(meshes);

        int retainedWhite = 0;
        int imported = 0;
        int importedBlack = 0;
        int importedWhite = 0;
        foreach (SmoMesh mesh in meshes)
        {
            if (!mesh.HasSkinningData || !mesh.HasNormals ||
                mesh.VertexCount == 0)
            {
                continue;
            }

            SmoVertexDiffuseProfile profile =
                SmoMaterialRenderState.ClassifyVertexDiffuse(mesh);
            if (mesh.Name.StartsWith(
                    "imp_o_x_", StringComparison.OrdinalIgnoreCase))
            {
                imported++;
                if (profile == SmoVertexDiffuseProfile.UniformOpaqueBlack)
                    importedBlack++;
                else if (profile == SmoVertexDiffuseProfile.UniformOpaqueWhite)
                    importedWhite++;
            }
            else if (!mesh.Name.StartsWith(
                         "imp_", StringComparison.OrdinalIgnoreCase) &&
                     profile == SmoVertexDiffuseProfile.UniformOpaqueWhite)
            {
                retainedWhite++;
            }
        }

        bool mismatch = retainedWhite > 0 && importedBlack > 0;
        string? diagnostic = mismatch
            ? "IMPORTED_FACE_VERTEX_DIFFUSE_MISMATCH: retained skinned " +
              $"surfaces include {retainedWhite} uniform FFFFFFFF run(s), " +
              $"while {importedBlack}/{imported} generated imp_o_x face " +
              "run(s) use FF000000. Native fixed-function lighting may treat " +
              "the runs differently as the model turns. This is a serialized " +
              "data mismatch; the OpenGL preview does not reproduce that native " +
              "equation and is not proof of native-frame parity."
            : null;

        return new SmoImportedFaceDiffuseInfo(
            retainedWhite,
            imported,
            importedBlack,
            importedWhite,
            mismatch,
            diagnostic);
    }
}
