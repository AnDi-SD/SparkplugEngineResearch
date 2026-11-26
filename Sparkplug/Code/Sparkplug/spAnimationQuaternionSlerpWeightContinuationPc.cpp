#include "spAnimationQuaternionSlerpWeightContinuationPc.h"
#include "Analysis/PC/spSphereContactMath.h"

#include <cstring>
#include <limits>

namespace sparkplug::reconstruction
{
    namespace
    {
        namespace math = evidence::pc::sphere_contact_math;

        constexpr int ExtendedBias = 16383;
        constexpr std::uint64_t IntegerBit = std::uint64_t{1} << 63;
        static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559);

        bool DecodeSine(AnimationSlerpSine80ForAnalysis input, math::Number& value) noexcept
        {
            const int exponent = input.signExponent & 0x7FFFu;
            const bool negative = (input.signExponent & 0x8000u) != 0;
            if (exponent == 0 && input.significand == 0)
            {
                value = {0, 0, negative};
                return true;
            }
            if (negative || exponent < ExtendedBias - 64 || exponent > ExtendedBias ||
                input.significand < IntegerBit || (exponent == ExtendedBias && input.significand != IntegerBit))
                return false;
            value = {input.significand, exponent - ExtendedBias - 63, false};
            return true;
        }

        constexpr bool AdmittedReciprocal(std::uint32_t word) noexcept
        {
            return 0x3F800000u <= word && word <= 0x44800000u;
        }

        constexpr bool AdmittedAngleOperand(std::uint32_t word, std::uint32_t maximum) noexcept
        {
            return (word & 0x7FFFFFFFu) == 0 || (0x1F800000u <= word && word <= maximum);
        }

        math::Number Widen(std::uint32_t word) noexcept
        {
            float value;
            std::memcpy(&value, &word, sizeof(value));
            return math::From(value);
        }

        std::uint32_t StoreWord(math::Number number) noexcept
        {
            const auto value = math::Store(number);
            std::uint32_t word;
            std::memcpy(&word, &value, sizeof(word));
            return word;
        }
    }

    bool TryFinishAnimationQuaternionSlerpFirstWeightPcForAnalysis(
        AnimationSlerpSine80ForAnalysis sineA, std::uint32_t inverseSineBits,
        std::uint32_t thetaBits, std::uint32_t factorBits, AnimationSlerpPostSineAPcForAnalysis& destination) noexcept
    {
        math::Number sine;
        if (!DecodeSine(sineA, sine) || !AdmittedReciprocal(inverseSineBits) ||
            !AdmittedAngleOperand(thetaBits, 0x40800000u) || !AdmittedAngleOperand(factorBits, 0x3F800000u))
            return false;

        destination = {StoreWord(math::Multiply(sine, Widen(inverseSineBits))),
                       StoreWord(math::Multiply(Widen(thetaBits), Widen(factorBits)))};
        return true;
    }

    bool TryFinishAnimationQuaternionSlerpSecondWeightPcForAnalysis(
        AnimationSlerpSine80ForAnalysis sineB, std::uint32_t inverseSineBits,
        AnimationSlerpBlendWeight80ForAnalysis& destination) noexcept
    {
        math::Number sine;
        if (!DecodeSine(sineB, sine) || !AdmittedReciprocal(inverseSineBits))
            return false;

        const auto weight = math::Multiply(sine, Widen(inverseSineBits));
        auto exponent = static_cast<std::uint16_t>(weight.mantissa ? weight.exponent + 63 + ExtendedBias : 0);
        if (weight.negative)
            exponent |= 0x8000u;
        destination = {weight.mantissa, exponent};
        return true;
    }
}
