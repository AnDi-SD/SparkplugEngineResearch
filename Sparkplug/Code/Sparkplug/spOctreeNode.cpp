#include "spOctreeNode.h"
#include "Analysis/PC/spSphereContactMath.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559);

        constexpr float Epsilon = 0.001F;
        constexpr std::array<std::uint32_t, 8> Orders{0xFAB888, 0xDE2AC1, 0xBE1CC2, 0x9A8E8B,
                                                      0x6571AC, 0x4C633D, 0x21D53E, 0x08C777};
        std::unique_ptr<spBaseObject> CreateOctreeNode()
        {
            return std::make_unique<spOctreeNode>();
        }
        const spRTTIRecord Record{spOctreeNode::ClassID, spPartitionNode::ClassID,
                                  "spOctreeNode",        &spPartitionNode::StaticRTTI(),
                                  &CreateOctreeNode,     nullptr};
        const bool Registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);

        bool IsAdmittedRayWord(std::uint32_t word) noexcept
        {
            const auto magnitude = word & 0x7FFFFFFFu;
            return magnitude == 0 || (magnitude >= 0x33800000u && magnitude <= 0x45800000u);
        }

        float RayFloatFromWord(std::uint32_t word) noexcept
        {
            float value;
            std::memcpy(&value, &word, sizeof(value));
            return value;
        }

        std::uint32_t RayFloatWord(float value) noexcept
        {
            std::uint32_t word;
            std::memcpy(&word, &value, sizeof(word));
            return word;
        }
    } // namespace
    spOctreeNode::spOctreeNode() : spPartitionNode(8)
    {
    }
    const spRTTIRecord& spOctreeNode::StaticRTTI() noexcept
    {
        (void)Registered;
        return Record;
    }
    const spRTTIRecord& spOctreeNode::vfunc_18() const noexcept
    {
        return Record;
    }
    std::unique_ptr<spBaseObject> spOctreeNode::vfunc_10(spCloneManager& manager) const
    {
        // PC41B100 uses Base-only copy, not geometry or children copy.
        auto clone = std::make_unique<spOctreeNode>();
        manager.RegisterClone(*this, *clone);
        return spBaseObject::vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }
    bool spOctreeNode::SetGeometryForAnalysis(const Vector3& pivot, const Vector3& mins,
                                              const Vector3& maxs) noexcept
    {
        for (unsigned i = 0; i < 3; ++i)
            if (!std::isfinite(pivot[i]) || !std::isfinite(mins[i]) || !std::isfinite(maxs[i]) ||
                mins[i] > pivot[i] || pivot[i] > maxs[i])
                return false; // host-only guard; keep previous valid state
        pivot_ = pivot;
        mins_ = mins;
        maxs_ = maxs;
        geometryKnown_ = true;
        return true;
    }
    bool spOctreeNode::HasGeometryForAnalysis() const noexcept
    {
        return geometryKnown_;
    }
    std::uint32_t spOctreeNode::OctantForAnalysis(const Vector3& p, const Vector3& pivot) noexcept
    {
        return (p[0] > pivot[0] ? 1u : 0u) | (p[1] > pivot[1] ? 2u : 0u) |
               (p[2] > pivot[2] ? 4u : 0u); // ties and unordered choose low
    }

    std::uint32_t spOctreeNode::UpdateRayCandidatesForAnalysis(
        RayCandidateCacheForAnalysis& cache, const RayInputForAnalysis& ray,
        const RayCandidateForAnalysis** borrowedFirst) noexcept
    {
        namespace math = evidence::pc::sphere_contact_math;
        Vector3 pivot{}, origin{}, direction{};
        for (std::size_t axis = 0; axis < pivot.size(); ++axis)
        {
            if (!IsAdmittedRayWord(cache.pivotBits[axis]) || !IsAdmittedRayWord(ray.originBits[axis])
                || !IsAdmittedRayWord(ray.directionBits[axis]))
                return 0;
            pivot[axis] = RayFloatFromWord(cache.pivotBits[axis]);
            origin[axis] = RayFloatFromWord(ray.originBits[axis]);
            direction[axis] = RayFloatFromWord(ray.directionBits[axis]);
        }

        if (borrowedFirst != nullptr)
            *borrowedFirst = cache.scratch.data();
        const auto first = OctantForAnalysis(origin, pivot);
        cache.scratch[0] = {first, 0};
        std::uint32_t count = 1;
        std::uint32_t visited = 1u << first;

        constexpr std::uint32_t parallelThreshold = 0x3727C5ACu;
        const auto sampleBias = math::From(RayFloatFromWord(0x3A83126Fu));
        for (std::size_t axis = 0; axis < direction.size(); ++axis)
        {
            const auto magnitude = ray.directionBits[axis] & 0x7FFFFFFFu;
            if (magnitude <= parallelThreshold)
                continue;

            const auto inverse = math::Reciprocal(math::From(RayFloatFromWord(magnitude)));
            auto signedInverse = inverse;
            signedInverse.negative = bool(ray.directionBits[axis] >> 31);
            const auto offset = math::Subtract(math::From(pivot[axis]), math::From(origin[axis]));
            const auto time = math::Multiply(offset, signedInverse);
            const auto storedTime = math::Store(time);
            if (time.negative && time.mantissa != 0)
                continue;

            // The sample bias uses the stored time and the extended absolute reciprocal.
            const auto sampleTime = math::Add(math::Multiply(inverse, sampleBias), math::From(storedTime));
            Vector3 sample{};
            for (std::size_t coordinate = 0; coordinate < sample.size(); ++coordinate)
            {
                auto product = math::Multiply(sampleTime, math::From(direction[coordinate]));
                if (coordinate != 0)
                    product = math::From(math::Store(product)); // Native Y/Z spill before adding origin.
                sample[coordinate] = math::Store(math::Add(product, math::From(origin[coordinate])));
            }

            const auto child = OctantForAnalysis(sample, pivot);
            auto& candidate = cache.scratch[count];
            candidate.octant = child; // This write also happens for a duplicate.
            const auto bit = 1u << child;
            if ((visited & bit) != 0)
                continue;
            candidate.timeBits = RayFloatWord(storedTime);
            visited |= bit;
            ++count;
        }

        // Pair zero stays first. Sort the subsequent accepted pairs stably by stored time.
        for (auto end = count - 1; end > 1; --end)
        {
            bool swapped = false;
            for (std::uint32_t index = 1; index < end; ++index)
            {
                auto& left = cache.scratch[index];
                auto& right = cache.scratch[index + 1];
                if (RayFloatFromWord(left.timeBits) > RayFloatFromWord(right.timeBits))
                {
                    std::swap(left, right);
                    swapped = true;
                }
            }
            if (!swapped)
                break;
        }
        return count;
    }
    spPartitionNode* spOctreeNode::FindLeafForAnalysis(const Vector3& point,
                                                       bool stopAtZone) noexcept
    {
        if (stopAtZone && HasZoneForAnalysis())
            return this;
        if (!geometryKnown_)
            return nullptr; // host-only: native reads unspecified pivot bytes
        auto* child = GetChildForAnalysis(OctantForAnalysis(point, pivot_));
        // Missing-child guard is host-only; native dereferences the slot.
        return child ? child->FindLeafForAnalysis(point, stopAtZone) : nullptr;
    }
    std::uint8_t spOctreeNode::PointMaskForAnalysis(const Vector3& point) const noexcept
    {
        if (!geometryKnown_)
            return 0;
        std::uint8_t mask = 255;
        constexpr std::uint8_t high[3]{0xAA, 0xCC, 0xF0};
        for (unsigned axis = 0; axis < 3; ++axis)
        {
            if (static_cast<double>(pivot_[axis]) + Epsilon < point[axis])
                mask &= high[axis];
            if (static_cast<double>(pivot_[axis]) - Epsilon > point[axis])
                mask &= static_cast<std::uint8_t>(~high[axis]);
        }
        return mask;
    }
    std::uint8_t spOctreeNode::SphereMaskForAnalysis(const Sphere& sphere) const noexcept
    {
        if (!geometryKnown_)
            return 0;
        const double dx = static_cast<double>(sphere[0]) - pivot_[0];
        const double dy = static_cast<double>(sphere[1]) - pivot_[1];
        const float dz = static_cast<float>(static_cast<double>(sphere[2]) - pivot_[2]);
        // Preserve native x87 store points; not a blanket exact80-bit claim.
        const float x2 = static_cast<float>(dx * dx);
        const double y2 = dy * dy;
        const float z2 = static_cast<float>(static_cast<double>(dz) * dz);
        const float r2 = static_cast<float>(static_cast<double>(sphere[3]) * sphere[3]);
        if (x2 + (y2 + z2) <= r2)
            return 255;
        const auto octant = OctantForAnalysis({sphere[0], sphere[1], sphere[2]}, pivot_);
        std::uint8_t mask = 0;
        if (x2 + y2 <= r2)
            mask |= (octant & 4u) ? 0xF0 : 0x0F;
        if (static_cast<double>(x2) + z2 <= r2)
            mask |= (octant & 2u) ? 0xCC : 0x33;
        if (y2 + z2 <= r2)
            mask |= (octant & 1u) ? 0xAA : 0x55;
        // Exact original optimization: if any axis-line mask matched,
        // individual plane crossings are skipped. Do not replace with a
        // mathematically broader sphere-vs-eight-orthants algorithm.
        if (!mask)
        {
            if (x2 <= r2)
                mask |= static_cast<std::uint8_t>(1u << (octant ^ 1u));
            if (y2 <= r2)
                mask |= static_cast<std::uint8_t>(1u << (octant ^ 2u));
            if (z2 <= r2)
                mask |= static_cast<std::uint8_t>(1u << (octant ^ 4u));
        }
        return mask | static_cast<std::uint8_t>(1u << octant);
    }
    std::uint32_t spOctreeNode::TraversalOrderForAnalysis(std::uint32_t octant) noexcept
    {
        return octant < 8 ? Orders[octant] : 0; // bounded host accessor
    }
    double spOctreeNode::BoxDistanceForAnalysis(std::uint32_t child,
                                                const evidence::pc::visibility_math::Plane& plane,
                                                bool positiveVertex) const noexcept
    {
        double product[3];
        for (unsigned axis = 0; axis < 3; ++axis)
        {
            const float lo = (child & (1u << axis)) ? pivot_[axis] : mins_[axis];
            const float hi = (child & (1u << axis)) ? maxs_[axis] : pivot_[axis];
            const bool useHigh =
                positiveVertex ? plane.equation[axis] > 0 : plane.equation[axis] < 0;
            product[axis] = static_cast<double>(useHigh ? hi : lo) * plane.equation[axis];
        }
        // Query50 sums z+y+x, query54 sums x+z+y in native x87 order.
        return (positiveVertex ? (product[2] + product[1]) + product[0]
                               : (product[0] + product[2]) + product[1]) -
               plane.equation[3];
    }
    std::uint32_t spOctreeNode::VisibleChildrenForAnalysis(const PlaneSet& planes,
                                                           const Vector3& camera) const noexcept
    {
        if (!geometryKnown_)
            return 0;
        std::uint32_t mask = 255;
        for (unsigned child = 0; child < 8; ++child)
            for (const auto& plane : planes.planes)
                if (plane.enabled && BoxDistanceForAnalysis(child, plane, true) < -Epsilon)
                {
                    mask &= ~(1u << child);
                    break;
                }
        const auto order = TraversalOrderForAnalysis(OctantForAnalysis(camera, pivot_));
        std::uint32_t result = 0, count = 0;
        for (unsigned i = 0; i < 8; ++i)
        {
            const auto child = (order >> (i * 3)) & 7;
            if (mask & (1u << child))
            {
                result |= child << (4 + count * 3);
                ++count;
            }
        }
        return result | count;
    }
    void spOctreeNode::ReducePlanesForChildForAnalysis(std::uint32_t child,
                                                       PlaneSet& planes) const noexcept
    {
        if (!geometryKnown_ || child >= 8)
            return; // host-only bounds guard
        for (auto& plane : planes.planes)
            if (plane.enabled && BoxDistanceForAnalysis(child, plane, false) > Epsilon)
            {
                plane.enabled = false;
                --planes.activeCount; // original unsigned cached count, no normalization
            }
    }
} // namespace sparkplug::reconstruction
