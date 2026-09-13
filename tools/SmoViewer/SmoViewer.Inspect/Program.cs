using System.Text.Json;
using System.Buffers.Binary;
using System.Numerics;
using Microsoft.Data.Sqlite;
using SmoViewer.Core;
using SmoViewer.Corpus;

namespace SmoViewer.Inspect;

internal static class Program
{
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        WriteIndented = true
    };

    private static readonly string[] ResearchAnalysisOrder =
    [
        "spNode", "spRenderNode", "spTextureData", "spMaterialData",
        "spMeshData", "spModel", "spStaticRenderObject", "spSkin",
        "spCollisionInfo", "spMeshBV", "spPartitionRenderable",
        "spPartitionNode", "spOctreeNode", "spPartitionSystem", "spZone",
        "spZonePortal", "spZonePortalNode", "spBSPNode",
        "spOcclusionVolume", "spMeshNavigationSet", "spNavigationPortal",
        "spNavigationGraph", "spSkyBox", "spParticleSystem",
        "spAnimTexController", "spLensFlare", "spFont", "spTextRenderable",
        "spTextNode", "spMaterialColorController", "spFog", "spOBBBV",
        "spBoxBV", "spSphereBV", "spUVController", "spLightData"
    ];

    public static int Main(string[] args)
    {
        if (args.Length == 0 || args[0] is "-h" or "--help")
        {
            PrintUsage();
            return args.Length == 0 ? 2 : 0;
        }

        bool json = args.Contains("--json", StringComparer.OrdinalIgnoreCase);
        bool collisionDetails = args.Contains(
            "--collision-details", StringComparer.OrdinalIgnoreCase);
        bool parsePckSmo = args.Contains(
            "--parse-smo", StringComparer.OrdinalIgnoreCase);
        string[] positional = args
            .Where(argument => !argument.StartsWith("--", StringComparison.Ordinal))
            .ToArray();

        try
        {
            if (positional.Length == 1)
                return InspectFile(positional[0], json, collisionDetails);

            return positional[0].ToLowerInvariant() switch
            {
                "inspect" when positional.Length == 2 =>
                    InspectFile(positional[1], json, collisionDetails),
                "scan" when positional.Length == 2 => ScanDirectory(positional[1], json),
                "class-inventory" when positional.Length == 2 =>
                    CreateClassInventory(positional[1], json),
                "corpus-db" when positional.Length == 4 &&
                    positional[1].Equals("update", StringComparison.OrdinalIgnoreCase) =>
                    UpdateCorpusDatabase(positional[2], positional[3], json),
                "corpus-db" when positional.Length == 3 &&
                    positional[1].Equals("summary", StringComparison.OrdinalIgnoreCase) =>
                    ShowCorpusDatabaseSummary(positional[2], json),
                "corpus-db" when positional.Length == 3 &&
                    positional[1].Equals("classes", StringComparison.OrdinalIgnoreCase) =>
                    ShowCorpusDatabaseClasses(positional[2], json),
                "corpus-db" when positional.Length == 3 &&
                    positional[1].Equals("metrics", StringComparison.OrdinalIgnoreCase) =>
                    ShowCorpusDatabaseMetrics(positional[2], json),
                "pck-inventory" when positional.Length == 2 =>
                    InspectPckDirectory(positional[1], parsePckSmo, json),
                "research-db" when positional.Length == 8 &&
                    positional[1].Equals("update-directory", StringComparison.OrdinalIgnoreCase) =>
                    UpdateResearchDirectory(
                        positional[2], positional[3], positional[4], positional[5],
                        positional[6], positional[7], json),
                "research-db" when positional.Length == 8 &&
                    positional[1].Equals("update-pck", StringComparison.OrdinalIgnoreCase) =>
                    UpdateResearchPck(
                        positional[2], positional[3], positional[4], positional[5],
                        positional[6], positional[7], json),
                "research-db" when positional.Length == 3 &&
                    positional[1].Equals("summary", StringComparison.OrdinalIgnoreCase) =>
                    ShowResearchSummary(positional[2], json),
                "research-db" when positional.Length == 3 &&
                    positional[1].Equals("sources", StringComparison.OrdinalIgnoreCase) =>
                    ShowResearchSources(positional[2], json),
                "research-db" when positional.Length == 3 &&
                    positional[1].Equals("classes", StringComparison.OrdinalIgnoreCase) =>
                    ShowResearchClasses(positional[2], json),
                "research-db" when positional.Length == 3 &&
                    positional[1].Equals("headers", StringComparison.OrdinalIgnoreCase) =>
                    ShowResearchHeaders(positional[2], json),
                "research-db" when positional.Length == 3 &&
                    positional[1].Equals("formats", StringComparison.OrdinalIgnoreCase) =>
                    ShowResearchFormats(positional[2], json),
                "research-db" when positional.Length == 4 &&
                    positional[1].Equals("format", StringComparison.OrdinalIgnoreCase) =>
                    ShowFormatResources(positional[2], positional[3], json),
                "research-db" when positional.Length == 3 &&
                    positional[1].Equals("resource-audit", StringComparison.OrdinalIgnoreCase) =>
                    ShowResourceAudit(positional[2], json),
                "research-db" when positional.Length == 3 &&
                    positional[1].Equals("resource-errors", StringComparison.OrdinalIgnoreCase) =>
                    ShowResourceErrors(positional[2], json),
                "research-db" when positional.Length == 3 &&
                    positional[1].Equals("refresh-unknown", StringComparison.OrdinalIgnoreCase) =>
                    RefreshUnknownResources(positional[2], json),
                "research-db" when positional.Length == 4 &&
                    positional[1].Equals("class", StringComparison.OrdinalIgnoreCase) =>
                    ShowResearchClass(positional[2], positional[3], json),
                "research-db" when positional.Length == 4 &&
                    positional[1].Equals("analyze-class", StringComparison.OrdinalIgnoreCase) =>
                    AnalyzeResearchClass(positional[2], positional[3], json),
                "research-db" when positional.Length == 3 &&
                    positional[1].Equals("analyze-all", StringComparison.OrdinalIgnoreCase) =>
                    AnalyzeAllResearchClasses(positional[2], json),
                "research-db" when positional.Length == 4 &&
                    positional[1].Equals("resources", StringComparison.OrdinalIgnoreCase) =>
                    ShowResearchResources(positional[2], positional[3], json),
                "research-db" when positional.Length == 3 &&
                    positional[1].Equals("conflicts", StringComparison.OrdinalIgnoreCase) =>
                    ShowResearchConflicts(positional[2], json),
                "research-db" when positional.Length == 3 &&
                    positional[1].Equals("integrity", StringComparison.OrdinalIgnoreCase) =>
                    CheckResearchIntegrity(positional[2], json),
                "research-db" when positional.Length == 4 &&
                    positional[1].Equals("import-evidence", StringComparison.OrdinalIgnoreCase) =>
                    ImportResearchEvidence(positional[2], positional[3], json),
                "research-db" when positional.Length == 5 &&
                    positional[1].Equals("compare", StringComparison.OrdinalIgnoreCase) =>
                    CompareResearchCorpora(
                        positional[2], positional[3], positional[4], json),
                "diff" when positional.Length == 3 =>
                    CompareLevels(positional[1], positional[2]),
                "animation-bindings" when positional.Length == 3 =>
                    AnalyzeAnimationBindings(positional[1], positional[2], json),
                "animation-coverage" when positional.Length == 3 =>
                    AnalyzeAnimationCoverage(positional[1], positional[2], json),
                "mesh-inventory" when positional.Length == 2 =>
                    ShowMeshInventory(positional[1], json),
                "object" when positional.Length == 3 &&
                    int.TryParse(positional[2], out int objectIndex) =>
                    InspectObject(positional[1], objectIndex),
                _ => InvalidArguments()
            };
        }
        catch (Exception exception) when (
            exception is IOException or UnauthorizedAccessException or SmoFormatException or
                InvalidDataException or InvalidOperationException or ArgumentException or
                SqliteException)
        {
            Console.Error.WriteLine(exception.Message);
            return 1;
        }
    }

    private static int InspectFile(string path, bool json, bool collisionDetails)
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
            $"FFPS: serializer=0x{result.SerializerVersion:X8}, " +
            $"word08=0x{result.Unknown08:X8}, platform-mask=0x{result.PlatformMask:X8}, " +
            $"bytes={result.ActualSize}, " +
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

        if (collisionDetails)
            PrintCollisionDetails(document);

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

    private static int InspectObject(string path, int objectIndex)
    {
        SmoDocument document = SmoDocument.Load(path);
        if ((uint)objectIndex >= (uint)document.Objects.Count)
            throw new ArgumentOutOfRangeException(nameof(objectIndex));

        SmoObjectEntry selected = document.Objects[objectIndex];
        Console.WriteLine($"File: {path}");
        Console.WriteLine("Ancestry:");
        SmoObjectEntry? cursor = selected;
        while (cursor is not null)
        {
            string transform = SmoNodeTransformDecoder.TryDecode(
                    document,
                    cursor,
                    out SmoNodeTransform? nodeTransform) &&
                nodeTransform is not null
                    ? $" transform=position:{nodeTransform.Position}, " +
                      $"rotation:{nodeTransform.Rotation}, scale:{nodeTransform.Scale}"
                    : string.Empty;
            Console.WriteLine(
                $"  [{cursor.Index}] id={cursor.Id} " +
                $"{cursor.ClassName ?? $"0x{cursor.TypeHash:X8}"} " +
                $"\"{cursor.Name}\" offset=0x{cursor.PhysicalOffset:X} " +
                $"size={cursor.SerializedSize}{transform}");
            cursor = cursor.ParentIndex is int parentIndex
                ? document.Objects[parentIndex]
                : null;
        }

        Console.WriteLine("Direct children:");
        foreach (SmoObjectEntry child in document.Objects.Where(entry =>
                     entry.ParentIndex == selected.Index))
        {
            Console.WriteLine(
                $"  [{child.Index}] id={child.Id} " +
                $"{child.ClassName ?? $"0x{child.TypeHash:X8}"} " +
                $"\"{child.Name}\" size={child.SerializedSize}");
        }

        Console.WriteLine("Reference fields targeting this object:");
        int referenceCount = 0;
        foreach (SmoObjectEntry owner in document.Objects)
        {
            ReadOnlySpan<byte> bytes = document.Data.Span.Slice(
                checked((int)owner.PhysicalOffset),
                checked((int)owner.SerializedSize));
            int offset = 8;
            while (offset < bytes.Length &&
                   SmoDataBlockReader.TryReadHeader(
                       bytes, offset, out SmoDataBlockHeader field))
            {
                if (field.PayloadSize == 8 &&
                    BinaryPrimitives.ReadUInt32LittleEndian(
                        bytes[field.PayloadOffset..]) == selected.Id &&
                    BinaryPrimitives.ReadUInt32LittleEndian(
                        bytes[(field.PayloadOffset + 4)..]) == 0)
                {
                    Console.WriteLine(
                        $"  [{owner.Index}] id={owner.Id} " +
                        $"{owner.ClassName ?? $"0x{owner.TypeHash:X8}"} " +
                        $"\"{owner.Name}\" fieldType={field.FieldType}");
                    referenceCount++;
                }
                offset = checked((int)field.PayloadEnd);
            }
        }
        if (referenceCount == 0)
            Console.WriteLine("  none");
        PrintObjectStructure(document, objectIndex);
        return 0;
    }

    private static int ShowMeshInventory(string path, bool json)
    {
        SmoDocument document = SmoDocument.Load(path);
        Dictionary<int, SmoSkin> skinsByMesh = document.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.Skin)
            .Select(entry => SmoSkinDecoder.TryDecode(
                    document, entry, out SmoSkin? skin, out _)
                ? skin
                : null)
            .Where(skin => skin is not null &&
                skin.BaseMesh.TargetObjectIndex.HasValue)
            .Cast<SmoSkin>()
            .GroupBy(skin => skin.BaseMesh.TargetObjectIndex!.Value)
            .ToDictionary(group => group.Key, group => group.First());

        List<MeshInventoryItem> items = [];
        foreach (SmoObjectEntry entry in document.Objects.Where(item =>
                     item.TypeHash == SmoClassIds.MeshData))
        {
            if (!SmoMeshDecoder.TryDecode(
                    document, entry, out SmoMesh? mesh, out string error) ||
                mesh is null)
            {
                items.Add(new MeshInventoryItem(
                    entry.Index, entry.Name, entry.SerializedSize, null, null,
                    null, null, null, null, null, error));
                continue;
            }

            skinsByMesh.TryGetValue(entry.Index, out SmoSkin? skin);
            items.Add(new MeshInventoryItem(
                entry.Index,
                entry.Name,
                entry.SerializedSize,
                mesh.VertexCount,
                mesh.IndexCount,
                mesh.TriangleCount,
                mesh.Stride,
                mesh.RuntimeStride,
                skin?.ObjectIndex,
                skin?.Bones.Count,
                null));
        }

        if (json)
        {
            Console.WriteLine(JsonSerializer.Serialize(items, JsonOptions));
            return items.Any(item => item.Error is not null) ? 1 : 0;
        }

        Console.WriteLine($"File: {Path.GetFullPath(path)}");
        Console.WriteLine(
            " mesh     bytes  vertices   indices triangles stride skin palette name");
        foreach (MeshInventoryItem item in items)
        {
            if (item.Error is not null)
            {
                Console.WriteLine(
                    $"[{item.ObjectIndex,4}] {item.SerializedSize,9} " +
                    $"unsupported: {item.Error}");
                continue;
            }

            Console.WriteLine(
                $"[{item.ObjectIndex,4}] {item.SerializedSize,9} " +
                $"{item.VertexCount,9} {item.IndexCount,9} " +
                $"{item.TriangleCount,9} " +
                $"{item.SerializedStride}/{item.RuntimeStride,-3} " +
                $"{item.SkinObjectIndex?.ToString() ?? "-",4} " +
                $"{item.PaletteSize?.ToString() ?? "-",7} {item.Name}");
        }

        return items.Any(item => item.Error is not null) ? 1 : 0;
    }

    private static int CompareLevels(string originalPath, string modifiedPath)
    {
        SmoDocument original = SmoDocument.Load(originalPath);
        SmoDocument modified = SmoDocument.Load(modifiedPath);
        ReadOnlySpan<byte> originalData = original.Data.Span;
        ReadOnlySpan<byte> modifiedData = modified.Data.Span;
        bool directoryEqual = original.Header.DataStart == modified.Header.DataStart &&
            originalData.Slice(
                SmoHeader.ObjectTableOffset,
                checked((int)original.Header.DataStart) - SmoHeader.ObjectTableOffset)
            .SequenceEqual(modifiedData.Slice(
                SmoHeader.ObjectTableOffset,
                checked((int)modified.Header.DataStart) - SmoHeader.ObjectTableOffset));
        int firstDifference = FirstDifference(
            originalData[checked((int)original.Header.DataStart)..],
            modifiedData[checked((int)modified.Header.DataStart)..]);
        long firstPhysicalDifference = firstDifference < 0
            ? -1
            : original.Header.DataStart + firstDifference;
        SmoObjectEntry? firstObject = firstPhysicalDifference < 0
            ? null
            : original.Objects
                .Where(entry => entry.PhysicalOffset <= firstPhysicalDifference &&
                    entry.PhysicalEnd > firstPhysicalDifference)
                .OrderBy(entry => entry.SerializedSize)
                .FirstOrDefault();

        Console.WriteLine($"Original: {originalPath}");
        Console.WriteLine($"Modified: {modifiedPath}");
        Console.WriteLine(
            $"Size: {original.Data.Length} -> {modified.Data.Length} " +
            $"(delta {modified.Data.Length - original.Data.Length:+#;-#;0})");
        Console.WriteLine(
            $"Directory identical: {directoryEqual}; " +
            $"modified signature mismatches: " +
            $"{modified.Objects.Count(entry => !entry.SignatureMatches)}");
        Console.WriteLine(
            $"First data difference: 0x{firstPhysicalDifference:X}; " +
            $"object={(firstObject is null ? "NONE" :
                $"[{firstObject.Index}] {firstObject.ClassName ?? $"0x{firstObject.TypeHash:X8}"} " +
                $"\"{firstObject.Name.TrimEnd('\0')}\"")}");

        PlacementChange[] placementChanges =
            CompareStaticPlacements(original, modifiedData);
        CompareMeshGeometry(original, modifiedData);
        CompareCollisionMeshes(original, modifiedData);
        AnalyzeCollisionNearChangedPlacements(original, placementChanges);
        CompareSizedObjects(
            original,
            modifiedData,
            SmoClassIds.TextureData,
            "Texture objects");
        return 0;
    }

    private static int AnalyzeAnimationBindings(
        string modelPath,
        string animationPath,
        bool json)
    {
        SmoDocument model = SmoDocument.Load(modelPath);
        string fullAnimationPath = Path.GetFullPath(animationPath);
        bool isDirectory = Directory.Exists(fullAnimationPath);
        string[] paths = isDirectory
            ? Directory.EnumerateFiles(
                    fullAnimationPath, "*.san", SearchOption.AllDirectories)
                .Order(StringComparer.OrdinalIgnoreCase)
                .ToArray()
            : File.Exists(fullAnimationPath)
                ? [fullAnimationPath]
                : throw new FileNotFoundException(
                    $"Animation file or directory not found: {fullAnimationPath}");

        var results = new List<AnimationBindingResult>(paths.Length);
        foreach (string path in paths)
        {
            string displayPath = isDirectory
                ? Path.GetRelativePath(fullAnimationPath, path)
                : Path.GetFileName(path);
            if (!SmoAnimationDecoder.TryDecode(
                    path, out SmoAnimationClip? clip, out string error) ||
                clip is null)
            {
                results.Add(new AnimationBindingResult(
                    displayPath, false, null, error));
                continue;
            }

            results.Add(new AnimationBindingResult(
                displayPath,
                true,
                SmoAnimationBindingAnalyzer.Analyze(model, clip),
                null));
        }

        if (json)
        {
            Console.WriteLine(JsonSerializer.Serialize(results, JsonOptions));
        }
        else
        {
            Console.WriteLine($"Model: {Path.GetFullPath(modelPath)}");
            foreach (AnimationBindingResult result in results)
            {
                if (!result.Decoded || result.Analysis is null)
                {
                    Console.WriteLine($"FAIL {result.Path}: {result.Error}");
                    continue;
                }

                SmoAnimationBindingAnalysis analysis = result.Analysis;
                Console.WriteLine(
                    $"{result.Path}: tracks={analysis.TrackCount}, " +
                    $"exact={analysis.ExactMatchCount}, " +
                    $"case-only={analysis.CaseFoldedOnlyMatchCount}, " +
                    $"missing={analysis.MissingMatchCount}");
                if (analysis.CaseFoldedOnlyTrackNames.Count > 0)
                {
                    Console.WriteLine(
                        "  case-only: " +
                        string.Join(", ", analysis.CaseFoldedOnlyTrackNames));
                }
                if (analysis.MissingTrackNames.Count > 0)
                {
                    Console.WriteLine(
                        "  missing: " +
                        string.Join(", ", analysis.MissingTrackNames));
                }
            }

            AnimationBindingResult[] decoded = results
                .Where(result => result.Decoded && result.Analysis is not null)
                .ToArray();
            Console.WriteLine(
                $"Files: {results.Count}; decoded={decoded.Length}; " +
                $"tracks={decoded.Sum(result => result.Analysis!.TrackCount)}; " +
                $"exact={decoded.Sum(result => result.Analysis!.ExactMatchCount)}; " +
                $"case-only={decoded.Sum(result => result.Analysis!.CaseFoldedOnlyMatchCount)}; " +
                $"missing={decoded.Sum(result => result.Analysis!.MissingMatchCount)}");
        }

        return results.Any(result => !result.Decoded) ? 1 : 0;
    }

    private static int AnalyzeAnimationCoverage(
        string modelPath,
        string animationDirectory,
        bool json)
    {
        SmoDocument model = SmoDocument.Load(modelPath);
        string fullAnimationDirectory = Path.GetFullPath(animationDirectory);
        if (!Directory.Exists(fullAnimationDirectory))
            throw new DirectoryNotFoundException(
                $"Animation directory not found: {fullAnimationDirectory}");

        string[] paths = Directory.EnumerateFiles(
                fullAnimationDirectory, "*.san", SearchOption.AllDirectories)
            .Order(StringComparer.OrdinalIgnoreCase)
            .ToArray();
        var accumulators = model.Objects
            .Where(entry => entry.TypeHash is SmoClassIds.Node or SmoClassIds.RenderNode)
            .Select(entry => new AnimationNodeCoverageAccumulator(entry))
            .ToArray();
        Dictionary<string, AnimationNodeCoverageAccumulator[]> byName = accumulators
            .GroupBy(item => item.NodeName, StringComparer.Ordinal)
            .ToDictionary(group => group.Key, group => group.ToArray(), StringComparer.Ordinal);

        int decodedFiles = 0;
        var failures = new List<AnimationCoverageFailure>();
        foreach (string path in paths)
        {
            string displayPath = Path.GetRelativePath(fullAnimationDirectory, path);
            if (!SmoAnimationDecoder.TryDecode(
                    path, out SmoAnimationClip? clip, out string error) || clip is null)
            {
                failures.Add(new AnimationCoverageFailure(displayPath, error));
                continue;
            }

            decodedFiles++;
            var touched = new HashSet<AnimationNodeCoverageAccumulator>();
            foreach (SmoAnimationTrack track in clip.Tracks)
            {
                if (!byName.TryGetValue(track.NodeName, out AnimationNodeCoverageAccumulator[]? targets))
                    continue;
                foreach (AnimationNodeCoverageAccumulator target in targets)
                {
                    target.TrackOccurrences++;
                    target.PositionKeys += track.Positions.Count;
                    target.RotationKeys += track.Rotations.Count;
                    target.ScaleKeys += track.Scales.Count;
                    target.SampleFiles.Add(displayPath);
                    touched.Add(target);
                }
            }
            foreach (AnimationNodeCoverageAccumulator target in touched)
                target.AnimationFiles++;
        }

        AnimationNodeCoverage[] nodes = accumulators
            .Select(item => item.ToResult())
            .OrderByDescending(item => item.AnimationFiles)
            .ThenByDescending(item => item.TotalKeys)
            .ThenBy(item => item.NodeName, StringComparer.Ordinal)
            .ThenBy(item => item.ObjectIndex)
            .ToArray();
        var result = new AnimationCoverageResult(
            Path.GetFullPath(modelPath),
            fullAnimationDirectory,
            paths.Length,
            decodedFiles,
            failures,
            nodes);

        if (json)
        {
            Console.WriteLine(JsonSerializer.Serialize(result, JsonOptions));
        }
        else
        {
            Console.WriteLine($"Model: {result.ModelPath}");
            Console.WriteLine($"Animations: {result.AnimationDirectory}");
            Console.WriteLine(
                $"Files: {result.AnimationFiles}; decoded={result.DecodedFiles}; " +
                $"failed={result.Failures.Count}; nodes={result.Nodes.Count}");
            Console.WriteLine(
                "object  class         files tracks   pos    rot  scale  total  name");
            foreach (AnimationNodeCoverage node in nodes)
            {
                Console.WriteLine(
                    $"{node.ObjectIndex,6}  {node.ClassName,-12} " +
                    $"{node.AnimationFiles,5} {node.TrackOccurrences,6} " +
                    $"{node.PositionKeys,5} {node.RotationKeys,6} " +
                    $"{node.ScaleKeys,6} {node.TotalKeys,6}  {node.NodeName}");
            }
            foreach (AnimationCoverageFailure failure in failures)
                Console.WriteLine($"FAIL {failure.Path}: {failure.Error}");
        }

        return failures.Count > 0 ? 1 : 0;
    }

    private static void CompareSizedObjects(
        SmoDocument original,
        ReadOnlySpan<byte> modifiedData,
        uint classId,
        string label)
    {
        SmoObjectEntry[] objects = original.Objects
            .Where(entry => entry.TypeHash == classId)
            .OrderBy(entry => entry.PhysicalOffset)
            .ToArray();
        int[] occurrences = FindSignatures(
            modifiedData,
            checked((int)original.Header.DataStart),
            classId);
        int changed = 0;
        long oldBytes = 0;
        long newBytes = 0;
        int comparable = Math.Min(objects.Length, occurrences.Length);
        for (int index = 0; index < comparable; index++)
        {
            int oldSize = checked((int)objects[index].SerializedSize);
            oldBytes += oldSize;
            if (!TryGetSerializedObjectSize(
                    modifiedData, occurrences[index], out int newSize))
            {
                changed++;
                continue;
            }
            newBytes += newSize;
            if (oldSize != newSize ||
                !original.Data.Span.Slice(
                    checked((int)objects[index].PhysicalOffset), oldSize)
                .SequenceEqual(modifiedData.Slice(occurrences[index], newSize)))
            {
                changed++;
            }
        }
        Console.WriteLine(
            $"{label}: directory={objects.Length}, physical signatures={occurrences.Length}, " +
            $"changed={changed}, bytes={oldBytes}->{newBytes} " +
            $"(delta {newBytes - oldBytes:+#;-#;0})");
    }

    private static void CompareMeshGeometry(
        SmoDocument original,
        ReadOnlySpan<byte> modifiedData)
    {
        SmoObjectEntry[] meshes = original.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.MeshData)
            .OrderBy(entry => entry.PhysicalOffset)
            .ToArray();
        int[] occurrences = FindSignatures(
            modifiedData,
            checked((int)original.Header.DataStart),
            SmoClassIds.MeshData);
        var changes = new List<string>();
        int comparable = Math.Min(meshes.Length, occurrences.Length);
        for (int index = 0; index < comparable; index++)
        {
            SmoObjectEntry mesh = meshes[index];
            int oldSize = checked((int)mesh.SerializedSize);
            if (!TryGetSerializedObjectSize(
                    modifiedData, occurrences[index], out int newSize))
            {
                changes.Add($"  [{mesh.Index}] \"{mesh.Name.TrimEnd('\0')}\": unreadable");
                continue;
            }
            bool equal = oldSize == newSize &&
                original.Data.Span.Slice(
                    checked((int)mesh.PhysicalOffset), oldSize)
                .SequenceEqual(modifiedData.Slice(occurrences[index], newSize));
            if (!equal)
            {
                changes.Add(
                    $"  [{mesh.Index}] \"{mesh.Name.TrimEnd('\0')}\": " +
                    $"bytes {oldSize} -> {newSize}");
            }
        }
        Console.WriteLine(
            $"Render meshes: directory={meshes.Length}, " +
            $"physical signatures={occurrences.Length}, changed={changes.Count}");
        foreach (string change in changes.Take(48))
            Console.WriteLine(change);
    }

    private static bool TryGetSerializedObjectSize(
        ReadOnlySpan<byte> data,
        int physicalOffset,
        out int size)
    {
        size = 0;
        if (!SmoDataBlockReader.TryReadHeader(
                data[physicalOffset..], 8, out SmoDataBlockHeader outer) ||
            outer.PayloadEnd >= int.MaxValue)
        {
            return false;
        }
        size = checked((int)outer.PayloadEnd + 1);
        return physicalOffset <= data.Length - size;
    }

    private static PlacementChange[] CompareStaticPlacements(
        SmoDocument original,
        ReadOnlySpan<byte> modifiedData)
    {
        SmoObjectEntry[] placements = original.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.StaticRenderObject)
            .OrderBy(entry => entry.PhysicalOffset)
            .ToArray();
        int[] occurrences = FindSignatures(
            modifiedData,
            checked((int)original.Header.DataStart),
            SmoClassIds.StaticRenderObject);
        var changes = new List<PlacementChange>();
        int comparable = Math.Min(placements.Length, occurrences.Length);
        for (int index = 0; index < comparable; index++)
        {
            SmoObjectEntry placement = placements[index];
            ReadOnlySpan<byte> serialized = original.Data.Span.Slice(
                checked((int)placement.PhysicalOffset),
                checked((int)placement.SerializedSize));
            if (!SmoDataBlockReader.TryReadHeader(
                    serialized, 8, out SmoDataBlockHeader matrix) ||
                matrix.PayloadSize != 64 ||
                occurrences[index] > modifiedData.Length - matrix.PayloadOffset - 64)
            {
                continue;
            }
            ReadOnlySpan<byte> before = serialized.Slice(matrix.PayloadOffset, 64);
            ReadOnlySpan<byte> after = modifiedData.Slice(
                occurrences[index] + matrix.PayloadOffset, 64);
            if (before.SequenceEqual(after))
                continue;
            Vector3 oldPosition = ReadMatrixTranslation(before);
            Vector3 newPosition = ReadMatrixTranslation(after);
            changes.Add(new PlacementChange(
                placement.Index,
                placement.Name.TrimEnd('\0'),
                oldPosition,
                newPosition));
        }

        Console.WriteLine(
            $"Static placements: directory={placements.Length}, " +
            $"physical signatures={occurrences.Length}, changed matrices={changes.Count}");
        foreach (PlacementChange change in changes.Take(24))
        {
            Console.WriteLine(
                $"  [{change.ObjectIndex}] \"{change.Name}\": " +
                $"{change.OldPosition} -> {change.NewPosition}; " +
                $"delta={change.Delta}");
        }
        return changes.ToArray();
    }

    private static void AnalyzeCollisionNearChangedPlacements(
        SmoDocument document,
        IReadOnlyList<PlacementChange> changes)
    {
        if (changes.Count == 0)
            return;

        CollisionComponent[] components = SmoCollisionMeshDecoder.DecodeAll(document)
            .SelectMany(FindCollisionComponents)
            .ToArray();
        var groups = changes
            .GroupBy(change => (
                X: (int)MathF.Round(change.Delta.X * 10),
                Y: (int)MathF.Round(change.Delta.Y * 10),
                Z: (int)MathF.Round(change.Delta.Z * 10)))
            .Where(group => group.Count() > 1)
            .OrderByDescending(group => group.Count());

        Console.WriteLine("Collision proximity for composed placement groups:");
        foreach (IGrouping<(int X, int Y, int Z), PlacementChange> group in groups)
        {
            PlacementChange[] groupChanges = group.ToArray();
            Bounds3 visualBounds = Bounds3.Empty;
            var meshNames = new List<string>();
            foreach (PlacementChange change in groupChanges)
            {
                foreach (SmoObjectEntry entry in document.Objects.Where(entry =>
                             entry.TypeHash == SmoClassIds.MeshData &&
                             IsDescendantOf(document.Objects, entry, change.ObjectIndex)))
                {
                    if (!SmoMeshDecoder.TryDecode(
                            document, entry, out SmoMesh? mesh, out _))
                    {
                        continue;
                    }
                    Matrix4x4 world =
                        SmoNodeTransformDecoder.ResolveModelWorldMatrix(document, entry);
                    foreach (Vector3 position in mesh.Positions)
                        visualBounds = visualBounds.Include(Vector3.Transform(position, world));
                    meshNames.Add($"[{entry.Index}] {entry.Name.TrimEnd('\0')}");
                }
            }

            if (!visualBounds.IsValid)
                continue;

            CollisionComponent[] nearest = components
                .OrderBy(component => component.Bounds.DistanceTo(visualBounds))
                .Take(8)
                .ToArray();
            Console.WriteLine(
                $"  placements={string.Join(",", groupChanges.Select(item => item.ObjectIndex))}; " +
                $"delta={groupChanges[0].Delta}; bounds={visualBounds}; " +
                $"meshes={string.Join(", ", meshNames)}");
            foreach (PlacementChange change in groupChanges)
                PrintObjectStructure(document, change.ObjectIndex);
            foreach (CollisionComponent component in nearest)
            {
                Console.WriteLine(
                    $"    collision=[{component.CollisionInfoObjectIndex}] " +
                    $"meshBV=[{component.MeshBoundingVolumeObjectIndex}] " +
                    $"\"{component.Name}\" component={component.ComponentIndex} " +
                    $"triangles={component.TriangleCount}; " +
                    $"distance={component.Bounds.DistanceTo(visualBounds):G6}; " +
                    $"bounds={component.Bounds}");
            }
            PrintOverlappingCollisionTriangles(document, visualBounds);
            PrintNearbyWalkableTriangles(document, visualBounds);
            PrintNearestCollisionTriangles(document, visualBounds);
        }
    }

    private static void PrintOverlappingCollisionTriangles(
        SmoDocument document,
        Bounds3 visualBounds)
    {
        var candidates = new List<string>();
        foreach (SmoCollisionMesh collision in SmoCollisionMeshDecoder.DecodeAll(document))
        {
            Vector3[] positions = collision.Positions
                .Select(position => Vector3.Transform(position, collision.WorldTransform))
                .ToArray();
            for (int index = 0; index < collision.TriangleIndices.Count; index += 3)
            {
                Vector3 a = positions[collision.TriangleIndices[index]];
                Vector3 b = positions[collision.TriangleIndices[index + 1]];
                Vector3 c = positions[collision.TriangleIndices[index + 2]];
                Bounds3 bounds = Bounds3.Empty.Include(a).Include(b).Include(c);
                if (bounds.DistanceTo(visualBounds) > 0.0001f)
                    continue;
                Vector3 cross = Vector3.Cross(b - a, c - a);
                Vector3 normal = cross.LengthSquared() > 0.000001f
                    ? Vector3.Normalize(cross)
                    : Vector3.Zero;
                candidates.Add(
                    $"      collision=[{collision.CollisionInfoObjectIndex}] " +
                    $"meshBV=[{collision.MeshBoundingVolumeObjectIndex}] " +
                    $"triangle={index / 3}; center={(a + b + c) / 3.0f}; " +
                    $"normal={normal}; bounds={bounds}");
            }
        }
        Console.WriteLine($"    triangle AABB overlaps visual bounds={candidates.Count}");
        foreach (string candidate in candidates.Take(24))
            Console.WriteLine(candidate);
    }

    private static void PrintObjectStructure(SmoDocument document, int objectIndex)
    {
        SmoObjectEntry entry = document.Objects[objectIndex];
        string ancestors = string.Join(" <- ", EnumerateAncestors(document, entry)
            .Select(item =>
                $"[{item.Index}] {item.ClassName ?? $"0x{item.TypeHash:X8}"} " +
                $"\"{item.Name.TrimEnd('\0')}\""));
        string descendants = string.Join(", ", document.Objects
            .Where(candidate => candidate.Index != entry.Index &&
                IsDescendantOf(document.Objects, candidate, entry.Index))
            .Select(candidate =>
                $"[{candidate.Index}] {candidate.ClassName ?? $"0x{candidate.TypeHash:X8}"} " +
                $"\"{candidate.Name.TrimEnd('\0')}\""));
        Console.WriteLine(
            $"    object [{entry.Index}] size={entry.SerializedSize}: {ancestors}");
        Console.WriteLine($"      descendants: {descendants}");

        ReadOnlySpan<byte> data = document.Data.Span.Slice(
            checked((int)entry.PhysicalOffset), checked((int)entry.SerializedSize));
        int offset = 8;
        var fields = new List<string>();
        while (offset < data.Length &&
               SmoDataBlockReader.TryReadHeader(
                   data, offset, out SmoDataBlockHeader field))
        {
            fields.Add(
                $"type={field.FieldType}/payload={field.PayloadSize}/" +
                $"offset=0x{field.PayloadOffset:X}");
            int next = checked((int)field.PayloadEnd);
            if (next <= offset)
                break;
            offset = next;
        }
        Console.WriteLine($"      fields: {string.Join(", ", fields)}");
    }

    private static IEnumerable<SmoObjectEntry> EnumerateAncestors(
        SmoDocument document,
        SmoObjectEntry entry)
    {
        SmoObjectEntry? cursor = entry;
        var visited = new HashSet<int>();
        while (cursor is not null && visited.Add(cursor.Index))
        {
            yield return cursor;
            cursor = cursor.ParentIndex is int parentIndex &&
                     (uint)parentIndex < (uint)document.Objects.Count
                ? document.Objects[parentIndex]
                : null;
        }
    }

    private static void PrintNearestCollisionTriangles(
        SmoDocument document,
        Bounds3 visualBounds)
    {
        var triangles = new List<(float Distance, string Description)>();
        foreach (SmoCollisionMesh collision in SmoCollisionMeshDecoder.DecodeAll(document))
        {
            Vector3[] positions = collision.Positions
                .Select(position => Vector3.Transform(position, collision.WorldTransform))
                .ToArray();
            for (int index = 0; index < collision.TriangleIndices.Count; index += 3)
            {
                Vector3 a = positions[collision.TriangleIndices[index]];
                Vector3 b = positions[collision.TriangleIndices[index + 1]];
                Vector3 c = positions[collision.TriangleIndices[index + 2]];
                Bounds3 bounds = Bounds3.Empty.Include(a).Include(b).Include(c);
                float distance = bounds.DistanceTo(visualBounds);
                Vector3 cross = Vector3.Cross(b - a, c - a);
                Vector3 normal = cross.LengthSquared() > 0.000001f
                    ? Vector3.Normalize(cross)
                    : Vector3.Zero;
                triangles.Add((distance,
                    $"      distance={distance:G6}; " +
                    $"collision=[{collision.CollisionInfoObjectIndex}] " +
                    $"meshBV=[{collision.MeshBoundingVolumeObjectIndex}] " +
                    $"triangle={index / 3}; center={(a + b + c) / 3.0f}; " +
                    $"normal={normal}; bounds={bounds}"));
            }
        }
        Console.WriteLine("    nearest collision triangles:");
        foreach ((_, string description) in triangles
                     .OrderBy(item => item.Distance)
                     .Take(8))
        {
            Console.WriteLine(description);
        }
    }

    private static void PrintNearbyWalkableTriangles(
        SmoDocument document,
        Bounds3 visualBounds)
    {
        const float horizontalPadding = 12.0f;
        const float verticalPadding = 12.0f;
        var candidates = new List<string>();
        foreach (SmoCollisionMesh collision in SmoCollisionMeshDecoder.DecodeAll(document))
        {
            Vector3[] positions = collision.Positions
                .Select(position => Vector3.Transform(position, collision.WorldTransform))
                .ToArray();
            for (int index = 0; index < collision.TriangleIndices.Count; index += 3)
            {
                Vector3 a = positions[collision.TriangleIndices[index]];
                Vector3 b = positions[collision.TriangleIndices[index + 1]];
                Vector3 c = positions[collision.TriangleIndices[index + 2]];
                Vector3 center = (a + b + c) / 3.0f;
                Bounds3 triangleBounds = Bounds3.Empty.Include(a).Include(b).Include(c);
                if (triangleBounds.Maximum.X < visualBounds.Minimum.X - horizontalPadding ||
                    triangleBounds.Minimum.X > visualBounds.Maximum.X + horizontalPadding ||
                    triangleBounds.Maximum.Z < visualBounds.Minimum.Z - horizontalPadding ||
                    triangleBounds.Minimum.Z > visualBounds.Maximum.Z + horizontalPadding ||
                    triangleBounds.Maximum.Y < visualBounds.Minimum.Y - verticalPadding ||
                    triangleBounds.Minimum.Y > visualBounds.Maximum.Y + verticalPadding)
                {
                    continue;
                }
                Vector3 cross = Vector3.Cross(b - a, c - a);
                Vector3 normal = cross.LengthSquared() > 0.000001f
                    ? Vector3.Normalize(cross)
                    : Vector3.Zero;
                if (MathF.Abs(normal.Y) < 0.5f)
                    continue;
                candidates.Add(
                    $"      collision=[{collision.CollisionInfoObjectIndex}] " +
                    $"meshBV=[{collision.MeshBoundingVolumeObjectIndex}] " +
                    $"triangle={index / 3}; center={center}; normal={normal}; " +
                    $"A={a}; B={b}; C={c}");
            }
        }
        Console.WriteLine($"    nearby mostly-horizontal triangles={candidates.Count}");
        foreach (string candidate in candidates.Take(24))
            Console.WriteLine(candidate);
    }

    private static IEnumerable<CollisionComponent> FindCollisionComponents(
        SmoCollisionMesh collision)
    {
        int[] parents = Enumerable.Range(0, collision.Positions.Count).ToArray();
        int Find(int value)
        {
            while (parents[value] != value)
            {
                parents[value] = parents[parents[value]];
                value = parents[value];
            }
            return value;
        }
        void Union(int left, int right)
        {
            left = Find(left);
            right = Find(right);
            if (left != right)
                parents[right] = left;
        }

        for (int index = 0; index < collision.TriangleIndices.Count; index += 3)
        {
            int a = collision.TriangleIndices[index];
            Union(a, collision.TriangleIndices[index + 1]);
            Union(a, collision.TriangleIndices[index + 2]);
        }

        Vector3[] worldPositions = collision.Positions
            .Select(position => Vector3.Transform(position, collision.WorldTransform))
            .ToArray();
        var boundsByRoot = new Dictionary<int, Bounds3>();
        var trianglesByRoot = new Dictionary<int, int>();
        for (int index = 0; index < collision.TriangleIndices.Count; index += 3)
        {
            int root = Find(collision.TriangleIndices[index]);
            Bounds3 bounds = boundsByRoot.GetValueOrDefault(root, Bounds3.Empty);
            bounds = bounds.Include(worldPositions[collision.TriangleIndices[index]]);
            bounds = bounds.Include(worldPositions[collision.TriangleIndices[index + 1]]);
            bounds = bounds.Include(worldPositions[collision.TriangleIndices[index + 2]]);
            boundsByRoot[root] = bounds;
            trianglesByRoot[root] = trianglesByRoot.GetValueOrDefault(root) + 1;
        }

        int componentIndex = 0;
        foreach ((int root, Bounds3 bounds) in boundsByRoot.OrderBy(item => item.Key))
        {
            yield return new CollisionComponent(
                collision.CollisionInfoObjectIndex,
                collision.MeshBoundingVolumeObjectIndex,
                collision.Name,
                componentIndex++,
                trianglesByRoot[root],
                bounds);
        }
    }

    private static bool IsDescendantOf(
        IReadOnlyList<SmoObjectEntry> objects,
        SmoObjectEntry entry,
        int ancestorIndex)
    {
        SmoObjectEntry? cursor = entry;
        var visited = new HashSet<int>();
        while (cursor is not null && visited.Add(cursor.Index))
        {
            if (cursor.Index == ancestorIndex)
                return true;
            cursor = cursor.ParentIndex is int parentIndex &&
                     (uint)parentIndex < (uint)objects.Count
                ? objects[parentIndex]
                : null;
        }
        return false;
    }

    private static void CompareCollisionMeshes(
        SmoDocument original,
        ReadOnlySpan<byte> modifiedData)
    {
        SmoObjectEntry[] meshes = original.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.MeshBoundingVolume)
            .OrderBy(entry => entry.PhysicalOffset)
            .ToArray();
        int[] occurrences = FindSignatures(
            modifiedData,
            checked((int)original.Header.DataStart),
            SmoClassIds.MeshBoundingVolume);
        var changed = new List<int>();
        int comparable = Math.Min(meshes.Length, occurrences.Length);
        for (int index = 0; index < comparable; index++)
        {
            SmoObjectEntry mesh = meshes[index];
            int size = checked((int)mesh.SerializedSize);
            if (occurrences[index] > modifiedData.Length - size ||
                !original.Data.Span.Slice(
                    checked((int)mesh.PhysicalOffset), size)
                .SequenceEqual(modifiedData.Slice(occurrences[index], size)))
            {
                changed.Add(mesh.Index);
            }
        }
        Console.WriteLine(
            $"Collision meshes: directory={meshes.Length}, " +
            $"physical signatures={occurrences.Length}, " +
            $"byte-identical={comparable - changed.Count}, changed={changed.Count}");
        if (changed.Count > 0)
            Console.WriteLine($"  changed spMeshBV indices: {string.Join(",", changed)}");
    }

    private static int[] FindSignatures(
        ReadOnlySpan<byte> data,
        int start,
        uint classId)
    {
        Span<byte> signature = stackalloc byte[8];
        BinaryPrimitives.WriteUInt32LittleEndian(signature, classId);
        "SBOO"u8.CopyTo(signature[4..]);
        var result = new List<int>();
        int cursor = start;
        while (cursor <= data.Length - signature.Length)
        {
            int relative = data[cursor..].IndexOf(signature);
            if (relative < 0)
                break;
            cursor += relative;
            result.Add(cursor);
            cursor += signature.Length;
        }
        return result.ToArray();
    }

    private static int FirstDifference(ReadOnlySpan<byte> a, ReadOnlySpan<byte> b)
    {
        int length = Math.Min(a.Length, b.Length);
        for (int index = 0; index < length; index++)
        {
            if (a[index] != b[index])
                return index;
        }
        return a.Length == b.Length ? -1 : length;
    }

    private static Vector3 ReadMatrixTranslation(ReadOnlySpan<byte> matrix) =>
        new(ReadSingle(matrix, 48), ReadSingle(matrix, 52), ReadSingle(matrix, 56));

    private static float ReadSingle(ReadOnlySpan<byte> data, int offset) =>
        BitConverter.Int32BitsToSingle(
            BinaryPrimitives.ReadInt32LittleEndian(data.Slice(offset, sizeof(float))));

    private static void PrintCollisionDetails(SmoDocument document)
    {
        IReadOnlyList<SmoCollisionMesh> decoded =
            SmoCollisionMeshDecoder.DecodeAll(document);
        Console.WriteLine(
            $"Decoded collision meshes: {decoded.Count}; " +
            $"triangles={decoded.Sum(item => item.TriangleIndices.Count / 3)}; " +
            $"vertices={decoded.Sum(item => item.Positions.Count)}; " +
            $"writableNodes={decoded.Select(item => item.NodeObjectIndex).Distinct().Count(index =>
                SmoPlacementTransformWriter.CanWriteNodeTransform(document, index))}; " +
            $"writableCollisionMeshes={decoded.Count(item =>
                SmoPlacementTransformWriter.CanWriteCollisionTransform(
                    document, item.CollisionInfoObjectIndex))}");
        Console.WriteLine(
            "Collision sectors: " +
            string.Join(", ", decoded
                .GroupBy(item => FindSector(document, item.NodeObjectIndex))
                .OrderBy(group => group.Key)
                .Select(group => $"{group.Key}:{group.Count()}")));
        SmoObjectEntry[] collisions = document.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.CollisionInfo)
            .ToArray();
        Console.WriteLine($"Collision objects: {collisions.Length}");
        foreach (SmoObjectEntry collision in collisions.Take(24))
        {
            SmoObjectEntry? parent = collision.ParentIndex is int parentIndex
                ? document.Objects[parentIndex]
                : null;
            string transform = parent is not null &&
                SmoNodeTransformDecoder.TryDecode(
                    document, parent, out SmoNodeTransform? nodeTransform) &&
                nodeTransform is not null
                    ? $"P={nodeTransform.Position} Q={nodeTransform.Rotation} S={nodeTransform.Scale}"
                    : "NO_NODE_TRANSFORM";
            string children = string.Join(", ", document.Objects
                .Where(entry => entry.ParentIndex == collision.Index)
                .Select(entry =>
                    $"[{entry.Index}] {SmoClassRegistry.GetDisplayName(entry.TypeHash)} " +
                    $"\"{entry.Name.TrimEnd('\0')}\" size=0x{entry.SerializedSize:X}"));
            Console.WriteLine(
                $"  [{collision.Index}] \"{collision.Name.TrimEnd('\0')}\" " +
                $"parent=[{parent?.Index}] {parent?.Name.TrimEnd('\0')} " +
                $"offset=0x{collision.PhysicalOffset:X} " +
                $"size=0x{collision.SerializedSize:X}; {transform}; children={children}");
            if (collision.PhysicalOffset <= int.MaxValue &&
                collision.SerializedSize <= int.MaxValue)
            {
                ReadOnlySpan<byte> data = document.Data.Span.Slice(
                    checked((int)collision.PhysicalOffset),
                    checked((int)collision.SerializedSize));
                int offset = 8;
                var fields = new List<string>();
                while (offset < data.Length &&
                       SmoDataBlockReader.TryReadHeader(
                           data, offset, out SmoDataBlockHeader field))
                {
                    fields.Add($"type={field.FieldType}/size=0x{field.PayloadSize:X}");
                    int next = checked((int)field.PayloadEnd);
                    if (next <= offset)
                        break;
                    offset = next;
                }
                Console.WriteLine($"      fields: {string.Join(", ", fields)}");
            }
            foreach (SmoObjectEntry child in document.Objects.Where(entry =>
                         entry.ParentIndex == collision.Index).Take(2))
            {
                ReadOnlySpan<byte> childData = document.Data.Span.Slice(
                    checked((int)child.PhysicalOffset),
                    checked((int)child.SerializedSize));
                int childOffset = 8;
                var childFields = new List<string>();
                while (childOffset < childData.Length &&
                       SmoDataBlockReader.TryReadHeader(
                           childData, childOffset, out SmoDataBlockHeader field))
                {
                    childFields.Add(
                        $"@{field.Offset:X}:type={field.FieldType}/" +
                        $"size=0x{field.PayloadSize:X}");
                    int next = checked((int)field.PayloadEnd);
                    if (next <= childOffset)
                        break;
                    childOffset = next;
                }
                Console.WriteLine(
                    $"      child [{child.Index}] offset=0x{child.PhysicalOffset:X} " +
                    $"fields: {string.Join(", ", childFields)}; " +
                    $"head={Convert.ToHexString(childData[..Math.Min(childData.Length, 96)])}");
            }
        }
    }

    private static string FindSector(SmoDocument document, int objectIndex)
    {
        SmoObjectEntry? cursor = document.Objects[objectIndex];
        var visited = new HashSet<int>();
        while (cursor is not null && visited.Add(cursor.Index))
        {
            string name = cursor.Name.TrimEnd('\0');
            if (name.StartsWith("sector", StringComparison.OrdinalIgnoreCase))
                return $"{name}[{cursor.Index}]";
            cursor = cursor.ParentIndex is int parentIndex &&
                     (uint)parentIndex < (uint)document.Objects.Count
                ? document.Objects[parentIndex]
                : null;
        }
        return "unassigned";
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
                    document.Header.SerializerVersion,
                    document.Header.PlatformMask,
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
                    $"{state} serializer=0x{result.SerializerVersion:X2} " +
                    $"mask=0x{result.PlatformMask:X2} obj={result.Objects,-5} " +
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

    private static int CreateClassInventory(string path, bool json)
    {
        string fullPath = Path.GetFullPath(path);
        bool singleFile = File.Exists(fullPath);
        if (!singleFile && !Directory.Exists(fullPath))
        {
            throw new DirectoryNotFoundException(
                $"SMO file or directory not found: {fullPath}");
        }

        string[] files = singleFile
            ? [fullPath]
            : Directory.EnumerateFiles(
                    fullPath, "*.smo", SearchOption.AllDirectories)
                .Order(StringComparer.OrdinalIgnoreCase)
                .ToArray();
        var classes = new Dictionary<uint, ClassInventoryAccumulator>();
        var failures = new List<ClassInventoryFailure>();
        long objectCount = 0;
        int parsedFiles = 0;
        foreach (string file in files)
        {
            string displayPath = singleFile
                ? Path.GetFileName(file)
                : Path.GetRelativePath(fullPath, file);
            try
            {
                SmoDocument document = SmoDocument.Load(file);
                parsedFiles++;
                objectCount += document.Objects.Count;
                var seenClasses = new HashSet<uint>();
                foreach (SmoObjectEntry entry in document.Objects)
                {
                    if (!classes.TryGetValue(
                            entry.TypeHash, out ClassInventoryAccumulator? item))
                    {
                        item = new ClassInventoryAccumulator(entry.TypeHash);
                        classes.Add(entry.TypeHash, item);
                    }
                    item.ObjectCount++;
                    if (seenClasses.Add(entry.TypeHash))
                        item.FileCount++;
                    string name = entry.Name.TrimEnd('\0');
                    item.Names[name] = item.Names.GetValueOrDefault(name) + 1;
                }
            }
            catch (Exception exception) when (
                exception is IOException or UnauthorizedAccessException or SmoFormatException)
            {
                failures.Add(new ClassInventoryFailure(displayPath, exception.Message));
            }
        }

        ClassInventoryItem[] inventory = classes.Values
            .OrderByDescending(item => item.ObjectCount)
            .ThenBy(item => item.TypeHash)
            .Select(item =>
            {
                bool known = SmoClassRegistry.TryGetName(
                    item.TypeHash, out string? className);
                string[] sampleNames = item.Names
                    .Where(pair => pair.Key.Length > 0)
                    .OrderByDescending(pair => pair.Value)
                    .ThenBy(pair => pair.Key, StringComparer.OrdinalIgnoreCase)
                    .Take(5)
                    .Select(pair => pair.Key)
                    .ToArray();
                return new ClassInventoryItem(
                    item.TypeHash,
                    className ?? $"0x{item.TypeHash:X8}",
                    known,
                    item.ObjectCount,
                    item.FileCount,
                    sampleNames);
            })
            .ToArray();
        var result = new ClassInventoryResult(
            fullPath,
            files.Length,
            parsedFiles,
            objectCount,
            inventory.Count(item => item.Known),
            inventory.Count(item => !item.Known),
            inventory,
            failures);

        if (json)
        {
            Console.WriteLine(JsonSerializer.Serialize(result, JsonOptions));
        }
        else
        {
            Console.WriteLine($"Root: {result.Root}");
            Console.WriteLine(
                $"Files: {result.ParsedFiles}/{result.Files}; " +
                $"objects: {result.Objects}; classes: {result.Classes.Count}; " +
                $"known: {result.KnownClasses}; unknown: {result.UnknownClasses}");
            Console.WriteLine();
            Console.WriteLine("Hash       Class                         Objects  Files  Examples");
            foreach (ClassInventoryItem item in result.Classes)
            {
                string examples = item.SampleNames.Count > 0
                    ? string.Join(", ", item.SampleNames)
                    : "<unnamed>";
                Console.WriteLine(
                    $"0x{item.TypeHash:X8} {item.Name,-29} " +
                    $"{item.ObjectCount,7}  {item.FileCount,5}  {examples}");
            }
            foreach (ClassInventoryFailure failure in failures)
                Console.WriteLine($"FAIL {failure.Path}: {failure.Error}");
        }

        return failures.Count > 0 || result.UnknownClasses > 0 ? 1 : 0;
    }

    private static int UpdateCorpusDatabase(
        string databasePath,
        string sourcePath,
        bool json)
    {
        SmoCorpusUpdateResult result = SmoCorpusDatabase.Update(
            databasePath, sourcePath);
        if (json)
        {
            Console.WriteLine(JsonSerializer.Serialize(result, JsonOptions));
        }
        else
        {
            Console.WriteLine($"Database: {result.DatabasePath}");
            Console.WriteLine($"Source: {result.SourceRoot}");
            Console.WriteLine(
                $"Files: {result.DiscoveredFiles}; scanned={result.ScannedFiles}; " +
                $"unchanged={result.UnchangedFiles}; failed={result.FailedFiles}; " +
                $"removed={result.RemovedFiles}");
            Console.WriteLine(
                $"Objects: {result.Objects}; direct fields: {result.DirectFields}; " +
                $"classes: known={result.KnownClasses}, unknown={result.UnknownClasses}");
            Console.WriteLine(
                $"Database bytes: {result.DatabaseBytes}; " +
                $"elapsed: {result.Elapsed.TotalSeconds:F2}s");
        }
        return result.FailedFiles > 0 || result.UnknownClasses > 0 ? 1 : 0;
    }

    private static int ShowCorpusDatabaseSummary(string databasePath, bool json)
    {
        SmoCorpusSummary result = SmoCorpusDatabase.GetSummary(databasePath);
        if (json)
        {
            Console.WriteLine(JsonSerializer.Serialize(result, JsonOptions));
        }
        else
        {
            Console.WriteLine($"Database: {result.DatabasePath}");
            Console.WriteLine($"Source: {result.SourceRoot ?? "<unknown>"}");
            Console.WriteLine(
                $"Files: {result.ParsedFiles}/{result.Files}; failed={result.FailedFiles}");
            Console.WriteLine(
                $"Objects: {result.Objects}; direct fields: {result.DirectFields}; " +
                $"classes: {result.Classes} " +
                $"(known={result.KnownClasses}, unknown={result.UnknownClasses})");
            Console.WriteLine(
                $"Variants: {result.VariantDefinitions} definitions / " +
                $"{result.VariantAssignments} assignments; " +
                $"field definitions: {result.FieldDefinitions}");
            Console.WriteLine($"Database bytes: {result.DatabaseBytes}");
        }
        return result.FailedFiles > 0 || result.UnknownClasses > 0 ? 1 : 0;
    }

    private static int ShowCorpusDatabaseClasses(string databasePath, bool json)
    {
        IReadOnlyList<SmoCorpusClassSummary> result =
            SmoCorpusDatabase.GetClasses(databasePath);
        if (json)
        {
            Console.WriteLine(JsonSerializer.Serialize(result, JsonOptions));
        }
        else
        {
            Console.WriteLine("Hash       Class                         Objects  Files  Families");
            foreach (SmoCorpusClassSummary item in result)
            {
                Console.WriteLine(
                    $"0x{item.TypeHash:X8} " +
                    $"{item.EngineName ?? "<unknown>",-29} " +
                    $"{item.ObjectCount,7}  {item.FileCount,5}  {item.FamilyCount,8}");
            }
        }
        return result.Any(item => !item.IsKnown) ? 1 : 0;
    }

    private static int ShowCorpusDatabaseMetrics(string databasePath, bool json)
    {
        IReadOnlyList<SmoCorpusClassMetrics> result =
            SmoCorpusDatabase.GetClassMetrics(databasePath);
        if (json)
        {
            Console.WriteLine(JsonSerializer.Serialize(result, JsonOptions));
        }
        else
        {
            Console.WriteLine(
                "Hash       Class                         Objects Files " +
                "Sizes Shapes Variants Fields(avg/max) Bytes(min/max)");
            foreach (SmoCorpusClassMetrics item in result)
            {
                Console.WriteLine(
                    $"0x{item.TypeHash:X8} {item.EngineName ?? "<unknown>",-29} " +
                    $"{item.ObjectCount,7} {item.FileCount,5} " +
                    $"{item.SerializedSizeCount,5} {item.FieldShapeCount,6} " +
                    $"{item.StructuralVariantCount,8} " +
                    $"{item.AverageFieldCount,5:F1}/{item.MaximumFieldCount,-3} " +
                    $"{item.MinimumSerializedSize}/{item.MaximumSerializedSize}");
            }
        }
        return result.Any(item => item.FieldParseErrors > 0) ? 1 : 0;
    }

    private static int InspectPckDirectory(
        string directoryPath,
        bool parseSmo,
        bool json)
    {
        string root = Path.GetFullPath(directoryPath);
        if (!Directory.Exists(root))
            throw new DirectoryNotFoundException($"PCK directory not found: {root}");
        string[] paths = Directory.EnumerateFiles(
                root, "*.pck", SearchOption.AllDirectories)
            .Order(StringComparer.OrdinalIgnoreCase)
            .ToArray();
        var extensions = new Dictionary<string, long>(StringComparer.OrdinalIgnoreCase);
        var platformMasks = new Dictionary<uint, long>();
        long entries = 0;
        long smoFiles = 0;
        long smoObjects = 0;
        var failures = new List<PckInventoryFailure>();
        foreach (string path in paths)
        {
            SparkplugPckArchive archive = SparkplugPckArchive.Load(path);
            entries += archive.Entries.Count;
            FileStream? stream = parseSmo
                ? new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read)
                : null;
            try
            {
                foreach (SparkplugPckEntry entry in archive.Entries)
                {
                    string extension = Path.GetExtension(entry.FileName).ToLowerInvariant();
                    extensions[extension] = extensions.GetValueOrDefault(extension) + 1;
                    if (!extension.Equals(".smo", StringComparison.OrdinalIgnoreCase))
                        continue;
                    smoFiles++;
                    if (stream is null)
                        continue;
                    try
                    {
                        SmoDocument document = SmoDocument.ParseOwned(
                            SparkplugPckArchive.ReadEntry(stream, entry),
                            $"{path}::{entry.LogicalPath}");
                        smoObjects += document.Objects.Count;
                        platformMasks[document.Header.PlatformMask] =
                            platformMasks.GetValueOrDefault(document.Header.PlatformMask) + 1;
                    }
                    catch (Exception exception) when (
                        exception is IOException or InvalidDataException or
                            SmoFormatException or OverflowException)
                    {
                        failures.Add(new PckInventoryFailure(
                            path, entry.Index, entry.LogicalPath, exception.Message));
                    }
                }
            }
            finally
            {
                stream?.Dispose();
            }
        }

        var result = new PckInventoryResult(
            root,
            paths.Length,
            entries,
            smoFiles,
            parseSmo ? smoObjects : null,
            extensions.OrderByDescending(item => item.Value)
                .ThenBy(item => item.Key)
                .ToDictionary(),
            platformMasks.OrderBy(item => item.Key).ToDictionary(),
            failures);
        if (json)
        {
            Console.WriteLine(JsonSerializer.Serialize(result, JsonOptions));
        }
        else
        {
            Console.WriteLine(
                $"PCK: archives={result.Archives}, entries={result.Entries}, " +
                $"SMO={result.SmoFiles}, objects=" +
                $"{(result.SmoObjects?.ToString() ?? "<not parsed>")}, " +
                $"failures={result.Failures.Count}");
            foreach ((string extension, long count) in result.Extensions)
                Console.WriteLine($"  {extension,-8} {count,6}");
            foreach ((uint mask, long count) in result.PlatformMasks)
                Console.WriteLine($"  FFPS platform mask 0x{mask:X}: {count} SMO");
            foreach (PckInventoryFailure failure in result.Failures.Take(20))
            {
                Console.WriteLine(
                    $"  FAIL {Path.GetFileName(failure.ArchivePath)}" +
                    $"[{failure.EntryIndex}] {failure.LogicalPath}: {failure.Error}");
            }
        }
        return failures.Count == 0 ? 0 : 1;
    }

    private static int UpdateResearchDirectory(
        string databasePath,
        string corpusKey,
        string platformKey,
        string provenance,
        string sourceDirectory,
        string executablePath,
        bool json)
    {
        SmoResearchUpdateResult result = SmoResearchDatabase.UpdateDirectory(
            databasePath, corpusKey, platformKey, provenance,
            sourceDirectory, executablePath);
        PrintResearchUpdate(result, json);
        return result.FailedSmo > 0 ? 1 : 0;
    }

    private static int UpdateResearchPck(
        string databasePath,
        string corpusKey,
        string platformKey,
        string provenance,
        string pckDirectory,
        string executablePath,
        bool json)
    {
        SmoResearchUpdateResult result = SmoResearchDatabase.UpdatePckDirectory(
            databasePath, corpusKey, platformKey, provenance,
            pckDirectory, executablePath);
        PrintResearchUpdate(result, json);
        return result.FailedSmo > 0 ? 1 : 0;
    }

    private static void PrintResearchUpdate(
        SmoResearchUpdateResult result,
        bool json)
    {
        if (json)
        {
            Console.WriteLine(JsonSerializer.Serialize(result, JsonOptions));
            return;
        }
        Console.WriteLine(
            $"Research DB: {result.DatabasePath}\n" +
            $"Corpus: {result.CorpusKey} ({result.PlatformKey}, " +
            $"{result.Provenance}, {result.SourceKind})");
        Console.WriteLine(
            $"Containers: {result.Containers}; occurrences={result.Occurrences}; " +
            $"scanned={result.ScannedResources}; unchanged={result.UnchangedResources}; " +
            $"removed={result.RemovedOccurrences}");
        Console.WriteLine(
            $"SMO: unique={result.SmoResources}; parsed={result.ParsedSmo}; " +
            $"failed={result.FailedSmo}; platform-conflicts={result.PlatformConflicts}");
        Console.WriteLine(
            $"Resources: {result.UniqueResources}; objects={result.Objects}; " +
            $"direct fields={result.DirectFields}; elapsed={result.Elapsed.TotalSeconds:F2}s");
    }

    private static int ShowResearchSummary(string databasePath, bool json)
    {
        SmoResearchSummary result = SmoResearchDatabase.GetSummary(databasePath);
        if (json)
        {
            Console.WriteLine(JsonSerializer.Serialize(result, JsonOptions));
        }
        else
        {
            Console.WriteLine(
                $"Research DB schema {result.SchemaVersion}: {result.DatabasePath}");
            foreach (SmoResearchCorpusSummary corpus in result.Corpora)
            {
                Console.WriteLine(
                    $"  {corpus.CorpusKey} ({corpus.PlatformKey}, " +
                    $"{corpus.Provenance}, {corpus.SourceKind})");
                Console.WriteLine(
                    $"    resources={corpus.UniqueResources}, " +
                    $"occurrences={corpus.PhysicalOccurrences}, " +
                    $"SMO={corpus.ParsedSmo}/{corpus.SmoResources}, " +
                    $"failed={corpus.FailedSmo}, conflicts={corpus.PlatformConflicts}");
                Console.WriteLine(
                    $"    objects={corpus.Objects}, fields={corpus.DirectFields}, " +
                    $"classes={corpus.Classes}");
            }
            Console.WriteLine(
                $"Classes: {result.Classes}; common={result.CommonClasses}; " +
                $"PC-only={result.PcOnlyClasses}; PS2-only={result.Ps2OnlyClasses}");
            Console.WriteLine($"Database bytes: {result.DatabaseBytes}");
        }
        return result.Corpora.Any(corpus => corpus.FailedSmo > 0) ? 1 : 0;
    }

    private static int ShowResearchSources(string databasePath, bool json)
    {
        IReadOnlyList<SmoResearchRegisteredSource> result =
            SmoResearchDatabase.GetRegisteredSources(databasePath);
        if (json)
            Console.WriteLine(JsonSerializer.Serialize(result, JsonOptions));
        else
            foreach (SmoResearchRegisteredSource item in result)
                Console.WriteLine(
                    $"{item.CorpusKey} ({item.PlatformKey}, {item.Provenance}, {item.SourceKind})\n" +
                    $"  source: {item.SourceRoot}\n  executable: {item.ExecutablePath ?? "<none>"}");
        return 0;
    }

    private static int ShowResearchFormats(string databasePath, bool json)
    {
        IReadOnlyList<GameResourceFormatSummary> result =
            SmoResearchDatabase.GetResourceFormats(databasePath);
        if (json)
        {
            Console.WriteLine(JsonSerializer.Serialize(result, JsonOptions));
            return 0;
        }
        Console.WriteLine(
            "Format           Category                 Files   Paths     Occ  OK/partial/inventory/error  Extensions");
        foreach (GameResourceFormatSummary item in result)
        {
            Console.WriteLine(
                $"{item.FormatKey,-16} {item.Category,-24} {item.UniqueResourceVersions,6} " +
                $"{item.LogicalPaths,7} {item.PhysicalOccurrences,7}  " +
                $"{item.DecodedResources}/{item.PartialResources}/" +
                $"{item.InventoryOnlyResources}/{item.ErrorResources}  {item.Extensions}");
        }
        return 0;
    }

    private static int ShowFormatResources(
        string databasePath,
        string formatKey,
        bool json)
    {
        IReadOnlyList<GameResourceFileRecord> result =
            SmoResearchDatabase.GetFormatResources(databasePath, formatKey);
        if (json)
        {
            Console.WriteLine(JsonSerializer.Serialize(result, JsonOptions));
        }
        else
        {
            foreach (GameResourceFileRecord item in result)
                Console.WriteLine(
                    $"{item.CorpusKey,-16} {item.DecodeStatus,-18} " +
                    $"{item.ByteSize,10} x{item.PhysicalOccurrences,-4} {item.RelativePath}");
        }
        return result.Count == 0 ? 1 : 0;
    }

    private static int ShowResourceAudit(string databasePath, bool json)
    {
        GameResourceAudit result = SmoResearchDatabase.GetResourceAudit(databasePath);
        if (json)
        {
            Console.WriteLine(JsonSerializer.Serialize(result, JsonOptions));
        }
        else
        {
            Console.WriteLine(
                $"Resources: total={result.TotalResources}, assigned={result.AssignedResources}, " +
                $"unassigned={result.UnassignedResources}, analysis-errors={result.AnalysisErrors}");
            Console.WriteLine(
                $"Dependencies: total={result.Dependencies}, resolved={result.ResolvedDependencies}, " +
                $"ambiguous={result.AmbiguousDependencies}, unresolved={result.UnresolvedDependencies}");
            foreach (GameResourceDependencySummary item in result.DependencyStatuses)
            {
                Console.WriteLine(
                    $"  {item.CorpusKey,-16} {item.SourceFormat,-6} {item.RelationKind,-24} " +
                    $"{item.ResolutionStatus,-28} {item.DependencyCount,6}");
            }
        }
        return result.UnassignedResources == 0 && result.AnalysisErrors == 0 ? 0 : 1;
    }

    private static int ShowResourceErrors(string databasePath, bool json)
    {
        IReadOnlyList<GameResourceAnalysisError> result =
            SmoResearchDatabase.GetResourceAnalysisErrors(databasePath);
        if (json)
        {
            Console.WriteLine(JsonSerializer.Serialize(result, JsonOptions));
        }
        else
        {
            foreach (GameResourceAnalysisError item in result)
                Console.WriteLine(
                    $"{item.CorpusKey} {item.FormatKey} {item.RelativePath}: {item.Error}");
        }
        return result.Count == 0 ? 0 : 1;
    }

    private static int RefreshUnknownResources(string databasePath, bool json)
    {
        int changed = SmoResearchDatabase.RefreshUnknownResourceFormats(databasePath);
        if (json)
            Console.WriteLine(JsonSerializer.Serialize(new { Reclassified = changed }, JsonOptions));
        else
            Console.WriteLine($"Reclassified unknown resources: {changed}");
        return 0;
    }

    private static int ShowResearchClasses(string databasePath, bool json)
    {
        IReadOnlyList<SmoResearchClassCount> result =
            SmoResearchDatabase.GetClassCounts(databasePath);
        if (json)
        {
            Console.WriteLine(JsonSerializer.Serialize(result, JsonOptions));
        }
        else
        {
            foreach (IGrouping<string, SmoResearchClassCount> corpus in
                     result.GroupBy(item => item.CorpusKey))
            {
                Console.WriteLine(corpus.Key);
                Console.WriteLine(
                    "  Hash       Class                         UniqueObj Resources PhysicalObj");
                foreach (SmoResearchClassCount item in corpus)
                {
                    Console.WriteLine(
                        $"  0x{item.TypeHash:X8} " +
                        $"{item.EngineName ?? "<unknown>",-29} " +
                        $"{item.UniqueObjectCount,9} {item.UniqueResourceCount,9} " +
                        $"{item.PhysicalObjectOccurrences,11}");
                }
            }
        }
        return 0;
    }

    private static int ShowResearchClass(
        string databasePath,
        string classIdentifier,
        bool json)
    {
        SmoResearchClassReport result = SmoResearchDatabase.GetClassReport(
            databasePath, classIdentifier);
        if (json)
        {
            Console.WriteLine(JsonSerializer.Serialize(result, JsonOptions));
            return 0;
        }

        Console.WriteLine(
            $"0x{result.TypeHash:X8} {result.EngineName ?? "<unknown>"}");
        Console.WriteLine("Corpora:");
        foreach (SmoResearchClassProfile profile in result.Profiles)
        {
            Console.WriteLine(
                $"  {profile.CorpusKey,-14} {profile.PlatformKey,-3} " +
                $"objects={profile.UniqueObjectCount}, resources={profile.UniqueResourceCount}, " +
                $"physical={profile.PhysicalObjectOccurrences}, named={profile.NamedObjectCount}, " +
                $"size={profile.MinimumSerializedSize}..{profile.MaximumSerializedSize}");
        }
        Console.WriteLine("Resources:");
        foreach (SmoResearchClassResource resource in result.Resources.Take(100))
        {
            Console.WriteLine(
                $"  {resource.CorpusKey,-14} objects={resource.UniqueObjectCount,6} " +
                $"physical={resource.PhysicalObjectOccurrences,7} " +
                resource.RelativePath);
        }
        if (result.Resources.Count > 100)
        {
            Console.WriteLine(
                $"  ... {result.Resources.Count - 100} more; use --json for the full list");
        }
        Console.WriteLine("Same-path resources on the other platform:");
        foreach (SmoResearchClassCounterpart counterpart in result.Counterparts.Take(100))
        {
            Console.WriteLine(
                $"  {counterpart.SourceCorpusKey} -> {counterpart.CandidateCorpusKey} " +
                $"objects={counterpart.CandidateObjectCount}, " +
                $"same-class={counterpart.CandidateClassObjectCount}, " +
                $"occurrences={counterpart.PhysicalOccurrences} " +
                counterpart.CandidateRelativePath);
        }
        if (result.Counterparts.Count > 100)
        {
            Console.WriteLine(
                $"  ... {result.Counterparts.Count - 100} more; use --json for the full list");
        }
        Console.WriteLine("Direct object-directory relations:");
        foreach (SmoResearchClassRelation relation in result.Relations)
        {
            string related = relation.RelatedTypeHash.HasValue
                ? $"0x{relation.RelatedTypeHash:X8} " +
                  (relation.RelatedEngineName ?? "<unknown>")
                : "<root>";
            Console.WriteLine(
                $"  {relation.CorpusKey,-14} {relation.Direction,-6} " +
                $"{related,-40} count={relation.RelationCount} " +
                $"resources={relation.ResourceCount}");
        }
        Console.WriteLine("Structural candidates:");
        foreach (SmoResearchClassVariantCandidate variant in result.Variants)
        {
            Console.WriteLine(
                $"  {variant.CorpusKey,-14} size={variant.SerializedSize,6} " +
                $"objects={variant.ObjectCount,6} resources={variant.ResourceCount,4} " +
                $"fields={variant.FieldShape}");
        }
        Console.WriteLine("Direct fields:");
        foreach (SmoResearchClassFieldShape field in result.Fields)
        {
            Console.WriteLine(
                $"  {field.CorpusKey,-14} section={field.SectionIndex} " +
                $"from-end={field.SectionFromEnd} type={field.FieldType} " +
                $"occ={field.Occurrence} size={field.PayloadSize} " +
                $"count={field.OccurrenceCount} resources={field.ResourceCount} " +
                $"semantic={field.SemanticKey ?? "-"} layout={field.PayloadLayout ?? "-"}");
        }
        Console.WriteLine("Representative field payloads (up to five distinct values per shape):");
        foreach (SmoResearchClassFieldSample sample in result.FieldSamples)
        {
            Console.WriteLine(
                $"  {sample.CorpusKey,-14} from-end={sample.SectionFromEnd} " +
                $"type={sample.FieldType} occ={sample.Occurrence} " +
                $"size={sample.PayloadSize} hex={sample.PayloadPreviewHex} " +
                $"decoded={sample.DecodedValue ?? "-"} " +
                $"{sample.RelativePath} [{sample.ObjectIndex}] {sample.ObjectName}");
        }
        Console.WriteLine("Examples (up to twelve per corpus):");
        foreach (SmoResearchClassExample example in result.Examples)
        {
            string parent = example.ParentIndex.HasValue
                ? $"parent=[{example.ParentIndex}] {example.ParentName ?? "<unnamed>"} " +
                  $"0x{example.ParentTypeHash.GetValueOrDefault():X8}"
                : "parent=<root>";
            Console.WriteLine(
                $"  {example.CorpusKey,-14} {example.RelativePath} " +
                $"[{example.ObjectIndex}] id={example.ObjectId} " +
                $"'{example.ObjectName}' {parent}");
        }
        return 0;
    }

    private static int AnalyzeResearchClass(
        string databasePath,
        string classIdentifier,
        bool json)
    {
        SmoResearchClassAnalysisResult result = SmoResearchDatabase.AnalyzeClass(
            databasePath,classIdentifier);
        if (json)
        {
            Console.WriteLine(JsonSerializer.Serialize(result, JsonOptions));
        }
        else
        {
            Console.WriteLine(
                $"0x{result.TypeHash:X8} {result.EngineName}: {result.Status}");
            Console.WriteLine(
                $"Corpora={result.Corpora}; objects={result.UniqueObjects}; " +
                $"resources={result.UniqueResources}; " +
                $"distinct payloads={result.DistinctPayloads}");
            Console.WriteLine(
                $"Variant assignments={result.VariantAssignments}; " +
                $"evidence rows={result.EvidenceRows}");
            Console.WriteLine(result.Notes);
        }
        return 0;
    }

    private static int ImportResearchEvidence(
        string databasePath,
        string evidencePath,
        bool json)
    {
        string source = Path.GetFullPath(evidencePath);
        if (!File.Exists(source))
            throw new FileNotFoundException("Runtime evidence file not found.", source);
        IReadOnlyList<SmoResearchRuntimeEvidence> records =
            JsonSerializer.Deserialize<List<SmoResearchRuntimeEvidence>>(
                File.ReadAllText(source), JsonOptions) ??
            throw new InvalidDataException("Runtime evidence JSON is null.");
        int imported = SmoResearchDatabase.ImportRuntimeEvidence(databasePath, records);
        if (json)
        {
            Console.WriteLine(JsonSerializer.Serialize(new
            {
                DatabasePath = Path.GetFullPath(databasePath),
                EvidencePath = source,
                ImportedRows = imported
            }, JsonOptions));
        }
        else
        {
            Console.WriteLine($"Imported runtime evidence rows: {imported}");
            Console.WriteLine($"Source: {source}");
        }
        return 0;
    }

    private static int AnalyzeAllResearchClasses(string databasePath, bool json)
    {
        var results = new List<SmoResearchClassAnalysisResult>(
            ResearchAnalysisOrder.Length);
        for (int index = 0; index < ResearchAnalysisOrder.Length; index++)
        {
            string engineName = ResearchAnalysisOrder[index];
            if (!json)
            {
                Console.WriteLine(
                    $"[{index + 1}/{ResearchAnalysisOrder.Length}] {engineName}");
                Console.Out.Flush();
            }
            SmoResearchClassAnalysisResult result =
                SmoResearchDatabase.AnalyzeClass(databasePath, engineName);
            results.Add(result);
            if (!json)
            {
                Console.WriteLine(
                    $"  {result.Status}: objects={result.UniqueObjects}, " +
                    $"resources={result.UniqueResources}, variants={result.VariantAssignments}, " +
                    $"evidence={result.EvidenceRows}");
                Console.WriteLine($"  {result.Notes}");
                Console.Out.Flush();
            }
        }
        if (json)
            Console.WriteLine(JsonSerializer.Serialize(results, JsonOptions));
        return 0;
    }

    private static int ShowResearchConflicts(string databasePath, bool json)
    {
        IReadOnlyList<SmoResearchPlatformConflict> result =
            SmoResearchDatabase.GetPlatformConflicts(databasePath);
        if (json)
        {
            Console.WriteLine(JsonSerializer.Serialize(result, JsonOptions));
        }
        else
        {
            Console.WriteLine($"Platform-tag conflicts: {result.Count}");
            foreach (SmoResearchPlatformConflict item in result)
            {
                Console.WriteLine(
                    $"  {item.CorpusKey}: {item.RelativePath} " +
                    $"mask=0x{item.PlatformMask:X} target={item.ResourcePlatformKey}");
            }
        }
        return 0;
    }

    private static int ShowResearchResources(
        string databasePath,
        string pathSubstring,
        bool json)
    {
        IReadOnlyList<SmoResearchResourceMatch> result =
            SmoResearchDatabase.FindResources(databasePath, pathSubstring);
        if (json)
        {
            Console.WriteLine(JsonSerializer.Serialize(result, JsonOptions));
        }
        else
        {
            Console.WriteLine($"Resource matches: {result.Count}");
            foreach (SmoResearchResourceMatch item in result)
            {
                Console.WriteLine(
                    $"  {item.CorpusKey,-14} {item.RelativePath} " +
                    $"bytes={item.ByteSize} objects={item.ObjectCount} " +
                    $"occurrences={item.PhysicalOccurrences} " +
                    $"serializer={(item.SerializerVersion.HasValue ? $"0x{item.SerializerVersion:X}" : "-")} " +
                    $"word08={(item.Unknown08.HasValue ? $"0x{item.Unknown08:X8}" : "-")} " +
                    $"mask={(item.PlatformMask.HasValue ? $"0x{item.PlatformMask:X}" : "-")} " +
                    $"sha={item.Sha256[..Math.Min(12, item.Sha256.Length)]}");
            }
        }
        return 0;
    }

    private static int ShowResearchHeaders(string databasePath, bool json)
    {
        IReadOnlyList<SmoResearchHeaderProfile> result =
            SmoResearchDatabase.GetHeaderProfiles(databasePath);
        if (json)
        {
            Console.WriteLine(JsonSerializer.Serialize(result, JsonOptions));
        }
        else
        {
            Console.WriteLine($"FFPS header profiles: {result.Count}");
            foreach (SmoResearchHeaderProfile item in result)
            {
                Console.WriteLine(
                    $"  {item.CorpusKey,-14} serializer=0x{item.SerializerVersion:X} " +
                    $"mask=0x{item.PlatformMask:X} resources={item.ResourceCount,-4} " +
                    $"word08 distinct={item.DistinctUnknown08,-4} " +
                    $"range=0x{item.MinimumUnknown08:X8}..0x{item.MaximumUnknown08:X8}");
            }
        }
        return 0;
    }

    private static int CheckResearchIntegrity(string databasePath, bool json)
    {
        string result = SmoResearchDatabase.CheckIntegrity(databasePath);
        if (json)
            Console.WriteLine(JsonSerializer.Serialize(new { Result = result }, JsonOptions));
        else
            Console.WriteLine($"SQLite integrity_check: {result}");
        return result.Equals("ok", StringComparison.OrdinalIgnoreCase) ? 0 : 1;
    }

    private static int CompareResearchCorpora(
        string databasePath,
        string leftCorpus,
        string rightCorpus,
        bool json)
    {
        SmoResearchCorpusComparison result = SmoResearchDatabase.CompareCorpora(
            databasePath, leftCorpus, rightCorpus);
        if (json)
        {
            Console.WriteLine(JsonSerializer.Serialize(result, JsonOptions));
        }
        else
        {
            Console.WriteLine($"{result.LeftCorpus} <-> {result.RightCorpus}");
            Console.WriteLine(
                $"Paths: {result.LeftPaths}/{result.RightPaths}; common={result.CommonPaths}; " +
                $"identical={result.IdenticalPaths}; changed={result.ChangedPaths}; " +
                $"left-only={result.LeftOnlyPaths}; right-only={result.RightOnlyPaths}");
            Console.WriteLine("Differences by status and extension:");
            foreach (var group in result.Differences
                         .GroupBy(item => new
                         {
                             item.Status,
                             Extension = Path.GetExtension(item.NormalizedPath)
                         })
                         .OrderBy(item => item.Key.Status, StringComparer.Ordinal)
                         .ThenByDescending(item => item.Count())
                         .ThenBy(item => item.Key.Extension, StringComparer.OrdinalIgnoreCase))
            {
                string extension = string.IsNullOrEmpty(group.Key.Extension)
                    ? "<none>"
                    : group.Key.Extension.ToUpperInvariant();
                Console.WriteLine($"  {group.Key.Status,-15} {extension,-8} {group.Count(),6}");
            }
            Console.WriteLine("First 50 differences:");
            foreach (SmoResearchResourceDifference item in result.Differences.Take(50))
            {
                Console.WriteLine(
                    $"  {item.Status,-15} {item.NormalizedPath} " +
                    $"{item.LeftByteSize?.ToString() ?? "-"}/" +
                    $"{item.RightByteSize?.ToString() ?? "-"} B");
            }
            if (result.Differences.Count > 50)
            {
                Console.WriteLine(
                    $"  ... {result.Differences.Count - 50} more; use --json for the full manifest");
            }
        }
        return 0;
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
            document.Header.SerializerVersion,
            document.Header.Unknown08,
            document.Header.PlatformMask,
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
        Console.WriteLine(
            "  SmoViewer.Inspect class-inventory <file.smo|directory> [--json]");
        Console.WriteLine(
            "  SmoViewer.Inspect corpus-db update <database.sqlite> <file.smo|directory> [--json]");
        Console.WriteLine(
            "  SmoViewer.Inspect corpus-db summary <database.sqlite> [--json]");
        Console.WriteLine(
            "  SmoViewer.Inspect corpus-db classes <database.sqlite> [--json]");
        Console.WriteLine(
            "  SmoViewer.Inspect corpus-db metrics <database.sqlite> [--json]");
        Console.WriteLine(
            "  SmoViewer.Inspect pck-inventory <directory> [--parse-smo] [--json]");
        Console.WriteLine(
            "  SmoViewer.Inspect research-db update-directory <db> <corpus> <pc|ps2> <provenance> <directory> <executable> [--json]");
        Console.WriteLine(
            "  SmoViewer.Inspect research-db update-pck <db> <corpus> <pc|ps2> <provenance> <pck-directory> <executable> [--json]");
        Console.WriteLine(
            "  SmoViewer.Inspect research-db summary <database.sqlite> [--json]");
        Console.WriteLine(
            "  SmoViewer.Inspect research-db sources <database.sqlite> [--json]");
        Console.WriteLine(
            "  SmoViewer.Inspect research-db classes <database.sqlite> [--json]");
        Console.WriteLine(
            "  SmoViewer.Inspect research-db headers <database.sqlite> [--json]");
        Console.WriteLine(
            "  SmoViewer.Inspect research-db formats <database.sqlite> [--json]");
        Console.WriteLine(
            "  SmoViewer.Inspect research-db format <database.sqlite> <format-key> [--json]");
        Console.WriteLine(
            "  SmoViewer.Inspect research-db resource-audit <database.sqlite> [--json]");
        Console.WriteLine(
            "  SmoViewer.Inspect research-db resource-errors <database.sqlite> [--json]");
        Console.WriteLine(
            "  SmoViewer.Inspect research-db refresh-unknown <database.sqlite> [--json]");
        Console.WriteLine(
            "  SmoViewer.Inspect research-db class <database.sqlite> <class-name|0xhash> [--json]");
        Console.WriteLine(
            "  SmoViewer.Inspect research-db analyze-class <database.sqlite> <class-name|0xhash> [--json]");
        Console.WriteLine(
            "  SmoViewer.Inspect research-db analyze-all <database.sqlite> [--json]");
        Console.WriteLine(
            "  SmoViewer.Inspect research-db resources <database.sqlite> <path-substring> [--json]");
        Console.WriteLine(
            "  SmoViewer.Inspect research-db conflicts <database.sqlite> [--json]");
        Console.WriteLine(
            "  SmoViewer.Inspect research-db integrity <database.sqlite> [--json]");
        Console.WriteLine(
            "  SmoViewer.Inspect research-db import-evidence <database.sqlite> <evidence.json> [--json]");
        Console.WriteLine(
            "  SmoViewer.Inspect research-db compare <database.sqlite> <left-corpus> <right-corpus> [--json]");
        Console.WriteLine(
            "  SmoViewer.Inspect animation-bindings <model.smo> <file.san|directory> [--json]");
        Console.WriteLine(
            "  SmoViewer.Inspect animation-coverage <model.smo> <directory> [--json]");
        Console.WriteLine(
            "  SmoViewer.Inspect mesh-inventory <file.smo> [--json]");
        Console.WriteLine("  SmoViewer.Inspect object <file.smo> <object-index>");
        Console.WriteLine("  SmoViewer.Inspect <file.smo> [--json]");
    }

    private sealed record PckInventoryFailure(
        string ArchivePath,
        int EntryIndex,
        string LogicalPath,
        string Error);

    private sealed record MeshInventoryItem(
        int ObjectIndex,
        string Name,
        uint SerializedSize,
        int? VertexCount,
        int? IndexCount,
        int? TriangleCount,
        int? SerializedStride,
        int? RuntimeStride,
        int? SkinObjectIndex,
        int? PaletteSize,
        string? Error);

    private sealed record PckInventoryResult(
        string Root,
        int Archives,
        long Entries,
        long SmoFiles,
        long? SmoObjects,
        IReadOnlyDictionary<string, long> Extensions,
        IReadOnlyDictionary<uint, long> PlatformMasks,
        IReadOnlyList<PckInventoryFailure> Failures);

    private sealed record InspectionResult(
        string Path,
        int ActualSize,
        uint SerializerVersion,
        uint Unknown08,
        uint PlatformMask,
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

    private sealed record PlacementChange(
        int ObjectIndex,
        string Name,
        Vector3 OldPosition,
        Vector3 NewPosition)
    {
        public Vector3 Delta => NewPosition - OldPosition;
    }

    private sealed record CollisionComponent(
        int CollisionInfoObjectIndex,
        int MeshBoundingVolumeObjectIndex,
        string Name,
        int ComponentIndex,
        int TriangleCount,
        Bounds3 Bounds);

    private readonly record struct Bounds3(Vector3 Minimum, Vector3 Maximum)
    {
        public static Bounds3 Empty => new(
            new Vector3(float.PositiveInfinity),
            new Vector3(float.NegativeInfinity));

        public bool IsValid =>
            Minimum.X <= Maximum.X &&
            Minimum.Y <= Maximum.Y &&
            Minimum.Z <= Maximum.Z;

        public Bounds3 Include(Vector3 point) => !IsValid
            ? new Bounds3(point, point)
            : new Bounds3(Vector3.Min(Minimum, point), Vector3.Max(Maximum, point));

        public float DistanceTo(Bounds3 other)
        {
            float x = MathF.Max(0, MathF.Max(
                Minimum.X - other.Maximum.X,
                other.Minimum.X - Maximum.X));
            float y = MathF.Max(0, MathF.Max(
                Minimum.Y - other.Maximum.Y,
                other.Minimum.Y - Maximum.Y));
            float z = MathF.Max(0, MathF.Max(
                Minimum.Z - other.Maximum.Z,
                other.Minimum.Z - Maximum.Z));
            return MathF.Sqrt(x * x + y * y + z * z);
        }

        public override string ToString() => $"{Minimum}..{Maximum}";
    }

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
        uint SerializerVersion,
        uint PlatformMask,
        int Objects,
        int Meshes,
        int SignatureMismatches,
        int Warnings,
        int Errors,
        string? ErrorMessage);

    private sealed record AnimationBindingResult(
        string Path,
        bool Decoded,
        SmoAnimationBindingAnalysis? Analysis,
        string? Error);

    private sealed record AnimationCoverageFailure(string Path, string Error);

    private sealed record AnimationNodeCoverage(
        int ObjectIndex,
        string ClassName,
        string NodeName,
        int AnimationFiles,
        int TrackOccurrences,
        int PositionKeys,
        int RotationKeys,
        int ScaleKeys,
        int TotalKeys,
        IReadOnlyList<string> SampleFiles);

    private sealed record AnimationCoverageResult(
        string ModelPath,
        string AnimationDirectory,
        int AnimationFiles,
        int DecodedFiles,
        IReadOnlyList<AnimationCoverageFailure> Failures,
        IReadOnlyList<AnimationNodeCoverage> Nodes);

    private sealed class AnimationNodeCoverageAccumulator(SmoObjectEntry entry)
    {
        public int ObjectIndex { get; } = entry.Index;
        public string ClassName { get; } = entry.ClassName ?? $"0x{entry.TypeHash:X8}";
        public string NodeName { get; } = entry.Name.TrimEnd('\0');
        public int AnimationFiles { get; set; }
        public int TrackOccurrences { get; set; }
        public int PositionKeys { get; set; }
        public int RotationKeys { get; set; }
        public int ScaleKeys { get; set; }
        public SortedSet<string> SampleFiles { get; } =
            new(StringComparer.OrdinalIgnoreCase);

        public AnimationNodeCoverage ToResult() => new(
            ObjectIndex,
            ClassName,
            NodeName,
            AnimationFiles,
            TrackOccurrences,
            PositionKeys,
            RotationKeys,
            ScaleKeys,
            PositionKeys + RotationKeys + ScaleKeys,
            SampleFiles.Take(8).ToArray());
    }

    private sealed class ClassInventoryAccumulator(uint typeHash)
    {
        public uint TypeHash { get; } = typeHash;
        public int ObjectCount { get; set; }
        public int FileCount { get; set; }
        public Dictionary<string, int> Names { get; } =
            new(StringComparer.Ordinal);
    }

    private sealed record ClassInventoryResult(
        string Root,
        int Files,
        int ParsedFiles,
        long Objects,
        int KnownClasses,
        int UnknownClasses,
        IReadOnlyList<ClassInventoryItem> Classes,
        IReadOnlyList<ClassInventoryFailure> Failures);

    private sealed record ClassInventoryItem(
        uint TypeHash,
        string Name,
        bool Known,
        int ObjectCount,
        int FileCount,
        IReadOnlyList<string> SampleNames);

    private sealed record ClassInventoryFailure(string Path, string Error);
}
