#include "spProjectionFX.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord record{spProjectionFX::ClassID,spCrossPlatform::ClassID,
            "spProjectionFX",&spCrossPlatform::StaticRTTI(),nullptr,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    const spRTTIRecord& spProjectionFX::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& spProjectionFX::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> spProjectionFX::vfunc_10(spCloneManager&) const { return nullptr; }
}
