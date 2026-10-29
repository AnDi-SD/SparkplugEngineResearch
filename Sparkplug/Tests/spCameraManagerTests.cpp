#include "Code/Sparkplug/spCameraManager.h"
#include "Analysis/PC/spCameraManagerAbi.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

// Observers compare allocation addresses only. The sole object query in a
// delete observer is the independent STATIC singleton; no dead object is read.
namespace
{
    struct AllocationWatch final
    {
        const void* address=nullptr;unsigned frees=0;bool armed=false;
        const void* singletonAtFree=nullptr;
    };
    std::array<AllocationWatch,16> watched{};unsigned watchCount=0;
    void Released(void* address) noexcept
    {
        for(unsigned i=0;i<watchCount;++i)if(watched[i].armed&&watched[i].address==address)
        {
            // Stop watching this allocation generation on its first release;
            // a later test snapshot may legitimately reuse the freed address.
            ++watched[i].frees;watched[i].armed=false;
            watched[i].singletonAtFree=sparkplug::reconstruction::spCameraManager::GetInstance();
        }
    }
}
void* operator new(std::size_t bytes)
{if(auto* value=std::malloc(bytes?bytes:1))return value;throw std::bad_alloc();}
void operator delete(void* value) noexcept{Released(value);std::free(value);}
void operator delete(void* value,std::size_t) noexcept{::operator delete(value);}
void* operator new[](std::size_t bytes){return ::operator new(bytes);}
void operator delete[](void* value) noexcept{::operator delete(value);}
void operator delete[](void* value,std::size_t) noexcept{::operator delete(value);}

using namespace sparkplug::reconstruction;
namespace
{
    using Captured=spCameraManagerCapturedRegistryForAnalysis;
    unsigned checks=0;
    void Check(bool value,const char* message)
    {++checks;if(!value)throw std::runtime_error(message);}
    struct State final
    {
        std::uint32_t field14=0;std::optional<std::uint32_t> opaque18;
        std::vector<std::uint32_t> words;
        bool operator==(const State& other) const
        {return field14==other.field14&&opaque18==other.opaque18&&words==other.words;}
    };
    State Snapshot(const spCameraManager& value)
    {
        Check(value.GetHostWordCapacityForAnalysis()>=32,"Physical host capacity at least32, not native128B equality");
        return {value.GetField14ForAnalysis(),value.GetCapturedOpaque18ForAnalysis(),value.GetRawRegistryWordsForAnalysis()};
    }
    void WriteState(const State& value)
    {
        std::cout<<"{\"field14\":"<<value.field14<<",\"opaque18\":";
        if(value.opaque18)std::cout<<*value.opaque18;else std::cout<<"null";
        std::cout<<",\"words\":[";
        for(std::size_t i=0;i<value.words.size();++i){if(i)std::cout<<',';std::cout<<value.words[i];}
        std::cout<<"],\"logicalSlots\":"<<spCameraManager::InitialLogicalWordSlotsForAnalysis<<'}';
    }
    void WriteOptionalState(const std::optional<State>& value)
    {if(value)WriteState(*value);else std::cout<<"null";}
    void Assign(spCameraManager& value,const State& state)
    {
        Check(state.words.size()<=8,"Bounded raw metadata input");Captured input;
        input.profile=Captured::Profile::PCNonOwningRawWords;input.field14=state.field14;
        input.capturedOpaque18=state.opaque18;input.count=state.words.size();
        for(std::size_t i=0;i<input.count;++i)input.rawWords[i]=state.words[i];
        const void* storage=value.GetHostStorageForAnalysis();const auto capacity=value.GetHostWordCapacityForAnalysis();
        Check(value.ApplyCapturedRegistryForAnalysis(input),"Explicit captured-input admission");
        Check(value.GetHostStorageForAnalysis()==storage&&value.GetHostWordCapacityForAnalysis()==capacity,
            "Captured metadata fits existing reserved storage without growth");
        Check(Snapshot(value)==state,"Exact raw metadata admission, including known zero versus unknown18");
    }
    struct Owner final
    {
        std::unique_ptr<spBaseObject> owned;void* callerStorage=nullptr;spCameraManager* live=nullptr;
        void Create(bool deleting)
        {
            if(deleting){auto value=std::make_unique<spCameraManager>();live=value.get();owned=std::move(value);}
            else{callerStorage=::operator new(sizeof(spCameraManager));live=new(callerStorage)spCameraManager();}
        }
        void Adopt(std::unique_ptr<spBaseObject> value)
        {owned=std::move(value);live=dynamic_cast<spCameraManager*>(owned.get());Check(live!=nullptr,"Actual final Camera clone object");}
        void Destroy()
        {
            Check(live!=nullptr,"Destroy only a live Camera object");
            if(owned)owned.reset();else live->~spCameraManager();
            live=nullptr;
        }
        void ReleaseCallerStorage()
        {Check(callerStorage!=nullptr&&!live,"Separate flag0 caller releases storage after lifetime end");::operator delete(callerStorage);callerStorage=nullptr;}
        ~Owner()
        {
            if(live){if(owned)owned.reset();else live->~spCameraManager();}
            if(callerStorage)::operator delete(callerStorage);
        }
    };
    unsigned Watch(const void* address)
    {
        Check(address&&watchCount<watched.size(),"Live backing/object/name allocation to watch");
        for(unsigned i=0;i<watchCount;++i)Check(watched[i].address!=address,"Distinct simultaneously live storage witnesses");
        const auto slot=watchCount++;watched[slot]={address,0,true,nullptr};return slot;
    }
    unsigned Identity(const void* value,const void* source,const void* other)
    {if(!value)return 0;if(value==source)return 1;if(other&&value==other)return 2;throw std::runtime_error("Unexpected singleton identity");}
    struct LifetimeResult final
    {
        unsigned which=0,flag=0,publishedBefore=0;
        bool backingBeforeClear=false,objectAfterClear=false,globalZero=false,otherLiveUnchanged=false;
        unsigned bufferFrees=0,objectDtorFrees=0,objectCallerFrees=0;
    };
    void One(unsigned mode,unsigned flag,bool olderFirst,const State& supplied,const State& destination,bool emit)
    {
        Check(mode<=3&&flag<=1&&(!olderFirst||mode==1),"Bounded lifetime/Copy/Clone composition");
        Check(supplied.words.size()<=8&&destination.words.size()<=8,"Bounded captured raw counts");
        watchCount=0;watched={};
        // Independent live nonownership controls. Their host addresses are
        // never truncated or put in raw32 metadata. These names are host
        // ownership witnesses, not an asserted native Camera element type.
        constexpr const char* text="independent-camera-metadata-nonownership-control-name-longer-than-sixty-four-characters";
        std::array<std::unique_ptr<spNamedObject>,4> controls;
        for(auto& control:controls){control=std::make_unique<spNamedObject>();control->SetName(text);}
        spCloneManager clones;Owner source,other;source.Create(flag!=0);
        const void* sourceAddress=source.live;const State initial=Snapshot(*source.live);
        Check(initial==State{}&&spCameraManager::GetInstance()==source.live,"Default field14/unknown18/empty registry and complete singleton");
        Assign(*source.live,supplied);const auto before=Snapshot(*source.live);
        if(mode==1){other.Create(true);Assign(*other.live,destination);}
        const std::optional<State> otherBefore=other.live?std::optional<State>(Snapshot(*other.live)):std::nullopt;
        const void* beforeSingleton=spCameraManager::GetInstance();bool returned=true,mapped=false;
        if(mode==1||mode==2)returned=source.live->vfunc_14(mode==1?*other.live:*source.live,clones);
        else if(mode==3)
        {other.Adopt(source.live->vfunc_10(clones));returned=other.live!=nullptr;mapped=clones.FindClone(*source.live)==other.live;Check(mapped,"Existing common CloneManager contains the actual source/clone pair");}
        const void* otherAddress=other.live;const auto after=Snapshot(*source.live);
        const std::optional<State> otherAfter=other.live?std::optional<State>(Snapshot(*other.live)):std::nullopt;
        Check(returned&&before==after,"Inherited Base Copy/Clone preserves source own metadata");
        Check(mode!=1||otherBefore==otherAfter,"Inherited Base Copy preserves destination own metadata");
        Check(mode!=3||otherAfter==std::optional<State>(State{}),"Clone has fresh empty/default field14 and unknown18");
        Check(mode==3?spCameraManager::GetInstance()==other.live:spCameraManager::GetInstance()==beforeSingleton,
            "Only new construction/Clone overwrites singleton; Copy preserves it");
        const auto singletonBefore=Identity(beforeSingleton,sourceAddress,otherAddress);
        const auto singletonAfter=Identity(spCameraManager::GetInstance(),sourceAddress,otherAddress);
        std::array<unsigned,2> bufferSlots{},objectSlots{};
        bufferSlots[0]=Watch(source.live->GetHostStorageForAnalysis());objectSlots[0]=Watch(sourceAddress);
        if(other.live){bufferSlots[1]=Watch(other.live->GetHostStorageForAnalysis());objectSlots[1]=Watch(otherAddress);}
        std::array<unsigned,4> controlSlots{},nameSlots{};
        for(unsigned i=0;i<4;++i){controlSlots[i]=Watch(controls[i].get());nameSlots[i]=Watch(controls[i]->GetName());}
        std::array<unsigned,2> order{1,0};unsigned lives=other.live?2:1;
        if(!other.live)order[0]=0;else if(olderFirst)order={0,1};
        std::array<LifetimeResult,2> results{};
        for(unsigned i=0;i<lives;++i)
        {
            const auto which=order[i];Owner& dying=which?other:source;Owner& survivor=which?source:other;
            const auto selectedFlag=which?1u:flag;const void* published=spCameraManager::GetInstance();
            const std::optional<State> survivorBefore=survivor.live?std::optional<State>(Snapshot(*survivor.live)):std::nullopt;
            const void* survivorStorage=survivor.live?survivor.live->GetHostStorageForAnalysis():nullptr;
            dying.Destroy();
            auto& result=results[i];result.which=which+1;result.flag=selectedFlag;result.publishedBefore=Identity(published,sourceAddress,otherAddress);
            result.bufferFrees=watched[bufferSlots[which]].frees;
            result.backingBeforeClear=result.bufferFrees==1&&watched[bufferSlots[which]].singletonAtFree==published;
            result.objectDtorFrees=watched[objectSlots[which]].frees;
            result.globalZero=spCameraManager::GetInstance()==nullptr;
            Check(result.backingBeforeClear&&result.globalZero,"Backing storage frees before unconditional independent static clear");
            Check(result.objectDtorFrees==selectedFlag,"Flag0 explicit destructor does not release object storage");
            result.otherLiveUnchanged=!survivorBefore||(survivor.live&&Snapshot(*survivor.live)==*survivorBefore&&survivor.live->GetHostStorageForAnalysis()==survivorStorage);
            Check(result.otherLiveUnchanged,"Other still-live metadata/storage survives unconditional singleton clear");
            if(!selectedFlag){dying.ReleaseCallerStorage();result.objectCallerFrees=1;}
            result.objectAfterClear=watched[objectSlots[which]].frees==1&&watched[objectSlots[which]].singletonAtFree==nullptr;
            Check(result.objectAfterClear,"Actual object or separate caller storage release occurs after clear");
        }
        bool controlsSurvive=true;
        for(unsigned i=0;i<4;++i)controlsSurvive&=watched[controlSlots[i]].frees==0&&watched[nameSlots[i]].frees==0&&std::strcmp(controls[i]->GetName(),text)==0;
        Check(controlsSurvive,"Independent retained Named controls remain live and unchanged");
        for(unsigned i=0;i<4;++i){controls[i].reset();Check(watched[controlSlots[i]].frees==1&&watched[nameSlots[i]].frees==1,"External control/name owner releases its own storage separately");}
        watchCount=0;
        if(emit)
        {
            std::cout<<"{\"mode\":"<<mode<<",\"initial\":";WriteState(initial);std::cout<<",\"before\":";WriteState(before);
            std::cout<<",\"after\":";WriteState(after);std::cout<<",\"otherBefore\":";WriteOptionalState(otherBefore);
            std::cout<<",\"otherAfter\":";WriteOptionalState(otherAfter);
            std::cout<<",\"returned\":"<<returned<<",\"mapped\":"<<mapped<<",\"singletonBefore\":"<<singletonBefore<<",\"singletonAfter\":"<<singletonAfter<<",\"teardown\":[";
            for(unsigned i=0;i<lives;++i)
            {
                if(i)std::cout<<',';const auto& r=results[i];
                std::cout<<"{\"which\":"<<r.which<<",\"flag\":"<<r.flag<<",\"publishedBefore\":"<<r.publishedBefore
                    <<",\"bufferFrees\":"<<r.bufferFrees<<",\"objectDtorFrees\":"<<r.objectDtorFrees<<",\"objectCallerFrees\":"<<r.objectCallerFrees
                    <<",\"backingBeforeClear\":"<<r.backingBeforeClear<<",\"objectAfterClear\":"<<r.objectAfterClear
                    <<",\"globalZero\":"<<r.globalZero<<",\"otherLiveUnchanged\":"<<r.otherLiveUnchanged<<'}';
            }
            std::cout<<"],\"externalControlsSurvive\":"<<controlsSurvive<<",\"hostCapacityQualified\":1}\n";
        }
    }
    State ReadState()
    {
        State value;std::uint32_t raw18=0;unsigned count=0;
        Check(bool(std::cin>>value.field14>>raw18>>count)&&count<=8,"Complete bounded captured state");value.opaque18=raw18;value.words.resize(count);
        for(auto& word:value.words)Check(bool(std::cin>>word),"Complete raw32 word metadata");return value;
    }
    void Replay()
    {
        unsigned mode=0,flag=0,older=0;
        while(std::cin>>mode>>flag>>older)
        {Check(older<=1,"Bounded teardown ordering");const auto source=ReadState();const auto other=ReadState();One(mode,flag,older!=0,source,other,true);}
        Check(std::cin.eof(),"Complete Camera replay input");
    }
    void Contracts()
    {
        static_assert(!std::is_copy_constructible_v<spCameraManager>&&!std::is_copy_assignable_v<spCameraManager>);
        static_assert(!std::is_move_constructible_v<spCameraManager>&&!std::is_move_assignable_v<spCameraManager>);
        static_assert(std::is_final_v<spCameraManager>);
        static_assert(sizeof(sparkplug::evidence::pc::spCameraManagerLayout)==40);
        One(0,0,false,{7,0u,{0,1,0xffffffff,0x80000000}}, {},false);
        One(1,1,false,{7,0xccccccccu,{}},{9,0u,{0x12345678,0x12345678}},false);
        One(2,0,false,{0xffffffff,0xa5a5a5a5u,{1,1,0,2}}, {},false);
        One(3,1,false,{123,0u,{1,2,3,4,5,6,7,8}}, {},false);
        One(1,1,true,{0x12345678,0xabcdef12u,{1,1,2,3,4,5,6,7}},{8,9u,{0,1,2,3}},false);
        {
            spCameraManager value;Assign(value,{7,0u,{0,0xffffffff,123}});const auto before=Snapshot(value);
            const void* storage=value.GetHostStorageForAnalysis();const auto capacity=value.GetHostWordCapacityForAnalysis();
            Captured unqualified;unqualified.count=0;unqualified.field14=999;
            Check(!value.ApplyCapturedRegistryForAnalysis(unqualified)&&Snapshot(value)==before&&value.GetHostStorageForAnalysis()==storage,
                "Default unqualified capture refuses before mutation");
            unqualified.profile=Captured::Profile::PCNonOwningRawWords;unqualified.count=9;
            Check(!value.ApplyCapturedRegistryForAnalysis(unqualified)&&Snapshot(value)==before&&value.GetHostStorageForAnalysis()==storage&&value.GetHostWordCapacityForAnalysis()==capacity,
                "Count above8 refuses before storage/own-field effects");
            auto created=spRTTIManager::Instance().Create(spCameraManager::ClassID);auto* typed=dynamic_cast<spCameraManager*>(created.get());
            Check(typed&&typed->IsExactly(spCameraManager::ClassID)&&Snapshot(*typed)==State{}&&spCameraManager::GetInstance()==typed,"Actual portable factory creates default exact class");
            created.reset();Check(spCameraManager::GetInstance()==nullptr,"Newer factory object's teardown clears independently of still-live older object");
        }
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==2&&std::string(argv[1])=="--replay")Replay();
        else{Contracts();std::cout<<"CameraManager contracts passed checks="<<checks<<'\n';}
        return 0;
    }
    catch(const std::exception& error){watchCount=0;std::cerr<<"CameraManager "<<error.what()<<'\n';return 1;}
}
