#pragma once
// Own frame preparation order. Native reads, camera/light API calls and the
// actual instance commit belong to injected adapters. Caller serializes use.
#include <cstdint>

namespace winx_remix {
class FrameScheduler final {
public:
  struct Context {
    std::uint32_t frame = 0;
    std::uint64_t deviceEpoch = 0, device = 0, scene = 0, camera = 0, sceneScope = 0;
    bool apiLights = false;
    bool operator==(const Context& other) const {
      return frame == other.frame && deviceEpoch == other.deviceEpoch && device == other.device &&
        scene == other.scene && camera == other.camera && sceneScope == other.sceneScope && apiLights == other.apiLights;
    }
    bool Valid() const { return device && scene && camera && sceneScope; }
  };
  struct Ticket { Context context{}; std::uint64_t revision = 0; bool valid = false; };
  enum class Phase { Idle, Camera, Lights, Ready, Rejected };

  FrameScheduler() = default;
  FrameScheduler(const FrameScheduler&) = delete;
  FrameScheduler& operator=(const FrameScheduler&) = delete;

  void Invalidate() noexcept { ++revision_; phase_ = Phase::Rejected; }
  Phase Stage() const noexcept { return phase_; }
  bool Preparing() const noexcept { return preparing_; }

  // A previous completion never replaces fresh adapter predicates. Preparation
  // may be repeated, while the camera/light adapters preserve their own exact
  // single-attempt policy. This class never replays an API instance commit.
  template<class Camera, class Lights, class Predicate>
  Ticket Prepare(const Context& context, const Camera& camera, const Lights& lights, const Predicate& current) {
    if (preparing_) { Invalidate(); return {}; }
    preparing_ = true;
    struct PreparingScope {
      FrameScheduler& owner;
      bool completed = false;
      ~PreparingScope() {
        owner.preparing_ = false;
        if (!completed) owner.phase_ = Phase::Rejected;
      }
    } scope{*this};
    const auto revision = ++revision_;
    context_ = context;
    const auto stable = [&] {
      if (revision_ != revision || !(context_ == context)) return false;
      if (!current()) return false;
      return revision_ == revision && context_ == context;
    };
    if (!context.Valid() || !stable()) return {};
    phase_ = Phase::Camera;
    if (!camera() || !stable()) return {};
    phase_ = Phase::Lights;
    if (!lights() || !stable()) return {};
    phase_ = Phase::Ready;
    scope.completed = true;
    return {context, revision, true};
  }

  // Check immediately around external resource/instance operations. The
  // supplied context comes from current frame/device/scene state, not Ticket.
  template<class Predicate>
  bool Current(const Ticket& ticket, const Context& context, const Predicate& current) const {
    const auto revision = revision_;
    if (!ticket.valid || preparing_ || phase_ != Phase::Ready || ticket.revision != revision ||
        !(ticket.context == context_) || !(ticket.context == context)) return false;
    if (!current()) return false;
    return revision_ == revision && phase_ == Phase::Ready && !preparing_ &&
      ticket.valid && ticket.revision == revision && ticket.context == context_ && ticket.context == context;
  }

private:
  Context context_{};
  std::uint64_t revision_ = 0;
  Phase phase_ = Phase::Idle;
  bool preparing_ = false;
};
} // namespace winx_remix
