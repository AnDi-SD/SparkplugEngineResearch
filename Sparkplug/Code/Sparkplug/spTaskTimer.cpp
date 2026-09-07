#include "spTaskTimer.h"
#include <unordered_set>

namespace sparkplug::reconstruction
{
    namespace
    {
        bool Fail(std::string* error, const char* message)
        {
            if (error)
                *error = message;
            return false;
        }
        bool ReadClock(const spTaskTimer::ClockSourceForAnalysis& source,
                       std::uint32_t& milliseconds, std::string* error)
        {
            if (!source)
                return Fail(error, "Task timer clock provider is missing.");
            const auto sample = source();
            if (!sample.divisor)
                return Fail(error, "Task timer clock divisor must be nonzero.");
            milliseconds = sample.rawTicks / sample.divisor;
            return true;
        }
    } // namespace

    spTaskTimer::spTaskTimer(spTaskTimer* borrowedSource) noexcept : source_(borrowedSource)
    {
    }
    const spRTTIRecord& spTaskTimer::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{
            ClassID,
            spBaseObject::ClassID,
            "spTaskTimer",
            &spBaseObject::StaticRTTI(),
            +[]() -> std::unique_ptr<spBaseObject> { return std::make_unique<spTaskTimer>(); },
            nullptr};
        static const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;
        return record;
    }
    const spRTTIRecord& spTaskTimer::vfunc_18() const noexcept
    {
        return StaticRTTI();
    }
    std::unique_ptr<spBaseObject> spTaskTimer::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<spTaskTimer>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }
    bool spTaskTimer::vfunc_14(spBaseObject& destination, spCloneManager& manager) const
    {
        return destination.IsKindOf(ClassID) && spBaseObject::vfunc_14(destination, manager);
    }

    bool spTaskTimer::ValidateTreeForAnalysis(std::string* error) const
    {
        std::unordered_set<const spTaskTimer*> visited;
        std::vector<std::pair<const spTaskTimer*, std::size_t>> pending{{this, 0}};
        while (!pending.empty())
        {
            const auto [timer, depth] = pending.back();
            pending.pop_back();
            if (!timer || depth > 128 || visited.size() >= 4096 || !visited.insert(timer).second)
                return Fail(
                    error,
                    "Task timer child graph is null, repeated, cyclic or exceeds host bounds.");
            if (timer->children_.size() > 4096 - pending.size())
                return Fail(error, "Task timer child count exceeds host bounds.");
            for (auto* child : timer->children_)
                pending.emplace_back(child, depth + 1);
        }
        return true;
    }
    bool spTaskTimer::SetChildrenForAnalysis(std::vector<spTaskTimer*> children, std::string* error)
    {
        children_.swap(children);
        try
        {
            if (ValidateTreeForAnalysis(error))
                return true;
        }
        catch (...)
        {
            children_.swap(children);
            throw;
        }
        children_.swap(children);
        return false;
    }
    bool spTaskTimer::AppendClockChildForAnalysis(spTaskTimer& child, std::string* error)
    {
        auto appended = children_;
        appended.push_back(&child);
        if (!SetChildrenForAnalysis(std::move(appended), error))
            return false;
        child.source_ = this;
        return true;
    }
    bool spTaskTimer::UpdateForAnalysis(const ClockSourceForAnalysis& clock, std::string* error)
    {
        if (!ValidateTreeForAnalysis(error))
            return false;
        if (!state_.active)
        {
            state_.deltaSeconds = 0;
        }
        else if (source_)
        {
            state_.currentMilliseconds = source_->state_.currentMilliseconds;
            state_.deltaSeconds = source_->state_.deltaSeconds;
        }
        else
        {
            std::uint32_t now;
            if (!ReadClock(clock, now, error))
                return false;
            const std::uint32_t current =
                state_.relative ? now - state_.startMilliseconds + state_.pausedMilliseconds : now;
            const std::uint32_t difference = current - state_.currentMilliseconds;
            // Native fild signed +2^32 fixup preserves the full unsigned value
            // before multiplying by the float32 constant3A83126F in x87.
            state_.deltaSeconds =
                static_cast<float>(static_cast<double>(difference) * static_cast<double>(0.001f));
            state_.currentMilliseconds = current;
        }
        for (auto* child : children_)
        {
            child->state_.active = state_.active;
            child->state_.relative = state_.relative;
            if (!child->UpdateForAnalysis(clock, error))
                return false;
        }
        return true;
    }
    bool spTaskTimer::StartForAnalysis(const ClockSourceForAnalysis& clock, std::string* error)
    {
        std::uint32_t now;
        if (!ReadClock(clock, now, error))
            return false;
        state_.active = true;
        state_.startMilliseconds = now;
        return true;
    }
    void spTaskTimer::PauseForAnalysis() noexcept
    {
        state_.pausedMilliseconds = state_.currentMilliseconds;
        state_.active = false;
    }
    void spTaskTimer::ResetForAnalysis() noexcept
    {
        state_.pausedMilliseconds = 0;
        state_.startMilliseconds = 0;
        state_.currentMilliseconds = 0;
        state_.active = false;
    }
} // namespace sparkplug::reconstruction
