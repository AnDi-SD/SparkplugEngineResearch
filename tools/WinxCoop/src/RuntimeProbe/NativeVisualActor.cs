using System.Reflection.PortableExecutable;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// Own Stage-A experiment: a passive visual graph and stock actor, with no
/// second wx player, input, camera, physics or story component. Exact native
/// calls are gated by the pinned image and live bytes, on the original thread.
/// </summary>
internal sealed class NativeVisualActor(ProbeConfiguration config, PEReader image, Action<object> log)
{
    private DebuggerHost _host = null!;
    private OwnedRemoteBuffer? _data;
    private uint _profile, _record, _localRoot, _parent, _input, _inputState, _inputDevice, _camera;
    private uint _source, _wrapper, _actor, _clip;
    private int _stage, _part, _liveUpdates, _cycles;
    private bool _sourceOwner, _wrapperOwner;
    private HashSet<uint> _localNodes = [];
    private VisualGraphSelection? _selection;
    private float[] _origin = [];
    internal bool EverCreated { get; private set; }
    internal bool Cleaned => _cycles == 2 && _stage == 30 && _source == 0 && _wrapper == 0 && _actor == 0;
    internal object Summary => new { stage = _stage, completedCycles = _cycles, everCreated = EverCreated,
        cleaned = Cleaned, source = Hex(_source), wrapper = Hex(_wrapper), actor = Hex(_actor),
        borrowedClip = Hex(_clip), sourceOwner = _sourceOwner, wrapperOwner = _wrapperOwner,
        data = _data?.Summary, unloadSafetyQualified = false };

    internal void Step(DebuggerHost host, uint tid, ref Win32Native.X86Context ctx,
        NativeCallDispatcher calls, Action resume)
    {
        _host = host;
        if (!host.IsAtDebugStop || tid != host.MainThreadId || ctx.Eip != NativeCallDispatcher.WorldReturn || calls.Pending)
            throw new InvalidOperationException("Visual experiment requires the owned world stop without a pending call");
        if (_stage == 30) return;
        if (_stage != 0) VerifyLocalIdentity();
        switch (_stage)
        {
            case 0:
                CaptureLocalIdentity();
                var local = new VisualGraphReader(host.ProcessHandle).Read(_localRoot);
                LogGraph("local-before", local);
                if (!local.Success) throw new InvalidDataException("Cannot inventory local visual tree: " + local.Error);
                _localNodes = local.Graph.Nodes.Select(n => n.Address).ToHashSet();
                _data = new OwnedRemoteBuffer(host, 1024);
                _data.WriteAsciiZ(0, Path.Combine(config.MediaDirectory, "Characters", "Bloom", "bloom_jeans.smo"));
                _stage = 1;
                Call(host, tid, ref ctx, calls, resume,0x458260, U(0x75DB78), [_data.Address], false, value =>
                {
                    if (value == 0 || _localNodes.Contains(value)) throw new InvalidDataException("Fresh SMO load failed or aliased local root");
                    _source = value;
                });
                break;
            case 1:
                var source = new VisualGraphReader(host.ProcessHandle).Read(_source, _localNodes);
                LogGraph("fresh-source", source);
                if (!source.Success) throw new InvalidDataException("Fresh source graph rejected: " + source.Error);
                if (U(_source + 0x2C) != 0 || Count(_source) != 0)
                    throw new InvalidDataException("Fresh source must have no parent and zero incoming owners before adapter retain");
                Retain(_source); _sourceOwner = true;
                _selection = VisualGraphReader.SelectVisualParts(source.Graph);
                log(new { @event = "visual-selection", selection = _selection });
                if (!_selection.Success) throw new InvalidDataException("No passive visual subtree: " + _selection.Error);
                _stage = 2;
                Call(host, tid, ref ctx, calls, resume,0x421E20, 0, [], true, value =>
                {
                    if (value == 0 || U(value) != VisualGraphReader.NodeVtable || Count(value) != 0 || U(value + 0x2C) != 0)
                        throw new InvalidDataException("Fresh wrapper factory identity/ownership mismatch");
                    _wrapper = value; Retain(_wrapper); _wrapperOwner = true;
                });
                break;
            case 2:
                if (_part < _selection!.VisualRoots.Count)
                {
                    uint part = _selection.VisualRoots[_part++];
                    Call(host, tid, ref ctx, calls, resume, 0x421A60, _wrapper, [part], false, _ =>
                    {
                        if (U(part + 0x2C) != _wrapper) throw new InvalidDataException("Visual reparent did not establish reciprocal owner");
                    });
                }
                else
                {
                    var own = new VisualGraphReader(host.ProcessHandle).Read(_wrapper, _localNodes);
                    LogGraph("passive-wrapper", own);
                    if (!own.Success || own.Graph.Skins.Count == 0 || own.Graph.Nodes.Any(n => n.Collisions.Items.Count != 0 ||
                            n.Kind is NativeVisualNodeKind.Light or NativeVisualNodeKind.LightData || n.Scene != 0))
                        throw new InvalidDataException("Passive wrapper has an unsupported relation or aliases a local node");
                    _stage = 3;
                }
                break;
            case 3:
                _stage = 4;
                Call(host, tid, ref ctx, calls, resume,0x5A3620, 0, [], true, value =>
                {
                    if (value == 0 || U(value) != 0x703F80) throw new InvalidDataException("Stock actor factory identity mismatch");
                    _actor = value;
                });
                break;
            case 4:
                _stage = 5;
                Call(host, tid, ref ctx, calls, resume,0x5A35C0, _actor, [_wrapper], false, _ => { });
                break;
            case 5:
                uint begin = U(_actor + 0x40), end = U(_actor + 0x44);
                if (begin == 0 || end <= begin || (end - begin) % 4 != 0 || (end - begin) / 4 > 256)
                    throw new InvalidDataException("Stock actor did not create a bounded controller vector");
                log(new { @event = "visual-controller-count", count = (end - begin) / 4 });
                _stage = 13;
                ReleaseSource(host, tid, ref ctx, calls, resume);
                break;
            case 13:
                _clip = BorrowLocalClip();
                byte[] request = new byte[0x38];
                Put(request, 0, _clip); Put(request, 4, 1); Put(request, 0xC, 0x3F800000); Put(request, 0x30, 0x3F800000);
                _data!.Write(512, request);
                log(new { @event = "visual-actor-bound", actor = Hex(_actor),
                    clip = Hex(_clip), duration = Float(_clip + 0x14) });
                _stage = 6;
                Call(host, tid, ref ctx, calls, resume,0x5A1E30, _actor, [_data.Address + 512], false, value =>
                {
                    if (value == uint.MaxValue || value >= U(_actor + 0x2C)) throw new InvalidDataException("Stock actor rejected animation request");
                });
                break;
            case 6:
                WriteTransform(_origin[0] + 180, _origin[1], _origin[2]);
                _stage = 7;
                Call(host, tid, ref ctx, calls, resume,0x421A60, _parent, [_wrapper], false, _ =>
                {
                    if (U(_wrapper + 0x2C) != _parent || U(_wrapper + 0x3C) != U(_localRoot + 0x3C))
                        throw new InvalidDataException("Remote wrapper was not registered into the local scene");
                    EverCreated = true;
                    log(new { @event = "visual-attached", cycle = _cycles + 1, wrapper = Hex(_wrapper), actor = Hex(_actor),
                        localRoot = Hex(_localRoot), scene = Hex(U(_wrapper + 0x3C)), position = new[] { _origin[0] + 180, _origin[1], _origin[2] } });
                });
                break;
            case 7:
                ++_liveUpdates;
                if (_liveUpdates % 120 == 0)
                    log(new { @event = "visual-live", cycle = _cycles + 1, updates = _liveUpdates,
                        wrapper = Hex(_wrapper), actor = Hex(_actor), playbackSample = Float(U(_actor + 0x28) + 0x34),
                        localPosition = new[] { Float(_localRoot + 0x20), Float(_localRoot + 0x24), Float(_localRoot + 0x28) },
                        remoteWorldPosition = new[] { Float(_wrapper + 0x74), Float(_wrapper + 0x78), Float(_wrapper + 0x7C) } });
                if (_liveUpdates >= 2400) _stage = 8;
                break;
            case 8:
                _stage = 9;
                Call(host, tid, ref ctx, calls, resume,0x5A21C0, _actor, [], false, _ => { });
                break;
            case 9:
                _stage = 10;
                Call(host, tid, ref ctx, calls, resume,0x5A35A0, _actor, [1], false, _ => { _actor = 0; _clip = 0; });
                break;
            case 10:
                _stage = 11;
                Call(host, tid, ref ctx, calls, resume,0x4217E0, _wrapper, [], false, _ =>
                {
                    if (U(_wrapper + 0x2C) != 0) throw new InvalidDataException("Remote wrapper remained parented after detach");
                });
                break;
            case 11:
                if (!_wrapperOwner || Count(_wrapper) != 1) throw new InvalidDataException("Unexpected final wrapper ownership");
                uint delete = U(U(_wrapper));
                SetCount(_wrapper, 0); _wrapperOwner = false;
                _stage = 12;
                Call(host, tid, ref ctx, calls, resume,delete, _wrapper, [1], false, _ => { _wrapper = 0; });
                break;
            case 12:
                _data!.Release(calls.Pending); _data = null;
                ++_cycles;
                log(new { @event = "visual-cycle-cleaned", completedCycles = _cycles, localIdentityPreserved = true });
                _stage = _cycles == 2 ? 30 : 0;
                _part = 0; _liveUpdates = 0;
                break;
            default: throw new InvalidOperationException("Unknown visual experiment state");
        }
    }

    private void Call(DebuggerHost host, uint tid, ref Win32Native.X86Context ctx,
        NativeCallDispatcher calls, Action resume, uint fn, uint receiver, uint[] args, bool cdecl, Action<uint> done)
    {
        VerifyFunction(fn);
        calls.Begin(host, tid, ref ctx, fn, receiver, args, cdecl, value => { done(value); resume(); });
    }

    private void ReleaseSource(DebuggerHost host, uint tid, ref Win32Native.X86Context ctx,
        NativeCallDispatcher calls, Action resume)
    {
        if (!_sourceOwner || Count(_source) != 1 || U(_source + 0x2C) != 0)
            throw new InvalidDataException("Unexpected source ownership after extraction");
        uint delete = U(U(_source)); VerifyFunction(delete);
        SetCount(_source, 0); _sourceOwner = false;
        calls.Begin(host, tid, ref ctx, delete, _source, [1], false, _ => { _source = 0; resume(); });
    }

    private void CaptureLocalIdentity()
    {
        _profile = U(0x765AD4); _record = U(_profile + 0x2B4); _localRoot = U(_record + 0x24);
        _parent = U(_localRoot + 0x2C); _input = U(0x7552A0); _inputState = U(_input + 0x18);
        _inputDevice = U(_input + 0x1C); _camera = U(U(0x755274) + 0x1C);
        if (U(_profile) != 0x6F5A44 || U(_record) != 0x710F80 || U(_localRoot) != VisualGraphReader.NodeVtable ||
            _parent == 0 || U(_input) != 0x6F5508 || U(_record + 0x20) != 11)
            throw new InvalidDataException("Local player ownership gate rejected the current scene");
        _origin = [Float(_localRoot + 0x20), Float(_localRoot + 0x24), Float(_localRoot + 0x28)];
        if (_origin.Any(v => !float.IsFinite(v))) throw new InvalidDataException("Local position is not finite");
    }
    private void VerifyLocalIdentity()
    {
        if (U(0x765AD4) != _profile || U(_profile + 0x2B4) != _record || U(_record + 0x24) != _localRoot ||
            U(_record + 0x20) != 11 || U(_localRoot + 0x2C) != _parent || U(0x7552A0) != _input ||
            U(_input + 0x18) != _inputState || U(_input + 0x1C) != _inputDevice || U(U(0x755274) + 0x1C) != _camera)
            throw new InvalidDataException("Local input, camera or player ownership changed during passive visual experiment");
    }
    private uint BorrowLocalClip()
    {
        uint head = U(_record + 0x1C), link = U(head), controller = 0;
        for (int i = 0; i < 11 && link != head; ++i, link = U(link))
        { uint item = U(link + 8); if (U(item) == 0x6F670C) controller = item; }
        if (controller == 0) throw new InvalidDataException("Local animation controller not found");
        uint localActor = U(controller + 0x128), states = U(localActor + 0x28), capacity = U(localActor + 0x2C);
        if (U(localActor) != 0x703F80 || localActor == _actor || states == 0 || capacity is 0 or > 64)
            throw new InvalidDataException("Local stock actor cannot supply a borrowed clip");
        for (uint i = 0; i < capacity; ++i)
        {
            uint playback = checked(states + i * 0x60), clip = U(playback);
            if (U(playback + 0x4C) == 0 || clip == 0) continue;
            if (U(clip) != 0x6DE6CC || !float.IsFinite(Float(clip + 0x14)) || Float(clip + 0x14) <= 0 ||
                U(clip + 0x1C) == 0 || U(clip + 0x20) is 0 or > 256)
                throw new InvalidDataException("Active borrowed animation layout rejected");
            return clip;
        }
        throw new InvalidDataException("Local actor has no active animation to borrow");
    }
    private void WriteTransform(float x, float y, float z)
    {
        if (!_wrapperOwner || U(_wrapper) != VisualGraphReader.NodeVtable) throw new InvalidDataException("No owned plain wrapper");
        // Own diagnostic producer follows the measured Node ABI. This is not a
        // reconstructed native setter: local PRS writes + dirty bit1, caches
        // and descendant world transforms remain stock engine work.
        Write(_wrapper + 0x20, BitConverter.GetBytes(x).Concat(BitConverter.GetBytes(y)).Concat(BitConverter.GetBytes(z)).ToArray());
        Write(_wrapper + 0x30, Win32Native.ReadMemory(_host.ProcessHandle, _localRoot + 0x30, 12));
        Write(_wrapper + 0x40, Win32Native.ReadMemory(_host.ProcessHandle, _localRoot + 0x40, 36));
        Write(_wrapper + 0xB0, BitConverter.GetBytes(U(_wrapper + 0xB0) | 1u));
    }
    private void VerifyFunction(uint function)
    {
        if (function < 0x401000 || function > 0x6DA000) throw new InvalidDataException("Native function outside pinned code image");
        byte[] expected = image.GetSectionData(checked((int)(function - 0x400000))).GetContent(0, 6).ToArray();
        if (!Win32Native.ReadMemory(_host.ProcessHandle, function, expected.Length).SequenceEqual(expected))
            throw new InvalidDataException($"Native live signature mismatch at {Hex(function)}");
    }
    private void LogGraph(string phase, VisualGraphReadResult graph) => log(new { @event = "visual-graph", phase, result = graph });
    private void Retain(uint pointer) { ushort old = Count(pointer); if (old == ushort.MaxValue) throw new InvalidDataException("Reference overflow"); SetCount(pointer, (ushort)(old + 1)); }
    private ushort Count(uint pointer) => BitConverter.ToUInt16(Win32Native.ReadMemory(_host.ProcessHandle, pointer + 8, 2));
    private void SetCount(uint pointer, ushort value) => Write(pointer + 8, BitConverter.GetBytes(value));
    private void Write(uint address, byte[] bytes)
    {
        if (!_host.IsAtDebugStop) throw new InvalidOperationException("Remote data mutation outside debug stop");
        Win32Native.WriteMemory(_host.ProcessHandle, address, bytes, false);
        if (!Win32Native.ReadMemory(_host.ProcessHandle, address, bytes.Length).SequenceEqual(bytes)) throw new InvalidDataException("Native data write readback mismatch");
    }
    private uint U(uint address) { if (address < 0x10000) throw new InvalidDataException("Null or low native pointer"); return Win32Native.ReadUInt32(_host.ProcessHandle, address); }
    private float Float(uint address) => BitConverter.ToSingle(Win32Native.ReadMemory(_host.ProcessHandle, address, 4));
    private static void Put(byte[] target, int offset, uint value) => BitConverter.GetBytes(value).CopyTo(target, offset);
    private static string Hex(uint value) => $"0x{value:X8}";
}
