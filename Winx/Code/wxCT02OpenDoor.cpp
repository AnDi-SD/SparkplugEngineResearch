#include "wxCT02OpenDoor.h"

#include "../Analysis/PC/wxCT02OpenDoorAbi.h"
#include "Analysis/PC/spSphereContactMath.h"

#include <cstring>
#include <limits>

namespace winx::reconstruction
{
    namespace
    {
        namespace math = sparkplug::evidence::pc::sphere_contact_math;

        static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559);

        bool AdmitPoint(const wxCT02OpenDoor::PointForAnalysis& point) noexcept
        {
            for (const auto& coordinate : point)
            {
                std::uint32_t bits;
                std::memcpy(&bits, &coordinate, sizeof(bits));
                const auto magnitude = bits & 0x7FFFFFFFu;
                if (magnitude != 0 && (magnitude < 0x00800000u || magnitude > 0x46800000u))
                    return false;
            }
            return true;
        }
    }

    bool wxCT02OpenDoor::TryEvaluatePcProximityForAnalysis(const PointForAnalysis& first,
        const PointForAnalysis& second, bool& inside) noexcept
    {
        if (!AdmitPoint(first) || !AdmitPoint(second))
            return false;

        auto y = math::Subtract(math::From(second[1]), math::From(first[1]));
        auto height = y;
        height.negative = false;
        if (math::nearest::Compare(height, math::From(evidence::pc::CT02OpenDoorHeightTolerance)) < 0)
            y = {};

        const auto x = math::Subtract(math::From(second[0]), math::From(first[0]));
        const auto z = math::Subtract(math::From(second[2]), math::From(first[2]));
        const auto xz = math::Add(math::Multiply(z, z), math::Multiply(x, x));
        const auto square = math::Add(xz, math::Multiply(y, y));
        const auto limit = math::From(evidence::pc::CT02OpenDoorSquaredLimit);
        inside = math::nearest::Compare(square, limit) < 0;
        return true;
    }
} // namespace winx::reconstruction
