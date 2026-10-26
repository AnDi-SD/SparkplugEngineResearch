#include "Code/wxIceGargoyleSleepingAIAction.h"
#include "Code/wxGameTimer.h"
#include "Analysis/PC/wxIceGargoyleSleepingAIActionAbi.h"
#include "Analysis/PS2/wxIceGargoyleSleepingAIActionAbi.h"
#include "wxSleepingTargetCompositionForAnalysis.h"
#include <array>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    using Result = wxSleepingTickResultForAnalysis;
    int checks = 0;
    void Check(bool value, const char* text) { ++checks; if (!value) throw std::runtime_error(text); }
    struct World final : wxIceGargoyleSleepingAIActionHost
    {
        spTimer clock;
        wxGameTimer timer{clock};
        int owner = 1;
        std::array<std::uint8_t, 0x80> command;
        std::uint32_t characterState = 1;
        bool warm = true;
        unsigned timeReads = 0, targetReads = 0, stateReads = 0;
        std::string trace;
        std::vector<std::tuple<void*, std::uint32_t, std::uint32_t>> selections;
        World() { command.fill(0xA5); }
        void Now(std::uint32_t value)
        { auto state = timer.GetStateForAnalysis(); state.currentMilliseconds = value; timer.SetStateForAnalysis(state); }
        void* ReadCommandForAnalysis(void* object) noexcept override
        { if (object != &owner) std::terminate(); trace += "command;"; return command.data(); }
        void WriteCommandByteForAnalysis(void* object, std::uint32_t offset, std::uint8_t value) noexcept override
        {
            if (object != command.data() || offset != 0x1B) std::terminate();
            command[offset] = value; trace += "write;";
        }
        std::uint32_t ReadCharacterStateWordForAnalysis(void* object) noexcept override
        { if (object != &owner) std::terminate(); ++stateReads; trace += "state;"; return characterState; }
        spTaskTimer& GameTimerPrefixForAnalysis() noexcept override
        { ++timeReads; trace += "time;"; return timer; }
        bool HasExistingTargetManagerForAnalysis() noexcept override
        { ++targetReads; trace += "target;"; return warm; }
        void SelectActionForAnalysis(void* object, std::uint32_t key, std::uint32_t parameter) noexcept override
        { trace += "select;"; selections.emplace_back(object, key, parameter); }
    };
    struct Child final : wxAIAction
    {
        unsigned notifications = 0;
        void vfunc_0C(const void*) noexcept override { ++notifications; }
    };
    struct Owned final : spBaseObject
    {
        unsigned& deaths;
        explicit Owned(unsigned& value) : deaths(value) {}
        ~Owned() override { ++deaths; }
    };
    void UnitTests()
    {
        World world; wxIceGargoyleSleepingAIAction action(world); action.SetOwnerForAnalysis(&world.owner);
        Check(action.IsExactly(0x3C7B1A31) && action.IsKindOf(wxAIAction::ClassID), "Registered identity and real AI base");
        Check(action.PhaseForAnalysis() == 0 && action.StampForAnalysis() == 0, "Fresh own scalar words");
        world.Now(0); Check(action.vfunc_24() && action.StampForAnalysis() == 0 && world.command[0x1B] == 1,
            "Stage0 stores zero verbatim");
        Check(world.trace == "command;write;time;", "Stage0 write precedes actual timer read");
        world.Now(17); Check(action.vfunc_24() && action.StampForAnalysis() == 17 && world.timeReads == 2,
            "Zero stamp repeats stage0 initialization");
        world.trace.clear(); world.Now(999); Check(action.vfunc_24() && action.StampForAnalysis() == 17 && world.trace.empty(),
            "Nonzero phase0 latch avoids all external reads");
        action.WakeForAnalysis(); Check(action.PhaseForAnalysis() == 1 && action.StampForAnalysis() == 0, "Wake begins stage1");
        world.Now(0xFFFFFF9C); world.trace.clear();
        Check(action.TickForAnalysis() == Result::Completed && action.StampForAnalysis() == 0 && world.command[0x1B] == 0,
            "Stage1 plus100 wraps exactly back to zero");
        Check(world.trace == "command;write;time;target;", "Stage1 prefix before target presence");
        world.Now(0xFFFFFF9D); Check(action.TickForAnalysis() == Result::Completed && action.StampForAnalysis() == 1,
            "Wrapped zero repeats initialization on next tick");
        const auto stamp = action.StampForAnalysis(); action.WakeForAnalysis();
        Check(action.StampForAnalysis() == stamp, "Repeated wake preserves phase1 stamp");
        world.trace.clear(); world.Now(0xFFFFFFFF); world.characterState = 1;
        Check(action.TickForAnalysis() == Result::Completed && world.trace == "state;", "Nonzero latch has no deadline comparison");
        world.characterState = 0; world.trace.clear();
        Check(action.TickForAnalysis() == Result::SelectorBoundary && world.trace == "state;select;"
            && world.selections.back() == std::make_tuple(static_cast<void*>(&world.owner), 0U, 0U), "Real own decision emits foreign selector0,0 boundary");
        action.SetStateForAnalysis(1, 0); world.warm = false; world.Now(50); world.trace.clear();
        Check(action.TickForAnalysis() == Result::UnqualifiedTargetManagerAcquisition && action.StampForAnalysis() == 150
            && world.trace == "command;write;time;target;", "Cold target guard preserves owned prefix and remains unqualified");
        action.SetStateForAnalysis(1, 0); Check(!action.vfunc_24(), "Boolean false is the analytical cold guard");
        for (auto phase : {2U, 0x80000000U, 0xFFFFFFFFU})
        {
            action.SetStateForAnalysis(phase, 0); world.trace.clear();
            Check(action.TickForAnalysis() == Result::Completed && world.trace.empty() && action.PhaseForAnalysis() == phase,
                "Unknown phase returns without coercion or external reads");
        }
        Child child; action.SetCurrentActionForAnalysis(&child); action.SetStateForAnalysis(0, 99);
        wxAIActionMessageForAnalysis message{0x27B2}; action.vfunc_0C(&message);
        Check(child.notifications == 0 && action.PhaseForAnalysis() == 1 && action.StampForAnalysis() == 0,
            "Wake event consumes message and bypasses child");
        action.SetStateForAnalysis(1, 777); action.vfunc_0C(&message);
        Check(action.StampForAnalysis() == 777 && child.notifications == 0, "Repeated wake event preserves latch");
        message.code = 0x1C; world.command[0x1B] = 0xA5; world.trace.clear(); action.vfunc_0C(&message);
        Check(child.notifications == 1 && world.command[0x1B] == 0xA5 && world.trace.empty(), "Base1C uses query, forwards child, does not call own Exit");
        action.SetClearableWordForAnalysis(0xDEADBEEF); action.vfunc_34_ClearForAnalysis();
        Check(world.command[0x1B] == 0 && action.StampForAnalysis() == 777 && action.GetClearableWordForAnalysis() == 0xDEADBEEF,
            "Exit clears only command1B");
        world.trace.clear(); action.vfunc_30(); Check(action.PhaseForAnalysis() == 0 && action.StampForAnalysis() == 0
            && world.trace.empty(), "Entry clears only own words");
        unsigned deaths = 0;
        wxIceGargoyleSleepingAIAction destination(world); destination.SetOwnerForAnalysis(&world.owner);
        destination.SetCurrentActionForAnalysis(&child); destination.AddOwnedActionForAnalysis(std::make_unique<Owned>(deaths));
        destination.SetClearableWordForAnalysis(123); action.SetStateForAnalysis(0xFFFFFFFF, 0x80000000);
        spCloneManager manager; Check(action.vfunc_14(destination, manager) && deaths == 1
            && destination.GetCurrentActionForAnalysis() == nullptr && destination.GetOwnedActionCountForAnalysis() == 0
            && destination.GetOwnerForAnalysis() == &world.owner && destination.GetClearableWordForAnalysis() == 123
            && destination.PhaseForAnalysis() == 0xFFFFFFFF && destination.StampForAnalysis() == 0x80000000,
            "Copy uses real base destination cleanup then transfers only own words");
        auto cloneBase = action.Clone(); auto* clone = dynamic_cast<wxIceGargoyleSleepingAIAction*>(cloneBase.get());
        Check(clone && clone->PhaseForAnalysis() == action.PhaseForAnalysis() && clone->StampForAnalysis() == action.StampForAnalysis()
            && clone->GetOwnerForAnalysis() == nullptr && clone->GetCurrentActionForAnalysis() == nullptr, "Clone transfers two own words into fresh base");
        wxAIAction wrong; Check(!action.vfunc_14(wrong, manager), "Analytical typed destination guard");
        wxIceGargoyleSleepingAIAction::SetFactoryHostForAnalysis(&world);
        auto made = spRTTIManager::Instance().Create(wxIceGargoyleSleepingAIAction::ClassID);
        Check(made && made->IsExactly(wxIceGargoyleSleepingAIAction::ClassID), "Registered factory uses explicit host");
        made.reset(); wxIceGargoyleSleepingAIAction::SetFactoryHostForAnalysis(nullptr);
        for (std::size_t index = 0; index < world.command.size(); ++index)
            if (index != 0x1B) Check(world.command[index] == 0xA5, "All other command bytes preserved");
        action.SetCurrentActionForAnalysis(nullptr); // borrowed child must outlive the link
    }
    void Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            if (line.empty()) continue;
            std::istringstream in(line); char tag; in >> tag;
            World world; Child child; wxIceGargoyleSleepingAIAction action(world); action.SetOwnerForAnalysis(&world.owner);
            if (tag == 's')
            {
                unsigned op, warm, hasChild; std::uint32_t phase, stamp, now, state, code;
                in >> op >> phase >> stamp >> now >> state >> warm >> code >> hasChild;
                Check(bool(in), "Complete Sleeping replay");
                action.SetStateForAnalysis(phase, stamp); world.Now(now); world.characterState = state; world.warm = warm != 0;
                if (hasChild) action.SetCurrentActionForAnalysis(&child);
                Result result = Result::Completed;
                if (op == 0) result = action.TickForAnalysis();
                else if (op == 1) action.WakeForAnalysis();
                else if (op == 2) action.vfunc_30();
                else if (op == 3) action.vfunc_34_ClearForAnalysis();
                else if (op == 4) { wxAIActionMessageForAnalysis message{code}; action.vfunc_0C(&message); }
                else throw std::runtime_error("Unknown Sleeping operation");
                std::cout << action.PhaseForAnalysis() << ' ' << action.StampForAnalysis() << ' '
                    << unsigned(world.command[0x1B]) << ' ' << world.timeReads << ' ' << child.notifications << ' ' << unsigned(result) << '\n';
            }
            else if (tag == 'c')
            {
                std::uint32_t phase, stamp; in >> phase >> stamp; Check(bool(in), "Complete Copy replay");
                action.SetStateForAnalysis(phase, stamp); auto clone = action.Clone();
                auto* typed = dynamic_cast<wxIceGargoyleSleepingAIAction*>(clone.get()); Check(typed != nullptr, "Copy replay type");
                std::cout << typed->PhaseForAnalysis() << ' ' << typed->StampForAnalysis() << '\n';
            }
            else throw std::runtime_error("Unknown Sleeping replay tag");
        }
    }
}
int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--replay") { Replay(); return 0; }
        if (argc == 3 && std::string(argv[1]) == "--composition-replay") { sleeping_target_tests::Run(argv[2], true); return 0; }
        UnitTests(); checks += sleeping_target_tests::UnitTests(); std::cout << "PASS " << checks << " Sleeping AI checks\n"; return 0;
    }
    catch (const std::exception& error) { std::cerr << "FAIL " << error.what() << '\n'; return 1; }
}
