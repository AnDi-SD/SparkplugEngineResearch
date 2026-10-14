#pragma once
// Own CP13/PC scene-registry reader, not reconstructed native game classes.
// Capture complete loaded Light membership, including lights outside the
// camera and regardless of active/enabled flags. Payload bytes are observed
// unchanged; this source does not run native producers or recompute dirty data.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <set>
#include <vector>

namespace winx_remix {
struct SceneLightSnapshot {
  struct Record { std::uint32_t address; unsigned char raw[0x158]; };
  std::uint32_t scene = 0, manager = 0, count = 0;
  bool valid = false;
  std::vector<Record> lights;
};

class SceneLightSnapshotSource final {
public:
  // Both readers implement bool(uint32_t address, void* out, size_t size).
  // The caller supplies one fresh record-read window per observation; this
  // stateless source retains no bytes across observations or external calls.
  // Structural validation does not prove an atomic snapshot or freshness of
  // descendant producers. Allocation failures propagate to the owning caller.
  template<class HeaderReader, class RecordReader>
  SceneLightSnapshot Capture(std::uint32_t scene, const HeaderReader& read, const RecordReader& readRecord) const {
    // CP8/CP92: complete Light pointers, borrowed B8/BC links, owned manager34.
    // Observe the scene registry rather than eight per-object D3D light slots.
    std::uint32_t manager = 0;
    if (!read(scene + 0x34u, &manager, sizeof(manager))) return {};
    unsigned char header[0x24]{};
    if (!read(manager, header, sizeof(header)) || At(header, 0) != 0x6e8ca4 ||
        At(header, 0x20) != scene || At(header, 0x18) > 512) return {};

    SceneLightSnapshot snapshot;
    snapshot.scene = scene;
    snapshot.manager = manager;
    auto& lights = snapshot.lights;
    const auto count = At(header, 0x18);
    snapshot.count = count;
    std::uint32_t pointer = At(header, 0x10), previous = 0;
    std::set<std::uint32_t> visited;
    bool valid = true;
    for (unsigned i = 0; i < count; ++i) {
      SceneLightSnapshot::Record record{ pointer, {} };
      if (!pointer || !visited.insert(pointer).second || !readRecord(pointer, record.raw, sizeof(record.raw)) ||
          At(record.raw, 0) != 0x6f0c88 || At(record.raw, 0x3c) != scene || At(record.raw, 0xb8) != previous) {
        valid = false; break;
      }
      previous = pointer;
      pointer = At(record.raw, 0xbc);
      lights.push_back(record);
    }
    valid = valid && !pointer && previous == At(header, 0x14) && lights.size() == count;
    snapshot.valid = valid;
    return snapshot;
  }

private:
  static std::uint32_t At(const unsigned char* bytes, std::size_t offset) {
    std::uint32_t result;
    std::memcpy(&result, bytes + offset, sizeof(result));
    return result;
  }
};
} // namespace winx_remix
