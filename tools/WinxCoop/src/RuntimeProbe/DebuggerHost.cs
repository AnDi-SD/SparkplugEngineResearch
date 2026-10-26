using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

internal sealed record DebuggerRunResult(
    uint ProcessPid, uint ExitCode, bool TimedOut, bool CleanupConfirmed,
    uint FatalExceptions, uint EventsHandled, string? Error,
    bool ExitCodeKnown, bool ForcedTermination);

/// <summary>
/// Own-child debugger transport only. No attach, MediaPath override, asset
/// redirection, actor policy or native function invocation belongs here.
/// Run and every debug API call stay on its synchronous calling thread.
/// </summary>
internal sealed class DebuggerHost
{
    private const int ErrorSemTimeout = 121;
    private const uint DuplicateSameAccess = 2;
    private readonly string _executable;
    private readonly string _workingDirectory;
    private readonly Action<object> _log;
    private Win32Native.ProcessInformation _process;
    private IntPtr _heldProcess;
    private SoftwareBreakpointManager? _breakpoints;
    private bool _used, _initialBreakpointConsumed, _initialNative64BreakpointConsumed, _exitEventContinued;
    private bool _processHandleAutoClosed, _threadHandleAutoClosed;
    private bool _stopRequested, _exitCodeKnown, _forcedTermination;
    private uint _exitCode = Win32Native.StillActive, _fatalExceptions, _events;
    private uint _foreignFirstChances;
    private string? _error;

    internal DebuggerHost(string executable, string workingDir, Action<object> log)
    {
        _executable = Path.GetFullPath(executable);
        _workingDirectory = Path.GetFullPath(workingDir);
        _log = log ?? throw new ArgumentNullException(nameof(log));
        if (!File.Exists(_executable)) throw new FileNotFoundException("Owned executable missing.", _executable);
        if (!Directory.Exists(_workingDirectory)) throw new DirectoryNotFoundException(_workingDirectory);
        if (_executable.Contains('"')) throw new ArgumentException("An executable path cannot contain a quote.", nameof(executable));
    }

    internal IntPtr ProcessHandle => _heldProcess;
    internal uint ImageBase { get; private set; }
    internal uint MainThreadId { get; private set; }
    internal uint ProcessPid => _process.ProcessId;
    internal bool IsAtDebugStop { get; private set; }
    internal SoftwareBreakpointManager Breakpoints => _breakpoints ??
        throw new InvalidOperationException("Breakpoints are available only inside the owned debug session.");

    internal DebuggerRunResult Run(TimeSpan duration, Action<DebuggerHost> onCreate,
        Action<DebuggerHost>? idle = null)
    {
        ArgumentNullException.ThrowIfNull(onCreate);
        if (duration <= TimeSpan.Zero) throw new ArgumentOutOfRangeException(nameof(duration));
        if (_used) throw new InvalidOperationException("A debugger host owns exactly one terminal run.");
        _used = true;
        bool timedOut = false, cleaned = false;
        var elapsed = Stopwatch.StartNew();
        try
        {
            Launch();
            while (!_stopRequested && !_exitEventContinued)
            {
                idle?.Invoke(this); // Bounded call deadlines also advance during a busy debug-event stream.
                if (elapsed.Elapsed >= duration) { timedOut = true; break; }
                if (!Win32Native.WaitForDebugEvent(out var debugEvent, 100))
                {
                    int error = Marshal.GetLastWin32Error();
                    if (error != ErrorSemTimeout) throw new Win32Exception(error, "WaitForDebugEvent failed.");
                    // This callback is not a game-thread execution scope. It may
                    // observe borrowed bytes; actor writes require a verified BP.
                    continue;
                }
                Dispatch(debugEvent, onCreate, cleanup: false);
            }
        }
        catch (Exception exception)
        {
            SetError(exception.ToString());
            Emit(new { @event = "debugger-error", error = exception.Message });
        }
        finally
        {
            cleaned = Cleanup();
        }
        return new DebuggerRunResult(ProcessPid, _exitCode, timedOut, cleaned,
            _fatalExceptions, _events, _error, _exitCodeKnown, _forcedTermination);
    }

    private void Launch()
    {
        var startup = new Win32Native.StartupInfo { Size = (uint)Marshal.SizeOf<Win32Native.StartupInfo>() };
        if (!Win32Native.CreateProcessW(_executable, new StringBuilder('"' + _executable + '"'),
                IntPtr.Zero, IntPtr.Zero, false,
                Win32Native.DebugOnlyThisProcess | Win32Native.CreateNewProcessGroup | Win32Native.CreateDefaultErrorMode,
                IntPtr.Zero, _workingDirectory, ref startup, out _process))
            throw new Win32Exception(Marshal.GetLastWin32Error(), "CreateProcessW failed.");
        MainThreadId = _process.ThreadId;
        if (!Win32Native.DebugSetProcessKillOnExit(true))
            throw new Win32Exception(Marshal.GetLastWin32Error(), "DebugSetProcessKillOnExit failed.");
        // Debug-event process/thread handles are closed by ContinueDebugEvent on
        // exit. Hold an independent duplicate for signal-based lifetime proof.
        if (!DuplicateHandle(GetCurrentProcess(), _process.Process, GetCurrentProcess(),
                out _heldProcess, 0, false, DuplicateSameAccess))
            throw new Win32Exception(Marshal.GetLastWin32Error(), "DuplicateHandle for owned process failed.");
        Emit(new { @event = "process-start", processPid = ProcessPid, mainThreadId = MainThreadId,
            executable = _executable, workingDirectory = _workingDirectory, debugOnlyThisProcess = true });
    }

    private void Dispatch(Win32Native.DebugEvent debugEvent, Action<DebuggerHost>? onCreate, bool cleanup)
    {
        ++_events;
        IsAtDebugStop = true;
        uint status = debugEvent.Code == Win32Native.DebugEventCode.Exception
            ? Win32Native.DbgExceptionNotHandled : Win32Native.DbgContinue;
        try
        {
            if (debugEvent.ProcessId != ProcessPid)
                throw new InvalidDataException("Unexpected process in DEBUG_ONLY_THIS_PROCESS session.");
            switch (debugEvent.Code)
            {
                case Win32Native.DebugEventCode.CreateProcess:
                    Close(debugEvent.Info.CreateProcess.File, "create-process-file");
                    _processHandleAutoClosed = debugEvent.Info.CreateProcess.Process == _process.Process;
                    _threadHandleAutoClosed = debugEvent.Info.CreateProcess.Thread == _process.Thread;
                    ImageBase = checked((uint)debugEvent.Info.CreateProcess.BaseOfImage.ToInt64());
                    if (!cleanup)
                    {
                        _breakpoints = new SoftwareBreakpointManager(ProcessHandle);
                        onCreate!(this); // AddStatic performs live-byte verification.
                    }
                    break;
                case Win32Native.DebugEventCode.LoadDll:
                    Close(debugEvent.Info.LoadDll.File, "load-dll-file");
                    break;
                case Win32Native.DebugEventCode.CreateThread:
                    // The event's thread handle is owned by the debug subsystem;
                    // ContinueDebugEvent closes it at EXIT_THREAD/EXIT_PROCESS.
                    break;
                case Win32Native.DebugEventCode.ExitThread:
                    _breakpoints?.RemoveThread(debugEvent.ThreadId);
                    break;
                case Win32Native.DebugEventCode.ExitProcess:
                    _exitCode = debugEvent.Info.ExitProcess.ExitCode;
                    _exitCodeKnown = true;
                    Emit(new { @event = "process-exit-event", processPid = ProcessPid, exitCode = _exitCode });
                    break;
                case Win32Native.DebugEventCode.Exception:
                    status = HandleException(debugEvent, cleanup);
                    break;
                case Win32Native.DebugEventCode.Rip:
                    throw new Win32Exception((int)debugEvent.Info.Rip.Error, "Debugger RIP event.");
            }
        }
        catch (Exception exception)
        {
            SetError(exception.ToString());
            Emit(new { @event = "debug-event-error", code = debugEvent.Code.ToString(), error = exception.Message });
        }
        finally
        {
            // Never strand the owned process at a debug event if a callback,
            // reader, logger or signature check throws.
            if (!Win32Native.ContinueDebugEvent(debugEvent.ProcessId, debugEvent.ThreadId, status))
                SetError(new Win32Exception(Marshal.GetLastWin32Error(), "ContinueDebugEvent failed.").ToString());
            else if (debugEvent.Code == Win32Native.DebugEventCode.ExitProcess)
                _exitEventContinued = true;
            IsAtDebugStop = false;
        }
    }

    private uint HandleException(Win32Native.DebugEvent debugEvent, bool cleanup)
    {
        var exception = debugEvent.Info.Exception;
        uint code = exception.ExceptionRecord.ExceptionCode;
        ulong wideAddress = unchecked((ulong)exception.ExceptionRecord.ExceptionAddress.ToInt64());
        bool first = exception.FirstChance != 0;
        // A WOW64 launch may stop in the native 64-bit loader before its x86
        // system breakpoint. Never narrow that address or request an x86 EIP
        // for this independent, one-time loader event.
        if (!cleanup && first && NativeDebugExceptionClassifier.IsSoftwareBreakpoint(code) &&
            wideAddress > uint.MaxValue && debugEvent.ThreadId == MainThreadId &&
            !_initialNative64BreakpointConsumed && !_initialBreakpointConsumed)
        {
            _initialNative64BreakpointConsumed = true;
            Emit(new { @event = "initial-native64-system-breakpoint", threadId = debugEvent.ThreadId,
                address = $"0x{wideAddress:X16}", exceptionCode = code });
            return Win32Native.DbgContinue;
        }
        uint address = wideAddress <= uint.MaxValue ? (uint)wideAddress : 0;
        if (!cleanup && first && wideAddress <= uint.MaxValue &&
            NativeDebugExceptionClassifier.IsSoftwareBreakpoint(code) && _breakpoints is not null)
        {
            var context = Win32Native.GetX86Context(debugEvent.ThreadId);
            uint candidate = _breakpoints.Contains(address) ? address : context.Eip > 0 ? context.Eip - 1 : 0;
            if (_breakpoints.TryHandleBreakpoint(debugEvent.ThreadId, candidate, ref context))
            {
                _initialBreakpointConsumed = true;
                return Win32Native.DbgContinue;
            }
            if (!_initialBreakpointConsumed && debugEvent.ThreadId == MainThreadId)
            {
                _initialBreakpointConsumed = true;
                Emit(new { @event = "initial-system-breakpoint", threadId = debugEvent.ThreadId, address });
                return Win32Native.DbgContinue;
            }
        }
        if (!cleanup && first && wideAddress <= uint.MaxValue &&
            NativeDebugExceptionClassifier.IsSingleStep(code) && _breakpoints is not null)
        {
            var context = Win32Native.GetX86Context(debugEvent.ThreadId);
            if (_breakpoints.TryHandleSingleStep(debugEvent.ThreadId, ref context))
                return Win32Native.DbgContinue;
        }
        if (!first)
        {
            ++_fatalExceptions;
            _stopRequested = true;
            Emit(new { @event = "fatal-exception", processPid = ProcessPid,
                threadId = debugEvent.ThreadId, exceptionCode = code, address = $"0x{wideAddress:X16}", cleanup });
        }
        else if (++_foreignFirstChances <= 32)
            Emit(new { @event = "foreign-first-chance", threadId = debugEvent.ThreadId,
                exceptionCode = code, address = $"0x{wideAddress:X16}", passedToApplication = true, cleanup });
        return Win32Native.DbgExceptionNotHandled;
    }

    private bool Cleanup()
    {
        var elapsed = Stopwatch.StartNew();
        try { _breakpoints?.Dispose(); }
        catch (Exception exception) { SetError("Breakpoint cleanup: " + exception); }
        _breakpoints = null;
        IntPtr process = _heldProcess != IntPtr.Zero ? _heldProcess : _process.Process;
        bool signaled = false;
        try
        {
            if (process != IntPtr.Zero)
            {
                if (Win32Native.WaitForSingleObject(process, 0) != Win32Native.WaitObject0)
                {
                    if (Win32Native.TerminateProcess(process, 124)) _forcedTermination = true;
                    else
                    {
                        int terminateError = Marshal.GetLastWin32Error();
                        if (Win32Native.WaitForSingleObject(process, 0) != Win32Native.WaitObject0)
                            SetError(new Win32Exception(terminateError, "Own-process termination failed.").ToString());
                    }
                }
                // Pump exit events before waiting: a debugged process may need
                // its EXIT_PROCESS event continued before it can fully signal.
                while (!_exitEventContinued && elapsed.ElapsedMilliseconds < 3000)
                {
                    if (Win32Native.WaitForDebugEvent(out var debugEvent, 50))
                        Dispatch(debugEvent, null, cleanup: true);
                    else if (Marshal.GetLastWin32Error() != ErrorSemTimeout)
                        break;
                }
                uint remaining = (uint)Math.Max(0, 3000 - elapsed.ElapsedMilliseconds);
                signaled = Win32Native.WaitForSingleObject(process, remaining) == Win32Native.WaitObject0;
                if (signaled && Win32Native.GetExitCodeProcess(process, out uint exitCode))
                { _exitCode = exitCode; _exitCodeKnown = true; }
                if (!signaled) SetError("Owned process did not signal within the three-second cleanup budget.");
            }
        }
        catch (Exception exception) { SetError("Process cleanup: " + exception); }
        finally
        {
            // PI handles may be different from their debug-event counterparts.
            // Only the latter are auto-closed by successful exit continuation.
            if (!_threadHandleAutoClosed) Close(_process.Thread, "primary-thread");
            if (!_processHandleAutoClosed) Close(_process.Process, "primary-process");
            Close(_heldProcess, "held-process");
            _process.Thread = IntPtr.Zero; _process.Process = IntPtr.Zero; _heldProcess = IntPtr.Zero;
        }
        Emit(new { @event = "cleanup", processPid = ProcessPid, processSignaled = signaled,
            exitEventContinued = _exitEventContinued, forcedTermination = _forcedTermination,
            cleanupMs = elapsed.ElapsedMilliseconds });
        return signaled;
    }

    private void Close(IntPtr handle, string kind)
    {
        if (handle != IntPtr.Zero && handle != new IntPtr(-1) && !Win32Native.CloseHandle(handle))
            SetError(new Win32Exception(Marshal.GetLastWin32Error(), "CloseHandle(" + kind + ") failed.").ToString());
    }
    private void SetError(string error)
    { _error ??= error; _stopRequested = true; }
    private void Emit(object value)
    {
        try { _log(value); }
        catch (Exception exception) { SetError("Diagnostic log failed: " + exception.Message); }
    }

    [DllImport("kernel32.dll")]
    private static extern IntPtr GetCurrentProcess();
    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool DuplicateHandle(IntPtr sourceProcess, IntPtr sourceHandle,
        IntPtr targetProcess, out IntPtr targetHandle, uint desiredAccess,
        [MarshalAs(UnmanagedType.Bool)] bool inheritHandle, uint options);
}
