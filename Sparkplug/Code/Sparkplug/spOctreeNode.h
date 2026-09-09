#pragma once

// Inferred declaration/implementation paths, not an original source string.
// Original PC class/RTTI/8-child constructor and query40/44/48/50/54/5C slice.
// Ray query4C, registrations, debug/query tail and full Scene wiring
// remain open. Native uninitialized pivot is represented by an explicit gap.
#include "spPartitionNode.h"
#include "Analysis/PC/spVisibilityMath.h"

namespace sparkplug::reconstruction
{
    class spOctreeNode : public spPartitionNode
    {
      public:
        static constexpr spClassID ClassID = 0x21A70829;
        using PlaneSet = evidence::pc::visibility_math::PlaneSet;
        using Sphere = evidence::pc::visibility_math::Sphere;
        spOctreeNode();
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;

        // Explicit host setup: finite ordered bounds required. Native reader
        // writes fields directly without this validation. No midpoint rewrite.
        [[nodiscard]] bool SetGeometryForAnalysis(const Vector3& pivot, const Vector3& mins,
                                                  const Vector3& maxs) noexcept;
        [[nodiscard]] bool HasGeometryForAnalysis() const noexcept;
        [[nodiscard]] const Vector3& GetPivotForAnalysis() const noexcept{return pivot_;}
        [[nodiscard]] const Vector3& GetMinsForAnalysis() const noexcept{return mins_;}
        [[nodiscard]] const Vector3& GetMaxsForAnalysis() const noexcept{return maxs_;}
        [[nodiscard]] spPartitionNode* FindLeafForAnalysis(
            const Vector3& point, bool stopAtZone = true) noexcept override;
        [[nodiscard]] static std::uint32_t OctantForAnalysis(const Vector3& point,
                                                             const Vector3& pivot) noexcept;
        [[nodiscard]] std::uint8_t PointMaskForAnalysis(const Vector3& point) const noexcept;
        [[nodiscard]] std::uint8_t SphereMaskForAnalysis(const Sphere& sphere) const noexcept;
        [[nodiscard]] static std::uint32_t TraversalOrderForAnalysis(std::uint32_t octant) noexcept;
        // Packed return: low4=count, then3bits per child from bit4.
        [[nodiscard]] std::uint32_t VisibleChildrenForAnalysis(
            const PlaneSet& planes, const Vector3& camera) const noexcept;
        void ReducePlanesForChildForAnalysis(std::uint32_t child, PlaneSet& planes) const noexcept;

      private:
        friend class spOctreeNodeSerializer;
        [[nodiscard]] double BoxDistanceForAnalysis(
            std::uint32_t child, const evidence::pc::visibility_math::Plane& plane,
            bool positiveVertex) const noexcept;
        Vector3 pivot_{}; // not usable until explicit geometry input
        Vector3 mins_{};
        Vector3 maxs_{};
        bool geometryKnown_ = false;
    };
} // namespace sparkplug::reconstruction
