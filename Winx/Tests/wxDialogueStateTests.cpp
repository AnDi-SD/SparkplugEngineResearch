#include "Analysis/Host/wxDialogueStateHost.h"
#include "Code/wxDialogueState.h"

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
    struct Host final : wxDialogueStateHost
    {
        wxDialogueState& state;
        void* next = Pointer(22);
        bool predicate = false;
        std::uint32_t game = 0;
        mutable std::string trace;
        explicit Host(wxDialogueState& value) : state(value) {}
        void Event(const std::string& name) const
        {
            if (!trace.empty()) trace += ';';
            trace += name + '@' + std::to_string(Token(state.GetPendingHandleForAnalysis()));
        }
        void SendEntryMessageForAnalysis(wxDialogueState& source, std::uint32_t code,
            std::uint32_t parameter, std::uint32_t first, std::uint32_t second) override
        {
            Require(&source == &state && code == 0x27BA && parameter == 0x0E && !first && !second,
                "original entry message fields"); Event("message");
        }
        std::uint32_t GameStateForAnalysis() const override { Event("game"); return game; }
        void* ResolveAnimationForAnalysis(void*, std::uint32_t key) override
        { Event("lookup:" + std::to_string(key)); return next; }
        bool OwnerPredicateForAnalysis(void*) override { Event("predicate"); return predicate; }
        void ResetCompletionForAnalysis(void*, void* handle) override
        { Event("reset:" + std::to_string(Token(handle))); }
        void StartAnimationForAnalysis(void*, void* handle, bool mode,
            std::uint32_t fade, bool interrupt) override
        {
            Event("start:" + std::to_string(Token(handle)) + ':' + std::to_string(mode)
                + ':' + std::to_string(fade) + ':' + std::to_string(interrupt));
        }
        void StopAnimationForAnalysis(void*, void* handle) override
        { Event("stop:" + std::to_string(Token(handle))); }
        void FadeAnimationForAnalysis(void*, void* handle, float duration) override
        { Require(duration == 0.4f, "native fade duration"); Event("fade:" + std::to_string(Token(handle))); }
        bool IsPendingAnimationCompleteForAnalysis(void*, void*, bool) override
        { throw std::logic_error("dialogue does not query completion"); }
        void ClearOwnerActionControlForAnalysis(void*) override
        { throw std::logic_error("dialogue does not clear control"); }
    };
    std::string Case(unsigned slot, std::uint32_t key, std::uintptr_t pending,
        std::uintptr_t next, bool predicate, std::uint32_t game)
    {
        wxDialogueState state;
        Host host(state); host.next=Pointer(next); host.predicate=predicate; host.game=game;
        state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);
        state.SetPendingHandleForAnalysis(Pointer(pending));
        wxAnimationRequestForAnalysis request{key};
        unsigned result = 2;
        if (slot==0x1C) result=state.vfunc_1C(request);
        else if (slot==0x20) result=state.vfunc_20(request);
        else if (slot==0x30) state.vfunc_30(request);
        else if (slot==0x34) result=state.vfunc_34(key);
        else throw std::logic_error("unknown case slot");
        std::ostringstream output;
        output << result << ' ' << request.packedKey << ' '
            << Token(state.GetPendingHandleForAnalysis()) << '|' << host.trace;
        return output.str();
    }
}
int main(int argc, char** argv)
{
    if (argc==8 && std::string(argv[1])=="--case")
    {
        std::uint64_t v[6]{};
        for (unsigned i=0;i<6;++i) v[i]=std::stoull(argv[i+2]);
        std::cout << Case(unsigned(v[0]),std::uint32_t(v[1]),v[2],v[3],v[4]!=0,std::uint32_t(v[5])) << '\n';
        return EXIT_SUCCESS;
    }
    wxDialogueState state;
    Require(state.GetStateSelectorForAnalysis()==32 && state.IsExactly(wxDialogueState::ClassID)
        && state.IsKindOf(wxCharacterState::ClassID), "constructor selector and RTTI");
    Require(dynamic_cast<wxDialogueState*>(spRTTIManager::Instance().Create(wxDialogueState::ClassID).get()),
        "registered factory");
    Host host(state); state.SetBindingsForAnalysis(Pointer(1),Pointer(2),&host);
    state.SetPendingHandleForAnalysis(Pointer(11));
    state.SetTransitionFlagsForAnalysis(false,false,false,false,false);
    auto clone=state.Clone(); auto* fresh=dynamic_cast<wxDialogueState*>(clone.get());
    Require(fresh && fresh->GetStateSelectorForAnalysis()==32 && !fresh->GetPendingHandleForAnalysis()
        && fresh->GetTransitionFlagsForAnalysis()==std::array<bool,5>{true,true,true,true,true},
        "clone uses constructor state and empty host binding");
    wxDialogueState target; target.SetPendingHandleForAnalysis(Pointer(33)); spCloneManager manager;
    Require(state.vfunc_14(target,manager) && target.GetPendingHandleForAnalysis()==Pointer(33),
        "empty inherited Copy preserves target handle");
    Require(host.trace.empty(), "Copy and clone do not send messages or play");
    for (std::uint32_t action=0;action<256;++action)
    {
        const auto expected=action>=28 && action<=57 ? (action<<7)|8u : 0u;
        Require(wxDialogueState::ComposeDialogueKeyForAnalysis(action<<7)==expected,
            "inclusive dialogue action range; fallback also clears mode eight");
    }
    Require(Case(0x1C,28u<<7,11,22,true,0)==
        "1 3592 22|message@11;predicate@11;fade:11@11;lookup:3592@0;predicate@0;start:22:1:2:1@0",
        "entry message precedes release, key filtering and playback");
    Require(Case(0x30,28u<<7,11,22,true,0)==
        "2 3592 22|lookup:3592@11;predicate@11;stop:11@11;predicate@0;start:22:1:2:1@0",
        "ordinary update forces stop while playback uses owner fade selector");
    Require(Case(0x30,28u<<7,11,11,true,0)=="2 3592 11|lookup:3592@11",
        "same handle skips stop and playback");
    Require(Case(0x34,0,11,22,false,70)=="0 0 11|game@11"
        && Case(0x34,70,11,22,false,71)=="1 70 11|game@11", "game-state permission gate");
    std::cout << "Dialogue state checks passed\n";
}
