#include "spAnimation.h"
#include "spController.h"
#include <algorithm>
#include <cmath>

namespace sparkplug::reconstruction
{
    const spRTTIRecord& spAnimation::StaticRTTI() noexcept
    {
        // Engine identity is distinct from this object's physical named base.
        const auto& controller = spController::StaticRTTI();
        static const spRTTIRecord record{
            ClassID,
            controller.classID,
            "spAnimation",
            &controller,
            +[]() -> std::unique_ptr<spBaseObject> { return std::make_unique<spAnimation>(); },
            nullptr};
        static const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;
        return record;
    }
    const spRTTIRecord& spAnimation::vfunc_18() const noexcept
    {
        return StaticRTTI();
    }
    std::unique_ptr<spBaseObject> spAnimation::vfunc_10(spCloneManager& manager) const
    {
        auto result = std::make_unique<spAnimation>();
        manager.RegisterClone(*this, *result);
        return vfunc_14(*result, manager) ? std::move(result) : nullptr;
    }
    bool spAnimation::vfunc_14(spBaseObject& destination, spCloneManager&) const
    {
        auto* target = dynamic_cast<spAnimation*>(&destination);
        if (!target)
            return false;
        CopyNameToForAnalysis(*target);
        return true;
    }
    bool spAnimation::SetTotalTimeForAnalysis(float time) noexcept
    {
        if (!std::isfinite(time))
            return false; // host-only guard; native reader stores the raw float
        totalTime_ = time;
        return true;
    }
    spAnimTrack* spAnimation::GetTrackForAnalysis(std::size_t index) noexcept
    {
        return index < tracks_.size() ? tracks_[index].get() : nullptr;
    }
    const spAnimTrack* spAnimation::GetTrackForAnalysis(std::size_t index) const noexcept
    {
        return index < tracks_.size() ? tracks_[index].get() : nullptr;
    }
    spAnimTrack* spAnimation::AppendTrackForAnalysis()
    {
        if (tracks_.size() >= 100000)
            return nullptr;
        auto track = std::make_unique<spAnimTrack>();
        track->owner_ = this;
        tracks_.push_back(std::move(track));
        // Native grows the raw 0x44-byte array by exactly one when full. Host
        // stable pointees differ from native byte relocation/pointer invalidation.
        trackCapacity_ = std::max(trackCapacity_, tracks_.size());
        return tracks_.back().get();
    }
    bool spAnimation::ResizeTrackCapacityForAnalysis(std::size_t capacity)
    {
        if (capacity > 100000)
            return false;
        if (capacity < tracks_.size())
            tracks_.resize(capacity);
        // Do not destruct unconstructed reserve slots as native shrink can do.
        trackCapacity_ = capacity;
        return true;
    }
    bool spAnimation::InsertTagForAnalysis(TagForAnalysis tag)
    {
        if (!std::isfinite(tag.time) || tags_.size() >= 100000)
            return false;
        auto position = std::upper_bound(
            tags_.begin(), tags_.end(), tag.time,
            [](float time, const TagForAnalysis& item) { return time < item.time; });
        tags_.insert(position, std::move(tag));
        return true;
    }
} // namespace sparkplug::reconstruction
