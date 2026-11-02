using System.Buffers.Binary;
using System.Reflection.PortableExecutable;
using System.Security.Cryptography;
using System.Text;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

// Our immutable entry payload. The original loader, permission logic, character
// placement and spawn functions still execute. This is not a SavedXX codec.
internal sealed class WorldEntrySeedState
{
    internal const int Schema = 1, Bytes = 448, HeaderBytes = 24;
    private readonly byte[] _values;
    internal WorldEntrySeedState(uint level, uint epoch, uint generation, uint hostOrdinal, byte[] values)
    {
        Require(level is > 0 and <= 50 && epoch != 0 && generation != 0 && hostOrdinal != 0,
            "Entry seed level/epoch/generation/Host ordinal is invalid.");
        SharedProfileState.ValidateEntryValues(values);
        Level = level; LevelEpoch = epoch; Generation = generation; HostEntryOrdinal = hostOrdinal;
        _values = (byte[])values.Clone();
    }
    internal uint Level { get; }
    internal uint LevelEpoch { get; }
    internal uint Generation { get; }
    internal uint HostEntryOrdinal { get; }
    internal byte LoadFlag => _values[SharedProfileState.PayloadBytes];
    internal byte[] CopyValues() => (byte[])_values.Clone();
    internal byte[] Pack()
    {
        byte[] result = new byte[Bytes];
        uint[] header = [Schema, Level, LevelEpoch, Generation, HostEntryOrdinal, 0];
        for (int index = 0; index < header.Length; ++index)
            BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(index * 4, 4), header[index]);
        _values.CopyTo(result, HeaderBytes); return result;
    }
    internal static WorldEntrySeedState Unpack(byte[] payload)
    {
        Require(payload.Length == Bytes && U(0) == Schema && U(20) == 0,
            "Entry seed schema/length/reserved value is invalid.");
        return new(U(4), U(8), U(12), U(16), payload.AsSpan(HeaderBytes).ToArray());
        uint U(int offset) => BinaryPrimitives.ReadUInt32LittleEndian(payload.AsSpan(offset, 4));
    }
    private static void Require(bool value, string message)
    { if (!value) throw new InvalidDataException(message); }
}

// This borrowed frame is used only during the one stopped LoadLevel entry.
// Scene/body/Flow-top readiness is intentionally not invented before native init.
internal sealed record NativeWorldEntryFrame(IntPtr Process, uint Pid, uint Thread, int OwnerThread,
    uint DebugStop, uint Playing, uint PlayingTable, uint Level, uint Esp, uint Caller,
    uint Profile, uint SideQuestManager, uint Raw40, uint Raw44, uint Raw48)
{
    internal const uint LoadEntry = 0x5F9580, LoadCaller = 0x5F9BF1;
    internal static NativeWorldEntryFrame Capture(DebuggerHost host, uint tid,
        in Win32Native.X86Context context, uint expectedLevel)
    {
        Require(host.IsAtDebugStop && tid == host.MainThreadId && host.ProcessHandle != IntPtr.Zero &&
            host.ImageBase == 0x400000 && context.Eip == LoadEntry, "Entry frame is not original main-thread LoadLevel entry.");
        uint receiver = context.Ecx, table = U(receiver), profile = U(0x765AD4), manager = U(0x765AE0);
        Require(receiver >= 0x10000 && (receiver & 3) == 0 && table is 0x710874 or 0x7105C8 &&
            U(receiver + 0x10) == expectedLevel && U(context.Esp) == LoadCaller,
            "Entry receiver/level/native caller is not admitted.");
        Require(profile >= 0x10000 && manager >= 0x10000 && (profile & 3) == 0 && (manager & 3) == 0 &&
            U(profile) == 0x6F5A44 && U(manager) == 0x6FDEDC,
            "Entry canonical Profile/SideQuest singleton is absent/untyped; native bootstrap is required before seeding.");
        var frame = new NativeWorldEntryFrame(host.ProcessHandle, host.ProcessPid, tid,
            Environment.CurrentManagedThreadId, host.DebugStopSequence, receiver, table, expectedLevel,
            context.Esp, LoadCaller, profile, manager, U(receiver + 0x40), U(receiver + 0x44), U(receiver + 0x48));
        frame.VerifyAtStop(host, tid, context); return frame;
        uint U(uint address) => Win32Native.ReadUInt32(host.ProcessHandle, address);
    }
    internal void VerifyAtStop(DebuggerHost host, uint tid, in Win32Native.X86Context context)
    {
        Require(host.IsAtDebugStop && host.ProcessHandle == Process && host.ProcessPid == Pid &&
            host.MainThreadId == Thread && tid == Thread && Environment.CurrentManagedThreadId == OwnerThread &&
            host.DebugStopSequence == DebugStop && context.Eip == LoadEntry && context.Ecx == Playing &&
            context.Esp == Esp && U(Esp) == Caller && U(Playing) == PlayingTable && U(Playing + 0x10) == Level &&
            U(Playing + 0x40) == Raw40 && U(Playing + 0x44) == Raw44 && U(Playing + 0x48) == Raw48 &&
            U(0x765AD4) == Profile && U(Profile) == 0x6F5A44 &&
            U(0x765AE0) == SideQuestManager && U(SideQuestManager) == 0x6FDEDC,
            "Stopped entry process/frame/receiver/canonical owners changed.");
        uint U(uint address) => Win32Native.ReadUInt32(host.ProcessHandle, address);
    }
    internal bool IsCurrentAtStop(DebuggerHost host, uint tid, in Win32Native.X86Context context)
    { try { VerifyAtStop(host, tid, context); return true; } catch { return false; } }
    private static void Require(bool value, string message)
    { if (!value) throw new InvalidDataException(message); }
}

/// <summary>
/// Own bounded observer/quiet seed of one original LoadLevel entry. A Host seed
/// precedes native initialization; actual chosen authored groups and spawn calls
/// produce the witness after the original native entry and first owned World.
/// </summary>
internal sealed class NativeWorldEntrySeed
{
    internal const int WitnessBytes = 64;
    private const uint PlaceEntry = 0x4EEF30, SelectGroup = 0x4EEAF0, PlaceGroup = 0x4EE5B0;
    private const uint PlacementCaller = 0x5F9940;
    private const uint AcceptPlacement = 0x4EE70D;
    private const uint SavedSpawnCall = 0x5F9A2F, DefaultSpawnCall = 0x5F9A58;
    private readonly DebuggerHost _host;
    private readonly PEReader _image;
    private readonly Action<object> _log;
    private readonly WorldStateRole _role;
    private readonly uint _level, _epoch, _generation, _captureOrdinal, _thread, _pid;
    private readonly IntPtr _process;
    private readonly int _owner;
    private readonly List<string> _semanticPaths = [];
    private readonly Dictionary<uint, uint> _acceptedSlots = [];
    private NativeWorldEntryFrame? _frame;
    private WorldEntrySeedState? _seed;
    private SharedProfileEntryApply? _apply;
    private uint _nativeOrdinal, _selectedGroups, _partGroups, _placementManager, _cohortScene, _createdCount;
    private uint _placementEsp, _placementCaller, _spawnRoute, _subgroupNode, _acceptedPlacements;
    private uint _placementArgument44, _guardMismatches;
    private bool _installed, _active, _placementActive, _placementReturned, _spawnObserved;
    private bool _loadReturned, _worldQualified, _retired, _failed;
    private byte[]? _witness;

    internal NativeWorldEntrySeed(DebuggerHost host, PEReader image, Action<object> log,
        WorldStateRole role, uint level, uint levelEpoch, uint generation,
        byte[]? guestSeed = null, uint captureEntryOrdinal = 1)
    {
        Require(host.IsAtDebugStop && host.ProcessHandle != IntPtr.Zero && host.ImageBase == 0x400000 &&
            role is WorldStateRole.Host or WorldStateRole.Guest && level is 28 or 47 &&
            levelEpoch != 0 && generation != 0 && captureEntryOrdinal is > 0 and <= 8,
            "Entry observer requires an explicit owned Alfea28/Battle47 initial epoch.");
        Require(Convert.ToHexString(SHA256.HashData(image.GetEntireImage().GetContent().ToArray())) ==
            "3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F",
            "Entry observer requires the pinned original PC image.");
        _host = host; _image = image; _log = log; _role = role; _level = level;
        _epoch = levelEpoch; _generation = generation; _captureOrdinal = captureEntryOrdinal;
        _thread = host.MainThreadId; _pid = host.ProcessPid; _process = host.ProcessHandle;
        _owner = Environment.CurrentManagedThreadId;
        Require(role == WorldStateRole.Host ? guestSeed is null : guestSeed is not null,
            "Only Guest entry has an authoritative pre-init seed payload.");
        if (guestSeed is not null)
        {
            _seed = WorldEntrySeedState.Unpack(guestSeed);
            Require(_seed.Level == level && _seed.LevelEpoch == levelEpoch && _seed.Generation == generation,
                "Guest entry seed belongs to another level epoch/generation.");
        }
    }

    internal bool EntryCompleted => _loadReturned && _placementReturned && _spawnObserved && !_failed;
    internal bool LoadReturned => _loadReturned && !_failed;
    internal uint NativeLocalEntryOrdinal => _nativeOrdinal;
    internal bool WorldQualified => _worldQualified && !_failed;
    internal byte[] GetSeed() => EntryCompleted && _seed is not null ? _seed.Pack() :
        throw new InvalidDataException("Native entry seed was requested before actual original entry completion.");
    internal byte[] GetWitness() => WorldQualified && _witness is not null ? (byte[])_witness.Clone() :
        throw new InvalidDataException("Native entry witness was requested before its post-init World qualification.");
    internal object Summary => new { role = _role.ToString(), installed = _installed, retired = _retired,
        failed = _failed, nativeLocalEntryOrdinal = _nativeOrdinal, targetLocalOrdinal = _captureOrdinal,
        hostEntryOrdinal = _seed?.HostEntryOrdinal, entryCompleted = EntryCompleted, worldQualified = WorldQualified,
        placementReturned = _placementReturned, selectedCohortCalls = _selectedGroups, authoredSubgroupCalls = _partGroups,
        placementCurrentArgument44 = _placementArgument44, boundedGuardMismatches = _guardMismatches,
        populationInstanceCount = _createdCount, observedAcceptedPlacements = _acceptedPlacements,
        actualSpawnCallObserved = _spawnObserved,
        observedSpawnRoute = _spawnObserved ? _spawnRoute : (uint?)null,
        orderedSemanticPaths = _semanticPaths.ToArray(), preSeedApply = _apply,
        seedBytes = WorldEntrySeedState.Bytes, witnessBytes = WitnessBytes,
        preSeedHash = _seed is null ? null : Hash(_seed.Pack()),
        witness = _witness is null ? null : Convert.ToBase64String(_witness),
        nativeCallsAdded = 0, entryArgumentsRewritten = false, pointersInPayload = false,
        fullLevelEntryOrAllNpcBehaviorQualified = false };

    internal void InstallStopped()
    {
        Ensure(_thread); Require(!_installed && !_retired, "Entry observer installation is single-use.");
        foreach (uint site in new[] { NativeWorldEntryFrame.LoadEntry, PlaceEntry, SelectGroup, PlaceGroup, AcceptPlacement,
                     SavedSpawnCall, DefaultSpawnCall })
            Require(Read(site, 8).SequenceEqual(Image(site, 8)), "Original entry observation site changed before installation.");
        // Original LoadLevel assigns a new +44 at5F9620, then reads that live
        // value for placement at5F9931. PRE-entry Raw44 is not this argument.
        Pin(0x5F9617, "E86475FAFF8D4C2414894544");
        Pin(0x5F9931, "8B4D448B551051528BC8E8F055EFFF8BCD");
        Pin(0x4EF22B, "C20800");
        _installed = true;
        Repeat(NativeWorldEntryFrame.LoadEntry, "Entry.LoadLevel", OnLoadEntry);
        Repeat(PlaceEntry, "Entry.CharacterPlacement", OnPlacementEntry);
        Repeat(SelectGroup, "Entry.SelectedCohort", OnSelectedGroup);
        Repeat(PlaceGroup, "Entry.AuthoredSubgroup", OnPlaceGroup);
        Repeat(AcceptPlacement, "Entry.AcceptedPlacementSlot", OnAcceptedPlacement);
        Repeat(SavedSpawnCall, "Entry.SavedSpawnCall", OnSpawn);
        Repeat(DefaultSpawnCall, "Entry.DefaultSpawnCall", OnSpawn);
    }

    internal void ObserveAtWorld(uint tid, ref Win32Native.X86Context context, in SharedProfileScene scene)
    {
        if (_worldQualified || _retired || !EntryCompleted) return;
        Ensure(tid); Require(context.Eip == NativeCallDispatcher.WorldReturn && EntryCompleted && _frame is not null &&
            scene.LevelEpoch == _epoch && scene.BodyGeneration == _generation && scene.Level == _level &&
            scene.PlayingState == _frame!.Playing && scene.PlayingTable == _frame.PlayingTable,
            "Entry post-init World does not own the captured original playing receiver.");
        VerifyRetainedOwners();
        Require(U(_frame!.Profile + 0x6E0) % 256 == 0 && (_cohortScene == 0 || _cohortScene == scene.Scene),
            "Original spawn load flag was not consumed or cohort belongs to a different scene.");
        uint primary = P(_frame.Profile + 0x2B4), root = P(primary + 0x24);
        Require(U(primary) == 0x710F80 && U(root) == VisualGraphReader.NodeVtable && U(root + 0x3C) == scene.Scene,
            "Entry post-init primary scene identity changed.");
        VerifyPlacementManager(_placementManager); HashSet<uint> instances = [];
        for (uint index = 0; index < 300; ++index)
        {
            uint instance = U(_placementManager + 0x30 + index * 4);
            if (instance == 0)
            {
                Require(!_acceptedSlots.ContainsKey(index), "An originally accepted placement slot disappeared before first World.");
                continue;
            }
            uint node = P(instance + 0x24);
            Guard(_acceptedSlots.TryGetValue(index, out uint accepted) && accepted == instance &&
                instances.Add(instance) && U(instance) == 0x710F80 &&
                U(node) == VisualGraphReader.NodeVtable && U(node + 0x3C) == scene.Scene,
                "post-world-placement-slot", "Entry placement slot differs from its actual accepted instance or is duplicate/untyped/foreign-scene.",
                new { context = Context(context), slot = index, instance, acceptedInstance = accepted,
                    actualInstanceTable = U(instance), root = node, actualRootTable = U(node), actualScene = U(node + 0x3C),
                    expectedScene = scene.Scene, expectedInstanceTable = 0x710F80, expectedRootTable = VisualGraphReader.NodeVtable });
        }
        Require(instances.Count == _acceptedSlots.Count, "Accepted placement slots differ from actual post-entry population.");
        _createdCount = checked((uint)instances.Count);
        byte[] witness = new byte[WitnessBytes];
        uint[] header = [1, _level, _epoch, _generation, _seed!.HostEntryOrdinal, _spawnRoute, _selectedGroups, _createdCount];
        for (int index = 0; index < header.Length; ++index)
            BinaryPrimitives.WriteUInt32LittleEndian(witness.AsSpan(index * 4, 4), header[index]);
        SHA256.HashData(Encoding.UTF8.GetBytes(string.Join("\n", _semanticPaths))).CopyTo(witness, 32);
        _witness = witness; _worldQualified = true;
        _log(new { @event = "world-entry-native-qualified", nativeLocalEntryOrdinal = _nativeOrdinal,
            hostEntryOrdinal = _seed.HostEntryOrdinal, level = _level, levelEpoch = _epoch, generation = _generation,
            observedSpawnRoute = _spawnRoute, selectedCohortCalls = _selectedGroups,
            populationInstanceCount = _createdCount, observedAcceptedPlacements = _acceptedPlacements,
            exactAcceptedPlacementSlotJoin = true, orderedSemanticPaths = _semanticPaths.ToArray(),
            witnessBase64 = Convert.ToBase64String(witness), witnessHash = Hash(witness),
            preSeedHash = Hash(_seed.Pack()), pointersInPayload = false });
    }

    internal void VerifyExpectedWitness(byte[] expected)
    {
        Require(WorldQualified && _witness is not null && expected.Length == WitnessBytes &&
            _witness.AsSpan().SequenceEqual(expected), "Guest native entry cohort/spawn witness differs from Host.");
    }

    internal void RetireCached()
    {
        Require(Environment.CurrentManagedThreadId == _owner, "Entry retirement changed its owner thread.");
        _retired = true; _active = false; _placementActive = false;
        _log(new { @event = "world-entry-retired", nativeReads = 0, nativeWrites = 0,
            historicalEntryOrSceneOwnersWillNotBeFollowed = true });
    }

    private void OnLoadEntry(uint tid, uint at, ref Win32Native.X86Context context)
    {
        Ensure(tid); Require(context.Eip == at && at == NativeWorldEntryFrame.LoadEntry, "Entry callback moved.");
        uint table = U(context.Ecx);
        if (table is not (0x710874 or 0x7105C8) || U(context.Ecx + 0x10) != _level) return;
        Require(++_nativeOrdinal <= 8 && !_active && !_loadReturned, "Entry observer saw a nested or repeated accepted native load.");
        if (_nativeOrdinal != _captureOrdinal) return;
        _frame = NativeWorldEntryFrame.Capture(_host, tid, context, _level);
        byte[] before = SharedProfileState.CaptureEntryValuesAtStop(_host, tid, context, _frame, _log);
        if (_role == WorldStateRole.Host) _seed = new(_level, _epoch, _generation, _nativeOrdinal, before);
        else _apply = SharedProfileState.ApplyEntryValuesAtStop(_host, tid, context, _frame, _seed!.CopyValues(), _log);
        _active = true;
        _log(new { @event = "world-entry-seed-admitted", role = _role.ToString(),
            nativeLocalEntryOrdinal = _nativeOrdinal, hostEntryOrdinal = _seed!.HostEntryOrdinal,
            level = _level, levelEpoch = _epoch, generation = _generation,
            seedBase64 = Convert.ToBase64String(_seed.Pack()), seedHash = Hash(_seed.Pack()),
            stoppedBeforeOriginalLoadLevelInstruction = true, originalEntryArgumentsChanged = false });
        _host.Breakpoints.AddReturn(_frame.Caller, tid, "Entry.LoadLevel.return", OnLoadReturned);
    }

    private void OnLoadReturned(uint tid, uint at, ref Win32Native.X86Context context)
    {
        Ensure(tid); VerifyRetainedOwners();
        Require(_active && _frame is not null && at == _frame.Caller && context.Eip == at &&
            context.Esp == checked(_frame.Esp + 8) && (context.Eax & 255) == 1 &&
            _placementReturned && !_placementActive && _spawnObserved,
            "Original LoadLevel return/ESP/AL/placement/spawn did not complete its exact entry.");
        _loadReturned = true; _active = false;
        _log(new { @event = "world-entry-load-returned", nativeLocalEntryOrdinal = _nativeOrdinal,
            hostEntryOrdinal = _seed!.HostEntryOrdinal, nativeLoadSucceeded = true, ret4EspMatched = true,
            actualSpawnCallObserved = true, selectedCohortCalls = _selectedGroups });
    }

    private void OnPlacementEntry(uint tid, uint at, ref Win32Native.X86Context context)
    {
        if (!_active) return;
        Ensure(tid); VerifyRetainedOwners();
        uint manager = P(0x765AEC); VerifyPlacementManager(manager);
        uint caller = U(context.Esp), argumentLevel = U(context.Esp + 4), argument44 = U(context.Esp + 8);
        uint current44 = U(_frame!.Playing + 0x44);
        Guard(!_placementActive && !_placementReturned && at == PlaceEntry && context.Eip == PlaceEntry &&
            context.Ecx == manager && context.Ebp == _frame.Playing && argumentLevel == _level &&
            argument44 == current44 && caller == PlacementCaller,
            "placement-entry", "Character placement entry does not match the original LoadLevel call.",
            new { context = Context(context), actualCaller = caller, expectedCaller = PlacementCaller,
                actualArgumentLevel = argumentLevel, expectedLevel = _level, actualArgument44 = argument44,
                currentPlaying44 = current44, preLoadRaw44 = _frame.Raw44, capturedPlaying = _frame.Playing,
                canonicalPlacementManager = manager, placementActive = _placementActive, placementReturned = _placementReturned });
        _placementManager = manager; _placementActive = true;
        _placementEsp = context.Esp; _placementCaller = caller; _placementArgument44 = argument44;
        _log(new { @event = "world-entry-placement-entered", nativeLocalEntryOrdinal = _nativeOrdinal,
            hostEntryOrdinal = _seed!.HostEntryOrdinal, actualCaller = caller, argumentLevel,
            actualArgument44 = argument44, preLoadRaw44 = _frame.Raw44, currentPlaying44 = current44,
            originalNativeReplacedRaw44 = _frame.Raw44 != current44, nativeEspDeltaOnReturn = 12 });
        _host.Breakpoints.AddReturn(_placementCaller, tid, "Entry.CharacterPlacement.return", OnPlacementReturned);
    }
    private void OnPlacementReturned(uint tid, uint at, ref Win32Native.X86Context context)
    {
        Ensure(tid); VerifyRetainedOwners(); VerifyPlacementManager(_placementManager);
        Guard(_active && _placementActive && at == _placementCaller && context.Eip == at &&
            context.Esp == checked(_placementEsp + 12), "placement-return", "Character placement native RET8 did not pair.",
            new { context = Context(context), actualCallback = at, expectedCaller = _placementCaller,
                entryEsp = _placementEsp, expectedReturnedEsp = checked(_placementEsp + 12) });
        _placementActive = false; _placementReturned = true;
    }
    private void OnSelectedGroup(uint tid, uint at, ref Win32Native.X86Context context)
    {
        if (!_active || !_placementActive) return;
        EnsurePlacement(tid, context); Require(++_selectedGroups <= 16, "Entry selected more than16 authored cohorts.");
        uint group = P(context.Esp + 4); string name = NodeName(group);
        uint scene = P(group + 0x3C);
        Require(U(scene) == 0x6E7358 && (_cohortScene == 0 || _cohortScene == scene), "Cohort Node changed scene.");
        _cohortScene = scene; _semanticPaths.Add("Cohort:" + name);
    }
    private void OnPlaceGroup(uint tid, uint at, ref Win32Native.X86Context context)
    {
        if (!_active || !_placementActive) return;
        EnsurePlacement(tid, context); Require(_selectedGroups > 0 && ++_partGroups <= 300,
            "Entry authored subgroup escaped the finite selected cohort.");
        uint group = P(context.Esp + 4); Require(U(group + 0x3C) == _cohortScene, "Authored subgroup changed its cohort scene.");
        _subgroupNode = group;
        _semanticPaths.Add("Group:" + NodeName(group));
        uint count = U(group + 0x1C), head = P(group + 0x18), link = U(head), previous = head;
        Require(count <= 64, "Authored subgroup child count exceeded64."); HashSet<uint> links = [], children = [];
        for (uint index = 0; index < count; ++index)
        {
            Require(link >= 0x10000 && link != head && links.Add(link) && U(link + 4) == previous,
                "Authored subgroup child list is not finite/reciprocal.");
            uint child = P(link + 8);
            Require(children.Add(child) && U(child + 0x2C) == group && U(child + 0x3C) == _cohortScene,
                "Authored subgroup child belongs to another parent/scene.");
            _semanticPaths.Add("Child:" + NodeName(child)); previous = link; link = U(link);
        }
        Require(link == head && U(head + 4) == previous && _semanticPaths.Count <= 4096,
            "Authored subgroup count/sentinel/path budget differs.");
    }
    private void OnAcceptedPlacement(uint tid, uint at, ref Win32Native.X86Context context)
    {
        if (!_active || !_placementActive) return;
        Ensure(tid); VerifyRetainedOwners(); VerifyPlacementManager(_placementManager);
        Require(context.Eip == AcceptPlacement && at == AcceptPlacement && context.Ebx == _placementManager &&
            context.Edx == context.Eax && context.Edx < 300 && _subgroupNode != 0 &&
            _selectedGroups != 0 && ++_acceptedPlacements <= 4096,
            "Actual native placement store escaped its admitted manager/subgroup/slot budget.");
        uint marker = context.Esi, instance = context.Edi, slot = context.Edx;
        string name = NodeName(marker);
        Guard(U(marker + 0x2C) == _subgroupNode && U(marker + 0x3C) == _cohortScene &&
            instance >= 0x10000 && (instance & 3) == 0 && U(instance) == 0x710F80,
            "accepted-marker-instance", "Accepted placement marker/instance does not belong to its actual authored subgroup.",
            new { context = Context(context), slot, marker, markerName = name, markerParent = U(marker + 0x2C),
                expectedSubgroup = _subgroupNode, markerScene = U(marker + 0x3C), expectedScene = _cohortScene,
                instance, actualInstanceTable = instance >= 0x10000 && (instance & 3) == 0 ? U(instance) : (uint?)null,
                expectedInstanceTable = 0x710F80 });
        uint root = P(instance + 0x24);
        Guard(U(root) == VisualGraphReader.NodeVtable && U(root + 0x3C) == _cohortScene,
            "accepted-instance-root", "Accepted placement instance root belongs to a different scene.",
            new { context = Context(context), slot, marker, instance, root, actualRootTable = U(root),
                expectedRootTable = VisualGraphReader.NodeVtable, actualScene = U(root + 0x3C), expectedScene = _cohortScene });
        uint before = U(_placementManager + 0x30 + slot * 4);
        Require(_acceptedSlots.TryGetValue(slot, out uint previous) ? before == previous : before == 0,
            "Native placement slot has an unobserved prior owner.");
        _acceptedSlots[slot] = instance;
        _semanticPaths.Add("Accepted:" + slot + ":" + name);
        Require(_semanticPaths.Count <= 4096, "Accepted semantic path budget exceeded4096.");
    }
    private void OnSpawn(uint tid, uint at, ref Win32Native.X86Context context)
    {
        if (!_active) return;
        Ensure(tid); VerifyRetainedOwners();
        Require(_placementReturned && !_spawnObserved && context.Ecx == _frame!.Playing && context.Eip == at,
            "Original spawn call escaped its captured entry receiver.");
        uint route = at == SavedSpawnCall ? 1u : 0u;
        Require(route == _seed!.LoadFlag, "Original spawn route differs from its authoritative pre-entry load flag.");
        if (route == 1)
            Require(U(context.Esp) == U(_frame!.Profile + 0x2B4) && U(context.Esp + 4) == _frame.Profile + 0x584,
                "Original saved spawn call does not consume the canonical saved position.");
        _spawnRoute = route; _spawnObserved = true;
    }

    private void EnsurePlacement(uint tid, in Win32Native.X86Context context)
    { Ensure(tid); VerifyRetainedOwners(); VerifyPlacementManager(_placementManager);
      Require(context.Ecx == _placementManager, "Authored cohort callback is from a different placement manager."); }
    private void VerifyPlacementManager(uint manager)
    {
        Require(manager >= 0x10000 && manager == U(0x765AEC), "Character-placement canonical owner changed.");
        uint table = P(manager), getter = U(table + 0x10); byte[] code = Image(getter, 6);
        Require(table is >= 0x6DA000 and < 0x744000 && getter is >= 0x401000 and < 0x6DA000 &&
            U(table + 0x10) == BitConverter.ToUInt32(Image(table + 0x10, 4)) && Read(getter, 6).SequenceEqual(code) &&
            code[0] == 0xB8 && code[5] == 0xC3 && BitConverter.ToUInt32(code, 1) == 0x74E368 &&
            U(0x74E368) == 0x6C56254E, "Character-placement table/getter/runtime metadata is not the original type.");
    }
    private void VerifyRetainedOwners()
    {
        Require(_frame is not null && U(_frame.Playing) == _frame.PlayingTable && U(_frame.Playing + 0x10) == _level &&
            U(0x765AD4) == _frame.Profile && U(_frame.Profile) == 0x6F5A44 &&
            U(0x765AE0) == _frame.SideQuestManager && U(_frame.SideQuestManager) == 0x6FDEDC,
            "Entry retained receiver/canonical owners changed before qualification.");
    }
    private string NodeName(uint node)
    {
        Require(U(node) == VisualGraphReader.NodeVtable, "Authored placement marker is not a plain native Node.");
        uint name = U(node + 0x10); if (name == 0) return "";
        Require(name >= 0x10000, "Authored Node name pointer is low."); List<byte> value = [];
        for (uint index = 0; index < 192; ++index)
        {
            byte character = Read(checked(name + 9 + index), 1)[0];
            if (character == 0) return Encoding.ASCII.GetString(value.ToArray());
            Require(character is >= 32 and <= 126, "Authored Node semantic name is not bounded ASCII."); value.Add(character);
        }
        throw new InvalidDataException("Authored Node semantic name exceeded192 bytes.");
    }
    private void Repeat(uint site, string name, BreakpointCallback callback)
    {
        BreakpointCallback repeat = null!;
        repeat = (uint tid, uint at, ref Win32Native.X86Context context) =>
        {
            if (_retired) return;
            try { Require(Read(site, 8).SequenceEqual(Image(site, 8)), "Original entry instruction pin changed."); callback(tid, at, ref context); }
            catch { _failed = true; _retired = true; throw; }
            if (!_retired) _host.Breakpoints.AddReturn(site, _thread, name, repeat);
        };
        _host.Breakpoints.AddReturn(site, _thread, name, repeat);
    }
    private void Guard(bool value, string name, string message, object actual)
    {
        if (value) return;
        if (_guardMismatches < 8)
        {
            ++_guardMismatches;
            _log(new { @event = "world-entry-guard-mismatch", name, message, role = _role.ToString(),
                nativeLocalEntryOrdinal = _nativeOrdinal, hostEntryOrdinal = _seed?.HostEntryOrdinal,
                level = _level, levelEpoch = _epoch, generation = _generation, actual,
                maximumMismatchSamples = 8, nativeReadsAreBoundedToCurrentStoppedOwners = true });
        }
        throw new InvalidDataException(message);
    }
    private static object Context(in Win32Native.X86Context context) => new
    { context.Eip, context.Esp, context.Ebp, context.Eax, context.Ebx, context.Ecx, context.Edx,
        context.Esi, context.Edi, context.EFlags };
    private void Pin(uint address, string hex)
    {
        byte[] expected = Convert.FromHexString(hex);
        Require(Image(address, expected.Length).SequenceEqual(expected) && Read(address, expected.Length).SequenceEqual(expected),
            $"Original entry argument/return source pin changed at0x{address:X8}.");
    }
    private void Ensure(uint tid) => Require(!_retired && !_failed && _host.IsAtDebugStop &&
        _host.ProcessHandle == _process && _host.ProcessPid == _pid && tid == _thread && _host.MainThreadId == _thread &&
        Environment.CurrentManagedThreadId == _owner, "Entry observer lost its stopped native process/thread owner.");
    private uint U(uint address) => Win32Native.ReadUInt32(_process, address);
    private uint P(uint address)
    { uint value = U(address); Require(value >= 0x10000 && (value & 3) == 0, "Entry borrowed pointer is null/low/unaligned."); return value; }
    private byte[] Read(uint address, int bytes) => Win32Native.ReadMemory(_process, address, bytes);
    private byte[] Image(uint address, int bytes) => _image.GetSectionData(checked((int)(address - 0x400000))).GetContent(0, bytes).ToArray();
    private static string Hash(byte[] bytes) => Convert.ToHexString(SHA256.HashData(bytes));
    private static void Require(bool value, string message)
    { if (!value) throw new InvalidDataException(message); }
}
