using System.Diagnostics;
using System.Globalization;
using System.Numerics;
using System.Reflection;
using System.Security.Cryptography;
using System.Text.Json;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.Formats.Png;
using SixLabors.ImageSharp.PixelFormats;
using SmoImporter.Core;
using SmoLVLcreator.Core;
using SmoViewer.Core;

namespace SmoLVLcreator.CoreTests;

internal static class SmoLevelRandomStressRunner
{
    private const int DefaultMemoryLimitMb = 2048;
    private static readonly TimeSpan DefaultTimeout = TimeSpan.FromMinutes(30);

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
                "SmoLVLcreator"));
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
        string outputPath = Path.Combine(
            runDirectory,
            $"{Path.GetFileNameWithoutExtension(sourcePath)}-stress.smo");
        string reportPath = Path.Combine(runDirectory, "stress-report.json");

        string assemblyPath = Assembly.GetExecutingAssembly().Location;
        var startInfo = new ProcessStartInfo("dotnet")
        {
            UseShellExecute = false,
            WorkingDirectory = FindRepositoryRoot()
        };
        startInfo.ArgumentList.Add(assemblyPath);
        startInfo.ArgumentList.Add("--stress-random-save-child");
        startInfo.ArgumentList.Add(sourcePath);
        startInfo.ArgumentList.Add(modelsDirectory);
        startInfo.ArgumentList.Add(changeCount.ToString(CultureInfo.InvariantCulture));
        startInfo.ArgumentList.Add(seed.ToString(CultureInfo.InvariantCulture));
        startInfo.ArgumentList.Add(outputPath);
        // The GC hard limit is the first line of defence. The parent also
        // watches total private bytes because native image/model decoders are
        // not fully represented by the managed heap limit.
        long memoryLimitBytes = checked((long)memoryLimitMb * 1024 * 1024);
        startInfo.Environment["DOTNET_GCHeapHardLimit"] =
            memoryLimitBytes.ToString("X", CultureInfo.InvariantCulture);
        startInfo.Environment["SMOLVLCREATOR_STRESS_REPORT"] = reportPath;

        using Process child = Process.Start(startInfo) ??
            throw new InvalidOperationException("Could not start the isolated stress process.");
        Console.WriteLine(
            $"STRESS SUPERVISOR: pid={child.Id}; changes={changeCount}; seed={seed}; " +
            $"memory-limit={memoryLimitMb} MiB; output={outputPath}");
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
                    reportPath,
                    sourcePath,
                    outputPath,
                    changeCount,
                    seed,
                    memoryLimitMb,
                    peakPrivateBytes,
                    timer.Elapsed,
                    "Private-memory watchdog limit exceeded.");
                Console.Error.WriteLine(
                    $"FAIL: isolated stress process exceeded {memoryLimitMb} MiB and was stopped. " +
                    $"The OS and editor were not allowed to absorb the runaway allocation. " +
                    $"Report: {reportPath}");
                return 2;
            }
            if (timer.Elapsed > DefaultTimeout)
            {
                child.Kill(entireProcessTree: true);
                child.WaitForExit();
                WriteSupervisorFailure(
                    reportPath,
                    sourcePath,
                    outputPath,
                    changeCount,
                    seed,
                    memoryLimitMb,
                    peakPrivateBytes,
                    timer.Elapsed,
                    $"Timeout after {DefaultTimeout}.");
                Console.Error.WriteLine(
                    $"FAIL: isolated stress process timed out and was stopped. Report: {reportPath}");
                return 3;
            }
        }

        if (child.ExitCode != 0)
        {
            Console.Error.WriteLine(
                $"FAIL: isolated stress process exited with code {child.ExitCode}. " +
                $"Sandbox retained at {runDirectory}");
            return child.ExitCode;
        }
        if (!File.Exists(outputPath) || !File.Exists(reportPath))
            throw new InvalidDataException("Stress child exited successfully without its artifacts.");
        Console.WriteLine(
            $"PASS: isolated stress save finished in {timer.Elapsed}; " +
            $"supervisedPeak={peakPrivateBytes / (1024d * 1024d):N1} MiB; " +
            $"output={outputPath}; report={reportPath}");
        return 0;
    }

    public static int RunChild(string[] args)
    {
        string sourcePath = Path.GetFullPath(args[1]);
        string modelsDirectory = Path.GetFullPath(args[2]);
        int requestedChanges = ParsePositive(args[3], "change count");
        int seed = int.Parse(args[4], CultureInfo.InvariantCulture);
        string outputPath = Path.GetFullPath(args[5]);
        string reportPath = Environment.GetEnvironmentVariable(
            "SMOLVLCREATOR_STRESS_REPORT") ??
            Path.ChangeExtension(outputPath, ".stress-report.json");
        Directory.CreateDirectory(Path.GetDirectoryName(outputPath)!);

        using Process process = Process.GetCurrentProcess();
        var totalTimer = Stopwatch.StartNew();
        SmoLevelWorkspace workspace = SmoLevelWorkspace.Load(sourcePath);
        var document = new SmoLevelDocument(workspace);
        Random random = new(seed);
        StressChangeCounts counts = ApplyRandomChanges(
            document,
            modelsDirectory,
            requestedChanges,
            random);

        int lastPrintedPercent = -1;
        var progress = new InlineProgress<SmoLevelSaveProgress>(value =>
        {
            int percent = (int)Math.Floor(value.Fraction * 100);
            if (percent == lastPrintedPercent && value.CompletedSteps != value.TotalSteps)
                return;
            lastPrintedPercent = percent;
            process.Refresh();
            Console.WriteLine(
                $"SAVE {percent,3}% · {value.CompletedSteps}/{value.TotalSteps} · " +
                $"{value.Message} · container={value.CurrentContainerBytes / (1024d * 1024d):N1} MiB · " +
                $"private={process.PrivateMemorySize64 / (1024d * 1024d):N1} MiB");
        });

        var saveTimer = Stopwatch.StartNew();
        // The child is already constrained by a GC hard limit and an external
        // watchdog. Bypass the production system-wide preflight here so the
        // stress run measures the pipeline itself instead of ambient desktop
        // memory pressure.
        SmoLevelSaveResult result = SmoLevelSaveService.SaveWithoutMemoryGuard(
            document,
            outputPath,
            progress,
            CancellationToken.None);
        saveTimer.Stop();
        SmoDocument verified = SmoDocument.Load(outputPath);
        if (verified.HasErrors)
            throw new InvalidDataException("The stress output has parser errors.");
        process.Refresh();
        totalTimer.Stop();

        var report = new
        {
            Success = true,
            SourcePath = sourcePath,
            OutputPath = outputPath,
            Seed = seed,
            RequestedChanges = requestedChanges,
            AppliedChanges = counts.Total,
            Changes = counts,
            SourceBytes = workspace.Document.Data.Length,
            OutputBytes = verified.Data.Length,
            SourceObjects = workspace.Document.Objects.Count,
            OutputObjects = verified.Objects.Count,
            SaveSeconds = saveTimer.Elapsed.TotalSeconds,
            TotalSeconds = totalTimer.Elapsed.TotalSeconds,
            PeakWorkingSetBytes = process.PeakWorkingSet64,
            FinalPrivateBytes = process.PrivateMemorySize64,
            SaveLogPath = result.LogPath,
            OutputSha256 = Convert.ToHexString(SHA256.HashData(verified.Data.Span)),
            CompletedUtc = DateTimeOffset.UtcNow
        };
        File.WriteAllText(
            reportPath,
            JsonSerializer.Serialize(report, new JsonSerializerOptions { WriteIndented = true }));
        Console.WriteLine(
            $"STRESS PASS: changes={counts.Total}; objects={verified.Objects.Count}; " +
            $"bytes={verified.Data.Length}; save={saveTimer.Elapsed}; " +
            $"peak-working-set={process.PeakWorkingSet64 / (1024d * 1024d):N1} MiB; " +
            $"report={reportPath}");
        return 0;
    }

    private static StressChangeCounts ApplyRandomChanges(
        SmoLevelDocument document,
        string modelsDirectory,
        int requestedChanges,
        Random random)
    {
        SmoLevelEntity[] transformable = document.Entities
            .Where(entity => entity.Kind == SmoLevelEntityKind.Visual &&
                             document.CanPersistTransform(entity.Id))
            .ToArray();
        SmoLevelAsset[] cloneableAssets = document.Workspace.Assets
            .Where(asset => asset.Placements.Count > 0 &&
                SmoSharedPlacementCloner.CanClone(
                    document.Workspace.Document,
                    asset.ObjectIndex))
            .ToArray();
        if (transformable.Length == 0 || cloneableAssets.Length == 0)
            throw new InvalidDataException(
                "The source level has no transformable entities or cloneable placements.");

        ImportedCandidate[] importedCandidates = LoadImportedCandidates(modelsDirectory, maxCount: 3);
        var externalModelIds = new List<Guid>();
        foreach (ImportedCandidate candidate in importedCandidates)
        {
            try
            {
                externalModelIds.Add(document.AddExternalModel(
                    candidate.Scene,
                    candidate.Path,
                    $"Stress_{Path.GetFileNameWithoutExtension(candidate.Path)}"));
            }
            catch (Exception exception) when (
                exception is InvalidDataException or InvalidOperationException or ArgumentException)
            {
                Console.WriteLine($"MODEL SKIP: {candidate.Path} · {exception.Message}");
            }
        }

        var replacedCompositeParents = new HashSet<int>();
        var replacedTextures = new HashSet<int>();
        var removedEntities = new HashSet<SmoLevelEntityId>();
        int transforms = 0;
        int sharedPlacements = 0;
        int externalPlacements = 0;
        int modelReplacements = 0;
        int textureReplacements = 0;
        int removals = 0;
        int applied = 0;
        int attempts = 0;
        while (applied < requestedChanges && attempts++ < requestedChanges * 30)
        {
            int roll = random.Next(100);
            bool changed = roll switch
            {
                < 42 => TryTransform(),
                < 68 => TryAddSharedPlacement(),
                < 82 => TryAddExternalPlacement(),
                < 89 => TryReplaceModel(),
                < 95 => TryReplaceTexture(),
                _ => TryRemoveEntity()
            };
            if (!changed)
                changed = TryTransform();
            if (changed)
                applied++;
        }
        if (applied != requestedChanges)
            throw new InvalidOperationException(
                $"Could apply only {applied} of {requestedChanges} requested random changes.");
        return new StressChangeCounts(
            transforms,
            sharedPlacements,
            externalPlacements,
            modelReplacements,
            textureReplacements,
            removals);

        bool TryTransform()
        {
            SmoLevelEntity entity = transformable[random.Next(transformable.Length)];
            if (removedEntities.Contains(entity.Id) ||
                document.RemovedEntityIds.Contains(entity.Id) ||
                !Matrix4x4.Decompose(
                    entity.WorldTransform,
                    out Vector3 scale,
                    out Quaternion rotation,
                    out Vector3 translation))
            {
                return false;
            }
            Vector3 scaleFactor = new(
                NextFloat(0.985f, 1.015f),
                NextFloat(0.985f, 1.015f),
                NextFloat(0.985f, 1.015f));
            Vector3 nextScale = scale * scaleFactor;
            Quaternion deltaRotation = Quaternion.CreateFromYawPitchRoll(
                NextFloat(-0.025f, 0.025f),
                NextFloat(-0.015f, 0.015f),
                NextFloat(-0.015f, 0.015f));
            Quaternion nextRotation = Quaternion.Normalize(rotation * deltaRotation);
            Vector3 nextTranslation = translation + new Vector3(
                NextFloat(-12, 12),
                NextFloat(-4, 4),
                NextFloat(-12, 12));
            Matrix4x4 next =
                Matrix4x4.CreateScale(nextScale) *
                Matrix4x4.CreateFromQuaternion(nextRotation) *
                Matrix4x4.CreateTranslation(nextTranslation);
            try
            {
                if (!document.SetEntityTransform(entity.Id, next, "Stress transform"))
                    return false;
            }
            catch (NotSupportedException)
            {
                // A node under a non-uniform parent cannot represent every
                // world rotation/scale without shear. Exercise the universally
                // writable translation path for that entity instead.
                Matrix4x4 translationOnly = entity.WorldTransform;
                translationOnly.M41 += NextFloat(-12, 12);
                translationOnly.M42 += NextFloat(-4, 4);
                translationOnly.M43 += NextFloat(-12, 12);
                if (!document.SetEntityTransform(
                        entity.Id,
                        translationOnly,
                        "Stress translation fallback"))
                {
                    return false;
                }
            }
            transforms++;
            return true;
        }

        bool TryAddSharedPlacement()
        {
            SmoLevelAsset asset = cloneableAssets[random.Next(cloneableAssets.Length)];
            SmoLevelPlacement template = asset.Placements[random.Next(asset.Placements.Count)];
            Matrix4x4 transform = template.WorldTransform;
            transform.M41 += NextFloat(-1800, 1800);
            transform.M42 += NextFloat(-80, 240);
            transform.M43 += NextFloat(-1800, 1800);
            document.AddSharedPlacement(
                asset.ObjectIndex,
                transform,
                $"StressShared_{sharedPlacements + 1}");
            sharedPlacements++;
            return true;
        }

        bool TryAddExternalPlacement()
        {
            if (externalModelIds.Count == 0)
                return false;
            Guid modelId = externalModelIds[random.Next(externalModelIds.Count)];
            float scale = document.ExternalModels[modelId].SuggestedPlacementScale;
            Matrix4x4 transform =
                Matrix4x4.CreateScale(scale) *
                Matrix4x4.CreateRotationY(NextFloat(-MathF.PI, MathF.PI)) *
                Matrix4x4.CreateTranslation(
                    NextFloat(-4500, 4500),
                    NextFloat(-100, 1000),
                    NextFloat(-4500, 4500));
            document.AddExternalPlacement(
                modelId,
                transform,
                $"StressExternal_{externalPlacements + 1}");
            externalPlacements++;
            return true;
        }

        bool TryReplaceModel()
        {
            if (importedCandidates.Length == 0)
                return false;
            foreach (ImportedCandidate candidate in importedCandidates.OrderBy(_ => random.Next()))
            {
                foreach (IGrouping<string, SmoCompositeModel> group in document.CompositeModels
                             .Where(model =>
                                 !replacedCompositeParents.Contains(model.ParentObjectIndex) &&
                                 model.Entities.All(entity =>
                                     !document.RemovedEntityIds.Contains(entity.Id)) &&
                                 Matrix4x4.Invert(model.WorldTransform, out _))
                             .GroupBy(model => model.Name, StringComparer.OrdinalIgnoreCase)
                             .OrderBy(_ => random.Next())
                             .Take(80))
                {
                    SmoCompositeModel[] instances = group.ToArray();
                    try
                    {
                        Guid replacementId = document.ReplaceCompositeModels(
                            instances,
                            candidate.Scene,
                            ReplacementTransform.Identity,
                            instances[0].WorldTransform,
                            candidate.Path,
                            $"StressReplacement_{modelReplacements + 1}");
                        externalModelIds.Add(replacementId);
                        foreach (SmoCompositeModel instance in instances)
                        {
                            replacedCompositeParents.Add(instance.ParentObjectIndex);
                            foreach (SmoLevelEntity entity in instance.Entities)
                                removedEntities.Add(entity.Id);
                        }
                        modelReplacements++;
                        return true;
                    }
                    catch (Exception exception) when (
                        exception is InvalidDataException or InvalidOperationException or ArgumentException)
                    {
                        continue;
                    }
                }
            }
            return false;
        }

        bool TryReplaceTexture()
        {
            SmoLevelTexture[] candidates = document.Workspace.Assets
                .Select(asset => asset.Texture)
                .Where(texture => texture is not null &&
                    texture.Width > 0 && texture.Height > 0 &&
                    texture.Width <= 1024 && texture.Height <= 1024 &&
                    !replacedTextures.Contains(texture.ObjectIndex))
                .Cast<SmoLevelTexture>()
                .DistinctBy(texture => texture.ObjectIndex)
                .ToArray();
            if (candidates.Length == 0)
                return false;
            SmoLevelTexture texture = candidates[random.Next(candidates.Length)];
            byte[] png = EncodePerturbedTexture(texture, random);
            document.ReplaceTexture(
                texture.ObjectIndex,
                png,
                $"stress-texture-{texture.ObjectIndex}.png",
                replaceAlpha: true);
            replacedTextures.Add(texture.ObjectIndex);
            textureReplacements++;
            return true;
        }

        bool TryRemoveEntity()
        {
            SmoLevelEntity[] candidates = transformable
                .Where(entity => !removedEntities.Contains(entity.Id) &&
                                 !document.RemovedEntityIds.Contains(entity.Id))
                .ToArray();
            if (candidates.Length == 0)
                return false;
            SmoLevelEntity entity = candidates[random.Next(candidates.Length)];
            if (!document.RemoveEntities([entity.Id]))
                return false;
            removedEntities.Add(entity.Id);
            removals++;
            return true;
        }

        float NextFloat(float minimum, float maximum) =>
            minimum + (float)random.NextDouble() * (maximum - minimum);
    }

    private static ImportedCandidate[] LoadImportedCandidates(
        string modelsDirectory,
        int maxCount)
    {
        var result = new List<ImportedCandidate>(maxCount);
        string[] files = Directory.EnumerateFiles(
                modelsDirectory,
                "*.*",
                SearchOption.AllDirectories)
            .Where(path => Path.GetExtension(path).Equals(".obj", StringComparison.OrdinalIgnoreCase) ||
                           Path.GetExtension(path).Equals(".glb", StringComparison.OrdinalIgnoreCase))
            .OrderBy(path => new FileInfo(path).Length)
            .ThenBy(path => path, StringComparer.OrdinalIgnoreCase)
            .ToArray();
        foreach (string path in files)
        {
            try
            {
                ImportedScene scene = ImportedModelReader.Read(path);
                SmoLevelImportValidationReport report = SmoLevelImportValidator.Validate(
                    scene,
                    SmoLevelImportPurpose.AddExternalModel);
                if (!report.CanImport)
                    continue;
                result.Add(new ImportedCandidate(path, scene));
                Console.WriteLine(
                    $"MODEL LOAD: {path} · meshes={scene.Meshes.Count} · " +
                    $"vertices={scene.Meshes.Sum(mesh => mesh.Positions.Length):N0} · " +
                    $"textures={scene.Textures.Count}");
                if (result.Count == maxCount)
                    break;
            }
            catch (Exception exception) when (
                exception is InvalidDataException or NotSupportedException or ArgumentException)
            {
                Console.WriteLine($"MODEL SKIP: {path} · {exception.Message}");
            }
        }
        return result.ToArray();
    }

    private static byte[] EncodePerturbedTexture(SmoLevelTexture texture, Random random)
    {
        byte[] bgra = texture.Bgra32Pixels.ToArray();
        if (bgra.Length != checked(texture.Width * texture.Height * 4))
            throw new InvalidDataException("Texture preview has an invalid BGRA payload.");
        using var image = new Image<Rgba32>(texture.Width, texture.Height);
        image.ProcessPixelRows(accessor =>
        {
            for (int y = 0; y < texture.Height; y++)
            {
                Span<Rgba32> row = accessor.GetRowSpan(y);
                int source = y * texture.Width * 4;
                for (int x = 0; x < texture.Width; x++, source += 4)
                {
                    row[x] = new Rgba32(
                        bgra[source + 2],
                        bgra[source + 1],
                        bgra[source],
                        bgra[source + 3]);
                }
            }
        });
        for (int index = 0; index < Math.Min(32, texture.Width * texture.Height); index++)
        {
            int x = random.Next(texture.Width);
            int y = random.Next(texture.Height);
            Rgba32 pixel = image[x, y];
            pixel.R = (byte)(pixel.R ^ 1);
            image[x, y] = pixel;
        }
        using var stream = new MemoryStream();
        image.Save(stream, new PngEncoder());
        return stream.ToArray();
    }

    private static void WriteSupervisorFailure(
        string reportPath,
        string sourcePath,
        string outputPath,
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
                    OutputPath = outputPath,
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
        if (!int.TryParse(text, NumberStyles.Integer, CultureInfo.InvariantCulture, out int value) ||
            value <= 0)
        {
            throw new ArgumentOutOfRangeException(name, text, "Value must be a positive integer.");
        }
        return value;
    }

    private static string FindRepositoryRoot()
    {
        DirectoryInfo? cursor = new(AppContext.BaseDirectory);
        while (cursor is not null)
        {
            if (File.Exists(Path.Combine(cursor.FullName, "SparkplugEngineResearch.slnx")))
                return cursor.FullName;
            cursor = cursor.Parent;
        }
        return Directory.GetCurrentDirectory();
    }

    private sealed record ImportedCandidate(string Path, ImportedScene Scene);

    private sealed record StressChangeCounts(
        int Transforms,
        int SharedPlacements,
        int ExternalPlacements,
        int ModelReplacements,
        int TextureReplacements,
        int Removals)
    {
        public int Total => checked(
            Transforms + SharedPlacements + ExternalPlacements +
            ModelReplacements + TextureReplacements + Removals);
    }

    private sealed class InlineProgress<T>(Action<T> callback) : IProgress<T>
    {
        public void Report(T value) => callback(value);
    }
}
