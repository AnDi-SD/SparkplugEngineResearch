#include "Code/wxGhoulAttackState.h"
#include "Analysis/Host/wxGhoulAttackStateHost.h"
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
    struct Host final : wxGhoulAttackStateHost
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
            Event("tag"); static const char* names[]={"event_impact_begin","event_impact_end","event_kick_begin","event_kick_end","event_air_begin","event_throw","prefix_event_impact_begin","prefix_event_impact_end","prefix_event_kick_begin","prefix_event_kick_end","prefix_event_air_begin","prefix_event_throw","event_impact_begin_suffix","event_impact_end_suffix","event_kick_begin_suffix","event_kick_end_suffix","event_air_begin_suffix","event_throw_suffix","event_impact_begi","event_impact_en","event_kick_begi","event_kick_en","event_air_begi","event_thro","Event_impact_begin","Event_impact_end","Event_kick_begin","Event_kick_end","Event_air_begin","Event_throw","","event_land"};
            Require(first<32,"event index"); return names[first];
        }
        void* OwnerField24ForAnalysis(void*) override { Event("owner24"); return second ? Pointer(0x400) : nullptr; }
        void* OwnerEntityField12CForAnalysis(void*) override { Event("entity12c"); return Pointer(0x600); }
        void InvokeControllerFloatServiceForAnalysis(void* controller,float value) override
        { Require(controller==Pointer(0x600) && value==200.0f,"borrowed float service"); Event("floatservice:200"); }
        void* OwnerEntityField140ForAnalysis(void*) override { Event("entity140"); return Pointer(0x500); }
        void InvokeControllerSlot38ForAnalysis(void* controller,unsigned argument) override
        { Require(controller==Pointer(0x500),"borrowed controller"); Event("slot38:"+std::to_string(argument)); }
        void SendNamedFlagNotificationForAnalysis(void* receiver,wxCharacterState& source,const char* name,bool flag) override
        { Require(receiver==Pointer(0x400) && &source==&state,"borrowed flag receiver/source"); Event("namedflag:"+std::string(name)+':'+std::to_string(flag)); }
        void SendThrowNotificationForAnalysis(void* receiver,wxCharacterState& source) override
        { Require(receiver==Pointer(0x400) && &source==&state,"borrowed throw receiver/source"); Event("notify:10068"); }
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
        Require(kind==0,"GhoulAttackState kind"); state=std::make_unique<wxGhoulAttackState>();
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
    CheckLifetime<wxGhoulAttackState>(19);
    wxGhoulAttackState state; wxAnimationRequestForAnalysis request{0xFFFFFFFF}; Host host(state,request);
    state.SetBindingsForAnalysis(Pointer(1),Pointer(2),&host); state.SetPendingHandleForAnalysis(Pointer(22)); host.word=7; host.first=host.second=22;
    Require(state.vfunc_1C(request) && request.packedKey==0xF007FF8F && host.word==7 && !host.first && !host.second,"entry always queues mode-zero, leaves control");
    Require(host.trace.find("stop:")==std::string::npos && host.trace.find("reset:22")!=std::string::npos,"entry does not release or compare same handle");
    host.trace.clear(); host.first=host.second=22;
    Require(state.vfunc_1C(request) && host.trace.find("reset:22")!=std::string::npos && !host.first && !host.second,"second same-handle entry queues again");
    host.trace.clear(); host.first=host.second=22;
    Require(state.vfunc_34(9) && state.vfunc_34(10) && host.trace.empty() && host.first==22,"permission bypass leaves records");
    Require(state.vfunc_34(0) && !host.first && !host.second && !state.vfunc_34(0),"consuming completion query");
    for(unsigned tag=0;tag<4;++tag)
    { host.trace.clear(); host.first=tag; host.second=1; state.vfunc_3C(Pointer(3)); Require(host.trace.find(tag<2?"namedflag:hand_left:":"namedflag:L_Toe:")!=std::string::npos,"impact/kick name payload"); }
    host.trace.clear(); host.first=4; state.vfunc_3C(Pointer(3)); Require(host.trace.find("entity12c")<host.trace.find("floatservice:200"),"air service follows borrowed controller read");
    host.trace.clear(); host.first=5; state.vfunc_3C(Pointer(3));
    Require(host.trace.find("slot38:0")<host.trace.find("owner24") && host.trace.find("owner24")<host.trace.find("notify:10068"),"throw reads receiver after controller callback");
    host.trace.clear(); host.second=0; state.vfunc_3C(Pointer(3)); Require(host.trace.find("slot38:0")!=std::string::npos && host.trace.find("notify:")==std::string::npos,"null receiver still calls controller");
    for(unsigned tag=6;tag<32;++tag)
    { host.trace.clear(); host.first=tag; state.vfunc_3C(Pointer(3)); Require(host.trace.find("entity140")==std::string::npos && host.trace.find("entity12c")==std::string::npos && host.trace.find("owner24")==std::string::npos,"full string equality"); }
    std::cout<<"GhoulAttack state checks passed\n";
}
