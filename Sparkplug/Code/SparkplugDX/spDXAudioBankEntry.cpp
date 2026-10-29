#include "spDXAudioBankEntry.h"
#include <utility>

namespace sparkplug::reconstruction
{
    spDXAudioBankEntry::spDXAudioBankEntry(CapturedEmptyProfileForAnalysis profile):spAudioBankEntry(profile){}
    spDXAudioBankEntry::~spDXAudioBankEntry()=default;
    const spRTTIRecord& spDXAudioBankEntry::StaticRTTI() noexcept
    {
        // Actual PC factory identity is known. Portable RTTI cannot infer a
        // captured nonzero native unassigned E0 value, so it stays unbound.
        static const spRTTIRecord record{ClassID,spAudioBankEntry::ClassID,"spDXAudioBankEntry",
            &spAudioBankEntry::StaticRTTI(),nullptr,nullptr};
        static const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;return record;
    }
    const spRTTIRecord& spDXAudioBankEntry::vfunc_18() const noexcept{return StaticRTTI();}
    std::unique_ptr<spBaseObject> spDXAudioBankEntry::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spDXAudioBankEntry>(GetCapturedEmptyProfileForAnalysis());
        manager.RegisterCloneForAnalysis(*this,*clone);
        return vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
}
