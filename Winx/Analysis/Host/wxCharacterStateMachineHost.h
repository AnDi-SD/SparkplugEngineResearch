#pragma once

#include <cstdint>

namespace winx::reconstruction
{
    class wxCharacterStateMachine;

    // Per-object notifications and the two unclosed update branches belong
    // to the game scene. The state machine supplies their measured arguments.
    class wxCharacterStateMachineHost
    {
    public:
        virtual ~wxCharacterStateMachineHost() = default;
        virtual void DispatchEventForAnalysis(wxCharacterStateMachine& machine,
            std::uint32_t code, std::uint32_t state, std::uint32_t data) = 0;
        virtual void HandleCode28ForAnalysis(wxCharacterStateMachine& machine) = 0;
        virtual void HandleCode30ForAnalysis(wxCharacterStateMachine& machine) = 0;
    };
}
