using System.Reflection.PortableExecutable;
using System.Security.Cryptography;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

// A one-time diagnostic startup through the original LoadSave. This is not a
// recurring profile setter and is never admitted after our template is built.
internal sealed class NativeInitialSaveBootstrap
{
    private const uint LoadSave = 0x4E7580, ProfileTable = 0x6F5A44;
    private readonly DebuggerHost _host;
    private readonly Action<object> _log;
    private readonly string _savedFile, _expectedHash;
    private readonly long _deadline;
    private readonly byte[] _entryPin, _returnPin;
    private uint _stage, _profile, _originalPlaying, _originalRoot, _worlds;
    private bool _complete;

    internal NativeInitialSaveBootstrap(ProbeConfiguration config, DebuggerHost host, PEReader image, Action<object> log)
    {
        if (config.InitialSaveSha256 is not { Length: 64 } hash || !hash.All(Uri.IsHexDigit) ||
            config.DiagnosticWorldState?.Role != WorldStateRole.Host ||
            config.TemplateSharedPlayerProfile != "alfea-28-network-input" || !config.EnableWorldEntry ||
            config.WorldEntryCaptureOrdinal != 2 || config.WorldEntrySeed is not null)
            throw new InvalidDataException("Initial save bootstrap requires the explicit Host Alfea28 second-entry cohort.");
        _host = host; _log = log; _expectedHash = hash;
        _savedFile = Path.GetFullPath(Path.Combine(config.MediaDirectory, "Saved", "Saved00.wxs"));
        _deadline = checked(Environment.TickCount64 + config.DurationSeconds * 1000L);
        _entryPin = image.GetSectionData(checked((int)(LoadSave - 0x400000))).GetContent(0, 7).ToArray();
        _returnPin = image.GetSectionData(0x4E7B88 - 0x400000).GetContent(0, 3).ToArray();
        Require(Convert.ToHexString(_entryPin) == "6AFFE9AF70F3FF" &&
            _returnPin.AsSpan().SequenceEqual(new byte[] { 0xC2, 0x04, 0x00 }),
            "Initial original LoadSave gateway/RET4 pins differ.");
        VerifyFile();
    }

    internal bool Complete => _complete;
    internal object Summary => new { complete = _complete, stage = _stage, worlds = _worlds,
        originalSaveFileSha256 = _expectedHash, nativeSaveSlot = 0,
        nativeLoadCalls = _stage == 0 ? 0 : 1, recurringLoadSaveSetter = false,
        ownBodiesCreatedBeforeLoad = 0, gameSaveFileProducedByAdapter = false };

    // true means the original startup is still being observed; no template or
    // world snapshot may be started by the caller during this interval.
    internal bool ObserveAtWorld(uint tid, ref Win32Native.X86Context context,
        NativeCallDispatcher calls, Action resume, bool mediaVerified, bool originalSecondEntryQualified)
    {
        if (_complete) return false;
        Require(_host.IsAtDebugStop && tid == _host.MainThreadId &&
            context.Eip == NativeCallDispatcher.WorldReturn && !calls.Pending &&
            Environment.TickCount64 <= _deadline && ++_worlds <= NativeObservationBudget.WorldLimit,
            "Initial save bootstrap escaped its bounded owned WorldRET.");
        if (!mediaVerified) return true;
        uint flow = U(0x755294), profile = U(0x765AD4);
        if (flow < 0x10000 || profile < 0x10000 || U(profile) != ProfileTable ||
            U(flow + 0x1B0) != 28 || U(flow + 0x1B8) != 0) return true;
        uint depth = U(flow + 0x1AC); Require(depth <= 20, "Initial save bootstrap flow bound exceeded.");
        uint playing = U(flow + 0x15C + depth * 4), record = U(profile + 0x2B4);
        if (playing < 0x10000 || U(playing) != 0x710874 || U(playing + 0x10) != 28 ||
            record < 0x10000 || U(record) != 0x710F80) return true;
        uint root = U(record + 0x24);
        if (root < 0x10000 || U(root) != 0x6DC4F4 || U(root + 0x3C) < 0x10000) return true;
        if (_stage == 0)
        {
            VerifyFile();
            Require(Win32Native.ReadMemory(_host.ProcessHandle, LoadSave, _entryPin.Length).SequenceEqual(_entryPin) &&
                Win32Native.ReadMemory(_host.ProcessHandle, 0x4E7B88, 3).SequenceEqual(_returnPin),
                "Live original LoadSave gateway/RET4 changed before bootstrap.");
            _profile = profile; _originalPlaying = playing; _originalRoot = root; _stage = 1;
            _log(new { @event = "world-initial-native-save-load-begin", slot = 0, fileSha256 = _expectedHash,
                existingTypedPrimaryRequired = true, ownTemplateNotStarted = true,
                initialLevel = 28, nativeFunction = LoadSave, nativeArguments = new uint[] { 0 } });
            calls.Begin(_host, tid, ref context, LoadSave, profile, [0], false, value =>
            {
                Require((value & 0xFF) == 1, "Original initial LoadSave returned false.");
                _stage = 2;
                _log(new { @event = "world-initial-native-save-load-returned", nativeResult = value,
                    originalSaveRestorationRan = true, awaitingFreshScene = true });
                resume();
            });
            return true;
        }
        Require(_stage == 2 && profile == _profile, "Initial save singleton changed before its new scene was ready.");
        // The allocator may reuse both addresses. The separate original-loader
        // observer proves ordinal2, the real return, spawn and accepted cohort.
        if (!originalSecondEntryQualified) return true;
        _complete = true; _stage = 3;
        _log(new { @event = "world-initial-native-save-bootstrap-complete", nativeLevel = 28,
            originalSecondEntryQualified = true, playingAddressReused = playing == _originalPlaying,
            rootAddressReused = root == _originalRoot, currentStars = U(profile + 0x6F0),
            nativeSaveSlot = Win32Native.ReadMemory(_host.ProcessHandle, profile + 0x6DC, 1)[0],
            nativeCalls = 1, profileFieldWritesByAdapter = 0, oldNativeOwnersDereferenced = false });
        return false;
    }

    private void VerifyFile()
    {
        Require(File.Exists(_savedFile) && (File.GetAttributes(_savedFile) & FileAttributes.ReparsePoint) == 0 &&
            Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(_savedFile))).Equals(_expectedHash, StringComparison.OrdinalIgnoreCase),
            "Initial save bootstrap requires the exact staged donor in its isolated game's Saved00 slot.");
    }
    private uint U(uint address) => Win32Native.ReadUInt32(_host.ProcessHandle, address);
    private static void Require(bool value, string message) { if (!value) throw new InvalidDataException(message); }
}
