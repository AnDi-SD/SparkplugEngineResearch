using System.Diagnostics;
using SmoExporter.Core;

namespace SmoImporter.Core;

/// <summary>
/// Converts FBX to an internal temporary GLB through Blender, then delegates to
/// the GLB reader. The normal path prefers meshes driven by an armature when one
/// exists. The rigid-bundle path exports all mesh objects, then its material
/// resolver keeps only unskinned matN primitives and reports unrelated helpers.
/// </summary>
public static class FbxModelReader
{
    public static ImportedScene Read(string path, string? blenderPath = null)
        => ReadCore(
            path, blenderPath, includeAllRigidMeshes: false, ignoreSkinning: false);

    public static ImportedScene ReadRigid(string path, string? blenderPath = null)
        => ReadCore(
            path, blenderPath, includeAllRigidMeshes: true, ignoreSkinning: false);

    public static ImportedScene ReadGeometryOnly(
        string path,
        string? blenderPath = null) =>
        ReadCore(
            path, blenderPath, includeAllRigidMeshes: true, ignoreSkinning: true);

    private static ImportedScene ReadCore(
        string path,
        string? blenderPath,
        bool includeAllRigidMeshes,
        bool ignoreSkinning)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(path);
        string fullInput = Path.GetFullPath(path);
        if (!File.Exists(fullInput))
            throw new FileNotFoundException("FBX model was not found.", fullInput);
        string? blender = FbxExporter.FindBlenderExecutable(blenderPath);
        if (blender is null)
            throw new InvalidOperationException(
                "Для импорта FBX нужен Blender. Установите Blender, добавьте blender.exe " +
                "в PATH/BLENDER_PATH либо укажите его вручную.");

        string temporaryDirectory = Path.Combine(
            Path.GetTempPath(), "smo-import-fbx-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(temporaryDirectory);
        try
        {
            string glbPath = Path.Combine(temporaryDirectory, "converted.glb");
            Convert(fullInput, glbPath, blender, includeAllRigidMeshes);
            return ignoreSkinning
                ? GlbModelReader.ReadGeometryOnly(glbPath)
                : GlbModelReader.Read(glbPath);
        }
        finally
        {
            try { Directory.Delete(temporaryDirectory, recursive: true); }
            catch { }
        }
    }

    private static void Convert(
        string inputPath,
        string outputPath,
        string blenderPath,
        bool includeAllRigidMeshes)
    {
        string expression =
            "import bpy;" +
            "bpy.ops.wm.read_factory_settings(use_empty=True);" +
            $"bpy.ops.import_scene.fbx(filepath={PythonString(inputPath)}," +
            "use_anim=False,use_image_search=True,ignore_leaf_bones=True);" +
            "all_meshes=[o for o in bpy.context.scene.objects if o.type=='MESH'];" +
            "skinned=[o for o in all_meshes if any(m.type=='ARMATURE' and m.object for m in o.modifiers)];" +
            $"meshes=all_meshes if {(includeAllRigidMeshes ? "True" : "False")} else (skinned if skinned else all_meshes);" +
            "assert meshes,'FBX contains no mesh objects';" +
            "arms={m.object for o in meshes for m in o.modifiers " +
            "if m.type=='ARMATURE' and m.object};" +
            "bpy.ops.object.select_all(action='DESELECT');" +
            "[(o.select_set(True)) for o in meshes+list(arms)];" +
            "bpy.context.view_layer.objects.active=meshes[0];" +
            $"bpy.ops.export_scene.gltf(filepath={PythonString(outputPath)}," +
            "export_format='GLB',use_selection=True,export_skins=True," +
            "export_all_influences=False,export_influence_nb=4," +
            "export_def_bones=False,export_leaf_bone=False,export_animations=False," +
            "export_materials='EXPORT',export_image_format='AUTO'," +
            "export_vertex_color='ACTIVE',export_all_vertex_colors=True," +
            "export_yup=True,export_apply=False);";

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
            throw new InvalidOperationException("Не удалось запустить Blender для импорта FBX.");
        Task<string> stdoutTask = process.StandardOutput.ReadToEndAsync();
        Task<string> stderrTask = process.StandardError.ReadToEndAsync();
        process.WaitForExit();
        string stdout = stdoutTask.GetAwaiter().GetResult();
        string stderr = stderrTask.GetAwaiter().GetResult();
        if (process.ExitCode != 0 || !File.Exists(outputPath))
            throw new InvalidOperationException(
                $"Blender FBX import failed ({process.ExitCode}). " +
                (string.IsNullOrWhiteSpace(stderr) ? stdout : stderr));
        if (new FileInfo(outputPath).Length < 20)
            throw new InvalidDataException("Blender создал пустой или неполный GLB из FBX.");
    }

    private static string PythonString(string value) =>
        "r\"" + value.Replace("\"", "\\\"") + "\"";
}
