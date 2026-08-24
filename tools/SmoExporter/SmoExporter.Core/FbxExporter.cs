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

    private static void ValidateAlphaCompatibility(SmoExportScene scene)
    {
        bool includeMaterials =
            (scene.Resources & SmoExportResourceTypes.Materials) != 0;
        bool includeTextures = includeMaterials &&
            (scene.Resources & SmoExportResourceTypes.Textures) != 0;
        foreach (SmoExportMesh mesh in scene.Meshes)
        {
            if (mesh.Colors.Length == mesh.Positions.Length)
            {
                foreach (Vector4 color in mesh.Colors)
                {
                    ValidateAlpha(color.W, "COLOR_0", mesh);
                    if (color.W < 1f)
                    {
                        throw new InvalidDataException(
                            $"FBX cannot preserve COLOR_0 alpha on mesh " +
                            $"[{mesh.ObjectIndex}] {mesh.Name}; export this model as GLB instead.");
                    }
                }
            }
            if (!includeMaterials) continue;
            ValidateAlpha(mesh.MaterialColor.W, "material factor", mesh);
            if (includeTextures && mesh.Texture?.OpacityMaskPngBytes is not null &&
                mesh.MaterialColor.W < 1f)
            {
                throw new InvalidDataException(
                    $"FBX cannot preserve both texture alpha and a translucent material " +
                    $"factor on mesh [{mesh.ObjectIndex}] {mesh.Name}; " +
                    "export this model as GLB instead.");
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
