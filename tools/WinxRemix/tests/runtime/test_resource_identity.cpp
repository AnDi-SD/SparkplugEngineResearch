// Own byte-protocol fixture. No game, SDK, COM, file publication or GPU.
#if defined(_MSC_VER)
#pragma warning(push)
// DoubleVertex deliberately instantiates the existing implicit double->float
// fallback. Scope C4244 to its template definitions only; the test body and
// production translation units keep their normal /W4 /WX warning policy.
#pragma warning(disable:4244)
#endif
#include "../../src/runtime/winx_resource_identity.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#define XXH_INLINE_ALL
#include "third-party/xxhash.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <vector>

namespace {
using Source = winx_remix::ResourceIdentitySource;
static unsigned checks;
static void Check(bool value, const char* message) {
  ++checks;
  if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
struct Hash {
  std::vector<std::uint8_t> bytes;
  std::vector<std::size_t> updates;
  std::uint64_t value = 14695981039346656037ull;
  unsigned resets = 0;
  bool zero = false;
  void Reset() { bytes.clear(); updates.clear(); value = 14695981039346656037ull; ++resets; }
  void Update(const void* data, std::size_t size) {
    updates.push_back(size);
    const auto* first = static_cast<const std::uint8_t*>(data);
    bytes.insert(bytes.end(), first, first + size);
    for (std::size_t index = 0; index < size; ++index) { value ^= first[index]; value *= 1099511628211ull; }
  }
  std::uint64_t Digest() const { return zero ? 0 : value; }
};
// Deliberately unlike the SDK layout. Content identities must not inspect its
// leading/trailing or interleaved padding, source allocation or extra fields.
struct Vertex {
  std::uint64_t leading = 0;
  float position[3]{};
  std::uint32_t pad0 = 0;
  float normal[3]{};
  std::uint32_t color = 0;
  float texcoord[2]{};
  std::uint64_t trailing = 0;
};
static std::array<Vertex, 3> Triangle() {
  std::array<Vertex, 3> output{};
  output[1].position[0] = 1; output[2].position[1] = 1;
  output[1].texcoord[0] = 1; output[2].texcoord[1] = 1;
  for (std::size_t index = 0; index < output.size(); ++index) {
    output[index].normal[2] = 1; output[index].color = static_cast<std::uint32_t>(index + 1);
  }
  return output;
}
static Source::MaterialRequest Material() {
  Source::MaterialRequest output;
  output.textureHash = 0x0123456789abcdefull; output.u = 1; output.v = 2; output.mag = 3;
  output.albedo[0] = 1; output.albedo[1] = .5f; output.albedo[2] = -0.0f;
  output.emission[0] = 0; output.emission[1] = .25f; output.emission[2] = .75f;
  return output;
}
static std::uint64_t ExpectedHash(const std::vector<std::uint8_t>& bytes) {
  std::uint64_t output = 14695981039346656037ull;
  for (std::uint8_t value : bytes) output = (output ^ value) * 1099511628211ull;
  return output;
}
static void ExpectedWord(std::vector<std::uint8_t>& bytes, std::uint32_t word) {
  bytes.push_back(static_cast<std::uint8_t>(word)); bytes.push_back(static_cast<std::uint8_t>(word >> 8));
  bytes.push_back(static_cast<std::uint8_t>(word >> 16)); bytes.push_back(static_cast<std::uint8_t>(word >> 24));
}
static void Protocol() {
  Hash hash;
  static constexpr char hello[] = "hello";
  hash.Update(hello, sizeof(hello) - 1);
  Check(hash.Digest() == 0xa430d84680aabd0bull, "fixture FNV-1a known answer");
  const std::vector<std::uint8_t> expectedMaterial{
    0x77,0x69,0x6e,0x78,0x2d,0x69,0x6e,0x64,0x65,0x70,0x65,0x6e,0x64,0x65,0x6e,0x74,0x2d,0x66,0x66,0x70,0x2d,0x76,0x31,
    0xef,0xcd,0xab,0x89,0x67,0x45,0x23,0x01,
    1,0,0,0,0,0,0,0, 2,0,0,0,0,0,0,0, 3,0,0,0,0,0,0,0,
    0,0,0x80,0x3f, 0,0,0,0x3f, 0,0,0,0x80,
    0,0,0,0, 0,0,0x80,0x3e, 0,0,0x40,0x3f
  };
  const auto materialHash = Source::Material(Material(), hash);
  Check(hash.resets == 1 && hash.bytes == expectedMaterial, "literal legacy material bytes include LE uint64 sampler fields and signed-zero float bits");
  Check(materialHash == ExpectedHash(expectedMaterial), "material digest consumes the exact complete protocol");
  const auto vertices = Triangle(); const std::uint32_t indices[]{0, 1, 2};
  const auto geometryHash = Source::GeometryIndexed(vertices.data(), vertices.size(), indices, 3, hash);
  static constexpr char domain[] = "winx-geometry-content-v1";
  std::vector<std::uint8_t> expectedGeometry(domain, domain + sizeof(domain) - 1);
  const std::uint8_t counts[]{3,0,0,0,0,0,0,0, 3,0,0,0,0,0,0,0};
  expectedGeometry.insert(expectedGeometry.end(), std::begin(counts), std::end(counts));
  // Semantic order is position3, normal3, uv2, color, independent of Vertex's
  // actual in-memory order. The stream ends with the three topology DWORDs.
  const std::uint32_t words[]{
    0,0,0, 0,0,0x3f800000u, 0,0,1,
    0x3f800000u,0,0, 0,0,0x3f800000u, 0x3f800000u,0,2,
    0,0x3f800000u,0, 0,0,0x3f800000u, 0,0x3f800000u,3,
    0,1,2
  };
  for (std::uint32_t word : words) ExpectedWord(expectedGeometry, word);
  Check(hash.bytes == expectedGeometry && geometryHash == ExpectedHash(expectedGeometry), "complete geometry byte protocol excludes all source padding");
  Check(Source::GeometryExpanded(vertices.data(), vertices.size(), hash) == geometryHash && hash.bytes == expectedGeometry,
    "expanded and equivalent sequential indexed geometry share a byte stream");
  const auto boundHash = Source::BoundMesh(0x0123456789abcdefull, 0xfedcba9876543210ull, hash);
  static constexpr char boundDomain[] = "winx-bound-mesh-v1";
  std::vector<std::uint8_t> expectedBound(boundDomain, boundDomain + sizeof(boundDomain) - 1);
  const std::uint8_t boundValues[]{0xef,0xcd,0xab,0x89,0x67,0x45,0x23,0x01, 0x10,0x32,0x54,0x76,0x98,0xba,0xdc,0xfe};
  expectedBound.insert(expectedBound.end(), std::begin(boundValues), std::end(boundValues));
  Check(hash.bytes == expectedBound && boundHash == ExpectedHash(expectedBound), "bound mesh has an independent domain and geometry/material order");
  Check(Source::BoundMesh(geometryHash, materialHash, hash) != geometryHash, "bound API resource differs from pure geometry fingerprint");
  hash.zero = true;
  Check(!Source::Material(Material(), hash) && !Source::GeometryExpanded(vertices.data(), vertices.size(), hash) &&
    !Source::BoundMesh(1, 2, hash), "zero digest remains unavailable rather than receiving an invented replacement");
}
static void IndependenceAndChanges() {
  Hash hash; const auto vertices = Triangle(); auto other = vertices;
  const auto original = Source::GeometryExpanded(vertices.data(), vertices.size(), hash);
  for (auto& vertex : other) { vertex.leading = 0x0123456789abcdefull; vertex.pad0 = 0xfedcba98u; vertex.trailing = 0xfedcba9876543210ull; }
  Check(other.data() != vertices.data() && Source::GeometryExpanded(other.data(), other.size(), hash) == original,
    "different allocations and deliberately changed padding retain geometry identity");
  for (unsigned field = 0; field < 9; ++field) {
    other = vertices;
    if (field < 3) other[0].position[field] = .25f;
    else if (field < 6) other[0].normal[field - 3] = .25f;
    else if (field < 8) other[0].texcoord[field - 6] = .25f;
    else other[0].color ^= 0x01000000u;
    Check(Source::GeometryExpanded(other.data(), other.size(), hash) != original, "every meaningful vertex component affects geometry identity");
  }
  other = vertices; other[0].position[0] = -0.0f;
  Check(Source::GeometryExpanded(other.data(), other.size(), hash) != original, "geometry retains finite signed-zero bits");
  const std::uint32_t reversed[]{1, 0, 2};
  Check(Source::GeometryIndexed(vertices.data(), vertices.size(), reversed, 3, hash) != original, "triangle winding affects geometry identity");
  const std::uint32_t repeated[]{0, 1, 2, 0, 1, 2};
  Check(Source::GeometryIndexed(vertices.data(), vertices.size(), repeated, 6, hash) != original, "triangle count and repeated topology affect geometry identity");
  auto material = Material(); const auto materialHash = Source::Material(material, hash);
  auto copiedMaterial = material;
  Check(Source::Material(copiedMaterial, hash) == materialHash, "material identity is independent of request allocation");
  for (unsigned field = 0; field < 10; ++field) {
    auto changed = material;
    if (!field) changed.textureHash ^= 1;
    else if (field == 1) changed.u = 2;
    else if (field == 2) changed.v = 1;
    else if (field == 3) changed.mag = 1;
    else if (field < 7) changed.albedo[field - 4] = .125f;
    else changed.emission[field - 7] = .125f;
    Check(Source::Material(changed, hash) != materialHash, "every material content field affects identity");
  }
  copiedMaterial.albedo[2] = 0.0f;
  Check(Source::Material(copiedMaterial, hash) != materialHash, "legacy material signed-zero identity is retained");
  const auto bound = Source::BoundMesh(original, materialHash, hash);
  Check(Source::BoundMesh(original + 1, materialHash, hash) != bound && Source::BoundMesh(original, materialHash + 1, hash) != bound,
    "bound resource identity tracks both geometry and stable material content");
  Check(Source::BoundMesh(materialHash, original, hash) != bound, "geometry and material identities have ordered roles");
}
static void Rejections() {
  Hash hash; const auto vertices = Triangle(); const std::uint32_t indices[]{0, 1, 2};
  Source::Material(Material(), hash);
  const auto bytes = hash.bytes; const auto resets = hash.resets;
  const auto unchanged = [&]() { Check(hash.bytes == bytes && hash.resets == resets, "invalid input never resets or changes caller's hash state"); };
  for (unsigned field = 0; field < 4; ++field) for (std::uint32_t value : {0u, 4u}) {
    auto material = Material();
    if (!field) { if (value) continue; material.textureHash = 0; }
    else if (field == 1) material.u = value;
    else if (field == 2) material.v = value;
    else material.mag = value;
    Check(!Source::Material(material, hash), "zero texture or unsupported sampler rejects"); unchanged();
  }
  const float invalid[]{-.25f, 1.25f, std::numeric_limits<float>::quiet_NaN(),
    std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()};
  for (unsigned field = 0; field < 6; ++field) for (float value : invalid) {
    auto material = Material(); if (field < 3) material.albedo[field] = value; else material.emission[field - 3] = value;
    Check(!Source::Material(material, hash), "unbounded or nonfinite material coefficient rejects"); unchanged();
  }
  Check(!Source::GeometryExpanded(static_cast<const Vertex*>(nullptr), 3, hash), "null expanded vertices reject"); unchanged();
  Check(!Source::GeometryExpanded(vertices.data(), 0, hash), "empty expanded vertices reject"); unchanged();
  Check(!Source::GeometryExpanded(vertices.data(), 2, hash), "expanded nontriangle count rejects"); unchanged();
  Check(!Source::GeometryExpanded(vertices.data(), Source::MaximumVertices + 1, hash), "oversized expanded input rejects before dereferencing"); unchanged();
  Check(!Source::GeometryIndexed(static_cast<const Vertex*>(nullptr), 3, indices, 3, hash), "null indexed vertices reject"); unchanged();
  Check(!Source::GeometryIndexed(vertices.data(), 0, indices, 3, hash), "empty indexed vertices reject"); unchanged();
  Check(!Source::GeometryIndexed(vertices.data(), Source::MaximumVertices + 1, indices, 3, hash), "oversized indexed vertices reject before dereferencing"); unchanged();
  Check(!Source::GeometryIndexed(vertices.data(), vertices.size(), nullptr, 3, hash), "null topology rejects"); unchanged();
  for (std::size_t count : {std::size_t(0), std::size_t(2), Source::MaximumIndices + 1}) {
    Check(!Source::GeometryIndexed(vertices.data(), vertices.size(), indices, count, hash), "empty nontriangle or oversized topology rejects"); unchanged();
  }
  for (std::uint32_t value : {3u, (std::numeric_limits<std::uint32_t>::max)()}) {
    const std::uint32_t bad[]{0, 1, value};
    Check(!Source::GeometryIndexed(vertices.data(), vertices.size(), bad, 3, hash), "out of range index rejects"); unchanged();
  }
  for (unsigned field = 0; field < 8; ++field) for (float value : {std::numeric_limits<float>::quiet_NaN(),
      std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()}) {
    auto bad = vertices;
    if (field < 3) bad[2].position[field] = value;
    else if (field < 6) bad[2].normal[field - 3] = value;
    else bad[2].texcoord[field - 6] = value;
    Check(!Source::GeometryExpanded(bad.data(), bad.size(), hash) &&
      !Source::GeometryIndexed(bad.data(), bad.size(), indices, 3, hash), "nonfinite vertex component rejects both representations"); unchanged();
  }
  Check(!Source::BoundMesh(0, 1, hash) && !Source::BoundMesh(1, 0, hash) && !Source::BoundMesh(0, 0, hash), "missing stable geometry or material identity rejects"); unchanged();
}
static void BoundsAndIndependence() {
  Hash first, second; auto vertices = Triangle(); const auto firstMaterial = Material();
  const auto geometry = Source::GeometryExpanded(vertices.data(), vertices.size(), first);
  const auto material = Source::Material(firstMaterial, second);
  Check(Source::GeometryExpanded(vertices.data(), vertices.size(), first) == geometry &&
    Source::Material(firstMaterial, second) == material, "independent hash objects and repeated captures preserve content identities");
  std::vector<Vertex> maximum(Source::MaximumVertices);
  const auto maxHash = Source::GeometryExpanded(maximum.data(), maximum.size(), first);
  Check(maxHash && first.bytes.size() == sizeof("winx-geometry-content-v1") - 1 + 16 + maximum.size() * 40,
    "maximum expanded triangle stream is accepted with bounded exact byte length");
  std::vector<std::uint32_t> maximumIndices(Source::MaximumIndices, 0);
  Vertex one;
  Check(Source::GeometryIndexed(&one, 1, maximumIndices.data(), maximumIndices.size(), second) != 0,
    "maximum indexed triangle stream accepts repeated in-range indices without semantic topology invention");
}
// Independent scalar oracle: construct protocol bytes explicitly, without the
// production GeometryWords/AppendVertex code or host-layout serialization.
template<class V> static std::vector<std::uint8_t> ReferenceGeometry(
    const std::vector<V>& vertices, const std::vector<std::uint32_t>* topology = nullptr) {
  static constexpr char domain[] = "winx-geometry-content-v1";
  std::vector<std::uint8_t> bytes(domain, domain + sizeof(domain) - 1);
  const auto count = vertices.size(), indices = topology ? topology->size() : count;
  bytes.reserve(sizeof(domain) - 1 + 16 + count * 36 + indices * 4);
  for (std::uint64_t value : {std::uint64_t(count), std::uint64_t(indices)}) {
    ExpectedWord(bytes, std::uint32_t(value)); ExpectedWord(bytes, std::uint32_t(value >> 32));
  }
  const auto scalar = [&](float value) {
    std::uint32_t bits = 0; std::memcpy(&bits, &value, 4); ExpectedWord(bytes, bits);
  };
  for (const auto& v : vertices) {
    for (unsigned i = 0; i < 3; ++i) scalar(float(v.position[i]));
    for (unsigned i = 0; i < 3; ++i) scalar(float(v.normal[i]));
    for (unsigned i = 0; i < 2; ++i) scalar(float(v.texcoord[i]));
    ExpectedWord(bytes, v.color);
  }
  for (std::size_t i = 0; i < indices; ++i) ExpectedWord(bytes, topology ? (*topology)[i] : std::uint32_t(i));
  return bytes;
}
static std::vector<std::size_t> ReferenceUpdates(std::size_t vertices, std::size_t indices) {
  std::vector<std::size_t> sizes{sizeof("winx-geometry-content-v1") - 1, 8, 8};
  for (std::size_t remaining = vertices * 36 + indices * 4; remaining;) {
    const auto part = remaining < 4096 ? remaining : std::size_t(4096);
    sizes.push_back(part); remaining -= part;
  }
  return sizes;
}
struct ProductionHash {
  XXH3_state_t state{};
  void Reset() { XXH3_64bits_reset(&state); }
  void Update(const void* data, std::size_t size) { XXH3_64bits_update(&state, data, size); }
  std::uint64_t Digest() const { return XXH3_64bits_digest(&state); }
};
struct PrefixVertex {
  float position[3]{}, normal[3]{}, texcoord[2]{};
  std::uint32_t color = 0, padding[7]{};
};
struct ArrayVertex {
  std::array<float, 3> position{}, normal{};
  std::array<float, 2> texcoord{};
  std::uint32_t color = 0;
};
struct DoubleVertex { double position[3]{}, normal[3]{}, texcoord[2]{}; std::uint32_t color = 0; };
struct BitfieldVertex { float position[3]{}, normal[3]{}, texcoord[2]{}; std::uint32_t color : 32; };
template<class V> static void FillGeometry(std::vector<V>& vertices) {
  static constexpr std::uint32_t edgeBits[]{0,0x80000000u,1,0x80000001u,0x007fffffu,0x00800000u,
    0x7f7fffffu,0xff7fffffu,0x3f800000u,0xbf800000u};
  std::uint32_t sequence = 0x51e076bdu;
  for (std::size_t i = 0; i < vertices.size(); ++i) {
    auto& v = vertices[i];
    for (unsigned field = 0; field < 8; ++field) {
      sequence = sequence * 1664525u + 1013904223u;
      auto bits = i < 10 ? edgeBits[(i + field) % 10] : sequence;
      if ((bits & 0x7f800000u) == 0x7f800000u) bits ^= 0x00800000u;
      float value = 0; std::memcpy(&value, &bits, 4);
      if (field < 3) v.position[field] = value;
      else if (field < 6) v.normal[field - 3] = value;
      else v.texcoord[field - 6] = value;
    }
    v.color = sequence ^ std::uint32_t(i);
  }
}
template<class V> static void DifferentialGeometry(std::size_t count) {
  std::vector<V> vertices(count); FillGeometry(vertices);
  const auto expected = ReferenceGeometry(vertices);
  Hash recording; ProductionHash production;
  const auto geometry = Source::GeometryExpanded(vertices.data(), vertices.size(), recording);
  Check(recording.bytes == expected && geometry == ExpectedHash(expected), "packed geometry equals independent scalar byte oracle");
  Check(recording.updates == ReferenceUpdates(count, count), "packed geometry preserves every provider Update chunk boundary");
  Check(Source::GeometryExpanded(vertices.data(), vertices.size(), production) == XXH3_64bits(expected.data(), expected.size()),
    "production streaming XXH3 ID equals independent canonical stream one-shot ID");
  std::vector<std::uint32_t> indices(count);
  for (std::size_t i = 0; i < count; ++i) indices[i] = std::uint32_t(count - i - 1);
  const auto indexed = ReferenceGeometry(vertices, &indices);
  Source::GeometryIndexed(vertices.data(), count, indices.data(), count, recording);
  Check(recording.bytes == indexed && recording.updates == ReferenceUpdates(count, count),
    "indexed topology keeps the same vertex packing and unchanged canonical index order/chunks");
  Check(Source::GeometryIndexed(vertices.data(), count, indices.data(), count, production) == XXH3_64bits(indexed.data(), indexed.size()),
    "production indexed XXH3 ID equals independent byte oracle");
}
struct ProviderFailure {};
struct ThrowingHash : Hash {
  std::size_t updateCalls = 0, failUpdate = 0;
  unsigned resetCalls = 0, digestCalls = 0;
  bool failReset = false, failDigest = false;
  void Reset() { ++resetCalls; if (failReset) throw ProviderFailure{}; Hash::Reset(); }
  void Update(const void* data, std::size_t size) {
    ++updateCalls; if (updateCalls == failUpdate) throw ProviderFailure{}; Hash::Update(data, size);
  }
  std::uint64_t Digest() { ++digestCalls; if (failDigest) throw ProviderFailure{}; return Hash::Digest(); }
};
static void PackingBoundariesAndFailures() {
  // All nine split-vertex offsets occur across these 4096-byte payload edges.
  static constexpr std::size_t counts[]{3,111,114,117,228,342,456,570,684,798,912,1026,Source::MaximumVertices};
  for (std::size_t count : counts) {
    DifferentialGeometry<Vertex>(count); DifferentialGeometry<PrefixVertex>(count);
  }
  DifferentialGeometry<ArrayVertex>(117); DifferentialGeometry<DoubleVertex>(117);
  DifferentialGeometry<BitfieldVertex>(117);
  volatile PrefixVertex volatileVertices[3]{}; PrefixVertex plainVertices[3]{}; Hash volatileHash, plainHash;
  Check(Source::GeometryExpanded(volatileVertices, 3, volatileHash) == Source::GeometryExpanded(plainVertices, 3, plainHash) &&
    volatileHash.bytes == plainHash.bytes, "volatile source retains scalar compatibility rather than entering memcpy packing");
  std::vector<PrefixVertex> vertices(231); FillGeometry(vertices);
  Hash first, paddingChanged;
  Source::GeometryExpanded(vertices.data(), vertices.size(), first);
  for (auto& v : vertices) for (auto& pad : v.padding) pad = 0xdeadbeefu;
  Source::GeometryExpanded(vertices.data(), vertices.size(), paddingChanged);
  Check(first.bytes == paddingChanged.bytes, "nonzero SDK-like trailing padding is excluded from packed copies");
  const auto expected = ReferenceGeometry(vertices); const auto updates = ReferenceUpdates(vertices.size(), vertices.size());
  for (std::size_t failure = 1; failure <= updates.size(); ++failure) {
    ThrowingHash hash; hash.failUpdate = failure; bool caught = false;
    try { Source::GeometryExpanded(vertices.data(), vertices.size(), hash); } catch (const ProviderFailure&) { caught = true; }
    std::size_t prefix = 0; for (std::size_t i = 0; i + 1 < failure; ++i) prefix += updates[i];
    Check(caught && hash.resetCalls == 1 && hash.updateCalls == failure && hash.digestCalls == 0 &&
      hash.bytes == std::vector<std::uint8_t>(expected.begin(), expected.begin() + prefix),
      "provider Update exception propagates at identical chunk with no unwind flush or digest");
  }
  for (bool atReset : {false, true}) {
    ThrowingHash hash; hash.failReset = atReset; hash.failDigest = !atReset; bool caught = false;
    try { Source::GeometryExpanded(vertices.data(), vertices.size(), hash); } catch (const ProviderFailure&) { caught = true; }
    Check(caught && hash.resetCalls == 1 && hash.updateCalls == (atReset ? 0 : updates.size()) &&
      hash.digestCalls == (atReset ? 0u : 1u), "provider Reset/Digest exceptions are propagated without extra provider calls");
  }
  for (float invalid : {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
    Hash hash; hash.Update("keep", 4); const auto before = hash.bytes;
    const auto last = vertices.back().texcoord[1]; vertices.back().texcoord[1] = invalid;
    Check(!Source::GeometryExpanded(vertices.data(), vertices.size(), hash) && hash.resets == 0 && hash.bytes == before,
      "late malformed component rejects before Reset despite multiple potential packed chunks");
    vertices.back().texcoord[1] = last;
  }
}
struct BoundaryObserver : Hash {
  PrefixVertex* vertices = nullptr;
  bool changed = false;
  void Update(const void* data, std::size_t size) {
    Hash::Update(data, size);
    if (size == 4096 && !changed) {
      changed = true; vertices[113].texcoord[1] = .375f; vertices[113].color ^= 0x78563412u;
    }
  }
};
static void ProviderBoundaryReads() {
  std::vector<PrefixVertex> vertices(117); FillGeometry(vertices);
  BoundaryObserver hash; hash.vertices = vertices.data();
  Source::GeometryExpanded(vertices.data(), vertices.size(), hash);
  Check(hash.changed && hash.bytes == ReferenceGeometry(vertices) && hash.updates == ReferenceUpdates(117, 117),
    "split vertex reads UV/color after provider callback, preserving original scalar callback boundary");
}
}
int main() {
  Protocol(); IndependenceAndChanges(); Rejections(); BoundsAndIndependence(); PackingBoundariesAndFailures(); ProviderBoundaryReads();
  std::printf("{\"status\":\"PASS\",\"checks\":%u,\"gpu\":false,\"nativeCode\":false}\n", checks);
}
