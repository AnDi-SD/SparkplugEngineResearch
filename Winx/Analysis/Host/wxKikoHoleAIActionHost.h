#pragma once
#include <cstdint>

namespace winx::reconstruction
{
    // Borrowed owner144 -> character130 -> command data. This adapter exposes
    // storage only; the own Update body chooses and performs the byte write.
    class wxKikoHoleAIActionHost
    {
    public:
        virtual ~wxKikoHoleAIActionHost() = default;
        virtual void* ReadCommandForAnalysis(void* owner) noexcept = 0;
        virtual void WriteCommandByteForAnalysis(
            void* command, std::uint32_t offset, std::uint8_t value) noexcept = 0;
    };
}
