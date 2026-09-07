#include "spZonePortal.h"
#include <cmath>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreatePortal()
        {
            return std::make_unique<spZonePortal>();
        }
        const spRTTIRecord Record{spZonePortal::ClassID, spNamedObject::ClassID,
                                  "spZonePortal",        &spNamedObject::StaticRTTI(),
                                  &CreatePortal,         nullptr};
        const bool Registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    } // namespace
    const spRTTIRecord& spZonePortal::StaticRTTI() noexcept
    {
        (void)Registered;
        return Record;
    }
    const spRTTIRecord& spZonePortal::vfunc_18() const noexcept
    {
        return Record;
    }
    std::unique_ptr<spBaseObject> spZonePortal::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<spZonePortal>();
        manager.RegisterClone(*this, *clone);
        return spNamedObject::vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }
    spZonePortal::Plane spZonePortal::PlaneFromFirstThreeForAnalysis(const Vector3& first,
                                                                     const Vector3& second,
                                                                     const Vector3& third) noexcept
    {
        Vector3 a{}, b{}, normal{};
        for (std::size_t i = 0; i < 3; ++i)
        {
            a[i] = static_cast<float>(double(second[i]) - first[i]);
            b[i] = static_cast<float>(double(third[i]) - first[i]);
        }
        for (std::size_t i = 0; i < 3; ++i)
            normal[i] = static_cast<float>(double(a[(i + 1) % 3]) * b[(i + 2) % 3] -
                                           double(b[(i + 1) % 3]) * a[(i + 2) % 3]);
        const double length =
            std::sqrt((double(normal[0]) * normal[0] + double(normal[1]) * normal[1]) +
                      double(normal[2]) * normal[2]);
        if (length > double(0.001F))
        {
            const double inverse = 1.0 / length;
            for (auto& value : normal)
                value = static_cast<float>(inverse * value);
        }
        else
            normal = {0, 0, 0};
        const float distance =
            static_cast<float>((double(first[0]) * normal[0] + double(normal[1]) * first[1]) +
                               double(normal[2]) * first[2]);
        return {normal[0], normal[1], normal[2], distance};
    }
    bool spZonePortal::SetPolygonForAnalysis(const std::vector<Vector3>& points)
    {
        if (points.size() < 3 || points.size() > 4096)
            return false;
        for (const auto& point : points)
            for (float value : point)
                if (!std::isfinite(value))
                    return false;
        auto copied = points; // host alias/allocation safety, not native failure semantics
        const auto plane = PlaneFromFirstThreeForAnalysis(points[0], points[1], points[2]);
        polygon_ = std::move(copied);
        plane_ = plane;
        planeKnown_ = true;
        return true;
    }
    const std::vector<spZonePortal::Vector3>& spZonePortal::GetPolygonForAnalysis() const noexcept
    {
        return polygon_;
    }
    std::uint32_t spZonePortal::GetPolygonVertexCount() const noexcept
    {
        return static_cast<std::uint32_t>(polygon_.size());
    }
    const spZonePortal::Vector3& spZonePortal::GetPolygonVertex(std::size_t index) const
    {
        return polygon_.at(index);
    }
    bool spZonePortal::HasPlaneForAnalysis() const noexcept
    {
        return planeKnown_;
    }
    const spZonePortal::Plane& spZonePortal::GetPlaneForAnalysis() const noexcept
    {
        return plane_;
    }
    void spZonePortal::SetDestinationForAnalysis(spZone* destination) noexcept
    {
        destination_ = destination;
    }
    spZone* spZonePortal::GetDestinationZone() const noexcept
    {
        return destination_;
    }
    void spZonePortal::SetOpenForAnalysis(bool open) noexcept
    {
        open_ = open;
    }
    bool spZonePortal::IsOpen() const noexcept
    {
        return open_;
    }
    void spZonePortal::SetVisibilityMarkForAnalysis(std::uint32_t mark) noexcept
    {
        visibilityMark_ = mark;
    }
    std::uint32_t spZonePortal::GetVisibilityMarkForAnalysis() const noexcept
    {
        return visibilityMark_;
    }
} // namespace sparkplug::reconstruction
