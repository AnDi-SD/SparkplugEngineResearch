#include "Code/Sparkplug/spAudioBank.h"
#include "Code/SparkplugDX/spDXAudioBank.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>

// A bounded source-only ownership witness for the actual host allocations.
// No native allocator-address or object-layout equivalence is asserted.
namespace
{
    struct Watched final{const void* address=nullptr;unsigned releases=0;};
    std::array<Watched,4> watched{};unsigned watchedCount=0;
    void Released(void* value) noexcept
    {for(unsigned i=0;i<watchedCount;++i)if(value==watched[i].address)++watched[i].releases;}
}
void* operator new(std::size_t bytes)
{if(void* value=std::malloc(bytes?bytes:1))return value;throw std::bad_alloc();}
void operator delete(void* value) noexcept{Released(value);std::free(value);}
void operator delete(void* value,std::size_t) noexcept{::operator delete(value);}
void* operator new[](std::size_t bytes){return ::operator new(bytes);}
void operator delete[](void* value) noexcept{::operator delete(value);}
void operator delete[](void* value,std::size_t) noexcept{::operator delete(value);}

using namespace sparkplug::reconstruction;
namespace
{
    unsigned checks=0;
    void Check(bool value,const char* why)
    {++checks;if(!value)throw std::runtime_error(why);}
    using State=std::array<std::uint32_t,4>;
    State Capture(const spDXAudioBank& bank)
    {
        const auto& own=bank.GetStateForAnalysis();
        return {own.identifier10,own.rawWord14,static_cast<std::uint32_t>(bank.GetEntryCountForAnalysis()),
            static_cast<std::uint32_t>(bank.GetReservedEntrySlotsForAnalysis())};
    }
    void Watch(const void* address)
    {Check(address!=nullptr&&watchedCount<watched.size(),"Distinct live allocation witness");watched[watchedCount++]={address,0};}
    void Print(const State& state){for(auto word:state)std::cout<<word<<' ';}
    void Replay()
    {
        unsigned mode=0;std::uint32_t id=0,word=0,otherId=0,otherWord=0;
        while(std::cin>>mode>>id>>word>>otherId>>otherWord)
        {
            Check(mode<=4,"Bounded bank replay mode");
            std::unique_ptr<spDXAudioBank> owner;
            void* external=nullptr;spDXAudioBank* bank=nullptr;
            if(mode==1)
            {external=::operator new(sizeof(spDXAudioBank));bank=new(external)spDXAudioBank();}
            else {owner=std::make_unique<spDXAudioBank>();bank=owner.get();}
            const auto initial=Capture(*bank);bank->SetStateForFixture({id,word});
            const auto before=Capture(*bank);std::unique_ptr<spBaseObject> other;
            spCloneManager clones;bool returned=true,mapped=false;
            if(mode==2)
            {
                auto destination=std::make_unique<spDXAudioBank>();destination->SetStateForFixture({otherId,otherWord});
                const auto untouched=Capture(*destination);returned=bank->vfunc_14(*destination,clones);
                Check(untouched==Capture(*destination),"Inherited Copy preserves destination own fields/storage");other=std::move(destination);
            }
            else if(mode==3)returned=bank->vfunc_14(*bank,clones);
            else if(mode==4)
            {other=bank->vfunc_10(clones);returned=other!=nullptr;mapped=other&&clones.FindClone(*bank)==other.get();}
            Check(before==Capture(*bank),"Copy/Clone preserves source bank own fields/storage");
            const auto after=Capture(*bank);const auto* typed=dynamic_cast<const spDXAudioBank*>(other.get());
            Check(!other||typed!=nullptr,"Concrete clone/destination type");const bool hasOther=typed!=nullptr;
            const auto otherAfter=typed?Capture(*typed):State{};
            watched={};watchedCount=0;
            Watch(bank->GetEmptyEntryStorageForAnalysis());Watch(bank);
            if(typed){Watch(typed->GetEmptyEntryStorageForAnalysis());Watch(typed);}
            other.reset();
            if(mode==1)bank->~spDXAudioBank();else owner.reset();
            const std::array<unsigned,4> releases{watched[0].releases,watched[1].releases,watched[2].releases,watched[3].releases};
            watchedCount=0;
            Check(releases[0]==1&&releases[1]==(mode==1?0u:1u),"Bank empty storage freed once; external object ownership retained");
            Check(releases[2]==(hasOther?1u:0u)&&releases[3]==(hasOther?1u:0u),"Other bank and empty storage each freed once");
            if(external)::operator delete(external);
            Print(initial);Print(after);Print(otherAfter);
            std::cout<<returned<<' '<<releases[0]<<' '<<releases[1]<<' '<<releases[2]<<' '<<releases[3]<<' '<<mapped<<'\n';
        }
        Check(std::cin.eof(),"Complete native replay input");
    }
    void Contracts()
    {
        const auto& base=spAudioBank::StaticRTTI();const auto& concrete=spDXAudioBank::StaticRTTI();
        Check(base.factory==nullptr&&base.baseClassID==spBaseObject::ClassID,"AudioBank has registered Base ancestry and no native factory");
        Check(concrete.base==&base&&concrete.classID==0x32A624E0,"Concrete DX bank registered ancestry");
        auto object=spRTTIManager::Instance().Create(spDXAudioBank::ClassID);auto* bank=dynamic_cast<spDXAudioBank*>(object.get());
        Check(bank&&bank->IsKindOf(spAudioBank::ClassID)&&bank->IsKindOf(spBaseObject::ClassID),"Concrete actual factory role through shared RTTI");
        Check(Capture(*bank)==State{0,5,0,300},"Two native own defaults and empty300-slot storage");
        Check(bank->GetEmptyEntryStorageForAnalysis()!=nullptr,"Real empty reserved buffer exists");
        bank->SetStateForFixture({0xFFFFFFFFu,0});spCloneManager clones;
        auto clone=bank->vfunc_10(clones);auto* copy=dynamic_cast<spDXAudioBank*>(clone.get());
        Check(copy&&copy!=bank&&clones.FindClone(*bank)==copy,"Clone uses shared real portable map");
        Check(Capture(*copy)==State{0,5,0,300}&&Capture(*bank)==State{0xFFFFFFFFu,0,0,300},"Clone retains constructor own fields and source remains unchanged");
        copy->SetStateForFixture({7,0x12345678u});const auto destination=Capture(*copy);
        Check(bank->vfunc_14(*copy,clones)&&Capture(*copy)==destination,"Literal Base Copy leaves destination bank state unchanged");
        Check(bank->vfunc_14(*bank,clones)&&Capture(*bank)==State{0xFFFFFFFFu,0,0,300},"Literal self Copy leaves source own state unchanged");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==2&&std::string(argv[1])=="--replay")Replay();
        else {Contracts();std::cout<<"AudioBank empty PC contracts passed checks="<<checks<<'\n';}
        return 0;
    }
    catch(const std::exception& error){std::cerr<<"AudioBank "<<error.what()<<'\n';return 1;}
}
