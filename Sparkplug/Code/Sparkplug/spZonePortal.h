#pragma once

// Original PC class/RTTI; declaration and implementation paths are inferred.
// Geometry481130 and Named-only clone4813F0. The analytical accessors name
// observed storage, not recovered original member symbols. No Scene traversal,
// serializer or DebugDraw481270 implementation is implied by this class.
#include "../SparkBase/spBaseObject.h"
#include <array>
#include <vector>

namespace sparkplug::reconstruction
{
    class spZone;
    class spZonePortal : public spNamedObject
    {
      public:
        static constexpr spClassID ClassID = 0x6523AC37;
        using Vector3 = std::array<float, 3>;
        using Plane = std::array<float, 4>;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;

        // Host guard: 3..4096 finite vertices. Native481130 does not validate
        // that bound, planarity, convexity or collinear first three points.
        // Invalid host input preserves prior geometry. Self-input is allowed.
        [[nodiscard]] bool SetPolygonForAnalysis(const std::vector<Vector3>& points);
        [[nodiscard]] const std::vector<Vector3>& GetPolygonForAnalysis() const noexcept;
        // Names survive in original serializer diagnostics. Host signatures
        // are not original ABI; vertex accessor throws on invalid index.
        [[nodiscard]] std::uint32_t GetPolygonVertexCount() const noexcept;
        [[nodiscard]] const Vector3& GetPolygonVertex(std::size_t index) const;
        [[nodiscard]] bool HasPlaneForAnalysis() const noexcept;
        [[nodiscard]] const Plane& GetPlaneForAnalysis() const noexcept;
        // Finite-input PC471420/426A40/41D2D0 slice. Double intermediates model
        // x87 store boundaries; not a universal bit-identical x87 claim.
        [[nodiscard]] static Plane PlaneFromFirstThreeForAnalysis(const Vector3& first,
                                                                  const Vector3& second,
                                                                  const Vector3& third) noexcept;
        void SetDestinationForAnalysis(spZone* destination) noexcept;
        [[nodiscard]] spZone* GetDestinationZone() const noexcept;
        void SetOpenForAnalysis(bool open) noexcept;
        [[nodiscard]] bool IsOpen() const noexcept;
        void SetVisibilityMarkForAnalysis(std::uint32_t mark) noexcept;
        [[nodiscard]] std::uint32_t GetVisibilityMarkForAnalysis() const noexcept;

      private:
        spZone* destination_ = nullptr; // borrowed, neither retain nor release
        std::vector<Vector3> polygon_;  // native directly owned count18/array1C
        bool open_ = true;
        Plane plane_{}; // native ctor leaves plane uninitialized; see flag below
        bool planeKnown_ = false;
        std::uint32_t visibilityMark_ = 0;
    };
} // namespace sparkplug::reconstruction
