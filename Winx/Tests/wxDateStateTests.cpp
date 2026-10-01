#include "Code/wxDateIdleState.h"
#include "Code/wxDateTalkingState.h"
#include "Code/wxDateReactionState.h"
#include "Analysis/Host/wxDateStateHost.h"
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
    struct Host final : wxDateStateHost
    {
        wxCharacterState& state; wxAnimationRequestForAnalysis& request;
        void* next=Pointer(22); bool predicate=false;
        std::uint32_t word=0; std::uintptr_t first=0,second=0;
        std::string trace; bool replaceOnMessage=false;
        Host(wxCharacterState& s,wxAnimationRequestForAnalysis& r) : state(s),request(r) {}
        void Event(const std::string& name)
        {
            if (!trace.empty()) trace+=';';
            trace+=name+'@'+std::to_string(Token(state.GetPendingHandleForAnalysis()))+':'
                +std::to_string(Flags(state))+':'+std::to_string(request.packedKey);
        }
        void* OwnerField24ForAnalysis(void*) override { Event("owner24"); return Pointer(word); }
        void SendExitNotificationForAnalysis(wxCharacterState& source,std::uint32_t code,
            std::uint32_t parameter,std::uint32_t selector,void* ownerWord) override
        {
            Require(&source==&state,"message source is state");
            Event("message:"+std::to_string(code)+':'+std::to_string(parameter)+':'+std::to_string(selector)+':'+std::to_string(Token(ownerWord)));
            if (replaceOnMessage) state.SetPendingHandleForAnalysis(Pointer(33));
        }
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
        if (kind==0) state=std::make_unique<wxDateIdleState>();
        else if (kind==1) state=std::make_unique<wxDateTalkingState>();
        else if (kind==2) state=std::make_unique<wxDateReactionState>();
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
    CheckLifetime<wxDateIdleState>(34); CheckLifetime<wxDateTalkingState>(35); CheckLifetime<wxDateReactionState>(36);
    wxDateIdleState idle; wxAnimationRequestForAnalysis request{0xFFFFFFFF}; Host host(idle,request);
    idle.SetBindingsForAnalysis(Pointer(1),Pointer(2),&host); idle.SetPendingHandleForAnalysis(Pointer(22));
    Require(idle.vfunc_1C(request) && request.packedKey==0xFF800059
        && host.trace.find("stop:")==std::string::npos && host.trace.find("start:")==std::string::npos,
        "date idle entry calls update without unconditional release");
    host.trace.clear(); host.word=0x13572468; host.replaceOnMessage=true;
    Require(idle.vfunc_20(request) && !idle.GetPendingHandleForAnalysis()
        && host.trace.find("message:10221:29:34:324478056@22:")!=std::string::npos
        && host.trace.find("stop:33")!=std::string::npos,"date exit sends fields before release and rereads callback-modified pending");
    wxDateReactionState reaction; wxAnimationRequestForAnalysis reactionRequest{0xFFFFFFFF}; Host reactionHost(reaction,reactionRequest);
    reaction.SetBindingsForAnalysis(Pointer(1),Pointer(2),&reactionHost); reaction.SetPendingHandleForAnalysis(Pointer(11));
    reactionHost.next=nullptr;
    Require(reaction.vfunc_1C(reactionRequest) && reaction.GetPendingHandleForAnalysis()==Pointer(11)
        && reactionRequest.packedKey==0xFF800009 && reactionHost.trace.find("start:")==std::string::npos,
        "reaction null lookup preserves pending");
    reactionHost.next=Pointer(11); reactionHost.trace.clear(); Require(reaction.vfunc_1C(reactionRequest),"reaction same-handle entry returns true");
    Require(reactionHost.trace.find("stop:11")!=std::string::npos && reactionHost.trace.find("reset:11")!=std::string::npos,"reaction nonnull same handle is released and restarted");
    reaction.SetPendingHandleForAnalysis(nullptr); reactionHost.trace.clear();
    Require(reaction.vfunc_34(0) && reactionHost.trace.empty(),"reaction null permission skips query");
    std::cout<<"Date state checks passed\n";
}
