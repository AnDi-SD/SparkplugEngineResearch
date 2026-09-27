#include "Analysis/PC/wxAttackAIActionAbi.h"
#include "Analysis/PS2/wxAttackAIActionAbi.h"
#include "Code/wxAttackAIAction.h"

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

    struct Host final : wxAttackAIActionHost
    {
        std::array<std::uint8_t, 0x30> command{};
        std::vector<std::uint32_t> commandWrites;
        std::vector<std::uint32_t> dispatchedSlots;
        const void* lastArgument = nullptr;
        bool flag = false;
        void* registry = nullptr;
        void* nearest = nullptr;
        std::uint32_t base = 100, minimum = 20, maximum = 30, random = 13;
        std::uint32_t selectedKey = 0xFFFFFFFF;
        std::uint32_t selectedParameter = 0xFFFFFFFF;

        void SetCommandByteForAnalysis(std::uint32_t offset,
            std::uint8_t value) noexcept override
        {
            command[offset] = value;
            commandWrites.push_back(offset);
        }
        bool GetOwnerFlag1C5ForAnalysis() noexcept override { return flag; }
        void* FindRegistryTargetForAnalysis() noexcept override { return registry; }
        void* FindNearestTargetForAnalysis() noexcept override { return nearest; }
        std::uint32_t GetTimerBaseForAnalysis() noexcept override { return base; }
        std::uint32_t GetOwnerMinForAnalysis() noexcept override { return minimum; }
        std::uint32_t GetOwnerMaxForAnalysis() noexcept override { return maximum; }
        std::uint32_t NextRandomForAnalysis() noexcept override { return random; }
        void SelectActionForAnalysis(std::uint32_t key,
            std::uint32_t parameter) noexcept override
        {
            selectedKey = key;
            selectedParameter = parameter;
        }
        void DispatchSlotForAnalysis(wxAttackAIAction&,
            std::uint32_t slot, const void* argument) noexcept override
        {
            dispatchedSlots.push_back(slot);
            lastArgument = argument;
        }
    };
}

int main()
{
    Require(sizeof(winx::evidence::pc::wxAttackAIActionLayout) == 0x3F4
        && sizeof(winx::evidence::ps2::wxAttackAIActionLayout) == 0x3F8,
        "paired native sizes");
    Host host;
    wxAttackAIAction::SetFactoryHostForAnalysis(&host);
    wxAttackAIAction action(host);
    Require(action.IsExactly(wxAttackAIAction::ClassID)
        && action.IsKindOf(wxAIAction::ClassID), "registered inheritance");
    Require(spRTTIManager::Instance().Create(wxAttackAIAction::ClassID)->IsExactly(
        wxAttackAIAction::ClassID), "factory");
    Require(action.GetTargetForAnalysis() == nullptr
        && action.GetStateForAnalysis() == 1
        && action.GetDeadlineForAnalysis() == 0
        && action.GetField3B4ForAnalysis() == 0
        && action.GetField3B8ForAnalysis() == 0
        && action.GetFlag3F0ForAnalysis() == 0, "constructor fields");

    int target = 0;
    host.nearest = &target;
    action.vfunc_30();
    Require(host.command[0x1D] == 1 && action.GetTargetForAnalysis() == &target
        && action.GetStateForAnalysis() == 4 && action.GetDeadlineForAnalysis() == 122
        && host.selectedKey == 0xFFFFFFFF, "perception entry and timer");
    host.flag = true;
    host.registry = nullptr;
    action.vfunc_30();
    Require(action.GetTargetForAnalysis() == nullptr && action.GetStateForAnalysis() == 0
        && action.GetDeadlineForAnalysis() == 0 && host.selectedKey == 1
        && host.selectedParameter == 0, "registry entry without target");

    host.commandWrites.clear();
    action.vfunc_34_ClearForAnalysis();
    Require(host.commandWrites == std::vector<std::uint32_t>({0x1D, 0x20, 0x21, 0x1F})
        && action.GetTargetForAnalysis() == nullptr, "exit write order and target");
    host.flag = false;
    host.nearest = nullptr;
    host.selectedKey = 0xFFFFFFFF;
    action.vfunc_38();
    Require(host.selectedKey == 0, "query selects key zero when no target");
    host.flag = true;
    host.selectedKey = 0xFFFFFFFF;
    action.vfunc_38();
    Require(host.selectedKey == 0xFFFFFFFF, "owner flag inhibits query transition");

    constexpr std::array<std::uint32_t, 7> slots{23, 18, 19, 20, 21, 22, 24};
    for (std::uint32_t i = 0; i != slots.size(); ++i)
    {
        action.SetOwnFieldsForAnalysis(nullptr, i, 0, 0, 0, 0);
        Require(action.vfunc_24() && host.dispatchedSlots.back() == slots[i],
            "state dispatch");
    }
    const auto dispatchCount = host.dispatchedSlots.size();
    action.SetOwnFieldsForAnalysis(nullptr, 7, 0, 0, 0, 0);
    Require(action.vfunc_24() && host.dispatchedSlots.size() == dispatchCount,
        "out-of-range state returns without dispatch");
    wxAIActionMessageForAnalysis special{0x273B};
    action.vfunc_0C(&special);
    wxAIActionMessageForAnalysis ignored{0x2754};
    action.vfunc_0C(&ignored);
    Require(host.dispatchedSlots.back() == 17
        && host.dispatchedSlots.size() == dispatchCount + 1
        && host.lastArgument == &special,
        "special notification dispatch and ignored notification");

    wxAttackAIAction destination(host);
    action.SetOwnFieldsForAnalysis(&target, 6, 42, 99, 77, 9);
    destination.SetOwnFieldsForAnalysis(nullptr, 1, 0, 0, 55, 8);
    spCloneManager manager;
    Require(action.vfunc_14(destination, manager), "derived copy");
    Require(destination.GetTargetForAnalysis() == &target
        && destination.GetStateForAnalysis() == 6
        && destination.GetDeadlineForAnalysis() == 42
        && destination.GetField3B4ForAnalysis() == 99
        && destination.GetField3B8ForAnalysis() == 55
        && destination.GetFlag3F0ForAnalysis() == 8,
        "copy transfers four words only");
    auto clone = action.Clone();
    auto* derived = dynamic_cast<wxAttackAIAction*>(clone.get());
    Require(derived && derived->GetTargetForAnalysis() == &target
        && derived->GetStateForAnalysis() == 6
        && derived->GetField3B8ForAnalysis() == 0
        && derived->GetFlag3F0ForAnalysis() == 0,
        "clone combines copy fields with constructor defaults");
    wxAttackAIAction::SetFactoryHostForAnalysis(nullptr);
    std::cout << "wxAttackAIAction reconstruction tests passed\n";
}
