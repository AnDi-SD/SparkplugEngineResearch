#include "Code/wxDroidMovingState.h"
#include "Code/wxSpiderMovingState.h"
#include "Code/wxIceBatIdleState.h"
#include "Code/wxSpiritFollowState.h"
#include "Code/wxMosquitoHurtState.h"
#include "Analysis/Host/wxCharacterMotionStateHost.h"
#include "Analysis/Host/wxCharacterSpeedStateHost.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    void Require(bool value, const char* message)
    { if (!value) { std::cerr << message << '\n'; std::exit(EXIT_FAILURE); } }
    void* Pointer(std::uintptr_t value) { return reinterpret_cast<void*>(value); }
    std::uintptr_t Token(void* value) { return reinterpret_cast<std::uintptr_t>(value); }
    unsigned Flags(const wxCharacterState& state)
    {
        unsigned result=0; const auto flags=state.GetTransitionFlagsForAnalysis();
        for (unsigned i=0;i<5;++i) if (flags[i]) result|=1u<<i;
        return result;
    }
    struct Host final : wxCharacterMotionStateHost, wxCharacterSpeedStateHost
    {
        wxCharacterState& state; wxAnimationRequestForAnalysis& request;
        void* next=Pointer(22); bool predicate=false;
        std::uint32_t word=0, speedBits=0x3F400000; std::uintptr_t first=0,second=0;
        std::string trace;
        Host(wxCharacterState& s,wxAnimationRequestForAnalysis& r) : state(s),request(r) {}
        void Event(const std::string& name)
        {
            if (!trace.empty()) trace+=';';
            trace+=name+'@'+std::to_string(Token(state.GetPendingHandleForAnalysis()))+':'
                +std::to_string(Flags(state))+':'+std::to_string(request.packedKey);
        }
        float OwnerMotionForAnalysis(void*) override
        { Event("motion"); float value; std::memcpy(&value,&word,sizeof(value)); return value; }
        void SetConsumerSpeedForAnalysis(void*,float speed) override
        { std::memcpy(&speedBits,&speed,sizeof(speed)); Event("speed:"+std::to_string(speedBits)); }
        void* ResolveAnimationForAnalysis(void*,std::uint32_t key) override
        { Event("lookup:"+std::to_string(key)); return next; }
        bool OwnerPredicateForAnalysis(void*) override { Event("predicate"); return predicate; }
        void ResetCompletionForAnalysis(void*,void* handle) override { Event("reset:"+std::to_string(Token(handle))); }
        void StartAnimationForAnalysis(void*,void* handle,bool mode,std::uint32_t fade,bool interrupt) override
        { Event("start:"+std::to_string(Token(handle))+':'+std::to_string(mode)+':'+std::to_string(fade)+':'+std::to_string(interrupt)); }
        void StopAnimationForAnalysis(void*,void* handle) override { Event("stop:"+std::to_string(Token(handle))); }
        void FadeAnimationForAnalysis(void*,void* handle,float duration) override
        { Require(duration==0.4f,"native fade"); Event("fade:"+std::to_string(Token(handle))); }
        bool IsPendingAnimationCompleteForAnalysis(void*,void* handle,bool consume) override
        {
            Require(consume,"consume completion"); const auto token=Token(handle); Event("query:"+std::to_string(token));
            if (!token) return true;
            const bool complete=first==token || second==token;
            if (first==token) first=0;
            if (second==token) second=0;
            return complete;
        }
        void ClearOwnerActionControlForAnalysis(void*) override { Event("word"); word=0; }
    };
    std::string Case(unsigned kind,unsigned slot,std::uint32_t key,std::uintptr_t pending,
        std::uintptr_t next,bool predicate,std::uintptr_t first,std::uintptr_t second,unsigned flags,std::uint32_t motion)
    {
        std::unique_ptr<wxCharacterState> state;
        if (kind==0) state=std::make_unique<wxDroidMovingState>();
        else if (kind==1) state=std::make_unique<wxSpiderMovingState>();
        else if (kind==2) state=std::make_unique<wxIceBatIdleState>();
        else if (kind==3) state=std::make_unique<wxSpiritFollowState>();
        else if (kind==4) state=std::make_unique<wxMosquitoHurtState>();
        else throw std::logic_error("unknown state kind");
        wxAnimationRequestForAnalysis request{key}; Host host(*state,request);
        state->SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host); state->SetPendingHandleForAnalysis(Pointer(pending));
        state->SetTransitionFlagsForAnalysis(flags&1,flags&2,flags&4,flags&8,flags&16);
        host.next=Pointer(next); host.predicate=predicate; host.first=first; host.second=second; host.word=motion;
        unsigned result=2;
        if (slot==0x1C) result=state->vfunc_1C(request);
        else if (slot==0x20) result=state->vfunc_20(request);
        else if (slot==0x30) state->vfunc_30(request);
        else if (slot==0x34) result=state->vfunc_34(key);
        else throw std::logic_error("unknown case slot");
        std::ostringstream output;
        output<<result<<' '<<request.packedKey<<' '<<Token(state->GetPendingHandleForAnalysis())
            <<' '<<host.word<<' '<<host.first<<' '<<host.second<<' '<<Flags(*state)<<' '<<host.speedBits<<'|'<<host.trace;
        return output.str();
    }
    template<class State> void CheckLifetime(unsigned selector)
    {
        State state; Require(state.GetStateSelectorForAnalysis()==selector && state.IsExactly(State::ClassID)
            && state.IsKindOf(wxCharacterState::ClassID),"selector and RTTI");
        Require(dynamic_cast<State*>(spRTTIManager::Instance().Create(State::ClassID).get()),"factory");
        state.SetPendingHandleForAnalysis(Pointer(11)); state.SetTransitionFlagsForAnalysis(false,false,false,false,false);
        auto clone=state.Clone(); auto* fresh=dynamic_cast<State*>(clone.get());
        Require(fresh && fresh->GetStateSelectorForAnalysis()==selector && !fresh->GetPendingHandleForAnalysis()
            && Flags(*fresh)==31,"clone has fresh flags and pending");
        State target; spCloneManager manager; target.SetPendingHandleForAnalysis(Pointer(33));
        Require(state.vfunc_14(target,manager) && target.GetPendingHandleForAnalysis()==Pointer(33),"empty Copy");
        state.vfunc_40_ResetForAnalysis(); Require(!state.GetPendingHandleForAnalysis(),"base reset");
    }
}
int main(int argc,char** argv)
{
    if (argc==12 && std::string(argv[1])=="--case")
    {
        std::uint64_t v[10]{}; for (unsigned i=0;i<10;++i) v[i]=std::stoull(argv[i+2]);
        std::cout<<Case(unsigned(v[0]),unsigned(v[1]),std::uint32_t(v[2]),v[3],v[4],v[5]!=0,v[6],v[7],unsigned(v[8]),std::uint32_t(v[9]))<<'\n';
        return EXIT_SUCCESS;
    }
    CheckLifetime<wxDroidMovingState>(0); CheckLifetime<wxSpiderMovingState>(0);
    CheckLifetime<wxIceBatIdleState>(0); CheckLifetime<wxSpiritFollowState>(0);
    CheckLifetime<wxMosquitoHurtState>(10);
    wxIceBatIdleState idle; wxAnimationRequestForAnalysis request{0xFFFFFFFF}; Host host(idle,request);
    idle.SetBindingsForAnalysis(Pointer(1),Pointer(2),&host); idle.SetPendingHandleForAnalysis(Pointer(11));
    idle.vfunc_30(request);
    Require(!idle.GetTransitionFlagsForAnalysis()[1] && idle.GetPendingHandleForAnalysis()==Pointer(22)
        && host.trace.find("predicate@22:")!=std::string::npos,"idle stores before queue and clears once flag afterwards");
    host.trace.clear(); const auto onceKey=request.packedKey; idle.vfunc_30(request);
    Require(host.trace.empty() && request.packedKey==onceKey,"second idle update has no calls or key rewrite");
    Require(!idle.vfunc_20(request) && !idle.GetTransitionFlagsForAnalysis()[2],"first idle exit waits");
    host.first=host.second=22;
    Require(idle.vfunc_20(request) && !idle.GetPendingHandleForAnalysis() && !host.first && !host.second,"completed idle exit consumes both records and releases");
    wxMosquitoHurtState hurt; wxAnimationRequestForAnalysis hurtRequest{0xFFFFFFFF}; Host hurtHost(hurt,hurtRequest);
    hurt.SetBindingsForAnalysis(Pointer(1),Pointer(2),&hurtHost); hurt.SetPendingHandleForAnalysis(Pointer(11));
    Require(hurt.vfunc_1C(hurtRequest) && hurtHost.speedBits==0x40000000,"hurt entry sets speed2");
    Require(hurtHost.trace.find("stop:")==std::string::npos && hurtHost.trace.find("word")==std::string::npos,"hurt entry leaves old playback and control alone");
    Require(hurt.vfunc_20(hurtRequest) && hurtHost.speedBits==0x3F800000 && hurt.GetPendingHandleForAnalysis()==Pointer(22),"hurt exit restores speed without releasing handle");
    wxSpiderMovingState spider; wxAnimationRequestForAnalysis spiderRequest{0xFFFFFFFF}; Host spiderHost(spider,spiderRequest);
    spider.SetBindingsForAnalysis(Pointer(1),Pointer(2),&spiderHost); spider.SetPendingHandleForAnalysis(Pointer(22));
    spiderHost.word=0x3DCCCCCC; spider.vfunc_30(spiderRequest);
    Require(!spiderHost.word && spiderHost.speedBits==0x3F800000 && spiderHost.trace.find("start:")==std::string::npos,"stationary spider still sets speed/control on unchanged handle");
    spiderHost.word=0x3DCCCCCD; spider.vfunc_30(spiderRequest);
    Require(spiderHost.word==0x3DCCCCCD && spiderHost.speedBits==0x3DCCCCCD && (spiderRequest.packedKey&0x70)==0x50,"spider equality uses motion as speed");
    wxDroidMovingState droid; wxAnimationRequestForAnalysis droidRequest{0xFFFFFFFF}; Host droidHost(droid,droidRequest);
    droid.SetBindingsForAnalysis(Pointer(1),Pointer(2),&droidHost); droidHost.word=0x3DCCCCCD;
    droid.vfunc_30(droidRequest);
    Require(droidHost.word==0x3DCCCCCD && (droidRequest.packedKey&0x70)==0,"droid equality is stationary without clearing control");
    wxSpiritFollowState spirit; wxAnimationRequestForAnalysis spiritRequest{0xFFFFFFFF}; Host spiritHost(spirit,spiritRequest);
    spirit.SetBindingsForAnalysis(Pointer(1),Pointer(2),&spiritHost); spirit.vfunc_30(spiritRequest);
    Require(!spirit.GetTransitionFlagsForAnalysis()[1] && spiritHost.trace.find("predicate@22:")!=std::string::npos,"spirit stores before queue");
    spiritHost.trace.clear(); spirit.vfunc_30(spiritRequest); Require(spiritHost.trace.empty(),"spirit update is once per reset");
    std::cout<<"Additional character state checks passed\n";
}
