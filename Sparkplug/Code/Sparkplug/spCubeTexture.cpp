#include "spCubeTexture.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord Record{spCubeTexture::ClassID, spTexture::ClassID,
            "spCubeTexture", &spTexture::StaticRTTI(), nullptr, nullptr};
        const bool Registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spCubeTexture::StaticRTTI() noexcept
    { (void)Registered; return Record; }
    const spRTTIRecord& spCubeTexture::vfunc_18() const noexcept { return Record; }
    std::unique_ptr<spBaseObject> spCubeTexture::vfunc_10(spCloneManager&) const
    { return nullptr; }
}
