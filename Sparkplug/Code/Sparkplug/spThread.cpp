#include "spThread.h"

namespace sparkplug::reconstruction
{
    const spRTTIRecord& spThread::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID, spCrossPlatform::ClassID,
            "spThread", &spCrossPlatform::StaticRTTI(), nullptr, nullptr};
        static const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;
        return record;
    }
    const spRTTIRecord& spThread::vfunc_18() const noexcept { return StaticRTTI(); }
}
