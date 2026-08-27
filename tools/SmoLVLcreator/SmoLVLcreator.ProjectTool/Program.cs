using SmoLVLcreator.Core;
using SmoViewer.Core;
using System.Globalization;
using System.Numerics;

namespace SmoLVLcreator.ProjectTool;

internal static class Program
{
    public static int Main(string[] args)
    {
        try
        {
            if (args.Length == 3 && Is(args[0], "import"))
            {
                Import(args[1], args[2]);
                return 0;
            }
            if (args.Length == 3 && Is(args[0], "build"))
            {
                Build(args[1], args[2]);
                return 0;
            }
            if (args.Length == 4 && Is(args[0], "roundtrip"))
            {
                RoundTrip(args[1], args[2], args[3]);
                return 0;
            }
            if (args.Length == 2 && Is(args[0], "placements"))
            {
                ListPlacements(args[1]);
                return 0;
            }
            if (args.Length == 2 && Is(args[0], "missing-transforms"))
            {
                ListMissingTransforms(args[1]);
                return 0;
            }
            if (args.Length == 2 && Is(args[0], "reference-templates"))
            {
                ListReferenceTemplates(args[1]);
                return 0;
            }
            if (args.Length == 8 && Is(args[0], "clone-reference"))
            {
                CloneReferencePlacement(
                    args[1],
                    ParseObjectIndex(args[2]),
                    new Vector3(
                        ParseSingle(args[3]),
                        ParseSingle(args[4]),
                        ParseSingle(args[5])),
                    args[6],
                    args[7]);
                return 0;
            }
            if (args.Length == 4 && Is(args[0], "remove-branch"))
            {
                RemoveBranch(
                    args[1],
                    ParseObjectIndex(args[2]),
                    args[3]);
                return 0;
            }
            if (args.Length == 4 && Is(args[0], "remove-branch-preserve"))
            {
                RemoveBranchPreservingSharedResources(
                    args[1],
                    ParseObjectIndex(args[2]),
                    args[3]);
                return 0;
            }
            if (args.Length == 7 && Is(args[0], "translate"))
            {
                Translate(
                    args[1],
                    ParseObjectIndex(args[2]),
                    new Vector3(
                        ParseSingle(args[3]),
                        ParseSingle(args[4]),
                        ParseSingle(args[5])),
                    args[6]);
                return 0;
            }
            if (args.Length == 8 && Is(args[0], "set-vector"))
            {
                SetVector(
                    args[1],
                    ParseObjectIndex(args[2]),
                    args[3],
                    new Vector3(
                        ParseSingle(args[4]),
                        ParseSingle(args[5]),
                        ParseSingle(args[6])),
                    args[7]);
                return 0;
            }
            if (args.Length == 9 && Is(args[0], "set-quaternion"))
            {
                SetQuaternion(
                    args[1],
                    ParseObjectIndex(args[2]),
                    args[3],
                    new Quaternion(
                        ParseSingle(args[4]),
                        ParseSingle(args[5]),
                        ParseSingle(args[6]),
                        ParseSingle(args[7])),
                    args[8]);
                return 0;
            }

            PrintUsage();
            return 64;
        }
        catch (Exception exception)
        {
            Console.Error.WriteLine($"ОШИБКА: {exception.Message}");
            Console.Error.WriteLine(exception);
            return 1;
        }
    }

    private static void Import(string sourceArgument, string projectArgument)
    {
        string sourcePath = ExistingFile(sourceArgument);
        string projectPath = OutputFile(projectArgument, ".smolvlproj");
        SmoDocument source = SmoDocument.Load(sourcePath);
        SmoProject project = SmoProject.Import(source);
        SmoProjectArchive.Save(project, projectPath);
        Console.WriteLine(
            $"PROJECT OK: {projectPath}\n" +
            $"source={source.Data.Length:N0} bytes; " +
            $"data={project.DataSection.Length:N0} bytes; " +
            $"objects={project.Objects.Count:N0}; " +
            $"sha256={project.Manifest.SourceSha256}");
    }

    private static void Build(string projectArgument, string outputArgument)
    {
        string projectPath = ExistingFile(projectArgument);
        string outputPath = OutputFile(outputArgument, ".smo");
        SmoProject project = SmoProjectArchive.Load(projectPath);
        SmoProjectBuildResult result =
            SmoProjectSerializer.Build(project, outputPath);
        Console.WriteLine(FormatResult(result));
    }

    private static void RoundTrip(
        string sourceArgument,
        string projectArgument,
        string outputArgument)
    {
        string sourcePath = ExistingFile(sourceArgument);
        string projectPath = OutputFile(projectArgument, ".smolvlproj");
        string outputPath = OutputFile(outputArgument, ".smo");
        if (string.Equals(sourcePath, outputPath, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException(
                "Project round-trip refuses to overwrite its source SMO.");
        }

        SmoDocument source = SmoDocument.Load(sourcePath);
        SmoProjectArchive.Save(
            SmoProject.Import(source),
            projectPath);
        SmoProject reopened =
            SmoProjectArchive.Load(projectPath);
        SmoProjectBuildResult result =
            SmoProjectSerializer.Build(reopened, outputPath);
        Console.WriteLine(
            $"ROUNDTRIP OK: {sourcePath}\n" +
            $"project={projectPath}\n" +
            FormatResult(result));
        if (!result.IsByteIdenticalToImportedSource)
        {
            throw new InvalidDataException(
                "The zero-edit project round-trip is structurally valid but not " +
                "byte-identical to its imported source.");
        }
    }

    private static void ListPlacements(string projectArgument)
    {
        string projectPath = ExistingFile(projectArgument);
        SmoProject project =
            SmoProjectArchive.Load(projectPath);
        SmoProjectObject[] placements = project.Objects
            .Where(item => item.TypeHash == SmoClassIds.StaticRenderObject)
            .ToArray();
        foreach (SmoProjectObject placement in placements)
        {
            Console.WriteLine(
                $"{placement.Index}\t0x{placement.Id:X8}\t{placement.DisplayName}");
        }
        Console.WriteLine($"PLACEMENTS: {placements.Length:N0}");
    }

    private static void ListMissingTransforms(string projectArgument)
    {
        string projectPath = ExistingFile(projectArgument);
        SmoProject project =
            SmoProjectArchive.Load(projectPath);
        SmoProjectObject[] candidates = project.Objects
            .Where(item =>
                item.TypeHash is SmoClassIds.Node or
                    SmoClassIds.RenderNode or SmoClassIds.Model &&
                item.Fields.Any(field =>
                    field.FieldType == 0 && field.Occurrence == 0 &&
                    field.PayloadSize == 12) &&
                (!item.Fields.Any(field =>
                     field.FieldType == 1 && field.Occurrence == 0 &&
                     field.PayloadSize == 16) ||
                 !item.Fields.Any(field =>
                     field.FieldType == 2 && field.Occurrence == 0 &&
                     field.PayloadSize == 12)))
            .ToArray();
        foreach (SmoProjectObject item in candidates)
        {
            bool rotation = item.Fields.Any(field =>
                field.FieldType == 1 && field.Occurrence == 0 &&
                field.PayloadSize == 16);
            bool scale = item.Fields.Any(field =>
                field.FieldType == 2 && field.Occurrence == 0 &&
                field.PayloadSize == 12);
            Console.WriteLine(
                $"{item.Index}\t0x{item.Id:X8}\t{item.DisplayName}\t" +
                $"rotation={(rotation ? "present" : "missing")};" +
                $"scale={(scale ? "present" : "missing")}");
        }
        Console.WriteLine($"MISSING TRANSFORMS: {candidates.Length:N0}");
    }

    private static void ListReferenceTemplates(string projectArgument)
    {
        string projectPath = ExistingFile(projectArgument);
        SmoProject project =
            SmoProjectArchive.Load(projectPath);
        SmoProjectObject[] templates = project.Objects
            .Where(item => project.CanAddReferencePlacement(item.Index, out _))
            .ToArray();
        foreach (SmoProjectObject template in templates)
        {
            Console.WriteLine(
                $"{template.Index}\t0x{template.Id:X8}\t{template.DisplayName}");
        }
        Console.WriteLine($"REFERENCE TEMPLATES: {templates.Length:N0}");
    }

    private static void CloneReferencePlacement(
        string projectArgument,
        int templateObjectIndex,
        Vector3 delta,
        string displayName,
        string outputArgument)
    {
        string projectPath = ExistingFile(projectArgument);
        string outputPath = OutputFile(outputArgument, ".smolvlproj");
        SmoProject project =
            SmoProjectArchive.Load(projectPath);
        uint newId = project.AddReferencePlacementTranslated(
            templateObjectIndex,
            delta,
            displayName);
        SmoProjectArchive.Save(project, outputPath);
        Console.WriteLine(
            $"PROJECT REFERENCE PLACEMENT OK: {outputPath}\n" +
            $"template={templateObjectIndex}; new-root=0x{newId:X8}; " +
            $"delta=({FormatSingle(delta.X)}, {FormatSingle(delta.Y)}, " +
            $"{FormatSingle(delta.Z)}); " +
            $"reference-placements={project.Manifest.ReferencePlacements.Count:N0}; " +
            $"immutable-data={project.DataSection.Length:N0} bytes");
    }

    private static void Translate(
        string projectArgument,
        int objectIndex,
        Vector3 delta,
        string outputArgument)
    {
        string projectPath = ExistingFile(projectArgument);
        string outputPath = OutputFile(outputArgument, ".smolvlproj");
        SmoProject project =
            SmoProjectArchive.Load(projectPath);
        project.TranslateStaticPlacement(objectIndex, delta);
        SmoProjectArchive.Save(project, outputPath);
        Console.WriteLine(
            $"PROJECT EDIT OK: {outputPath}\n" +
            $"object={objectIndex}; delta=({FormatSingle(delta.X)}, " +
            $"{FormatSingle(delta.Y)}, {FormatSingle(delta.Z)}); " +
            $"property-edits={project.Manifest.PropertyEdits.Count:N0}");
    }

    private static void RemoveBranch(
        string projectArgument,
        int objectIndex,
        string outputArgument)
    {
        string projectPath = ExistingFile(projectArgument);
        string outputPath = OutputFile(outputArgument, ".smolvlproj");
        SmoProject project =
            SmoProjectArchive.Load(projectPath);
        project.RemoveInlineBranch(objectIndex);
        SmoProjectArchive.Save(project, outputPath);
        Console.WriteLine(
            $"PROJECT STRUCTURE EDIT OK: {outputPath}\n" +
            $"removed-root={objectIndex}; " +
            $"branch-removals={project.Manifest.BranchRemovals.Count:N0}; " +
            $"immutable-data={project.DataSection.Length:N0} bytes");
    }

    private static void RemoveBranchPreservingSharedResources(
        string projectArgument,
        int objectIndex,
        string outputArgument)
    {
        string projectPath = ExistingFile(projectArgument);
        string outputPath = OutputFile(outputArgument, ".smolvlproj");
        SmoProject project =
            SmoProjectArchive.Load(projectPath);
        IReadOnlyList<uint> relocated =
            project.RelocateSharedLeavesForBranch(objectIndex);
        project.RemoveInlineBranch(objectIndex);
        SmoProjectArchive.Save(project, outputPath);
        Console.WriteLine(
            $"PROJECT STRUCTURE EDIT OK: {outputPath}\n" +
            $"removed-root={objectIndex}; relocated-resources=" +
            $"{string.Join(',', relocated)}; " +
            $"branch-removals={project.Manifest.BranchRemovals.Count:N0}; " +
            $"resource-relocations={project.Manifest.ResourceRelocations.Count:N0}; " +
            $"immutable-data={project.DataSection.Length:N0} bytes");
    }

    private static void SetVector(
        string projectArgument,
        int objectIndex,
        string propertyKey,
        Vector3 value,
        string outputArgument)
    {
        SavePropertyEdit(
            projectArgument,
            outputArgument,
            objectIndex,
            propertyKey,
            project => project.SetProperty(objectIndex, propertyKey, value));
    }

    private static void SetQuaternion(
        string projectArgument,
        int objectIndex,
        string propertyKey,
        Quaternion value,
        string outputArgument)
    {
        SavePropertyEdit(
            projectArgument,
            outputArgument,
            objectIndex,
            propertyKey,
            project => project.SetProperty(objectIndex, propertyKey, value));
    }

    private static void SavePropertyEdit(
        string projectArgument,
        string outputArgument,
        int objectIndex,
        string propertyKey,
        Action<SmoProject> apply)
    {
        string projectPath = ExistingFile(projectArgument);
        string outputPath = OutputFile(outputArgument, ".smolvlproj");
        SmoProject project =
            SmoProjectArchive.Load(projectPath);
        apply(project);
        SmoProjectArchive.Save(project, outputPath);
        Console.WriteLine(
            $"PROJECT EDIT OK: {outputPath}\n" +
            $"object={objectIndex}; property={propertyKey}; " +
            $"property-edits={project.Manifest.PropertyEdits.Count:N0}");
    }

    private static string FormatResult(SmoProjectBuildResult result) =>
        $"output={result.OutputPath}\n" +
        $"bytes={result.FileSize:N0}; objects={result.ObjectCount:N0}; " +
        $"sha256={result.Sha256}; " +
        $"byte-identical={result.IsByteIdenticalToImportedSource}";

    private static string ExistingFile(string argument)
    {
        string path = Path.GetFullPath(argument);
        if (!File.Exists(path))
            throw new FileNotFoundException("Input file does not exist.", path);
        return path;
    }

    private static string OutputFile(string argument, string expectedExtension)
    {
        string path = Path.GetFullPath(argument);
        if (!Path.GetExtension(path).Equals(
                expectedExtension,
                StringComparison.OrdinalIgnoreCase))
        {
            throw new ArgumentException(
                $"Output file must use the {expectedExtension} extension.");
        }
        string? directory = Path.GetDirectoryName(path);
        if (string.IsNullOrWhiteSpace(directory) || !Directory.Exists(directory))
            throw new DirectoryNotFoundException(directory);
        return path;
    }

    private static bool Is(string value, string expected) =>
        value.Equals(expected, StringComparison.OrdinalIgnoreCase);

    private static int ParseObjectIndex(string value)
    {
        if (!int.TryParse(
                value,
                NumberStyles.None,
                CultureInfo.InvariantCulture,
                out int result) ||
            result < 0)
        {
            throw new ArgumentException($"Invalid non-negative object index: {value}");
        }
        return result;
    }

    private static float ParseSingle(string value)
    {
        if (!float.TryParse(
                value,
                NumberStyles.Float,
                CultureInfo.InvariantCulture,
                out float result) ||
            !float.IsFinite(result))
        {
            throw new ArgumentException($"Invalid finite number: {value}");
        }
        return result;
    }

    private static string FormatSingle(float value) =>
        value.ToString("R", CultureInfo.InvariantCulture);

    private static void PrintUsage()
    {
        Console.WriteLine(
            "SmoLVLcreator.ProjectTool\n\n" +
            "  import    <source.smo> <project.smolvlproj>\n" +
            "  build     <project.smolvlproj> <output.smo>\n" +
            "  roundtrip <source.smo> <project.smolvlproj> <output.smo>\n" +
            "  placements <project.smolvlproj>\n" +
            "  missing-transforms <project.smolvlproj>\n" +
            "  reference-templates <project.smolvlproj>\n" +
            "  clone-reference <project.smolvlproj> <template-index> <dx> <dy> <dz> " +
            "<name> <output.smolvlproj>\n" +
            "  remove-branch <project.smolvlproj> <object-index> <output.smolvlproj>\n" +
            "  remove-branch-preserve <project.smolvlproj> <object-index> " +
            "<output.smolvlproj>\n" +
            "  translate <project.smolvlproj> <object-index> <dx> <dy> <dz> " +
            "<output.smolvlproj>\n" +
            "  set-vector <project> <object-index> <property> <x> <y> <z> " +
            "<output.smolvlproj>\n" +
            "  set-quaternion <project> <object-index> <property> <x> <y> <z> <w> " +
            "<output.smolvlproj>\n\n" +
            "Рабочий saver SmoLVLcreator этими командами не изменяется.");
    }
}
