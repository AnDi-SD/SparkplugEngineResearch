using System.Diagnostics;

namespace SmoExporter.Core;

/// <summary>
/// Produces binary FBX through Blender's maintained FBX exporter. The common
/// scene is first serialized as lossless GLB, so skeleton, weights, materials,
/// textures and selected animations share one implementation.
/// </summary>
public static class FbxExporter
{
    public static void Export(SmoExportScene scene, string outputPath, string? blenderPath = null)
    {
        ArgumentNullException.ThrowIfNull(scene);
        ArgumentException.ThrowIfNullOrWhiteSpace(outputPath);
        blenderPath ??= FindBlenderExecutable();
        if (blenderPath is null)
            throw new InvalidOperationException(
                "Для бинарного FBX нужен Blender. Установите Blender или добавьте blender.exe в PATH.");

        string fullOutput = Path.GetFullPath(outputPath);
        Directory.CreateDirectory(Path.GetDirectoryName(fullOutput)!);
        string stagedOutput = Path.Combine(
            Path.GetDirectoryName(fullOutput)!,
            $".{Path.GetFileNameWithoutExtension(fullOutput)}.{Guid.NewGuid():N}.tmp.fbx");
        string temporaryDirectory = Path.Combine(
            Path.GetTempPath(), "smo-fbx-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(temporaryDirectory);
        try
        {
            string glb = Path.Combine(temporaryDirectory, "scene.glb");
            GlbExporter.Export(scene, glb);
            string expression =
                "import bpy;" +
                "bpy.ops.wm.read_factory_settings(use_empty=True);" +
                $"bpy.ops.import_scene.gltf(filepath={PythonString(glb)});" +
                $"bpy.ops.export_scene.fbx(filepath={PythonString(stagedOutput)}," +
                "use_selection=False,object_types={'EMPTY','ARMATURE','MESH'}," +
                "apply_unit_scale=True,apply_scale_options='FBX_SCALE_ALL'," +
                "use_mesh_modifiers=True,mesh_smooth_type='FACE',colors_type='SRGB'," +
                "use_armature_deform_only=False,add_leaf_bones=False," +
                "bake_anim=True,bake_anim_use_all_bones=True,bake_anim_use_nla_strips=True," +
                "bake_anim_use_all_actions=True,bake_anim_force_startend_keying=True," +
                "bake_anim_step=1.0,bake_anim_simplify_factor=0.0," +
                "path_mode='COPY',embed_textures=True,use_custom_props=True);";

            var start = new ProcessStartInfo(blenderPath)
            {
                UseShellExecute = false,
                CreateNoWindow = true,
                RedirectStandardOutput = true,
                RedirectStandardError = true
            };
            start.ArgumentList.Add("--background");
            start.ArgumentList.Add("--python-expr");
            start.ArgumentList.Add(expression);
            using Process process = Process.Start(start) ??
                throw new InvalidOperationException("Не удалось запустить Blender.");
            Task<string> stdoutTask = process.StandardOutput.ReadToEndAsync();
            Task<string> stderrTask = process.StandardError.ReadToEndAsync();
            process.WaitForExit();
            string stdout = stdoutTask.GetAwaiter().GetResult();
            string stderr = stderrTask.GetAwaiter().GetResult();
            if (process.ExitCode != 0 || !File.Exists(stagedOutput))
                throw new InvalidOperationException(
                    $"Blender FBX export failed ({process.ExitCode}). " +
                    (string.IsNullOrWhiteSpace(stderr) ? stdout : stderr));
            if (new FileInfo(stagedOutput).Length < 27)
                throw new InvalidDataException("Blender создал пустой или неполный FBX.");

            // Blender writes FBX progressively. Keep that incomplete file hidden
            // under a temporary name and publish it only after Blender exits.
            File.Move(stagedOutput, fullOutput, overwrite: true);
        }
        finally
        {
            try { File.Delete(stagedOutput); }
            catch { }
            try { Directory.Delete(temporaryDirectory, true); }
            catch { }
        }
    }

    public static string? FindBlenderExecutable()
    {
        string? path = Environment.GetEnvironmentVariable("PATH")?.Split(Path.PathSeparator)
            .Select(directory => Path.Combine(directory, "blender.exe"))
            .FirstOrDefault(File.Exists);
        if (path is not null) return path;
        string programFiles = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles);
        string root = Path.Combine(programFiles, "Blender Foundation");
        if (!Directory.Exists(root)) return null;
        return Directory.EnumerateFiles(root, "blender.exe", SearchOption.AllDirectories)
            .OrderByDescending(value => value, StringComparer.OrdinalIgnoreCase).FirstOrDefault();
    }

    private static string PythonString(string value) =>
        "r\"" + value.Replace("\"", "\\\"") + "\"";
}
