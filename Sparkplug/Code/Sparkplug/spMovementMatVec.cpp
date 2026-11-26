#include "spMovementMatVec.h"
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
            return magnitude == 0 || (0x2F800000u <= magnitude && magnitude <= 0x45800000u);
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

    bool TransformMovementVectorPcNearestForAnalysis(const std::array<std::uint32_t, 9>& matrixBits,
        const std::array<std::uint32_t, 3>& inputBits, std::array<std::uint32_t, 3>& outputBits) noexcept
    {
        if (!std::all_of(matrixBits.begin(), matrixBits.end(), Admitted) ||
            !std::all_of(inputBits.begin(), inputBits.end(), Admitted))
            return false;

        const std::array<math::Number, 3> point{FromBits(inputBits[0]), FromBits(inputBits[1]), FromBits(inputBits[2])};
        const auto term = [&](std::size_t row, std::size_t column) noexcept
        {
            return math::Multiply(point[row], FromBits(matrixBits[3*row + column]));
        };
        const auto x = math::Add(math::Add(term(1, 0), term(2, 0)), term(0, 0));
        const auto y = math::Add(math::Add(term(0, 1), term(1, 1)), term(2, 1));
        const auto z = math::Add(math::Add(term(0, 2), term(1, 2)), term(2, 2));
        outputBits = {StoreBits(x), StoreBits(y), StoreBits(z)};
        return true;
    }
}
