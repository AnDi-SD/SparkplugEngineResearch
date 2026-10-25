#include "spVisibilityManager.h"
#include "spZonePortal.h"
#include "Analysis/PC/spPolygonClip.h"
#include "Analysis/PC/spVisibilityPortalMath.h"
#include <cmath>

namespace sparkplug::reconstruction
{
    spVisibilityManager::TraversalStatusForAnalysis spVisibilityManager::PreparePortalForAnalysis(
        const spZonePortal& portal,const TraversalCameraForAnalysis& camera,
        const std::array<float,3>& incoming,const PlaneSetForAnalysis& current,PortalStepForAnalysis& output)
    {
        using Status=TraversalStatusForAnalysis;
        namespace math=evidence::pc::visibility_portal_math;
        const auto finite=[](const auto& values)
        {for(float value:values)if(!std::isfinite(value))return false;return true;};
        const auto& points=portal.GetPolygonForAnalysis();
        if(!portal.HasPlaneForAnalysis() || points.size()<3 || points.size()>127)
            return Status::InvalidInput; // native short/oversized scratch access is not qualified
        const auto& plane=portal.GetPlaneForAnalysis();
        if(!finite(plane)||!finite(incoming))return Status::InvalidInput;
        for(const auto& point:points)if(!finite(point))return Status::InvalidInput;
        const auto q=math::AddCameraOffset(camera.position,incoming);
        if(!finite(q))return Status::InvalidInput;
        const auto distance=math::PortalDistance(plane,q);
        PortalStepForAnalysis result;
        if(math::Compare(distance,math::FromFloat(0.001f))>0)
        {output=std::move(result);return Status::Complete;}
        if(math::Compare(distance,math::FromFloat(-0.001f))>=0)
        {
            // Inclusive band: projected edge tests, including closing edge.
            // No polygon clipping, no generic convex-hull replacement.
            const auto first=math::Relative(points.front(),q,false);
            auto previous=first;
            for(std::size_t i=1;i<points.size();++i)
            {
                const auto next=math::Relative(points[i],q,true);
                if(!math::EdgeFacesCamera(previous,next,plane,false))
                {output=std::move(result);return Status::Complete;}
                previous=next;
            }
            if(!math::EdgeFacesCamera(previous,first,plane,true))
            {output=std::move(result);return Status::Complete;}
            result.planes=current;
            // Native45E870 copies vector, caller copies activeCount separately,
            // then46ADC0(1) enables ALL live equations, including disabled ones.
            for(auto& value:result.planes.planes)value.enabled=true;
            result.planes.activeCount=std::uint32_t(result.planes.planes.size());
            result.incoming=math::ShiftedIncoming(camera.position,incoming,plane);
        }
        else
        {
            // Far-facing side: two original scratch polygons alternate on every
            // enabled plane. The shared geometric helper preserves491AA0 quirks.
            if(current.planes.size()<2)return Status::InvalidInput;
            auto polygon=points;
            for(const auto& value:current.planes)
            {
                if(!value.enabled)continue;
                if(polygon.size()>127)return Status::HostTraversalLimit;
                std::vector<spZonePortal::Vector3> clipped;
                (void)evidence::pc::polygon_clip::ClipForAnalysis(polygon,value.equation,false,clipped);
                polygon=std::move(clipped);
            }
            if(polygon.empty()){output=std::move(result);return Status::Complete;}
            if(polygon.size()>127)return Status::HostTraversalLimit;
            result.planes.planes={current.planes[0],current.planes[1]};
            for(auto& value:result.planes.planes)value.enabled=true;
            for(std::size_t i=0;i<polygon.size();++i)
            {
                const auto equation=spZonePortal::PlaneFromFirstThreeForAnalysis(
                    camera.position,polygon[i],polygon[(i+1)%polygon.size()]);
                if(!finite(equation))return Status::InvalidInput;
                result.planes.planes.push_back({equation,true});
            }
            result.planes.activeCount=std::uint32_t(result.planes.planes.size());
            result.incoming=incoming; // clipping branch preserves caller argument
        }
        if(!finite(result.incoming))return Status::InvalidInput;
        result.traverse=true;output=std::move(result);return Status::Complete;
    }
}
