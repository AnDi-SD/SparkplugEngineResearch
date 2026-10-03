#include "Code/wxYetiAttackState.h"
#include "Analysis/Host/wxYetiAttackStateHost.h"
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
    struct Host final : wxYetiAttackStateHost
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
            Event("tag"); static const char* names[]={"yeti_backspike_attack","event_shoot","event_blast_begin","event_blast_end","prefix_yeti_backspike_attack","prefix_event_shoot","prefix_event_blast_begin","prefix_event_blast_end","yeti_backspike_attack_suffix","event_shoot_suffix","event_blast_begin_suffix","event_blast_end_suffix","yeti_backspike_attac","event_shoo","event_blast_begi","event_blast_en","Yeti_backspike_attack","Event_shoot","Event_blast_begin","Event_blast_end","event_blast_endevent_blast_begin","yeti_backspike_attackevent_blast_begin","","event_land"};
            Require(first<24,"event index"); return names[first];
        }
        void* OwnerField24ForAnalysis(void*) override { Event("owner24"); return second ? Pointer(0x400) : nullptr; }
        void SendBackspikeNotificationForAnalysis(void* receiver,wxCharacterState& source) override
        { Require(receiver==Pointer(0x400) && &source==&state,"borrowed receiver/source"); Event("notify:10069"); }
        void* OwnerEntityField140ForAnalysis(void*) override { Event("entity140"); return Pointer(0x500); }
        void InvokeControllerSlot38ForAnalysis(void* controller,unsigned argument) override
        { Require(controller==Pointer(0x500),"borrowed controller"); Event("slot38:"+std::to_string(argument)); }
        void InvokeControllerSlot3CForAnalysis(void* controller,unsigned a0,unsigned a1) override
        { Require(controller==Pointer(0x500),"borrowed controller"); Event("slot3c:"+std::to_string(a0)+':'+std::to_string(a1)); }
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
        Require(kind==0,"YetiAttackState kind"); state=std::make_unique<wxYetiAttackState>();
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
    CheckLifetime<wxYetiAttackState>(3);
    wxYetiAttackState state; wxAnimationRequestForAnalysis request{0xFFFFFFFF}; Host host(state,request);
    state.SetBindingsForAnalysis(Pointer(1),Pointer(2),&host); state.SetPendingHandleForAnalysis(Pointer(11)); host.word=7;
    state.vfunc_30(request); Require(request.packedKey==0xF01FFF8F && host.word==7,"mask and unchanged control word");
    Require(host.trace.find("stop:11")<host.trace.find("reset:22"),"changed update releases before mode-zero queue");
    host.trace.clear(); host.first=host.second=22;
    Require(state.vfunc_34(10) && state.vfunc_34(33) && host.trace.empty() && host.first==22 && host.second==22,"selector3 bypass codesA/21");
    Require(state.vfunc_34(0) && !host.first && !host.second && !state.vfunc_34(0),"other permission consumes records");
    host.trace.clear(); host.first=0; host.second=1; state.vfunc_3C(Pointer(3)); Require(host.trace.find("notify:10069")!=std::string::npos,"backspike packet");
    host.trace.clear(); host.first=1; state.vfunc_3C(Pointer(3)); Require(host.trace.find("slot38:0")!=std::string::npos,"shoot controller callback");
    for(unsigned tag : {2u,6u,10u,20u,21u})
    { host.trace.clear(); host.first=tag; state.vfunc_3C(Pointer(3)); Require(host.trace.find("slot3c:1:1")!=std::string::npos,"begin substring and priority"); }
    host.trace.clear(); host.first=3; state.vfunc_3C(Pointer(3)); Require(host.trace.find("slot3c:0:1")!=std::string::npos,"end exact match");
    for(unsigned tag : {4u,5u,7u,8u,9u,11u,14u,18u,22u,23u})
    { host.trace.clear(); host.first=tag; state.vfunc_3C(Pointer(3)); Require(host.trace.find("entity140")==std::string::npos && host.trace.find("owner24")==std::string::npos,"nonmatching tag has no owner or controller access"); }
    std::cout<<"YetiAttack state checks passed\n";
}
