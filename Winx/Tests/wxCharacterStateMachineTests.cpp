#include "Analysis/PC/wxCharacterStateMachineAbi.h"
#include "Analysis/PS2/wxCharacterStateMachineAbi.h"
#include "Code/wxCharacterStateMachine.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;

namespace
{
    void Require(bool value, const char* message)
    {
        if (!value) { std::cerr << message << '\n'; std::exit(EXIT_FAILURE); }
    }

    struct State final : wxCharacterState
    {
        State(unsigned number, std::vector<std::string>& out) : id(number), trace(out) {}
        unsigned id;
        std::vector<std::string>& trace;
        bool accept = true;
        void vfunc_0C(const void* message) noexcept override
        { trace.push_back("message:" + std::to_string(id) + ":" + std::to_string(message != nullptr)); }
        bool vfunc_1C(wxAnimationRequestForAnalysis& request) override
        {
            trace.push_back("enter:" + std::to_string(id) + ":" +
                std::to_string(request.packedKey));
            return accept;
        }
        void vfunc_28(wxAnimationRequestForAnalysis&) override
        { trace.push_back("suspend:" + std::to_string(id)); }
        void vfunc_2C(wxAnimationRequestForAnalysis&) override
        { trace.push_back("resume:" + std::to_string(id)); }
    };

    struct Host final : wxCharacterStateMachineHost
    {
        explicit Host(std::vector<std::string>& out) : trace(out) {}
        std::vector<std::string>& trace;
        void DispatchEventForAnalysis(wxCharacterStateMachine& machine,
            std::uint32_t code, std::uint32_t state, std::uint32_t data) override
        {
            trace.push_back("event:" + std::to_string(code) + ":" +
                std::to_string(state) + ":" + std::to_string(data) + ":" +
                std::to_string(machine.GetRuntimeForAnalysis().gate));
        }
        void HandleCode28ForAnalysis(wxCharacterStateMachine&) override
        { trace.push_back("code28"); }
        void HandleCode30ForAnalysis(wxCharacterStateMachine&) override
        { trace.push_back("code30"); }
    };

    wxCharacterStateMachine::SnapshotForAnalysis Fixture(unsigned depth = 2)
    {
        return {2, 9, 153, 0xABCDEF01, 0x13572468, 0x24681357,
            depth, 0x87654321, 0xA5};
    }
}

int main()
{
    static_assert(sizeof(winx::evidence::pc::wxCharacterStateMachineLayout) == 0x224);
    static_assert(sizeof(winx::evidence::ps2::wxCharacterStateMachineLayout) == 0x230);
    auto factory = spRTTIManager::Instance().Create(wxCharacterStateMachine::ClassID);
    auto* machine = dynamic_cast<wxCharacterStateMachine*>(factory.get());
    Require(machine && machine->IsKindOf(0x796A1869), "factory and ancestry");
    Require(machine->GetRuntimeForAnalysis().gate == 1
        && machine->GetRuntimeForAnalysis().lastFlag == 1,
        "constructor defaults");
    Require(dynamic_cast<wxCharacterStateMachine*>(machine->Clone().get()) != nullptr,
        "default clone");
    Require(wxCharacterStateMachine::AllowsBoundFlagsForAnalysis(0x100)
        && !wxCharacterStateMachine::AllowsBoundFlagsForAnalysis(0x18),
        "slot 11 flag filter");

    std::vector<std::string> trace;
    Host host(trace);
    State zero(0, trace), two(2, trace);
    machine->SetHostForAnalysis(&host);
    machine->SetStateForAnalysis(0, &zero);
    machine->SetStateForAnalysis(2, &two);
    Require(machine->Clone() == nullptr, "bound state clone remains open");

    machine->SetRuntimeForAnalysis(Fixture());
    two.accept = false;
    Require(!machine->EnterCurrentForAnalysis()
        && machine->GetRuntimeForAnalysis().gate == 153,
        "rejected entry preserves gate");
    two.accept = true;
    Require(machine->EnterCurrentForAnalysis()
        && machine->GetRuntimeForAnalysis().gate == 0,
        "accepted entry clears gate");
    trace.clear();

    machine->SetRuntimeForAnalysis(Fixture());
    zero.SetTransitionFlagsForAnalysis(false, false, false, false, false);
    machine->SwitchToNextForAnalysis();
    auto value = machine->GetRuntimeForAnalysis();
    Require(value.current == 0 && value.previous == 2 && value.gate == 0
        && value.previousData == 0x13572468 && value.currentData == 0x24681357,
        "original switch fields");
    Require(trace == std::vector<std::string>{
        "event:10016:0:610800471:153", "enter:0:610800471"},
        "switch event precedes gate update and entry");
    Require(zero.GetTransitionFlagsForAnalysis()
        == std::array<bool, 5>{true, true, true, false, false},
        "switch enables the three measured state flags");
    trace.clear();

    machine->SetRuntimeForAnalysis(Fixture(0));
    machine->PushCurrentForAnalysis();
    value = machine->GetRuntimeForAnalysis();
    Require(value.depth == 1 && machine->GetSavedStateForAnalysis(0) == 2
        && machine->GetSavedDataForAnalysis(0) == 0x13572468,
        "push stores state and data");
    Require(trace == std::vector<std::string>{
        "event:10018:2:324478056:153", "suspend:2",
        "event:10016:0:610800471:153", "enter:0:610800471"},
        "push, suspend and switch order");
    trace.clear();

    machine->SetRuntimeForAnalysis(Fixture(1));
    machine->SetSavedForAnalysis(0, 0, 0x11111111);
    machine->PopSavedForAnalysis();
    value = machine->GetRuntimeForAnalysis();
    Require(value.current == 0 && value.previous == 2 && value.gate == 0
        && value.depth == 0 && value.currentData == 0x11111111,
        "pop restores saved pair and clears gate");
    Require(trace == std::vector<std::string>{
        "event:10019:0:286331153:0", "resume:0"},
        "pop event precedes state resume");
    trace.clear();

    machine->SetRuntimeForAnalysis(Fixture());
    machine->SetModeFromCodeForAnalysis(2);
    Require(machine->GetRuntimeForAnalysis().mode == 0x87654321,
        "unmapped mode keeps prior value");
    machine->SetModeFromCodeForAnalysis(1);
    Require(machine->GetRuntimeForAnalysis().mode == 5, "code 1 mode");
    machine->SetModeFromCodeForAnalysis(3);
    Require(machine->GetRuntimeForAnalysis().mode == 6, "code 3 mode");
    machine->SetModeFromCodeForAnalysis(0);
    Require(machine->GetRuntimeForAnalysis().mode == 0, "code 0 mode");

    machine->SetRuntimeForAnalysis(Fixture());
    machine->ResetForAnalysis();
    value = machine->GetRuntimeForAnalysis();
    Require(value.current == 0 && value.previous == 0 && value.gate == 1
        && value.currentData == 0 && value.nextData == 0 && value.depth == 0
        && value.mode == 0 && value.lastFlag == 1,
        "reset leaves gate set after direct state entry");
    Require(trace == std::vector<std::string>{"enter:0:0"}, "reset callback");
    trace.clear();

    machine->HandleMessageForAnalysis(0x1C, nullptr);
    machine->HandleMessageForAnalysis(0x1E, nullptr);
    machine->HandleMessageForAnalysis(0x99, machine);
    Require(trace == std::vector<std::string>{"code28", "code30", "message:0:1"},
        "notification branch dispatch");
    std::cout << "wxCharacterStateMachine checks passed\n";
}
