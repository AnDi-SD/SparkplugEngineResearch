using System.ComponentModel;
using System.Reflection.PortableExecutable;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// Read-only census of a short, already loaded original gameplay interval.
/// It observes dispatch and its known bypasses; it does not create a client role.
/// </summary>
internal sealed class ClientDispatchProbe
{
    private const uint WorldLimit = 8, StopLimit = 50000, InventoryLimit = 4096;
    private const int BucketLimit = 1024, SampleLimit = 48, NestingLimit = 64;
    private readonly DebuggerHost _host;
    private readonly PEReader _image;
    private readonly Action<object> _log;
    private readonly IntPtr _process;
    private readonly uint _pid;
    private readonly int _ownerThread;
    private readonly bool _suppressSelectedGameplay;
    private ClientAuthorityGate? _authorityGate;
    private readonly Dictionary<uint, EntityInfo> _inventory = [];
    private readonly Dictionary<uint, uint?> _classIds = [];
    private readonly Dictionary<DispatchKey, uint> _dispatch = [];
    private readonly Dictionary<(uint Table, uint Code), uint> _animatorMessages = [];
    private readonly Dictionary<string, uint> _counts = [];
    private readonly Stack<DispatchFrame> _pending = [];
    private readonly List<object> _samples = [], _frames = [];
    private readonly HashSet<uint> _retired = [];
    private uint _manager, _aiManager, _flow, _active, _profile, _record, _root;
    private uint _engine, _scene, _camera, _input, _command, _device;
    private uint _character, _move, _machine, _animator, _intelliCam;
    private uint _startWorld, _currentWorld, _worlds, _stops, _entries, _returns, _deliveries;
    private uint _predictionMismatches, _maximumNesting;
    private bool _started, _completed;

    private sealed record EntityInfo(uint Address, uint Table, uint? ClassId,
        uint Owner24Raw, uint Node18Raw, uint NotifyEntry, uint PredicateEntry, string Role);
    private readonly record struct DispatchKey(uint Table, uint? ClassId, uint Code,
        byte Predicate, byte ForceArgument, byte ForceManager, bool Delivered, bool Withheld);
    private sealed class DispatchFrame(uint esp, uint caller, uint manager, uint entity,
        uint message, uint code, EntityInfo info, byte predicate, byte forceArgument, byte forceManager)
    {
        internal readonly uint Esp = esp, Caller = caller, Manager = manager, Entity = entity, Message = message, Code = code;
        internal readonly EntityInfo Info = info;
        internal readonly byte Predicate = predicate, ForceArgument = forceArgument, ForceManager = forceManager;
        internal bool Delivered;
        internal bool Withheld;
        internal bool ExpectedDelivery => Predicate != 0 || ForceArgument != 0 || ForceManager != 0;
    }

    internal ClientDispatchProbe(DebuggerHost host, PEReader image, Action<object> log, bool suppressSelectedGameplay = false)
    {
        Require(host.IsAtDebugStop && host.ProcessHandle != IntPtr.Zero && host.ProcessPid != 0 &&
            host.ImageBase == 0x400000 && host.MainThreadId != 0, "Client dispatch observer requires the pinned owned initial debug stop.");
        _host = host; _image = image; _log = log;
        _process = host.ProcessHandle; _pid = host.ProcessPid;
        _ownerThread = Environment.CurrentManagedThreadId;
        _suppressSelectedGameplay = suppressSelectedGameplay;
    }

    internal bool ModeCompleted => _completed;
    internal bool ModeSucceeded => _completed && _worlds == WorldLimit && _pending.Count == 0 &&
        _entries > 0 && _entries == _returns && _deliveries > 0 && _predictionMismatches == 0 &&
        (_authorityGate is null ? Count("ai-update") > 0 : Count("ai-update") == 0 && _authorityGate.ModeSucceeded) &&
        Count("input-return") > 0 && Count("physics-update") > 0 &&
        Count("animation-update") > 0 && Count("camera-notify") > 0;
    internal object Summary => new
    {
        readOnly = !_suppressSelectedGameplay, started = _started, completed = _completed, succeeded = ModeSucceeded,
        nativeWorlds = _worlds, requiredNativeWorlds = WorldLimit,
        callbacks = _stops, callbackLimit = StopLimit, maximumNesting = _maximumNesting, nestingLimit = NestingLimit,
        typedEntries = _entries, typedReturns = _returns, actualDeliveries = _deliveries,
        typedDeliveryHookCountsPreCallCandidates = true, actualDeliveriesExcludeWithheld = true,
        nativeBranchPredictionMismatches = _predictionMismatches, pendingTypedFrames = _pending.Count,
        manager = Hex(_manager), localCharacter = Hex(_character), localMove = Hex(_move),
        localMachine = Hex(_machine), localAnimator = Hex(_animator), localIntelliCam = Hex(_intelliCam),
        counts = _counts.OrderBy(pair => pair.Key).ToDictionary(pair => pair.Key, pair => pair.Value),
        typedHistogram = _dispatch.OrderBy(pair => pair.Key.Table).ThenBy(pair => pair.Key.Code).Select(pair => new
        {
            table = Hex(pair.Key.Table), classId = pair.Key.ClassId is uint id ? Hex(id) : null,
            code = Hex(pair.Key.Code), predicateLowByte = pair.Key.Predicate,
            forceArgumentLowByte = pair.Key.ForceArgument, forceManagerByte55F4 = pair.Key.ForceManager,
            delivered = pair.Key.Delivered, withheld = pair.Key.Withheld, count = pair.Value
        }).ToArray(),
        animatorHistogram = _animatorMessages.OrderBy(pair => pair.Key.Table).ThenBy(pair => pair.Key.Code)
            .Select(pair => new { table = Hex(pair.Key.Table), code = Hex(pair.Key.Code), count = pair.Value }).ToArray(),
        inventory = _inventory.Values.OrderBy(value => value.Address).Select(Describe).ToArray(),
        samples = _samples.ToArray(), frames = _frames.ToArray(),
        retiredWhenNextHit = _retired.Order().Select(Hex).ToArray(),
        unhitSubscriptionsEndWithOwnedProcess = true,
        nativeCallsInvoked = 0, gameDataWrites = 0,
        selectedAuthorityGate = _authorityGate?.Summary,
        suppressionImplemented = _suppressSelectedGameplay, fullClientAuthorityClaimed = false,
        zeroBypassCountDoesNotEstablishAbsence = true, performanceBenchmarkClaimed = false
    };

    /// <summary>Called by ActorObserver only at its verified original45A802 WorldRET.</summary>
    internal void ObserveWorld(uint thread, uint world, bool mediaVerified)
    {
        EnsureStop(thread);
        _currentWorld = world;
        if (_completed) return;
        if (!_started)
        {
            if (!mediaVerified || !ReadyAlfea()) return;
            CaptureOwners();
            _startWorld = world; _started = true;
            if (_suppressSelectedGameplay) _authorityGate = new ClientAuthorityGate(_host, _image, _log);
            InstallWitnesses();
            _authorityGate?.Install();
            _log(new { @event = "client-dispatch-census-begin", world,
                worldLimit = WorldLimit, callbackLimit = StopLimit, nestingLimit = NestingLimit,
                localCharacter = Hex(_character), localIntelliCam = Hex(_intelliCam), manager = Hex(_manager),
                inventory = _inventory.Values.OrderBy(value => value.Address).Select(Describe).ToArray(),
                gameDataWrites = 0, selectedSuppressionEnabled = _suppressSelectedGameplay,
                fullClientAuthorityClaimed = false });
            return;
        }
        Require(world > _startWorld && _pending.Count == 0, "Client census reached a world with unfinished typed native calls.");
        VerifyOwners();
        Require(++_worlds <= WorldLimit, "Client dispatch census exceeded its native world bound.");
        object frame = new { world, ordinal = _worlds, typedEntries = _entries, typedReturns = _returns,
            actualDeliveries = _deliveries, nativeBranchPredictionMismatches = _predictionMismatches,
            rootWorldPosition = FloatWords(_root + 0x74, 3),
            commandWords = Convert.ToHexString(Read(_command + 4, 0x18)),
            counts = _counts.OrderBy(pair => pair.Key).ToDictionary(pair => pair.Key, pair => pair.Value) };
        _frames.Add(frame);
        _log(new { @event = "client-dispatch-census-world", observation = frame });
        if (_worlds != WorldLimit) return;
        _completed = true;
        _log(new { @event = "client-dispatch-census-complete", succeeded = ModeSucceeded,
            nativeWorlds = _worlds, typedEntries = _entries, typedReturns = _returns,
            actualDeliveries = _deliveries, nativeBranchPredictionMismatches = _predictionMismatches,
            nativeCallsInvoked = 0, gameDataWrites = 0, suppressionImplemented = _suppressSelectedGameplay });
    }

    private bool ReadyAlfea()
    {
        uint flow = TryU(0x755294), profile = TryU(0x765AD4), record = TryU(profile + 0x2B4);
        if (flow < 0x10000 || profile < 0x10000 || record < 0x10000 ||
            TryU(flow + 0x1B0) != 28 || TryU(flow + 0x1B8) != 0 ||
            TryU(profile) != 0x6F5A44 || TryU(record) != 0x710F80) return false;
        uint stack = TryU(flow + 0x1AC);
        if (stack > 20) return false;
        uint active = TryU(flow + 0x15C + stack * 4), root = TryU(record + 0x24);
        uint input = TryU(0x7552A0), device = TryU(input + 0x1C);
        return TryU(active) == 0x710874 && TryU(active + 0x10) == 28 &&
            TryU(root) == 0x6DC4F4 && TryU(TryU(root + 0x3C)) == 0x6E7358 &&
            TryU(input) == 0x6F5508 && TryU(device) == 0x7116CC && TryU(device + 0x14) == 3 &&
            TryU(TryU(input + 0x18)) == 0x703090 && TryU(TryU(0x75DB90) + 0x20) == 0;
    }

    private void CaptureOwners()
    {
        _manager = Pointer(0x765ACC); Table(_manager, 0x70069C);
        _aiManager = Pointer(0x765BC0); Table(_aiManager, 0x706640);
        _flow = Pointer(0x755294); uint stack = U(_flow + 0x1AC);
        Require(stack <= 20, "Client census flow stack exceeds its bound.");
        _active = Pointer(_flow + 0x15C + stack * 4);
        _profile = Pointer(0x765AD4); _record = Pointer(_profile + 0x2B4); _root = Pointer(_record + 0x24);
        _engine = Pointer(0x755274); _scene = Pointer(_engine + 0x18); _camera = Pointer(_engine + 0x1C);
        _input = Pointer(0x7552A0); _command = Pointer(_input + 0x18); _device = Pointer(_input + 0x1C);
        uint[] local = ListItems(Pointer(_record + 0x1C), U(_record + 0x20), 11);
        uint Unique(uint table) => local.Single(item => U(item) == table);
        _character = Unique(0x6F6438); _move = Unique(0x6F64F8); _machine = Unique(0x6FA288);
        _animator = Unique(0x6F670C); _intelliCam = Unique(0x6F522C);
        foreach (uint item in new[] { _character, _move, _machine, _animator, _intelliCam })
            Require(U(item + 0x24) == _record, "Client census local component is not owned by the local instance.");
        VerifyOwners();
        foreach (uint item in ListItems(Pointer(_manager + 0x18), U(_manager + 0x1C), InventoryLimit)
            .Concat(ListItems(Pointer(_manager + 0x24), U(_manager + 0x28), InventoryLimit))) _ = Entity(item);
        foreach (uint item in local.Where(item => U(item) != 0x6DC4F4)) _ = Entity(item);
    }

    private void VerifyOwners()
    {
        Require(ReadyAlfea() && U(0x765ACC) == _manager && U(0x765BC0) == _aiManager &&
            U(0x755294) == _flow && U(0x765AD4) == _profile && U(_profile + 0x2B4) == _record &&
            U(_record + 0x24) == _root && U(_root + 0x3C) == _scene && U(0x755274) == _engine &&
            U(_engine + 0x18) == _scene && U(_engine + 0x1C) == _camera &&
            U(0x7552A0) == _input && U(_input + 0x18) == _command && U(_input + 0x1C) == _device,
            "Client census left its captured ready scene/profile/input/camera owners.");
        Table(_manager, 0x70069C); Table(_aiManager, 0x706640);
        Table(_character, 0x6F6438); Table(_move, 0x6F64F8); Table(_machine, 0x6FA288);
        Table(_animator, 0x6F670C); Table(_intelliCam, 0x6F522C);
    }

    private void InstallWitnesses()
    {
        Hook(0x573D50, 6, "typed-entry", (ref Win32Native.X86Context context) =>
        {
            Require(context.Ecx == _manager && _pending.Count < NestingLimit, "Typed census manager or nesting mismatch.");
            uint message = Pointer(context.Esp + 4), entity = Pointer(context.Esp + 8);
            var frame = new DispatchFrame(context.Esp, U(context.Esp), context.Ecx, entity,
                message, U(message), Entity(entity), Read(context.Esp + 0xC, 1)[0],
                Read(context.Esp + 0x10, 1)[0], Read(_manager + 0x55F4, 1)[0]);
            Require(frame.Caller is >= 0x401000 and < 0x6DA000, "Typed census return address is outside the pinned native code.");
            _pending.Push(frame); ++_entries;
            _maximumNesting = Math.Max(_maximumNesting, checked((uint)_pending.Count));
        });
        Hook(0x573D9D, 6, "typed-delivery", (ref Win32Native.X86Context context) =>
        {
            Require(_pending.Count > 0, "Typed actual delivery has no paired entry.");
            DispatchFrame frame = _pending.Peek();
            Require(!frame.Delivered && !frame.Withheld && context.Esp == checked(frame.Esp - 20) &&
                context.Esi == frame.Manager && context.Ecx == frame.Entity && context.Edx == frame.Message &&
                U(context.Esp + 0x18) == frame.Message, "Typed actual delivery differs from its exact entry/stack/receiver/message pair.");
            frame.Withheld = _authorityGate?.TryWithholdTyped(frame.Code, frame.Info.Table, frame.Info.ClassId, ref context) == true;
            if (!frame.Withheld) { frame.Delivered = true; ++_deliveries; }
        });
        Hook(0x573DCE, 3, "typed-return", (ref Win32Native.X86Context context) =>
        {
            Require(_pending.Count > 0, "Typed native return has no paired entry.");
            DispatchFrame frame = _pending.Pop();
            Require(context.Esp == frame.Esp && U(context.Esp) == frame.Caller,
                "Typed RET10 differs from its exact entry ESP/caller pair.");
            ++_returns;
            Require(!(frame.Delivered && frame.Withheld), "A typed call cannot be both forwarded and withheld.");
            if ((frame.Delivered || frame.Withheld) != frame.ExpectedDelivery) ++_predictionMismatches;
            var key = new DispatchKey(frame.Info.Table, frame.Info.ClassId, frame.Code,
                frame.Predicate, frame.ForceArgument, frame.ForceManager, frame.Delivered, frame.Withheld);
            Require(_dispatch.Count < BucketLimit || _dispatch.ContainsKey(key), "Typed census histogram exceeds its bound.");
            _dispatch[key] = _dispatch.GetValueOrDefault(key) + 1;
            Sample("typed", new { entity = Hex(frame.Entity), table = Hex(frame.Info.Table), role = frame.Info.Role,
                code = Hex(frame.Code), predicate = frame.Predicate, forceArgument = frame.ForceArgument,
                forceManager = frame.ForceManager, delivered = frame.Delivered, withheld = frame.Withheld,
                expectedNativeDelivery = frame.ExpectedDelivery,
                entryEsp = Hex(frame.Esp), returnEsp = Hex(context.Esp), caller = Hex(frame.Caller) });
        });
        Hook(0x574238, 9, "pending-flags-callback", (ref Win32Native.X86Context context) =>
        {
            Require(context.Esi == _manager && context.Ecx >= 0x10000 &&
                context.Edi >= _manager + 0x52D4 && context.Edi < _manager + 0x55F4 &&
                (context.Edi - (_manager + 0x52D4)) % 4 == 0 && U(context.Edi) == context.Ecx,
                "Pending +28 callback receiver is not the captured bounded manager array item.");
            EntityInfo entity = Entity(context.Ecx);
            Sample("pending-flags-callback", new { entity = Hex(entity.Address), table = Hex(entity.Table),
                role = entity.Role, referencePoint = Hex(_manager + 0x2C), notifyMessageClaimed = false });
        });
        Hook(0x574409, 9, "secondary-notify", (ref Win32Native.X86Context context) =>
        {
            Require(context.Esi == _manager && context.Edi >= 0x10000 && context.Eax >= 0x10000,
                "Secondary-list Notify bypass lacks the captured manager/link/message.");
            EntityInfo entity = Entity(Pointer(context.Edi + 8));
            Sample("secondary-notify", new { entity = Hex(entity.Address), table = Hex(entity.Table),
                role = entity.Role, code = Hex(U(context.Eax)), typedPredicateApplied = false });
        });
        Hook(0x5A5CF0, 6, "ai-update", (ref Win32Native.X86Context context) =>
            Require(context.Ecx == _aiManager, "Independent AI update receiver is not the captured singleton."));
        Hook(0x4FB780, 6, "animator-notify", (ref Win32Native.X86Context context) =>
        {
            uint code = U(Pointer(context.Esp + 4)); EntityInfo entity = Entity(context.Ecx);
            Require(entity.Table == 0x6F670C, "Animator Notify receiver is not the qualified controller type.");
            var key = (entity.Table, code);
            Require(_animatorMessages.Count < BucketLimit || _animatorMessages.ContainsKey(key), "Animator census histogram exceeds its bound.");
            _animatorMessages[key] = _animatorMessages.GetValueOrDefault(key) + 1;
            Sample("animator-notify", new { entity = Hex(entity.Address), code = Hex(code), role = entity.Role });
        });
        Hook(0x4FB7F2, 3, "animation-state-tag", (ref Win32Native.X86Context context) =>
        {
            Require(context.Esi >= 0x10000 && U(context.Esi) == 0x6F670C && context.Edi >= 0x10000 &&
                context.Ecx == context.Eax && U(context.Ecx) == context.Edx && U(context.Esp) == context.Edi,
                "Animation tag-to-state witness has an unexpected native call ABI.");
            Sample("animation-state-tag", new { animator = Hex(context.Esi), state = Hex(context.Ecx),
                stateTable = Hex(context.Edx), message = Hex(context.Edi), code = Hex(U(context.Edi)) });
            _authorityGate?.WithholdAnimationTag(ref context);
        });
        Hook(0x4E1530, 8, "camera-notify", (ref Win32Native.X86Context context) =>
        {
            EntityInfo entity = Entity(context.Ecx);
            Require(entity.Table == 0x6F522C && entity.ClassId == 0x7FC114C3,
                "Camera Notify receiver is not the pinned IntelliCam type.");
            Sample("camera-notify", new { entity = Hex(entity.Address), code = Hex(U(Pointer(context.Esp + 4))), local = entity.Address == _intelliCam });
        });
        Hook(0x4E2D3E, 1, "input-return", (ref Win32Native.X86Context context) => { });
        Hook(0x45A0C0, 6, "physics-update", (ref Win32Native.X86Context context) => { });
        Hook(0x4535A0, 6, "animation-update", (ref Win32Native.X86Context context) => { });
    }

    private delegate void ContextObservation(ref Win32Native.X86Context context);
    private void Hook(uint address, int bytes, string name, ContextObservation observe)
    {
        byte[] expected = _image.GetSectionData(checked((int)(address - 0x400000))).GetContent(0, bytes).ToArray();
        Require(Read(address, bytes).SequenceEqual(expected), $"Client census signature mismatch at {Hex(address)}.");
        if (address == 0x573DCE) Require(expected.SequenceEqual(new byte[] { 0xC2, 0x10, 0 }), "Typed return is not RET10.");
        _log(new { @event = "client-dispatch-signature", name, address = Hex(address), bytes = Convert.ToHexString(expected) });
        BreakpointCallback repeat = null!;
        repeat = (uint thread, uint at, ref Win32Native.X86Context context) =>
        {
            EnsureStop(thread);
            Require(at == address && context.Eip == address && Read(address, bytes).SequenceEqual(expected),
                "Client census callback address/signature changed.");
            if (_completed) { _retired.Add(address); return; }
            Require(_started && ++_stops <= StopLimit, "Client census callback budget exceeded.");
            _counts[name] = Count(name) + 1;
            observe(ref context);
            _host.Breakpoints.AddReturn(address, thread, "ClientDispatch." + name, repeat);
        };
        _host.Breakpoints.AddReturn(address, _host.MainThreadId, "ClientDispatch." + name, repeat);
    }

    private EntityInfo Entity(uint address)
    {
        uint table = Pointer(address);
        if (_inventory.TryGetValue(address, out EntityInfo? old))
        {
            Require(old.Table == table, "An observed entity address changed native type during the census.");
            return old;
        }
        Require(_inventory.Count < InventoryLimit, "Client census entity inventory exceeds its bound.");
        byte[] prefix = Read(address, 0x40);
        string role = address == _character ? "local-character" : address == _move ? "local-move" :
            address == _machine ? "local-bloom-fsm" : address == _animator ? "local-animator" :
            address == _intelliCam ? "local-intelli-cam" : "other-native-object";
        var info = new EntityInfo(address, table, ClassId(table), BitConverter.ToUInt32(prefix, 0x24),
            BitConverter.ToUInt32(prefix, 0x18), U(table + 4), U(table + 0x2C), role);
        _inventory.Add(address, info);
        return info;
    }

    private uint? ClassId(uint table)
    {
        if (_classIds.TryGetValue(table, out uint? found)) return found;
        uint? result = null; uint getter = U(table + 0x10);
        if (getter is >= 0x401000 and < 0x6DA000)
        {
            byte[] expected = _image.GetSectionData(checked((int)(getter - 0x400000))).GetContent(0, 6).ToArray();
            if (expected[0] == 0xB8 && expected[5] == 0xC3 && Read(getter, 6).SequenceEqual(expected))
                result = U(BitConverter.ToUInt32(expected, 1));
        }
        _classIds.Add(table, result);
        return result;
    }

    private uint[] ListItems(uint head, uint count, uint maximum)
    {
        Require(count <= maximum && (maximum != 11 || count == 11), "Client census native list count exceeds its bound.");
        uint link = Pointer(head), previous = head;
        var seen = new HashSet<uint>(); var values = new List<uint>();
        for (uint index = 0; index < count; ++index)
        {
            Require(link != head && seen.Add(link) && U(link + 4) == previous,
                "Client census native list is not reciprocal/distinct.");
            values.Add(Pointer(link + 8)); previous = link; link = Pointer(link);
        }
        Require(link == head && U(head + 4) == previous, "Client census native list count/sentinel mismatch.");
        return values.ToArray();
    }

    private void Sample(string kind, object data)
    {
        if (_samples.Count < SampleLimit) _samples.Add(new { kind, world = _currentWorld, callback = _stops, data });
    }
    private static object Describe(EntityInfo value) => new { address = Hex(value.Address), table = Hex(value.Table),
        classId = value.ClassId is uint id ? Hex(id) : null, owner24Raw = Hex(value.Owner24Raw), node18Raw = Hex(value.Node18Raw),
        notifyEntry = Hex(value.NotifyEntry), predicateEntry = Hex(value.PredicateEntry), role = value.Role };
    private object[] FloatWords(uint address, int count) => Enumerable.Range(0, count).Select(index =>
    {
        byte[] bytes = Read(checked(address + (uint)index * 4), 4); float value = BitConverter.ToSingle(bytes);
        return (object)new { bits = Hex(BitConverter.ToUInt32(bytes)), value = float.IsFinite(value) ? (float?)value : null };
    }).ToArray();
    private uint Count(string name) => _counts.GetValueOrDefault(name);
    private void Table(uint address, uint table) => Require(address >= 0x10000 && U(address) == table, $"Client census native table mismatch at {Hex(address)}.");
    private uint Pointer(uint address) { uint value = U(address); Require(value >= 0x10000, $"Client census null/low pointer at {Hex(address)}."); return value; }
    private uint U(uint address) => BitConverter.ToUInt32(Read(address, 4));
    private uint TryU(uint address)
    {
        if (address < 0x10000) return 0;
        try { return U(address); } catch (Win32Exception) { return 0; }
    }
    private byte[] Read(uint address, int bytes)
    {
        EnsureStop(_host.MainThreadId);
        Require(address >= 0x10000 && bytes is >= 1 and <= 65536, "Client census read is outside its pointer/length bound.");
        _ = checked(address + (uint)bytes - 1);
        return Win32Native.ReadMemory(_process, address, bytes);
    }
    private void EnsureStop(uint thread) => Require(_host.IsAtDebugStop && thread == _host.MainThreadId &&
        _host.ProcessHandle == _process && _host.ProcessPid == _pid && Environment.CurrentManagedThreadId == _ownerThread,
        "Client dispatch observation requires the captured owned main-thread debug stop.");
    private static string Hex(uint value) => $"0x{value:X8}";
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidDataException(message); }
}
