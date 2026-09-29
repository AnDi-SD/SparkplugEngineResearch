#pragma once

#include <cstdint>
#include <memory>

namespace winx::reconstruction
{
    class wxBacoStateMachine;
    class wxCharacterState;

    // The native scene registration, base-machine setup, state factories and
    // the 0x2717 recipient are external to this recovered leaf class.
    class wxBacoStateMachineHost
    {
    public:
        virtual ~wxBacoStateMachineHost() = default;
        virtual void PrepareMachineForAnalysis(wxBacoStateMachine& machine) = 0;
        virtual std::unique_ptr<wxCharacterState> CreateStateForAnalysis(
            std::uint32_t classID) = 0;
        virtual void BindStateForAnalysis(wxCharacterState& state,
            wxBacoStateMachine& machine) = 0;
        virtual void Handle2717ForAnalysis(wxCharacterState& hurtState) = 0;
    };
}
