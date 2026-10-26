using System.ComponentModel;
using System.Reflection.PortableExecutable;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>Bounded borrowed reads while the owned process is at a debug stop.</summary>
internal sealed class ActorObserver(ProbeConfiguration config, ExecutableProfile profile, PEReader image, Action<object> log)
{
    private DebuggerHost _host = null!;
    private uint _updates, _samples, _readySamples, _factoryReturns;
    private bool _mediaVerified;
    private NativeCallDispatcher _calls = new(log);
    private bool _callCheckBegun, _callCheckPassed;
    private uint _localCharacter;
    private NativeVisualActor? _visual;
    private readonly HashSet<uint> _machines = [];
    private readonly HashSet<uint> _bodyReceivers = [];
    internal uint ReadySamples => _readySamples;
    internal bool RemoteCreated => _visual?.EverCreated == true;
    internal bool ModeSucceeded => config.Mode switch
    {
        "observe" => _readySamples > 0,
        "native-call-check" => _callCheckPassed,
        "visual-actor" => _visual?.Cleaned == true,
        _ => false
    };
    internal object Summary => new { worldUpdates = _updates, samples = _samples,
        readySamples = _readySamples, mediaVerified = _mediaVerified, factoryReturns = _factoryReturns,
        observedBloomMachines = _machines.Select(Hex).ToArray(), observedBodyReceivers = _bodyReceivers.Select(Hex).ToArray(),
        nativeCallCheckPassed = _callCheckPassed, nativeCalls = _calls.Summary, visual = _visual?.Summary };
    internal void CheckTimeout() => _calls.CheckTimeout();

    internal void Install(DebuggerHost host)
    {
        _host = host;
        if (host.ImageBase != 0x400000) throw new InvalidDataException("Image base mismatch");
        // Native scene-manager RET after currentScene has been cleared. This
        // establishes an observation boundary, not a mutation permission.
        Persistent(0x45A802, 8, "SceneManager.return", (uint tid, uint at, ref Win32Native.X86Context ctx) =>
        {
            ++_updates;
            if (_updates % config.SampleEvery == 0 && _samples < 512) Snapshot(tid);
            _calls.ReleaseRetiredAtWorldStop(host, tid, ref ctx);
            if (config.Mode == "visual-actor" && _readySamples >= 15)
            {
                _visual ??= new NativeVisualActor(config, image, log);
                _visual.Step(host, tid, ref ctx, _calls, () => _resumeObservation?.Invoke());
            }
            if (config.Mode == "native-call-check" && !_callCheckBegun && _readySamples >= 15 && _localCharacter != 0)
            {
                byte[] expected = image.GetSectionData(0x4F3DF0 - 0x400000).GetContent(0, 3).ToArray();
                if (!Win32Native.ReadMemory(host.ProcessHandle, 0x4F3DF0, 3).SequenceEqual(expected))
                    throw new InvalidDataException("Constant predicate live signature mismatch");
                _callCheckBegun = true;
                _calls.Begin(host, tid, ref ctx, 0x4F3DF0, _localCharacter, [], false, result =>
                {
                    _callCheckPassed = (result & 255) == 1;
                    log(new { @event = "native-call-check-result", result, passed = _callCheckPassed });
                    if (!_callCheckPassed) throw new InvalidDataException("Native character predicate did not return true");
                    _resumeObservation?.Invoke();
                });
            }
        });
        Persistent(0x4044C0, 6, "Bloom.factory", (uint tid, uint at, ref Win32Native.X86Context ctx) =>
        {
            uint ret = U(ctx.Esp);
            host.Breakpoints.AddReturn(ret, tid, "Bloom.factory.return", (uint rt, uint ra, ref Win32Native.X86Context rc) =>
            {
                ++_factoryReturns;
                if (rc.Eax != 0 && U(rc.Eax) == 0x6FA288 && _machines.Count < 32) _machines.Add(rc.Eax);
                log(new { @event = "bloom-factory-return", threadId = rt, receiver = Hex(rc.Eax), vtable = Hex(TryU(rc.Eax)) });
            });
        });
        ExecutableCheckpoint media = profile.Checkpoints.Single(p => p.Kind == ExecutableCheckpointKind.FindMediaPath);
        Persistent(0x400000 + media.RelativeVirtualAddress, media.VerificationPattern.Length, "MediaPath.return-validation",
            (uint tid, uint at, ref Win32Native.X86Context ctx) =>
            {
                uint manager = ctx.Ecx, ret = U(ctx.Esp);
                host.Breakpoints.AddReturn(ret, tid, "MediaPath.return", (uint rt, uint ra, ref Win32Native.X86Context rc) =>
                {
                    string actual = Text(U(manager + 0x14), 250);
                    _mediaVerified = actual.TrimEnd('\\').Equals(config.MediaDirectory.TrimEnd('\\'), StringComparison.OrdinalIgnoreCase);
                    log(new { @event = "media-path-validation", manager = Hex(manager), actual, matched = _mediaVerified });
                    if (!_mediaVerified) throw new InvalidDataException("Native MediaPath did not select the isolated corpus");
                });
            });
        foreach (ExecutableCheckpoint body in profile.Checkpoints.Where(p => p.Kind == ExecutableCheckpointKind.BloomBodyLoad))
            Persistent(0x400000 + body.RelativeVirtualAddress, body.VerificationPattern.Length, "BodyLoad.entry",
                (uint tid, uint at, ref Win32Native.X86Context ctx) =>
                {
                    if (_bodyReceivers.Count < 32) _bodyReceivers.Add(ctx.Ecx);
                    log(new { @event = "body-load-entry", threadId = tid, receiver = Hex(ctx.Ecx), vtable = Hex(TryU(ctx.Ecx)) });
                });
    }

    private Action? _resumeObservation;
    private void Persistent(uint address, int length, string name, BreakpointCallback callback)
    {
        byte[] expected = image.GetSectionData(checked((int)(address - 0x400000))).GetContent(0, length).ToArray();
        byte[] actual = Win32Native.ReadMemory(_host.ProcessHandle, address, length);
        if (!actual.SequenceEqual(expected)) throw new InvalidDataException($"Live signature mismatch for {name} at {Hex(address)}");
        log(new { @event = "verified-observation-stop", name, address = Hex(address), signature = Convert.ToHexString(actual) });
        // A verified native instruction is observed repeatedly on the same
        // thread using the existing return-probe/rearm implementation.
        BreakpointCallback repeat = null!;
        repeat = (uint tid, uint at, ref Win32Native.X86Context ctx) =>
        {
            callback(tid, at, ref ctx);
            if (address == NativeCallDispatcher.WorldReturn && _calls.Pending)
                _resumeObservation = () => _host.Breakpoints.AddReturn(at, tid, name, repeat);
            else _host.Breakpoints.AddReturn(at, tid, name, repeat);
        };
        _host.Breakpoints.AddReturn(address, _host.MainThreadId, name, repeat);
    }

    private void Snapshot(uint thread)
    {
        ++_samples;
        uint engine = TryU(0x755274), defaultScene = TryU(engine + 0x18), defaultCamera = TryU(engine + 0x1C);
        uint manager = TryU(0x75DB90), flow = TryU(0x755294), input = TryU(0x7552A0);
        uint state = TryU(flow + 0x1B0), pending = TryU(flow + 0x1B8);
        int stack = unchecked((int)TryU(flow + 0x1AC));
        uint active = stack is >= 0 and <= 20 ? TryU(flow + 0x15C + (uint)stack * 4) : 0;
        uint activeId = TryU(active + 0x10);
        uint playerProfile = TryU(0x765AD4), record = TryU(playerProfile + 0x2B4);
        uint root = TryU(record + 0x24);
        bool recordTyped = record != 0 && TryU(record) == 0x710F80;
        bool nodeTyped = false;
        uint actorScene = 0;
        if (root >= 0x10000)
        {
            try
            {
                byte[] prefix = Win32Native.ReadMemory(_host.ProcessHandle, root, 0xB4);
                actorScene = BitConverter.ToUInt32(prefix, 0x3C);
                nodeTyped = BitConverter.ToUInt32(prefix, 0) == 0x6DC4F4 &&
                    actorScene != 0 && TryU(actorScene) == 0x6E7358 &&
                    new[] { 0x20, 0x24, 0x28, 0x74, 0x78, 0x7C }.All(o => float.IsFinite(BitConverter.ToSingle(prefix, o)));
            }
            catch (Win32Exception) { }
        }
        var objects = new List<object>();
        bool bloomFound = false, controllerFound = false, listConsistent = false;
        if (recordTyped)
        {
            uint head = U(record + 0x1C), count = U(record + 0x20), link = TryU(head);
            if (head != 0 && count <= 128)
            {
                var visited = new HashSet<uint>();
                for (uint i = 0; i < count && link != head && link != 0 && visited.Add(link); ++i)
                {
                    uint item = U(link + 8), table = TryU(item);
                    if (table == 0x6F6438) _localCharacter = item;
                    bloomFound |= table == 0x6FA288;
                    controllerFound |= table == 0x6F64F8;
                    bool knownEntity = table is 0x6FA288 or 0x6F64F8 or 0x6F6438 or 0x6F670C;
                    objects.Add(new { address = Hex(item), vtable = Hex(table), sharedNameEntry = Hex(TryU(item + 0x10)),
                        reference24 = knownEntity ? Hex(TryU(item + 0x24)) : null,
                        node18 = knownEntity ? Node(TryU(item + 0x18)) : null,
                        knownType = table switch { 0x6FA288 => "wxBloomStateMachine", 0x6F64F8 => "wxCharacterMoveCtrl",
                            0x6F6438 => "wxCharacter", 0x6F670C => "wxAnimationController", _ => "unclassified" } });
                    link = U(link);
                }
                listConsistent = objects.Count == count && link == head;
            }
        }
        bool ready = _mediaVerified && state is >= 1 and <= 50 && pending == 0 && activeId == state &&
            recordTyped && nodeTyped && listConsistent && bloomFound && controllerFound &&
            manager != 0 && TryU(manager + 0x20) == 0;
        if (ready) ++_readySamples;
        log(new { @event = "world-observation", update = _updates, sample = _samples, threadId = thread,
            stoppedAt = "SceneManager.return", ready, mediaVerified = _mediaVerified,
            flow = new { address = Hex(flow), state, pending, stack, active = Hex(active), activeId },
            engine = Hex(engine), coreDefaultScene = Hex(defaultScene), coreDefaultCamera = Hex(defaultCamera),
            actorScene = Hex(actorScene), nodeTyped, listConsistent, bloomFound, controllerFound,
            sceneManager = Hex(manager), managerCurrentScene = Hex(TryU(manager + 0x20)),
            input = new { address = Hex(input), vtable = Hex(TryU(input)), state = Hex(TryU(input + 0x18)), device = Hex(TryU(input + 0x1C)) },
            playerProfile = Hex(playerProfile), playerRecord = Hex(record), recordTyped,
            localBodyRoot = Hex(TryU(playerProfile + 0x18)),
            playerRoot = Node(root), objects,
            machines = _machines.Select(p => new { address = Hex(p), table = Hex(TryU(p)),
                owner124 = Hex(TryU(p + 0x124)), control12C = Hex(TryU(p + 0x12C)), current130 = TryU(p + 0x130) }).ToArray() });
    }

    private object? Node(uint node)
    {
        if (node < 0x10000) return null;
        try
        {
            byte[] bytes = Win32Native.ReadMemory(_host.ProcessHandle, node, 0xB4);
            uint Word(int offset) => BitConverter.ToUInt32(bytes, offset);
            float? Float(int offset) { float value = BitConverter.ToSingle(bytes, offset); return float.IsFinite(value) ? value : null; }
            return new { address = Hex(node), vtable = Hex(Word(0)), sharedNameEntry = Hex(Word(0x10)),
                parent = Hex(Word(0x2C)), sceneLink = Hex(Word(0x3C)), flags = Word(0xB0),
                local = new[] { Float(0x20), Float(0x24), Float(0x28) },
                world = new[] { Float(0x74), Float(0x78), Float(0x7C) } };
        }
        catch (Win32Exception) { return new { address = Hex(node), unreadable = true }; }
    }
    private uint U(uint address) => Win32Native.ReadUInt32(_host.ProcessHandle, address);
    private uint TryU(uint address) { if (address < 0x10000) return 0; try { return U(address); } catch (Win32Exception) { return 0; } }
    private string Text(uint address, int length) { if (address < 0x10000) return ""; try { return Win32Native.ReadAnsiString(_host.ProcessHandle, address, length); } catch (Win32Exception) { return ""; } }
    private static string Hex(uint value) => $"0x{value:X8}";
}
