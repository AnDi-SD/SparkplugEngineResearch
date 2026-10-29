#include "Code/Sparkplug/spAudioManager.h"
#include "Code/SparkplugDX/spDXAudioManager.h"
#include "Code/SparkplugDX/spDXAudioBank.h"
#include "Analysis/Host/spAudioManagerHost.h"
#include "Analysis/PC/spAudioManagerBankAbi.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{
    struct Watch final{const void* address=nullptr;unsigned releases=0;};
    std::array<Watch,16> watched{};unsigned watchCount=0;bool failNextAllocation=false;
    void Released(void* pointer) noexcept
    {for(unsigned i=0;i<watchCount;++i)if(pointer==watched[i].address)++watched[i].releases;}
}
void* operator new(std::size_t size)
{
    if(failNextAllocation){failNextAllocation=false;throw std::bad_alloc();}
    if(void* pointer=std::malloc(size?size:1))return pointer;
    throw std::bad_alloc();
}
void operator delete(void* pointer) noexcept{Released(pointer);std::free(pointer);}
void operator delete(void* pointer,std::size_t) noexcept{::operator delete(pointer);}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete[](void* pointer) noexcept{::operator delete(pointer);}
void operator delete[](void* pointer,std::size_t) noexcept{::operator delete(pointer);}

using namespace sparkplug::reconstruction;
namespace
{
    unsigned checks=0;
    void Check(bool value,const char* why)
    {++checks;if(!value)throw std::runtime_error(why);}
    spAudioManagerCapturedPcRegistryForAnalysis Input(const std::vector<std::uint32_t>& ids)
    {
        spAudioManagerCapturedPcRegistryForAnalysis result;
        result.profile=spAudioManagerPcRegistryProfileForAnalysis::StableOwnedInactiveNullDevice;
        for(std::size_t i=0;i<ids.size();++i)
        {
            auto bank=std::make_unique<spDXAudioBank>();
            bank->SetStateForFixture({ids[i],0xA5A50000u|static_cast<std::uint32_t>(i)});
            result.banks.push_back(std::move(bank));
        }
        return result;
    }
    using State=std::array<std::uint32_t,4>;
    State Bank(const spAudioBank& bank)
    {
        const auto& raw=bank.GetStateForAnalysis();
        return {raw.identifier10,raw.rawWord14,static_cast<std::uint32_t>(bank.GetEntryCountForAnalysis()),
            static_cast<std::uint32_t>(bank.GetReservedEntrySlotsForAnalysis())};
    }
    std::vector<State> Capture(const spAudioManager& manager)
    {
        std::vector<State> result;
        for(std::size_t i=0;i<manager.GetBankCountForAnalysis();++i)
        {const auto* bank=manager.GetBankForAnalysis(i);Check(bank!=nullptr,"Live active bank");result.push_back(Bank(*bank));}
        return result;
    }
    void AddWatch(const void* pointer)
    {Check(pointer&&watchCount<watched.size(),"Bounded unique allocation witness");watched[watchCount++]={pointer,0};}
    void Print(const std::vector<State>& states)
    {std::cout<<states.size()<<' ';for(const auto& state:states)for(auto word:state)std::cout<<word<<' ';}
    void Replay()
    {
        std::string line;
        while(std::getline(std::cin,line))
        {
            std::istringstream stream(line);unsigned count=0;std::uint32_t key=0;
            Check(bool(stream>>count>>key)&&count<=8,"Captured registry replay header");
            std::vector<std::uint32_t> ids(count);for(auto& id:ids)Check(bool(stream>>id),"Captured registry identifier");
            std::string extra;Check(!(stream>>extra),"Complete captured registry input");
            auto input=Input(ids);spDXAudioManager manager(std::move(input));const auto before=Capture(manager);
            watched={};watchCount=0;
            for(std::size_t i=0;i<manager.GetBankCountForAnalysis();++i)
            {
                const auto* bank=manager.GetBankForAnalysis(i);
                AddWatch(bank->GetEmptyEntryStorageForAnalysis());AddWatch(bank);
            }
            const bool completed=manager.RemoveBankPcForAnalysis(key);
            std::array<unsigned,16> releases{};
            for(unsigned i=0;i<watchCount;++i)releases[i]=watched[i].releases;
            watchCount=0;
            const auto after=Capture(manager);
            // Native full EAX and native post-deletion object bytes are not
            // compared. Native AL1 matches this analytical completion value.
            Print(before);std::cout<<completed<<' ';Print(after);
            std::cout<<manager.GetReservedBankSlotsForAnalysis()<<' ';
            for(unsigned i=0;i<count;++i)std::cout<<releases[2*i]<<' ';
            for(unsigned i=0;i<count;++i)std::cout<<releases[2*i+1]<<' ';
            std::cout<<manager.IsRegistryPoisonedForAnalysis()<<'\n';
        }
    }
    void Contracts()
    {
        (void)spAudioManager::StaticRTTI();const auto& concrete=spDXAudioManager::StaticRTTI();
        Check(concrete.baseClassID==spAudioManager::ClassID&&concrete.factory==nullptr,
            "Native DX factory exists but portable capture cannot fabricate startup");
        Check(!spRTTIManager::Instance().Create(spDXAudioManager::ClassID),"Unconfigured native manager factory remains refused");
        {
            auto input=Input({7});input.profile=spAudioManagerPcRegistryProfileForAnalysis::Unqualified;
            auto* owned=input.banks[0].get();bool refused=false;
            try{spDXAudioManager invalid(std::move(input));}catch(const std::logic_error&){refused=true;}
            Check(refused&&input.banks.size()==1&&input.banks[0].get()==owned,"Unqualified profile refuses before taking bank ownership");
        }
        {
            auto input=Input({1});input.banks.push_back(nullptr);bool refused=false;
            try{spDXAudioManager invalid(std::move(input));}catch(const std::logic_error&){refused=true;}
            Check(refused&&input.banks[0]!=nullptr,"Null active entry is an analytical input refusal");
        }
        {
            auto input=Input({1,2,3,4,5,6,7,8,9});bool refused=false;
            try{spDXAudioManager invalid(std::move(input));}catch(const std::logic_error&){refused=true;}
            Check(refused&&input.banks.size()==9,"Larger captured input is not silently admitted");
        }
        {
            auto input=Input({2,3,4,5,6,7,8,9});spDXAudioManager manager(std::move(input));
            const auto before=Capture(manager);
            Check(!manager.RemoveBankPcForAnalysis(1)&&before==Capture(manager)&&!manager.IsRegistryPoisonedForAnalysis(),
                "Unsupported ninth active result refuses before deleting/allocating");
            Check(manager.RemoveBankPcForAnalysis(0xFFFFFFFFu)&&before==Capture(manager),"No-match remains native AL1/analytical completion");
            Check(!manager.GetBankForAnalysis(8),"Bounded host inspection outside active registry");
            spCloneManager clones;bool refused=false;
            try{(void)manager.vfunc_10(clones);}catch(const std::logic_error&){refused=true;}
            Check(refused,"Source manager construction/Clone are explicitly unqualified");
        }
        {
            auto input=Input({1});spDXAudioManager manager(std::move(input));
            failNextAllocation=true;bool failed=false;
            try{(void)manager.RemoveBankPcForAnalysis(1);}catch(const std::bad_alloc&){failed=true;}
            failNextAllocation=false;
            Check(failed&&manager.GetBankCountForAnalysis()==0&&manager.IsRegistryPoisonedForAnalysis(),
                "Host allocation failure after deletion poisons partial context; no native failure parity claim");
            Check(!manager.RemoveBankPcForAnalysis(9)&&manager.GetBankCountForAnalysis()==0,
                "Poisoned captured registry cannot silently retry");
        }
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==2&&std::string(argv[1])=="--replay")Replay();
        else {Contracts();std::cout<<"AudioManager captured PC bank contracts passed checks="<<checks<<'\n';}
        return 0;
    }
    catch(const std::exception& error)
    {watchCount=0;failNextAllocation=false;std::cerr<<"AudioManagerBank "<<error.what()<<'\n';return 1;}
}
