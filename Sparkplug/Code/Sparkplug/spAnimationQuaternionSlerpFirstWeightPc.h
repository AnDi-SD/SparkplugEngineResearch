#pragma once

#include "spAnimationQuaternionSlerpSmallAnglePc.h"

namespace sparkplug::reconstruction
{
    struct AnimationSlerpFirstWeightPcForAnalysis
    {
        std::uint32_t inverseSineBits;
        std::uint32_t angleBits;
    };

    // PC 0x004649B1 to the exclusive stop 0x004649D1 under FPCW=037F.
    // The physical sine is canonical binary80 in [F32(0.001), 1].
    // Theta and factor are signed zeros or normals in [2^-32, F32(pi/2)]
    // and [2^-32, 1], respectively. Refusal preserves the destination.
    [[nodiscard]] bool TryPrepareAnimationQuaternionSlerpFirstWeightPcForAnalysis(
        AnimationSlerpSine80ForAnalysis sine, std::uint32_t thetaBits, std::uint32_t factorBits,
        AnimationSlerpFirstWeightPcForAnalysis& destination) noexcept;
}
