// Own CPU reader contract fixture: literal fake PC memory, no game or GPU.
// It checks loaded membership and byte ownership, not native producer refresh.
#include "winx_scene_light_snapshot.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <map>
#include <new>
#include <vector>

using winx_remix::SceneLightSnapshot;
using winx_remix::SceneLightSnapshotSource;
static unsigned checks;
static volatile bool throwRecordRead;

static void Check(bool condition, const char* message) {
  ++checks;
  if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
// Encode independently as little-endian PC bytes, without using the source's
// field decoder or any game class representation.
template<class Bytes>
static void Put32(Bytes& bytes, std::size_t offset, std::uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) bytes[offset + i] = static_cast<unsigned char>(value >> (i * 8));
}

struct Memory {
  std::map<std::uint32_t, std::vector<unsigned char>> ranges;
  mutable unsigned reads = 0;
  bool Read(std::uint32_t address, void* output, std::size_t size) const {
    ++reads;
    for (const auto& range : ranges) {
      if (address < range.first) continue;
      const std::size_t offset = std::size_t(address) - range.first;
      if (offset <= range.second.size() && size <= range.second.size() - offset) {
        std::memcpy(output, range.second.data() + offset, size); return true;
      }
    }
    return false;
  }
};

static constexpr std::uint32_t Scene = 0x1000, Manager = 0x2000, First = 0x3000, Second = 0x4000;
using LightBytes = std::array<unsigned char, 0x158>;

static LightBytes FirstBytes() {
  LightBytes bytes; bytes.fill(0xa5);
  Put32(bytes, 0, 0x006f0c88); Put32(bytes, 0x3c, Scene);
  Put32(bytes, 0xb8, 0); Put32(bytes, 0xbc, Second);
  // Synthetic far-position marker (-900.0f); no camera input reaches Capture.
  Put32(bytes, 0x74, 0xc4610000);
  Put32(bytes, 0xb0, 0); bytes[0xed] = 1; // Inactive hierarchy, individually enabled.
  return bytes;
}
static LightBytes SecondBytes() {
  LightBytes bytes; bytes.fill(0x5a);
  Put32(bytes, 0, 0x006f0c88); Put32(bytes, 0x3c, Scene);
  Put32(bytes, 0xb8, First); Put32(bytes, 0xbc, 0);
  Put32(bytes, 0xb0, 0x108); bytes[0xed] = 0; // Active/dirty flags, individually disabled.
  return bytes;
}
struct Fixture {
  Memory headers, records;
  Fixture() {
    auto& scene = headers.ranges[Scene]; scene.resize(0x38);
    Put32(scene, 0x34, Manager);
    // Literal 0x24-byte manager header: vtable, head, tail, count, owner.
    headers.ranges[Manager] = {
      0xa4,0x8c,0x6e,0x00, 0,0,0,0, 0,0,0,0, 0,0,0,0,
      0x00,0x30,0x00,0x00, 0x00,0x40,0x00,0x00, 0x02,0,0,0,
      0,0,0,0, 0x00,0x10,0x00,0x00
    };
    const auto first = FirstBytes(), second = SecondBytes();
    records.ranges[First] = { first.begin(), first.end() };
    records.ranges[Second] = { second.begin(), second.end() };
  }
  SceneLightSnapshot Capture() const {
    SceneLightSnapshotSource source;
    return source.Capture(Scene,
      [&](std::uint32_t address, void* out, std::size_t size) { return headers.Read(address, out, size); },
      [&](std::uint32_t address, void* out, std::size_t size) { return records.Read(address, out, size); });
  }
};

static void InvalidHeader(const SceneLightSnapshot& snapshot, const char* message) {
  Check(!snapshot.valid && snapshot.scene == 0 && snapshot.manager == 0 && snapshot.count == 0 && snapshot.lights.empty(), message);
}
static void InvalidPartial(const SceneLightSnapshot& snapshot, unsigned count, std::size_t retained, const char* message) {
  Check(!snapshot.valid && snapshot.scene == Scene && snapshot.manager == Manager &&
    snapshot.count == count && snapshot.lights.size() == retained, message);
}

int main() {
  Fixture fixture;
  const auto first = FirstBytes(), second = SecondBytes();
  const auto snapshot = fixture.Capture();
  Check(snapshot.valid && snapshot.scene == Scene && snapshot.manager == Manager && snapshot.count == 2 &&
    snapshot.lights.size() == 2, "complete two-node loaded registry");
  Check(snapshot.lights[0].address == First && snapshot.lights[1].address == Second, "native list order retained");
  Check(std::memcmp(snapshot.lights[0].raw, first.data(), first.size()) == 0 &&
    std::memcmp(snapshot.lights[1].raw, second.data(), second.size()) == 0, "every observed payload byte retained");
  Check(snapshot.lights[0].raw[0x76] == 0x61 && snapshot.lights[0].raw[0x77] == 0xc4,
    "far-position offscreen marker retained without a camera filter");
  Check(snapshot.lights[0].raw[0xb0] == 0 && snapshot.lights[0].raw[0xb1] == 0 &&
    snapshot.lights[0].raw[0xed] == 1, "inactive hierarchy record retained");
  Check(snapshot.lights[1].raw[0xed] == 0 && snapshot.lights[1].raw[0xb0] == 8 &&
    snapshot.lights[1].raw[0xb1] == 1, "disabled record and dirty marker retained unchanged");
  Check(fixture.headers.reads == 2 && fixture.records.reads == 2,
    "manager headers and light payloads use different supplied readers");

  fixture.records.ranges[First][0xc4] = 0xee;
  Check(snapshot.lights[0].raw[0xc4] == first[0xc4] &&
    std::memcmp(snapshot.lights[0].raw, first.data(), first.size()) == 0, "snapshot owns bytes after source mutation");
  const auto fresh = fixture.Capture();
  Check(fresh.valid && fresh.lights[0].raw[0xc4] == 0xee && snapshot.lights[0].raw[0xc4] == 0xa5,
    "next observation reads changed bytes while prior snapshot stays unchanged");
  Check(fixture.headers.reads == 4 && fixture.records.reads == 4, "fresh observation performs fresh supplied reads");

  {
    Fixture f; Put32(f.headers.ranges[Scene], 0x34, 0x9000);
    InvalidHeader(f.Capture(), "unmapped manager rejected before any membership result");
    Check(f.records.reads == 0, "invalid manager never reaches record reader");
  }
  {
    Fixture f; Put32(f.headers.ranges[Manager], 0, 0x006e8ca5);
    InvalidHeader(f.Capture(), "different manager vtable rejected");
  }
  {
    Fixture f; Put32(f.headers.ranges[Manager], 0x20, 0x1100);
    InvalidHeader(f.Capture(), "manager owned by another scene rejected");
  }
  {
    Fixture f; Put32(f.headers.ranges[Manager], 0x18, 513);
    InvalidHeader(f.Capture(), "declared count above bound rejected");
    Check(f.records.reads == 0, "oversized count makes no light reads");
  }
  {
    Fixture f; f.headers.ranges.erase(Scene);
    InvalidHeader(f.Capture(), "scene manager-pointer read failure rejected");
  }
  {
    Fixture f; unsigned reads = 0;
    const auto failed = SceneLightSnapshotSource{}.Capture(Scene,
      [&](std::uint32_t address, void* out, std::size_t size) {
        ++reads;
        if(address == Scene + 0x34) { std::memcpy(out, &Manager, sizeof(Manager)); return false; }
        return f.headers.Read(address, out, size);
      }, [&](std::uint32_t address, void* out, std::size_t size) { return f.records.Read(address, out, size); });
    InvalidHeader(failed, "failed pointer read cannot publish valid-looking partial bytes");
    Check(reads == 1 && f.headers.reads == 0 && f.records.reads == 0, "failed pointer read stops all manager and record reads");
  }
  {
    Fixture f; f.headers.ranges[Manager].resize(0x23);
    InvalidHeader(f.Capture(), "partial manager header read rejected");
  }
  {
    Fixture f; Put32(f.headers.ranges[Manager], 0x10, 0);
    InvalidPartial(f.Capture(), 2, 0, "null head preserves declared count but no records");
    Check(f.records.reads == 0, "null pointer never reaches record reader");
  }
  {
    Fixture f; Put32(f.records.ranges[First], 0xbc, 0);
    InvalidPartial(f.Capture(), 2, 1, "premature null next retains only valid prefix");
  }
  {
    Fixture f; Put32(f.headers.ranges[Manager], 0x18, 3); Put32(f.records.ranges[Second], 0xbc, First);
    InvalidPartial(f.Capture(), 3, 2, "cycle rejected while preserving unique prefix");
    Check(f.records.reads == 2, "cycle rejected before rereading an already visited node");
  }
  {
    Fixture f; Put32(f.records.ranges[First], 0xb8, Second);
    InvalidPartial(f.Capture(), 2, 0, "head previous link must be null");
  }
  {
    Fixture f; Put32(f.records.ranges[Second], 0xb8, 0);
    InvalidPartial(f.Capture(), 2, 1, "broken previous link excludes malformed record");
  }
  {
    Fixture f; Put32(f.headers.ranges[Manager], 0x14, First);
    InvalidPartial(f.Capture(), 2, 2, "final tail mismatch keeps already read records");
  }
  {
    Fixture f; Put32(f.headers.ranges[Manager], 0x18, 1);
    InvalidPartial(f.Capture(), 1, 1, "declared short count rejects a remaining next link");
  }
  {
    Fixture f; Put32(f.headers.ranges[Manager], 0x18, 3);
    InvalidPartial(f.Capture(), 3, 2, "declared long count rejects early list end");
  }
  {
    Fixture f; Put32(f.records.ranges[Second], 0, 0x006f0c89);
    InvalidPartial(f.Capture(), 2, 1, "different light vtable rejects record without discarding valid prefix");
  }
  {
    Fixture f; Put32(f.records.ranges[Second], 0x3c, 0x1100);
    InvalidPartial(f.Capture(), 2, 1, "light ownership mismatch rejects record");
  }
  {
    Fixture f; f.records.ranges.erase(Second);
    InvalidPartial(f.Capture(), 2, 1, "unreadable light preserves declared count and valid prefix");
  }
  {
    Fixture f; f.records.ranges[First].resize(0x157);
    InvalidPartial(f.Capture(), 2, 0, "partial light read cannot publish a record");
  }
  {
    Fixture f;
    Put32(f.headers.ranges[Manager], 0x10, 0); Put32(f.headers.ranges[Manager], 0x14, 0);
    Put32(f.headers.ranges[Manager], 0x18, 0);
    const auto empty = f.Capture();
    Check(empty.valid && empty.scene == Scene && empty.manager == Manager && empty.count == 0 && empty.lights.empty(),
      "empty registry with null head and tail is valid");
    Check(f.records.reads == 0, "empty registry does not read any record");
    Put32(f.headers.ranges[Manager], 0x10, First);
    InvalidPartial(f.Capture(), 0, 0, "empty declared count with nonnull head is invalid");
    Put32(f.headers.ranges[Manager], 0x10, 0); Put32(f.headers.ranges[Manager], 0x14, Second);
    InvalidPartial(f.Capture(), 0, 0, "empty declared count with nonnull tail is invalid");
  }
  {
    Fixture f; bool propagated = false;throwRecordRead=true;
    try {
      SceneLightSnapshotSource source;
      source.Capture(Scene,
        [&](std::uint32_t address, void* out, std::size_t size) { return f.headers.Read(address, out, size); },
        [](std::uint32_t, void*, std::size_t) -> bool { if(throwRecordRead)throw std::bad_alloc();return false; });
    } catch (const std::bad_alloc&) { propagated = true; }
    Check(propagated, "reader allocation failure propagates to owning caller");
  }

  std::printf("{\"status\":\"PASS\",\"checks\":%u,\"gpu\":false,\"gameExecuted\":false}\n", checks);
  return 0;
}
