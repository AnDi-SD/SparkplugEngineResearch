#pragma once

#include <array>
#include <cstdint>

namespace winx::reconstruction
{
    class wxWinxHelpStateMachine
    {
    public:
        using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x228>;

        // PC slot +0x4C ignores its argument and returns a complete DWORD.
        [[nodiscard]] static bool TryReadPcMaskedCodeForAnalysis(const CapturedPrefixForAnalysis& state,
            std::uint32_t ignoredArgument, std::uint32_t& eax) noexcept;
    };
}
