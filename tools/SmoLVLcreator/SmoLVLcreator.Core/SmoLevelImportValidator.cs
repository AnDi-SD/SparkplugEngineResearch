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
    public static SmoLevelImportValidationReport Validate(
        ImportedScene scene,
        SmoLevelImportPurpose purpose,
        int? expectedMeshCount = null)
    {
        ArgumentNullException.ThrowIfNull(scene);
        var errors = new List<string>();
        var warnings = new List<string>(scene.ImportWarnings);
        if (scene.Meshes.Count == 0)
            errors.Add("Модель не содержит мешей.");
        if (expectedMeshCount is int expected && scene.Meshes.Count != expected)
        {
            errors.Add(
                $"Ресурс SMO содержит {expected} mesh-частей, а импортируемая " +
                $"модель — {scene.Meshes.Count}. Для ресурсной замены число " +
                "частей должно совпадать.");
        }
        if (scene.HasSkinning)
        {
            errors.Add(
                "Уровни принимают только rigid-модели. Skin/joints/weights " +
                "нужно удалить или запечь во внешнем редакторе.");
        }

        for (int meshIndex = 0; meshIndex < scene.Meshes.Count; meshIndex++)
        {
            ImportedMesh mesh = scene.Meshes[meshIndex];
            string name = string.IsNullOrWhiteSpace(mesh.Name)
                ? $"mesh {meshIndex}"
                : $"mesh {meshIndex} «{mesh.Name}»";
            if (mesh.Positions.Length < 3)
                errors.Add($"{name}: меньше трёх вершин.");
            if (mesh.TriangleIndices.Length == 0 ||
                mesh.TriangleIndices.Length % 3 != 0)
            {
                errors.Add($"{name}: индексный буфер не образует треугольники.");
            }
            if (mesh.TriangleIndices.Any(index => index >= mesh.Positions.Length))
                errors.Add($"{name}: индекс выходит за границы массива вершин.");
            if (mesh.Positions.Any(position => !IsFinite(position)))
                errors.Add($"{name}: координаты содержат NaN или Infinity.");
            ValidateOptionalChannel(mesh.Normals.Length, mesh.Positions.Length,
                name, "нормали", errors);
            ValidateOptionalChannel(mesh.TextureCoordinates.Length, mesh.Positions.Length,
                name, "UV0", errors);
            ValidateOptionalChannel(mesh.DiffuseColors.Length, mesh.Positions.Length,
                name, "vertex colors", errors);
            if (mesh.MaterialIndex < -1 || mesh.MaterialIndex >= scene.Materials.Count)
                errors.Add($"{name}: неверный индекс материала {mesh.MaterialIndex}.");
        }

        for (int materialIndex = 0;
             materialIndex < scene.Materials.Count;
             materialIndex++)
        {
            ImportedMaterial material = scene.Materials[materialIndex];
            if (material.BaseColorTextureIndex < -1 ||
                material.BaseColorTextureIndex >= scene.Textures.Count)
            {
                errors.Add(
                    $"Материал {materialIndex} «{material.Name}» ссылается на " +
                    $"несуществующую текстуру {material.BaseColorTextureIndex}.");
            }
            if (material.UsesTextureAlpha && material.BaseColorTextureIndex < 0)
            {
                warnings.Add(
                    $"Материал «{material.Name}» использует {material.AlphaMode}, " +
                    "но не имеет base-color текстуры; прозрачность в SMO будет неприменима.");
            }
            if (material.AlphaMode == ImportedMaterialAlphaMode.Mask)
            {
                warnings.Add(
                    $"Материал «{material.Name}»: MASK {material.AlphaCutoff:0.###} " +
                    "будет записан через подтверждённый alpha-blend путь движка; " +
                    "отдельный cutoff в SMO не подтверждён.");
            }
        }

        for (int textureIndex = 0; textureIndex < scene.Textures.Count; textureIndex++)
        {
            ImportedTexture texture = scene.Textures[textureIndex];
            if (texture.Width <= 0 || texture.Height <= 0 || texture.Data.Length == 0)
                errors.Add($"Текстура {textureIndex} «{texture.Name}» пуста.");
        }

        if (purpose == SmoLevelImportPurpose.ReplaceModelResource &&
            expectedMeshCount is null)
        {
            errors.Add("Для ресурсной замены не задано число mesh-частей SMO.");
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
        throw new InvalidDataException(
            "Модель не подходит для уровня:\n• " +
            string.Join("\n• ", report.Errors));
    }

    private static void ValidateOptionalChannel(
        int count,
        int vertexCount,
        string meshName,
        string channel,
        ICollection<string> errors)
    {
        if (count != 0 && count != vertexCount)
            errors.Add($"{meshName}: канал {channel} содержит {count} значений для {vertexCount} вершин.");
    }

    private static bool IsFinite(Vector3 value) =>
        float.IsFinite(value.X) &&
        float.IsFinite(value.Y) &&
        float.IsFinite(value.Z);
}
