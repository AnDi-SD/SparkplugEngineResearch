#include "Code/wxMikaelOpenGateState.h"
#include "Code/wxMikaelWandringState.h"
#include "Code/wxWandringNPCWaitState.h"
#include "Analysis/Host/wxNPCStateHost.h"
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
    const wxNPCStateFieldsForAnalysis& Fields(const wxCharacterState& state)
    {
        if(auto* p=dynamic_cast<const wxMikaelOpenGateState*>(&state)) return p->GetFieldsForAnalysis();
        if(auto* p=dynamic_cast<const wxMikaelWandringState*>(&state)) return p->GetFieldsForAnalysis();
        if(auto* p=dynamic_cast<const wxWandringNPCWaitState*>(&state)) return p->GetFieldsForAnalysis();
        throw std::logic_error("unknown NPC state");
    }
    void SetFields(wxCharacterState& state,std::uint8_t flag,void* cached)
    {
        if(auto* p=dynamic_cast<wxMikaelOpenGateState*>(&state)) p->SetFieldsForAnalysis(flag,cached);
        else if(auto* p=dynamic_cast<wxMikaelWandringState*>(&state)) p->SetFieldsForAnalysis(flag,cached);
        else if(auto* p=dynamic_cast<wxWandringNPCWaitState*>(&state)) p->SetFieldsForAnalysis(flag,cached);
        else throw std::logic_error("unknown NPC state");
    }
    std::string OwnFields(const wxCharacterState& state)
    {
        const auto& fields=Fields(state);
        return std::to_string(fields.field3C)+':'
            +(fields.field40?std::to_string(Token(*fields.field40)):"u");
    }
    struct Host final : wxNPCStateHost
    {
        wxCharacterState& state; wxAnimationRequestForAnalysis& request;
        void* next=Pointer(22); bool predicate=false;
        std::uint32_t word=0; std::uintptr_t first=0,second=0;
        std::string trace; void* cached=Pointer(0x500); bool mutateOnQuery=false;
        Host(wxCharacterState& s,wxAnimationRequestForAnalysis& r) : state(s),request(r) {}
        void Event(const std::string& name)
        {
            if (!trace.empty()) trace+=';';
            trace+=name+'@'+std::to_string(Token(state.GetPendingHandleForAnalysis()))+':'
                +std::to_string(Flags(state))+':'+std::to_string(request.packedKey)+':'+OwnFields(state);
        }
        void* OwnerEntityField130ForAnalysis(void*) override { Event("cache"); return cached; }
        void WriteOwnerActionControlForAnalysis(void*,unsigned value) override { Event("write:"+std::to_string(value)); word=value; }
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
            if (mutateOnQuery) request.packedKey=0x40;
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
        Require(kind<=13,"NPC kind");
        if(kind%3==0) state=std::make_unique<wxMikaelOpenGateState>();
        else if(kind%3==1) state=std::make_unique<wxMikaelWandringState>();
        else state=std::make_unique<wxWandringNPCWaitState>();
        const auto flagGroup=(kind/3)%3;
        SetFields(*state,std::uint8_t(flagGroup==0?0:flagGroup==1?1:0xCC),Pointer(0x600));
        wxAnimationRequestForAnalysis request{key}; Host host(*state,request);
        state->SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host); state->SetPendingHandleForAnalysis(Pointer(pending));
        state->SetTransitionFlagsForAnalysis(flags&1,flags&2,flags&4,flags&8,flags&16);
        host.cached=kind<9?Pointer(0x500):nullptr; host.mutateOnQuery=kind==13;
        host.next=Pointer(next); host.predicate=predicate; host.first=first; host.second=second; host.word=motion;
        unsigned result=2;
        if (slot==0x1C) result=state->vfunc_1C(request);
        else if (slot==0x20) result=state->vfunc_20(request);
        else if (slot==0x24) state->vfunc_24();
        else if (slot==0x28) state->vfunc_28(request);
        else if (slot==0x2C) state->vfunc_2C(request);
        else if (slot==0x30) state->vfunc_30(request);
        else if (slot==0x34) result=state->vfunc_34(key);
        else throw std::logic_error("unknown case slot");
        std::ostringstream output;
        output<<result<<' '<<request.packedKey<<' '<<Token(state->GetPendingHandleForAnalysis())
            <<' '<<host.word<<' '<<host.first<<' '<<host.second<<' '<<Flags(*state)
            <<' '<<unsigned(Fields(*state).field3C)<<' '<<Token(*Fields(*state).field40)<<'|'<<host.trace;
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
    CheckLifetime<wxMikaelOpenGateState>(0); CheckLifetime<wxMikaelWandringState>(0); CheckLifetime<wxWandringNPCWaitState>(0);
    wxMikaelOpenGateState pc;
    Require(pc.GetFieldsForAnalysis().field3C==0 && !pc.GetFieldsForAnalysis().field40,"both platform constructors clear byte and leave pointer unknown");
    pc.SetFieldsForAnalysis(1,Pointer(99));
    auto pcClone=pc.Clone();
    Require(static_cast<wxMikaelOpenGateState&>(*pcClone).GetFieldsForAnalysis().field3C==0
        && !static_cast<wxMikaelOpenGateState&>(*pcClone).GetFieldsForAnalysis().field40,"clone starts with zero byte and unknown pointer");
    pc.vfunc_40_ResetForAnalysis(); Require(pc.GetFieldsForAnalysis().field3C==1 && *pc.GetFieldsForAnalysis().field40==Pointer(99),"base Reset leaves own fields");
    wxMikaelOpenGateState gate; wxAnimationRequestForAnalysis request{0xFFFFFFFF}; Host host(gate,request);
    gate.SetBindingsForAnalysis(Pointer(1),Pointer(2),&host); gate.SetPendingHandleForAnalysis(Pointer(11)); host.word=7;
    Require(gate.vfunc_1C(request) && request.packedKey==0xF1000000 && gate.GetFieldsForAnalysis().field3C==1
        && *gate.GetFieldsForAnalysis().field40==Pointer(0x500),"entry stores cache and clears flag before two updates");
    const auto first=host.trace.find("lookup:");const auto second=host.trace.find("lookup:",first+1);
    Require(host.trace.find("cache")<first && host.trace.find("stop:22")>first && host.trace.find("stop:22")<second && host.word==7,"shared entry update/base release/second update order");
    wxMikaelWandringState moving; wxAnimationRequestForAnalysis walk{0xFFFFFFFF}; Host walkHost(moving,walk);
    moving.SetBindingsForAnalysis(Pointer(1),Pointer(2),&walkHost);
    Require(moving.vfunc_34(0),"default permission reads constructor zero byte without a host");
    moving.SetFieldsForAnalysis(1,Pointer(99));moving.SetPendingHandleForAnalysis(Pointer(11));walkHost.next=Pointer(22);
    moving.vfunc_30(walk); Require(moving.GetPendingHandleForAnalysis()==Pointer(11) && walkHost.word==0x3DCCCCCD && !moving.vfunc_34(0),"unfinished locked handle writes0.1 and defers replacement");
    walkHost.first=walkHost.second=11; walkHost.trace.clear(); moving.vfunc_30(walk);
    Require(moving.GetPendingHandleForAnalysis()==Pointer(22) && moving.GetFieldsForAnalysis().field3C==0 && moving.vfunc_34(0)
        && !walkHost.first && !walkHost.second && walkHost.trace.find("start:22:1:")!=std::string::npos,"completed lock consumes records and queues mode1");
    std::cout<<"NPC state checks passed\n";
}
