using System.Reflection.PortableExecutable;
using System.Security.Cryptography;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

// Local borrowed identities only. Category/slot are pool locations, not stable
// network IDs or activation generations. No wx Instance/Character is implied.
internal sealed record WorldPresentationStarTransform(IReadOnlyList<float> Position,
    IReadOnlyList<float> Scale, IReadOnlyList<float> Rotation);
internal sealed record WorldPresentationStarNode(uint Address, uint Table, uint Parent,
    uint Scene, uint Flags, bool InExpectedScene, WorldPresentationStarTransform Local,
    WorldPresentationStarTransform CachedWorld);
internal sealed record WorldPresentationStarPoolEntry(uint Category, uint Slot, uint Star,
    uint Table, byte Active40, uint StateC8, uint NodeCC, bool NativeSchedulerEligible,
    WorldPresentationStarNode? Node, uint NativeSlotAddress = 0);
internal sealed record WorldPresentationStarPoolObservation(uint Manager, uint Scene,
    uint DebugStopSequence, byte OptionalPool55, bool IncludesInactiveSlots,
    bool InspectedInactiveNodes, uint RepeatedStarAddresses, uint RepeatedNodeAddresses,
    IReadOnlyList<WorldPresentationStarPoolEntry> Entries);

/// <summary>
/// Own read-only ABI observer of the original FairyStarsManager fixed pools.
/// Does not tick, launch, award, hide, allocate, bind or delete any native object.
/// Inactive Node pointers are followed only by the explicit inventory option.
/// </summary>
internal sealed class WorldPresentationStarPoolReader(DebuggerHost host, PEReader image)
{
    internal const uint StarClassId = 0x5C2941A4;
    private const uint ManagerSlot = 0x765B00, ManagerTable = 0x6F61CC, StarTable = 0x702160;
    private const uint NodeTable = 0x6DC4F4, SceneTable = 0x6E7358;
    private const string SchedulerSha256 = "5E1FB5677D38136CE29756A49DBB683AE31EE9CE3ACDC40B247DB75BA7391DC2";
    private readonly int _ownerThread = Environment.CurrentManagedThreadId;
    private uint _manager, _scene, _pid, _observations, _activeNodeReads, _inactiveNodeReads;
    private IntPtr _process;
    private bool _retired;

    internal object Summary => new { bound = _manager != 0, retired = _retired,
        observations = _observations, activeNodeReads = _activeNodeReads,
        explicitlyRequestedInactiveNodeReads = _inactiveNodeReads, maximumPoolSlots = 250,
        nativeWrites = 0, nativeCalls = 0, nativeObjectsOwned = 0,
        stableIdsAssigned = false, wireContractDefined = false,
        runtimeInventoryQualified = false, runtimeMirroringQualified = false };

    internal WorldPresentationStarPoolObservation ReadActiveAtWorld(uint tid,
        ref Win32Native.X86Context context, uint scene) =>
        ReadAtWorld(tid, ref context, scene, includeInactive: false, inspectInactiveNodes: false);

    // This explicit diagnostic inventory can return null/inactive slots. With
    // inspectInactiveNodes=false, NodeCC is opaque and is never dereferenced.
    internal WorldPresentationStarPoolObservation InventorySlotsAtWorld(uint tid,
        ref Win32Native.X86Context context, uint scene, bool inspectInactiveNodes = false) =>
        ReadAtWorld(tid, ref context, scene, includeInactive: true, inspectInactiveNodes);

    internal void RetireAtStop(uint tid, ref Win32Native.X86Context context)
    {
        Require(host.IsAtDebugStop && tid == host.MainThreadId && !_retired &&
            Environment.CurrentManagedThreadId == _ownerThread &&
            context.Eip is NativeCallDispatcher.WorldReturn or 0x5F2300 or 0x5F86C0,
            "Star pool reader must retire at its owned World/pre-unload stop.");
        // All returned pointers become historical. Nothing is owned or freed.
        _retired = true;
    }

    private WorldPresentationStarPoolObservation ReadAtWorld(uint tid,
        ref Win32Native.X86Context context, uint scene, bool includeInactive, bool inspectInactiveNodes)
    {
        Ensure(tid, context); uint stop = host.DebugStopSequence;
        Require(scene >= 0x10000 && (scene & 3) == 0 && U(scene) == SceneTable,
            "Star pool observation requires an admitted current native scene.");
        if (_manager == 0)
        {
            uint manager = Pointer(U(ManagerSlot));
            ValidateType(manager, ManagerTable, 0x454D6412, 0x4F0200, 0x74E3C8);
            PinScheduler();
            _manager = manager; _scene = scene; _pid = host.ProcessPid; _process = host.ProcessHandle;
        }
        VerifyOwners(scene, stop);
        byte optional = Read(_manager + 0x55, 1)[0];
        uint[] poolOffsets = [0x6C, 0x134, 0x1FC, 0x2C4];
        uint[][] pools = [Pool(poolOffsets[0], 50), Pool(poolOffsets[1], 50), Pool(poolOffsets[2], 50),
            optional != 0 ? Pool(poolOffsets[3], 100) : []];
        List<WorldPresentationStarPoolEntry> entries = [];
        HashSet<uint> stars = [], nodes = [];
        uint repeatedStars = 0, repeatedNodes = 0;
        for (uint category = 0; category < pools.Length; ++category)
            for (uint slot = 0; slot < pools[category].Length; ++slot)
            {
                uint star = pools[category][slot];
                uint nativeSlotAddress = checked(_manager + poolOffsets[category] + slot * sizeof(uint));
                bool eligible = category == 3 || pools[0][slot] != 0 && pools[1][slot] != 0 && pools[2][slot] != 0;
                if (star == 0)
                {
                    if (includeInactive) entries.Add(new(category, slot, 0, 0, 0, 0, 0, eligible, null, nativeSlotAddress));
                    continue;
                }
                Pointer(star); if (!stars.Add(star)) ++repeatedStars;
                ValidateType(star, StarTable, StarClassId, 0x58C420, 0x7550E8);
                Require(U(StarTable + 0x20) == 0x58D0A0 && ImageWord(StarTable + 0x20) == 0x58D0A0,
                    "Star pool object update slot differs from the admitted original.");
                byte active = Read(star + 0x40, 1)[0]; uint state = U(star + 0xC8), node = U(star + 0xCC);
                if (!includeInactive && active == 0) continue;
                WorldPresentationStarNode? observedNode = null;
                if (active != 0 || inspectInactiveNodes && node != 0)
                {
                    Pointer(node); if (!nodes.Add(node)) ++repeatedNodes;
                    observedNode = ReadNode(node, active != 0);
                    if (active != 0) ++_activeNodeReads; else ++_inactiveNodeReads;
                }
                Require(U(star) == StarTable && Read(star + 0x40, 1)[0] == active &&
                    U(star + 0xC8) == state && U(star + 0xCC) == node,
                    "Star pool object changed within the owned stopped observation.");
                entries.Add(new(category, slot, star, StarTable, active, state, node, eligible, observedNode, nativeSlotAddress));
            }
        VerifyOwners(scene, stop);
        Require(Read(_manager + 0x55, 1)[0] == optional &&
            Pool(poolOffsets[0], 50).SequenceEqual(pools[0]) && Pool(poolOffsets[1], 50).SequenceEqual(pools[1]) &&
            Pool(poolOffsets[2], 50).SequenceEqual(pools[2]) &&
            (optional == 0 || Pool(poolOffsets[3], 100).SequenceEqual(pools[3])),
            "Star pool slots changed within the owned stopped observation.");
        ++_observations;
        // Pool alias semantics are not established here. Preserve duplicates
        // explicitly; a future visual binding must decide its own admission.
        return new(_manager, scene, stop, optional, includeInactive, inspectInactiveNodes,
            repeatedStars, repeatedNodes, entries.AsReadOnly());
    }

    private WorldPresentationStarNode ReadNode(uint node, bool active)
    {
        Require(U(node) == NodeTable, "Star pool Node has an unqualified native table.");
        uint scene = U(node + 0x3C), parent = U(node + 0x2C);
        Require(active ? scene == _scene && parent >= 0x10000 && (parent & 3) == 0 : scene == 0 || scene == _scene,
            "Star pool Node belongs to a foreign scene or active Node lacks a scene parent.");
        // An explicitly inventoried inactive resource may still be unattached
        // (scene0). That is not an admitted presentation binding.
        return new(node, NodeTable, parent, scene, U(node + 0xB0), scene == _scene,
            Transform(node + 0x20, node + 0x30, node + 0x40),
            Transform(node + 0x74, node + 0x80, node + 0x8C));
    }
    private WorldPresentationStarTransform Transform(uint position, uint scale, uint rotation) =>
        new(Floats(position, 3), Floats(scale, 3), Floats(rotation, 9));
    private IReadOnlyList<float> Floats(uint at, int count)
    {
        byte[] bytes = Read(at, checked(count * 4));
        float[] values = Enumerable.Range(0, count).Select(i => BitConverter.ToSingle(bytes, i * 4)).ToArray();
        Require(values.All(float.IsFinite), "Star pool Node PRS contains a nonfinite value.");
        return Array.AsReadOnly(values);
    }
    private uint[] Pool(uint offset, int count)
    {
        Require(count is 50 or 100, "Star pool observation exceeds a fixed original array.");
        byte[] bytes = Read(_manager + offset, checked(count * 4));
        return Enumerable.Range(0, count).Select(i => BitConverter.ToUInt32(bytes, i * 4)).ToArray();
    }
    private void ValidateType(uint value, uint table, uint classId, uint getter, uint record)
    {
        Require(U(value) == table && U(table + 0x10) == getter && ImageWord(table + 0x10) == getter,
            "Star pool native object/table/getter identity differs.");
        byte[] expected = [0xB8, .. BitConverter.GetBytes(record), 0xC3];
        Require(Read(getter, 6).SequenceEqual(expected) && Image(getter, 6).SequenceEqual(expected) &&
            U(record) == classId,
            "Star pool native literal getter or initialized RTTI class identity differs.");
    }
    private void PinScheduler()
    {
        byte[] original = Image(0x4F1220, 0xBE);
        Require(Convert.ToHexString(SHA256.HashData(original)) == SchedulerSha256 &&
            Read(0x4F1220, original.Length).SequenceEqual(original),
            "Star pool scheduler layout differs from its admitted E3 PE window.");
    }
    private void VerifyOwners(uint scene, uint stop) => Require(host.IsAtDebugStop && !_retired &&
        host.DebugStopSequence == stop && host.ProcessPid == _pid && host.ProcessHandle == _process &&
        scene == _scene && U(scene) == SceneTable && U(ManagerSlot) == _manager && U(_manager) == ManagerTable,
        "Star pool stopped process/manager/scene owner changed.");
    private void Ensure(uint tid, Win32Native.X86Context context) => Require(host.IsAtDebugStop && !_retired &&
        host.ProcessHandle != IntPtr.Zero && host.ProcessPid != 0 && host.ImageBase == 0x400000 &&
        tid == host.MainThreadId && context.Eip == NativeCallDispatcher.WorldReturn &&
        Environment.CurrentManagedThreadId == _ownerThread, "Star pool observation requires owned original WorldRET.");
    private uint ImageWord(uint at) => BitConverter.ToUInt32(Image(at, 4));
    private byte[] Image(uint at, int count) => image.GetSectionData(checked((int)(at - 0x400000))).GetContent(0, count).ToArray();
    private uint U(uint at) => Win32Native.ReadUInt32(host.ProcessHandle, at);
    private byte[] Read(uint at, int count) => Win32Native.ReadMemory(host.ProcessHandle, at, count);
    private static uint Pointer(uint value) { Require(value >= 0x10000 && (value & 3) == 0, "Star pool native pointer is not aligned/live."); return value; }
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidDataException(message); }
}
