#include "spBoundingVolumeSizePc.h"
#include "Analysis/PC/spSphereContactMath.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace sparkplug::reconstruction
{
    namespace
    {
        namespace math = evidence::pc::sphere_contact_math;
        static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559);

        bool Finite(std::uint32_t bits) noexcept
        {
            return (bits & 0x7F800000u) != 0x7F800000u;
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

    bool ApplyBoundingVolumeSizePcNearestForAnalysis(const std::array<std::uint32_t, 3>& fullSizeBits,
        spBoundingVolumeSizePcForAnalysis& target) noexcept
    {
        if (&fullSizeBits == &target.fullSizeBits || !std::all_of(fullSizeBits.begin(), fullSizeBits.end(), Finite))
            return false;

        const auto half = math::From(0.5F);
        const auto squaredHalf = [half](std::uint32_t bits) noexcept
        {
            const auto extent = math::Multiply(FromBits(bits), half);
            return math::Multiply(extent, extent);
        };
        const auto z = squaredHalf(fullSizeBits[2]);
        const auto y = squaredHalf(fullSizeBits[1]);
        const auto x = squaredHalf(fullSizeBits[0]);
        const auto sum = math::Add(math::Add(z, y), x);
        const auto radius = sum.mantissa ? math::SquareRoot(sum) : sum;
        target.fullSizeBits = fullSizeBits;
        target.radiusBits = StoreBits(radius);
        return true;
    }
}
