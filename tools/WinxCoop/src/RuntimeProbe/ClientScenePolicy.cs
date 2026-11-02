using System.Reflection.PortableExecutable;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>Finite native type and callback contract for the loaded Alfea client scene.</summary>
internal sealed class ClientScenePolicy
{
    private const uint Limit = 4096;
    private static readonly HashSet<uint> PresentationNotifications =
        [0x7FC114C3, 0x69F878BA, 0x7F2E3F1A, 0x401A393C, 0x65F4C9A8, 0xCD2B2876];
    private static readonly HashSet<uint> KnownEntityPredicates =
        [0x5A7DB0, 0x4FD880, 0x4F5000, 0x4F4070, 0x4FB510, 0x518DC0, 0x55CE30];
    private static readonly (uint Slot, string Name)[] HostOnlyServices =
    [
        (0x765BF8, "wxDebugManager"), (0x765BC0, "wxAIManager"),
        (0x765AF0, "wxTargetManager"), (0x765AF4, "wxOneLinerTimeTrigger"),
        (0x765AE4, "wxCinematicManager"), (0x765AE0, "wxSideQuestManager"),
        (0x765B00, "wxFairyStarsManager"), (0x765AD4, "wxPlayerProfile")
    ];
    internal sealed record NativeType(uint Table, uint ClassId, string Name, uint Notify, uint Predicate, uint FlagsCallback);
    private readonly DebuggerHost _host;
    private readonly PEReader _image;
    private readonly Action<object> _log;
    private readonly Dictionary<uint, NativeType> _types = [];
    private readonly Dictionary<uint, NativeType> _quarantine = [];
    private readonly List<object> _clockWitness = [], _controllerWitness = [];
    private uint _entityManager, _core, _animationManager, _input, _device, _keyboard;
    private uint _entityCount, _controllerCount, _clockCount;
    private uint _frameCallback30, _frameCallback34;
    private bool _captured;

    internal ClientScenePolicy(DebuggerHost host, PEReader image, Action<object> log)
    { _host = host; _image = image; _log = log; }

    internal object Summary => new
    {
        captured = _captured, supportedLevel = 28, entityCount = _entityCount,
        controllerCount = _controllerCount, clockCount = _clockCount,
        keyboard = Hex(_keyboard),
        quarantine = _quarantine.Values.OrderBy(t => t.Table).Select(Describe).ToArray(),
        initialTypes = _types.Values.OrderBy(t => t.Table).Select(Describe).ToArray(),
        clocks = _clockWitness.ToArray(), controllers = _controllerWitness.ToArray(),
        frameCallback30 = Hex(_frameCallback30), frameCallback34 = Hex(_frameCallback34),
        profileIsFinite = true, otherScenesQualified = false
    };

    internal void Capture()
    {
        Require(!_captured && _host.IsAtDebugStop, "Client scene policy capture requires its first owned stop.");
        _core = Pointer(0x755274); _entityManager = Pointer(0x765ACC);
        _input = Pointer(0x7552A0);
        _device = Pointer(_input + 0x1C);
        _keyboard = Pointer(_device + 0x38);
        Require(U(_device) == 0x7116CC && Read(_device + 0x20, 1)[0] == 0,
            "Client profile requires the input device targeting latch initially cleared.");
        Require(U(_entityManager) == 0x70069C, "Unexpected client entity manager.");
        var entityTypes = new Dictionary<uint, NativeType>();
        foreach (uint entity in List(Pointer(_entityManager + 0x18), U(_entityManager + 0x1C))
            .Concat(List(Pointer(_entityManager + 0x24), U(_entityManager + 0x28))))
        {
            Require(++_entityCount <= Limit, "Client scene entity budget exceeded.");
            NativeType type = Type(entity);
            entityTypes[type.Table] = type;
            if (type.Name.StartsWith("wx", StringComparison.Ordinal) && !PresentationNotifications.Contains(type.ClassId))
                _quarantine[type.Table] = type;
        }
        _log(new { @event = "client-scene-entity-types", entityCount = _entityCount,
            types = entityTypes.Values.OrderBy(t => t.Table).Select(Describe).ToArray() });
        foreach (NativeType type in entityTypes.Values)
        {
            // wxButton is a GUI state object: these slots are its state setter
            // and visual selector, rather than the ordinary wxEntity flag API.
            if (type.Table == 0x6FE988 && type.ClassId == 0x69F878BA &&
                type.FlagsCallback == 0x4358A0 && type.Predicate == 0x55CE30 && U(type.Table + 0x34) == 0x5A7DB0)
                continue;
            Require(type.FlagsCallback is 0x4DA930 or 0x4F4040 or 0x5B7A00,
                $"Unqualified client flags callback {type.Name}/{Hex(type.FlagsCallback)}.");
            Require(KnownEntityPredicates.Contains(type.Predicate),
                $"Unqualified client predicate {type.Name}/{Hex(type.Predicate)}.");
        }
        foreach ((uint slot, string name) in HostOnlyServices)
        {
            NativeType type = Type(Pointer(slot));
            Require(type.Name == name, $"Client service {Hex(slot)} is not {name}.");
            _quarantine[type.Table] = type;
        }
        Require(_quarantine.Count is > 0 and <= 64, "Client receiver policy exceeds its finite type budget.");
        _animationManager = Pointer(_core + 0x3C);
        Require(_animationManager == U(0x75F880) && Type(_animationManager).Name == "spAnimationManager",
            "Client animation manager is not the captured original service.");
        CaptureControllers();
        var seenClocks = new HashSet<uint>();
        CaptureClock(_core + 0x54, seenClocks, 0);
        CaptureClock(_core + 0x90, seenClocks, 0);
        CaptureClock(Pointer(0x755298), seenClocks, 0);
        _frameCallback30 = U(_core + 0x30); _frameCallback34 = U(_core + 0x34);
        _captured = true;
        _log(new { @event = "client-scene-policy-captured", policy = Summary });
        Require(_frameCallback30 == 0 && _frameCallback34 == 0,
            "Client scene has an unqualified raw render callback.");
    }

    internal void VerifyStableFrame()
    {
        Require(_captured && U(0x755274) == _core && U(0x765ACC) == _entityManager &&
            U(_core + 0x3C) == _animationManager && U(0x75F880) == _animationManager &&
            U(0x7552A0) == _input && U(_input + 0x1C) == _device &&
            U(_device + 0x38) == _keyboard &&
            U(_device) == 0x7116CC && Read(_device + 0x20, 1)[0] == 0 &&
            U(_core + 0x30) == _frameCallback30 && U(_core + 0x34) == _frameCallback34,
            "Client presentation service or raw callback identity changed.");
    }

    internal void VerifyCallbackGraphs()
    {
        VerifyStableFrame();
        _controllerWitness.Clear(); _clockWitness.Clear();
        CaptureControllers();
        var visited = new HashSet<uint>();
        CaptureClock(_core + 0x54, visited, 0);
        CaptureClock(_core + 0x90, visited, 0);
        CaptureClock(Pointer(0x755298), visited, 0);
    }

    internal void Configure(ClientNativeGate gate, bool diagnosticGuestPadInput = false,
        bool worldBattlePresentation = false)
    {
        Require(_captured, "Client scene policy has not been captured.");
        Require(!worldBattlePresentation || diagnosticGuestPadInput,
            "Battle presentation raw ordinary input requires the separate diagnostic Guest pad policy.");
        foreach (NativeType type in _quarantine.Values.OrderBy(t => t.Table))
            gate.AddRecipientQuarantine(type.Name, type.Table, type.ClassId, type.Notify, [0x1C, 0x1D]);
        AddRegion(gate, "actor-loop-callback-a", 0x5A2632, "5653FFD083C408", 0x5A2639);
        AddRegion(gate, "actor-loop-callback-b", 0x5A2843, "5653FFD083C408", 0x5A284A);
        AddRegion(gate, "animator-completion-forward", 0x4FB7B8, "57E8A241F1FF", 0x4FB7BE);
        AddCall(gate, "core-physics", 0x41CDC9, 0x45A0C0);
        AddRegion(gate, "core-particles", 0x41CE14, "8B108BC8FF522C", 0x41CE1B);
        AddVirtualRegion(gate, "debug-manager", 0x576374, 0x1C);
        AddVirtualRegion(gate, "target-manager", 0x576463, 0x1C);
        AddVirtualRegion(gate, "one-liner-timer", 0x57647D, 0x24);
        AddVirtualRegion(gate, "cinematic-manager", 0x576497, 0x1C);
        AddVirtualRegion(gate, "side-quest-manager", 0x5764B1, 0x1C);
        AddCall(gate, "fairy-stars", 0x5764C9, 0x4F1220);
        AddCall(gate, "profile-update", 0x5764E3, 0x4E6680);
        if (diagnosticGuestPadInput)
        {
            // Native logical9/1E queries only write command20 (logical9 also
            // writes companion2A). The E5 Battle opt-in keeps those original
            // raw producers; it still skips charged21/shield27 below. No wx
            // action is executed here, and E2's recipient/AI/physics/Actor-tag
            // quarantine remains responsible for suppressing consequences.
            if (worldBattlePresentation)
                gate.AddForwardRedirect("world-battle-pad-after-ordinary", 0x604B9F, Convert.FromHexString("8B4E3C6A1D"),
                    0x604BC1, Convert.FromHexString("8B4E3C6A07"));
            else
                gate.AddForwardRedirect("diagnostic-pad-after-jump", 0x604B7A, Convert.FromHexString("8B4E3C6A09"),
                    0x604BC1, Convert.FromHexString("8B4E3C6A07"));
            gate.AddForwardRedirect("diagnostic-pad-after-confirm", 0x604BD2, Convert.FromHexString("8B4E3C6A10"),
                0x604D75, Convert.FromHexString("8B4E3C6A26"));
            gate.AddForwardRedirect("diagnostic-pad-after-pause", 0x604DA8, Convert.FromHexString("8B4E3C6A15"),
                0x604E0C, Convert.FromHexString("5F5E5BC20400"));
        }
        else
            AddRegion(gate, "input-direct-interaction", 0x604EB7, "578BCEE841FCFFFF", 0x604EBF);
        // Keep the helper's native logical6 pause-command tail. Its preceding
        // cheat paths are skipped while the native prologue, argument load and
        // four-register epilogue retain their original stack relationship.
        gate.AddForwardRedirect("input-cheat-prefix", 0x60437B, Convert.FromHexString("8D50018BFF"),
            0x6044AA, Convert.FromHexString("8B742414"));
        gate.AddForwardRedirect("input-cheat-body", 0x6044AE, Convert.FromHexString("74698B4D38"),
            0x604AAA, Convert.FromHexString("8B4D386A06"));
        AddCall(gate, "input-target-action-a", 0x6041AA, 0x59D820, 8);
        AddCall(gate, "input-target-action-b", 0x60424F, 0x59D820, 8);
        AddCall(gate, "input-target-query", 0x60428E, 0x59D5A0, 8);
        AddCall(gate, "input-target-lock-a", 0x604147, 0x5A62B0, 4);
        AddCall(gate, "input-target-lock-b", 0x604297, 0x5A62B0, 4);
        AddCall(gate, "playing-level-world-effects", 0x5F77C7, 0x5F74B0);
    }

    private void AddVirtualRegion(ClientNativeGate gate, string name, uint site, byte slot)
    {
        // MOV EDX,[EAX]; MOV ECX,EAX; CALL [EDX+slot]. The next singleton load
        // remains original, so the unused callee return is overwritten normally.
        byte[] expected = [0x8B, 0x10, 0x8B, 0xC8, 0xFF, 0x52, slot];
        uint start = checked(site - 4);
        Require(Image(start, expected.Length).SequenceEqual(expected),
            $"Unexpected client virtual call at {Hex(site)}.");
        gate.AddSkipRegion(name, start, expected, site + 3, 0);
    }

    private void AddCall(ClientNativeGate gate, string name, uint site, uint target, uint argumentBytes = 0)
    {
        byte[] bytes = Image(site, 5);
        Require(bytes[0] == 0xE8 && unchecked((uint)((long)site + 5 + BitConverter.ToInt32(bytes, 1))) == target,
            $"Client policy call target mismatch for {name}.");
        gate.AddSkipCall(name, site, bytes, target, site + 5, argumentBytes);
    }
    private static void AddRegion(ClientNativeGate gate, string name, uint site, string hex, uint next) =>
        gate.AddSkipRegion(name, site, Convert.FromHexString(hex), next, 0);

    private void CaptureControllers()
    {
        uint item = U(_animationManager + 0x24), previous = 0;
        var seen = new HashSet<uint>();
        while (item != 0)
        {
            Require(item >= 0x10000 && seen.Add(item) && seen.Count <= Limit && U(item + 0x18) == previous,
                "Client animation controller list is not finite/reciprocal.");
            NativeType type = Type(item);
            uint update = U(type.Table + 0x1C);
            Require((type.Table == 0x6DE8CC && type.ClassId == 0x1C0053D6 &&
                type.Name == "spUVController" && update == 0x423190) ||
                (type.Table == 0x703F80 && type.ClassId == 0x19D676E6 &&
                type.Name == "spActor" && update == 0x5A2380),
                $"Unqualified physical client controller {type.Name}/{Hex(type.Table)}/{Hex(update)}.");
            _controllerWitness.Add(new { address = Hex(item), type = Describe(type),
                update = Hex(update), enabled = Read(item + 0x10, 1)[0] });
            previous = item; item = U(item + 0x14);
        }
        Require(U(_animationManager + 0x28) == previous, "Client animation tail differs from its complete list.");
        _controllerCount = checked((uint)seen.Count);
    }

    private void CaptureClock(uint timer, HashSet<uint> visited, uint depth, HashSet<uint>? ancestors = null)
    {
        ancestors ??= [];
        Require(!ancestors.Contains(timer), "Client task clock graph is cyclic.");
        if (visited.Contains(timer)) return;
        Require(timer >= 0x10000 && depth <= 128 && visited.Add(timer) && visited.Count <= Limit,
            "Client task clock graph exceeds its finite bound.");
        ancestors.Add(timer);
        NativeType type = Type(timer);
        Require((type.ClassId is 0x1ACD36E2 or 0x78665312) && U(type.Table + 0x1C) == 0x450750,
            "Client clock has an unqualified callback rather than the native clock-only update.");
        uint count = U(timer + 0x38), child = U(timer + 0x30), previous = 0;
        Require(count <= Limit, "Client clock child count exceeds its bound.");
        _clockWitness.Add(new { address = Hex(timer), type = Describe(type), children = count, update = Hex(U(type.Table + 0x1C)) });
        for (uint index = 0; index < count; ++index)
        {
            Require(child >= 0x10000 && U(child + 0x10) == previous, "Client clock sibling relation changed.");
            CaptureClock(child, visited, depth + 1, ancestors);
            previous = child; child = U(child + 0x14);
        }
        Require(child == 0 && U(timer + 0x34) == previous, "Client clock count/tail mismatch.");
        ancestors.Remove(timer);
        _clockCount = checked((uint)visited.Count);
    }

    private NativeType Type(uint instance)
    {
        uint table = Pointer(instance);
        if (_types.TryGetValue(table, out NativeType? old)) return old;
        Require(_types.Count <= 256, "Client policy type budget exceeded.");
        uint getter = U(table + 0x10); byte[] bytes = Image(getter, 6);
        Require(bytes[0] == 0xB8 && bytes[5] == 0xC3 && Read(getter, 6).SequenceEqual(bytes),
            "Client policy RTTI getter is not the pinned literal accessor.");
        uint record = BitConverter.ToUInt32(bytes, 1);
        string name = Win32Native.ReadAnsiString(_host.ProcessHandle, record + 8, 64);
        Require(name.Length is > 0 and < 64 && name.All(c => c is >= ' ' and <= '~'), "Client policy type name is invalid.");
        var type = new NativeType(table, U(record), name, U(table + 4), U(table + 0x2C), U(table + 0x28));
        _types.Add(table, type); return type;
    }

    private uint[] List(uint head, uint count)
    {
        Require(count <= Limit, "Client entity list exceeds its finite bound.");
        uint link = Pointer(head), previous = head; var seen = new HashSet<uint>(); var result = new List<uint>();
        for (uint i = 0; i < count; ++i)
        {
            Require(link != head && seen.Add(link) && U(link + 4) == previous, "Client entity list is not reciprocal/distinct.");
            result.Add(Pointer(link + 8)); previous = link; link = Pointer(link);
        }
        Require(link == head && U(head + 4) == previous, "Client entity list count/sentinel mismatch.");
        return result.ToArray();
    }

    private static object Describe(NativeType type) => new { table = Hex(type.Table), classId = Hex(type.ClassId),
        type.Name, slot04Raw = Hex(type.Notify), slot2CRaw = Hex(type.Predicate), slot28Raw = Hex(type.FlagsCallback) };
    private byte[] Image(uint address, int bytes)
    {
        Require(address is >= 0x401000 and < 0x740000 && bytes is > 0 and <= 64, "Client policy image address is outside its pin.");
        return _image.GetSectionData(checked((int)(address - 0x400000))).GetContent(0, bytes).ToArray();
    }
    private uint Pointer(uint at) { uint value = U(at); Require(value >= 0x10000, "Client policy null native pointer."); return value; }
    private uint U(uint at) => BitConverter.ToUInt32(Read(at, 4));
    private byte[] Read(uint at, int bytes)
    {
        Require(_host.IsAtDebugStop && at >= 0x10000 && bytes is > 0 and <= 65536, "Client policy read requires the owned debug stop.");
        return Win32Native.ReadMemory(_host.ProcessHandle, at, bytes);
    }
    private static string Hex(uint value) => $"0x{value:X8}";
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidDataException(message); }
}
