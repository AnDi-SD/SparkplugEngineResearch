#include "Analysis/Host/wxCrouchingStateHost.h"
#include "Code/wxCrouchingState.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
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
        unsigned bits = 0, index = 0;
        for (bool flag : state.GetTransitionFlagsForAnalysis()) bits |= unsigned(flag) << index++;
        return bits;
    }
    struct Host final : wxCrouchingStateHost
    {
        wxCrouchingState& state;
        void* next = Pointer(22);
        void* first = nullptr;
        void* second = nullptr;
        bool predicate = false;
        float motion = 0;
        std::uint32_t word = 0xFFFFFFFF;
        unsigned service = 165;
        std::string trace;
        explicit Host(wxCrouchingState& value) : state(value) {}
        void Event(const std::string& name)
        {
            if (!trace.empty()) trace += ';';
            trace += name + '@' + std::to_string(Token(state.GetPendingHandleForAnalysis()))
                + ':' + std::to_string(Flags(state));
        }
        float CrouchingMotionForAnalysis(void*) override { Event("motion"); return motion; }
        void SetEntryServiceByteForAnalysis(std::uint8_t value) override
        { Require(value == 0, "entry service argument"); Event("service"); service = value; }
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
        { Require(duration == 0.4f, "fade duration"); Event("fade:" + std::to_string(Token(handle))); }
        bool IsPendingAnimationCompleteForAnalysis(void*, void* handle, bool consume) override
        {
            Require(consume, "consuming completion query"); Event("query:" + std::to_string(Token(handle)));
            if (!handle) return true;
            if (second && second == handle)
            { second = nullptr; if (first == handle) first = nullptr; return true; }
            if (first && first == handle) { first = nullptr; return true; }
            return false;
        }
        void ClearOwnerActionControlForAnalysis(void*) override { Event("word"); word = 0; }
    };
    std::string Case(unsigned slot, std::uint32_t key, std::uintptr_t pending,
        std::uintptr_t next, std::uintptr_t first, std::uintptr_t second,
        bool predicate, unsigned flags, std::uint32_t motionBits)
    {
        wxCrouchingState state;
        Host host(state);
        host.next = Pointer(next); host.first = Pointer(first); host.second = Pointer(second);
        host.predicate = predicate;
        std::memcpy(&host.motion, &motionBits, 4);
        state.SetBindingsForAnalysis(Pointer(0x100), Pointer(0x200), &host);
        state.SetTransitionFlagsForAnalysis(flags&1, flags&2, flags&4, flags&8, flags&16);
        state.SetPendingHandleForAnalysis(Pointer(pending));
        wxAnimationRequestForAnalysis request{key};
        unsigned result = 2;
        if (slot == 0x1C) result = state.vfunc_1C(request);
        else if (slot == 0x20) result = state.vfunc_20(request);
        else if (slot == 0x30) state.vfunc_30(request);
        else if (slot == 0x34) result = state.vfunc_34(key);
        else throw std::logic_error("unknown case slot");
        std::ostringstream output;
        output << result << ' ' << request.packedKey << ' ' << Token(state.GetPendingHandleForAnalysis())
            << ' ' << Flags(state) << ' ' << host.word << ' ' << host.service
            << ' ' << Token(host.first) << ' ' << Token(host.second) << '|' << host.trace;
        return output.str();
    }
}

int main(int argc, char** argv)
{
    if (argc == 11 && std::string(argv[1]) == "--case")
    {
        std::uint64_t v[9]{};
        for (unsigned i=0; i<9; ++i) v[i] = std::stoull(argv[i+2]);
        std::cout << Case(unsigned(v[0]), std::uint32_t(v[1]), v[2], v[3], v[4], v[5],
            v[6]!=0, unsigned(v[7]), std::uint32_t(v[8])) << '\n';
        return EXIT_SUCCESS;
    }
    wxCrouchingState state;
    Require(state.GetStateSelectorForAnalysis() == 2
        && state.IsExactly(wxCrouchingState::ClassID) && state.IsKindOf(wxCharacterState::ClassID),
        "constructor and RTTI");
    Require(dynamic_cast<wxCrouchingState*>(spRTTIManager::Instance().Create(wxCrouchingState::ClassID).get()),
        "registered factory");
    Host host(state);
    state.SetBindingsForAnalysis(Pointer(1), Pointer(2), &host);
    state.SetTransitionFlagsForAnalysis(false,false,false,false,false);
    state.SetPendingHandleForAnalysis(Pointer(11));
    auto clone = state.Clone();
    auto* fresh = dynamic_cast<wxCrouchingState*>(clone.get());
    Require(fresh && fresh->GetStateSelectorForAnalysis() == 2 && Flags(*fresh) == 31
        && !fresh->GetPendingHandleForAnalysis(), "clone retains fresh flags and bindings");
    wxAnimationRequestForAnalysis request{};
    bool rejected = false;
    try { fresh->vfunc_30(request); } catch (const std::logic_error&) { rejected = true; }
    Require(rejected, "unbound update fails explicitly");
    wxCrouchingState target;
    target.SetPendingHandleForAnalysis(Pointer(33));
    spCloneManager manager;
    Require(state.vfunc_14(target,manager) && target.GetPendingHandleForAnalysis() == Pointer(33),
        "inherited empty Copy leaves target untouched");
    Require(host.trace.empty(), "clone and Copy do not play or release animations");
    for (unsigned code=0; code<45; ++code)
        Require(state.vfunc_34(code) == (code!=1 && code!=3 && code!=4 && code!=5 && code!=8),
            "forbidden transition table");
    Require(wxCrouchingState::ComposeCrouchingKeyForAnalysis(0xFFFFFFFF,0.0f) == 0xF007FF8F
        && wxCrouchingState::ComposeCrouchingKeyForAnalysis(0xFFFFFFFF,0.2f) == 0xF007FFDF
        && wxCrouchingState::ComposeCrouchingKeyForAnalysis(0x200,0) == 0
        && wxCrouchingState::ComposeCrouchingKeyForAnalysis(0x280,0) == 0x280,
        "movement threshold and exact action-field filtering");
    Require(wxCrouchingState::ComposeCrouchingKeyForAnalysis(0xFFFFFFFF,
        std::numeric_limits<float>::quiet_NaN()) == 0xF007FFDF, "unordered comparison follows moving path");
    Require(Case(0x1C,0xFFFFFFFF,11,22,0,0,false,31,0) ==
        "0 4029120527 22 30 0 0 0 0|lookup:4029120527@11:31;predicate@11:31;stop:11@11:31;reset:22@0:31;predicate@0:31;start:22:0:0:1@0:31;service@22:31;word@22:30",
        "entry queues, sets service, clears once flag, then clears control");
    Require(Case(0x20,0xFFFFFFFF,11,22,0,0,false,0,0) ==
        "0 4294967295 11 0 0 165 0 0|query:11@11:0;word@11:0",
        "incomplete exit clears movement and retains handle");
    Require(Case(0x30,0xFFFFFFFF,11,11,0,0,false,31,0) ==
        "2 4027056015 11 31 4294967295 165 0 0|motion@11:31;lookup:4027056015@11:31",
        "same animation avoids playback and control clear");
    std::cout << "Crouching state checks passed\n";
}
