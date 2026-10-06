#include "Code/Sparkplug/spMasterTimer.h"
#include "Analysis/PC/spMasterTimerAbi.h"
#include <iostream>
#include <stdexcept>

using namespace sparkplug::reconstruction;
namespace
{
    unsigned checks = 0;
    void Check(bool value, const char* description)
    { ++checks; if (!value) throw std::runtime_error(description); }
    void Tests()
    {
        Check(!spMasterTimer::GetInstanceForAnalysis(), "initial singleton");
        auto source = std::make_unique<spMasterTimer>();
        Check(spMasterTimer::GetInstanceForAnalysis() == source.get(), "constructor publishes complete object");
        Check(source->IsExactly(spMasterTimer::ClassID) && source->IsKindOf(spTimer::ClassID) &&
              source->IsKindOf(spBaseObject::ClassID), "RTTI chain");
        source->SetStateForAnalysis({255, 123, 456, 254, 789});
        auto clone = source->Clone();
        auto* timer = dynamic_cast<spMasterTimer*>(clone.get());
        Check(timer && spMasterTimer::GetInstanceForAnalysis() == timer, "clone publishes new instance");
        const auto& state = timer->GetStateForAnalysis();
        Check(!state.active && !state.accumulated && !state.startedAt && !state.limited && !state.limit,
              "clone has default inherited state");
        source.reset();
        Check(!spMasterTimer::GetInstanceForAnalysis(), "older destruction clears current singleton");
        Check(timer->StartForAnalysis([] { return std::uint32_t(100); }) &&
              timer->StopForAnalysis([] { return std::uint32_t(125); }) &&
              timer->GetStateForAnalysis().accumulated == 25, "inherited timer operations");
        clone.reset();
        Check(!spMasterTimer::GetInstanceForAnalysis(), "clone destruction leaves null");
        auto factory = spMasterTimer::StaticRTTI().factory();
        Check(factory && spMasterTimer::GetInstanceForAnalysis() == factory.get(), "RTTI factory publication");
    }
    int Sequence()
    {
        // Normalized literal fields for comparison with the original factory,
        // clone, getter and destructor stages. Native pointers become roles.
        auto source = std::make_unique<spMasterTimer>();
        auto emit = [](unsigned role, const spMasterTimer& timer) {
            const auto& state = timer.GetStateForAnalysis();
            std::cout << '[' << role << ',' << unsigned(state.active) << ',' << state.accumulated
                      << ',' << state.startedAt << ',' << unsigned(state.limited) << ','
                      << state.limit << ']';
        };
        std::cout << '[';
        emit(spMasterTimer::GetInstanceForAnalysis() == source.get() ? 1 : 0, *source);
        source->SetStateForAnalysis({255, 123, 456, 254, 789});
        auto clone = source->Clone();
        auto* timer = dynamic_cast<spMasterTimer*>(clone.get());
        if (!timer) throw std::runtime_error("master clone type");
        std::cout << ',';
        emit(spMasterTimer::GetInstanceForAnalysis() == timer ? 2 : 0, *timer);
        source.reset();
        std::cout << ',';
        emit(spMasterTimer::GetInstanceForAnalysis() ? 2 : 0, *timer);
        clone.reset();
        std::cout << ",[" << (spMasterTimer::GetInstanceForAnalysis() ? 2 : 0) << "]]\n";
        return 0;
    }
}
int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--sequence") return Sequence();
        Tests();
        Check(!spMasterTimer::GetInstanceForAnalysis(), "factory destruction clears singleton");
        std::cout << "PASS " << checks << '/' << checks << ": spMasterTimer lifecycle checks\n";
        return 0;
    }
    catch (const std::exception& error)
    { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
