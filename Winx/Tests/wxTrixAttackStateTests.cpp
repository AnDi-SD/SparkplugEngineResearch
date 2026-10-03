#include "Code/wxTrixAttackState.h"
#include "Analysis/Host/wxTrixAttackStateHost.h"
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
    struct Host final : wxTrixAttackStateHost
    {
        wxCharacterState& state; wxAnimationRequestForAnalysis& request;
        void* next=Pointer(22); bool predicate=false;
        std::uint32_t word=0; std::uintptr_t first=0,second=0;
        std::string trace;
        Host(wxCharacterState& s,wxAnimationRequestForAnalysis& r) : state(s),request(r) {}
        void Event(const std::string& name)
        {
            if (!trace.empty()) trace+=';';
            trace+=name+'@'+std::to_string(Token(state.GetPendingHandleForAnalysis()))+':'
                +std::to_string(Flags(state))+':'+std::to_string(request.packedKey);
        }
        const char* EventTagNameForAnalysis(const void*) override
        {
            Event("tag"); static const char* names[]={"event_lightning","event_spiral","event_iceshard","event_thunder","event_icemine","event_circles_begin","event_circles_end","event_shieldbubble","icy_freeze","event_lightning_extra","event_spiral_extra","event_iceshard_extra","event_thunder_extra","event_icemine_extra","event_circles_begin_extra","event_circles_end_extra","event_shieldbubble_extra","icy_freeze_extra","event_lightnin","event_spira","event_iceshar","event_thunde","event_icemin","event_circles_begi","event_circles_en","event_shieldbubbl","icy_freez","Event_lightning","Event_spiral","Event_iceshard","Event_thunder","Event_icemine","Event_circles_begin","Event_circles_end","Event_shieldbubble","Icy_freeze","","event_land"};
            Require(first<38,"event index"); return names[first];
        }
        void* OwnerField24ForAnalysis(void*) override { Event("owner24"); return second ? Pointer(0x400) : nullptr; }
        void SendZeroNotificationForAnalysis(void* receiver,wxCharacterState& source,unsigned code) override
        { Require(receiver==Pointer(0x400) && &source==&state,"borrowed receiver/source"); Event("notify:"+std::to_string(code)); }
        void SendCirclesNotificationForAnalysis(void* receiver,wxCharacterState& source,bool begin) override
        { Require(receiver==Pointer(0x400) && &source==&state,"borrowed circles receiver/source"); Event("circles:"+std::to_string(begin)); }
        void* ResolveAnimationForAnalysis(void*,std::uint32_t key) override
        { Event("lookup:"+std::to_string(key)); return next; }
        bool OwnerPredicateForAnalysis(void*) override { Event("predicate"); return predicate; }
        void ResetCompletionForAnalysis(void*,void* handle) override { Event("reset:"+std::to_string(Token(handle))); if(first==Token(handle)) first=0; if(second==Token(handle)) second=0; }
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
        Require(kind==0,"TrixAttackState kind"); state=std::make_unique<wxTrixAttackState>();
        wxAnimationRequestForAnalysis request{key}; Host host(*state,request);
        state->SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host); state->SetPendingHandleForAnalysis(Pointer(pending));
        state->SetTransitionFlagsForAnalysis(flags&1,flags&2,flags&4,flags&8,flags&16);
        host.next=Pointer(next); host.predicate=predicate; host.first=first; host.second=second; host.word=motion;
        unsigned result=2;
        if (slot==0x1C) result=state->vfunc_1C(request);
        else if (slot==0x20) result=state->vfunc_20(request);
        else if (slot==0x24) state->vfunc_24();
        else if (slot==0x28) state->vfunc_28(request);
        else if (slot==0x2C) state->vfunc_2C(request);
        else if (slot==0x30) state->vfunc_30(request);
        else if (slot==0x34) result=state->vfunc_34(key);
        else if (slot==0x3C) state->vfunc_3C(Pointer(0x300));
        else throw std::logic_error("unknown case slot");
        std::ostringstream output;
        output<<result<<' '<<request.packedKey<<' '<<Token(state->GetPendingHandleForAnalysis())
            <<' '<<host.word<<' '<<host.first<<' '<<host.second<<' '<<Flags(*state)<<'|'<<host.trace;
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
    CheckLifetime<wxTrixAttackState>(3);
    wxTrixAttackState state; wxAnimationRequestForAnalysis request{0xFFFFFFFF}; Host host(state,request);
    state.SetBindingsForAnalysis(Pointer(1),Pointer(2),&host); state.SetPendingHandleForAnalysis(Pointer(11));
    host.word=7; host.first=host.second=22;
    Require(state.vfunc_1C(request) && request.packedKey==0xF007FF81 && host.word==7 && !host.first && !host.second,"base entry, mode-zero queue without control clear");
    host.trace.clear(); host.first=host.second=22;
    for (unsigned code : {0x1Cu,0x11u,0xAu}) Require(state.vfunc_34(code),"unconditional permission code");
    Require(host.trace.empty() && host.first==22 && host.second==22,"permission bypass leaves completion records");
    Require(state.vfunc_34(0) && !host.first && !host.second && !state.vfunc_34(0),"other permission consumes both records");
    for (unsigned tag=0;tag<9;++tag)
    {
        host.trace.clear(); host.first=tag; host.second=1; state.vfunc_3C(Pointer(3));
        Require(host.trace.find("owner24")!=std::string::npos,"matching event reads receiver");
        if(tag==5 || tag==6) Require(host.trace.find(tag==5?"circles:1":"circles:0")!=std::string::npos,"circles flag");
        else Require(host.trace.find("notify:")!=std::string::npos,"event message");
    }
    for (unsigned tag=9;tag<38;++tag)
    { host.trace.clear(); host.first=tag; state.vfunc_3C(Pointer(3)); Require(host.trace.find("owner24")==std::string::npos,"full case-sensitive match"); }
    host.trace.clear(); host.first=0; host.second=0; state.vfunc_3C(Pointer(3)); Require(host.trace.find("notify:")==std::string::npos,"null receiver skips dispatch");
    std::cout<<"TrixAttack state checks passed\n";
}
