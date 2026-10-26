using System.Buffers.Binary;
using System.ComponentModel;
using System.Runtime.InteropServices;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// One owned RW data page in the already-owned debuggee. Every remote-memory
/// operation requires its debug stop. Native retention/borrowing semantics are
/// the caller's responsibility; no IDisposable/finalizer can free a live borrow.
/// </summary>
internal sealed class OwnedRemoteBuffer
{
    private const int PageBytes = 4096;
    private const uint MemRelease = 0x8000;
    private readonly DebuggerHost _host;
    private readonly IntPtr _process;
    private readonly uint _pid;
    private readonly int _ownerThread;
    private readonly IntPtr _allocation;
    private readonly uint _address;
    private bool _released;
    private string? _releaseKind;

    internal OwnedRemoteBuffer(DebuggerHost host, int size)
    {
        ArgumentNullException.ThrowIfNull(host);
        if (size is < 1 or > PageBytes) throw new ArgumentOutOfRangeException(nameof(size), "Remote data size must be 1..4096 bytes.");
        if (!host.IsAtDebugStop || host.ProcessHandle == IntPtr.Zero || host.ProcessPid == 0)
            throw new InvalidOperationException("Remote allocation requires the owned process's debug stop.");
        _host = host; _process = host.ProcessHandle; _pid = host.ProcessPid;
        _ownerThread = Environment.CurrentManagedThreadId;
        Size = size;
        _allocation = Win32Native.VirtualAllocEx(_process, IntPtr.Zero, (UIntPtr)PageBytes,
            Win32Native.MemReserve | Win32Native.MemCommit, Win32Native.PageReadWrite);
        if (_allocation == IntPtr.Zero)
            throw new Win32Exception(Marshal.GetLastWin32Error(), "Owned RW data-page allocation failed.");
        try
        {
            _address = checked((uint)_allocation.ToInt64());
            _ = checked(_address + PageBytes - 1);
            byte[] initial = Win32Native.ReadMemory(_process, _address, Size);
            if (initial.Any(value => value != 0)) throw new InvalidDataException("New committed data page was not zero initialized.");
        }
        catch (Exception exception)
        {
            // The constructor has not published an address to native code.
            if (!VirtualFreeEx(_process, _allocation, UIntPtr.Zero, MemRelease))
                throw new AggregateException(exception,
                    new Win32Exception(Marshal.GetLastWin32Error(), "Failed to release unpublished RW data page."));
            throw;
        }
    }

    internal int Size { get; }
    internal bool Released => _released;
    internal uint Address
    {
        get { EnsureAtStop(); return _address; }
    }
    internal object Summary => new { address = $"0x{_address:X8}", requestedBytes = Size,
        allocatedPageBytes = PageBytes, processPid = _pid, protection = "PAGE_READWRITE",
        released = _released, releaseKind = _releaseKind };

    internal void Write(int offset, byte[] bytes)
    {
        ArgumentNullException.ThrowIfNull(bytes);
        EnsureAtStop(); ValidateRange(offset, bytes.Length);
        if (bytes.Length == 0) return;
        uint at = checked(_address + (uint)offset);
        Win32Native.WriteMemory(_process, at, bytes, executable: false);
        if (!Win32Native.ReadMemory(_process, at, bytes.Length).SequenceEqual(bytes))
            throw new InvalidDataException("Owned RW data write/readback mismatch.");
    }

    /// <summary>Printable ASCII plus one NUL; returns the validated guest pointer.</summary>
    internal uint WriteAsciiZ(int offset, string text)
    {
        ArgumentNullException.ThrowIfNull(text);
        EnsureAtStop(); ValidateRange(offset, checked(text.Length + 1));
        byte[] bytes = new byte[text.Length + 1];
        for (int i = 0; i < text.Length; ++i)
        {
            if (text[i] is < ' ' or > '~')
                throw new ArgumentException("Printable ASCII without embedded NUL/control characters required.", nameof(text));
            bytes[i] = (byte)text[i];
        }
        Write(offset, bytes);
        return checked(_address + (uint)offset);
    }

    internal byte[] Read(int offset, int count)
    {
        EnsureAtStop(); ValidateRange(offset, count);
        return count == 0 ? [] : Win32Native.ReadMemory(_process, checked(_address + (uint)offset), count);
    }
    internal uint ReadUInt32(int offset) => BinaryPrimitives.ReadUInt32LittleEndian(Read(offset, sizeof(uint)));

    internal void Release(bool callPending)
    {
        if (callPending) throw new InvalidOperationException("Cannot free data while a native call may still borrow it.");
        EnsureAtStop();
        if (!VirtualFreeEx(_process, _allocation, UIntPtr.Zero, MemRelease))
            throw new Win32Exception(Marshal.GetLastWin32Error(), "Owned RW data-page release failed.");
        _released = true; _releaseKind = "VirtualFreeEx-at-debug-stop";
    }

    /// <summary>Bookkeeping only; caller must pass this host's confirmed exit result.</summary>
    internal void AcknowledgeProcessExit(bool processExitConfirmed)
    {
        if (!processExitConfirmed || _host.ProcessHandle != IntPtr.Zero || _host.ProcessPid != _pid)
            throw new InvalidOperationException("This owned process must have a confirmed completed lifetime.");
        if (_released) return;
        _released = true; _releaseKind = "reclaimed-by-confirmed-owned-process-exit";
    }

    private void EnsureAtStop()
    {
        if (_released) throw new InvalidOperationException("Remote data buffer has been released.");
        if (!_host.IsAtDebugStop || _host.ProcessHandle != _process || _host.ProcessPid != _pid ||
            Environment.CurrentManagedThreadId != _ownerThread)
            throw new InvalidOperationException("Remote data access requires the original owned host, handle and debug-stop thread.");
    }
    private void ValidateRange(int offset, int count)
    {
        if (offset < 0 || count < 0 || offset > Size || count > Size - offset)
            throw new ArgumentOutOfRangeException(nameof(offset), "Requested data range exceeds the owned logical buffer.");
    }

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool VirtualFreeEx(IntPtr process, IntPtr address, UIntPtr size, uint freeType);
}
