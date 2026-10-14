// Own passive-observation CPU fixture. No native hooks/instructions, game,
// graphics API or RT producers. Fake before/after bytes are independent inputs.
#include "winx_light_world_observer.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <new>
#include <type_traits>

static unsigned checks, allocations, liveAllocations;
static volatile bool denyAllocations, throwPredicate;
static volatile long failAfter = -1;
void* operator new(std::size_t size) {
  if (denyAllocations || failAfter == 0) throw std::bad_alloc();
  if (failAfter > 0) --failAfter;
  void* memory = std::malloc(size ? size : 1);
  if (!memory) throw std::bad_alloc();
  ++allocations; ++liveAllocations;
  return memory;
}
void operator delete(void* memory) noexcept {
  if (memory) { --liveAllocations; std::free(memory); }
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete[](void* memory) noexcept { ::operator delete(memory); }
void operator delete(void* memory, std::size_t) noexcept { ::operator delete(memory); }
void operator delete[](void* memory, std::size_t) noexcept { ::operator delete(memory); }

using Observer = winx_remix::LightWorldObserver;
using Status = Observer::LookupStatus;
static constexpr std::uint32_t Scene = 0x20000, Root = 0x21000;
static void Check(bool condition, const char* message) {
  ++checks;
  if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
static void Put(Observer::Record& record, std::size_t offset, std::uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) record.raw[offset + i] = static_cast<unsigned char>(value >> (i * 8));
}
static Observer::Record Record(std::uint32_t address = 0x30000, std::uint32_t scene = Scene) {
  Observer::Record record{address, {}};
  std::memset(record.raw, 0x55, sizeof(record.raw));
  Put(record, 0, 0x6f0c88); Put(record, 0x3c, scene); Put(record, 0xb0, 0x108);
  return record;
}
static Observer::Record After(Observer::Record record, unsigned char marker = 0xaa) {
  Put(record, 0xb0, 0x100); record.raw[0xf0] = marker;
  return record;
}
static bool SameRecord(const Observer::Record& a, const Observer::Record& b) {
  return a.address == b.address && !std::memcmp(a.raw, b.raw, sizeof(a.raw));
}
static bool SameSample(const Observer::Sample& a, const Observer::Sample& b) {
  return SameRecord(a.before, b.before) && SameRecord(a.after, b.after) && a.scene == b.scene &&
    a.root == b.root && a.inheritedFlags == b.inheritedFlags && a.call == b.call &&
    a.calls == b.calls && a.completed == b.completed;
}
static Observer::Sample Sentinel() {
  Observer::Sample output{};
  output.before = Record(0xdead0); output.after = After(output.before, 0x5a);
  output.scene = 9; output.root = 8; output.inheritedFlags = 7; output.call = 6; output.calls = 5;
  return output;
}
static bool SceneCurrent(std::uint32_t scene, std::uint32_t root) noexcept { return scene == Scene && root == Root; }

int main() {
  static_assert(!std::is_copy_constructible_v<Observer> && !std::is_move_constructible_v<Observer>);
  for (const long fail : {0L, 1L}) {
    const auto before = liveAllocations;
    bool failed = false; failAfter = fail;
    try { Observer incomplete; } catch (const std::bad_alloc&) { failed = true; }
    failAfter = -1;
    Check(failed && liveAllocations == before, "both reserve-failure boundaries leave no owned allocation");
  }

  Observer observer;
  const auto reservedAllocations = allocations;
  // Every method below must use constructor-reserved banks. Injected callback
  // failures use a trivial exception; fixtures allocate no containers here.
  denyAllocations = true;
  Observer::Context context{0x10000, 0, 7, 11, 12, 13, 14};
  auto before = Record(), after = After(before);
  Observer::Sample output = Sentinel(); const auto untouched = output;
  Check(!observer.Count() && !observer.CallCount() && !observer.Collecting() && !observer.Sealed(), "fresh observer");
  Check(observer.Lookup(context, after, output) == Status::Unqualified && SameSample(output, untouched),
    "unqualified lookup leaves output untouched");
  Check(observer.Begin(context), "begin qualified caller observation");
  const auto firstTicket = observer.BeginCall(context, Scene, Root, before, 9);
  Check(firstTicket.valid && observer.Count() == 1 && observer.CallCount() == 1, "record before original call");
  Check(observer.CompleteCall(firstTicket, context, after), "completion owns caller-supplied post-callback bytes");
  observer.CancelCall(firstTicket);
  Check(observer.Collecting(), "cancellation of a completed ticket is harmless before seal");
  Check(observer.Seal(context, SceneCurrent) && observer.Sealed() && !observer.Collecting(), "publish completed scene/root observation");
  Check(observer.Lookup(context, after, output) == Status::Current && output.completed && output.calls == 1 &&
    output.call == firstTicket.call && output.scene == Scene && output.root == Root && output.inheritedFlags == 9 &&
    SameRecord(output.before, before) && SameRecord(output.after, after), "current lookup returns owned before/after and caller metadata");
  const auto owned = output;
  before.raw[0xc4] = 1; after.raw[0xd8] = 2;
  Check(SameSample(owned, output), "mutating caller records does not change owned sample");
  output = Sentinel();
  Check(observer.Lookup(context, after, output) == Status::Changed && SameSample(output, untouched), "changed post bytes never write output");
  Check(observer.Lookup(context, owned.after, output) == Status::Current && SameSample(output, owned),
    "observer bank retains owned before/after bytes after caller buffers change");
  auto other = owned.after; other.address += 0x200;
  output = Sentinel();
  Check(observer.Lookup(context, other, output) == Status::NotObserved && SameSample(output, untouched),
    "unobserved address gives no missing-update conclusion and leaves output untouched");
  other = owned.after; other.raw[0] ^= 1;
  Check(observer.Lookup(context, other, output) == Status::Changed, "structural byte mutation cannot match a published post record");
  for (unsigned field = 0; field < 7; ++field) {
    auto changed = context;
    switch (field) {
    case 0: ++changed.manager; break;
    case 1: ++changed.frame; break;
    case 2: ++changed.ownerThread; break;
    case 3: ++changed.sequence; break;
    case 4: ++changed.mutation; break;
    case 5: ++changed.deviceEpoch; break;
    case 6: ++changed.observerEpoch; break;
    }
    output = Sentinel();
    Check(observer.Lookup(changed, owned.after, output) == Status::Unqualified && SameSample(output, untouched),
      "every context identity boundary excludes prior publication");
  }

  ++context.frame; before = Record(); after = After(before);
  Check(observer.Begin(context) && observer.Lookup(context, after, output) == Status::Unqualified, "new begin revokes prior publication immediately");
  const auto newer = observer.BeginCall(context, Scene, Root, before, 0);
  observer.CancelCall(firstTicket);
  Check(observer.Collecting() && observer.Count() == 1 && observer.CallCount() == 1,
    "old-ticket cancellation cannot revoke a newer active observation");
  Check(!observer.CompleteCall(firstTicket, context, after) && observer.Collecting(), "old ticket cannot poison a newer batch");
  Check(observer.CompleteCall(newer, context, after) && observer.Seal(context, SceneCurrent), "newer ticket remains completable");

  Check(observer.Begin(context), "begin repeated-call observation");
  const auto repeat1 = observer.BeginCall(context, Scene, Root, before, 1);
  Check(observer.CompleteCall(repeat1, context, after), "first repeated call completes");
  const auto before2 = after, after2 = After(before2, 0xbb);
  const auto repeat2 = observer.BeginCall(context, Scene, Root, before2, 8);
  observer.CancelCall(repeat1);
  Check(repeat2.valid && !observer.CompleteCall(repeat1, context, after) && observer.CompleteCall(repeat2, context, after2),
    "duplicate completed key overwrites latest call while obsolete call ticket is harmless");
  Check(observer.Seal(context, SceneCurrent) && observer.Count() == 1 && observer.CallCount() == 2 &&
    observer.Lookup(context, after2, output) == Status::Current && output.calls == 2 && output.call == repeat2.call &&
    output.inheritedFlags == 8 && SameRecord(output.before, before2), "repeat publication keeps only latest bytes and cumulative calls");

  Check(observer.Begin(context), "begin same-key reentry case");
  const auto active = observer.BeginCall(context, Scene, Root, before, 0);
  Check(!observer.BeginCall(context, Scene, Root, before, 0).valid && !observer.Collecting() && !observer.Sealed(),
    "same-key nested call revokes whole observation");
  Check(!observer.CompleteCall(active, context, after) && !observer.Seal(context, SceneCurrent), "invalidated outer call cannot publish");

  Check(observer.Begin(context), "begin different nested lights");
  const auto secondBefore = Record(0x30200), secondAfter = After(secondBefore, 0xcc);
  const auto a = observer.BeginCall(context, Scene, Root, before, 1);
  const auto b = observer.BeginCall(context, Scene, Root, secondBefore, 2);
  Check(a.valid && b.valid && observer.CompleteCall(b, context, secondAfter) && observer.CompleteCall(a, context, after),
    "different nested light calls complete in reverse order");
  unsigned sceneChecks = 0;
  Check(observer.Seal(context, [&](std::uint32_t scene, std::uint32_t root) noexcept { ++sceneChecks; return SceneCurrent(scene, root); }) &&
    sceneChecks == 1 && observer.Count() == 2, "one fresh predicate per distinct scene/root publishes nested lights");
  Check(observer.Lookup(context, secondAfter, output) == Status::Current && output.calls == 1 && output.inheritedFlags == 2,
    "second nested light retains independent sample");

  Check(observer.Begin(context), "begin multiple scenes in one manager batch");
  const auto thirdBefore = Record(0x30400, Scene + 0x1000), thirdAfter = After(thirdBefore, 0xdd);
  const auto sceneA = observer.BeginCall(context, Scene, Root, before, 0);
  const auto sceneB = observer.BeginCall(context, Scene + 0x1000, Root + 0x1000, thirdBefore, 0);
  Check(observer.CompleteCall(sceneA, context, after) && observer.CompleteCall(sceneB, context, thirdAfter),
    "one qualified manager context can own different scene/root observations");
  sceneChecks = 0;
  Check(observer.Seal(context, [&](std::uint32_t scene, std::uint32_t root) noexcept {
    ++sceneChecks; return SceneCurrent(scene, root) || (scene == Scene + 0x1000 && root == Root + 0x1000);
  }) && sceneChecks == 2 && observer.Lookup(context, thirdAfter, output) == Status::Current && output.scene == Scene + 0x1000,
    "publication validates every distinct scene/root rather than only first sample");
  Check(observer.Begin(context), "begin second-scene failure case");
  const auto goodScene = observer.BeginCall(context, Scene, Root, before, 0);
  const auto badScene = observer.BeginCall(context, Scene + 0x1000, Root + 0x1000, thirdBefore, 0);
  Check(observer.CompleteCall(goodScene, context, after) && observer.CompleteCall(badScene, context, thirdAfter) &&
    !observer.Seal(context, SceneCurrent) && !observer.Sealed() && !observer.Count(),
    "one stale scene/root excludes the entire manager observation");

  Check(observer.Begin(context), "begin disabled/inactive passive record");
  auto disabled = before; disabled.raw[0xed] = 0; Put(disabled, 0xb0, 0); Put(disabled, 0xc0, 3);
  const auto disabledCall = observer.BeginCall(context, Scene, Root, disabled, 8);
  Check(disabledCall.valid && observer.CompleteCall(disabledCall, context, disabled) && observer.Seal(context, SceneCurrent) &&
    observer.Lookup(context, disabled, output) == Status::Current && SameRecord(output.before, disabled) && SameRecord(output.after, disabled),
    "passive observer retains disabled/inactive/ambient call bytes without eligibility or payload policy");

  Check(observer.Begin(context), "begin cancel boundary");
  const auto cancel = observer.BeginCall(context, Scene, Root, before, 0);
  observer.CancelCall(cancel);
  Check(!observer.Count() && !observer.Collecting() && !observer.CompleteCall(cancel, context, after), "cancel revokes complete observation");
  Check(observer.Begin(context), "begin unfinished call case");
  const auto unfinished = observer.BeginCall(context, Scene, Root, before, 0);
  Check(unfinished.valid && !observer.Seal(context, SceneCurrent) && !observer.Collecting(), "in-flight calls cannot seal partial evidence");

  for (unsigned problem = 0; problem < 4; ++problem) {
    Check(observer.Begin(context), "begin invalid post case");
    const auto ticket = observer.BeginCall(context, Scene, Root, before, 0);
    auto bad = after; auto changed = context;
    if (problem == 0) ++bad.address;
    if (problem == 1) Put(bad, 0x3c, Scene + 1);
    if (problem == 2) Put(bad, 0, 0);
    if (problem == 3) ++changed.deviceEpoch;
    Check(!observer.CompleteCall(ticket, changed, bad) && !observer.Collecting() && !observer.Sealed(),
      "changed post identity or reset context revokes whole observation");
  }
  for (unsigned problem = 0; problem < 5; ++problem) {
    Check(observer.Begin(context), "begin invalid entry case");
    auto bad = before; auto scene = Scene, root = Root;
    if (problem == 0) bad.address = 0;
    if (problem == 1) scene = 0;
    if (problem == 2) root = 0;
    if (problem == 3) Put(bad, 0x3c, Scene + 1);
    if (problem == 4) Put(bad, 0, 0);
    Check(!observer.BeginCall(context, scene, root, bad, 0).valid && !observer.Collecting(), "invalid entry cannot create partial sample");
  }
  Check(observer.Begin(context), "begin mismatched entry context");
  auto changed = context; ++changed.mutation;
  Check(!observer.BeginCall(changed, Scene, Root, before, 0).valid && !observer.Collecting(), "topology mutation during entry revokes observation");
  Check(observer.Begin(context), "begin nested batch");
  Check(!observer.Begin(context) && !observer.Collecting(), "nested begin revokes both affected batches");

  const auto completeOne = [&]() {
    Check(observer.Begin(context), "begin publication predicate case");
    const auto ticket = observer.BeginCall(context, Scene, Root, before, 0);
    Check(ticket.valid && observer.CompleteCall(ticket, context, after), "prepare complete predicate input");
  };
  completeOne(); changed = context; ++changed.frame;
  Check(!observer.Seal(changed, SceneCurrent) && !observer.Sealed(), "frame changed before seal cannot publish");
  completeOne();
  Check(!observer.Seal(context, [](std::uint32_t, std::uint32_t) noexcept { return false; }) && !observer.Sealed(),
    "failed scene/root predicate revokes owned evidence");
  completeOne();
  struct InjectedFailure {};
  bool threw = false; throwPredicate = true;
  try { observer.Seal(context, [](std::uint32_t, std::uint32_t) -> bool {
    if (throwPredicate) throw InjectedFailure{}; return true;
  }); } catch (const InjectedFailure&) { threw = true; }
  Check(threw && !observer.Collecting() && !observer.Sealed() && !observer.Count(), "predicate exception revokes observation before propagating");
  completeOne();
  Check(!observer.Seal(context, [&](std::uint32_t, std::uint32_t) { observer.Invalidate(); return true; }) && !observer.Sealed(),
    "predicate invalidation cannot publish old revision");
  completeOne();
  Check(!observer.Seal(context, [&](std::uint32_t, std::uint32_t) {
    return observer.Seal(context, SceneCurrent);
  }) && !observer.Sealed(), "recursive seal invalidates outer publication");
  completeOne();
  Check(!observer.Seal(context, [&](std::uint32_t, std::uint32_t) {
    return observer.BeginCall(context, Scene, Root, secondBefore, 0).valid;
  }) && !observer.Sealed(), "new call during publication cannot change bank behind predicate");
  completeOne();
  Check(!observer.Seal(context, [&](std::uint32_t, std::uint32_t) { ++context.observerEpoch; return true; }) && !observer.Sealed(),
    "predicate cannot mutate an aliased context and publish old evidence");
  completeOne();
  auto subsequent = context; ++subsequent.sequence;
  Check(!observer.Seal(context, [&](std::uint32_t, std::uint32_t) {
    observer.Invalidate(); return observer.Begin(subsequent);
  }) && observer.Collecting(), "failed outer seal does not revoke a later independently begun revision");
  const auto next = observer.BeginCall(subsequent, Scene, Root, before, 0);
  Check(observer.CompleteCall(next, subsequent, after) && observer.Seal(subsequent, SceneCurrent), "later revision survives old scope teardown");

  context = subsequent;
  Check(observer.Begin(context), "begin empty observation");
  sceneChecks = 0;
  Check(observer.Seal(context, [&](std::uint32_t, std::uint32_t) noexcept { ++sceneChecks; return false; }) && !observer.Count() && !sceneChecks,
    "empty normal batch publishes no scene-call or missing-update claim");
  Check(observer.Lookup(context, after, output) == Status::NotObserved, "empty sealed observation has no sample");

  Check(observer.Begin(context), "begin distinct-record capacity case");
  bool complete = true; Observer::Record last{};
  for (std::size_t index = 0; index < Observer::MaxSamples; ++index) {
    const auto record = Record(0x30000 + static_cast<std::uint32_t>(index) * 0x200);
    last = After(record);
    const auto ticket = observer.BeginCall(context, Scene, Root, record, 0);
    complete &= ticket.valid && observer.CompleteCall(ticket, context, last);
  }
  Check(complete && observer.Count() == 512 && observer.Seal(context, SceneCurrent) && observer.Lookup(context, last, output) == Status::Current,
    "fixed reserved bank accepts 512 completed distinct records without allocation");
  Check(observer.Begin(context), "begin distinct-record overflow case");
  for (std::size_t index = 0; index < Observer::MaxSamples; ++index) {
    const auto record = Record(0x30000 + static_cast<std::uint32_t>(index) * 0x200);
    const auto ticket = observer.BeginCall(context, Scene, Root, record, 0);
    complete &= observer.CompleteCall(ticket, context, After(record));
  }
  Check(complete && !observer.BeginCall(context, Scene, Root, Record(0x90000), 0).valid && !observer.Collecting() && !observer.Count(),
    "513th distinct record fails closed without growing bank");

  for (unsigned overflow = 0; overflow < 2; ++overflow) {
    Check(observer.Begin(context), "begin total-call budget case");
    complete = true;
    for (unsigned call = 0; call < Observer::MaxCalls; ++call) {
      const auto ticket = observer.BeginCall(context, Scene, Root, before, call);
      complete &= ticket.valid && observer.CompleteCall(ticket, context, after);
    }
    if (!overflow) Check(complete && observer.CallCount() == 4096 && observer.Seal(context, SceneCurrent) &&
      observer.Lookup(context, after, output) == Status::Current && output.calls == 4096 && output.inheritedFlags == 4095,
      "4096 duplicate completed calls retain bounded latest sample and exact call count");
    else Check(complete && !observer.BeginCall(context, Scene, Root, before, 0).valid && !observer.Sealed() && !observer.Collecting(),
      "4097th call revokes observation without allocation");
  }
  observer.Invalidate();
  Check(allocations == reservedAllocations, "all observer methods use constructor allocations only");
  denyAllocations = false;
  std::printf("{\"status\":\"PASS\",\"checks\":%u,\"gpu\":false,\"gameExecuted\":false,\"nativeInstructions\":false,\"freshRtQualified\":false}\n", checks);
  return 0;
}
