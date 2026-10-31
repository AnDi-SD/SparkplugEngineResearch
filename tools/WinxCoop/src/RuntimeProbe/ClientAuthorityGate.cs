using System.Reflection.PortableExecutable;
using System.Security.Cryptography;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// Bounded diagnostic policy for three selected client-side game call paths.
/// It preserves the original predicates and presentation callbacks, but does
/// not establish that every remaining callback is free of gameplay effects.
/// </summary>
internal sealed class ClientAuthorityGate
{
    private const uint TypedSite = 0x573D9D, TypedNext = 0x573DA3;
    private const uint TagSite = 0x4FB7F2, TagNext = 0x4FB7F5;
    private const uint AiSite = 0x57642D, AiNext = 0x576432;
    private const uint CallbackLimit = 50000;
    private const int SampleLimit = 24, HistogramLimit = 1024;
    private static readonly byte[] TypedBytes = [0x8B, 0x01, 0x52, 0xFF, 0x50, 0x04];
    private static readonly byte[] TagBytes = [0xFF, 0x52, 0x3C];
    private static readonly byte[] AiBytes = [0xE8, 0xBE, 0xF8, 0x02, 0x00];
    private static readonly (uint Table, uint ClassId)[] PresentationTypes =
    [
        (0x6F522C, 0x7FC114C3), // wxIntelliCam
        (0x6FE988, 0x69F878BA), // wxButton
        (0x6FEFAC, 0x7F2E3F1A), // wxHUDPart
        (0x6FF058, 0x401A393C), // wxHudMeters
        (0x6FF1F0, 0x65F4C9A8)  // wxLabelDisplayText
    ];
    private readonly DebuggerHost _host;
    private readonly PEReader _image;
    private readonly Action<object> _log;
    private readonly IntPtr _process;
    private readonly uint _pid;
    private readonly int _ownerThread;
    private readonly Dictionary<(uint Table, uint? ClassId), uint> _withheldTypes = [];
    private readonly List<object> _samples = [], _typePins = [];
    private uint _aiManager, _callbacks, _typedConsidered, _typedWithheld, _typedPreserved;
    private uint _otherMessagesPreserved, _animationTagsWithheld, _aiCallsWithheld, _contextOnlySkips;
    private bool _installed;

    internal ClientAuthorityGate(DebuggerHost host, PEReader image, Action<object> log)
    {
        Require(host.IsAtDebugStop && host.ProcessHandle != IntPtr.Zero && host.ProcessPid != 0 &&
            host.ImageBase == 0x400000 && host.MainThreadId != 0,
            "Client authority gate requires the pinned owned main-thread debug stop.");
        _host = host; _image = image; _log = log; _process = host.ProcessHandle;
        _pid = host.ProcessPid; _ownerThread = Environment.CurrentManagedThreadId;
    }

    internal bool Enabled => _installed;
    internal bool ModeSucceeded => _installed && _typedWithheld > 0 &&
        _typedPreserved > 0 && _animationTagsWithheld > 0 && _aiCallsWithheld > 0 &&
        _contextOnlySkips == _typedWithheld + _animationTagsWithheld + _aiCallsWithheld;
    internal object Summary => new
    {
        enabled = Enabled, succeeded = ModeSucceeded,
        policy = "selected typed1E, current-state animation tag and direct AI call suppression",
        callbacks = _callbacks, callbackLimit = CallbackLimit,
        typedConsidered = _typedConsidered, typedWithheld = _typedWithheld,
        typedPresentationPreserved = _typedPreserved, otherMessagesPreserved = _otherMessagesPreserved,
        animationTagsWithheld = _animationTagsWithheld, aiCallsWithheld = _aiCallsWithheld,
        contextOnlySkips = _contextOnlySkips,
        typedSkip = new { before = Hex(TypedSite), after = Hex(TypedNext), espDelta = 0 },
        animationTagSkip = new { before = Hex(TagSite), after = Hex(TagNext), espDelta = 4 },
        aiCallSkip = new { before = Hex(AiSite), after = Hex(AiNext), espDelta = 0 },
        presentationTypes = _typePins.ToArray(),
        withheldTypes = _withheldTypes.OrderBy(pair => pair.Key.Table).ThenBy(pair => pair.Key.ClassId)
            .Select(pair => new { table = Hex(pair.Key.Table),
                classId = pair.Key.ClassId is uint id ? Hex(id) : null, count = pair.Value }).ToArray(),
        samples = _samples.ToArray(),
        originalPredicatesAndPendingCallbacksPreserved = true,
        postTagAudioAndHistoryPathPreserved = true,
        nativeCallsInvoked = 0, gameDataWrites = 0, timerOrRoleFlagWrites = 0,
        capturedContextOtherThanEipEspPreserved = true,
        nativeFpEquivalenceClaimed = false, returnValueEmulated = false,
        unhitSubscriptionEndsWithOwnedProcess = true, inProcessGateRestorationClaimed = false,
        fullClientAuthorityClaimed = false,
        openPaths = new[] { "other AppHelper managers", "base Physics callbacks", "event queues",
            "direct/secondary notifications", "unobserved message effects" }
    };

    internal void Install()
    {
        EnsureStop(_host.MainThreadId);
        Require(!_installed, "Client authority gate was installed twice.");
        // The observer owns these two hooks and has already installed them.
        // Only its manager-owned leading INT3 may differ at this instant.
        VerifySite(TypedSite, TypedBytes, sharedBreakpointAllowed: true);
        VerifySite(TagSite, TagBytes, sharedBreakpointAllowed: true);
        VerifySite(AiSite, AiBytes);
        VerifySite(AiNext, [0xA1, 0xF8, 0x5A, 0x76, 0x00]); // overwrites the unused AI return
        foreach ((uint table, uint classId) in PresentationTypes) VerifyType(table, classId, presentation: true);
        _aiManager = U(0x765BC0);
        Require(_aiManager >= 0x10000 && U(_aiManager) == 0x706640,
            "Client authority gate AI singleton is not the qualified manager.");
        VerifyType(0x706640, 0x56887B3F, presentation: false);
        _installed = true;
        BreakpointCallback repeat = null!;
        repeat = (uint thread, uint at, ref Win32Native.X86Context context) =>
        {
            EnsureStop(thread);
            Require(at == AiSite && context.Eip == AiSite && U(0x765BC0) == _aiManager &&
                context.Ecx == _aiManager && U(_aiManager) == 0x706640,
                "Client authority gate direct AI call lost its captured native caller/owner.");
            VerifySite(AiSite, AiBytes);
            VerifySite(AiNext, [0xA1, 0xF8, 0x5A, 0x76, 0x00]);
            ++_aiCallsWithheld;
            Skip("direct-ai-call", ref context, AiNext, 0,
                new { receiver = Hex(context.Ecx), nativeTarget = Hex(0x5A5CF0),
                    arguments = 0, nativeReturnImmediatelyOverwritten = true });
            _host.Breakpoints.AddReturn(AiSite, thread, "ClientAuthority.AiCaller", repeat);
        };
        _host.Breakpoints.AddReturn(AiSite, _host.MainThreadId, "ClientAuthority.AiCaller", repeat);
        _log(new { @event = "client-authority-gate-installed", aiManager = Hex(_aiManager),
            typedObserverOwnsHook = Hex(TypedSite), animationObserverOwnsHook = Hex(TagSite),
            ownedAiCallerHook = Hex(AiSite), presentationTypes = _typePins.ToArray(),
            nativeCallsInvoked = 0, gameDataWrites = 0, fullClientAuthorityClaimed = false });
    }

    /// <summary>Called only after the observer's exact entry/ABI delivery pairing.</summary>
    internal bool TryWithholdTyped(uint code, uint table, uint? classId, ref Win32Native.X86Context context)
    {
        EnsureStop(_host.MainThreadId);
        Require(Enabled && context.Eip == TypedSite && context.Ecx >= 0x10000 && U(context.Ecx) == table,
            "Client authority typed gate lacks its exact pre-push receiver/site.");
        VerifySite(TypedSite, TypedBytes);
        ++_typedConsidered;
        if (code != 0x1E) { ++_otherMessagesPreserved; return false; }
        foreach ((uint expectedTable, uint expectedClass) in PresentationTypes)
        {
            if (table != expectedTable) continue;
            Require(classId == expectedClass, "Presentation table and independently observed class ID disagree.");
            ++_typedPreserved;
            return false;
        }
        var key = (table, classId);
        Require(_withheldTypes.Count < HistogramLimit || _withheldTypes.ContainsKey(key),
            "Client authority gate type histogram exceeds its bound.");
        _withheldTypes[key] = _withheldTypes.GetValueOrDefault(key) + 1;
        ++_typedWithheld;
        Skip("typed-1E-delivery", ref context, TypedNext, 0,
            new { code = Hex(code), table = Hex(table), classId = classId is uint id ? Hex(id) : null,
                receiver = Hex(context.Ecx), argumentPushSkipped = true,
                nativePredicatesAlreadyExecuted = true });
        return true;
    }

    /// <summary>Called after the observer validates Animator/message/current-state ABI.</summary>
    internal void WithholdAnimationTag(ref Win32Native.X86Context context)
    {
        EnsureStop(_host.MainThreadId);
        Require(Enabled && context.Eip == TagSite && context.Esi >= 0x10000 && U(context.Esi) == 0x6F670C &&
            context.Edi >= 0x10000 && U(context.Edi) == 11 && U(context.Esp) == context.Edi &&
            context.Ecx == context.Eax && context.Ecx >= 0x10000 && U(context.Ecx) == context.Edx,
            "Client authority animation tag gate lost its qualified pending argument/current state.");
        VerifySite(TagSite, TagBytes);
        ++_animationTagsWithheld;
        Skip("animation-current-state-tag", ref context, TagNext, 4,
            new { animator = Hex(context.Esi), state = Hex(context.Ecx), message = Hex(context.Edi),
                pendingArgumentRemoved = true, postCallbackAudioPathRetained = true });
    }

    private void VerifyType(uint table, uint classId, bool presentation)
    {
        byte[] tableBytes = Image(table, 0x38);
        Require(Read(table, tableBytes.Length).SequenceEqual(tableBytes),
            "Client authority gate type table differs from the pinned original image.");
        uint getter = BitConverter.ToUInt32(tableBytes, 0x10);
        byte[] getterBytes = Image(getter, 6);
        Require(getterBytes[0] == 0xB8 && getterBytes[5] == 0xC3 &&
            Read(getter, 6).SequenceEqual(getterBytes), "Client authority type getter is not its original literal registration getter.");
        uint record = BitConverter.ToUInt32(getterBytes, 1);
        Require(U(record) == classId, "Client authority type registration does not match its exact table/class pair.");
        var proof = new { table = Hex(table), classId = Hex(classId), getter = Hex(getter),
            record = Hex(record), getterBytes = Convert.ToHexString(getterBytes),
            notifyEntry = Hex(BitConverter.ToUInt32(tableBytes, 4)), presentation };
        if (presentation) _typePins.Add(proof);
        _log(new { @event = "client-authority-type-pin", proof });
    }

    private void Skip(string channel, ref Win32Native.X86Context context, uint next, uint espDelta, object detail)
    {
        Require(++_callbacks <= CallbackLimit, "Client authority gate exceeded its callback bound.");
        uint originalEip = context.Eip, originalEsp = context.Esp;
        string before = ContextRemainderHash(context);
        context.Eip = next;
        context.Esp = checked(context.Esp + espDelta);
        string after = ContextRemainderHash(context);
        Require(before == after, "Client authority gate changed captured context outside EIP/ESP.");
        ++_contextOnlySkips;
        if (_samples.Count >= SampleLimit) return;
        object sample = new { channel, originalEip = Hex(originalEip), resultEip = Hex(context.Eip),
            originalEsp = Hex(originalEsp), resultEsp = Hex(context.Esp), espDelta,
            capturedContextExceptEipEspSha256 = before, contextRemainderUnchanged = true, detail };
        _samples.Add(sample);
        _log(new { @event = "client-authority-call-withheld", ordinal = _contextOnlySkips, sample });
    }

    private static string ContextRemainderHash(Win32Native.X86Context context)
    {
        using var bytes = new MemoryStream();
        using var writer = new BinaryWriter(bytes);
        foreach (uint value in new[] { context.ContextFlags, context.Dr0, context.Dr1, context.Dr2,
            context.Dr3, context.Dr6, context.Dr7, context.SegGs, context.SegFs, context.SegEs, context.SegDs,
            context.Edi, context.Esi, context.Ebx, context.Edx, context.Ecx, context.Eax, context.Ebp,
            context.SegCs, context.EFlags, context.SegSs, context.FloatSave.ControlWord,
            context.FloatSave.StatusWord, context.FloatSave.TagWord, context.FloatSave.ErrorOffset,
            context.FloatSave.ErrorSelector, context.FloatSave.DataOffset, context.FloatSave.DataSelector,
            context.FloatSave.Cr0NpxState }) writer.Write(value);
        writer.Write(context.FloatSave.RegisterArea ?? Array.Empty<byte>());
        writer.Write(context.ExtendedRegisters ?? Array.Empty<byte>());
        writer.Flush();
        return Convert.ToHexString(SHA256.HashData(bytes.ToArray()));
    }

    private void VerifySite(uint address, byte[] literal, bool sharedBreakpointAllowed = false)
    {
        Require(Image(address, literal.Length).SequenceEqual(literal),
            $"Client authority gate original-image signature mismatch at {Hex(address)}.");
        byte[] actual = Read(address, literal.Length);
        bool shared = sharedBreakpointAllowed && actual[0] == 0xCC && _host.Breakpoints.Contains(address) &&
            actual.AsSpan(1).SequenceEqual(literal.AsSpan(1));
        Require(actual.SequenceEqual(literal) || shared,
            $"Client authority gate live signature mismatch at {Hex(address)}.");
    }

    private byte[] Image(uint address, int bytes)
    {
        Require(address is >= 0x401000 and < 0x740000 && bytes is >= 1 and <= 256,
            "Client authority gate image read exceeds its fixed code/table bounds.");
        return _image.GetSectionData(checked((int)(address - 0x400000))).GetContent(0, bytes).ToArray();
    }
    private uint U(uint address) => BitConverter.ToUInt32(Read(address, 4));
    private byte[] Read(uint address, int bytes)
    {
        EnsureStop(_host.MainThreadId);
        Require(address >= 0x10000 && bytes is >= 1 and <= 256, "Client authority gate read exceeds its pointer/length bound.");
        _ = checked(address + (uint)bytes - 1);
        return Win32Native.ReadMemory(_process, address, bytes);
    }
    private void EnsureStop(uint thread) => Require(_host.IsAtDebugStop && thread == _host.MainThreadId &&
        _host.ProcessHandle == _process && _host.ProcessPid == _pid && Environment.CurrentManagedThreadId == _ownerThread,
        "Client authority gate requires its captured owned main-thread debug stop.");
    private static string Hex(uint value) => $"0x{value:X8}";
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidDataException(message); }
}
