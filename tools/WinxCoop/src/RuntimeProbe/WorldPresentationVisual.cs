using System.Reflection.PortableExecutable;
using System.Security.Cryptography;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

// Root aggregates these plans and installs ONE deletion witness per entry.
// AddReturn is LIFO, not multicast, so plans never install their own probe.
internal sealed record WorldPresentationCleanupPlan(IReadOnlyList<NativeCallSpec> Calls,
    uint DeletingEntry, uint DeletedRoot, Action<uint, Win32Native.X86Context> ObserveDeletion, Action Complete);

/// <summary>
/// One bounded Mosquito visual: native SMO visual cut + stock spActor, with no
/// wx instance, Traits, AI, TargetRegion, collision detector or projectile pool.
/// Reuses the Stage-A native sequence and shared VisualGraphReader selector.
/// </summary>
internal sealed class WorldPresentationVisual
{
    private const string MosquitoSha = "9FB89A75049813ED71E2A45DBD5D60E1975568C5D836D9F49777153F4F10FB5E";
    private readonly DebuggerHost _host;
    private readonly PEReader _image;
    private readonly Action<object> _log;
    private readonly string _assetPath;
    private readonly uint _stableId;
    private readonly int _ownerThread;
    private OwnedRemoteBuffer? _data;
    private VisualGraphSelection? _selection;
    private HashSet<uint> _forbidden = [];
    private uint _source, _wrapper, _actor, _parent, _scene;
    private bool _sourceOwned, _wrapperOwned, _cleanupPrepared, _deleted, _deletingWitness, _modelBasisInitialized;
    private int _stage, _part;

    internal WorldPresentationVisual(DebuggerHost host, PEReader image, Action<object> log,
        string mediaDirectory, uint stableId)
    {
        _host = host; _image = image; _log = log; _stableId = stableId;
        _ownerThread = Environment.CurrentManagedThreadId;
        _assetPath = Path.Combine(mediaDirectory, "Characters", "Mosquito", "Mosquito.smo");
        Require(stableId > 2 && Path.IsPathFullyQualified(_assetPath) && _assetPath.All(c => c is >= ' ' and <= '~'),
            "Passive Mosquito requires a server-issued enemy ID and explicit ASCII Media path.");
        byte[] asset = File.ReadAllBytes(_assetPath);
        Require(asset.Length == 129047 && Convert.ToHexString(SHA256.HashData(asset)) == MosquitoSha,
            "Passive Mosquito asset is outside its exact bounded cohort.");
    }
    internal bool Ready => _stage == 6 && !_cleanupPrepared && !_deleted;
    internal bool Deleted => _deleted;
    internal uint Root => Ready ? _wrapper : throw new InvalidOperationException("Passive visual is not ready.");
    internal uint Actor => Ready ? _actor : throw new InvalidOperationException("Passive visual is not ready.");
    internal object Summary => new { stableId = _stableId, typeId = WorldPresentationActorState.MosquitoType,
        stage = _stage, ready = Ready, cleanupPrepared = _cleanupPrepared, deleted = _deleted,
        nativeDeletingWitness = _deletingWitness, sourceOwned = _sourceOwned, wrapperOwned = _wrapperOwned,
        passiveOnly = true, nativeEnemyInstancesCreated = 0, nativeHealthReceiversCreated = 0,
        aiOrDamageOrLootEnabled = false, modelHalfTurnEnabledByDefault = true,
        modelHalfTurnApplied = _modelBasisInitialized, appearanceRuntimeQualificationClaimed = false,
        data = _data?.Summary };

    // Returns true when this step issued a native call. The caller re-arms its
    // ordinary WorldRET through resume, exactly as in NativeVisualActor.
    internal bool StepCreateAtWorld(uint tid, ref Win32Native.X86Context context, NativeCallDispatcher calls,
        Action resume, uint parent, uint scene, uint localRoot, WorldPresentationActorState initial)
    {
        Ensure(tid, context); initial.Validate();
        Require(!calls.Pending && !_cleanupPrepared && !_deleted && initial.StableId == _stableId &&
            initial.TypeId == WorldPresentationActorState.MosquitoType, "Passive visual create state/dispatcher changed.");
        if (_stage == 6) { VerifyLive(); return false; }
        if (_stage > 0) Require(parent == _parent && scene == _scene, "Passive visual create changed its scene owners.");
        switch (_stage)
        {
            case 0:
                Require(U(parent) == VisualGraphReader.NodeVtable && U(scene) == 0x6E7358 && U(localRoot + 0x3C) == scene,
                    "Passive visual parent/scene/local reference is not admitted.");
                _parent = parent; _scene = scene;
                VisualGraphReadResult local = new VisualGraphReader(_host.ProcessHandle).Read(localRoot, inspectSkins: false);
                Require(local.Success, "Passive visual could not inventory the local forbidden graph: " + local.Error);
                _forbidden = local.Graph.Nodes.Select(n => n.Address).ToHashSet();
                _data = new OwnedRemoteBuffer(_host, 1024); uint path = _data.WriteAsciiZ(0, _assetPath);
                _stage = 1;
                Call(tid, ref context, calls, resume, 0x458260, U(0x75DB78), [path], false, value =>
                { Require(value >= 0x10000 && !_forbidden.Contains(value), "Passive SMO load failed/aliased local graph."); _source = value; });
                return true;
            case 1:
                VisualGraphReadResult graph = new VisualGraphReader(_host.ProcessHandle).Read(_source, _forbidden);
                Require(graph.Success && U(_source + 0x2C) == 0 && Count(_source) == 0,
                    "Passive Mosquito fresh source ownership/skin graph rejected: " + graph.Error);
                _selection = VisualGraphReader.SelectVisualParts(graph.Graph);
                Require(_selection.Success, "Passive Mosquito visual ownership cut rejected: " + _selection.Error);
                Retain(_source); _sourceOwned = true; _stage = 2;
                Call(tid, ref context, calls, resume, 0x421E20, 0, [], true, value =>
                {
                    Require(value >= 0x10000 && U(value) == VisualGraphReader.NodeVtable && Count(value) == 0 && U(value + 0x2C) == 0,
                        "Passive visual wrapper factory rejected."); _wrapper = value; Retain(value); _wrapperOwned = true;
                });
                return true;
            case 2:
                if (_part < _selection!.VisualRoots.Count)
                {
                    uint part = _selection.VisualRoots[_part++];
                    Call(tid, ref context, calls, resume, 0x421A60, _wrapper, [part], false, _ =>
                        Require(U(part + 0x2C) == _wrapper, "Passive native reparent failed its owner witness."));
                    return true;
                }
                VisualGraphReadResult own = new VisualGraphReader(_host.ProcessHandle).Read(_wrapper, _forbidden);
                Require(own.Success && own.Graph.Skins.Count != 0 && own.Graph.Nodes.All(n => n.Scene == 0 && n.Collisions.Items.Count == 0 &&
                    n.Kind is NativeVisualNodeKind.Node or NativeVisualNodeKind.RenderNode),
                    "Passive Mosquito cut contains unsupported light/audio/collision/scene ownership.");
                _stage = 3;
                Call(tid, ref context, calls, resume, 0x5A3620, 0, [], true, value =>
                { Require(value >= 0x10000 && U(value) == 0x703F80, "Passive actor factory rejected."); _actor = value; });
                return true;
            case 3:
                _stage = 4; Call(tid, ref context, calls, resume, 0x5A35C0, _actor, [_wrapper], false, _ =>
                {
                    uint begin = U(_actor + 0x40), end = U(_actor + 0x44);
                    Require(begin >= 0x10000 && end > begin && (end - begin) % 4 == 0 && (end - begin) / 4 <= 256,
                        "Passive actor did not bind a bounded native controller vector.");
                }); return true;
            case 4:
                Require(_sourceOwned && Count(_source) == 1 && U(_source + 0x2C) == 0, "Passive residual source ownership changed.");
                uint delete = U(U(_source)); SetCount(_source, 0); _sourceOwned = false; _stage = 5;
                Call(tid, ref context, calls, resume, delete, _source, [1], false, _ => _source = 0); return true;
            case 5:
                Write(_wrapper + 0x20, Bytes(initial.Position)); Write(_wrapper + 0x30, Bytes(initial.Scale));
                Write(_wrapper + 0x40, Bytes(WorldPresentationModelOrientation.Apply(initial.Rotation, characterVisualHalfTurn: true)));
                _modelBasisInitialized = true;
                Write(_wrapper + 0xB0, BitConverter.GetBytes(U(_wrapper + 0xB0) | 1));
                _stage = 7; Call(tid, ref context, calls, resume, 0x421A60, _parent, [_wrapper], false, _ =>
                {
                    _stage = 6;
                    VerifyLive(); _log(new { @event = "world-presentation-passive-created", stableId = _stableId,
                        typeId = WorldPresentationActorState.MosquitoType, ownedVisualOnly = true, nativeWxInstanceCreated = false,
                        sourceAssetSha256 = MosquitoSha, characterVisualHalfTurn = true, modelBasisRuntimeQualified = false });
                }); return true;
            default: throw new InvalidDataException("Unknown passive visual construction phase.");
        }
    }
    internal WorldPresentationBinding BindingAtWorld(uint tid, ref Win32Native.X86Context context)
    {
        Ensure(tid, context); Require(Ready, "Passive binding precedes native construction completion."); VerifyLive();
        if (_data is { Released: false }) _data.Release(callPending: false); // no native borrower after SMO call completed
        return new(_stableId, WorldPresentationActorState.MosquitoType, _wrapper, _actor, _scene, _parent, true);
    }
    internal WorldPresentationCleanupPlan PrepareCleanupAtStop(uint tid, ref Win32Native.X86Context context)
    {
        Require(_host.IsAtDebugStop && tid == _host.MainThreadId && Environment.CurrentManagedThreadId == _ownerThread &&
            context.Eip is NativeCallDispatcher.WorldReturn or 0x5F86C0 or 0x5F2300 or 0x4E8AA0 or 0x4E8C80 or 0x59AE80 or 0x59B190,
            "Passive cleanup preparation is outside the owned world/pre-unload stop.");
        Require(Ready && _source == 0 && _wrapperOwned && Count(_wrapper) == 2 && _data is { Released: true },
            "Passive cleanup arrived before a complete/sole adapter+parent visual lifetime.");
        VerifyLive(); uint deletingEntry = U(U(_wrapper)), root = _wrapper;
        foreach (uint function in new[] { deletingEntry, 0x5A21C0u, 0x5A35A0u, 0x4217E0u }) Pin(function);
        SetCount(_wrapper, 1); _wrapperOwned = false; _cleanupPrepared = true;
        NativeCallSpec[] plan = [new(0x5A21C0, _actor, [], false), new(0x5A35A0, _actor, [1], false), new(0x4217E0, _wrapper, [], false)];
        return new(plan, deletingEntry, root,
            (wt, wc) =>
            {
                Require(_host.IsAtDebugStop && wt == _host.MainThreadId && wc.Eip == deletingEntry && wc.Ecx == root &&
                    !_deletingWitness && Count(root) == 0, "Passive deletion lacks its actual native zero-owner destructor entry.");
                _deletingWitness = true;
            },
            () =>
            {
                Require(_host.IsAtDebugStop && _cleanupPrepared && _deletingWitness && !_deleted,
                    "Passive cleanup batch lacks its native deletion witness.");
                // root/actor are now historical. Never dereference freed bytes.
                _wrapper = _actor = 0; _deleted = true; _stage = 30;
                _log(new { @event = "world-presentation-passive-deleted", stableId = _stableId,
                    nativeZeroOwnerDestructorObserved = true, historicalGraphReads = false });
            });
    }
    private void VerifyLive() => Require(_wrapperOwned && U(_wrapper) == VisualGraphReader.NodeVtable &&
        U(_wrapper + 0x2C) == _parent && U(_wrapper + 0x3C) == _scene && U(_actor) == 0x703F80,
        "Passive visual lost its own live root/actor/parent/scene.");
    private void Call(uint tid, ref Win32Native.X86Context context, NativeCallDispatcher calls, Action resume,
        uint function, uint receiver, uint[] args, bool cdecl, Action<uint> completed)
    { Pin(function); calls.Begin(_host, tid, ref context, function, receiver, args, cdecl, value => { completed(value); resume(); }); }
    private void Pin(uint function)
    {
        Require(function is >= 0x401000 and < 0x6DA000 &&
            Read(function, 6).SequenceEqual(_image.GetSectionData(checked((int)(function - 0x400000))).GetContent(0, 6).ToArray()),
            "Passive visual native call signature changed.");
    }
    private void Ensure(uint tid, Win32Native.X86Context context) => Require(_host.IsAtDebugStop && _host.ProcessHandle != IntPtr.Zero &&
        tid == _host.MainThreadId && context.Eip == NativeCallDispatcher.WorldReturn && Environment.CurrentManagedThreadId == _ownerThread,
        "Passive visual construction requires the owned original WorldRET.");
    private ushort Count(uint value) => BitConverter.ToUInt16(Read(value + 8, 2));
    private void Retain(uint value) { ushort count = Count(value); Require(count < ushort.MaxValue, "Passive owner count overflow."); SetCount(value, (ushort)(count + 1)); }
    private void SetCount(uint value, ushort count) => Write(value + 8, BitConverter.GetBytes(count));
    private uint U(uint address) => Win32Native.ReadUInt32(_host.ProcessHandle, address);
    private byte[] Read(uint address, int count) => Win32Native.ReadMemory(_host.ProcessHandle, address, count);
    private void Write(uint address, byte[] bytes) { Win32Native.WriteMemory(_host.ProcessHandle, address, bytes, false); Require(Read(address, bytes.Length).SequenceEqual(bytes), "Passive visual data readback changed."); }
    private static byte[] Bytes(float[] values) => values.SelectMany(BitConverter.GetBytes).ToArray();
    private static void Require(bool value, string message) { if (!value) throw new InvalidDataException(message); }
}
