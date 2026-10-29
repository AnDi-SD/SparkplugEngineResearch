#include "spAudioBankEntry.h"
#include <stdexcept>

namespace sparkplug::reconstruction
{
    spAudioBankEntry::spAudioBankEntry(CapturedEmptyProfileForAnalysis profile):profile_(profile)
    {
        if(profile!=CapturedEmptyProfileForAnalysis::NonzeroUnassignedE0WithNullResources)
            throw std::logic_error("AudioBankEntry requires its captured empty lifetime profile");
    }
    spAudioBankEntry::~spAudioBankEntry()=default;
    const spRTTIRecord& spAudioBankEntry::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID,spBaseObject::ClassID,"spAudioBankEntry",
            &spBaseObject::StaticRTTI(),nullptr,nullptr};
        static const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;return record;
    }
    const spRTTIRecord& spAudioBankEntry::vfunc_18() const noexcept{return StaticRTTI();}
    std::unique_ptr<spBaseObject> spAudioBankEntry::vfunc_10(spCloneManager&) const
    {
        // Do not inherit Named's concrete factory as an invented base Entry
        // clone. Only the actual DX clone has a qualified source producer.
        throw std::logic_error("Base AudioBankEntry Clone remains unqualified");
    }
    bool spAudioBankEntry::vfunc_14(spBaseObject& destination,spCloneManager& manager) const
    {
        auto* entry=dynamic_cast<spAudioBankEntry*>(&destination);
        if(!entry||!spBaseObject::vfunc_14(destination,manager))return false;
        // Actual primary0C413120 copies physical Name despite Named not being
        // in engine RTTI. The shared helper preserves self-Copy clearing.
        CopyNameToForAnalysis(*entry);
        return true;
    }
}
