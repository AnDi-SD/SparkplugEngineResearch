#pragma once
#include "Code/Sparkplug/spTaskTimer.h"
#include "Code/Sparkplug/spFunctionEval.h"
#include <cstdint>
namespace winx::reconstruction
{
    class wxTrollBeforeFightAIActionHost
    {
    public:
        virtual ~wxTrollBeforeFightAIActionHost() = default;
        virtual void* ReadCommandForAnalysis(void* owner) noexcept = 0;
        // Foreign receiver operation PC5992E0/PS23730E0 through proven
        // command tables703090/494710. Its native class name remains unknown.
        virtual void ResetCommandForAnalysis(void* command) noexcept = 0;
        virtual void WriteCommandByteForAnalysis(void* command, std::uint32_t offset,
            std::uint8_t value) noexcept = 0;
        virtual std::uint32_t ReadCharacterStateWordForAnalysis(void* owner) noexcept = 0;
        // Must return the actual wxGameTimer's shared physical timer prefix.
        virtual sparkplug::reconstruction::spTaskTimer& GameTimerPrefixForAnalysis() noexcept = 0;
        virtual sparkplug::reconstruction::spFunctionEval::RandomStateForAnalysis& RandomStateForAnalysis() noexcept = 0;
    };
}
