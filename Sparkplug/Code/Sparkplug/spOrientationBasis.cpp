#include "spOrientationBasis.h"
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
        using Vector = std::array<std::uint32_t, 3>;
        static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559);

        bool Admitted(std::uint32_t bits) noexcept
        {
            const auto absolute = bits & 0x7FFFFFFFu;
            return absolute == 0 || (0x2F800000u <= absolute && absolute <= 0x45800000u);
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

    bool BuildOrientationBasisPcNearestForAnalysis(const std::array<std::uint32_t, 3>& forward,
        const std::array<std::uint32_t, 3>& up, std::array<std::uint32_t, 9>& basis) noexcept
    {
        if (!std::all_of(forward.begin(), forward.end(), Admitted) || !std::all_of(up.begin(), up.end(), Admitted))
            return false;

        const std::array<math::Number, 3> f{FromBits(forward[0]), FromBits(forward[1]), FromBits(forward[2])};
        const std::array<math::Number, 3> u{FromBits(up[0]), FromBits(up[1]), FromBits(up[2])};
        const auto threshold = FromBits(0x3A83126Fu);
        const auto differs = [&](std::size_t lane) noexcept
        {
            const auto delta = math::Subtract(f[lane], u[lane]);
            return delta.mantissa && (delta.exponent > threshold.exponent ||
                (delta.exponent == threshold.exponent && delta.mantissa > threshold.mantissa));
        };
        if (!differs(0) && !differs(1) && !differs(2))
            return false;

        const auto cross = [&](std::size_t first, std::size_t second) noexcept
        {
            return StoreBits(math::Subtract(math::Multiply(u[first], f[second]), math::Multiply(u[second], f[first])));
        };
        basis = {cross(1, 2), cross(2, 0), cross(0, 1), up[0], up[1], up[2], forward[0], forward[1], forward[2]};
        return true;
    }
}
