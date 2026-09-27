#pragma once

#include <cstdint>

namespace winx::reconstruction
{
    class wxBacoAttackAIAction;

    // Project adapter for the original owner, command, registry and perception.
    // Targets are borrowed. The host must outlive the action.
    class wxBacoAttackAIActionHost
    {
    public:
        virtual ~wxBacoAttackAIActionHost() = default;
        virtual void SetCommandByteForAnalysis(std::uint32_t offset,
            std::uint8_t value) noexcept = 0;
        virtual void* FindRegistryTargetForAnalysis() noexcept = 0;
        virtual void* FindNearestTargetForAnalysis() noexcept = 0;
        virtual void SelectActionForAnalysis(std::uint32_t key,
            std::uint32_t parameter) noexcept = 0;
        // PC 5B88A0 / PS2 2448A0 dispatch to five active state bodies.
        // These bodies require game services not yet reconstructed here.
        virtual void DispatchStateBodyForAnalysis(wxBacoAttackAIAction&,
            std::uint32_t state) noexcept = 0;
    };
}
