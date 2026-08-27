using System.Reflection;

namespace SmoLVLcreator.Core;

public sealed record SmoWorkerHostInfo(
    string ExecutablePath,
    string? ManagedEntryAssemblyPath);

public static class SmoWorkerHost
{
    public static SmoWorkerHostInfo ResolveCurrent()
    {
        string executablePath = Environment.ProcessPath ??
            throw new InvalidOperationException(
                "Could not resolve the current process executable.");
        if (!Path.GetFileNameWithoutExtension(executablePath).Equals(
                "dotnet",
                StringComparison.OrdinalIgnoreCase))
        {
            return new SmoWorkerHostInfo(executablePath, null);
        }

        // Assembly.Location is valid only for the framework-dependent `dotnet
        // app.dll` host handled by this branch. A published single-file editor
        // has its own process executable and returns in the branch above.
#pragma warning disable IL3000
        string? assemblyPath = Assembly.GetEntryAssembly()?.Location;
#pragma warning restore IL3000
        if (string.IsNullOrWhiteSpace(assemblyPath) || !File.Exists(assemblyPath))
        {
            throw new InvalidOperationException(
                "Could not resolve the managed entry assembly for the dotnet host.");
        }

        return new SmoWorkerHostInfo(executablePath, assemblyPath);
    }
}
