#pragma once

// PC490360/490480: direct-VM plane-set/packed Vector3 queries. Analytical
// names; no invented native class or successful protection-dispatch stub.
// Plane signs use the inclusive .001 band, as471350 does for its own input.
// Finite x87 nearest64 arithmetic is shared with the camera plane classifier.
#include "spVisibilityMath.h"
#include "spOcclusionCameraMath.h"
#include <optional>
#include <vector>

namespace sparkplug::evidence::pc::visibility_point_math
{
    using Vector3=std::array<float,3>;
    using Plane=visibility_math::Plane;
    using PlaneSet=visibility_math::PlaneSet;
    enum class PointClass : std::uint8_t {PositiveOnly,NegativeOnly,Intersecting};
    struct QueryResult {bool completed=false,value=false;};
    inline std::optional<PointClass> ClassifyPoints(const Plane& plane,
        const std::vector<Vector3>& points) noexcept
    {
        bool positive=false,negative=false;
        for(const auto& point:points)
        {
            bool unordered=false;
            for(float value:point){if(std::isinf(value))return {};unordered|=std::isnan(value);}
            for(float value:plane.equation){if(std::isinf(value))return {};unordered|=std::isnan(value);}
            // Native unordered comparisons add neither sign. A later ordinary
            // point can still give this plane a positive-only/negative-only class.
            if(unordered)continue;
            const auto distance=occlusion_camera_math::FaceDistance(plane.equation,point);
            negative|=occlusion_camera_math::Compare(distance,occlusion_camera_math::FromFloat(-.001f))<0;
            positive|=occlusion_camera_math::Compare(distance,occlusion_camera_math::FromFloat(.001f))>0;
            if(positive&&negative)return PointClass::Intersecting;
        }
        return positive?PointClass::PositiveOnly:negative?PointClass::NegativeOnly:PointClass::Intersecting;
    }
    inline QueryResult IsOutsidePoints(const PlaneSet& set,const std::vector<Vector3>& points) noexcept
    {
        // Both point queries gate on cached count before reading the vector.
        if(!set.activeCount)return {true,false};
        for(const auto& plane:set.planes)
        {
            if(!plane.enabled)continue;
            const auto classification=ClassifyPoints(plane,points);if(!classification)return {};
            if(*classification==PointClass::NegativeOnly)return {true,true};
        }
        return {true,false};
    }
    inline QueryResult IsFullyInsidePoints(const PlaneSet& set,const std::vector<Vector3>& points) noexcept
    {
        if(!set.activeCount)return {true,true};
        for(const auto& plane:set.planes)
        {
            if(!plane.enabled)continue;
            const auto classification=ClassifyPoints(plane,points);if(!classification)return {};
            if(*classification!=PointClass::PositiveOnly)return {true,false};
        }
        return {true,true};
    }
}
