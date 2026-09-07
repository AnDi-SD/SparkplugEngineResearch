#include "spTrack.h"

namespace sparkplug::reconstruction
{
    const spRTTIRecord& spTrack::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{
            ClassID,
            spNamedObject::ClassID,
            "spTrack",
            &spNamedObject::StaticRTTI(),
            +[]() -> std::unique_ptr<spBaseObject> { return std::make_unique<spTrack>(); },
            nullptr};
        static const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;
        return record;
    }
    const spRTTIRecord& spTrack::vfunc_18() const noexcept
    {
        return StaticRTTI();
    }
    std::unique_ptr<spBaseObject> spTrack::vfunc_10(spCloneManager& manager) const
    {
        auto result = std::make_unique<spTrack>();
        manager.RegisterClone(*this, *result);
        return vfunc_14(*result, manager) ? std::move(result) : nullptr;
    }
    float spTrack::GetDurationForAnalysis() const noexcept
    {
        return 0;
    }
    void spTrack::ReleaseKeysForAnalysis() noexcept
    {
    }
} // namespace sparkplug::reconstruction
