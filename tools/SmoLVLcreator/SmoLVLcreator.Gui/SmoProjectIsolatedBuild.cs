using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Text.Json;
using SmoLVLcreator.Core;

namespace SmoLVLcreator.Gui;

internal sealed record SmoProjectBuildProgress(
    int CompletedSteps,
    int TotalSteps,
    string Message,
    long CurrentContainerBytes);

internal sealed record SmoProjectBuildMemoryAssessment(
    long AvailablePhysicalMiB,
    int EstimatedPeakMiB,
    long RecommendedAvailableMiB,
    int WorkerLimitMiB,
    bool ShouldWarn);

internal static class SmoProjectIsolatedBuildClient
{
    private const long MiB = 1024L * 1024;
    private const int ProcessLimitMiB = 1536;
    private const int WarningReserveMiB = 512;
    private static readonly TimeSpan BuildTimeout = TimeSpan.FromMinutes(30);

    public static SmoProjectBuildMemoryAssessment AssessMemory(SmoProject project)
    {
        ArgumentNullException.ThrowIfNull(project);
        long assetBytes = project.AssetDataLength;
        long estimatedBytes =
            384L * MiB +
            project.DataSection.Length * 4L +
            assetBytes * 3L;
        int estimatedPeakMiB = (int)Math.Clamp(
            (estimatedBytes + MiB - 1) / MiB,
            512,
            ProcessLimitMiB);
        long availableMiB = WindowsMemory.AvailablePhysicalBytes > 0
            ? WindowsMemory.AvailablePhysicalBytes / MiB
            : 0;
        long recommendedMiB = estimatedPeakMiB + WarningReserveMiB;
        return new SmoProjectBuildMemoryAssessment(
            availableMiB,
            estimatedPeakMiB,
            recommendedMiB,
            ProcessLimitMiB,
            availableMiB > 0 && availableMiB < recommendedMiB);
    }

    public static async Task<SmoProjectBuildResult> BuildAsync(
        SmoProject project,
        string outputPath,
        IProgress<SmoProjectBuildProgress>? progress,
        CancellationToken cancellationToken)
    {
        ArgumentNullException.ThrowIfNull(project);
        ArgumentException.ThrowIfNullOrWhiteSpace(outputPath);
        string output = Path.GetFullPath(outputPath);
        string logPath = SmoLevelSaveService.GetLogPath(output);
        string jobsRoot = Path.Combine(
            Path.GetTempPath(),
            "SmoLVLcreator",
            "project-build-jobs");
        string jobDirectory = Path.Combine(jobsRoot, Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(jobDirectory);
        string snapshotPath = Path.Combine(jobDirectory, "snapshot.smolvlproj");
        string resultPath = Path.Combine(jobDirectory, "result.json");
        string progressPath = Path.Combine(jobDirectory, "progress.json");
        string gatePath = Path.Combine(jobDirectory, "worker.ready");
        try
        {
            progress?.Report(new SmoProjectBuildProgress(
                0, 5, "Подготовка снимка проекта…", project.DataSection.Length));
            await Task.Run(
                () => SmoProjectArchive.Save(project, snapshotPath),
                CancellationToken.None);
            cancellationToken.ThrowIfCancellationRequested();
            SmoLevelSaveService.TryAppendDiagnostic(
                logPath,
                "INFO",
                "PROJECT_BUILD_BEGIN",
                $"Output={output}; snapshot={snapshotPath}; " +
                $"objects={project.Objects.Count}; data={project.DataSection.Length}; " +
                $"workerLimitMiB={ProcessLimitMiB}.");

            ProcessStartInfo startInfo = CreateWorkerStartInfo(
                snapshotPath,
                output,
                resultPath,
                progressPath,
                gatePath);
            using Process worker = Process.Start(startInfo) ??
                throw new InvalidOperationException(
                    "Could not start the isolated project-build worker.");
            using var memoryLimit = WindowsProcessMemoryLimit.Assign(
                worker,
                checked((long)ProcessLimitMiB * MiB));
            File.WriteAllText(gatePath, "go");

            var timer = Stopwatch.StartNew();
            DateTime progressWriteTime = DateTime.MinValue;
            SmoProjectBuildProgress? lastProgress = null;
            while (!worker.HasExited)
            {
                if (TryReadProgress(
                        progressPath,
                        ref progressWriteTime,
                        out SmoProjectBuildProgress? update))
                {
                    lastProgress = update;
                    progress?.Report(update!);
                }
                if (cancellationToken.IsCancellationRequested)
                {
                    TryKill(worker);
                    SmoLevelSaveService.TryAppendDiagnostic(
                        logPath,
                        "WARN",
                        "PROJECT_BUILD_CANCELLED",
                        "The isolated project build was cancelled. The source project was not changed.");
                    throw new OperationCanceledException(cancellationToken);
                }
                if (timer.Elapsed > BuildTimeout)
                {
                    TryKill(worker);
                    throw new TimeoutException(
                        $"Isolated project build exceeded {BuildTimeout.TotalMinutes:N0} minutes.");
                }
                await Task.Delay(150, CancellationToken.None);
                worker.Refresh();
            }

            if (TryReadProgress(
                    progressPath,
                    ref progressWriteTime,
                    out SmoProjectBuildProgress? finalUpdate))
            {
                lastProgress = finalUpdate;
                progress?.Report(finalUpdate!);
            }
            SmoProjectBuildWorkerResult? workerResult = ReadResult(resultPath);
            if (worker.ExitCode != 0 || workerResult?.Success != true ||
                workerResult.BuildResult is null)
            {
                string stage = lastProgress is null
                    ? "before progress was reported"
                    : $"during {lastProgress.Message}";
                string error = workerResult?.Error ??
                    $"Worker exited with code {worker.ExitCode}.";
                SmoLevelSaveService.TryAppendDiagnostic(
                    logPath,
                    "ERROR",
                    "PROJECT_BUILD_WORKER_ERROR",
                    $"Failure {stage}: {error}\n{workerResult?.Details}");
                throw new InvalidOperationException(
                    $"Isolated project build failed {stage}. {error}\n\n" +
                    "The worker was memory-limited; the editor and Windows were kept alive.");
            }
            return workerResult.BuildResult;
        }
        catch (Exception exception) when (exception is not OperationCanceledException)
        {
            SmoLevelSaveService.TryAppendDiagnostic(
                logPath,
                "ERROR",
                "PROJECT_BUILD_ERROR",
                exception.ToString());
            throw;
        }
        finally
        {
            TryDeleteJobDirectory(jobDirectory, jobsRoot);
        }
    }

    private static ProcessStartInfo CreateWorkerStartInfo(
        string snapshotPath,
        string outputPath,
        string resultPath,
        string progressPath,
        string gatePath)
    {
        SmoWorkerHostInfo workerHost = SmoWorkerHost.ResolveCurrent();
        var startInfo = new ProcessStartInfo
        {
            FileName = workerHost.ExecutablePath,
            UseShellExecute = false,
            CreateNoWindow = true,
            WorkingDirectory = AppContext.BaseDirectory
        };
        if (workerHost.ManagedEntryAssemblyPath is not null)
            startInfo.ArgumentList.Add(workerHost.ManagedEntryAssemblyPath);
        startInfo.ArgumentList.Add("--isolated-project-build-worker");
        startInfo.ArgumentList.Add(snapshotPath);
        startInfo.ArgumentList.Add(outputPath);
        startInfo.ArgumentList.Add(resultPath);
        startInfo.ArgumentList.Add(progressPath);
        startInfo.ArgumentList.Add(gatePath);
        long gcLimitBytes = checked((long)(ProcessLimitMiB - 128) * MiB);
        startInfo.Environment["DOTNET_GCHeapHardLimit"] =
            gcLimitBytes.ToString("X", CultureInfo.InvariantCulture);
        startInfo.Environment["DOTNET_GCConserveMemory"] = "9";
        return startInfo;
    }

    private static bool TryReadProgress(
        string path,
        ref DateTime lastWriteTime,
        out SmoProjectBuildProgress? progress)
    {
        progress = null;
        try
        {
            if (!File.Exists(path))
                return false;
            DateTime writeTime = File.GetLastWriteTimeUtc(path);
            if (writeTime <= lastWriteTime)
                return false;
            progress = JsonSerializer.Deserialize<SmoProjectBuildProgress>(
                File.ReadAllText(path));
            if (progress is null)
                return false;
            lastWriteTime = writeTime;
            return true;
        }
        catch (Exception exception) when (
            exception is IOException or JsonException)
        {
            return false;
        }
    }

    private static SmoProjectBuildWorkerResult? ReadResult(string path)
    {
        try
        {
            return File.Exists(path)
                ? JsonSerializer.Deserialize<SmoProjectBuildWorkerResult>(
                    File.ReadAllText(path))
                : null;
        }
        catch (Exception exception) when (
            exception is IOException or JsonException)
        {
            return new SmoProjectBuildWorkerResult(
                false,
                null,
                "The worker produced an unreadable result.",
                exception.ToString());
        }
    }

    private static void TryKill(Process process)
    {
        try
        {
            if (!process.HasExited)
                process.Kill(entireProcessTree: true);
            process.WaitForExit(5000);
        }
        catch
        {
        }
    }

    private static void TryDeleteJobDirectory(string jobDirectory, string jobsRoot)
    {
        try
        {
            string resolvedJob = Path.GetFullPath(jobDirectory)
                .TrimEnd(Path.DirectorySeparatorChar);
            string resolvedRoot = Path.GetFullPath(jobsRoot)
                .TrimEnd(Path.DirectorySeparatorChar);
            if (resolvedJob.StartsWith(
                    resolvedRoot + Path.DirectorySeparatorChar,
                    StringComparison.OrdinalIgnoreCase) &&
                Directory.Exists(resolvedJob))
            {
                Directory.Delete(resolvedJob, recursive: true);
            }
        }
        catch
        {
            // A stale isolated job is harmless and can be cleaned on next start.
        }
    }
}

internal sealed record SmoProjectBuildWorkerResult(
    bool Success,
    SmoProjectBuildResult? BuildResult,
    string? Error,
    string? Details)
{
    public static SmoProjectBuildWorkerResult FromSuccess(
        SmoProjectBuildResult result) => new(true, result, null, null);

    public static SmoProjectBuildWorkerResult FromException(
        Exception exception) => new(
            false,
            null,
            exception.Message,
            exception.ToString());
}

internal static class SmoProjectBuildWorkerHost
{
    public static int Run(string[] args)
    {
        if (args.Length != 6)
            return 64;
        string snapshotPath = Path.GetFullPath(args[1]);
        string outputPath = Path.GetFullPath(args[2]);
        string resultPath = Path.GetFullPath(args[3]);
        string progressPath = Path.GetFullPath(args[4]);
        string gatePath = Path.GetFullPath(args[5]);
        string logPath = SmoLevelSaveService.GetLogPath(outputPath);
        try
        {
            WaitForParentGate(gatePath);
            Report(progressPath, 1, "Загрузка снимка проекта…", 0);
            SmoProject project = SmoProjectArchive.Load(snapshotPath);
            Report(
                progressPath,
                2,
                "Проверка журнала изменений…",
                project.DataSection.Length);
            project.Validate();
            Report(
                progressPath,
                3,
                "Потоковая сборка и проверка SMO…",
                project.DataSection.Length);
            SmoProjectBuildResult result =
                SmoProjectSerializer.Build(project, outputPath);
            Report(
                progressPath,
                5,
                "SMO собран и повторно проверен",
                result.FileSize);
            SmoLevelSaveService.TryAppendDiagnostic(
                logPath,
                "INFO",
                "PROJECT_BUILD_COMPLETE",
                $"Output={result.OutputPath}; bytes={result.FileSize}; " +
                $"objects={result.ObjectCount}; sha256={result.Sha256}; " +
                $"sourceIdentical={result.IsByteIdenticalToImportedSource}; " +
                $"backup={result.BackupPath ?? "NONE"}.");
            WriteResult(resultPath, SmoProjectBuildWorkerResult.FromSuccess(result));
            return 0;
        }
        catch (Exception exception)
        {
            SmoLevelSaveService.TryAppendDiagnostic(
                logPath,
                "ERROR",
                "PROJECT_BUILD_WORKER_EXCEPTION",
                exception.ToString());
            WriteResult(
                resultPath,
                SmoProjectBuildWorkerResult.FromException(exception));
            return 1;
        }
    }

    private static void WaitForParentGate(string gatePath)
    {
        var timer = Stopwatch.StartNew();
        while (!File.Exists(gatePath))
        {
            if (timer.Elapsed > TimeSpan.FromSeconds(30))
                throw new TimeoutException(
                    "The project-build worker was not admitted to its memory sandbox.");
            Thread.Sleep(25);
        }
    }

    private static void Report(
        string path,
        int completedSteps,
        string message,
        long bytes) => WriteAtomic(
            path,
            JsonSerializer.Serialize(new SmoProjectBuildProgress(
                completedSteps,
                5,
                message,
                bytes)));

    private static void WriteResult(
        string path,
        SmoProjectBuildWorkerResult result)
    {
        try
        {
            WriteAtomic(path, JsonSerializer.Serialize(result));
        }
        catch
        {
            // The worker exit code remains available if the hard limit also
            // prevents allocating the result payload.
        }
    }

    private static void WriteAtomic(string path, string contents)
    {
        string temporary = path + ".tmp";
        File.WriteAllText(temporary, contents);
        File.Move(temporary, path, overwrite: true);
    }
}
