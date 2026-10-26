#pragma once

#include "Code/Sparkplug/spFunctionEval.h"
#include "Code/Sparkplug/spTaskTimer.h"

namespace winx::reconstruction
{
    class wxMinotaurDefendAIAction;
    class wxPerception;

    class wxMinotaurDefendAIActionHost
    {
    public:
        virtual ~wxMinotaurDefendAIActionHost() = default;
        virtual wxPerception& ReadPerceptionForAnalysis(void* owner) noexcept = 0;
        // Borrowed spTaskTimer prefix of the real wxGameTimer service.
        virtual sparkplug::reconstruction::spTaskTimer& GameTimerForAnalysis() noexcept = 0;
        // Same actual generator algorithm/state used by FunctionEval/Particle;
        // explicit test states are supported without substituting RNG returns.
        virtual sparkplug::reconstruction::spFunctionEval::RandomStateForAnalysis&
            RandomStateForAnalysis() noexcept = 0;
        virtual void* ReadCommandForAnalysis(void* owner) noexcept = 0;
        virtual void WriteCommandByteForAnalysis(void* command,
            std::uint32_t offset, std::uint8_t value) noexcept = 0;
        virtual void SelectActionForAnalysis(void* owner, std::uint32_t key,
            std::uint32_t parameter) noexcept = 0;
        // Diagnostic only: it cannot turn an unknown owned stage into success.
        virtual void ObserveUnqualifiedStateForAnalysis(
            const wxMinotaurDefendAIAction&, std::uint32_t state) noexcept = 0;
    };
}
