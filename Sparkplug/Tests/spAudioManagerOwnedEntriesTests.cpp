#include "Code/SparkplugDX/spDXAudioManager.h"
#include "Code/SparkplugDX/spDXAudioBankEntry.h"
#include "Analysis/Host/spAudioManagerHost.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    struct Watch{const void* address=nullptr;unsigned kind=0,index=0,frees=0;};
    struct Event{unsigned kind=0,index=0;};
    std::array<Watch,20> watched{};unsigned watchCount=0;
    std::array<Event,40> events{};unsigned eventCount=0;bool tracking=false;
    void Released(void* address) noexcept
    {
        if(!tracking)return;
        for(unsigned i=0;i<watchCount;++i)if(watched[i].address==address)
        {++watched[i].frees;if(eventCount==events.size())std::abort();events[eventCount++]={watched[i].kind,watched[i].index};}
    }
}
void* operator new(std::size_t n){if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{Released(p);std::free(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}

using namespace sparkplug::reconstruction;
namespace
{
    using Profile=spAudioManagerPcRegistryProfileForAnalysis;
    constexpr auto Populated=Profile::StableOwnedCapturedEntriesInactiveNullDevice;
    constexpr auto EntryProfile=spAudioBankEntry::CapturedEmptyProfileForAnalysis::NonzeroUnassignedE0WithNullResources;
    constexpr auto BankProfile=spDXAudioBank::CapturedOwnedEntriesProfileForAnalysis::StableUniqueCapturedEntries;
    using State=std::array<std::uint32_t,24>;
    using BankState=std::array<std::uint32_t,4>;
    struct Input{std::uint32_t id=0,word14=0;std::vector<unsigned> names,order;};
    struct NameOwner final:spNamedObject{void ShareTo(spNamedObject& other)const noexcept{CopyNameToForAnalysis(other);}};
    const char* Names[]={nullptr,"manager-retained-name-A-with-real-shared-external-owner-over-forty-eight-characters",
        "manager-retained-name-B-with-real-shared-external-owner-over-forty-eight-characters"};
    const std::array<std::uint32_t,8> Words{0,1,0xFFFFFFFF,123,7,9,0x80000000,0xA5A5A5A5};
    unsigned checks=0;
    void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
    void ResetWatch(){tracking=false;watched={};events={};watchCount=eventCount=0;}
    void AddWatch(const void* address,unsigned kind,unsigned index)
    {Check(address&&watchCount<watched.size(),"Live allocation to watch");watched[watchCount++]={address,kind,index,0};}
    State Capture(const spDXAudioBankEntry& value)
    {
        const auto& a=value.GetStateForAnalysis();const auto& b=value.GetDxStateForAnalysis();
        State out{a.rawByte14,a.rawWord18,a.rawWord1C,a.rawWord20,a.rawWord24,a.rawByte28,a.rawWord2C,a.rawByte30};
        for(unsigned i=0;i<11;++i)out[8+i]=b.rawWords34To5C[i];
        out[19]=b.rawWord64;out[20]=b.rawWord68;out[21]=b.rawWord94;out[22]=b.rawWordE4;out[23]=b.rawWordF4;return out;
    }
    BankState Capture(const spAudioBank& value)
    {return {value.GetStateForAnalysis().identifier10,value.GetStateForAnalysis().rawWord14,
        static_cast<std::uint32_t>(value.GetEntryCountForAnalysis()),static_cast<std::uint32_t>(value.GetReservedEntrySlotsForAnalysis())};}
    std::unique_ptr<spDXAudioBank> PlainBank(unsigned count,std::uint32_t id)
    {
        std::vector<std::unique_ptr<spDXAudioBankEntry>> entries;
        for(unsigned i=0;i<count;++i)entries.push_back(std::make_unique<spDXAudioBankEntry>(EntryProfile));
        auto bank=std::make_unique<spDXAudioBank>(BankProfile,std::move(entries));bank->SetStateForFixture({id,5});return bank;
    }
    template<class Range>void Numbers(const Range& values)
    {std::cout<<'[';bool comma=false;for(auto value:values){if(comma)std::cout<<',';comma=true;std::cout<<value;}std::cout<<']';}
    template<class Range>void Rows(const Range& values)
    {std::cout<<'[';bool comma=false;for(const auto& value:values){if(comma)std::cout<<',';comma=true;Numbers(value);}std::cout<<']';}
    void One(const std::vector<Input>& inputs,std::uint32_t key,bool emit)
    {
        Check(!inputs.empty()&&inputs.size()<=2,"Captured bank dimensions");
        std::array<std::unique_ptr<NameOwner>,2> external;
        std::vector<spDXAudioBankEntry*> entries;std::vector<spDXAudioBank*> banks;
        spAudioManagerCapturedPcRegistryForAnalysis capture;capture.profile=Populated;
        for(const auto& input:inputs)
        {
            Check(input.names.size()==input.order.size()&&entries.size()+input.names.size()<=8,"Captured Entry dimensions");
            std::vector<std::unique_ptr<spDXAudioBankEntry>> created;
            for(auto name:input.names)
            {
                Check(name<=2,"Captured plain Name identity");auto entry=std::make_unique<spDXAudioBankEntry>(EntryProfile);
                entry->SetRawWord18ForFixture(Words[entries.size()]);entries.push_back(entry.get());
                if(name){auto& owner=external[name-1];if(!owner){owner=std::make_unique<NameOwner>();owner->SetName(Names[name]);}owner->ShareTo(*entry);}
                created.push_back(std::move(entry));
            }
            std::vector<std::unique_ptr<spDXAudioBankEntry>> ordered;unsigned seen=0;
            for(auto index:input.order){Check(index<created.size()&&!(seen&(1u<<index)),"Unique Entry permutation");seen|=1u<<index;ordered.push_back(std::move(created[index]));}
            auto bank=std::make_unique<spDXAudioBank>(BankProfile,std::move(ordered));bank->SetStateForFixture({input.id,input.word14});
            banks.push_back(bank.get());capture.banks.push_back(std::move(bank));
        }
        Check(!entries.empty(),"At least one populated bank");
        auto manager=std::make_unique<spDXAudioManager>(std::move(capture));
        Check(capture.banks.empty()&&manager->GetReservedBankSlotsForAnalysis()==20,"Validated transfer into real20-slot owner");
        std::vector<BankState> before,after;std::vector<std::vector<State>> entryStates;
        std::vector<std::vector<unsigned>> vectorsBefore,vectorsAfter,namesBefore,namesAfter;
        const auto entryID=[&](const spBaseObject* object,const std::array<bool,8>& dead)
        {for(unsigned i=0;i<entries.size();++i)if(!dead[i]&&entries[i]==object)return i;throw std::runtime_error("Live owned Entry identity");};
        const auto nameID=[&](const spDXAudioBankEntry& entry)
        {if(!entry.GetName())return 0u;for(unsigned i=0;i<2;++i)if(external[i]&&entry.GetName()==external[i]->GetName())return i+1;throw std::runtime_error("Shared retained Name identity");};
        std::array<bool,8> deadEntries{};
        for(unsigned b=0;b<banks.size();++b)
        {
            auto* bank=manager->GetBankForAnalysis(b);Check(bank==banks[b],"Manager preserves supplied Bank pointer");before.push_back(Capture(*bank));
            std::vector<State> states;std::vector<unsigned> ids,names;
            for(unsigned j=0;j<bank->GetEntryCountForAnalysis();++j)
            {auto* entry=bank->GetEntryForAnalysis(j);ids.push_back(entryID(entry,deadEntries));auto& typed=*static_cast<spDXAudioBankEntry*>(entry);states.push_back(Capture(typed));names.push_back(nameID(typed));}
            entryStates.push_back(std::move(states));vectorsBefore.push_back(std::move(ids));namesBefore.push_back(std::move(names));
        }
        ResetWatch();for(unsigned i=0;i<entries.size();++i)AddWatch(entries[i],0,i);
        for(unsigned i=0;i<banks.size();++i){AddWatch(banks[i]->GetEntryStorageForAnalysis(),1,i);AddWatch(banks[i],2,i);}
        for(unsigned i=0;i<2;++i)if(external[i])AddWatch(external[i]->GetName(),3,i+1);AddWatch(manager.get(),4,0);
        tracking=true;const bool completed=manager->RemoveBankPcForAnalysis(key);tracking=false;
        const auto selectedEvents=events;const auto selectedCount=eventCount;
        Check(completed&&!manager->IsRegistryPoisonedForAnalysis(),"Whole admitted source Remove completed");
        int removed=-1;unsigned bankFrees=0;
        for(unsigned i=0;i<watchCount;++i)
        {Check(watched[i].frees<=1,"Selected allocation freed at most once");if(watched[i].kind>=3)Check(watched[i].frees==0,"Manager and retained Names survive selected Remove");}
        for(unsigned i=0;i<selectedCount;++i)
        {const auto event=selectedEvents[i];if(event.kind==0)deadEntries[event.index]=true;else if(event.kind==2){removed=static_cast<int>(event.index);++bankFrees;}else Check(event.kind==1,"Selected frees are Entry/buffer/Bank only");}
        Check(bankFrees<=1,"At most one original Bank removed");
        bool namesSurvive=true;for(unsigned i=0;i<2;++i)if(external[i])namesSurvive&=std::strcmp(external[i]->GetName(),Names[i+1])==0;
        Check(namesSurvive,"External Name text remains live");
        std::vector<int> origins;
        for(unsigned b=0;b<manager->GetBankCountForAnalysis();++b)
        {
            const auto* bank=manager->GetBankForAnalysis(b);after.push_back(Capture(*bank));int origin=-1;
            // A new factory allocation may reuse the removed Bank's address.
            // Compare identity only with original owners still alive.
            for(unsigned i=0;i<banks.size();++i)if(static_cast<int>(i)!=removed&&bank==banks[i])origin=static_cast<int>(i);
            origins.push_back(origin);if(origin<0)Check(key==1&&Capture(*bank)==BankState{1,5,0,300},"Actual typed default Bank result");
            std::vector<unsigned> ids,names;
            for(unsigned j=0;j<bank->GetEntryCountForAnalysis();++j)
            {auto* entry=bank->GetEntryForAnalysis(j);ids.push_back(entryID(entry,deadEntries));names.push_back(nameID(*static_cast<spDXAudioBankEntry*>(entry)));}
            vectorsAfter.push_back(std::move(ids));namesAfter.push_back(std::move(names));
        }
        // Restart using only live allocations. Reused addresses never
        // impersonate a destroyed owner in the separate supporting witness.
        ResetWatch();
        for(unsigned i=0;i<manager->GetBankCountForAnalysis();++i)
        {const auto* bank=manager->GetBankForAnalysis(i);for(unsigned j=0;j<bank->GetEntryCountForAnalysis();++j)AddWatch(bank->GetEntryForAnalysis(j),0,j);
            AddWatch(bank->GetEntryStorageForAnalysis(),1,i);AddWatch(bank,2,i);}
        AddWatch(manager.get(),4,0);for(unsigned i=0;i<2;++i)if(external[i])AddWatch(external[i]->GetName(),3,i+1);
        tracking=true;manager.reset();tracking=false;
        for(unsigned i=0;i<watchCount;++i)Check(watched[i].frees==(watched[i].kind==3?0u:1u),"Supporting graph frees once and retained Names survive");
        ResetWatch();for(unsigned i=0;i<2;++i)if(external[i])AddWatch(external[i]->GetName(),3,i+1);
        tracking=true;for(auto& owner:external)owner.reset();tracking=false;
        for(unsigned i=0;i<watchCount;++i)Check(watched[i].frees==1,"Separate final source Name allocation free");ResetWatch();
        if(emit)
        {
            std::cout<<"{\"returned\":1,\"before\":";Rows(before);std::cout<<",\"after\":";Rows(after);std::cout<<",\"originsAfter\":";Numbers(origins);
            std::cout<<",\"vectorsBefore\":";Rows(vectorsBefore);std::cout<<",\"vectorsAfter\":";Rows(vectorsAfter);std::cout<<",\"namesBefore\":";Rows(namesBefore);std::cout<<",\"namesAfter\":";Rows(namesAfter);
            std::cout<<",\"entryStatesBefore\":[";for(unsigned i=0;i<entryStates.size();++i){if(i)std::cout<<',';Rows(entryStates[i]);}
            std::cout<<"],\"selectedRemovedIndex\":"<<removed<<",\"selectedFreeRoles\":[";
            for(unsigned i=0;i<selectedCount;++i){if(i)std::cout<<',';const auto event=selectedEvents[i];std::cout<<'\"'<<(event.kind==0?"entry:":event.kind==1?"bankBuffer:":"bank:")<<event.index<<'\"';}
            std::cout<<"],\"externalNamesSurvive\":true,\"supportingSourceGraphFreed\":true}\n";
        }
    }
    void Replay()
    {
        unsigned count;std::uint32_t key;
        while(std::cin>>count>>key)
        {
            Check(count>=1&&count<=2,"Bounded Manager replay header");std::vector<Input> inputs(count);
            for(auto& input:inputs){unsigned n;Check(bool(std::cin>>input.id>>input.word14>>n)&&n<=8,"Complete bank fields");input.names.resize(n);input.order.resize(n);
                for(auto& x:input.names)Check(bool(std::cin>>x),"Complete Name identities");for(auto& x:input.order)Check(bool(std::cin>>x),"Complete Entry order");}
            One(inputs,key,true);
        }
        Check(std::cin.eof(),"Complete populated Manager replay input");
    }
    void Rejected(spAudioManagerCapturedPcRegistryForAnalysis& input)
    {
        std::vector<const void*> pointers;for(auto& bank:input.banks)pointers.push_back(bank.get());bool refused=false;
        try{spDXAudioManager manager(std::move(input));}catch(const std::logic_error&){refused=true;}
        Check(refused&&input.banks.size()==pointers.size(),"Constructor refuses before consuming owners");
        for(unsigned i=0;i<pointers.size();++i)Check(input.banks[i].get()==pointers[i],"Caller retains every rejected Bank");
    }
    void Contracts()
    {
        One({{2,0xA5A50000,{0},{0}}},2,false);
        One({{1,0xA5A50000,{1,1,1},{2,1,0}},{2,0xA5A50001,{1,1,1,1,1},{1,2,3,4,0}}},1,false);
        One({{2,5,{},{}},{3,6,{1,2},{1,0}}},9,false);
        {spAudioManagerCapturedPcRegistryForAnalysis input;input.profile=Populated;input.banks.push_back(nullptr);Rejected(input);}
        {spAudioManagerCapturedPcRegistryForAnalysis input;input.profile=Populated;for(unsigned i=0;i<3;++i)input.banks.push_back(PlainBank(1,i));Rejected(input);}
        {spAudioManagerCapturedPcRegistryForAnalysis input;input.profile=Populated;input.banks.push_back(PlainBank(5,2));input.banks.push_back(PlainBank(4,3));Rejected(input);}
        {spAudioManagerCapturedPcRegistryForAnalysis input;input.profile=Populated;input.banks.push_back(PlainBank(0,2));Rejected(input);}
        {spAudioManagerCapturedPcRegistryForAnalysis input;input.profile=Profile::StableOwnedInactiveNullDevice;input.banks.push_back(PlainBank(1,2));Rejected(input);}
        {spAudioManagerCapturedPcRegistryForAnalysis input;input.banks.push_back(PlainBank(1,2));Rejected(input);}
        {
            spAudioManagerCapturedPcRegistryForAnalysis input;input.profile=Populated;input.banks.push_back(PlainBank(1,2));input.banks.emplace_back(input.banks.front().get());
            try{Rejected(input);}catch(...){if(input.banks.size()==2&&input.banks[0].get()==input.banks[1].get())(void)input.banks[1].release();throw;}
            (void)input.banks.back().release();input.banks.pop_back();
        }
        {
            spAudioManagerCapturedPcRegistryForAnalysis input;input.profile=Populated;input.banks.push_back(PlainBank(1,2));input.banks.push_back(PlainBank(1,3));spDXAudioManager manager(std::move(input));
            const auto* first=manager.GetBankForAnalysis(0);const auto* last=manager.GetBankForAnalysis(1);
            Check(!manager.RemoveBankPcForAnalysis(1)&&manager.GetBankCountForAnalysis()==2&&manager.GetBankForAnalysis(0)==first&&manager.GetBankForAnalysis(1)==last,
                "Key1 miss result3 refuses before deleting or growing");
        }
        {spAudioManagerCapturedPcRegistryForAnalysis input;input.profile=Populated;input.banks.push_back(PlainBank(1,2));spDXAudioManager manager(std::move(input));
            Check(manager.RemoveBankPcForAnalysis(2)&&manager.GetBankCountForAnalysis()==0&&!manager.RemoveBankPcForAnalysis(2),"Total0 leaves new-mode operation domain; lifetime remains valid");}
    }
}
int main(int argc,char** argv)
{
    try{if(argc==2&&std::string(argv[1])=="--replay")Replay();else{Contracts();std::cout<<"AudioManager owned Entry contracts passed checks="<<checks<<'\n';}return 0;}
    catch(const std::exception& error){tracking=false;std::cerr<<"AudioManager owned Entries "<<error.what()<<'\n';return 1;}
}
