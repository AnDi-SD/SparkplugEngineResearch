using System.Diagnostics;
using System.Globalization;
using System.Numerics;
using System.Reflection;
using System.Security.Cryptography;
using System.Text.Json;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;
using SmoImporter.Core;
using SmoLVLcreator.Core;
using SmoViewer.Core;

namespace SmoLVLcreator.CoreTests;

/// <summary>
/// End-to-end stress for the project-native editor path. The supervised child
/// owns every large allocation, so a failed experiment cannot exhaust the GUI
/// process or the operating system.
/// </summary>
internal static class SmoProjectRandomStressRunner
{
    private const int DefaultMemoryLimitMb = 1536;
    private static readonly TimeSpan DefaultTimeout = TimeSpan.FromMinutes(45);

    public static int RunSupervised(string[] args)
    {
        string sourcePath = Path.GetFullPath(args[1]);
        string modelsDirectory = Path.GetFullPath(args[2]);
        int changeCount = ParsePositive(args[3], "change count");
        int seed = args.Length >= 5
            ? int.Parse(args[4], CultureInfo.InvariantCulture)
            : RandomNumberGenerator.GetInt32(int.MaxValue);
        string outputRoot = args.Length >= 6
            ? Path.GetFullPath(args[5])
            : Path.GetFullPath(Path.Combine(
                FindRepositoryRoot(),
                "artifacts",
                "stress",
                "SmoLVLcreator-project"));
        int memoryLimitMb = args.Length >= 7
            ? ParsePositive(args[6], "memory limit")
            : DefaultMemoryLimitMb;
        if (!File.Exists(sourcePath))
            throw new FileNotFoundException("Stress source level was not found.", sourcePath);
        if (!Directory.Exists(modelsDirectory))
            throw new DirectoryNotFoundException(modelsDirectory);

        string runDirectory = Path.Combine(
            outputRoot,
            $"{DateTime.Now:yyyyMMdd-HHmmss}-n{changeCount}-seed{seed}");
        Directory.CreateDirectory(runDirectory);
        string reportPath = Path.Combine(runDirectory, "project-stress-report.json");
        string assemblyPath = Assembly.GetExecutingAssembly().Location;
        var startInfo = new ProcessStartInfo("dotnet")
        {
            UseShellExecute = false,
            WorkingDirectory = FindRepositoryRoot()
        };
        startInfo.ArgumentList.Add(assemblyPath);
        startInfo.ArgumentList.Add("--stress-project-save-child");
        startInfo.ArgumentList.Add(sourcePath);
        startInfo.ArgumentList.Add(modelsDirectory);
        startInfo.ArgumentList.Add(changeCount.ToString(CultureInfo.InvariantCulture));
        startInfo.ArgumentList.Add(seed.ToString(CultureInfo.InvariantCulture));
        startInfo.ArgumentList.Add(runDirectory);
        long memoryLimitBytes = checked((long)memoryLimitMb * 1024 * 1024);
        startInfo.Environment["DOTNET_GCHeapHardLimit"] =
            memoryLimitBytes.ToString("X", CultureInfo.InvariantCulture);
        startInfo.Environment["SMOLVLCREATOR_PROJECT_STRESS_REPORT"] = reportPath;

        using Process child = Process.Start(startInfo) ??
            throw new InvalidOperationException("Could not start project stress child.");
        Console.WriteLine(
            $"PROJECT STRESS: pid={child.Id}; changes={changeCount}; seed={seed}; " +
            $"memory-limit={memoryLimitMb} MiB; sandbox={runDirectory}");
        var timer = Stopwatch.StartNew();
        long peakPrivateBytes = 0;
        while (!child.WaitForExit(250))
        {
            long privateBytes;
            try
            {
                child.Refresh();
                privateBytes = child.PrivateMemorySize64;
            }
            catch (InvalidOperationException) when (child.HasExited)
            {
                break;
            }
            peakPrivateBytes = Math.Max(peakPrivateBytes, privateBytes);
            if (privateBytes > memoryLimitBytes)
            {
                child.Kill(entireProcessTree: true);
                child.WaitForExit();
                WriteSupervisorFailure(
                    reportPath, sourcePath, changeCount, seed, memoryLimitMb,
                    peakPrivateBytes, timer.Elapsed,
                    "Private-memory watchdog limit exceeded.");
                Console.Error.WriteLine(
                    $"FAIL: project stress exceeded {memoryLimitMb} MiB and was stopped. " +
                    $"Report: {reportPath}");
                return 2;
            }
            if (timer.Elapsed > DefaultTimeout)
            {
                child.Kill(entireProcessTree: true);
                child.WaitForExit();
                WriteSupervisorFailure(
                    reportPath, sourcePath, changeCount, seed, memoryLimitMb,
                    peakPrivateBytes, timer.Elapsed,
                    $"Timeout after {DefaultTimeout}.");
                Console.Error.WriteLine(
                    $"FAIL: project stress timed out and was stopped. Report: {reportPath}");
                return 3;
            }
        }

        if (child.ExitCode != 0)
        {
            Console.Error.WriteLine(
                $"FAIL: project stress child exited with code {child.ExitCode}. " +
                $"Sandbox retained at {runDirectory}");
            return child.ExitCode;
        }
        if (!File.Exists(reportPath))
            throw new InvalidDataException("Project stress child produced no report.");
        Console.WriteLine(
            $"PASS: project stress finished in {timer.Elapsed}; " +
            $"supervisedPeak={peakPrivateBytes / (1024d * 1024d):N1} MiB; " +
            $"report={reportPath}");
        return 0;
    }

    public static int RunChild(string[] args)
    {
        string sourcePath = Path.GetFullPath(args[1]);
        string modelsDirectory = Path.GetFullPath(args[2]);
        int requestedChanges = ParsePositive(args[3], "change count");
        int seed = int.Parse(args[4], CultureInfo.InvariantCulture);
        string runDirectory = Path.GetFullPath(args[5]);
        string reportPath = Environment.GetEnvironmentVariable(
            "SMOLVLCREATOR_PROJECT_STRESS_REPORT") ??
            Path.Combine(runDirectory, "project-stress-report.json");
        Directory.CreateDirectory(runDirectory);

        using Process process = Process.GetCurrentProcess();
        var totalTimer = Stopwatch.StartNew();
        SmoLevelWorkspace workspace = SmoLevelWorkspace.Load(sourcePath);
        SmoDocument source = workspace.Document;
        var level = new SmoLevelDocument(workspace);
        SmoProject project = SmoProject.Import(source);
        var session = new SmoProjectSession(project);
        var random = new Random(seed);
        var counts = new ProjectStressCounts();
        var operations = new List<string>(requestedChanges);
        var activePlacements = level.Entities
            .Where(entity => entity.Kind == SmoLevelEntityKind.Visual &&
                             entity.Id.SceneObjectIndex >= 0 &&
                             entity.Id.SceneObjectIndex < source.Objects.Count &&
                             source.Objects[entity.Id.SceneObjectIndex].TypeHash ==
                                 SmoClassIds.StaticRenderObject)
            .Select(entity => new ActivePlacement(
                source.Objects[entity.Id.SceneObjectIndex].Id,
                entity.WorldTransform,
                entity.Name))
            .DistinctBy(item => item.ObjectId)
            .ToList();
        uint[] meshResourceIds = workspace.Assets
            .Select(asset => source.Objects[asset.ObjectIndex].Id)
            .Where(id => project.CanAddReferencePlacementForResource(id, out _))
            .Distinct()
            .ToArray();
        uint[] textureIds = source.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.TextureData)
            .Select(entry => entry.Id)
            .ToArray();
        SmoCollisionMesh? collisionTemplate = SmoCollisionMeshDecoder.DecodeAll(source)
            .FirstOrDefault(collision => collision.Positions.Count >= 4 &&
                                         collision.TriangleIndices.Count >= 12);
        ImportedCandidate? imported = LoadImportedCandidate(modelsDirectory);
        SmoWorkerHostInfo workerHost = SmoWorkerHost.ResolveCurrent();
        var importedMeshIds = new List<uint>();
        var activeCollisions = new List<uint>();
        if (activePlacements.Count == 0 || meshResourceIds.Length == 0)
            throw new InvalidDataException("Source has no project-writable placements/resources.");

        void Execute(string label, Action<SmoProject> edit)
        {
            if (!session.Execute(label, edit))
                throw new InvalidOperationException($"Stress operation '{label}' changed nothing.");
            counts.Total++;
            operations.Add($"{counts.Total:D6} {label}");
            if (counts.Total % 100 == 0 || counts.Total == requestedChanges)
            {
                process.Refresh();
                Console.WriteLine(
                    $"EDIT {counts.Total,6}/{requestedChanges} · {label} · " +
                    $"journal≈{EstimateJournalBytes(project) / (1024d * 1024d):N1} MiB · " +
                    $"private={process.PrivateMemorySize64 / (1024d * 1024d):N1} MiB");
            }
        }

        // Guaranteed coverage prelude. Larger runs always exercise every
        // editor mutation category before random repetition begins.
        if (counts.Total < requestedChanges)
        {
            ActivePlacement target = activePlacements[0];
            Matrix4x4 moved = target.World;
            moved.M41 += 11;
            moved.M42 += 2;
            moved.M43 -= 7;
            Execute("prelude visual transform", current =>
                current.SetPlacementTransform(target.ObjectId, moved));
            activePlacements[0] = target with { World = moved };
            counts.Transforms++;
        }
        if (counts.Total < requestedChanges)
        {
            ActivePlacement sourcePlacement = activePlacements[0];
            Matrix4x4 placed = sourcePlacement.World;
            placed.M41 += 75;
            uint newId = 0;
            Execute("prelude reference placement", current =>
                newId = current.AddReferencePlacementForResource(
                    meshResourceIds[0], placed, "StressReference"));
            activePlacements.Add(new ActivePlacement(newId, placed, "StressReference"));
            counts.ReferencePlacements++;
        }
        if (counts.Total < requestedChanges && textureIds.Length > 0)
        {
            byte[] texture = CreateStressTexture(seed, alpha: 91);
            Execute("prelude texture replacement", current =>
                SmoProjectTextureReplacement.Replace(
                    current, textureIds[0], texture, replaceAlpha: true));
            counts.TextureReplacements++;
        }
        if (counts.Total < requestedChanges && imported is not null)
        {
            SmoProjectExternalModelAddition? addition = null;
            Matrix4x4 world = Matrix4x4.CreateScale(imported.SuggestedScale) *
                Matrix4x4.CreateTranslation(-4200, 30, -550);
            Execute("prelude external import", current =>
                addition = SmoProjectImporterBridge.AddExternalModelBatched(
                    current,
                    imported.Scene,
                    [world],
                    imported.Path,
                    imported.Name,
                    workerHost.ExecutablePath,
                    workerHost.ManagedEntryAssemblyPath));
            importedMeshIds.AddRange(addition!.MeshObjectIds);
            activePlacements.AddRange(addition.PlacementRootObjectIds.Select(id =>
                new ActivePlacement(id, world, imported.Name)));
            counts.ExternalImports++;
        }
        if (counts.Total < requestedChanges && imported is not null)
        {
            ActivePlacement replaced = activePlacements.First(item =>
                project.Objects.Any(entry => entry.Id == item.ObjectId));
            SmoProjectExternalModelAddition? addition = null;
            Execute("prelude complete model replacement", current =>
            {
                addition = SmoProjectImporterBridge.AddExternalModelBatched(
                    current,
                    imported.Scene,
                    [replaced.World],
                    imported.Path,
                    $"{imported.Name}_replacement",
                    workerHost.ExecutablePath,
                    workerHost.ManagedEntryAssemblyPath);
                current.RemoveSceneBranches([replaced.ObjectId]);
            });
            activePlacements.RemoveAll(item => item.ObjectId == replaced.ObjectId);
            importedMeshIds.AddRange(addition!.MeshObjectIds);
            activePlacements.AddRange(addition.PlacementRootObjectIds.Select(id =>
                new ActivePlacement(id, replaced.World, $"{imported.Name}_replacement")));
            counts.ModelReplacements++;
        }
        if (counts.Total < requestedChanges && collisionTemplate is not null)
        {
            Vector3[] positions = collisionTemplate.Positions
                .Select(position => Vector3.Transform(
                    position, collisionTemplate.WorldTransform))
                .ToArray();
            SmoProjectCollisionAddition? addition = null;
            Execute("prelude collision add", current =>
                addition = SmoProjectImporterBridge.AddCollision(
                    current,
                    positions,
                    collisionTemplate.TriangleIndices,
                    "StressCollision"));
            activeCollisions.Add(addition!.CollisionInfoObjectId);
            counts.CollisionAdds++;
        }
        if (counts.Total < requestedChanges && activeCollisions.Count > 0)
        {
            uint id = activeCollisions[0];
            Matrix4x4 desired = Matrix4x4.CreateTranslation(4, 2, -3);
            Execute("prelude collision transform", current =>
                current.SetCollisionTransforms(
                [new SmoProjectCollisionTransformEdit(id, Matrix4x4.Identity, desired)]));
            counts.CollisionTransforms++;
        }
        if (counts.Total < requestedChanges && activeCollisions.Count > 0)
        {
            uint id = activeCollisions[0];
            Execute("prelude collision delete", current =>
                current.RemoveSceneBranches([id]));
            activeCollisions.Remove(id);
            counts.CollisionDeletes++;
        }
        if (counts.Total < requestedChanges && activePlacements.Count > 2)
        {
            ActivePlacement removed = activePlacements[^1];
            Execute("prelude placement delete", current =>
                current.RemoveSceneBranches([removed.ObjectId]));
            activePlacements.RemoveAt(activePlacements.Count - 1);
            counts.Deletes++;
        }

        int checkpointOne = Math.Max(1, requestedChanges / 3);
        int checkpointTwo = Math.Max(checkpointOne + 1, requestedChanges * 2 / 3);
        while (counts.Total < requestedChanges)
        {
            int roll = random.Next(100);
            if (roll < 58 || activePlacements.Count < 2)
            {
                int index = random.Next(activePlacements.Count);
                ActivePlacement placement = activePlacements[index];
                Matrix4x4 moved = placement.World;
                moved.M41 += NextFloat(random, -15, 15);
                moved.M42 += NextFloat(random, -4, 4);
                moved.M43 += NextFloat(random, -15, 15);
                Execute("random visual transform", current =>
                    current.SetPlacementTransform(placement.ObjectId, moved));
                activePlacements[index] = placement with { World = moved };
                counts.Transforms++;
            }
            else if (roll < 83)
            {
                ActivePlacement basis = activePlacements[random.Next(activePlacements.Count)];
                uint[] liveImportedMeshes = importedMeshIds
                    .Where(id => project.CanUseMeshResource(id, out _))
                    .ToArray();
                uint[] liveBaseMeshes = meshResourceIds
                    .Where(id => project.CanUseMeshResource(id, out _))
                    .ToArray();
                bool useImported =
                    liveImportedMeshes.Length > 0 && random.Next(4) == 0;
                uint[] candidates = useImported
                    ? liveImportedMeshes
                    : liveBaseMeshes;
                uint meshId = 0;
                bool foundPlaceable = false;
                for (int attempt = 0; attempt < candidates.Length; attempt++)
                {
                    uint candidate = candidates[random.Next(candidates.Length)];
                    if (!project.CanAddReferencePlacementForResource(candidate, out _))
                        continue;
                    meshId = candidate;
                    foundPlaceable = true;
                    break;
                }
                if (!foundPlaceable)
                    continue;
                Matrix4x4 placed = basis.World;
                placed.M41 += NextFloat(random, -250, 250);
                placed.M42 += NextFloat(random, -30, 100);
                placed.M43 += NextFloat(random, -250, 250);
                uint id = 0;
                Execute(
                    $"random reference placement resource=0x{meshId:X8} " +
                    $"origin={(useImported ? "imported" : "source")}", current =>
                    id = current.AddReferencePlacementForResource(
                        meshId, placed, $"StressReference_{counts.ReferencePlacements + 1}"));
                activePlacements.Add(new ActivePlacement(id, placed, "StressReference"));
                counts.ReferencePlacements++;
            }
            else if (roll < 90 && textureIds.Length > 0)
            {
                uint[] liveTextureIds = textureIds
                    .Where(id => project.CanUseObject(
                        id,
                        SmoClassIds.TextureData,
                        out _))
                    .ToArray();
                if (liveTextureIds.Length == 0)
                    continue;
                uint textureId = liveTextureIds[random.Next(liveTextureIds.Length)];
                byte[] texture = CreateStressTexture(
                    random.Next(),
                    (byte)random.Next(32, 256));
                Execute("random texture replacement", current =>
                    SmoProjectTextureReplacement.Replace(
                        current, textureId, texture, replaceAlpha: true));
                counts.TextureReplacements++;
            }
            else if (roll < 96 && activePlacements.Count > 2)
            {
                int[] deletableIndices = activePlacements
                    .Select((item, index) => (item, index))
                    .Where(pair =>
                        project.Objects.Any(entry => entry.Id == pair.item.ObjectId) ||
                        project.Manifest.ReferencePlacements.Any(reference =>
                            reference.NewObjectIds.Count > 0 &&
                            reference.NewObjectIds[0] == pair.item.ObjectId))
                    .Select(pair => pair.index)
                    .ToArray();
                if (deletableIndices.Length == 0)
                    continue;
                int index = deletableIndices[random.Next(deletableIndices.Length)];
                ActivePlacement removed = activePlacements[index];
                Execute("random placement delete", current =>
                    current.RemoveSceneBranches([removed.ObjectId]));
                activePlacements.RemoveAt(index);
                activePlacements.RemoveAll(item =>
                    !IsCurrentPlacementRoot(project, item.ObjectId));
                counts.Deletes++;
            }
            else if (collisionTemplate is not null)
            {
                int collisionAction = activeCollisions.Count == 0
                    ? 0
                    : random.Next(100);
                if (collisionAction < 45)
                {
                    Vector3 delta = new(
                        NextFloat(random, -500, 500),
                        NextFloat(random, -50, 250),
                        NextFloat(random, -500, 500));
                    Vector3[] positions = collisionTemplate.Positions
                        .Select(position => Vector3.Transform(
                            position, collisionTemplate.WorldTransform) + delta)
                        .ToArray();
                    SmoProjectCollisionAddition? addition = null;
                    Execute("random collision add", current =>
                        addition = SmoProjectImporterBridge.AddCollision(
                            current,
                            positions,
                            collisionTemplate.TriangleIndices,
                            $"StressCollision_{counts.CollisionAdds + 1}"));
                    activeCollisions.Add(addition!.CollisionInfoObjectId);
                    counts.CollisionAdds++;
                }
                else if (collisionAction < 78)
                {
                    uint id = activeCollisions[random.Next(activeCollisions.Count)];
                    Matrix4x4 desired = Matrix4x4.CreateTranslation(
                        NextFloat(random, -20, 20),
                        NextFloat(random, -8, 8),
                        NextFloat(random, -20, 20));
                    Execute("random collision transform", current =>
                        current.SetCollisionTransforms(
                        [new SmoProjectCollisionTransformEdit(
                            id, Matrix4x4.Identity, desired)]));
                    counts.CollisionTransforms++;
                }
                else
                {
                    int index = random.Next(activeCollisions.Count);
                    uint id = activeCollisions[index];
                    Execute("random collision delete", current =>
                        current.RemoveSceneBranches([id]));
                    activeCollisions.RemoveAt(index);
                    counts.CollisionDeletes++;
                }
            }

            if (counts.Total == checkpointOne || counts.Total == checkpointTwo)
            {
                string checkpoint = Path.Combine(
                    runDirectory,
                    $"checkpoint-{counts.Total}.smolvlproj");
                SmoProjectArchive.Save(project, checkpoint);
                project = SmoProjectArchive.Load(checkpoint);
                session = new SmoProjectSession(project);
                SmoDocument preview = SmoProjectSerializer.CreateCurrentDocument(project);
                if (preview.HasErrors)
                    throw new InvalidDataException("Checkpoint preview has parser errors.");
                counts.ArchiveCheckpoints++;
            }
        }

        string projectPath = Path.Combine(runDirectory, "stress.smolvlproj");
        string outputPath = Path.Combine(runDirectory, "stress.smo");
        string repeatedPath = Path.Combine(runDirectory, "stress-repeated.smo");
        string reimportedPath = Path.Combine(runDirectory, "stress-reimported.smo");
        var saveTimer = Stopwatch.StartNew();
        SmoProjectArchive.Save(project, projectPath);
        SmoProject reopened = SmoProjectArchive.Load(projectPath);
        SmoProjectBuildResult built = SmoProjectSerializer.Build(reopened, outputPath);
        SmoProjectBuildResult repeated = SmoProjectSerializer.Build(reopened, repeatedPath);
        SmoDocument verified = SmoDocument.Load(outputPath);
        if (verified.HasErrors || built.Sha256 != repeated.Sha256 ||
            !File.ReadAllBytes(outputPath).AsSpan().SequenceEqual(
                File.ReadAllBytes(repeatedPath)))
        {
            throw new InvalidDataException(
                "Repeated project build is invalid or nondeterministic.");
        }
        SmoProject reimported = SmoProject.Import(verified);
        SmoProjectBuildResult reimportedResult =
            SmoProjectSerializer.Build(reimported, reimportedPath);
        if (built.Sha256 != reimportedResult.Sha256)
            throw new InvalidDataException("Re-importing the built SMO changed its bytes.");
        saveTimer.Stop();
        process.Refresh();
        totalTimer.Stop();

        var report = new
        {
            Success = true,
            SourcePath = sourcePath,
            ProjectPath = projectPath,
            OutputPath = outputPath,
            Seed = seed,
            RequestedChanges = requestedChanges,
            AppliedChanges = counts.Total,
            Changes = counts,
            Operations = operations,
            SourceBytes = source.Data.Length,
            OutputBytes = verified.Data.Length,
            SourceObjects = source.Objects.Count,
            OutputObjects = verified.Objects.Count,
            JournalBytes = EstimateJournalBytes(reopened),
            UndoDepthBeforeFinalSave = session.UndoCount,
            BuildSeconds = saveTimer.Elapsed.TotalSeconds,
            TotalSeconds = totalTimer.Elapsed.TotalSeconds,
            PeakWorkingSetBytes = process.PeakWorkingSet64,
            FinalPrivateBytes = process.PrivateMemorySize64,
            OutputSha256 = built.Sha256,
            RepeatedBuildSha256 = repeated.Sha256,
            ReimportedBuildSha256 = reimportedResult.Sha256,
            CompletedUtc = DateTimeOffset.UtcNow
        };
        File.WriteAllText(
            reportPath,
            JsonSerializer.Serialize(report, new JsonSerializerOptions { WriteIndented = true }));
        Console.WriteLine(
            $"PROJECT STRESS PASS: changes={counts.Total}; objects={verified.Objects.Count}; " +
            $"bytes={verified.Data.Length}; build={saveTimer.Elapsed}; " +
            $"peak-working-set={process.PeakWorkingSet64 / (1024d * 1024d):N1} MiB; " +
            $"report={reportPath}");
        return 0;
    }

    private static ImportedCandidate? LoadImportedCandidate(string directory)
    {
        foreach (string path in Directory.EnumerateFiles(
                     directory, "*.*", SearchOption.AllDirectories)
                 .Where(path => Path.GetExtension(path).Equals(
                                    ".obj", StringComparison.OrdinalIgnoreCase) ||
                                Path.GetExtension(path).Equals(
                                    ".glb", StringComparison.OrdinalIgnoreCase))
                 .OrderBy(path => new FileInfo(path).Length)
                 .ThenBy(path => path, StringComparer.OrdinalIgnoreCase))
        {
            try
            {
                ImportedScene scene = ImportedModelReader.Read(path);
                SmoLevelImportValidationReport validation =
                    SmoLevelImportValidator.Validate(
                        scene,
                        SmoLevelImportPurpose.AddExternalModel);
                if (!validation.CanImport || scene.Meshes.Count == 0)
                    continue;
                float scale = Path.GetExtension(path).Equals(
                    ".glb", StringComparison.OrdinalIgnoreCase) ? 100f : 1f;
                Console.WriteLine(
                    $"PROJECT MODEL: {path} · meshes={scene.Meshes.Count} · " +
                    $"vertices={scene.Meshes.Sum(mesh => mesh.Positions.Length):N0} · " +
                    $"textures={scene.Textures.Count}");
                return new ImportedCandidate(
                    path,
                    Path.GetFileNameWithoutExtension(path),
                    scene,
                    scale);
            }
            catch (Exception exception) when (exception is
                       InvalidDataException or NotSupportedException or ArgumentException)
            {
                Console.WriteLine($"PROJECT MODEL SKIP: {path} · {exception.Message}");
            }
        }
        return null;
    }

    private static byte[] CreateStressTexture(int seed, byte alpha)
    {
        var random = new Random(seed);
        using var image = new Image<Rgba32>(4, 4);
        image.ProcessPixelRows(accessor =>
        {
            for (int y = 0; y < accessor.Height; y++)
            {
                Span<Rgba32> row = accessor.GetRowSpan(y);
                for (int x = 0; x < row.Length; x++)
                {
                    row[x] = new Rgba32(
                        (byte)random.Next(256),
                        (byte)random.Next(256),
                        (byte)random.Next(256),
                        alpha);
                }
            }
        });
        using var stream = new MemoryStream();
        image.SaveAsPng(stream);
        return stream.ToArray();
    }

    private static float NextFloat(Random random, float minimum, float maximum) =>
        minimum + (float)random.NextDouble() * (maximum - minimum);

    private static bool IsCurrentPlacementRoot(SmoProject project, uint objectId) =>
        project.Manifest.ReferencePlacements.Any(item =>
            item.NewObjectIds.Count > 0 && item.NewObjectIds[0] == objectId) ||
        project.CanUseObject(
            objectId,
            SmoClassIds.StaticRenderObject,
            out _);

    private static long EstimateJournalBytes(SmoProject project) =>
        project.Manifest.PropertyEdits.Sum(item => (long)item.Value.Length + 64) +
        project.Manifest.AddedObjectPropertyEdits.Sum(item =>
            (long)item.Value.Length + 80) +
        project.Manifest.ReferencePlacements.Sum(item =>
            (long)item.WorldMatrix.Length + item.InverseWorldMatrix.Length +
            item.RootRawName.Length + item.NewObjectIds.Count * sizeof(uint) + 96) +
        project.Manifest.AddedForests.Sum(item =>
            (long)item.Objects.Count * 64 + 96) +
        project.Manifest.ObjectDataReplacements.Count * 80L +
        project.Manifest.BranchRemovals.Count * 24L +
        project.Manifest.ResourceRelocations.Count * 40L +
        project.Manifest.ResourceRedirects.Count * 32L;

    private static void WriteSupervisorFailure(
        string reportPath,
        string sourcePath,
        int changeCount,
        int seed,
        int memoryLimitMb,
        long peakPrivateBytes,
        TimeSpan elapsed,
        string reason)
    {
        File.WriteAllText(
            reportPath,
            JsonSerializer.Serialize(
                new
                {
                    Success = false,
                    Reason = reason,
                    SourcePath = sourcePath,
                    RequestedChanges = changeCount,
                    Seed = seed,
                    MemoryLimitMb = memoryLimitMb,
                    PeakPrivateBytes = peakPrivateBytes,
                    ElapsedSeconds = elapsed.TotalSeconds,
                    CompletedUtc = DateTimeOffset.UtcNow
                },
                new JsonSerializerOptions { WriteIndented = true }));
    }

    private static int ParsePositive(string text, string name)
    {
        if (!int.TryParse(
                text,
                NumberStyles.Integer,
                CultureInfo.InvariantCulture,
                out int value) || value <= 0)
        {
            throw new ArgumentOutOfRangeException(
                name, text, "Value must be a positive integer.");
        }
        return value;
    }

    private static string FindRepositoryRoot()
    {
        DirectoryInfo? cursor = new(AppContext.BaseDirectory);
        while (cursor is not null)
        {
            if (File.Exists(Path.Combine(
                    cursor.FullName,
                    "SparkplugEngineResearch.slnx")))
            {
                return cursor.FullName;
            }
            cursor = cursor.Parent;
        }
        return Directory.GetCurrentDirectory();
    }

    private sealed record ImportedCandidate(
        string Path,
        string Name,
        ImportedScene Scene,
        float SuggestedScale);

    private sealed record ActivePlacement(
        uint ObjectId,
        Matrix4x4 World,
        string Name);

    private sealed class ProjectStressCounts
    {
        public int Total { get; set; }
        public int Transforms { get; set; }
        public int ReferencePlacements { get; set; }
        public int ExternalImports { get; set; }
        public int ModelReplacements { get; set; }
        public int TextureReplacements { get; set; }
        public int Deletes { get; set; }
        public int CollisionAdds { get; set; }
        public int CollisionTransforms { get; set; }
        public int CollisionDeletes { get; set; }
        public int ArchiveCheckpoints { get; set; }
    }
}
