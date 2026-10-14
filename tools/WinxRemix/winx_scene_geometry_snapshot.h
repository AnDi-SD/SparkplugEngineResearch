#pragma once
// Own bounded reader of loaded PC support membership, extracted from the
// render adapter. Numeric join addresses are owned values, not borrowed native
// spans or object lifetime identities. No camera/active selection is applied.
// This does not establish mesh-byte/world freshness or refresh native producers.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <set>
#include <vector>

namespace winx_remix {
enum class SceneGeometryOccurrenceSource : std::uint32_t {
  RenderNodeVector = 0x20, StaticVector = 0x30, PartitionPayload = 0x78
};
struct SceneGeometrySupportOccurrence {
  std::uint32_t node, object, support;
  SceneGeometryOccurrenceSource source;
  std::uint32_t ordinal;
};

struct SceneGeometrySnapshot {
  using OccurrenceSource = SceneGeometryOccurrenceSource;
  using SupportOccurrence = SceneGeometrySupportOccurrence;
  static constexpr unsigned kOccurrenceLimit = 65536;
  bool valid = false, occurrencesComplete = true;
  std::uint32_t scene = 0, system = 0, root = 0;
  std::uint64_t mutationSerial = 0;
  unsigned nodes = 0, dynamic = 0, unsupportedDynamic = 0;
  // Visibility input stays sorted/unique. Occurrences retain physical vector
  // slots and payloads, including repeated objects and their original ordinals.
  std::vector<std::uint32_t> supports;
  std::vector<SupportOccurrence> occurrences;

  void Occurrence(std::uint32_t node, std::uint32_t object, std::uint32_t support,
                  OccurrenceSource source, std::uint32_t ordinal) {
    if (!occurrencesComplete) return;
    if (occurrences.size() >= kOccurrenceLimit) { occurrencesComplete = false; return; }
    // Optional provenance must not prevent the existing membership walk from
    // completing if its additional allocation cannot be satisfied.
    try { occurrences.push_back({node, object, support, source, ordinal}); }
    catch (const std::bad_alloc&) { occurrencesComplete = false; }
  }
};

class SceneGeometrySnapshotSource final {
public:
  static constexpr unsigned kLimit = 4096;
  static constexpr unsigned kOccurrenceLimit = SceneGeometrySnapshot::kOccurrenceLimit;

  // read implements bool(uint32_t address, void* out, size_t size). Vector
  // headers and elements are copied; empty vectors require no element read.
  template<class Reader>
  static bool ReadVector(std::uint32_t object, unsigned offset,
                         std::vector<std::uint32_t>& result, const Reader& read) {
    std::uint32_t fields[3]{};
    if (!read(object + offset + 4, fields, sizeof(fields))) return false;
    const auto begin = fields[0], end = fields[1], capacity = fields[2];
    if (begin > end || end > capacity || (end - begin) % 4 || end - begin > kLimit * 4) return false;
    result.resize((end - begin) / 4);
    return result.empty() || read(begin, result.data(), result.size() * 4);
  }

  template<class Reader>
  static bool ReadSupport(std::uint32_t object, std::uint32_t scene, bool partition,
                          std::uint32_t& adjusted, const Reader& read) {
    unsigned char raw[0x8c]{};
    if (!object || !read(object, raw, sizeof(raw))) return false;
    const unsigned offset = partition ? 0x10 : 0x14;
    if (At(raw, 0) != (partition ? 0x6f4540u : 0x6e65e8u) ||
        At(raw, offset) != (partition ? 0x6f4528u : 0x6e6604u) ||
        At(raw, offset + 0x70) != object || At(raw, 0x88) != scene) return false;
    adjusted = object + offset;
    return true;
  }

  // mutation implements uint64_t(). It is own adapter metadata, not a native
  // object generation. Matching before/after serials do not prove atomicity,
  // lifetime, or freshness of mesh bytes/world transforms. Essential walk
  // allocations propagate; only optional occurrence allocation degrades.
  template<class Reader, class Mutation>
  SceneGeometrySnapshot Capture(std::uint32_t scene, const Reader& read,
                                const Mutation& mutation) const {
    SceneGeometrySnapshot output;
    output.scene = scene;
    output.mutationSerial = mutation();
    output.system = Word(scene + 0x38, read);
    output.root = output.system ? Word(output.system + 0x1d4, read) : 0;
    const auto root = output.root;
    if (!root) return output;
    std::vector<std::uint32_t> pending{root};
    std::set<std::uint32_t> visited, supports, zones, dynamic, unsupported;
    while (!pending.empty()) {
      const auto node = pending.back();
      pending.pop_back();
      if (!node || !visited.insert(node).second) continue;
      if (visited.size() > kLimit) return output;
      unsigned char raw[0x84]{};
      if (!read(node, raw, sizeof(raw)) || At(raw, 0x80) != scene) return output;
      const auto table = At(raw, 0);
      if (table != 0x6dcb08 && table != 0x6e4420 && table != 0x6eba30) return output;
      const auto payload = At(raw, 0x78);
      std::uint32_t support = 0;
      if (payload) {
        if (!ReadSupport(payload, scene, true, support, read)) return output;
        supports.insert(support);
        output.Occurrence(node, payload, support, SceneGeometryOccurrenceSource::PartitionPayload, 0);
      }
      std::vector<std::uint32_t> values;
      if (!ReadVector(node, 0x30, values, read)) return output;
      for (unsigned ordinal = 0; ordinal < values.size(); ++ordinal) if (const auto value = values[ordinal]) {
        if (!ReadSupport(value, scene, false, support, read)) return output;
        supports.insert(support);
        output.Occurrence(node, value, support, SceneGeometryOccurrenceSource::StaticVector, ordinal);
      }
      if (!ReadVector(node, 0x20, values, read)) return output;
      for (unsigned ordinal = 0; ordinal < values.size(); ++ordinal) if (const auto value = values[ordinal]) {
        if (!dynamic.count(value) && !unsupported.count(value)) {
          unsigned char renderNode[0x130]{};
          if (!read(value, renderNode, sizeof(renderNode)) || At(renderNode, 0x3c) != scene ||
              At(renderNode, 0x124) != value) return output;
          // Unknown custom dynamic support stays on the original path and is
          // counted; an unrecognized static/partition support rejects the walk.
          if (At(renderNode, 0xb4) != 0x6dcadc) { unsupported.insert(value); continue; }
          dynamic.insert(value);
          supports.insert(value + 0xb4);
        }
        if (dynamic.count(value))
          output.Occurrence(node, value, value + 0xb4, SceneGeometryOccurrenceSource::RenderNodeVector, ordinal);
      }
      if (supports.size() > kLimit) return output;
      const auto count = At(raw, 0x5c), children = At(raw, 0x58);
      if (count > 8 || (count && !children)) return output;
      std::uint32_t child[8]{};
      if (count && !read(children, child, count * 4)) return output;
      for (unsigned i = 0; i < count; ++i) if (child[i]) pending.push_back(child[i]);
      const auto zone = At(raw, 0x60);
      if (zone && zones.insert(zone).second) {
        if (Word(zone, read) != 0x6ebacc || !ReadVector(zone, 0xb4, values, read)) return output;
        pending.insert(pending.end(), values.begin(), values.end());
      }
      if (pending.size() > kLimit * 8) return output;
    }
    output.nodes = static_cast<unsigned>(visited.size());
    output.dynamic = static_cast<unsigned>(dynamic.size());
    output.unsupportedDynamic = static_cast<unsigned>(unsupported.size());
    output.supports.assign(supports.begin(), supports.end());
    output.valid = output.mutationSerial == mutation();
    return output;
  }

private:
  static std::uint32_t At(const unsigned char* bytes, std::size_t offset) {
    std::uint32_t result;
    std::memcpy(&result, bytes + offset, sizeof(result));
    return result;
  }
  template<class Reader>
  static std::uint32_t Word(std::uint32_t address, const Reader& read) {
    // Preserve the existing adapter's zero-initialized, best-effort Word read.
    std::uint32_t value = 0;
    read(address, &value, sizeof(value));
    return value;
  }
};
} // namespace winx_remix
