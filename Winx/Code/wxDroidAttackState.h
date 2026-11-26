#pragma once

#include <array>
#include <cstdint>

namespace winx::reconstruction
{
    class wxDroidAttackState
    {
    public:
        using CapturedStatePrefixForAnalysis = std::array<std::uint8_t, 0x44>;

        // A recognized PC capture supplies the byte queried by slot +0x34.
        [[nodiscard]] static bool TryReadPcTransitionPermissionForAnalysis(
            const CapturedStatePrefixForAnalysis& state, std::uint32_t rawTarget,
            std::uint32_t& eax) noexcept;
    };
}
