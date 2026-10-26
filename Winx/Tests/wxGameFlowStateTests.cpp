#include "Code/wxGameFlowState.h"
#include "Analysis/PC/wxGameFlowStateAbi.h"
#include "Analysis/PS2/wxGameFlowStateAbi.h"
#include <algorithm>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace sparkplug::reconstruction;
using namespace winx::reconstruction;

namespace
{
    void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
    struct Host final : wxGameFlowStateHost
    {
        wxGameFlowResourcesForAnalysis resources;
        std::vector<int> calls;
        int inputObject = 7;
        void* input = &inputObject;
        std::uint32_t ticks = 1200, divisor = 1;
        Host()
        {
            for (std::size_t i = 0; i < 56; ++i)
            {
                resources.sourceNames[i] = reinterpret_cast<const void*>(std::uintptr_t{0x60000000} + i);
                resources.cache[i] = &inputObject;
            }
        }
        void* BorrowInputForAnalysis() override { calls.push_back(1); return input; }
        spTaskTimer::ClockReadingForAnalysis ReadClockForAnalysis() override
        { calls.push_back(2); return {ticks, divisor}; }
        wxGameFlowResourcesForAnalysis& ResourcesForAnalysis() override { calls.push_back(3); return resources; }
    };
    void EmitTimer(const spTaskTimer& timer)
    {
        const auto& t = timer.GetStateForAnalysis();
        std::cout << t.active << ' ' << t.relative << ' ' << t.currentMilliseconds << ' '
            << t.startMilliseconds << ' ' << t.pausedMilliseconds << ' ' << t.deltaSeconds;
    }
    void EmitState(const wxGameFlowState& object)
    {
        const auto& s = object.GetStateForAnalysis();
        std::cout << s.field10 << ' ' << unsigned(s.field14);
        for (auto code : s.codes) std::cout << ' ' << code;
        std::cout << ' ' << unsigned(s.field24) << ' ' << unsigned(s.field25) << ' '
            << s.field34 << ' ' << unsigned(s.field38) << ' ';
        EmitTimer(object.TimerForAnalysis());
        std::cout << '\n';
    }
    void Emit()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream row(line);
            std::string op;
            if (!(row >> op)) continue;
            Host host;
            if (op == "remap")
            {
                std::uint32_t code, state, rawProgress;
                if (!(row >> code >> state >> rawProgress)) throw std::invalid_argument("remap inputs");
                // Convert bits without implementation-defined unsigned->signed narrowing.
                const auto progress = rawProgress <= 0x7FFFFFFF ? static_cast<std::int32_t>(rawProgress)
                    : static_cast<std::int32_t>(static_cast<std::int64_t>(rawProgress) - 0x100000000LL);
                std::cout << wxGameFlowState::RemapResourceCodeForAnalysis(code, state, progress) << '\n';
            }
            else if (op == "set")
            {
                wxGameFlowState object(host);
                auto& codes = object.StateForFixture().codes;
                std::uint32_t a, b, c;
                if (!(row >> codes[0] >> codes[1] >> codes[2] >> a >> b >> c)) throw std::invalid_argument("set inputs");
                object.SetCodesForAnalysis(a,b,c);
                std::cout << codes[0] << ' ' << codes[1] << ' ' << codes[2] << '\n';
            }
            else if (op == "construct")
            {
                if (!(row >> host.ticks >> host.divisor)) throw std::invalid_argument("clock inputs");
                wxGameFlowState object(host);
                EmitState(object);
            }
            else if (op == "get")
            {
                wxGameFlowState object(host);
                auto& codes = object.StateForFixture().codes;
                if (!(row >> codes[0] >> codes[1] >> codes[2])) throw std::invalid_argument("getter inputs");
                std::uint32_t a,b,c;
                object.GetCodesForAnalysis(a,b,c);
                std::cout << a << ' ' << b << ' ' << c << '\n';
            }
            else if (op == "hook")
            {
                wxGameFlowState object(host);
                unsigned slot;
                if (!(row >> slot)) throw std::invalid_argument("hook slot");
                switch (slot)
                {
                case 0x1C: std::cout << object.FlowSlot1CForAnalysis(); break;
                case 0x20: object.FlowSlot20ForAnalysis(); std::cout << "void"; break;
                case 0x24: object.FlowSlot24ForAnalysis(); std::cout << "void"; break;
                case 0x28: object.FlowSlot28ForAnalysis(); std::cout << "void"; break;
                case 0x2C: object.FlowSlot2CForAnalysis(); std::cout << "void"; break;
                case 0x30: std::cout << object.FlowSlot30ForAnalysis(); break;
                default: throw std::invalid_argument("unknown native hook slot");
                }
                std::cout << '\n';
            }
            else if (op == "clone")
            {
                std::array<std::uint32_t,3> codes{};
                if (!(row >> host.ticks >> host.divisor >> codes[0] >> codes[1] >> codes[2])) throw std::invalid_argument("clone inputs");
                wxGameFlowState object(host);
                object.StateForFixture().codes = codes;
                spCloneManager manager;
                auto clone = manager.Clone(object);
                auto* state = dynamic_cast<wxGameFlowState*>(clone.get());
                Check(state != nullptr, "Typed clone observation");
                EmitState(*state);
            }
            else if (op == "mapping")
            {
                wxGameFlowState::ResetResourcesForAnalysis(host.resources);
                for (auto pointer : host.resources.names) std::cout << reinterpret_cast<std::uintptr_t>(pointer) << ' ';
                std::cout << std::count(host.resources.cache.begin(),host.resources.cache.end(),nullptr) << '\n';
            }
            else throw std::invalid_argument("Unknown observation operation");
        }
    }
    void TestLifecycle()
    {
        Host host;
        wxGameFlowState object(host);
        Check(host.calls == std::vector<int>({1,2,3}), "Borrow input, start timer, reset globals in native order");
        const auto& initial = object.GetStateForAnalysis();
        Check(initial.field10 == 0 && initial.field14 == 0 && initial.field24 == 1
            && initial.field25 == 1 && initial.field34 == 0 && initial.field38 == 1, "Constructor known defaults");
        Check(initial.codes == std::array<std::uint32_t,3>{0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF}, "PC sentinel defaults");
        Check(object.GetBorrowedInputForAnalysis() == &host.inputObject, "Borrowed input bound once");
        Check(object.TimerForAnalysis().GetStateForAnalysis().active
            && object.TimerForAnalysis().GetStateForAnalysis().startMilliseconds == 1200, "Owned task timer reset and started");
        Check(object.IsExactly(wxGameFlowState::ClassID) && object.IsKindOf(spBaseObject::ClassID), "Typed identity");

        object.StateForFixture() = {13,255,{1,2,3},4,5,6,7};
        auto timerState = object.TimerForAnalysis().GetStateForAnalysis();
        timerState.currentMilliseconds = 400;
        object.TimerForAnalysis().SetStateForAnalysis(timerState);
        host.ticks = 9000;
        int nextInput = 9;
        host.input = &nextInput;
        host.resources.cache[55] = &nextInput;
        spCloneManager manager;
        auto rawClone = manager.Clone(object);
        auto* clone = dynamic_cast<wxGameFlowState*>(rawClone.get());
        Check(clone && clone != &object, "Clone has the correct receiver type");
        Check(clone->GetStateForAnalysis().codes
            == std::array<std::uint32_t,3>{0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF}, "Clone constructs fresh codes");
        Check(clone->GetStateForAnalysis().field10 == 0 && clone->GetStateForAnalysis().field24 == 1, "Clone ignores own runtime state");
        Check(&clone->TimerForAnalysis() != &object.TimerForAnalysis()
            && clone->TimerForAnalysis().GetStateForAnalysis().startMilliseconds == 9000, "Clone owns a fresh started timer");
        Check(clone->GetBorrowedInputForAnalysis() == &nextInput
            && object.GetBorrowedInputForAnalysis() == &host.inputObject, "Clone resamples external input");
        Check(host.resources.cache[55] == nullptr, "Clone constructor repeats global reset");
        Check(manager.FindClone(object) == nullptr, "Clone transaction clears mapping");
        const auto before = object.GetStateForAnalysis().codes;
        Check(clone->vfunc_14(object,manager) && object.GetStateForAnalysis().codes == before, "Inherited Copy preserves existing own state");
        rawClone.reset();
        Check(nextInput == 9 && host.inputObject == 7, "Destruction does not own borrowed input");
        wxGameFlowState::SetFactoryHostForAnalysis(&host);
        auto factoryObject = spRTTIManager::Instance().Create(wxGameFlowState::ClassID);
        Check(dynamic_cast<wxGameFlowState*>(factoryObject.get()) != nullptr, "Registered factory uses required host");
        factoryObject.reset();
        wxGameFlowState::SetFactoryHostForAnalysis(nullptr);
    }
    void TestLeavesAndGuards()
    {
        Host host;
        wxGameFlowState object(host);
        object.StateForFixture().codes = {1,2,3};
        object.SetCodesForAnalysis(0xFFFFFFFF,0x80000000,0);
        Check(object.GetStateForAnalysis().codes == std::array<std::uint32_t,3>{1,0x80000000,0}, "Sentinel suppresses only its own write");
        auto& codes = object.StateForFixture().codes;
        codes = {11,22,33};
        object.GetCodesForAnalysis(codes[1],codes[2],codes[0]);
        Check(codes == std::array<std::uint32_t,3>{11,11,11}, "Getter preserves sequential aliased output loads");
        const auto before = object.GetStateForAnalysis().codes;
        Check(object.FlowSlot1CForAnalysis() && object.FlowSlot30ForAnalysis(), "Two actual true hooks");
        object.FlowSlot20ForAnalysis(); object.FlowSlot24ForAnalysis();
        object.FlowSlot28ForAnalysis(); object.FlowSlot2CForAnalysis();
        Check(object.GetStateForAnalysis().codes == before, "Void hooks do not mutate own fields");
        Host invalid;
        invalid.divisor = 0;
        bool rejected = false;
        try { wxGameFlowState guarded(invalid); } catch (const std::logic_error&) { rejected = true; }
        Check(rejected && invalid.calls == std::vector<int>({1,2}), "Invalid host clock rejects before global reset");
    }
}
int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--emit") { Emit(); return 0; }
        TestLifecycle();
        TestLeavesAndGuards();
        std::cout << "wxGameFlowState checks passed\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
