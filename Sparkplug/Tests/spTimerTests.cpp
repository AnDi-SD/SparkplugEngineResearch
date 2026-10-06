#include "Code/Sparkplug/spTimer.h"
#include "Analysis/PC/spTimerAbi.h"
#include <iostream>
#include <stdexcept>

using namespace sparkplug::reconstruction;
namespace
{
    unsigned checks = 0;
    void Check(bool value, const char* description)
    { ++checks; if (!value) throw std::runtime_error(description); }
    int Batch()
    {
        char operation;
        unsigned active, limited, mutations;
        std::uint32_t accumulated, startedAt, limit, first, second;
        unsigned cases = 0;
        std::cout << '[';
        while (std::cin >> operation >> active >> accumulated >> startedAt >> limited >> limit
                        >> first >> second >> mutations)
        {
            if (++cases > 1024 || active > 255 || limited > 255 || mutations > 3)
                throw std::runtime_error("bounded timer batch input");
            spTimer timer;
            timer.SetStateForAnalysis({static_cast<std::uint8_t>(active), accumulated, startedAt,
                                      static_cast<std::uint8_t>(limited), limit});
            unsigned reads = 0;
            auto clock = [&]() -> std::uint32_t {
                if (++reads > 2) throw std::runtime_error("extra timer sample");
                if (mutations)
                {
                    auto state = timer.GetStateForAnalysis();
                    state.active = 127;
                    state.limited = reads == 1 ? 0 : 254;
                    if (mutations & 1) { state.accumulated = 100 + reads; state.startedAt = 50 + reads; }
                    if (mutations & 2) state.limit = 20 + reads;
                    timer.SetStateForAnalysis(state);
                }
                return reads == 1 ? first : second;
            };
            bool result;
            if (operation == 'S') result = timer.StartForAnalysis(clock);
            else if (operation == 'T') result = timer.StopForAnalysis(clock);
            else if (operation == 'R') result = timer.ResetForAnalysis(clock);
            else throw std::runtime_error("unknown timer operation");
            Check(result, "batch operation");
            const auto& state = timer.GetStateForAnalysis();
            if (cases > 1) std::cout << ',';
            std::cout << '[' << unsigned(state.active) << ',' << state.accumulated << ','
                      << state.startedAt << ',' << unsigned(state.limited) << ',' << state.limit
                      << ',' << reads << ']';
        }
        std::cout << "]\n";
        return 0;
    }
    void UnitTests()
    {
        spTimer timer;
        Check(timer.IsExactly(spTimer::ClassID) && timer.IsKindOf(spBaseObject::ClassID), "RTTI");
        Check(spTimer::StaticRTTI().factory() != nullptr, "factory");
        unsigned reads = 0;
        std::uint32_t now = 10;
        auto clock = [&] { ++reads; return now; };
        spTimer cold(0, {}), active(255, clock);
        Check(reads == 1 && active.GetStateForAnalysis().startedAt == 10 &&
              active.GetStateForAnalysis().active == 1 && !cold.GetStateForAnalysis().active,
              "constructor starts on every nonzero byte");
        timer.SetStateForAnalysis({255, 0xFFFFFFFFu, 0xFFFFFFFFu, 254, 0xFFFFFFFFu});
        std::string error;
        Check(!timer.StartForAnalysis({}, &error) && !error.empty(), "missing provider guard");
        Check(!timer.StopForAnalysis({}) && !timer.ResetForAnalysis({}) &&
              timer.GetStateForAnalysis().accumulated == 0xFFFFFFFFu &&
              timer.GetStateForAnalysis().active == 255, "guards precede mutation");
        auto clone = timer.Clone();
        const auto* copy = dynamic_cast<spTimer*>(clone.get());
        Check(copy && copy->GetStateForAnalysis().active == 0 &&
              copy->GetStateForAnalysis().limited == 0 &&
              copy->GetStateForAnalysis().accumulated == 0 &&
              copy->GetStateForAnalysis().startedAt == 0 && copy->GetStateForAnalysis().limit == 0,
              "clone fresh payload");
        spCloneManager manager;
        const auto before = timer.GetStateForAnalysis();
        Check(timer.vfunc_14(timer, manager) && timer.GetStateForAnalysis().active == before.active,
              "inherited self copy no-op");
        bool resetSawZero = false;
        Check(timer.ResetForAnalysis([&] { resetSawZero = timer.GetStateForAnalysis().accumulated == 0;
                                          return std::uint32_t(40); }) && resetSawZero,
              "reset clears accumulation before clock");
        Check(timer.StartForAnalysis(clock), "start");
        now = 15;
        Check(timer.StopForAnalysis(clock) && timer.GetStateForAnalysis().accumulated == 5,
              "first stop");
        now = 18;
        Check(timer.StopForAnalysis(clock) && timer.GetStateForAnalysis().accumulated == 13,
              "inactive stop still adds from unchanged start");
    }
}
int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--batch") return Batch();
        UnitTests();
        std::cout << "PASS " << checks << '/' << checks << ": portable spTimer checks\n";
        return 0;
    }
    catch (const std::exception& error)
    { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
