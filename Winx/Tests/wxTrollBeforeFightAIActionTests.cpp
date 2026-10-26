#include "Code/wxTrollBeforeFightAIAction.h"
#include "Code/wxGameTimer.h"
#include "Analysis/PC/wxTrollBeforeFightAIActionAbi.h"
#include "Analysis/PS2/wxTrollBeforeFightAIActionAbi.h"
#include <array>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    using Profile = wxTrollBeforeFightProfileForAnalysis;
    using Result = wxTrollBeforeFightTickResultForAnalysis;
    int checks = 0;
    void Check(bool value, const char* text) { ++checks; if (!value) throw std::runtime_error(text); }
    struct World final : wxTrollBeforeFightAIActionHost
    {
        spTimer clock;
        wxGameTimer timer{clock};
        spFunctionEval::RandomStateForAnalysis random;
        int owner = 1;
        std::array<std::uint8_t, 0x74> command;
        std::vector<std::uint32_t> times{0};
        std::uint32_t observedState = 1;
        unsigned timeReads = 0, rngReads = 0, resets = 0, stateReads = 0;
        std::string trace;
        World() { command.fill(0xA5); random.index = 0; random.state.fill(0); }
        void* ReadCommandForAnalysis(void* object) noexcept override
        { if (object != &owner) std::terminate(); trace += "command;"; return command.data(); }
        void ResetCommandForAnalysis(void* object) noexcept override
        {
            if (object != command.data()) std::terminate(); ++resets; trace += "reset;";
            for (unsigned offset : {4U,8U,12U,16U,20U,0x6CU})
                for (unsigned index = 0; index < 4; ++index) command[offset + index] = 0;
            for (unsigned index = 0x19; index <= 0x31; ++index) command[index] = 0;
            for (unsigned index = 0x5C; index <= 0x60; ++index) command[index] = 0;
            command[0x64] = command[0x70] = 0;
        }
        void WriteCommandByteForAnalysis(void* object, std::uint32_t offset, std::uint8_t value) noexcept override
        {
            if (object != command.data() || offset >= command.size()) std::terminate();
            command[offset] = value; trace += "write;";
        }
        std::uint32_t ReadCharacterStateWordForAnalysis(void* object) noexcept override
        { if (object != &owner) std::terminate(); ++stateReads; trace += "state;"; return observedState; }
        spTaskTimer& GameTimerPrefixForAnalysis() noexcept override
        {
            auto state = timer.GetStateForAnalysis(); state.currentMilliseconds = times[timeReads < times.size() ? timeReads : times.size() - 1];
            timer.SetStateForAnalysis(state); ++timeReads; trace += "time;"; return timer;
        }
        spFunctionEval::RandomStateForAnalysis& RandomStateForAnalysis() noexcept override
        { ++rngReads; trace += "rng;"; return random; }
        std::uint32_t CommandWord04() const
        { std::uint32_t value; std::memcpy(&value, command.data() + 4, sizeof(value)); return value; }
    };
    struct Child final : wxAIAction
    {
        unsigned messages = 0;
        void vfunc_0C(const void*) noexcept override { ++messages; }
    };
    struct Owned final : spBaseObject
    {
        unsigned& deaths;
        explicit Owned(unsigned& value) : deaths(value) {}
        ~Owned() override { ++deaths; }
    };
    void UnitTests()
    {
        World world; Child child; wxTrollBeforeFightAIAction action(world); action.SetOwnerForAnalysis(&world.owner);
        Check(action.IsExactly(0x03197B06) && action.IsKindOf(wxAIAction::ClassID), "Registered Troll identity/base");
        Check(action.PhaseForAnalysis() == 0 && action.LatchForAnalysis() == 0 && action.NextDecisionForAnalysis() == 0
            && action.CooldownForAnalysis() == 0, "Four fresh zero words");
        world.times = {0xFFFFFFF0}; Check(action.vfunc_24() && action.NextDecisionForAnalysis() == 984
            && world.random.index == 1 && world.resets == 1 && world.trace == "command;reset;time;rng;", "Stage0 exact reset/time/RNG order and wrapped sum");
        Check(world.command[0x18] == 0xA5 && world.command[0x32] == 0xA5 && world.command[0x54] == 0xA5
            && world.CommandWord04() == 0 && world.command[0x19] == 0 && world.command[0x70] == 0,
            "Foreign Reset precise fields, not whole-record memset");
        world.trace.clear(); world.timeReads = 0; world.times = {984};
        Check(action.vfunc_24() && world.trace == "time;" && world.random.index == 1, "Deadline equality waits without RNG");
        action.SetStateForAnalysis(0xFFFFFFFF, 9, 1, 5); world.times = {10, 1}; world.timeReads = 0; world.random.index = 0;
        world.trace.clear(); Check(action.TickForAnalysis() == Result::Completed && action.PhaseForAnalysis() == 1
            && action.LatchForAnalysis() == 0 && action.NextDecisionForAnalysis() == 0 && action.CooldownForAnalysis() == 5
            && world.timeReads == 2 && world.trace == "time;rng;time;", "Unknown phase uses stage0 and cooldown compares second clock sample");
        action.SetStateForAnalysis(0, 9, 1, 5); world.times = {2, 10}; world.timeReads = 0; world.random.index = 0;
        Check(action.TickForAnalysis() == Result::Completed && action.PhaseForAnalysis() == 2, "Strict cooldown below second sample chooses phase2");
        action.SetStateForAnalysis(0, 9, 1, 0); world.times = {2}; world.timeReads = 0; world.random.index = 0;
        world.random.state[0] = 0x80000000; Check(action.vfunc_24() && action.PhaseForAnalysis() == 1 && world.timeReads == 1,
            "High actual shared RNG output avoids second clock read");
        for (auto phase : {1U,2U,3U})
        {
            const auto offset = phase + 0x21;
            action.SetStateForAnalysis(phase, 0, 77, 88); world.timeReads = 0; world.times = {0xFFFFFFF0};
            Check(action.vfunc_24() && world.command[offset] == 1 && action.LatchForAnalysis() == 0
                && world.timeReads == unsigned(phase == 2), "Command stage latch0 initializes without advancing latch");
            if (phase == 2)
            {
                Check(action.CooldownForAnalysis() == 11984, "Phase2 cooldown wraps");
                world.times = {100}; world.timeReads = 0; Check(action.vfunc_24() && action.CooldownForAnalysis() == 12100,
                    "Phase2 latch0 rewrites cooldown each tick");
            }
            world.observedState = phase + 4; Check(action.vfunc_28() && action.LatchForAnalysis() == 1, "Query recognizes state5/6/7");
            Check(action.vfunc_24() && world.command[offset] == 0, "Latch1 clears own command byte");
            world.observedState = 0; Check(action.vfunc_28() && action.LatchForAnalysis() == 2, "Query waits for character state zero");
            Check(action.vfunc_24() && action.PhaseForAnalysis() == 0 && action.LatchForAnalysis() == 0
                && action.NextDecisionForAnalysis() == 77, "Completed command clears only phase/latch");
        }
        action.SetStateForAnalysis(0xFFFFFFFF, 2, 7, 8); world.stateReads = 0;
        Check(action.vfunc_28() && world.stateReads == 1 && action.LatchForAnalysis() == 2, "Query reads character state before unknown-phase no-op");
        action.SetCurrentActionForAnalysis(&child); wxAIActionMessageForAnalysis message{0x1C}; action.vfunc_0C(&message);
        Check(child.messages == 0 && action.PhaseForAnalysis() == 0xFFFFFFFF, "Own Notify is a no-op even for1C and nonnull child");
        action.SetClearableWordForAnalysis(123); world.command.fill(0xA5); action.vfunc_34_ClearForAnalysis();
        Check(action.PhaseForAnalysis() == 0xFFFFFFFF && action.GetClearableWordForAnalysis() == 123, "Own Exit preserves action/base words");
        for (unsigned index = 0; index < world.command.size(); ++index)
        {
            const bool cleared = index == 0x1D || (index >= 0x20 && index <= 0x24);
            Check(world.command[index] == (cleared ? 0 : 0xA5), "Exact Exit command guard");
        }
        World guardedWorld; guardedWorld.times = {2};
        wxTrollBeforeFightAIAction guarded(guardedWorld, Profile::PS2IntegerBoundaries); guarded.SetOwnerForAnalysis(&guardedWorld.owner);
        guarded.SetStateForAnalysis(0, 9, 1, 5);
        Check(guarded.TickForAnalysis() == Result::UnqualifiedPs2RandomPredicate && guarded.PhaseForAnalysis() == 0
            && guarded.LatchForAnalysis() == 9 && guarded.NextDecisionForAnalysis() == 1 && guardedWorld.random.index == 1
            && guardedWorld.timeReads == 1, "PS2 explicit guard after actual shared RNG consumption, before phase mutation");
        unsigned deaths = 0; wxTrollBeforeFightAIAction destination(world); destination.SetOwnerForAnalysis(&world.owner);
        destination.AddOwnedActionForAnalysis(std::make_unique<Owned>(deaths)); destination.SetCurrentActionForAnalysis(&child);
        action.SetStateForAnalysis(3, 2, 0x80000000, 0xFFFFFFFF); spCloneManager manager;
        Check(action.vfunc_14(destination, manager) && deaths == 1 && destination.GetCurrentActionForAnalysis() == nullptr
            && destination.GetOwnerForAnalysis() == &world.owner && destination.PhaseForAnalysis() == 3
            && destination.LatchForAnalysis() == 2 && destination.NextDecisionForAnalysis() == 0x80000000
            && destination.CooldownForAnalysis() == 0xFFFFFFFF, "Real base cleanup then four-word Copy");
        auto cloneBase = action.Clone(); auto* clone = dynamic_cast<wxTrollBeforeFightAIAction*>(cloneBase.get());
        Check(clone && clone->PhaseForAnalysis() == 3 && clone->LatchForAnalysis() == 2 && clone->GetOwnerForAnalysis() == nullptr,
            "Clone preserves own words in fresh base");
        world.trace.clear(); action.vfunc_30(); Check(action.PhaseForAnalysis() == 0 && action.LatchForAnalysis() == 0
            && action.NextDecisionForAnalysis() == 0 && action.CooldownForAnalysis() == 0 && world.trace.empty(), "Entry only clears own words");
        wxTrollBeforeFightAIAction::SetFactoryHostForAnalysis(&world);
        auto made = spRTTIManager::Instance().Create(wxTrollBeforeFightAIAction::ClassID);
        Check(made && made->IsExactly(wxTrollBeforeFightAIAction::ClassID), "Actual registered factory contract");
        made.reset(); wxTrollBeforeFightAIAction::SetFactoryHostForAnalysis(nullptr);
    }
    void Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            if (line.empty()) continue; std::istringstream in(line); char tag; in >> tag;
            World world; wxTrollBeforeFightAIAction action(world); action.SetOwnerForAnalysis(&world.owner);
            if (tag == 't')
            {
                unsigned ps2, op; std::uint32_t phase,latch,deadline,cooldown,now,second,state,inputWord;
                in >> ps2 >> op >> phase >> latch >> deadline >> cooldown >> now >> second >> state >> inputWord;
                Check(bool(in), "Complete Troll replay input");
                wxTrollBeforeFightAIAction selected(world, ps2 ? Profile::PS2IntegerBoundaries : Profile::PC);
                selected.SetOwnerForAnalysis(&world.owner); selected.SetStateForAnalysis(phase,latch,deadline,cooldown);
                world.times = {now,second}; world.observedState = state; world.random.state[0] = inputWord;
                Result result = Result::Completed;
                if (op == 0) result = selected.TickForAnalysis();
                else if (op == 1) (void)selected.vfunc_28();
                else if (op == 2) selected.vfunc_30();
                else if (op == 3) selected.vfunc_34_ClearForAnalysis();
                else if (op == 4) { wxAIActionMessageForAnalysis message{0x27B2}; selected.vfunc_0C(&message); }
                else throw std::runtime_error("Unknown Troll operation");
                std::cout << selected.PhaseForAnalysis() << ' ' << selected.LatchForAnalysis() << ' '
                    << selected.NextDecisionForAnalysis() << ' ' << selected.CooldownForAnalysis() << ' ' << world.CommandWord04() << ' '
                    << unsigned(world.command[0x1D]);
                for (unsigned offset = 0x20; offset <= 0x24; ++offset) std::cout << ' ' << unsigned(world.command[offset]);
                std::cout << ' ' << world.timeReads << ' ' << world.random.index << ' ' << world.resets << ' ' << world.stateReads << ' ' << unsigned(result) << '\n';
            }
            else if (tag == 'c')
            {
                std::uint32_t phase,latch,deadline,cooldown; in >> phase >> latch >> deadline >> cooldown; Check(bool(in), "Complete Copy replay");
                action.SetStateForAnalysis(phase,latch,deadline,cooldown); auto clone = action.Clone();
                auto* typed = dynamic_cast<wxTrollBeforeFightAIAction*>(clone.get()); Check(typed != nullptr, "Copy replay type");
                std::cout << typed->PhaseForAnalysis() << ' ' << typed->LatchForAnalysis() << ' ' << typed->NextDecisionForAnalysis() << ' ' << typed->CooldownForAnalysis() << '\n';
            }
            else throw std::runtime_error("Unknown Troll replay tag");
        }
    }
}
int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--replay") { Replay(); return 0; }
        UnitTests(); std::cout << "PASS " << checks << " TrollBeforeFight checks\n"; return 0;
    }
    catch (const std::exception& error) { std::cerr << "FAIL " << error.what() << '\n'; return 1; }
}
