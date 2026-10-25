#pragma once
// Own passive, bounded observation of qualified native light-world calls.
// Caller serializes use and qualifies the original root/scene/thread before
// entry. Samples own bytes but do not extend native object lifetime, establish
// fresh RT inputs, or prove that NotObserved means a missing native update.
#include "../scene/winx_scene_light_snapshot.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace winx_remix {
class LightWorldObserver final {
public:
  using Record = SceneLightSnapshot::Record;
  static constexpr std::size_t MaxSamples = 512;
  static constexpr unsigned MaxCalls = 4096;
  struct Context {
    std::uint32_t manager = 0, frame = 0, ownerThread = 0;
    std::uint64_t sequence = 0, mutation = 0, deviceEpoch = 0, observerEpoch = 0;
    bool operator==(const Context& other) const noexcept {
      return manager == other.manager && frame == other.frame && ownerThread == other.ownerThread &&
        sequence == other.sequence && mutation == other.mutation && deviceEpoch == other.deviceEpoch &&
        observerEpoch == other.observerEpoch;
    }
  };
  struct Sample {
    Record before{}, after{};
    std::uint32_t scene = 0, root = 0, inheritedFlags = 0;
    std::uint64_t call = 0;
    unsigned calls = 0;
    bool completed = false;
  };
  struct Ticket {
    std::uint64_t revision = 0, call = 0;
    std::size_t index = 0;
    bool valid = false;
  };
  enum class LookupStatus { Current, Changed, NotObserved, Unqualified };

  // Reserve both banks once. A construction failure propagates to the caller;
  // every later operation uses this bounded storage without allocation.
  LightWorldObserver() { pending_.reserve(MaxSamples); published_.reserve(MaxSamples); }
  LightWorldObserver(const LightWorldObserver&) = delete;
  LightWorldObserver& operator=(const LightWorldObserver&) = delete;
  LightWorldObserver(LightWorldObserver&&) = delete;
  LightWorldObserver& operator=(LightWorldObserver&&) = delete;

  bool Begin(const Context& context) noexcept {
    if (collecting_) { Invalidate(); return false; }
    Invalidate();
    context_ = context;
    collecting_ = true;
    return true;
  }
  Ticket BeginCall(const Context& context, std::uint32_t scene, std::uint32_t root,
                   const Record& record, std::uint32_t inheritedFlags) noexcept {
    if (!collecting_) return {};
    if (sealing_ || !(context_ == context) || !root || !ValidRecord(record, scene) || totalCalls_ == MaxCalls) {
      Invalidate(); return {};
    }
    // Scene/address is the lookup key. Root is separately requalified at Seal;
    // a repeated completed call retains only its latest before/after bytes.
    std::size_t index = 0;
    while (index < pending_.size() &&
      !(pending_[index].scene == scene && pending_[index].before.address == record.address)) ++index;
    unsigned calls = 1;
    if (index < pending_.size()) {
      if (!pending_[index].completed) { Invalidate(); return {}; }
      calls = pending_[index].calls + 1;
    } else if (pending_.size() == MaxSamples) { Invalidate(); return {}; }

    Sample sample{};
    sample.before = record;
    sample.scene = scene; sample.root = root; sample.inheritedFlags = inheritedFlags;
    sample.call = ++totalCalls_; sample.calls = calls;
    if (index == pending_.size()) pending_.push_back(sample);
    else pending_[index] = sample;
    ++inFlight_;
    return {revision_, sample.call, index, true};
  }
  bool CompleteCall(const Ticket& ticket, const Context& context, const Record& postRecord) noexcept {
    // An obsolete ticket cannot edit or cancel an unrelated newer observation.
    if (!collecting_ || !ticket.valid || ticket.revision != revision_ || ticket.index >= pending_.size()) return false;
    const auto& sample = pending_[ticket.index];
    if (sample.call != ticket.call || sample.completed) return false;
    if (sealing_ || !(context_ == context) || postRecord.address != sample.before.address ||
        !ValidRecord(postRecord, sample.scene)) { Invalidate(); return false; }
    auto& completing = pending_[ticket.index];
    completing.after = postRecord; completing.completed = true;
    --inFlight_;
    return true;
  }
  void CancelCall(const Ticket& ticket) noexcept {
    if (!collecting_ || !ticket.valid || ticket.revision != revision_ || ticket.index >= pending_.size()) return;
    const auto& sample = pending_[ticket.index];
    if (sample.call != ticket.call || sample.completed) return;
    Invalidate();
  }
  void Invalidate() noexcept {
    ++revision_;
    pending_.clear(); published_.clear();
    totalCalls_ = inFlight_ = 0;
    collecting_ = sealed_ = sealing_ = false;
  }

  // Predicate is bool(scene, root), normally a nonthrowing current-state read.
  // No borrowed Sample reference crosses it. Exceptions propagate after the
  // failed observation is revoked; reentry can never publish the old bank.
  template<class ScenePredicate>
  bool Seal(const Context& context, const ScenePredicate& current) {
    if (!collecting_) return false;
    if (sealing_ || inFlight_ || !(context_ == context)) { Invalidate(); return false; }
    const auto revision = revision_;
    const Context expected = context;
    sealing_ = true;
    struct RejectScope {
      LightWorldObserver& owner;
      std::uint64_t revision;
      bool accepted = false;
      ~RejectScope() {
        if (!accepted && owner.revision_ == revision) owner.Invalidate();
      }
    } scope{*this, revision};
    const auto stable = [&]() noexcept {
      return revision_ == revision && collecting_ && sealing_ && !inFlight_ &&
        context_ == expected && context == expected;
    };
    for (std::size_t index = 0; index < pending_.size(); ++index) {
      if (!pending_[index].completed) return false;
      const auto scene = pending_[index].scene, root = pending_[index].root;
      bool first = true;
      for (std::size_t previous = 0; previous < index; ++previous)
        if (pending_[previous].scene == scene && pending_[previous].root == root) { first = false; break; }
      if (first && (!current(scene, root) || !stable())) return false;
    }
    if (!stable()) return false;
    published_.swap(pending_);
    pending_.clear();
    collecting_ = sealing_ = false;
    sealed_ = true;
    scope.accepted = true;
    return true;
  }

  // Only Current writes the caller's output. Changed is byte disagreement with
  // an observed post-call snapshot, not a conclusion about producer freshness.
  LookupStatus Lookup(const Context& context, const Record& record, Sample& output) const noexcept {
    if (!sealed_ || collecting_ || sealing_ || !(context_ == context) || !record.address) return LookupStatus::Unqualified;
    const auto scene = At(record.raw, 0x3c);
    if (!scene) return LookupStatus::Unqualified;
    for (const auto& sample : published_) if (sample.scene == scene && sample.after.address == record.address) {
      if (std::memcmp(sample.after.raw, record.raw, sizeof(record.raw))) return LookupStatus::Changed;
      output = sample;
      return LookupStatus::Current;
    }
    return LookupStatus::NotObserved;
  }
  std::size_t Count() const noexcept { return sealed_ ? published_.size() : pending_.size(); }
  unsigned CallCount() const noexcept { return totalCalls_; }
  bool Collecting() const noexcept { return collecting_; }
  bool Sealed() const noexcept { return sealed_; }

private:
  static std::uint32_t At(const unsigned char* bytes, std::size_t offset) noexcept {
    std::uint32_t result; std::memcpy(&result, bytes + offset, sizeof(result)); return result;
  }
  static bool ValidRecord(const Record& record, std::uint32_t scene) noexcept {
    return scene && record.address && At(record.raw, 0) == 0x6f0c88 && At(record.raw, 0x3c) == scene;
  }
  std::vector<Sample> pending_, published_;
  Context context_{};
  std::uint64_t revision_ = 0;
  unsigned totalCalls_ = 0, inFlight_ = 0;
  bool collecting_ = false, sealed_ = false, sealing_ = false;
};
} // namespace winx_remix
