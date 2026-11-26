#include "wxGiantBoulderTrigger.h"

#include "../Analysis/PC/wxGiantBoulderTriggerAbi.h"
#include "Analysis/PC/spSphereContactMath.h"

#include <cstring>
#include <limits>

namespace winx::reconstruction
{
    namespace
    {
        namespace math = sparkplug::evidence::pc::sphere_contact_math;

        static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559);

        bool AdmitPoint(const wxGiantBoulderTrigger::PointForAnalysis& point) noexcept
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

    bool wxGiantBoulderTrigger::TryEvaluatePcProximityForAnalysis(const PointForAnalysis& first,
        const PointForAnalysis& second, bool& inside) noexcept
    {
        if (!AdmitPoint(first) || !AdmitPoint(second))
            return false;

        const auto x = math::Subtract(math::From(second[0]), math::From(first[0]));
        const auto z = math::Subtract(math::From(second[2]), math::From(first[2]));
        // Force-horizontal zeroes both Y copies in the native helper.
        const auto square = math::Add(math::Multiply(z, z), math::Multiply(x, x));
        const auto limit = math::From(evidence::pc::GiantBoulderTriggerSquaredLimit);
        inside = math::nearest::Compare(square, limit) < 0;
        return true;
    }
} // namespace winx::reconstruction
