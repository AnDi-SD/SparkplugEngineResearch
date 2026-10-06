#include "spTimer.h"
#include <stdexcept>

namespace sparkplug::reconstruction
{
    namespace
    {
        bool HasClock(const spTimer::ClockSourceForAnalysis& clock, std::string* error)
        {
            if (clock) return true;
            if (error) *error = "Timer clock provider is missing.";
            return false;
        }
    }
    spTimer::spTimer(std::uint8_t startImmediately, const ClockSourceForAnalysis& clock)
    {
        if (startImmediately && !StartForAnalysis(clock))
            throw std::logic_error("Timer constructor clock provider is missing.");
    }
    const spRTTIRecord& spTimer::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID, spBaseObject::ClassID, "spTimer",
            &spBaseObject::StaticRTTI(),
            +[]() -> std::unique_ptr<spBaseObject> { return std::make_unique<spTimer>(); }, nullptr};
        static const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;
        return record;
    }
    const spRTTIRecord& spTimer::vfunc_18() const noexcept { return StaticRTTI(); }
    std::unique_ptr<spBaseObject> spTimer::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<spTimer>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }
    bool spTimer::StartForAnalysis(const ClockSourceForAnalysis& clock, std::string* error)
    {
        if (!HasClock(clock, error)) return false;
        const auto now = clock();
        state_.startedAt = now;
        state_.active = 1;
        return true;
    }
    bool spTimer::StopForAnalysis(const ClockSourceForAnalysis& clock, std::string* error)
    {
        if (!HasClock(clock, error)) return false;
        // Stop deliberately ignores active, including repeated Stop calls.
        // The branch byte is read before the first foreign clock call.
        const bool limited = state_.limited != 0;
        const auto first = clock();
        const std::uint32_t difference = first - state_.startedAt;
        std::uint32_t increment = difference;
        if (limited)
        {
            const auto limit = state_.limit;
            if (difference > limit) increment = limit;
            else
            {
                // The second sample is not clamped. Reload startedAt after
                // the callback, as in both shipped integer bodies.
                const auto second = clock();
                increment = second - state_.startedAt;
            }
        }
        state_.accumulated += increment;
        state_.active = 0;
        return true;
    }
    bool spTimer::ResetForAnalysis(const ClockSourceForAnalysis& clock, std::string* error)
    {
        if (!HasClock(clock, error)) return false;
        // Native Reset writes zero before requesting its new clock sample.
        state_.accumulated = 0;
        const auto now = clock();
        state_.startedAt = now;
        state_.active = 1;
        return true;
    }
}
