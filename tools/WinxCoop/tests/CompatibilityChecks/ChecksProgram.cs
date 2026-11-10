using System.Reflection;
using System.Runtime.Loader;
using System.Security.Cryptography;
using System.Text.Json;

namespace WinxCoop.CompatibilityChecks;

// Pure managed contracts against exact Actor/Core bytes. No game, debug API,
// native DLL, process launcher or VM is started by these checks.
internal static class ChecksProgram
{
    private sealed record Check(string Name, bool Passed, string? Failure);
    private static int Main(string[] args)
    {
        var results = new List<Check>();
        string? output = null, failure = null;
        try
        {
            Require(args.Length == 8, "Use --actor-directory PATH --actor-sha256 SHA64 --core-sha256 SHA64 --output FRESH_PATH.");
            var options = new Dictionary<string, string>(StringComparer.Ordinal);
            for (int index = 0; index < args.Length; index += 2)
                Require(options.TryAdd(args[index], args[index + 1]), "Repeated check option.");
            Require(options.Count == 4 && options.ContainsKey("--actor-directory") && options.ContainsKey("--actor-sha256") &&
                options.ContainsKey("--core-sha256") && options.ContainsKey("--output"), "Unknown check option.");
            output = options["--output"];
            string directory = options["--actor-directory"];
            Require(Path.IsPathFullyQualified(directory) && Path.IsPathFullyQualified(output) &&
                !File.Exists(output) && !Directory.Exists(output), "Pinned Actor directory and fresh absolute output required.");
            using var coreFile = OpenPinned(Path.Combine(directory, "SmoNativeValidator.Core.dll"), options["--core-sha256"]);
            using var actorFile = OpenPinned(Path.Combine(directory, "WinxCoop.ActorProbe.dll"), options["--actor-sha256"]);
            Assembly core = AssemblyLoadContext.Default.LoadFromStream(coreFile);
            Assembly actor = AssemblyLoadContext.Default.LoadFromStream(actorFile);
            Require(core.GetName().Name == "SmoNativeValidator.Core" && actor.GetName().Name == "WinxCoop.ActorProbe",
                "Pinned assembly identity changed.");
            Directory.CreateDirectory(output);
            Run("native MBI32/MBI64 complete field layouts", () => AbiChecks.MemoryLayouts(actor));
            Run("MBI32 normalization zero-extends all native fields", () => AbiChecks.Normalize32(actor));
            Run("debug event/context ABI matches the actual checker architecture", () => AbiChecks.DebuggerLayouts(core));
            Run("VirtualQueryEx imports retain exact native layouts", () => AbiChecks.QueryImports(actor));
            Run("pose C ABI is sixty bytes on both architectures", () => AbiChecks.PoseLayout(actor));
            Run("local control JSON rejects root and nested unknown members", () => ControlJsonChecks.Run(actor));
            Run("ASCII and stream-hash compatibility wrappers", () => ControlJsonChecks.FrameworkWrappers(actor));
            Run("NET6 missing-BCL shims retain sorted/exact contracts", () => ControlJsonChecks.FrameworkShims(actor));
            Run("framework guard exception types and parameter payloads", () => ControlJsonChecks.FrameworkGuards(core));
        }
        catch (Exception error) { failure = error.ToString(); }
        bool passed = failure is null && results.Count == 9 && results.All(result => result.Passed);
        if (output is not null && Directory.Exists(output))
        {
            using var file = new FileStream(Path.Combine(output, "report.json"), FileMode.CreateNew, FileAccess.Write, FileShare.Read);
            JsonSerializer.Serialize(file, new { schema = 1, passed, failure, checkCount = results.Count, checks = results,
                framework = System.Runtime.InteropServices.RuntimeInformation.FrameworkDescription,
                processArchitecture = System.Runtime.InteropServices.RuntimeInformation.ProcessArchitecture.ToString(),
                gameStarted = false, nativeApiInvoked = false, nativeModuleLoaded = false, vmChanged = false,
                gameplayOrWin7RuntimeQualified = false }, new JsonSerializerOptions { WriteIndented = true });
        }
        Console.WriteLine(passed ? "Compatibility checks PASS (managed contracts only)." : "Compatibility checks FAIL: " + failure);
        return passed ? 0 : 1;

        void Run(string name, Action action)
        {
            try { action(); results.Add(new(name, true, null)); }
            catch (Exception error) { results.Add(new(name, false, error.ToString())); throw; }
        }
    }

    private static FileStream OpenPinned(string path, string expected)
    {
        Require(expected.Length == 64 && expected.All(Uri.IsHexDigit), "Explicit SHA256 pin required.");
        var file = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read);
        try
        {
            using var hash = SHA256.Create();
            string actual = Convert.ToHexString(hash.ComputeHash(file));
            Require(actual.Equals(expected, StringComparison.OrdinalIgnoreCase), "Pinned managed assembly bytes changed.");
            file.Position = 0; return file;
        }
        catch { file.Dispose(); throw; }
    }

    internal static void Require(bool condition, string message)
    { if (!condition) throw new InvalidDataException(message); }
    internal static Type Type(Assembly assembly, string name) => assembly.GetType(name, throwOnError: true)!;
    internal static object? Invoke(MethodInfo method, params object?[]? arguments)
    {
        try { return method.Invoke(null, arguments); }
        catch (TargetInvocationException error) when (error.InnerException is not null)
        { System.Runtime.ExceptionServices.ExceptionDispatchInfo.Capture(error.InnerException).Throw(); throw; }
    }
}
