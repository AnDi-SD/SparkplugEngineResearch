#include "Code/wxBirdFlyingState.h"
#include "Analysis/Host/wxBirdFlyingStateHost.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    void Check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
    void* Pointer(std::uintptr_t value){return reinterpret_cast<void*>(value);}
    std::uintptr_t Token(void* value){return reinterpret_cast<std::uintptr_t>(value);}
    struct Host final:wxBirdFlyingStateHost
    {
        wxBirdFlyingState& state;wxAnimationRequestForAnalysis& request;
        std::uintptr_t next=22,first=0,second=0;bool predicate=false;
        std::uint32_t control=0xffffffff;unsigned takeoff=255;std::string tag="unknown",trace;
        Host(wxBirdFlyingState& s,wxAnimationRequestForAnalysis& r):state(s),request(r){}
        unsigned Flags() const{auto flags=state.GetTransitionFlagsForAnalysis();unsigned n=0;for(unsigned i=0;i<5;++i)if(flags[i])n|=1u<<i;return n;}
        void Event(const std::string& name)
        {if(!trace.empty())trace+=';';trace+=name+'@'+std::to_string(Token(state.GetPendingHandleForAnalysis()))+':'+std::to_string(Flags())+':'+std::to_string(request.packedKey);}
        void* ResolveAnimationForAnalysis(void*,std::uint32_t key) override{Event("lookup:"+std::to_string(key));return Pointer(next);}
        bool OwnerPredicateForAnalysis(void*) override{Event("predicate");return predicate;}
        void ResetCompletionForAnalysis(void*,void* h) override{Event("reset:"+std::to_string(Token(h)));}
        void StartAnimationForAnalysis(void*,void* h,bool mode,std::uint32_t fade,bool interrupt) override
        {Event("start:"+std::to_string(Token(h))+':'+std::to_string(mode)+':'+std::to_string(fade)+':'+std::to_string(interrupt));}
        void StopAnimationForAnalysis(void*,void* h) override{Event("stop:"+std::to_string(Token(h)));}
        void FadeAnimationForAnalysis(void*,void* h,float value) override{Check(value==0.4f,"fade word");Event("fade:"+std::to_string(Token(h)));}
        bool IsPendingAnimationCompleteForAnalysis(void*,void* h,bool consume) override
        {
            Check(consume,"completion consumption");auto token=Token(h);Event("query:"+std::to_string(token));
            if(!token)return true;const bool result=first==token||second==token;
            if(first==token)first=0;if(second==token)second=0;return result;
        }
        void ClearOwnerActionControlForAnalysis(void*) override{Event("word");control=0;}
        const char* EventTagNameForAnalysis(const void*) override{return tag.c_str();}
        void ClearTakeoffFlagForAnalysis(void*) override{Event("takeoff");takeoff=0;}
    };
    std::string Case(unsigned slot,std::uint32_t key,std::uintptr_t pending,std::uintptr_t next,bool predicate,unsigned flags,std::uintptr_t first,std::uintptr_t second,const std::string& tag)
    {
        wxBirdFlyingState state;wxAnimationRequestForAnalysis request{key};Host host(state,request);
        Check(state.GetStateSelectorForAnalysis()==20,"original selector20");
        state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);state.SetPendingHandleForAnalysis(Pointer(pending));
        state.SetTransitionFlagsForAnalysis(flags&1,flags&2,flags&4,flags&8,flags&16);
        host.next=next;host.predicate=predicate;host.first=first;host.second=second;host.tag=tag;unsigned result=2;
        if(slot==0x1c)result=state.vfunc_1C(request);else if(slot==0x20)result=state.vfunc_20(request);
        else if(slot==0x30)state.vfunc_30(request);else if(slot==0x3c)state.vfunc_3C(nullptr);
        else if(slot==0x34)result=state.vfunc_34(key);else if(slot==0x38)result=state.vfunc_38(key);
        else if(slot==0x40)state.vfunc_40_ResetForAnalysis();else throw std::logic_error("Unknown slot");
        std::ostringstream out;out<<result<<' '<<request.packedKey<<' '<<Token(state.GetPendingHandleForAnalysis())<<' '<<host.control<<' '<<host.first<<' '<<host.second<<' '<<host.Flags()<<' '<<host.takeoff<<'|'<<host.trace;return out.str();
    }
    void Lifecycle()
    {
        wxBirdFlyingState state;state.SetTransitionFlagsForAnalysis(false,false,false,false,false);state.SetPendingHandleForAnalysis(Pointer(11));
        spCloneManager manager;auto clone=state.vfunc_10(manager);Check(bool(clone)&&manager.FindClone(state)==clone.get(),"clone registration");
        auto& s=static_cast<wxBirdFlyingState&>(*clone);Check(s.GetStateSelectorForAnalysis()==20&&s.GetPendingHandleForAnalysis()==nullptr&&s.GetTransitionFlagsForAnalysis()==std::array<bool,5>{true,true,true,true,true},"fresh clone runtime");
        Check(s.IsExactly(wxBirdFlyingState::ClassID)&&s.IsKindOf(wxCharacterState::ClassID),"physical RTTI base");
        Check(bool(spRTTIManager::Instance().Create(wxBirdFlyingState::ClassID)),"registered factory");
        wxAnimationRequestForAnalysis request;bool rejected=false;
        try{state.vfunc_1C(request);}catch(const std::logic_error&){rejected=true;}
        Check(rejected,"missing host rejected");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==11&&std::string(argv[1])=="--case")
        {
            std::uint64_t v[8];for(unsigned i=0;i<8;++i)v[i]=std::stoull(argv[i+2]);
            std::cout<<Case(unsigned(v[0]),std::uint32_t(v[1]),v[2],v[3],v[4]!=0,unsigned(v[5]),v[6],v[7],argv[10])<<'\n';return 0;
        }
        for(unsigned slot:{0x1cu,0x20u,0x30u})for(unsigned flags:{0u,1u,2u,4u,31u})
            for(auto pending:{0u,11u})for(auto next:{0u,11u,22u})for(bool pred:{false,true})
                (void)Case(slot,0x87654321,pending,next,pred,flags,11,11,"unknown");
        Check(Case(0x3c,0,0,0,false,0,0,0,"event_takeoff").find("takeoff")!=std::string::npos,"takeoff event");Lifecycle();
        std::cout<<"PASS BirdFlying operations and lifetime\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
