using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;

namespace SmoNativeValidator.Core;

internal sealed record NativeDebuggerRunResult(
    NativeValidationStatus Status,
    string Summary,
    int? ExitCode,
    string? RedirectedAssetPath,
    NativeExceptionSnapshot? FatalException,
    NativeCrashPhase? CrashPhase,
    NativeCrashAttributionConfidence? CrashAttributionConfidence);

internal sealed class NativeDebuggerHarness
{
    private const uint ErrorSemTimeout = 121;
    private const uint ValidatorTerminationCode = 0xE000534D;
    private readonly NativeValidationRequest _request;
    private readonly ExecutableProfile _profile;
    private readonly string _redirectAssetPath;
    private readonly string _triggerGameAssetPath;
    private readonly string _launchWorkingDirectory;
    private readonly ValidationEventSink _events;
    private readonly Stopwatch _clock;
    private readonly CancellationToken _cancellationToken;
    private readonly NativeThreadLoadState _threadLoadState = new();
    private readonly NativeTargetEngineDiagnosticTracker _targetEngineDiagnostics = new();
    private IntPtr _process;
    private Win32Native.ProcessInformation _processInformation;
    private SoftwareBreakpointManager? _breakpoints;
    private NativeValidationStatus? _status;
    private string? _summary;
    private int? _exitCode;
    private string? _redirectedAssetPath;
    private NativeExceptionSnapshot? _fatalException;
    private NativeCrashAttribution? _fatalCrashAttribution;
    private TimeSpan _lastTargetProgress;
    private TimeSpan? _targetAcceptedAt;
    private bool _processExited;
    private bool _initialSystemBreakpointConsumed;
    private uint _imageBase;

    internal NativeDebuggerHarness(
        NativeValidationRequest request,
        ExecutableProfile profile,
        string redirectAssetPath,
        string triggerGameAssetPath,
        string launchWorkingDirectory,
        ValidationEventSink events,
        Stopwatch clock,
        CancellationToken cancellationToken)
    {
        _request = request;
        _profile = profile;
        _redirectAssetPath = redirectAssetPath;
        _triggerGameAssetPath = triggerGameAssetPath;
        _launchWorkingDirectory = launchWorkingDirectory;
        _events = events;
        _clock = clock;
        _cancellationToken = cancellationToken;
    }

    internal NativeDebuggerRunResult Run()
    {
        if (!OperatingSystem.IsWindows())
        {
            return new NativeDebuggerRunResult(
                NativeValidationStatus.LaunchFailed,
                "Native engine validation is available only on Windows.",
                null,
                null,
                null,
                null,
                null);
        }

        try
        {
            LaunchOwnedProcess();
            DebugLoop();
        }
        catch (OperationCanceledException)
        {
            SetOutcome(NativeValidationStatus.Cancelled, "Validation was cancelled.");
        }
        catch (Exception exception) when (
            exception is Win32Exception or IOException or InvalidDataException or ArgumentException)
        {
            if (_status is null)
            {
                SetOutcome(NativeValidationStatus.LaunchFailed,
                    $"Native debugger failed: {exception.Message}");
            }
            _events.Emit(
                NativeValidationEventKind.Diagnostic,
                NativeValidationStage.Runtime,
                NativeValidationSeverity.Error,
                exception.ToString());
        }
        finally
        {
            CleanupOwnedProcess();
        }

        return new NativeDebuggerRunResult(
            _status ?? NativeValidationStatus.LaunchFailed,
            _summary ?? "Native validation ended without a classified result.",
            _exitCode,
            _redirectedAssetPath,
            _fatalException,
            _fatalCrashAttribution?.Phase,
            _fatalCrashAttribution?.Confidence);
    }

    private void LaunchOwnedProcess()
    {
        string executable = Path.GetFullPath(_request.ExecutablePath);
        string workingDirectory = _launchWorkingDirectory;
        string commandLine = QuoteCommandLineArgument(executable);
        if (!string.IsNullOrWhiteSpace(_request.Arguments))
            commandLine += " " + _request.Arguments;

        Win32Native.StartupInfo startup = new()
        {
            Size = (uint)Marshal.SizeOf<Win32Native.StartupInfo>()
        };
        uint flags = Win32Native.DebugOnlyThisProcess |
            Win32Native.CreateNewProcessGroup |
            Win32Native.CreateDefaultErrorMode;
        if (!Win32Native.CreateProcessW(
                executable,
                new StringBuilder(commandLine),
                IntPtr.Zero,
                IntPtr.Zero,
                inheritHandles: false,
                flags,
                IntPtr.Zero,
                workingDirectory,
                ref startup,
                out _processInformation))
        {
            throw new Win32Exception(Marshal.GetLastWin32Error(), "CreateProcessW failed.");
        }

        _process = _processInformation.Process;
        if (!Win32Native.DebugSetProcessKillOnExit(killOnExit: true))
        {
            throw new Win32Exception(
                Marshal.GetLastWin32Error(),
                "DebugSetProcessKillOnExit failed.");
        }
        _events.Emit(
            NativeValidationEventKind.ProcessStarted,
            NativeValidationStage.LaunchingEngine,
            NativeValidationSeverity.Information,
            $"Started owned game process {_processInformation.ProcessId} under DEBUG_ONLY_THIS_PROCESS.",
            progressPercent: 25,
            data: new Dictionary<string, string?>
            {
                ["processId"] = _processInformation.ProcessId.ToString(),
                ["workingDirectory"] = workingDirectory,
                ["debugMode"] = "DEBUG_ONLY_THIS_PROCESS",
                ["secuRomCaveat"] = "Legacy SecuROM builds may reject launch-under-debug."
            });
    }

    private void DebugLoop()
    {
        while (_status is null)
        {
            CheckTerminalConditions();
            if (_status is not null)
                break;

            if (!Win32Native.WaitForDebugEvent(out Win32Native.DebugEvent debugEvent, 100))
            {
                int error = Marshal.GetLastWin32Error();
                if ((uint)error == ErrorSemTimeout)
                    continue;
                throw new Win32Exception(error, "WaitForDebugEvent failed.");
            }

            uint continueStatus = Win32Native.DbgContinue;
            try
            {
                continueStatus = HandleDebugEvent(debugEvent);
            }
            catch (InvalidDataException exception)
            {
                SetOutcome(NativeValidationStatus.InstrumentationUnavailable, exception.Message);
                _events.Emit(
                    NativeValidationEventKind.Diagnostic,
                    NativeValidationStage.Fingerprinting,
                    NativeValidationSeverity.Error,
                    exception.Message);
            }
            catch (Exception exception) when (exception is Win32Exception or IOException)
            {
                SetOutcome(NativeValidationStatus.LaunchFailed,
                    $"Debugger instrumentation failed: {exception.Message}");
                _events.Emit(
                    NativeValidationEventKind.Diagnostic,
                    NativeValidationStage.Runtime,
                    NativeValidationSeverity.Error,
                    exception.ToString());
            }
            finally
            {
                if (!Win32Native.ContinueDebugEvent(
                        debugEvent.ProcessId,
                        debugEvent.ThreadId,
                        continueStatus) &&
                    debugEvent.Code != Win32Native.DebugEventCode.ExitProcess)
                {
                    int error = Marshal.GetLastWin32Error();
                    if (_status is null)
                        throw new Win32Exception(error, "ContinueDebugEvent failed.");
                }
            }
        }
    }

    private uint HandleDebugEvent(Win32Native.DebugEvent debugEvent)
    {
        switch (debugEvent.Code)
        {
            case Win32Native.DebugEventCode.CreateProcess:
                if (debugEvent.Info.CreateProcess.File != IntPtr.Zero)
                    Win32Native.CloseHandle(debugEvent.Info.CreateProcess.File);
                _imageBase = checked((uint)debugEvent.Info.CreateProcess.BaseOfImage.ToInt64());
                InstallProfileBreakpoints();
                return Win32Native.DbgContinue;

            case Win32Native.DebugEventCode.CreateThread:
                return Win32Native.DbgContinue;

            case Win32Native.DebugEventCode.ExitThread:
                HandleThreadExit(debugEvent.ThreadId);
                return Win32Native.DbgContinue;

            case Win32Native.DebugEventCode.LoadDll:
                if (debugEvent.Info.LoadDll.File != IntPtr.Zero)
                    Win32Native.CloseHandle(debugEvent.Info.LoadDll.File);
                return Win32Native.DbgContinue;

            case Win32Native.DebugEventCode.Exception:
                return HandleExceptionEvent(debugEvent);

            case Win32Native.DebugEventCode.OutputDebugString:
                LogDebugString(debugEvent);
                return Win32Native.DbgContinue;

            case Win32Native.DebugEventCode.ExitProcess:
                _processExited = true;
                _exitCode = unchecked((int)debugEvent.Info.ExitProcess.ExitCode);
                _events.Emit(
                    NativeValidationEventKind.ProcessExited,
                    NativeValidationStage.Runtime,
                    NativeValidationSeverity.Warning,
                    $"Game process exited with code 0x{debugEvent.Info.ExitProcess.ExitCode:X8}.",
                    data: new Dictionary<string, string?>
                    {
                        ["exitCode"] = $"0x{debugEvent.Info.ExitProcess.ExitCode:X8}"
                    });
                ClassifyUnexpectedExit();
                return Win32Native.DbgContinue;

            default:
                return Win32Native.DbgContinue;
        }
    }

    private uint HandleExceptionEvent(Win32Native.DebugEvent debugEvent)
    {
        Win32Native.ExceptionDebugInfo info = debugEvent.Info.Exception;
        uint code = info.ExceptionRecord.ExceptionCode;
        bool firstChance = info.FirstChance != 0;

        if (firstChance && NativeDebugExceptionClassifier.IsSoftwareBreakpoint(code))
        {
            Win32Native.X86Context context = Win32Native.GetX86Context(debugEvent.ThreadId);
            uint candidate = context.Eip == 0 ? 0 : context.Eip - 1;
            if (_breakpoints is not null &&
                _breakpoints.TryHandleBreakpoint(debugEvent.ThreadId, candidate, ref context))
            {
                return Win32Native.DbgContinue;
            }

            if (!_initialSystemBreakpointConsumed)
            {
                _initialSystemBreakpointConsumed = true;
                _events.Emit(
                    NativeValidationEventKind.Diagnostic,
                    NativeValidationStage.LaunchingEngine,
                    NativeValidationSeverity.Trace,
                    $"Consumed the debugger's initial system breakpoint at 0x{context.Eip:X8}.",
                    address: context.Eip,
                    threadId: debugEvent.ThreadId);
                return Win32Native.DbgContinue;
            }

            // A later foreign INT3 can be part of the game's own exception flow.
            // Let the debuggee handle it instead of silently swallowing it.
            return Win32Native.DbgExceptionNotHandled;
        }

        if (firstChance && NativeDebugExceptionClassifier.IsSingleStep(code))
        {
            Win32Native.X86Context context = Win32Native.GetX86Context(debugEvent.ThreadId);
            if (_breakpoints is not null &&
                _breakpoints.TryHandleSingleStep(debugEvent.ThreadId, ref context))
            {
                return Win32Native.DbgContinue;
            }
        }

        if (firstChance && !_request.CollectFirstChanceExceptions)
            return Win32Native.DbgExceptionNotHandled;

        NativeExceptionSnapshot snapshot = CaptureException(debugEvent.ThreadId, code, firstChance);
        bool currentThreadTarget = _threadLoadState.IsCurrentTarget(debugEvent.ThreadId);
        NativeCrashAttribution? crashAttribution = firstChance
            ? null
            : NativeCrashAttributionClassifier.Classify(new NativeCrashContext(
                currentThreadTarget,
                _events.Tracker.TargetRedirected,
                _events.Tracker.TargetLoadEntered,
                _events.Tracker.TargetLoadReturned,
                _events.Tracker.TargetLoadAccepted,
                _targetAcceptedAt is not null));
        string crashPhase = crashAttribution?.EventPhase ?? "first-chance";
        string crashDescription = crashAttribution?.Description ??
            "First-chance exception; native handling has not finished.";
        if (!firstChance)
        {
            _fatalException = snapshot;
            _fatalCrashAttribution = crashAttribution;
            _events.Tracker.SetException(snapshot);
        }

        Dictionary<string, string?> exceptionData = new(StringComparer.OrdinalIgnoreCase)
        {
            ["code"] = $"0x{code:X8}",
            ["firstChance"] = firstChance.ToString().ToLowerInvariant(),
            ["lastCheckpoint"] = _events.Tracker.LastCheckpoint,
            ["lastTargetCheckpoint"] = _events.Tracker.LastTargetCheckpoint,
            ["mappedGamePath"] = _redirectedAssetPath,
            ["logicalGameAssetPath"] = _request.LogicalGameAssetPath,
            ["triggerGameAssetPath"] = _triggerGameAssetPath,
            ["crashPhase"] = crashPhase,
            ["crashAttributionConfidence"] = crashAttribution?.Confidence.ToString() ??
                NativeCrashAttributionConfidence.None.ToString(),
            ["modelAttributed"] = (crashAttribution?.ModelDirectlyAttributed ?? false)
                .ToString().ToLowerInvariant(),
            ["targetRedirected"] = _events.Tracker.TargetRedirected.ToString().ToLowerInvariant(),
            ["targetLoadEntered"] = _events.Tracker.TargetLoadEntered.ToString().ToLowerInvariant(),
            ["targetLoadReturned"] = _events.Tracker.TargetLoadReturned.ToString().ToLowerInvariant(),
            ["targetLoadAccepted"] = _events.Tracker.TargetLoadAccepted.ToString().ToLowerInvariant(),
            ["currentThreadTarget"] = currentThreadTarget.ToString().ToLowerInvariant(),
            ["stackWords"] = string.Join(' ', snapshot.StackWords.Select(value => $"0x{value:X8}"))
        };
        foreach ((string register, uint value) in snapshot.Registers)
            exceptionData[register.ToLowerInvariant()] = $"0x{value:X8}";

        _events.Emit(
            NativeValidationEventKind.Exception,
            NativeValidationStage.Runtime,
            firstChance ? NativeValidationSeverity.Warning : NativeValidationSeverity.Error,
            $"{(firstChance ? "First-chance" : "Second-chance")} exception " +
            $"0x{code:X8} at 0x{snapshot.InstructionPointer:X8}.",
            address: snapshot.InstructionPointer,
            threadId: debugEvent.ThreadId,
            checkpoint: _events.Tracker.LastTargetCheckpoint ?? _events.Tracker.LastCheckpoint,
            data: exceptionData);

        if (!firstChance)
        {
            SetOutcome(NativeValidationStatus.Crash,
                $"Game crashed with exception 0x{code:X8}. {crashDescription} " +
                $"Last target checkpoint: {_events.Tracker.LastTargetCheckpoint ?? "none"}.");
        }

        return Win32Native.DbgExceptionNotHandled;
    }

    private void HandleThreadExit(uint threadId)
    {
        NativeThreadLoadCleanup loadCleanup = _threadLoadState.RemoveThread(threadId);
        BreakpointThreadCleanup breakpointCleanup = _breakpoints?.RemoveThread(threadId) ?? default;
        if (loadCleanup.RemovedFrames == 0 &&
            !loadCleanup.RemovedPendingRedirect &&
            breakpointCleanup.RemovedReturnProbes == 0 &&
            !breakpointCleanup.RemovedPendingRearm)
        {
            return;
        }

        _events.Emit(
            NativeValidationEventKind.Diagnostic,
            NativeValidationStage.Runtime,
            NativeValidationSeverity.Trace,
            $"Cleared validator state for exiting thread {threadId}.",
            threadId: threadId,
            data: new Dictionary<string, string?>
            {
                ["removedResourceLoadFrames"] = loadCleanup.RemovedFrames.ToString(),
                ["removedPendingRedirect"] = loadCleanup.RemovedPendingRedirect.ToString().ToLowerInvariant(),
                ["removedReturnProbes"] = breakpointCleanup.RemovedReturnProbes.ToString(),
                ["removedPendingRearm"] = breakpointCleanup.RemovedPendingRearm.ToString().ToLowerInvariant()
            });
    }

    private void InstallProfileBreakpoints()
    {
        _breakpoints = new SoftwareBreakpointManager(_process);
        foreach (ExecutableCheckpoint checkpoint in _profile.Checkpoints)
        {
            if (checkpoint.BloomSpecific && !_request.IncludeBloomCheckpoints)
                continue;
            uint address = checked(_imageBase + checkpoint.RelativeVirtualAddress);
            try
            {
                _breakpoints.AddStatic(address, checkpoint,
                    (uint threadId, uint breakpointAddress, ref Win32Native.X86Context context) =>
                        HandleStaticCheckpoint(checkpoint, threadId, breakpointAddress, ref context));
            }
            catch (InvalidDataException exception) when (!checkpoint.Required)
            {
                _events.Emit(
                    NativeValidationEventKind.Diagnostic,
                    NativeValidationStage.Fingerprinting,
                    NativeValidationSeverity.Warning,
                    $"Skipped optional checkpoint {checkpoint.Id}: {exception.Message}",
                    address: address,
                    checkpoint: checkpoint.Id,
                    data: new Dictionary<string, string?>
                    {
                        ["required"] = "false",
                        ["runtimeVerification"] = "mismatch-skipped"
                    });
            }
        }

        _events.Emit(
            NativeValidationEventKind.Diagnostic,
            NativeValidationStage.LaunchingEngine,
            NativeValidationSeverity.Information,
            $"Installed {_breakpoints.Addresses.Count()} verified software breakpoints.",
            progressPercent: 30,
            data: new Dictionary<string, string?>
            {
                ["imageBase"] = $"0x{_imageBase:X8}",
                ["profile"] = _profile.Id
            });
    }

    private void HandleStaticCheckpoint(
        ExecutableCheckpoint checkpoint,
        uint threadId,
        uint address,
        ref Win32Native.X86Context context)
    {
        switch (checkpoint.Kind)
        {
            case ExecutableCheckpointKind.BuildAssetPath:
                HandleBuildAssetPathEnter(checkpoint, threadId, address, ref context);
                break;
            case ExecutableCheckpointKind.ResourceLoad:
                HandleResourceLoadEnter(checkpoint, threadId, address, ref context);
                break;
            default:
                bool targetContext = _threadLoadState.IsCurrentTarget(threadId);
                _events.Emit(
                    NativeValidationEventKind.CheckpointEnter,
                    checkpoint.Stage,
                    NativeValidationSeverity.Information,
                    DescribeCheckpoint(checkpoint),
                    progressPercent: ProgressFor(checkpoint.Stage),
                    address: address,
                    threadId: threadId,
                    checkpoint: checkpoint.Id,
                    data: new Dictionary<string, string?>
                    {
                        ["kind"] = checkpoint.Kind.ToString(),
                        ["targetContext"] = targetContext.ToString().ToLowerInvariant()
                    });
                if (targetContext)
                    _lastTargetProgress = _clock.Elapsed;
                break;
        }
    }

    private void HandleBuildAssetPathEnter(
        ExecutableCheckpoint checkpoint,
        uint threadId,
        uint address,
        ref Win32Native.X86Context context)
    {
        uint returnAddress = Win32Native.ReadUInt32(_process, context.Esp);
        uint outputAddress = Win32Native.ReadUInt32(_process, context.Esp + 4);
        uint capacity = Win32Native.ReadUInt32(_process, context.Esp + 8);
        uint fileNameAddress = Win32Native.ReadUInt32(_process, context.Esp + 12);
        string requestedFileName = SafeReadAnsi(fileNameAddress);
        bool targetCandidate = LogicalAssetMatcher.IsMatch(
            requestedFileName,
            _triggerGameAssetPath);
        _events.Emit(
            NativeValidationEventKind.CheckpointEnter,
            checkpoint.Stage,
            NativeValidationSeverity.Information,
            $"BuildAssetPath requested '{requestedFileName}'.",
            progressPercent: 35,
            address: address,
            threadId: threadId,
            checkpoint: checkpoint.Id,
            data: new Dictionary<string, string?>
            {
                ["fileName"] = requestedFileName,
                ["targetCandidate"] = targetCandidate.ToString().ToLowerInvariant(),
                ["outputAddress"] = $"0x{outputAddress:X8}",
                ["capacity"] = capacity.ToString(),
                ["returnAddress"] = $"0x{returnAddress:X8}"
            });

        if (returnAddress == 0 || outputAddress == 0 || capacity is 0 or > 32_768)
        {
            _events.Emit(
                NativeValidationEventKind.Diagnostic,
                NativeValidationStage.AssetPathBuild,
                NativeValidationSeverity.Warning,
                "BuildAssetPath arguments were not safe to instrument; this call was only observed.",
                address: address,
                threadId: threadId,
                checkpoint: checkpoint.Id);
            return;
        }

        _breakpoints!.AddReturn(returnAddress, threadId, "BuildAssetPath.return",
            (uint returnThread, uint returnBreakpoint, ref Win32Native.X86Context returnContext) =>
                HandleBuildAssetPathReturn(
                    returnThread,
                    returnBreakpoint,
                    outputAddress,
                    capacity,
                    requestedFileName,
                    ref returnContext));
    }

    private void HandleBuildAssetPathReturn(
        uint threadId,
        uint address,
        uint outputAddress,
        uint capacity,
        string requestedFileName,
        ref Win32Native.X86Context context)
    {
        string observedPath = SafeReadAnsi(outputAddress, checked((int)Math.Min(capacity, 4096)));
        bool matches = LogicalAssetMatcher.IsMatch(observedPath, _triggerGameAssetPath);
        _events.Emit(
            NativeValidationEventKind.PathObserved,
            NativeValidationStage.AssetPathBuild,
            NativeValidationSeverity.Information,
            $"BuildAssetPath returned '{observedPath}'.",
            progressPercent: matches ? 45 : 40,
            address: address,
            threadId: threadId,
            checkpoint: "CP02.return",
            data: new Dictionary<string, string?>
            {
                ["requestedFileName"] = requestedFileName,
                ["observedPath"] = observedPath,
                ["matchesLogicalTarget"] = matches.ToString().ToLowerInvariant(),
                ["eax"] = $"0x{context.Eax:X8}"
            });
        if (!matches)
            return;

        byte[] replacement = Encoding.ASCII.GetBytes(_redirectAssetPath + '\0');
        if (replacement.Length > capacity)
        {
            SetOutcome(NativeValidationStatus.PathError,
                $"Staged path requires {replacement.Length} bytes but BuildAssetPath capacity is {capacity}.");
            _events.Emit(
                NativeValidationEventKind.Diagnostic,
                NativeValidationStage.AssetPathBuild,
                NativeValidationSeverity.Error,
                _summary!,
                address: address,
                threadId: threadId,
                checkpoint: "CP02.return");
            return;
        }

        Win32Native.WriteMemory(_process, outputAddress, replacement, executable: false);
        context.Eax = 1;
        _redirectedAssetPath = _redirectAssetPath;
        _threadLoadState.MarkTargetPathRedirected(threadId);
        _lastTargetProgress = _clock.Elapsed;
        _events.Emit(
            NativeValidationEventKind.PathRedirected,
            NativeValidationStage.AssetPathBuild,
            NativeValidationSeverity.Success,
            $"Redirected the matching game request to '{_redirectAssetPath}'.",
            progressPercent: 50,
            address: address,
            threadId: threadId,
            checkpoint: "CP02.return",
            data: new Dictionary<string, string?>
            {
                ["logicalGameAssetPath"] = _request.LogicalGameAssetPath,
                ["triggerGameAssetPath"] = _triggerGameAssetPath,
                ["originalPath"] = observedPath,
                ["redirectedPath"] = _redirectAssetPath
            });
    }

    private void HandleResourceLoadEnter(
        ExecutableCheckpoint checkpoint,
        uint threadId,
        uint address,
        ref Win32Native.X86Context context)
    {
        uint returnAddress = Win32Native.ReadUInt32(_process, context.Esp);
        uint pathAddress = Win32Native.ReadUInt32(_process, context.Esp + 4);
        string observedArgument = SafeReadAnsi(pathAddress);
        bool argumentMatches = PathsEqual(observedArgument, _redirectAssetPath);
        bool followsRedirect = _threadLoadState.ConsumePendingTargetRedirect(threadId);
        // Only the verified first path argument is authoritative. The same-thread
        // sequence is retained as diagnostic evidence, never as grounds for PASS.
        bool target = argumentMatches;
        string path = target ? _redirectAssetPath : observedArgument;
        string correlation = argumentMatches
            ? "first-path-argument"
            : followsRedirect
                ? "same-thread-after-target-BuildAssetPath-unverified"
                : "none";
        _events.Emit(
            NativeValidationEventKind.CheckpointEnter,
            checkpoint.Stage,
            target ? NativeValidationSeverity.Success : NativeValidationSeverity.Information,
            $"ResourceLoad entered for '{path}'.",
            progressPercent: target ? 60 : 55,
            address: address,
            threadId: threadId,
            checkpoint: checkpoint.Id,
            data: new Dictionary<string, string?>
            {
                ["path"] = path,
                ["target"] = target.ToString().ToLowerInvariant(),
                ["observedFirstArgument"] = observedArgument,
                ["targetCorrelation"] = correlation,
                ["returnAddress"] = $"0x{returnAddress:X8}"
            });

        if (returnAddress == 0)
        {
            _events.Emit(
                NativeValidationEventKind.Diagnostic,
                NativeValidationStage.ResourceLoad,
                NativeValidationSeverity.Warning,
                "ResourceLoad has no safe return address; nested target correlation was not activated for this call.",
                address: address,
                threadId: threadId,
                checkpoint: checkpoint.Id);
            return;
        }

        ResourceLoadFrame frame = _threadLoadState.Push(threadId, target);
        if (target)
            _lastTargetProgress = _clock.Elapsed;
        _breakpoints!.AddReturn(returnAddress, threadId, "ResourceLoad.return",
            (uint returnThread, uint returnBreakpoint, ref Win32Native.X86Context returnContext) =>
                HandleResourceLoadReturn(
                    returnThread,
                    returnBreakpoint,
                    path,
                    frame,
                    ref returnContext));
    }

    private void HandleResourceLoadReturn(
        uint threadId,
        uint address,
        string path,
        ResourceLoadFrame frame,
        ref Win32Native.X86Context context)
    {
        ResourceLoadFramePopResult pop = _threadLoadState.Pop(threadId, frame.Id);
        if (!pop.Found)
        {
            _events.Emit(
                NativeValidationEventKind.Diagnostic,
                NativeValidationStage.ResourceLoad,
                NativeValidationSeverity.Warning,
                $"Ignored stale ResourceLoad return probe for frame {frame.Id}.",
                address: address,
                threadId: threadId,
                checkpoint: "CP03.return");
            return;
        }
        if (pop.AbandonedNestedFrames > 0)
        {
            _events.Emit(
                NativeValidationEventKind.Diagnostic,
                NativeValidationStage.ResourceLoad,
                NativeValidationSeverity.Warning,
                $"ResourceLoad frame {frame.Id} returned after unwinding {pop.AbandonedNestedFrames} nested frame(s).",
                address: address,
                threadId: threadId,
                checkpoint: "CP03.return");
        }

        bool target = frame.Target;
        bool accepted = context.Eax != 0;
        bool engineRejected = target && _targetEngineDiagnostics.HasDiagnostics;
        _events.Emit(
            NativeValidationEventKind.CheckpointReturn,
            NativeValidationStage.ResourceLoad,
            target && accepted && !engineRejected ? NativeValidationSeverity.Success :
                target ? NativeValidationSeverity.Error : NativeValidationSeverity.Information,
            $"ResourceLoad returned 0x{context.Eax:X8} for '{path}'." +
            (engineRejected
                ? " Target-scoped serializer diagnostics reject this result."
                : string.Empty),
            progressPercent: target ? 75 : 58,
            address: address,
            threadId: threadId,
            checkpoint: "CP03.return",
            data: new Dictionary<string, string?>
            {
                ["path"] = path,
                ["target"] = target.ToString().ToLowerInvariant(),
                ["accepted"] = accepted.ToString().ToLowerInvariant(),
                ["engineRejected"] = engineRejected.ToString().ToLowerInvariant(),
                ["eax"] = $"0x{context.Eax:X8}"
            });

        if (!target)
            return;
        _lastTargetProgress = _clock.Elapsed;
        if (engineRejected)
        {
            SetOutcome(
                NativeValidationStatus.EngineRejected,
                _targetEngineDiagnostics.DescribeRejection(accepted));
            return;
        }
        if (!accepted)
        {
            SetOutcome(NativeValidationStatus.EngineRejected,
                DescribeTargetRejection());
            return;
        }

        _targetAcceptedAt = _clock.Elapsed;
        _events.Emit(
            NativeValidationEventKind.Diagnostic,
            NativeValidationStage.Runtime,
            NativeValidationSeverity.Information,
            $"Target resource was accepted; observing a {_request.SurvivalWindow.TotalSeconds:0.###} s survival window.",
            progressPercent: 85,
            checkpoint: "CP03.return");
    }

    private void CheckTerminalConditions()
    {
        if (_cancellationToken.IsCancellationRequested)
        {
            _events.Emit(
                NativeValidationEventKind.Cancelled,
                NativeValidationStage.Runtime,
                NativeValidationSeverity.Warning,
                "Cancellation requested.");
            SetOutcome(NativeValidationStatus.Cancelled, "Validation was cancelled.");
            return;
        }

        if (_clock.Elapsed >= _request.OverallTimeout)
        {
            _events.Emit(
                NativeValidationEventKind.Timeout,
                NativeValidationStage.Runtime,
                NativeValidationSeverity.Error,
                $"Overall timeout of {_request.OverallTimeout.TotalSeconds:0.###} s elapsed.");
            if (_targetEngineDiagnostics.HasDiagnostics)
            {
                SetOutcome(
                    NativeValidationStatus.EngineRejected,
                    _targetEngineDiagnostics.DescribeRejection(returnedNonNull: false));
            }
            else
            {
                SetOutcome(NativeValidationStatus.Timeout,
                    "The game did not complete native validation before the overall timeout.");
            }
            return;
        }

        if (_events.Tracker.TargetRedirected &&
            _targetAcceptedAt is null &&
            _request.NoProgressTimeout > TimeSpan.Zero &&
            _clock.Elapsed - _lastTargetProgress >= _request.NoProgressTimeout)
        {
            _events.Emit(
                NativeValidationEventKind.Timeout,
                NativeValidationStage.ResourceLoad,
                NativeValidationSeverity.Error,
                $"No target progress for {_request.NoProgressTimeout.TotalSeconds:0.###} s after redirection.",
                checkpoint: _events.Tracker.LastCheckpoint);
            if (_targetEngineDiagnostics.HasDiagnostics)
            {
                SetOutcome(
                    NativeValidationStatus.EngineRejected,
                    _targetEngineDiagnostics.DescribeRejection(returnedNonNull: false));
            }
            else
            {
                SetOutcome(NativeValidationStatus.Timeout,
                    "The engine stopped making progress after the target path was redirected.");
            }
            return;
        }

        if (_targetAcceptedAt is TimeSpan acceptedAt &&
            _clock.Elapsed - acceptedAt >= _request.SurvivalWindow)
        {
            if (_targetEngineDiagnostics.HasDiagnostics)
            {
                SetOutcome(
                    NativeValidationStatus.EngineRejected,
                    _targetEngineDiagnostics.DescribeRejection(returnedNonNull: true));
                return;
            }
            if (_events.Tracker.TargetFfpsHeaderEntered &&
                _events.Tracker.TargetFfpsMagicAccepted &&
                _events.Tracker.TargetFfpsVersionAccepted)
            {
                SetOutcome(NativeValidationStatus.Passed,
                    _request.Route == NativeValidationRoute.FastGeneric
                        ? "Fast native loader/scene-construction smoke-test passed: the redirected SMO reached FFPS version acceptance, returned a non-null native resource and survived the observation window."
                        : "The redirected SMO reached FFPS version acceptance, returned a non-null native resource and survived the observation window.");
            }
            else
            {
                SetOutcome(NativeValidationStatus.Inconclusive,
                    "ResourceLoad returned a non-null value and the process survived, but target-scoped FFPS version acceptance was not observed; the result is not a native SMO pass.");
            }
        }
    }

    private void ClassifyUnexpectedExit()
    {
        if (_status is not null)
            return;
        if (_targetEngineDiagnostics.HasDiagnostics)
        {
            SetOutcome(
                NativeValidationStatus.EngineRejected,
                _targetEngineDiagnostics.DescribeRejection(
                    _events.Tracker.TargetLoadAccepted));
        }
        else if (!_events.Tracker.TargetRedirected)
        {
            SetOutcome(NativeValidationStatus.TargetNotRequested,
                _clock.Elapsed < TimeSpan.FromSeconds(5)
                    ? "The game exited before requesting the configured SMO slot; this early exit may be a SecuROM launch-under-debug rejection."
                    : "The game exited before requesting the configured logical SMO slot.");
        }
        else if (_events.Tracker.TargetLoadReturned && !_events.Tracker.TargetLoadAccepted)
        {
            SetOutcome(NativeValidationStatus.EngineRejected,
                DescribeTargetRejection());
        }
        else
        {
            SetOutcome(NativeValidationStatus.EngineRejected,
                "The game exited before the target completed its runtime survival window.");
        }
    }

    private NativeExceptionSnapshot CaptureException(uint threadId, uint code, bool firstChance)
    {
        Win32Native.X86Context context = Win32Native.GetX86Context(threadId);
        List<uint> stackWords = [];
        try
        {
            byte[] stack = Win32Native.ReadMemory(_process, context.Esp, 32 * sizeof(uint));
            for (int offset = 0; offset < stack.Length; offset += sizeof(uint))
                stackWords.Add(BitConverter.ToUInt32(stack, offset));
        }
        catch (Win32Exception)
        {
            // An invalid ESP is itself useful context; registers are still reported.
        }

        return new NativeExceptionSnapshot
        {
            Code = code,
            FirstChance = firstChance,
            ThreadId = threadId,
            InstructionPointer = context.Eip,
            Registers = new Dictionary<string, uint>(StringComparer.OrdinalIgnoreCase)
            {
                ["EIP"] = context.Eip,
                ["ESP"] = context.Esp,
                ["EBP"] = context.Ebp,
                ["EAX"] = context.Eax,
                ["EBX"] = context.Ebx,
                ["ECX"] = context.Ecx,
                ["EDX"] = context.Edx,
                ["ESI"] = context.Esi,
                ["EDI"] = context.Edi,
                ["EFLAGS"] = context.EFlags
            },
            StackWords = stackWords
        };
    }

    private void LogDebugString(Win32Native.DebugEvent debugEvent)
    {
        Win32Native.OutputDebugStringInfo info = debugEvent.Info.DebugString;
        if (info.DebugStringData == IntPtr.Zero || info.DebugStringLength == 0)
            return;
        try
        {
            int byteLength = info.Unicode != 0
                ? checked(info.DebugStringLength * 2)
                : info.DebugStringLength;
            byte[] bytes = Win32Native.ReadMemory(
                _process,
                checked((uint)info.DebugStringData.ToInt64()),
                byteLength);
            string value = info.Unicode != 0
                ? Encoding.Unicode.GetString(bytes)
                : Encoding.Latin1.GetString(bytes);
            value = value.TrimEnd('\0', '\r', '\n');
            if (value.Length == 0)
                return;
            bool targetContext = _threadLoadState.IsCurrentTarget(debugEvent.ThreadId);
            _events.Emit(
                NativeValidationEventKind.Diagnostic,
                NativeValidationStage.Runtime,
                NativeValidationSeverity.Trace,
                $"Game debug output: {value}",
                threadId: debugEvent.ThreadId,
                data: new Dictionary<string, string?>
                {
                    ["targetContext"] = targetContext.ToString().ToLowerInvariant()
                });
            IReadOnlyList<NativeTargetEngineDiagnostic> diagnostics =
                _targetEngineDiagnostics.Observe(
                    value,
                    targetContext,
                    _redirectAssetPath);
            foreach (NativeTargetEngineDiagnostic diagnostic in diagnostics)
            {
                _events.Emit(
                    NativeValidationEventKind.Diagnostic,
                    NativeValidationStage.ResourceLoad,
                    NativeValidationSeverity.Error,
                    $"Target serializer diagnostic: {diagnostic.Description}.",
                    threadId: debugEvent.ThreadId,
                    checkpoint: _events.Tracker.LastTargetCheckpoint,
                    data: new Dictionary<string, string?>
                    {
                        ["targetContext"] = "true",
                        ["engineDiagnosticCode"] = diagnostic.Code,
                        ["redirectedAssetPath"] = _redirectAssetPath
                    });
            }
        }
        catch (Exception exception) when (exception is Win32Exception or OverflowException)
        {
            _events.Emit(
                NativeValidationEventKind.Diagnostic,
                NativeValidationStage.Runtime,
                NativeValidationSeverity.Warning,
                $"Could not read game debug output: {exception.Message}",
                threadId: debugEvent.ThreadId);
        }
    }

    private string SafeReadAnsi(uint address, int maximumBytes = 4096)
    {
        try
        {
            return Win32Native.ReadAnsiString(_process, address, maximumBytes);
        }
        catch (Exception exception) when (exception is Win32Exception or OverflowException)
        {
            return $"<unreadable@0x{address:X8}>";
        }
    }

    private static bool PathsEqual(string left, string right)
    {
        try
        {
            return string.Equals(Path.GetFullPath(left), Path.GetFullPath(right),
                StringComparison.OrdinalIgnoreCase);
        }
        catch (Exception exception) when (exception is ArgumentException or NotSupportedException)
        {
            return string.Equals(LogicalAssetMatcher.Normalize(left), LogicalAssetMatcher.Normalize(right),
                StringComparison.OrdinalIgnoreCase);
        }
    }

    private void SetOutcome(NativeValidationStatus status, string summary)
    {
        _status ??= status;
        _summary ??= summary;
    }

    private void CleanupOwnedProcess()
    {
        if (_process != IntPtr.Zero && !_processExited)
        {
            bool processMayBeRunning = true;
            if (Win32Native.GetExitCodeProcess(_process, out uint exitCode))
            {
                processMayBeRunning = exitCode == Win32Native.StillActive;
            }
            else
            {
                _events.Emit(
                    NativeValidationEventKind.Cleanup,
                    NativeValidationStage.Cleanup,
                    NativeValidationSeverity.Warning,
                    $"GetExitCodeProcess failed with Win32 error {Marshal.GetLastWin32Error()}; termination will still be attempted.");
            }

            bool terminationRequested = false;
            if (processMayBeRunning)
            {
                try
                {
                    if (!Win32Native.TerminateProcess(_process, ValidatorTerminationCode))
                    {
                        throw new Win32Exception(
                            Marshal.GetLastWin32Error(),
                            "TerminateProcess failed for the owned validation process.");
                    }
                    terminationRequested = true;
                    _events.Emit(
                        NativeValidationEventKind.Cleanup,
                        NativeValidationStage.Cleanup,
                        NativeValidationSeverity.Information,
                        "Terminated the owned validation process; no external game process was touched.");
                }
                catch (Exception exception) when (exception is Win32Exception or ObjectDisposedException)
                {
                    _events.Emit(
                        NativeValidationEventKind.Cleanup,
                        NativeValidationStage.Cleanup,
                        NativeValidationSeverity.Error,
                        $"Owned-process termination failed: {exception.Message}");
                }
            }

            // If termination failed, remove our INT3 bytes before releasing a live process.
            if (processMayBeRunning && !terminationRequested)
                DisposeBreakpoints();

            bool debugConnectionReleased = TryDetachDebuggee();
            if (!debugConnectionReleased && (terminationRequested || !processMayBeRunning))
                debugConnectionReleased = DrainExitDebugEvents(TimeSpan.FromSeconds(2));
            if (!debugConnectionReleased)
                debugConnectionReleased = TryDetachDebuggee();
            if (!debugConnectionReleased)
            {
                _events.Emit(
                    NativeValidationEventKind.Cleanup,
                    NativeValidationStage.Cleanup,
                    NativeValidationSeverity.Error,
                    "The owned process debug connection could not be released on the validator thread.");
            }

            if (terminationRequested)
                WaitForOwnedProcessTermination();
            else if (processMayBeRunning)
            {
                _events.Emit(
                    NativeValidationEventKind.Cleanup,
                    NativeValidationStage.Cleanup,
                    NativeValidationSeverity.Error,
                    "The owned game process may still be running because native termination failed.");
            }
        }

        DisposeBreakpoints();

        if (_processInformation.Thread != IntPtr.Zero)
            Win32Native.CloseHandle(_processInformation.Thread);
        if (_processInformation.Process != IntPtr.Zero)
            Win32Native.CloseHandle(_processInformation.Process);
        _process = IntPtr.Zero;
    }

    private bool TryDetachDebuggee()
    {
        if (_processInformation.ProcessId == 0)
            return true;
        if (Win32Native.DebugActiveProcessStop(_processInformation.ProcessId))
            return true;

        int error = Marshal.GetLastWin32Error();
        uint signaled = _process == IntPtr.Zero
            ? Win32Native.WaitFailed
            : Win32Native.WaitForSingleObject(_process, 0);
        if (error == Win32Native.ErrorInvalidParameter &&
            signaled == Win32Native.WaitObject0) // already exited/detached
        {
            _processExited = true;
            return true;
        }

        _events.Emit(
            NativeValidationEventKind.Cleanup,
            NativeValidationStage.Cleanup,
            NativeValidationSeverity.Warning,
            $"Debugger detach reported Win32 error {error}.");
        return false;
    }

    private bool DrainExitDebugEvents(TimeSpan timeout)
    {
        Stopwatch drainClock = Stopwatch.StartNew();
        while (drainClock.Elapsed < timeout)
        {
            if (!Win32Native.WaitForDebugEvent(out Win32Native.DebugEvent debugEvent, 100))
            {
                int error = Marshal.GetLastWin32Error();
                if ((uint)error == ErrorSemTimeout)
                    continue;
                _events.Emit(
                    NativeValidationEventKind.Cleanup,
                    NativeValidationStage.Cleanup,
                    NativeValidationSeverity.Warning,
                    $"Draining the debug connection failed with Win32 error {error}.");
                return false;
            }

            if (debugEvent.Code == Win32Native.DebugEventCode.CreateProcess &&
                debugEvent.Info.CreateProcess.File != IntPtr.Zero)
            {
                Win32Native.CloseHandle(debugEvent.Info.CreateProcess.File);
            }
            else if (debugEvent.Code == Win32Native.DebugEventCode.LoadDll &&
                debugEvent.Info.LoadDll.File != IntPtr.Zero)
            {
                Win32Native.CloseHandle(debugEvent.Info.LoadDll.File);
            }

            bool exited = debugEvent.Code == Win32Native.DebugEventCode.ExitProcess;
            if (!Win32Native.ContinueDebugEvent(
                    debugEvent.ProcessId,
                    debugEvent.ThreadId,
                    Win32Native.DbgContinue))
            {
                _events.Emit(
                    NativeValidationEventKind.Cleanup,
                    NativeValidationStage.Cleanup,
                    NativeValidationSeverity.Warning,
                    $"ContinueDebugEvent during cleanup failed with Win32 error {Marshal.GetLastWin32Error()}.");
                return false;
            }

            if (exited)
            {
                _processExited = true;
                return true;
            }
        }

        return false;
    }

    private void WaitForOwnedProcessTermination()
    {
        uint wait = Win32Native.WaitForSingleObject(_process, 2_000);
        if (wait == Win32Native.WaitObject0)
        {
            _processExited = true;
            return;
        }

        _events.Emit(
            NativeValidationEventKind.Cleanup,
            NativeValidationStage.Cleanup,
            NativeValidationSeverity.Warning,
            wait == Win32Native.WaitTimeout
                ? "The owned process did not signal termination within two seconds."
                : $"Waiting for owned-process termination failed with Win32 error {Marshal.GetLastWin32Error()}.");
    }

    private void DisposeBreakpoints()
    {
        _breakpoints?.Dispose();
        _breakpoints = null;
    }

    private static string DescribeCheckpoint(ExecutableCheckpoint checkpoint) => checkpoint.Kind switch
    {
        ExecutableCheckpointKind.FindMediaPath => "FindMediaPath entered.",
        ExecutableCheckpointKind.FfpsHeaderValidation => "FFPS header validation entered.",
        ExecutableCheckpointKind.FfpsMagicAccepted => "FFPS magic accepted; serializer version is being checked.",
        ExecutableCheckpointKind.FfpsVersionAccepted => "FFPS serializer version 0x26 accepted.",
        ExecutableCheckpointKind.BloomBodyLoad => "Bloom body load entered.",
        ExecutableCheckpointKind.BloomSnowSpecialCase => "Bloom snow/hair special-case checkpoint reached.",
        ExecutableCheckpointKind.BloomHairLoad => "Bloom hair resource load entered.",
        ExecutableCheckpointKind.BloomHairSetup => "Bloom hair setup entered.",
        ExecutableCheckpointKind.NodeAttachment => "Node parent/attachment candidate entered.",
        ExecutableCheckpointKind.BloomHairRuntimeUpdate => "Bloom hair runtime update entered.",
        _ => $"{checkpoint.Kind} entered."
    };

    private string DescribeTargetRejection()
    {
        NativeValidationTracker tracker = _events.Tracker;
        if (!tracker.TargetFfpsHeaderEntered)
            return "The native loader rejected the redirected SMO before target FFPS header validation was observed.";
        if (!tracker.TargetFfpsMagicAccepted)
            return "The redirected SMO entered FFPS validation but did not reach the accepted-magic checkpoint.";
        if (!tracker.TargetFfpsVersionAccepted)
            return "The redirected SMO passed FFPS magic but did not reach serializer-version 0x26 acceptance.";
        return "The redirected SMO passed FFPS magic/version checks, but high-level ResourceLoad returned null.";
    }

    private static double ProgressFor(NativeValidationStage stage) => stage switch
    {
        NativeValidationStage.MediaPathInitialization => 32,
        NativeValidationStage.AssetPathBuild => 40,
        NativeValidationStage.ResourceLoad => 60,
        NativeValidationStage.FfpsHeader => 65,
        NativeValidationStage.FfpsVersion => 70,
        NativeValidationStage.SceneInitialization => 80,
        NativeValidationStage.Runtime => 90,
        _ => 30
    };

    private static string QuoteCommandLineArgument(string value) =>
        "\"" + value.Replace("\"", "\\\"") + "\"";
}
