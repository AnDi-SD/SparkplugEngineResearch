using System.ComponentModel;
using System.Security.Cryptography;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

internal enum NativeVisualNodeKind { Node, RenderNode, Light, LightData, AudioSound, DXLight }

internal sealed record VisualNodeTransform(
    IReadOnlyList<float> Position, IReadOnlyList<float> Scale, IReadOnlyList<float> Orientation);
internal sealed record VisualChildLink(uint Address, uint Next, uint Previous, uint Child);
internal sealed record VisualNativeVector(uint Begin, uint End, uint Capacity, IReadOnlyList<uint> Items);
internal sealed record VisualNodeRead(
    uint Address, uint Vtable, NativeVisualNodeKind Kind, ushort ReferenceCount, uint NamePointer,
    uint Parent, uint Scene, uint Flags, VisualNodeTransform LocalTransform, VisualNodeTransform WorldTransform,
    uint ChildrenHead, uint ChildrenCount, IReadOnlyList<uint> Children, IReadOnlyList<VisualChildLink> ChildLinks,
    VisualNativeVector Collisions, VisualNativeVector? Renderables, uint? LightType, bool? LightEnabled);
internal sealed record VisualSkinRead(
    uint Address, uint Vtable, uint InfluenceCount, uint BoneCount, uint BonesArray, uint InverseBindArray,
    IReadOnlyList<uint> BoneNodes, string InverseBindSha256);
internal sealed record VisualMemoryRead(uint Address, int Length, string Purpose, bool Success, int? NativeError);
internal sealed record VisualGraphSnapshot(
    bool Complete, bool SkinInspectionPerformed, uint Root,
    IReadOnlyList<VisualNodeRead> Nodes, IReadOnlyList<VisualSkinRead> Skins,
    IReadOnlyList<VisualMemoryRead> Reads, int ReadBytes);
internal sealed record VisualGraphReadResult(bool Success, string? Error, VisualGraphSnapshot Graph);
internal sealed record VisualGraphSelection(
    bool Success, string? Error, uint CommonParent, uint PrimaryRenderRoot, uint SkeletonRoot,
    IReadOnlyList<uint> VisualRoots, IReadOnlyList<uint> SelectedNodeAddresses,
    IReadOnlyList<uint> ExcludedRoots, bool OmittedAncestorsIdentity,
    IReadOnlyList<uint> OmittedAnimatedNodes);

/// <summary>
/// Own diagnostic adapter, not a serializer or an original engine method.
/// The caller must hold every game thread at a verified debug stop for the
/// entire read. ReadProcessMemory alone neither pins objects nor makes a
/// snapshot atomic. Only the pinned original PC ABI is accepted. No native
/// functions, remote writes, name lookups, allocation, attach or release calls.
/// </summary>
internal sealed class VisualGraphReader
{
    internal const uint NodeVtable = 0x6DC4F4;
    internal const uint RenderNodeVtable = 0x6DCAA4;
    internal const uint LightVtable = 0x6DCC18;
    internal const uint LightDataVtable = 0x6DE990;
    // spDXLight physically derives from spLight; inspect only inherited
    // Node/Light fields through ED, never the device payload at F0..157.
    internal const uint DXLightVtable = 0x6F0C88;
    // PC/spAudioSoundAbi.h proves vtable and the inherited B4 Node prefix.
    // AudioSound is accepted only by inventory mode; its tail is never read.
    internal const uint AudioSoundVtable = 0x6EEEA4;
    internal const uint SkinVtable = 0x6E8C5C;
    internal const int MaximumNodes = 1024;
    internal const int MaximumChildrenPerNode = 256;
    internal const int MaximumRenderablesPerNode = 64;
    internal const int MaximumCollisionsPerNode = 64;
    internal const int MaximumSkins = 128;
    internal const int MaximumPaletteSlots = 256;
    internal const int MaximumTotalBoneReferences = 4096;
    internal const int MaximumReadCalls = 8192;
    internal const int MaximumReadBytes = 2 * 1024 * 1024;
    private readonly nint _processHandle;

    internal VisualGraphReader(nint processHandle)
    {
        if (processHandle == 0 || processHandle == -1)
            throw new ArgumentException("A borrowed owned-game process handle is required", nameof(processHandle));
        _processHandle = processHandle;
    }

    /// <summary>
    /// Root may have an external parent; descendants must have exactly one
    /// parent in this tree. forbiddenNodes is a caller-owned, stopped local
    /// graph inventory, not a claim that this reader enumerated the whole heap.
    /// inspectSkins=false inventories validated Node ownership and renderable
    /// pointer vectors without interpreting their items as a particular type.
    /// Such an inventory cannot qualify a passive visual selection.
    /// Failure returns bounded partial records, never a selectable graph.
    /// </summary>
    internal VisualGraphReadResult Read(uint root, IReadOnlySet<uint>? forbiddenNodes = null,
        bool inspectSkins = true)
    {
        var state = new ReadState(_processHandle, root, forbiddenNodes, inspectSkins);
        try
        {
            state.ReadTree();
            return new(true, null, state.Snapshot(true));
        }
        catch (Exception exception) when (exception is InvalidDataException or Win32Exception or OverflowException)
        {
            return new(false, exception.Message, state.Snapshot(false));
        }
    }

    /// <summary>
    /// Fresh detached-SMO policy, derived only from ownership and Skin bone
    /// relationships. The common attachment parent is omitted only when all
    /// omitted ancestors have exact identity local transforms. Native reparent
    /// preserves a child's local transform, not its old world transform.
    /// </summary>
    internal static VisualGraphSelection SelectVisualParts(VisualGraphSnapshot graph)
    {
        VisualGraphSelection Fail(string error) => new(false, error, 0, 0, 0, [], [], [], false, []);
        if (!graph.Complete || graph.Nodes.Count == 0 || graph.Nodes.Count > MaximumNodes)
            return Fail("A complete bounded graph is required");
        if (!graph.SkinInspectionPerformed)
            return Fail("A completed strict Skin inspection is required for visual selection");
        if (graph.Nodes.Any(n => n.Kind == NativeVisualNodeKind.AudioSound))
            return Fail("A passive visual source cannot contain AudioSound nodes");
        var nodes = new Dictionary<uint, VisualNodeRead>();
        foreach (var node in graph.Nodes)
            if (!nodes.TryAdd(node.Address, node)) return Fail("Duplicate node record");
        if (!nodes.TryGetValue(graph.Root, out var root) || root.Parent != 0)
            return Fail("Selection requires a detached source root");
        if (graph.Nodes.Any(n => n.Scene != 0))
            return Fail("Selection requires an entirely unregistered source tree");
        var renders = graph.Nodes.Where(n => n.Kind == NativeVisualNodeKind.RenderNode).ToArray();
        if (renders.Length == 0 || graph.Skins.Count == 0 || graph.Skins.Any(s => s.BoneCount == 0))
            return Fail("Source has no complete Skin/render-node graph");
        var bones = graph.Skins.SelectMany(s => s.BoneNodes).Distinct().ToArray();
        if (bones.Length == 0 || bones.Any(b => !nodes.ContainsKey(b)))
            return Fail("Every Skin bone must belong to the source tree");

        // All paths are bounded and must terminate at the supplied root.
        List<uint>? Path(uint address)
        {
            var result = new List<uint>();
            var seen = new HashSet<uint>();
            while (nodes.TryGetValue(address, out var node) && seen.Add(address) && result.Count < MaximumNodes)
            {
                result.Add(address);
                if (address == graph.Root) { result.Reverse(); return result; }
                address = node.Parent;
            }
            return null;
        }
        var paths = new List<List<uint>>();
        foreach (uint address in renders.Select(r => r.Address).Concat(bones).Distinct())
        {
            var path = Path(address);
            if (path is null) return Fail("Disconnected or cyclic ownership path");
            paths.Add(path);
        }
        int shared = 0, shortest = paths.Min(p => p.Count);
        while (shared < shortest && paths.All(p => p[shared] == paths[0][shared])) ++shared;
        if (shared == 0) return Fail("No common attachment ancestor");
        uint common = paths[0][shared - 1];
        if (nodes[common].Kind != NativeVisualNodeKind.Node)
            return Fail("The omitted common attachment parent must be an exact plain Node");
        var boneBranches = new HashSet<uint>();
        foreach (uint bone in bones)
        {
            var path = Path(bone)!;
            if (path.Count <= shared) return Fail("A bone is the common attachment parent");
            boneBranches.Add(path[shared]);
        }
        if (boneBranches.Count != 1) return Fail("Bones span multiple sibling subtrees");
        uint skeleton = boneBranches.Single();
        var renderBranches = new HashSet<uint>();
        foreach (var render in renders)
        {
            var path = Path(render.Address)!;
            if (path.Count <= shared) return Fail("A render node is the common attachment parent");
            if (path[shared] != skeleton) renderBranches.Add(path[shared]);
        }
        if (renderBranches.Count != 1) return Fail("Expected exactly one render branch outside the skeleton");
        uint primary = renderBranches.Single();

        var selected = new HashSet<uint>();
        var pending = new Stack<uint>(new[] { skeleton, primary });
        while (pending.Count != 0)
        {
            uint address = pending.Pop();
            if (!nodes.TryGetValue(address, out var node) || !selected.Add(address))
                return Fail("Invalid or overlapping visual subtrees");
            if (node.Kind is NativeVisualNodeKind.Light or NativeVisualNodeKind.LightData or NativeVisualNodeKind.DXLight || node.Collisions.Items.Count != 0)
                return Fail("Selected visual subtrees contain light or collision relationships");
            if ((node.Flags & 0x2000) != 0)
                return Fail("Selected nodes are already bound to an actor");
            foreach (uint child in node.Children) pending.Push(child);
        }
        if (renders.Any(r => !selected.Contains(r.Address)) || bones.Any(b => !selected.Contains(b)))
            return Fail("Visual roots do not own every render node and Skin bone");
        bool identity = paths[0].Take(shared).All(a => IsIdentity(nodes[a].LocalTransform));
        if (!identity) return Fail("Omitted ancestors have a nonidentity local transform");
        var omittedAncestors = paths[0].Take(shared).ToHashSet();
        uint[] excluded = graph.Nodes.Where(n => !selected.Contains(n.Address) &&
            !omittedAncestors.Contains(n.Address) && omittedAncestors.Contains(n.Parent))
            .Select(n => n.Address).ToArray();
        // These are diagnostic source branches, not separately owned handles
        // or a deletion list. Source ancestors still own them. Animated nodes
        // outside palette reachability may be SAN track targets; this selection
        // proves a visual ownership cut, not complete animation-track coverage.
        uint[] omittedAnimated = graph.Nodes.Where(n => !selected.Contains(n.Address) &&
            (n.Flags & 0x800) != 0).Select(n => n.Address).ToArray();
        return new(true, null, common, primary, skeleton, Freeze(new[] { primary, skeleton }),
            Freeze(graph.Nodes.Where(n => selected.Contains(n.Address)).Select(n => n.Address)), Freeze(excluded), true,
            Freeze(omittedAnimated));
    }

    private static bool IsIdentity(VisualNodeTransform transform) =>
        transform.Position.Count == 3 && transform.Scale.Count == 3 && transform.Orientation.Count == 9 &&
        transform.Position.All(v => v == 0) && transform.Scale.All(v => v == 1) &&
        transform.Orientation.Select((v, i) => v == (i is 0 or 4 or 8 ? 1 : 0)).All(v => v);

    private static IReadOnlyList<T> Freeze<T>(IEnumerable<T> values) => Array.AsReadOnly(values.ToArray());

    private sealed class ReadState(nint processHandle, uint root, IReadOnlySet<uint>? forbidden, bool inspectSkins)
    {
        private readonly List<VisualNodeRead> _nodes = [];
        private readonly Dictionary<uint, VisualSkinRead> _skins = [];
        private readonly List<VisualMemoryRead> _reads = [];
        private readonly HashSet<uint> _listHeads = [], _listLinks = [];
        private readonly List<(uint Address, int Size)> _skinStorage = [];
        private int _readBytes, _boneReferences;

        // A partial graph never reports a completed Skin inspection, even if
        // strict inspection was requested and some Skin records were read.
        internal VisualGraphSnapshot Snapshot(bool complete) => new(complete, complete && inspectSkins, root, Freeze(_nodes),
            Freeze(_skins.Values), Freeze(_reads), _readBytes);

        private static void Require(bool value, string message)
        {
            if (!value) throw new InvalidDataException(message);
        }
        private static void Pointer(uint address, string purpose)
        {
            Require(address >= 0x10000 && (address & 3) == 0, $"Invalid {purpose} pointer 0x{address:X8}");
        }
        private static uint Add(uint address, uint offset) => checked(address + offset);
        private byte[] Bytes(uint address, int length, string purpose)
        {
            Require(length > 0 && address >= 0x10000 && (ulong)address + (uint)length <= 0x100000000UL,
                $"Invalid read range for {purpose}");
            Require(_reads.Count < MaximumReadCalls && length <= MaximumReadBytes - _readBytes,
                "Native graph read budget exceeded");
            _readBytes += length; // Includes a failed read request, not just successful bytes.
            try
            {
                byte[] value = Win32Native.ReadMemory(processHandle, address, length);
                _reads.Add(new(address, length, purpose, true, null));
                return value;
            }
            catch (Win32Exception exception)
            {
                _reads.Add(new(address, length, purpose, false, exception.NativeErrorCode));
                throw;
            }
        }
        private static uint U(byte[] bytes, int offset) => BitConverter.ToUInt32(bytes, offset);
        private static IReadOnlyList<float> Floats(byte[] bytes, int offset, int count)
        {
            var result = new float[count];
            for (int i = 0; i < count; ++i)
            {
                result[i] = BitConverter.ToSingle(bytes, offset + i * 4);
                Require(float.IsFinite(result[i]), "Nonfinite native node transform");
            }
            return Array.AsReadOnly(result);
        }
        private static VisualNodeTransform Transform(byte[] b, int p, int s, int m) =>
            new(Floats(b, p, 3), Floats(b, s, 3), Floats(b, m, 9));

        private VisualNativeVector Vector(uint begin, uint end, uint capacity, int maximum, string purpose)
        {
            if (begin == 0)
            {
                Require(end == 0 && capacity == 0, $"Inconsistent empty {purpose} vector");
                return new(0, 0, 0, []);
            }
            Pointer(begin, purpose); Pointer(end, purpose); Pointer(capacity, purpose);
            Require(end >= begin && capacity >= end && (end - begin) % 4 == 0 &&
                (capacity - begin) % 4 == 0 && (capacity - begin) <= 64 * 1024 &&
                (end - begin) / 4 <= maximum, $"Invalid bounded {purpose} vector");
            int count = checked((int)((end - begin) / 4));
            if (count == 0) return new(begin, end, capacity, []);
            byte[] values = Bytes(begin, checked(count * 4), purpose);
            var result = new uint[count];
            for (int i = 0; i < count; ++i)
            {
                result[i] = U(values, i * 4);
                Pointer(result[i], purpose + " item");
            }
            return new(begin, end, capacity, Array.AsReadOnly(result));
        }

        private IReadOnlyList<VisualChildLink> Children(uint head, uint count)
        {
            Pointer(head, "child-list sentinel");
            Require(count <= MaximumChildrenPerNode && _listHeads.Add(head) && !_listLinks.Contains(head),
                "Duplicate sentinel or oversized native child list");
            byte[] sentinel = Bytes(head, 8, "child-list sentinel");
            uint link = U(sentinel, 0), last = U(sentinel, 4), previous = head;
            var result = new List<VisualChildLink>();
            for (uint i = 0; i < count; ++i)
            {
                Pointer(link, "child-list entry");
                Require(link != head && !_listHeads.Contains(link) && _listLinks.Add(link),
                    "Premature sentinel, repeated link or child-list cycle");
                byte[] entry = Bytes(link, 12, "child-list entry");
                uint next = U(entry, 0), back = U(entry, 4), child = U(entry, 8);
                Require(back == previous, "Native child-list backlink mismatch");
                Pointer(child, "child");
                result.Add(new(link, next, back, child));
                previous = link; link = next;
            }
            Require(link == head && last == previous, "Native child-list count or sentinel backlink mismatch");
            return Freeze(result);
        }

        private void SkinStorage(uint address, int size)
        {
            if (size == 0) return;
            Pointer(address, "owned Skin storage");
            Require((ulong)address + (uint)size <= 0x100000000UL &&
                _skinStorage.All(r => (ulong)address + (uint)size <= r.Address ||
                    (ulong)r.Address + (uint)r.Size <= address), "Overlapping owned Skin arrays");
            _skinStorage.Add((address, size));
        }
        private void Skin(uint address)
        {
            if (_skins.ContainsKey(address)) return; // Renderable aliases preserve their vector occurrences.
            Require(_skins.Count < MaximumSkins, "Skin count bound exceeded");
            uint actualVtable = U(Bytes(address, sizeof(uint), "renderable vtable"), 0);
            Require(actualVtable == SkinVtable,
                $"Unsupported renderable vtable 0x{actualVtable:X8} at 0x{address:X8}; expected exact spSkin 0x{SkinVtable:X8}");
            byte[] skin = Bytes(address, 0x70, "Skin prefix");
            Require(U(skin, 0) == SkinVtable, $"Skin identity changed during the stopped read at 0x{address:X8}");
            uint count = U(skin, 0x64), bones = U(skin, 0x68), matrices = U(skin, 0x6C);
            Require(count <= MaximumPaletteSlots && count <= MaximumTotalBoneReferences - _boneReferences,
                "Skin palette bound exceeded");
            _boneReferences += checked((int)count);
            var targets = new uint[checked((int)count)];
            byte[] matrixBytes = [];
            if (count != 0)
            {
                int boneBytes = checked((int)count * 4), matrixSize = checked((int)count * 64);
                SkinStorage(bones, boneBytes); SkinStorage(matrices, matrixSize);
                byte[] pointers = Bytes(bones, boneBytes, "Skin bone references");
                matrixBytes = Bytes(matrices, matrixSize, "Skin inverse-bind matrices");
                for (int i = 0; i < targets.Length; ++i)
                {
                    targets[i] = U(pointers, i * 4); Pointer(targets[i], "Skin bone");
                    Require(forbidden?.Contains(targets[i]) != true, "Skin bone aliases the forbidden local graph");
                }
            }
            // Zero-length original allocations may return nonnull pointers;
            // never dereference them or infer an owning Node from the address.
            _skins.Add(address, new(address, SkinVtable, U(skin, 0x60), count, bones, matrices,
                Array.AsReadOnly(targets), Convert.ToHexString(SHA256.HashData(matrixBytes))));
        }

        internal void ReadTree()
        {
            Pointer(root, "root");
            Require(forbidden is null || forbidden.Count <= 65536, "Forbidden-node inventory bound exceeded");
            var reserved = new HashSet<uint> { root };
            var pending = new Stack<(uint Address, uint? Parent, int Depth)>();
            pending.Push((root, null, 0));
            uint? expectedScene = null;
            while (pending.Count != 0)
            {
                var (address, expectedParent, depth) = pending.Pop();
                Require(_nodes.Count < MaximumNodes && depth <= 128, "Node count/depth bound exceeded");
                Require(forbidden?.Contains(address) != true, "Node aliases the forbidden local graph");
                byte[] node = Bytes(address, 0xB4, "Node prefix");
                uint table = U(node, 0);
                NativeVisualNodeKind kind = table switch
                {
                    NodeVtable => NativeVisualNodeKind.Node,
                    RenderNodeVtable => NativeVisualNodeKind.RenderNode,
                    LightVtable => NativeVisualNodeKind.Light,
                    LightDataVtable => NativeVisualNodeKind.LightData,
                    DXLightVtable => NativeVisualNodeKind.DXLight,
                    AudioSoundVtable when !inspectSkins => NativeVisualNodeKind.AudioSound,
                    _ => throw new InvalidDataException($"Unsupported node vtable 0x{table:X8} at 0x{address:X8}")
                };
                uint parent = U(node, 0x2C), scene = U(node, 0x3C);
                if (parent != 0) Pointer(parent, "parent");
                if (scene != 0) Pointer(scene, "scene link");
                Require(parent != address && (!expectedParent.HasValue || parent == expectedParent.Value),
                    "Node parent disagrees with its owning child-list relation");
                ushort references = BitConverter.ToUInt16(node, 8);
                Require(parent == 0 || references != 0, "Parent-owned Node has no intrusive reference");
                expectedScene ??= scene;
                Require(scene == expectedScene, "Mixed scene links inside the owned Node tree");
                uint head = U(node, 0x18), childCount = U(node, 0x1C);
                var links = Children(head, childCount);
                foreach (var link in links)
                    Require(reserved.Count < MaximumNodes && reserved.Add(link.Child),
                        "Duplicate child owner, cycle or node count bound exceeded");
                var collisions = Vector(U(node, 0x68), U(node, 0x6C), U(node, 0x70),
                    MaximumCollisionsPerNode, "collision references");
                foreach (uint collision in collisions.Items) _ = Bytes(collision, 4, "collision vtable");
                VisualNativeVector? renderables = null;
                uint? lightType = null; bool? lightEnabled = null;
                if (kind == NativeVisualNodeKind.RenderNode)
                {
                    byte[] vector = Bytes(Add(address, 0xBC), 12, "RenderNode renderable vector");
                    renderables = Vector(U(vector, 0), U(vector, 4), U(vector, 8),
                        MaximumRenderablesPerNode, "renderable references");
                    if (inspectSkins)
                        foreach (uint renderable in renderables.Items) Skin(renderable);
                }
                else if (kind is NativeVisualNodeKind.Light or NativeVisualNodeKind.LightData or NativeVisualNodeKind.DXLight)
                {
                    byte[] light = Bytes(Add(address, 0xC0), 0x2E, "Light parameters");
                    lightType = U(light, 0); lightEnabled = light[0x2D] != 0;
                }
                _nodes.Add(new(address, table, kind, references, U(node, 0x10), parent, scene, U(node, 0xB0),
                    Transform(node, 0x20, 0x30, 0x40), Transform(node, 0x74, 0x80, 0x8C), head, childCount,
                    Freeze(links.Select(l => l.Child)), links, collisions, renderables, lightType, lightEnabled));
                for (int i = links.Count - 1; i >= 0; --i)
                    pending.Push((links[i].Child, address, depth + 1));
            }
            foreach (var skin in _skins.Values)
                foreach (uint bone in skin.BoneNodes)
                    Require(reserved.Contains(bone), "Skin bone has no owner in this Node tree");
        }
    }
}
