#pragma once
// Own versioned content identities, not recovered game logic or native lifetime
// tokens. The caller supplies the hash algorithm and qualifies live resources.
// No address, API handle, world transform, frame or generation enters a stream.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace winx_remix {
class ResourceIdentitySource final {
public:
  static constexpr std::size_t MaximumVertices = 32768 * 3;
  static constexpr std::size_t MaximumIndices = 32768 * 3;
  struct MaterialRequest {
    std::uint64_t textureHash = 0;
    std::uint32_t u = 0, v = 0, mag = 0;
    float albedo[3]{}, emission[3]{};
  };

  // Hash provides Reset(), Update(const void*, size_t), and uint64_t Digest().
  // Invalid input returns zero without touching it. Hash exceptions propagate.
  // Float fields use finite IEEE-754 binary32 bits, retaining signed zero.
  // A zero digest is reserved as an unavailable identity; no substitute is made.
  template<class Hash>
  static std::uint64_t Material(const MaterialRequest& request, Hash& hash) {
    if (!request.textureHash || request.u < 1 || request.u > 3 ||
        request.v < 1 || request.v > 3 || request.mag < 1 || request.mag > 3 ||
        !Bounded(request.albedo) || !Bounded(request.emission)) return 0;
    // Preserve the existing channel-material descriptor byte for byte, without
    // its terminating NUL. Existing DDS names therefore keep their identities.
    static constexpr char domain[] = "winx-independent-ffp-v1";
    hash.Reset();
    hash.Update(domain, sizeof(domain) - 1);
    U64(hash, request.textureHash);
    U64(hash, request.u); U64(hash, request.v); U64(hash, request.mag);
    for (float value : request.albedo) Float(hash, value);
    for (float value : request.emission) Float(hash, value);
    return hash.Digest();
  }

  // Vertex supplies position[3], normal[3], texcoord[2], and uint32 color.
  // Only those fields are serialized; SDK/host padding is deliberately absent.
  // The index order is the submitted triangle-list order. Expanded geometry
  // emits sequential indices and has exactly the same identity as that indexed
  // representation, rather than introducing a second expanded-only domain.
  template<class Vertex, class Hash>
  static std::uint64_t GeometryExpanded(const Vertex* vertices, std::size_t vertexCount, Hash& hash) {
    if (!vertexCount || vertexCount > MaximumIndices || vertexCount % 3 ||
        !ValidVertices(vertices, vertexCount)) return 0;
    return Geometry(vertices, vertexCount, vertexCount,
      [](std::size_t index) { return static_cast<std::uint32_t>(index); }, hash);
  }

  template<class Vertex, class Hash>
  static std::uint64_t GeometryIndexed(const Vertex* vertices, std::size_t vertexCount,
      const std::uint32_t* indices, std::size_t indexCount, Hash& hash) {
    if (!indices || !indexCount || indexCount > MaximumIndices || indexCount % 3 ||
        !ValidVertices(vertices, vertexCount)) return 0;
    for (std::size_t index = 0; index < indexCount; ++index)
      if (indices[index] >= vertexCount) return 0;
    return Geometry(vertices, vertexCount, indexCount,
      [indices](std::size_t index) { return indices[index]; }, hash);
  }

  // A mesh resource binds geometry to a material. A pure geometry fingerprint
  // remains separate; using it alone as an API resource ID could alias material
  // bindings in a renderer whose mesh storage is keyed by the supplied hash.
  template<class Hash>
  static std::uint64_t BoundMesh(std::uint64_t geometryHash, std::uint64_t materialHash, Hash& hash) {
    if (!geometryHash || !materialHash) return 0;
    static constexpr char domain[] = "winx-bound-mesh-v1";
    hash.Reset(); hash.Update(domain, sizeof(domain) - 1);
    U64(hash, geometryHash); U64(hash, materialHash);
    return hash.Digest();
  }

private:
  static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559,
    "Resource identities require IEEE-754 binary32 floats");
  template<class Hash> static void U32(Hash& hash, std::uint32_t value) {
    std::uint8_t bytes[4]{};
    for (unsigned index = 0; index < 4; ++index) bytes[index] = static_cast<std::uint8_t>(value >> (index * 8));
    hash.Update(bytes, sizeof(bytes));
  }
  template<class Hash> static void U64(Hash& hash, std::uint64_t value) {
    std::uint8_t bytes[8]{};
    for (unsigned index = 0; index < 8; ++index) bytes[index] = static_cast<std::uint8_t>(value >> (index * 8));
    hash.Update(bytes, sizeof(bytes));
  }
  template<class Hash> static void Float(Hash& hash, float value) {
    std::uint32_t bits = 0; std::memcpy(&bits, &value, sizeof(bits)); U32(hash, bits);
  }
  static bool Bounded(const float (&values)[3]) {
    for (float value : values) if (!std::isfinite(value) || value < 0.0f || value > 1.0f) return false;
    return true;
  }
  template<class Vertex> static bool ValidVertices(const Vertex* vertices, std::size_t count) {
    if (!vertices || !count || count > MaximumVertices) return false;
    for (std::size_t index = 0; index < count; ++index) {
      for (unsigned component = 0; component < 3; ++component)
        if (!std::isfinite(vertices[index].position[component]) || !std::isfinite(vertices[index].normal[component])) return false;
      for (unsigned component = 0; component < 2; ++component)
        if (!std::isfinite(vertices[index].texcoord[component])) return false;
    }
    return true;
  }
  template<class Vertex, class IndexAt, class Hash>
  static std::uint64_t Geometry(const Vertex* vertices, std::size_t vertexCount,
      std::size_t indexCount, const IndexAt& indexAt, Hash& hash) {
    static constexpr char domain[] = "winx-geometry-content-v1";
    hash.Reset(); hash.Update(domain, sizeof(domain) - 1);
    U64(hash, static_cast<std::uint64_t>(vertexCount)); U64(hash, static_cast<std::uint64_t>(indexCount));
    for (std::size_t index = 0; index < vertexCount; ++index) {
      const auto& vertex = vertices[index];
      for (unsigned component = 0; component < 3; ++component) Float(hash, vertex.position[component]);
      for (unsigned component = 0; component < 3; ++component) Float(hash, vertex.normal[component]);
      for (unsigned component = 0; component < 2; ++component) Float(hash, vertex.texcoord[component]);
      U32(hash, vertex.color);
    }
    for (std::size_t index = 0; index < indexCount; ++index) U32(hash, indexAt(index));
    return hash.Digest();
  }
};
} // namespace winx_remix
