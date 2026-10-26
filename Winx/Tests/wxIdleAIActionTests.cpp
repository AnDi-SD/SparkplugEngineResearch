#include "Code/wxIdleAIAction.h"
#include "Analysis/PC/wxIdleAIActionAbi.h"
#include "Analysis/PS2/wxIdleAIActionAbi.h"

#include <array>
#include <bit>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;

namespace
{
    void Check(bool condition, const char* text) { if (!condition) throw std::runtime_error(text); }
    struct Owner { int id; std::uint8_t permission; };
    struct World final : wxIdleAIActionHost, wxPerceptionHost
    {
        Owner first{1, 1}, second{2, 1};
        int target = 3;
        bool commandPresent = true;
        std::array<std::uint8_t, 0x74> command{};
        wxPerception perception;
        std::map<const void*, std::array<float, 3>> positions;
        mutable std::string trace;
        mutable wxIdleAIAction* changeOwnerOnPosition = nullptr;
        mutable unsigned positionsRead = 0;
        unsigned resets = 0, queries = 0;
        std::vector<std::tuple<void*, std::uint32_t, std::uint32_t>> selections;
        World(unsigned fill = 0xA5, bool ps2 = false)
        {
            command.fill(static_cast<std::uint8_t>(fill));
            const std::uint32_t table = ps2 ? 0x494710 : 0x703090;
            for (unsigned index = 0; index != 4; ++index) command[index] = static_cast<std::uint8_t>(table >> (index * 8));
            positions[&first] = {0, 0, 0}; positions[&target] = {1, 0, 0};
            perception.BindForAnalysis(*this, nullptr, {});
        }
        ~World() override { perception.ResetForAnalysis(); }
        void BindTargets(bool hasOwner, bool hasTarget)
        {
            perception.ResetForAnalysis();
            perception.BindForAnalysis(*this, hasOwner ? &first : nullptr,
                hasTarget ? std::vector<void*>{&target} : std::vector<void*>{});
        }
        void* ReadCommandForAnalysis(void* owner) noexcept override
        { trace += "command:" + std::to_string(static_cast<Owner*>(owner)->id) + ";"; return commandPresent ? command.data() : nullptr; }
        void ResetCommandForAnalysis(wxIdleAIAction&, void* address) noexcept override
        {
            if (address != command.data()) std::terminate();
            trace += "reset;"; ++resets;
            // Explicit foreign reset fixture, independently proved original
            // PC5992E0/PS23730E0. No command class identity is invented.
            for (unsigned offset : {4u, 8u, 12u, 16u, 20u, 0x6Cu})
                for (unsigned byte = 0; byte != 4; ++byte) command[offset + byte] = 0;
            for (unsigned offset = 0x19; offset <= 0x31; ++offset) command[offset] = 0;
            for (unsigned offset = 0x5C; offset <= 0x60; ++offset) command[offset] = 0;
            command[0x64] = command[0x70] = 0;
        }
        std::uint8_t ReadPermissionForAnalysis(void* owner) noexcept override
        { trace += "permission:" + std::to_string(static_cast<Owner*>(owner)->id) + ";"; return static_cast<Owner*>(owner)->permission; }
        wxPerception& ReadPerceptionForAnalysis(void* owner) noexcept override
        { trace += "query:" + std::to_string(static_cast<Owner*>(owner)->id) + ";"; ++queries; return perception; }
        void SelectActionForAnalysis(void* owner, std::uint32_t key, std::uint32_t parameter) noexcept override
        { trace += "select:" + std::to_string(static_cast<Owner*>(owner)->id) + ";"; selections.emplace_back(owner, key, parameter); }
        std::array<float, 3> ReadNodePositionForAnalysis(const void* object) const override
        {
            trace += "position;"; ++positionsRead;
            if (changeOwnerOnPosition && object == &target) changeOwnerOnPosition->SetOwnerForAnalysis(const_cast<Owner*>(&second));
            return positions.at(object);
        }
        void DetachOwnerForAnalysis(wxPerception&, void*) noexcept override {}
    };
    struct SpyAction final : wxAIAction
    {
        unsigned notifications = 0;
        void vfunc_0C(const void*) noexcept override { ++notifications; }
    };
    std::string Hex(const std::array<std::uint8_t, 0x74>& bytes)
    {
        std::ostringstream output;
        for (unsigned value : bytes) output << std::hex << std::setw(2) << std::setfill('0') << value;
        return output.str();
    }
    void Replay()
    {
        std::string operation;
        while (std::cin >> operation)
        {
            if (operation == "e")
            {
                unsigned ps2, fill, command, permission;
                std::cin >> ps2 >> fill >> command >> permission;
                World world(fill, ps2 != 0); world.commandPresent = command != 0;
                world.first.permission = static_cast<std::uint8_t>(permission);
                wxIdleAIAction action(world, ps2 ? wxPerceptionNumericProfileForAnalysis::ExactSmallInteger :
                    wxPerceptionNumericProfileForAnalysis::PcFiniteNearest64);
                action.SetOwnerForAnalysis(&world.first); action.vfunc_30();
                std::cout << world.resets << ' ' << world.queries << ' ' << world.selections.size() << ' ' << Hex(world.command) << '\n';
            }
            else if (operation == "q")
            {
                unsigned ps2, permission, hasOwner, hasTarget;
                std::cin >> ps2 >> permission >> hasOwner >> hasTarget;
                World world; world.first.permission = static_cast<std::uint8_t>(permission);
                world.BindTargets(hasOwner != 0, hasTarget != 0);
                world.perception.StateForAnalysis().scratchWords30 = std::array<std::uint32_t, 3>{0xA5A5A5A5, 0xA5A5A5A5, 0xA5A5A5A5};
                wxIdleAIAction action(world, ps2 ? wxPerceptionNumericProfileForAnalysis::ExactSmallInteger :
                    wxPerceptionNumericProfileForAnalysis::PcFiniteNearest64);
                action.SetOwnerForAnalysis(&world.first); action.vfunc_38();
                const auto scratch = world.perception.StateForAnalysis().scratchWords30.value();
                std::cout << world.queries << ' ' << world.selections.size() << ' ';
                if (world.selections.empty()) std::cout << "0 0";
                else std::cout << std::get<1>(world.selections.front()) << ' ' << std::get<2>(world.selections.front());
                std::cout << ' ' << scratch[0] << ' ' << scratch[1] << ' ' << scratch[2] << '\n';
            }
            else if (operation == "n")
            {
                std::uint32_t code; std::cin >> code;
                World world; wxIdleAIAction action(world); SpyAction child;
                action.SetCurrentActionForAnalysis(&child); action.SetClearableWordForAnalysis(0xAABBCCDD);
                wxAIActionMessageForAnalysis message{code};
                action.vfunc_0C(&message); action.vfunc_34_ClearForAnalysis();
                std::cout << child.notifications << ' ' << action.GetClearableWordForAnalysis() << ' '
                    << unsigned(action.GetCurrentActionForAnalysis() == &child) << '\n';
                action.SetCurrentActionForAnalysis(nullptr);
            }
            else if (operation == "c")
            {
                unsigned hasCurrent, hasNested; std::cin >> hasCurrent >> hasNested;
                World world; wxIdleAIAction source(world), destination(world); SpyAction current, nested;
                source.SetOwnerForAnalysis(&world.first); destination.SetOwnerForAnalysis(&world.second);
                source.SetDurationMillisecondsForAnalysis(7); destination.SetDurationMillisecondsForAnalysis(9);
                source.SetClearableWordForAnalysis(11); destination.SetClearableWordForAnalysis(13);
                current.SetClearableWordForAnalysis(17);
                current.SetCurrentActionForAnalysis(hasNested ? &nested : nullptr);
                destination.SetCurrentActionForAnalysis(hasCurrent ? &current : nullptr);
                spCloneManager manager; Check(source.vfunc_14(destination, manager), "Idle replay Copy");
                std::cout << unsigned(destination.GetOwnerForAnalysis() == &world.second ? 2 : 0) << ' '
                    << destination.GetDurationMillisecondsForAnalysis() << ' ' << destination.GetClearableWordForAnalysis() << ' '
                    << unsigned(destination.GetCurrentActionForAnalysis() != nullptr) << ' ' << current.GetClearableWordForAnalysis() << '\n';
                current.SetCurrentActionForAnalysis(nullptr);
            }
            else throw std::invalid_argument("Idle replay operation");
            Check(bool(std::cin), "complete Idle replay input");
        }
    }
    void Operations()
    {
        World world;
        wxIdleAIAction action(world);
        action.SetOwnerForAnalysis(&world.first);
        world.first.permission = 0;
        const auto commandBefore = world.command;
        action.vfunc_30();
        Check(world.trace == "command:1;reset;permission:1;" && world.resets == 1 && world.queries == 0,
            "entry resets command before lazy permission test");
        Check(world.command[0x18] == commandBefore[0x18] && world.command[0x32] == commandBefore[0x32]
            && world.command[0x19] == 0 && world.command[4] == 0, "foreign reset fixture preserves unrelated command bytes");
        world.commandPresent = false; world.first.permission = 1; world.trace.clear();
        action.vfunc_30();
        Check(world.trace == "command:1;permission:1;query:1;" && world.resets == 1 && world.queries == 1,
            "missing command still invokes query and null perception owner returns no target");
        world.BindTargets(true, true); world.trace.clear();
        action.vfunc_30();
        Check(world.trace == "command:1;permission:1;query:1;position;position;select:1;", "entry uses actual shared perception and selector order");
        Check(world.selections.back() == std::tuple<void*, std::uint32_t, std::uint32_t>{&world.first, 2, 0}, "Idle selector key2 parameter0");
        world.trace.clear(); world.changeOwnerOnPosition = &action;
        action.vfunc_38();
        Check(std::get<0>(world.selections.back()) == &world.second && world.trace.ends_with("select:2;"),
            "selector reloads action owner after perception");
        world.changeOwnerOnPosition = nullptr;
        SpyAction child;
        action.SetCurrentActionForAnalysis(&child); action.SetClearableWordForAnalysis(0xAABBCCDD);
        wxAIActionMessageForAnalysis message{0x1C};
        const auto trace = world.trace;
        action.vfunc_0C(&message); action.vfunc_0C(nullptr); action.vfunc_34_ClearForAnalysis();
        Check(child.notifications == 0 && action.GetCurrentActionForAnalysis() == &child &&
            action.GetClearableWordForAnalysis() == 0xAABBCCDD && world.trace == trace, "Idle Notify and Clear are actual no-ops");
        // Keep the borrowed child alive across explicit base destination cleanup.
        action.SetCurrentActionForAnalysis(nullptr);
    }
    void CopyAndFactory()
    {
        World world;
        wxIdleAIAction source(world), destination(world);
        source.SetOwnerForAnalysis(&world.first); destination.SetOwnerForAnalysis(&world.second);
        source.SetDurationMillisecondsForAnalysis(7); destination.SetDurationMillisecondsForAnalysis(9);
        source.SetClearableWordForAnalysis(11); destination.SetClearableWordForAnalysis(13);
        SpyAction current, nested;
        current.SetCurrentActionForAnalysis(&nested); current.SetClearableWordForAnalysis(17);
        destination.SetCurrentActionForAnalysis(&current);
        struct Owned final : spBaseObject { bool& destroyed; explicit Owned(bool& flag) : destroyed(flag) {} ~Owned() override { destroyed = true; } };
        bool destroyed = false;
        destination.AddOwnedActionForAnalysis(std::make_unique<Owned>(destroyed));
        spCloneManager manager;
        Check(source.vfunc_14(destination, manager) && destroyed && destination.GetCurrentActionForAnalysis() == nullptr &&
            current.GetClearableWordForAnalysis() == 0, "Idle Copy delegates real common destination cleanup");
        Check(destination.GetOwnerForAnalysis() == &world.second && destination.GetDurationMillisecondsForAnalysis() == 9 &&
            destination.GetClearableWordForAnalysis() == 13, "Idle Copy transfers no runtime payload");
        auto copy = manager.Clone(source); auto* typed = dynamic_cast<wxIdleAIAction*>(copy.get());
        Check(typed && typed->GetOwnerForAnalysis() == nullptr && typed->GetDurationMillisecondsForAnalysis() == 2000 &&
            typed->GetClearableWordForAnalysis() == 0 && typed->IsKindOf(wxAIAction::ClassID), "Idle clone constructs fresh base defaults");
        Check(!source.vfunc_14(current, manager), "portable Copy receiver type guard");
        wxIdleAIAction::SetFactoryHostForAnalysis(&world);
        auto made = spRTTIManager::Instance().Create(wxIdleAIAction::ClassID);
        Check(dynamic_cast<wxIdleAIAction*>(made.get()) != nullptr, "Idle factory and RTTI");
        wxIdleAIAction::SetFactoryHostForAnalysis(nullptr);
        bool rejected = false;
        try { (void)spRTTIManager::Instance().Create(wxIdleAIAction::ClassID); }
        catch (const std::logic_error&) { rejected = true; }
        Check(rejected, "missing analytical factory host explicitly rejected");
        current.SetCurrentActionForAnalysis(nullptr);
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--replay") Replay();
        else { Operations(); CopyAndFactory(); std::cout << "wxIdleAIAction checks passed\n"; }
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
