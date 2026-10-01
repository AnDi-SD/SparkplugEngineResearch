#include "Analysis/Host/wxCharacterControlStateHost.h"
#include "Code/wxMinotaurStunnedState.h"
#include "Code/wxWayToGoState.h"
#include "Code/wxFrogHurtState.h"
#include "Code/wxShadowBeastHurtState.h"

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
    struct Host final : wxCharacterControlStateHost
    {
        wxCharacterState& state;
        void* next = Pointer(22);
        void* first = nullptr;
        void* second = nullptr;
        bool predicate = false;
        std::uint32_t word = 0xFFFFFFFF;
        std::uint8_t byte1A = 0xA5, byte60 = 0xA5, byte61 = 0xA5;
        std::string trace;
        explicit Host(wxCharacterState& value) : state(value) {}
        void Event(const std::string& name)
        {
            if (!trace.empty()) trace += ';';
            trace += name + '@' + std::to_string(Token(state.GetPendingHandleForAnalysis()));
        }
        void* ResolveAnimationForAnalysis(void*, std::uint32_t key) override
        { Event("lookup:" + std::to_string(key)); return next; }
        bool OwnerPredicateForAnalysis(void*) override
        { Event("predicate"); return predicate; }
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
        {
            Require(duration == 0.4f, "native fade duration");
            Event("fade:" + std::to_string(Token(handle)));
        }
        bool IsPendingAnimationCompleteForAnalysis(void*, void* handle, bool consume) override
        {
            Require(consume, "consuming query"); Event("query:" + std::to_string(Token(handle)));
            if (!handle) return true;
            if (second && second == handle)
            {
                second = nullptr;
                if (first == handle) first = nullptr;
                return true;
            }
            if (first && first == handle) { first = nullptr; return true; }
            return false;
        }
        void ClearOwnerActionControlForAnalysis(void*) override
        { Event("word"); word = 0; }
        void ClearOwnerControlByteForAnalysis(void*, std::uint32_t offset) override
        {
            Require(offset == 0x1A || offset == 0x60 || offset == 0x61, "known control-byte offset");
            Event("byte:" + std::to_string(offset));
            (offset == 0x1A ? byte1A : offset == 0x60 ? byte60 : byte61) = 0;
        }
    };
    std::string Case(unsigned kind, unsigned slot, std::uint32_t key,
        std::uintptr_t pending, std::uintptr_t next, std::uintptr_t first,
        std::uintptr_t second, bool predicate)
    {
        std::unique_ptr<wxCharacterState> state;
        switch (kind)
        {
        case 0: state = std::make_unique<wxWayToGoState>(); break;
        case 1: state = std::make_unique<wxMinotaurStunnedState>(); break;
        case 2: state = std::make_unique<wxFrogHurtState>(); break;
        case 3: state = std::make_unique<wxShadowBeastHurtState>(); break;
        default: throw std::logic_error("unknown state kind");
        }
        Host host(*state);
        host.next = Pointer(next); host.first = Pointer(first); host.second = Pointer(second);
        host.predicate = predicate;
        state->SetBindingsForAnalysis(Pointer(0x100), Pointer(0x200), &host);
        state->SetPendingHandleForAnalysis(Pointer(pending));
        wxAnimationRequestForAnalysis request{key};
        unsigned result = 2;
        switch (slot)
        {
        case 0x1C: result = state->vfunc_1C(request); break;
        case 0x20: result = state->vfunc_20(request); break;
        case 0x24: state->vfunc_24(); break;
        case 0x28: state->vfunc_28(request); break;
        case 0x2C: state->vfunc_2C(request); break;
        case 0x30: state->vfunc_30(request); break;
        case 0x34: result = state->vfunc_34(key); break;
        default: throw std::logic_error("unknown case slot");
        }
        std::ostringstream output;
        output << result << ' ' << request.packedKey << ' '
            << Token(state->GetPendingHandleForAnalysis()) << ' ' << host.word << ' '
            << unsigned(host.byte60) << ' ' << unsigned(host.byte61) << ' '
            << Token(host.first) << ' ' << Token(host.second);
        if (kind >= 2) output << ' ' << unsigned(host.byte1A);
        output << '|' << host.trace;
        return output.str();
    }
    template<class State> void CheckLifetime(std::uint32_t selector)
    {
        State state;
        Require(state.GetStateSelectorForAnalysis() == selector
            && state.IsExactly(State::ClassID) && state.IsKindOf(wxCharacterState::ClassID),
            "state selector and RTTI");
        Require(dynamic_cast<State*>(spRTTIManager::Instance().Create(State::ClassID).get()),
            "registered state factory");
        Host host(state);
        state.SetBindingsForAnalysis(Pointer(1), Pointer(2), &host);
        state.SetPendingHandleForAnalysis(Pointer(11));
        state.SetTransitionFlagsForAnalysis(false, false, false, false, false);
        auto clone = state.Clone();
        auto* fresh = dynamic_cast<State*>(clone.get());
        Require(fresh && fresh->GetStateSelectorForAnalysis() == selector
            && !fresh->GetPendingHandleForAnalysis()
            && fresh->GetTransitionFlagsForAnalysis() == std::array<bool,5>{true,true,true,true,true},
            "clone retains fresh runtime defaults");
        wxAnimationRequestForAnalysis request{};
        bool rejected = false;
        try { fresh->vfunc_30(request); } catch (const std::logic_error&) { rejected = true; }
        Require(rejected, "clone does not copy host binding");
        State destination;
        destination.SetPendingHandleForAnalysis(Pointer(33));
        spCloneManager manager;
        Require(state.vfunc_14(destination, manager)
            && destination.GetPendingHandleForAnalysis() == Pointer(33),
            "inherited empty Copy leaves target state");
        state.vfunc_40_ResetForAnalysis();
        Require(!state.GetPendingHandleForAnalysis() && state.GetStateSelectorForAnalysis() == selector,
            "base reset preserves selector");
        Require(host.trace.empty(), "copy, clone, reset and destructors do not play or release");
    }
}

int main(int argc, char** argv)
{
    if (argc == 10 && std::string(argv[1]) == "--case")
    {
        std::uint64_t values[8]{};
        for (unsigned i=0; i<8; ++i) values[i] = std::stoull(argv[i+2]);
        std::cout << Case(unsigned(values[0]), unsigned(values[1]), std::uint32_t(values[2]),
            values[3], values[4], values[5], values[6], values[7] != 0) << '\n';
        return EXIT_SUCCESS;
    }
    CheckLifetime<wxWayToGoState>(0);
    CheckLifetime<wxMinotaurStunnedState>(28);
    CheckLifetime<wxFrogHurtState>(10);
    CheckLifetime<wxShadowBeastHurtState>(10);
    Require(Case(2,0x1C,0xFFFFFFFF,11,22,0,0,false) ==
        "1 4026564608 22 0 165 165 0 0 0|byte:26@11;lookup:4026564608@11;reset:22@11;predicate@11;start:22:0:0:1@11;word@22",
        "Frog Hurt clears byte before lookup and queues without old release");
    Require(Case(3,0x1C,0xFFFFFFFF,11,22,0,0,true) ==
        "1 4026564608 22 0 165 165 0 0 165|lookup:4026564608@11;reset:22@11;predicate@11;start:22:0:2:1@11;word@22",
        "Shadow Hurt preserves byte1A and clears control after play");
    Require(Case(0,0x1C,0xFFFFFFFF,11,22,0,0,false) ==
        "1 4026564480 22 0 165 0 0 0|lookup:4026564480@11;reset:22@11;predicate@11;start:22:0:0:1@11;byte:97@22;word@22",
        "WayToGo entry ordering and key");
    Require(Case(1,0x1C,0xFFFFFFFF,11,22,0,0,false) ==
        "1 4026597376 22 0 0 165 0 0|lookup:4026597376@11;reset:22@11;predicate@11;start:22:0:0:1@11;byte:96@22;word@22",
        "Minotaur entry ordering and key");
    Require(Case(0,0x34,11,0,22,0,0,false) == "1 11 0 4294967295 165 165 0 0|",
        "WayToGo null handle short circuit");
    Require(Case(1,0x34,11,11,22,11,11,false) ==
        "0 11 11 4294967295 165 165 0 0|query:11@11;query:11@11",
        "Minotaur consumes matching records then queries again");
    Require(Case(1,0x34,11,11,22,0,0,true) ==
        "1 11 0 4294967295 165 165 0 0|query:11@11;predicate@11;fade:11@11",
        "Minotaur unfinished animation fades and permits target eleven");
    Require(Case(1,0x34,12,11,22,0,0,false) ==
        "0 12 11 4294967295 165 165 0 0|query:11@11",
        "Minotaur ordinary target only queries");
    std::cout << "Control state checks passed\n";
}
