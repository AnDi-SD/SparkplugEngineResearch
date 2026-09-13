using System.Text.Json;
using SmoViewer.Core;

namespace SmoViewer.Inspect;

internal static class Program
{
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        WriteIndented = true
    };

    public static int Main(string[] args)
    {
        if (args.Length == 0 || args[0] is "-h" or "--help")
        {
            PrintUsage();
            return args.Length == 0 ? 2 : 0;
        }

        bool json = args.Contains("--json", StringComparer.OrdinalIgnoreCase);
        string[] positional = args
            .Where(argument => !argument.StartsWith("--", StringComparison.Ordinal))
            .ToArray();

        try
        {
            if (positional.Length == 1)
                return InspectFile(positional[0], json);

            return positional[0].ToLowerInvariant() switch
            {
                "inspect" when positional.Length == 2 => InspectFile(positional[1], json),
                "scan" when positional.Length == 2 => ScanDirectory(positional[1], json),
                _ => InvalidArguments()
            };
        }
        catch (Exception exception) when (
            exception is IOException or UnauthorizedAccessException or SmoFormatException)
        {
            Console.Error.WriteLine(exception.Message);
            return 1;
        }
    }

    private static int InspectFile(string path, bool json)
    {
        SmoDocument document = SmoDocument.Load(path);
        InspectionResult result = CreateResult(document);

        if (json)
        {
            Console.WriteLine(JsonSerializer.Serialize(result, JsonOptions));
            return HasErrors(document) ? 1 : 0;
        }

        Console.WriteLine($"File: {result.Path}");
        Console.WriteLine(
            $"FFPS: version-field={result.Version}, bytes={result.ActualSize}, " +
            $"data=0x{result.DataStart:X8}+{result.DataSize}, objects={result.ObjectCount}");
        Console.WriteLine(
            $"Table: end=0x{result.ObjectTableEnd:X8}, " +
            $"signature mismatches={result.SignatureMismatchCount}");

        Console.WriteLine(
            $"Meshes: decoded={result.MeshDecoding.Decoded}/" +
            $"{result.MeshDecoding.Total}, " +
            $"unsupported={result.MeshDecoding.Unsupported}");
        foreach (MeshLayoutCount layout in result.MeshDecoding.Layouts)
        {
            Console.WriteLine(
                $"  {layout.Count,6}  marker={layout.Marker} " +
                $"format={layout.VertexFormat} " +
                $"stride={layout.SerializedStride}/{layout.RuntimeStride}");
        }

        foreach (MeshFailureGroup failure in result.MeshDecoding.Failures)
        {
            Console.WriteLine($"  unsupported {failure.Count,5}  {failure.Reason}");
            foreach (string sample in failure.Samples)
                Console.WriteLine($"    {sample}");
        }

        if (result.GuiScene.IsGuiContent)
        {
            Console.WriteLine(
                $"GUI: kind={result.GuiScene.Kind}, " +
                $"planar={result.GuiScene.PlanarMeshes}/" +
                $"{result.GuiScene.DecodedMeshes}, " +
                $"states={result.GuiScene.StateMeshes}, " +
                $"collisions={result.GuiScene.CollisionMeshes}, " +
                $"text-classes={result.GuiScene.TextClassObjects}");
            foreach (GuiRootGroupResult group in result.GuiScene.RootGroups)
            {
                Console.WriteLine(
                    $"  [{group.ObjectIndex,4}] {group.Name,-24} " +
                    $"objects={group.Objects,-4} nodes={group.Nodes,-4} " +
                    $"meshes={group.Meshes,-3}");
            }
            foreach (GuiLayoutAnchorResult anchor in result.GuiScene.LayoutAnchors)
            {
                Console.WriteLine(
                    $"  anchor [{anchor.ObjectIndex,4}] {anchor.Name,-20} " +
                    $"state={anchor.State,-12} " +
                    $"position=({anchor.X:G6}, {anchor.Y:G6}, {anchor.Z:G6})");
            }
        }

        Console.WriteLine("Classes:");
        foreach (ClassCount item in result.Classes)
            Console.WriteLine($"  0x{item.TypeHash:X8}  {item.Name,-26} {item.Count,6}");

        if (Path.GetExtension(path).Equals(".san", StringComparison.OrdinalIgnoreCase))
        {
            if (SmoAnimationDecoder.TryDecode(path, out SmoAnimationClip? clip, out string error) && clip is not null)
            {
                Console.WriteLine($"Animation: duration={clip.Duration:G6}s, tracks={clip.Tracks.Count}, max-keys={clip.FrameCount}");
                foreach (SmoAnimationTrack track in clip.Tracks.Take(12))
                    Console.WriteLine($"  {track.NodeName}: P={track.Positions.Count}, R={track.Rotations.Count}, S={track.Scales.Count}");
                if (clip.Tracks.Count > 12) Console.WriteLine($"  … {clip.Tracks.Count - 12} more tracks");
            }
            else Console.WriteLine($"Animation: unsupported: {error}");
        }

        if (result.Diagnostics.Count > 0)
        {
            Console.WriteLine("Diagnostics:");
            foreach (DiagnosticResult diagnostic in result.Diagnostics)
            {
                string location = diagnostic.Offset is long offset
                    ? $" @0x{offset:X}"
                    : string.Empty;
                Console.WriteLine(
                    $"  {diagnostic.Severity,-7} {diagnostic.Code}{location}: " +
                    diagnostic.Message);
            }
        }

        return HasErrors(document) ? 1 : 0;
    }

    private static int ScanDirectory(string path, bool json)
    {
        string fullPath = Path.GetFullPath(path);
        if (!Directory.Exists(fullPath))
            throw new DirectoryNotFoundException($"Directory not found: {fullPath}");

        string[] files = Directory
            .EnumerateFiles(fullPath, "*.*", SearchOption.AllDirectories)
            .Where(file => Path.GetExtension(file).Equals(".smo", StringComparison.OrdinalIgnoreCase) ||
                           Path.GetExtension(file).Equals(".san", StringComparison.OrdinalIgnoreCase))
            .OrderBy(file => file, StringComparer.OrdinalIgnoreCase)
            .ToArray();

        var results = new List<ScanResult>(files.Length);
        foreach (string file in files)
        {
            try
            {
                SmoDocument document = SmoDocument.Load(file);
                if (Path.GetExtension(file).Equals(".san", StringComparison.OrdinalIgnoreCase) &&
                    !SmoAnimationDecoder.TryDecode(file, out _, out string animationError))
                    throw new SmoFormatException($"Unsupported SAN: {animationError}");
                results.Add(new ScanResult(
                    Path.GetRelativePath(fullPath, file),
                    true,
                    document.Header.Version,
                    document.Objects.Count,
                    document.Objects.Count(item => item.TypeHash == SmoClassIds.MeshData),
                    document.Objects.Count(item => !item.SignatureMatches),
                    document.Diagnostics.Count(
                        item => item.Severity == SmoDiagnosticSeverity.Warning),
                    document.Diagnostics.Count(
                        item => item.Severity == SmoDiagnosticSeverity.Error),
                    null));
            }
            catch (Exception exception) when (
                exception is IOException or UnauthorizedAccessException or SmoFormatException)
            {
                results.Add(new ScanResult(
                    Path.GetRelativePath(fullPath, file),
                    false,
                    0,
                    0,
                    0,
                    0,
                    0,
                    1,
                    exception.Message));
            }
        }

        if (json)
        {
            Console.WriteLine(JsonSerializer.Serialize(results, JsonOptions));
        }
        else
        {
            foreach (ScanResult result in results)
            {
                string state = result.Parsed ? "OK  " : "FAIL";
                Console.WriteLine(
                    $"{state} v={result.Version,-2} obj={result.Objects,-5} " +
                    $"mesh={result.Meshes,-4} sig={result.SignatureMismatches,-4} " +
                    $"w={result.Warnings,-3} e={result.Errors,-3} {result.Path}");
                if (result.ErrorMessage is not null)
                    Console.WriteLine($"     {result.ErrorMessage}");
            }

            Console.WriteLine();
            Console.WriteLine(
                $"Files: {results.Count}; parsed: {results.Count(item => item.Parsed)}; " +
                $"failed: {results.Count(item => !item.Parsed)}; " +
                $"objects: {results.Sum(item => item.Objects)}; " +
                $"meshes: {results.Sum(item => item.Meshes)}");
        }

        return results.Any(item => !item.Parsed || item.Errors > 0) ? 1 : 0;
    }

    private static InspectionResult CreateResult(SmoDocument document)
    {
        ClassCount[] classes = document.Objects
            .GroupBy(item => item.TypeHash)
            .OrderByDescending(group => group.Count())
            .ThenBy(group => group.Key)
            .Select(group => new ClassCount(
                group.Key,
                SmoClassRegistry.GetDisplayName(group.Key),
                group.Count()))
            .ToArray();

        DiagnosticResult[] diagnostics = document.Diagnostics
            .Select(item => new DiagnosticResult(
                item.Severity.ToString(),
                item.Code,
                item.Message,
                item.Offset,
                item.ObjectIndex))
            .ToArray();

        MeshDecodingResult meshDecoding = DecodeMeshes(document);
        SmoGuiSceneInfo gui = SmoGuiSceneAnalyzer.Analyze(document);
        GuiSceneResult guiScene = new(
            gui.IsGuiContent,
            gui.ContentKind.ToString(),
            gui.DecodedMeshCount,
            gui.PlanarMeshCount,
            gui.StateMeshCount,
            gui.CollisionMeshCount,
            gui.NamedControlNodeCount,
            gui.TextClassObjectCount,
            gui.RootGroups.Select(group => new GuiRootGroupResult(
                group.ObjectIndex,
                group.Name,
                group.ObjectCount,
                group.NodeCount,
                group.MeshCount)).ToArray(),
            gui.LayoutAnchors.Select(anchor => new GuiLayoutAnchorResult(
                anchor.ObjectIndex,
                anchor.RootGroupObjectIndex,
                anchor.RootGroupName,
                anchor.Name,
                anchor.VisualState.ToString(),
                anchor.WorldPosition.X,
                anchor.WorldPosition.Y,
                anchor.WorldPosition.Z)).ToArray());

        return new InspectionResult(
            document.SourcePath ?? "<memory>",
            document.Data.Length,
            document.Header.Version,
            document.Header.DataStart,
            document.Header.DataSize,
            document.Objects.Count,
            document.Header.ObjectTableEnd,
            document.Objects.Count(item => !item.SignatureMatches),
            meshDecoding,
            guiScene,
            classes,
            diagnostics);
    }

    private static MeshDecodingResult DecodeMeshes(SmoDocument document)
    {
        SmoObjectEntry[] entries = document.Objects
            .Where(item => item.TypeHash == SmoClassIds.MeshData)
            .ToArray();
        var layouts = new Dictionary<MeshLayout, int>();
        var failures = new Dictionary<string, FailureAccumulator>(
            StringComparer.Ordinal);
        int decoded = 0;

        foreach (SmoObjectEntry entry in entries)
        {
            if (SmoMeshDecoder.TryDecode(
                    document,
                    entry,
                    out SmoMesh? mesh,
                    out string error))
            {
                decoded++;
                var layout = new MeshLayout(
                    $"0x{mesh.Marker:X2}",
                    $"0x{mesh.VertexFormat:X}",
                    mesh.Stride,
                    mesh.RuntimeStride);
                layouts[layout] = layouts.GetValueOrDefault(layout) + 1;
                continue;
            }

            string reason = ClassifyMeshFailure(error);
            if (!failures.TryGetValue(reason, out FailureAccumulator? failure))
            {
                failure = new FailureAccumulator();
                failures.Add(reason, failure);
            }

            failure.Count++;
            if (failure.Samples.Count < 3)
                failure.Samples.Add($"[{entry.Index}] {entry.Name}: {error}");
        }

        MeshLayoutCount[] layoutResults = layouts
            .OrderBy(item => item.Key.Marker, StringComparer.Ordinal)
            .ThenBy(item => item.Key.VertexFormat, StringComparer.Ordinal)
            .ThenBy(item => item.Key.SerializedStride)
            .Select(item => new MeshLayoutCount(
                item.Key.Marker,
                item.Key.VertexFormat,
                item.Key.SerializedStride,
                item.Key.RuntimeStride,
                item.Value))
            .ToArray();
        MeshFailureGroup[] failureResults = failures
            .OrderByDescending(item => item.Value.Count)
            .ThenBy(item => item.Key, StringComparer.Ordinal)
            .Select(item => new MeshFailureGroup(
                item.Key,
                item.Value.Count,
                item.Value.Samples))
            .ToArray();

        return new MeshDecodingResult(
            entries.Length,
            decoded,
            entries.Length - decoded,
            layoutResults,
            failureResults);
    }

    private static string ClassifyMeshFailure(string error)
    {
        if (error.Contains("directory offset may be stale", StringComparison.Ordinal))
            return "stale object-table offset";
        if (error.Contains("preamble separator", StringComparison.Ordinal))
            return "unconfirmed E1/PS2 preamble";
        if (error.Contains("outer payload ends", StringComparison.Ordinal))
            return "unconfirmed E1/PS2 object boundary";
        if (error.Contains("primitive type 2", StringComparison.Ordinal))
            return "unconfirmed primitive type 2";

        return "other strict-layout mismatch";
    }

    private static bool HasErrors(SmoDocument document) =>
        document.Diagnostics.Any(item => item.Severity == SmoDiagnosticSeverity.Error);

    private static int InvalidArguments()
    {
        PrintUsage();
        return 2;
    }

    private static void PrintUsage()
    {
        Console.WriteLine("SMO Inspector");
        Console.WriteLine("  SmoViewer.Inspect inspect <file.smo> [--json]");
        Console.WriteLine("  SmoViewer.Inspect scan <directory> [--json]");
        Console.WriteLine("  SmoViewer.Inspect <file.smo> [--json]");
    }

    private sealed record InspectionResult(
        string Path,
        int ActualSize,
        uint Version,
        uint DataStart,
        uint DataSize,
        int ObjectCount,
        int ObjectTableEnd,
        int SignatureMismatchCount,
        MeshDecodingResult MeshDecoding,
        GuiSceneResult GuiScene,
        IReadOnlyList<ClassCount> Classes,
        IReadOnlyList<DiagnosticResult> Diagnostics);

    private sealed record GuiSceneResult(
        bool IsGuiContent,
        string Kind,
        int DecodedMeshes,
        int PlanarMeshes,
        int StateMeshes,
        int CollisionMeshes,
        int NamedControlNodes,
        int TextClassObjects,
        IReadOnlyList<GuiRootGroupResult> RootGroups,
        IReadOnlyList<GuiLayoutAnchorResult> LayoutAnchors);

    private sealed record GuiRootGroupResult(
        int ObjectIndex,
        string Name,
        int Objects,
        int Nodes,
        int Meshes);

    private sealed record GuiLayoutAnchorResult(
        int ObjectIndex,
        int RootGroupObjectIndex,
        string RootGroupName,
        string Name,
        string State,
        float X,
        float Y,
        float Z);

    private sealed record MeshDecodingResult(
        int Total,
        int Decoded,
        int Unsupported,
        IReadOnlyList<MeshLayoutCount> Layouts,
        IReadOnlyList<MeshFailureGroup> Failures);

    private sealed record MeshLayoutCount(
        string Marker,
        string VertexFormat,
        int SerializedStride,
        int RuntimeStride,
        int Count);

    private sealed record MeshFailureGroup(
        string Reason,
        int Count,
        IReadOnlyList<string> Samples);

    private sealed record MeshLayout(
        string Marker,
        string VertexFormat,
        int SerializedStride,
        int RuntimeStride);

    private sealed class FailureAccumulator
    {
        public int Count { get; set; }
        public List<string> Samples { get; } = [];
    }

    private sealed record ClassCount(uint TypeHash, string Name, int Count);

    private sealed record DiagnosticResult(
        string Severity,
        string Code,
        string Message,
        long? Offset,
        int? ObjectIndex);

    private sealed record ScanResult(
        string Path,
        bool Parsed,
        uint Version,
        int Objects,
        int Meshes,
        int SignatureMismatches,
        int Warnings,
        int Errors,
        string? ErrorMessage);
}
