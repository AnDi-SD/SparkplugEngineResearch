#include "spAnimationKeyIntervalPc.h"
#include "Analysis/PC/spSphereContactMath.h"

#include <cstring>
#include <limits>

namespace sparkplug::reconstruction
{
    namespace
    {
        namespace math = evidence::pc::sphere_contact_math;

        constexpr std::uint64_t LeadingMantissaBit = std::uint64_t{1} << 63;
        static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559);

        constexpr bool Admitted(std::uint32_t bits) noexcept
        {
            const auto magnitude = bits & 0x7FFFFFFFu;
            return magnitude == 0 || (magnitude >= 0x1F800000u && magnitude <= 0x45800000u);
        }

        float FloatFromWord(std::uint32_t bits) noexcept
        {
            float value;
            std::memcpy(&value, &bits, sizeof(value));
            return value;
        }

        std::uint32_t StoreWord(math::Number value) noexcept
        {
            const auto rounded = math::Store(value);
            std::uint32_t bits;
            std::memcpy(&bits, &rounded, sizeof(bits));
            return bits;
        }

        math::Number DivideIntervalDifferences(math::Number numerator, math::Number denominator) noexcept
        {
            if (numerator.mantissa == 0)
                return numerator;

            const auto divisor = denominator.mantissa;
            const bool leadingQuotientAtLeastOne = numerator.mantissa >= divisor;
            auto exponent = numerator.exponent - denominator.exponent - (leadingQuotientAtLeastOne ? 63 : 64);
            auto remainder = leadingQuotientAtLeastOne
                ? numerator.mantissa - divisor
                : numerator.mantissa - (divisor - numerator.mantissa);
            auto mantissa = LeadingMantissaBit;

            for (int bit = 62; bit >= 0; --bit)
            {
                // Comparing r with d-r forms the next digit without overflowing 2*r.
                const auto complement = divisor - remainder;
                if (remainder >= complement)
                {
                    remainder -= complement;
                    mantissa |= std::uint64_t{1} << bit;
                }
                else
                    remainder += remainder;
            }

            const auto complement = divisor - remainder;
            if (remainder > complement || (remainder == complement && (mantissa & 1u) != 0))
            {
                ++mantissa;
                if (mantissa == 0)
                {
                    mantissa = LeadingMantissaBit;
                    ++exponent;
                }
            }
            return {mantissa, exponent, false};
        }
    }

    bool TryFindAnimationKeyIntervalPcForAnalysis(
        const std::uint32_t* timestampBits, std::size_t keyCount, std::uint32_t timeBits,
        std::int32_t cachedIndex, AnimationKeyIntervalPcForAnalysis& result) noexcept
    {
        constexpr auto MaximumCount = static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max());
        if (timestampBits == nullptr || keyCount == 0 || keyCount > MaximumCount ||
            cachedIndex < 0 || !Admitted(timeBits) || !Admitted(timestampBits[0]))
            return false;

        const auto firstTime = FloatFromWord(timestampBits[0]);
        auto previous = firstTime;
        for (std::size_t i = 1; i < keyCount; ++i)
        {
            if (!Admitted(timestampBits[i]))
                return false;
            const auto current = FloatFromWord(timestampBits[i]);
            if (current < previous)
                return false;
            previous = current;
        }

        const auto time = FloatFromWord(timeBits);
        if (time < firstTime)
        {
            result = {0, 0};
            return true;
        }

        auto index = static_cast<std::size_t>(cachedIndex);
        if (index >= keyCount)
            index = 0;
        while (index > 0 && time < FloatFromWord(timestampBits[index]))
            --index;
        while (index + 1 < keyCount && time >= FloatFromWord(timestampBits[index + 1]))
            ++index;

        if (index == keyCount - 1)
        {
            result = index > 1
                ? AnimationKeyIntervalPcForAnalysis{static_cast<std::int32_t>(index - 1), 0x3F800000u}
                : AnimationKeyIntervalPcForAnalysis{0, 0};
            return true;
        }

        const auto left = math::From(FloatFromWord(timestampBits[index]));
        const auto right = math::From(FloatFromWord(timestampBits[index + 1]));
        const auto elapsed = math::Subtract(math::From(time), left);
        const auto duration = math::Subtract(right, left);
        result = {static_cast<std::int32_t>(index), StoreWord(DivideIntervalDifferences(elapsed, duration))};
        return true;
    }
}
