#pragma once

#include <cstdint>

namespace winx::reconstruction
{
    class wxIdleAIAction;
    class wxPerception;

    // Required foreign owner/Character/command/selector adapter. Query results
    // are computed by the shared wxPerception source rather than fabricated here.
    class wxIdleAIActionHost
    {
    public:
        virtual ~wxIdleAIActionHost() = default;
        virtual void* ReadCommandForAnalysis(void* owner) noexcept = 0;
        virtual void ResetCommandForAnalysis(wxIdleAIAction&, void* command) noexcept = 0;
        virtual std::uint8_t ReadPermissionForAnalysis(void* owner) noexcept = 0;
        virtual wxPerception& ReadPerceptionForAnalysis(void* owner) noexcept = 0;
        virtual void SelectActionForAnalysis(void* owner, std::uint32_t key,
            std::uint32_t parameter) noexcept = 0;
    };
}
