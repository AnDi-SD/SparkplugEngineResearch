using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Runtime.InteropServices;
using System.Text.Json;
using Microsoft.Win32.SafeHandles;
using SmoImporter.Core;
using SmoLVLcreator.Core;

namespace SmoLVLcreator.Gui;

internal static class SmoLevelIsolatedSaveClient
{
    private const long MiB = 1024L * 1024;
    private const int DesiredProcessLimitMiB = 1536;
    private const int WarningReserveMiB = 512;
    private const int MinimumEstimatedPeakMiB = 768;
    private const int OrdinarySaveBaselineMiB = 384;
    private const int ExternalSaveBaselineMiB = 768;
    private static readonly TimeSpan SaveTimeout = TimeSpan.FromMinutes(30);

    public static SmoLevelSaveMemoryAssessment AssessMemory(
        SmoLevelDocument document)
    {
        ArgumentNullException.ThrowIfNull(document);

        long availableBytes = WindowsMemory.AvailablePhysicalBytes;
        long availableMiB = availableBytes > 0
            ? availableBytes / MiB
            : 0;
        long sourceBytes = document.Workspace.Document.Data.Length;
        double importedBytes = 0;
        foreach (SmoLevelModelReplacement replacement in
                 document.ModelReplacements.Values)
        {
            importedBytes += EstimateImportedSceneBytes(replacement.ImportedScene);
        }
        HashSet<Guid> placedExternalModelIds = document.ExternalPlacements.Values
            .Select(placement => placement.ModelId)
            .ToHashSet();
        foreach (SmoLevelExternalModel model in document.ExternalModels.Values
                     .Where(model => placedExternalModelIds.Contains(model.Id)))
        {
            importedBytes += EstimateImportedSceneBytes(model.ImportedScene);
        }

        long replacedTextureBytes = document.TextureReplacements.Values.Sum(
            replacement => (long)replacement.EncodedImage.Length);
        long generatedCollisionBytes = document.GeneratedCollisions.Values.Sum(
            collision =>
                (long)collision.Positions.Count * 12 +
                (long)collision.TriangleIndices.Count * sizeof(int));

        // Saving repeatedly parses and rebuilds the SMO container. Measurements
        // from the stress suite put that working set at roughly five source
        // containers plus imported geometry/decoded textures and the CLR/native
        // decoder baseline. This is deliberately an estimate, not an admission
        // rule: an unusual codec or native importer can still use more or less.
        bool hasExternalPlacement = placedExternalModelIds.Count > 0;
        int baselineMiB = hasExternalPlacement
            ? ExternalSaveBaselineMiB
            : OrdinarySaveBaselineMiB;
        double sourceCopies = hasExternalPlacement ? 6d : 4d;
        double estimatedBytes =
            baselineMiB * (double)MiB +
            sourceBytes * sourceCopies +
            importedBytes * 2d +
            replacedTextureBytes * 6d +
            generatedCollisionBytes * 2d;
        int estimatedPeakMiB = (int)Math.Clamp(
            Math.Ceiling(estimatedBytes / MiB),
            MinimumEstimatedPeakMiB,
            DesiredProcessLimitMiB);
        long recommendedAvailableMiB = estimatedPeakMiB + WarningReserveMiB;
        int processLimitMiB = DesiredProcessLimitMiB;
        return new SmoLevelSaveMemoryAssessment(
            availableMiB,
            estimatedPeakMiB,
            recommendedAvailableMiB,
            processLimitMiB,
            availableMiB > 0 && availableMiB < recommendedAvailableMiB);
    }

    public static async Task<SmoLevelSaveResult> SaveAsync(
        SmoLevelDocument document,
        string outputPath,
        IProgress<SmoLevelSaveProgress>? progress,
        CancellationToken cancellationToken)
    {
        ArgumentNullException.ThrowIfNull(document);
        string jobsRoot = Path.Combine(
            Path.GetTempPath(),
            "SmoLVLcreator",
            "save-jobs");
        string jobDirectory = Path.Combine(jobsRoot, Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(jobDirectory);
        string resultPath = Path.Combine(jobDirectory, "result.json");
        string progressPath = Path.Combine(jobDirectory, "progress.json");
        string gatePath = Path.Combine(jobDirectory, "worker.ready");
        bool keepCancellationDeferred = false;

        try
        {
            string jobPath = SmoLevelSaveJob.Write(
                document,
                outputPath,
                jobDirectory,
                ResolveNativeFbxBridgePath());

            int processLimitMiB = ResolveProcessLimitMiB();
            ProcessStartInfo startInfo = CreateWorkerStartInfo(
                jobPath,
                resultPath,
                progressPath,
                gatePath,
                processLimitMiB);
            using Process worker = Process.Start(startInfo) ??
                throw new InvalidOperationException(
                    "Could not start the isolated level-save worker.");
            using var memoryLimit = WindowsProcessMemoryLimit.Assign(
                worker,
                checked((long)processLimitMiB * 1024 * 1024));
            File.WriteAllText(gatePath, "go");

            var timer = Stopwatch.StartNew();
            DateTime progressWriteTime = DateTime.MinValue;
            SmoLevelSaveProgress? lastProgress = null;
            while (!worker.HasExited)
            {
                if (TryReadProgress(
                        progressPath,
                        ref progressWriteTime,
                        out SmoLevelSaveProgress? update))
                {
                    lastProgress = update;
                    progress?.Report(update!);
                    keepCancellationDeferred = update!.Stage is
                        SmoLevelSaveStage.Installing or SmoLevelSaveStage.Complete;
                }

                if (cancellationToken.IsCancellationRequested &&
                    !keepCancellationDeferred)
                {
                    TryKill(worker);
                    throw new OperationCanceledException(cancellationToken);
                }
                if (timer.Elapsed > SaveTimeout)
                {
                    TryKill(worker);
                    throw new TimeoutException(
                        $"Isolated save exceeded {SaveTimeout.TotalMinutes:N0} minutes " +
                        "and was stopped before it could consume the desktop session.");
                }

                await Task.Delay(150, CancellationToken.None);
                worker.Refresh();
            }

            if (TryReadProgress(
                    progressPath,
                    ref progressWriteTime,
                    out SmoLevelSaveProgress? finalUpdate))
            {
                lastProgress = finalUpdate;
                progress?.Report(finalUpdate!);
            }

            SmoLevelSaveWorkerResult? workerResult = ReadWorkerResult(resultPath);
            if (worker.ExitCode != 0 || workerResult?.Success != true ||
                workerResult.SaveResult is null)
            {
                string stage = lastProgress is null
                    ? "before progress was reported"
                    : $"during {lastProgress.Stage}: {lastProgress.Message}";
                string error = workerResult?.Error ??
                    $"Worker exited with code {worker.ExitCode}.";
                throw new InvalidOperationException(
                    $"Isolated save failed {stage}. {error}\n\n" +
                    "The worker was memory-limited; the editor and Windows were kept alive.",
                    workerResult?.Details is null
                        ? null
                        : new SmoLevelSaveWorkerRemoteException(workerResult.Details));
            }
            return workerResult.SaveResult;
        }
        finally
        {
            TryDeleteJobDirectory(jobDirectory, jobsRoot);
        }
    }

    private static ProcessStartInfo CreateWorkerStartInfo(
        string jobPath,
        string resultPath,
        string progressPath,
        string gatePath,
        int processLimitMiB)
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
        startInfo.ArgumentList.Add("--isolated-save-worker");
        startInfo.ArgumentList.Add(jobPath);
        startInfo.ArgumentList.Add(resultPath);
        startInfo.ArgumentList.Add(progressPath);
        startInfo.ArgumentList.Add(gatePath);

        // Leave native decoders and parser structures room inside the harder
        // process-wide Windows Job Object limit.
        long gcLimitBytes = checked(
            (long)Math.Max(128, processLimitMiB - 128) * MiB);
        startInfo.Environment["DOTNET_GCHeapHardLimit"] =
            gcLimitBytes.ToString("X", CultureInfo.InvariantCulture);
        startInfo.Environment["DOTNET_GCConserveMemory"] = "9";
        return startInfo;
    }

    private static int ResolveProcessLimitMiB()
        // The user has already seen and accepted the physical-memory estimate.
        // This remains a hard upper bound, never a requirement that this much
        // physical RAM must be free before saving starts.
        => DesiredProcessLimitMiB;

    private static double EstimateImportedSceneBytes(ImportedScene scene)
    {
        double bytes = 0;
        foreach (ImportedMesh mesh in scene.Meshes)
        {
            bytes += mesh.Positions.LongLength * 12d;
            bytes += mesh.Normals.LongLength * 12d;
            bytes += mesh.TextureCoordinates.LongLength * 8d;
            bytes += mesh.TriangleIndices.LongLength * sizeof(uint);
            bytes += mesh.DiffuseColors.LongLength * sizeof(uint);
            if (mesh.Skinning is not null)
            {
                bytes += mesh.Skinning.JointIndices.LongLength * 8d;
                bytes += mesh.Skinning.Weights.LongLength * 16d;
                bytes += mesh.Skinning.Skeleton.InverseBindMatrices.Count * 64d;
                bytes += mesh.Skinning.Skeleton.BindWorldMatrices?.Count * 64d ?? 0;
                bytes += mesh.Skinning.Skeleton.BindLocalMatrices?.Count * 64d ?? 0;
            }
        }
        foreach (ImportedTexture texture in scene.Textures)
        {
            bytes += texture.Data.LongLength;
            bytes += Math.Max(0, texture.Width) *
                     (double)Math.Max(0, texture.Height) * 4d;
        }
        return bytes;
    }

    private static string? ResolveNativeFbxBridgePath()
    {
        string candidate = Path.Combine(
            AppContext.BaseDirectory,
            "SmoFbxBridge.exe");
        return File.Exists(candidate) ? candidate : null;
    }

    private static bool TryReadProgress(
        string path,
        ref DateTime lastWriteTime,
        out SmoLevelSaveProgress? progress)
    {
        progress = null;
        try
        {
            if (!File.Exists(path))
                return false;
            DateTime writeTime = File.GetLastWriteTimeUtc(path);
            if (writeTime <= lastWriteTime)
                return false;
            progress = JsonSerializer.Deserialize<SmoLevelSaveProgress>(
                File.ReadAllText(path));
            if (progress is null)
                return false;
            lastWriteTime = writeTime;
            return true;
        }
        catch (IOException)
        {
            return false;
        }
        catch (JsonException)
        {
            return false;
        }
    }

    private static SmoLevelSaveWorkerResult? ReadWorkerResult(string path)
    {
        if (!File.Exists(path))
            return null;
        try
        {
            return JsonSerializer.Deserialize<SmoLevelSaveWorkerResult>(
                File.ReadAllText(path));
        }
        catch (Exception exception) when (
            exception is IOException or JsonException)
        {
            return new SmoLevelSaveWorkerResult(
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
            {
                process.Kill(entireProcessTree: true);
                process.WaitForExit(5000);
            }
        }
        catch
        {
            // The Job Object is also configured to terminate the worker when
            // its owning handle is closed.
        }
    }

    private static void TryDeleteJobDirectory(string directory, string jobsRoot)
    {
        try
        {
            string fullDirectory = Path.GetFullPath(directory);
            string fullRoot = Path.GetFullPath(jobsRoot)
                .TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
            if (fullDirectory.StartsWith(fullRoot, StringComparison.OrdinalIgnoreCase) &&
                Directory.Exists(fullDirectory))
            {
                Directory.Delete(fullDirectory, recursive: true);
            }
        }
        catch
        {
            // A stale job is safe and can be cleaned on the next application
            // start; never turn a successful level save into a UI error here.
        }
    }

    private sealed class SmoLevelSaveWorkerRemoteException(string details)
        : Exception(details);
}

internal sealed record SmoLevelSaveMemoryAssessment(
    long AvailablePhysicalMiB,
    int EstimatedPeakMiB,
    long RecommendedAvailableMiB,
    int WorkerLimitMiB,
    bool ShouldWarn);

internal static class SmoLevelSaveWorkerHost
{
    public static int Run(string[] args)
    {
        if (args.Length != 5)
            return 64;
        string jobPath = Path.GetFullPath(args[1]);
        string resultPath = Path.GetFullPath(args[2]);
        string progressPath = Path.GetFullPath(args[3]);
        string gatePath = Path.GetFullPath(args[4]);
        try
        {
            WaitForParentGate(gatePath);
            SmoLevelSaveJob job = SmoLevelSaveJob.Read(jobPath);
            var progress = new AtomicFileProgress(progressPath);
            SmoLevelSaveResult result = job.Execute(progress);
            WriteResult(
                resultPath,
                SmoLevelSaveWorkerResult.FromSuccess(result));
            return 0;
        }
        catch (Exception exception)
        {
            WriteResult(
                resultPath,
                SmoLevelSaveWorkerResult.FromException(exception));
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
                    "The save worker was not admitted to its memory sandbox.");
            Thread.Sleep(25);
        }
    }

    private static void WriteResult(
        string path,
        SmoLevelSaveWorkerResult result)
    {
        try
        {
            WriteAtomic(path, JsonSerializer.Serialize(result));
        }
        catch
        {
            // The exit code still informs the parent when a hard memory limit
            // also prevents allocation of the error report.
        }
    }

    private static void WriteAtomic(string path, string contents)
    {
        string temporary = path + ".tmp";
        File.WriteAllText(temporary, contents);
        File.Move(temporary, path, overwrite: true);
    }

    private sealed class AtomicFileProgress(string path)
        : IProgress<SmoLevelSaveProgress>
    {
        public void Report(SmoLevelSaveProgress value) =>
            WriteAtomic(path, JsonSerializer.Serialize(value));
    }
}

internal sealed class WindowsProcessMemoryLimit : IDisposable
{
    private const uint JobObjectLimitProcessMemory = 0x00000100;
    private const uint JobObjectLimitJobMemory = 0x00000200;
    private const uint JobObjectLimitKillOnJobClose = 0x00002000;
    private const int JobObjectExtendedLimitInformationClass = 9;
    private readonly SafeFileHandle _jobHandle;

    private WindowsProcessMemoryLimit(SafeFileHandle jobHandle) =>
        _jobHandle = jobHandle;

    public static WindowsProcessMemoryLimit Assign(Process process, long limitBytes)
    {
        IntPtr rawHandle = CreateJobObject(IntPtr.Zero, null);
        if (rawHandle == IntPtr.Zero)
            throw new InvalidOperationException(
                $"Could not create the save memory sandbox ({Marshal.GetLastWin32Error()}).");
        var handle = new SafeFileHandle(rawHandle, ownsHandle: true);
        try
        {
            var limits = new JobObjectExtendedLimitInformation
            {
                BasicLimitInformation = new JobObjectBasicLimitInformation
                {
                    LimitFlags = JobObjectLimitProcessMemory |
                                 JobObjectLimitJobMemory |
                                 JobObjectLimitKillOnJobClose
                },
                ProcessMemoryLimit = (UIntPtr)checked((ulong)limitBytes),
                JobMemoryLimit = (UIntPtr)checked((ulong)limitBytes)
            };
            int size = Marshal.SizeOf<JobObjectExtendedLimitInformation>();
            IntPtr pointer = Marshal.AllocHGlobal(size);
            try
            {
                Marshal.StructureToPtr(limits, pointer, false);
                if (!SetInformationJobObject(
                        handle,
                        JobObjectExtendedLimitInformationClass,
                        pointer,
                        (uint)size))
                {
                    throw new InvalidOperationException(
                        $"Could not set the save memory limit " +
                        $"({Marshal.GetLastWin32Error()}).");
                }
            }
            finally
            {
                Marshal.FreeHGlobal(pointer);
            }
            if (!AssignProcessToJobObject(handle, process.Handle))
            {
                throw new InvalidOperationException(
                    $"Could not place the save worker in its memory sandbox " +
                    $"({Marshal.GetLastWin32Error()}).");
            }
            return new WindowsProcessMemoryLimit(handle);
        }
        catch
        {
            handle.Dispose();
            try
            {
                if (!process.HasExited)
                    process.Kill(entireProcessTree: true);
            }
            catch
            {
            }
            throw;
        }
    }

    public void Dispose() => _jobHandle.Dispose();

    [StructLayout(LayoutKind.Sequential)]
    private struct JobObjectBasicLimitInformation
    {
        public long PerProcessUserTimeLimit;
        public long PerJobUserTimeLimit;
        public uint LimitFlags;
        public UIntPtr MinimumWorkingSetSize;
        public UIntPtr MaximumWorkingSetSize;
        public uint ActiveProcessLimit;
        public UIntPtr Affinity;
        public uint PriorityClass;
        public uint SchedulingClass;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct IoCounters
    {
        public ulong ReadOperationCount;
        public ulong WriteOperationCount;
        public ulong OtherOperationCount;
        public ulong ReadTransferCount;
        public ulong WriteTransferCount;
        public ulong OtherTransferCount;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct JobObjectExtendedLimitInformation
    {
        public JobObjectBasicLimitInformation BasicLimitInformation;
        public IoCounters IoInfo;
        public UIntPtr ProcessMemoryLimit;
        public UIntPtr JobMemoryLimit;
        public UIntPtr PeakProcessMemoryUsed;
        public UIntPtr PeakJobMemoryUsed;
    }

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern IntPtr CreateJobObject(IntPtr attributes, string? name);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool SetInformationJobObject(
        SafeFileHandle job,
        int informationClass,
        IntPtr information,
        uint informationLength);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool AssignProcessToJobObject(
        SafeFileHandle job,
        IntPtr process);
}

internal static class WindowsMemory
{
    public static long AvailablePhysicalBytes
    {
        get
        {
            var status = new MemoryStatusEx
            {
                Length = (uint)Marshal.SizeOf<MemoryStatusEx>()
            };
            return GlobalMemoryStatusEx(ref status)
                ? checked((long)Math.Min(status.AvailablePhysical, long.MaxValue))
                : -1;
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct MemoryStatusEx
    {
        public uint Length;
        public uint MemoryLoad;
        public ulong TotalPhysical;
        public ulong AvailablePhysical;
        public ulong TotalPageFile;
        public ulong AvailablePageFile;
        public ulong TotalVirtual;
        public ulong AvailableVirtual;
        public ulong AvailableExtendedVirtual;
    }

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool GlobalMemoryStatusEx(ref MemoryStatusEx buffer);
}
