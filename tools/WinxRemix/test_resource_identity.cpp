// Own byte-protocol fixture. No game, SDK, COM, file publication or GPU.
#include "winx_resource_identity.h"
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
  std::uint64_t value = 14695981039346656037ull;
  unsigned resets = 0;
  bool zero = false;
  void Reset() { bytes.clear(); value = 14695981039346656037ull; ++resets; }
  void Update(const void* data, std::size_t size) {
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
}
int main() {
  Protocol(); IndependenceAndChanges(); Rejections(); BoundsAndIndependence();
  std::printf("{\"status\":\"PASS\",\"checks\":%u,\"gpu\":false,\"nativeCode\":false}\n", checks);
}
