using System.Numerics;

namespace SmoExporter.Core;

/// <summary>
/// Writes binary FBX directly through the bundled Autodesk FBX SDK bridge.
/// Blender, GLB conversion and Python are deliberately absent from this path.
/// </summary>
public static class FbxExporter
{
    public static void Export(
        SmoExportScene scene,
        string outputPath,
        string? nativeBridgePath = null)
    {
        ArgumentNullException.ThrowIfNull(scene);
        ArgumentException.ThrowIfNullOrWhiteSpace(outputPath);
        ValidateAlphaCompatibility(scene);

        string fullOutput = Path.GetFullPath(outputPath);
        string outputDirectory = Path.GetDirectoryName(fullOutput)!;
        Directory.CreateDirectory(outputDirectory);
        string stagedOutput = Path.Combine(
            outputDirectory,
            $".{Path.GetFileNameWithoutExtension(fullOutput)}.{Guid.NewGuid():N}.tmp.fbx");
        string temporaryDirectory = Path.Combine(
            Path.GetTempPath(), "smo-export-fbx-native-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(temporaryDirectory);
        try
        {
            string payload = Path.Combine(temporaryDirectory, "scene.bin");
            FbxExportPayloadWriter.Write(scene, payload);
            NativeFbxBridge.Run(
                "export", [payload, stagedOutput], nativeBridgePath);
            if (!File.Exists(stagedOutput) || new FileInfo(stagedOutput).Length < 27)
                throw new InvalidDataException(
                    "Нативный модуль FBX создал пустой или неполный файл.");
            File.Move(stagedOutput, fullOutput, overwrite: true);
        }
        finally
        {
            try { File.Delete(stagedOutput); }
            catch { }
            try { Directory.Delete(temporaryDirectory, recursive: true); }
            catch { }
        }
    }

    public static string? FindNativeBridgeExecutable(string? preferredPath = null) =>
        NativeFbxBridge.ResolveExecutable(preferredPath);

    public static IReadOnlyList<string> GetConversionNotes(SmoExportScene scene)
    {
        ArgumentNullException.ThrowIfNull(scene);
        int count = scene.Meshes.Count(mesh => RequiresTextureAlphaBake(scene, mesh));
        return count == 0 ? [] :
        [
            $"FBX: совместная alpha текстуры и материала для {count} мешей " +
            "сохраняется в 16-битной карте; погрешность alpha не более 0,000008. " +
            "Совместимость проверена в Blender 4.5."
        ];
    }

    internal static bool RequiresTextureAlphaBake(SmoExportScene scene, SmoExportMesh mesh) =>
        (scene.Resources & (SmoExportResourceTypes.Materials | SmoExportResourceTypes.Textures)) ==
        (SmoExportResourceTypes.Materials | SmoExportResourceTypes.Textures) &&
        mesh.UsesAlphaBlend && mesh.Texture?.OpacityMaskPngBytes is not null &&
        mesh.MaterialColor.W < 1f;

    private static void ValidateAlphaCompatibility(SmoExportScene scene)
    {
        bool includeMaterials =
            (scene.Resources & SmoExportResourceTypes.Materials) != 0;
        foreach (SmoExportMesh mesh in scene.Meshes)
        {
            if (mesh.Colors.Length == mesh.Positions.Length)
            {
                foreach (Vector4 color in mesh.Colors)
                    ValidateAlpha(color.W, "COLOR_0", mesh);
            }
            if (!includeMaterials) continue;
            ValidateAlpha(mesh.MaterialColor.W, "material factor", mesh);
            if (RequiresTextureAlphaBake(scene, mesh) && mesh.Texture is { } texture &&
                (texture.Width is <= 0 or > 16384 || texture.Height is <= 0 or > 16384 ||
                 (long)texture.Width * texture.Height * 4 != texture.Bgra32Pixels.Length))
            {
                throw new InvalidDataException(
                    $"FBX combined alpha on mesh [{mesh.ObjectIndex}] {mesh.Name} " +
                    "requires the original BGRA32 pixels. Build the export scene " +
                    "with SmoSceneBuilder or supply the texture pixel buffer.");
            }
        }
    }

    private static void ValidateAlpha(float alpha, string source, SmoExportMesh mesh)
    {
        if (!float.IsFinite(alpha) || alpha is < 0f or > 1f)
        {
            throw new InvalidDataException(
                $"FBX mesh [{mesh.ObjectIndex}] {mesh.Name} has an invalid " +
                $"{source} alpha value ({alpha}).");
        }
    }
}
