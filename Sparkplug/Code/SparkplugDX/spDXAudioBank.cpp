#include "spDXAudioBank.h"
#include "spDXAudioBankEntry.h"
#include <utility>
#include <stdexcept>

namespace sparkplug::reconstruction
{
    spDXAudioBank::spDXAudioBank(CapturedOwnedEntriesProfileForAnalysis profile,
        std::vector<std::unique_ptr<spDXAudioBankEntry>>&& entries)
    {
        if(profile!=CapturedOwnedEntriesProfileForAnalysis::StableUniqueCapturedEntries)
            throw std::logic_error("Bank owned Entry profile is unqualified");
        if(GetEntryCountForAnalysis()!=0||entries.size()>MaximumCapturedOwnedEntriesForAnalysis
            ||GetReservedEntrySlotsForAnalysis()!=InitialReservedEntrySlotsForAnalysis)
            throw std::logic_error("Captured Bank input requires empty fixed300 storage and at most8 entries");
        for(std::size_t i=0;i<entries.size();++i)
        {
            if(!entries[i])throw std::logic_error("Captured Bank input cannot contain null entries");
            for(std::size_t j=0;j<i;++j)
                if(entries[j].get()==entries[i].get())throw std::logic_error("Captured Bank input requires unique Entry owners");
            if(entries[i]->GetCapturedEmptyProfileForAnalysis()
                !=spAudioBankEntry::CapturedEmptyProfileForAnalysis::NonzeroUnassignedE0WithNullResources)
                throw std::logic_error("Captured Bank input requires the bounded Entry lifetime profile");
        }
        // All type/profile/count/identity guards precede moves. Unique pointer
        // conversion is noexcept and the Base helper cannot grow storage.
        for(auto& entry:entries)
        {
            std::unique_ptr<spBaseObject> owner=std::move(entry);
            AdoptCapturedEntryForAnalysis(std::move(owner));
        }
    }
    const spRTTIRecord& spDXAudioBank::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID,spAudioBank::ClassID,
            "spDXAudioBank",&spAudioBank::StaticRTTI(),
            +[]()->std::unique_ptr<spBaseObject>{return std::make_unique<spDXAudioBank>();},nullptr};
        static const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;return record;
    }
    const spRTTIRecord& spDXAudioBank::vfunc_18() const noexcept{return StaticRTTI();}
    std::unique_ptr<spBaseObject> spDXAudioBank::vfunc_10(spCloneManager& manager) const
    {
        if(GetEntryCountForAnalysis()!=0)throw std::logic_error("Populated Bank Clone remains unqualified");
        auto clone=std::make_unique<spDXAudioBank>();
        manager.RegisterCloneForAnalysis(*this,*clone);
        return vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
}
