#include "spPlaneFromThree.h"
#include "spVector3Normalization.h"
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
            return magnitude == 0 || (0x2B800000u <= magnitude && magnitude <= 0x41800000u);
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

    bool BuildPlaneFromThreePcNearestForAnalysis(const std::array<std::array<std::uint32_t, 3>, 3>& points,
        std::array<std::uint32_t, 4>& plane) noexcept
    {
        for (const auto& point : points)
            if (!std::all_of(point.begin(), point.end(), Admitted))
                return false;

        std::array<math::Number, 3> first{}, edge1{}, edge2{};
        for (std::size_t lane = 0; lane < first.size(); ++lane)
        {
            first[lane] = FromBits(points[0][lane]);
            edge2[lane] = FromBits(StoreBits(math::Subtract(FromBits(points[2][lane]), first[lane])));
            edge1[lane] = FromBits(StoreBits(math::Subtract(FromBits(points[1][lane]), first[lane])));
        }

        const auto cross = [&](std::size_t a, std::size_t b) noexcept
        {
            return StoreBits(math::Subtract(math::Multiply(edge1[a], edge2[b]), math::Multiply(edge1[b], edge2[a])));
        };
        std::array<std::uint32_t, 3> normal{cross(1, 2), cross(2, 0), cross(0, 1)};
        spVector3Length80ForAnalysis length{};
        if (!NormalizeVector3PcNearestForAnalysis(normal, length))
            return false;

        const auto x = math::Multiply(first[0], FromBits(normal[0]));
        const auto y = math::Multiply(first[1], FromBits(normal[1]));
        const auto z = math::Multiply(first[2], FromBits(normal[2]));
        const auto distance = math::Add(math::Add(x, y), z);
        plane = {normal[0], normal[1], normal[2], StoreBits(distance)};
        return true;
    }
}
