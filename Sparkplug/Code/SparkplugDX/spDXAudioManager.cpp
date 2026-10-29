#include "spDXAudioManager.h"
#include "Analysis/Host/spAudioManagerHost.h"
#include <utility>

namespace sparkplug::reconstruction
{
    spDXAudioManager::spDXAudioManager(spAudioManagerCapturedPcRegistryForAnalysis&& input)
        :spAudioManager(std::move(input)){}
    const spRTTIRecord& spDXAudioManager::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID,spAudioManager::ClassID,
            "spDXAudioManager",&spAudioManager::StaticRTTI(),nullptr,nullptr};
        static const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;return record;
    }
    const spRTTIRecord& spDXAudioManager::vfunc_18() const noexcept{return StaticRTTI();}
}
