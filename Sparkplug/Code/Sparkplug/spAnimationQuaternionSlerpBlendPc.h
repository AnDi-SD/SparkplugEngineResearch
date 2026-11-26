#pragma once

#include "spAnimationQuaternionSlerpPreparationPc.h"

namespace sparkplug::reconstruction
{
    struct AnimationSlerpBlendWeight80ForAnalysis
    {
        std::uint64_t significand;
        std::uint16_t signExponent;
    };

    // PC 0x004649F5 through RET 0x00464A8F under FPCW=037F.
    // Input binary32 words are signed zeros or normals of magnitude [2^-32, 4096].
    // The supplied binary80 weight is signed zero or a canonical normal of magnitude [2^-64, 4096].
    // The destination may alias either whole endpoint. Refusal preserves its words.
    [[nodiscard]] bool TryBlendAnimationQuaternionSlerpPcForAnalysis(
        const AnimationQuaternionPcWordsForAnalysis& first,
        const AnimationQuaternionPcWordsForAnalysis& adjustedSecond,
        std::uint32_t weightABits, AnimationSlerpBlendWeight80ForAnalysis weightB,
        AnimationQuaternionPcWordsForAnalysis& destination) noexcept;
}
