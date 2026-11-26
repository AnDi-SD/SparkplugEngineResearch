#include "spNodeWorldPoint.h"
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
            return magnitude == 0 || (0x37800000u <= magnitude && magnitude <= 0x45800000u);
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

    bool TransformNodeWorldPointPcNearestForAnalysis(const spNodeWorldPointCacheForAnalysis& cache,
        const std::array<std::uint32_t, 3>& pointBits, std::array<std::uint32_t, 3>& outputBits) noexcept
    {
        if (&outputBits == &cache.positionBits || &outputBits == &cache.scaleBits)
            return false;
        const auto inProfile = [](const auto& words) noexcept
        {
            return std::all_of(words.begin(), words.end(), Admitted);
        };
        if (!inProfile(pointBits) || !inProfile(cache.positionBits) ||
            !inProfile(cache.scaleBits) || !inProfile(cache.orientationBits))
            return false;

        std::array<math::Number, 3> scaled;
        for (std::size_t lane = 0; lane < scaled.size(); ++lane)
            scaled[lane] = math::Multiply(FromBits(pointBits[lane]), FromBits(cache.scaleBits[lane]));

        const auto contribution = [&](std::size_t row, std::size_t column) noexcept
        {
            return math::Multiply(scaled[row], FromBits(cache.orientationBits[3*row + column]));
        };
        const auto rotatedX = math::Add(math::Add(contribution(2, 0), contribution(1, 0)), contribution(0, 0));
        const auto rotatedY = math::Add(math::Add(contribution(2, 1), contribution(0, 1)), contribution(1, 1));
        const auto rotatedZ = math::Add(math::Add(contribution(2, 2), contribution(0, 2)), contribution(1, 2));
        const std::array<float, 3> storedRotation{math::Store(rotatedX), math::Store(rotatedY), math::Store(rotatedZ)};
        for (std::size_t lane = 0; lane < storedRotation.size(); ++lane)
            outputBits[lane] = StoreBits(math::Add(math::From(storedRotation[lane]), FromBits(cache.positionBits[lane])));
        return true;
    }
}
