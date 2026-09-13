using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;

namespace SmoNativeValidator.Core;

internal static class Win32Native
{
    internal const uint DebugOnlyThisProcess = 0x00000002;
    internal const uint CreateNewProcessGroup = 0x00000200;
    internal const uint CreateDefaultErrorMode = 0x04000000;
    internal const uint DbgContinue = 0x00010002;
    internal const uint DbgExceptionNotHandled = 0x80010001;
    internal const uint StillActive = 259;
    internal const uint WaitObject0 = 0;
    internal const uint WaitTimeout = 258;
    internal const uint WaitFailed = 0xFFFFFFFF;
    internal const int ErrorInvalidParameter = 87;
    internal const uint PageExecuteReadWrite = 0x40;
    internal const uint ThreadGetContext = 0x0008;
    internal const uint ThreadSetContext = 0x0010;
    internal const uint ThreadQueryInformation = 0x0040;

    internal enum DebugEventCode : uint
    {
        Exception = 1,
        CreateThread = 2,
        CreateProcess = 3,
        ExitThread = 4,
        ExitProcess = 5,
        LoadDll = 6,
        UnloadDll = 7,
        OutputDebugString = 8,
        Rip = 9
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    internal struct StartupInfo
    {
        internal uint Size;
        internal string? Reserved;
        internal string? Desktop;
        internal string? Title;
        internal uint X;
        internal uint Y;
        internal uint XSize;
        internal uint YSize;
        internal uint XCountChars;
        internal uint YCountChars;
        internal uint FillAttribute;
        internal uint Flags;
        internal ushort ShowWindow;
        internal ushort Reserved2Length;
        internal IntPtr Reserved2;
        internal IntPtr StandardInput;
        internal IntPtr StandardOutput;
        internal IntPtr StandardError;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct ProcessInformation
    {
        internal IntPtr Process;
        internal IntPtr Thread;
        internal uint ProcessId;
        internal uint ThreadId;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct ExceptionRecord
    {
        internal uint ExceptionCode;
        internal uint ExceptionFlags;
        internal IntPtr NestedRecord;
        internal IntPtr ExceptionAddress;
        internal uint NumberParameters;
        internal UIntPtr ExceptionInformation0;
        internal UIntPtr ExceptionInformation1;
        internal UIntPtr ExceptionInformation2;
        internal UIntPtr ExceptionInformation3;
        internal UIntPtr ExceptionInformation4;
        internal UIntPtr ExceptionInformation5;
        internal UIntPtr ExceptionInformation6;
        internal UIntPtr ExceptionInformation7;
        internal UIntPtr ExceptionInformation8;
        internal UIntPtr ExceptionInformation9;
        internal UIntPtr ExceptionInformation10;
        internal UIntPtr ExceptionInformation11;
        internal UIntPtr ExceptionInformation12;
        internal UIntPtr ExceptionInformation13;
        internal UIntPtr ExceptionInformation14;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct ExceptionDebugInfo
    {
        internal ExceptionRecord ExceptionRecord;
        internal uint FirstChance;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct CreateThreadDebugInfo
    {
        internal IntPtr Thread;
        internal IntPtr ThreadLocalBase;
        internal IntPtr StartAddress;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct CreateProcessDebugInfo
    {
        internal IntPtr File;
        internal IntPtr Process;
        internal IntPtr Thread;
        internal IntPtr BaseOfImage;
        internal uint DebugInfoFileOffset;
        internal uint DebugInfoSize;
        internal IntPtr ThreadLocalBase;
        internal IntPtr StartAddress;
        internal IntPtr ImageName;
        internal ushort Unicode;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct ExitThreadDebugInfo
    {
        internal uint ExitCode;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct ExitProcessDebugInfo
    {
        internal uint ExitCode;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct LoadDllDebugInfo
    {
        internal IntPtr File;
        internal IntPtr BaseOfDll;
        internal uint DebugInfoFileOffset;
        internal uint DebugInfoSize;
        internal IntPtr ImageName;
        internal ushort Unicode;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct UnloadDllDebugInfo
    {
        internal IntPtr BaseOfDll;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct OutputDebugStringInfo
    {
        internal IntPtr DebugStringData;
        internal ushort Unicode;
        internal ushort DebugStringLength;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct RipInfo
    {
        internal uint Error;
        internal uint Type;
    }

    [StructLayout(LayoutKind.Explicit)]
    internal struct DebugEventUnion
    {
        [FieldOffset(0)] internal ExceptionDebugInfo Exception;
        [FieldOffset(0)] internal CreateThreadDebugInfo CreateThread;
        [FieldOffset(0)] internal CreateProcessDebugInfo CreateProcess;
        [FieldOffset(0)] internal ExitThreadDebugInfo ExitThread;
        [FieldOffset(0)] internal ExitProcessDebugInfo ExitProcess;
        [FieldOffset(0)] internal LoadDllDebugInfo LoadDll;
        [FieldOffset(0)] internal UnloadDllDebugInfo UnloadDll;
        [FieldOffset(0)] internal OutputDebugStringInfo DebugString;
        [FieldOffset(0)] internal RipInfo Rip;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct DebugEvent
    {
        internal DebugEventCode Code;
        internal uint ProcessId;
        internal uint ThreadId;
        internal DebugEventUnion Info;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct FloatingSaveArea
    {
        internal uint ControlWord;
        internal uint StatusWord;
        internal uint TagWord;
        internal uint ErrorOffset;
        internal uint ErrorSelector;
        internal uint DataOffset;
        internal uint DataSelector;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 80)]
        internal byte[] RegisterArea;
        internal uint Cr0NpxState;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct X86Context
    {
        private const uint ContextI386 = 0x00010000;
        private const uint ContextControl = ContextI386 | 0x00000001;
        private const uint ContextInteger = ContextI386 | 0x00000002;
        private const uint ContextSegments = ContextI386 | 0x00000004;

        internal uint ContextFlags;
        internal uint Dr0;
        internal uint Dr1;
        internal uint Dr2;
        internal uint Dr3;
        internal uint Dr6;
        internal uint Dr7;
        internal FloatingSaveArea FloatSave;
        internal uint SegGs;
        internal uint SegFs;
        internal uint SegEs;
        internal uint SegDs;
        internal uint Edi;
        internal uint Esi;
        internal uint Ebx;
        internal uint Edx;
        internal uint Ecx;
        internal uint Eax;
        internal uint Ebp;
        internal uint Eip;
        internal uint SegCs;
        internal uint EFlags;
        internal uint Esp;
        internal uint SegSs;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 512)]
        internal byte[] ExtendedRegisters;

        internal static X86Context Create() => new()
        {
            ContextFlags = ContextControl | ContextInteger | ContextSegments,
            FloatSave = new FloatingSaveArea { RegisterArea = new byte[80] },
            ExtendedRegisters = new byte[512]
        };
    }

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool CreateProcessW(
        string? applicationName,
        StringBuilder? commandLine,
        IntPtr processAttributes,
        IntPtr threadAttributes,
        [MarshalAs(UnmanagedType.Bool)] bool inheritHandles,
        uint creationFlags,
        IntPtr environment,
        string? currentDirectory,
        ref StartupInfo startupInfo,
        out ProcessInformation processInformation);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool WaitForDebugEvent(out DebugEvent debugEvent, uint milliseconds);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool ContinueDebugEvent(uint processId, uint threadId, uint continueStatus);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool DebugSetProcessKillOnExit([MarshalAs(UnmanagedType.Bool)] bool killOnExit);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool DebugActiveProcessStop(uint processId);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool ReadProcessMemory(
        IntPtr process,
        IntPtr baseAddress,
        [Out] byte[] buffer,
        UIntPtr size,
        out UIntPtr bytesRead);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool WriteProcessMemory(
        IntPtr process,
        IntPtr baseAddress,
        byte[] buffer,
        UIntPtr size,
        out UIntPtr bytesWritten);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool VirtualProtectEx(
        IntPtr process,
        IntPtr address,
        UIntPtr size,
        uint newProtection,
        out uint oldProtection);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool FlushInstructionCache(IntPtr process, IntPtr baseAddress, UIntPtr size);

    [DllImport("kernel32.dll", SetLastError = true)]
    internal static extern IntPtr OpenThread(uint desiredAccess, [MarshalAs(UnmanagedType.Bool)] bool inheritHandle, uint threadId);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool Wow64GetThreadContext(IntPtr thread, ref X86Context context);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool Wow64SetThreadContext(IntPtr thread, ref X86Context context);

    [DllImport("kernel32.dll", EntryPoint = "GetThreadContext", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool GetThreadContext32(IntPtr thread, ref X86Context context);

    [DllImport("kernel32.dll", EntryPoint = "SetThreadContext", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool SetThreadContext32(IntPtr thread, ref X86Context context);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool TerminateProcess(IntPtr process, uint exitCode);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool GetExitCodeProcess(IntPtr process, out uint exitCode);

    [DllImport("kernel32.dll", SetLastError = true)]
    internal static extern uint WaitForSingleObject(IntPtr handle, uint milliseconds);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool CloseHandle(IntPtr handle);

    internal static X86Context GetX86Context(uint threadId)
    {
        IntPtr thread = OpenThread(ThreadGetContext | ThreadSetContext | ThreadQueryInformation, false, threadId);
        if (thread == IntPtr.Zero)
            throw new Win32Exception(Marshal.GetLastWin32Error(), $"OpenThread({threadId}) failed.");
        try
        {
            X86Context context = X86Context.Create();
            bool success = Environment.Is64BitProcess
                ? Wow64GetThreadContext(thread, ref context)
                : GetThreadContext32(thread, ref context);
            if (!success)
                throw new Win32Exception(Marshal.GetLastWin32Error(), "GetThreadContext failed.");
            return context;
        }
        finally
        {
            CloseHandle(thread);
        }
    }

    internal static void SetX86Context(uint threadId, ref X86Context context)
    {
        IntPtr thread = OpenThread(ThreadGetContext | ThreadSetContext | ThreadQueryInformation, false, threadId);
        if (thread == IntPtr.Zero)
            throw new Win32Exception(Marshal.GetLastWin32Error(), $"OpenThread({threadId}) failed.");
        try
        {
            bool success = Environment.Is64BitProcess
                ? Wow64SetThreadContext(thread, ref context)
                : SetThreadContext32(thread, ref context);
            if (!success)
                throw new Win32Exception(Marshal.GetLastWin32Error(), "SetThreadContext failed.");
        }
        finally
        {
            CloseHandle(thread);
        }
    }

    internal static byte[] ReadMemory(IntPtr process, uint address, int length)
    {
        byte[] buffer = new byte[length];
        if (!ReadProcessMemory(process, (IntPtr)(long)address, buffer, (UIntPtr)(uint)length, out UIntPtr read) ||
            read.ToUInt64() != (ulong)length)
        {
            throw new Win32Exception(Marshal.GetLastWin32Error(),
                $"ReadProcessMemory(0x{address:X8}, {length}) failed.");
        }
        return buffer;
    }

    internal static uint ReadUInt32(IntPtr process, uint address) =>
        BitConverter.ToUInt32(ReadMemory(process, address, sizeof(uint)));

    internal static string ReadAnsiString(IntPtr process, uint address, int maximumBytes = 4096)
    {
        if (address == 0)
            return string.Empty;
        List<byte> result = [];
        const int blockSize = 128;
        for (int offset = 0; offset < maximumBytes; offset += blockSize)
        {
            int count = Math.Min(blockSize, maximumBytes - offset);
            byte[] block = ReadMemory(process, checked(address + (uint)offset), count);
            int zero = Array.IndexOf(block, (byte)0);
            if (zero >= 0)
            {
                result.AddRange(block.AsSpan(0, zero).ToArray());
                break;
            }
            result.AddRange(block);
        }
        return Encoding.Latin1.GetString(result.ToArray());
    }

    internal static void WriteMemory(IntPtr process, uint address, byte[] bytes, bool executable)
    {
        uint oldProtection = 0;
        bool protectionChanged = false;
        try
        {
            if (executable)
            {
                protectionChanged = VirtualProtectEx(
                    process,
                    (IntPtr)(long)address,
                    (UIntPtr)(uint)bytes.Length,
                    PageExecuteReadWrite,
                    out oldProtection);
                if (!protectionChanged)
                    throw new Win32Exception(Marshal.GetLastWin32Error(), "VirtualProtectEx failed.");
            }

            if (!WriteProcessMemory(
                    process,
                    (IntPtr)(long)address,
                    bytes,
                    (UIntPtr)(uint)bytes.Length,
                    out UIntPtr written) ||
                written.ToUInt64() != (ulong)bytes.Length)
            {
                throw new Win32Exception(Marshal.GetLastWin32Error(),
                    $"WriteProcessMemory(0x{address:X8}, {bytes.Length}) failed.");
            }

            if (executable)
                FlushInstructionCache(process, (IntPtr)(long)address, (UIntPtr)(uint)bytes.Length);
        }
        finally
        {
            if (protectionChanged)
            {
                VirtualProtectEx(
                    process,
                    (IntPtr)(long)address,
                    (UIntPtr)(uint)bytes.Length,
                    oldProtection,
                    out _);
            }
        }
    }
}
