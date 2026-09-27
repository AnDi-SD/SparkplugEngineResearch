#include "Analysis/PC/wxBacoAttackAIActionAbi.h"
#include "Analysis/PS2/wxBacoAttackAIActionAbi.h"
#include "Code/wxBacoAttackAIAction.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

namespace
{
    using namespace winx::reconstruction;
    using namespace sparkplug::reconstruction;

    void Require(bool ok, const char* reason)
    {
        if (!ok) { std::cerr << "FAILED: " << reason << '\n'; std::exit(1); }
    }

    struct Host final : wxBacoAttackAIActionHost
    {
        std::array<std::uint8_t, 0x30> command{};
        std::vector<std::uint32_t> writes;
        std::vector<std::uint32_t> states;
        void* registry = nullptr;
        void* nearest = nullptr;
        std::uint32_t selectedKey = 0xFFFFFFFF;
        std::uint32_t selectedParameter = 0xFFFFFFFF;
        std::uint32_t registryCalls = 0;
        std::uint32_t nearestCalls = 0;

        void SetCommandByteForAnalysis(std::uint32_t offset,
            std::uint8_t value) noexcept override
        {
            command[offset] = value;
            writes.push_back(offset);
        }
        void* FindRegistryTargetForAnalysis() noexcept override
        {
            ++registryCalls;
            return registry;
        }
        void* FindNearestTargetForAnalysis() noexcept override
        {
            ++nearestCalls;
            return nearest;
        }
        void SelectActionForAnalysis(std::uint32_t key,
            std::uint32_t parameter) noexcept override
        {
            selectedKey = key;
            selectedParameter = parameter;
        }
        void DispatchStateBodyForAnalysis(wxBacoAttackAIAction&,
            std::uint32_t state) noexcept override
        {
            states.push_back(state);
        }
    };

    struct NotificationProbe final : wxAIAction
    {
        std::uint32_t calls = 0;
        void vfunc_0C(const void*) noexcept override { ++calls; }
    };
}

int main()
{
    Require(sizeof(winx::evidence::pc::wxBacoAttackAIActionLayout) == 0x3CC
        && sizeof(winx::evidence::ps2::wxBacoAttackAIActionLayout) == 0x3D0,
        "paired native sizes");
    Host host;
    wxBacoAttackAIAction::SetFactoryHostForAnalysis(&host);
    wxBacoAttackAIAction action(host);
    Require(action.IsExactly(wxBacoAttackAIAction::ClassID)
        && action.IsKindOf(wxAIAction::ClassID), "registered inheritance");
    auto factoryObject = spRTTIManager::Instance().Create(wxBacoAttackAIAction::ClassID);
    Require(factoryObject && factoryObject->IsExactly(wxBacoAttackAIAction::ClassID),
        "factory with host");
    Require(action.GetTargetForAnalysis() == nullptr
        && action.GetStateForAnalysis() == 4
        && action.GetField3B0ForAnalysis() == 0
        && action.GetField3B4ForAnalysis() == 0
        && action.GetField3B8ForAnalysis() == 0x3CF5C28F
        && action.GetFlag3BCForAnalysis() == 0, "constructor fields");

    NotificationProbe child;
    action.SetCurrentActionForAnalysis(&child);
    wxAIActionMessageForAnalysis message{0x1C};
    action.vfunc_0C(&message);
    Require(child.calls == 0, "empty notification does not forward to child");
    action.SetCurrentActionForAnalysis(nullptr);

    int target = 0;
    host.registry = &target;
    action.SetPathIndexForAnalysis(456);
    action.SetClearableWordForAnalysis(123);
    action.vfunc_30();
    Require(host.command[0x1D] == 1 && host.registryCalls == 1
        && action.GetTargetForAnalysis() == &target
        && action.GetStateForAnalysis() == 4
        && action.GetField3B4ForAnalysis() == 0x47AFC800
        && action.GetZeroWordsForAnalysis()[0] == 0
        && action.GetClearableWordForAnalysis() == 123
        && host.selectedKey == 0xFFFFFFFF, "entry with registry target");

    host.registry = nullptr;
    action.vfunc_30();
    Require(action.GetTargetForAnalysis() == nullptr
        && host.registryCalls == 2 && host.selectedKey == 1
        && host.selectedParameter == 0, "entry without registry target");

    host.writes.clear();
    action.SetOwnFieldsForAnalysis(&target, 3, 11, 22, 33, 7);
    action.vfunc_34_ClearForAnalysis();
    Require(host.writes == std::vector<std::uint32_t>({0x1D, 0x20})
        && action.GetTargetForAnalysis() == nullptr
        && action.GetStateForAnalysis() == 3
        && action.GetField3B0ForAnalysis() == 11, "exit write order and scope");

    host.selectedKey = 0xFFFFFFFF;
    action.vfunc_38();
    Require(host.nearestCalls == 1 && host.selectedKey == 1,
        "no target requests action one");
    host.nearest = &target;
    host.selectedKey = 0xFFFFFFFF;
    action.vfunc_38();
    Require(host.nearestCalls == 2 && host.selectedKey == 0xFFFFFFFF,
        "found target does not transition");

    for (std::uint32_t state = 0; state != 5; ++state)
    {
        action.SetOwnFieldsForAnalysis(nullptr, state, 0, 0, 0, 0);
        Require(action.vfunc_24() && host.states.back() == state,
            "state body boundary");
    }
    const auto stateCalls = host.states.size();
    action.SetOwnFieldsForAnalysis(nullptr, 0xFFFFFFFF, 0, 0, 0, 0);
    Require(action.vfunc_24() && action.GetStateForAnalysis() == 0
        && host.states.size() == stateCalls, "invalid state resets to zero");

    wxBacoAttackAIAction destination(host);
    action.SetOwnFieldsForAnalysis(&target, 2, 42, 99, 77, 9);
    destination.SetOwnFieldsForAnalysis(nullptr, 1, 0, 0, 55, 8);
    spCloneManager manager;
    Require(action.vfunc_14(destination, manager), "derived copy");
    Require(destination.GetTargetForAnalysis() == &target
        && destination.GetStateForAnalysis() == 2
        && destination.GetField3B0ForAnalysis() == 42
        && destination.GetField3B4ForAnalysis() == 99
        && destination.GetField3B8ForAnalysis() == 77
        && destination.GetFlag3BCForAnalysis() == 9,
        "copy transfers five words and byte");
    auto clone = action.Clone();
    auto* derived = dynamic_cast<wxBacoAttackAIAction*>(clone.get());
    Require(derived && derived->GetTargetForAnalysis() == &target
        && derived->GetField3B8ForAnalysis() == 77
        && derived->GetFlag3BCForAnalysis() == 9,
        "clone includes modified own fields");
    wxBacoAttackAIAction::SetFactoryHostForAnalysis(nullptr);
    std::cout << "wxBacoAttackAIAction reconstruction tests passed\n";
}
