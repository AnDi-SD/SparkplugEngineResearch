# Read-only memory snapshot without WMI permissions or external dependencies.
$ErrorActionPreference='Stop'
if (-not ('WinxRemixResourceMonitor' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class WinxRemixResourceMonitor {
    [StructLayout(LayoutKind.Sequential)] public struct Memory {
        public uint length, load;
        public ulong totalPhysical, availablePhysical, totalCommit, availableCommit;
        public ulong totalVirtual, availableVirtual, unused;
    }
    [DllImport("kernel32.dll", SetLastError=true)]
    private static extern bool GlobalMemoryStatusEx(ref Memory memory);
    public static Memory Read() {
        var memory = new Memory(); memory.length = (uint)Marshal.SizeOf(typeof(Memory));
        if (!GlobalMemoryStatusEx(ref memory)) throw new System.ComponentModel.Win32Exception();
        return memory;
    }
}
'@
}
$memory=[WinxRemixResourceMonitor]::Read()
[pscustomobject]@{totalPhysicalBytes=$memory.totalPhysical;availablePhysicalBytes=$memory.availablePhysical;
    totalCommitBytes=$memory.totalCommit;availableCommitBytes=$memory.availableCommit;loadPercent=$memory.load}
