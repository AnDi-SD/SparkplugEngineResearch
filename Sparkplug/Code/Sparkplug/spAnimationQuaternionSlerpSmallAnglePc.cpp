#include "spAnimationQuaternionSlerpSmallAnglePc.h"

#include <algorithm>

namespace sparkplug::reconstruction
{
    namespace
    {
        constexpr std::uint16_t ExtendedExponentMask = 0x7FFFu;
        constexpr std::uint64_t ExtendedIntegerBit = UINT64_C(0x8000000000000000);
        // Exact widening of the original binary32 threshold 0x3A83126F.
        constexpr std::uint16_t ThresholdExponent = 0x3FF5u;
        constexpr std::uint64_t ThresholdSignificand = UINT64_C(0x83126F0000000000);

        constexpr bool AdmittedEndpointWord(std::uint32_t bits) noexcept
        {
            const auto magnitude = bits & 0x7FFFFFFFu;
            return magnitude == 0 || (0x00800000u <= magnitude && magnitude < 0x7F800000u);
        }
    }

    AnimationSlerpSmallAngleBranchForAnalysis ApplyAnimationQuaternionSlerpSmallAnglePcForAnalysis(
        AnimationSlerpSine80ForAnalysis sine,
        const AnimationQuaternionPcWordsForAnalysis& first,
        AnimationQuaternionPcWordsForAnalysis& destination) noexcept
    {
        using Branch = AnimationSlerpSmallAngleBranchForAnalysis;
        const auto exponent = sine.signExponent & ExtendedExponentMask;
        const bool zero = exponent == 0 && sine.significand == 0;
        const bool positiveNormal = sine.signExponent == exponent && exponent != 0 &&
                                    exponent < ExtendedExponentMask && (sine.significand & ExtendedIntegerBit) != 0;
        if ((!zero && !positiveNormal) || !std::all_of(first.begin(), first.end(), AdmittedEndpointWord))
            return Branch::Unsupported;

        const bool belowThreshold = exponent < ThresholdExponent ||
            (exponent == ThresholdExponent && sine.significand < ThresholdSignificand);
        if (!belowThreshold)
            return Branch::Continue;

        destination = first;
        return Branch::CopyFirst;
    }
}
