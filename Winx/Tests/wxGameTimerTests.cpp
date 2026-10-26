#include "Code/wxGameTimer.h"
#include "Analysis/PC/wxGameTimerAbi.h"
#include "Analysis/PS2/wxGameTimerAbi.h"
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace sparkplug::reconstruction;
using winx::reconstruction::wxGameTimer;
namespace
{
    int checks = 0;
    void Check(bool value, const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
    float Float(std::uint32_t bits) { float value; std::memcpy(&value, &bits, sizeof(value)); return value; }
    std::uint32_t Bits(float value) { std::uint32_t bits; std::memcpy(&bits, &value, sizeof(bits)); return bits; }
    struct Receiver final : spBaseObject
    {
        unsigned notifications = 0;
        void vfunc_0C(const void*) noexcept override { ++notifications; }
    };
    spNotificationForAnalysis Packet(wxGameTimer& timer)
    {
        spNotificationForAnalysis packet; packet.code = 0x2717; packet.sender = &timer; packet.word14 = 1;
        return packet;
    }
    void Queue(wxGameTimer& timer, Receiver& receiver, std::uint32_t delay)
    {
        auto packet = Packet(timer); timer.QueueForAnalysis().EnqueueForAnalysis(packet, delay, {1, &receiver});
    }
    void UnitTests()
    {
        {
            spTimer clock; clock.SetStateForAnalysis({255, 17, 29, 255, 0xFFFFFFFF});
            Receiver receiver; wxGameTimer timer(clock);
            Check(timer.IsExactly(wxGameTimer::ClassID) && timer.IsKindOf(spTaskTimer::ClassID)
                && !timer.IsKindOf(spTimer::ClassID), "TaskTimer inheritance and separate global clock role");
            Check(&timer.GlobalClockForAnalysis() == &clock && clock.GetStateForAnalysis().active == 255
                && clock.GetStateForAnalysis().accumulated == 17 && clock.GetStateForAnalysis().startedAt == 29,
                "Constructor preserves actual shared clock state");
            Check(clock.GetStateForAnalysis().limited == 1 && clock.GetStateForAnalysis().limit == 100,
                "Constructor configures actual global spTimer limit");
            Check(!timer.GetStateForAnalysis().active && timer.GetStateForAnalysis().relative
                && timer.GetStateForAnalysis().currentMilliseconds == 0 && timer.GetSourceForAnalysis() == nullptr
                && timer.GetPausedByteForAnalysis() == 0 && timer.QueueForAnalysis().PendingCountForAnalysis() == 0,
                "Fresh default TaskTimer and Queue");
            auto state = timer.GetStateForAnalysis(); state.currentMilliseconds = 1000; timer.SetStateForAnalysis(state);
            auto packet = Packet(timer); timer.QueueForAnalysis().EnqueueForAnalysis(packet, 15, {1, &receiver});
            Check(packet.word04 == 1015 && clock.GetStateForAnalysis().accumulated == 17, "Queue borrows this TaskTimer, not global spTimer");
            state.currentMilliseconds = 1014; timer.SetStateForAnalysis(state);
            Check(timer.QueueForAnalysis().TickForAnalysis() == spMessageQueueTickResultForAnalysis::Completed
                && receiver.notifications == 0, "Self clock before due");
            state.currentMilliseconds = 1015; timer.SetStateForAnalysis(state);
            Check(timer.QueueForAnalysis().TickForAnalysis() == spMessageQueueTickResultForAnalysis::Completed
                && receiver.notifications == 1, "Actual Queue/direct receiver composition");
            Queue(timer, receiver, 100);
            timer.SetPausedByteForAnalysis(255);
            spTaskTimer& base = timer; base.PauseForAnalysis();
            Check(timer.GetPausedByteForAnalysis() == 1 && !timer.GetStateForAnalysis().active
                && timer.GetStateForAnalysis().pausedMilliseconds == 1015, "Virtual Pause own prefix then shared body");
            const spTaskTimer::ClockSourceForAnalysis source = [] { return spTaskTimer::ClockReadingForAnalysis{1200, 3}; };
            Check(base.StartForAnalysis(source) && timer.GetPausedByteForAnalysis() == 0
                && timer.GetStateForAnalysis().startMilliseconds == 400, "Virtual Start own prefix then shared body");
            timer.SetPausedByteForAnalysis(255); timer.ResetForAnalysis();
            Check(timer.GetPausedByteForAnalysis() == 255 && timer.QueueForAnalysis().PendingCountForAnalysis() == 1
                && timer.GetStateForAnalysis().currentMilliseconds == 0, "Inherited Reset preserves raw own byte and queued messages");
            timer.SetPausedByteForAnalysis(1);
            Check(!timer.StartForAnalysis({}) && timer.GetPausedByteForAnalysis() == 0,
                "Missing-clock analytical guard follows native own flag write");
        }
        Check(wxGameTimer::GetInstanceForAnalysis() == nullptr, "Destructor clears singleton");
        {
            spTimer clock; Receiver receiver;
            auto original = std::make_unique<wxGameTimer>(clock);
            auto state = original->GetStateForAnalysis(); state.active = true; state.currentMilliseconds = 1234;
            original->SetStateForAnalysis(state); original->SetPausedByteForAnalysis(255); Queue(*original, receiver, 500);
            auto global = clock.GetStateForAnalysis(); global.limited = 255; global.limit = 0xDEADBEEF; clock.SetStateForAnalysis(global);
            auto cloneBase = original->Clone(); auto* clone = dynamic_cast<wxGameTimer*>(cloneBase.get());
            Check(clone && clone != original.get() && clone->IsExactly(wxGameTimer::ClassID), "Actual derived clone type");
            Check(!clone->GetStateForAnalysis().active && clone->GetStateForAnalysis().relative
                && clone->GetStateForAnalysis().currentMilliseconds == 0 && clone->GetPausedByteForAnalysis() == 0
                && clone->QueueForAnalysis().PendingCountForAnalysis() == 0, "Clone reconstructs empty runtime");
            Check(original->GetStateForAnalysis().currentMilliseconds == 1234 && original->GetPausedByteForAnalysis() == 255
                && original->QueueForAnalysis().PendingCountForAnalysis() == 1, "Clone leaves original runtime intact");
            Check(clock.GetStateForAnalysis().limited == 1 && clock.GetStateForAnalysis().limit == 100,
                "Clone reapplies real global clock configuration");
            Check(wxGameTimer::GetInstanceForAnalysis() == clone, "Clone publishes itself");
            spBaseObject unrelated; spCloneManager manager;
            Check(original->vfunc_14(unrelated, manager) && original->QueueForAnalysis().PendingCountForAnalysis() == 1,
                "Literal root Copy has no own runtime or destination-kind operation");
            original.reset();
            Check(wxGameTimer::GetInstanceForAnalysis() == nullptr && clone->QueueForAnalysis().PendingCountForAnalysis() == 0,
                "Deleting earlier instance clears singleton while clone remains live");
            Check(receiver.notifications == 0 && clock.GetStateForAnalysis().limited == 1 && clock.GetStateForAnalysis().limit == 100,
                "Owned Queue destruction does not dispatch or restore global limit");
        }
        {
            spTimer clock; wxGameTimer::SetFactoryBindingsForAnalysis({}); bool refused = false;
            try { (void)wxGameTimer::StaticRTTI().factory(); } catch (const std::logic_error&) { refused = true; }
            Check(refused, "Factory requires typed actual global clock binding");
            wxGameTimer::SetFactoryBindingsForAnalysis({&clock, nullptr});
            auto object = wxGameTimer::StaticRTTI().factory(); auto* timer = dynamic_cast<wxGameTimer*>(object.get());
            Check(timer && &timer->GlobalClockForAnalysis() == &clock, "Factory uses real shared clock object");
            wxGameTimer::SetFactoryBindingsForAnalysis({});
        }
    }

    void PrintClock(const spTimer& clock)
    {
        const auto& state = clock.GetStateForAnalysis();
        std::cout << static_cast<unsigned>(state.active) << ' ' << state.accumulated << ' ' << state.startedAt
            << ' ' << static_cast<unsigned>(state.limited) << ' ' << state.limit << '\n';
    }
    spTimer::StateForAnalysis ReadClock(std::istream& in)
    {
        unsigned active, limited; spTimer::StateForAnalysis state;
        in >> active >> state.accumulated >> state.startedAt >> limited >> state.limit;
        state.active = static_cast<std::uint8_t>(active); state.limited = static_cast<std::uint8_t>(limited); return state;
    }
    void Replay(std::istream& input)
    {
        std::string line;
        while (std::getline(input, line))
        {
            if (line.empty()) continue;
            std::istringstream in(line); char command; in >> command; spTimer clock; Receiver receiver;
            if (command == 'm')
            {
                unsigned operation, active, paused; std::uint32_t current, start, storedPaused, delta, raw, divisor;
                in >> operation >> active >> paused >> current >> start >> storedPaused >> delta >> raw >> divisor;
                wxGameTimer timer(clock); auto state = timer.GetStateForAnalysis(); state.active = active != 0;
                state.currentMilliseconds = current; state.startMilliseconds = start; state.pausedMilliseconds = storedPaused;
                state.deltaSeconds = Float(delta); timer.SetStateForAnalysis(state); timer.SetPausedByteForAnalysis(static_cast<std::uint8_t>(paused));
                Queue(timer, receiver, 100);
                if (operation == 0)
                {
                    if (!timer.StartForAnalysis([&] { return spTaskTimer::ClockReadingForAnalysis{raw, divisor}; }))
                        throw std::runtime_error("Replay Start guard");
                }
                else if (operation == 1) timer.PauseForAnalysis();
                else if (operation == 2) timer.ResetForAnalysis();
                else throw std::runtime_error("Replay timer operation");
                const auto& after = timer.GetStateForAnalysis();
                std::cout << after.active << ' ' << static_cast<unsigned>(timer.GetPausedByteForAnalysis()) << ' '
                    << after.currentMilliseconds << ' ' << after.startMilliseconds << ' ' << after.pausedMilliseconds << ' '
                    << Bits(after.deltaSeconds) << ' ' << timer.QueueForAnalysis().PendingCountForAnalysis() << '\n';
            }
            else if (command == 'c')
            {
                clock.SetStateForAnalysis(ReadClock(in)); wxGameTimer timer(clock); PrintClock(clock);
            }
            else if (command == 'g')
            {
                wxGameTimer timer(clock); clock.SetStateForAnalysis(ReadClock(in)); auto clone = timer.Clone();
                if (!clone) throw std::runtime_error("Replay global clone"); PrintClock(clock);
            }
            else if (command == 'd')
            {
                clock.SetStateForAnalysis(ReadClock(in)); { wxGameTimer timer(clock); } PrintClock(clock);
            }
            else if (command == 'x')
            {
                auto original = std::make_unique<wxGameTimer>(clock); auto state = original->GetStateForAnalysis();
                state.active = true; state.currentMilliseconds = 1234; original->SetStateForAnalysis(state);
                original->SetPausedByteForAnalysis(255); Queue(*original, receiver, 500);
                auto cloneBase = original->Clone(); auto* clone = dynamic_cast<wxGameTimer*>(cloneBase.get());
                if (!clone) throw std::runtime_error("Replay runtime clone type");
                const auto& fresh = clone->GetStateForAnalysis();
                std::cout << fresh.active << ' ' << fresh.relative << ' ' << fresh.currentMilliseconds << ' '
                    << fresh.startMilliseconds << ' ' << fresh.pausedMilliseconds << ' ' << Bits(fresh.deltaSeconds) << ' '
                    << static_cast<unsigned>(clone->GetPausedByteForAnalysis()) << ' ' << clone->QueueForAnalysis().PendingCountForAnalysis() << ' '
                    << clone->QueueForAnalysis().LiveMessageCountForAnalysis() << ' ' << (wxGameTimer::GetInstanceForAnalysis() == clone) << ' '
                    << original->QueueForAnalysis().PendingCountForAnalysis() << ' ' << static_cast<unsigned>(original->GetPausedByteForAnalysis()) << ' '
                    << original->GetStateForAnalysis().currentMilliseconds << ' ' << original->GetStateForAnalysis().active << ' '
                    << static_cast<unsigned>(clock.GetStateForAnalysis().limited) << ' ' << clock.GetStateForAnalysis().limit << ' ';
                original.reset(); std::cout << (wxGameTimer::GetInstanceForAnalysis() == nullptr) << '\n';
            }
            else throw std::runtime_error("Unknown GameTimer replay command");
            if (!in) throw std::runtime_error("Malformed GameTimer replay row");
        }
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--replay") { Replay(std::cin); return 0; }
        UnitTests(); std::cout << "PASS " << checks << " GameTimer checks\n"; return 0;
    }
    catch (const std::exception& error) { std::cerr << "FAIL " << error.what() << '\n'; return 1; }
}
