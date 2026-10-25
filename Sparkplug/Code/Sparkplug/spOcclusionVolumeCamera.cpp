// Continuation of original spOcclusionVolume.cpp:46FFE0 and4702E0.
#include "spOcclusionVolume.h"
#include "spZonePortal.h"
#include "Analysis/PC/spOcclusionCameraMath.h"
#include <algorithm>

namespace sparkplug::reconstruction
{
    spOcclusionVolume::CameraResultForAnalysis spOcclusionVolume::PrepareCameraForAnalysis(
        const Vector3& worldCamera)
    {
        namespace math=evidence::pc::occlusion_camera_math;
        if(!initialized_)return {true,false}; // no cache/geometry reads on this native branch
        if(!math::Finite(worldCamera))return {};
        if(!cameraDirty_&&math::Near(worldCamera,cachedCamera_))return {true,true};
        Vector3 localCamera;
        if(!math::WorldToLocal420710(worldCamera,GetWorldPositionForAnalysis(),
            GetWorldOrientationForAnalysis(),GetWorldScaleForAnalysis(),localCamera))return {};
        if(points_.size()>4096||faces_.size()>1024||edges_.size()>MaxShapeEdgesForAnalysis)return {};
        const auto pointKnown=[&](const Vector3* value)
        {return std::any_of(points_.begin(),points_.end(),[&](const auto& point){return &point==value;});};
        const auto faceKnown=[&](const FaceForAnalysis* value)
        {return std::any_of(faces_.begin(),faces_.end(),[&](const auto& face){return &face==value;});};
        const auto edgeKnown=[&](const EdgeForAnalysis* value)
        {return std::any_of(edges_.begin(),edges_.end(),[&](const auto& edge){return edge.get()==value;});};
        for(const auto& face:faces_)
        {
            if(!math::Finite(face.plane))return {};
            for(const auto* point:face.positions)if(!pointKnown(point)||!math::Finite(*point))return {};
        }
        for(const auto& edge:edges_)
        {
            if(!edge||!pointKnown(edge->start)||!pointKnown(edge->end)||!faceKnown(edge->own)||
                (edge->opposite&&!faceKnown(edge->opposite)))return {};
            for(const auto* next:edge->outgoing)if(!edgeKnown(next))return {};
        }
        // Native publishes world camera/dirty before geometry rebuild. A later
        // host failure may therefore leave partial cache/face/edge mutation.
        cameraDirty_=false;cachedCamera_=worldCamera;borderPoints_.clear();cameraFacePlanes_.clear();
        for(auto& face:faces_)
        {
            const auto distance=math::FaceDistance(face.plane,localCamera);
            face.cameraSide=math::Compare(distance,math::FromFloat(-.001f))<0?1u:
                math::Compare(distance,math::FromFloat(.001f))>0?0u:2u;
            face.cameraSideKnown=true;
            if(!face.cameraSide)
            {
                const auto plane=spZonePortal::PlaneFromFirstThreeForAnalysis(
                    *face.positions[2],*face.positions[1],*face.positions[0]);
                //470182..470199 pushes positions0,1,2: formal args are2,1,0.
                if(!math::Finite(plane))return {};
                cameraFacePlanes_.push_back(plane);
            }
        }
        const auto eligible=[](const EdgeForAnalysis* edge)
        {return edge->own->cameraSide==0&&(!edge->opposite||edge->opposite->cameraSide!=0);};
        EdgeForAnalysis* selected=nullptr;
        for(const auto& edge:edges_)if(eligible(edge.get())){selected=edge.get();break;}
        ++walkStamp_; // original wrap is retained, including stamp0 early termination
        if(!selected)return {true,true};
        borderPoints_.push_back(*selected->start);
        std::size_t visited=0;
        while(selected&&selected->walkStamp!=walkStamp_)
        {
            if(++visited>MaxShapeEdgesForAnalysis)return {};
            borderPoints_.push_back(*selected->end);selected->walkStamp=walkStamp_;
            EdgeForAnalysis* next=nullptr;
            for(auto* candidate:selected->outgoing)if(eligible(candidate)){next=candidate;break;}
            selected=next; // first matching outgoing; no generic polygon sorting
        }
        return {true,true};
    }

    spOcclusionVolume::CameraResultForAnalysis spOcclusionVolume::BuildCameraPlanesForAnalysis(
        const Vector3& worldCamera)
    {
        namespace math=evidence::pc::occlusion_camera_math;
        const auto prepared=PrepareCameraForAnalysis(worldCamera);
        if(!prepared.completed||!prepared.nativeReturn)return prepared; // old planes survive nativefalse
        cameraPlanes_.planes.clear();cameraPlanes_.activeCount=0;
        if(borderPoints_.size()<3)return {true,true};
        for(const auto& plane:cameraFacePlanes_)
        {cameraPlanes_.planes.push_back({plane,true});++cameraPlanes_.activeCount;}
        // Border is explicitly closed by the walk. Native has no implicit
        // last-to-first side if the point sequence happens to remain open.
        for(std::size_t i=0;i+1<borderPoints_.size();++i)
        {
            const auto plane=spZonePortal::PlaneFromFirstThreeForAnalysis(
                worldCamera,borderPoints_[i+1],borderPoints_[i]);
            if(!math::Finite(plane))return {};
            if(!cameraPlanes_.planes.empty()&&math::Near(cameraPlanes_.planes.back().equation,plane))continue;
            cameraPlanes_.planes.push_back({plane,true});++cameraPlanes_.activeCount;
        }
        return {true,true};
    }
}
