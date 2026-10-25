using System.Diagnostics;
using System.Runtime;
using System.Runtime.InteropServices;

namespace SmoImporter.Core;

/// <summary>
/// Keeps repeated whole-container rewrites from accumulating dead large-object
/// heap buffers while editing unusually large level files.
/// </summary>
public static class SmoLargeContainerMemory
{
    private const int LargeContainerThreshold = 32 * 1024 * 1024;
    private const long MinimumRewriteHeadroom = 512L * 1024 * 1024;
    // The complete optimized save pipeline peaks at roughly eleven container
    // lengths above its baseline in the 80 MiB regression fixture. Keep two
    // additional lengths as safety margin without rejecting healthy saves.
    private const int EstimatedPeakContainerCopies = 13;

    public static void PrepareRepeatedRewrite(int containerLength)
    {
        if (containerLength < LargeContainerThreshold)
            return;

        // Start a structural repack from a known baseline. MemoryLoadBytes is
        // refreshed by the full collection and describes system-wide pressure,
        // not just managed allocations in this process.
        ReleaseIntermediates(containerLength, compact: true);
        GCMemoryInfo memory = GC.GetGCMemoryInfo();
        if (memory.HighMemoryLoadThresholdBytes <= 0 || memory.MemoryLoadBytes <= 0)
            return;

        long requiredHeadroom = Math.Max(
            MinimumRewriteHeadroom,
            checked((long)containerLength * EstimatedPeakContainerCopies));
        long availableHeadroom = Math.Max(
            0,
            memory.HighMemoryLoadThresholdBytes - memory.MemoryLoadBytes);
        if (availableHeadroom >= requiredHeadroom)
            return;

        throw new InvalidOperationException(
            "Сохранение остановлено до упаковки внешней модели: системе не " +
            "хватает безопасного запаса памяти для этого уровня. " +
            $"Нужно примерно {requiredHeadroom / (1024 * 1024):N0} МиБ; " +
            $"свободно до системного порога: " +
            $"{availableHeadroom / (1024 * 1024):N0} МиБ. " +
            "Закройте другие тяжёлые программы и повторите сохранение.");
    }

    public static void ReleaseIntermediates(
        int containerLength,
        bool compact = false)
    {
        if (containerLength < LargeContainerThreshold)
            return;
        if (compact)
        {
            GCSettings.LargeObjectHeapCompactionMode =
                GCLargeObjectHeapCompactionMode.CompactOnce;
        }
        GC.Collect(
            GC.MaxGeneration,
            GCCollectionMode.Forced,
            blocking: true,
            compacting: compact);
        // Trimming after every internal rewrite causes the same large container
        // pages to be faulted back in immediately. Only return pages to Windows
        // at a real operation boundary (one completed imported mesh part).
        if (compact && OperatingSystem.IsWindows())
        {
            using Process process = Process.GetCurrentProcess();
            _ = SetProcessWorkingSetSize(
                process.Handle,
                new IntPtr(-1),
                new IntPtr(-1));
        }
    }

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool SetProcessWorkingSetSize(
        IntPtr process,
        IntPtr minimumWorkingSetSize,
        IntPtr maximumWorkingSetSize);
}
