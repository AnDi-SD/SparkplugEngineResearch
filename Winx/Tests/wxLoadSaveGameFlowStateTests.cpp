#include "Code/wxLoadSaveGameFlowState.h"
#include "Analysis/PC/wxLoadSaveGameFlowStateAbi.h"
#include "Analysis/PS2/wxLoadSaveGameFlowStateAbi.h"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;

namespace
{
    void Check(const bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
    void* Token(const std::uintptr_t value) { return reinterpret_cast<void*>(value); }

    struct Host final : wxLoadSaveGameFlowStateHost
    {
        wxGameFlowResourcesForAnalysis resources;
        std::vector<std::string> effects;
        std::uint32_t state = 0, flags = 0, resetState = 0;
        void* currentOwner = Token(0x100);
        void* firstOwnerInput = Token(1);
        void* currentManager = Token(10);
        bool unavailableOwner = false, unavailableInput = false;
        std::uint32_t parentConstructions = 0;

        void* BorrowInputForAnalysis() override { ++parentConstructions; return Token(0x18); }
        spTaskTimer::ClockReadingForAnalysis ReadClockForAnalysis() override { return {1200,1}; }
        wxGameFlowResourcesForAnalysis& ResourcesForAnalysis() override { return resources; }
        void* ResolveHookInputOwnerForAnalysis() override { return unavailableOwner ? nullptr : currentOwner; }
        void* ReadHookInputForAnalysis(void* owner) override
        {
            if (unavailableInput) return nullptr;
            Check(owner == Token(0x100) || owner == Token(0x200), "Known captured input owner");
            return owner == Token(0x100) ? firstOwnerInput : Token(3);
        }
        void ResetHookInputForAnalysis(void* input) override
        {
            effects.push_back("R" + std::to_string(reinterpret_cast<std::uintptr_t>(input)));
            if (flags & 1) firstOwnerInput = Token(2);
            if (flags & 2) currentOwner = Token(0x200);
            if (flags & 4) state = resetState;
        }
        void SetHookInputModeForAnalysis(void* input, std::uint32_t mode) override
        {
            effects.push_back("M" + std::to_string(reinterpret_cast<std::uintptr_t>(input)) + ":" + std::to_string(mode));
            if (flags & 8) currentManager = Token(11);
        }
        void* ResolveLoadSaveManagerForAnalysis() override { return currentManager; }
        std::uint32_t ReadLoadSaveStateForAnalysis(void* manager) override
        {
            Check(manager == currentManager, "Read current load-save manager");
            effects.push_back("S" + std::to_string(state));
            return state;
        }
        void* ResolveLoadSaveControllerForAnalysis() override { return Token(20); }
        void InvokeLoadSaveControllerForAnalysis(void* controller, std::uint32_t parameter, std::uint32_t value) override
        {
            Check(controller == Token(20), "Controller receiver");
            effects.push_back("C" + std::to_string(parameter) + ":" + std::to_string(value));
        }
        void UpdateLoadSaveManagerForAnalysis(void* manager) override
        {
            Check(manager == Token(10) || manager == Token(11), "Update manager receiver");
            effects.push_back(manager == Token(10) ? "U1" : "U2");
        }
    };

    std::string Join(const std::vector<std::string>& effects)
    {
        std::string result;
        for (const auto& effect : effects) { if (!result.empty()) result += ','; result += effect; }
        return result.empty() ? "-" : result;
    }

    void Emit()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream row(line);
            std::string platform;
            std::uint32_t hook;
            Host host;
            if (!(row >> platform >> hook >> host.state >> host.flags >> host.resetState)
                || (platform != "PC" && platform != "PS2") || (hook != 24 && hook != 30))
                throw std::invalid_argument("Expected PC/PS2 hook24/30 state flags resetState");
            const auto profile = platform == "PC" ? wxLoadSaveGameFlowState::HookPlatformForAnalysis::PC
                : wxLoadSaveGameFlowState::HookPlatformForAnalysis::PS2;
            wxLoadSaveGameFlowState object(host,profile);
            bool result = true;
            if (hook == 24) object.FlowSlot24ForAnalysis(); else result = object.FlowSlot30ForAnalysis();
            std::cout << result << ' ' << Join(host.effects) << '\n';
        }
    }

    void TestHookOrder()
    {
        Host host;
        wxLoadSaveGameFlowState object(host);
        Check(object.GetBorrowedInputForAnalysis() == Token(0x18), "Parent input differs from active hook input");
        host.state = 4;
        object.FlowSlot24ForAnalysis();
        Check(Join(host.effects) == "R1", "PC slot24 resets input without PS2 services");
        host.effects.clear();host.flags = 3;
        Check(object.FlowSlot30ForAnalysis(), "PC slot30 literal true");
        Check(Join(host.effects) == "R1,M2:7", "Reload input member from captured owner after reset");
        Check(host.currentOwner == Token(0x200), "Reset also replaced global owner");
    }

    void TestPS2Branches()
    {
        for (const auto state : {0u,3u,4u,5u,0xFFFFFFFFu})
        {
            Host host;host.state = state;
            wxLoadSaveGameFlowState object(host,wxLoadSaveGameFlowState::HookPlatformForAnalysis::PS2);
            object.FlowSlot24ForAnalysis();
            const auto expected = std::string("R1,S") + std::to_string(state) + (state == 4 ? ",C54:1" : "");
            Check(Join(host.effects) == expected, "PS2 controller branch is exact state==4");
        }
        Host host;host.state = 4;host.flags = 4;host.resetState = 3;
        wxLoadSaveGameFlowState object(host,wxLoadSaveGameFlowState::HookPlatformForAnalysis::PS2);
        object.FlowSlot24ForAnalysis();
        Check(Join(host.effects) == "R1,S3", "Read manager state after reset callback");
        host.effects.clear();host.flags = 3 | 8;
        Check(object.FlowSlot30ForAnalysis(), "PS2 slot30 literal true after consumer");
        Check(Join(host.effects) == "R1,M2:7,U2", "PS2 resolves manager after reset and mode callbacks");
    }

    void TestCloneAndGuards()
    {
        Host host;
        wxLoadSaveGameFlowState object(host,wxLoadSaveGameFlowState::HookPlatformForAnalysis::PS2);
        Check(object.IsExactly(wxLoadSaveGameFlowState::ClassID) && object.IsKindOf(wxGameFlowState::ClassID),
            "Derived state RTTI and parent");
        object.SetCodesForAnalysis(1,2,3);
        spCloneManager manager;
        auto clone = manager.Clone(object);
        auto* derived = dynamic_cast<wxLoadSaveGameFlowState*>(clone.get());
        Check(derived && derived->GetHookPlatformForAnalysis() == object.GetHookPlatformForAnalysis(),
            "Clone preserves own analytical hook profile");
        Check(derived->GetStateForAnalysis().codes == std::array<std::uint32_t,3>{0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF},
            "Inherited no-op Copy leaves fresh constructor codes");
        Check(&derived->TimerForAnalysis() != &object.TimerForAnalysis() && host.parentConstructions == 2,
            "Clone constructs its own parent/timer");
        Check(manager.FindClone(object) == nullptr, "Clone transaction clears temporary mapping");
        wxLoadSaveGameFlowState::SetFactoryHostForAnalysis(&host);
        auto created = spRTTIManager::Instance().Create(wxLoadSaveGameFlowState::ClassID);
        Check(dynamic_cast<wxLoadSaveGameFlowState*>(created.get()) != nullptr, "Derived registered factory");
        wxLoadSaveGameFlowState::SetFactoryHostForAnalysis(nullptr);
        host.effects.clear();host.unavailableOwner = true;
        bool rejected = false;
        try { object.FlowSlot30ForAnalysis(); } catch (const std::logic_error&) { rejected = true; }
        Check(rejected && host.effects.empty(), "Missing borrowed owner is a portable refusal");
        host.unavailableOwner = false;host.unavailableInput = true;rejected = false;
        try { object.FlowSlot24ForAnalysis(); } catch (const std::logic_error&) { rejected = true; }
        Check(rejected && host.effects.empty(), "Missing borrowed input is a portable refusal");
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--emit") { Emit(); return 0; }
        TestHookOrder();TestPS2Branches();TestCloneAndGuards();
        std::cout << "PASS wxLoadSaveGameFlowState ordering, platform branches, owner reload and lifetime\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL " << error.what() << '\n';return 1;
    }
}
