using System.ComponentModel;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// Read-only, bounded correlation of the stock player's command and controller.
/// Offset labels intentionally do not assign gameplay meaning to raw fields.
/// </summary>
internal sealed class ControlObserver
{
    internal const uint InputReturn = 0x4E2D3E;
    private const int CommandBytes = 0x74, PhaseLimit = 256;
    // Original reset5992E0 initializes these individual bytes every update.
    // Bytes4E..51 are deliberately not assigned button semantics here.
    private static readonly int[] EdgeByteOffsets =
        Enumerable.Range(0x19, 0x31 - 0x19 + 1).Concat(Enumerable.Range(0x5C, 5)).Concat([0x64, 0x70]).ToArray();
    private readonly DebuggerHost _host;
    private readonly IntPtr _process;
    private readonly uint _pid;
    private readonly int _ownerThread, _sampleEvery;
    private readonly Action<object> _log;
    private readonly PhaseState _world = new(), _input = new();
    private uint _rejected, _relationshipsMatched;
    private uint _cachedInput, _cachedCommand, _edgeCaptures, _edgeBaselines;
    private byte[]? _lastObservedInputFingerprint;

    private sealed class PhaseState
    {
        internal uint Stops, Samples, PeriodicSamples, ExtraSamples;
        internal uint PreviousCommand;
        internal byte[]? PreviousBytes;
        internal readonly HashSet<int> ChangedBytes = [], ChangedFloats = [];
    }

    internal ControlObserver(DebuggerHost host, int sampleEvery, Action<object> log)
    {
        if (!host.IsAtDebugStop || host.ProcessHandle == IntPtr.Zero || host.ProcessPid == 0 ||
            host.ImageBase != 0x400000 || host.MainThreadId == 0)
            throw new InvalidOperationException("Control observer requires the pinned owned process at its initial debug stop.");
        _host = host; _process = host.ProcessHandle; _pid = host.ProcessPid;
        _ownerThread = Environment.CurrentManagedThreadId;
        _sampleEvery = sampleEvery; _log = log;
    }

    internal bool ModeSucceeded => _world.Samples > 0 && _input.Samples > 0 && _relationshipsMatched > 0;
    internal object Summary => new
    {
        readOnly = true, commandBytes = CommandBytes, samplesPerPhaseLimit = PhaseLimit,
        samplingMode = "periodic-and-input-command-edges",
        edgeFloatWordOffsets = new[] { Offset(0x04), Offset(0x08) },
        edgeByteOffsets = EdgeByteOffsets.Select(Offset).ToArray(),
        continuousReferenceHeading14TriggersCapture = false,
        inputCommandEdgeCaptures = _edgeCaptures, inputEdgeBaselineCaptures = _edgeBaselines,
        extraEventCaptures = _edgeCaptures + _edgeBaselines,
        world = PhaseSummary(_world), inputReturn = PhaseSummary(_input),
        rejectedObservations = _rejected, relationshipMatches = _relationshipsMatched,
        semanticFieldNamesAssigned = false, independentControllerClaimed = false
    };

    private static object PhaseSummary(PhaseState phase) => new
    {
        stops = phase.Stops, samples = phase.Samples,
        periodicSamples = phase.PeriodicSamples, extraSamples = phase.ExtraSamples,
        changedByteOffsets = phase.ChangedBytes.Order().Select(Offset).ToArray(),
        changedAlignedFloatOffsets = phase.ChangedFloats.Order().Select(Offset).ToArray()
    };

    internal void Observe(uint thread, uint address, ref Win32Native.X86Context context,
        uint worldUpdate, bool mediaVerified)
    {
        EnsureAtStop();
        if (thread != _host.MainThreadId || context.Eip != address ||
            address is not (InputReturn or NativeCallDispatcher.WorldReturn) || Read(address, 1)[0] != 0xC3)
            throw new InvalidDataException("Control observation return/thread/signature mismatch.");
        PhaseState phase = address == InputReturn ? _input : _world;
        ++phase.Stops;
        if (phase.Samples >= PhaseLimit || !mediaVerified) return;
        try
        {
            string? reason = phase.Stops % _sampleEvery == 0 ? "periodic" : null;
            if (address == InputReturn && _cachedCommand != 0)
            {
                // This baseline advances on every admitted input observation,
                // including stops which emit no full sample. It is not the last
                // periodic sample, so a short press and release are distinct.
                bool changed = ObserveInputEdge(out bool baseline);
                if (changed && reason is null)
                    reason = baseline ? "input-edge-baseline" : "input-command-edge";
            }
            if (reason is not null) Capture(phase, thread, address, worldUpdate, reason);
        }
        catch (Exception exception) when (exception is Win32Exception or InvalidDataException or OverflowException)
        {
            ++_rejected;
            ClearInputCache();
            // Missing startup/teardown dependencies are observations, not a
            // reason to fabricate pointers or to continue with partial data.
            if (_rejected <= 32)
                _log(new { @event = "control-observation-rejected", stoppedAt = Hex(address),
                    threadId = thread, worldUpdate, reason = exception.Message });
        }
    }

    private static byte[] EdgeFingerprint(byte[] command)
    {
        byte[] fingerprint = new byte[8 + EdgeByteOffsets.Length];
        Array.Copy(command, 0x04, fingerprint, 0, 8);
        for (int i = 0; i < EdgeByteOffsets.Length; ++i) fingerprint[8 + i] = command[EdgeByteOffsets[i]];
        return fingerprint;
    }

    private void ClearInputCache()
    {
        _cachedInput = _cachedCommand = 0;
        _lastObservedInputFingerprint = null;
    }

    private bool ObserveInputEdge(out bool baseline)
    {
        baseline = false;
        uint input = Pointer(0x7552A0);
        RequireTable(input, 0x6F5508);
        uint command = Pointer(input + 0x18);
        RequireTable(command, 0x703090);
        if (input != _cachedInput || command != _cachedCommand)
        {
            // A new command must pass the complete graph/owner capture before
            // it can become a source of extra input observations.
            ClearInputCache();
            return false;
        }
        byte[] fingerprint = EdgeFingerprint(Read(command, CommandBytes));
        baseline = _lastObservedInputFingerprint is null;
        bool changed = baseline || !_lastObservedInputFingerprint!.SequenceEqual(fingerprint);
        _lastObservedInputFingerprint = fingerprint;
        return changed;
    }

    private void Capture(PhaseState phase, uint thread, uint address, uint worldUpdate, string captureReason)
    {
        uint profile = Pointer(0x765AD4);
        RequireTable(profile, 0x6F5A44);
        uint record = Pointer(profile + 0x2B4);
        RequireTable(record, 0x710F80);
        uint root = Pointer(record + 0x24);
        RequireTable(root, 0x6DC4F4);
        uint scene = Pointer(root + 0x3C);
        RequireTable(scene, 0x6E7358);

        uint flow = Pointer(0x755294);
        uint state = U(flow + 0x1B0), pending = U(flow + 0x1B8);
        int stack = unchecked((int)U(flow + 0x1AC));
        if (state is < 1 or > 50 || pending != 0 || stack is < 0 or > 20 ||
            U(Pointer(flow + 0x15C + (uint)stack * 4) + 0x10) != state)
            throw new InvalidDataException("Stock playing-level flow is not ready.");

        uint head = Pointer(record + 0x1C), count = U(record + 0x20);
        if (count != 11) throw new InvalidDataException("Expected exactly eleven stock player components.");
        var objects = new List<object>();
        var byTable = new Dictionary<uint, List<uint>>();
        var links = new HashSet<uint>();
        var items = new HashSet<uint>();
        uint previous = head, link = Pointer(head);
        for (uint i = 0; i < count; ++i)
        {
            if (link == head || !links.Add(link) || U(link + 4) != previous)
                throw new InvalidDataException("Player component list is not reciprocal/distinct.");
            uint item = Pointer(link + 8), table = U(item);
            if (!items.Add(item)) throw new InvalidDataException("Player component alias encountered.");
            if (!byTable.TryGetValue(table, out List<uint>? values)) byTable[table] = values = [];
            values.Add(item);
            objects.Add(new { address = Hex(item), vtable = Hex(table) });
            uint next = Pointer(link);
            if (U(next + 4) != link) throw new InvalidDataException("Player component next/back link mismatch.");
            previous = link; link = next;
        }
        if (link != head || U(head + 4) != previous)
            throw new InvalidDataException("Player component count/sentinel mismatch.");
        uint Unique(uint table) => byTable.TryGetValue(table, out List<uint>? values) && values.Count == 1
            ? values[0] : throw new InvalidDataException($"Expected one local component {Hex(table)}.");
        uint character = Unique(0x6F6438), move = Unique(0x6F64F8), bloom = Unique(0x6FA288), animator = Unique(0x6F670C);
        foreach (uint entity in new[] { character, move, bloom, animator })
            if (U(entity + 0x24) != record)
                throw new InvalidDataException("Typed local component owner is not the stock player instance.");

        uint input = Pointer(0x7552A0);
        RequireTable(input, 0x6F5508);
        uint command = Pointer(input + 0x18);
        RequireTable(command, 0x703090);
        uint device = Pointer(input + 0x1C);
        RequireTable(device, 0x7116CC);
        uint deviceWord14 = U(device + 0x14);
        byte[] bytes = Read(command, CommandBytes);
        byte[] characterFields = Read(character + 0x124, 0x3C);
        uint CharacterWord(int offset) => BitConverter.ToUInt32(characterFields, offset - 0x124);
        bool relationshipsMatch = CharacterWord(0x130) == command && U(move + 0x160) == character &&
            CharacterWord(0x124) == bloom && CharacterWord(0x128) == animator &&
            CharacterWord(0x12C) == move && U(bloom + 0x124) == character && U(bloom + 0x12C) == command;
        if (!relationshipsMatch && captureReason != "periodic")
            throw new InvalidDataException("Typed command/controller relationships changed before an input edge capture.");

        bool baselineReset = phase.PreviousBytes is null || phase.PreviousCommand != command;
        byte[]? before = baselineReset ? null : phase.PreviousBytes;
        var byteDelta = new List<object>();
        var floatDelta = new List<object>();
        if (before is not null)
        {
            for (int offset = 0; offset < bytes.Length; ++offset)
                if (before[offset] != bytes[offset])
                {
                    phase.ChangedBytes.Add(offset);
                    byteDelta.Add(new { offset = Offset(offset), before = before[offset], after = bytes[offset] });
                }
            for (int offset = 0; offset <= bytes.Length - 4; offset += 4)
                if (BitConverter.ToUInt32(before, offset) != BitConverter.ToUInt32(bytes, offset))
                {
                    phase.ChangedFloats.Add(offset);
                    floatDelta.Add(new { offset = Offset(offset), before = FloatWord(before, offset), after = FloatWord(bytes, offset) });
                }
        }
        uint timer = Pointer(0x755298);
        RequireTable(timer, 0x702F94);
        byte[] timerBytes = Read(timer, 0x44);
        var rootObservation = Node(root);
        ++phase.Samples;
        if (captureReason == "periodic") ++phase.PeriodicSamples;
        else
        {
            ++phase.ExtraSamples;
            if (captureReason == "input-edge-baseline") ++_edgeBaselines;
            else ++_edgeCaptures;
        }
        if (relationshipsMatch) ++_relationshipsMatched;
        _log(new
        {
            @event = "control-observation", phase = address == InputReturn ? "UserInput.return" : "SceneManager.return",
            captureReason, samplingMode = "periodic-and-input-command-edges",
            sample = phase.Samples, phaseStop = phase.Stops, threadId = thread, processPid = _pid,
            stoppedAt = Hex(address), worldUpdate, diagnosticTick = System.Diagnostics.Stopwatch.GetTimestamp(),
            diagnosticTickFrequency = System.Diagnostics.Stopwatch.Frequency, baselineReset,
            profile = Hex(profile), playerRecord = Hex(record), actorScene = Hex(scene), levelState = state,
            componentCount = count, objects, character = Hex(character), moveController = Hex(move),
            bloom = Hex(bloom), animator = Hex(animator), input = Hex(input), command = Hex(command),
            inputDevice = new { address = Hex(device), vtable = Hex(U(device)),
                word14 = deviceWord14, word14Bits = Hex(deviceWord14),
                userInputWord14 = Hex(U(input + 0x14)) },
            characterOwnHex = Convert.ToHexString(characterFields),
            characterWords = Enumerable.Range(0, characterFields.Length / 4).Select(i => new
            { offset = Offset(0x124 + i * 4), bits = Hex(BitConverter.ToUInt32(characterFields, i * 4)) }).ToArray(),
            relationships = new
            {
                allMatched = relationshipsMatch,
                character130MatchesInput18 = CharacterWord(0x130) == command,
                move160MatchesCharacter = U(move + 0x160) == character,
                character124MatchesBloom = CharacterWord(0x124) == bloom,
                character128MatchesAnimator = CharacterWord(0x128) == animator,
                character12CMatchesMove = CharacterWord(0x12C) == move,
                bloom124MatchesCharacter = U(bloom + 0x124) == character,
                bloom12CMatchesCommand = U(bloom + 0x12C) == command
            },
            movePointers = new { at160 = Hex(U(move + 0x160)), at2B4 = Hex(U(move + 0x2B4)),
                at2B8 = Hex(U(move + 0x2B8)), at2BC = Hex(U(move + 0x2BC)) },
            bloomWords = new { at124 = Hex(U(bloom + 0x124)), at12C = Hex(U(bloom + 0x12C)),
                at130 = U(bloom + 0x130), at134 = U(bloom + 0x134), at13C = Hex(U(bloom + 0x13C)),
                at140 = Hex(U(bloom + 0x140)), at144 = Hex(U(bloom + 0x144)) },
            commandHex = Convert.ToHexString(bytes),
            commandBytes = bytes.Select((value, offset) => new { offset = Offset(offset), value,
                readByMoveCtrlAtKnownOffsets = offset is 0x4E or 0x4F or 0x50 or 0x51 or 0x70 }).ToArray(),
            alignedFloatWords = Enumerable.Range(0, bytes.Length / 4).Select(i => new
            { offset = Offset(i * 4), interpretation = FloatWord(bytes, i * 4) }).ToArray(),
            byteDelta, alignedFloatDelta = floatDelta, root = rootObservation,
            gameTimer = new { address = Hex(timer), at1C = U(timer + 0x1C),
                at28 = FloatWord(timerBytes, 0x28), at40 = timerBytes[0x40] }
        });
        phase.PreviousCommand = command;
        phase.PreviousBytes = bytes;
        if (!relationshipsMatch) ClearInputCache();
        else
        {
            if (_cachedInput != input || _cachedCommand != command)
            {
                _cachedInput = input; _cachedCommand = command;
                _lastObservedInputFingerprint = null;
            }
            if (address == InputReturn) _lastObservedInputFingerprint = EdgeFingerprint(bytes);
        }
    }

    private object Node(uint node)
    {
        RequireTable(node, 0x6DC4F4);
        byte[] bytes = Read(node, 0xB4);
        object[] Words(int offset, int count) => Enumerable.Range(0, count).Select(i => FloatWord(bytes, offset + i * 4)).ToArray();
        return new { address = Hex(node), parent = Hex(BitConverter.ToUInt32(bytes, 0x2C)),
            scene = Hex(BitConverter.ToUInt32(bytes, 0x3C)), flags = BitConverter.ToUInt32(bytes, 0xB0),
            localPosition = Words(0x20, 3), localScale = Words(0x30, 3), localOrientation = Words(0x40, 9),
            worldPosition = Words(0x74, 3), worldScale = Words(0x80, 3), worldOrientation = Words(0x8C, 9) };
    }

    private static object FloatWord(byte[] bytes, int offset)
    {
        float value = BitConverter.ToSingle(bytes, offset);
        bool finite = float.IsFinite(value);
        return new { bits = Hex(BitConverter.ToUInt32(bytes, offset)), finite, value = finite ? (float?)value : null };
    }

    private void RequireTable(uint address, uint table)
    {
        if (U(address) != table) throw new InvalidDataException($"Unexpected vtable at {Hex(address)}; expected {Hex(table)}.");
    }
    private uint Pointer(uint address)
    {
        uint value = U(address);
        if (value < 0x10000 || (value & 3) != 0) throw new InvalidDataException($"Invalid aligned borrowed pointer at {Hex(address)}.");
        return value;
    }
    private uint U(uint address) => BitConverter.ToUInt32(Read(address, 4));
    private byte[] Read(uint address, int count)
    {
        EnsureAtStop();
        if (address < 0x10000 || count is < 1 or > 0x1000 || (ulong)address + (uint)count > uint.MaxValue)
            throw new InvalidDataException("Control read exceeds its bounded x86 address range.");
        return Win32Native.ReadMemory(_process, address, count);
    }
    private void EnsureAtStop()
    {
        if (!_host.IsAtDebugStop || _host.ProcessHandle != _process || _host.ProcessPid != _pid ||
            _host.ImageBase != 0x400000 || Environment.CurrentManagedThreadId != _ownerThread)
            throw new InvalidOperationException("Control reads require the original owned game, handle and debug-stop thread.");
    }
    private static string Offset(int value) => $"+0x{value:X2}";
    private static string Hex(uint value) => $"0x{value:X8}";
}
