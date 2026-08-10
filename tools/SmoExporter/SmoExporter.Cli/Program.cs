using SmoExporter.Core;
using SmoViewer.Core;

if (args.Length == 0 || args.Contains("--help"))
{
    Console.WriteLine("Usage: smo-export <input.smo> [--output directory] [--glb] [--obj]");
    Console.WriteLine("Without format switches both GLB and OBJ are exported.");
    return args.Length == 0 ? 2 : 0;
}

string input = Path.GetFullPath(args[0]);
string outputDirectory = GetOption(args, "--output") is string requested
    ? Path.GetFullPath(requested)
    : Path.Combine(Path.GetDirectoryName(input)!, Path.GetFileNameWithoutExtension(input) + "_export");
bool glb = args.Contains("--glb");
bool obj = args.Contains("--obj");
if (!glb && !obj) glb = obj = true;

try
{
    SmoDocument document = SmoDocument.Load(input);
    SmoExportScene scene = SmoSceneBuilder.Build(document);
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

static string? GetOption(string[] values, string name)
{
    int index = Array.IndexOf(values, name);
    if (index < 0) return null;
    if (index == values.Length - 1)
        throw new ArgumentException($"Missing value after {name}.");
    return values[index + 1];
}
