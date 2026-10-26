#include "Code/wxMinotaurDefendAIAction.h"
#include "Analysis/PC/wxMinotaurDefendAIActionAbi.h"
#include "Analysis/PS2/wxMinotaurDefendAIActionAbi.h"
#include <array>
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
    using Platform = wxMinotaurDefendPlatformForAnalysis;
    using Gate = wxMinotaurDefendGateResultForAnalysis;
    void Check(bool value, const char* text) { if (!value) throw std::runtime_error(text); }
    struct World final : wxMinotaurDefendAIActionHost, wxPerceptionHost
    {
        int owner = 1, target = 2;
        std::array<std::uint8_t, 0x74> command{}, otherCommand{};
        void* nextCommand = command.data();
        spTaskTimer timer;
        spFunctionEval::RandomStateForAnalysis random;
        wxPerception perception;
        std::map<const void*, std::array<float, 3>> positions;
        std::vector<std::uint32_t> times{1000};
        std::size_t timeReads = 0;
        std::vector<std::tuple<void*, std::uint32_t, std::uint32_t>> selections;
        std::vector<std::uint32_t> refusals;
        mutable std::string trace;
        World()
        {
            command.fill(0xA5); otherCommand.fill(0x5A);
            random.Seed(5489); (void)random.Next(); // same returned cold state as original RNG proof
            positions[&owner] = {0, 0, 0}; positions[&target] = {3, 4, 0};
            perception.BindForAnalysis(*this, &owner, {&target});
            perception.StateForAnalysis().scratchWords30 = std::array<std::uint32_t, 3>{0xA5A5A5A5, 0xA5A5A5A5, 0xA5A5A5A5};
        }
        ~World() override { perception.ResetForAnalysis(); }
        void Targets(bool present)
        { perception.ResetForAnalysis(); perception.BindForAnalysis(*this, &owner, present ? std::vector<void*>{&target} : std::vector<void*>{}); }
        wxPerception& ReadPerceptionForAnalysis(void*) noexcept override { trace += "query;"; return perception; }
        spTaskTimer& GameTimerForAnalysis() noexcept override
        {
            const auto now = times[timeReads < times.size() ? timeReads : times.size() - 1]; ++timeReads;
            auto state = timer.GetStateForAnalysis(); state.currentMilliseconds = now; timer.SetStateForAnalysis(state);
            trace += "time;"; return timer;
        }
        spFunctionEval::RandomStateForAnalysis& RandomStateForAnalysis() noexcept override { trace += "rng;"; return random; }
        void* ReadCommandForAnalysis(void*) noexcept override { trace += "command;"; return nextCommand; }
        void WriteCommandByteForAnalysis(void* object, std::uint32_t offset, std::uint8_t value) noexcept override
        {
            if (offset >= command.size() || (object != command.data() && object != otherCommand.data())) std::terminate();
            static_cast<std::uint8_t*>(object)[offset] = value;
            trace += "write:" + std::to_string(offset) + ";";
        }
        void SelectActionForAnalysis(void* object, std::uint32_t key, std::uint32_t parameter) noexcept override
        { trace += "select:" + std::to_string(key) + ";"; selections.emplace_back(object, key, parameter); }
        void ObserveUnqualifiedStateForAnalysis(const wxMinotaurDefendAIAction&, std::uint32_t state) noexcept override
        { trace += "refuse;"; refusals.push_back(state); }
        std::array<float, 3> ReadNodePositionForAnalysis(const void* object) const override
        { trace += "position;"; return positions.at(object); }
        void DetachOwnerForAnalysis(wxPerception&, void*) noexcept override {}
    };
    struct Child final : wxAIAction
    {
        unsigned messages = 0;
        void vfunc_0C(const void*) noexcept override { ++messages; }
    };
    Platform SelectPlatform(unsigned value) { return value ? Platform::PS2 : Platform::PC; }
    void Replay()
    {
        std::array<std::uint32_t, 624> initialRandom{};
        std::string op;
        while (std::cin >> op)
        {
            if (op == "r")
            {
                for (auto& word : initialRandom) std::cin >> word;
                Check(bool(std::cin), "complete RNG state"); std::cout << "state624\n"; continue;
            }
            World world;
            if (op == "e")
            {
                unsigned ps2, target; std::uint32_t now, index;
                std::cin >> ps2 >> target >> now >> index;
                world.Targets(target != 0); world.times = {now};
                world.random.state = initialRandom; world.random.index = index;
                wxMinotaurDefendAIAction action(world, SelectPlatform(ps2)); action.SetOwnerForAnalysis(&world.owner);
                action.vfunc_30();
                std::cout << unsigned(action.TargetForAnalysis() != nullptr) << ' ' << action.StateForAnalysis() << ' '
                    << action.DeadlineForAnalysis() << ' ' << unsigned(action.CachedCommandForAnalysis() == world.command.data()) << ' '
                    << world.random.index << ' ' << world.selections.size() << ' '
                    << (world.selections.empty() ? 0 : std::get<1>(world.selections.front())) << '\n';
            }
            else if (op == "n")
            {
                unsigned ps2, hasChild; std::uint32_t code, deadline, now, second;
                std::cin >> ps2 >> code >> deadline >> now >> second >> hasChild;
                world.times = {now, second};
                wxMinotaurDefendAIAction action(world, SelectPlatform(ps2)); Child child;
                action.SetFieldsForAnalysis(&world.target, 7, deadline, world.command.data());
                action.SetCurrentActionForAnalysis(hasChild ? &child : nullptr);
                wxAIActionMessageForAnalysis message{code}; action.vfunc_0C(&message);
                std::cout << action.DeadlineForAnalysis() << ' ' << world.timeReads << ' ' << child.messages << '\n';
                action.SetCurrentActionForAnalysis(nullptr);
            }
            else if (op == "g")
            {
                unsigned ps2; std::uint32_t now, deadline, state;
                std::cin >> ps2 >> now >> deadline >> state; world.times = {now};
                wxMinotaurDefendAIAction action(world, SelectPlatform(ps2)); action.SetOwnerForAnalysis(&world.owner);
                action.SetFieldsForAnalysis(&world.target, state, deadline, world.command.data());
                const auto gate = action.UpdateGateForAnalysis();
                std::cout << unsigned(gate) << ' ' << action.StateForAnalysis() << ' ' << world.selections.size() << ' '
                    << (world.selections.empty() ? 0 : std::get<1>(world.selections.front())) << '\n';
            }
            else if (op == "x")
            {
                std::uint32_t state, deadline; std::cin >> state >> deadline;
                wxMinotaurDefendAIAction action(world); action.SetFieldsForAnalysis(&world.target, state, deadline, world.command.data());
                world.nextCommand = world.otherCommand.data(); action.vfunc_34_ClearForAnalysis();
                std::cout << unsigned(action.TargetForAnalysis() != nullptr) << ' ' << action.StateForAnalysis() << ' '
                    << action.DeadlineForAnalysis() << ' ' << unsigned(world.command[0x1D]) << ' '
                    << unsigned(world.command[0x1F]) << '\n';
            }
            else if (op == "c")
            {
                std::uint32_t target, state, deadline, command; std::cin >> target >> state >> deadline >> command;
                wxMinotaurDefendAIAction source(world), destination(world);
                source.SetFieldsForAnalysis(reinterpret_cast<void*>(std::uintptr_t(target)), state, deadline,
                    reinterpret_cast<void*>(std::uintptr_t(command)));
                spCloneManager manager; Check(source.vfunc_14(destination, manager), "replay Copy");
                std::cout << reinterpret_cast<std::uintptr_t>(destination.TargetForAnalysis()) << ' ' << destination.StateForAnalysis() << ' '
                    << destination.DeadlineForAnalysis() << ' ' << reinterpret_cast<std::uintptr_t>(destination.CachedCommandForAnalysis()) << '\n';
            }
            else throw std::invalid_argument("MinotaurDefend replay operation");
            Check(bool(std::cin), "complete MinotaurDefend replay input");
        }
    }
    void TestEntryAndExit()
    {
        World world;
        wxMinotaurDefendAIAction action(world);
        Check(action.TargetForAnalysis() == nullptr && action.StateForAnalysis() == 1 && action.DeadlineForAnalysis() == 0 &&
            action.CachedCommandForAnalysis() == nullptr, "native constructor defaults");
        action.SetOwnerForAnalysis(&world.owner); action.vfunc_30();
        Check(world.trace == "query;position;position;time;rng;command;", "Perception, timer capture, actual RNG Next, cached command order");
        Check(action.TargetForAnalysis() == &world.target && action.StateForAnalysis() == 0 && world.random.index == 2 &&
            action.DeadlineForAnalysis() == 4000u + 581869302u % 1501u, "native entry deadline and one consumed generator word");
        world.nextCommand = world.otherCommand.data(); world.trace.clear(); action.vfunc_34_ClearForAnalysis();
        Check(world.trace == "write:29;write:31;" && !action.TargetForAnalysis() &&
            world.command[0x1D] == 0 && world.command[0x1F] == 0 && world.otherCommand[0x1D] == 0x5A &&
            action.CachedCommandForAnalysis() == world.command.data(), "exit uses cached command and preserves cached pointer");
        world.Targets(false); world.trace.clear(); world.timeReads = 0; action.vfunc_30();
        Check(world.trace == "query;position;time;rng;command;select:1;" && world.random.index == 3 &&
            std::get<1>(world.selections.back()) == 1, "missing target still consumes RNG and caches command before selector");
        action.SetFieldsForAnalysis(&world.target, 5, 99, world.command.data()); world.trace.clear(); action.vfunc_38();
        Check(world.trace == "query;position;select:1;" && action.TargetForAnalysis() == &world.target,
            "query requests selector1 without overwriting the cached target");
    }
    void TestNotificationsAndGate()
    {
        for (auto platform : {Platform::PC, Platform::PS2})
        {
            World world; wxMinotaurDefendAIAction action(world, platform); Child child;
            action.SetOwnerForAnalysis(&world.owner); action.SetCurrentActionForAnalysis(&child);
            action.SetFieldsForAnalysis(&world.target, 7, 3999, world.command.data()); world.times = {1000, 2000};
            wxAIActionMessageForAnalysis message{0x2717}; action.vfunc_0C(&message);
            Check(action.DeadlineForAnalysis() == (platform == Platform::PC ? 4000u : 5000u) &&
                world.timeReads == (platform == Platform::PC ? 1u : 2u), "PC captured now versus PS2 repeated timer load");
            Check(child.messages == 0 && action.StateForAnalysis() == 7 && action.TargetForAnalysis() == &world.target,
                "2717 consumes notification and preserves other fields");
            world.timeReads = 0; action.SetFieldsForAnalysis(&world.target, 7, 4000, world.command.data());
            action.vfunc_0C(&message); Check(action.DeadlineForAnalysis() == 4000 && world.timeReads == 1, "remaining equality3000 does not extend");
            world.timeReads = 0; action.SetFieldsForAnalysis(&world.target, 7, 0x800003E8, world.command.data());
            action.vfunc_0C(&message); Check(action.DeadlineForAnalysis() == (platform == Platform::PC ? 4000u : 5000u), "signed-wrap remaining interval preserved");
            message.code = 0x27A4; const auto reads = world.timeReads; action.vfunc_0C(&message);
            Check(child.messages == 0 && world.timeReads == reads, "27A4 consumed without base forwarding");
            message.code = 0x1C; action.vfunc_0C(&message); Check(child.messages == 1, "other message follows actual common Notify forwarding");
            action.SetCurrentActionForAnalysis(nullptr);
            world.times = {100}; world.timeReads = 0; world.selections.clear();
            action.SetFieldsForAnalysis(&world.target, 9, 99, world.command.data());
            Check(action.UpdateGateForAnalysis() == Gate::ExpiredSelector && std::get<1>(world.selections.back()) == 2 &&
                action.StateForAnalysis() == 9, "unsigned expired deadline requests selector2 before state branch");
            world.selections.clear(); action.SetFieldsForAnalysis(&world.target, 0xFFFFFFFF, 100, world.command.data());
            Check(action.UpdateGateForAnalysis() == Gate::StateReset && action.StateForAnalysis() == 1 && world.selections.empty(),
                "unexpected state resets to1 at deadline equality");
            action.SetFieldsForAnalysis(&world.target, 0, 100, world.command.data());
            Check(action.UpdateGateForAnalysis() == Gate::UnqualifiedState0 && !action.vfunc_24() &&
                action.LastGateResultForAnalysis() == Gate::UnqualifiedState0 && world.refusals == std::vector<std::uint32_t>{0},
                "state0 body is explicitly unqualified; false is an adapter refusal with diagnostic");
            action.SetFieldsForAnalysis(&world.target, 1, 100, world.command.data());
            Check(action.UpdateGateForAnalysis() == Gate::UnqualifiedState1 && !action.vfunc_24() && world.refusals.back() == 1 &&
                world.command[0x1F] == 0xA5, "state1 body never becomes a successful host stub");
        }
    }
    void TestCopyAndFactory()
    {
        World world; wxMinotaurDefendAIAction source(world), destination(world);
        source.SetFieldsForAnalysis(&world.target, 3, 0xAABBCCDD, world.command.data()); source.SetOwnerForAnalysis(&world.owner);
        destination.SetDurationMillisecondsForAnalysis(17); destination.SetOwnerForAnalysis(&world.target);
        spCloneManager manager;
        Check(source.vfunc_14(destination, manager) && destination.TargetForAnalysis() == &world.target && destination.StateForAnalysis() == 3 &&
            destination.DeadlineForAnalysis() == 0xAABBCCDD && destination.CachedCommandForAnalysis() == world.command.data() &&
            destination.GetOwnerForAnalysis() == &world.target && destination.GetDurationMillisecondsForAnalysis() == 17,
            "Copy transfers exactly four own words and retains base runtime payload");
        auto cloned = manager.Clone(source); auto* typed = dynamic_cast<wxMinotaurDefendAIAction*>(cloned.get());
        Check(typed && typed->TargetForAnalysis() == &world.target && typed->CachedCommandForAnalysis() == world.command.data() &&
            typed->GetOwnerForAnalysis() == nullptr, "clone retains borrowed own refs without copying base owner");
        cloned.reset(); Check(world.command[0x1D] == 0xA5, "destruction does not run Exit or release borrowed command");
        wxAIAction wrong; Check(!source.vfunc_14(wrong, manager), "portable type guard");
        wxMinotaurDefendAIAction::SetFactoryHostForAnalysis(&world);
        auto factory = spRTTIManager::Instance().Create(wxMinotaurDefendAIAction::ClassID);
        Check(dynamic_cast<wxMinotaurDefendAIAction*>(factory.get()) && factory->IsKindOf(wxAIAction::ClassID), "factory/RTTI base identity");
        wxMinotaurDefendAIAction::SetFactoryHostForAnalysis(nullptr);
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--replay") Replay();
        else { TestEntryAndExit(); TestNotificationsAndGate(); TestCopyAndFactory(); std::cout << "wxMinotaurDefendAIAction checks passed\n"; }
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
