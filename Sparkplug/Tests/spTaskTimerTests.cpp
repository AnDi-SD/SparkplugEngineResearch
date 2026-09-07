#include "Code/Sparkplug/spTaskTimer.h"
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace sparkplug::reconstruction;
namespace
{
    int checks = 0;
    void Check(bool condition, const char* name)
    {
        ++checks;
        if (!condition)
            throw std::runtime_error(name);
    }
    std::uint32_t Bits(float value)
    {
        std::uint32_t result;
        std::memcpy(&result, &value, sizeof(result));
        return result;
    }
    void UnitTests()
    {
        spTaskTimer root, child(&root), grandchild(&child), sibling(&root);
        std::uint32_t ticks = 1000, divisor = 1;
        int reads = 0;
        spTaskTimer::ClockSourceForAnalysis clock = [&] {
            ++reads;
            return spTaskTimer::ClockReadingForAnalysis{ticks, divisor};
        };
        std::string error;
        Check(root.IsKindOf(spBaseObject::ClassID) && root.IsExactly(spTaskTimer::ClassID),
              "original RTTI");
        Check(spTaskTimer::StaticRTTI().factory() != nullptr, "concrete factory");
        Check(!root.GetStateForAnalysis().active && root.GetStateForAnalysis().relative,
              "paused relative defaults");
        Check(root.GetStateForAnalysis().deltaSeconds == 0 &&
                  root.GetSourceForAnalysis() == nullptr,
              "default time/source");
        Check(child.GetSourceForAnalysis() == &root && root.GetChildrenForAnalysis().empty(),
              "source is not child attachment");
        Check(root.SetChildrenForAnalysis({&child, &sibling}, &error), "literal child list");
        Check(child.SetChildrenForAnalysis({&grandchild}, &error), "nested literal child list");
        Check(!grandchild.SetChildrenForAnalysis({&root}, &error) &&
                  grandchild.GetChildrenForAnalysis().empty(),
              "cycle rejected before publication");
        Check(!root.SetChildrenForAnalysis({&child, &child}, &error) &&
                  root.GetChildrenForAnalysis().size() == 2,
              "duplicate rejection keeps old children");
        Check(!root.SetChildrenForAnalysis({nullptr}, &error), "null child rejected");
        Check(root.StartForAnalysis(clock, &error) && reads == 1, "start reads once");
        Check(!child.GetStateForAnalysis().active, "start does not recurse");
        ticks = 1250;
        Check(root.UpdateForAnalysis(clock, &error), "hierarchy update");
        Check(reads == 2, "linked children do not query hardware");
        for (auto* timer : {&root, &child, &grandchild, &sibling})
            Check(timer->GetStateForAnalysis().currentMilliseconds == 250 &&
                      timer->GetStateForAnalysis().deltaSeconds == .25f &&
                      timer->GetStateForAnalysis().active,
                  "parent time/active propagation");
        root.PauseForAnalysis();
        Check(root.GetStateForAnalysis().deltaSeconds == .25f && child.GetStateForAnalysis().active,
              "pause leaves delta/children until update");
        Check(root.UpdateForAnalysis({}, &error) && !grandchild.GetStateForAnalysis().active &&
                  reads == 2,
              "paused tree requires no provider");
        ticks = 9000;
        Check(root.StartForAnalysis(clock), "resume");
        ticks = 18700;
        divisor = 2;
        Check(root.UpdateForAnalysis(clock) &&
                  root.GetStateForAnalysis().currentMilliseconds == 600,
              "resume preserves accumulated250ms");
        Check(root.GetStateForAnalysis().deltaSeconds == static_cast<float>(350. * double(.001f)),
              "float32 original multiplier");
        root.ResetForAnalysis();
        Check(!root.GetStateForAnalysis().active &&
                  root.GetStateForAnalysis().currentMilliseconds == 0 &&
                  root.GetStateForAnalysis().startMilliseconds == 0 &&
                  root.GetStateForAnalysis().pausedMilliseconds == 0 &&
                  root.GetStateForAnalysis().deltaSeconds > 0,
              "reset only integer timestamps/active");
        const auto before = root.GetStateForAnalysis();
        divisor = 0;
        Check(!root.StartForAnalysis(clock, &error) && !root.GetStateForAnalysis().active &&
                  root.GetStateForAnalysis().deltaSeconds == before.deltaSeconds,
              "zero divisor host guard");
        auto active = before;
        active.active = true;
        root.SetStateForAnalysis(active);
        Check(!root.UpdateForAnalysis(clock, &error) &&
                  root.GetStateForAnalysis().currentMilliseconds == 0,
              "invalid direct clock does not publish local state");
        Check(child.UpdateForAnalysis({}, &error), "linked active timer can use no provider");
        auto clone = child.Clone();
        auto* timerClone = dynamic_cast<spTaskTimer*>(clone.get());
        Check(timerClone && !timerClone->GetStateForAnalysis().active &&
                  timerClone->GetSourceForAnalysis() == nullptr &&
                  timerClone->GetChildrenForAnalysis().empty(),
              "blank native clone");
        Check(root.SetChildrenForAnalysis({}, &error), "clear literal list");
        Check(root.GetChildrenForAnalysis().empty() && child.GetSourceForAnalysis() == &root,
              "child-list changes do not rewrite independent source pointer");
        spTaskTimer clockRoot, earlierSource, firstChild(&earlierSource), secondChild;
        Check(clockRoot.AppendClockChildForAnalysis(firstChild) &&
                  firstChild.GetSourceForAnalysis() == &clockRoot,
              "native core append replaces borrowed source");
        Check(clockRoot.AppendClockChildForAnalysis(secondChild) &&
                  clockRoot.GetChildrenForAnalysis() ==
                      std::vector<spTaskTimer*>{&firstChild, &secondChild},
              "native core append preserves insertion order");
        Check(!clockRoot.AppendClockChildForAnalysis(secondChild) &&
                  clockRoot.GetChildrenForAnalysis().size() == 2,
              "host duplicate guard preserves old graph");
        Check(!firstChild.AppendClockChildForAnalysis(clockRoot) &&
                  clockRoot.GetSourceForAnalysis() == nullptr,
              "host cycle guard leaves source unchanged");
    }
    int Batch()
    {
        char operation;
        unsigned active, relative, linked;
        std::uint32_t current, start, paused, ticks, divisor, sourceCurrent;
        float delta, sourceDelta;
        std::size_t count = 0;
        std::cout << '[';
        while (std::cin >> operation >> active >> relative >> current >> start >> paused >> delta >>
               linked >> sourceCurrent >> sourceDelta >> ticks >> divisor)
        {
            if (++count > 1024 || active > 1 || relative > 1 || linked > 1 || divisor == 0)
                throw std::runtime_error("bounded batch input");
            spTaskTimer source, timer(linked ? &source : nullptr);
            source.SetStateForAnalysis({true, true, sourceCurrent, 0, 0, sourceDelta});
            timer.SetStateForAnalysis(
                {bool(active), bool(relative), current, start, paused, delta});
            auto clock = [&] { return spTaskTimer::ClockReadingForAnalysis{ticks, divisor}; };
            bool success = true;
            if (operation == 'U')
                success = timer.UpdateForAnalysis(clock);
            else if (operation == 'S')
                success = timer.StartForAnalysis(clock);
            else if (operation == 'P')
                timer.PauseForAnalysis();
            else if (operation == 'R')
                timer.ResetForAnalysis();
            else
                throw std::runtime_error("unknown timer batch operation");
            if (!success)
                throw std::runtime_error("timer batch unexpectedly rejected");
            const auto& s = timer.GetStateForAnalysis();
            if (count > 1)
                std::cout << ',';
            std::cout << '[' << s.active << ',' << s.relative << ',' << s.currentMilliseconds << ','
                      << s.startMilliseconds << ',' << s.pausedMilliseconds << ','
                      << Bits(s.deltaSeconds) << ']';
        }
        std::cout << "]\n";
        return 0;
    }
} // namespace
int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--batch")
            return Batch();
        UnitTests();
        std::cout << "PASS " << checks << '/' << checks << ": PC task timer portable checks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
