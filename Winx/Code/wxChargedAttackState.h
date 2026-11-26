#pragma once

#include <array>
#include <cstdint>

namespace winx::reconstruction
{
    class wxChargedAttackState
    {
    public:
        using CapturedStatePrefixForAnalysis = std::array<std::uint8_t, 0x48>;
        using CapturedCharacterPrefixForAnalysis = std::array<std::uint8_t, 0x144>;

        struct BorrowedCharacterForAnalysis
        {
            std::uint32_t capturedAddress = 0;
            const CapturedCharacterPrefixForAnalysis* prefix = nullptr;
        };

        // Target 1 requires a typed character capture matching the state's owner word.
        [[nodiscard]] static bool TryReadPcTransitionPermissionForAnalysis(
            const CapturedStatePrefixForAnalysis& state, std::uint32_t rawTarget,
            const BorrowedCharacterForAnalysis& character, std::uint8_t& al) noexcept;
    };
}
