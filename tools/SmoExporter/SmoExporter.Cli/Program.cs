using SmoExporter.Core;
using SmoViewer.Core;

if (args.Length == 0 || args.Contains("--help"))
{
    Console.WriteLine("Usage: smo-export <input.smo> [--output directory] [--glb] [--fbx] [--obj] [--animation clip.san]...");
    Console.WriteLine("Level modes: --level-only | --bake-instances | --preserve-instances | --mesh objectIndex ...");
    Console.WriteLine("Without format switches both GLB and OBJ are exported.");
    return args.Length == 0 ? 2 : 0;
}

string input = Path.GetFullPath(args[0]);
string outputDirectory = GetOption(args, "--output") is string requested
    ? Path.GetFullPath(requested)
    : Path.Combine(Path.GetDirectoryName(input)!, Path.GetFileNameWithoutExtension(input) + "_export");
bool glb = args.Contains("--glb");
bool obj = args.Contains("--obj");
bool fbx = args.Contains("--fbx");
if (!glb && !obj && !fbx) glb = obj = true;
bool objOnly = obj && !glb && !fbx;
string[] requestedAnimations = GetOptions(args, "--animation")
    .Select(Path.GetFullPath).ToArray();
int[] selectedMeshes = GetOptions(args, "--mesh")
    .Select(value => int.TryParse(value, out int index) && index >= 0
        ? index
        : throw new ArgumentException($"Invalid mesh object index: {value}"))
    .Distinct().ToArray();
int requestedLevelModes = new[]
{
    args.Contains("--level-only"),
    args.Contains("--bake-instances"),
    args.Contains("--preserve-instances"),
    selectedMeshes.Length > 0
}.Count(value => value);
if (requestedLevelModes > 1)
    throw new ArgumentException("Select only one level export mode.");
SmoExportSceneMode sceneMode = selectedMeshes.Length > 0
    ? SmoExportSceneMode.SeparateMeshes
    : args.Contains("--level-only")
        ? SmoExportSceneMode.LevelOnly
        : args.Contains("--bake-instances")
            ? SmoExportSceneMode.LevelWithBakedObjects
            : args.Contains("--preserve-instances")
                ? SmoExportSceneMode.LevelWithInstances
                : SmoExportSceneMode.All;
if (sceneMode == SmoExportSceneMode.LevelWithInstances && obj)
    throw new ArgumentException(
        "OBJ cannot preserve instances. Use GLB/FBX or --bake-instances.");
string[] animations = objOnly ? [] : requestedAnimations;
SmoExportResourceTypes resources = objOnly
    ? SmoExportResourceTypes.Meshes |
      SmoExportResourceTypes.Materials |
      SmoExportResourceTypes.Textures
    : SmoExportResourceTypes.All;

try
{
    SmoDocument document = SmoDocument.Load(input);
    SmoExportContentProfile profile = SmoExportContentProfileAnalyzer.Analyze(document);
    SmoExportScene scene = SmoSceneBuilder.Build(
        document, new SmoExportOptions(
            AnimationPaths: animations,
            Resources: resources,
            SceneMode: sceneMode,
            SelectedMeshObjectIndices: selectedMeshes.ToHashSet()));
    Directory.CreateDirectory(outputDirectory);
    string stem = Path.GetFileNameWithoutExtension(input);
    void ExportFormats(SmoExportScene exportScene, string fileStem)
    {
        if (glb)
        {
            string path = Path.Combine(outputDirectory, fileStem + ".glb");
            GlbExporter.Export(exportScene, path);
            Console.WriteLine($"GLB: {path}");
        }
        if (obj)
        {
            string path = Path.Combine(outputDirectory, fileStem + ".obj");
            ObjExporter.Export(exportScene, path);
            Console.WriteLine($"OBJ: {path}");
        }
        if (fbx)
        {
            string path = Path.Combine(outputDirectory, fileStem + ".fbx");
            FbxExporter.Export(exportScene, path);
            Console.WriteLine($"FBX: {path}");
            foreach (string note in FbxExporter.GetConversionNotes(exportScene))
                Console.WriteLine(note);
        }
    }
    if (sceneMode == SmoExportSceneMode.SeparateMeshes)
    {
        foreach (int meshObjectIndex in selectedMeshes)
        {
            SmoExportScene single = SmoExportSceneSplitter.CreateSingleMeshScene(
                scene, meshObjectIndex);
            SmoExportMesh mesh = single.Meshes[0];
            ExportFormats(single, $"{stem}_{SafeFileName(mesh.Name)}_{mesh.ObjectIndex}");
        }
    }
    else
    {
        ExportFormats(scene, stem);
    }
    Console.WriteLine($"Detected: {profile.Kind} ({profile.Reason})");
    Console.WriteLine($"Meshes: {scene.Meshes.Count}; placements: " +
                      $"{scene.MeshPlacements.Count}; warnings: {scene.Warnings.Count}");
    foreach (string warning in scene.Warnings)
        Console.Error.WriteLine($"warning: {warning}");
    return scene.Meshes.Count == 0 ? 1 : 0;
}

catch (Exception exception)
{
    Console.Error.WriteLine(exception.Message);
    return 1;
}

static IEnumerable<string> GetOptions(string[] values, string name)
{
    for (int index = 0; index < values.Length; index++)
        if (values[index].Equals(name, StringComparison.OrdinalIgnoreCase))
        {
            if (index == values.Length - 1)
                throw new ArgumentException($"Missing value after {name}.");
            yield return values[index + 1];
        }
}

static string? GetOption(string[] values, string name)
{
    int index = Array.IndexOf(values, name);
    if (index < 0) return null;
    if (index == values.Length - 1)
        throw new ArgumentException($"Missing value after {name}.");
    return values[index + 1];
}

static string SafeFileName(string value)
{
    HashSet<char> invalid = Path.GetInvalidFileNameChars().ToHashSet();
    string result = new(value.Select(character =>
        invalid.Contains(character) || char.IsControl(character) ? '_' : character)
        .ToArray());
    result = result.Trim().TrimEnd('.');
    return string.IsNullOrWhiteSpace(result) ? "mesh" : result;
}
