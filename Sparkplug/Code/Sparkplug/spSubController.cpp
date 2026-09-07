#include "spSubController.h"

namespace sparkplug::reconstruction
{
    const spRTTIRecord& spSubController::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID,           spBaseObject::ClassID,
                                         "spSubController", &spBaseObject::StaticRTTI(),
                                         nullptr,           nullptr};
        static const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;
        return record;
    }
    const spRTTIRecord& spSubController::vfunc_18() const noexcept
    {
        return StaticRTTI();
    }
} // namespace sparkplug::reconstruction
