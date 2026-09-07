#pragma once

// Analytical names for original PC4902D0/4903E0/490500. The original plane-set
// class name is not recovered; this is not a newly invented RTTI class.
#include <array>
#include <cstdint>
#include <vector>

namespace sparkplug::evidence::pc::visibility_math
{
    using Sphere = std::array<float, 4>;
    struct Plane
    {
        std::array<float, 4> equation{}; // n.x,n.y,n.z,d: dot(n,p)-d
        bool enabled = true;
    };
    struct PlaneSet
    {
        std::vector<Plane> planes;
        std::uint32_t activeCount = 0;
    };
    enum class SphereClass : std::uint32_t
    {
        Inside = 1,
        Outside = 2,
        Intersecting = 3
    };

    inline double SignedDistance(const Plane& plane, const Sphere& sphere) noexcept
    {
        const auto& n = plane.equation;
        return static_cast<double>(n[1]) * sphere[1] + static_cast<double>(n[0]) * sphere[0] +
               static_cast<double>(sphere[2]) * n[2] - n[3];
    }
    inline bool IsOutside(const PlaneSet& set, const Sphere& sphere) noexcept
    {
        // Unlike Classify, 4902D0 tests the cached count before the vector.
        if (!set.activeCount)
            return false;
        for (const auto& plane : set.planes)
            if (plane.enabled && SignedDistance(plane, sphere) < -static_cast<double>(sphere[3]))
                return true;
        return false;
    }
    inline bool IsFullyInside(const PlaneSet& set, const Sphere& sphere) noexcept
    {
        // 4903E0: empty cached set accepts everything. Both tangent sides
        // and unordered comparisons fail; retain the first test for a
        // negative radius too, without inventing input normalization.
        if (!set.activeCount)
            return true;
        for (const auto& plane : set.planes)
        {
            if (!plane.enabled)
                continue;
            const double distance = SignedDistance(plane, sphere);
            if (distance < -static_cast<double>(sphere[3]) ||
                !(distance > static_cast<double>(sphere[3])))
                return false;
        }
        return true;
    }
    inline SphereClass Classify(const PlaneSet& set, const Sphere& sphere) noexcept
    {
        auto result = SphereClass::Inside;
        // 490500 deliberately does NOT test activeCount. Native unordered
        //  comparisons take the intersecting branch, not the outside branch.
        for (const auto& plane : set.planes)
        {
            if (!plane.enabled)
                continue;
            const double distance = SignedDistance(plane, sphere);
            if (distance < -static_cast<double>(sphere[3]))
                return SphereClass::Outside;
            if (!(distance > static_cast<double>(sphere[3])))
                result = SphereClass::Intersecting;
        }
        return result;
    }
} // namespace sparkplug::evidence::pc::visibility_math
