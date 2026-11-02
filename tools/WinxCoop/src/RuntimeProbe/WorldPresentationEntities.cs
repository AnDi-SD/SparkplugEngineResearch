using System.Reflection.PortableExecutable;
using System.Text;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

// A wire-independent view of the agreed 100-byte ActorRecord. The shared
// WorldState codec owns serialization; native addresses never enter this view.
internal sealed record WorldPresentationActorState(uint StableId, uint TypeId, uint StateId,
    uint Animation, uint Flags, float[] Position, float[] Scale, float[] Rotation,
    float[] Velocity, int Health, int MaximumHealth)
{
    internal const uint Present = 1, HasHealth = 2, Dead = 4, HasVelocity = 8, Visible = 16;
    internal const uint BloomType = 0x37810000, MosquitoType = 0x22D3211F, StingerType = 0x206A45A2;
    internal void Validate()
    {
        if (StableId == 0 || TypeId is not (BloomType or MosquitoType or StingerType) || Animation > 5 ||
            (Flags & ~31u) != 0 || Position.Length != 3 || Scale.Length != 3 || Rotation.Length != 9 ||
            Velocity.Length != 3 || Position.Concat(Scale).Concat(Rotation).Concat(Velocity).Any(v => !float.IsFinite(v)) ||
            Scale.Any(v => v <= 0 || v > 100) || (Flags & HasVelocity) == 0 && Velocity.Any(v => v != 0) ||
            ((Flags & HasHealth) != 0 ? Health < 0 || MaximumHealth <= 0 || Health > MaximumHealth || MaximumHealth > 100000 :
                Health != -1 || MaximumHealth != -1))
            throw new InvalidDataException("Unqualified bounded world actor state.");
        if (TypeId == StingerType && (StableId is <= 0x10000 or > 0x11000 || StateId != 1 || Animation != 5 ||
            Flags != (Present | Visible | HasVelocity)))
            throw new InvalidDataException("Stinger presentation must identify one live native activation without health/gameplay state.");
    }
}

// Borrowed identities used only while all game threads are at the owned stop.
internal sealed record WorldPresentationNativeRig(uint Instance, uint Root, uint Scene, uint Parent,
    uint Character, uint Traits, uint Machine, uint Animator, uint Actor, uint TypeId);

/// <summary>Read-only native owner graph. Stable IDs are caller-assigned, never pointers.</summary>
internal sealed class WorldPresentationEntityReader(DebuggerHost host, PEReader image)
{
    private readonly int _ownerThread = Environment.CurrentManagedThreadId;
    private NativeCharacterJumpState? _jumpStates;
    internal WorldPresentationNativeRig ReadRigAtWorld(uint tid, ref Win32Native.X86Context context,
        uint instance, uint expectedType)
    {
        Ensure(tid, context); Require(U(instance) == 0x710F80, "World actor is not an original instance.");
        uint root = P(instance + 0x24), scene = P(root + 0x3C), parent = P(root + 0x2C);
        Require(U(root) == VisualGraphReader.NodeVtable && U(scene) == 0x6E7358, "World actor root/scene changed.");
        uint count = U(instance + 0x20), head = P(instance + 0x1C), link = P(head), previous = head;
        Require(count is > 0 and <= 64, "World actor component count exceeds the finite bound.");
        Dictionary<uint, uint> components = []; HashSet<uint> links = [], entities = []; uint modelNodes = 0;
        for (uint i = 0; i < count; ++i)
        {
            Require(link != head && links.Add(link) && U(link + 4) == previous, "World actor component links changed.");
            uint entity = P(link + 8), type = ClassId(entity);
            Require(entities.Add(entity), "World actor component identity is duplicated.");
            if (type == 0x695C0F65)
            {
                Require(++modelNodes == 1 && U(entity) == VisualGraphReader.NodeVtable &&
                    U(entity + 0x2C) == root && U(entity + 0x3C) == scene,
                    "World actor model Node is not the captured instance root's direct scene child.");
            }
            else
            {
                Require(U(entity + 0x24) == instance, "World actor component belongs to another instance.");
                uint bound = P(entity + 0x18);
                if (type == 0x4307193A) NativeTargetNodeOwnership.VerifyAtStop(host, bound, root, scene);
                else Require(bound == root, "World actor component belongs to another root.");
            }
            if (type is 0x0003CC73 or 0x1BC6108E or 0x28C56217 or 0xCD2B2876 or
                WorldPresentationActorState.BloomType or WorldPresentationActorState.MosquitoType)
                Require(components.TryAdd(type, entity), "World actor has a duplicate admitted component.");
            previous = link; link = P(link);
        }
        Require(link == head && U(head + 4) == previous && modelNodes == 1, "World actor component sentinel/model Node changed.");
        uint character = Required(0x0003CC73), animator = Required(0xCD2B2876), machine = Required(expectedType);
        uint traits = expectedType == WorldPresentationActorState.BloomType ? Required(0x28C56217) : Required(0x1BC6108E);
        uint actor = P(animator + 0x128);
        Require(U(character) == 0x6F6438 && U(animator) == 0x6F670C && U(actor) == 0x703F80 &&
            U(character + 0x124) == machine && U(character + 0x128) == animator && U(character + 0x138) == traits &&
            U(animator + 0x124) == character && U(traits + 0x18C) == character &&
            U(traits) == (expectedType == WorldPresentationActorState.BloomType ? 0x6F6990u : 0x6F67F0u),
            "World actor Character/Traits/Animator/Actor ownership is not admitted.");
        return new(instance, root, scene, parent, character, traits, machine, animator, actor, expectedType);
        uint Required(uint key) => components.TryGetValue(key, out uint value) ? value :
            throw new InvalidDataException($"World actor lacks component {key:X8}.");
    }

    internal WorldPresentationActorState CaptureAtWorld(uint tid, ref Win32Native.X86Context context,
        WorldPresentationNativeRig rig, uint stableId)
    {
        WorldPresentationNativeRig current = ReadRigAtWorld(tid, ref context, rig.Instance, rig.TypeId);
        Require(current == rig, "World actor identity changed before capture.");
        uint hp = U(rig.Traits + 0x124), maximum = U(rig.Traits + 0x128);
        Require(hp <= maximum && maximum is > 0 and <= 100000, "World actor native health is outside the finite cohort.");
        uint state = U(rig.Machine + 0x130), animation = SemanticAnimation(tid, context, rig, state, hp);
        uint flags = WorldPresentationActorState.Present | WorldPresentationActorState.HasHealth;
        if ((U(rig.Root + 0xB0) & 0x200) != 0) flags |= WorldPresentationActorState.Visible;
        if (hp == 0) flags |= WorldPresentationActorState.Dead;
        var result = new WorldPresentationActorState(stableId, rig.TypeId, state, animation, flags,
            F(rig.Root + 0x20, 3), F(rig.Root + 0x30, 3), F(rig.Root + 0x40, 9), [0, 0, 0], (int)hp, (int)maximum);
        result.Validate(); return result;
    }

    internal IReadOnlyList<WorldPresentationNativeRig> ReadBattleMosquitoesAtWorld(uint tid,
        ref Win32Native.X86Context context, uint scene, uint primaryInstance, uint secondaryInstance)
    {
        Ensure(tid, context);
        uint manager = P(0x765AF0); Require(U(manager) == 0x703954, "World enemy registry is not wxTargetManager.");
        uint head = P(manager + 0x18), count = U(manager + 0x1C), link = P(head), previous = head;
        Require(count <= 1024, "World target registry exceeds its admitted bound.");
        List<uint> candidates = []; HashSet<uint> links = [], regions = [], instances = [];
        for (uint index = 0; index < count; ++index)
        {
            Require(link != head && links.Add(link) && U(link + 4) == previous, "World target registry link changed.");
            uint region = P(link + 8);
            Require(regions.Add(region) && U(region) == 0x6F7ED0 && ClassId(region) == 0x4307193A,
                "World target registry contains a duplicate or untyped region.");
            uint instance = P(region + 0x24);
            if (instance != primaryInstance && instance != secondaryInstance && U(instance) == 0x710F80)
            {
                uint root = P(instance + 0x24), info = U(root + 0x0C);
                if (U(root + 0x3C) == scene && info >= 0x10000 && U(info) == 0x7061D0 && U(info + 8) == instance &&
                    (BitConverter.ToUInt16(Read(info + 4, 2)) & 0x18) == 8)
                {
                    NativeTargetNodeOwnership.VerifyAtStop(host, P(region + 0x18), root, scene);
                    // Stock TargetRegion is bound to target_node, not the
                    // instance wrapper. Distinct typed regions can share one
                    // enemy; the wire carries that instance exactly once.
                    if (instances.Add(instance)) candidates.Add(instance);
                    Require(candidates.Count <= 64, "World enemy presentation exceeds64 entities.");
                }
            }
            previous = link; link = P(link);
        }
        Require(link == head && U(head + 4) == previous, "World target registry count/sentinel changed.");
        List<WorldPresentationNativeRig> result = [];
        foreach (uint instance in candidates)
            result.Add(ReadRigAtWorld(tid, ref context, instance, WorldPresentationActorState.MosquitoType));
        return result.AsReadOnly();
    }

    private uint SemanticAnimation(uint tid, in Win32Native.X86Context context,
        WorldPresentationNativeRig rig, uint state, uint hp)
    {
        if (hp == 0) return 4;
        if (rig.TypeId == WorldPresentationActorState.MosquitoType)
        {
            // SAN's inherited name is optional and need not contain the cache
            // filename. Native Mosquito attack state3 independently identifies
            // the stock0x401 request that resolves to mcatst.san.
            if (state == 3)
            {
                uint attack = P(rig.Machine + 0x148 + 3 * 4);
                Require(U(attack) == 0x6F9A78 && ClassId(attack) == 0x6CF96918,
                    "World Mosquito attack semantic lacks its original typed state3.");
                return 3;
            }
            uint clip = U(rig.Animator + 0x17C);
            if (clip >= 0x10000 && U(clip) == 0x6DE6CC)
            {
                uint namePointer = U(clip + 0x10);
                if (namePointer < 0x10000) return 5;
                string name;
                try { name = Name(namePointer); }
                catch (Exception error) when (error is InvalidDataException or System.ComponentModel.Win32Exception)
                { return 5; } // An anonymous native clip is an explicit unmapped semantic.
                if (name.EndsWith("mcatst.san", StringComparison.OrdinalIgnoreCase)) return 3;
                if (name.EndsWith("mchuko.san", StringComparison.OrdinalIgnoreCase)) return 4;
                if (name.EndsWith("mcho.san", StringComparison.OrdinalIgnoreCase)) return 0;
            }
            return 5;
        }
        _jumpStates ??= new(host, image);
        if (_jumpStates.ReadAtWorld(tid, context, rig.Machine, rig.Animator, state) is not null) return 2;
        if (state == 5) return 3; // separately paired original Ordinary state in E4
        if (state != 0) return 5;
        uint command = P(rig.Character + 0x130);
        Require(U(command) == 0x703090, "World Bloom movement command type changed.");
        float magnitude = F(command + 4, 1)[0];
        Require(magnitude is >= 0 and <= 1, "World Bloom movement magnitude is outside0..1.");
        return magnitude > .01f ? 1u : 0u;
    }
    private uint ClassId(uint entity)
    {
        uint table = P(entity), getter = U(table + 0x10); byte[] bytes = Read(getter, 6);
        Require(bytes[0] == 0xB8 && bytes[5] == 0xC3 && getter is >= 0x401000 and < 0x6DA000 &&
            bytes.SequenceEqual(image.GetSectionData(checked((int)(getter - 0x400000))).GetContent(0, 6).ToArray()),
            "World entity class getter is not its pinned native literal getter.");
        return U(BitConverter.ToUInt32(bytes, 1));
    }
    private string Name(uint entry)
    {
        List<byte> result = [];
        for (uint i = 0; i < 128; ++i)
        {
            byte value = Read(checked(entry + 9 + i), 1)[0];
            if (value == 0) return Encoding.ASCII.GetString(result.ToArray());
            Require(value is >= 32 and <= 126, "World animation name is not bounded ASCII."); result.Add(value);
        }
        throw new InvalidDataException("World animation name exceeds128 bytes.");
    }
    private float[] F(uint address, int count)
    { byte[] bytes = Read(address, checked(count * 4)); return Enumerable.Range(0, count).Select(i => BitConverter.ToSingle(bytes, i * 4)).ToArray(); }
    private uint P(uint address) { uint value = U(address); Require(value >= 0x10000 && (value & 3) == 0, "World owner pointer is not aligned/live."); return value; }
    private uint U(uint address) => Win32Native.ReadUInt32(host.ProcessHandle, address);
    private byte[] Read(uint address, int count) => Win32Native.ReadMemory(host.ProcessHandle, address, count);
    private void Ensure(uint tid, Win32Native.X86Context context) => Require(host.IsAtDebugStop && host.ProcessHandle != IntPtr.Zero &&
        host.ImageBase == 0x400000 && tid == host.MainThreadId && context.Eip == NativeCallDispatcher.WorldReturn &&
        Environment.CurrentManagedThreadId == _ownerThread, "World entity observation requires the owned original WorldRET.");
    private static void Require(bool value, string message) { if (!value) throw new InvalidDataException(message); }
}
