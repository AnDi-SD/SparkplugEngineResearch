#include "spAudioManager.h"
#include "spAudioBank.h"
#include "Code/SparkplugDX/spDXAudioBank.h"
#include "Analysis/Host/spAudioManagerHost.h"
#include <stdexcept>
#include <utility>

namespace sparkplug::reconstruction
{
    spAudioManager::spAudioManager(spAudioManagerCapturedPcRegistryForAnalysis&& input)
    {
        const bool capturedEntries=input.profile==spAudioManagerPcRegistryProfileForAnalysis::StableOwnedCapturedEntriesInactiveNullDevice;
        if((input.profile!=spAudioManagerPcRegistryProfileForAnalysis::StableOwnedInactiveNullDevice&&!capturedEntries)
            ||input.banks.size()>(capturedEntries?MaximumCapturedPopulatedBanksForAnalysis:MaximumCapturedBanksForAnalysis))
            throw std::logic_error("AudioManager requires a qualified captured PC bank registry");
        std::size_t entries=0;
        for(std::size_t i=0;i<input.banks.size();++i)
        {
            const auto& bank=input.banks[i];
            if(!bank||(!capturedEntries&&bank->GetEntryCountForAnalysis()!=0))
            {
                if(capturedEntries)throw std::logic_error("Captured Entry registry requires nonnull concrete DX banks");
                throw std::logic_error("AudioManager requires unique owned concrete empty DX banks");
            }
            if(capturedEntries)
            {
                for(std::size_t j=0;j<i;++j)
                    if(input.banks[j].get()==bank.get())throw std::logic_error("Captured Entry banks require distinct owners");
                const auto count=bank->GetEntryCountForAnalysis();
                if(count>MaximumCapturedEntriesForAnalysis-entries)
                    throw std::logic_error("Captured Entry registry exceeds8 total Entries");
                entries+=count;
            }
        }
        if(capturedEntries&&entries==0)throw std::logic_error("Captured Entry registry requires1..8 total Entries");
        // Our explicit captured-profile owner, not native344-byte construction.
        // Retain fixed20 capacity independently of the caller vector capacity.
        banks_.reserve(ReservedBankSlotsForAnalysis);
        capturedEntries_=capturedEntries;
        for(auto& bank:input.banks)banks_.push_back(std::move(bank));
        input.banks.clear();
    }
    spAudioManager::~spAudioManager()=default;
    const spRTTIRecord& spAudioManager::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID,spBaseObject::ClassID,
            "spAudioManager",&spBaseObject::StaticRTTI(),nullptr,nullptr};
        static const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;return record;
    }
    const spRTTIRecord& spAudioManager::vfunc_18() const noexcept{return StaticRTTI();}
    std::unique_ptr<spBaseObject> spAudioManager::vfunc_10(spCloneManager&) const
    {throw std::logic_error("AudioManager native construction/Clone remain unqualified");}
    const spAudioBank* spAudioManager::GetBankForAnalysis(std::size_t index) const noexcept
    {return index<banks_.size()?banks_[index].get():nullptr;}
    bool spAudioManager::RemoveBankPcForAnalysis(std::uint32_t identifier)
    {
        const auto maximum=capturedEntries_?MaximumCapturedPopulatedBanksForAnalysis:MaximumCapturedBanksForAnalysis;
        if(removing_||poisoned_||banks_.size()>maximum)return false;
        std::size_t match=banks_.size();
        std::size_t entries=0;
        for(std::size_t i=0;i<banks_.size();++i)
        {
            if(!banks_[i]||(!capturedEntries_&&banks_[i]->GetEntryCountForAnalysis()!=0))return false;
            if(capturedEntries_)
            {
                const auto count=banks_[i]->GetEntryCountForAnalysis();
                if(count>MaximumCapturedEntriesForAnalysis-entries)return false;
                entries+=count;
            }
            if(match==banks_.size()&&banks_[i]->GetStateForAnalysis().identifier10==identifier)match=i;
        }
        // A previous successful removal may leave no Entries. Query/lifetime
        // remain valid, but another new-mode Remove is outside its admission.
        if(capturedEntries_&&entries==0)return false;
        if(identifier==1&&match==banks_.size()&&banks_.size()==maximum)
            return false;
        removing_=true;poisoned_=true;
        struct Leave final{bool& active;~Leave(){active=false;}} leave{removing_};
        if(match<banks_.size())
        {
            // Delete the first match before replacing its slot with the last
            // owner. Native rereads its vector after deletion; foreign entry
            // types/callback mutations are outside both stable profiles.
            banks_[match].reset();
            if(match+1!=banks_.size())banks_[match]=std::move(banks_.back());
            banks_.pop_back();
        }
        if(identifier==1)
        {
            // Actual48FA50 calls the real concrete factory, then48FA55 stores
            // ID1. Reuse its qualified constructor directly; no success callback.
            auto bank=std::make_unique<spDXAudioBank>();
            bank->state_.identifier10=1;
            banks_.push_back(std::move(bank));
        }
        poisoned_=false;
        return true;
        // If allocation throws after deletion, the host context stays poisoned
        // and must be discarded. Native allocation-failure parity is unqualified.
    }
}
