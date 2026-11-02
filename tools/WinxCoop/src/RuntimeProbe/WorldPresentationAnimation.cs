using System.Reflection.PortableExecutable;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// Semantic clip playback on an already admitted stock/owned spActor. Uses the
/// original resource manager and Actor API; never drives a wx state machine.
/// </summary>
internal sealed class WorldPresentationAnimation(DebuggerHost host, PEReader image,
    Func<bool> nativeGameplaySuppressed, Action<object> log, WorldPresentationBinding binding)
{
    private readonly int _ownerThread = Environment.CurrentManagedThreadId;
    private readonly Dictionary<string, uint> _clips = [];
    private OwnedRemoteBuffer? _data;
    private uint _manager, _desired = uint.MaxValue, _current = uint.MaxValue, _selectedClip;
    private uint _starts, _fallbacks;
    private int _stage;
    private bool _retired;
    internal bool IsApplied(uint semantic) => !_retired && _stage == 0 && _current == semantic;
    internal object Summary => new { binding.StableId, binding.TypeId, retired = _retired, stage = _stage,
        currentSemantic = _current, desiredSemantic = _desired, nativeStarts = _starts,
        otherSemanticIdleFallbacks = _fallbacks, fsmOrCommandWrites = 0, syntheticTags = 0, data = _data?.Summary };

    internal bool StepAtWorld(uint tid, ref Win32Native.X86Context context, uint semantic,
        NativeCallDispatcher calls, Action resume)
    {
        Ensure(tid, context); Require(semantic <= 5 && !calls.Pending, "Semantic animation has an invalid state/pending call.");
        VerifyBinding();
        if (_stage == 0)
        {
            if (_current == semantic) return false;
            _desired = semantic; _stage = 1;
            if (semantic == 5) ++_fallbacks;
            _data ??= new OwnedRemoteBuffer(host, 512);
            if (_manager == 0) _manager = U(0x765ADC);
            Require(U(_manager) == 0x703470 && U(0x765ADC) == _manager,
                "Semantic animation lost its original resource manager.");
        }
        (string name, uint set, uint mode) = Clip(binding.TypeId, _desired);
        switch (_stage)
        {
            case 1:
                _stage = 2; Call(tid, ref context, calls, resume, 0x5A21C0, binding.Actor, [], _ => { }); return true;
            case 2:
                if (_clips.TryGetValue(name, out _selectedClip)) { _stage = 3; return false; }
                uint namePointer = _data!.WriteAsciiZ(0, name);
                _stage = 3; Call(tid, ref context, calls, resume, 0x59B780, _manager, [namePointer, set], value =>
                {
                    VerifyClip(value); _selectedClip = value; _clips.Add(name, value);
                }); return true;
            case 3:
                VerifyClip(_selectedClip);
                byte[] request = new byte[0x38];
                Put(request, 0, _selectedClip); Put(request, 4, mode);
                Put(request, 0xC, 0x3F800000); Put(request, 0x30, 0x3F800000);
                _data!.Write(256, request); _stage = 4;
                Call(tid, ref context, calls, resume, 0x5A1E30, binding.Actor, [_data.Address + 256], slot =>
                {
                    Require(slot != uint.MaxValue && slot < U(binding.Actor + 0x2C), "Native Actor rejected semantic playback.");
                    _current = _desired; _stage = 0; ++_starts;
                    log(new { @event = "world-presentation-animation-started", binding.StableId, binding.TypeId,
                        semantic = _current, clip = name, nativeAnimationSet = set, nativePlaybackSlot = slot,
                        originalActorPlayback = true, nativeFsmTransition = false, gameplayCallbacksQuarantined = true });
                }); return true;
            default: throw new InvalidDataException("Semantic playback was stepped during an unfinished native call.");
        }
    }
    internal void RetireBeforeUnload(uint tid, ref Win32Native.X86Context context, bool nativeCallPending)
    {
        Require(host.IsAtDebugStop && tid == host.MainThreadId && Environment.CurrentManagedThreadId == _ownerThread &&
            !nativeCallPending && _stage == 0 && !_retired &&
            context.Eip is NativeCallDispatcher.WorldReturn or 0x5F86C0 or 0x5F2300 or 0x4E8AA0 or 0x4E8C80 or 0x59AE80 or 0x59B190,
            "Semantic animation retirement requires a completed native operation before unload.");
        _retired = true; if (_data is { Released: false }) _data.Release(callPending: false);
        // All resource handles are borrowed from the original manager. Native
        // actor cleanup must precede that manager's stock cache release.
        _clips.Clear(); _selectedClip = 0;
    }
    private static (string, uint, uint) Clip(uint type, uint semantic)
    {
        if (type == WorldPresentationActorState.MosquitoType)
            return semantic switch { 3 => ("mcatst.san", 12, 0), 4 => ("mchuko.san", 12, 0), _ => ("mcho.san", 12, 1) };
        Require(type == WorldPresentationActorState.BloomType, "No semantic clip family for this actor type.");
        return semantic switch
        {
            1 => ("blwa.san", 0, 1), 2 => ("bljusm.san", 0, 0),
            3 => ("bloid1.san", 0, 0), 4 => ("blidhuko.san", 0, 0), _ => ("blidsh.san", 0, 1)
        };
    }
    private void VerifyBinding()
    {
        Require(U(binding.Root) == VisualGraphReader.NodeVtable && U(binding.Root + 0x3C) == binding.Scene &&
            U(binding.Root + 0x2C) == binding.Parent && U(binding.Actor) == 0x703F80,
            "Semantic animation lost its bound scene/root/Actor.");
        if (binding.Rig is { } rig) Require(U(rig.Animator + 0x128) == binding.Actor && U(rig.Instance + 0x24) == binding.Root,
            "Semantic animation stock rig ownership changed.");
        if (_manager != 0) Require(U(0x765ADC) == _manager && U(_manager) == 0x703470,
            "Semantic animation resource manager changed before retirement.");
    }
    private void VerifyClip(uint clip)
    {
        Require(clip >= 0x10000 && U(clip) == 0x6DE6CC && U(clip + 0x1C) >= 0x10000 && U(clip + 0x20) is > 0 and <= 256,
            "Semantic animation resource is not an admitted native SAN.");
        float duration = BitConverter.ToSingle(Win32Native.ReadMemory(host.ProcessHandle, clip + 0x14, 4));
        Require(float.IsFinite(duration) && duration > 0, "Semantic animation resource has no finite positive duration.");
    }
    private void Call(uint tid, ref Win32Native.X86Context context, NativeCallDispatcher calls, Action resume,
        uint function, uint receiver, uint[] args, Action<uint> completed)
    {
        byte[] original = image.GetSectionData(checked((int)(function - 0x400000))).GetContent(0, 6).ToArray();
        Require(Win32Native.ReadMemory(host.ProcessHandle, function, 6).SequenceEqual(original), "Semantic animation native call pin changed.");
        calls.Begin(host, tid, ref context, function, receiver, args, false, value => { completed(value); resume(); });
    }
    private void Ensure(uint tid, Win32Native.X86Context context) => Require(host.IsAtDebugStop && host.ProcessHandle != IntPtr.Zero &&
        tid == host.MainThreadId && context.Eip == NativeCallDispatcher.WorldReturn && !_retired &&
        Environment.CurrentManagedThreadId == _ownerThread && nativeGameplaySuppressed(),
        "Semantic playback requires owned WorldRET with active E2 gameplay quarantine.");
    private uint U(uint address) => Win32Native.ReadUInt32(host.ProcessHandle, address);
    private static void Put(byte[] bytes, int offset, uint value) => BitConverter.GetBytes(value).CopyTo(bytes, offset);
    private static void Require(bool value, string message) { if (!value) throw new InvalidDataException(message); }
}
