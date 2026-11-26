#include "spVector3Normalization.h"
#include "Analysis/PC/spSphereContactMath.h"

#include <cstring>
#include <limits>

namespace sparkplug::reconstruction
{
    namespace
    {
        namespace math = evidence::pc::sphere_contact_math;
        static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559);

        float FloatFromBits(std::uint32_t bits) noexcept
        {
            float value;
            std::memcpy(&value, &bits, sizeof(value));
            return value;
        }

        std::uint32_t FloatBits(float value) noexcept
        {
            std::uint32_t bits;
            std::memcpy(&bits, &value, sizeof(bits));
            return bits;
        }
    }

    bool NormalizeVector3PcNearestForAnalysis(std::array<std::uint32_t, 3>& vectorBits,
        spVector3Length80ForAnalysis& length) noexcept
    {
        for (const auto bits : vectorBits)
        {
            const auto magnitude = bits & 0x7FFFFFFFu;
            if (magnitude != 0 && (magnitude < 0x27800000u || magnitude > 0x45800000u))
                return false;
        }

        const auto x = math::From(FloatFromBits(vectorBits[0]));
        const auto y = math::From(FloatFromBits(vectorBits[1]));
        const auto z = math::From(FloatFromBits(vectorBits[2]));
        const auto squared = math::Add(math::Add(math::Multiply(x, x), math::Multiply(y, y)), math::Multiply(z, z));
        const auto norm = squared.mantissa ? math::SquareRoot(squared) : math::Number{};
        const auto threshold = math::From(FloatFromBits(0x3A83126Fu));
        if (!norm.mantissa || norm.exponent < threshold.exponent ||
            (norm.exponent == threshold.exponent && norm.mantissa <= threshold.mantissa))
        {
            vectorBits = {};
            length = {};
            return true;
        }

        const auto reciprocal = math::Reciprocal(norm);
        vectorBits = {FloatBits(math::Store(math::Multiply(x, reciprocal))),
                      FloatBits(math::Store(math::Multiply(y, reciprocal))),
                      FloatBits(math::Store(math::Multiply(z, reciprocal)))};
        length = {norm.mantissa, static_cast<std::uint16_t>(norm.exponent + 63 + 0x3FFF)};
        return true;
    }
}
