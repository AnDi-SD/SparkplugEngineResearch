using System.Diagnostics;
using System.Text.Json;
using SmoImporter.Core;
using SmoViewer.Core;

namespace SmoLVLcreator.Core;

internal sealed record SmoExternalModelForestPlanResult(
    SmoAdditiveForestPlan Plan,
    IReadOnlyList<uint> MeshObjectIds,
    IReadOnlyDictionary<int, uint> ImportedTextureObjectIds,
    int PlacementCount);

internal static class SmoExternalModelBatchPipeline
{
    private const int PartsPerProcess = 1;
    private static readonly TimeSpan BatchTimeout = TimeSpan.FromMinutes(10);

    public static SmoExternalLevelModelAppendResult Append(
        SmoDocument current,
        int templateMeshObjectIndex,
        ImportedScene importedScene,
        IReadOnlyList<System.Numerics.Matrix4x4> worldTransforms,
        string modelName,
        string sourcePath,
        string workerExecutable,
        string? workerAssembly,
        string? nativeFbxBridgePath,
        Action<int, int>? batchProgress,
        CancellationToken cancellationToken)
    {
        BatchRunResult run = Run(
            current,
            templateMeshObjectIndex,
            importedScene,
            worldTransforms,
            modelName,
            sourcePath,
            workerExecutable,
            workerAssembly,
            nativeFbxBridgePath,
            batchProgress,
            cancellationToken,
            emitForestPlans: false,
            retainFinalContainer: true);
        return new SmoExternalLevelModelAppendResult(
            run.FinalContainer ?? throw new InvalidOperationException(
                "The external-model batch did not retain its final container."),
            run.AddedObjectCount,
            run.MeshObjectIds,
            worldTransforms.Count)
        {
            ImportedTextureObjectIds = run.ImportedTextureObjectIds
        };
    }

    /// <summary>
    /// Runs the serializer in isolated one-part workers and
    /// returns only additive forests to the editor. The large rolling SMO is
    /// kept on disk between workers and never materialized in the GUI process.
    /// </summary>
    public static SmoExternalModelForestPlanResult CreatePlan(
        SmoDocument current,
        int templateMeshObjectIndex,
        ImportedScene importedScene,
        IReadOnlyList<System.Numerics.Matrix4x4> worldTransforms,
        string modelName,
        string sourcePath,
        string workerExecutable,
        string? workerAssembly,
        string? nativeFbxBridgePath,
        Action<int, int>? batchProgress,
        CancellationToken cancellationToken)
    {
        BatchRunResult run = Run(
            current,
            templateMeshObjectIndex,
            importedScene,
            worldTransforms,
            modelName,
            sourcePath,
            workerExecutable,
            workerAssembly,
            nativeFbxBridgePath,
            batchProgress,
            cancellationToken,
            emitForestPlans: true,
            retainFinalContainer: false);
        return new SmoExternalModelForestPlanResult(
            new SmoAdditiveForestPlan(run.ForestOperations, run.GeneratedObjectIds)
            { ReferenceRanges = run.ReferenceRanges },
            run.MeshObjectIds,
            run.ImportedTextureObjectIds,
            worldTransforms.Count);
    }

    private static BatchRunResult Run(
        SmoDocument current,
        int templateMeshObjectIndex,
        ImportedScene importedScene,
        IReadOnlyList<System.Numerics.Matrix4x4> worldTransforms,
        string modelName,
        string sourcePath,
        string workerExecutable,
        string? workerAssembly,
        string? nativeFbxBridgePath,
        Action<int, int>? batchProgress,
        CancellationToken cancellationToken,
        bool emitForestPlans,
        bool retainFinalContainer)
    {
        ArgumentNullException.ThrowIfNull(current);
        ArgumentNullException.ThrowIfNull(importedScene);
        // The worker re-reads the source model, so split it here and again in
        // the worker through the same deterministic preparer. This keeps part
        // indices and progress counts identical across the process boundary.
        importedScene = SmoLevelRigidImportPreparer.Prepare(importedScene);
        string root = Path.Combine(
            Path.GetTempPath(),
            "SmoLVLcreator",
            "external-batches",
            Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(root);
        try
        {
            string inputPath = Path.Combine(root, "input-000.smo");
            using (var stream = new FileStream(
                       inputPath,
                       FileMode.CreateNew,
                       FileAccess.Write,
                       FileShare.Read,
                       1024 * 1024,
                       FileOptions.SequentialScan))
            {
                stream.Write(current.Data.Span);
            }

            var referenceRanges = new List<SmoFileReferenceRange>();
            var referenceTransport = new SmoFileReferenceWorkerContext();
            var textureFiles = new List<SmoExternalBatchTexture>(
                importedScene.Textures.Count);
            for (int index = 0; index < importedScene.Textures.Count; index++)
            {
                ImportedTexture texture = importedScene.Textures[index];
                string texturePath = Path.Combine(root, $"texture-{index:D4}.bin");
                File.WriteAllBytes(texturePath, texture.Data);
                textureFiles.Add(new SmoExternalBatchTexture(
                    index,
                    texture.Name,
                    texture.MimeType,
                    texture.Width,
                    texture.Height,
                    texturePath,
                    texture.SourcePath));
            }

            uint templateObjectId = current.Objects[templateMeshObjectIndex].Id;
            int totalParts = importedScene.Meshes.Count;
            int addedObjectCount = 0;
            var generatedMeshIds = new List<uint>(totalParts);
            var generatedObjectIds = new List<uint>();
            var forestOperations = new List<SmoVisualForestOperation>();
            IReadOnlyDictionary<int, uint> importedTextureObjectIds =
                new Dictionary<int, uint>();
            int partsPerProcess = emitForestPlans
                ? Math.Max(1, totalParts)
                : PartsPerProcess;
            int batchOrdinal = 0;
            for (int firstPart = 0; firstPart < totalParts;
                 firstPart += partsPerProcess, batchOrdinal++)
            {
                cancellationToken.ThrowIfCancellationRequested();
                int count = Math.Min(partsPerProcess, totalParts - firstPart);
                string outputPath = Path.Combine(
                    root,
                    $"output-{batchOrdinal:D3}.smo");
                string resultPath = Path.Combine(
                    root,
                    $"result-{batchOrdinal:D3}.json");
                string jobPath = Path.Combine(
                    root,
                    $"job-{batchOrdinal:D3}.json");
                var job = new SmoExternalModelBatchJob
                {
                    InputLevelPath = inputPath,
                    OutputLevelPath = outputPath,
                    ResultPath = resultPath,
                    ModelSourcePath = sourcePath,
                    NativeFbxBridgePath = nativeFbxBridgePath,
                    TemplateMeshObjectId = templateObjectId,
                    ModelName = modelName,
                    FirstPartIndex = firstPart,
                    PartCount = count,
                    WorldTransforms = worldTransforms
                        .Select(SmoSaveMatrix.From)
                        .ToList(),
                    PreparedTextures = textureFiles,
                    ReusableImportedTextureObjectIds =
                        new Dictionary<int, uint>(importedTextureObjectIds),
                    EmitForestPlan = emitForestPlans,
                    WriteOutputContainer = !emitForestPlans
                };
                File.WriteAllText(jobPath, JsonSerializer.Serialize(job));

                RunBatchWorker(
                    workerExecutable,
                    workerAssembly,
                    jobPath,
                    cancellationToken);
                SmoExternalModelBatchResult result =
                    JsonSerializer.Deserialize<SmoExternalModelBatchResult>(
                        File.ReadAllText(resultPath)) ??
                    throw new InvalidDataException(
                        "External-model batch worker returned an empty result.");
                if (!result.Success)
                {
                    throw new InvalidOperationException(
                        result.Error ?? "External-model batch failed.",
                        result.Details is null
                            ? null
                            : new SmoExternalBatchRemoteException(result.Details));
                }
                addedObjectCount += result.AddedObjectCount;
                generatedMeshIds.AddRange(result.MeshObjectIds);
                importedTextureObjectIds = result.ImportedTextureObjectIds;
                if (emitForestPlans)
                {
                    if (result.Forests is null)
                    {
                        throw new InvalidDataException(
                            "External-model worker omitted its additive forest plan.");
                    }
                    foreach (SmoExternalBatchForest forest in result.Forests)
                    {
                        byte[] fieldData = File.ReadAllBytes(forest.FieldDataPath);
                        if (forest.ReferenceRange is null)
                            throw new InvalidDataException("External-model worker omitted actual-reader reference provenance.");
                        referenceRanges.Add(SmoFileReferenceRange.RestoreWorkerTransport(fieldData, forest.ReferenceRange, referenceTransport));
                        SmoVisualForestEntry[] entries = forest.Entries.Select(entry =>
                            new SmoVisualForestEntry(
                                entry.Id,
                                entry.RawName,
                                entry.TypeHash,
                                entry.RelativeOffset,
                                entry.SerializedSize)).ToArray();
                        forestOperations.Add(new SmoVisualForestOperation(
                            new SmoVisualForestAttachment(
                                forest.TargetOwnerId,
                                fieldData,
                                entries),
                            forest.InsertionKind,
                            forest.AnchorFieldType));
                        generatedObjectIds.AddRange(entries.Select(entry => entry.Id));
                    }
                }
                batchProgress?.Invoke(firstPart + count, totalParts);

                if (!emitForestPlans)
                {
                    if (!string.Equals(inputPath, Path.Combine(root, "input-000.smo"),
                            StringComparison.OrdinalIgnoreCase))
                    {
                        File.Delete(inputPath);
                    }
                    inputPath = outputPath;
                    SmoLargeContainerMemory.ReleaseIntermediates(
                        checked((int)new FileInfo(inputPath).Length),
                        compact: true);
                }
            }

            byte[]? data = retainFinalContainer ? File.ReadAllBytes(inputPath) : null;
            if (data is not null)
            {
                SmoDocument verified = SmoDocument.ParseOwned(data, current.SourcePath);
                if (verified.HasErrors)
                    throw new InvalidDataException(
                        "The complete external-model batch pipeline failed verification.");
            }
            if (emitForestPlans && generatedObjectIds.Count != addedObjectCount)
            {
                throw new InvalidDataException(
                    $"External-model workers reported {addedObjectCount} objects but " +
                    $"returned {generatedObjectIds.Count} planned objects.");
            }
            return new BatchRunResult(
                data,
                addedObjectCount,
                generatedMeshIds,
                importedTextureObjectIds,
                forestOperations,
                generatedObjectIds,
                referenceRanges);
        }
        finally
        {
            TryDelete(root);
        }
    }

    private static void RunBatchWorker(
        string executable,
        string? assembly,
        string jobPath,
        CancellationToken cancellationToken)
    {
        var startInfo = new ProcessStartInfo
        {
            FileName = executable,
            UseShellExecute = false,
            CreateNoWindow = true,
            WorkingDirectory = AppContext.BaseDirectory
        };
        if (!string.IsNullOrWhiteSpace(assembly))
            startInfo.ArgumentList.Add(assembly);
        // A malformed or pathological donor must fail inside the isolated
        // worker instead of forcing the operating system into swap thrashing.
        startInfo.Environment["DOTNET_GCHeapHardLimitPercent"] = "0x32";
        startInfo.Environment["DOTNET_GCConserveMemory"] = "9";
        startInfo.ArgumentList.Add("--isolated-external-model-batch");
        startInfo.ArgumentList.Add(jobPath);
        using Process process = Process.Start(startInfo) ??
            throw new InvalidOperationException(
                "Could not start the external-model batch worker.");
        var timer = Stopwatch.StartNew();
        while (!process.WaitForExit(150))
        {
            if (cancellationToken.IsCancellationRequested)
            {
                TryKill(process);
                throw new OperationCanceledException(cancellationToken);
            }
            if (timer.Elapsed > BatchTimeout)
            {
                TryKill(process);
                throw new TimeoutException(
                    "An external-model batch exceeded its ten-minute timeout.");
            }
        }
        if (process.ExitCode != 0)
        {
            // The detailed exception is normally present in the result file;
            // the caller reads it immediately after this method.
            return;
        }
    }

    private static void TryKill(Process process)
    {
        try
        {
            if (!process.HasExited)
            {
                process.Kill(entireProcessTree: true);
                process.WaitForExit(5000);
            }
        }
        catch
        {
        }
    }

    private static void TryDelete(string directory)
    {
        try
        {
            string full = Path.GetFullPath(directory);
            string allowed = Path.GetFullPath(Path.Combine(
                    Path.GetTempPath(),
                    "SmoLVLcreator",
                    "external-batches"))
                .TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
            if (full.StartsWith(allowed, StringComparison.OrdinalIgnoreCase) &&
                Directory.Exists(full))
            {
                Directory.Delete(full, recursive: true);
            }
        }
        catch
        {
        }
    }

    private sealed class SmoExternalBatchRemoteException(string details)
        : Exception(details);

    private sealed record BatchRunResult(
        byte[]? FinalContainer,
        int AddedObjectCount,
        IReadOnlyList<uint> MeshObjectIds,
        IReadOnlyDictionary<int, uint> ImportedTextureObjectIds,
        IReadOnlyList<SmoVisualForestOperation> ForestOperations,
        IReadOnlyList<uint> GeneratedObjectIds,
        IReadOnlyList<SmoFileReferenceRange> ReferenceRanges);
}

public sealed class SmoExternalModelBatchJob
{
    public required string InputLevelPath { get; init; }
    public required string OutputLevelPath { get; init; }
    public required string ResultPath { get; init; }
    public required string ModelSourcePath { get; init; }
    public string? NativeFbxBridgePath { get; init; }
    public uint TemplateMeshObjectId { get; init; }
    public required string ModelName { get; init; }
    public int FirstPartIndex { get; init; }
    public int PartCount { get; init; }
    public List<SmoSaveMatrix> WorldTransforms { get; init; } = [];
    public List<SmoExternalBatchTexture> PreparedTextures { get; init; } = [];
    public Dictionary<int, uint> ReusableImportedTextureObjectIds { get; init; } = [];
    public bool EmitForestPlan { get; init; }
    public bool WriteOutputContainer { get; init; } = true;

    public static int Run(string jobPath)
    {
        SmoExternalModelBatchJob? job = null;
        try
        {
            job = JsonSerializer.Deserialize<SmoExternalModelBatchJob>(
                File.ReadAllText(Path.GetFullPath(jobPath))) ??
                throw new InvalidDataException(
                    "External-model batch job is empty.");
            SmoDocument document = SmoDocument.Load(job.InputLevelPath);
            ImportedScene imported = ImportedModelReader.Read(
                job.ModelSourcePath,
                job.NativeFbxBridgePath);
            ImportedTexture[] textures = imported.Textures.ToArray();
            foreach (SmoExternalBatchTexture item in job.PreparedTextures)
            {
                if ((uint)item.Index >= (uint)textures.Length)
                    throw new InvalidDataException(
                        $"Prepared texture index {item.Index} is outside the imported scene.");
                textures[item.Index] = new ImportedTexture(
                    item.Name,
                    item.MimeType,
                    item.Width,
                    item.Height,
                    File.ReadAllBytes(item.DataPath),
                    item.SourcePath);
            }
            imported = imported with { EmbeddedTextures = textures };
            int templateIndex = document.Objects.Single(entry =>
                entry.Id == job.TemplateMeshObjectId).Index;
            SmoExternalLevelModelAppendResult appended =
                SmoExternalLevelModelAppender.AppendPartRange(
                    document,
                    templateIndex,
                    imported,
                    job.WorldTransforms.Select(item => item.ToMatrix()).ToArray(),
                    job.ModelName,
                    job.FirstPartIndex,
                    job.PartCount,
                    job.ReusableImportedTextureObjectIds);
            if (job.WriteOutputContainer)
                File.WriteAllBytes(job.OutputLevelPath, appended.Data);
            List<SmoExternalBatchForest>? forests = null;
            if (job.EmitForestPlan)
            {
                SmoDocument additiveResult = SmoDocument.ParseOwned(
                    appended.Data,
                    document.SourcePath);
                SmoAdditiveForestPlan plan = SmoAdditiveForestPlanner.Create(
                    document,
                    additiveResult);
                forests = WriteForestPlan(job.ResultPath, plan);
            }
            WriteResult(job.ResultPath, new SmoExternalModelBatchResult(
                true,
                appended.AddedObjectCount,
                appended.MeshObjectIds.ToList(),
                new Dictionary<int, uint>(appended.ImportedTextureObjectIds),
                null,
                null,
                forests));
            return 0;
        }
        catch (Exception exception)
        {
            if (job is not null)
            {
                WriteResult(job.ResultPath, new SmoExternalModelBatchResult(
                    false,
                    0,
                    [],
                    [],
                    exception.Message,
                    exception.ToString(),
                    null));
            }
            return 1;
        }
    }

    private static void WriteResult(
        string path,
        SmoExternalModelBatchResult result)
    {
        string temporary = path + ".tmp";
        File.WriteAllText(temporary, JsonSerializer.Serialize(result));
        File.Move(temporary, path, overwrite: true);
    }

    private static List<SmoExternalBatchForest> WriteForestPlan(
        string resultPath,
        SmoAdditiveForestPlan plan)
    {
        string directory = Path.GetDirectoryName(resultPath) ??
            throw new InvalidDataException("Batch result path has no directory.");
        string stem = Path.GetFileNameWithoutExtension(resultPath);
        var result = new List<SmoExternalBatchForest>(plan.Operations.Count);
        var referenceTransport = new SmoFileReferenceWorkerContext();
        if (plan.ReferenceRanges is null || plan.ReferenceRanges.Count != plan.Operations.Count)
            throw new InvalidDataException("Worker forest plan lacks actual-reader reference provenance.");
        for (int index = 0; index < plan.Operations.Count; index++)
        {
            SmoVisualForestOperation operation = plan.Operations[index];
            string dataPath = Path.Combine(directory, $"{stem}-forest-{index:D3}.bin");
            File.WriteAllBytes(dataPath, operation.Attachment.FieldData);
            result.Add(new SmoExternalBatchForest(
                operation.Attachment.TargetOwnerId,
                operation.InsertionKind,
                operation.AnchorFieldType,
                dataPath,
                operation.Attachment.Entries.Select(entry =>
                    new SmoExternalBatchForestEntry(
                        entry.Id,
                        entry.RawName,
                        entry.TypeHash,
                        entry.RelativeOffset,
                        entry.SerializedSize)).ToList(),
                plan.ReferenceRanges[index].ExportWorkerTransport(referenceTransport)));
        }
        return result;
    }
}

public sealed record SmoExternalBatchTexture(
    int Index,
    string Name,
    string MimeType,
    int Width,
    int Height,
    string DataPath,
    string? SourcePath);

public sealed record SmoExternalModelBatchResult(
    bool Success,
    int AddedObjectCount,
    List<uint> MeshObjectIds,
    Dictionary<int, uint> ImportedTextureObjectIds,
    string? Error,
    string? Details,
    List<SmoExternalBatchForest>? Forests = null);

public sealed record SmoExternalBatchForest(
    uint TargetOwnerId,
    SmoVisualForestInsertionKind InsertionKind,
    int AnchorFieldType,
    string FieldDataPath,
    List<SmoExternalBatchForestEntry> Entries,
    SmoFileReferenceRangeTransport? ReferenceRange = null);

public sealed record SmoExternalBatchForestEntry(
    uint Id,
    byte[] RawName,
    uint TypeHash,
    int RelativeOffset,
    uint SerializedSize);
