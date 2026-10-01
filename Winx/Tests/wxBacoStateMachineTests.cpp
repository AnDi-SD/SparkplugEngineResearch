#include "Analysis/PC/wxBacoStateMachineAbi.h"
#include "Analysis/PS2/wxBacoStateMachineAbi.h"
#include "Code/wxBacoStateMachine.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
    using namespace winx::reconstruction;
    using namespace sparkplug::reconstruction;

    void Require(const bool value, const char* message)
    {
        if (!value)
        {
            std::cerr << "FAILED: " << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    class TraceHost final : public wxBacoStateMachineHost
    {
    public:
        void PrepareMachineForAnalysis(wxBacoStateMachine&) override
        {
            events.push_back(0x1C);
        }
        std::unique_ptr<wxCharacterState> CreateStateForAnalysis(
            const std::uint32_t classID) override
        {
            events.push_back(classID);
            return std::make_unique<wxCharacterState>();
        }
        void BindStateForAnalysis(wxCharacterState& state,
            wxBacoStateMachine&) override
        {
            bindings.push_back(&state);
        }
        void Handle2717ForAnalysis(wxCharacterState& hurtState) override
        {
            Require(&hurtState == bindings.at(2), "2717 recipient");
            events.push_back(0x2717);
        }
        std::vector<std::uint32_t> events;
        std::vector<wxCharacterState*> bindings;
    };
}

int main()
{
    wxBacoStateMachine machine;
    Require(machine.vfunc_18().classID == wxBacoStateMachine::ClassID
        && machine.IsKindOf(0xD32F3AA1)
        && machine.IsKindOf(0x796A1869)
        && dynamic_cast<wxCharacterStateMachine*>(&machine) != nullptr
        && wxBacoStateMachine::MachineKind == 17,
        "native identity and registration ancestry");
    auto created = spRTTIManager::Instance().Create(wxBacoStateMachine::ClassID);
    Require(dynamic_cast<wxBacoStateMachine*>(created.get()) != nullptr,
        "registered factory");
    auto clone = machine.Clone();
    Require(dynamic_cast<wxBacoStateMachine*>(clone.get()) != nullptr,
        "default clone");
    Require(!machine.RejectTransitionForAnalysis(), "slot 14 returns false");

    const auto classify = wxBacoStateMachine::ClassifyRequestForAnalysis;
    Require(classify(3u << 15, false) == 11
        && classify(3u << 15, true) == 11,
        "category 3 precedes owner test");
    Require(classify(1u << 15, false) == 11
        && classify(1u << 15, true) == 10,
        "category 1 requires owner");
    Require(classify(2u << 15 | 8u << 7, true) == 3
        && classify(9u << 7, true) == 3
        && classify(7u << 7, true) == 0
        && classify(8u << 7, false) == 11,
        "action byte 8/9 and owner gate");

    wxBacoStateMachine::ControlFlagsForAnalysis flags{};
    const auto compute = wxBacoStateMachine::ComputeFlagsForAnalysis;
    Require(compute(0xFFFFFFFFu, flags) == (0xFFFFFFFFu & ~15u & 0xFFFF807Fu & 0xFFF87FFFu),
        "clear selected flag fields, preserve others");
    flags.flag21 = flags.flag5C = true;
    Require(compute(0, flags) == 0x8480u, "secondary control flags");
    flags.flag20 = flags.flag5F = true;
    Require(compute(0, flags) == 0x18400u, "primary controls take priority");
    Require(compute(0xFFFFFFFFu, flags) == 0xFFF98470u,
        "unrelated high bits survive both control groups");
    machine.UpdateFlagsForAnalysis(0, flags);
    Require(machine.GetComputedFlagsForAnalysis() == 0x18400u,
        "computed flags are stored");
    auto changedClone = machine.Clone();
    auto* freshBaco = dynamic_cast<wxBacoStateMachine*>(changedClone.get());
    Require(freshBaco && freshBaco->GetComputedFlagsForAnalysis() == 0,
        "inherited Copy leaves derived flags at constructor defaults");

    wxBacoStateMachine empty;
    TraceHost host;
    Require(!empty.HandleMessageForAnalysis(0x9999, host)
        && host.events.empty(), "unknown message belongs to missing base");
    bool missingState = false;
    try { (void)empty.HandleMessageForAnalysis(0x2717, host); }
    catch (const std::logic_error&) { missingState = true; }
    Require(missingState, "2717 requires initialized state");
    Require(empty.HandleMessageForAnalysis(0x1C, host), "setup notification");
    Require(host.events == std::vector<std::uint32_t>{0x1C, 0x463733DF,
        0x6ADD2466, 0x54F716CC, 0xBCC87DA1},
        "four original state factories in order");
    for (std::size_t slot = 0; slot < 4; ++slot)
        Require(empty.GetStateForAnalysis(slot) == host.bindings[slot],
            "state ownership and binding order");
    for (std::size_t slot = 0; slot < 4; ++slot)
        Require(static_cast<wxCharacterStateMachine&>(empty).GetStateForAnalysis(
            wxBacoStateMachine::StateSlots[slot]) == host.bindings[slot],
            "states connect to common machine slots");
    Require(empty.GetStateForAnalysis(4) == nullptr,
        "state slot bound");
    Require(empty.HandleMessageForAnalysis(0x2717, host)
        && host.events.back() == 0x2717, "2717 dispatch");
    bool repeated = false;
    try { empty.SetupForAnalysis(host); }
    catch (const std::logic_error&) { repeated = true; }
    Require(repeated, "repeat setup is not guessed");
    auto setupClone = empty.Clone();
    auto* freshSetup = dynamic_cast<wxBacoStateMachine*>(setupClone.get());
    Require(freshSetup && !freshSetup->GetStateForAnalysis(0)
        && !freshSetup->GetStateForAnalysis(1)
        && !freshSetup->GetStateForAnalysis(2)
        && !freshSetup->GetStateForAnalysis(3),
        "inherited Copy does not share constructed states");
    return EXIT_SUCCESS;
}
