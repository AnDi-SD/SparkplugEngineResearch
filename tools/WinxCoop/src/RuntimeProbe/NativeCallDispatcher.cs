using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

internal sealed record NativeCallSpec(uint Function, uint ThisPointer, uint[] Arguments, bool Cdecl);
internal sealed record NativeInstancePositionCall(uint Function, uint XBits, uint YBits, uint ZBits);

/// <summary>
/// Bounded native calls on the original x86 main thread at a verified stop.
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
    private uint _originalReturnAddress, _entryAddress;
    private int _resultCount = 1;
    private byte _entryOriginalByte;
    private bool _entryBatch;
    private bool _dependencyActive;
    private int _dependencyCheckpoints, _dependencyCodeLength, _dependencyRequiredCheckpoints;
    private uint _dependentBegun, _dependentCompleted, _dependentReadStops;
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
        terminalFailure = _terminalFailure, timeoutMs = _timeout.TotalMilliseconds, dependentLifecycle = DependentSummary };

    internal object DependentSummary => new { begun = _dependentBegun, completed = _dependentCompleted,
        readStops = _dependentReadStops, active = _dependencyActive, checkpointsInCurrentInterval = _dependencyCheckpoints };

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
        _entryBatch = false;
        _dependencyActive = false;
        _resultCount = 1;
        _originalReturnAddress = returnAddress;
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

    /// <summary>
    /// One-shot pre-entry cleanup: the original first instruction has been
    /// restored, but has not executed. The caller verifies the full native
    /// signature and owns the phase/argument semantics. It MUST NOT immediately
    /// rearm this entry breakpoint: reinstall only at a later world stop.
    /// A retired page also requires a later world stop before this operation.
    /// On success the original entry instruction executes with the exact saved
    /// legacy context and unchanged original stack. This is not a plain RET.
    /// </summary>
    internal void BeginAtEntry(DebuggerHost host, uint threadId, ref Win32Native.X86Context ctx,
        uint entry, byte expectedFirstByte, IReadOnlyList<NativeCallSpec> calls, Action<uint[]> completed)
    {
        ArgumentNullException.ThrowIfNull(host);
        ArgumentNullException.ThrowIfNull(calls);
        ArgumentNullException.ThrowIfNull(completed);
        if (Pending || _terminalFailure)
            throw new InvalidOperationException("A native call is pending or this dispatcher has failed terminally; no nested call or retry is allowed.");
        if (_retired)
            throw new InvalidOperationException("A previous private page still requires release at a later world stop before entry cleanup.");
        if (_host is not null && !ReferenceEquals(_host, host))
            throw new InvalidOperationException("One dispatcher belongs to one owned debugger session.");
        if (calls.Count is < 1 or > 4)
            throw new ArgumentException("Entry cleanup requires one to four native calls.", nameof(calls));
        var ownedCalls = new NativeCallSpec[calls.Count];
        for (int i = 0; i < calls.Count; ++i)
        {
            NativeCallSpec call = calls[i] ?? throw new ArgumentException("Null native call specification.", nameof(calls));
            if (call.Function < 0x10000 || call.Arguments is null || call.Arguments.Length > 8 ||
                (!call.Cdecl && call.ThisPointer < 0x10000))
                throw new ArgumentException("A readable x86 function, a this pointer for thiscall, and at most eight DWORD arguments are required.", nameof(calls));
            _ = Win32Native.ReadMemory(host.ProcessHandle, call.Function, 1);
            if (!call.Cdecl) _ = Win32Native.ReadMemory(host.ProcessHandle, call.ThisPointer, 1);
            ownedCalls[i] = new NativeCallSpec(call.Function, call.ThisPointer, (uint[])call.Arguments.Clone(), call.Cdecl);
        }
        Win32Native.X86Context full = VerifyEntryStop(host, threadId, ref ctx, entry, expectedFirstByte);
        if ((full.ContextFlags & FullLegacyContext) != FullLegacyContext || (full.EFlags & TrapFlag) != 0)
            throw new InvalidDataException("Entry cleanup requires the full legacy context without an outstanding single-step flag.");
        if (full.Esp < 0x10000 + 64 || full.Esp > uint.MaxValue - sizeof(uint) || (full.Esp & 3) != 0)
            throw new InvalidDataException("Invalid original x86 entry stack.");
        uint returnAddress = Win32Native.ReadUInt32(host.ProcessHandle, full.Esp);
        if (returnAddress < 0x10000) throw new InvalidDataException("Null or low original entry return address.");
        _ = Win32Native.ReadMemory(host.ProcessHandle, returnAddress, 1);

        _host = host;
        _saved = Clone(full);
        _saved.Eip = entry;
        _entryBatch = true;
        _dependencyActive = false;
        _entryAddress = entry;
        _entryOriginalByte = expectedFirstByte;
        _resultCount = ownedCalls.Length;
        _originalReturnAddress = returnAddress;
        _resumeEip = entry;
        _resumeEsp = full.Esp;
        _threadId = threadId;
        try
        {
            _page = Win32Native.VirtualAllocEx(host.ProcessHandle, IntPtr.Zero, (UIntPtr)PageSize,
                Win32Native.MemReserve | Win32Native.MemCommit, Win32Native.PageExecuteReadWrite);
            if (_page == IntPtr.Zero)
                throw new Win32Exception(Marshal.GetLastWin32Error(), "Private entry-cleanup page allocation failed.");
            _pageAddress = checked((uint)_page.ToInt64());
            uint resultOffset = checked(PageSize - (uint)(_resultCount * sizeof(uint)));
            _resultAddress = checked(_pageAddress + resultOffset);
            byte[] stub = BuildEntryStub(ownedCalls, _resultAddress, _saved.Esp);
            if (stub.Length >= resultOffset || stub[^1] != 0xCC)
                throw new InvalidDataException("Entry-cleanup stub exceeded its one-page contract.");
            _trapAddress = checked(_pageAddress + (uint)stub.Length - 1);
            _codeHash = Convert.ToHexString(SHA256.HashData(stub));
            byte[] page = new byte[PageSize];
            Array.Copy(stub, page, stub.Length);
            Win32Native.WriteMemory(host.ProcessHandle, _pageAddress, page, executable: false);
            if (!Win32Native.FlushInstructionCache(host.ProcessHandle, _page, (UIntPtr)(uint)stub.Length))
                throw new Win32Exception(Marshal.GetLastWin32Error(), "Entry-cleanup instruction cache flush failed.");
            if (!Win32Native.ReadMemory(host.ProcessHandle, _pageAddress, stub.Length).SequenceEqual(stub))
                throw new InvalidDataException("Private entry-cleanup code readback mismatch.");
            host.Breakpoints.AddReturn(_trapAddress, threadId, "NativeCall.private-entry-completion",
                (uint tid, uint at, ref Win32Native.X86Context context) => CompleteEntry(host, tid, at, ref context, completed));
            ++_sequence;
            Pending = true;
            _clock.Restart();
            ctx = Clone(_saved);
            ctx.Eip = _pageAddress;
            ctx.EFlags &= ~TrapFlag;
            _log(new { @event = "native-entry-batch-begin", sequence = _sequence, threadId,
                entry = Hex(entry), expectedFirstByte, calls = ownedCalls,
                page = Hex(_pageAddress), trap = Hex(_trapAddress), results = Hex(_resultAddress),
                code = Convert.ToHexString(stub), codeSha256 = _codeHash,
                originalEsp = Hex(_saved.Esp), originalReturnAddress = Hex(returnAddress),
                resumeEip = Hex(_resumeEip), resumeEsp = Hex(_resumeEsp),
                contextFlags = Hex(_saved.ContextFlags), legacyFloatingPointStateSaved = true,
                contextBufferCopiesOwned = true, nativeSehChainChanged = false,
                eachCallStackGuardedBeforeNextCall = true, entryRearmDeferredToLaterWorldStop = true,
                timeoutMs = _timeout.TotalMilliseconds });
        }
        catch
        {
            _terminalFailure = true;
            throw;
        }
    }

    private void Complete(DebuggerHost host, uint threadId, uint address,
        ref Win32Native.X86Context ctx, Action<uint> completed)
    {
        try { completed(CompleteCore(host, threadId, address, ref ctx)[0]); }
        catch { _terminalFailure = true; throw; }
    }

    /// <summary>
    /// A genuine WorldRET interval: first call produces one owned pointer;
    /// second cdecl consumes it as its sole argument; final thiscall destroys
    /// it with flag1 unless retainAfterInitialization explicitly leaves its
    /// ownership with the caller. Two read-only checkpoints guard construction
    /// and initialization. Original gameplay never resumes between these calls.
    /// Only the final trap restores saved context; later updates/deletion of a
    /// retained result require the caller's separately bounded lifetime witness.
    /// An opt-in retained instance-position call runs after initialization and
    /// has its own third read-only checkpoint; its return value is opaque.
    /// Call/ownership semantics and pinned signatures belong to the caller.
    /// </summary>
    internal void BeginDependentLifecycle(DebuggerHost host, uint threadId, ref Win32Native.X86Context ctx,
        NativeCallSpec firstCall, uint scopedCdecl, uint deletingThiscall,
        Action<uint> createdWitness, Action<uint> initializedWitness, Action<uint[]> completed,
        bool retainAfterInitialization = false, NativeInstancePositionCall? positionAfterInitialization = null,
        Action<uint>? positionedWitness = null)
    {
        ArgumentNullException.ThrowIfNull(host); ArgumentNullException.ThrowIfNull(firstCall);
        ArgumentNullException.ThrowIfNull(createdWitness); ArgumentNullException.ThrowIfNull(initializedWitness);
        ArgumentNullException.ThrowIfNull(completed);
        if (Pending || _terminalFailure || (_host is not null && !ReferenceEquals(_host, host)))
            throw new InvalidOperationException("Dependent lifecycle requires the same nonfailed owned dispatcher without a pending call.");
        if (firstCall.Function < 0x10000 || firstCall.Arguments is null || firstCall.Arguments.Length > 8 ||
            (!firstCall.Cdecl && firstCall.ThisPointer < 0x10000) || scopedCdecl < 0x10000 || deletingThiscall < 0x10000)
            throw new ArgumentException("Invalid bounded dependent native-call specification.");
        if ((positionAfterInitialization is null) != (positionedWitness is null) ||
            (positionAfterInitialization is not null && (!retainAfterInitialization || positionAfterInitialization.Function < 0x10000 ||
             !new[] { positionAfterInitialization.XBits, positionAfterInitialization.YBits, positionAfterInitialization.ZBits }
                 .All(bits => float.IsFinite(BitConverter.Int32BitsToSingle(unchecked((int)bits)))))))
            throw new ArgumentException("Optional instance position requires finite by-value words, a retained result and a read-only witness.");
        var ownedFirst = new NativeCallSpec(firstCall.Function, firstCall.ThisPointer, (uint[])firstCall.Arguments.Clone(), firstCall.Cdecl);
        foreach (uint function in new[] { ownedFirst.Function, scopedCdecl, deletingThiscall })
            _ = Win32Native.ReadMemory(host.ProcessHandle, function, 1);
        if (positionAfterInitialization is not null)
            _ = Win32Native.ReadMemory(host.ProcessHandle, positionAfterInitialization.Function, 1);
        Win32Native.X86Context full = VerifyWorldStop(host, threadId, ref ctx);
        if ((full.ContextFlags & FullLegacyContext) != FullLegacyContext || (full.EFlags & TrapFlag) != 0 ||
            full.Esp < 0x10000 + 64 || full.Esp > uint.MaxValue - sizeof(uint) || (full.Esp & 3) != 0)
            throw new InvalidDataException("Dependent lifecycle requires the complete legacy context and qualified WorldRET stack.");
        uint returnAddress = Win32Native.ReadUInt32(host.ProcessHandle, full.Esp);
        if (returnAddress < 0x10000) throw new InvalidDataException("Invalid original dependent-lifecycle return address.");
        _ = Win32Native.ReadMemory(host.ProcessHandle, returnAddress, 1);
        ReleaseRetiredAtWorldStop(host, threadId, ref ctx);
        _host = host; _saved = Clone(full); _saved.Eip = WorldReturn;
        _entryBatch = false; _dependencyActive = true; _dependencyCheckpoints = 0;
        _dependencyRequiredCheckpoints = positionAfterInitialization is null ? 2 : 3;
        _resultCount = positionAfterInitialization is not null ? 3 : retainAfterInitialization ? 2 : 3;
        _originalReturnAddress = returnAddress; _resumeEip = returnAddress;
        _resumeEsp = checked(full.Esp + sizeof(uint)); _threadId = threadId;
        try
        {
            _page = Win32Native.VirtualAllocEx(host.ProcessHandle, IntPtr.Zero, (UIntPtr)PageSize,
                Win32Native.MemReserve | Win32Native.MemCommit, Win32Native.PageExecuteReadWrite);
            if (_page == IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error(), "Dependent lifecycle page allocation failed.");
            _pageAddress = checked((uint)_page.ToInt64());
            uint resultOffset = checked(PageSize - (uint)_resultCount * sizeof(uint));
            _resultAddress = checked(_pageAddress + resultOffset);
            var plan = BuildDependentStub(ownedFirst, scopedCdecl, deletingThiscall, _resultAddress, full.Esp,
                retainAfterInitialization, positionAfterInitialization);
            byte[] stub = plan.Code;
            if (stub.Length >= resultOffset || stub[^1] != 0xCC)
                throw new InvalidDataException("Dependent native stub exceeds its owned page.");
            _dependencyCodeLength = stub.Length; _trapAddress = checked(_pageAddress + (uint)stub.Length - 1);
            uint createdAt = checked(_pageAddress + (uint)plan.CreatedStop);
            uint initializedAt = checked(_pageAddress + (uint)plan.InitializedStop);
            _codeHash = Convert.ToHexString(SHA256.HashData(stub));
            byte[] page = new byte[PageSize]; stub.CopyTo(page, 0);
            Win32Native.WriteMemory(host.ProcessHandle, _pageAddress, page, executable: false);
            if (!Win32Native.FlushInstructionCache(host.ProcessHandle, _page, (UIntPtr)(uint)stub.Length) ||
                !Win32Native.ReadMemory(host.ProcessHandle, _pageAddress, stub.Length).SequenceEqual(stub))
                throw new InvalidDataException("Dependent native code flush/readback failed.");
            host.Breakpoints.AddReturn(createdAt, threadId, "NativeCall.dependent-created-checkpoint",
                (uint tid, uint at, ref Win32Native.X86Context context) =>
                    DependentCheckpoint(host, tid, at, createdAt, 1, ref context, createdWitness));
            host.Breakpoints.AddReturn(initializedAt, threadId, "NativeCall.dependent-initialized-checkpoint",
                (uint tid, uint at, ref Win32Native.X86Context context) =>
                    DependentCheckpoint(host, tid, at, initializedAt, 2, ref context, initializedWitness));
            if (plan.PositionedStop is int positionedOffset)
            {
                uint positionedAt = checked(_pageAddress + (uint)positionedOffset);
                host.Breakpoints.AddReturn(positionedAt, threadId, "NativeCall.dependent-positioned-checkpoint",
                    (uint tid, uint at, ref Win32Native.X86Context context) =>
                        DependentCheckpoint(host, tid, at, positionedAt, 3, ref context, positionedWitness!));
            }
            host.Breakpoints.AddReturn(_trapAddress, threadId, "NativeCall.dependent-final-completion",
                (uint tid, uint at, ref Win32Native.X86Context context) => CompleteEntry(host, tid, at, ref context, completed));
            ++_sequence; ++_dependentBegun; Pending = true; _clock.Restart();
            ctx = Clone(_saved); ctx.Eip = _pageAddress; ctx.EFlags &= ~TrapFlag;
            _log(new { @event = "native-dependent-lifecycle-begin", sequence = _sequence, threadId,
                firstCall = ownedFirst, scopedCdecl = Hex(scopedCdecl), deletingThiscall = Hex(deletingThiscall),
                page = Hex(_pageAddress), createdCheckpoint = Hex(createdAt), initializedCheckpoint = Hex(initializedAt),
                guardFailureAndFinalTrap = Hex(_trapAddress), results = Hex(_resultAddress),
                code = Convert.ToHexString(stub), codeSha256 = _codeHash, originalEsp = Hex(full.Esp),
                resumeEip = Hex(_resumeEip), resumeEsp = Hex(_resumeEsp),
                originalWorldResumesOnlyAfterFinalDelete = !retainAfterInitialization,
                retainedInitializedResult = retainAfterInitialization, eachCallStackGuardedBeforeNextCall = true,
                checkpointCallbacksReadOnly = true, legacyFloatingPointStateSaved = true, timeoutMs = _timeout.TotalMilliseconds });
            if (positionAfterInitialization is not null)
                _log(new { @event = "native-dependent-instance-position-plan", sequence = _sequence,
                    function = Hex(positionAfterInitialization.Function), instanceFromResultSlot = 0,
                    xBits = Hex(positionAfterInitialization.XBits), yBits = Hex(positionAfterInitialization.YBits), zBits = Hex(positionAfterInitialization.ZBits),
                    cdecl = true, stackArgumentBytes = 16, positionResultSlot = 2, positionResultMeaningAssigned = false,
                    positionedCheckpoint = Hex(checked(_pageAddress + (uint)plan.PositionedStop!.Value)),
                    requiredReadCheckpoints = _dependencyRequiredCheckpoints, originalGameplayResumed = false });
        }
        catch { _terminalFailure = true; throw; }
    }

    private void DependentCheckpoint(DebuggerHost host, uint tid, uint at, uint expectedAt, int completedCalls,
        ref Win32Native.X86Context ctx, Action<uint> witness)
    {
        try
        {
            if (!Pending || !_dependencyActive || _terminalFailure || !ReferenceEquals(host, _host) ||
                tid != _threadId || tid != host.MainThreadId || at != expectedAt || ctx.Eip != at ||
                ctx.Esp != _saved.Esp || _dependencyCheckpoints != completedCalls - 1 || _clock.Elapsed > _timeout ||
                Win32Native.ReadUInt32(host.ProcessHandle, _saved.Esp) != _originalReturnAddress)
                throw new InvalidDataException("Dependent checkpoint thread/phase/stack/timeout identity mismatch.");
            VerifyDependentCode(host);
            uint instance = Win32Native.ReadUInt32(host.ProcessHandle, _resultAddress);
            uint currentResult = Win32Native.ReadUInt32(host.ProcessHandle, checked(_resultAddress + (uint)(completedCalls - 1) * sizeof(uint)));
            if (instance < 0x10000 || ctx.Eax != currentResult || (ctx.EFlags & TrapFlag) != 0)
                throw new InvalidDataException("Dependent checkpoint result/register/flag mismatch.");
            witness(instance); // read-only; an exception aborts before the next native call
            if (_clock.Elapsed > _timeout) throw new TimeoutException("Dependent checkpoint exceeded the bounded native interval.");
            VerifyDependentCode(host);
            if (Win32Native.ReadUInt32(host.ProcessHandle, _saved.Esp) != _originalReturnAddress)
                throw new InvalidDataException("Original return DWORD changed during dependent witness.");
            if (Win32Native.ReadUInt32(host.ProcessHandle, _resultAddress) != instance ||
                Win32Native.ReadUInt32(host.ProcessHandle, checked(_resultAddress + (uint)(completedCalls - 1) * sizeof(uint))) != currentResult)
                throw new InvalidDataException("Owned dependent result slots changed during a read-only witness.");
            ++_dependencyCheckpoints; ++_dependentReadStops;
            ctx.Eip = checked(at + 1); // skip this private INT3, continue only our owned stub
            ctx.EFlags &= ~TrapFlag;
            _log(new { @event = "native-dependent-read-checkpoint", sequence = _sequence, threadId = tid,
                completedCalls, instance = Hex(instance), currentResult = Hex(currentResult),
                elapsedMs = _clock.Elapsed.TotalMilliseconds, originalGameplayResumed = false,
                savedLegacyContextRestoredHere = false, nextPrivateInstruction = Hex(ctx.Eip) });
        }
        catch { _terminalFailure = true; throw; }
    }

    private void VerifyDependentCode(DebuggerHost host)
    {
        byte[] code = Win32Native.ReadMemory(host.ProcessHandle, _pageAddress, _dependencyCodeLength);
        if (Convert.ToHexString(SHA256.HashData(code)) != _codeHash)
            throw new InvalidDataException("Owned dependent native code changed during its interval.");
    }

    private void CompleteEntry(DebuggerHost host, uint threadId, uint address,
        ref Win32Native.X86Context ctx, Action<uint[]> completed)
    {
        try { completed(CompleteCore(host, threadId, address, ref ctx)); }
        catch { _terminalFailure = true; throw; }
    }

    private uint[] CompleteCore(DebuggerHost host, uint threadId, uint address,
        ref Win32Native.X86Context ctx)
    {
        if (!Pending || _terminalFailure || !ReferenceEquals(host, _host) ||
            threadId != _threadId || threadId != host.MainThreadId || address != _trapAddress || ctx.Eip != _trapAddress)
            throw new InvalidDataException("Unexpected native-call completion identity.");
        if (_clock.Elapsed > _timeout)
            throw new TimeoutException("Native-call completion arrived beyond its bounded deadline; do not resume or retry.");
        if (ctx.Esp != _saved.Esp)
            throw new InvalidDataException("Native-call stack imbalance; terminate the owned run without retry.");
        if (_dependencyActive)
        {
            if (_dependencyCheckpoints != _dependencyRequiredCheckpoints)
                throw new InvalidDataException("Dependent lifecycle reached completion without all guarded witnesses.");
            VerifyDependentCode(host);
        }
        var results = new uint[_resultCount];
        for (int i = 0; i < results.Length; ++i)
            results[i] = Win32Native.ReadUInt32(host.ProcessHandle, checked(_resultAddress + (uint)(i * sizeof(uint))));
        if (results[^1] != ctx.Eax) throw new InvalidDataException("Native-call result register/readback mismatch.");
        if (Win32Native.ReadUInt32(host.ProcessHandle, _saved.Esp) != _originalReturnAddress)
            throw new InvalidDataException("The original caller return DWORD changed during the native call.");
        if (_entryBatch && Win32Native.ReadMemory(host.ProcessHandle, _entryAddress, 1)[0] != _entryOriginalByte)
            throw new InvalidDataException("Entry instruction changed or was rearmed before original execution; terminate without resuming it.");

        // This replaces the full partial-BP context. Core will commit it with
        // SetX86Context after this callback, using CONTEXT_ALL 0x1003F.
        ctx = Clone(_saved);
        ctx.Eip = _resumeEip;
        ctx.Esp = _resumeEsp;
        Pending = false;
        _retired = true;
        _clock.Stop();
        ++_completed;
        if (_dependencyActive) { ++_dependentCompleted; _dependencyActive = false; }
        _log(new { @event = "native-call-end", sequence = _sequence, threadId,
            result = results[0], results, codeSha256 = _codeHash, elapsedMs = _clock.Elapsed.TotalMilliseconds,
            resumeEip = Hex(ctx.Eip), resumeEsp = Hex(ctx.Esp), contextFlags = Hex(ctx.ContextFlags),
            fullLegacyContextRestorePrepared = true, commitAfterCallback = "SoftwareBreakpointManager.SetX86Context",
            originalRetReexecuted = false, originalEntryInstructionDeferred = _entryBatch,
            resumeMode = _entryBatch ? "original-entry-with-unchanged-stack" : "plain-C3-equivalent-return",
            pageRetiredNotFreedInCallback = true });
        return results;
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

    private static Win32Native.X86Context VerifyEntryStop(DebuggerHost host, uint threadId,
        ref Win32Native.X86Context ctx, uint entry, byte expectedFirstByte)
    {
        if (!host.IsAtDebugStop || host.ProcessHandle == IntPtr.Zero || threadId != host.MainThreadId ||
            entry < 0x10000 || entry == uint.MaxValue || ctx.Eip != entry || expectedFirstByte == 0xCC)
            throw new InvalidOperationException("Entry cleanup requires the owned main thread's one-shot restored entry stop.");
        if (Win32Native.ReadMemory(host.ProcessHandle, entry, 1)[0] != expectedFirstByte)
            throw new InvalidDataException("The original entry instruction does not match its caller-verified signature.");
        Win32Native.X86Context full = Win32Native.GetX86Context(threadId, includeFloatingPointState: true);
        if (full.Eip != entry + 1 || full.Esp != ctx.Esp ||
            full.Eax != ctx.Eax || full.Ebx != ctx.Ebx || full.Ecx != ctx.Ecx || full.Edx != ctx.Edx ||
            full.Esi != ctx.Esi || full.Edi != ctx.Edi || full.Ebp != ctx.Ebp || full.EFlags != ctx.EFlags ||
            full.SegCs != ctx.SegCs || full.SegSs != ctx.SegSs || full.SegDs != ctx.SegDs ||
            full.SegEs != ctx.SegEs || full.SegFs != ctx.SegFs || full.SegGs != ctx.SegGs)
            throw new InvalidDataException("Logical/live entry contexts do not match the expected INT3 stop.");
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

    private sealed record DependentStub(byte[] Code, int CreatedStop, int InitializedStop, int? PositionedStop);

    private static DependentStub BuildDependentStub(NativeCallSpec first, uint scopedCdecl, uint deletingThiscall,
        uint results, uint originalEsp, bool retainAfterInitialization, NativeInstancePositionCall? positionAfterInitialization)
    {
        var bytes = new List<byte>(256);
        var failures = new List<int>(4);
        void Imm(uint value) { for (int n = 0; n < 4; ++n) bytes.Add((byte)(value >> (n * 8))); }
        void ConditionalFailure(byte condition)
        {
            bytes.Add(0x0F); bytes.Add(condition); failures.Add(bytes.Count); Imm(0);
        }
        void StackGuard()
        {
            bytes.Add(0x81); bytes.Add(0xFC); Imm(originalEsp); // cmp esp, original
            ConditionalFailure(0x85);                         // jne private failure
        }
        void SaveResult(int index) { bytes.Add(0xA3); Imm(checked(results + (uint)index * sizeof(uint))); }
        void Call(uint function) { bytes.Add(0xB8); Imm(function); bytes.Add(0xFF); bytes.Add(0xD0); }
        for (int i = first.Arguments.Length - 1; i >= 0; --i) { bytes.Add(0x68); Imm(first.Arguments[i]); }
        bytes.Add(0xB9); Imm(first.ThisPointer);
        Call(first.Function);
        if (first.Cdecl && first.Arguments.Length != 0)
        { bytes.Add(0x83); bytes.Add(0xC4); bytes.Add(checked((byte)(first.Arguments.Length * 4))); }
        SaveResult(0); StackGuard();
        bytes.Add(0x3D); Imm(0x10000); ConditionalFailure(0x82); // cmp eax,10000; jb failure
        int created = bytes.Count; bytes.Add(0xCC);
        // The first witness checks type/ownership before allowing this call.
        bytes.Add(0xFF); bytes.Add(0x35); Imm(results);          // push [first result]
        bytes.Add(0xB9); Imm(0);                              // cdecl receiver unspecified/unused
        Call(scopedCdecl);
        bytes.Add(0x83); bytes.Add(0xC4); bytes.Add(4);
        SaveResult(1); StackGuard();
        int initialized = bytes.Count; bytes.Add(0xCC);
        int? positioned = null;
        if (positionAfterInitialization is not null)
        {
            // cdecl(instance, x, y, z): the runtime instance comes from Create,
            // never a guessed pre-call pointer. EAX is opaque, stored only for
            // the same register/result and stack guards as the other calls.
            foreach (uint bits in new[] { positionAfterInitialization.ZBits, positionAfterInitialization.YBits, positionAfterInitialization.XBits })
            { bytes.Add(0x68); Imm(bits); }
            bytes.Add(0xFF); bytes.Add(0x35); Imm(results);
            bytes.Add(0xB9); Imm(0);
            Call(positionAfterInitialization.Function);
            bytes.Add(0x83); bytes.Add(0xC4); bytes.Add(16);
            SaveResult(2); StackGuard();
            positioned = bytes.Count; bytes.Add(0xCC);
        }
        // The initialized witness admits either deletion in this same interval
        // or retention by the caller for a separately bounded original update.
        if (!retainAfterInitialization)
        {
            bytes.Add(0x68); Imm(1);                          // deleting flag
            bytes.Add(0x8B); bytes.Add(0x0D); Imm(results);    // mov ecx,[first result]
            Call(deletingThiscall);
            SaveResult(2); StackGuard();
        }
        int completion = bytes.Count; bytes.Add(0xCC);
        void Patch(int offset, int target)
        {
            int relative = checked(target - (offset + sizeof(uint)));
            byte[] value = BitConverter.GetBytes(relative);
            for (int n = 0; n < value.Length; ++n) bytes[offset + n] = value[n];
        }
        foreach (int branch in failures) Patch(branch, completion);
        // Every failure reaches the same one-shot final trap. Wrong ESP or
        // fewer than two witnessed phases rejects before any context restore.
        // No unused failure breakpoint remains registered when this page frees.
        return new(bytes.ToArray(), created, initialized, positioned);
    }

    private static byte[] BuildEntryStub(IReadOnlyList<NativeCallSpec> calls, uint results, uint originalEsp)
    {
        var bytes = new List<byte>(512);
        var stackFailureBranches = new List<int>(calls.Count);
        void Imm(uint value) { for (int n = 0; n < 4; ++n) bytes.Add((byte)(value >> (n * 8))); }
        for (int c = 0; c < calls.Count; ++c)
        {
            NativeCallSpec call = calls[c];
            for (int i = call.Arguments.Length - 1; i >= 0; --i) { bytes.Add(0x68); Imm(call.Arguments[i]); }
            bytes.Add(0xB9); Imm(call.ThisPointer);       // mov ecx, this
            bytes.Add(0xB8); Imm(call.Function);          // mov eax, function
            bytes.Add(0xFF); bytes.Add(0xD0);             // call eax
            if (call.Cdecl && call.Arguments.Length != 0)
            { bytes.Add(0x83); bytes.Add(0xC4); bytes.Add(checked((byte)(call.Arguments.Length * 4))); }
            bytes.Add(0xA3); Imm(checked(results + (uint)(c * sizeof(uint))));
            bytes.Add(0x81); bytes.Add(0xFC); Imm(originalEsp); // cmp esp, original
            bytes.Add(0x0F); bytes.Add(0x85);             // jne completion: no later call may run
            stackFailureBranches.Add(bytes.Count);
            Imm(0);
        }
        int completion = bytes.Count;
        bytes.Add(0xCC);
        foreach (int displacement in stackFailureBranches)
        {
            uint relative = checked((uint)(completion - (displacement + sizeof(uint))));
            for (int n = 0; n < sizeof(uint); ++n) bytes[displacement + n] = (byte)(relative >> (n * 8));
        }
        // A failed stack guard reaches the same one-shot trap with the wrong
        // ESP. CompleteCore rejects it before reading results or restoring any
        // context, so the enclosing owned run terminates without later calls.
        return bytes.ToArray();
    }

    private static string Hex(uint value) => $"0x{value:X8}";
    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool VirtualFreeEx(IntPtr process, IntPtr address, UIntPtr size, uint freeType);
}
