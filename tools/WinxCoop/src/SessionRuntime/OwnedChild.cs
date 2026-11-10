using System.ComponentModel;
using System.Diagnostics;
using System.Globalization;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using Microsoft.Win32.SafeHandles;
using WinxCoop.RuntimeProbe;

namespace WinxCoop.SessionRuntime;

internal sealed class OwnedChild : IDisposable
{
    private readonly SafeWaitHandle _job;
    private readonly RuntimeProcessPolicy? _runtimePolicy;
    private Task? _stdout, _stderr;
    private long _stdoutBytes, _stderrBytes;
    private bool _disposed;
    private object? _finalReport;
    private const long CaptureLimit = 64L * 1024 * 1024;
    internal string Role { get; }
    internal Process Process { get; }
    internal string ExpectedImage { get; }
    internal string ImageSha256 { get; }
    internal string ExpectedJobName { get; }
    internal ulong CreationFileTime { get; private set; }
    internal bool JobConfirmed { get; private set; }
    internal bool KilledByCoordinator { get; private set; }
    internal string StdoutPath { get; }
    internal string StderrPath { get; }

    private OwnedChild(string role, Process process, string executable, string output, string jobName, SafeWaitHandle job,
        RuntimeProcessPolicy? runtimePolicy)
    {
        Role = role; Process = process; ExpectedImage = executable; ExpectedJobName = jobName; _job = job;
        _runtimePolicy = runtimePolicy;
        ImageSha256 = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(executable)));
        StdoutPath = Path.Combine(output, role + "-stdout.txt"); StderrPath = Path.Combine(output, role + "-stderr.txt");
    }

    internal static OwnedChild Start(string role, string executable, IEnumerable<string> arguments,
        string outputDirectory, string expectedJobName)
    {
        Require(role.Length is > 0 and <= 48 && role.All(c => RuntimeFramework.IsAsciiLetterOrDigit(c) || c is '-' or '_'), "Bounded child role required.");
        string image = WorkspacePath(executable), output = WorkspacePath(outputDirectory);
        Require(File.Exists(image) && (File.GetAttributes(image) & FileAttributes.ReparsePoint) == 0 &&
            !Path.GetFileName(image).Equals("Outer.exe", StringComparison.OrdinalIgnoreCase) &&
            !Path.GetFileName(image).Equals("XpGameProbe.exe", StringComparison.OrdinalIgnoreCase) &&
            !role.Contains("parent", StringComparison.OrdinalIgnoreCase), "Native rollback parents cannot be OwnedChild descendants.");
        Require(expectedJobName.StartsWith(@"Local\WinxCoop.E6.", StringComparison.Ordinal) &&
            expectedJobName.Length > @"Local\WinxCoop.E6.".Length && expectedJobName.Length <= @"Local\WinxCoop.E6.".Length + 64 &&
            expectedJobName.Skip(@"Local\WinxCoop.E6.".Length).All(c => RuntimeFramework.IsAsciiLetterOrDigit(c) || c is '.' or '-' or '_'),
            "An exact bounded E6 native-parent Job name is required.");
        return StartAdmitted(role, image, arguments, output, expectedJobName, null);
    }

    // E7 callers explicitly admit their pinned bundle and local case output.
    // The retained process/parent/exact-Job and direct-child stop rules below
    // are shared with the original E6 path; no parent or tree kill is added.
    internal static OwnedChild Start(string role, string executable, IEnumerable<string> arguments,
        string outputDirectory, RuntimeProcessPolicy policy)
    {
        ArgumentNullException.ThrowIfNull(policy);
        Require(role.Length is > 0 and <= 48 && role.All(c => RuntimeFramework.IsAsciiLetterOrDigit(c) || c is '-' or '_'), "Bounded child role required.");
        string image = policy.ValidateImage(executable), output = policy.ValidateOutput(outputDirectory);
        Require(!Path.GetFileName(image).Equals("Outer.exe", StringComparison.OrdinalIgnoreCase) &&
            !Path.GetFileName(image).Equals("XpGameProbe.exe", StringComparison.OrdinalIgnoreCase) &&
            !role.Contains("parent", StringComparison.OrdinalIgnoreCase), "Native rollback parents cannot be OwnedChild descendants.");
        return StartAdmitted(role, image, arguments, output, policy.JobName, policy);
    }

    private static OwnedChild StartAdmitted(string role, string image, IEnumerable<string> arguments,
        string output, string expectedJobName, RuntimeProcessPolicy? runtimePolicy)
    {
        // E7 keeps the pinned image open without write/delete sharing until
        // Process.Start has returned; E6 retains its original launch behavior.
        using FileStream? pinnedImage = runtimePolicy?.OpenPinnedImage(image);
        Directory.CreateDirectory(output);
        runtimePolicy?.ValidateOutput(output);
        var job = OpenJobObject(4, false, expectedJobName); // query-only, never configure/terminate the native parent's Job
        if (job.IsInvalid) { job.Dispose(); throw new Win32Exception(Marshal.GetLastWin32Error(), "Open exact native-parent Job"); }
        using (Process self = Process.GetCurrentProcess())
        {
            if (!IsProcessInJob(self.SafeHandle, job, out bool member) || !member)
            {
                job.Dispose();
                throw new InvalidDataException("Coordinator itself is not in the supplied exact native-parent Job.");
            }
        }
        var info = new ProcessStartInfo(image)
        {
            UseShellExecute = false, CreateNoWindow = true, WindowStyle = ProcessWindowStyle.Hidden,
            WorkingDirectory = Path.GetDirectoryName(image)!, RedirectStandardOutput = true, RedirectStandardError = true
        };
        foreach (string argument in arguments) info.ArgumentList.Add(argument);
        var process = new Process { StartInfo = info };
        var child = new OwnedChild(role, process, image, output, expectedJobName, job, runtimePolicy);
        try
        {
            Require(process.Start(), "Process.Start returned false for " + role);
            child.CreationFileTime = NativeCreation(process);
            child._stdout = child.DrainAsync(process.StandardOutput.BaseStream, child.StdoutPath, true);
            child._stderr = child.DrainAsync(process.StandardError.BaseStream, child.StderrPath, false);
            child.VerifyAlive(); child.JobConfirmed = true;
            return child;
        }
        catch (Exception error)
        {
            // Preserve the retained object for exact-handle recovery. A native
            // parent is never killed, and a failed identity is not guessed.
            throw new OwnedChildStartException(child, error);
        }
    }

    internal void VerifyAlive()
    {
        VerifyRetainedIdentity(); CheckDrain();
    }
    private void VerifyRetainedIdentity()
    {
        _runtimePolicy?.ValidateImagePath(ExpectedImage);
        _runtimePolicy?.ValidateOutput(Path.GetDirectoryName(StdoutPath)!);
        Require(!_disposed && !Process.HasExited && CreationFileTime != 0 && NativeCreation(Process) == CreationFileTime &&
            SamePath(NativeImage(Process), ExpectedImage) && ParentPid(Process) == checked((uint)Environment.ProcessId) &&
            IsProcessInJob(Process.SafeHandle, _job, out bool member) && member,
            "Exact retained child image/creation/parent/named-Job identity changed: " + Role);
    }

    internal async Task<int> WaitAsync(TimeSpan timeout)
    {
        Require(!_disposed && timeout > TimeSpan.Zero, "Bounded retained-child wait required.");
        await Process.WaitForExitAsync().WaitAsync(timeout);
        await FinishDrainAsync(TimeSpan.FromSeconds(5)); return Process.ExitCode;
    }
    internal async Task StopAsync()
    {
        Require(!_disposed, "Retained child is disposed.");
        if (!Process.HasExited)
        {
            VerifyRetainedIdentity(); Process.Kill(entireProcessTree: false); KilledByCoordinator = true;
        }
        await WaitAsync(TimeSpan.FromSeconds(10));
    }
    internal async Task FinishDrainAsync(TimeSpan timeout)
    { if (_stdout is not null && _stderr is not null) await Task.WhenAll(_stdout, _stderr).WaitAsync(timeout); }
    internal void CheckDrain()
    {
        if (_stdout?.IsFaulted == true) _stdout.GetAwaiter().GetResult();
        if (_stderr?.IsFaulted == true) _stderr.GetAwaiter().GetResult();
    }
    private async Task DrainAsync(Stream source, string destination, bool stdout)
    {
        await using var file = new FileStream(destination, FileMode.CreateNew, FileAccess.Write, FileShare.Read,
            65536, FileOptions.Asynchronous | FileOptions.SequentialScan);
        byte[] buffer = new byte[65536]; long total = 0;
        while (true)
        {
            int count = await source.ReadAsync(buffer); if (count == 0) break;
            int preserve = checked((int)Math.Max(0, Math.Min(count, CaptureLimit - total)));
            if (preserve != 0) await file.WriteAsync(buffer.AsMemory(0, preserve));
            total += count; if (stdout) _stdoutBytes = total; else _stderrBytes = total;
        }
        await file.FlushAsync();
        Require(total <= CaptureLimit, "Child pipe exceeded64MiB; remaining bytes drained, not silently accepted.");
    }
    internal object Report() => _finalReport ?? new
    {
        Role, pid = Process.Id, creationFileTimeDecimal = CreationFileTime.ToString(CultureInfo.InvariantCulture),
        expectedImage = ExpectedImage, imageSha256 = ImageSha256, jobName = ExpectedJobName,
        exactNamedJobConfirmed = JobConfirmed, stopped = Process.HasExited,
        exitCode = Process.HasExited ? Process.ExitCode : (int?)null, killedByCoordinator = KilledByCoordinator,
        stdout = StdoutPath, stderr = StderrPath, stdoutBytes = _stdoutBytes, stderrBytes = _stderrBytes,
        nativeParentTerminated = false, processTreeKillRequested = false
    };
    public void Dispose()
    {
        if (_disposed) return;
        Require(Process.HasExited, "A running retained child must be explicitly stopped/waited before Dispose.");
        _finalReport = Report(); _disposed = true; _job.Dispose(); Process.Dispose();
    }

    internal static string WorkspacePath(string path)
    {
        string full = Path.GetFullPath(path);
        Require(Path.IsPathFullyQualified(full) && full.StartsWith(@"X:\VSProjects\SparkplugEngineResearch\", StringComparison.OrdinalIgnoreCase),
            "Owned E6 path must remain inside this workspace."); return full;
    }
    internal static bool SamePath(string first, string second) => Path.GetFullPath(first).Equals(Path.GetFullPath(second), StringComparison.OrdinalIgnoreCase);
    internal static ulong NativeCreation(Process process)
    {
        if (!GetProcessTimes(process.SafeHandle, out long created, out _, out _, out _))
            throw new Win32Exception(Marshal.GetLastWin32Error(), "Owned process creation time");
        return checked((ulong)created);
    }
    private static string NativeImage(Process process)
    {
        var value = new StringBuilder(32768); uint count = checked((uint)value.Capacity);
        if (!QueryFullProcessImageName(process.SafeHandle, 0, value, ref count))
            throw new Win32Exception(Marshal.GetLastWin32Error(), "Owned process image"); return value.ToString();
    }
    private static uint ParentPid(Process process)
    {
        int status = NtQueryInformationProcess(process.SafeHandle, 0, out ProcessBasicInformation info,
            Marshal.SizeOf<ProcessBasicInformation>(), out int returned);
        Require(status == 0 && returned == Marshal.SizeOf<ProcessBasicInformation>() &&
            info.ProcessId.ToInt64() == process.Id, "Owned child basic process identity failed.");
        return checked((uint)info.ParentProcessId.ToInt64());
    }
    internal static void Require(bool value, string message) { if (!value) throw new InvalidDataException(message); }
    [StructLayout(LayoutKind.Sequential)] private struct ProcessBasicInformation
    { internal IntPtr Reserved1, Peb, Reserved2A, Reserved2B, ProcessId, ParentProcessId; }
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern SafeWaitHandle OpenJobObject(uint access, bool inherit, string name);
    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)] private static extern bool IsProcessInJob(SafeProcessHandle process, SafeWaitHandle job, out bool result);
    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)] private static extern bool GetProcessTimes(SafeProcessHandle process, out long created, out long exited, out long kernel, out long user);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)] private static extern bool QueryFullProcessImageName(SafeProcessHandle process, uint flags, StringBuilder name, ref uint size);
    [DllImport("ntdll.dll")] private static extern int NtQueryInformationProcess(SafeProcessHandle process, int informationClass,
        out ProcessBasicInformation information, int length, out int returned);
}

internal sealed class OwnedChildStartException(OwnedChild child, Exception cause) : Exception("Owned child launch qualification failed; retained handle is available for cleanup.", cause)
{ internal OwnedChild Child { get; } = child; }
