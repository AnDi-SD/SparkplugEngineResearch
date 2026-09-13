// Own CPU-only contract fixture for the real bridge skinning serializer.
// No game, COM device, renderer, or GPU calls. Link with util_remixapi.cpp.
#include <windows.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
#include "util_remixapi.h"

// Allocation accounting is armed only around the real owning deserialize.
// No bookkeeping allocation is made by these hooks. The allocation header
// remembers ownership so deletes after the fault also discharge the ledger.
namespace allocation_fault {
thread_local bool armed = false;
thread_local size_t attempts = 0, failAt = SIZE_MAX;
thread_local long live = 0;
struct alignas(std::max_align_t) Header { bool tracked; };
void* Allocate(size_t size) {
  if (armed && attempts++ == failAt) throw std::bad_alloc();
  if (size > SIZE_MAX - sizeof(Header)) throw std::bad_alloc();
  auto* header = static_cast<Header*>(std::malloc(sizeof(Header) + (size ? size : 1)));
  if (!header) throw std::bad_alloc();
  header->tracked = armed;
  if (armed) ++live;
  return header + 1;
}
void Free(void* value) noexcept {
  if (!value) return;
  auto* header = static_cast<Header*>(value) - 1;
  if (header->tracked) --live;
  std::free(header);
}
}
void* operator new(size_t size) { return allocation_fault::Allocate(size); }
void* operator new[](size_t size) { return allocation_fault::Allocate(size); }
void operator delete(void* value) noexcept { allocation_fault::Free(value); }
void operator delete[](void* value) noexcept { allocation_fault::Free(value); }
void operator delete(void* value, size_t) noexcept { allocation_fault::Free(value); }
void operator delete[](void* value, size_t) noexcept { allocation_fault::Free(value); }

namespace {
namespace wire = remixapi::util;
using Bytes = std::vector<uint8_t>;
unsigned checks = 0, errors = 0;
const ULONGLONG started = GetTickCount64();

void Check(bool value, const char* name) {
  ++checks;
  if (!value) { ++errors; std::fprintf(stderr, "FAIL %s\n", name); }
  if (GetTickCount64() - started > 25000) throw std::runtime_error("fixture deadline");
}
void Require(bool value, const char* name) {
  Check(value, name);
  if (!value) throw std::runtime_error(name);
}

// The last byte of the requested storage is immediately before PAGE_NOACCESS.
// A separate readable prefix catches underruns; it never cushions an overrun.
class Guarded {
  uint8_t* allocation_ = nullptr;
  uint8_t* committed_ = nullptr;
  size_t committedBytes_ = 0;
public:
  uint8_t* data = nullptr;
  size_t size = 0;
  explicit Guarded(size_t bytes) : size(bytes) {
    SYSTEM_INFO system{}; GetSystemInfo(&system);
    const size_t page = system.dwPageSize;
    committedBytes_ = ((bytes + 32 + page - 1) / page) * page;
    allocation_ = static_cast<uint8_t*>(VirtualAlloc(nullptr, committedBytes_ + 2 * page,
      MEM_RESERVE, PAGE_NOACCESS));
    if (!allocation_) throw std::bad_alloc();
    committed_ = static_cast<uint8_t*>(VirtualAlloc(allocation_ + page, committedBytes_,
      MEM_COMMIT, PAGE_READWRITE));
    if (!committed_) { VirtualFree(allocation_, 0, MEM_RELEASE); throw std::bad_alloc(); }
    data = committed_ + committedBytes_ - bytes;
    std::memset(data - 32, 0xa7, 32);
    if (bytes) std::memset(data, 0xcc, bytes);
  }
  Guarded(const Guarded&) = delete;
  Guarded& operator=(const Guarded&) = delete;
  ~Guarded() { if (allocation_) VirtualFree(allocation_, 0, MEM_RELEASE); }
  void ReadOnly() {
    DWORD old = 0;
    Require(VirtualProtect(committed_, committedBytes_, PAGE_READONLY, &old) != FALSE,
      "guard source readonly");
  }
  bool PrefixIntact() const {
    for (unsigned i = 0; i < 32; ++i) if (data[-32 + int(i)] != 0xa7) return false;
    return true;
  }
};

template<class T> void Append(Bytes& bytes, T value) {
  static_assert(std::is_integral_v<T> || std::is_enum_v<T>);
  const auto begin = reinterpret_cast<const uint8_t*>(&value);
  bytes.insert(bytes.end(), begin, begin + sizeof(value));
}
void AppendRaw(Bytes& bytes, const void* source, size_t size) {
  if (!size) return;
  const auto begin = static_cast<const uint8_t*>(source);
  bytes.insert(bytes.end(), begin, begin + size);
}
template<class T> void Put(Bytes& bytes, size_t offset, T value) {
  Require(offset <= bytes.size() && sizeof(value) <= bytes.size() - offset, "mutation inside packet");
  std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

void Vertex(remixapi_HardcodedVertex& vertex, unsigned seed) {
  std::memset(&vertex, 0x5a, sizeof(vertex)); // ABI padding must never enter the wire.
  const uint32_t bits[8] = {0x80000000u, 0x3f800000u + seed, 0xc0000000u + seed,
    0x00000000u, 0x3f000000u + seed, 0xbf000000u, 0x3e800000u, 0x3f400000u};
  std::memcpy(vertex.position, bits, 12);
  std::memcpy(vertex.normal, bits + 3, 12);
  std::memcpy(vertex.texcoord, bits + 6, 8);
  vertex.color = 0x80402010u + seed;
}

struct SurfaceData {
  std::array<remixapi_HardcodedVertex, 3> vertices{};
  std::array<uint32_t, 3> indices{{2, 0, 1}};
  std::vector<float> weights;
  std::vector<uint32_t> blends;
};
struct MeshData {
  std::vector<SurfaceData> storage;
  std::vector<remixapi_MeshInfoSurfaceTriangles> surfaces;
  remixapi_MeshInfo info{};
  explicit MeshData(const std::vector<unsigned>& bones) : storage(bones.size()), surfaces(bones.size()) {
    info.sType = REMIXAPI_STRUCT_TYPE_MESH_INFO;
    info.hash = 0x0123456789abcdefull;
    info.surfaces_count = static_cast<uint32_t>(surfaces.size());
    info.surfaces_values = surfaces.data();
    for (size_t i = 0; i < bones.size(); ++i) {
      auto& data = storage[i]; auto& surface = surfaces[i];
      for (unsigned v = 0; v < 3; ++v) Vertex(data.vertices[v], static_cast<unsigned>(i * 11 + v));
      surface.vertices_count = 3; surface.vertices_values = data.vertices.data();
      surface.indices_count = 3; surface.indices_values = data.indices.data();
      surface.material = reinterpret_cast<remixapi_MaterialHandle>(uintptr_t(0x12340001u + i));
      if (!bones[i]) continue;
      data.weights.resize(3 * bones[i]); data.blends.resize(data.weights.size());
      for (size_t j = 0; j < data.weights.size(); ++j) {
        uint32_t bits = j == 0 ? 0x80000000u : 0x3e000000u + static_cast<uint32_t>(j);
        std::memcpy(&data.weights[j], &bits, 4); data.blends[j] = static_cast<uint32_t>((j * 17) % 256);
      }
      surface.skinning_hasvalue = 1;
      surface.skinning_value.bonesPerVertex = bones[i];
      surface.skinning_value.blendWeights_count = static_cast<uint32_t>(data.weights.size());
      surface.skinning_value.blendWeights_values = data.weights.data();
      surface.skinning_value.blendIndices_count = static_cast<uint32_t>(data.blends.size());
      surface.skinning_value.blendIndices_values = data.blends.data();
    }
  }
};

// Explicit test oracle: field order/width from the bridge wire contract, not
// native sizeof(MeshInfo/Surface/Vertex), and not the production size helpers.
Bytes ExpectedMesh(const remixapi_MeshInfo& info) {
  Bytes bytes;
  Append(bytes, uint32_t(0)); Append(bytes, uint32_t(info.sType));
  Append(bytes, info.hash); Append(bytes, info.surfaces_count);
  for (uint32_t i = 0; i < info.surfaces_count; ++i) {
    const auto& s = info.surfaces_values[i];
    Append(bytes, s.vertices_count);
    for (uint64_t v = 0; v < s.vertices_count; ++v) {
      AppendRaw(bytes, s.vertices_values[v].position, 12);
      AppendRaw(bytes, s.vertices_values[v].normal, 12);
      AppendRaw(bytes, s.vertices_values[v].texcoord, 8);
      Append(bytes, s.vertices_values[v].color);
    }
    Append(bytes, s.indices_count); AppendRaw(bytes, s.indices_values, size_t(s.indices_count) * 4);
    Append(bytes, s.skinning_hasvalue);
    if (s.skinning_hasvalue) {
      const auto& skin = s.skinning_value;
      Append(bytes, skin.bonesPerVertex); Append(bytes, skin.blendWeights_count);
      AppendRaw(bytes, skin.blendWeights_values, size_t(skin.blendWeights_count) * 4);
      Append(bytes, skin.blendIndices_count);
      AppendRaw(bytes, skin.blendIndices_values, size_t(skin.blendIndices_count) * 4);
    }
    Append(bytes, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(s.material)));
  }
  Put(bytes, 0, static_cast<uint32_t>(bytes.size()));
  return bytes;
}

void CheckDecodedMesh(void* packet, uint32_t size, const remixapi_MeshInfo& expected) {
  Require(wire::validateMeshInfoPayload(packet, size), "mesh payload valid before deserialize");
  wire::serialize::MeshInfo decoded(packet); decoded.deserialize();
  Check(decoded.sType == expected.sType && decoded.hash == expected.hash, "mesh header roundtrip");
  Require(decoded.surfaces_count == expected.surfaces_count, "surface count roundtrip");
  for (uint32_t i = 0; i < expected.surfaces_count; ++i) {
    const auto& a = decoded.surfaces_values[i]; const auto& b = expected.surfaces_values[i];
    Require(a.vertices_count == b.vertices_count && a.indices_count == b.indices_count, "geometry counts roundtrip");
    for (uint64_t v = 0; v < b.vertices_count; ++v) {
      Check(std::memcmp(a.vertices_values[v].position, b.vertices_values[v].position, 12) == 0 &&
        std::memcmp(a.vertices_values[v].normal, b.vertices_values[v].normal, 12) == 0 &&
        std::memcmp(a.vertices_values[v].texcoord, b.vertices_values[v].texcoord, 8) == 0 &&
        a.vertices_values[v].color == b.vertices_values[v].color, "vertex field bits roundtrip");
    }
    Check(!b.indices_count || std::memcmp(a.indices_values, b.indices_values, size_t(b.indices_count) * 4) == 0,
      "triangle index bits roundtrip");
    Check(a.material == b.material, "material handle remains 32 bit wire value");
    Require(a.skinning_hasvalue == b.skinning_hasvalue, "skinning presence roundtrip");
    if (b.skinning_hasvalue) {
      const auto& x = a.skinning_value; const auto& y = b.skinning_value;
      Require(x.bonesPerVertex == y.bonesPerVertex && x.blendWeights_count == y.blendWeights_count &&
        x.blendIndices_count == y.blendIndices_count, "skinning counts roundtrip");
      Check(std::memcmp(x.blendWeights_values, y.blendWeights_values, size_t(y.blendWeights_count) * 4) == 0,
        "weight bits roundtrip");
      Check(std::memcmp(x.blendIndices_values, y.blendIndices_values, size_t(y.blendIndices_count) * 4) == 0,
        "bone index bits roundtrip");
    }
  }
}

Bytes MeshRoundTrip(const remixapi_MeshInfo& info) {
  const auto expected = ExpectedMesh(info);
  uint32_t preflight = 0;
  Require(wire::isSupportedMeshInfo(&info, &preflight), "mesh preflight accepts supported input");
  wire::serialize::MeshInfo source(info);
  Check(preflight == expected.size() && source.size() == expected.size(), "mesh exact wire size");
  Require(source.size() <= 1024 * 1024, "fixture serialization bounded");
  Guarded output(source.size()); source.serialize(output.data);
  Check(output.PrefixIntact(), "mesh output prefix intact");
  Check(source.size() == expected.size() && std::memcmp(output.data, expected.data(), expected.size()) == 0,
    "mesh wire bytes including rigid regression");
  CheckDecodedMesh(output.data, source.size(), info);
  Check(output.PrefixIntact(), "mesh deserialize did not write before input");
  return Bytes(output.data, output.data + source.size());
}

struct BoneData {
  std::vector<remixapi_Transform> transforms;
  remixapi_InstanceInfoBoneTransformsEXT info{};
  explicit BoneData(unsigned count) : transforms(count) {
    static_assert(sizeof(remixapi_Transform) == 48);
    info.sType = REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_BONE_TRANSFORMS_EXT;
    info.boneTransforms_count = count; info.boneTransforms_values = count ? transforms.data() : nullptr;
    const uint32_t words[12] = {0, 0x80000000u, 0x3f800000u, 0xbf800000u, 0x3e800001u,
      0x7f7fffffu, 0x00800000u, 1, 0x7fc12345u, 0xff800000u, 0x7f800000u, 0xc0200000u};
    for (unsigned i = 0; i < count; ++i) {
      std::memcpy(&transforms[i], words, sizeof(words));
      const uint32_t unique = 0x3f000000u + i; std::memcpy(&transforms[i], &unique, 4);
    }
  }
};
void CheckDecodedBones(void* packet, uint32_t size, const BoneData& expected) {
  Require(wire::validateBoneTransformsPayload(packet, size), "palette payload valid before deserialize");
  wire::serialize::InstanceInfoTransforms decoded(packet); decoded.deserialize();
  Check(decoded.sType == expected.info.sType && decoded.pNext == nullptr, "palette header roundtrip");
  Require(decoded.boneTransforms_count == expected.info.boneTransforms_count, "palette count roundtrip");
  Check(!decoded.boneTransforms_count || std::memcmp(decoded.boneTransforms_values,
    expected.transforms.data(), expected.transforms.size() * 48) == 0, "all matrix bits roundtrip including signed zero and nonfinite");
}
Bytes BoneRoundTrip(const BoneData& bones) {
  Require(wire::isSupportedBoneTransforms(&bones.info), "palette preflight accepts supported input");
  wire::serialize::InstanceInfoTransforms source(bones.info);
  const uint32_t expectedSize = 12 + bones.info.boneTransforms_count * 48;
  Require(source.size() == expectedSize, "palette exact wire size");
  Guarded output(source.size()); source.serialize(output.data);
  Bytes expected;
  Append(expected, expectedSize); Append(expected, uint32_t(bones.info.sType));
  Append(expected, bones.info.boneTransforms_count);
  AppendRaw(expected, bones.transforms.data(), bones.transforms.size() * 48);
  Check(std::memcmp(output.data, expected.data(), expected.size()) == 0, "palette exact wire bytes");
  CheckDecodedBones(output.data, source.size(), bones);
  Check(output.PrefixIntact(), "palette output prefix intact");
  return Bytes(output.data, output.data + source.size());
}

void GuardedSourceCase() {
  MeshData mesh({2}); auto& s = mesh.surfaces[0];
  Guarded vertices(3 * sizeof(remixapi_HardcodedVertex)), indices(12), weights(24), blends(24);
  std::memcpy(vertices.data, s.vertices_values, vertices.size);
  std::memcpy(indices.data, s.indices_values, indices.size);
  std::memcpy(weights.data, s.skinning_value.blendWeights_values, weights.size);
  std::memcpy(blends.data, s.skinning_value.blendIndices_values, blends.size);
  vertices.ReadOnly(); indices.ReadOnly(); weights.ReadOnly(); blends.ReadOnly();
  s.vertices_values = reinterpret_cast<const remixapi_HardcodedVertex*>(vertices.data);
  s.indices_values = reinterpret_cast<const uint32_t*>(indices.data);
  s.skinning_value.blendWeights_values = reinterpret_cast<const float*>(weights.data);
  s.skinning_value.blendIndices_values = reinterpret_cast<const uint32_t*>(blends.data);
  const auto bytes = MeshRoundTrip(mesh.info);
  Check(bytes.size() == 224, "V3 B2 count6 reads 24 bytes per skinning array, not 144");
  Check(vertices.PrefixIntact() && indices.PrefixIntact() && weights.PrefixIntact() && blends.PrefixIntact(),
    "guarded source arrays remain unchanged");
  BoneData bones(256); Guarded matrices(bones.transforms.size() * 48);
  std::memcpy(matrices.data, bones.transforms.data(), matrices.size); matrices.ReadOnly();
  bones.info.boneTransforms_values = reinterpret_cast<const remixapi_Transform*>(matrices.data);
  BoneRoundTrip(bones);
}

void PreflightCases() {
  MeshData mesh({2}); const auto original = mesh.info; const auto surface = mesh.surfaces[0];
  auto& s = mesh.surfaces[0];
  Check(!wire::isSupportedMeshInfo(nullptr), "null mesh rejected");
  mesh.info.sType = REMIXAPI_STRUCT_TYPE_NONE;
  Check(!wire::isSupportedMeshInfo(&mesh.info), "wrong mesh type rejected"); mesh.info = original;
  mesh.info.pNext = reinterpret_cast<void*>(uintptr_t(1));
  Check(!wire::isSupportedMeshInfo(&mesh.info), "mesh extension rejected"); mesh.info = original;
  mesh.info.surfaces_values = nullptr;
  Check(!wire::isSupportedMeshInfo(&mesh.info), "nonnull surface count requires pointer"); mesh.info = original;
  mesh.info.surfaces_count = UINT32_MAX;
  Check(!wire::isSupportedMeshInfo(&mesh.info), "surface count cannot fit uint32 envelope"); mesh.info = original;
  s.vertices_values = nullptr;
  Check(!wire::isSupportedMeshInfo(&mesh.info), "nonnull vertex count requires pointer"); s = surface;
  s.indices_values = nullptr;
  Check(!wire::isSupportedMeshInfo(&mesh.info), "nonnull index count requires pointer"); s = surface;
  s.skinning_value.bonesPerVertex = 0;
  Check(!wire::isSupportedMeshInfo(&mesh.info), "present skinning requires nonzero B"); s = surface;
  for (uint32_t count : {0u, 1u, 5u, 7u, UINT32_MAX}) {
    s.skinning_value.blendWeights_count = count;
    Check(!wire::isSupportedMeshInfo(&mesh.info), "weight count must equal V times B"); s = surface;
    s.skinning_value.blendIndices_count = count;
    Check(!wire::isSupportedMeshInfo(&mesh.info), "bone index count must equal V times B"); s = surface;
  }
  s.skinning_value.blendWeights_values = nullptr;
  Check(!wire::isSupportedMeshInfo(&mesh.info), "weights require pointer"); s = surface;
  s.skinning_value.blendIndices_values = nullptr;
  Check(!wire::isSupportedMeshInfo(&mesh.info), "bone indices require pointer"); s = surface;
  s.vertices_count = UINT64_MAX;
  Check(!wire::isSupportedMeshInfo(&mesh.info), "vertex multiplication overflow rejected"); s = surface;
  s.indices_count = UINT64_MAX;
  Check(!wire::isSupportedMeshInfo(&mesh.info), "index multiplication overflow rejected"); s = surface;
  s.skinning_value.bonesPerVertex = UINT32_MAX;
  Check(!wire::isSupportedMeshInfo(&mesh.info), "V times B cannot fit skinning count"); s = surface;
  s.skinning_hasvalue = 0; s.vertices_count = uint64_t(UINT32_MAX) / 36 + 1;
  Check(!wire::isSupportedMeshInfo(&mesh.info), "rigid vertex bytes overflow wire envelope"); s = surface;
  s.skinning_hasvalue = 0; s.indices_count = uint64_t(UINT32_MAX) / 4 + 1;
  Check(!wire::isSupportedMeshInfo(&mesh.info), "rigid index bytes overflow wire envelope"); s = surface;
  // Each section fits uint32 separately, but their sum plus headers does not.
  s.skinning_hasvalue = 0; s.vertices_count = uint64_t(UINT32_MAX) / 72;
  s.indices_count = uint64_t(UINT32_MAX) / 8;
  Check(!wire::isSupportedMeshInfo(&mesh.info), "aggregate and envelope overhead overflow rejected"); s = surface;
  Check(wire::isSupportedMeshInfo(&mesh.info), "valid mesh accepted after rejected inputs");
  BoneData bones(2); const auto palette = bones.info;
  Check(!wire::isSupportedBoneTransforms(nullptr), "null palette rejected");
  bones.info.sType = REMIXAPI_STRUCT_TYPE_NONE;
  Check(!wire::isSupportedBoneTransforms(&bones.info), "wrong palette type rejected"); bones.info = palette;
  bones.info.boneTransforms_values = nullptr;
  Check(!wire::isSupportedBoneTransforms(&bones.info), "palette count requires pointer"); bones.info = palette;
  for (uint32_t count : {257u, UINT32_MAX}) {
    bones.info.boneTransforms_count = count;
    Check(!wire::isSupportedBoneTransforms(&bones.info), "palette maximum is 256"); bones.info = palette;
  }
  bones.info.pNext = reinterpret_cast<void*>(uintptr_t(1));
  Check(wire::isSupportedBoneTransforms(&bones.info), "palette pNext remains separately transported chain");
  bones.info = palette; Check(wire::isSupportedBoneTransforms(&bones.info), "valid palette accepted after rejected inputs");
}

using Validator = bool (*)(const void*, uint32_t);
void RejectGuarded(const Bytes& packet, Validator validate, const char* label) {
  Guarded input(packet.size());
  if (!packet.empty()) std::memcpy(input.data, packet.data(), packet.size());
  input.ReadOnly();
  Check(!validate(input.data, static_cast<uint32_t>(packet.size())), label);
  Check(input.PrefixIntact(), "rejected packet prefix intact");
}
void EnvelopeCases(const Bytes& valid, Validator validate, bool everyPrefix) {
  Check(!validate(nullptr, static_cast<uint32_t>(valid.size())), "null wire buffer rejected");
  for (size_t size = 0; size < valid.size(); ++size) {
    if (!everyPrefix && size > 16 && size + 3 < valid.size() && size % 48 != 11 && size % 48 != 12) continue;
    Bytes prefix(valid.begin(), valid.begin() + size);
    RejectGuarded(prefix, validate, "truncated original envelope rejected without overread");
    if (size >= 4) {
      Put(prefix, 0, static_cast<uint32_t>(size));
      RejectGuarded(prefix, validate, "truncated matching envelope rejected without overread");
    }
  }
  auto changed = valid; changed.push_back(0x9b);
  RejectGuarded(changed, validate, "trailing byte rejected");
  Put(changed, 0, static_cast<uint32_t>(changed.size()));
  RejectGuarded(changed, validate, "trailing byte inside declared envelope rejected");
  for (uint32_t declared : {0u, 3u, uint32_t(valid.size() - 1), uint32_t(valid.size() + 1), UINT32_MAX}) {
    changed = valid; Put(changed, 0, declared);
    RejectGuarded(changed, validate, "incorrect declared size rejected");
  }
  changed = valid; Put(changed, 4, uint32_t(REMIXAPI_STRUCT_TYPE_NONE));
  RejectGuarded(changed, validate, "incorrect wire type rejected");
  Check(validate(valid.data(), static_cast<uint32_t>(valid.size())), "valid packet accepted after malformed envelopes");
}
void MalformedCases(const Bytes& skin, const Bytes& palette) {
  EnvelopeCases(skin, wire::validateMeshInfoPayload, true);
  EnvelopeCases(palette, wire::validateBoneTransformsPayload, false);
  // Offsets are fixed contract values for V3/B2/count6, with 36-byte vertices.
  for (size_t offset : {size_t(16), size_t(160), size_t(164), size_t(192)}) {
    for (uint32_t value : {0u, 1u, 7u, UINT32_MAX}) {
      auto changed = skin; Put(changed, offset, value);
      if ((offset == 16 && value == 1) || (offset == 160 && value == 2)) continue;
      RejectGuarded(changed, wire::validateMeshInfoPayload, "malformed mesh counts rejected");
    }
  }
  for (size_t offset : {size_t(20), size_t(136)}) {
    auto changed = skin; Put(changed, offset, UINT64_MAX);
    RejectGuarded(changed, wire::validateMeshInfoPayload, "wire 64-bit count overflow rejected");
  }
  for (uint32_t count : {0u, 1u, 257u, UINT32_MAX}) {
    auto changed = palette; Put(changed, 8, count);
    RejectGuarded(changed, wire::validateBoneTransformsPayload, "wire palette count/envelope mismatch rejected");
  }
  MeshData mesh({2}); BoneData bones(256);
  Guarded meshInput(skin.size()), boneInput(palette.size());
  std::memcpy(meshInput.data, skin.data(), skin.size()); std::memcpy(boneInput.data, palette.data(), palette.size());
  CheckDecodedMesh(meshInput.data, static_cast<uint32_t>(skin.size()), mesh.info);
  CheckDecodedBones(boneInput.data, static_cast<uint32_t>(palette.size()), bones);
}

template<class Decoder> void AllocationCases(const Bytes& bytes) {
  Guarded input(bytes.size()); std::memcpy(input.data, bytes.data(), bytes.size()); input.ReadOnly();
  bool reachedSuccess = false;
  for (size_t fail = 0; fail < 64; ++fail) {
    Require(allocation_fault::live == 0, "empty deserialize allocation ledger before attempt");
    allocation_fault::attempts = 0; allocation_fault::failAt = fail;
    allocation_fault::armed = true;
    bool failed = false;
    try { Decoder decoded(input.data); decoded.deserialize(); }
    catch (const std::bad_alloc&) { failed = true; }
    catch (...) { allocation_fault::armed = false; throw; }
    allocation_fault::armed = false;
    Check(allocation_fault::live == 0, "all owning deserialize arrays freed after success or allocation failure");
    Check(input.PrefixIntact(), "allocation unwind leaves wire buffer unchanged");
    if (!failed) {
      Check(allocation_fault::attempts == fail, "each allocation site faulted once before successful decode");
      reachedSuccess = true; break;
    }
    Check(allocation_fault::attempts == fail + 1, "requested allocation fault reached");
  }
  Check(reachedSuccess, "bounded allocation fault sweep reaches valid subsequent packet");
}

struct Record { uint32_t kind, parameter; Bytes bytes; };
std::vector<Record> LocalCases() {
  Check(sizeof(remixapi_Bool) == 4 && sizeof(remixapi_HardcodedVertex) == 64, "SDK ABI widths");
  GuardedSourceCase(); PreflightCases();
  std::vector<Record> records;
  for (unsigned bones : {0u, 1u, 2u, 3u, 4u, 5u}) {
    MeshData mesh({bones}); records.push_back({1, bones, MeshRoundTrip(mesh.info)});
  }
  MeshData multi({0, 1, 2, 3, 4, 5}); records.push_back({1, 100, MeshRoundTrip(multi.info)});
  for (unsigned count : {0u, 1u, 2u, 256u}) {
    BoneData bones(count); records.push_back({2, count, BoneRoundTrip(bones)});
  }
  Check(records[0].bytes.size() == 164, "rigid byte length unchanged");
  MalformedCases(records[2].bytes, records.back().bytes);
  Require(wire::validateMeshInfoPayload(records[6].bytes.data(), static_cast<uint32_t>(records[6].bytes.size())),
    "multisurface validated before allocation fault sweep");
  AllocationCases<wire::serialize::MeshInfo>(records[6].bytes);
  AllocationCases<wire::serialize::InstanceInfoTransforms>(records.back().bytes);
  return records;
}

Bytes Bundle(const std::vector<Record>& records) {
  Bytes file; Append(file, uint32_t(0x314e4b53)); Append(file, uint32_t(1));
  Append(file, static_cast<uint32_t>(records.size()));
  for (const auto& record : records) {
    Append(file, record.kind); Append(file, record.parameter);
    Append(file, static_cast<uint32_t>(record.bytes.size())); AppendRaw(file, record.bytes.data(), record.bytes.size());
  }
  return file;
}
void WriteBundle(const wchar_t* path, const std::vector<Record>& records) {
  const auto bytes = Bundle(records);
  HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
  Require(file != INVALID_HANDLE_VALUE, "create fresh cross-architecture bundle");
  DWORD written = 0; const bool result = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) != FALSE;
  const bool closed = CloseHandle(file) != FALSE;
  Require(result && written == bytes.size() && closed, "write complete cross-architecture bundle");
}
void ReadBundle(const wchar_t* path, const std::vector<Record>& records) {
  HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  Require(file != INVALID_HANDLE_VALUE, "open cross-architecture bundle");
  LARGE_INTEGER length{};
  if (!GetFileSizeEx(file, &length) || length.QuadPart < 12 || length.QuadPart > 1024 * 1024) {
    CloseHandle(file); Require(false, "bundle length bounded");
  }
  Bytes bytes(static_cast<size_t>(length.QuadPart)); DWORD read = 0;
  const bool result = ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr) != FALSE;
  const bool closed = CloseHandle(file) != FALSE;
  Require(result && read == bytes.size() && closed, "read complete cross-architecture bundle");
  const auto expected = Bundle(records);
  Require(bytes == expected, "x86/x64 complete wire bytes are identical");
  size_t offset = 12;
  for (const auto& record : records) {
    offset += 12; Guarded input(record.bytes.size());
    std::memcpy(input.data, bytes.data() + offset, record.bytes.size()); input.ReadOnly();
    if (record.kind == 1) {
      MeshData mesh(record.parameter == 100 ? std::vector<unsigned>{0, 1, 2, 3, 4, 5} : std::vector<unsigned>{record.parameter});
      CheckDecodedMesh(input.data, static_cast<uint32_t>(record.bytes.size()), mesh.info);
    } else {
      BoneData bones(record.parameter); CheckDecodedBones(input.data, static_cast<uint32_t>(record.bytes.size()), bones);
    }
    Check(input.PrefixIntact(), "foreign packet input remains unchanged"); offset += record.bytes.size();
  }
  Check(offset == bytes.size(), "all foreign packets consumed exactly");
}
}

int wmain(int argc, wchar_t** argv) {
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
  size_t packets = 0, bundleBytes = 0;
  try {
    Require(argc == 1 || (argc == 3 && (!std::wcscmp(argv[1], L"--write") || !std::wcscmp(argv[1], L"--read"))),
      "usage: test_skinning [--write fresh-file | --read peer-file]");
    auto records = LocalCases(); packets = records.size(); bundleBytes = Bundle(records).size();
    if (argc == 3 && !std::wcscmp(argv[1], L"--write")) WriteBundle(argv[2], records);
    if (argc == 3 && !std::wcscmp(argv[1], L"--read")) ReadBundle(argv[2], records);
  } catch (const std::exception& error) {
    ++errors; std::fprintf(stderr, "fixture exception: %s\n", error.what());
  }
  std::printf("{\"checks\":%u,\"pass\":%s,\"errors\":%u,\"pointerBits\":%u,\"packets\":%llu,\"bundleBytes\":%llu,\"cpuOnly\":true}\n",
    checks, errors ? "false" : "true", errors, unsigned(sizeof(void*) * 8),
    static_cast<unsigned long long>(packets), static_cast<unsigned long long>(bundleBytes));
  return errors ? 1 : 0;
}
