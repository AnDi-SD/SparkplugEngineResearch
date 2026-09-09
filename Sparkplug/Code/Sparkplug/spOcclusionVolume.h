#pragma once

// Original TU is proven by PC 006E8D14. This is the shared, partial class:
// readable geometry methods, not the still unresolved full Init/reader.
#include "spNode.h"
#include <optional>

namespace sparkplug::reconstruction
{
class spOcclusionVolume final : public spNode
{
public:
    static constexpr spClassID ClassID=0x43D24430;
    using PlaneForAnalysis=std::array<float,4>;
    struct FaceForAnalysis
    {
        PlaneForAnalysis plane{};
        std::array<const Vector3*,3> positions{};
        std::uint32_t cameraSide=0;
    };
    struct EdgeForAnalysis
    {
        const Vector3* start=nullptr;
        const Vector3* end=nullptr;
        FaceForAnalysis* opposite=nullptr;
        FaceForAnalysis* own=nullptr;
        std::vector<EdgeForAnalysis*> outgoing;
        std::uint32_t walkStamp=0;
        std::uint8_t border=0;
    };
    struct PreparedEdgeForAnalysis
    {
        std::size_t start,end,own;
        std::optional<std::size_t> opposite;
    };
    [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
    [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
    [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;

    // Explicit internal state for direct leaf comparisons. This does not
    // initialize an occluder, read geometry, construct faces or weld buffers.
    bool SetPreparedTopologyForAnalysis(std::vector<Vector3> points,
        const std::vector<PlaneForAnalysis>& planes,
        const std::vector<PreparedEdgeForAnalysis>& edges,
        std::uint32_t borderCount,std::uint8_t priorPlanar);
    [[nodiscard]] bool IsEdgeConvexForAnalysis(const EdgeForAnalysis&) const noexcept;
    bool MergeCollinearEdgesForAnalysis();
    bool LinkOppositeEdgesForAnalysis();
    bool RemoveCoplanarEdgesForAnalysis();
    bool CheckPlanarityForAnalysis();
    bool ConnectOutgoingEdgesForAnalysis(EdgeForAnalysis&);
    [[nodiscard]] const auto& GetEdgesForAnalysis() const noexcept{return edges_;}
    [[nodiscard]] const auto& GetFacesForAnalysis() const noexcept{return faces_;}
    [[nodiscard]] const auto& GetPointsForAnalysis() const noexcept{return points_;}
    [[nodiscard]] auto GetBorderCountForAnalysis() const noexcept{return borderCount_;}
    [[nodiscard]] auto GetPlanarByteForAnalysis() const noexcept{return planar_;}
private:
    std::vector<Vector3> points_;
    std::vector<FaceForAnalysis> faces_;
    std::vector<std::unique_ptr<EdgeForAnalysis>> edges_;
    // Actual PC factory initializes D4/160/164 to0, preserving only padding.
    std::uint32_t borderCount_=0;
    std::uint8_t planar_=0;
};
}
