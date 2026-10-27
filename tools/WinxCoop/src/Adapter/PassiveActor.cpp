#include "PassiveActor.h"
#include <new>

static_assert(sizeof(void*) == 4, "PassiveActor requires the pinned x86 game ABI");

namespace winxcoop::adapter
{
    namespace
    {
        using U = std::uint32_t;
        constexpr U NodeTable = 0x6DC4F4, RenderTable = 0x6DCAA4, SkinTable = 0x6E8C5C;
        constexpr U LightTable = 0x6DCC18, LightDataTable = 0x6DE990, DXLightTable = 0x6F0C88, AudioTable = 0x6EEEA4;
        constexpr U MaxNodes = 1024, MaxSkins = 128, MaxBones = 256, MaxRenderables = 8192;
        bool Pointer(U value) noexcept { return value >= 0x10000 && !(value & 3); }
        bool Equal(const void* left, const void* right, SIZE_T size) noexcept
        {
            const auto* a = static_cast<const BYTE*>(left); const auto* b = static_cast<const BYTE*>(right);
            for (SIZE_T i = 0; i < size; ++i) if (a[i] != b[i]) return false;
            return true;
        }
        U Word(const BYTE* bytes, U at) noexcept { U value{}; CopyMemory(&value, bytes + at, 4); return value; }
        bool Finite(float value) noexcept { U bits{}; CopyMemory(&bits, &value, 4); return (bits & 0x7F800000) != 0x7F800000; }
        bool FiniteWords(const float* values, U count) noexcept
        { for (U i = 0; i < count; ++i) if (!Finite(values[i])) return false; return true; }
        float Absolute(float value) noexcept { return value < 0 ? -value : value; }
        bool Matrix(const float* values) noexcept
        {
            if (!FiniteWords(values, 9)) return false;
            for (U row = 0; row < 3; ++row) for (U other = row; other < 3; ++other)
            {
                float dot{};
                for (U c = 0; c < 3; ++c) dot += values[row * 3 + c] * values[other * 3 + c];
                if (Absolute(dot - (row == other ? 1.0f : 0.0f)) > 0.02f) return false;
            }
            const float determinant = values[0] * (values[4] * values[8] - values[5] * values[7])
                - values[1] * (values[3] * values[8] - values[5] * values[6])
                + values[2] * (values[3] * values[7] - values[4] * values[6]);
            return determinant > 0.95f && determinant < 1.05f;
        }
        struct GraphNode final
        {
            U address{}, table{}, parent{}, scene{}, flags{}, name{}, head{}, childStart{}, childCount{}, renderStart{}, renderCount{};
            USHORT references{};
            Transform local{};
            bool collisions{};
        };
        struct GraphSkin final
        { U address{}, count{}, bones{}, matrices{}, material{}, mesh{}, targets[MaxBones]{}; };
        struct Storage final { U address{}, size{}; };
        struct Pending final { U address{}, parent{}, depth{}; bool hasParent{}; };
        struct Graph final
        {
            U root{}, count{}, skinCount{}, childCount{}, renderCount{}, headCount{}, linkCount{}, storageCount{}, totalBones{}, reads{}, readBytes{};
            GraphNode nodes[MaxNodes]{};
            GraphSkin skins[MaxSkins]{};
            U children[MaxNodes]{}, renderables[MaxRenderables]{}, heads[MaxNodes]{}, links[MaxNodes]{};
            Storage storage[MaxSkins * 2]{};
            Pending pending[MaxNodes]{};
            U pendingCount{};
            int Find(U address) const noexcept
            { for (U i = 0; i < count; ++i) if (nodes[i].address == address) return static_cast<int>(i); return -1; }
            int FindSkin(U address) const noexcept
            { for (U i = 0; i < skinCount; ++i) if (skins[i].address == address) return static_cast<int>(i); return -1; }
            bool Contains(U address) const noexcept { return Find(address) >= 0; }
        };
        struct MaterialSnapshot final { U address{}; BYTE bytes[0xBC]{}; };
        struct LocalSkin final { U address{}, mesh{}, material{}; };
        bool Identity(const Transform& t) noexcept
        {
            for (U i = 0; i < 3; ++i) if (t.position[i] != 0 || t.scale[i] != 1) return false;
            for (U i = 0; i < 9; ++i) if (t.orientation[i] != (i == 0 || i == 4 || i == 8 ? 1 : 0)) return false;
            return true;
        }
    }

    struct PassiveActor::State final
    {
        PassiveActor* owner{};
        Graph local{}, sourceGraph{}, hairGraph{}, combined{};
        BYTE matrixScratch[MaxBones * 64]{};
        U profile{}, record{}, localRoot{}, parent{}, scene{}, input{}, inputState{}, inputDevice{}, camera{};
        U source{}, hair{}, wrapper{}, actor{}, clips[3]{}, activeAnimation{}, playback{};
        bool sourceOwned{}, hairOwned{}, wrapperOwned{}, sourceQualified{}, hairQualified{}, wrapperQualified{}, attached{}, created{}, busy{}, readFailed{};
        Transform initial{};
        char bodyPath[MAX_PATH]{}, hairPath[MAX_PATH]{};
        BYTE request[0x38]{};
        float appearanceRgb[3]{};
        MaterialSnapshot localMaterials[MaxSkins]{};
        U localMaterialCount{}, ownMaterials[MaxSkins]{}, ownMaterialCount{};
        BYTE expectedAmbient[16]{};

        bool Fail(PassiveActorError error, const char* text, bool poison = false) noexcept { return owner->Fail(error, text, poison); }
        bool Read(U at, void* output, SIZE_T size) noexcept
        {
            SIZE_T got{};
            if (at < 0x10000 || !size || size > 65536 || static_cast<ULONGLONG>(at) + size > 0x100000000ULL ||
                !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(at), output, size, &got) || got != size)
            {
                readFailed = true;
                return Fail(PassiveActorError::Memory, "bounded native read failed", source || hair || wrapper || actor);
            }
            return true;
        }
        bool Get(U at, U& value) noexcept { return Read(at, &value, 4); }
        U Value(U at) noexcept { U value{}; Get(at, value); return value; }
        bool Put(U at, const void* inputBytes, SIZE_T size) noexcept
        {
            if (!owner->Phase() || readFailed) return false;
            BYTE verify[64]{}; SIZE_T wrote{};
            if (!size || size > sizeof(verify) || at < 0x10000 || static_cast<ULONGLONG>(at) + size > 0x100000000ULL ||
                !WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(at), inputBytes, size, &wrote) || wrote != size ||
                !Read(at, verify, size) || !Equal(verify, inputBytes, size))
                return Fail(PassiveActorError::Memory, "owned native write/readback failed", true);
            return true;
        }
        bool Table(U at, U table) noexcept
        { U actual{}; return Get(at, actual) && (actual == table || Fail(PassiveActorError::Graph, "native object vtable mismatch")); }
        bool References(U at, USHORT& value) noexcept { return Read(at + 8, &value, 2); }
        bool SetReferences(U at, USHORT value) noexcept { return Put(at + 8, &value, 2); }
        bool Function(U at) noexcept
        {
            if (!owner->Phase()) return false;
            if (readFailed) return Fail(PassiveActorError::Memory, "previous read failed; no native invocation is permitted", source || hair || wrapper || actor);
            if (at < 0x401000 || at > 0x6DA000 || !owner->context_.gate(at, 6))
                return Fail(PassiveActorError::Signature, "native function did not match pinned original bytes");
            return true;
        }
        bool NativeException() noexcept { return Fail(PassiveActorError::NativeException, "native call raised an exception; terminate the owned game", true); }
        bool Factory(U at, U& result) noexcept
        {
            if (!Function(at)) return false;
            ++owner->statistics_.nativeCalls;
            __try { result = reinterpret_cast<U(__cdecl*)()>(at)(); }
            __except (EXCEPTION_EXECUTE_HANDLER) { return NativeException(); }
            return !owner->poisoned_;
        }
        bool Void0(U at, U receiver) noexcept
        {
            if (!Function(at)) return false;
            ++owner->statistics_.nativeCalls;
            __try { reinterpret_cast<void(__thiscall*)(void*)>(at)(reinterpret_cast<void*>(receiver)); }
            __except (EXCEPTION_EXECUTE_HANDLER) { return NativeException(); }
            return !owner->poisoned_;
        }
        bool Void1(U at, U receiver, U argument) noexcept
        {
            if (!Function(at)) return false;
            ++owner->statistics_.nativeCalls;
            __try { reinterpret_cast<void(__thiscall*)(void*, U)>(at)(reinterpret_cast<void*>(receiver), argument); }
            __except (EXCEPTION_EXECUTE_HANDLER) { return NativeException(); }
            return !owner->poisoned_;
        }
        bool Load(const char* path, U& result) noexcept
        {
            U manager{};
            if (!Get(0x75DB78, manager) || !Pointer(manager) || !Function(0x458260)) return false;
            ++owner->statistics_.nativeCalls;
            __try { result = reinterpret_cast<U(__thiscall*)(void*, const char*)>(0x458260)(reinterpret_cast<void*>(manager), path); }
            __except (EXCEPTION_EXECUTE_HANDLER) { return NativeException(); }
            return !owner->poisoned_;
        }
        bool Animation(const char* name, U& result) noexcept
        {
            U manager{};
            if (!Get(0x765ADC, manager) || !Pointer(manager) || !Function(0x59B780)) return false;
            ++owner->statistics_.nativeCalls;
            __try { result = reinterpret_cast<U(__thiscall*)(void*, const char*, U)>(0x59B780)(reinterpret_cast<void*>(manager), name, 0); }
            __except (EXCEPTION_EXECUTE_HANDLER) { return NativeException(); }
            return !owner->poisoned_;
        }
        bool Retain(U at) noexcept
        {
            USHORT count{};
            if (!References(at, count)) return false;
            if (count == 0xFFFF) return Fail(PassiveActorError::Ownership, "intrusive reference overflow");
            return SetReferences(at, static_cast<USHORT>(count + 1));
        }
        bool DeleteRoot(U& at, bool& owned) noexcept
        {
            if (!at) return true;
            USHORT count{}; U parentAt{}, table{}, deleting{};
            if (!owned || !References(at, count) || count != 1 || !Get(at + 0x2C, parentAt) || parentAt ||
                !Get(at, table) || !Get(table, deleting))
                return Fail(PassiveActorError::Ownership, "detached root does not have exactly the adapter owner", true);
            if (!Function(deleting) || !SetReferences(at, 0)) return false;
            owned = false;
            if (!Void1(deleting, at, 1)) return false;
            at = 0; // Native deletion completed; never inspect freed bytes.
            return true;
        }
        bool GraphRead(Graph& graph, U at, void* output, SIZE_T size) noexcept
        {
            if (++graph.reads > 8192 || size > 2 * 1024 * 1024 - graph.readBytes)
                return Fail(PassiveActorError::Capacity, "graph read budget exceeded");
            graph.readBytes += static_cast<U>(size);
            return Read(at, output, size);
        }
        bool Vector(Graph& graph, U begin, U end, U capacity, U maximum, U* output, U& count) noexcept
        {
            count = 0;
            if (!begin) return (!end && !capacity) || Fail(PassiveActorError::Graph, "inconsistent empty vector");
            if (!Pointer(begin) || !Pointer(end) || !Pointer(capacity) || end < begin || capacity < end ||
                (end - begin) % 4 || (capacity - begin) % 4 || capacity - begin > 65536 || (end - begin) / 4 > maximum)
                return Fail(PassiveActorError::Graph, "unbounded native vector");
            count = (end - begin) / 4;
            if (count && !GraphRead(graph, begin, output, count * 4)) return false;
            for (U i = 0; i < count; ++i) if (!Pointer(output[i])) return Fail(PassiveActorError::Graph, "invalid vector element");
            return true;
        }
        bool StorageRange(Graph& graph, U at, U size) noexcept
        {
            if (!Pointer(at) || static_cast<ULONGLONG>(at) + size > 0x100000000ULL || graph.storageCount == MaxSkins * 2)
                return Fail(PassiveActorError::Graph, "invalid skin storage");
            for (U i = 0; i < graph.storageCount; ++i)
            {
                const auto& other = graph.storage[i];
                if (static_cast<ULONGLONG>(at) + size > other.address && static_cast<ULONGLONG>(other.address) + other.size > at)
                    return Fail(PassiveActorError::Graph, "owned skin arrays overlap");
            }
            graph.storage[graph.storageCount++] = {at, size};
            return true;
        }
        bool Skin(Graph& graph, U at, const Graph* forbidden) noexcept
        {
            if (graph.FindSkin(at) >= 0) return true;
            if (graph.skinCount == MaxSkins) return Fail(PassiveActorError::Capacity, "skin count exceeded");
            BYTE bytes[0x70]{};
            if (!GraphRead(graph, at, bytes, sizeof(bytes)) || Word(bytes, 0) != SkinTable)
                return Fail(PassiveActorError::Graph, "strict renderable is not an exact Skin");
            auto& skin = graph.skins[graph.skinCount++];
            skin.address = at; skin.count = Word(bytes, 0x64); skin.bones = Word(bytes, 0x68); skin.matrices = Word(bytes, 0x6C);
            skin.material = Word(bytes, 0x20); skin.mesh = Word(bytes, 0x58);
            if (!skin.count || skin.count > MaxBones || skin.count > 4096 - graph.totalBones ||
                !Pointer(skin.material) || !Pointer(skin.mesh)) return Fail(PassiveActorError::Graph, "skin palette/material bound failed");
            graph.totalBones += skin.count;
            if (!StorageRange(graph, skin.bones, skin.count * 4) || !StorageRange(graph, skin.matrices, skin.count * 64) ||
                !GraphRead(graph, skin.bones, skin.targets, skin.count * 4) ||
                !GraphRead(graph, skin.matrices, matrixScratch, skin.count * 64)) return false;
            for (U i = 0; i < skin.count; ++i)
                if (!Pointer(skin.targets[i]) || (forbidden && forbidden->Contains(skin.targets[i])))
                    return Fail(PassiveActorError::Graph, "skin bone is invalid or aliases the local graph");
            return true;
        }
        bool Tree(Graph& graph, U rootAt, bool strictSkins, const Graph* forbidden) noexcept
        {
            ZeroMemory(&graph, sizeof(graph)); graph.root = rootAt;
            if (!Pointer(rootAt)) return Fail(PassiveActorError::Graph, "invalid root");
            graph.pending[graph.pendingCount++] = {rootAt, 0, 0, false};
            bool first = true; U expectedScene{};
            while (graph.pendingCount)
            {
                const Pending current = graph.pending[--graph.pendingCount];
                if (graph.count == MaxNodes || current.depth > 128 || graph.Contains(current.address) ||
                    (forbidden && forbidden->Contains(current.address))) return Fail(PassiveActorError::Graph, "node cycle/alias/bound failed");
                BYTE bytes[0xB4]{};
                if (!GraphRead(graph, current.address, bytes, sizeof(bytes))) return false;
                auto& node = graph.nodes[graph.count++];
                node.address = current.address; node.table = Word(bytes, 0); node.parent = Word(bytes, 0x2C); node.scene = Word(bytes, 0x3C);
                node.flags = Word(bytes, 0xB0); node.name = Word(bytes, 0x10); node.head = Word(bytes, 0x18); node.childCount = Word(bytes, 0x1C);
                CopyMemory(&node.references, bytes + 8, 2);
                CopyMemory(node.local.position, bytes + 0x20, 12); CopyMemory(node.local.scale, bytes + 0x30, 12);
                CopyMemory(node.local.orientation, bytes + 0x40, 36);
                if (node.table != NodeTable && node.table != RenderTable && node.table != LightTable && node.table != LightDataTable &&
                    node.table != DXLightTable && (strictSkins || node.table != AudioTable))
                    return Fail(PassiveActorError::Graph, "unsupported Node physical type");
                if ((node.parent && !Pointer(node.parent)) || (node.scene && !Pointer(node.scene)) || node.parent == node.address ||
                    (current.hasParent && node.parent != current.parent) || (node.parent && !node.references) ||
                    !FiniteWords(node.local.position, 3) || !FiniteWords(node.local.scale, 3) || !FiniteWords(node.local.orientation, 9))
                    return Fail(PassiveActorError::Graph, "node ownership/transform failed");
                float world[15]{};
                CopyMemory(world, bytes + 0x74, 60);
                if (!FiniteWords(world, 15)) return Fail(PassiveActorError::Graph, "nonfinite cached world transform");
                if (first) { expectedScene = node.scene; first = false; }
                if (node.scene != expectedScene || !Pointer(node.head) || node.childCount > 256 || graph.childCount + node.childCount > MaxNodes)
                    return Fail(PassiveActorError::Graph, "scene/child-list bound failed");
                for (U i = 0; i < graph.headCount; ++i) if (graph.heads[i] == node.head) return Fail(PassiveActorError::Graph, "repeated child sentinel");
                for (U i = 0; i < graph.linkCount; ++i) if (graph.links[i] == node.head) return Fail(PassiveActorError::Graph, "sentinel aliases a list entry");
                graph.heads[graph.headCount++] = node.head;
                U sentinel[2]{};
                if (!GraphRead(graph, node.head, sentinel, 8)) return false;
                U link = sentinel[0], previous = node.head;
                node.childStart = graph.childCount;
                for (U i = 0; i < node.childCount; ++i)
                {
                    if (!Pointer(link) || link == node.head || graph.linkCount == MaxNodes)
                        return Fail(PassiveActorError::Graph, "premature sentinel/list bound failed");
                    for (U j = 0; j < graph.headCount; ++j) if (graph.heads[j] == link) return Fail(PassiveActorError::Graph, "entry aliases a sentinel");
                    for (U j = 0; j < graph.linkCount; ++j) if (graph.links[j] == link) return Fail(PassiveActorError::Graph, "repeated child-list entry");
                    graph.links[graph.linkCount++] = link;
                    U entry[3]{};
                    if (!GraphRead(graph, link, entry, 12) || entry[1] != previous || !Pointer(entry[2]))
                        return Fail(PassiveActorError::Graph, "child backlink/pointer failed");
                    if (graph.pendingCount == MaxNodes) return Fail(PassiveActorError::Capacity, "pending graph bound failed");
                    graph.children[graph.childCount++] = entry[2];
                    graph.pending[graph.pendingCount++] = {entry[2], node.address, current.depth + 1, true};
                    previous = link; link = entry[0];
                }
                if (link != node.head || sentinel[1] != previous) return Fail(PassiveActorError::Graph, "child-list count/sentinel failed");
                U collisionPointers[64]{}, collisionCount{};
                if (!Vector(graph, Word(bytes, 0x68), Word(bytes, 0x6C), Word(bytes, 0x70), 64, collisionPointers, collisionCount)) return false;
                node.collisions = collisionCount != 0;
                for (U i = 0; i < collisionCount; ++i) { U table{}; if (!GraphRead(graph, collisionPointers[i], &table, 4)) return false; }
                if (node.table == RenderTable)
                {
                    U vector[3]{}, values[64]{}, count{};
                    if (!GraphRead(graph, node.address + 0xBC, vector, 12) || !Vector(graph, vector[0], vector[1], vector[2], 64, values, count)) return false;
                    if (count > MaxRenderables - graph.renderCount) return Fail(PassiveActorError::Capacity, "renderable graph bound failed");
                    node.renderStart = graph.renderCount; node.renderCount = count;
                    for (U i = 0; i < count; ++i)
                    { graph.renderables[graph.renderCount++] = values[i]; if (strictSkins && !Skin(graph, values[i], forbidden)) return false; }
                }
            }
            for (U i = 0; i < graph.skinCount; ++i) for (U b = 0; b < graph.skins[i].count; ++b)
                if (!graph.Contains(graph.skins[i].targets[b])) return Fail(PassiveActorError::Graph, "skin bone has no graph owner");
            return true;
        }

        bool Path(const Graph& graph, U at, U* path, U& count) noexcept
        {
            U reverse[129]{}; count = 0;
            for (;;)
            {
                const int index = graph.Find(at);
                if (index < 0 || count == 129) return Fail(PassiveActorError::Graph, "broken ancestor path");
                reverse[count++] = at;
                if (at == graph.root) break;
                at = graph.nodes[index].parent;
            }
            for (U i = 0; i < count; ++i) path[i] = reverse[count - 1 - i];
            return true;
        }
        bool Under(const Graph& graph, U at, U rootAt) noexcept
        {
            for (U depth = 0; depth <= 128; ++depth)
            {
                if (at == rootAt) return true;
                const int index = graph.Find(at);
                if (index < 0 || at == graph.root) return false;
                at = graph.nodes[index].parent;
            }
            return false;
        }
        bool Passive(const GraphNode& node) noexcept
        { return (node.table == NodeTable || node.table == RenderTable) && !node.collisions; }
        bool SelectBody(U& primary, U& skeleton) noexcept
        {
            const Graph& graph = sourceGraph;
            if (graph.count != 98 || graph.skinCount != 6 || graph.nodes[0].parent || graph.nodes[0].references)
                return Fail(PassiveActorError::Graph, "body does not match the qualified fresh source shape");
            U commonPath[129]{}, shared{};
            bool seeded = false;
            auto AddPath = [&](U at) noexcept -> bool
            {
                U path[129]{}, count{};
                if (!Path(graph, at, path, count)) return false;
                if (!seeded) { CopyMemory(commonPath, path, count * 4); shared = count; seeded = true; }
                else { if (count < shared) shared = count; U i{}; while (i < shared && commonPath[i] == path[i]) ++i; shared = i; }
                return shared != 0 || Fail(PassiveActorError::Graph, "no common visual ancestor");
            };
            for (U i = 0; i < graph.count; ++i)
            {
                if (graph.nodes[i].scene || graph.nodes[i].table == AudioTable) return Fail(PassiveActorError::Graph, "source is scene-attached or contains audio");
                if (graph.nodes[i].table == RenderTable && !AddPath(graph.nodes[i].address)) return false;
            }
            for (U i = 0; i < graph.skinCount; ++i) for (U b = 0; b < graph.skins[i].count; ++b)
                if (!AddPath(graph.skins[i].targets[b])) return false;
            if (!seeded || !shared) return Fail(PassiveActorError::Graph, "empty visual selection");
            for (U i = 0; i < shared; ++i)
            {
                const auto& node = graph.nodes[graph.Find(commonPath[i])];
                if (!Identity(node.local) || (i == shared - 1 && node.table != NodeTable))
                    return Fail(PassiveActorError::Graph, "omitted attachment ancestor is not identity plain Node");
            }
            for (U i = 0; i < graph.skinCount; ++i) for (U b = 0; b < graph.skins[i].count; ++b)
            {
                U path[129]{}, count{};
                if (!Path(graph, graph.skins[i].targets[b], path, count) || count <= shared)
                    return Fail(PassiveActorError::Graph, "bone is common parent");
                if (!skeleton) skeleton = path[shared];
                else if (skeleton != path[shared]) return Fail(PassiveActorError::Graph, "bones span multiple sibling branches");
            }
            for (U i = 0; i < graph.count; ++i) if (graph.nodes[i].table == RenderTable)
            {
                U path[129]{}, count{};
                if (!Path(graph, graph.nodes[i].address, path, count) || count <= shared) return Fail(PassiveActorError::Graph, "render is common parent");
                if (path[shared] != skeleton)
                { if (!primary) primary = path[shared]; else if (primary != path[shared]) return Fail(PassiveActorError::Graph, "multiple independent render branches"); }
            }
            if (!primary || !skeleton || primary == skeleton) return Fail(PassiveActorError::Graph, "missing disjoint render/skeleton branches");
            U selected{};
            for (U i = 0; i < graph.count; ++i)
            {
                const auto& node = graph.nodes[i];
                if (Under(graph, node.address, primary) || Under(graph, node.address, skeleton))
                {
                    if (!Passive(node) || node.flags & 0x2000) return Fail(PassiveActorError::Graph, "selected subtree has nonvisual or previously bound relations");
                    ++selected;
                }
                else if (node.table == RenderTable) return Fail(PassiveActorError::Graph, "render outside visual selection");
            }
            return selected == 90 || Fail(PassiveActorError::Graph, "body ownership cut differs from qualified ninety nodes");
        }
        bool Named(const Graph& graph, const char* wanted, U& result) noexcept
        {
            U matches{};
            for (U i = 0; i < graph.count; ++i) if (graph.nodes[i].name)
            {
                char text[128]{};
                if (!Read(graph.nodes[i].name + 9, text, sizeof(text))) return false;
                U length{}; while (length < sizeof(text) && text[length]) ++length;
                if (length == sizeof(text)) return Fail(PassiveActorError::Graph, "unbounded shared-name text");
                U j{}; while (j < length && wanted[j] && wanted[j] == text[j]) ++j;
                if (j == length && !wanted[j]) { result = graph.nodes[i].address; ++matches; }
            }
            return matches == 1 || Fail(PassiveActorError::Graph, "expected one typed named node");
        }

        bool LocalIdentity(bool capture, bool requirePlaying = true) noexcept
        {
            U p{}, r{}, n{}, flow{}, manager{}, level{}, pending{}, stack{}, active{}, inputAt{}, engine{};
            if (!Get(0x765AD4, p) || !Table(p, 0x6F5A44) || !Get(p + 0x2B4, r) || !Table(r, 0x710F80) ||
                !Get(r + 0x24, n) || !Table(n, NodeTable) || !Get(0x755294, flow) || !Get(0x75DB90, manager) ||
                !Get(flow + 0x1B0, level) || level != 28 || !Get(flow + 0x1B8, pending) ||
                !Get(flow + 0x1AC, stack) || stack > 20 || !Get(flow + 0x15C + stack * 4, active) ||
                (requirePlaying && (pending || Value(active + 0x10) != 28)) ||
                !Get(0x7552A0, inputAt) || !Table(inputAt, 0x6F5508) ||
                !Get(0x755274, engine)) return Fail(PassiveActorError::LocalIdentity, "current stock Alfea28 local identity is unavailable");
            U currentScene{}, componentCount{};
            if (!Get(manager + 0x20, currentScene) || currentScene || !Get(r + 0x20, componentCount) || componentCount != 11)
                return Fail(PassiveActorError::LocalIdentity, "world context or stock component count is unavailable");
            U parentAt{}, sceneAt{}, inputStateAt{}, inputDeviceAt{}, cameraAt{};
            if (!Get(n + 0x2C, parentAt) || !Pointer(parentAt) || !Get(n + 0x3C, sceneAt) || !Table(sceneAt, 0x6E7358) ||
                !Get(inputAt + 0x18, inputStateAt) || !Pointer(inputStateAt) || !Get(inputAt + 0x1C, inputDeviceAt) || !Pointer(inputDeviceAt) ||
                !Get(engine + 0x1C, cameraAt) || !Pointer(cameraAt)) return Fail(PassiveActorError::LocalIdentity, "local owner links are not ready");
            U head{}, link{}, previous{}, last{}; U componentAddresses[11]{}; U kinds[4]{};
            if (!Get(r + 0x1C, head) || !Pointer(head) || !Get(head, link) || !Get(head + 4, last)) return false;
            previous = head;
            for (U i = 0; i < 11; ++i)
            {
                U entry[3]{}, table{};
                if (!Pointer(link) || link == head || !Read(link, entry, 12) || entry[1] != previous || !Pointer(entry[2]) || !Get(entry[2], table))
                    return Fail(PassiveActorError::LocalIdentity, "local component list is not reciprocal");
                for (U j = 0; j < i; ++j) if (componentAddresses[j] == entry[2]) return Fail(PassiveActorError::LocalIdentity, "local component alias");
                componentAddresses[i] = entry[2];
                U kind = table == 0x6F6438 ? 0 : table == 0x6FA288 ? 1 : table == 0x6F64F8 ? 2 : table == 0x6F670C ? 3 : 4;
                if (kind != 4) { ++kinds[kind]; if (Value(entry[2] + 0x24) != r) return Fail(PassiveActorError::LocalIdentity, "local component owner mismatch"); }
                previous = link; link = entry[0];
            }
            if (link != head || last != previous) return Fail(PassiveActorError::LocalIdentity, "local component sentinel mismatch");
            for (U kind : kinds) if (kind != 1) return Fail(PassiveActorError::LocalIdentity, "local component type count mismatch");
            if (capture)
            {
                profile = p; record = r; localRoot = n; parent = parentAt; scene = sceneAt; input = inputAt;
                inputState = inputStateAt; inputDevice = inputDeviceAt; camera = cameraAt;
                if (!Read(n + 0x20, initial.position, 12) || !Read(n + 0x30, initial.scale, 12) || !Read(n + 0x40, initial.orientation, 36) ||
                    !FiniteWords(initial.position, 3) || !FiniteWords(initial.scale, 3) || !Matrix(initial.orientation))
                    return Fail(PassiveActorError::Transform, "local PRS is not qualified");
            }
            else if (profile != p || record != r || localRoot != n || parent != parentAt || scene != sceneAt || input != inputAt ||
                inputState != inputStateAt || inputDevice != inputDeviceAt || camera != cameraAt)
                return Fail(PassiveActorError::LocalIdentity, "local player/input/camera identity changed");
            return true;
        }
        bool Paths(const char* media) noexcept
        {
            if (!media) return Fail(PassiveActorError::Context, "media directory required");
            U length{};
            while (length < MAX_PATH && media[length])
            { if (static_cast<unsigned char>(media[length]) < 32 || static_cast<unsigned char>(media[length]) > 126) return Fail(PassiveActorError::Context, "ASCII media path required"); ++length; }
            if (length < 3 || length >= MAX_PATH || media[1] != ':' || (media[2] != '\\' && media[2] != '/'))
                return Fail(PassiveActorError::Context, "absolute drive media path required");
            while (length && (media[length - 1] == '\\' || media[length - 1] == '/')) --length;
            const char* suffixes[2] = {"\\Characters\\Bloom\\bloom_jeans.smo", "\\Characters\\Bloom\\bloom_hair.smo"};
            char* targets[2] = {bodyPath, hairPath};
            for (U k = 0; k < 2; ++k)
            {
                U n{}; while (suffixes[k][n]) ++n;
                if (length + n >= MAX_PATH) return Fail(PassiveActorError::Context, "media asset path exceeds MAX_PATH");
                CopyMemory(targets[k], media, length); CopyMemory(targets[k] + length, suffixes[k], n + 1);
                const DWORD attributes = GetFileAttributesA(targets[k]);
                if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY)) return Fail(PassiveActorError::Context, "qualified body/hair asset missing");
            }
            return true;
        }
        bool Clip(U at) noexcept
        {
            BYTE bytes[0x24]{}; float duration{};
            if (!Read(at, bytes, sizeof(bytes))) return false;
            CopyMemory(&duration, bytes + 0x14, 4);
            return (Word(bytes, 0) == 0x6DE6CC && Finite(duration) && duration > 0 && Pointer(Word(bytes, 0x1C)) &&
                Word(bytes, 0x20) > 0 && Word(bytes, 0x20) <= 256) || Fail(PassiveActorError::Animation, "unsupported borrowed animation clip");
        }
        bool Start(U index) noexcept
        {
            if (index > 2 || !Clip(clips[index]) || !Function(0x5A1E30)) return false;
            ZeroMemory(request, sizeof(request)); U mode = index == 2 ? 0 : 1; U one = 0x3F800000;
            CopyMemory(request, &clips[index], 4); CopyMemory(request + 4, &mode, 4);
            CopyMemory(request + 0xC, &one, 4); CopyMemory(request + 0x30, &one, 4);
            U result{}; ++owner->statistics_.nativeCalls;
            __try { result = reinterpret_cast<U(__thiscall*)(void*, void*)>(0x5A1E30)(reinterpret_cast<void*>(actor), request); }
            __except (EXCEPTION_EXECUTE_HANDLER) { return NativeException(); }
            if (owner->poisoned_) return false;
            if (result == 0xFFFFFFFF || result >= Value(actor + 0x2C)) return Fail(PassiveActorError::Animation, "native Actor rejected animation request");
            playback = result; activeAnimation = index;
            return true;
        }
        bool Material(U at, BYTE* bytes) noexcept
        {
            if (!Read(at, bytes, 0xBC)) return false;
            return (Word(bytes, 0) == 0x6EF264 && Word(bytes, 0x14) == 0x6EF238) || Fail(PassiveActorError::Appearance, "DXMaterial physical interfaces mismatch");
        }
        bool Appearance() noexcept
        {
            LocalSkin localSkins[MaxSkins]{}; U localSkinCount{};
            for (U n = 0; n < local.count; ++n)
            {
                const auto& node = local.nodes[n];
                for (U r = 0; r < node.renderCount; ++r)
                {
                    U at = local.renderables[node.renderStart + r], table{};
                    if (!Get(at, table)) return false;
                    if (table != SkinTable) continue;
                    bool seen = false; for (U i = 0; i < localSkinCount; ++i) if (localSkins[i].address == at) seen = true;
                    if (seen) continue;
                    if (localSkinCount == MaxSkins) return Fail(PassiveActorError::Capacity, "local appearance Skin bound exceeded");
                    BYTE skin[0x60]{};
                    if (!Read(at, skin, sizeof(skin))) return false;
                    localSkins[localSkinCount++] = {at, Word(skin, 0x58), Word(skin, 0x20)};
                }
            }
            bool ambientSeeded = false;
            for (U n = 0; n < combined.count; ++n) if (combined.nodes[n].table == RenderTable)
            {
                U ambient{}, sceneAt{};
                if (!Get(combined.nodes[n].address + 0x110, ambient) || ambient || !Get(combined.nodes[n].address + 0x3C, sceneAt) || sceneAt)
                    return Fail(PassiveActorError::Appearance, "native color requires detached graph and null cached ambient");
            }
            for (U s = 0; s < combined.skinCount; ++s)
            {
                const auto& own = combined.skins[s]; U matches{}, counterpart{};
                for (U l = 0; l < localSkinCount; ++l)
                {
                    if (own.address == localSkins[l].address || own.material == localSkins[l].material)
                        return Fail(PassiveActorError::Appearance, "owned Skin/material aliases local appearance");
                    if (own.mesh == localSkins[l].mesh) { ++matches; counterpart = localSkins[l].material; }
                }
                if (matches != 1) return Fail(PassiveActorError::Appearance, "owned Skin has no unique local shared-mesh counterpart");
                BYTE ownBytes[0xBC]{}, localBytes[0xBC]{};
                if (!Material(own.material, ownBytes) || !Material(counterpart, localBytes)) return false;
                float ambient[4]{}; CopyMemory(ambient, localBytes + 0x88, 16);
                if (!FiniteWords(ambient, 4) || Word(localBytes, 0x94) != 0x3F800000)
                    return Fail(PassiveActorError::Appearance, "local ambient is not qualified finite alpha-one color");
                if (!ambientSeeded) { CopyMemory(expectedAmbient, localBytes + 0x88, 16); ambientSeeded = true; }
                else if (!Equal(expectedAmbient, localBytes + 0x88, 16)) return Fail(PassiveActorError::Appearance, "local matched ambients differ");
                bool seen = false; for (U i = 0; i < localMaterialCount; ++i) if (localMaterials[i].address == counterpart) seen = true;
                if (!seen) { auto& m = localMaterials[localMaterialCount++]; m.address = counterpart; CopyMemory(m.bytes, localBytes, 0xBC); }
                seen = false; for (U i = 0; i < ownMaterialCount; ++i) if (ownMaterials[i] == own.material) seen = true;
                if (!seen) ownMaterials[ownMaterialCount++] = own.material;
            }
            if (!ambientSeeded || ownMaterialCount != 3 || localMaterialCount != 3)
                return Fail(PassiveActorError::Appearance, "qualified Alfea material set differs from three own/three matched local materials");
            U alpha{};
            if (!Read(profile + 0x840, appearanceRgb, 12) || !FiniteWords(appearanceRgb, 3) || !Get(0x75528C, alpha) || !Pointer(alpha) || !Function(0x594820)) return false;
            ++owner->statistics_.nativeCalls;
            __try { reinterpret_cast<void(__thiscall*)(void*, void*, const float*)>(0x594820)(reinterpret_cast<void*>(alpha), reinterpret_cast<void*>(wrapper), appearanceRgb); }
            __except (EXCEPTION_EXECUTE_HANDLER) { return NativeException(); }
            if (owner->poisoned_) return false;
            for (U i = 0; i < ownMaterialCount; ++i)
            {
                BYTE after[0xBC]{};
                if (!Material(ownMaterials[i], after) || !Equal(after + 0x88, expectedAmbient, 16))
                    return Fail(PassiveActorError::Appearance, "native own appearance does not match local ambient", true);
            }
            for (U i = 0; i < localMaterialCount; ++i)
            {
                BYTE after[0xBC]{};
                if (!Material(localMaterials[i].address, after) || !Equal(after, localMaterials[i].bytes, 0xBC))
                    return Fail(PassiveActorError::Appearance, "native appearance changed a matched local material", true);
            }
            owner->statistics_.materials = ownMaterialCount;
            return true;
        }
        bool Pose(const Transform& transform) noexcept
        {
            if (!wrapperOwned || !Table(wrapper, NodeTable) || !FiniteWords(transform.position, 3) || !Matrix(transform.orientation) ||
                !Equal(transform.scale, initial.scale, 12)) return Fail(PassiveActorError::Transform, "remote PRS or qualified local scale mismatch");
            U flags{};
            if (!Get(wrapper + 0xB0, flags)) return false;
            flags |= 1;
            return Put(wrapper + 0x20, transform.position, 12) && Put(wrapper + 0x30, initial.scale, 12) &&
                Put(wrapper + 0x40, transform.orientation, 36) && Put(wrapper + 0xB0, &flags, 4);
        }
        bool Build(const char* media) noexcept
        {
            if (!Paths(media) || !LocalIdentity(true) || !Tree(local, localRoot, false, nullptr)) return false;
            if (!Load(bodyPath, source)) return false;
            if (!source || !Tree(sourceGraph, source, true, &local)) return Fail(PassiveActorError::Graph, "fresh body graph failed qualification", true);
            U primary{}, skeleton{};
            if (!SelectBody(primary, skeleton)) return Fail(PassiveActorError::Graph, "body ownership cut failed qualification", true);
            sourceQualified = true;
            if (!Retain(source)) return false;
            sourceOwned = true;
            if (!Factory(0x421E20, wrapper)) return false;
            USHORT count{};
            if (!Table(wrapper, NodeTable) || !References(wrapper, count) || count || Value(wrapper + 0x2C) || Value(wrapper + 0x3C))
                return Fail(PassiveActorError::Ownership, "fresh wrapper owner mismatch", true);
            wrapperQualified = true;
            if (!Retain(wrapper)) return false;
            wrapperOwned = true;
            if (!Void1(0x421A60, wrapper, primary) || Value(primary + 0x2C) != wrapper ||
                !Void1(0x421A60, wrapper, skeleton) || Value(skeleton + 0x2C) != wrapper)
                return Fail(PassiveActorError::Ownership, "native body reparent postcondition failed", true);
            if (!Tree(combined, wrapper, true, &local) || combined.count != 91 || combined.skinCount != 6)
                return Fail(PassiveActorError::Graph, "detached body wrapper shape failed", true);
            for (U i = 0; i < combined.count; ++i) if (!Passive(combined.nodes[i]) || combined.nodes[i].scene)
                return Fail(PassiveActorError::Graph, "body wrapper contains forbidden relations", true);
            U head{};
            if (!Named(combined, "Head", head)) return false;
            if (!Load(hairPath, hair)) return false;
            if (!hair || !Tree(hairGraph, hair, true, &local) || hairGraph.count != 21 || hairGraph.skinCount != 1 ||
                hairGraph.nodes[0].parent || hairGraph.nodes[0].references || !Identity(hairGraph.nodes[0].local))
                return Fail(PassiveActorError::Graph, "fresh hair graph failed qualification", true);
            for (U i = 0; i < hairGraph.count; ++i) if (combined.Contains(hairGraph.nodes[i].address))
                return Fail(PassiveActorError::Graph, "fresh hair node aliases the body wrapper", true);
            U hairMaster{};
            if (!Named(hairGraph, "Hair_Master", hairMaster)) return Fail(PassiveActorError::Graph, "Hair_Master identity missing", true);
            U hairNodes{};
            for (U i = 0; i < hairGraph.count; ++i) if (Under(hairGraph, hairGraph.nodes[i].address, hairMaster))
            {
                if (!Passive(hairGraph.nodes[i]) || hairGraph.nodes[i].scene || hairGraph.nodes[i].flags & 0x2000)
                    return Fail(PassiveActorError::Graph, "hair subtree has forbidden relations", true);
                ++hairNodes;
            }
            if (hairNodes != 19) return Fail(PassiveActorError::Graph, "Hair_Master subtree differs from nineteen nodes", true);
            for (U i = 0; i < hairGraph.count; ++i) if (hairGraph.nodes[i].table == RenderTable && !Under(hairGraph, hairGraph.nodes[i].address, hairMaster))
                return Fail(PassiveActorError::Graph, "hair render extends outside Hair_Master", true);
            for (U i = 0; i < hairGraph.skinCount; ++i) for (U b = 0; b < hairGraph.skins[i].count; ++b)
                if (!Under(hairGraph, hairGraph.skins[i].targets[b], hairMaster)) return Fail(PassiveActorError::Graph, "hair palette extends outside Hair_Master", true);
            hairQualified = true;
            if (!Retain(hair)) return false;
            hairOwned = true;
            if (!Void1(0x421A60, head, hairMaster) || Value(hairMaster + 0x2C) != head)
                return Fail(PassiveActorError::Ownership, "native hair attachment postcondition failed", true);
            if (!Tree(combined, wrapper, true, &local) || combined.count != 110 || combined.skinCount != 7)
                return Fail(PassiveActorError::Graph, "combined graph differs from qualified110nodes/7skins", true);
            for (U i = 0; i < combined.count; ++i) if (!Passive(combined.nodes[i]) || combined.nodes[i].scene)
                return Fail(PassiveActorError::Graph, "combined wrapper contains forbidden relations", true);
            if (!DeleteRoot(hair, hairOwned) || !Factory(0x5A3620, actor) || !Table(actor, 0x703F80)) return false;
            if (!Void1(0x5A35C0, actor, wrapper)) return false;
            U begin{}, end{};
            if (!Get(actor + 0x40, begin) || !Get(actor + 0x44, end) || !Pointer(begin) || end <= begin || (end - begin) % 4 || (end - begin) / 4 > 256)
                return Fail(PassiveActorError::Animation, "native Actor controller vector is invalid", true);
            if (!DeleteRoot(source, sourceOwned)) return false;
            const char* names[3] = {"blidsh.san", "blwa.san", "bljusm.san"};
            for (U i = 0; i < 3; ++i) if (!Animation(names[i], clips[i]) || !Clip(clips[i])) return false;
            if (!Start(0) || !Appearance()) return false;
            Transform first = initial; first.position[0] += 180;
            if (!Pose(first) || !LocalIdentity(false) || !Void1(0x421A60, parent, wrapper)) return false;
            attached = true;
            if (Value(wrapper + 0x2C) != parent || Value(wrapper + 0x3C) != scene || !References(wrapper, count) || count != 2)
                return Fail(PassiveActorError::Ownership, "scene attachment did not establish exactly parent+adapter ownership", true);
            created = true;
            ++owner->statistics_.created; owner->statistics_.nodes = combined.count; owner->statistics_.skins = combined.skinCount;
            return true;
        }
        bool Cleanup() noexcept
        {
            if ((source && !sourceQualified) || (hair && !hairQualified) || (wrapper && !wrapperQualified))
                return Fail(PassiveActorError::Ownership, "partial graph ownership is unqualified; terminate owned game", true);
            if (actor)
            {
                if (!Table(actor, 0x703F80) || !Void0(0x5A21C0, actor) || !Void1(0x5A35A0, actor, 1)) return false;
                actor = 0;
                for (U& clip : clips) clip = 0;
            }
            if (wrapper)
            {
                USHORT count{}; U actualParent{};
                if (!wrapperOwned || !Table(wrapper, NodeTable) || !References(wrapper, count) || !Get(wrapper + 0x2C, actualParent))
                    return Fail(PassiveActorError::Ownership, "wrapper cleanup ownership unavailable", true);
                if (actualParent)
                {
                    if (actualParent != parent || count != 2) return Fail(PassiveActorError::Ownership, "attached wrapper owner count changed", true);
                    if (!Function(0x4217E0) || !SetReferences(wrapper, 1)) return false;
                    wrapperOwned = false;
                    if (!Void0(0x4217E0, wrapper)) return false;
                    wrapper = 0; // Native final-parent release deletes the tree.
                }
                else if (!DeleteRoot(wrapper, wrapperOwned)) return false;
            }
            if (!DeleteRoot(hair, hairOwned) || !DeleteRoot(source, sourceOwned)) return false;
            if (created) ++owner->statistics_.destroyed;
            created = false; attached = false;
            return true;
        }
    };

    bool PassiveActor::Fail(PassiveActorError error, const char* text, bool poison) noexcept
    { error_ = error; errorText_ = text; poisoned_ = poisoned_ || poison; return false; }
    bool PassiveActor::Phase() noexcept
    {
        if (poisoned_) return false;
        if (!context_.mainThreadId || !context_.gate || !context_.safeGamePhase || GetCurrentThreadId() != context_.mainThreadId || !context_.safeGamePhase())
            return Fail(PassiveActorError::Context, "passive actor requires the original main-thread world/pre-unload phase");
        return true;
    }
    bool PassiveActor::Configure(const PassiveActorContext& context) noexcept
    {
        if (state_ || poisoned_ || !context.mainThreadId || !context.gate || !context.safeGamePhase)
            return Fail(PassiveActorError::Context, "invalid or repeated live passive-actor configuration");
        context_ = context; error_ = PassiveActorError::None; errorText_ = "none";
        return true;
    }
    bool PassiveActor::Created() const noexcept { return state_ && state_->created && !poisoned_; }
    bool PassiveActor::Create(const char* mediaDirectory) noexcept
    {
        if (!Phase()) return false;
        if (state_) return state_->created && !state_->busy ? true : Fail(PassiveActorError::Context, "reentrant/incomplete create", true);
        void* storage = HeapAlloc(GetProcessHeap(), 0, sizeof(State));
        if (!storage) return Fail(PassiveActorError::Capacity, "bounded passive-actor scratch allocation failed");
        state_ = ::new (storage) State;
        state_->owner = this; state_->busy = true; error_ = PassiveActorError::None; errorText_ = "none";
        const bool result = state_->Build(mediaDirectory);
        state_->busy = false;
        if (!result && !poisoned_)
        {
            const PassiveActorError originalError = error_; const char* originalText = errorText_;
            if (state_->Cleanup()) { state_->~State(); HeapFree(GetProcessHeap(), 0, state_); state_ = nullptr; error_ = originalError; errorText_ = originalText; }
            else poisoned_ = true;
        }
        return result;
    }
    bool PassiveActor::ApplyRemote(const Transform& transform, PassiveAnimation animation) noexcept
    {
        if (!Phase()) return false;
        if (!state_ || !state_->created || state_->busy) return Fail(PassiveActorError::Context, "remote apply requires one completed nonreentrant actor", state_ && state_->busy);
        const U index = static_cast<U>(animation);
        if (index > 2) return Fail(PassiveActorError::Animation, "invalid remote animation enum");
        error_ = PassiveActorError::None; errorText_ = "none";
        state_->busy = true;
        bool result = state_->LocalIdentity(false) && state_->Pose(transform);
        if (result && index != state_->activeAnimation) result = state_->Void0(0x5A21C0, state_->actor) && state_->Start(index);
        state_->busy = false;
        return result;
    }
    bool PassiveActor::ValidateLiveOwnership() noexcept
    {
        if (!Phase()) return false;
        if (!state_ || !state_->created || state_->busy)
            return Fail(PassiveActorError::Context, "live ownership requires a completed passive actor", true);
        U parent{}, scene{}; USHORT references{};
        if (!state_->LocalIdentity(false, false) || !state_->Table(state_->actor, 0x703F80) ||
            !state_->Table(state_->wrapper, NodeTable) || !state_->Get(state_->wrapper + 0x2C, parent) || parent != state_->parent ||
            !state_->Get(state_->wrapper + 0x3C, scene) || scene != state_->scene ||
            !state_->References(state_->wrapper, references) || references != 2)
            return Fail(PassiveActorError::Ownership, "live scene/actor/parent ownership changed", true);
        for (const U clip : state_->clips) if (!state_->Clip(clip)) return false;
        return true;
    }
    bool PassiveActor::DestroyBeforeUnload() noexcept
    {
        if (!Phase()) return false;
        if (!state_) return true;
        if (state_->busy) return Fail(PassiveActorError::Context, "unload entered during an unfinished passive transaction", true);
        state_->busy = true;
        if (!state_->Cleanup()) { state_->busy = false; poisoned_ = true; return false; }
        state_->~State(); HeapFree(GetProcessHeap(), 0, state_); state_ = nullptr;
        return true;
    }
}
