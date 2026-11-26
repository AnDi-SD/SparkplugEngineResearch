#pragma once

#include <array>
#include <cstdint>

namespace winx::reconstruction
{
    class wxChestTrigger
    {
    public:
        using CapturedPrefixForAnalysis = std::array<std::uint8_t, 0x3D4>;

        // PC slot +0x48 tests the full owned word and returns a DWORD result.
        [[nodiscard]] static bool TryReadPcOwnedWordIsZeroForAnalysis(
            const CapturedPrefixForAnalysis& state, std::uint32_t& eax) noexcept;
    };
}
