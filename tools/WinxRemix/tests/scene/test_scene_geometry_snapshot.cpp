// Own CPU structural/mutation fixture with literal numeric PC memory.
// No game instructions, camera inputs, mesh/world evaluation, or API/GPU calls.
#include "../../src/scene/winx_scene_geometry_snapshot.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <map>
#include <new>
#include <vector>

using winx_remix::SceneGeometrySnapshot;
using winx_remix::SceneGeometrySnapshotSource;
using Source = SceneGeometrySnapshot::OccurrenceSource;
static unsigned checks;
static bool failNextAllocation;

void* operator new(std::size_t size) {
  if (failNextAllocation) { failNextAllocation = false; throw std::bad_alloc(); }
  if (void* result = std::malloc(size ? size : 1)) return result;
  throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* value) noexcept { std::free(value); }
void operator delete[](void* value) noexcept { std::free(value); }
void operator delete(void* value, std::size_t) noexcept { std::free(value); }
void operator delete[](void* value, std::size_t) noexcept { std::free(value); }

static void Check(bool condition, const char* message) {
  ++checks;
  if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
static void Put32(std::vector<unsigned char>& bytes, std::size_t offset, std::uint32_t value) {
  // Independent little-endian encoder; no game classes/source field decoder.
  for (unsigned i = 0; i < 4; ++i) bytes[offset + i] = static_cast<unsigned char>(value >> (i * 8));
}

struct Memory {
  std::map<std::uint32_t, std::vector<unsigned char>> ranges;
  mutable unsigned reads = 0;
  std::uint32_t reject = 0;
  bool Read(std::uint32_t address, void* out, std::size_t size) const {
    ++reads;
    if (address == reject) return false;
    auto found = ranges.upper_bound(address);
    if (found == ranges.begin()) return false;
    --found;
    const std::size_t offset = std::size_t(address) - found->first;
    if (offset > found->second.size() || size > found->second.size() - offset) return false;
    std::memcpy(out, found->second.data() + offset, size);
    return true;
  }
};

static constexpr std::uint32_t Scene = 0x10000, System = 0x20000, Root = 0x30000,
  Child = 0x31000, Zone = 0x40000, StaticA = 0x50000, StaticB = 0x51000,
  Partition = 0x52000, Dynamic = 0x60000, Custom = 0x61000;

struct Fixture {
  Memory memory;
  std::uint64_t mutation = 7;
  void Put(std::uint32_t object, unsigned offset, std::uint32_t value) {
    Put32(memory.ranges.at(object), offset, value);
  }
  void Values(std::uint32_t address, const std::vector<std::uint32_t>& values) {
    auto& bytes = memory.ranges[address]; bytes.resize(values.size() * 4);
    for (std::size_t i = 0; i < values.size(); ++i) Put32(bytes, i * 4, values[i]);
  }
  void VectorHeader(std::uint32_t object, unsigned offset, std::uint32_t data, unsigned count) {
    Put(object, offset + 4, data);
    Put(object, offset + 8, data + count * 4);
    Put(object, offset + 12, data + count * 4);
  }
  void Vector(std::uint32_t object, unsigned offset, std::uint32_t data,
              const std::vector<std::uint32_t>& values) {
    Values(data, values);
    VectorHeader(object, offset, data, static_cast<unsigned>(values.size()));
  }
  void Node(std::uint32_t address, std::uint32_t table = 0x6dcb08) {
    memory.ranges[address] = std::vector<unsigned char>(0x84);
    Put(address, 0, table); Put(address, 0x80, Scene);
  }
  void Support(std::uint32_t address, bool partition) {
    memory.ranges[address] = std::vector<unsigned char>(0x8c);
    const unsigned offset = partition ? 0x10 : 0x14;
    Put(address, 0, partition ? 0x6f4540 : 0x6e65e8);
    Put(address, offset, partition ? 0x6f4528 : 0x6e6604);
    Put(address, offset + 0x70, address); Put(address, 0x88, Scene);
  }
  void RenderNode(std::uint32_t address, std::uint32_t supportTable) {
    memory.ranges[address] = std::vector<unsigned char>(0x130);
    Put(address, 0, 0x12345678); // Outer derived table is not the support ABI.
    Put(address, 0x3c, Scene); Put(address, 0xb4, supportTable); Put(address, 0x124, address);
  }
  Fixture() {
    memory.ranges[Scene].resize(0x3c); Put(Scene, 0x38, System);
    memory.ranges[System].resize(0x1d8); Put(System, 0x1d4, Root);
    Node(Root); Node(Child, 0x6e4420);
    memory.ranges[Zone].resize(0xc4); Put(Zone, 0, 0x6ebacc);
    Support(StaticA, false); Support(StaticB, false); Support(Partition, true);
    RenderNode(Dynamic, 0x6dcadc); RenderNode(Custom, 0x76543210);
    Vector(Root, 0x30, 0x70000, {StaticB, 0, StaticA, StaticB});
    Vector(Root, 0x20, 0x71000, {Dynamic, 0, Dynamic, Custom, Custom});
    Vector(Child, 0x30, 0x72000, {StaticA, 0, StaticA});
    Vector(Child, 0x20, 0x73000, {Dynamic});
    Values(0x74000, {Child, 0, Root, Child});
    Put(Root, 0x58, 0x74000); Put(Root, 0x5c, 4);
    Put(Root, 0x60, Zone); Vector(Zone, 0xb4, 0x75000, {Root, Child, 0});
    Put(Child, 0x60, Zone); Put(Child, 0x78, Partition);
  }
  SceneGeometrySnapshot Capture() const {
    return SceneGeometrySnapshotSource{}.Capture(Scene,
      [&](std::uint32_t address, void* out, std::size_t size) { return memory.Read(address, out, size); },
      [&] { return mutation; });
  }
};

static void Occurrence(const SceneGeometrySnapshot& snapshot, std::size_t index,
                       std::uint32_t node, std::uint32_t object, std::uint32_t support,
                       Source source, std::uint32_t ordinal, const char* message) {
  Check(index < snapshot.occurrences.size(), "expected occurrence exists");
  const auto& found = snapshot.occurrences[index];
  Check(found.node == node && found.object == object && found.support == support &&
        found.source == source && found.ordinal == ordinal, message);
}

int main() {
  Fixture f;
  const auto originalBytes = f.memory.ranges;
  const auto snapshot = f.Capture();
  Check(snapshot.valid && snapshot.scene == Scene && snapshot.system == System && snapshot.root == Root &&
    snapshot.mutationSerial == 7, "loaded numeric ownership and starting mutation retained");
  Check(snapshot.nodes == 2 && snapshot.dynamic == 1 && snapshot.unsupportedDynamic == 1,
    "cycles/repeated child and zone references visit each node once; unknown dynamic counted once");
  Check(snapshot.supports == std::vector<std::uint32_t>{StaticA + 0x14, StaticB + 0x14, Partition + 0x10, Dynamic + 0xb4},
    "visibility supports are sorted unique adjusted addresses");
  Check(snapshot.occurrencesComplete && snapshot.occurrences.size() == 9,
    "distinct physical slots remain repeated across native sources");
  Occurrence(snapshot, 0, Root, StaticB, StaticB + 0x14, Source::StaticVector, 0, "root first static slot");
  Occurrence(snapshot, 1, Root, StaticA, StaticA + 0x14, Source::StaticVector, 2, "null static slot does not renumber");
  Occurrence(snapshot, 2, Root, StaticB, StaticB + 0x14, Source::StaticVector, 3, "repeated static object retains original ordinal");
  Occurrence(snapshot, 3, Root, Dynamic, Dynamic + 0xb4, Source::RenderNodeVector, 0, "supported derived render node");
  Occurrence(snapshot, 4, Root, Dynamic, Dynamic + 0xb4, Source::RenderNodeVector, 2, "null dynamic slot does not renumber");
  Occurrence(snapshot, 5, Child, Partition, Partition + 0x10, Source::PartitionPayload, 0, "child payload source retained");
  Occurrence(snapshot, 6, Child, StaticA, StaticA + 0x14, Source::StaticVector, 0, "cross-node repeated static source");
  Occurrence(snapshot, 7, Child, StaticA, StaticA + 0x14, Source::StaticVector, 2, "cross-node null gap retains ordinal");
  Occurrence(snapshot, 8, Child, Dynamic, Dynamic + 0xb4, Source::RenderNodeVector, 0, "cross-node repeated dynamic source");
  Check(f.memory.ranges == originalBytes, "capture does not write fake native bytes");

  const auto reads = f.memory.reads;
  f.Vector(Root, 0x30, 0x70000, {StaticA});
  const auto next = f.Capture();
  Check(next.valid && next.supports == std::vector<std::uint32_t>{StaticA + 0x14, Partition + 0x10, Dynamic + 0xb4} &&
    next.occurrences.size() == 7, "next capture observes changed membership");
  Check(snapshot.supports.size() == 4 && snapshot.occurrences.size() == 9 &&
    snapshot.occurrences[0].object == StaticB, "previous snapshot owns its numerical membership and provenance");
  Check(f.memory.reads > reads, "new capture uses fresh supplied reads");

  {
    Fixture x; unsigned mutationReads = 0;
    const auto changed = SceneGeometrySnapshotSource{}.Capture(Scene,
      [&](std::uint32_t address, void* out, std::size_t size) { return x.memory.Read(address, out, size); },
      [&] { return ++mutationReads == 1 ? std::uint64_t(10) : std::uint64_t(11); });
    Check(!changed.valid && changed.mutationSerial == 10 && changed.supports == snapshot.supports &&
      changed.nodes == 2 && changed.occurrences.size() == 9 && mutationReads == 2,
      "mutation invalidates full observation while retaining owned result and initial serial");
  }
  {
    Fixture x; x.memory.reject = Root;
    const auto invalid = x.Capture();
    Check(!invalid.valid && invalid.scene == Scene && invalid.system == System && invalid.root == Root &&
      invalid.nodes == 0 && invalid.supports.empty() && invalid.occurrences.empty(), "unreadable node rejected");
  }
  {
    Fixture x; x.memory.ranges.erase(Scene);
    const auto invalid = x.Capture();
    Check(!invalid.valid && invalid.scene == Scene && !invalid.system && !invalid.root,
      "missing scene pointer returns invalid zero ownership");
    x.memory.ranges[Scene].resize(0x3c); x.Put(Scene, 0x38, System); x.Put(System, 0x1d4, 0);
    Check(!x.Capture().valid, "null root is invalid rather than empty graph");
  }
  {
    Fixture x; x.Put(StaticA, 0, 0x6e65e9);
    const auto invalid = x.Capture();
    Check(!invalid.valid && invalid.nodes == 0 && invalid.supports.empty() && invalid.occurrences.size() == 1 &&
      invalid.occurrences[0].object == StaticB, "unknown static ABI rejects while retaining optional valid prefix");
  }
  {
    Fixture x; x.Put(Partition, 0x10, 0x6f4529);
    const auto invalid = x.Capture();
    Check(!invalid.valid && invalid.occurrences.size() == 5, "unknown partition support rejects child payload");
  }
  {
    Fixture x; x.Put(Dynamic, 0xb4, 0x76543210);
    const auto unknown = x.Capture();
    Check(unknown.valid && unknown.dynamic == 0 && unknown.unsupportedDynamic == 2 &&
      unknown.supports.size() == 3 && unknown.occurrences.size() == 6,
      "unknown dynamic ABI is counted and omitted without rejecting static membership");
    x.Put(Dynamic, 0x124, 0);
    Check(!x.Capture().valid, "unknown dynamic must still have the expected complete-object backlink");
  }
  {
    Fixture x; x.Put(Dynamic, 0x3c, Scene + 4);
    Check(!x.Capture().valid, "dynamic scene ownership mismatch rejected");
    x.Put(Dynamic, 0x3c, Scene); x.Put(Child, 0x80, Scene + 4);
    Check(!x.Capture().valid, "partition node scene ownership mismatch rejected");
    x.Put(Child, 0x80, Scene); x.Put(Child, 0, 0x76543210);
    Check(!x.Capture().valid, "unknown partition node table rejected");
    x.Put(Child, 0, 0x6eba30);
    Check(x.Capture().valid, "third existing partition node ABI accepted");
  }
  {
    Fixture x; x.Put(Root, 0x5c, 9);
    Check(!x.Capture().valid, "child count above eight rejected");
    x.Put(Root, 0x5c, 1); x.Put(Root, 0x58, 0);
    Check(!x.Capture().valid, "nonnull child count needs child storage");
    x.Put(Root, 0x58, 0x74000); x.memory.reject = 0x74000;
    Check(!x.Capture().valid, "child pointer read failure rejected");
    x.memory.reject = 0; x.Put(Zone, 0, 0x6ebacd);
    Check(!x.Capture().valid, "unknown zone ABI rejected");
  }
  {
    Fixture x;
    x.Node(Root); // Empty vectors, no children/payload/zone.
    const auto empty = x.Capture();
    Check(empty.valid && empty.nodes == 1 && empty.supports.empty() && empty.occurrences.empty() &&
      empty.occurrencesComplete, "loaded empty graph is valid");
    std::vector<std::uint32_t> values{42};
    const auto read = [&](std::uint32_t address, void* out, std::size_t size) { return x.memory.Read(address, out, size); };
    x.VectorHeader(Root, 0x30, 0x765400, 0);
    const unsigned before = x.memory.reads;
    Check(SceneGeometrySnapshotSource::ReadVector(Root, 0x30, values, read) && values.empty() &&
      x.memory.reads == before + 1, "empty nonnull vector needs only its header read");
    x.Put(Root, 0x34, 100); x.Put(Root, 0x38, 96); x.Put(Root, 0x3c, 100);
    values = {42};
    Check(!SceneGeometrySnapshotSource::ReadVector(Root, 0x30, values, read) && values == std::vector<std::uint32_t>{42},
      "descending vector rejects without changing caller values");
    x.Put(Root, 0x34, 0); x.Put(Root, 0x38, 8); x.Put(Root, 0x3c, 4);
    Check(!SceneGeometrySnapshotSource::ReadVector(Root, 0x30, values, read), "vector end above capacity rejected");
    x.Put(Root, 0x38, 3); x.Put(Root, 0x3c, 4);
    Check(!SceneGeometrySnapshotSource::ReadVector(Root, 0x30, values, read), "partial vector element rejected");
    x.Put(Root, 0x38, 4097 * 4); x.Put(Root, 0x3c, 4097 * 4);
    Check(!SceneGeometrySnapshotSource::ReadVector(Root, 0x30, values, read), "vector above 4096 entries rejected");
    x.VectorHeader(Root, 0x30, 0x765400, 1);
    Check(!SceneGeometrySnapshotSource::ReadVector(Root, 0x30, values, read), "unreadable vector elements rejected");
    x.memory.reject = Root + 0x34; values = {42};
    Check(!SceneGeometrySnapshotSource::ReadVector(Root, 0x30, values, read) && values == std::vector<std::uint32_t>{42},
      "unreadable vector header leaves existing result unchanged");
  }
  {
    Fixture x; const auto read = [&](std::uint32_t address, void* out, std::size_t size) { return x.memory.Read(address, out, size); };
    std::uint32_t adjusted = 0xdeadbeef;
    Check(SceneGeometrySnapshotSource::ReadSupport(StaticA, Scene, false, adjusted, read) && adjusted == StaticA + 0x14,
      "public static support helper returns adjusted numerical address");
    Check(SceneGeometrySnapshotSource::ReadSupport(Partition, Scene, true, adjusted, read) && adjusted == Partition + 0x10,
      "public partition support helper returns adjusted numerical address");
    adjusted = 0xdeadbeef; x.Put(StaticA, 0x84, StaticB);
    Check(!SceneGeometrySnapshotSource::ReadSupport(StaticA, Scene, false, adjusted, read) && adjusted == 0xdeadbeef,
      "support backlink mismatch leaves output unchanged");
    x.Put(StaticA, 0x84, StaticA); x.Put(StaticA, 0x88, Scene + 4);
    Check(!SceneGeometrySnapshotSource::ReadSupport(StaticA, Scene, false, adjusted, read), "support scene mismatch rejected");
    Check(!SceneGeometrySnapshotSource::ReadSupport(0, Scene, false, adjusted, read), "null support rejected");
  }
  {
    Fixture x; x.Node(Root); x.Put(Root, 0x60, Zone);
    const std::vector<std::uint32_t> repeated(4096, StaticA);
    x.Values(0x200000, repeated); x.VectorHeader(Root, 0x30, 0x200000, 4096);
    std::vector<std::uint32_t> nodes{Root};
    for (unsigned i = 0; i < 16; ++i) {
      const std::uint32_t node = 0x80000 + i * 0x1000;
      x.Node(node); x.VectorHeader(node, 0x30, 0x200000, 4096); nodes.push_back(node);
    }
    x.Vector(Zone, 0xb4, 0x75000, nodes);
    const auto capped = x.Capture();
    Check(capped.valid && capped.nodes == 17 && capped.supports == std::vector<std::uint32_t>{StaticA + 0x14} &&
      !capped.occurrencesComplete && capped.occurrences.size() == 65536,
      "provenance overflow degrades only occurrences while loaded support membership completes");
  }
  {
    SceneGeometrySnapshot optional; optional.supports = {StaticA + 0x14};
    failNextAllocation = true;
    optional.Occurrence(Root, StaticA, StaticA + 0x14, Source::StaticVector, 0);
    Check(!failNextAllocation && !optional.occurrencesComplete && optional.occurrences.empty() &&
      optional.supports == std::vector<std::uint32_t>{StaticA + 0x14}, "optional provenance allocation failure retains support membership");
    optional.Occurrence(Root, StaticB, StaticB + 0x14, Source::StaticVector, 1);
    Check(optional.occurrences.empty(), "degraded provenance is not partially resumed");
  }
  {
    Fixture x; bool propagated = false;
    try {
      SceneGeometrySnapshotSource{}.Capture(Scene,
        [&](std::uint32_t address, void* out, std::size_t size) {
          if (address == Root) throw std::bad_alloc();
          return x.memory.Read(address, out, size);
        }, [&] { return x.mutation; });
    } catch (const std::bad_alloc&) { propagated = true; }
    Check(propagated, "essential reader allocation failure propagates to owning caller");
  }

  std::printf("{\"status\":\"PASS\",\"checks\":%u,\"gameExecuted\":false,\"gpu\":false}\n", checks);
  return 0;
}
