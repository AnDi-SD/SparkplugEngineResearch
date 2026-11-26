#include "spAnimationScalarLinearPc.h"
#include "Analysis/PC/spSphereContactMath.h"

#include <cstring>
#include <limits>

namespace sparkplug::reconstruction
{
    namespace
    {
        namespace math = evidence::pc::sphere_contact_math;
        static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559);

        constexpr std::uint32_t MagnitudeMask = 0x7FFFFFFFu;
        constexpr std::uint32_t MinimumEndpointMagnitude = 0x1F800000u;
        constexpr std::uint32_t MaximumEndpointMagnitude = 0x45800000u;
        constexpr std::uint32_t MinimumFactorMagnitude = 0x2F800000u;
        constexpr std::uint32_t OneBits = 0x3F800000u;

        constexpr bool AdmittedEndpoint(std::uint32_t bits) noexcept
        {
            const auto magnitude = bits & MagnitudeMask;
            return magnitude == 0 || (MinimumEndpointMagnitude <= magnitude && magnitude <= MaximumEndpointMagnitude);
        }

        constexpr bool AdmittedFactor(std::uint32_t bits) noexcept
        {
            return (bits & MagnitudeMask) == 0 || (MinimumFactorMagnitude <= bits && bits <= OneBits);
        }

        math::Number Widen(std::uint32_t bits) noexcept
        {
            float value;
            std::memcpy(&value, &bits, sizeof(value));
            return math::From(value);
        }
    }

    bool TrySampleAnimationScalarLinearPcForAnalysis(
        std::uint32_t leftBits, std::uint32_t rightBits, std::uint32_t factorBits,
        std::uint32_t& outputBits) noexcept
    {
        if (!AdmittedEndpoint(leftBits) || !AdmittedEndpoint(rightBits) || !AdmittedFactor(factorBits))
            return false;

        const auto factor = Widen(factorBits);
        const auto complement = math::Subtract(Widen(OneBits), factor);
        const auto left = math::Multiply(complement, Widen(leftBits));
        const auto right = math::Multiply(factor, Widen(rightBits));
        const auto rounded = math::Store(math::Add(left, right));
        std::memcpy(&outputBits, &rounded, sizeof(outputBits));
        return true;
    }
}
