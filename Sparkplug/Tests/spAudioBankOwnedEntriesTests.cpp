#include "Code/SparkplugDX/spDXAudioBank.h"
#include "Code/SparkplugDX/spDXAudioBankEntry.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

// Allocation addresses are only compared as opaque values. No destroyed
// C++ Bank/Entry is read. Observer storage itself never allocates or reenters.
namespace
{
    struct Watch final{const void* address=nullptr;unsigned kind=0,index=0,frees=0;};
    struct Event final{unsigned kind=0,index=0;};
    std::array<Watch,12> watched{};unsigned watchCount=0;
    std::array<Event,32> events{};unsigned eventCount=0;
    void Released(void* address) noexcept
    {
        for(unsigned i=0;i<watchCount;++i)if(watched[i].address==address)
        {++watched[i].frees;if(eventCount==events.size())std::abort();events[eventCount++]={watched[i].kind,watched[i].index};}
    }
}
void* operator new(std::size_t bytes){if(auto* p=std::malloc(bytes?bytes:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{Released(p);std::free(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}

using namespace sparkplug::reconstruction;
namespace
{
    using EntryProfile=spAudioBankEntry::CapturedEmptyProfileForAnalysis;
    using BankProfile=spDXAudioBank::CapturedOwnedEntriesProfileForAnalysis;
    constexpr auto Captured=EntryProfile::NonzeroUnassignedE0WithNullResources;
    constexpr auto Stable=BankProfile::StableUniqueCapturedEntries;
    using Entries=std::vector<std::unique_ptr<spDXAudioBankEntry>>;
    using State=std::array<std::uint32_t,24>;
    const std::array<std::uint32_t,8> Words{0,1,0xFFFFFFFF,123,7,9,0x80000000,0xA5A5A5A5};
    const char* Names[]={nullptr,"audio-entry-retained-name-A-with-real-shared-owner-over-forty-eight-characters",
        "audio-entry-retained-name-B-with-distinct-live-owner-over-forty-eight-characters"};
    unsigned checks=0;
    void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
    struct NameOwner final:spNamedObject{void ShareTo(spNamedObject& value)const noexcept{CopyNameToForAnalysis(value);}};
    State Capture(const spDXAudioBankEntry& value)
    {
        const auto& a=value.GetStateForAnalysis();const auto& b=value.GetDxStateForAnalysis();
        State out{a.rawByte14,a.rawWord18,a.rawWord1C,a.rawWord20,a.rawWord24,a.rawByte28,a.rawWord2C,a.rawByte30};
        for(unsigned i=0;i<11;++i)out[8+i]=b.rawWords34To5C[i];
        out[19]=b.rawWord64;out[20]=b.rawWord68;out[21]=b.rawWord94;out[22]=b.rawWordE4;out[23]=b.rawWordF4;return out;
    }
    void WatchAddress(const void* address,unsigned kind,unsigned index)
    {Check(address&&watchCount<watched.size(),"Live allocation to watch");watched[watchCount++]={address,kind,index,0};}
    template<class Values>void Numbers(const Values& values)
    {std::cout<<'[';bool comma=false;for(auto value:values){if(comma)std::cout<<',';comma=true;std::cout<<value;}std::cout<<']';}
    void One(unsigned count,unsigned flag,const std::vector<unsigned>& names,const std::vector<unsigned>& order,bool emit)
    {
        Check(count<=8&&flag<=1&&names.size()==count&&order.size()==count,"Captured matrix dimensions");
        unsigned seen=0;for(auto index:order){Check(index<count&&!(seen&(1u<<index)),"Unique vector permutation");seen|=1u<<index;}
        std::array<std::unique_ptr<NameOwner>,2> externalNames;
        std::array<std::unique_ptr<spDXAudioBankEntry>,8> created;
        std::array<spDXAudioBankEntry*,8> pointers{};
        for(unsigned i=0;i<count;++i)
        {
            Check(names[i]<=2,"Captured null/retained name identity");created[i]=std::make_unique<spDXAudioBankEntry>(Captured);pointers[i]=created[i].get();
            created[i]->SetRawWord18ForFixture(Words[i]);
            if(auto identity=names[i])
            {auto& owner=externalNames[identity-1];if(!owner){owner=std::make_unique<NameOwner>();owner->SetName(Names[identity]);}owner->ShareTo(*created[i]);}
        }
        Entries input;input.reserve(count);for(auto index:order)input.push_back(std::move(created[index]));
        void* storage=nullptr;std::unique_ptr<spDXAudioBank> owner;spDXAudioBank* bank=nullptr;
        if(flag){owner=std::make_unique<spDXAudioBank>(Stable,std::move(input));bank=owner.get();}
        else {storage=::operator new(sizeof(spDXAudioBank));bank=new(storage)spDXAudioBank(Stable,std::move(input));}
        Check(bank->GetEntryCountForAnalysis()==count&&bank->GetReservedEntrySlotsForAnalysis()==300,"Actual live owners/fixed storage");
        Check(bank->GetStateForAnalysis().identifier10==0&&bank->GetStateForAnalysis().rawWord14==5,"Producer preserves Bank own defaults");
        for(auto& value:input)Check(!value,"All caller owners transferred after validation");
        for(unsigned i=0;i<count;++i)Check(bank->GetEntryForAnalysis(i)==pointers[order[i]],"Adoption preserves actual pointer identity/order");
        std::vector<State> before;std::vector<unsigned> orderedNames;
        for(auto index:order)
        {Check(pointers[index]->GetName()==(names[index]?externalNames[names[index]-1]->GetName():nullptr),"Entries share the retained name allocation");before.push_back(Capture(*pointers[index]));orderedNames.push_back(names[index]);}
        watched={};watchCount=eventCount=0;events={};
        for(unsigned i=0;i<count;++i)WatchAddress(pointers[i],0,i);
        WatchAddress(bank->GetEntryStorageForAnalysis(),1,0);WatchAddress(bank,2,0);
        for(unsigned i=0;i<2;++i)if(externalNames[i])WatchAddress(externalNames[i]->GetName(),3,i+1);
        if(flag)owner.reset();else bank->~spDXAudioBank();
        const auto selectedEvents=eventCount;Check(selectedEvents==count+1+flag,"Only Entries/buffer/optional Bank freed during selected teardown");
        for(unsigned i=0;i<count;++i)Check(events[i].kind==0&&events[i].index==order[i]&&watched[i].frees==1,"Forward virtual Entry ownership release once");
        Check(events[count].kind==1&&watched[count].frees==1,"Buffer released after all Entries");
        Check(watched[count+1].frees==flag,"Deleting Bank ownership matches flag");
        if(flag)Check(events[count+1].kind==2,"Bank allocation released after buffer");
        bool namesSurvive=true;
        for(unsigned i=0;i<2;++i)if(externalNames[i])namesSurvive&=std::strcmp(externalNames[i]->GetName(),Names[i+1])==0;
        Check(namesSurvive,"Retained external Name text remains alive");
        if(!flag){::operator delete(storage);Check(watched[count+1].frees==1,"External caller releases dead Bank storage once");}
        for(unsigned i=0;i<2;++i)if(externalNames[i])
        {
            const void* name=externalNames[i]->GetName();unsigned slot=0;while(slot<watchCount&&watched[slot].address!=name)++slot;
            Check(slot<watchCount&&watched[slot].frees==0,"Name allocation not freed by Bank");externalNames[i].reset();Check(watched[slot].frees==1,"Last portable external Name free is a separate host witness");
        }
        watchCount=0;
        if(emit)
        {
            std::cout<<"{\"before\":[0,5,"<<count<<",300],\"entries\":[";
            for(unsigned i=0;i<count;++i){if(i)std::cout<<',';Numbers(before[i]);}
            std::cout<<"],\"names\":";Numbers(orderedNames);std::cout<<",\"order\":";Numbers(order);
            std::cout<<",\"frees\":[";for(unsigned i=0;i<selectedEvents;++i)
            {if(i)std::cout<<',';std::cout<<'"';if(events[i].kind==0)std::cout<<"entry:"<<events[i].index;else std::cout<<(events[i].kind==1?"buffer":"bank");std::cout<<'"';}
            std::cout<<"],\"entryFrees\":"<<count<<",\"bufferFrees\":1,\"bankDtorFrees\":"<<flag<<",\"bankCallerFrees\":"<<unsigned(!flag)<<",\"namesSurvive\":true}\n";
        }
    }
    void Replay()
    {
        unsigned count,flag;
        while(std::cin>>count>>flag)
        {
            Check(count<=8&&flag<=1,"Bounded replay header");std::vector<unsigned> names(count),order(count);
            for(auto& value:names)Check(bool(std::cin>>value),"Complete captured names");for(auto& value:order)Check(bool(std::cin>>value),"Complete vector order");
            One(count,flag,names,order,true);
        }
        Check(std::cin.eof(),"Complete owned Entry replay input");
    }
    void Rejected(Entries& input,BankProfile profile,const char* label)
    {
        std::vector<const void*> before;for(auto& value:input)before.push_back(value.get());bool refused=false;
        try{spDXAudioBank invalid(profile,std::move(input));}catch(const std::logic_error&){refused=true;}
        Check(refused&&input.size()==before.size(),label);for(unsigned i=0;i<input.size();++i)Check(input[i].get()==before[i],"Refusal preserves caller ownership before moves");
    }
    void Contracts()
    {
        One(0,0,{}, {},false);One(3,1,{1,0,2},{2,0,1},false);One(8,0,std::vector<unsigned>(8,1),{7,6,5,4,3,2,1,0},false);
        {Entries input;input.push_back(nullptr);Rejected(input,Stable,"Null active entry refuses");}
        {Entries input;for(unsigned i=0;i<9;++i)input.push_back(std::make_unique<spDXAudioBankEntry>(Captured));Rejected(input,Stable,"More than8 refuses");}
        {Entries input;input.push_back(std::make_unique<spDXAudioBankEntry>(Captured));Rejected(input,static_cast<BankProfile>(255),"Unqualified profile refuses");}
        {
            Entries input;auto first=std::make_unique<spDXAudioBankEntry>(Captured);auto* pointer=first.get();input.push_back(std::move(first));input.emplace_back(pointer);
            try{Rejected(input,Stable,"Duplicate owner identity refuses before transfer");}
            catch(...){if(input.size()==2&&input[0].get()==input[1].get())(void)input[1].release();throw;}
            // Intentionally malformed caller input is repaired by its owner,
            // never destructed with two unique_ptr aliases to the same object.
            (void)input.back().release();input.pop_back();
        }
        {Entries input;input.push_back(std::make_unique<spDXAudioBankEntry>(Captured));spDXAudioBank populated(Stable,std::move(input));spCloneManager clones;bool refused=false;
            try{(void)populated.vfunc_10(clones);}catch(const std::logic_error&){refused=true;}
            Check(refused&&populated.GetEntryCountForAnalysis()==1,"Unqualified populated Clone refuses without losing owner");}
    }
}
int main(int argc,char** argv)
{
    try{if(argc==2&&std::string(argv[1])=="--replay")Replay();else{Contracts();std::cout<<"AudioBank owned Entry contracts passed checks="<<checks<<'\n';}return 0;}
    catch(const std::exception& error){std::cerr<<"AudioBank owned Entry "<<error.what()<<'\n';return 1;}
}
