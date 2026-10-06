#include "spMasterTimer.h"

namespace sparkplug::reconstruction
{
    spMasterTimer* spMasterTimer::instance_ = nullptr;
    spMasterTimer::spMasterTimer() noexcept { instance_ = this; }
    spMasterTimer::~spMasterTimer()
    {
        // Both shipped destructors clear the global unconditionally, including
        // destruction of an older object after another construction or clone.
        instance_ = nullptr;
    }
    spMasterTimer* spMasterTimer::GetInstanceForAnalysis() noexcept { return instance_; }
    const spRTTIRecord& spMasterTimer::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID, spTimer::ClassID, "spMasterTimer",
            &spTimer::StaticRTTI(),
            +[]() -> std::unique_ptr<spBaseObject> { return std::make_unique<spMasterTimer>(); }, nullptr};
        static const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;
        return record;
    }
    const spRTTIRecord& spMasterTimer::vfunc_18() const noexcept { return StaticRTTI(); }
    std::unique_ptr<spBaseObject> spMasterTimer::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<spMasterTimer>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }
}
