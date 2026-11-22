using SixLabors.ImageSharp;

namespace SmoImporter.Core;

/// <summary>
/// Appearance-only recovery at the external-reader boundary. Geometry and skin
/// are never changed; unavailable images keep their logical names for an
/// explicit later assignment, but their bytes cannot reach preview or writers.
/// </summary>
internal static class ImportedTextureRecovery
{
    internal static bool TryReadReferencedFile(
        string path, out ImportedTexture? texture, out string? issue)
    {
        try
        {
            texture = ImportedTextureFileReader.ReadReferencedFile(path);
            issue = null;
            return true;
        }
        catch (Exception exception) when (IsImageFailure(exception) ||
                                          exception is IOException or UnauthorizedAccessException)
        {
            texture = null;
            issue = exception.Message;
            return false;
        }
    }

    internal static ImportedScene Recover(
        ImportedScene scene,
        string format,
        CancellationToken cancellationToken = default,
        IReadOnlyDictionary<int, string>? unavailableReasons = null,
        bool inferTextureAlpha = false)
    {
        ImportedModelResourceLimits.ValidateCount(
            scene.Textures.Count, ImportedModelResourceLimits.MaximumTextures, $"{format} texture");
        var warnings = new List<string>(scene.ImportWarnings);
        var diagnostics = new List<ImportedDiagnostic>(scene.ImportDiagnostics);
        var validTextures = new List<ImportedTexture>();
        var remap = Enumerable.Repeat(-1, scene.Textures.Count).ToArray();
        var imageFailures = new Dictionary<int, string>();
        var transparent = new HashSet<int>();
        long encodedBytes = 0;
        long pixels = 0;
        for (int index = 0; index < scene.Textures.Count; index++)
        {
            cancellationToken.ThrowIfCancellationRequested();
            ImportedTexture texture = scene.Textures[index];
            encodedBytes = checked(encodedBytes + texture.Data.LongLength);
            if (texture.Data.LongLength > ImportedModelResourceLimits.MaximumEncodedTextureBytes ||
                encodedBytes > ImportedModelResourceLimits.MaximumTotalEncodedTextureBytes)
                throw new InvalidDataException($"{format}: каталог текстур превышает безопасный объём; уменьшите изображения.");

            ImageInfo? info;
            try { info = Image.Identify(texture.Data); }
            catch (Exception exception) when (IsImageFailure(exception))
            {
                imageFailures.Add(index, "изображение повреждено или его формат не поддерживается");
                continue;
            }
            if (info is null)
            {
                imageFailures.Add(index, "изображение повреждено или его формат не поддерживается");
                continue;
            }
            // Resource guards are outside codec recovery: a compression bomb
            // remains a visible rejection, rather than a hidden dropped image.
            pixels = ImportedModelResourceLimits.AddTexturePixels(
                pixels, info.Width, info.Height, $"{format} texture '{texture.Name}'");
            if (texture.Width == 0 && texture.Height == 0)
                texture = texture with { Width = info.Width, Height = info.Height };
            try
            {
                bool hasTransparency = ImportedTextureImageTools.TextureContainsTransparency(texture);
                remap[index] = validTextures.Count;
                if (hasTransparency) transparent.Add(validTextures.Count);
                validTextures.Add(texture);
            }
            catch (Exception exception) when (IsImageFailure(exception) || exception is InvalidDataException)
            {
                imageFailures.Add(index, "данные изображения повреждены или не соответствуют его размеру");
            }
        }

        var materials = scene.Materials.ToArray();
        for (int index = 0; index < materials.Length; index++)
        {
            ImportedMaterial material = materials[index];
            int sourceIndex = material.BaseColorTextureIndex;
            string? failure = null;
            string code = "TEXTURE_UNBOUND";
            int textureIndex = -1;
            if (sourceIndex < -1 || sourceIndex >= remap.Length)
            {
                code = "TEXTURE_BINDING";
                failure = $"некорректный индекс текстуры {sourceIndex}";
            }
            else if (sourceIndex >= 0)
            {
                textureIndex = remap[sourceIndex];
                if (textureIndex < 0)
                {
                    code = "TEXTURE_PAYLOAD";
                    failure = imageFailures[sourceIndex];
                }
            }
            else if (unavailableReasons?.TryGetValue(index, out string? reason) == true)
            {
                code = "TEXTURE_MISSING";
                failure = reason;
            }
            else if (!string.IsNullOrWhiteSpace(material.BaseColorTextureName) ||
                     !string.IsNullOrWhiteSpace(material.PreviewTextureUri))
            {
                code = "TEXTURE_MISSING";
                failure = "указанное изображение недоступно";
            }
            if (textureIndex < 0)
            {
                string[] meshes = scene.Meshes.Where(mesh => mesh.MaterialIndex == index)
                    .Select(mesh => $"«{mesh.Name}»").ToArray();
                if (meshes.Length > 0)
                {
                    string meshNames = string.Join(", ", meshes);
                    string message = $"[{code}] {format}: материал «{material.Name}», меш {meshNames}: " +
                        (failure ?? "текстура не назначена") +
                        (string.IsNullOrWhiteSpace(material.BaseColorTextureName) ? "" : $" («{material.BaseColorTextureName}»)") +
                        "; продолжено без текстуры на этих мешах.";
                    warnings.Add(message);
                    diagnostics.Add(new ImportedDiagnostic(code, message,
                        format, material.Name, meshNames, material.BaseColorTextureName ?? string.Empty, sourceIndex)
                    {
                        UntexturedMaterialIndex = index
                    });
                }
            }
            materials[index] = material with
            {
                BaseColorTextureName = material.BaseColorTextureName ??
                    (sourceIndex >= 0 && sourceIndex < scene.Textures.Count ? scene.Textures[sourceIndex].Name : null),
                BaseColorTextureIndex = textureIndex,
                PreviewTextureUri = null,
                AlphaMode = inferTextureAlpha && textureIndex >= 0 && transparent.Contains(textureIndex) &&
                            material.AlphaMode == ImportedMaterialAlphaMode.Opaque
                    ? ImportedMaterialAlphaMode.Blend : material.AlphaMode
            };
        }
        foreach (ImportedMesh mesh in scene.Meshes)
        {
            if (mesh.MaterialIndex == -1)
            {
                string message = $"[TEXTURE_UNBOUND] {format}: меш «{mesh.Name}»: материал и текстура не назначены; продолжено без текстуры.";
                warnings.Add(message);
                diagnostics.Add(new ImportedDiagnostic("MeshWithoutMaterial", message, format, mesh.Name));
            }
            else if (mesh.MaterialIndex < -1 || mesh.MaterialIndex >= materials.Length)
                throw new InvalidDataException($"{format}: меш «{mesh.Name}» ссылается на отсутствующий материал {mesh.MaterialIndex}; исправьте модель.");
        }
        return scene with
        {
            EmbeddedTextures = validTextures.AsReadOnly(),
            SourceMaterials = Array.AsReadOnly(materials),
            ImportWarnings = warnings.AsReadOnly(),
            ImportDiagnostics = diagnostics.AsReadOnly()
        };
    }

    private static bool IsImageFailure(Exception exception) => exception is
        UnknownImageFormatException or InvalidImageContentException or NotSupportedException;
}
