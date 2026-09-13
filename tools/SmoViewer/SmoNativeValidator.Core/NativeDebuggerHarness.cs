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
    private const uint RemoteMediaPathPreferredAddress = 0x50000000;
    private const uint FindMediaPathFinalCheckOffset = 0x18B;
    // Verified in every supported pristine PC executable. The executable is
    // non-ASLR, but keeping the value as an RVA makes the dependency explicit.
    private const uint GameFlowControllerSingletonRva = 0x00355294;
    private static readonly byte[] FindMediaPathFinalCheckBytes =
        [0x8B, 0x45, 0x14, 0x85, 0xC0, 0x5D, 0x75];
    private readonly NativeValidationRequest _request;
    private readonly ExecutableProfile _profile;
    private readonly string _redirectAssetPath;
    private readonly string _triggerGameAssetPath;
    private readonly string _launchWorkingDirectory;
    private readonly string _mediaRootPath;
    private readonly ValidationEventSink _events;
    private readonly Stopwatch _clock;
    private readonly CancellationToken _cancellationToken;
    private readonly NativeThreadLoadState _threadLoadState = new();
    private readonly NativeTargetEngineDiagnosticTracker _targetEngineDiagnostics = new();
    private readonly Dictionary<uint, PendingTransformBindingLookup> _pendingTransformBindings = [];
    private readonly Dictionary<uint, TrackedTransformEvaluator> _trackedTransformEvaluators = [];
    private readonly Dictionary<uint, int> _transformEvaluationSampleCounts = [];
    private readonly NativeSceneReadyTracker? _sceneReadyTracker;
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
    private TimeSpan? _sceneReadyAt;
    private NativeGameFlowSnapshot? _lastObservedGameFlowSnapshot;
    private bool _sceneReadyControllerUnavailableReported;
    private bool _processExited;
    private bool _initialSystemBreakpointConsumed;
    private uint _imageBase;
    private uint _remoteMediaPathAddress;

    internal NativeDebuggerHarness(
        NativeValidationRequest request,
        ExecutableProfile profile,
        string redirectAssetPath,
        string triggerGameAssetPath,
        string launchWorkingDirectory,
        string mediaRootPath,
        ValidationEventSink events,
        Stopwatch clock,
        CancellationToken cancellationToken)
    {
        _request = request;
        _profile = profile;
        _redirectAssetPath = redirectAssetPath;
        _triggerGameAssetPath = triggerGameAssetPath;
        _launchWorkingDirectory = launchWorkingDirectory;
        _mediaRootPath = Path.GetFullPath(mediaRootPath);
        _events = events;
        _clock = clock;
        _cancellationToken = cancellationToken;
        _sceneReadyTracker = request.RequireSceneReady
            ? new NativeSceneReadyTracker(request.StartLevel ?? 0)
            : null;
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
        if (_request.RequireSceneReady && !HasNativeSceneReadyCheckpoints())
        {
            return new NativeDebuggerRunResult(
                NativeValidationStatus.InstrumentationUnavailable,
                "The executable profile does not resolve all native game-flow checkpoints required for scene-ready validation.",
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
            if (checkpoint.StringComparisonSpecific &&
                string.IsNullOrWhiteSpace(_request.StringComparisonProbe))
            {
                continue;
            }
            // These optional signatures verify the controller layout used by
            // native state polling. The corresponding logging paths are
            // buffered/bypassed in normal gameplay and are not live probes.
            if (checkpoint.SceneReadySpecific)
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
            case ExecutableCheckpointKind.FindMediaPath:
                HandleFindMediaPathEnter(checkpoint, threadId, address, ref context);
                break;
            case ExecutableCheckpointKind.BuildAssetPath:
                HandleBuildAssetPathEnter(checkpoint, threadId, address, ref context);
                break;
            case ExecutableCheckpointKind.ResourceLoad:
                HandleResourceLoadEnter(checkpoint, threadId, address, ref context);
                break;
            case ExecutableCheckpointKind.TransformTrackBindingWrite:
                HandleTransformTrackBindingWrite(checkpoint, threadId, address, ref context);
                break;
            case ExecutableCheckpointKind.TransformTrackEvaluate:
                HandleTransformTrackEvaluate(checkpoint, threadId, address, ref context);
                break;
            case ExecutableCheckpointKind.StringComparisonCall:
                HandleStringComparisonCall(checkpoint, threadId, address, ref context);
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

    private bool HasNativeSceneReadyCheckpoints()
    {
        ExecutableCheckpointKind[] requiredKinds =
        [
            ExecutableCheckpointKind.GameFlowStateEntered,
            ExecutableCheckpointKind.GameFlowStateExiting,
            ExecutableCheckpointKind.GameFlowStateResumed
        ];
        return requiredKinds.All(kind =>
            _profile.Checkpoints.Any(checkpoint => checkpoint.Kind == kind));
    }

    private void HandleFindMediaPathEnter(
        ExecutableCheckpoint checkpoint,
        uint threadId,
        uint address,
        ref Win32Native.X86Context context)
    {
        uint managerAddress = context.Ecx;
        if (managerAddress == 0)
        {
            throw new InvalidDataException(
                "FindMediaPath was entered with a null resource-manager address.");
        }

        string mediaPath = Path.EndsInDirectorySeparator(_mediaRootPath)
            ? _mediaRootPath
            : _mediaRootPath + Path.DirectorySeparatorChar;
        if (mediaPath.Any(character => character > 0x7F))
        {
            throw new InvalidDataException(
                $"Selected Media root cannot be represented by the game's verified ASCII path contract: '{mediaPath}'.");
        }

        if (_remoteMediaPathAddress == 0)
        {
            byte[] encodedPath = Encoding.ASCII.GetBytes(mediaPath + '\0');
            _remoteMediaPathAddress = Win32Native.AllocateAndWriteMemory(
                _process,
                encodedPath,
                RemoteMediaPathPreferredAddress);
        }

        uint finalCheckAddress = checked(address + FindMediaPathFinalCheckOffset);
        byte[] finalCheckBytes = Win32Native.ReadMemory(
            _process,
            finalCheckAddress,
            FindMediaPathFinalCheckBytes.Length);
        if (!finalCheckBytes.SequenceEqual(FindMediaPathFinalCheckBytes))
        {
            throw new InvalidDataException(
                $"FindMediaPath final-check signature mismatch at 0x{finalCheckAddress:X8}: " +
                $"expected {Convert.ToHexString(FindMediaPathFinalCheckBytes)}, " +
                $"actual {Convert.ToHexString(finalCheckBytes)}.");
        }

        uint previousMediaPathAddress = Win32Native.ReadUInt32(_process, managerAddress + 0x14);
        uint languageId = Win32Native.ReadUInt32(_process, managerAddress + 0x18);
        uint languagePathAddress = Win32Native.ReadUInt32(_process, managerAddress + 0x1C);
        string languagePath = SafeReadAnsi(languagePathAddress, maximumBytes: 30);
        _breakpoints!.AddReturn(
            finalCheckAddress,
            threadId,
            "FindMediaPath.final-check",
            (uint finalThread, uint finalAddress, ref Win32Native.X86Context finalContext) =>
                HandleFindMediaPathFinalCheck(
                    checkpoint,
                    finalThread,
                    finalAddress,
                    managerAddress,
                    mediaPath,
                    ref finalContext));

        _events.Emit(
            NativeValidationEventKind.CheckpointEnter,
            checkpoint.Stage,
            NativeValidationSeverity.Information,
            $"Prepared a child-process-only MediaPath fallback for the selected root: '{mediaPath}'.",
            progressPercent: 32,
            address: address,
            threadId: threadId,
            checkpoint: checkpoint.Id,
            data: new Dictionary<string, string?>
            {
                ["kind"] = checkpoint.Kind.ToString(),
                ["managerAddress"] = $"0x{managerAddress:X8}",
                ["previousMediaPathAddress"] = $"0x{previousMediaPathAddress:X8}",
                ["mediaPathAddress"] = $"0x{_remoteMediaPathAddress:X8}",
                ["mediaPath"] = mediaPath,
                ["languageId"] = languageId.ToString(),
                ["languagePathAddress"] = $"0x{languagePathAddress:X8}",
                ["languagePath"] = languagePath,
                ["finalCheckAddress"] = $"0x{finalCheckAddress:X8}",
                ["registryLookupBypassed"] = "false"
            });
    }

    private void HandleFindMediaPathFinalCheck(
        ExecutableCheckpoint checkpoint,
        uint threadId,
        uint address,
        uint managerAddress,
        string mediaPath,
        ref Win32Native.X86Context context)
    {
        if (context.Ebp != managerAddress)
        {
            throw new InvalidDataException(
                $"FindMediaPath manager changed before its final check: " +
                $"expected 0x{managerAddress:X8}, EBP=0x{context.Ebp:X8}.");
        }

        uint nativeMediaPathAddress = Win32Native.ReadUInt32(_process, managerAddress + 0x14);
        bool fallbackApplied = nativeMediaPathAddress == 0;
        if (fallbackApplied)
        {
            Win32Native.WriteMemory(
                _process,
                managerAddress + 0x14,
                BitConverter.GetBytes(_remoteMediaPathAddress),
                executable: false);
        }

        uint selectedAddress = fallbackApplied
            ? _remoteMediaPathAddress
            : nativeMediaPathAddress;
        _events.Emit(
            NativeValidationEventKind.Diagnostic,
            checkpoint.Stage,
            NativeValidationSeverity.Success,
            fallbackApplied
                ? $"Applied the selected MediaPath fallback in child-process memory: '{mediaPath}'."
                : "The game supplied MediaPath natively; the validator fallback was not needed.",
            progressPercent: 32,
            address: address,
            threadId: threadId,
            checkpoint: $"{checkpoint.Id}.final-check",
            data: new Dictionary<string, string?>
            {
                ["managerAddress"] = $"0x{managerAddress:X8}",
                ["nativeMediaPathAddress"] = $"0x{nativeMediaPathAddress:X8}",
                ["selectedMediaPathAddress"] = $"0x{selectedAddress:X8}",
                ["mediaPath"] = fallbackApplied
                    ? mediaPath
                    : SafeReadAnsi(nativeMediaPathAddress),
                ["fallbackApplied"] = fallbackApplied.ToString().ToLowerInvariant(),
                ["postQuitBranchPrevented"] = fallbackApplied.ToString().ToLowerInvariant()
            });
    }

    private void HandleStringComparisonCall(
        ExecutableCheckpoint checkpoint,
        uint threadId,
        uint address,
        ref Win32Native.X86Context context)
    {
        string? probe = _request.StringComparisonProbe?.Trim();
        if (string.IsNullOrEmpty(probe))
            return;

        uint argumentOffset = checkpoint.StringComparisonMode switch
        {
            StringComparisonCallMode.DirectCdeclCall => 0,
            StringComparisonCallMode.CdeclImportThunk => sizeof(uint),
            StringComparisonCallMode.ObfuscatedThiscallTailCall => sizeof(uint),
            StringComparisonCallMode.DirectThiscallRangeCompare => 0,
            _ => throw new InvalidDataException(
                $"String comparison checkpoint {checkpoint.Id} has no call mode.")
        };
        string left;
        string right;
        uint leftAddress;
        uint rightAddress;
        if (checkpoint.StringComparisonMode is
            StringComparisonCallMode.ObfuscatedThiscallTailCall or
            StringComparisonCallMode.DirectThiscallRangeCompare)
        {
            leftAddress = context.Ecx;
            rightAddress = Win32Native.ReadUInt32(
                _process,
                checkpoint.StringComparisonMode == StringComparisonCallMode.DirectThiscallRangeCompare
                    ? checked(context.Esp + 2 * sizeof(uint))
                    : checked(context.Esp + argumentOffset));
            left = SafeReadMsvcBasicString(leftAddress, maximumBytes: 256);
            right = SafeReadAnsi(rightAddress, maximumBytes: 256);
        }
        else
        {
            // A direct CALL breakpoint runs before the CPU pushes its return address;
            // the shared import thunk runs after that push. Both ultimately use cdecl.
            leftAddress = Win32Native.ReadUInt32(
                _process,
                checked(context.Esp + argumentOffset));
            rightAddress = Win32Native.ReadUInt32(
                _process,
                checked(context.Esp + argumentOffset + sizeof(uint)));
            left = SafeReadAnsi(leftAddress, maximumBytes: 256);
            right = SafeReadAnsi(rightAddress, maximumBytes: 256);
        }
        bool match = left.Contains(probe, StringComparison.OrdinalIgnoreCase) ||
                     right.Contains(probe, StringComparison.OrdinalIgnoreCase);
        if (!match)
            return;

        ExecutableCheckpoint? bindingCheckpoint = _profile.Checkpoints.FirstOrDefault(candidate =>
            candidate.Kind == ExecutableCheckpointKind.TransformTrackBindingWrite);
        if (bindingCheckpoint is not null)
        {
            uint bindingAddress = checked(_imageBase + bindingCheckpoint.RelativeVirtualAddress);
            if (SafeStackContains(context.Esp, bindingAddress, 0x200))
            {
                bool exact = left.Equals(right, StringComparison.Ordinal);
                if (_pendingTransformBindings.TryGetValue(
                        threadId,
                        out PendingTransformBindingLookup? previous) &&
                    previous.Right.Equals(right, StringComparison.Ordinal))
                {
                    _pendingTransformBindings[threadId] = exact || !previous.Exact
                        ? new PendingTransformBindingLookup(
                            left,
                            right,
                            leftAddress,
                            rightAddress,
                            exact || previous.Exact,
                            _clock.Elapsed)
                        : previous;
                }
                else
                {
                    _pendingTransformBindings[threadId] = new PendingTransformBindingLookup(
                        left,
                        right,
                        leftAddress,
                        rightAddress,
                        exact,
                        _clock.Elapsed);
                }
            }
        }

        bool targetContext = _threadLoadState.IsCurrentTarget(threadId);
        string comparator = checkpoint.StringComparisonFunction switch
        {
            StringComparisonFunction.MsvcrStricmp => "MSVCR71!_stricmp",
            StringComparisonFunction.MsvcrStrcmpi => "MSVCR71!_strcmpi",
            StringComparisonFunction.Kernel32LstrcmpiA => "KERNEL32!lstrcmpiA",
            StringComparisonFunction.MsvcpBasicStringCstrCompare =>
                "MSVCP71!std::string::compare(const char*)",
            StringComparisonFunction.MsvcpBasicStringRangeCompare =>
                "MSVCP71!std::string::compare(pos,count,const char*,count)",
            StringComparisonFunction.MsvcpCharTraitsCompare =>
                "MSVCP71!char_traits<char>::compare",
            StringComparisonFunction.MsvcrStrncmp => "MSVCR71!strncmp",
            _ => "unknown string comparator"
        };
        uint? callerReturnAddress = checkpoint.StringComparisonMode is
            StringComparisonCallMode.CdeclImportThunk or
            StringComparisonCallMode.ObfuscatedThiscallTailCall
            ? Win32Native.ReadUInt32(_process, context.Esp)
            : null;
        var comparisonData = new Dictionary<string, string?>
        {
            ["kind"] = checkpoint.Kind.ToString(),
            ["comparator"] = comparator,
            ["left"] = left,
            ["right"] = right,
            ["probe"] = probe,
            ["callMode"] = checkpoint.StringComparisonMode.ToString(),
            ["callerReturnAddress"] = callerReturnAddress is uint caller
                ? $"0x{caller:X8}"
                : null,
            ["targetContext"] = targetContext.ToString().ToLowerInvariant()
        };
        if (checkpoint.StringComparisonFunction ==
                StringComparisonFunction.MsvcpCharTraitsCompare &&
            callerReturnAddress.HasValue)
        {
            // The verified PC lower_bound caller keeps the current tree node in
            // EDI and its lookup-key object in EBP. Raw register/value fields
            // let research runs distinguish duplicate-key selection without
            // baking that build-specific container layout into normal verdicts.
            comparisonData["registerEdi"] = $"0x{context.Edi:X8}";
            comparisonData["registerEbp"] = $"0x{context.Ebp:X8}";
            comparisonData["ediPlus28"] = SafeReadUInt32Hex(context.Edi, 0x28);
            comparisonData["ediPlus2C"] = SafeReadUInt32Hex(context.Edi, 0x2C);
            comparisonData["ebpPlus1C"] = SafeReadUInt32Hex(context.Ebp, 0x1C);
            comparisonData["ebpPlus20"] = SafeReadUInt32Hex(context.Ebp, 0x20);
            if (left.Equals(right, StringComparison.Ordinal))
            {
                comparisonData["stack20"] = SafeReadUInt32Hex(context.Esp, 0x20);
                comparisonData["stack24"] = SafeReadUInt32Hex(context.Esp, 0x24);
                comparisonData["stack28"] = SafeReadUInt32Hex(context.Esp, 0x28);
                comparisonData["stack2C"] = SafeReadUInt32Hex(context.Esp, 0x2C);
                comparisonData["stack30"] = SafeReadUInt32Hex(context.Esp, 0x30);
                comparisonData["stack34"] = SafeReadUInt32Hex(context.Esp, 0x34);
                comparisonData["stack38"] = SafeReadUInt32Hex(context.Esp, 0x38);
                comparisonData["stack3C"] = SafeReadUInt32Hex(context.Esp, 0x3C);
                comparisonData["stack40"] = SafeReadUInt32Hex(context.Esp, 0x40);
                comparisonData["stack44"] = SafeReadUInt32Hex(context.Esp, 0x44);
                comparisonData["stack48"] = SafeReadUInt32Hex(context.Esp, 0x48);
                comparisonData["stack4C"] = SafeReadUInt32Hex(context.Esp, 0x4C);
                comparisonData["stack50"] = SafeReadUInt32Hex(context.Esp, 0x50);
                comparisonData["stack54"] = SafeReadUInt32Hex(context.Esp, 0x54);
                comparisonData["stack58"] = SafeReadUInt32Hex(context.Esp, 0x58);
                comparisonData["stack5C"] = SafeReadUInt32Hex(context.Esp, 0x5C);
                comparisonData["stack60"] = SafeReadUInt32Hex(context.Esp, 0x60);
                comparisonData["stack64"] = SafeReadUInt32Hex(context.Esp, 0x64);
                comparisonData["stack68"] = SafeReadUInt32Hex(context.Esp, 0x68);
                comparisonData["stack6C"] = SafeReadUInt32Hex(context.Esp, 0x6C);
                comparisonData["stack70"] = SafeReadUInt32Hex(context.Esp, 0x70);
                comparisonData["stack74"] = SafeReadUInt32Hex(context.Esp, 0x74);
                comparisonData["stack78"] = SafeReadUInt32Hex(context.Esp, 0x78);
                comparisonData["stack7C"] = SafeReadUInt32Hex(context.Esp, 0x7C);
                comparisonData["stack80"] = SafeReadUInt32Hex(context.Esp, 0x80);
                comparisonData["mainImageStackCandidates"] =
                    SafeReadMainImageStackCandidates(context.Esp, 0x200);
            }
        }
        _events.Emit(
            NativeValidationEventKind.CheckpointEnter,
            NativeValidationStage.Runtime,
            NativeValidationSeverity.Success,
            $"{comparator} compared '{left}' with '{right}'.",
            address: address,
            threadId: threadId,
            checkpoint: checkpoint.Id,
            data: comparisonData);
    }

    private void HandleTransformTrackBindingWrite(
        ExecutableCheckpoint checkpoint,
        uint threadId,
        uint address,
        ref Win32Native.X86Context context)
    {
        if (!_pendingTransformBindings.Remove(threadId, out PendingTransformBindingLookup? lookup))
            return;
        string? probe = _request.StringComparisonProbe?.Trim();
        if (string.IsNullOrEmpty(probe) ||
            !lookup.Right.Contains(probe, StringComparison.OrdinalIgnoreCase))
        {
            return;
        }

        var data = new Dictionary<string, string?>
        {
            ["kind"] = checkpoint.Kind.ToString(),
            ["lookupLeft"] = lookup.Left,
            ["lookupRight"] = lookup.Right,
            ["lookupLeftAddress"] = $"0x{lookup.LeftAddress:X8}",
            ["lookupRightAddress"] = $"0x{lookup.RightAddress:X8}",
            ["lookupExact"] = lookup.Exact.ToString().ToLowerInvariant(),
            ["lookupAgeMilliseconds"] =
                (_clock.Elapsed - lookup.ObservedAt).TotalMilliseconds.ToString(
                    "F3", System.Globalization.CultureInfo.InvariantCulture),
            ["trackEvaluator"] = $"0x{context.Edi:X8}",
            ["bindingResult"] = $"0x{context.Eax:X8}",
            ["bindingManager"] = $"0x{context.Ebx:X8}",
            ["bindingWrapper"] = $"0x{context.Esi:X8}",
            ["previousEvaluatorBinding"] = SafeReadUInt32Hex(context.Edi, 0x10),
            ["trackEvaluatorWords"] = SafeReadUInt32Words(context.Edi, 0x60),
            ["bindingWrapperWords"] = SafeReadUInt32Words(context.Esi, 0x60),
            ["bindingResultWords"] = SafeReadUInt32Words(context.Eax, 0x40)
        };
        _trackedTransformEvaluators[context.Edi] = new TrackedTransformEvaluator(
            lookup.Right,
            context.Eax);
        _events.Emit(
            NativeValidationEventKind.CheckpointEnter,
            NativeValidationStage.Runtime,
            NativeValidationSeverity.Success,
            $"spTransformTrackEval binding for lookup '{lookup.Right}' returned 0x{context.Eax:X8}.",
            address: address,
            threadId: threadId,
            checkpoint: checkpoint.Id,
            data: data);
    }

    private void HandleTransformTrackEvaluate(
        ExecutableCheckpoint checkpoint,
        uint threadId,
        uint address,
        ref Win32Native.X86Context context)
    {
        uint evaluator = context.Ecx;
        if (!_trackedTransformEvaluators.TryGetValue(
                evaluator,
                out TrackedTransformEvaluator? tracked))
        {
            return;
        }
        int sampleIndex = _transformEvaluationSampleCounts.GetValueOrDefault(evaluator);
        if (sampleIndex >= 3)
            return;
        _transformEvaluationSampleCounts[evaluator] = sampleIndex + 1;

        uint returnAddress = Win32Native.ReadUInt32(_process, context.Esp);
        uint timeBits = Win32Native.ReadUInt32(_process, checked(context.Esp + 0x04));
        uint positionAddress = Win32Native.ReadUInt32(_process, checked(context.Esp + 0x08));
        uint rotationAddress = Win32Native.ReadUInt32(_process, checked(context.Esp + 0x0C));
        uint scaleAddress = Win32Native.ReadUInt32(_process, checked(context.Esp + 0x10));
        uint positionFlagAddress = Win32Native.ReadUInt32(_process, checked(context.Esp + 0x14));
        uint rotationFlagAddress = Win32Native.ReadUInt32(_process, checked(context.Esp + 0x18));
        uint scaleFlagAddress = Win32Native.ReadUInt32(_process, checked(context.Esp + 0x1C));
        string positionBefore = SafeReadFloatTuple(positionAddress, 3);
        string rotationBefore = SafeReadFloatTuple(rotationAddress, 4);
        string scaleBefore = SafeReadFloatTuple(scaleAddress, 3);

        _breakpoints!.AddReturn(
            returnAddress,
            threadId,
            $"{checkpoint.Id}.return",
            (uint returnThread, uint returnBreakpoint, ref Win32Native.X86Context returnContext) =>
            {
                _events.Emit(
                    NativeValidationEventKind.CheckpointReturn,
                    NativeValidationStage.Runtime,
                    NativeValidationSeverity.Success,
                    $"spTransformTrackEval evaluated '{tracked.LookupName}' sample {sampleIndex + 1}.",
                    address: returnBreakpoint,
                    threadId: returnThread,
                    checkpoint: $"{checkpoint.Id}.return",
                    data: new Dictionary<string, string?>
                    {
                        ["lookupName"] = tracked.LookupName,
                        ["trackEvaluator"] = $"0x{evaluator:X8}",
                        ["bindingSlot"] = $"0x{tracked.BindingSlot:X8}",
                        ["sampleIndex"] = (sampleIndex + 1).ToString(),
                        ["timeBits"] = $"0x{timeBits:X8}",
                        ["time"] = BitConverter.Int32BitsToSingle(unchecked((int)timeBits)).ToString(
                            "R", System.Globalization.CultureInfo.InvariantCulture),
                        ["trackCount"] = SafeReadUInt32Hex(evaluator, 0x14),
                        ["positionBefore"] = positionBefore,
                        ["positionAfter"] = SafeReadFloatTuple(positionAddress, 3),
                        ["rotationBefore"] = rotationBefore,
                        ["rotationAfter"] = SafeReadFloatTuple(rotationAddress, 4),
                        ["scaleBefore"] = scaleBefore,
                        ["scaleAfter"] = SafeReadFloatTuple(scaleAddress, 3),
                        ["positionFlag"] = SafeReadByteHex(positionFlagAddress),
                        ["rotationFlag"] = SafeReadByteHex(rotationFlagAddress),
                        ["scaleFlag"] = SafeReadByteHex(scaleFlagAddress),
                        ["returnEax"] = $"0x{returnContext.Eax:X8}"
                    });
            });
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
        {
            if (!NativeMediaPathMapper.TryMap(
                    observedPath,
                    requestedFileName,
                    _mediaRootPath,
                    out string mappedPath))
            {
                SetOutcome(
                    NativeValidationStatus.PathError,
                    $"BuildAssetPath result '{observedPath}' could not be rebound to the selected Media root '{_mediaRootPath}'.");
                _events.Emit(
                    NativeValidationEventKind.Diagnostic,
                    NativeValidationStage.MediaPathInitialization,
                    NativeValidationSeverity.Error,
                    $"Could not remap BuildAssetPath result '{observedPath}' to the selected Media root.",
                    address: address,
                    threadId: threadId,
                    checkpoint: "CP02.media-root");
                return;
            }

            byte[] mediaReplacement = Encoding.ASCII.GetBytes(mappedPath + '\0');
            if (mediaReplacement.Length > capacity)
            {
                SetOutcome(
                    NativeValidationStatus.PathError,
                    $"Selected Media path requires {mediaReplacement.Length} bytes but BuildAssetPath capacity is {capacity}.");
                return;
            }
            Win32Native.WriteMemory(_process, outputAddress, mediaReplacement, executable: false);
            context.Eax = 1;
            _events.Emit(
                NativeValidationEventKind.Diagnostic,
                NativeValidationStage.MediaPathInitialization,
                NativeValidationSeverity.Success,
                $"Remapped the game asset path to the selected Media root: '{mappedPath}'.",
                address: address,
                threadId: threadId,
                checkpoint: "CP02.media-root",
                data: new Dictionary<string, string?>
                {
                    ["requestedFileName"] = requestedFileName,
                    ["originalPath"] = observedPath,
                    ["mappedPath"] = mappedPath,
                    ["mediaRoot"] = _mediaRootPath
                });
            return;
        }

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
        ResourceLoadTargetRoute targetRoute = ResourceLoadTargetClassifier.Classify(
            observedArgument,
            _redirectAssetPath,
            _triggerGameAssetPath);
        bool argumentMatches = targetRoute == ResourceLoadTargetRoute.AlreadyRedirected;
        bool directlyRedirected = false;

        if (targetRoute == ResourceLoadTargetRoute.DirectLogicalPath)
        {
            byte[] replacement = Encoding.ASCII.GetBytes(_redirectAssetPath + '\0');
            int availableBytes = Encoding.Latin1.GetByteCount(observedArgument) + 1;
            if (replacement.Length > availableBytes)
            {
                SetOutcome(
                    NativeValidationStatus.PathError,
                    $"Staged path requires {replacement.Length} bytes but the direct ResourceLoad path buffer exposes only {availableBytes} bytes.");
                _events.Emit(
                    NativeValidationEventKind.Diagnostic,
                    NativeValidationStage.ResourceLoad,
                    NativeValidationSeverity.Error,
                    _summary!,
                    address: address,
                    threadId: threadId,
                    checkpoint: checkpoint.Id,
                    data: new Dictionary<string, string?>
                    {
                        ["originalPath"] = observedArgument,
                        ["redirectedPath"] = _redirectAssetPath
                    });
                return;
            }

            Win32Native.WriteMemory(_process, pathAddress, replacement, executable: false);
            directlyRedirected = true;
            _redirectedAssetPath = _redirectAssetPath;
            _lastTargetProgress = _clock.Elapsed;
            _events.Emit(
                NativeValidationEventKind.PathRedirected,
                NativeValidationStage.ResourceLoad,
                NativeValidationSeverity.Success,
                $"Redirected the direct ResourceLoad target to '{_redirectAssetPath}'.",
                progressPercent: 50,
                address: address,
                threadId: threadId,
                checkpoint: checkpoint.Id,
                data: new Dictionary<string, string?>
                {
                    ["logicalGameAssetPath"] = _request.LogicalGameAssetPath,
                    ["triggerGameAssetPath"] = _triggerGameAssetPath,
                    ["originalPath"] = observedArgument,
                    ["redirectedPath"] = _redirectAssetPath,
                    ["redirectRoute"] = "direct-resource-load"
                });
        }

        bool followsRedirect = _threadLoadState.ConsumePendingTargetRedirect(threadId);
        // Only the verified first path argument is authoritative. The same-thread
        // sequence is retained as diagnostic evidence, never as grounds for PASS.
        bool target = argumentMatches || directlyRedirected;
        string path = target ? _redirectAssetPath : observedArgument;
        string correlation = argumentMatches
            ? "first-path-argument"
            : directlyRedirected
                ? "direct-logical-path"
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
            _request.RequireSceneReady
                ? $"Target resource was accepted; waiting for native level state {_request.StartLevel} to become active with no pending transition."
                : $"Target resource was accepted; observing a {_request.SurvivalWindow.TotalSeconds:0.###} s survival window.",
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

        if (_request.RequireSceneReady &&
            _targetAcceptedAt is not null &&
            _sceneReadyAt is null)
        {
            PollNativeSceneReady();
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
                SetOutcome(
                    _request.RequireSceneReady && _targetAcceptedAt is not null &&
                    _sceneReadyAt is null
                        ? NativeValidationStatus.Inconclusive
                        : NativeValidationStatus.Timeout,
                    _request.RequireSceneReady && _targetAcceptedAt is not null &&
                    _sceneReadyAt is null
                        ? $"The redirected SMO loaded successfully, but native level state {_request.StartLevel} did not become active and transition-idle before the overall timeout."
                        : "The game did not complete native validation before the overall timeout.");
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

        if (_targetAcceptedAt is TimeSpan acceptedAt)
        {
            TimeSpan observationAnchor = acceptedAt;
            if (_request.RequireSceneReady)
            {
                if (_sceneReadyAt is not TimeSpan sceneReadyAt)
                    return;
                if (sceneReadyAt > observationAnchor)
                    observationAnchor = sceneReadyAt;
            }
            if (_clock.Elapsed - observationAnchor < _request.SurvivalWindow)
                return;

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
                        : _request.RequireSceneReady
                            ? $"The redirected SMO reached FFPS version acceptance, returned a non-null native resource, activated native level state {_request.StartLevel} with no pending transition and survived the scene-ready observation window."
                            : "The redirected SMO reached FFPS version acceptance, returned a non-null native resource and survived the observation window.");
            }
            else
            {
                SetOutcome(NativeValidationStatus.Inconclusive,
                    "ResourceLoad returned a non-null value and the process survived, but target-scoped FFPS version acceptance was not observed; the result is not a native SMO pass.");
            }
        }
    }

    private void PollNativeSceneReady()
    {
        try
        {
            uint singletonAddress = checked(_imageBase + GameFlowControllerSingletonRva);
            uint controllerAddress = Win32Native.ReadUInt32(_process, singletonAddress);
            if (controllerAddress == 0)
            {
                ReportSceneReadyControllerUnavailable(
                    singletonAddress,
                    "The game-flow controller singleton is null after target acceptance.");
                return;
            }

            uint currentStateId = Win32Native.ReadUInt32(
                _process, checked(controllerAddress + 0x1B0));
            uint pendingStateId = Win32Native.ReadUInt32(
                _process, checked(controllerAddress + 0x1B8));
            int stackIndex = unchecked((int)Win32Native.ReadUInt32(
                _process, checked(controllerAddress + 0x1AC)));
            uint activeStateAddress = 0;
            uint activeStateId = 0;
            if (stackIndex is >= 0 and <= 20)
            {
                activeStateAddress = Win32Native.ReadUInt32(
                    _process,
                    checked(controllerAddress + 0x15C + checked((uint)stackIndex * 4)));
                if (activeStateAddress != 0)
                {
                    activeStateId = Win32Native.ReadUInt32(
                        _process,
                        checked(activeStateAddress + 0x10));
                }
            }

            // Known game-flow IDs fit in one byte. Rejecting implausible data
            // prevents transient or invalid controller pointers from becoming evidence.
            if (currentStateId > byte.MaxValue || pendingStateId > byte.MaxValue ||
                activeStateId > byte.MaxValue || stackIndex is < -1 or > 20)
            {
                ReportSceneReadyControllerUnavailable(
                    singletonAddress,
                    "The game-flow controller returned an implausible state snapshot.",
                    controllerAddress,
                    currentStateId);
                return;
            }

            var snapshot = new NativeGameFlowSnapshot(
                currentStateId,
                pendingStateId,
                stackIndex,
                activeStateId);
            if (snapshot == _lastObservedGameFlowSnapshot)
                return;

            _lastObservedGameFlowSnapshot = snapshot;
            bool sceneReady = _sceneReadyTracker!.Observe(
                snapshot,
                _targetAcceptedAt is not null);
            _events.Emit(
                NativeValidationEventKind.Diagnostic,
                NativeValidationStage.Runtime,
                NativeValidationSeverity.Trace,
                $"Observed native game-flow state {currentStateId}; " +
                $"pending={pendingStateId}, stack={stackIndex}, active={activeStateId}.",
                checkpoint: "SCENE.poll",
                data: new Dictionary<string, string?>
                {
                    ["source"] = "native-controller-memory",
                    ["singletonAddress"] = $"0x{singletonAddress:X8}",
                    ["controllerAddress"] = $"0x{controllerAddress:X8}",
                    ["currentStateId"] = currentStateId.ToString(),
                    ["pendingStateId"] = pendingStateId.ToString(),
                    ["stackIndex"] = stackIndex.ToString(),
                    ["activeStateAddress"] = $"0x{activeStateAddress:X8}",
                    ["activeStateId"] = activeStateId.ToString(),
                    ["expectedStateId"] = _request.StartLevel?.ToString(),
                    ["targetStateObserved"] = _sceneReadyTracker.TargetStateObserved
                        .ToString().ToLowerInvariant(),
                    ["transitionQueueIdle"] = _sceneReadyTracker.TransitionQueueIdle
                        .ToString().ToLowerInvariant(),
                    ["activeStackStateMatched"] = _sceneReadyTracker.ActiveStackStateMatched
                        .ToString().ToLowerInvariant()
                });

            if (!sceneReady || _sceneReadyAt is not null)
                return;
            _sceneReadyAt = _clock.Elapsed;
            _events.Emit(
                NativeValidationEventKind.SceneReady,
                NativeValidationStage.Runtime,
                NativeValidationSeverity.Success,
                $"Native level state {_request.StartLevel} is active with no pending transition.",
                progressPercent: 92,
                address: checked(controllerAddress + 0x1B0),
                threadId: _processInformation.ThreadId,
                checkpoint: "SCENE01",
                data: new Dictionary<string, string?>
                {
                    ["source"] = "native-controller-memory",
                    ["controllerAddress"] = $"0x{controllerAddress:X8}",
                    ["expectedStateId"] = _request.StartLevel?.ToString(),
                    ["currentStateId"] = currentStateId.ToString(),
                    ["pendingStateId"] = pendingStateId.ToString(),
                    ["stackIndex"] = stackIndex.ToString(),
                    ["activeStateId"] = activeStateId.ToString()
                });
        }
        catch (Win32Exception)
        {
            // The controller can be replaced while a state transition is in
            // progress. A later 100 ms debugger-loop poll will retry safely.
        }
        catch (ArgumentException)
        {
            // Treat a transient invalid address exactly like an unreadable one.
        }
        catch (OverflowException)
        {
            // A corrupt/transient pointer is not scene-ready evidence.
        }
    }

    private void ReportSceneReadyControllerUnavailable(
        uint singletonAddress,
        string message,
        uint? controllerAddress = null,
        uint? stateId = null)
    {
        if (_sceneReadyControllerUnavailableReported)
            return;
        _sceneReadyControllerUnavailableReported = true;
        _events.Emit(
            NativeValidationEventKind.Diagnostic,
            NativeValidationStage.Runtime,
            NativeValidationSeverity.Warning,
            message,
            checkpoint: "SCENE.poll",
            data: new Dictionary<string, string?>
            {
                ["source"] = "native-controller-memory",
                ["singletonAddress"] = $"0x{singletonAddress:X8}",
                ["controllerAddress"] = controllerAddress is uint controller
                    ? $"0x{controller:X8}"
                    : null,
                ["stateId"] = stateId?.ToString()
            });
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

    private string SafeReadMsvcBasicString(uint address, int maximumBytes)
    {
        try
        {
            if (address == 0)
                return string.Empty;
            uint size = Win32Native.ReadUInt32(_process, checked(address + 0x10));
            uint capacity = Win32Native.ReadUInt32(_process, checked(address + 0x14));
            if (size > maximumBytes || size > capacity)
                return $"<invalid-msvc-string@0x{address:X8}:size={size},capacity={capacity}>";
            uint dataAddress = capacity < 0x10
                ? address
                : Win32Native.ReadUInt32(_process, address);
            byte[] bytes = Win32Native.ReadMemory(_process, dataAddress, checked((int)size));
            return Encoding.Latin1.GetString(bytes);
        }
        catch (Exception exception) when (
            exception is Win32Exception or OverflowException or ArgumentOutOfRangeException)
        {
            return $"<unreadable-msvc-string@0x{address:X8}>";
        }
    }

    private string? SafeReadUInt32Hex(uint address, uint offset)
    {
        if (address == 0)
            return null;
        try
        {
            uint value = Win32Native.ReadUInt32(_process, checked(address + offset));
            return $"0x{value:X8}";
        }
        catch (Exception exception) when (
            exception is Win32Exception or OverflowException or ArgumentOutOfRangeException)
        {
            return null;
        }
    }

    private string SafeReadMainImageStackCandidates(uint stackPointer, uint maximumOffset)
    {
        if (stackPointer == 0)
            return string.Empty;

        uint imageUpperBound = checked(_imageBase + 0x02000000);
        var candidates = new List<string>();
        for (uint offset = 0; offset <= maximumOffset; offset += sizeof(uint))
        {
            try
            {
                uint value = Win32Native.ReadUInt32(
                    _process,
                    checked(stackPointer + offset));
                if (value >= _imageBase && value < imageUpperBound)
                    candidates.Add($"+0x{offset:X}=0x{value:X8}");
            }
            catch (Exception exception) when (
                exception is Win32Exception or OverflowException or ArgumentOutOfRangeException)
            {
                break;
            }
        }
        return string.Join(' ', candidates);
    }

    private bool SafeStackContains(uint stackPointer, uint value, uint maximumOffset)
    {
        if (stackPointer == 0)
            return false;
        for (uint offset = 0; offset <= maximumOffset; offset += sizeof(uint))
        {
            try
            {
                if (Win32Native.ReadUInt32(_process, checked(stackPointer + offset)) == value)
                    return true;
            }
            catch (Exception exception) when (
                exception is Win32Exception or OverflowException or ArgumentOutOfRangeException)
            {
                return false;
            }
        }
        return false;
    }

    private string SafeReadUInt32Words(uint address, uint maximumOffset)
    {
        if (address == 0)
            return string.Empty;
        var words = new List<string>();
        for (uint offset = 0; offset <= maximumOffset; offset += sizeof(uint))
        {
            string? value = SafeReadUInt32Hex(address, offset);
            if (value is null)
                break;
            words.Add($"+0x{offset:X}={value}");
        }
        return string.Join(' ', words);
    }

    private string SafeReadFloatTuple(uint address, int count)
    {
        if (address == 0)
            return string.Empty;
        try
        {
            byte[] bytes = Win32Native.ReadMemory(_process, address, checked(count * sizeof(float)));
            return string.Join(",", Enumerable.Range(0, count).Select(index =>
                BitConverter.ToSingle(bytes, index * sizeof(float)).ToString(
                    "R", System.Globalization.CultureInfo.InvariantCulture)));
        }
        catch (Exception exception) when (
            exception is Win32Exception or OverflowException or ArgumentOutOfRangeException)
        {
            return $"<unreadable@0x{address:X8}>";
        }
    }

    private string SafeReadByteHex(uint address)
    {
        if (address == 0)
            return string.Empty;
        try
        {
            byte value = Win32Native.ReadMemory(_process, address, 1)[0];
            return $"0x{value:X2}";
        }
        catch (Exception exception) when (
            exception is Win32Exception or OverflowException or ArgumentOutOfRangeException)
        {
            return $"<unreadable@0x{address:X8}>";
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
        ExecutableCheckpointKind.TransformTrackBindingWrite =>
            "spTransformTrackEval binding result is about to be stored.",
        ExecutableCheckpointKind.TransformTrackEvaluate =>
            "spTransformTrackEval evaluation entered.",
        ExecutableCheckpointKind.StringComparisonCall => "String comparator entered.",
        _ => $"{checkpoint.Kind} entered."
    };

    private sealed record PendingTransformBindingLookup(
        string Left,
        string Right,
        uint LeftAddress,
        uint RightAddress,
        bool Exact,
        TimeSpan ObservedAt);

    private sealed record TrackedTransformEvaluator(
        string LookupName,
        uint BindingSlot);

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
