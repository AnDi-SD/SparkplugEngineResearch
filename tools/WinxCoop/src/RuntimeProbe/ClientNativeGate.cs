using System.ComponentModel;
using System.Reflection.PortableExecutable;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// Own-process native client policy, independent of the managed finite census.
/// Native code preserves integer registers/flags and uses original return
/// addresses; installation and restoration require an owned main-thread stop.
/// </summary>
internal sealed class ClientNativeGate
{
    private const int CodeBytes = 8192, DataBytes = 4096, CounterOffset = 0x40, CounterLimit = 240;
    private const uint PageExecuteRead = 0x20, MemRelease = 0x8000, SnapshotThreads = 4;
    private const uint DataGuard = 0x32474E43;
    private const int MaximumLiveStackBytes = 65536;
    private static readonly (uint Table, uint Class)[] PresentationTypes =
    [
        (0x6F522C, 0x7FC114C3), (0x6FE988, 0x69F878BA),
        (0x6FEFAC, 0x7F2E3F1A), (0x6FF058, 0x401A393C), (0x6FF1F0, 0x65F4C9A8)
    ];
    private readonly DebuggerHost _host;
    private readonly PEReader _image;
    private readonly Action<object> _log;
    private readonly IntPtr _process;
    private readonly uint _pid;
    private readonly int _ownerThread;
    private readonly List<Region> _regions = [];
    private readonly List<Recipient> _recipients = [];
    private readonly List<Patch> _patches = [];
    private readonly List<string> _counterNames = [];
    private readonly Dictionary<string, uint> _lastCounters = [];
    private readonly List<object> _threadWitnesses = [];
    private OwnedRemoteBuffer? _data;
    private IntPtr _codeAllocation;
    private uint _codeAddress, _flow, _playing, _manager;
    private byte[] _code = [];
    private string? _codeHash;
    private bool _installed, _restored, _released, _publicationStarted, _restoredFromUnload;
    private uint _restoreEntry, _releaseWorldStops, _restoreStopSequence;
    private readonly uint _level, _playingTable, _exitEntry;

    private sealed record Region(string Name, uint Site, byte[] Original, uint Next, uint EspDelta, uint? Target,
        byte[]? DestinationSignature = null);
    private sealed record Recipient(string Name, uint Table, uint Class, uint OriginalNotify, uint[] AllowedCodes);
    private sealed record Patch(string Name, uint Address, byte[] Original, byte[] Replacement, bool Vtable);

    internal ClientNativeGate(DebuggerHost host, PEReader image, Action<object> log, bool battlePresentation = false)
    {
        Require(host.IsAtDebugStop && host.ProcessHandle != IntPtr.Zero && host.ProcessPid != 0 &&
            host.ImageBase == 0x400000 && host.MainThreadId != 0,
            "Native client gate requires its pinned owned debug stop.");
        _host = host; _image = image; _log = log; _process = host.ProcessHandle;
        _pid = host.ProcessPid; _ownerThread = Environment.CurrentManagedThreadId;
        _level = battlePresentation ? 47u : 28u;
        _playingTable = battlePresentation ? 0x7105C8u : 0x710874u;
        _exitEntry = battlePresentation ? 0x5F2300u : 0x5F86C0u;
    }

    internal bool Enabled => _installed && !_restored;
    internal bool Restored => _restored;
    internal bool Released => _released;
    internal object Summary => new
    {
        installed = _installed, enabled = Enabled, restored = _restored, released = _released,
        patchPublicationAttempted = _publicationStarted,
        codeAddress = Hex(_codeAddress), codeUsedBytes = _code.Length, codeAllocatedBytes = CodeBytes,
        codeProtection = "PAGE_EXECUTE_READ", codeSha256 = _codeHash,
        data = _data?.Summary, maximumOwnedPages = 3,
        counters = new Dictionary<string, uint>(_lastCounters),
        patches = _patches.Select(patch => new
        {
            patch.Name, address = Hex(patch.Address), original = Convert.ToHexString(patch.Original),
            replacement = Convert.ToHexString(patch.Replacement), vtable = patch.Vtable
        }).ToArray(),
        regions = _regions.Select(region => new { region.Name, site = Hex(region.Site),
            original = Convert.ToHexString(region.Original), next = Hex(region.Next), region.EspDelta,
            destinationSignature = region.DestinationSignature is null ? null : Convert.ToHexString(region.DestinationSignature) }).ToArray(),
        restoreEntry = Hex(_restoreEntry), releaseWorldStops = _releaseWorldStops,
        restoredAtNativeUnload = _restoredFromUnload, restoreDebugStopSequence = _restoreStopSequence,
        ownedThreadInstructionWitnesses = _threadWitnesses.ToArray(),
        releaseReferenceScanScope = "All owned x86 thread GP registers and aligned DWORDs in the complete committed ESP..region-end range; both RX/RW leases",
        maximumLiveStackBytesPerThread = MaximumLiveStackBytes, wholeHeapCodePointerAbsenceClaimed = false,
        integerRegistersAndFlagsSaved = true, floatingPointInstructionsEmitted = false,
        originalNotifyReturnAddressPreserved = true, managedPerEntityCallbacks = false,
        vtablePolicyWrites = _patches.Count(patch => patch.Vtable), gameWorldFieldWrites = 0,
        nativeRoleOrTimerWrites = 0, fullClientAuthorityClaimed = false
    };

    internal void AddSkipCall(string name, uint site, byte[] expected, uint target, uint next, uint espArgumentBytes)
    {
        Require(expected.Length == 5 && expected[0] == 0xE8 && next == checked(site + 5) &&
            unchecked((uint)((long)site + 5 + BitConverter.ToInt32(expected, 1))) == target,
            "Native client call descriptor is not the exact original relative CALL.");
        AddRegion(new(name, site, expected.ToArray(), next, espArgumentBytes, target));
    }

    internal void AddSkipRegion(string name, uint site, byte[] exact, uint next, uint espDelta)
    {
        Require(exact.Length is >= 5 and <= 32 && next == checked(site + (uint)exact.Length),
            "Native client region must cover its exact complete original instruction span.");
        AddRegion(new(name, site, exact.ToArray(), next, espDelta, null));
    }

    internal void AddForwardRedirect(string name, uint site, byte[] exact, uint destination, byte[] destinationSignature)
    {
        Require(exact.Length is >= 5 and <= 32 && destination > checked(site + (uint)exact.Length) &&
            destination - site <= 4096 && destinationSignature.Length is >= 1 and <= 32 &&
            Image(destination, destinationSignature.Length).SequenceEqual(destinationSignature),
            "Native client redirect needs a bounded, separately pinned forward destination.");
        AddRegion(new(name, site, exact.ToArray(), destination, 0, null, destinationSignature.ToArray()));
    }

    private void AddRegion(Region region)
    {
        EnsureStop(_host.MainThreadId);
        Require(!_installed && _regions.Count < 32 && region.EspDelta <= 16 && region.EspDelta % 4 == 0 &&
            !string.IsNullOrWhiteSpace(region.Name), "Native client region descriptor exceeds its configuration bounds.");
        Require(!_regions.Any(value => value.Name == region.Name || Overlaps(value.Site, value.Original.Length,
            region.Site, region.Original.Length)), "Native client region descriptors overlap or repeat.");
        Require(!Overlaps(region.Site, region.Original.Length, 0x573D9D, 6) &&
            !Overlaps(region.Site, region.Original.Length, 0x4FB7F2, 9) &&
            !Overlaps(region.Site, region.Original.Length, 0x57642D, 5),
            "Additional native client region overlaps a built-in policy site.");
        Require(Image(region.Site, region.Original.Length).SequenceEqual(region.Original),
            "Native client region differs from the pinned original image.");
        _regions.Add(region);
    }

    internal void AddRecipientQuarantine(string name, uint table, uint classId, uint originalNotify, uint[] allowedCodes)
    {
        EnsureStop(_host.MainThreadId);
        Require(!_installed && _recipients.Count < 64 && !string.IsNullOrWhiteSpace(name) &&
            allowedCodes.Length is >= 1 and <= 8 && allowedCodes.Distinct().Count() == allowedCodes.Length &&
            allowedCodes.All(code => code is 0x1C or 0x1D),
            "Native client recipient configuration must preserve only explicitly qualified lifecycle codes.");
        Require(!PresentationTypes.Any(value => value.Table == table) &&
            !_recipients.Any(value => value.Table == table || value.Name == name),
            "Native client recipient repeats or aliases a protected presentation type.");
        // Descriptor construction can precede original RTTI initialization.
        // The live class ID is independently admitted by Install after Ready.
        uint getter = ImageWord(table + 0x10); byte[] accessor = Image(getter, 6);
        Require(accessor[0] == 0xB8 && accessor[5] == 0xC3 &&
            ImageWord(table + 4) == originalNotify && U(table + 4) == originalNotify &&
            originalNotify is >= 0x401000 and < 0x6DA000,
            "Native recipient Notify pointer differs from its original table slot.");
        _recipients.Add(new(name, table, classId, originalNotify, allowedCodes.ToArray()));
    }

    internal void Install(uint thread, ref Win32Native.X86Context context)
    {
        EnsureStop(thread);
        Require(!_installed && !_restored && !_released && context.Eip == NativeCallDispatcher.WorldReturn,
            "Native client gate installation requires its first qualified WorldRET.");
        CaptureEpoch();
        foreach ((uint table, uint @class) in PresentationTypes) VerifyType(table, @class);
        foreach (Recipient recipient in _recipients) VerifyType(recipient.Table, recipient.Class);
        ValidateOriginal(0x573D9D, [0x8B, 0x01, 0x52, 0xFF, 0x50, 0x04]);
        ValidateOriginal(0x4FB7F2, [0xFF, 0x52, 0x3C, 0x8B, 0xB6, 0x24, 0x01, 0x00, 0x00]);
        ValidateOriginal(0x57642D, [0xE8, 0xBE, 0xF8, 0x02, 0x00]);
        foreach (Region region in _regions)
        {
            ValidateOriginal(region.Site, region.Original);
            if (region.DestinationSignature is byte[] signature) ValidateOriginal(region.Next, signature);
        }
        foreach (Recipient recipient in _recipients)
            Require(U(recipient.Table + 4) == recipient.OriginalNotify, "Native recipient changed before installation.");

        // There are no published native pointers until every code/data byte is
        // emitted, read back, and the code allocation is sealed RX.
        try
        {
            _data = new OwnedRemoteBuffer(_host, DataBytes);
            _data.Write(0, BitConverter.GetBytes(DataGuard));
            _data.Write(DataBytes - 4, BitConverter.GetBytes(DataGuard));
            _codeAllocation = Win32Native.VirtualAllocEx(_process, IntPtr.Zero, (UIntPtr)CodeBytes,
                Win32Native.MemReserve | Win32Native.MemCommit, Win32Native.PageReadWrite);
            if (_codeAllocation == IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error(), "Native client RX allocation failed.");
            _codeAddress = checked((uint)_codeAllocation.ToInt64());
            var emitter = new Emitter(_codeAddress);
            uint typed = BuildTyped(emitter);
            _patches.Add(CodePatch("typed-notify", 0x573D9D, [0x8B, 0x01, 0x52, 0xFF, 0x50, 0x04], typed));
            uint tag = BuildSkip(emitter, Counter("animation-state-tags-withheld"), 4,
                [0x8B, 0xB6, 0x24, 0x01, 0x00, 0x00], 0x4FB7FB);
            _patches.Add(CodePatch("animation-state-tag", 0x4FB7F2,
                [0xFF, 0x52, 0x3C, 0x8B, 0xB6, 0x24, 0x01, 0x00, 0x00], tag));
            uint ai = BuildSkip(emitter, Counter("ai-calls-withheld"), 0, [], 0x576432);
            _patches.Add(CodePatch("app-helper-ai", 0x57642D, [0xE8, 0xBE, 0xF8, 0x02, 0x00], ai));
            foreach (Region region in _regions)
            {
                uint stub = BuildSkip(emitter, Counter("region:" + region.Name), region.EspDelta, [], region.Next);
                _patches.Add(CodePatch(region.Name, region.Site, region.Original, stub));
            }
            foreach (Recipient recipient in _recipients)
            {
                uint stub = BuildRecipient(emitter, recipient);
                _patches.Add(new(recipient.Name, recipient.Table + 4,
                    BitConverter.GetBytes(recipient.OriginalNotify), BitConverter.GetBytes(stub), true));
            }
            Require(_patches.Count <= 99 && _patches.SelectMany((a, i) => _patches.Skip(i + 1)
                .Select(b => !Overlaps(a.Address, a.Original.Length, b.Address, b.Original.Length))).All(value => value),
                "Native client patch spans overlap or exceed their cap.");
            _code = emitter.Bytes(); Require(_code.Length is > 0 and <= CodeBytes, "Native client emitted code exceeds its allocation.");
            Win32Native.WriteMemory(_process, _codeAddress, _code, executable: false);
            Require(Read(_codeAddress, _code.Length).SequenceEqual(_code), "Native client code write/readback mismatch.");
            if (!Win32Native.VirtualProtectEx(_process, _codeAllocation, (UIntPtr)CodeBytes, PageExecuteRead, out uint old) ||
                old != Win32Native.PageReadWrite || !Win32Native.FlushInstructionCache(_process, _codeAllocation, (UIntPtr)CodeBytes))
                throw new Win32Exception(Marshal.GetLastWin32Error(), "Native client code RX seal/cache flush failed.");
            _codeHash = Convert.ToHexString(SHA256.HashData(_code));
            // Publication is the only point at which native execution can use
            // either owned page. A partial publication aborts the owned process;
            // never free pages beneath a published detour.
            foreach (Patch patch in _patches)
            {
                Require(Read(patch.Address, patch.Original.Length).SequenceEqual(patch.Original),
                    "Native client original patch changed before publication.");
                _publicationStarted = true;
                WritePatch(patch.Address, patch.Replacement, patch.Vtable);
            }
            _installed = true;
            ReadCounters();
            _log(new { @event = "client-native-gate-installed", code = Hex(_codeAddress), codeBytes = _code.Length,
                codeSha256 = _codeHash, data = _data.Summary, patches = _patches.Count,
                recipientPolicies = _recipients.Count, originalReturnAddress = Hex(0x573DA3),
                managedPerEntityCallbacks = false, maximumOwnedPages = 3 });
        }
        catch
        {
            // If any patch is already visible, keeping the allocation until the
            // owned process is stopped is safer than freeing a possible target.
            if (!_publicationStarted)
            {
                if (_codeAllocation != IntPtr.Zero && !VirtualFreeEx(_process, _codeAllocation, UIntPtr.Zero, MemRelease))
                    throw new Win32Exception(Marshal.GetLastWin32Error(), "Unpublished native client code cleanup failed.");
                _codeAllocation = IntPtr.Zero;
                if (_data is { Released: false }) _data.Release(callPending: false);
            }
            throw;
        }
    }

    internal void AddOriginalVtableForwardAtWorld(uint thread, ref Win32Native.X86Context context,
        string name, uint table, uint slot, uint expectedFunction, uint targetFunction, byte[] expectedTargetPrefix)
    {
        EnsureStop(thread);
        Require(context.Eip == NativeCallDispatcher.WorldReturn && Enabled && !_released &&
            _level == 47 && _playingTable == 0x7105C8 && table == _playingTable && slot == 0x30 &&
            U(_playing) == table && U(_playing + 0x10) == _level &&
            U(_playing + 0x1BC) == 2 && U(_playing + 0x188) == 1 &&
            expectedFunction == 0x5F3230 && targetFunction == 0x5F76D0 &&
            expectedTargetPrefix.Length is >= 6 and <= 32 && !string.IsNullOrWhiteSpace(name),
            "Original playing-update forward is outside its owned ready Battle WorldRET.");
        uint address = checked(table + slot);
        byte[] original = BitConverter.GetBytes(expectedFunction), replacement = BitConverter.GetBytes(targetFunction);
        Require(ImageWord(address) == expectedFunction && Read(address, 4).SequenceEqual(original) &&
            !_patches.Any(patch => patch.Name == name || Overlaps(patch.Address, patch.Original.Length, address, 4)) &&
            _patches.Count < 99, "Original playing-update vtable slot is changed, repeated or outside bounds.");
        ValidateOriginal(targetFunction, expectedTargetPrefix);
        ValidateOriginal(expectedFunction, Image(expectedFunction, 12));
        // Track before publication: a failed/partial write must never leave an
        // untracked policy, and restoration uses the same owner as all gates.
        _patches.Add(new(name, address, original, replacement, true));
        WritePatch(address, replacement, true);
        _log(new { @event = "client-native-original-playing-update-forward", name,
            table = Hex(table), slot, original = Hex(expectedFunction), target = Hex(targetFunction),
            receiver = Hex(_playing), thread, debugStop = _host.DebugStopSequence,
            nativeArgumentsAndReturnAddressUnchanged = true, newCodeEmitted = false,
            countdownPhase = U(_playing + 0x1BC), originalBattleInitialized = U(_playing + 0x188),
            restoredBySameNativeGateOwner = true });
    }

    internal IReadOnlyDictionary<string, uint> ReadCounters()
    {
        EnsureStop(_host.MainThreadId);
        Require(_installed && !_released && _data is { Released: false }, "Native counters require their installed live pages.");
        Require(Read(_codeAddress, _code.Length).SequenceEqual(_code), "Native client RX code changed during its lifetime.");
        byte[] data = (_data ?? throw new InvalidOperationException("Native client data lease is absent.")).Read(0, DataBytes);
        Require(BitConverter.ToUInt32(data) == DataGuard && BitConverter.ToUInt32(data, DataBytes - 4) == DataGuard,
            "Native client counter page guards changed.");
        _lastCounters.Clear();
        for (int index = 0; index < _counterNames.Count; ++index)
            _lastCounters[_counterNames[index]] = BitConverter.ToUInt32(data, CounterOffset + index * 4);
        Require(_lastCounters.GetValueOrDefault("foreign-thread-calls") == 0,
            "Native client policy was reached by an unqualified foreign thread.");
        Require(_lastCounters.GetValueOrDefault("typed-considered") ==
            _lastCounters.GetValueOrDefault("typed-withheld") + _lastCounters.GetValueOrDefault("typed-presentation-preserved") +
            _lastCounters.GetValueOrDefault("typed-other-code-preserved"), "Native typed counters do not balance.");
        return new Dictionary<string, uint>(_lastCounters);
    }

    internal void RestoreAtUnload(uint thread, ref Win32Native.X86Context context, uint entry = 0x5F86C0)
    {
        EnsureStop(thread);
        Require(Enabled && entry == _exitEntry && context.Eip == entry && context.Ecx == _playing &&
            U(0x755294) == _flow && U(_playing) == _playingTable && U(_playing + 0x10) == _level,
            "Native client restoration requires the exact captured PlayingLevel unload entry.");
        byte[] entryBytes = Image(entry, 6);
        Require(Read(entry, entryBytes.Length).SequenceEqual(entryBytes), "Native unload entry has not been restored by its breakpoint owner.");
        RestorePatches(entry, fromUnload: true);
    }

    internal void RestoreAtWorldStop(uint thread, ref Win32Native.X86Context context)
    {
        EnsureStop(thread);
        Require(Enabled && context.Eip == NativeCallDispatcher.WorldReturn && U(0x755294) == _flow &&
            U(_flow + 0x1B0) == _level && U(_flow + 0x1B8) == 0 && U(_playing) == _playingTable,
            "Finite native client restoration requires its captured ready WorldRET.");
        uint stack = U(_flow + 0x1AC);
        Require(stack <= 20 && U(_flow + 0x15C + stack * 4) == _playing,
            "Finite native client restoration lost its captured playing state.");
        RestorePatches(NativeCallDispatcher.WorldReturn, fromUnload: false);
    }

    private void RestorePatches(uint entry, bool fromUnload)
    {
        ReadCounters();
        foreach (Patch patch in _patches.AsEnumerable().Reverse())
        {
            Require(Read(patch.Address, patch.Replacement.Length).SequenceEqual(patch.Replacement),
                "Native client patch changed before exact restoration.");
            WritePatch(patch.Address, patch.Original, patch.Vtable);
        }
        _restored = true; _restoreEntry = entry; _restoredFromUnload = fromUnload;
        _restoreStopSequence = _host.DebugStopSequence;
        _log(new { @event = fromUnload ? "client-native-gate-restored-before-unload" : "client-native-gate-restored-at-world",
            entry = Hex(entry), nativeUnloadClaimed = fromUnload, debugStopSequence = _restoreStopSequence,
            patches = _patches.Count, counterSnapshot = new Dictionary<string, uint>(_lastCounters),
            originalBoundaryInstructionStillNotExecuted = true, oldEntityObservationsMustStop = fromUnload,
            codePagesStillOwned = true });
    }

    internal bool ReleaseAtLaterWorld(uint thread, ref Win32Native.X86Context context)
    {
        EnsureStop(thread);
        Require(_restored && !_released && context.Eip == NativeCallDispatcher.WorldReturn &&
            _host.DebugStopSequence > _restoreStopSequence,
            "Native client page release requires a later actual WorldRET after restoration.");
        Require(++_releaseWorldStops <= 120, "Native client page release exceeded its deferred world budget.");
        foreach (Patch patch in _patches)
            Require(Read(patch.Address, patch.Original.Length).SequenceEqual(patch.Original),
                "Native client original instruction/table was changed after restoration.");
        if (!AllOwnedThreadReferencesOutsideLeases()) return false;
        ReadCounters();
        if (!VirtualFreeEx(_process, _codeAllocation, UIntPtr.Zero, MemRelease))
            throw new Win32Exception(Marshal.GetLastWin32Error(), "Native client RX page release failed.");
        _codeAllocation = IntPtr.Zero;
        _data!.Release(callPending: false); _released = true;
        _log(new { @event = "client-native-gate-pages-released", atLaterWorldStop = true,
            codeBytes = CodeBytes, dataBytes = DataBytes, ownedPagesReleased = 3,
            allOriginalPatchesVerified = true, allOwnedThreadInstructionsOutsideCode = true,
            allOwnedThreadGeneralRegistersOutsideLeases = true, completeCommittedLiveStacksScanned = true,
            allAlignedLiveStackDwordsOutsideLeases = true, bothRxAndRwLeasesChecked = true,
            wholeHeapCodePointerAbsenceClaimed = false,
            noRemoteReturnAddressesEmitted = true, oldEntityMemoryRead = false });
        return true;
    }

    private void CaptureEpoch()
    {
        _flow = U(0x755294); _manager = U(0x765ACC);
        Require(_flow >= 0x10000 && U(_manager) == 0x70069C && U(_flow + 0x1B0) == _level && U(_flow + 0x1B8) == 0,
            "Native client gate did not capture the exact ready Alfea manager/flow.");
        uint stack = U(_flow + 0x1AC); Require(stack <= 20, "Native client flow stack exceeds its bound.");
        _playing = U(_flow + 0x15C + stack * 4);
        Require(U(_playing) == _playingTable && U(_playing + 0x10) == _level, "Native client gate captured a non-playing flow state.");
    }

    private uint BuildTyped(Emitter e)
    {
        uint start = e.Address;
        e.Emit(0x9C, 0x50); ThreadCounter(e); Inc(e, Counter("typed-considered"));
        e.Emit(0x83, 0x3A, 0x1E); int other = e.Jump(0x85);
        e.Emit(0x8B, 0x01);
        var passes = new List<int>();
        foreach ((uint table, _) in PresentationTypes) { e.Emit(0x3D); e.U32(table); passes.Add(e.Jump(0x84)); }
        Inc(e, Counter("typed-withheld")); e.Emit(0x58, 0x9D); e.JumpAbsolute(0x573DA3);
        uint pass = e.Address; foreach (int branch in passes) e.PatchRelative(branch, pass);
        Inc(e, Counter("typed-presentation-preserved")); int forward = e.Jump();
        e.PatchRelative(other, e.Address); Inc(e, Counter("typed-other-code-preserved"));
        e.PatchRelative(forward, e.Address);
        // The original CALL's return is pushed literally before tail-jumping.
        // Receiver sees the exact original message argument/ESP/return address.
        e.Emit(0x58, 0x9D, 0x8B, 0x01, 0x52, 0x68); e.U32(0x573DA3); e.Emit(0xFF, 0x60, 0x04);
        return start;
    }

    private uint BuildSkip(Emitter e, uint counter, uint espDelta, byte[] replay, uint next)
    {
        uint start = e.Address; e.Emit(0x9C, 0x50); ThreadCounter(e); Inc(e, counter); e.Emit(0x58, 0x9D);
        if (espDelta != 0) e.Emit(0x8D, 0x64, 0x24, checked((byte)espDelta)); // LEA preserves flags
        e.Emit(replay); e.JumpAbsolute(next); return start;
    }

    private uint BuildRecipient(Emitter e, Recipient recipient)
    {
        uint start = e.Address; e.Emit(0x9C, 0x50); ThreadCounter(e); e.Emit(0x8B, 0x44, 0x24, 0x0C);
        var passes = new List<int>();
        foreach (uint code in recipient.AllowedCodes)
        {
            e.Emit(0x81, 0x38); e.U32(code); passes.Add(e.Jump(0x84));
        }
        Inc(e, Counter("recipient-drop:" + recipient.Name)); e.Emit(0x58, 0x9D, 0xC2, 0x04, 0x00);
        uint pass = e.Address; foreach (int branch in passes) e.PatchRelative(branch, pass);
        Inc(e, Counter("recipient-lifecycle:" + recipient.Name)); e.Emit(0x58, 0x9D); e.JumpAbsolute(recipient.OriginalNotify);
        return start;
    }

    private void ThreadCounter(Emitter e)
    {
        e.Emit(0x64, 0xA1); e.U32(0x24); e.Emit(0x3D); e.U32(_host.MainThreadId);
        int same = e.Jump(0x84); Inc(e, Counter("foreign-thread-calls")); e.PatchRelative(same, e.Address);
    }
    private static void Inc(Emitter e, uint address) { e.Emit(0xF0, 0xFF, 0x05); e.U32(address); }
    private uint Counter(string name)
    {
        int index = _counterNames.IndexOf(name);
        if (index < 0)
        {
            Require(_counterNames.Count < CounterLimit && CounterOffset + (_counterNames.Count + 1) * 4 < DataBytes - 4,
                "Native client counter inventory exceeds its owned data bound.");
            index = _counterNames.Count; _counterNames.Add(name);
        }
        return checked(_data!.Address + (uint)CounterOffset + (uint)index * 4);
    }

    private Patch CodePatch(string name, uint address, byte[] original, uint target)
    {
        byte[] bytes = Enumerable.Repeat((byte)0x90, original.Length).ToArray();
        bytes[0] = 0xE9; BitConverter.GetBytes(unchecked(target - (address + 5))).CopyTo(bytes, 1);
        return new(name, address, original.ToArray(), bytes, false);
    }
    private void ValidateOriginal(uint address, byte[] expected)
    {
        Require(Image(address, expected.Length).SequenceEqual(expected) && Read(address, expected.Length).SequenceEqual(expected),
            "Native client code site differs from the exact original bytes.");
        for (uint index = 0; index < expected.Length; ++index)
            Require(!_host.Breakpoints.Contains(address + index), "Native client patch overlaps an active managed breakpoint slot.");
    }
    private void VerifyType(uint table, uint classId)
    {
        byte[] bytes = Image(table, 0x14);
        Require(Read(table, bytes.Length).SequenceEqual(bytes), "Native client type table differs from its original image.");
        uint getter = BitConverter.ToUInt32(bytes, 0x10); byte[] accessor = Image(getter, 6);
        Require(accessor[0] == 0xB8 && accessor[5] == 0xC3 && Read(getter, 6).SequenceEqual(accessor) &&
            U(BitConverter.ToUInt32(accessor, 1)) == classId, "Native client table/class getter registration pin mismatch.");
    }
    private void WritePatch(uint address, byte[] bytes, bool vtable)
    {
        if (!vtable) Win32Native.WriteMemory(_process, address, bytes, executable: true);
        else
        {
            if (!Win32Native.VirtualProtectEx(_process, (IntPtr)(long)address, (UIntPtr)(uint)bytes.Length,
                Win32Native.PageReadWrite, out uint old))
                throw new Win32Exception(Marshal.GetLastWin32Error(), "Native client vtable temporary writable protection failed.");
            try { Win32Native.WriteMemory(_process, address, bytes, executable: false); }
            finally
            {
                if (!Win32Native.VirtualProtectEx(_process, (IntPtr)(long)address, (UIntPtr)(uint)bytes.Length, old, out _))
                    throw new Win32Exception(Marshal.GetLastWin32Error(), "Native client vtable protection restoration failed.");
            }
        }
        Require(Read(address, bytes.Length).SequenceEqual(bytes), "Native client patch write/readback mismatch.");
    }

    private bool AllOwnedThreadReferencesOutsideLeases()
    {
        Require(IntPtr.Size == 8 && Marshal.SizeOf<MemoryBasicInformation64>() == 48 &&
            _data is { Released: false }, "Native release scan requires its x64 query ABI and both live leases.");
        IntPtr snapshot = CreateToolhelp32Snapshot(SnapshotThreads, 0);
        if (snapshot == new IntPtr(-1)) throw new Win32Exception(Marshal.GetLastWin32Error(), "Owned native thread inventory failed.");
        var observations = new List<object>(); bool outside = true, mainFound = false;
        int registerMatches = 0, stackMatches = 0, stackBytes = 0, incompleteStacks = 0;
        try
        {
            ThreadEntry32 entry = new() { Size = (uint)Marshal.SizeOf<ThreadEntry32>() };
            bool present = Thread32First(snapshot, ref entry); uint total = 0;
            while (present)
            {
                Require(++total <= 65536, "System thread snapshot exceeds its diagnostic bound.");
                if (entry.OwnerPid == _pid)
                {
                    Require(observations.Count < 512, "Owned thread snapshot exceeds its diagnostic bound.");
                    Win32Native.X86Context context = Win32Native.GetX86Context(entry.ThreadId);
                    (string Name, uint Value)[] registers =
                    [
                        ("EAX", context.Eax), ("EBX", context.Ebx), ("ECX", context.Ecx),
                        ("EDX", context.Edx), ("ESI", context.Esi), ("EDI", context.Edi),
                        ("EBP", context.Ebp), ("ESP", context.Esp), ("EIP", context.Eip)
                    ];
                    int threadRegisterMatches = registers.Count(value => InOwnedLease(value.Value));
                    var stack = InspectLiveStack(context.Esp);
                    registerMatches += threadRegisterMatches; stackMatches += stack.Matches;
                    stackBytes += stack.Bytes; incompleteStacks += stack.Complete ? 0 : 1;
                    outside &= threadRegisterMatches == 0 && stack.Complete && stack.Matches == 0;
                    mainFound |= entry.ThreadId == _host.MainThreadId;
                    observations.Add(new { threadId = entry.ThreadId, instruction = Hex(context.Eip),
                        inOwnedCode = InRange(context.Eip, _codeAddress, CodeBytes),
                        registers = registers.Select(value => new { name = value.Name, value = Hex(value.Value),
                            inOwnedLease = InOwnedLease(value.Value) }).ToArray(), stack = stack.Witness });
                }
                entry.Size = (uint)Marshal.SizeOf<ThreadEntry32>(); present = Thread32Next(snapshot, ref entry);
            }
            Require(Marshal.GetLastWin32Error() == 18 && mainFound, "Owned thread snapshot terminated unexpectedly or omitted the main thread.");
        }
        finally { Win32Native.CloseHandle(snapshot); }
        if (_threadWitnesses.Count < 2 || outside)
            _threadWitnesses.Add(new { attempt = _releaseWorldStops, allOutside = outside, bothLeasesChecked = true,
                registerMatches, stackMatches, stackBytes, incompleteStacks, threads = observations.ToArray() });
        _log(new { @event = "client-native-gate-release-reference-scan", attempt = _releaseWorldStops,
            allOutside = outside, threads = observations.Count, registerMatches, stackMatches, stackBytes, incompleteStacks });
        return outside;
    }

    private (bool Complete, int Matches, int Bytes, object Witness) InspectLiveStack(uint esp)
    {
        // This is deliberately the complete current committed region above ESP,
        // not a fixed prefix which could miss a cached incoming stub pointer.
        UIntPtr queried = VirtualQueryEx(_process, (IntPtr)(long)esp, out MemoryBasicInformation64 info, (UIntPtr)48);
        ulong end = info.BaseAddress <= ulong.MaxValue - info.RegionSize ? info.BaseAddress + info.RegionSize : 0;
        ulong length = end > esp ? end - esp : 0;
        uint basicProtection = info.Protect & 0xFF;
        bool readable = basicProtection is 0x02 or 0x04 or 0x08 or 0x20 or 0x40 or 0x80;
        string? refusal = queried.ToUInt64() != 48 ? "VirtualQueryEx did not return its complete 48-byte structure" :
            esp < 0x10000 || info.BaseAddress > esp || end > 0x100000000UL || length == 0 ? "Invalid x86 live-stack bounds" :
            info.State != Win32Native.MemCommit || !readable || (info.Protect & 0x100) != 0 ? "Live stack is not wholly committed readable memory" :
            length > MaximumLiveStackBytes ? "Complete live-stack region exceeds the 64-KiB scan bound" : null;
        if (refusal is not null)
            return (false, 0, 0, new { complete = false, refusal, esp = Hex(esp),
                regionBase = $"0x{info.BaseAddress:X}", regionEnd = $"0x{end:X}", bytes = length,
                queryBytes = queried.ToUInt64(), state = Hex(info.State), protection = Hex(info.Protect) });

        byte[] bytes;
        try { bytes = Win32Native.ReadMemory(_process, esp, checked((int)length)); }
        catch (Exception exception) when (exception is Win32Exception or InvalidDataException)
        {
            return (false, 0, 0, new { complete = false, refusal = "Complete live-stack read failed",
                esp = Hex(esp), regionEnd = $"0x{end:X}", bytes = length, error = exception.Message });
        }
        if (bytes.Length != (int)length)
            return (false, 0, 0, new { complete = false, refusal = "Complete live-stack read was short",
                esp = Hex(esp), expectedBytes = length, actualBytes = bytes.Length });
        var samples = new List<object>(); int matches = 0, scannedDwords = 0;
        for (int offset = (int)((4 - (esp & 3)) & 3); offset <= bytes.Length - 4; offset += 4)
        {
            ++scannedDwords; uint value = BitConverter.ToUInt32(bytes, offset);
            if (!InOwnedLease(value)) continue;
            ++matches;
            if (samples.Count < 8) samples.Add(new { address = Hex(checked(esp + (uint)offset)), value = Hex(value) });
        }
        return (true, matches, bytes.Length, new { complete = true, esp = Hex(esp),
            regionBase = $"0x{info.BaseAddress:X}", regionEnd = $"0x{end:X}", bytes = bytes.Length,
            state = Hex(info.State), protection = Hex(info.Protect), scannedAlignedDwords = scannedDwords,
            matches, samples = samples.ToArray(), sha256 = Convert.ToHexString(SHA256.HashData(bytes)) });
    }

    private bool InOwnedLease(uint value) => InRange(value, _codeAddress, CodeBytes) ||
        InRange(value, _data!.Address, DataBytes);
    private static bool InRange(uint value, uint start, int bytes) => value >= start && (ulong)value < (ulong)start + (uint)bytes;

    private byte[] Image(uint address, int count)
    {
        Require(address is >= 0x401000 and < 0x740000 && count is >= 1 and <= 256,
            "Native client image lookup exceeds its pinned code/table range.");
        return _image.GetSectionData(checked((int)(address - 0x400000))).GetContent(0, count).ToArray();
    }
    private uint ImageWord(uint address) => BitConverter.ToUInt32(Image(address, 4));
    private uint U(uint address) => BitConverter.ToUInt32(Read(address, 4));
    private byte[] Read(uint address, int count)
    {
        EnsureStop(_host.MainThreadId);
        Require(address >= 0x10000 && count is >= 1 and <= CodeBytes, "Native client read exceeds its pointer/length bound.");
        _ = checked(address + (uint)count - 1); return Win32Native.ReadMemory(_process, address, count);
    }
    private void EnsureStop(uint thread) => Require(_host.IsAtDebugStop && thread == _host.MainThreadId &&
        _host.ProcessHandle == _process && _host.ProcessPid == _pid && Environment.CurrentManagedThreadId == _ownerThread,
        "Native client operation requires its captured owned main-thread debug stop.");
    private static bool Overlaps(uint a, int al, uint b, int bl) => (ulong)a < (ulong)b + (uint)bl && (ulong)b < (ulong)a + (uint)al;
    private static string Hex(uint value) => $"0x{value:X8}";
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidDataException(message); }

    private sealed class Emitter(uint start)
    {
        private readonly List<byte> _bytes = [];
        internal uint Address => checked(start + (uint)_bytes.Count);
        internal void Emit(params byte[] bytes) { _bytes.AddRange(bytes); }
        internal void U32(uint value) { _bytes.AddRange(BitConverter.GetBytes(value)); }
        internal int Jump(byte condition)
        {
            Emit(0x0F, condition); int displacement = _bytes.Count; U32(0); return displacement;
        }
        internal int Jump() { Emit(0xE9); int displacement = _bytes.Count; U32(0); return displacement; }
        internal void JumpAbsolute(uint target)
        {
            Emit(0xE9); U32(unchecked(target - (Address + 4)));
        }
        internal void PatchRelative(int displacement, uint target)
        {
            byte[] value = BitConverter.GetBytes(unchecked(target - (start + (uint)displacement + 4)));
            for (int index = 0; index < 4; ++index) _bytes[displacement + index] = value[index];
        }
        internal byte[] Bytes() => _bytes.ToArray();
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct ThreadEntry32
    {
        internal uint Size, Usage, ThreadId, OwnerPid;
        internal int BasePriority, DeltaPriority;
        internal uint Flags;
    }
    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    private struct MemoryBasicInformation64
    {
        internal ulong BaseAddress, AllocationBase;
        internal uint AllocationProtect, Alignment1;
        internal ulong RegionSize;
        internal uint State, Protect, Type, Alignment2;
    }
    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern UIntPtr VirtualQueryEx(IntPtr process, IntPtr address,
        out MemoryBasicInformation64 information, UIntPtr informationBytes);
    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern IntPtr CreateToolhelp32Snapshot(uint flags, uint processId);
    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool Thread32First(IntPtr snapshot, ref ThreadEntry32 entry);
    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool Thread32Next(IntPtr snapshot, ref ThreadEntry32 entry);
    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool VirtualFreeEx(IntPtr process, IntPtr address, UIntPtr size, uint freeType);
}
