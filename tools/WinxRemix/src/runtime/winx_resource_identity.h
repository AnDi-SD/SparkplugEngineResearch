#pragma once
// Own versioned content identities, not recovered game logic or native lifetime
// tokens. The caller supplies the hash algorithm and qualifies live resources.
// No address, API handle, world transform, frame or generation enters a stream.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

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
  // Batch only the serialization calls, never the identity bytes. A fixed
  // stack buffer avoids a whole-mesh allocation and repeated tiny hash updates.
  // Flush is explicit: unwinding a caller's hash exception must not write more.
  template<class Hash> class GeometryWords final {
  public:
    explicit GeometryWords(Hash& hash) : hash_(hash) {}
    void U32(std::uint32_t value) {
      for (unsigned index = 0; index < 4; ++index)
        bytes_[used_++] = static_cast<std::uint8_t>(value >> (index * 8));
      if (used_ == sizeof(bytes_)) Flush();
    }
    void Float(float value) {
      std::uint32_t bits = 0; std::memcpy(&bits, &value, sizeof(bits)); U32(bits);
    }
    template<class Vertex> void AppendVertex(const Vertex& vertex) {
#if defined(_WIN32) && (defined(_M_IX86) || defined(_M_X64))
      // Windows x86/x64 stores these scalar representations little-endian.
      // Only declared C float arrays and uint32 color qualify: an indexable
      // proxy, wider scalar, or different host uses the original word writer.
      using Position = decltype(vertex.position);
      using Normal = decltype(vertex.normal);
      using Texcoord = decltype(vertex.texcoord);
      if constexpr (!std::is_volatile<Vertex>::value &&
          std::is_array<Position>::value && std::extent<Position>::value >= 3 &&
          std::is_same<typename std::remove_extent<Position>::type, float>::value &&
          std::is_array<Normal>::value && std::extent<Normal>::value >= 3 &&
          std::is_same<typename std::remove_extent<Normal>::type, float>::value &&
          std::is_array<Texcoord>::value && std::extent<Texcoord>::value >= 2 &&
          std::is_same<typename std::remove_extent<Texcoord>::type, float>::value &&
          std::is_same<decltype(vertex.color), std::uint32_t>::value) {
        if (sizeof(bytes_) - used_ >= 36) {
          // Separate semantic fields exclude all leading, interleaved and SDK
          // padding. A full vertex fits before any provider Update callback.
          std::memcpy(bytes_ + used_, vertex.position, 12);
          std::memcpy(bytes_ + used_ + 12, vertex.normal, 12);
          std::memcpy(bytes_ + used_ + 24, vertex.texcoord, 8);
          const std::uint32_t color = vertex.color;
          std::memcpy(bytes_ + used_ + 32, &color, 4);
          used_ += 36;
          if (used_ == sizeof(bytes_)) Flush();
          return;
        }
      }
#endif
      // Preserve the exact 4096-byte Update boundary inside a split vertex:
      // fields after that callback/throw point must not be read in advance.
      for (unsigned component = 0; component < 3; ++component) Float(vertex.position[component]);
      for (unsigned component = 0; component < 3; ++component) Float(vertex.normal[component]);
      for (unsigned component = 0; component < 2; ++component) Float(vertex.texcoord[component]);
      U32(vertex.color);
    }
    void Flush() {
      if (used_) { hash_.Update(bytes_, used_); used_ = 0; }
    }
  private:
    Hash& hash_;
    std::uint8_t bytes_[4096];
    std::size_t used_ = 0;
  };
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
    GeometryWords<Hash> words(hash);
    for (std::size_t index = 0; index < vertexCount; ++index) words.AppendVertex(vertices[index]);
    for (std::size_t index = 0; index < indexCount; ++index) words.U32(indexAt(index));
    words.Flush();
    return hash.Digest();
  }
};
} // namespace winx_remix
