#include "wxGameTimer.h"
#include <stdexcept>
#include <utility>

namespace winx::reconstruction
{
    using sparkplug::reconstruction::spBaseObject;
    using sparkplug::reconstruction::spCloneManager;
    using sparkplug::reconstruction::spMessageQueueHost;
    using sparkplug::reconstruction::spRTTIManager;
    using sparkplug::reconstruction::spRTTIRecord;
    using sparkplug::reconstruction::spTaskTimer;
    using sparkplug::reconstruction::spTimer;

    namespace
    {
        wxGameTimerFactoryBindingsForAnalysis bindings;
        std::unique_ptr<spBaseObject> CreateGameTimer()
        {
            if (!bindings.globalClock)
                throw std::logic_error("GameTimer requires a real borrowed global spTimer binding.");
            return std::make_unique<wxGameTimer>(*bindings.globalClock, bindings.queueHost);
        }
    }

    wxGameTimer* wxGameTimer::instance_ = nullptr;

    wxGameTimer::wxGameTimer(spTimer& globalClock, spMessageQueueHost* queueHost) noexcept
        : spTaskTimer(nullptr), globalClock_(globalClock), queueHost_(queueHost), queue_(*this, queueHost)
    {
        instance_ = this;
        // Actual native global75F65C is a constructed spTimer. These are its
        // limited byte+1C and limit word+20, preserving all other clock state.
        auto state = globalClock_.GetStateForAnalysis();
        state.limited = 1;
        state.limit = 100;
        globalClock_.SetStateForAnalysis(state);
    }

    wxGameTimer::~wxGameTimer()
    {
        // Native owned Queue cleanup precedes clearing the singleton. The
        // member destructor subsequently sees an empty portable Queue.
        queue_.ClearForAnalysis();
        instance_ = nullptr;
    }

    void wxGameTimer::SetFactoryBindingsForAnalysis(wxGameTimerFactoryBindingsForAnalysis value) noexcept
    {
        bindings = value;
    }

    wxGameTimer* wxGameTimer::GetInstanceForAnalysis() noexcept
    {
        return instance_;
    }

    const spRTTIRecord& wxGameTimer::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID, spTaskTimer::ClassID, "wxGameTimer",
            &spTaskTimer::StaticRTTI(), &CreateGameTimer, nullptr};
        static const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;
        return record;
    }

    const spRTTIRecord& wxGameTimer::vfunc_18() const noexcept
    {
        return StaticRTTI();
    }

    std::unique_ptr<spBaseObject> wxGameTimer::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxGameTimer>(globalClock_, queueHost_);
        manager.RegisterCloneForAnalysis(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    bool wxGameTimer::vfunc_14(spBaseObject& destination, spCloneManager& manager) const
    {
        return spBaseObject::vfunc_14(destination, manager);
    }

    bool wxGameTimer::StartForAnalysis(const ClockSourceForAnalysis& clock, std::string* error)
    {
        paused_ = 0;
        return spTaskTimer::StartForAnalysis(clock, error);
    }

    void wxGameTimer::PauseForAnalysis() noexcept
    {
        paused_ = 1;
        spTaskTimer::PauseForAnalysis();
    }
}
