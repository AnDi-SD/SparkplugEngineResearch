#pragma once

#include "spAnimationQuaternionSlerpPreparationPc.h"

namespace sparkplug::reconstruction
{
    struct AnimationSlerpSine80ForAnalysis
    {
        std::uint64_t significand;
        std::uint16_t signExponent;
    };

    enum class AnimationSlerpSmallAngleBranchForAnalysis
    {
        Unsupported,
        Continue,
        CopyFirst,
    };

    // PC comparison and copy at 0x0046498B. The sine is the physical binary80 word.
    // Admits either signed zero or positive canonical finite normal sine values.
    // First XYZ/W words are signed binary32 zeros or finite normals.
    // The result identifies the branch. Only CopyFirst writes the destination.
    [[nodiscard]] AnimationSlerpSmallAngleBranchForAnalysis ApplyAnimationQuaternionSlerpSmallAnglePcForAnalysis(
        AnimationSlerpSine80ForAnalysis sine,
        const AnimationQuaternionPcWordsForAnalysis& first,
        AnimationQuaternionPcWordsForAnalysis& destination) noexcept;
}
