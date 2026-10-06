#pragma once

// PC601B20 and its464760/464C80 math. Finite inputs, default x87 rounding.
// The translation X path retains the exact float product through an x87
// 64-significand-bit sum; Y/Z spill that product to float first. The narrow
// integer helper below models those stores, not a general x87 environment.
// Rotation uses host sin/cos for FSIN/FCOS; bounded native cases qualify it,
// without a claim for every angle or x87 transcendental input.
#include "spFloat80TowardZero.h"
#include <array>
#include <cmath>
#include <cstring>

namespace sparkplug::evidence::pc::transform_const_math
{
    using Wide = float80_rtz::Wide;
    using Number = float80_rtz::Number;

    inline Number NormalizeNearest(const Wide& value, int exponent, bool negative)
    {
        const int shift = int(value.Highest()) - 63;
        std::uint64_t mantissa = shift > 0 ? value.Extract(unsigned(shift)) :
            value.Extract(0) << unsigned(-shift);
        if (shift > 0)
        {
            const unsigned guardIndex = unsigned(shift - 1);
            const bool guard = (value.words[guardIndex / 32] >> (guardIndex % 32)) & 1;
            bool sticky = false;
            for (unsigned index = 0; index < guardIndex; ++index)
                sticky |= bool((value.words[index / 32] >> (index % 32)) & 1);
            if (guard && (sticky || (mantissa & 1)))
            {
                if (++mantissa == 0)
                    return {std::uint64_t(1) << 63, exponent + shift + 1, negative};
            }
        }
        return {mantissa, exponent + shift, negative};
    }

    inline float StoreNearest(Number value)
    {
        std::uint32_t bits = value.negative ? 0x80000000U : 0;
        if (value.mantissa)
        {
            int field = value.exponent + 63 + 127;
            const int shift = 40 + (field <= 0 ? 1 - field : 0);
            std::uint64_t significand = shift < 64 ? value.mantissa >> shift : 0;
            const bool guard = shift <= 64 && ((value.mantissa >> (shift - 1)) & 1);
            const bool sticky = shift > 64 ? value.mantissa != 0 :
                (value.mantissa & ((std::uint64_t(1) << (shift - 1)) - 1)) != 0;
            if (guard && (sticky || (significand & 1)))
                ++significand;
            if (field <= 0)
            {
                if (significand >= 0x800000)
                    field = 1;
                else
                    field = 0;
            }
            else if (significand >= 0x1000000)
                ++field;
            bits |= field >= 255 ? 0x7F800000U :
                (std::uint32_t(field) << 23) | (std::uint32_t(significand) & 0x7FFFFFU);
        }
        float result;
        std::memcpy(&result, &bits, sizeof(result));
        return result;
    }

    inline Number AddNearest(Number first, Number second)
    {
        if (!first.mantissa && !second.mantissa)
            return {0, 0, first.negative && second.negative};
        if (!first.mantissa)
            return second;
        if (!second.mantissa)
            return first;
        if (first.exponent < second.exponent ||
            (first.exponent == second.exponent && first.mantissa < second.mantissa))
        {
            const auto saved = first;
            first = second;
            second = saved;
        }
        const unsigned difference = unsigned(first.exponent - second.exponent);
        if (difference > 64)
        {
            // Just below a power of two, extended spacing is half the spacing
            // above it. An opposite term at gap65 can cross that lower tie.
            if (difference == 65 && first.negative != second.negative &&
                first.mantissa == (std::uint64_t(1) << 63) &&
                second.mantissa > (std::uint64_t(1) << 63))
                return {~std::uint64_t(0), first.exponent - 1, first.negative};
            return first;
        }
        auto wide = Wide::Shifted(first.mantissa, difference);
        const auto small = Wide::Shifted(second.mantissa, 0);
        if (first.negative == second.negative)
            wide.Add(small);
        else
            wide.Subtract(small);
        // A zero cancellation has positive sign under round-to-nearest.
        if (wide.words == std::array<std::uint32_t, 4>{})
            return {};
        return NormalizeNearest(wide, second.exponent, first.negative);
    }

    inline float AddProduct(float baseline, float time, float rate, bool spillProduct)
    {
        auto product = float80_rtz::MultiplyFloat(float80_rtz::FromFloat(time), rate);
        if (spillProduct)
        {
            const float spilled = StoreNearest(product);
            if (!std::isfinite(spilled))
                return spilled;
            product = float80_rtz::FromFloat(spilled);
        }
        return StoreNearest(AddNearest(product, float80_rtz::FromFloat(baseline)));
    }

    inline std::array<float, 4> Rotate(float angle, const std::array<float, 3>& axis,
                                       const std::array<float, 4>& incoming)
    {
        const double half = double(angle) * 0.5;
        const double sine = std::sin(half);
        const std::array<float, 4> increment{
            float(sine * axis[0]), float(sine * axis[1]), float(sine * axis[2]),
            float(std::cos(half))};
        const double x = increment[0], y = increment[1], z = increment[2], w = increment[3];
        const double u = incoming[0], v = incoming[1], s = incoming[2], t = incoming[3];
        // Preserve464760's operation order; increment is the left operand.
        return {float(((w * u + y * s) + t * x) - v * z),
                float(((t * y + u * z) + v * w) - x * s),
                float(((t * z + w * s) + v * x) - y * u),
                float(((t * w - u * x) - v * y) - s * z)};
    }
}
