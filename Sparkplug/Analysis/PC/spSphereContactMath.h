#pragma once
// PC472270..4723AD: finite value helper, nearest/64-significand arithmetic.
// New root/division/product operations are local to this query. Existing
// transform arithmetic remains unchanged; no general x87 exception emulator.
#include "Code/Sparkplug/spBoundingVolumeQueries.h"
#include "spTransformConstMath.h"

namespace sparkplug::evidence::pc::sphere_contact_math
{
    using Number = float80_rtz::Number;
    using Wide = float80_rtz::Wide;
    namespace nearest = transform_const_math;

    inline int CompareWide(const Wide& first, const Wide& second) noexcept
    {
        for (unsigned i = 4; i--;)
            if (first.words[i] != second.words[i]) return first.words[i] < second.words[i] ? -1 : 1;
        return 0;
    }
    inline void ShiftLeft(Wide& value, unsigned count) noexcept
    {
        // Only counts1/2 are used; the query's root/division remainders fit65 bits.
        std::uint32_t carry = 0;
        for (auto& word : value.words)
        {
            const auto next = word >> (32 - count);
            word = (word << count) | carry;
            carry = next;
        }
    }
    inline Number Negate(Number value) noexcept { value.negative = !value.negative; return value; }
    inline Number From(float value) noexcept { return float80_rtz::FromFloat(value); }
    inline Number Add(Number a, Number b) noexcept { return nearest::AddNearest(a, b); }
    inline Number Subtract(Number a, Number b) noexcept { return Add(a, Negate(b)); }
    inline float Store(Number value) noexcept { return nearest::StoreNearest(value); }
    inline Number Multiply(Number first, Number second) noexcept
    {
        const bool negative = first.negative != second.negative;
        if (!first.mantissa || !second.mantissa) return {0, 0, negative};
        const std::uint32_t a[2]{std::uint32_t(first.mantissa), std::uint32_t(first.mantissa >> 32)};
        const std::uint32_t b[2]{std::uint32_t(second.mantissa), std::uint32_t(second.mantissa >> 32)};
        Wide product;
        for (unsigned i = 0; i != 2; ++i)
        {
            std::uint64_t carry = 0;
            for (unsigned j = 0; j != 2; ++j)
            {
                const auto value = std::uint64_t(a[i]) * b[j] + product.words[i + j] + carry;
                product.words[i + j] = std::uint32_t(value); carry = value >> 32;
            }
            product.words[i + 2] = std::uint32_t(carry);
        }
        return nearest::NormalizeNearest(product, first.exponent + second.exponent, negative);
    }
    inline Number Rounded(std::uint64_t mantissa, int exponent, bool roundUp) noexcept
    {
        if (roundUp && ++mantissa == 0) return {std::uint64_t(1) << 63, exponent + 1, false};
        return {mantissa, exponent, false};
    }
    inline Number SquareRoot(float positive) noexcept
    {
        const auto value = From(positive);
        const unsigned shift = value.exponent % 2 != 0 ? 63 : 64;
        const Wide radicand = Wide::Shifted(value.mantissa, shift);
        Wide remainder; std::uint64_t root = 0;
        // Restoring integer sqrt of the exact127/128-bit radicand.
        for (unsigned pair = 64; pair--;)
        {
            ShiftLeft(remainder, 2);
            remainder.words[0] |= (radicand.words[(pair * 2) / 32] >> ((pair * 2) % 32)) & 3;
            root <<= 1;
            auto trial = Wide::Shifted(root, 1); trial.words[0] |= 1;
            if (CompareWide(remainder, trial) >= 0) { remainder.Subtract(trial); ++root; }
        }
        // sqrt(integer) cannot lie exactly at an integer+1/2 tie. Its nearest
        // root rounds up iff the integer remainder exceeds the floor root.
        return Rounded(root, (value.exponent - int(shift)) / 2,
            CompareWide(remainder, Wide::Shifted(root, 0)) > 0);
    }
    inline Number Reciprocal(Number positive) noexcept
    {
        const unsigned power = positive.mantissa == (std::uint64_t(1) << 63) ? 126 : 127;
        const auto denominator = Wide::Shifted(positive.mantissa, 0);
        Wide remainder; std::uint64_t quotient = 0;
        for (unsigned bit = power + 1; bit--;)
        {
            ShiftLeft(remainder, 1);
            if (bit == power) remainder.words[0] |= 1;
            if (CompareWide(remainder, denominator) >= 0)
            {
                remainder.Subtract(denominator);
                // The selected power and normalized denominator guarantee
                // a quotient of exactly64 significant bits.
                if (bit < 64) quotient |= std::uint64_t(1) << bit;
            }
        }
        ShiftLeft(remainder, 1); const int halfway = CompareWide(remainder, denominator);
        return Rounded(quotient, -positive.exponent - int(power), halfway > 0 || (halfway == 0 && (quotient & 1)));
    }
    inline bool Query(const reconstruction::spSphereContactInputForAnalysis& input,
        reconstruction::spSphereContactForAnalysis& output) noexcept
    {
        Number delta[3];
        for (unsigned lane = 0; lane != 3; ++lane) delta[lane] = Subtract(From(input.center2[lane]), From(input.center1[lane]));
        const auto square = [](Number value) noexcept { return Multiply(value, value); };
        // Native distance squared is spilled toF32 before the strict test.
        const float storedSquared = Store(Add(Add(square(delta[2]), square(delta[1])), square(delta[0])));
        const Number squared = From(storedSquared), sumRadius = Add(From(input.radius1), From(input.radius2));
        if (nearest::Compare(squared, square(sumRadius)) >= 0) return false;
        if (!squared.mantissa)
        {
            // Native copies exact center words, then fixed normal and sumR.
            output.position[0] = input.center1[0]; output.position[1] = input.center1[1]; output.position[2] = input.center1[2];
            output.normal[0] = 0.0F; output.normal[1] = 1.0F; output.normal[2] = 0.0F;
            output.depth = Store(sumRadius); return true;
        }
        const Number distance = SquareRoot(storedSquared), reciprocal = Reciprocal(distance);
        Number normal[3];
        normal[0] = Subtract(From(input.center1[0]), From(input.center2[0]));
        normal[1] = From(Store(Subtract(From(input.center1[1]), From(input.center2[1]))));
        normal[2] = From(Store(Subtract(From(input.center1[2]), From(input.center2[2]))));
        // PC472344/347/34A: Z, X, Y stores before their point-offset rereads.
        output.normal[2] = Store(Multiply(normal[2], reciprocal));
        output.normal[0] = Store(Multiply(normal[0], reciprocal));
        output.normal[1] = Store(Multiply(normal[1], reciprocal));
        const Number shift = Multiply(Subtract(Subtract(From(input.radius2), From(input.radius1)), distance), From(0.5F));
        const float offsetX = Store(Multiply(shift, From(output.normal[0])));
        const float offsetY = Store(Multiply(shift, From(output.normal[1])));
        const Number offsetZ = Multiply(shift, From(output.normal[2]));
        output.position[2] = Store(Add(offsetZ, From(input.center1[2])));
        output.position[0] = Store(Add(From(offsetX), From(input.center1[0])));
        output.position[1] = Store(Add(From(offsetY), From(input.center1[1])));
        output.depth = Store(Subtract(sumRadius, distance));
        return true;
    }
}
