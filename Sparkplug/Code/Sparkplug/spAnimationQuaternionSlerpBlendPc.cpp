#include "spAnimationQuaternionSlerpBlendPc.h"
#include "Analysis/PC/spSphereContactMath.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace sparkplug::reconstruction
{
    namespace
    {
        namespace math = evidence::pc::sphere_contact_math;

        static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559);

        bool AdmittedF32(std::uint32_t word) noexcept
        {
            const auto magnitude = word & 0x7FFFFFFFu;
            return magnitude == 0 || (magnitude >= 0x2F800000u && magnitude <= 0x45800000u);
        }

        bool DecodeWeight(AnimationSlerpBlendWeight80ForAnalysis word, math::Number& out) noexcept
        {
            const int exponent = word.signExponent & 0x7FFFu;
            const bool negative = (word.signExponent & 0x8000u) != 0;
            if (exponent == 0 && word.significand == 0)
            {
                out = {0, 0, negative};
                return true;
            }
            constexpr std::uint64_t IntegerBit = std::uint64_t{1} << 63;
            constexpr int Bias = 16383;
            if (exponent < Bias - 64 || exponent > Bias + 12 || word.significand < IntegerBit ||
                (exponent == Bias + 12 && word.significand != IntegerBit))
                return false;
            out = {word.significand, exponent - Bias - 63, negative};
            return true;
        }

        math::Number Widen(std::uint32_t word) noexcept
        {
            float value;
            std::memcpy(&value, &word, sizeof(value));
            return math::From(value);
        }

        math::Number Spill(math::Number value) noexcept
        {
            return math::From(math::Store(value));
        }

        std::uint32_t StoreWord(math::Number value) noexcept
        {
            const auto stored = math::Store(value);
            std::uint32_t word;
            std::memcpy(&word, &stored, sizeof(word));
            return word;
        }
    }

    bool TryBlendAnimationQuaternionSlerpPcForAnalysis(
        const AnimationQuaternionPcWordsForAnalysis& first,
        const AnimationQuaternionPcWordsForAnalysis& adjustedSecond,
        std::uint32_t weightABits, AnimationSlerpBlendWeight80ForAnalysis weightB,
        AnimationQuaternionPcWordsForAnalysis& destination) noexcept
    {
        math::Number beta;
        if (!AdmittedF32(weightABits) || !DecodeWeight(weightB, beta) ||
            !std::all_of(first.begin(), first.end(), AdmittedF32) ||
            !std::all_of(adjustedSecond.begin(), adjustedSecond.end(), AdmittedF32))
            return false;

        const auto alpha = Widen(weightABits);
        AnimationQuaternionPcWordsForAnalysis result{};
        for (std::size_t axis = 0; axis < result.size(); ++axis)
        {
            const auto left = math::Multiply(Widen(first[axis]), alpha);
            const auto right = math::Multiply(Widen(adjustedSecond[axis]), beta);
            // W retains the first product. Z retains the second product.
            result[axis] = StoreWord(math::Add(axis == 3 ? left : Spill(left), axis == 2 ? right : Spill(right)));
        }
        destination = result;
        return true;
    }
}
