#include "Code/wxBasicMovingState.h"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
#include <string>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;

namespace
{
    void Require(bool condition, const char* message)
    {
        if (!condition) { std::cerr << message << '\n'; std::exit(EXIT_FAILURE); }
    }

    struct Host final : wxBasicMovingStateHost
    {
        unsigned queries = 0;
        bool result = false;
        float magnitude = 0.0f;
        bool movementFlag = false;
        bool randomBelowHalf = false;
        void* resolved = reinterpret_cast<void*>(0x400);
        std::uint32_t resolvedKey = 0;
        std::vector<std::string> calls;
        float MovementMagnitudeForAnalysis(void* owner) override
        {
            Require(owner == reinterpret_cast<void*>(0x100), "movement owner");
            calls.emplace_back("magnitude");
            return magnitude;
        }
        bool MovementFlagForAnalysis(void* owner) override
        {
            Require(owner == reinterpret_cast<void*>(0x100), "movement flag owner");
            calls.emplace_back("flag");
            return movementFlag;
        }
        void* ResolveAnimationForAnalysis(void* owner, std::uint32_t key) override
        {
            Require(owner == reinterpret_cast<void*>(0x100), "animation owner");
            calls.emplace_back("resolve");
            resolvedKey = key;
            return resolved;
        }
        bool OwnerPredicateForAnalysis(void*) override { return false; }
        void ResetCompletionForAnalysis(void*, void*) override
        { calls.emplace_back("reset completion"); }
        void StartAnimationForAnalysis(void*, void* handle, bool mode,
            std::uint32_t fade, bool interrupt) override
        {
            Require(handle == resolved && fade == 0 && interrupt,
                "movement start args");
            calls.emplace_back(mode ? "start movement" : "start random");
        }
        void StopAnimationForAnalysis(void*, void*) override
        { calls.emplace_back("stop"); }
        void FadeAnimationForAnalysis(void*, void*, float) override {}
        bool IsPendingAnimationCompleteForAnalysis(void* receiver, void* handle,
            bool consume) override
        {
            Require(receiver == reinterpret_cast<void*>(0x200)
                && handle == reinterpret_cast<void*>(0x300) && consume,
                "pending query inputs");
            ++queries;
            return result;
        }
        void ClearOwnerActionControlForAnalysis(void*) override {}
        void PrepareRandomMovementForAnalysis(wxBasicMovingState&) override
        { calls.emplace_back("prepare random"); }
        bool RandomMovementBelowHalfForAnalysis() override
        { calls.emplace_back("draw random"); return randomBelowHalf; }
        void FinishRandomMovementForAnalysis(wxBasicMovingState&) override
        { calls.emplace_back("finish random"); }
    };
}

int main()
{
    static_assert(wxBasicMovingState::ClassID == 0x1D533B89);
    auto instance = spRTTIManager::Instance().Create(wxBasicMovingState::ClassID);
    auto* state = dynamic_cast<wxBasicMovingState*>(instance.get());
    Require(state && state->IsKindOf(wxCharacterState::ClassID)
        && state->GetStateSelectorForAnalysis() == 0, "factory and base");
    Require(dynamic_cast<wxBasicMovingState*>(state->Clone().get()) != nullptr,
        "empty clone preserves type");

    Host host;
    state->SetBindingsForAnalysis(reinterpret_cast<void*>(0x100),
        reinterpret_cast<void*>(0x200), &host);
    state->SetPendingHandleForAnalysis(reinterpret_cast<void*>(0x300));
    for (std::uint32_t target : {0u, 9u, 10u, 11u, 12u})
        Require(state->vfunc_34(target), "other current state allows transition");
    Require(host.queries == 0, "current selector short circuits pending query");

    state->SetCurrentSelectorForAnalysis(9);
    Require(state->vfunc_34(10) && state->vfunc_34(11),
        "targets 10 and 11 bypass pending query");
    Require(host.queries == 0, "target short circuit");
    state->SetPendingHandleForAnalysis(nullptr);
    Require(state->vfunc_34(12) && host.queries == 0, "null handle short circuit");
    state->SetPendingHandleForAnalysis(reinterpret_cast<void*>(0x300));
    Require(!state->vfunc_34(12) && host.queries == 1, "unmatched pending handle");
    host.result = true;
    Require(state->vfunc_34(12) && host.queries == 2, "consumed pending handle");

    wxAnimationRequestForAnalysis request{};
    bool explicitBoundary = false;
    try { state->vfunc_30(request); }
    catch (const std::logic_error&) { explicitBoundary = true; }
    Require(explicitBoundary, "unrecovered movement body stays explicit");

    Require(wxBasicMovingState::ComposeMovingKeyForAnalysis(
        0xFFFFFFFF, 0.1f, false) == 0xF01F800F
        && wxBasicMovingState::ComposeMovingKeyForAnalysis(
            0xFFFFFFFF, 0.1f, true) == 0xF09F800F
        && wxBasicMovingState::ComposeMovingKeyForAnalysis(
            0xFFFFFFFF, 0.2f, true) == 0xF01F805F
        && wxBasicMovingState::ComposeMovingKeyForAnalysis(
            0xFFFFFFFF, 0.5f, false) == 0xF09F805F
        && wxBasicMovingState::ComposeMovingKeyForAnalysis(
            0xFFFFFFFF, std::numeric_limits<float>::quiet_NaN(), false)
            == 0xF09F805F,
        "PC/PS2 movement thresholds and packed masks");
    state->SetCurrentSelectorForAnalysis(0);
    state->SetMovementBindingsForAnalysis(reinterpret_cast<void*>(0x100),
        reinterpret_cast<void*>(0x200), &host);
    state->SetPendingHandleForAnalysis(reinterpret_cast<void*>(0x300));
    host.magnitude = 0.1f;
    host.movementFlag = true;
    host.calls.clear();
    request.packedKey = 0xFFFFFFFF;
    state->vfunc_30(request);
    Require(request.packedKey == 0xF09F800F
        && host.resolvedKey == request.packedKey
        && state->GetPendingHandleForAnalysis() == host.resolved,
        "movement request resolves and stores new handle");
    Require(host.calls == std::vector<std::string>{
        "magnitude", "flag", "resolve", "stop", "start movement"},
        "movement callback order");
    host.magnitude = 0.2f;
    host.calls.clear();
    request.packedKey = 0xFFFFFFFF;
    state->vfunc_30(request);
    Require(request.packedKey == 0xF01F805F
        && host.calls == std::vector<std::string>{"magnitude", "resolve"},
        "equal pending handle skips replay and high speed skips flag read");
    Require(wxBasicMovingState::ComposeRandomMovingKeyForAnalysis(
        0xFFFFFFFF, true) == 0xF01F81BF
        && wxBasicMovingState::ComposeRandomMovingKeyForAnalysis(
            0xFFFFFFFF, false) == 0xF01F81CF,
        "random movement packed masks");
    state->SetCurrentSelectorForAnalysis(9);
    state->SetTransitionFlagsForAnalysis(true, true, true, true, true);
    state->SetPendingHandleForAnalysis(reinterpret_cast<void*>(0x300));
    host.randomBelowHalf = true;
    host.calls.clear();
    request.packedKey = 0xFFFFFFFF;
    state->vfunc_30(request);
    Require(request.packedKey == 0xF01F81BF
        && state->GetPendingHandleForAnalysis() == host.resolved
        && !state->GetTransitionFlagsForAnalysis()[1],
        "random branch changes request, handle and once flag");
    Require(host.calls == std::vector<std::string>{"prepare random", "draw random",
        "resolve", "stop", "reset completion", "start random", "finish random"},
        "random branch callback order");
    host.calls.clear();
    request.packedKey = 0x12345678;
    state->vfunc_30(request);
    Require(request.packedKey == 0x12345678
        && host.calls == std::vector<std::string>{"finish random"},
        "random branch is one shot but always finishes");
    state->SetTransitionFlagsForAnalysis(true, true, true, true, true);
    state->SetPendingHandleForAnalysis(host.resolved);
    host.randomBelowHalf = false;
    host.calls.clear();
    request.packedKey = 0xFFFFFFFF;
    state->vfunc_30(request);
    Require(request.packedKey == 0xF01F81CF
        && host.calls == std::vector<std::string>{
            "prepare random", "draw random", "resolve", "finish random"},
        "second random branch and equal pending handle");
    std::cout << "wxBasicMovingState checks passed\n";
}
