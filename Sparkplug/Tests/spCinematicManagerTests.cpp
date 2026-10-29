#include "Analysis/Host/spCinematicManagerHost.h"
#include "Analysis/PC/spCinematicAbi.h"
#include "Analysis/PC/spCinematicManagerAbi.h"
#include "Code/Sparkplug/spCinematicManager.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

using namespace sparkplug::reconstruction;
namespace
{
    struct Watch{const void* address=nullptr;unsigned frees=0;};
    std::array<Watch,4> watched{};std::array<unsigned,8> freeOrder{};
    unsigned watchedCount=0,eventCount=0,allocationCalls=0,freeCalls=0;
    bool armed=false,selected=false,beforeUnlink=true;
    const spCinematicManager* observedManager=nullptr;
    std::size_t selectedIndex=0,beforeCount=0;
    void Released(void* address) noexcept
    {
        if(selected&&address)++freeCalls;
        if(!armed)return;
        for(unsigned i=0;i<watchedCount;++i)if(watched[i].address==address)
        {
            ++watched[i].frees;if(eventCount==freeOrder.size())std::abort();freeOrder[eventCount++]=i;
            // Pointer-only observation of live manager metadata. Never inspect
            // the destroyed Cinematic whose storage is being released.
            if(selected)beforeUnlink&=observedManager&&observedManager->GetCinematicCountForAnalysis()==beforeCount
                &&observedManager->GetCinematicForAnalysis(selectedIndex)==address;
        }
    }
}
void* operator new(std::size_t size){if(selected)++allocationCalls;if(auto* p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept{Released(p);std::free(p);}
void operator delete(void* p,std::size_t) noexcept{::operator delete(p);}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete[](void* p) noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept{::operator delete(p);}

namespace
{
    using Capture=spCinematicCapturedOwnersForAnalysis;
    using Context=spCinematicCapturedOwnerContextForAnalysis;
    constexpr auto Profile=Capture::Profile::EmptyCinematicsWithRetainedPlainNames;
    constexpr const char* Names[]={nullptr,"Cinematic-A","Cinematic-B","Cinematic","cinematic-a","Cinematic-A-tail","absent"};
    unsigned checks=0;
    void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
    struct NameOwner final:spNamedObject{void ShareTo(spNamedObject& value)const noexcept{CopyNameToForAnalysis(value);}};
    template<class F>void Refuses(F&& operation,const char* message)
    {bool refused=false;try{operation();}catch(const std::logic_error&){refused=true;}Check(refused,message);}

    void One(const std::vector<unsigned>& ids,unsigned key,bool emit)
    {
        Check(ids.size()<=4&&key>=1&&key<=6,"Replay captured dimensions");
        std::array<std::unique_ptr<NameOwner>,4> names;
        std::array<const spCinematic*,4> initial{};
        Capture input;input.profile=Profile;
        for(std::size_t i=0;i<ids.size();++i)
        {
            Check(ids[i]>=1&&ids[i]<=4,"Retained captured Name identity");
            auto object=std::make_unique<spCinematic>();initial[i]=object.get();
            Check(object->IsExactly(spCinematic::ClassID)&&object->IsKindOf(spBaseObject::ClassID)
                &&!object->IsKindOf(spNamedObject::ClassID),"Physical Named and engine registration are distinct");
            Check(object->GetName()==nullptr&&object->GetField14ForAnalysis()==0&&object->GetOwnedRecordCountForAnalysis()==0,"Empty Cinematic constructor");
            auto& owner=names[ids[i]-1];if(!owner){owner=std::make_unique<NameOwner>();owner->SetName(Names[ids[i]]);}
            owner->ShareTo(*object);input.owners.push_back(std::move(object));
        }
        auto context=std::make_unique<Context>(std::move(input));
        struct ObserverScope{~ObserverScope(){selected=false;armed=false;observedManager=nullptr;}} observerScope;
        Check(context->GetOwnedCountForAnalysis()==ids.size()&&!context->IsBoundForAnalysis(),"Context alone owns payloads");
        std::size_t match=ids.size();for(std::size_t i=0;i<ids.size();++i)if(std::strcmp(Names[ids[i]],Names[key])==0){match=i;break;}
        const bool hit=match<ids.size();
        watched={};watchedCount=static_cast<unsigned>(ids.size());eventCount=allocationCalls=freeCalls=0;beforeUnlink=true;
        for(unsigned i=0;i<watchedCount;++i)watched[i].address=initial[i];
        std::array<long long,19> row{};std::size_t cell=0;
        std::array<int,4> survivorIndices{-1,-1,-1,-1};std::array<unsigned,4> logicalNameCounts{};
        armed=true;
        {
            spCinematicManager manager(*context);observedManager=&manager;
            Check(manager.IsExactly(spCinematicManager::ClassID)&&manager.IsKindOf(spBaseObject::ClassID),"Captured Manager literal identity");
            Check(context->IsBoundForAnalysis()&&manager.GetCinematicCountForAnalysis()==ids.size(),"One borrowed binding");
            for(std::size_t i=0;i<ids.size();++i)Check(manager.GetCinematicForAnalysis(i)==initial[i],"Initial ordered borrowed slots");
            beforeCount=ids.size();selectedIndex=match;
            selected=true;const bool returned=manager.RemoveByNamePcForAnalysis(Names[key]);selected=false;
            Check(returned==hit&&eventCount==unsigned(hit)&&freeCalls==unsigned(hit)&&allocationCalls==0,"Selected actual C++ delete/no-growth witness");
            Check(beforeUnlink,"Selected real object free precedes registry unlink");
            Check(manager.GetCinematicCountForAnalysis()==ids.size()-hit&&context->GetOwnedCountForAnalysis()==ids.size()-hit,"Remove count and external ownership agree");
            unsigned next=0;
            for(unsigned i=0;i<ids.size();++i)
            {
                Check(watched[i].frees==unsigned(hit&&i==match),"Only selected payload storage released");
                if(hit&&i==match){Check(context->GetOwnedForAnalysis(i)==nullptr,"Consumed context owner is empty");continue;}
                const auto* live=context->GetOwnedForAnalysis(i);Check(live==initial[i]&&manager.GetCinematicForAnalysis(next)==live,"Survivors remain ordered and owned");
                Check(live->GetField14ForAnalysis()==0&&live->GetOwnedRecordCountForAnalysis()==0
                    &&live->GetName()==names[ids[i]-1]->GetName(),"Live represented payload/Name state intact");
                survivorIndices[next++]=static_cast<int>(i);
            }
            for(unsigned identity=1;identity<=4;++identity)if(names[identity-1])
            {
                Check(std::strcmp(names[identity-1]->GetName(),Names[identity])==0,"External Name owner text survives");
                unsigned owners=1;
                for(std::size_t i=0;i<ids.size();++i)if(const auto* live=context->GetOwnedForAnalysis(i))owners+=live->GetName()==names[identity-1]->GetName();
                logicalNameCounts[identity-1]=owners; // Logical owner graph, not shared_ptr ABI/use_count.
            }
            row[cell++]=ids.size();row[cell++]=manager.GetCinematicCountForAnalysis();row[cell++]=hit?match+1:0;
            row[cell++]=returned;row[cell++]=eventCount;row[cell++]=allocationCalls;
            row[cell++]=0; // Structural absence of source clock calls, not a Timer measurement.
            for(auto value:survivorIndices)row[cell++]=value;
            for(auto value:logicalNameCounts)row[cell++]=value;
            for(unsigned i=0;i<4;++i)row[cell++]=1; // Above represented-state/lifetime/owner guards passed.
        }
        observedManager=nullptr;
        Check(!context->IsBoundForAnalysis()&&context->GetOwnedCountForAnalysis()==ids.size()-hit,"Manager destruction preserves external payload owners");
        Check(eventCount==unsigned(hit),"Manager destruction never deletes surviving payloads");
        for(unsigned i=0;i<ids.size();++i)if(!(hit&&i==match))
        {
            const auto* live=context->GetOwnedForAnalysis(i);
            Check(live==initial[i]&&live->GetName()==names[ids[i]-1]->GetName(),"Live context payload/Name after Manager destruction");
        }
        context.reset(); // Separate caller cleanup of payloads, not Manager close.
        Check(eventCount==ids.size(),"Separate caller context disposal releases all surviving objects");
        unsigned next=hit?1:0;
        for(unsigned i=0;i<ids.size();++i)
        {Check(watched[i].frees==1,"Every real payload storage freed once");if(!(hit&&i==match))Check(freeOrder[next++]==i,"Separate caller cleanup order");}
        for(unsigned i=0;i<4;++i)if(names[i])Check(std::strcmp(names[i]->GetName(),Names[i+1])==0,"External Names remain after payload disposal");
        armed=false;Check(cell==row.size(),"Logical projection row width");
        if(emit){std::cout<<'[';for(std::size_t i=0;i<row.size();++i){if(i)std::cout<<',';std::cout<<row[i];}std::cout<<"]\n";}
    }
    std::unique_ptr<spCinematic> Plain(const char* name="contract-name")
    {auto object=std::make_unique<spCinematic>();object->SetName(name);return object;}
    void Rejected(Capture& input)
    {
        std::vector<const spCinematic*> pointers;for(const auto& owner:input.owners)pointers.push_back(owner.get());
        Refuses([&]{Context context(std::move(input));},"Invalid whole captured batch refuses");
        Check(input.owners.size()==pointers.size(),"Refusal retains whole input size");
        for(std::size_t i=0;i<pointers.size();++i)Check(input.owners[i].get()==pointers[i],"Refusal precedes first owner move");
    }
    void Contracts()
    {
        static_assert(!std::is_copy_constructible_v<spCinematic>&&!std::is_move_constructible_v<spCinematic>);
        static_assert(!std::is_copy_constructible_v<spCinematicManager>&&!std::is_move_constructible_v<spCinematicManager>);
        static_assert(!std::is_copy_constructible_v<Context>&&!std::is_move_constructible_v<Context>);
        spCinematic source,destination;source.SetName("source-name");destination.SetName("destination-name");spCloneManager clones;
        Refuses([&]{(void)source.vfunc_14(destination,clones);},"Cinematic own Copy is explicit unqualified refusal");
        Refuses([&]{(void)source.vfunc_14(source,clones);},"Cinematic self Copy is not silently inferred");
        Refuses([&]{(void)source.vfunc_10(clones);},"Cinematic own Clone is explicit unqualified refusal");
        Check(std::strcmp(source.GetName(),"source-name")==0&&std::strcmp(destination.GetName(),"destination-name")==0,"Copy/Clone refusal preserves Names");
        Check(!spCinematic::StaticRTTI().factory,"Cinematic captured source does not bind an implicit factory");
        {Capture input;input.owners.push_back(Plain());Rejected(input);}
        {Capture input;input.profile=Profile;input.owners.push_back(nullptr);Rejected(input);}
        {Capture input;input.profile=Profile;input.owners.push_back(Plain(nullptr));Rejected(input);}
        {Capture input;input.profile=Profile;input.owners.push_back(Plain(""));Rejected(input);}
        {Capture input;input.profile=Profile;input.owners.push_back(Plain("\xC3\xA9"));Rejected(input);}
        {Capture input;input.profile=Profile;std::string longName(64,'a');input.owners.push_back(Plain(longName.c_str()));Rejected(input);}
        {Capture input;input.profile=Profile;for(unsigned i=0;i<5;++i)input.owners.push_back(Plain());Rejected(input);}
        {Capture input;input.profile=Profile;input.owners.push_back(Plain());input.owners.emplace_back(input.owners[0].get());
            try{Rejected(input);}catch(...){if(input.owners.size()==2&&input.owners[0].get()==input.owners[1].get())(void)input.owners[1].release();throw;}
            (void)input.owners.back().release();input.owners.pop_back();}
        {
            Capture input;input.profile=Profile;input.owners.push_back(Plain());Context context(std::move(input));
            const auto* identity=context.GetOwnedForAnalysis(0);
            {spCinematicManager manager(context);Refuses([&]{spCinematicManager duplicate(context);},"One active Manager binding");
                Refuses([&]{(void)manager.vfunc_10(clones);},"Manager Clone requires unqualified original construction");
                Refuses([&]{(void)manager.RemoveByNamePcForAnalysis({});},"Empty query is outside captured domain");
                Refuses([&]{(void)manager.RemoveByNamePcForAnalysis(std::string_view("a\0b",3));},"Embedded-NUL query refuses");
                Check(manager.GetCinematicForAnalysis(0)==identity&&context.GetOwnedCountForAnalysis()==1,"All admission refusals precede effects");
                spBaseObject base;Check(manager.vfunc_14(base,clones),"Pinned literal inherited Base Copy only");}
            Check(context.GetOwnedForAnalysis(0)==identity&&!context.IsBoundForAnalysis(),"Manager destruction releases only binding");
            {spCinematicManager rebound(context);Check(rebound.GetCinematicForAnalysis(0)==identity,"Unconsumed context can rebind after Manager destruction");}
        }
        {
            std::unique_ptr<spCinematicManager> manager;
            {Capture input;input.profile=Profile;input.owners.push_back(Plain());auto context=std::make_unique<Context>(std::move(input));manager=std::make_unique<spCinematicManager>(*context);}
            Check(!manager->IsContextLiveForAnalysis()&&manager->GetCinematicForAnalysis(0)==nullptr,"Expired external context is never dereferenced");
            Refuses([&]{(void)manager->RemoveByNamePcForAnalysis("contract-name");},"Expired-context host safeguard refuses before effects");
            Check(manager->GetCinematicCountForAnalysis()==1,"Expired context refusal preserves borrowed metadata");manager.reset();
        }
        {
            Capture input;input.profile=Profile;auto object=Plain();auto* identity=object.get();input.owners.push_back(std::move(object));Context context(std::move(input));spCinematicManager manager(context);
            identity->SetName(nullptr);Refuses([&]{(void)manager.RemoveByNamePcForAnalysis("contract-name");},"All current Names revalidate before effects");
            Check(context.GetOwnedForAnalysis(0)==identity&&manager.GetCinematicCountForAnalysis()==1,"Changed input admission failure does not consume owner");
        }
        One({},1,false);One({3,1,2,1},1,false);One({1,1,1,1},1,false);One({4},1,false);
    }
    void Replay()
    {
        unsigned count,key;
        while(std::cin>>count>>key)
        {
            Check(count<=4,"Bounded replay owner count");std::vector<unsigned> ids(count);
            for(auto& identity:ids)Check(bool(std::cin>>identity),"Complete replay Name inputs");One(ids,key,true);
        }
    }
}
int main(int argc,char** argv)
{
    try{if(argc==2&&std::string_view(argv[1])=="--replay")Replay();else{Contracts();std::cout<<"CinematicManager captured Name contracts passed checks="<<checks<<'\n';}return 0;}
    catch(const std::exception& error){armed=selected=false;observedManager=nullptr;std::cerr<<"CinematicManager "<<error.what()<<'\n';return 1;}
}
