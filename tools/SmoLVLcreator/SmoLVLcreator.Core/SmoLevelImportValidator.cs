using SmoImporter.Core;
using System.Collections.ObjectModel;
using System.Numerics;

namespace SmoLVLcreator.Core;

public enum SmoLevelImportPurpose
{
    AddExternalModel,
    ReplaceModelResource,
    ReplaceCompositeModel
}

public sealed record SmoLevelImportValidationReport(
    IReadOnlyList<string> Errors,
    IReadOnlyList<string> Warnings)
{
    public bool CanImport => Errors.Count == 0;
}

/// <summary>
/// One rigid-level import contract shared by GUI preflight and document/core
/// mutations. It rejects data the level writer cannot represent before any
/// editor history or SMO resource state is changed.
/// </summary>
public static class SmoLevelImportValidator
{
    public static IEnumerable<string> DescribeWarnings(ImportedScene scene)
    {
        var translated = scene.ImportDiagnostics.ToLookup(item => item.Message, StringComparer.Ordinal);
        foreach (string warning in scene.ImportWarnings)
        {
            ImportedDiagnostic? diagnostic = translated[warning].FirstOrDefault();
            yield return diagnostic is null
                ? warning
                : LevelText.Get("Import.Warning." + diagnostic.Code, diagnostic.Arguments);
        }
    }

    public static SmoLevelImportValidationReport Validate(
        ImportedScene scene,
        SmoLevelImportPurpose purpose,
        int? expectedMeshCount = null)
    {
        ArgumentNullException.ThrowIfNull(scene);
        var errors = new List<string>();
        var warnings = new List<string>(DescribeWarnings(scene));
        if (scene.Meshes.Count == 0)
            errors.Add(LevelText.Get("Import.NoMeshes"));
        if (expectedMeshCount is int expected && scene.Meshes.Count != expected)
        {
            errors.Add(LevelText.Get("Import.MeshCountMismatch", expected, scene.Meshes.Count));
        }
        if (scene.HasSkinning)
        {
            errors.Add(LevelText.Get("Import.RigidOnly"));
        }

        for (int meshIndex = 0; meshIndex < scene.Meshes.Count; meshIndex++)
        {
            ImportedMesh mesh = scene.Meshes[meshIndex];
            string name = string.IsNullOrWhiteSpace(mesh.Name)
                ? LevelText.Get("Import.Mesh", meshIndex)
                : LevelText.Get("Import.NamedMesh", meshIndex, mesh.Name);
            if (mesh.Positions.Length < 3)
                errors.Add(LevelText.Get("Import.TooFewVertices", name));
            if (mesh.TriangleIndices.Length == 0 ||
                mesh.TriangleIndices.Length % 3 != 0)
            {
                errors.Add(LevelText.Get("Import.NonTriangleIndices", name));
            }
            if (mesh.TriangleIndices.Any(index => index >= mesh.Positions.Length))
                errors.Add(LevelText.Get("Import.IndexOutOfRange", name));
            if (mesh.Positions.Any(position => !IsFinite(position)))
                errors.Add(LevelText.Get("Import.NonFinitePosition", name));
            ValidateOptionalChannel(mesh.Normals.Length, mesh.Positions.Length,
                name, LevelText.Get("Import.Normals"), errors);
            ValidateOptionalChannel(mesh.TextureCoordinates.Length, mesh.Positions.Length,
                name, "UV0", errors);
            ValidateOptionalChannel(mesh.SecondaryTextureCoordinates.Length, mesh.Positions.Length,
                name, "UV1", errors);
            ValidateOptionalChannel(mesh.DiffuseColors.Length, mesh.Positions.Length,
                name, LevelText.Get("Import.VertexColors"), errors);
            if (mesh.MaterialIndex < -1 || mesh.MaterialIndex >= scene.Materials.Count)
                errors.Add(LevelText.Get("Import.BadMaterialIndex", name, mesh.MaterialIndex));
        }

        for (int materialIndex = 0;
             materialIndex < scene.Materials.Count;
             materialIndex++)
        {
            ImportedMaterial material = scene.Materials[materialIndex];
            if (material.BaseColorTextureIndex < -1 ||
                material.BaseColorTextureIndex >= scene.Textures.Count)
            {
                errors.Add(LevelText.Get("Import.BadTextureIndex", materialIndex, material.Name,
                    material.BaseColorTextureIndex));
            }
            if (material.UsesTextureAlpha && material.BaseColorTextureIndex < 0)
            {
                warnings.Add(LevelText.Get("Import.AlphaWithoutTexture", material.Name, material.AlphaMode));
            }
            if (material.AlphaMode == ImportedMaterialAlphaMode.Mask)
            {
                warnings.Add(LevelText.Get("Import.MaskAsBlend", material.Name, material.AlphaCutoff));
            }
        }

        for (int textureIndex = 0; textureIndex < scene.Textures.Count; textureIndex++)
        {
            ImportedTexture texture = scene.Textures[textureIndex];
            if (texture.Width <= 0 || texture.Height <= 0 || texture.Data.Length == 0)
                errors.Add(LevelText.Get("Import.EmptyTexture", textureIndex, texture.Name));
        }

        if (purpose == SmoLevelImportPurpose.ReplaceModelResource &&
            expectedMeshCount is null)
        {
            errors.Add(LevelText.Get("Import.MissingExpectedMeshCount"));
        }
        return new SmoLevelImportValidationReport(
            new ReadOnlyCollection<string>(errors.Distinct().ToArray()),
            new ReadOnlyCollection<string>(warnings.Distinct().ToArray()));
    }

    public static void ThrowIfInvalid(SmoLevelImportValidationReport report)
    {
        ArgumentNullException.ThrowIfNull(report);
        if (report.CanImport)
            return;
        throw new InvalidDataException(LevelText.Get("Import.InvalidForLevel", string.Join("\n• ", report.Errors)));
    }

    private static void ValidateOptionalChannel(
        int count,
        int vertexCount,
        string meshName,
        string channel,
        ICollection<string> errors)
    {
        if (count != 0 && count != vertexCount)
            errors.Add(LevelText.Get("Import.ChannelCountMismatch", meshName, channel, count, vertexCount));
    }

    private static bool IsFinite(Vector3 value) =>
        float.IsFinite(value.X) &&
        float.IsFinite(value.Y) &&
        float.IsFinite(value.Z);
}
