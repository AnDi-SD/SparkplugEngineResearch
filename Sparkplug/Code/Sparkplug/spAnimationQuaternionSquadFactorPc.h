#pragma once

#include <cstdint>

namespace sparkplug::reconstruction
{
    // PC SQUAD blend factor at 0x00464B3C. The input is a signed zero or a
    // positive normal binary32 from 2^-64 through 1.
    [[nodiscard]] bool TryEvaluateAnimationQuaternionSquadFactorPcForAnalysis(
        std::uint32_t intervalFactorBits, std::uint32_t& blendFactorBits) noexcept;
}
