#pragma once

// Narrow reconstruction of the PC CPU quad packed-color expressions.
// Uses the existing integer 64-significand x87 helpers; no host long-double
// assumption, GPU shader-color substitution or universal x87 emulator claim.
#include "spTransformConstMath.h"
#include <cmath>
#include <cstring>

namespace sparkplug::evidence::pc::ballistic_color_math
{
    using transform_const_math::Number;
    using transform_const_math::Wide;
    using transform_const_math::AddNearest;
    using transform_const_math::NormalizeNearest;
    using float80_rtz::FromFloat;

    inline Number Negate(Number value) noexcept
    { value.negative = !value.negative; return value; }

    inline Number FromUnsigned(const std::uint32_t value) noexcept
    {
        if (!value) return {};
        return NormalizeNearest(Wide::Shifted(value, 0), 0, false);
    }

    inline Number MultiplyFloatNearest(const Number first, const float second) noexcept
    {
        std::uint32_t bits;
        std::memcpy(&bits, &second, 4);
        const auto field = (bits >> 23) & 255;
        const auto significand = (bits & 0x7FFFFF) | (field ? 0x800000 : 0);
        if (!first.mantissa || !significand) return {};
        auto wide = Wide::Shifted(first.mantissa, 0);
        wide.Multiply(significand);
        return NormalizeNearest(wide, first.exponent + int(field ? field : 1) - 150,
            first.negative != bool(bits >> 31));
    }

    inline Number DividePositiveFloatNearest(const Number first, const float second) noexcept
    {
        if (!first.mantissa) return {};
        std::uint32_t bits;
        std::memcpy(&bits, &second, 4);
        const auto field = (bits >> 23) & 255;
        const auto significand = (bits & 0x7FFFFF) | (field ? 0x800000 : 0);
        auto wide = Wide::Shifted(first.mantissa, 32);
        std::uint64_t remainder = 0;
        for (unsigned index = 4; index--;)
        {
            const auto dividend = (remainder << 32) | wide.words[index];
            wide.words[index] = std::uint32_t(dividend / significand);
            remainder = dividend % significand;
        }
        // The quotient has >=8 low bits beyond the 64-bit significand.
        // A nonzero division remainder contributes sticky below all of them.
        if (remainder) wide.words[0] |= 1;
        return NormalizeNearest(wide,
            first.exponent - int(field ? field : 1) + 150 - 32, first.negative);
    }

    inline int Compare(const Number first, const Number second) noexcept
    {
        if (!first.mantissa && !second.mantissa) return 0;
        if (first.negative != second.negative) return first.negative ? -1 : 1;
        const auto magnitude = first.exponent < second.exponent ? -1
            : first.exponent > second.exponent ? 1
            : first.mantissa < second.mantissa ? -1 : first.mantissa > second.mantissa ? 1 : 0;
        return first.negative ? -magnitude : magnitude;
    }

    // Host domain: finite magnitude below2^32. Native FTOL2 also handles
    // larger/exceptional inputs, which are deliberately not claimed here.
    inline bool TruncateLowWord(const Number value, std::uint32_t& output) noexcept
    {
        if (!value.mantissa) { output = 0; return true; }
        if (value.exponent + 63 >= 32) return false;
        const auto magnitude = value.exponent <= -64 ? std::uint32_t{0}
            : std::uint32_t(value.mantissa >> unsigned(-value.exponent));
        output = value.negative ? std::uint32_t{0} - magnitude : magnitude;
        return true;
    }

    inline bool NormalizeAge(const float currentTime, const float birth,
        const float lifetime, const bool storedPosition, Number& result) noexcept
    {
        if (!std::isfinite(currentTime) || !std::isfinite(birth)
            || !std::isfinite(lifetime) || !(lifetime > 0)) return false;
        const double preliminary = double(currentTime) - birth;
        if (std::abs(preliminary) >= 4294967296.0
            || std::abs(preliminary / lifetime) >= 4294967296.0) return false;
        auto age = AddNearest(FromFloat(currentTime), Negate(FromFloat(birth)));
        if (!storedPosition)
        {
            if (Compare(age, FromFloat(lifetime)) > 0)
            {
                std::uint32_t periods = 0;
                if (!TruncateLowWord(DividePositiveFloatNearest(age, lifetime), periods)) return false;
                age = AddNearest(age, Negate(MultiplyFloatNearest(FromUnsigned(periods), lifetime)));
            }
            if (Compare(age, {}) < 0)
            {
                std::uint32_t integerAge = 0;
                if (!TruncateLowWord(Negate(age), integerAge)) return false;
                const auto factor = AddNearest(DividePositiveFloatNearest(FromUnsigned(integerAge), lifetime), FromUnsigned(1));
                age = AddNearest(age, MultiplyFloatNearest(factor, lifetime));
            }
        }
        result = DividePositiveFloatNearest(age, lifetime);
        return true;
    }
}

namespace sparkplug::evidence::pc
{
    // PC4B9D63..4B9EB7 and4BACxx, nearest64 arithmetic followed by FTOL2
    // truncation. RGB adds two separately truncated byte contributions; alpha
    // is overwritten with one truncation of the summed extended contributions.
    // Refusal leaves output unchanged. Bounds are host restrictions.
    [[nodiscard]] inline bool EvaluateBallisticCpuColorForAnalysis(
        const float currentTime, const float birthTime, const float lifetime,
        const bool storedPosition, const std::uint32_t colorBegin,
        const std::uint32_t colorEnd, std::uint32_t& output) noexcept
    {
        using namespace ballistic_color_math;
        Number age;
        if (!NormalizeAge(currentTime, birthTime, lifetime, storedPosition, age)) return false;
        const auto complement = AddNearest(FromUnsigned(1), Negate(age));
        std::uint32_t packed = 0;
        for (unsigned channel = 0; channel < 3; ++channel)
        {
            const auto begin = MultiplyFloatNearest(complement, float((colorBegin >> (8 * channel)) & 255));
            const auto end = MultiplyFloatNearest(age, float((colorEnd >> (8 * channel)) & 255));
            std::uint32_t first = 0, second = 0;
            if (!TruncateLowWord(begin, first) || !TruncateLowWord(end, second)) return false;
            packed |= ((first + second) & 255) << (8 * channel);
        }
        const auto beginAlpha = MultiplyFloatNearest(complement, float(colorBegin >> 24));
        const auto endAlpha = MultiplyFloatNearest(age, float(colorEnd >> 24));
        std::uint32_t alpha = 0;
        if (!TruncateLowWord(AddNearest(beginAlpha, endAlpha), alpha)) return false;
        output = packed | ((alpha & 255) << 24);
        return true;
    }
}
