#include "spMatrix4ProductPc.h"
#include "Analysis/PC/spSphereContactMath.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>

namespace sparkplug::reconstruction
{
    namespace
    {
        namespace math = evidence::pc::sphere_contact_math;
        static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559);

        // For each row-major destination cell, the k values in original x87 accumulation order.
        constexpr unsigned ProductOrder[16][4] = {
            {2, 1, 0, 3}, {1, 2, 0, 3}, {1, 2, 0, 3}, {1, 2, 0, 3},
            {1, 3, 2, 0}, {1, 0, 3, 2}, {1, 0, 3, 2}, {1, 0, 3, 2},
            {2, 1, 3, 0}, {3, 2, 0, 1}, {3, 2, 0, 1}, {3, 2, 0, 1},
            {0, 3, 1, 2}, {2, 1, 3, 0}, {2, 1, 3, 0}, {1, 3, 2, 0},
        };

        bool Admitted(std::uint32_t bits) noexcept
        {
            const auto magnitude = bits & 0x7FFFFFFFu;
            return magnitude == 0 || (0x1F800000u <= magnitude && magnitude <= 0x4F800000u);
        }

        math::Number FromBits(std::uint32_t bits) noexcept
        {
            float value;
            std::memcpy(&value, &bits, sizeof(value));
            return math::From(value);
        }

        std::uint32_t StoreBits(math::Number number) noexcept
        {
            const auto value = math::Store(number);
            std::uint32_t bits;
            std::memcpy(&bits, &value, sizeof(bits));
            return bits;
        }
    }

    bool MultiplyMatrix4PcNearestForAnalysis(const std::array<std::uint32_t, 16>& leftBits,
        const std::array<std::uint32_t, 16>& rightBits,
        std::array<std::uint32_t, 16>& destinationBits) noexcept
    {
        if (&leftBits == &rightBits || &leftBits == &destinationBits || &rightBits == &destinationBits ||
            !std::all_of(leftBits.begin(), leftBits.end(), Admitted) ||
            !std::all_of(rightBits.begin(), rightBits.end(), Admitted))
            return false;

        std::array<std::uint32_t, 16> result{};
        for (std::size_t column = 0; column < 4; ++column)
            for (std::size_t row = 0; row < 4; ++row)
            {
                const auto cell = 4 * row + column;
                const auto& order = ProductOrder[cell];
                const auto term = [&](unsigned k) noexcept
                {
                    return math::Multiply(FromBits(leftBits[4 * row + k]), FromBits(rightBits[4 * k + column]));
                };
                auto sum = term(order[0]);
                for (std::size_t product = 1; product < 4; ++product)
                    sum = math::Add(sum, term(order[product]));
                result[cell] = StoreBits(sum);
            }
        destinationBits = result;
        return true;
    }
}
