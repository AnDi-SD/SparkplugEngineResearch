#include "spTransformEval.h"

namespace sparkplug::reconstruction
{
    const spRTTIRecord& spTransformEval::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID,           spEvaluator::ClassID,
                                         "spTransformEval", &spEvaluator::StaticRTTI(),
                                         nullptr,           nullptr};
        static const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;
        return record;
    }
    const spRTTIRecord& spTransformEval::vfunc_18() const noexcept
    {
        return StaticRTTI();
    }
} // namespace sparkplug::reconstruction
