using System.Runtime.InteropServices;
using System.Runtime.Versioning;
using System.Text.Json;
using WinxCoop.RuntimeProbe;
using WinxCoop.SessionRuntime;

[assembly: SupportedOSPlatform("windows")]

namespace WinxCoop.SessionRuntimeChecks;

// Native receipts below are explicit contract fixtures, never actual game
// receipts. Only the control TCP sockets and native C ABI execute here.
internal static class ChecksProgram
{
    private sealed record Check(string Name, bool Passed, string? Failure);
    internal static void Require(bool value, string message)
    { if (!value) throw new InvalidDataException(message); }
    internal static void Reject(Action operation, string message)
    {
        try { operation(); }
        catch (Exception error) when (error is InvalidDataException or ArgumentException or InvalidOperationException) { return; }
        throw new InvalidDataException(message);
    }

    private static int Main(string[] args)
    {
        string? output = null;
        IntPtr module = IntPtr.Zero;
        var checks = new List<Check>();
        string? failure = null, nativeSha = null;
        try
        {
            Require(OperatingSystem.IsWindows() &&
                RuntimeInformation.ProcessArchitecture is Architecture.X64 or Architecture.X86,
                "Windows x86 or x64 checks required.");
            var values = new Dictionary<string, string>(StringComparer.Ordinal);
            Require(args.Length == 6, "Use --native-library <absolute DLL> --native-sha256 <SHA64> --output <fresh absolute directory>.");
            for (int index = 0; index < args.Length; index += 2)
                Require(values.TryAdd(args[index], args[index + 1]), "Repeated check option.");
            Require(values.Count == 3 && values.ContainsKey("--native-library") && values.ContainsKey("--native-sha256") && values.ContainsKey("--output"), "Unknown check option.");
            string library = values["--native-library"];
            output = values["--output"];
            Require(Path.IsPathFullyQualified(library) && Path.IsPathFullyQualified(output) &&
                !Directory.Exists(output) && !File.Exists(output), "Explicit native DLL and fresh absolute check output required.");
            library = Path.GetFullPath(library);
            output = Path.GetFullPath(output);
            string expected = values["--native-sha256"];
            Require(expected.Length == 64 && expected.All(Uri.IsHexDigit), "Explicit native SHA256 required.");
            using (var pinned = new FileStream(library, FileMode.Open, FileAccess.Read, FileShare.Read))
            {
                nativeSha = Convert.ToHexString(RuntimeFramework.HashSha256(pinned));
                Require(nativeSha.Equals(expected, StringComparison.OrdinalIgnoreCase), "Native check module pin differs.");
                module = NativeLibrary.Load(library);
            }
            IntPtr retainedModule = module;
            NativeLibrary.SetDllImportResolver(typeof(ProgressionNative).Assembly,
                (name, _, _) => name == "WinxCoop.ProgressionNative.dll" ? retainedModule : IntPtr.Zero);
            Directory.CreateDirectory(output);
            Run("actual TCP fragmented7/5 full transition", ProgressionChannelChecks.FragmentedTransition);
            Run("actual TCP prefix then EOF stays incomplete", ProgressionChannelChecks.PrefixThenEof);
            Run("positive complete record consumed before EOF", ProgressionChannelChecks.CompleteThenEof);
            Run("wrong native identity remains fenced", ProgressionChannelChecks.WrongIdentity);
            Run("incomplete native applied receipt cannot Resume", ProgressionChannelChecks.IncompleteAppliedReceipt);
            Run("native deadline survives absent Guest receipt", ProgressionChannelChecks.NativeTimeout);
            Run("socket Dispose is idempotent and Session borrowed", ProgressionChannelChecks.BorrowedSessionLifetime);
            Run("immutable runtime process admission policy", () => ProcessPolicyChecks.Run(output));
        }
        catch (Exception error) { failure = error.ToString(); }
        finally { if (module != IntPtr.Zero) NativeLibrary.Free(module); }
        bool passed = failure is null && checks.Count == 8 && checks.All(check => check.Passed);
        if (output is not null && Directory.Exists(output))
        {
            using var file = new FileStream(Path.Combine(output, "report.json"), FileMode.CreateNew, FileAccess.Write, FileShare.Read);
            JsonSerializer.Serialize(file, new
            {
                kind = "WinxCoop-SessionRuntime-actual-native-control-TCP-checks", passed, failure,
                nativeSha256 = nativeSha, checkCount = checks.Count, checks, gameStarted = false, nativeReceiptsAreFixtures = true,
                processArchitecture = RuntimeInformation.ProcessArchitecture.ToString(), processPointerBytes = IntPtr.Size,
                frameworkDescription = RuntimeInformation.FrameworkDescription,
                actualLoopbackTcp = true, LANGameplayQualified = false, windows7Qualified = false,
                nativeModuleReleasedAfterSessions = true
            }, new JsonSerializerOptions { WriteIndented = true });
        }
        Console.WriteLine(passed ? "SessionRuntime checks PASS (no game; native receipts are fixtures)." : "SessionRuntime checks FAIL: " + failure);
        return passed ? 0 : 1;

        void Run(string name, Action action)
        {
            try { action(); checks.Add(new(name, true, null)); }
            catch (Exception error) { checks.Add(new(name, false, error.ToString())); throw; }
        }
    }
}
