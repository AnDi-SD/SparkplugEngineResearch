#include "Code/wxShadowBeastJumpingState.h"

#include <cstdlib>
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
    struct Host final : wxCharacterStateHost
    {
        wxShadowBeastJumpingState& state;
        wxAnimationRequestForAnalysis& request;
        void* next = Pointer(22);
        bool predicate = false;
        std::uint32_t word = 0xFFFFFFFF;
        std::uintptr_t first = 0, second = 0;
        std::string trace;
        Host(wxShadowBeastJumpingState& s, wxAnimationRequestForAnalysis& r) : state(s), request(r) {}
        void Event(const std::string& name)
        {
            if (!trace.empty()) trace += ';';
            trace += name + '@' + std::to_string(Token(state.GetPendingHandleForAnalysis()))
                + ':' + std::to_string(state.GetTransitionFlagsForAnalysis()[0])
                + ':' + std::to_string(request.packedKey);
        }
        void* ResolveAnimationForAnalysis(void*, std::uint32_t key) override
        { Event("lookup:"+std::to_string(key)); return next; }
        bool OwnerPredicateForAnalysis(void*) override { Event("predicate"); return predicate; }
        void ResetCompletionForAnalysis(void*, void* handle) override
        { Event("reset:"+std::to_string(Token(handle))); }
        void StartAnimationForAnalysis(void*, void* handle, bool mode,
            std::uint32_t fade, bool interrupt) override
        {
            Event("start:"+std::to_string(Token(handle))+':'+std::to_string(mode)
                +':'+std::to_string(fade)+':'+std::to_string(interrupt));
        }
        void StopAnimationForAnalysis(void*, void* handle) override
        { Event("stop:"+std::to_string(Token(handle))); }
        void FadeAnimationForAnalysis(void*, void* handle, float duration) override
        { Require(duration==0.4f,"native fade"); Event("fade:"+std::to_string(Token(handle))); }
        bool IsPendingAnimationCompleteForAnalysis(void*, void* handle, bool consume) override
        {
            Require(consume,"completion is consumed"); Event("query:"+std::to_string(Token(handle)));
            const auto token=Token(handle);
            if (!token) return true;
            const bool completed=first==token || second==token;
            if (first==token) first=0;
            if (second==token) second=0;
            return completed;
        }
        void ClearOwnerActionControlForAnalysis(void*) override { Event("word"); word=0; }
    };
    std::string Case(unsigned slot, std::uint32_t key, std::uintptr_t pending,
        std::uintptr_t next, bool predicate, std::uintptr_t first, std::uintptr_t second, bool flag)
    {
        wxShadowBeastJumpingState state; wxAnimationRequestForAnalysis request{key}; Host host(state,request);
        state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);
        state.SetPendingHandleForAnalysis(Pointer(pending));state.SetTransitionFlagsForAnalysis(flag,true,true,true,true);
        host.next=Pointer(next);host.predicate=predicate;host.first=first;host.second=second;
        unsigned result=2;
        if (slot==0x1C) result=state.vfunc_1C(request);
        else if (slot==0x20) result=state.vfunc_20(request);
        else if (slot==0x30) state.vfunc_30(request);
        else if (slot==0x34) result=state.vfunc_34(key);
        else throw std::logic_error("unknown case slot");
        std::ostringstream output;
        output << result << ' ' << request.packedKey << ' ' << Token(state.GetPendingHandleForAnalysis())
            << ' ' << host.word << ' ' << state.GetTransitionFlagsForAnalysis()[0]
            << ' ' << host.first << ' ' << host.second << '|' << host.trace;
        return output.str();
    }
}
int main(int argc, char** argv)
{
    if (argc==10 && std::string(argv[1])=="--case")
    {
        std::uint64_t v[8]{};for(unsigned i=0;i<8;++i)v[i]=std::stoull(argv[i+2]);
        std::cout << Case(unsigned(v[0]),std::uint32_t(v[1]),v[2],v[3],v[4]!=0,v[5],v[6],v[7]!=0) << '\n';
        return EXIT_SUCCESS;
    }
    wxShadowBeastJumpingState state;
    Require(state.GetStateSelectorForAnalysis()==1 && state.IsExactly(wxShadowBeastJumpingState::ClassID)
        && state.IsKindOf(wxCharacterState::ClassID),"selector and RTTI");
    Require(dynamic_cast<wxShadowBeastJumpingState*>(spRTTIManager::Instance().Create(state.ClassID).get()),"factory");
    wxAnimationRequestForAnalysis request{0xFFFFFFFF};Host host(state,request);
    state.SetBindingsForAnalysis(Pointer(1),Pointer(2),&host);state.SetPendingHandleForAnalysis(Pointer(11));
    Require(state.vfunc_1C(request) && request.packedKey==0xF0087FA2
        && !state.GetTransitionFlagsForAnalysis()[0] && state.GetPendingHandleForAnalysis()==Pointer(22),
        "entry releases old handle, rewrites key, queues, stores and clears flag");
    const auto entryTrace=host.trace;
    auto clone=state.Clone();auto* fresh=dynamic_cast<wxShadowBeastJumpingState*>(clone.get());
    Require(fresh && fresh->GetStateSelectorForAnalysis()==1 && fresh->GetTransitionFlagsForAnalysis()[0]
        && !fresh->GetPendingHandleForAnalysis() && host.trace==entryTrace,"clone has constructor defaults");
    wxShadowBeastJumpingState target;spCloneManager manager;
    Require(state.vfunc_14(target,manager) && !target.GetPendingHandleForAnalysis(),"empty Copy");
    host.trace.clear();request.packedKey=0xFFFFFFFF;
    Require(state.vfunc_20(request) && request.packedKey==0xFFFFFFF0 && !state.GetPendingHandleForAnalysis()
        && host.word==0,"exit clears control, mode and pending handle");
    host.trace.clear();Require(state.vfunc_34(9) && host.trace.find("query:0")==0,"null handle is queried");
    host.first=11;state.SetPendingHandleForAnalysis(Pointer(11));
    Require(state.vfunc_34(9) && !host.first && !state.vfunc_34(9),"permission consumes completion record");
    std::cout << "Shadow Beast jumping state checks passed\n";
}
