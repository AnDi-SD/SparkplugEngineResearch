#include "Code/wxBasicMovingState.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;

namespace
{
    void Require(bool condition, const char* message)
    {
        if (!condition) { std::cerr << message << '\n'; std::exit(EXIT_FAILURE); }
    }

    struct Host final : wxCharacterStateHost
    {
        unsigned queries = 0;
        bool result = false;
        void* ResolveAnimationForAnalysis(void*, std::uint32_t) override { return nullptr; }
        bool OwnerPredicateForAnalysis(void*) override { return false; }
        void ResetCompletionForAnalysis(void*, void*) override {}
        void StartAnimationForAnalysis(void*, void*, bool, std::uint32_t, bool) override {}
        void StopAnimationForAnalysis(void*, void*) override {}
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
    std::cout << "wxBasicMovingState checks passed\n";
}
