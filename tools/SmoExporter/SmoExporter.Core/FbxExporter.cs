using System.Diagnostics;
using Microsoft.Win32;

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

    /// <summary>
    /// Resolves either a direct blender.exe path or a Blender installation
    /// directory. Environment variables and surrounding quotes are accepted.
    /// </summary>
    public static string? ResolveBlenderExecutable(string? path)
    {
        if (string.IsNullOrWhiteSpace(path))
            return null;

        string candidate = Environment.ExpandEnvironmentVariables(path.Trim().Trim('"'));
        int executableEnd = candidate.IndexOf(".exe", StringComparison.OrdinalIgnoreCase);
        if (executableEnd >= 0)
            candidate = candidate[..(executableEnd + 4)];

        try
        {
            if (Directory.Exists(candidate))
                candidate = Path.Combine(candidate, "blender.exe");
            if (!File.Exists(candidate) ||
                !Path.GetFileName(candidate).Equals("blender.exe", StringComparison.OrdinalIgnoreCase))
                return null;
            return Path.GetFullPath(candidate);
        }
        catch (Exception exception) when (exception is ArgumentException or NotSupportedException or PathTooLongException)
        {
            return null;
        }
    }

    public static string? FindBlenderExecutable(string? preferredPath = null)
    {
        string? resolved = ResolveBlenderExecutable(preferredPath);
        if (resolved is not null)
            return resolved;

        resolved = ResolveBlenderExecutable(Environment.GetEnvironmentVariable("BLENDER_PATH"));
        if (resolved is not null)
            return resolved;

        foreach (string directory in (Environment.GetEnvironmentVariable("PATH") ?? string.Empty)
                     .Split(Path.PathSeparator, StringSplitOptions.RemoveEmptyEntries))
        {
            resolved = ResolveBlenderExecutable(directory);
            if (resolved is not null)
                return resolved;
        }

        foreach (string candidate in EnumerateRegistryCandidates())
        {
            resolved = ResolveBlenderExecutable(candidate);
            if (resolved is not null)
                return resolved;
        }

        var discovered = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (string root in GetCommonInstallationRoots())
        {
            if (!Directory.Exists(root))
                continue;
            try
            {
                foreach (string candidate in Directory.EnumerateFiles(
                             root, "blender.exe", SearchOption.AllDirectories))
                    discovered.Add(Path.GetFullPath(candidate));
            }
            catch (Exception exception) when (exception is UnauthorizedAccessException or IOException or System.Security.SecurityException)
            {
                // One inaccessible installation root must not disable FBX when
                // another valid Blender installation is available.
            }
        }

        return discovered
            .OrderByDescending(GetBlenderVersion)
            .ThenByDescending(path => path, StringComparer.OrdinalIgnoreCase)
            .FirstOrDefault();
    }

    private static IEnumerable<string> GetCommonInstallationRoots()
    {
        string programFiles = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles);
        string programFilesX86 = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86);
        string localApplicationData = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        if (!string.IsNullOrWhiteSpace(programFiles))
            yield return Path.Combine(programFiles, "Blender Foundation");
        if (!string.IsNullOrWhiteSpace(programFilesX86))
            yield return Path.Combine(programFilesX86, "Blender Foundation");
        if (!string.IsNullOrWhiteSpace(localApplicationData))
        {
            yield return Path.Combine(localApplicationData, "Programs", "Blender Foundation");
            yield return Path.Combine(localApplicationData, "Programs", "Blender");
        }
    }

    private static IReadOnlyList<string> EnumerateRegistryCandidates()
    {
        if (!OperatingSystem.IsWindows())
            return [];

        var candidates = new List<string>();
        foreach (RegistryHive hive in new[] { RegistryHive.CurrentUser, RegistryHive.LocalMachine })
        foreach (RegistryView view in new[] { RegistryView.Registry64, RegistryView.Registry32 })
        {
            try
            {
                using RegistryKey baseKey = RegistryKey.OpenBaseKey(hive, view);
                using (RegistryKey? appPath = baseKey.OpenSubKey(
                           @"SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\blender.exe"))
                {
                    AddRegistryValue(candidates, appPath?.GetValue(null));
                    AddRegistryValue(candidates, appPath?.GetValue("Path"));
                }

                using RegistryKey? uninstall = baseKey.OpenSubKey(
                    @"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall");
                if (uninstall is null)
                    continue;
                foreach (string subKeyName in uninstall.GetSubKeyNames())
                {
                    using RegistryKey? application = uninstall.OpenSubKey(subKeyName);
                    string? displayName = application?.GetValue("DisplayName") as string;
                    if (displayName?.StartsWith("Blender", StringComparison.OrdinalIgnoreCase) != true)
                        continue;
                    AddRegistryValue(candidates, application?.GetValue("InstallLocation"));
                    AddRegistryValue(candidates, application?.GetValue("DisplayIcon"));
                }
            }
            catch (Exception exception) when (exception is UnauthorizedAccessException or IOException or System.Security.SecurityException)
            {
                // Registry discovery is best-effort; PATH and manual selection
                // remain available on restricted systems.
            }
        }
        return candidates;
    }

    private static void AddRegistryValue(ICollection<string> candidates, object? value)
    {
        if (value is string text && !string.IsNullOrWhiteSpace(text))
            candidates.Add(text);
    }

    private static Version GetBlenderVersion(string path)
    {
        try
        {
            FileVersionInfo info = FileVersionInfo.GetVersionInfo(path);
            return new Version(
                Math.Max(0, info.FileMajorPart), Math.Max(0, info.FileMinorPart),
                Math.Max(0, info.FileBuildPart), Math.Max(0, info.FilePrivatePart));
        }
        catch
        {
            return new Version(0, 0);
        }
    }

    private static string PythonString(string value) =>
        "r\"" + value.Replace("\"", "\\\"") + "\"";
}
