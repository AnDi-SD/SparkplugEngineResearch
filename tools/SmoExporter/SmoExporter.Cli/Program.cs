using SmoExporter.Core;
using SmoViewer.Core;

if (args.Length == 0 || args.Contains("--help"))
{
    Console.WriteLine("Usage: smo-export <input.smo> [--output directory] [--glb] [--fbx] [--obj] [--animation clip.san]...");
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
string[] animations = objOnly ? [] : requestedAnimations;
SmoExportResourceTypes resources = objOnly
    ? SmoExportResourceTypes.Meshes |
      SmoExportResourceTypes.Materials |
      SmoExportResourceTypes.Textures
    : SmoExportResourceTypes.All;

try
{
    SmoDocument document = SmoDocument.Load(input);
    SmoExportScene scene = SmoSceneBuilder.Build(
        document, new SmoExportOptions(
            AnimationPaths: animations,
            Resources: resources));
    Directory.CreateDirectory(outputDirectory);
    string stem = Path.GetFileNameWithoutExtension(input);
    if (glb)
    {
        string path = Path.Combine(outputDirectory, stem + ".glb");
        GlbExporter.Export(scene, path);
        Console.WriteLine($"GLB: {path}");
    }
    if (obj)
    {
        string path = Path.Combine(outputDirectory, stem + ".obj");
        ObjExporter.Export(scene, path);
        Console.WriteLine($"OBJ: {path}");
    }
    if (fbx)
    {
        string path = Path.Combine(outputDirectory, stem + ".fbx");
        FbxExporter.Export(scene, path);
        Console.WriteLine($"FBX: {path}");
    }
    Console.WriteLine($"Meshes: {scene.Meshes.Count}; warnings: {scene.Warnings.Count}");
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
