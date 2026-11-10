using System.Reflection.PortableExecutable;
using System.Security.Cryptography;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// Opt-in authority at the pinned ordinary Profile save writer, on the owned
/// main thread only. Guest returns the original false/RET4 result before the
/// writer's first instruction; Host observes its real caller return unchanged.
/// This is not a universal persistence or upstream capture interception policy.
/// </summary>
internal sealed class NativeSaveAuthorityGate
{
    private const uint Writer = 0x4E55E0, ProfileGlobal = 0x765AD4, ProfileTable = 0x6F5A44;
    private const uint SuccessRet = 0x4E5AB8, FailureRet = 0x4E5ACB;
    private const uint MaximumCalls = 256;
    private static readonly byte[] EntryPin = Convert.FromHexString("83EC1CE97E10F6FF");
    private static readonly byte[] Ret4Pin = [0xC2, 0x04, 0x00];
    private static readonly Dictionary<uint, byte[]> CallerPins = new()
    {
        [0x543BC4] = Convert.FromHexString("E81C1AFAFF"),
        [0x5E4D66] = Convert.FromHexString("E87A08F0FF"),
        [0x5EEB99] = Convert.FromHexString("E8476AEFFF")
    };

    private readonly DebuggerHost _host;
    private readonly PEReader _image;
    private readonly Action<object> _log;
    private readonly WorldStateRole _role;
    private readonly IntPtr _process;
    private readonly uint _pid, _thread;
    private readonly int _owner;
    private bool _installationAttempted, _installed, _retired, _failed, _entryProbePending;
    private uint _calls, _rejected, _hostReturns, _hostTrue, _hostFalse, _retiredPassThrough;
    private string _failure = "";
    private HostFrame? _pending;

    private sealed record HostFrame(uint Sequence, uint Profile, uint Slot, uint Caller,
        uint Esp, uint Ebx, uint Esi, uint Edi, uint Ebp);

    internal NativeSaveAuthorityGate(DebuggerHost host, PEReader image, Action<object> log,
        WorldStateRole role)
    {
        ArgumentNullException.ThrowIfNull(host); ArgumentNullException.ThrowIfNull(image);
        ArgumentNullException.ThrowIfNull(log);
        Require(role is WorldStateRole.Host or WorldStateRole.Guest,
            "Native save authority requires an explicit Host or Guest role.");
        Require(host.IsAtDebugStop && host.ImageBase == 0x400000 && host.ProcessPid != 0 &&
            host.MainThreadId != 0 && host.ProcessHandle != IntPtr.Zero,
            "Native save authority requires its owned original main-thread debug session.");
        Require(Convert.ToHexString(SHA256.HashData(image.GetEntireImage().GetContent().ToArray())) ==
            "3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F",
            "Native save authority requires the exact original PC executable.");
        _host = host; _image = image; _log = log; _role = role;
        _process = host.ProcessHandle; _pid = host.ProcessPid; _thread = host.MainThreadId;
        _owner = Environment.CurrentManagedThreadId;
        PinImage(Writer, EntryPin); PinImage(SuccessRet, Ret4Pin); PinImage(FailureRet, Ret4Pin);
        PinImage(0x4E5AA9, [0xB0, 0x01]); PinImage(0x4E5AC0, [0x32, 0xC0]);
        foreach ((uint caller, byte[] bytes) in CallerPins) PinImage(caller - 5, bytes);
    }

    internal bool Healthy => _installed && !_failed && _pending is null;
    internal WorldStateRole Role => _role;
    internal uint GuestRejections => _rejected;
    internal uint HostNativeReturns => _hostReturns;
    internal uint HostSuccessfulReturns => _hostTrue;
    internal object Summary => new
    {
        role = _role.ToString(), installationAttempted = _installationAttempted,
        installed = _installed, retired = _retired, failed = _failed, failure = _failure,
        calls = _calls, guestRejections = _rejected, hostNativeReturns = _hostReturns,
        hostAlTrue = _hostTrue, hostAlFalse = _hostFalse,
        pendingHostReturn = _pending is not null, entryProbePending = _entryProbePending,
        retiredPassThrough = _retiredPassThrough, maximumCalls = MaximumCalls,
        writer = Hex(Writer), allowedCallerReturns = CallerPins.Keys.Order().Select(Hex).ToArray(),
        ownedMainThreadOnly = true, allThreadSaveInterceptionClaimed = false,
        beforeOriginalWriterFirstInstruction = true, upstreamProfileCaptureSuppressed = false,
        otherPersistenceOrSettingsWritersSuppressed = false,
        guestFileOrProfileDataWrites = 0, guestNativeSaveCalls = 0,
        rejectedWriterResult = "AL=0/RET4; upper EAX and non-control registers preserved",
        hostSaveReplayCalls = 0, fileDurabilityOrRoundTripQualified = false,
        runtimeQualificationClaimed = false,
        breakpointRestorationOwner = "SoftwareBreakpointManager on next dormant hit or owned-session Dispose"
    };

    internal void InstallStopped()
    {
        EnsureStop(_thread);
        Require(!_installationAttempted && !_retired && !_failed,
            "Native save authority installation is single-use.");
        _installationAttempted = true;
        try
        {
            Require(!_host.Breakpoints.Contains(Writer) && _pending is null,
                "Native save writer entry already belongs to another probe.");
            VerifyLivePins();
            _installed = true;
            ArmEntry();
            _log(new { @event = "native-save-authority-installed", role = _role.ToString(),
                writer = Hex(Writer), processPid = _pid, mainThread = _thread,
                guestWriterSuppressed = _role == WorldStateRole.Guest,
                allThreadOrOtherPersistenceProtectionClaimed = false });
        }
        catch (Exception error) { Fail(error); throw; }
    }

    // This policy spans scene unload/reload. Retire only when ending the owned
    // diagnostic session, never merely because the old scene has disappeared.
    // Existing breakpoint infrastructure owns restoration, as for other cached
    // policies: no unrelated probes or threads are removed here.
    internal void RetireCached()
    {
        EnsureOwner();
        if (_retired) return;
        Require(_pending is null, "Native save authority cannot retire an unmatched Host writer frame.");
        _retired = true;
        _log(new { @event = "native-save-authority-retired", role = _role.ToString(),
            entryProbePending = _entryProbePending, pendingHostReturn = false,
            immediateAddressRestorationClaimed = false, noRemoteReads = true });
    }

    private void ArmEntry()
    {
        _host.Breakpoints.AddReturn(Writer, _thread, "NativeSaveAuthority.writer", Entered);
        _entryProbePending = true;
    }

    private void Entered(uint tid, uint at, ref Win32Native.X86Context context)
    {
        EnsureOwner(); _entryProbePending = false;
        if (_retired || _failed) { ++_retiredPassThrough; return; }
        try
        {
            EnsureStop(tid);
            Require(_installed && at == Writer && context.Eip == Writer && _pending is null &&
                ++_calls <= MaximumCalls && context.Esp >= 0x10000 && (context.Esp & 3) == 0,
                "Native save writer escaped its bounded unpaired main-thread entry.");
            VerifyLivePins();
            uint caller = U(context.Esp), slot = U(checked(context.Esp + 4));
            Require(CallerPins.TryGetValue(caller, out byte[]? callerPin) &&
                Read(caller - 5, 5).SequenceEqual(callerPin) &&
                U(ProfileGlobal) == context.Ecx && context.Ecx >= 0x10000 &&
                (context.Ecx & 3) == 0 && U(context.Ecx) == ProfileTable &&
                slot <= 9 && U(checked(context.Ecx + 0x6DC)) == slot,
                "Native save writer has an unknown callsite, profile owner or selected slot.");
            if (_role == WorldStateRole.Guest)
            {
                uint entryEsp = context.Esp, originalEax = context.Eax;
                RejectGuestWriter(ref context, caller);
                ++_rejected;
                _log(new { @event = "native-save-authority-guest-writer-rejected",
                    sequence = _calls, profile = Hex(context.Ecx), slot,
                    originalCaller = Hex(caller), entryEsp = Hex(entryEsp), returnEsp = Hex(context.Esp),
                    originalEax = Hex(originalEax), returnedEax = Hex(context.Eax),
                    falseAl = context.Eax & 255, nativeRetArgumentBytes = 4,
                    originalWriterFirstInstructionExecuted = false,
                    writerManagerCaptureOrFileEffectsExecuted = false,
                    upstreamCaptureClaimedSuppressed = false, gameDataWrites = 0 });
            }
            else
            {
                Require(!_host.Breakpoints.Contains(caller),
                    "Native Host save return already belongs to another probe.");
                HostFrame frame = new(_calls, context.Ecx, slot, caller, context.Esp,
                    context.Ebx, context.Esi, context.Edi, context.Ebp);
                _pending = frame;
                _host.Breakpoints.AddReturn(caller, _thread, "NativeSaveAuthority.Host.return",
                    (uint rt, uint address, ref Win32Native.X86Context returned) => Returned(rt, address, ref returned, frame));
                _log(new { @event = "native-save-authority-host-writer-begin", sequence = frame.Sequence,
                    profile = Hex(frame.Profile), slot = frame.Slot, caller = Hex(frame.Caller),
                    entryEsp = Hex(frame.Esp), nativeFrameUnchanged = true,
                    fileWriteSuccessClaimedByEntry = false });
            }
            ArmEntry();
        }
        catch (Exception error) { Fail(error); throw; }
    }

    // A pure context transform used only after the caller has admitted original
    // bytes, main-thread identity, known native caller, canonical Profile and
    // argument. It adds no call to the opaque writer or its protected gateway.
    internal static void RejectGuestWriter(ref Win32Native.X86Context context, uint caller)
    {
        Require(context.Eip == Writer && context.Esp >= 0x10000 && (context.Esp & 3) == 0 &&
            CallerPins.ContainsKey(caller) && context.Esp <= uint.MaxValue - 8,
            "Native Guest writer rejection lacks its exact entry/caller/RET4 context.");
        context.Eax &= 0xFFFFFF00;
        context.Esp += 8;
        context.Eip = caller;
    }

    private void Returned(uint tid, uint at, ref Win32Native.X86Context context, HostFrame frame)
    {
        try
        {
            EnsureStop(tid);
            Require(_installed && !_retired && !_failed && ReferenceEquals(_pending, frame) &&
                at == frame.Caller && context.Eip == at && context.Esp == checked(frame.Esp + 8) &&
                context.Ebx == frame.Ebx && context.Esi == frame.Esi && context.Edi == frame.Edi &&
                context.Ebp == frame.Ebp && U(ProfileGlobal) == frame.Profile && U(frame.Profile) == ProfileTable &&
                (context.Eax & 255) is 0 or 1,
                "Native Host writer return differs in caller/RET4/registers/profile or bool domain.");
            ++_hostReturns;
            if ((context.Eax & 255) == 1) ++_hostTrue; else ++_hostFalse;
            _pending = null;
            _log(new { @event = "native-save-authority-host-writer-return", sequence = frame.Sequence,
                profile = Hex(frame.Profile), slot = frame.Slot, caller = Hex(at),
                nativeEsp = Hex(context.Esp), nativeEspDelta = 8, actualAl = context.Eax & 255,
                nativeFrameUnchanged = true, fileDurabilityOrLoadRoundTripClaimed = false });
        }
        catch (Exception error) { Fail(error); throw; }
    }

    private void VerifyLivePins()
    {
        Require(Read(Writer, EntryPin.Length).SequenceEqual(EntryPin) &&
            Read(SuccessRet, 3).SequenceEqual(Ret4Pin) && Read(FailureRet, 3).SequenceEqual(Ret4Pin) &&
            Read(0x4E5AA9, 2).SequenceEqual(new byte[] { 0xB0, 0x01 }) &&
            Read(0x4E5AC0, 2).SequenceEqual(new byte[] { 0x32, 0xC0 }),
            "Original save writer entry, bool result or RET4 pins changed.");
    }
    private void PinImage(uint address, byte[] expected) => Require(
        _image.GetSectionData(checked((int)(address - 0x400000))).GetContent(0, expected.Length)
            .ToArray().SequenceEqual(expected), "Original save authority image pin differs.");
    private void EnsureOwner() => Require(Environment.CurrentManagedThreadId == _owner,
        "Native save authority is owned by its synchronous runner thread.");
    private void EnsureStop(uint tid)
    {
        EnsureOwner();
        Require(_host.IsAtDebugStop && _host.ProcessHandle == _process && _host.ProcessPid == _pid &&
            _host.MainThreadId == _thread && tid == _thread && _host.ImageBase == 0x400000,
            "Native save authority lost its owned original main-thread stop.");
    }
    private uint U(uint address) => BitConverter.ToUInt32(Read(address, 4));
    private byte[] Read(uint address, int bytes) => Win32Native.ReadMemory(_process, address, bytes);
    private void Fail(Exception error) { _failed = true; _retired = true; _failure = error.Message; }
    private static string Hex(uint value) => $"0x{value:X8}";
    private static void Require(bool value, string message) { if (!value) throw new InvalidDataException(message); }
}
