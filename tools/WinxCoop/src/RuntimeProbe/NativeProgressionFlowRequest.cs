using System.Reflection.PortableExecutable;
using System.Security.Cryptography;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

internal sealed class NativeProgressionFlowRequest
{
    private readonly DebuggerHost _host;
    private readonly PEReader _image;
    private readonly Action<object> _log;
    private readonly bool _useSavedReload;
    private readonly uint _sceneLevel;
    private readonly string? _savedPath;
    private string? _loadFileSha256;
    private int _loadFileBytes;
    private bool _called, _returned;
    internal NativeProgressionFlowRequest(DebuggerHost host, PEReader image, Action<object> log,
        bool useSavedReload = false, string? savedPath = null, CampaignSceneProfile? campaign = null)
    {
        _host = host; _image = image; _log = log; _useSavedReload = useSavedReload;
        campaign?.Validate(); _sceneLevel = campaign?.Level ?? 28u;
        if (useSavedReload)
        {
            if (savedPath is null || !Path.IsPathFullyQualified(savedPath) ||
                !Path.GetFileName(savedPath).Equals("Saved00.wxs", StringComparison.OrdinalIgnoreCase) ||
                Convert.ToHexString(SHA256.HashData(image.GetEntireImage().GetContent().ToArray())) != ProbeMain.OriginalSha256)
                throw new InvalidDataException("Saved progression reload requires the pinned original image and explicit isolated Saved00 path.");
            _savedPath = Path.GetFullPath(savedPath);
            PinImage(0x4E7580, "6AFFE9AF70F3FF"); PinImage(0x4E7B88, "C20400");
        }
    }
    internal bool Started => _called;
    internal bool Returned => _returned;
    internal string? LoadFileSha256 => _loadFileSha256;
    internal object Summary => new
    {
        called = _called, returned = _returned, useSavedReload = _useSavedReload,
        nativeFunction = _useSavedReload ? "0x004E7580" : "0x00595450",
        loadFileSha256 = _loadFileSha256, loadFileBytes = _loadFileBytes,
        nativeSaveSlot = _useSavedReload ? 0 : (int?)null,
        nativeSavedStateAndHealthRestorationDelegatedToOriginalLoader = _useSavedReload,
        fileUnchangedAfterNativeLoadRequired = _useSavedReload,
        profileOrHealthWritesByAdapter = 0, nativeCallCount = _called ? 1 : 0,
        completedEntryOrResourceRetirementClaimed = false
    };
    internal void BeginReloadAtWorld(uint tid, ref Win32Native.X86Context context, NativeCallDispatcher calls,
        NativeProgressionTransitionGate gate, Action resume)
    {
        if (_called) return;
        if (_useSavedReload)
        {
            BeginSavedReloadAtWorld(tid, ref context, calls, resume);
            return;
        }
        byte[] pin = _image.GetSectionData(0x595450 - 0x400000).GetContent(0, 8).ToArray();
        byte[] actual = Win32Native.ReadMemory(_host.ProcessHandle, 0x595450, pin.Length);
        if (actual[0] == 0xCC && _host.Breakpoints.Contains(0x595450)) actual[0] = pin[0];
        if (!_host.IsAtDebugStop || tid != _host.MainThreadId || context.Eip != NativeCallDispatcher.WorldReturn ||
            !actual.SequenceEqual(pin) || _called || calls.Pending || calls.HasRetiredPage)
            throw new InvalidDataException("Progression reload is outside its pinned one-call World boundary.");
        uint flow = Win32Native.ReadUInt32(_host.ProcessHandle, 0x755294);
        if (flow < 0x10000 || Win32Native.ReadUInt32(_host.ProcessHandle, flow) != 0x702D2C)
            throw new InvalidDataException("Progression reload Flow identity differs.");
        gate.PermitOwnedRequestAtWorld(tid, context, calls, 28, 1);
        _called = true;
        _log(new { @event = "progression-native-reload-requested", destination = 28, argument = 1,
            nativeFunction = "0x00595450", oldSceneInputHeld = true, originalRequestDoesNotClaimCompletedEntry = true });
        calls.Begin(_host, tid, ref context, 0x595450, flow, [28, 1], false, value =>
        {
            _returned = true;
            _log(new { @event = "progression-native-reload-request-returned", resultOpaque = value,
                newEntryOrRetirementClaimed = false });
            resume();
        });
    }

    private void BeginSavedReloadAtWorld(uint tid, ref Win32Native.X86Context context,
        NativeCallDispatcher calls, Action resume)
    {
        if (!_host.IsAtDebugStop || tid != _host.MainThreadId || context.Eip != NativeCallDispatcher.WorldReturn ||
            _called || calls.Pending || calls.HasRetiredPage)
            throw new InvalidDataException("Saved progression reload is outside its one-call original World boundary.");
        PinLive(0x4E7580, "6AFFE9AF70F3FF"); PinLive(0x4E7B88, "C20400");
        uint flow = U(0x755294), profile = U(0x765AD4);
        if (flow < 0x10000 || U(flow) != 0x702D2C || U(flow + 0x1B8) != 0 ||
            profile < 0x10000 || U(profile) != 0x6F5A44)
            throw new InvalidDataException("Saved progression reload lost its settled original Flow/canonical Profile.");
        uint depth = U(flow + 0x1AC);
        if (depth >= 20) throw new InvalidDataException("Saved progression Flow exceeds its original twenty slots.");
        uint current = U(flow + 0x1B0), top = U(flow + 0x15C + depth * 4), playing = 0;
        bool saveOverlay = current is 69 or 72;
        if (top < 0x10000 || current != _sceneLevel && !saveOverlay)
            throw new InvalidDataException("Saved progression reload has neither original Alfea28 nor its native save overlay.");
        if (saveOverlay) VerifyOriginalSaveOverlayAtStop(_host, _image, top, current);
        for (uint index = 0; index <= depth; ++index)
        {
            uint candidate = U(flow + 0x15C + index * 4);
            if (candidate < 0x10000)
                throw new InvalidDataException("Saved progression Flow contains an empty retained stack slot.");
            if (U(candidate) != 0x710874 || U(candidate + 0x10) != _sceneLevel) continue;
            if (playing != 0)
                throw new InvalidDataException("Saved progression reload found multiple retained Alfea28 owners.");
            playing = candidate;
        }
        uint record = U(profile + 0x2B4);
        if (playing == 0 || (saveOverlay ? playing == top : playing != top) ||
            record < 0x10000 || U(record) != 0x710F80)
            throw new InvalidDataException("Saved progression reload has no retained typed native Alfea28 primary.");
        uint root = U(record + 0x24);
        uint engine = U(0x755274), input = U(0x7552A0), timer = U(0x755298);
        if (root < 0x10000 || U(root) != 0x6DC4F4 || engine < 0x10000 || input < 0x10000 ||
            timer < 0x10000 || U(input) != 0x6F5508 || U(timer) != 0x702F94)
            throw new InvalidDataException("Saved progression reload canonical primary has no initialized native scene Root.");
        uint scene = U(engine + 0x18), device = U(input + 0x1C);
        if (scene < 0x10000 || U(scene) != 0x6E7358 || U(root + 0x3C) != scene ||
            device < 0x10000 || U(device) != 0x7116CC)
            throw new InvalidDataException("Saved progression reload scene/input/device owners differ.");
        byte paused = Win32Native.ReadMemory(_host.ProcessHandle, timer + 0x40, 1)[0];
        if (paused > 1 || !saveOverlay && (U(device + 0x14) != 3 || paused != 0))
            throw new InvalidDataException("Saved progression reload lost its ready game or typed retained-overlay timer.");
        _loadFileSha256 = HashSavedFile(out _loadFileBytes);
        _called = true;
        _log(new { @event = "progression-native-saved-reload-requested", nativeFunction = "0x004E7580",
            canonicalProfile = $"0x{profile:X8}", arguments = new uint[] { 0 }, nativeSaveSlot = 0,
            fileSha256 = _loadFileSha256, fileBytes = _loadFileBytes, wholeOriginalGatewayRequested = true,
            retainedOriginalSaveOverlay = saveOverlay, currentNativeFlow = current,
            retainedPlaying = $"0x{playing:X8}", uniqueOriginalAlfea28OwnerRequired = true,
            readyGameplayClaimedForSaveOverlay = false,
            nativeSavedStateAndHealthRestorationRequested = true, oldSceneInputHeld = true,
            nativeProfileOrHealthWritesByAdapter = 0, ownedDirectFlowPermitUsed = false,
            originalInnerFlowCaller = "0x004E779B", originalRequestDoesNotClaimCompletedEntry = true });
        // The inner native595450 call returns to original4E779B. It uses the
        // existing Host observer's native caller path, not a private permit.
        calls.Begin(_host, tid, ref context, 0x4E7580, profile, [0], false, value =>
        {
            if (!_host.IsAtDebugStop || _host.MainThreadId != tid || _returned ||
                (value & 0xFF) != 1 || U(0x765AD4) != profile || U(profile) != 0x6F5A44 ||
                HashSavedFile(out int bytes) != _loadFileSha256 || bytes != _loadFileBytes)
                throw new InvalidDataException("Original saved progression LoadSave failed, changed Profile or changed its pinned input file.");
            _returned = true;
            _log(new { @event = "progression-native-saved-reload-returned", nativeResult = value,
                nativeSaveSlot = 0, unchangedFileSha256 = _loadFileSha256, originalRet4StackMatchedByDispatcher = true,
                nativeSavedStateAndHealthRestorationRan = true, profileOrHealthWritesByAdapter = 0,
                newEntryOrRetirementClaimed = false, dispatcherPageStillRetired = true });
            resume();
        });
    }

    // These IDs are admitted only together with the actual original vtable's
    // pinned literal RTTI accessor and state ID. No overlay class is invented.
    internal static void VerifyOriginalSaveOverlayAtStop(DebuggerHost host, PEReader image, uint state, uint current)
    {
        if (!host.IsAtDebugStop || host.ProcessHandle == IntPtr.Zero || host.ImageBase != 0x400000 ||
            current is not (69 or 72) || state < 0x10000 || Word(state + 0x10) != current)
            throw new InvalidDataException("Retained save overlay is not its actual original owned state.");
        uint table = Word(state);
        if (table is < 0x6DA000 or >= 0x744000)
            throw new InvalidDataException("Retained save overlay table is outside the original image.");
        uint getter = Word(table + 0x10);
        if (getter != BitConverter.ToUInt32(Original(table + 0x10, 4)) || getter is < 0x401000 or >= 0x6DA000)
            throw new InvalidDataException("Retained save overlay accessor is not its original table's literal getter.");
        byte[] literal = Original(getter, 6);
        if (literal[0] != 0xB8 || literal[5] != 0xC3 ||
            !Win32Native.ReadMemory(host.ProcessHandle, getter, 6).SequenceEqual(literal) ||
            Word(BitConverter.ToUInt32(literal, 1)) == 0)
            throw new InvalidDataException("Retained save overlay has no pinned original registered class accessor.");
        uint Word(uint at) => Win32Native.ReadUInt32(host.ProcessHandle, at);
        byte[] Original(uint at, int bytes) => image.GetSectionData(checked((int)(at - 0x400000))).GetContent(0, bytes).ToArray();
    }

    private string HashSavedFile(out int bytes)
    {
        if (_savedPath is null || !File.Exists(_savedPath) ||
            (File.GetAttributes(_savedPath) & FileAttributes.ReparsePoint) != 0)
            throw new InvalidDataException("Saved progression reload needs its ordinary isolated Saved00 slot file.");
        byte[] data = File.ReadAllBytes(_savedPath); bytes = data.Length;
        if (bytes is <= 0 or > 262144) throw new InvalidDataException("Saved progression slot exceeds its bounded native input size.");
        return Convert.ToHexString(SHA256.HashData(data));
    }
    private void PinImage(uint at, string hex)
    {
        byte[] original = _image.GetSectionData(checked((int)(at - 0x400000))).GetContent(0, hex.Length / 2).ToArray();
        if (!original.SequenceEqual(Convert.FromHexString(hex))) throw new InvalidDataException("Original saved progression gateway/RET4 pin differs.");
    }
    private void PinLive(uint at, string hex)
    {
        PinImage(at, hex); byte[] expected = Convert.FromHexString(hex), actual = Win32Native.ReadMemory(_host.ProcessHandle, at, expected.Length);
        for (int index = 0; index < actual.Length; ++index)
            if (actual[index] == 0xCC && expected[index] != 0xCC && _host.Breakpoints.Contains(checked(at + (uint)index)))
                actual[index] = expected[index];
        if (!actual.SequenceEqual(expected)) throw new InvalidDataException("Live saved progression gateway/RET4 changed outside owned breakpoints.");
    }
    private uint U(uint at) => Win32Native.ReadUInt32(_host.ProcessHandle, at);
}
