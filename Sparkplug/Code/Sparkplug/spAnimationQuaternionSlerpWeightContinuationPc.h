#pragma once

#include "spAnimationQuaternionSlerpBlendPc.h"
#include "spAnimationQuaternionSlerpSmallAnglePc.h"

namespace sparkplug::reconstruction
{
    struct AnimationSlerpPostSineAPcForAnalysis
    {
        std::uint32_t weightABits;
        std::uint32_t angleBBits;
    };

    // PC 0x004649D6–0x004649E9. The independently supplied sineA is signed
    // zero or canonical positive binary80 in [2^-64, 1]. The reciprocal is
    // binary32 in [1, 1024]. Theta and factor admit signed zeros and normal
    // binary32 in [2^-64, 4] and [2^-64, 1], respectively.
    [[nodiscard]] bool TryFinishAnimationQuaternionSlerpFirstWeightPcForAnalysis(
        AnimationSlerpSine80ForAnalysis sineA, std::uint32_t inverseSineBits,
        std::uint32_t thetaBits, std::uint32_t factorBits, AnimationSlerpPostSineAPcForAnalysis& destination) noexcept;

    // PC 0x004649EE–0x004649F5. The second supplied sine and the reciprocal
    // use the same numeric domain. The result remains in binary80.
    [[nodiscard]] bool TryFinishAnimationQuaternionSlerpSecondWeightPcForAnalysis(
        AnimationSlerpSine80ForAnalysis sineB, std::uint32_t inverseSineBits,
        AnimationSlerpBlendWeight80ForAnalysis& destination) noexcept;
}
