using System.Reflection.PortableExecutable;
using System.Security.Cryptography;
using System.Text;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// Explicit process-local data-path substitution. Original RegOpenKeyExA and
/// RegQueryValueExA still execute and must actually succeed. Only the temporary
/// access argument is narrowed, then the successful MediaPath query's own stack
/// result is replaced. Native allocation, copy, language handling and free stay
/// original; no registry value, IAT target or executable instruction is changed
/// except breakpoints owned by the existing debugger transport.
/// </summary>
internal sealed class NativeProcessMediaPath
{
    private const uint FindMedia = 0x592F70, OpenCall = 0x592FB2, OpenReturn = 0x592FB8;
    private const uint QueryCall = 0x592FE5, QueryReturn = 0x592FE7, CloseCall = 0x5930F4, CloseReturn = 0x5930FA;
    private const uint ManagerTable = 0x7027E0, OpenImport = 0x6D900C, QueryImport = 0x6D9008, CloseImport = 0x6D9004;
    private const uint OriginalAccess = 0xF003F, QueryValueAccess = 1, BufferCapacity = 300;
    private const uint MaximumResolutions = 4;
    private readonly DebuggerHost _host;
    private readonly Action<object> _log;
    private readonly IntPtr _process;
    private readonly uint _pid, _thread;
    private readonly int _owner;
    private readonly byte[] _selectedPath, _function;
    private readonly string _path;
    private bool _installationAttempted, _installed, _retired, _failed;
    private bool _openProbe, _queryProbe, _closeProbe;
    private uint _resolverEntries, _resolutions, _completed, _accessWrites, _resultWrites, _bytesWritten;
    private string _failure = "";
    private ResolverFrame? _entered;
    private Frame? _frame;

    private sealed record ResolverFrame(uint Manager, uint Esp, uint Caller, uint DebugStop);

    private sealed class Frame(uint manager, uint entryEsp, uint openEsp, uint handleResult,
        uint openTarget, uint queryTarget, uint closeTarget)
    {
        internal readonly uint Manager = manager, EntryEsp = entryEsp, OpenEsp = openEsp;
        internal readonly uint HandleResult = handleResult, OpenTarget = openTarget, QueryTarget = queryTarget, CloseTarget = closeTarget;
        internal uint Handle, QueryEsp, Buffer, SizePointer, OriginalBytes, CloseEsp;
        internal bool OpenReturned, QueryEntered, QueryReturned, CloseEntered, CloseReturned;
    }

    internal NativeProcessMediaPath(DebuggerHost host, PEReader image, Action<object> log,
        string mediaDirectory)
    {
        ArgumentNullException.ThrowIfNull(host); ArgumentNullException.ThrowIfNull(image);
        ArgumentNullException.ThrowIfNull(log); FrameworkGuards.ThrowIfNullOrWhiteSpace(mediaDirectory);
        Require(host.IsAtDebugStop && host.ProcessHandle != IntPtr.Zero && host.ProcessPid != 0 &&
            host.MainThreadId != 0 && host.ImageBase == 0x400000,
            "Process-local MediaPath requires an owned original main-thread debug stop.");
        Require(Convert.ToHexString(SHA256.HashData(image.GetEntireImage().GetContent().ToArray())) ==
            "3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F",
            "Process-local MediaPath requires the exact original PC image.");
        Require(Path.IsPathFullyQualified(mediaDirectory), "Process-local MediaPath must be explicitly absolute.");
        _path = Path.GetFullPath(mediaDirectory).TrimEnd('\\') + "\\";
        Require(Directory.Exists(_path) && _path.All(c => c is >= ' ' and <= '~'),
            "Process-local MediaPath must be an existing directory represented by printable ASCII.");
        _selectedPath = Encoding.ASCII.GetBytes(_path + "\0");
        Require(_selectedPath.Length is > 1 and < (int)BufferCapacity,
            "Process-local MediaPath exceeds the original strict query/copy bound.");
        _host = host; _log = log; _process = host.ProcessHandle;
        _pid = host.ProcessPid; _thread = host.MainThreadId; _owner = Environment.CurrentManagedThreadId;
        _function = Image(image, FindMedia, 0x1C5);
        Require(Image(image, OpenCall, 6).SequenceEqual(Convert.FromHexString("FF150C906D00")) &&
            Image(image, QueryCall, 2).SequenceEqual(Convert.FromHexString("FFD6")) &&
            Image(image, CloseCall, 6).SequenceEqual(Convert.FromHexString("FF1504906D00")) &&
            Image(image, 0x593134, 1)[0] == 0xC3 &&
            Image(image, 0x592F9A, 30).SequenceEqual(Convert.FromHexString(
                "683F000F006A0068602870006802000080C7451400000000FF150C906D00")) &&
            Image(image, 0x592FC7, 32).SequenceEqual(Convert.FromHexString(
                "8D4C2408518D5424145250508B44241C685428700050C74424202C010000FFD6")) &&
            Image(image, 0x593191, 6).SequenceEqual(Convert.FromHexString("C706E0277000")),
            "Original MediaPath call sites, stack arguments or manager constructor differ.");
        Require(Image(image, 0x702860, 26).SequenceEqual(Encoding.ASCII.GetBytes("Software\\Konami\\Winx Club\0")) &&
            Image(image, 0x702854, 10).SequenceEqual(Encoding.ASCII.GetBytes("MediaPath\0")),
            "Original MediaPath registry literals differ.");
    }

    internal bool Completed => _completed > 0 && _frame is null && !_failed;
    internal bool Verified => Completed;
    internal bool HasPendingNativeCall => _entered is not null || _frame is not null;
    internal object Summary => new
    {
        installationAttempted = _installationAttempted, installed = _installed, retired = _retired,
        failed = _failed, failure = _failure, resolverEntries = _resolverEntries,
        resolutions = _resolutions, completedResolutions = _completed,
        selectedMediaDirectory = _path, selectedPathBytesIncludingNull = _selectedPath.Length,
        accessArgumentWrites = _accessWrites, successfulQueryResultWrites = _resultWrites,
        temporaryStackBytesWritten = _bytesWritten, pendingNativeResolution = HasPendingNativeCall,
        openEntryProbePending = _openProbe, queryEntryProbePending = _queryProbe, closeEntryProbePending = _closeProbe,
        originalRegistryOpenAndQueryRequired = true, originalSuccessfulStatusPreserved = true,
        originalAccessMask = "0x000F003F", requestedAccessMask = "0x00000001",
        nativeRegistryValueTypeObserved = false, originalLpTypeArgumentWasNull = true,
        originalAllocationCopyLanguageCloseAndFreePreserved = true,
        registryWrites = 0, apiCallsAdded = 0, apiReturnValuesFabricated = 0,
        nativeContextWritesByCallbacks = 0, executablePolicyPatches = 0,
        breakpointRestorationOwner = "SoftwareBreakpointManager", fullRuntimeQualificationClaimed = false
    };

    internal void InstallStopped()
    {
        EnsureStop(_thread);
        Require(!_installationAttempted && !_retired && !_failed, "Process-local MediaPath installation is single-use.");
        _installationAttempted = true;
        // Must install before the ordinary CP01 observer's entry breakpoint.
        Require(Read(FindMedia, _function.Length).SequenceEqual(_function),
            "Original FindMediaPath bytes differ before process-local probes are published.");
        _installed = true;
        Repeat(OpenCall, "MediaPath.original-registry-open", OnOpenCall);
        Repeat(QueryCall, "MediaPath.original-value-query", OnQueryCall);
        Repeat(CloseCall, "MediaPath.original-registry-close", OnCloseCall);
        _log(new { @event = "process-media-path-installed", selectedMediaDirectory = _path,
            originalRegistryCallsPreserved = true, registryWrites = 0, securitySettingsChanged = false,
            onlyCurrentOwnedProcess = _pid, originalManagerAllocationAndCopyPreserved = true });
    }

    private void OnOpenCall(uint tid, uint at, ref Win32Native.X86Context context)
    {
        CheckCallback(tid, at, OpenCall, context);
        Require(_frame is null && ++_resolutions <= MaximumResolutions &&
            Read(OpenCall, 6).SequenceEqual(Convert.FromHexString("FF150C906D00")),
            "Process-local MediaPath open is nested, over budget or moved.");
        ResolverFrame entered = _entered ?? throw new InvalidDataException("MediaPath open lacks its actual original resolver-entry observation.");
        uint stack = context.Esp, entry = entered.Esp, manager = context.Ebp;
        uint openTarget = U(OpenImport), queryTarget = U(QueryImport), closeTarget = U(CloseImport);
        Require(manager == entered.Manager && stack == checked(entry - 0x150) &&
            _host.DebugStopSequence > entered.DebugStop && manager >= 0x10000 && (manager & 3) == 0 &&
            U(manager) == ManagerTable && U(manager + 0x14) == 0 &&
            U(entry) == 0x59326B && openTarget >= 0x10000 && queryTarget >= 0x10000 && closeTarget >= 0x10000,
            "Process-local MediaPath lacks its exact original native manager/caller/import targets.");
        byte[] before = Read(stack, 20);
        uint handleResult = U(stack + 16);
        Require(U(stack) == 0x80000002 && U(stack + 4) == 0x702860 && U(stack + 8) == 0 &&
            U(stack + 12) == OriginalAccess && handleResult == checked(entry - 0x134),
            "Original RegOpenKeyExA five arguments or stack-local HKEY result changed.");
        _frame = new(manager, entry, stack, handleResult, openTarget, queryTarget, closeTarget);
        // This is the native call's own temporary samDesired DWORD. Registry
        // ACLs, key/value contents and API code are never changed.
        Write(stack + 12, BitConverter.GetBytes(QueryValueAccess)); ++_accessWrites;
        byte[] after = Read(stack, 20);
        Require(after.AsSpan(0, 12).SequenceEqual(before.AsSpan(0, 12)) &&
            after.AsSpan(16, 4).SequenceEqual(before.AsSpan(16, 4)) && U(stack + 12) == QueryValueAccess,
            "Only the exact original registry-open access argument may change.");
        _host.Breakpoints.AddReturn(OpenReturn, _thread, "MediaPath.original-registry-open-return", OnOpenReturn);
        _log(new { @event = "process-media-path-query-access", resolution = _resolutions,
            actualOriginalSamDesired = OriginalAccess, narrowedTemporarySamDesired = QueryValueAccess,
            originalOpenCallStillExecuted = true, nativeContextWrites = 0, registryWrites = 0 });
    }

    private void OnOpenReturn(uint tid, uint at, ref Win32Native.X86Context context)
    {
        EnsureStop(tid);
        Frame frame = RequireFrame();
        Require(at == OpenReturn && context.Eip == at && context.Esp == checked(frame.OpenEsp + 20) &&
            context.Ebp == frame.Manager && U(frame.Manager) == ManagerTable && !frame.OpenReturned &&
            U(OpenImport) == frame.OpenTarget && context.Eax == 0,
            "The original read-only RegOpenKeyExA did not actually succeed with its unchanged stdcall return.");
        uint handle = U(frame.HandleResult);
        Require(handle is not (0 or uint.MaxValue), "Original registry open did not produce an actual HKEY.");
        frame.Handle = handle; frame.OpenReturned = true;
        _log(new { @event = "process-media-path-original-open-returned", resolution = _resolutions,
            actualNativeStatus = context.Eax, originalStdcallArgumentBytes = 20, handleProduced = true,
            nativeStatusFabricated = false, registryWrites = 0 });
    }

    private void OnQueryCall(uint tid, uint at, ref Win32Native.X86Context context)
    {
        CheckCallback(tid, at, QueryCall, context);
        Frame frame = RequireFrame(); uint stack = context.Esp;
        Require(frame.OpenReturned && !frame.QueryEntered && context.Ebp == frame.Manager &&
            U(frame.Manager) == ManagerTable && U(frame.Manager + 0x14) == 0 &&
            Read(QueryCall, 2).SequenceEqual(Convert.FromHexString("FFD6")) &&
            context.Esi == frame.QueryTarget && U(QueryImport) == frame.QueryTarget &&
            stack == checked(frame.EntryEsp - 0x158),
            "Original MediaPath query lost its real opened manager/import/stack frame.");
        uint buffer = U(stack + 16), size = U(stack + 20);
        Require(U(stack) == frame.Handle && U(stack + 4) == 0x702854 && U(stack + 8) == 0 && U(stack + 12) == 0 &&
            buffer == checked(frame.EntryEsp - 0x130) && size == checked(frame.EntryEsp - 0x138) &&
            U(size) == BufferCapacity && checked(buffer + BufferCapacity) == checked(frame.EntryEsp - 4),
            "Original RegQueryValueExA arguments/capacity are not the exact owned MediaPath stack result.");
        frame.QueryEsp = stack; frame.Buffer = buffer; frame.SizePointer = size; frame.QueryEntered = true;
        _host.Breakpoints.AddReturn(QueryReturn, _thread, "MediaPath.original-value-query-return", OnQueryReturn);
    }

    private void OnQueryReturn(uint tid, uint at, ref Win32Native.X86Context context)
    {
        EnsureStop(tid);
        Frame frame = RequireFrame();
        Require(at == QueryReturn && context.Eip == at && frame.QueryEntered && !frame.QueryReturned &&
            context.Esp == checked(frame.QueryEsp + 24) && context.Ebp == frame.Manager &&
            U(frame.Manager) == ManagerTable && U(frame.Manager + 0x14) == 0 &&
            U(frame.HandleResult) == frame.Handle && U(QueryImport) == frame.QueryTarget && context.Eax == 0,
            "The original MediaPath RegQueryValueExA did not actually succeed with the same owner/result frame.");
        uint originalBytes = U(frame.SizePointer);
        Require(originalBytes is > 1 and < BufferCapacity,
            "Original MediaPath result exceeds the native strict copy bound.");
        byte[] original = Read(frame.Buffer, checked((int)originalBytes));
        Require(original[^1] == 0 && original.AsSpan(0, original.Length - 1).IndexOf((byte)0) < 0 &&
            original.AsSpan(0, original.Length - 1).ToArray().All(b => b is >= 32 and <= 126) &&
            Path.IsPathFullyQualified(Encoding.ASCII.GetString(original, 0, original.Length - 1)),
            "Original successful MediaPath query is not a bounded NUL-terminated absolute ASCII path.");
        frame.OriginalBytes = originalBytes;
        // The full native local buffer has capacity300 even when its successful
        // first result is shorter. Replace only path bytes and returned length;
        // native code performs its own allocation/copy after this real API return.
        Write(frame.Buffer, _selectedPath);
        Write(frame.SizePointer, BitConverter.GetBytes((uint)_selectedPath.Length));
        Require(Read(frame.Buffer, _selectedPath.Length).SequenceEqual(_selectedPath) &&
            U(frame.SizePointer) == _selectedPath.Length && U(frame.HandleResult) == frame.Handle,
            "Process-local MediaPath stack result readback differs.");
        frame.QueryReturned = true; ++_resultWrites;
        _log(new { @event = "process-media-path-successful-query-substituted", resolution = _resolutions,
            actualNativeStatus = context.Eax, originalResultBytes = originalBytes,
            selectedResultBytes = _selectedPath.Length, nativeBufferCapacity = BufferCapacity,
            originalStdcallArgumentBytes = 24, originalApiExecuted = true, originalApiStatusPreserved = true,
            originalLpTypeArgumentWasNull = true, nativeRegistryValueTypeObserved = false,
            allocationAndCopyStillOriginal = true, nativeContextWrites = 0, registryWrites = 0 });
    }

    private void OnCloseCall(uint tid, uint at, ref Win32Native.X86Context context)
    {
        CheckCallback(tid, at, CloseCall, context);
        Frame frame = RequireFrame();
        Require(frame.QueryReturned && !frame.CloseEntered && context.Ebp == frame.Manager &&
            U(frame.Manager) == ManagerTable && U(CloseImport) == frame.CloseTarget &&
            context.Esp == checked(frame.EntryEsp - 0x144) && U(context.Esp) == frame.Handle &&
            Read(CloseCall, 6).SequenceEqual(Convert.FromHexString("FF1504906D00")),
            "Original MediaPath registry close lost its actual successful-query handle/frame.");
        frame.CloseEsp = context.Esp; frame.CloseEntered = true;
        _host.Breakpoints.AddReturn(CloseReturn, _thread, "MediaPath.original-registry-close-return", OnCloseReturn);
    }

    private void OnCloseReturn(uint tid, uint at, ref Win32Native.X86Context context)
    {
        EnsureStop(tid); Frame frame = RequireFrame();
        Require(frame.CloseEntered && !frame.CloseReturned && at == CloseReturn && context.Eip == at &&
            context.Esp == checked(frame.CloseEsp + 4) && context.Ebp == frame.Manager &&
            U(frame.Manager) == ManagerTable && U(CloseImport) == frame.CloseTarget && context.Eax == 0,
            "The original MediaPath RegCloseKey did not actually succeed with its unchanged stdcall return.");
        frame.CloseReturned = true;
        _log(new { @event = "process-media-path-original-close-returned", resolution = _resolutions,
            actualNativeStatus = context.Eax, originalStdcallArgumentBytes = 4,
            originalApiExecuted = true, originalApiStatusPreserved = true, registryWrites = 0 });
    }

    // Called by the existing CP01 observer at its real return breakpoint. It
    // must keep the original entry receiver captured in that observer's frame;
    // no competing return probe is published at the same breakpoint slot.
    internal void ObserveResolverReturned(uint manager, uint tid, in Win32Native.X86Context context)
    {
        EnsureStop(tid);
        Require(_installed && !_retired && !_failed, "Process-local MediaPath result observation is retired.");
        Frame frame = RequireFrame();
        ResolverFrame entered = _entered ?? throw new InvalidDataException("MediaPath return lost its actual original resolver entry.");
        Require(frame.OpenReturned && frame.QueryReturned && frame.CloseReturned && manager == frame.Manager &&
            frame.EntryEsp == entered.Esp && manager == entered.Manager && context.Eip == entered.Caller &&
            context.Esp == checked(entered.Esp + 4) && U(frame.Manager) == ManagerTable &&
            U(frame.EntryEsp) == entered.Caller,
            "Original FindMediaPath did not return to its pinned caller after native query/close.");
        uint allocated = U(frame.Manager + 0x14);
        Require(allocated >= 0x10000 && Read(allocated, _selectedPath.Length).SequenceEqual(_selectedPath),
            "Original MediaPath allocation/copy did not produce the selected process-local directory.");
        ++_completed; _frame = null; _entered = null;
        _log(new { @event = "process-media-path-native-result-qualified", resolution = _resolutions,
            actualNativeAllocationAndCopyObserved = true, selectedMediaDirectory = _path,
            originalReturnInstructionActuallyExecuted = true, originalRegistryCloseActuallySucceeded = true,
            originalLanguageControlFlowPreserved = true,
            nativeHeapAllocationAddedByAdapter = 0, registryWrites = 0, nativeContextWrites = 0 });
    }

    // Called by the existing CP01 entry owner. Sharing its actual frame avoids
    // publishing a second return-probe callback at the same native entry slot.
    internal void ObserveResolverEntered(uint manager, uint tid, in Win32Native.X86Context context)
    {
        EnsureStop(tid);
        Require(_installed && !_retired && !_failed && _entered is null && _frame is null &&
            context.Eip == FindMedia && context.Ecx == manager && manager >= 0x10000 && (manager & 3) == 0 &&
            U(manager) == ManagerTable && U(context.Esp) == 0x59326B && ++_resolverEntries <= MaximumResolutions,
            "Process-local MediaPath needs its exact nonnested original CP01 entry/receiver/caller.");
        _entered = new(manager, context.Esp, U(context.Esp), _host.DebugStopSequence);
        _log(new { @event = "process-media-path-original-resolver-entry", ordinal = _resolverEntries,
            originalCaller = "0x0059326B", nativeFrameObservedBeforeOriginalPrologue = true,
            nativeContextWrites = 0, registryWrites = 0 });
    }

    internal void RetireCached()
    {
        EnsureOwner(); if (_retired) return;
        _retired = true;
        _log(new { @event = "process-media-path-retired", noRemoteReads = true,
            completedResolutions = _completed, pendingNativeResolution = HasPendingNativeCall,
            breakpointRestorationOwner = "SoftwareBreakpointManager", registryWrites = 0 });
    }

    private Frame RequireFrame() => _frame ?? throw new InvalidDataException("Process-local MediaPath lost its actual native frame.");
    private void Repeat(uint site, string name, BreakpointCallback callback)
    {
        BreakpointCallback repeat = null!;
        repeat = (uint tid, uint at, ref Win32Native.X86Context context) =>
        {
            SetPending(site, false);
            if (_retired || _failed) return;
            try { callback(tid, at, ref context); }
            catch (Exception error) { _failed = true; _retired = true; _failure = error.Message; throw; }
            if (!_retired && !_failed)
            { _host.Breakpoints.AddReturn(site, _thread, name, repeat); SetPending(site, true); }
        };
        _host.Breakpoints.AddReturn(site, _thread, name, repeat); SetPending(site, true);
    }
    private void SetPending(uint site, bool value)
    { if (site == OpenCall) _openProbe = value; else if (site == QueryCall) _queryProbe = value; else _closeProbe = value; }
    private void CheckCallback(uint tid, uint at, uint expected, in Win32Native.X86Context context)
    {
        EnsureStop(tid);
        Require(_installed && !_retired && !_failed && at == expected && context.Eip == at,
            "Process-local MediaPath callback left its exact original call/return site.");
    }
    private void Write(uint address, byte[] bytes)
    {
        EnsureStop(_thread);
        Require(!_retired && !_failed && address >= 0x10000 && bytes.Length is > 0 and <= (int)BufferCapacity &&
            (ulong)address + (uint)bytes.Length <= uint.MaxValue,
            "Process-local MediaPath write is retired or outside its bounded native result.");
        Win32Native.WriteMemory(_process, address, bytes, executable: false);
        _bytesWritten = checked(_bytesWritten + (uint)bytes.Length);
    }
    private uint U(uint address) => BitConverter.ToUInt32(Read(address, 4));
    private byte[] Read(uint address, int bytes)
    {
        EnsureStop(_thread);
        Require(!_retired && !_failed && address >= 0x10000 && bytes is > 0 and <= 0x200 &&
            (ulong)address + (uint)bytes <= uint.MaxValue,
            "Process-local MediaPath read is retired or outside its x86 bound.");
        return Win32Native.ReadMemory(_process, address, bytes);
    }
    private void EnsureOwner() => Require(Environment.CurrentManagedThreadId == _owner,
        "Process-local MediaPath has one debugger-owner thread.");
    private void EnsureStop(uint tid)
    {
        EnsureOwner();
        Require(_host.IsAtDebugStop && tid == _thread && _host.MainThreadId == _thread &&
            _host.ProcessHandle == _process && _host.ProcessPid == _pid && _host.ImageBase == 0x400000,
            "Process-local MediaPath requires its captured owned main-thread debug stop.");
    }
    private static byte[] Image(PEReader image, uint address, int bytes) =>
        image.GetSectionData(checked((int)(address - 0x400000))).GetContent(0, bytes).ToArray();
    private static void Require(bool value, string message) { if (!value) throw new InvalidDataException(message); }
}
