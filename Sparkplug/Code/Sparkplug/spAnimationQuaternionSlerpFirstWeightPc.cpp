#include "spAnimationQuaternionSlerpFirstWeightPc.h"
#include "Analysis/PC/spSphereContactMath.h"

#include <cstring>
#include <limits>

namespace sparkplug::reconstruction
{
    namespace
    {
        namespace math = evidence::pc::sphere_contact_math;
        static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559);

        bool AdmittedF32(std::uint32_t bits, std::uint32_t upper) noexcept
        {
            return (bits & 0x7FFFFFFFu) == 0 || (bits >= 0x2F800000u && bits <= upper);
        }

        bool DecodeSine(AnimationSlerpSine80ForAnalysis sine, math::Number& output) noexcept
        {
            constexpr std::uint64_t IntegerBit = std::uint64_t{1} << 63;
            constexpr std::uint64_t ThresholdSignificand = 0x83126F0000000000ull;
            constexpr int Bias = 16383;
            const int exponent = sine.signExponent;
            if (exponent < Bias - 10 || exponent > Bias || sine.significand < IntegerBit ||
                (exponent == Bias - 10 && sine.significand < ThresholdSignificand) ||
                (exponent == Bias && sine.significand != IntegerBit))
                return false;
            output = {sine.significand, exponent - Bias - 63, false};
            return true;
        }

        math::Number Widen(std::uint32_t bits) noexcept
        {
            float value;
            std::memcpy(&value, &bits, sizeof(value));
            return math::From(value);
        }

        std::uint32_t ToWord(math::Number value) noexcept
        {
            const auto stored = math::Store(value);
            std::uint32_t bits;
            std::memcpy(&bits, &stored, sizeof(bits));
            return bits;
        }
    }

    bool TryPrepareAnimationQuaternionSlerpFirstWeightPcForAnalysis(
        AnimationSlerpSine80ForAnalysis sine, std::uint32_t thetaBits, std::uint32_t factorBits,
        AnimationSlerpFirstWeightPcForAnalysis& destination) noexcept
    {
        math::Number decodedSine;
        if (!DecodeSine(sine, decodedSine) || !AdmittedF32(thetaBits, 0x3FC90FDBu) ||
            !AdmittedF32(factorBits, 0x3F800000u))
            return false;

        const auto inverse = math::Reciprocal(decodedSine);
        const auto complement = math::Subtract(math::From(1.0f), Widen(factorBits));
        const auto angle = math::Multiply(complement, Widen(thetaBits));
        destination = {ToWord(inverse), ToWord(angle)};
        return true;
    }
}
