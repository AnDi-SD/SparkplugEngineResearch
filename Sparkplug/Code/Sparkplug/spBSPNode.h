#pragma once

// Original PC class/RTTI. Source paths inferred, not original debug symbols.
// Partial two-child/plane/polygon/query slice; no Scene/registration/debug
// implementation or exact host ABI. GetPlane/GetPolygonVertex/Count names
// survive in original serializer diagnostics, other names are analytical.
#include "spPartitionNode.h"
#include "Analysis/PC/spVisibilityMath.h"

#include <array>

namespace sparkplug::reconstruction
{
    class spBSPNode : public spPartitionNode
    {
      public:
        static constexpr spClassID ClassID = 0x7362AB22;
        using Plane = std::array<float, 4>;
        using PlaneSet = evidence::pc::visibility_math::PlaneSet;
        using Sphere = evidence::pc::visibility_math::Sphere;
        struct RayCandidateForAnalysis
        {
            std::uint32_t child;
            float parameter;
        };
        spBSPNode();
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        // Native44CDA0 raw copy, no normalization/finite rejection.
        void SetPlaneForAnalysis(const Plane& plane) noexcept;
        [[nodiscard]] const Plane& GetPlane() const noexcept;
        [[nodiscard]] bool HasPlaneForAnalysis() const noexcept;
        // Native480210 independently copies vertices, never derives plane.
        // Host4096 count bound/alias/allocation safety, not native error policy.
        [[nodiscard]] bool SetPolygonForAnalysis(const std::vector<Vector3>& points);
        [[nodiscard]] std::uint32_t GetPolygonVertexCount() const noexcept;
        [[nodiscard]] const Vector3& GetPolygonVertex(std::size_t index) const;
        [[nodiscard]] spPartitionNode* FindLeafForAnalysis(
            const Vector3& point, bool stopAtZone = true) noexcept override;
        [[nodiscard]] std::uint8_t PointMaskForAnalysis(const Vector3& point) const noexcept;
        [[nodiscard]] std::uint8_t SphereMaskForAnalysis(const Sphere& sphere) const noexcept;
        [[nodiscard]] static std::uint32_t TraversalOrderForAnalysis(std::uint32_t side) noexcept;
        [[nodiscard]] std::uint32_t VisibleChildrenForAnalysis(
            const PlaneSet& planes, const Vector3& camera) const noexcept;
        // Native slot54 is a no-op; unlike Octree it does not disable planes.
        void ReducePlanesForChildForAnalysis(std::uint32_t child, PlaneSet& planes) const noexcept;
        // Host-owned result copy. Native480360 returns borrowed internal94,
        // two records invalidated by next ray call; unused tail is untouched.
        [[nodiscard]] std::vector<RayCandidateForAnalysis> RayCandidatesForAnalysis(
            const std::array<float, 6>& ray) const;

        struct RawRayCandidateForAnalysis final
        {
            std::uint32_t child = 0;
            std::uint32_t timeBits = 0;
        };

        struct RayScratchForAnalysis final
        {
            std::array<std::uint32_t, 4> planeBits{};
            std::array<RawRayCandidateForAnalysis, 2> scratch{};
        };

        // Exact PC 480360 on copied words. Ray input is origin XYZ, then
        // direction XYZ. Zero indicates refusal outside the finite profile.
        // On success the optional result borrows the supplied scratch.
        [[nodiscard]] static std::uint32_t UpdateRayScratchForAnalysis(
            RayScratchForAnalysis& state, const std::array<std::uint32_t, 6>& rayBits,
            const RawRayCandidateForAnalysis** borrowedFirst = nullptr) noexcept;

      private:
        [[nodiscard]] double DistanceForAnalysis(const Vector3& point) const noexcept;
        Plane plane_{}; // ctor native plane uninitialized; explicit gap flag
        bool planeKnown_ = false;
        std::vector<Vector3> polygon_;
    };
} // namespace sparkplug::reconstruction
