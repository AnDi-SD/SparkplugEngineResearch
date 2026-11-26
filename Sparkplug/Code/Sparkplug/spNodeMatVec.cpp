#include "spNodeMatVec.h"
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

        bool Admitted(std::uint32_t bits) noexcept
        {
            const auto magnitude = bits & 0x7FFFFFFFu;
            return magnitude == 0 || (0x2F800000u <= magnitude && magnitude <= 0x4D000000u);
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

    bool TransformNodeVectorPcNearestForAnalysis(const std::array<std::uint32_t, 9>& matrixBits,
        const std::array<std::uint32_t, 3>& inputBits, std::array<std::uint32_t, 3>& outputBits) noexcept
    {
        if (&inputBits == &outputBits || !std::all_of(matrixBits.begin(), matrixBits.end(), Admitted) ||
            !std::all_of(inputBits.begin(), inputBits.end(), Admitted))
            return false;

        const std::array<math::Number, 3> point{FromBits(inputBits[0]), FromBits(inputBits[1]), FromBits(inputBits[2])};
        const auto term = [&](std::size_t row, std::size_t column) noexcept
        {
            return math::Multiply(point[row], FromBits(matrixBits[3*row + column]));
        };
        constexpr std::array<std::size_t, 3> evaluationOrder{2, 1, 0};
        std::array<math::Number, 3> result{};
        for (const auto column : evaluationOrder)
            result[column] = math::Add(math::Add(term(2, column), term(1, column)), term(0, column));
        outputBits = {StoreBits(result[0]), StoreBits(result[1]), StoreBits(result[2])};
        return true;
    }
}
