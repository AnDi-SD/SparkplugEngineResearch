using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// One native call on the original x86 main thread at the verified plain RET.
/// Function identity, argument meaning and mutation authority belong to the
/// caller. No new thread, external input, retry or custom SEH chain is created.
/// </summary>
internal sealed class NativeCallDispatcher
{
    internal const uint WorldReturn = 0x45A802;
    private const uint TrapFlag = 0x100;
    private const uint FullLegacyContext = 0x1003F;
    private const uint PageSize = 4096;
    private const uint ResultOffset = PageSize - sizeof(uint);
    private const uint MemRelease = 0x8000;
    private readonly Action<object> _log;
    private readonly TimeSpan _timeout;
    private readonly Stopwatch _clock = new();
    private DebuggerHost? _host;
    private IntPtr _page;
    private uint _pageAddress, _trapAddress, _resultAddress;
    private uint _threadId, _resumeEip, _resumeEsp, _sequence, _completed, _freed;
    private Win32Native.X86Context _saved;
    private bool _retired, _terminalFailure;
    private string _codeHash = "";

    internal NativeCallDispatcher(Action<object> log, TimeSpan? callTimeout = null)
    {
        _log = log ?? throw new ArgumentNullException(nameof(log));
        _timeout = callTimeout ?? TimeSpan.FromSeconds(10);
        if (_timeout <= TimeSpan.Zero || _timeout > TimeSpan.FromSeconds(30))
            throw new ArgumentOutOfRangeException(nameof(callTimeout), "Native call timeout must be positive and at most thirty seconds.");
    }

    internal bool Pending { get; private set; }
    internal object Summary => new { callsBegun = _sequence, callsCompleted = _completed,
        pagesFreedAtLaterWorldStop = _freed, pending = Pending, retiredPage = _retired,
        terminalFailure = _terminalFailure, timeoutMs = _timeout.TotalMilliseconds };

    /// <summary>
    /// Caller must suppress the current persistent world-stop rearm while
    /// Pending. It may rearm that stop from completed, but cannot start the next
    /// call until a later, real world-return breakpoint is reached.
    /// </summary>
    internal void Begin(DebuggerHost host, uint threadId, ref Win32Native.X86Context ctx,
        uint function, uint thisPtr, uint[] args, bool cdecl, Action<uint> completed)
    {
        ArgumentNullException.ThrowIfNull(host);
        ArgumentNullException.ThrowIfNull(args);
        ArgumentNullException.ThrowIfNull(completed);
        if (Pending || _terminalFailure) throw new InvalidOperationException("A native call is pending or this dispatcher has failed terminally; no retry is allowed.");
        if (function == 0 || (!cdecl && thisPtr == 0) || args.Length > 8)
            throw new ArgumentException("A nonzero x86 function, a this pointer for thiscall, and at most eight DWORD arguments are required.");
        if (_host is not null && !ReferenceEquals(_host, host))
            throw new InvalidOperationException("One dispatcher belongs to one owned debugger session.");
        Win32Native.X86Context full = VerifyWorldStop(host, threadId, ref ctx);
        if ((full.ContextFlags & FullLegacyContext) != FullLegacyContext)
            throw new InvalidDataException("Full legacy x86 context was not captured.");
        if ((full.EFlags & TrapFlag) != 0)
            throw new InvalidDataException("Native calls cannot begin with an outstanding single-step flag.");
        if (full.Esp < 0x10000 + 64 || full.Esp > uint.MaxValue - sizeof(uint) || (full.Esp & 3) != 0)
            throw new InvalidDataException("Invalid original x86 return stack.");
        uint returnAddress = Win32Native.ReadUInt32(host.ProcessHandle, full.Esp);
        if (returnAddress == 0) throw new InvalidDataException("Null original return address.");
        _ = Win32Native.ReadMemory(host.ProcessHandle, returnAddress, 1);
        _ = Win32Native.ReadMemory(host.ProcessHandle, function, 1);
        uint[] arguments = (uint[])args.Clone();

        // A retired page can be freed here only because this is a *later*
        // native RET stop, not the completion breakpoint inside that page.
        ReleaseRetiredAtWorldStop(host, threadId, ref ctx);
        _host = host;
        _saved = Clone(full);
        _saved.Eip = WorldReturn; // Core has logically rewound the INT3 already.
        _resumeEip = returnAddress;
        _resumeEsp = checked(full.Esp + sizeof(uint)); // exactly one plain C3
        _threadId = threadId;
        try
        {
            _page = Win32Native.VirtualAllocEx(host.ProcessHandle, IntPtr.Zero, (UIntPtr)PageSize,
                Win32Native.MemReserve | Win32Native.MemCommit, Win32Native.PageExecuteReadWrite);
            if (_page == IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error(), "Private native-call page allocation failed.");
            _pageAddress = checked((uint)_page.ToInt64());
            _resultAddress = checked(_pageAddress + ResultOffset);
            byte[] stub = BuildStub(function, thisPtr, arguments, cdecl, _resultAddress);
            if (stub.Length >= ResultOffset || stub[^1] != 0xCC)
                throw new InvalidDataException("Native-call stub exceeded its one-page contract.");
            _trapAddress = checked(_pageAddress + (uint)stub.Length - 1);
            _codeHash = Convert.ToHexString(SHA256.HashData(stub));
            byte[] page = new byte[PageSize];
            Array.Copy(stub, page, stub.Length);
            Win32Native.WriteMemory(host.ProcessHandle, _pageAddress, page, executable: false);
            if (!Win32Native.FlushInstructionCache(host.ProcessHandle, _page, (UIntPtr)(uint)stub.Length))
                throw new Win32Exception(Marshal.GetLastWin32Error(), "Native-call instruction cache flush failed.");
            byte[] actual = Win32Native.ReadMemory(host.ProcessHandle, _pageAddress, stub.Length);
            if (!actual.SequenceEqual(stub)) throw new InvalidDataException("Private native-call code readback mismatch.");

            host.Breakpoints.AddReturn(_trapAddress, threadId, "NativeCall.private-completion",
                (uint tid, uint at, ref Win32Native.X86Context context) => Complete(host, tid, at, ref context, completed));
            ++_sequence;
            Pending = true;
            _clock.Restart();
            // Preserve the original native ESP: PUSH args/CALL use only space
            // below it. The original caller's return DWORD remains untouched.
            ctx = Clone(_saved);
            ctx.Eip = _pageAddress;
            ctx.EFlags &= ~TrapFlag;
            _log(new { @event = "native-call-begin", sequence = _sequence, threadId,
                function = Hex(function), thisPointer = Hex(thisPtr), arguments, cdecl,
                page = Hex(_pageAddress), trap = Hex(_trapAddress), result = Hex(_resultAddress),
                code = Convert.ToHexString(stub), codeSha256 = _codeHash,
                originalEsp = Hex(_saved.Esp), resumeEip = Hex(_resumeEip), resumeEsp = Hex(_resumeEsp),
                contextFlags = Hex(_saved.ContextFlags), legacyFloatingPointStateSaved = true,
                contextBufferCopiesOwned = true, nativeSehChainChanged = false,
                timeoutMs = _timeout.TotalMilliseconds });
        }
        catch
        {
            // If registration or redirection was attempted, terminate the
            // enclosing owned run. Do not retry or free a possibly active page.
            _terminalFailure = true;
            throw;
        }
    }

    private void Complete(DebuggerHost host, uint threadId, uint address,
        ref Win32Native.X86Context ctx, Action<uint> completed)
    {
        try { CompleteCore(host, threadId, address, ref ctx, completed); }
        catch { _terminalFailure = true; throw; }
    }

    private void CompleteCore(DebuggerHost host, uint threadId, uint address,
        ref Win32Native.X86Context ctx, Action<uint> completed)
    {
        if (!Pending || _terminalFailure || !ReferenceEquals(host, _host) ||
            threadId != _threadId || threadId != host.MainThreadId || address != _trapAddress || ctx.Eip != _trapAddress)
            throw new InvalidDataException("Unexpected native-call completion identity.");
        if (_clock.Elapsed > _timeout)
            throw new TimeoutException("Native-call completion arrived beyond its bounded deadline; do not resume or retry.");
        if (ctx.Esp != _saved.Esp)
            throw new InvalidDataException("Native-call stack imbalance; terminate the owned run without retry.");
        uint result = Win32Native.ReadUInt32(host.ProcessHandle, _resultAddress);
        if (result != ctx.Eax) throw new InvalidDataException("Native-call result register/readback mismatch.");
        if (Win32Native.ReadUInt32(host.ProcessHandle, _saved.Esp) != _resumeEip)
            throw new InvalidDataException("The original caller return DWORD changed during the native call.");

        // This replaces the full partial-BP context. Core will commit it with
        // SetX86Context after this callback, using CONTEXT_ALL 0x1003F.
        ctx = Clone(_saved);
        ctx.Eip = _resumeEip;
        ctx.Esp = _resumeEsp;
        Pending = false;
        _retired = true;
        _clock.Stop();
        ++_completed;
        _log(new { @event = "native-call-end", sequence = _sequence, threadId,
            result, codeSha256 = _codeHash, elapsedMs = _clock.Elapsed.TotalMilliseconds,
            resumeEip = Hex(ctx.Eip), resumeEsp = Hex(ctx.Esp), contextFlags = Hex(ctx.ContextFlags),
            fullLegacyContextRestorePrepared = true, commitAfterCallback = "SoftwareBreakpointManager.SetX86Context",
            originalRetReexecuted = false, pageRetiredNotFreedInCallback = true });
        completed(result);
    }

    /// <summary>Call from the debugger idle callback; timeout aborts the whole owned run.</summary>
    internal void CheckTimeout()
    {
        if (!Pending) return;
        if (_terminalFailure) throw new InvalidOperationException("A pending native call failed terminally; terminate the owned run.");
        if (_clock.Elapsed <= _timeout) return;
        _terminalFailure = true;
        _log(new { @event = "native-call-timeout", sequence = _sequence, threadId = _threadId,
            elapsedMs = _clock.Elapsed.TotalMilliseconds, page = Hex(_pageAddress), noRetry = true });
        throw new TimeoutException("Native call did not reach its private completion trap; terminate the owned process.");
    }

    /// <summary>Never call from Complete. A confirmed process exit also reclaims its page.</summary>
    internal void ReleaseRetiredAtWorldStop(DebuggerHost host, uint threadId, ref Win32Native.X86Context ctx)
    {
        if (Pending) throw new InvalidOperationException("Cannot free an active native-call page.");
        if (!_retired) return;
        if (_terminalFailure || !ReferenceEquals(host, _host)) throw new InvalidOperationException("Retired page belongs to a different or failed run.");
        _ = VerifyWorldStop(host, threadId, ref ctx);
        if (!VirtualFreeEx(host.ProcessHandle, _page, UIntPtr.Zero, MemRelease))
            throw new Win32Exception(Marshal.GetLastWin32Error(), "Retired native-call page release failed.");
        uint freedAddress = _pageAddress;
        _page = IntPtr.Zero; _pageAddress = 0; _retired = false; ++_freed;
        _log(new { @event = "native-call-page-released", threadId, page = Hex(freedAddress), atLaterWorldStop = true });
    }

    private static Win32Native.X86Context VerifyWorldStop(DebuggerHost host, uint threadId, ref Win32Native.X86Context ctx)
    {
        if (host.ProcessHandle == IntPtr.Zero || threadId != host.MainThreadId || ctx.Eip != WorldReturn)
            throw new InvalidOperationException("Native calls require the owned main thread's verified world-return stop.");
        if (Win32Native.ReadMemory(host.ProcessHandle, WorldReturn, 1)[0] != 0xC3)
            throw new InvalidDataException("The original world-return instruction is not plain C3.");
        Win32Native.X86Context full = Win32Native.GetX86Context(threadId, includeFloatingPointState: true);
        // Core has restored C3 and rewound the callback's EIP, but has not yet
        // committed SetX86Context: live OS EIP is still INT3-address + 1.
        if (full.Eip != WorldReturn + 1 || full.Esp != ctx.Esp ||
            full.Eax != ctx.Eax || full.Ebx != ctx.Ebx || full.Ecx != ctx.Ecx || full.Edx != ctx.Edx ||
            full.Esi != ctx.Esi || full.Edi != ctx.Edi || full.Ebp != ctx.Ebp || full.EFlags != ctx.EFlags)
            throw new InvalidDataException("Logical/live world-return contexts do not match the expected INT3 stop.");
        return full;
    }

    private static Win32Native.X86Context Clone(Win32Native.X86Context value)
    {
        if (value.FloatSave.RegisterArea is not { Length: 80 } || value.ExtendedRegisters is not { Length: 512 })
            throw new InvalidDataException("Missing legacy floating-point context buffers.");
        value.FloatSave.RegisterArea = (byte[])value.FloatSave.RegisterArea.Clone();
        value.ExtendedRegisters = (byte[])value.ExtendedRegisters.Clone();
        return value;
    }

    private static byte[] BuildStub(uint function, uint thisPtr, uint[] args, bool cdecl, uint result)
    {
        var bytes = new List<byte>(64);
        void Imm(uint value) { for (int n = 0; n < 4; ++n) bytes.Add((byte)(value >> (n * 8))); }
        for (int i = args.Length - 1; i >= 0; --i) { bytes.Add(0x68); Imm(args[i]); }
        bytes.Add(0xB9); Imm(thisPtr);            // mov ecx, this
        bytes.Add(0xB8); Imm(function);           // mov eax, function
        bytes.Add(0xFF); bytes.Add(0xD0);         // call eax
        if (cdecl && args.Length != 0)
        { bytes.Add(0x83); bytes.Add(0xC4); bytes.Add(checked((byte)(args.Length * 4))); }
        bytes.Add(0xA3); Imm(result);            // mov [absolute result], eax
        bytes.Add(0xCC);                         // completion only; never resumed here
        return bytes.ToArray();
    }

    private static string Hex(uint value) => $"0x{value:X8}";
    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool VirtualFreeEx(IntPtr process, IntPtr address, UIntPtr size, uint freeType);
}
