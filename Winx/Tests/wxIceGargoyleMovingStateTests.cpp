#include "Code/wxIceGargoyleMovingState.h"
#include "Analysis/Host/wxIceGargoyleMovingStateHost.h"
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
    struct Host final : wxIceGargoyleMovingStateHost
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
        std::uint32_t action=0, motionBits=0;
        std::uint32_t ReadOwnerActionKeyForAnalysis(void*) override
        { Event("action"); return action; }
        float ReadOwnerMotionForAnalysis(void*) override
        { Event("motion"); float value; std::memcpy(&value,&motionBits,sizeof(value)); return value; }
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
        std::uintptr_t next,bool predicate,std::uintptr_t first,std::uintptr_t second,unsigned flags,std::uint32_t motion,std::uint32_t action,std::uint32_t motionBits)
    {
        std::unique_ptr<wxCharacterState> state;
        Require(kind==0,"IceGargoyleMoving kind"); state=std::make_unique<wxIceGargoyleMovingState>();
        wxAnimationRequestForAnalysis request{key}; Host host(*state,request);
        state->SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host); state->SetPendingHandleForAnalysis(Pointer(pending));
        state->SetTransitionFlagsForAnalysis(flags&1,flags&2,flags&4,flags&8,flags&16);
        host.next=Pointer(next); host.predicate=predicate; host.first=first; host.second=second; host.word=motion; host.action=action; host.motionBits=motionBits;
        unsigned result=2;
        if (slot==0x1C) result=state->vfunc_1C(request);
        else if (slot==0x20) result=state->vfunc_20(request);
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
    if (argc==14 && std::string(argv[1])=="--case")
    {
        std::uint64_t v[12]{}; for (unsigned i=0;i<12;++i) v[i]=std::stoull(argv[i+2]);
        std::cout<<Case(unsigned(v[0]),unsigned(v[1]),std::uint32_t(v[2]),v[3],v[4],v[5]!=0,v[6],v[7],unsigned(v[8]),std::uint32_t(v[9]),std::uint32_t(v[10]),std::uint32_t(v[11]))<<'\n';
        return EXIT_SUCCESS;
    }
    CheckLifetime<wxIceGargoyleMovingState>(0);
    wxIceGargoyleMovingState state; wxAnimationRequestForAnalysis request{0xFFFFFFFF}; Host host(state,request);
    state.SetBindingsForAnalysis(Pointer(1),Pointer(2),&host); state.SetPendingHandleForAnalysis(Pointer(11));
    host.action=0x801; host.word=0x12345678; host.motionBits=0x7FC12345;
    state.vfunc_30(request);
    Require((request.packedKey&0x7F)==1 && host.trace.find("motion")==std::string::npos,"action800 skips motion and forces mode zero");
    host.trace.clear(); host.next=Pointer(33); request.packedKey=0xFFFFFFFF;
    Require(!state.vfunc_20(request),"first special exit waits");
    Require(request.packedKey==0xF05FFF80 && !(Flags(state)&4) && host.word==0
        && host.trace.find("start:33:0:")!=std::string::npos,"exit masks, queues mode zero and clears once/control");
    host.word=0x12345678; host.trace.clear();
    Require(!state.vfunc_20(request) && host.word==0 && Token(state.GetPendingHandleForAnalysis())==33,"repeated exit keeps pending while incomplete");
    host.word=0x12345678; host.first=33; host.second=33; host.trace.clear();
    Require(state.vfunc_20(request) && host.word==0x12345678 && !state.GetPendingHandleForAnalysis()
        && !host.first && !host.second,"completion consumes records and releases without control clear");
    host.action=0; host.motionBits=0x7FC12345; request.packedKey=0xFFFFFFFF; host.trace.clear();
    state.vfunc_30(request); Require((request.packedKey&0x7F)==0x51,"PC unordered selects moving mode");
    host.motionBits=0x80000000; request.packedKey=0xFFFFFFFF;
    state.vfunc_30(request); Require((request.packedKey&0x7F)==1,"signed zero stationary");
    std::cout<<"IceGargoyleMoving state checks passed\n";
}
