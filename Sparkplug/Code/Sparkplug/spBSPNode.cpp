#include "spBSPNode.h"
#include <cmath>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateBsp()
        {
            return std::make_unique<spBSPNode>();
        }
        const spRTTIRecord Record{spBSPNode::ClassID, spPartitionNode::ClassID,
                                  "spBSPNode",        &spPartitionNode::StaticRTTI(),
                                  &CreateBsp,         nullptr};
        const bool Registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    } // namespace
    spBSPNode::spBSPNode() : spPartitionNode(2)
    {
    }
    const spRTTIRecord& spBSPNode::StaticRTTI() noexcept
    {
        (void)Registered;
        return Record;
    }
    const spRTTIRecord& spBSPNode::vfunc_18() const noexcept
    {
        return Record;
    }
    std::unique_ptr<spBaseObject> spBSPNode::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<spBSPNode>();
        manager.RegisterClone(*this, *clone);
        return spBaseObject::vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }
    void spBSPNode::SetPlaneForAnalysis(const Plane& plane) noexcept
    {
        plane_ = plane;
        planeKnown_ = true;
    }
    const spBSPNode::Plane& spBSPNode::GetPlane() const noexcept
    {
        return plane_;
    }
    bool spBSPNode::HasPlaneForAnalysis() const noexcept
    {
        return planeKnown_;
    }
    bool spBSPNode::SetPolygonForAnalysis(const std::vector<Vector3>& points)
    {
        if (points.size() > 4096)
            return false;
        polygon_ = points;
        return true;
    }
    std::uint32_t spBSPNode::GetPolygonVertexCount() const noexcept
    {
        return static_cast<std::uint32_t>(polygon_.size());
    }
    const spBSPNode::Vector3& spBSPNode::GetPolygonVertex(std::size_t index) const
    {
        return polygon_.at(index);
    }
    double spBSPNode::DistanceForAnalysis(const Vector3& point) const noexcept
    {
        return (double(plane_[2]) * point[2] + double(plane_[1]) * point[1]) +
               double(plane_[0]) * point[0] - plane_[3];
    }
    spPartitionNode* spBSPNode::FindLeafForAnalysis(const Vector3& point, bool stopAtZone) noexcept
    {
        if (stopAtZone && HasZoneForAnalysis())
            return this;
        if (!planeKnown_)
            return nullptr;
        auto* child = GetChildForAnalysis(DistanceForAnalysis(point) > 0 ? 0 : 1);
        return child ? child->FindLeafForAnalysis(point, stopAtZone) : nullptr; // host null guard
    }
    std::uint8_t spBSPNode::PointMaskForAnalysis(const Vector3& point) const noexcept
    {
        if (!planeKnown_)
            return 0;
        const double distance = DistanceForAnalysis(point);
        return distance > double(.001F) ? 1 : distance < -double(.001F) ? 2 : 3;
    }
    std::uint8_t spBSPNode::SphereMaskForAnalysis(const Sphere& sphere) const noexcept
    {
        if (!planeKnown_)
            return 0;
        const double distance = DistanceForAnalysis({sphere[0], sphere[1], sphere[2]});
        if (std::fabs(distance) <= sphere[3])
            return 3;
        return distance > 0 ? 1 : 2;
    }
    std::uint32_t spBSPNode::TraversalOrderForAnalysis(std::uint32_t side) noexcept
    {
        return side == 0 ? 8 : side == 1 ? 1 : 0; // last branch host bound, native table unchecked
    }
    std::uint32_t spBSPNode::VisibleChildrenForAnalysis(const PlaneSet& planes,
                                                        const Vector3& camera) const noexcept
    {
        if (!planeKnown_)
            return 0;
        bool splitVisible = true;
        for (const auto& item : planes.planes)
        {
            if (!item.enabled)
                continue;
            bool anyPositive = false;
            const auto& plane = item.equation;
            for (std::size_t i = 0; i < polygon_.size(); ++i)
            {
                const auto& point = polygon_[i];
                // Original four-way unrolled blocks sum X+Z+Y; scalar tail
                // sums Z+Y+X. Preserve that distinction explicitly.
                const double distance =
                    i < (polygon_.size() / 4) * 4
                        ? (double(point[0]) * plane[0] + double(point[2]) * plane[2]) +
                              double(plane[1]) * point[1] - plane[3]
                        : (double(point[2]) * plane[2] + double(point[1]) * plane[1]) +
                              double(plane[0]) * point[0] - plane[3];
                if (distance > double(.001F))
                {
                    anyPositive = true;
                    break;
                }
            }
            if (!anyPositive)
            {
                splitVisible = false;
                break;
            }
        }
        const bool positive = DistanceForAnalysis(camera) > double(.001F);
        if (splitVisible)
            return positive ? 0x82 : 0x12; // count2, camera-side first
        return positive ? 0x01 : 0x11;     // count1, only camera side
    }
    void spBSPNode::ReducePlanesForChildForAnalysis(std::uint32_t, PlaneSet&) const noexcept
    {
    }
    std::vector<spBSPNode::RayCandidateForAnalysis> spBSPNode::RayCandidatesForAnalysis(
        const std::array<float, 6>& ray) const
    {
        if (!planeKnown_)
            return {};
        const float distance = static_cast<float>(DistanceForAnalysis({ray[0], ray[1], ray[2]}));
        const float denominator = static_cast<float>(
            (double(ray[5]) * plane_[2] + double(ray[4]) * plane_[1]) + double(ray[3]) * plane_[0]);
        const std::uint32_t side = distance > 0 ? 0 : 1;
        std::vector<RayCandidateForAnalysis> result{{side, 0}};
        if ((denominator > 1e-5F && side == 1) || (denominator < -1e-5F && side == 0))
            result.push_back({side ^ 1, static_cast<float>(-(double(distance) / denominator))});
        return result;
    }
} // namespace sparkplug::reconstruction
