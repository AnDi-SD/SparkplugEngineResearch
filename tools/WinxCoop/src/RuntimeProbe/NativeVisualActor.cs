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
    private uint _source, _wrapper, _actor, _clip, _playbackIndex;
    private uint _hairSource, _hairMaster, _bodyHead;
    private bool _hairSourceOwner;
    private uint[] _localRenderNodes = [], _remoteRenderNodes = [], _ownSkins = [];
    private NativeAppearanceColor? _appearance;
    private int _appearanceApplications;
    private readonly uint[] _clips = new uint[3];
    private static readonly string[] ClipNames = ["blidsh.san", "blwa.san", "bljusm.san"];
    private int _loadedClips, _activeClip, _wantedClip;
    private byte[] _scale = [], _orientation = [];
    private int _stage, _part, _liveUpdates, _cycles, _unloads;
    private bool _sourceOwner, _wrapperOwner, _wrapperDeletingObserved;
    private HashSet<uint> _localNodes = [];
    private VisualGraphSelection? _selection;
    private float[] _origin = [];
    private float _minimumObservedY = float.PositiveInfinity, _maximumObservedY = float.NegativeInfinity;
    internal bool EverCreated { get; private set; }
    internal bool HasActor => _actor != 0;
    internal bool Cleaned => (_cycles == 2 || (config.Mode == "visual-unload" && _unloads > 0)) &&
        _stage == 30 && _source == 0 && _hairSource == 0 && _wrapper == 0 && _actor == 0;
    internal object Summary => new { stage = _stage, completedCycles = _cycles, everCreated = EverCreated,
        cleaned = Cleaned, source = Hex(_source), wrapper = Hex(_wrapper), actor = Hex(_actor),
        borrowedClip = Hex(_clip), sourceOwner = _sourceOwner, wrapperOwner = _wrapperOwner,
        hairSource = Hex(_hairSource), hairSourceOwner = _hairSourceOwner, appearanceApplications = _appearanceApplications,
        data = _data?.Summary, nativeUnloadCleanups = _unloads,
        observedWorldMinimumY = float.IsFinite(_minimumObservedY) ? (float?)_minimumObservedY : null,
        observedWorldMaximumY = float.IsFinite(_maximumObservedY) ? (float?)_maximumObservedY : null };

    internal bool TryBeginUnload(DebuggerHost host, uint tid, ref Win32Native.X86Context ctx,
        uint entry, byte firstByte, NativeCallDispatcher calls)
    {
        if (!HasActor) return false;
        if ((entry is 0x4E8AA0 or 0x4E8C80) && ctx.Ecx != _profile ||
            (entry is 0x59AE80 or 0x59B190) && ctx.Ecx != U(0x765ADC))
            throw new InvalidDataException("Native unload receiver does not own this visual actor's borrowed resources");
        if (_stage < 7 || _source != 0 || _hairSource != 0 || !_wrapperOwner || calls.Pending)
            throw new InvalidOperationException("Unload arrived during an unfinished visual transaction; abort the owned case");
        _host = host;
        foreach (uint function in new uint[] { 0x5A21C0, 0x5A35A0, 0x4217E0 }) VerifyFunction(function);
        if (U(_wrapper + 0x2C) != _parent || Count(_wrapper) != 2)
            throw new InvalidDataException("Pre-unload requires exactly the parent and adapter wrapper owners");
        // Drop our one owner while the scene parent still retains the wrapper.
        // After Actor deletion, native Detach drops the final parent owner and
        // deletes the whole graph BEFORE stock resource/cache unload begins.
        SetCount(_wrapper, 1); _wrapperOwner = false;
        NativeCallSpec[] batch =
        [
            new(0x5A21C0, _actor, [], false),
            new(0x5A35A0, _actor, [1], false),
            new(0x4217E0, _wrapper, [], false)
        ];
        uint removedWrapper = _wrapper;
        uint deletingEntry = U(U(_wrapper)); VerifyFunction(deletingEntry);
        _wrapperDeletingObserved = false;
        BreakpointCallback witness = null!;
        witness = (uint wt, uint wa, ref Win32Native.X86Context wc) =>
        {
            if (wc.Ecx == removedWrapper)
            {
                if (Count(removedWrapper) != 0) throw new InvalidDataException("Final wrapper destructor entered without zero reference count");
                _wrapperDeletingObserved = true;
                log(new { @event = "visual-wrapper-deleting-destructor", wrapper = Hex(removedWrapper),
                    entry = Hex(wa), threadId = wt, referenceCountBeforeDelete = 0 });
            }
            else host.Breakpoints.AddReturn(wa, wt, "Visual.wrapper-delete-witness", witness);
        };
        host.Breakpoints.AddReturn(deletingEntry, tid, "Visual.wrapper-delete-witness", witness);
        _stage = 40;
        calls.BeginAtEntry(host, tid, ref ctx, entry, firstByte, batch, _ =>
        {
            // removedWrapper was deleted by native final-owner detach. Never
            // read its freed bytes to manufacture a post-delete check.
            if (!_wrapperDeletingObserved) throw new InvalidDataException("Native final-owner detach did not enter the observed wrapper destructor");
            _actor = 0; _clip = 0; _wrapper = 0;
            ++_unloads; _stage = 42;
            log(new { @event = "visual-pre-unload-cleaned", entry = Hex(entry), wrapper = Hex(removedWrapper),
                actorDeleted = true, finalParentOwnerReleasedByNativeDetach = true, originalEntryStillNotExecuted = true });
        });
        return true;
    }

    internal void Step(DebuggerHost host, uint tid, ref Win32Native.X86Context ctx,
        NativeCallDispatcher calls, Action resume)
    {
        _host = host;
        if (!host.IsAtDebugStop || tid != host.MainThreadId || ctx.Eip != NativeCallDispatcher.WorldReturn || calls.Pending)
            throw new InvalidOperationException("Visual experiment requires the owned world stop without a pending call");
        if (_stage == 30) return;
        if (_stage == 42)
        {
            _data!.Release(calls.Pending); _data = null; ++_cycles; _stage = 30;
            log(new { @event = "visual-post-unload-owner-released", completedCycles = _cycles, nativeUnloadCleanups = _unloads });
            return;
        }
        if (_stage != 0) VerifyLocalIdentity();
        switch (_stage)
        {
            case 0:
                CaptureLocalIdentity();
                var local = new VisualGraphReader(host.ProcessHandle).Read(_localRoot, inspectSkins: false);
                LogGraph("local-before", local);
                if (!local.Success) throw new InvalidDataException("Cannot inventory local visual tree: " + local.Error);
                _localNodes = local.Graph.Nodes.Select(n => n.Address).ToHashSet();
                _localRenderNodes = local.Graph.Nodes.Where(n => n.Kind == NativeVisualNodeKind.RenderNode).Select(n => n.Address).ToArray();
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
                            n.Kind is NativeVisualNodeKind.Light or NativeVisualNodeKind.LightData or NativeVisualNodeKind.DXLight || n.Scene != 0))
                        throw new InvalidDataException("Passive wrapper has an unsupported relation or aliases a local node");
                    _bodyHead = UniqueNamedNode(own.Graph, "Head");
                    _stage = 17;
                }
                break;
            case 17:
                uint hairPath = _data!.WriteAsciiZ(256, Path.Combine(config.MediaDirectory, "Characters", "Bloom", "bloom_hair.smo"));
                _stage = 18;
                Call(host, tid, ref ctx, calls, resume, 0x458260, U(0x75DB78), [hairPath], false, value =>
                {
                    if (value == 0 || _localNodes.Contains(value)) throw new InvalidDataException("Fresh hair load failed or aliases local nodes");
                    _hairSource = value;
                });
                break;
            case 18:
                var hair = new VisualGraphReader(host.ProcessHandle).Read(_hairSource, _localNodes);
                LogGraph("fresh-hair-source", hair);
                if (!hair.Success || U(_hairSource + 0x2C) != 0 || Count(_hairSource) != 0)
                    throw new InvalidDataException("Fresh hair ownership/graph rejected: " + hair.Error);
                _hairMaster = UniqueNamedNode(hair.Graph, "Hair_Master");
                var hairNodes = hair.Graph.Nodes.ToDictionary(n => n.Address);
                if (!Identity(hairNodes[_hairSource].LocalTransform)) throw new InvalidDataException("Omitted hair root is not identity");
                var hairOwned = new HashSet<uint>(); var todo = new Stack<uint>(); todo.Push(_hairMaster);
                while (todo.Count != 0)
                {
                    uint n = todo.Pop();
                    if (!hairOwned.Add(n)) throw new InvalidDataException("Hair subtree cycle");
                    var node = hairNodes[n];
                    if (node.Collisions.Items.Count != 0 || node.Kind is NativeVisualNodeKind.Light or NativeVisualNodeKind.LightData or NativeVisualNodeKind.DXLight or NativeVisualNodeKind.AudioSound)
                        throw new InvalidDataException("Hair subtree has nonvisual relations");
                    foreach (uint child in node.Children) todo.Push(child);
                }
                if (hair.Graph.Skins.Count == 0 || hair.Graph.Skins.Any(skin => skin.BoneNodes.Any(b => !hairOwned.Contains(b))) ||
                    hair.Graph.Nodes.Where(n => n.Kind == NativeVisualNodeKind.RenderNode).Any(n => !hairOwned.Contains(n.Address)))
                    throw new InvalidDataException("Hair skin/bone owners extend outside Hair_Master");
                Retain(_hairSource); _hairSourceOwner = true;
                _stage = 19;
                Call(host, tid, ref ctx, calls, resume, 0x421A60, _bodyHead, [_hairMaster], false, _ =>
                {
                    if (U(_hairMaster + 0x2C) != _bodyHead) throw new InvalidDataException("Hair_Master was not attached to own Head");
                });
                break;
            case 19:
                var combined = new VisualGraphReader(host.ProcessHandle).Read(_wrapper, _localNodes);
                LogGraph("combined-body-hair", combined);
                if (!combined.Success || combined.Graph.Nodes.Any(n => n.Collisions.Items.Count != 0 ||
                    n.Kind is NativeVisualNodeKind.Light or NativeVisualNodeKind.LightData or NativeVisualNodeKind.DXLight or NativeVisualNodeKind.AudioSound))
                    throw new InvalidDataException("Combined body/hair graph rejected: " + combined.Error);
                _remoteRenderNodes = combined.Graph.Nodes.Where(n => n.Kind == NativeVisualNodeKind.RenderNode).Select(n => n.Address).ToArray();
                _ownSkins = combined.Graph.Skins.Select(skin => skin.Address).ToArray();
                if (!_hairSourceOwner || Count(_hairSource) != 1 || U(_hairSource + 0x2C) != 0)
                    throw new InvalidDataException("Unexpected residual hair source owner");
                uint hairDelete = U(U(_hairSource)); VerifyFunction(hairDelete);
                SetCount(_hairSource, 0); _hairSourceOwner = false; _stage = 3;
                Call(host, tid, ref ctx, calls, resume, hairDelete, _hairSource, [1], false, _ => { _hairSource = 0; });
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
                if (_loadedClips < _clips.Length)
                {
                    int index = _loadedClips;
                    uint name = _data!.WriteAsciiZ(768, ClipNames[index]);
                    Call(host, tid, ref ctx, calls, resume, 0x59B780, U(0x765ADC), [name, 0], false, value =>
                    {
                        ValidateClip(value); _clips[index] = value; ++_loadedClips;
                        log(new { @event = "visual-clip-loaded", name = ClipNames[index], clip = Hex(value),
                            duration = Float(value + 0x14), tracks = U(value + 0x20), cacheSet = 0 });
                    });
                }
                else _stage = 14;
                break;
            case 14:
                _stage = 21;
                StartClip(host, tid, ref ctx, calls, resume, 0);
                break;
            case 21:
                _appearance = new NativeAppearanceColor(host);
                AppearancePreparation appearance = _appearance.Prepare(_profile, _localRenderNodes, _remoteRenderNodes, _ownSkins);
                _data!.Write(832, appearance.RgbBytes);
                log(new { @event = "visual-appearance-color-input", level = appearance.Level, profileOffset = appearance.ProfileOffset,
                    color = appearance.InputRgb, expectedAmbient = appearance.ExpectedAmbient, ownMaterials = appearance.OwnMaterials });
                _stage = 22;
                Call(host, tid, ref ctx, calls, resume, 0x594820, U(0x75528C), [_wrapper, _data.Address + 832], false, _ =>
                {
                    object verified = _appearance.VerifyAfter(); ++_appearanceApplications;
                    log(new { @event = "visual-appearance-color-applied", verified, sceneStillDetached = true });
                });
                break;
            case 22:
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
                float observedY = Float(_wrapper + 0x78);
                if (!float.IsFinite(observedY)) throw new InvalidDataException("Remote world position became nonfinite");
                _minimumObservedY = Math.Min(_minimumObservedY, observedY); _maximumObservedY = Math.Max(_maximumObservedY, observedY);
                if (_liveUpdates is 1 or 120 or 720 or 1320) ObserveLighting();
                int desired = _liveUpdates < 600 ? 0 : _liveUpdates < 1200 ? 1 : _liveUpdates < 1440 ? 2 : 0;
                if (desired != _activeClip) { _wantedClip = desired; _stage = 15; break; }
                if (_liveUpdates % 3 == 0)
                {
                    float x = _origin[0] + 180, y = _origin[1];
                    if (_activeClip == 1) x += 75 * MathF.Sin((_liveUpdates - 600) * MathF.Tau / 600);
                    if (_activeClip == 2)
                    {
                        float t = Math.Clamp(Float(Playback() + 0x34) / Float(_clip + 0x14), 0, 1);
                        y += 120 * 4 * t * (1 - t);
                    }
                    WriteTransform(x, y, _origin[2]);
                }
                if (_liveUpdates % 120 == 0)
                    log(new { @event = "visual-live", cycle = _cycles + 1, updates = _liveUpdates,
                        wrapper = Hex(_wrapper), actor = Hex(_actor), animation = ClipNames[_activeClip], playbackSample = Float(Playback() + 0x34),
                        localPosition = new[] { Float(_localRoot + 0x20), Float(_localRoot + 0x24), Float(_localRoot + 0x28) },
                        remoteWorldPosition = new[] { Float(_wrapper + 0x74), Float(_wrapper + 0x78), Float(_wrapper + 0x7C) } });
                if (_liveUpdates >= 2400 && (config.Mode != "visual-unload" || _cycles == 0)) _stage = 8;
                break;
            case 15:
                _stage = 16;
                Call(host, tid, ref ctx, calls, resume, 0x5A21C0, _actor, [], false, _ => { });
                break;
            case 16:
                _stage = 7;
                StartClip(host, tid, ref ctx, calls, resume, _wantedClip);
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
                _part = 0; _liveUpdates = 0; _loadedClips = 0; Array.Clear(_clips);
                break;
            default: throw new InvalidOperationException("Unknown visual experiment state");
        }
    }

    private uint UniqueNamedNode(VisualGraphSnapshot graph, string name)
    {
        uint[] matches = graph.Nodes.Where(n => n.NamePointer != 0 &&
            Win32Native.ReadAnsiString(_host.ProcessHandle, checked(n.NamePointer + 9), 128) == name).Select(n => n.Address).ToArray();
        if (matches.Length != 1) throw new InvalidDataException($"Expected exactly one typed Node named {name}, got {matches.Length}");
        return matches[0];
    }
    private static bool Identity(VisualNodeTransform t) => t.Position.All(v => v == 0) && t.Scale.All(v => v == 1) &&
        t.Orientation.SequenceEqual(new float[] { 1, 0, 0, 0, 1, 0, 0, 0, 1 });
    private void ObserveLighting()
    {
        var reader = new RenderLightingObserver(_host.ProcessHandle);
        log(new { @event = "paired-render-lighting", cycle = _cycles + 1, update = _liveUpdates,
            local = _localRenderNodes.Select(reader.Read).ToArray(), remote = _remoteRenderNodes.Select(reader.Read).ToArray() });
    }

    private uint Playback() => checked(U(_actor + 0x28) + _playbackIndex * 0x60);
    private void ValidateClip(uint clip)
    {
        if (clip < 0x10000 || U(clip) != 0x6DE6CC || !float.IsFinite(Float(clip + 0x14)) || Float(clip + 0x14) <= 0 ||
            U(clip + 0x1C) == 0 || U(clip + 0x20) is 0 or > 256)
            throw new InvalidDataException("Stock animation getter returned an unsupported SAN resource");
    }
    private void StartClip(DebuggerHost host, uint tid, ref Win32Native.X86Context ctx,
        NativeCallDispatcher calls, Action resume, int index)
    {
        _clip = _clips[index]; ValidateClip(_clip);
        byte[] request = new byte[0x38];
        Put(request, 0, _clip); Put(request, 4, index == 2 ? 0u : 1u);
        Put(request, 0xC, 0x3F800000); Put(request, 0x30, 0x3F800000);
        _data!.Write(512, request);
        Call(host, tid, ref ctx, calls, resume, 0x5A1E30, _actor, [_data.Address + 512], false, value =>
        {
            if (value == uint.MaxValue || value >= U(_actor + 0x2C)) throw new InvalidDataException("Stock actor rejected animation request");
            _playbackIndex = value; _activeClip = index;
            log(new { @event = "visual-animation-started", animation = ClipNames[index], clip = Hex(_clip), slot = value });
        });
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
        _scale = Win32Native.ReadMemory(_host.ProcessHandle, _localRoot + 0x30, 12);
        _orientation = Win32Native.ReadMemory(_host.ProcessHandle, _localRoot + 0x40, 36);
        if (_origin.Any(v => !float.IsFinite(v))) throw new InvalidDataException("Local position is not finite");
    }
    private void VerifyLocalIdentity()
    {
        if (U(0x765AD4) != _profile || U(_profile + 0x2B4) != _record || U(_record + 0x24) != _localRoot ||
            U(_record + 0x20) != 11 || U(_localRoot + 0x2C) != _parent || U(0x7552A0) != _input ||
            U(_input + 0x18) != _inputState || U(_input + 0x1C) != _inputDevice || U(U(0x755274) + 0x1C) != _camera)
            throw new InvalidDataException("Local input, camera or player ownership changed during passive visual experiment");
    }
    private void WriteTransform(float x, float y, float z)
    {
        if (!_wrapperOwner || U(_wrapper) != VisualGraphReader.NodeVtable) throw new InvalidDataException("No owned plain wrapper");
        // Own diagnostic producer follows the measured Node ABI. This is not a
        // reconstructed native setter: local PRS writes + dirty bit1, caches
        // and descendant world transforms remain stock engine work.
        Write(_wrapper + 0x20, BitConverter.GetBytes(x).Concat(BitConverter.GetBytes(y)).Concat(BitConverter.GetBytes(z)).ToArray());
        Write(_wrapper + 0x30, _scale);
        Write(_wrapper + 0x40, _orientation);
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
