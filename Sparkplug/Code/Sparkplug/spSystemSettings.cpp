#include "spSystemSettings.h"

namespace sparkplug::reconstruction
{
    spSystemSettings* spSystemSettings::instance_ = nullptr;
    spSystemSettings::spSystemSettings() noexcept { instance_ = this; }
    spSystemSettings::~spSystemSettings()
    {
        // PC4BE5F0 clears the singleton unconditionally, even when another
        // Settings object most recently replaced the published pointer.
        instance_ = nullptr;
    }
    const spRTTIRecord& spSystemSettings::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID, spBaseObject::ClassID, "spSystemSettings",
            &spBaseObject::StaticRTTI(),
            +[]() -> std::unique_ptr<spBaseObject> { return std::make_unique<spSystemSettings>(); }, nullptr};
        static const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered; return record;
    }
    const spRTTIRecord& spSystemSettings::vfunc_18() const noexcept { return StaticRTTI(); }
    std::unique_ptr<spBaseObject> spSystemSettings::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<spSystemSettings>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        // PC4BE580 invokes Base40ECE0. The 255 initialized bytes and unwritten
        // trailing byte remain fresh constructor defaults, without payload copy.
        return spBaseObject::vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }
}
