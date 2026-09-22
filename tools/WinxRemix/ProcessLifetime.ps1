# Own host helper. Call only with retained Process objects created or identified
# by the owning launcher. An exit code can precede the final process signal.
if (-not ('WinxRemix.ProcessLifetime' -as [type])) {
  Add-Type @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
namespace WinxRemix {
  public static class ProcessLifetime {
    [DllImport("kernel32.dll", SetLastError=true)]
    private static extern uint WaitForSingleObject(IntPtr process, uint milliseconds);
    [DllImport("kernel32.dll", SetLastError=true)]
    private static extern bool GetExitCodeProcess(IntPtr process, out uint code);
    [DllImport("kernel32.dll", SetLastError=true)]
    private static extern bool TerminateProcess(IntPtr process, uint code);
    public static bool Wait(IntPtr process, uint milliseconds) {
      uint result = WaitForSingleObject(process, milliseconds);
      if (result == 0) return true;
      if (result == 258) return false;
      throw new Win32Exception(Marshal.GetLastWin32Error(), "Owned process wait failed");
    }
    public static int ExitCode(IntPtr process, bool allowRunning) {
      if (!allowRunning && !Wait(process, 0))
        throw new InvalidOperationException("Owned process is not signaled");
      uint code;
      if (!GetExitCodeProcess(process, out code)) throw new Win32Exception();
      return unchecked((int)code);
    }
    public static void Stop(IntPtr process, uint code, uint milliseconds) {
      if (Wait(process, 0)) return;
      if (!TerminateProcess(process, code)) {
        int error = Marshal.GetLastWin32Error();
        // The process can finish between the first wait and termination.
        // A real signal establishes completion even when termination lost that race.
        if (Wait(process, 0)) return;
        throw new Win32Exception(error);
      }
      if (!Wait(process, milliseconds))
        throw new TimeoutException("Owned process did not signal after termination");
    }
  }
}
'@
}

function Wait-OwnedProcess {
  param([Parameter(Mandatory=$true)][Diagnostics.Process]$Process,
        [ValidateRange(0,60000)][uint32]$Milliseconds=0)
  [WinxRemix.ProcessLifetime]::Wait($Process.Handle,$Milliseconds)
}

function Get-OwnedProcessExitCode {
  param([Parameter(Mandatory=$true)][Diagnostics.Process]$Process,[switch]$AllowRunning)
  [WinxRemix.ProcessLifetime]::ExitCode($Process.Handle,[bool]$AllowRunning)
}

function Stop-OwnedProcess {
  param([Parameter(Mandatory=$true)][Diagnostics.Process]$Process,
        [uint32]$ExitCode=1,[ValidateRange(1,60000)][uint32]$Milliseconds=10000)
  [WinxRemix.ProcessLifetime]::Stop($Process.Handle,$ExitCode,$Milliseconds)
}
