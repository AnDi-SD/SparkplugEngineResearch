#include "Code/wxFlyingState.h"
#include "Code/wxTry2HoistState.h"
#include "Analysis/Host/wxTry2HoistStateHost.h"
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
    struct Host final : wxTry2HoistStateHost
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
        std::uint32_t OwnerField218ForAnalysis(void*) override { Event("field218"); return word; }
        void CallOwnerAfterReleaseForAnalysis(void*,bool nonzero) override
        { Event("owner:"+std::to_string(nonzero)); }
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
        if (kind==0) state=std::make_unique<wxFlyingState>();
        else state=std::make_unique<wxTry2HoistState>();
        wxAnimationRequestForAnalysis request{key}; Host host(*state,request);
        state->SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host); state->SetPendingHandleForAnalysis(Pointer(pending));
        state->SetTransitionFlagsForAnalysis(flags&1,flags&2,flags&4,flags&8,flags&16);
        host.next=Pointer(next); host.predicate=predicate; host.first=first; host.second=second; host.word=motion;
        unsigned result=2;
        if (slot==0x1C) result=state->vfunc_1C(request);
        else if (slot==0x20) result=state->vfunc_20(request);
        else if (slot==0x2C) state->vfunc_2C(request);
        else if (slot==0x38) result=state->vfunc_38(key);
        else if (slot==0x30) state->vfunc_30(request);
        else if (slot==0x34) result=state->vfunc_34(key);
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
    CheckLifetime<wxFlyingState>(20); CheckLifetime<wxTry2HoistState>(26);
    wxTry2HoistState hoist; wxAnimationRequestForAnalysis request{0xFFFFFFFF}; Host host(hoist,request);
    hoist.SetBindingsForAnalysis(Pointer(1),Pointer(2),&host); hoist.SetPendingHandleForAnalysis(Pointer(11));
    host.word=0xFFFFFFFF;
    Require(hoist.vfunc_1C(request) && !host.word && request.packedKey==0xF0000884,"hoist entry clears control after storing playback");
    Require(host.trace.find("stop:")==std::string::npos && host.trace.find("word@22:")!=std::string::npos,"hoist entry does not release old playback");
    host.trace.clear(); host.word=0x80000000; hoist.vfunc_2C(request);
    Require(!hoist.GetPendingHandleForAnalysis() && host.trace.find("field218@0:")!=std::string::npos
        && host.trace.find("owner:1@0:")!=std::string::npos,"hoist releases before reading unsigned high-bit word");
    host.trace.clear(); host.word=0; hoist.vfunc_2C(request);
    Require(host.trace.find("owner:0@0:")!=std::string::npos,"zero owner word selects other helper");
    host.trace.clear(); Require(hoist.vfunc_38(0xFFFFFFFF) && host.trace.empty(),"hoist second permission is unconditional true");
    wxFlyingState flying; wxAnimationRequestForAnalysis flightRequest{0xFFFFFFFF}; Host flightHost(flying,flightRequest);
    flying.SetBindingsForAnalysis(Pointer(1),Pointer(2),&flightHost); flying.SetPendingHandleForAnalysis(Pointer(22));
    flightHost.word=0x12345678; flying.vfunc_30(flightRequest);
    Require(flightHost.trace.find("start:")==std::string::npos && flightHost.word==0x12345678
        && flightRequest.packedKey==0xF007FFF1,"flying unchanged handle still rewrites key without clearing control");
    Require(flying.vfunc_20(flightRequest) && !flying.GetPendingHandleForAnalysis(),"flying exit uses shared release");
    std::cout<<"Flying and hoist state checks passed\n";
}
