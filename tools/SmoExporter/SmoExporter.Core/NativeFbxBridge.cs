using System.Diagnostics;

namespace SmoExporter.Core;

/// <summary>
/// Locates and invokes the isolated Autodesk FBX SDK host shipped with the SMO
/// tools. Keeping the SDK in a separate process prevents a malformed FBX from
/// taking down the WPF process and keeps all native lifetime rules on one side.
/// </summary>
public static class NativeFbxBridge
{
    public const string ExecutableName = "SmoFbxBridge.exe";
    public const string EnvironmentVariable = "SMO_FBX_BRIDGE_PATH";
    public static readonly TimeSpan DefaultTimeout = TimeSpan.FromMinutes(3);

    public static string? ResolveExecutable(string? preferredPath = null)
    {
        string? resolved = ResolveCandidate(preferredPath);
        if (resolved is not null) return resolved;
        resolved = ResolveCandidate(Environment.GetEnvironmentVariable(EnvironmentVariable));
        if (resolved is not null) return resolved;

        foreach (string root in EnumerateSearchRoots())
        foreach (string relative in new[]
                 {
                     ExecutableName,
                     Path.Combine("native", ExecutableName),
                     Path.Combine("FbxBridge", ExecutableName),
                     Path.Combine("tools", "FbxBridge.Native", "build", "bin", "Release", ExecutableName),
                     Path.Combine("tools", "FbxBridge.Native", "build", "bin", "Debug", ExecutableName)
                 })
        {
            resolved = ResolveCandidate(Path.Combine(root, relative));
            if (resolved is not null) return resolved;
        }
        return null;
    }

    public static void Run(
        string command,
        IEnumerable<string> arguments,
        string? preferredPath = null,
        TimeSpan? timeout = null,
        CancellationToken cancellationToken = default)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(command);
        TimeSpan effectiveTimeout = timeout ?? DefaultTimeout;
        if (effectiveTimeout <= TimeSpan.Zero &&
            effectiveTimeout != Timeout.InfiniteTimeSpan)
        {
            throw new ArgumentOutOfRangeException(
                nameof(timeout), "FBX bridge timeout must be positive or infinite.");
        }
        cancellationToken.ThrowIfCancellationRequested();
        string executable = ResolveExecutable(preferredPath) ??
            throw new InvalidOperationException(
                $"Нативный модуль FBX не найден. Файл {ExecutableName} должен находиться " +
                $"рядом с программой или быть указан в {EnvironmentVariable}.");
        var start = new ProcessStartInfo(executable)
        {
            UseShellExecute = false,
            CreateNoWindow = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true
        };
        start.ArgumentList.Add(command);
        foreach (string argument in arguments) start.ArgumentList.Add(argument);

        using Process process = Process.Start(start) ??
            throw new InvalidOperationException("Не удалось запустить нативный модуль FBX.");
        Task<string> stdoutTask = process.StandardOutput.ReadToEndAsync();
        Task<string> stderrTask = process.StandardError.ReadToEndAsync();
        using var timeoutCancellation = new CancellationTokenSource();
        if (effectiveTimeout != Timeout.InfiniteTimeSpan)
            timeoutCancellation.CancelAfter(effectiveTimeout);
        using CancellationTokenSource waitCancellation =
            CancellationTokenSource.CreateLinkedTokenSource(
                cancellationToken,
                timeoutCancellation.Token);
        try
        {
            process.WaitForExitAsync(waitCancellation.Token)
                .GetAwaiter()
                .GetResult();
        }
        catch (OperationCanceledException) when (!cancellationToken.IsCancellationRequested)
        {
            TryTerminate(process);
            throw new InvalidOperationException(
                $"Native FBX processing exceeded the safe timeout of " +
                $"{effectiveTimeout.TotalSeconds:N0} seconds and was terminated. " +
                "Simplify the FBX or export it again before retrying.");
        }
        catch (OperationCanceledException)
        {
            TryTerminate(process);
            throw;
        }
        string stdout = stdoutTask.GetAwaiter().GetResult().Trim();
        string stderr = stderrTask.GetAwaiter().GetResult().Trim();
        if (process.ExitCode == 0) return;
        string details = string.IsNullOrWhiteSpace(stderr) ? stdout : stderr;
        throw new InvalidOperationException(
            $"Нативная обработка FBX завершилась с кодом {process.ExitCode}." +
            (string.IsNullOrWhiteSpace(details) ? string.Empty : Environment.NewLine + details));
    }

    private static void TryTerminate(Process process)
    {
        try
        {
            if (!process.HasExited)
                process.Kill(entireProcessTree: true);
        }
        catch (Exception exception) when (exception is InvalidOperationException or
                                          System.ComponentModel.Win32Exception or
                                          NotSupportedException)
        {
            // A process that exited in the cancellation race needs no action.
        }
        try
        {
            process.WaitForExit(5000);
        }
        catch (InvalidOperationException)
        {
        }
    }

    private static string? ResolveCandidate(string? value)
    {
        if (string.IsNullOrWhiteSpace(value)) return null;
        try
        {
            string candidate = Environment.ExpandEnvironmentVariables(value.Trim().Trim('"'));
            if (Directory.Exists(candidate)) candidate = Path.Combine(candidate, ExecutableName);
            if (!File.Exists(candidate) ||
                !Path.GetFileName(candidate).Equals(ExecutableName, StringComparison.OrdinalIgnoreCase))
                return null;
            return Path.GetFullPath(candidate);
        }
        catch (Exception exception) when (exception is ArgumentException or
                                          NotSupportedException or
                                          PathTooLongException)
        {
            return null;
        }
    }

    private static IEnumerable<string> EnumerateSearchRoots()
    {
        var yielded = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (string? seed in new[]
                 {
                     AppContext.BaseDirectory,
                     Environment.CurrentDirectory
                 })
        {
            if (string.IsNullOrWhiteSpace(seed)) continue;
            DirectoryInfo? current;
            try { current = new DirectoryInfo(Path.GetFullPath(seed)); }
            catch { continue; }
            for (int depth = 0; current is not null && depth < 12; depth++, current = current.Parent)
            {
                if (yielded.Add(current.FullName)) yield return current.FullName;
            }
        }
    }
}
