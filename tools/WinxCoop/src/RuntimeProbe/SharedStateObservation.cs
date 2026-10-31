using System.Reflection.PortableExecutable;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>Battle47 resource observations and an explicitly requested combat fixture.</summary>
internal sealed class SharedStateObservation
{
    private readonly DebuggerHost _host;
    private readonly PEReader _image;
    private readonly Action<object> _log;
    private readonly bool _combatFixture;
    private readonly bool _prepareFirstMagicStage;
    private readonly Dictionary<string, uint> _counts = [];
    private readonly Dictionary<string, uint> _sampleCounts = [];
    private readonly List<object> _samples = [];
    private uint _profile, _record, _character, _traits, _root, _battle, _worlds, _readyWorlds, _pending, _command, _machine;
    private long _began;
    private bool _started, _retired, _completed;
    private readonly NativeCombatFixture _combatActivation;
    private int _ownerThread;

    internal SharedStateObservation(DebuggerHost host, PEReader image, Action<object> log, bool combatFixture, bool prepareFirstMagicStage)
    {
        _host = host; _image = image; _log = log; _combatFixture = combatFixture;
        _prepareFirstMagicStage = prepareFirstMagicStage; _ownerThread = Environment.CurrentManagedThreadId;
        _combatActivation = new(host, image, log, prepareFirstMagicStage);
    }

    internal bool ModeCompleted => _completed;
    internal bool ModeSucceeded => _completed && _started && _readyWorlds > 0 && _pending == 0 && (!_combatFixture || _combatActivation.Completed);
    internal uint ReadySamples => _readyWorlds;
    internal object Summary => new { started = _started, completed = _completed, succeeded = ModeSucceeded,
        supportedLevel = 47, nativeWorlds = _worlds, readyWorlds = _readyWorlds,
        retiredAtNativeUnload = _retired, pendingNativePairs = _pending,
        counts = new Dictionary<string, uint>(_counts), samples = _samples.ToArray(),
        profile = Hex(_profile), record = Hex(_record), character = Hex(_character), traits = Hex(_traits),
        directGameFieldWrites = _combatActivation.DirectFixtureWrites, nativeCombatFixture = _combatFixture,
        firstMagicStageFixtureRequested = _prepareFirstMagicStage, combatActivationBegun = _combatActivation.Begun,
        nativeCombatActivationCompleted = _combatActivation.Completed, sharedStateImplemented = false, fullE3Complete = false };

    internal void ObserveWorld(uint tid, ref Win32Native.X86Context context, bool mediaVerified,
        NativeCallDispatcher calls, Action rearm)
    {
        Ensure(tid); Require(context.Eip == NativeCallDispatcher.WorldReturn, "Shared-state observation requires WorldRET.");
        if (_completed) return;
        Require(++_worlds <= 36000, "Shared-state observation world budget exceeded.");
        uint flow = TryU(0x755294), stack = TryU(flow + 0x1AC);
        uint top = stack <= 20 ? TryU(flow + 0x15C + stack * 4) : 0;
        uint level = TryU(flow + 0x1B0), pending = TryU(flow + 0x1B8);
        if (_retired)
        {
            if (_pending == 0) _completed = true;
            return; // Never read historical player/component addresses after unload.
        }
        uint profile = TryU(0x765AD4), record = TryU(profile + 0x2B4), root = TryU(record + 0x24);
        bool ready = mediaVerified && level == 47 && pending == 0 && top >= 0x10000 &&
            TryU(top + 0x10) == 47 && TryU(profile) == 0x6F5A44 && TryU(record) == 0x710F80 &&
            TryU(root) == 0x6DC4F4 && TryU(root + 0x3C) >= 0x10000;
        if (!_started)
        {
            if (!ready)
            {
                if (_worlds <= 2 || _worlds % 120 == 0)
                    _log(new { @event = "shared-state-await-battle", world = _worlds, level, pending,
                        top = Hex(top), topTable = Hex(TryU(top)), playerRecord = Hex(record) });
                return;
            }
            Require(TypeName(U(top)) == "wxBattleChallengeGameFlowState", "Level47 is not the original BattleChallenge state.");
            _profile = profile; _record = record; _root = root; _battle = top;
            var components = Components(record);
            _character = components.Single(p => U(p) == 0x6F6438);
            _traits = components.Single(p => U(p) == 0x6F6990);
            _machine = components.Single(p => U(p) == 0x6FA288);
            _command = Pointer(_character + 0x130);
            Require(U(_character + 0x138) == _traits && U(_traits + 0x24) == record &&
                U(_traits + 0x18C) == _character, "Battle local Character/PlayerTraits owners differ.");
            _started = true; _began = Environment.TickCount64;
            _log(new { @event = "shared-state-battle-ready", level, stateTable = Hex(U(top)),
                profile = Hex(profile), record = Hex(record), root = Hex(root), character = Hex(_character),
                traits = Traits(_traits), components = components.Select(p => new { address = Hex(p),
                    table = Hex(U(p)), name = TypeName(U(p)) }).ToArray(), entityInventory = Inventory() });
            InstallObservations();
        }
        Require(profile == _profile && record == _record && root == _root && U(_traits) == 0x6F6990,
            "Shared-state observation lost its captured player ownership before unload.");
        if (_combatFixture && _combatActivation.TryActivate(tid, ref context, calls, _profile, _record, _traits, rearm)) return;
        if (ready) ++_readyWorlds;
        if (_readyWorlds <= 2 || _worlds % 120 == 0)
            _log(new { @event = "shared-state-world", world = _worlds, ready, level, pending,
                traits = Traits(_traits), counts = new Dictionary<string, uint>(_counts) });
        if (Environment.TickCount64 - _began >= (_combatFixture ? 240000 : 90000) && _pending == 0 && !calls.Pending)
        {
            _completed = true;
            _log(new { @event = "shared-state-observation-complete", readyWorlds = _readyWorlds,
                readOnly = !_combatFixture, fullE3Complete = false, actualNativeUnload = false });
        }
    }

    private void InstallObservations()
    {
        Hook(0x5F86C0, 6, "unload", (uint tid, ref Win32Native.X86Context c) =>
        {
            Require(_pending == 0, "Shared-state native unload entered during an unpaired resource call.");
            Require(c.Ecx == _battle && U(_battle + 0x10) == 47, "Shared-state unload lost its captured BattleChallenge receiver.");
            _retired = true;
            _log(new { @event = "shared-state-native-unload", receiver = Hex(c.Ecx),
                historicalPlayerReadsNowForbidden = true, ownObjectsCreated = 0 });
        });
        ObserveMessageCall(0x4FCF00, "damage", true);
        ObserveMessageCall(0x4FE750, "player-hit-front", true);
        ObserveProfileCall(0x4E7470, "gem-award");
        ObserveStarAward();
        Hook(0x4E2D3E, 1, "input-return", (uint tid, ref Win32Native.X86Context c) => ObserveInput());
        foreach (uint site in new uint[] { 0x525605, 0x5256F3 }) ObserveCombatGate(site);
    }

    private void ObserveInput()
    {
        byte[] command = Read(_command, 0x24);
        if (command[0x20] != 0) Add("input.primary-attack");
        if (command[0x21] != 0) Add("input.secondary-attack");
        if (command[0x1F] != 0) Add("input.defend");
        if (_counts.GetValueOrDefault("input-return") <= 2 || command[0x1F] != 0 || command[0x20] != 0 || command[0x21] != 0)
            Sample("input.intent", new { defend = command[0x1F], primaryAttack = command[0x20], secondaryAttack = command[0x21],
                combatEnabled = Read(_profile + 0x510, 1)[0], mode512 = Read(_profile + 0x512, 1)[0],
                kind = U(_machine + 0x128), state = U(_machine + 0x130), previousFlags = Hex(U(_machine + 0x140)),
                nextFlags = Hex(U(_machine + 0x144)), suppressionRaw = U(_machine + 0x294), attackTimer = U(_machine + 0x298) });
    }

    private void ObserveStarAward()
    {
        Hook(0x58C270, 6, "star-award", (uint tid, ref Win32Native.X86Context c) =>
        {
            uint star = c.Ecx, table = U(star), esp = c.Esp, caller = U(esp), value = U(star + 0x34);
            Require(table == 0x702160 && U(0x765AD4) == _profile,
                "Star award receiver/profile is not the captured native type.");
            // PlayerProfile factory allocates exactly2CB8 bytes. This is a
            // bounded before/after witness, not a guessed counter field.
            byte[] before = Read(_profile, 0x2CB8);
            Require(++_pending <= 32, "Star award nesting exceeded.");
            Sample("star-award.entry", new { star = Hex(star), value, profile = Hex(_profile) });
            _host.Breakpoints.AddReturn(caller, tid, "SharedState.star-award.return",
                (uint rt, uint at, ref Win32Native.X86Context rc) =>
                {
                    Ensure(rt); Require(!_retired && at == caller && rc.Esp == checked(esp + 4) && _pending > 0 &&
                        U(0x765AD4) == _profile, "Star award return owner/stack mismatch.");
                    --_pending; byte[] after = Read(_profile, before.Length);
                    var changed = Enumerable.Range(0, before.Length).Where(i => before[i] != after[i]).ToArray();
                    Require(changed.Length <= 512, "Star award changed more than its bounded profile witness budget.");
                    if (changed.Length != 0) Add("star-award.profile-changed");
                    Sample("star-award.return", new { star = Hex(star), value, profile = Hex(_profile),
                        profileBytes = before.Length,
                        changed = changed.Select(i => new { offset = Hex((uint)i), before = before[i], after = after[i] }).ToArray(),
                        exactFieldMeaningClaimed = false, espMatched = true });
                });
        });
    }

    private void ObserveMessageCall(uint entry, string name, bool health)
    {
        Hook(entry, 6, name, (uint tid, ref Win32Native.X86Context c) =>
        {
            uint receiver = c.Ecx, table = U(receiver), esp = c.Esp, caller = U(esp), msg = Pointer(esp + 4);
            uint code = U(msg), sender = U(msg + 0x10), value = U(msg + 0x18), tail = U(msg + 0x1C);
            uint before = U(receiver + 0x124), maximum = U(receiver + 0x128), owner = U(receiver + 0x24);
            Require(++_pending <= 32, "Shared-state native pairing nesting exceeded.");
            Sample(name + ".entry", new { receiver = Hex(receiver), table = Hex(table), owner = Hex(owner),
                local = receiver == _traits, caller = Hex(caller), code = Hex(code), sender = Hex(sender),
                senderTableRaw = Hex(TryU(sender)), value, tail = Hex(tail), before, maximum });
            _host.Breakpoints.AddReturn(caller, tid, "SharedState." + name + ".return",
                (uint rt, uint at, ref Win32Native.X86Context rc) =>
                {
                    Ensure(rt); Require(at == caller && rc.Esp == checked(esp + 8) && _pending > 0,
                        "Shared-state native resource call returned with an unexpected stack.");
                    --_pending;
                    bool live = !_retired && TryU(receiver) == table;
                    uint? after = live && health ? U(receiver + 0x124) : null;
                    if (after is uint hp && hp != before) Add(name + ".health-changed");
                    Sample(name + ".return", new { receiver = Hex(receiver), live, local = receiver == _traits,
                        before, after, maximum, nativeCaller = Hex(caller), espMatched = true });
                });
        });
    }

    private void ObserveProfileCall(uint entry, string name)
    {
        Hook(entry, 6, name, (uint tid, ref Win32Native.X86Context c) =>
        {
            Require(c.Ecx == _profile, "Pickup award reached an unexpected profile.");
            uint esp = c.Esp, caller = U(esp), id = U(esp + 4);
            byte[] before = Read(_profile + 0x3B8, 0x36);
            Require(++_pending <= 32, "Shared-state pickup nesting exceeded.");
            Sample(name + ".entry", new { profile = Hex(_profile), id, before = Convert.ToHexString(before) });
            _host.Breakpoints.AddReturn(caller, tid, "SharedState." + name + ".return",
                (uint rt, uint at, ref Win32Native.X86Context rc) =>
                {
                    Ensure(rt); Require(!_retired && at == caller && rc.Esp == checked(esp + 8) && _pending > 0 &&
                        U(0x765AD4) == _profile, "Shared-state pickup return owner/stack mismatch.");
                    --_pending; byte[] after = Read(_profile + 0x3B8, before.Length);
                    if (!before.SequenceEqual(after)) Add(name + ".profile-changed");
                    Sample(name + ".return", new { profile = Hex(_profile), id,
                        before = Convert.ToHexString(before), after = Convert.ToHexString(after), espMatched = true });
                });
        });
    }

    private void ObserveCombatGate(uint site)
    {
        byte[] call = Image(site, 5);
        Require(call[0] == 0xE8 && unchecked((uint)((long)site + 5 + BitConverter.ToInt32(call, 1))) == 0x4E3870,
            "Bloom tutorial branch is not the pinned native profile query.");
        Hook(site, 5, "tutorial-query", (uint tid, ref Win32Native.X86Context c) =>
        {
            Require(c.Ecx == _profile && U(c.Esp) == 0x35, "Bloom tutorial query receiver or selector differs.");
            uint esp = c.Esp; Require(++_pending <= 32, "Bloom gate nesting exceeded.");
            _host.Breakpoints.AddReturn(site + 5, tid, "SharedState.tutorial-query.return",
                (uint rt, uint at, ref Win32Native.X86Context rc) =>
                {
                    Ensure(rt); Require(!_retired && at == site + 5 && rc.Esp == checked(esp + 4) && _pending > 0,
                        "Bloom combat query return stack differs.");
                    --_pending; uint result = rc.Eax & 0xFF;
                    Add(result == 0 ? "tutorial-query.false" : "tutorial-query.true");
                    Sample("tutorial-query.result", new { site = Hex(site), selector = 0x35, result,
                        attackPermissionClaimed = false,
                        nativeQueryUnmodified = true, profile = Hex(_profile) });
                });
        });
    }

    private delegate void StopObservation(uint tid, ref Win32Native.X86Context context);
    private void Hook(uint address, int bytes, string name, StopObservation observe)
    {
        byte[] expected = Image(address, bytes);
        Require(Read(address, bytes).SequenceEqual(expected), "Shared-state hook differs from the original image.");
        _log(new { @event = "shared-state-verified-stop", name, address = Hex(address),
            original = Convert.ToHexString(expected), observationOnly = true });
        BreakpointCallback repeat = null!;
        repeat = (uint tid, uint at, ref Win32Native.X86Context c) =>
        {
            Ensure(tid);
            if (_completed || _retired) return;
            Require(at == address && c.Eip == address && Read(at, bytes).SequenceEqual(expected),
                "Shared-state observation signature changed.");
            Add(name); observe(tid, ref c);
            if (!_retired) _host.Breakpoints.AddReturn(address, tid, "SharedState." + name, repeat);
        };
        _host.Breakpoints.AddReturn(address, _host.MainThreadId, "SharedState." + name, repeat);
    }

    private object Traits(uint traits) => new { address = Hex(traits), table = Hex(U(traits)),
        currentHealth = U(traits + 0x124), maximumHealth = U(traits + 0x128),
        currentMagicEnergyRaw = Hex(U(traits + 0x1A8)), hurtDelayMilliseconds = U(traits + 0x1D8) };

    private uint[] Components(uint record)
    {
        uint count = U(record + 0x20); Require(count is > 0 and <= 64, "Battle component count exceeds its bound.");
        return List(Pointer(record + 0x1C), count);
    }
    private object Inventory()
    {
        uint manager = Pointer(0x765ACC); Require(U(manager) == 0x70069C, "Unexpected battle entity manager.");
        var entities = List(Pointer(manager + 0x18), U(manager + 0x1C))
            .Concat(List(Pointer(manager + 0x24), U(manager + 0x28))).ToArray();
        return entities.GroupBy(U).Select(g => new { table = Hex(g.Key), name = TypeName(g.Key), count = g.Count() }).ToArray();
    }
    private uint[] List(uint head, uint count)
    {
        Require(count <= 4096, "Shared-state list budget exceeded.");
        uint link = Pointer(head), previous = head; var seen = new HashSet<uint>(); var result = new List<uint>();
        for (uint i = 0; i < count; ++i)
        {
            Require(link != head && seen.Add(link) && U(link + 4) == previous, "Shared-state list is not reciprocal.");
            result.Add(Pointer(link + 8)); previous = link; link = Pointer(link);
        }
        Require(link == head && U(head + 4) == previous, "Shared-state list count/sentinel differs."); return result.ToArray();
    }
    private string TypeName(uint table)
    {
        // Descriptor0 is the scene root node; all captured runtime tables use
        // the original literal type getter at slot10.
        uint getter = U(table + 0x10); byte[] original = Image(getter, 6);
        Require(original[0] == 0xB8 && original[5] == 0xC3 && Read(getter, 6).SequenceEqual(original),
            "Shared-state RTTI getter is outside its pinned literal contract.");
        return Win32Native.ReadAnsiString(_host.ProcessHandle, BitConverter.ToUInt32(original, 1) + 8, 64);
    }
    private void Sample(string name, object data)
    {
        Add(name); uint seen = _sampleCounts.GetValueOrDefault(name) + 1; _sampleCounts[name] = seen;
        uint quota = name == "tutorial-query.result" ? 4u : name == "input.intent" ? 16u : 64u;
        if (seen > quota || _samples.Count >= 512) return;
        var sample = new { name, data }; _samples.Add(sample);
        _log(new { @event = "shared-state-native-sample", name, data });
    }
    private void Add(string name) { uint value = _counts.GetValueOrDefault(name) + 1; Require(value <= 100000, "Shared-state callback budget exceeded."); _counts[name] = value; }
    private byte[] Image(uint at, int bytes)
    { Require(at is >= 0x401000 and < 0x740000, "Shared-state image lookup outside original code."); return _image.GetSectionData(checked((int)(at - 0x400000))).GetContent(0, bytes).ToArray(); }
    private uint Pointer(uint at) { uint value = U(at); Require(value >= 0x10000, "Shared-state null pointer."); return value; }
    private uint U(uint at) => BitConverter.ToUInt32(Read(at, 4));
    private uint TryU(uint at) { if (at < 0x10000) return 0; try { return U(at); } catch (System.ComponentModel.Win32Exception) { return 0; } }
    private byte[] Read(uint at, int bytes) { Ensure(_host.MainThreadId); Require(at >= 0x10000 && bytes is > 0 and <= 0x2CB8, "Shared-state read exceeds its bound."); return Win32Native.ReadMemory(_host.ProcessHandle, at, bytes); }
    private void Ensure(uint tid) => Require(_host.IsAtDebugStop && tid == _host.MainThreadId && Environment.CurrentManagedThreadId == _ownerThread,
        "Shared-state observation requires its owned main-thread stop.");
    private static string Hex(uint value) => $"0x{value:X8}";
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidDataException(message); }
}
