#include "spEvaluator.h"

namespace sparkplug::reconstruction
{
    const spRTTIRecord& spEvaluator::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID,       spBaseObject::ClassID,
                                         "spEvaluator", &spBaseObject::StaticRTTI(),
                                         nullptr,       nullptr};
        static const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;
        return record;
    }
    const spRTTIRecord& spEvaluator::vfunc_18() const noexcept
    {
        return StaticRTTI();
    }
} // namespace sparkplug::reconstruction
