#pragma once
// HOST camera-parent projection using the common Node/SkyBox transform code.
// This is not a SkyBox clone or a complete scene-manager/reparent transaction.
#include "ViewerBridge.h"
#include "Code/Sparkplug/spSkyBox.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace spvhost {
inline SpvSkyPose SkyPose(const sparkplug::reconstruction::spSkyBox& sky) {
    SpvSkyPose value{};value.flags=sky.GetFlagsForAnalysis();
    std::copy_n(sky.GetPositionForAnalysis().data(),3,value.position);
    std::copy_n(sky.GetScaleForAnalysis().data(),3,value.scale);
    std::copy_n(sky.GetOrientationForAnalysis().data(),9,value.orientation);
    std::copy_n(sky.GetWorldPositionForAnalysis().data(),3,value.retainedWorldPosition);return value;
}
inline sparkplug::reconstruction::spNode::Matrix4 SkyCameraWorld(const SpvSkyPose& pose,const float* cameraWorld) {
    using namespace sparkplug::reconstruction;
    auto need=[](bool value,const char* error){if(!value)throw std::runtime_error(error);};
    need(cameraWorld,"SKY_CAMERA: missing camera world");
    for(float x:pose.position)need(std::isfinite(x),"SKY_POSE: nonfinite position");
    for(float x:pose.scale)need(std::isfinite(x),"SKY_POSE: nonfinite scale");
    for(float x:pose.orientation)need(std::isfinite(x),"SKY_POSE: nonfinite orientation");
    for(float x:pose.retainedWorldPosition)need(std::isfinite(x),"SKY_POSE: nonfinite retained world position");
    for(int i=0;i<16;++i)need(std::isfinite(cameraWorld[i]),"SKY_CAMERA: nonfinite matrix");
    need(cameraWorld[3]==0&&cameraWorld[7]==0&&cameraWorld[11]==0&&cameraWorld[15]==1,"SKY_CAMERA: affine matrix required");
    for(int r=0;r<3;++r)for(int c=0;c<3;++c) {
        float dot=0;for(int k=0;k<3;++k)dot+=cameraWorld[r*4+k]*cameraWorld[c*4+k];
        need(std::abs(dot-(r==c?1.f:0.f))<.001f,"SKY_CAMERA: unit camera basis required");
    }
    // The transient camera parent owns only a transform observation. Actual
    // graph membership, descendants, bounds and light caches remain unchanged.
    spNode camera;auto sky=std::make_shared<spSkyBox>();
    spNode::Vector3 position{},scale{};spNode::Matrix3 orientation{},cameraOrientation{};
    std::copy_n(pose.position,3,position.data());std::copy_n(pose.scale,3,scale.data());
    std::copy_n(pose.orientation,9,orientation.data());
    for(int r=0;r<3;++r)for(int c=0;c<3;++c)cameraOrientation[r*3+c]=cameraWorld[r*4+c];
    camera.SetPositionForAnalysis({cameraWorld[12],cameraWorld[13],cameraWorld[14]});
    camera.SetOrientationForAnalysis(cameraOrientation);camera.MarkLocalTransformDirtyForAnalysis();
    // Native no-inherit-position keeps its prior world value, not local PRS.
    // Seed that value through the common root update before attaching a parent.
    sky->SetPositionForAnalysis({pose.retainedWorldPosition[0],pose.retainedWorldPosition[1],pose.retainedWorldPosition[2]});
    sky->MarkLocalTransformDirtyForAnalysis();need(sky->UpdateWorldForAnalysis(),"SKY_WORLD: seed update failed");
    sky->SetPositionForAnalysis(position);sky->SetScaleForAnalysis(scale);sky->SetOrientationForAnalysis(orientation);
    sky->SetInheritanceForAnalysis((pose.flags&spNode::InheritPositionMask)!=0,
        (pose.flags&spNode::InheritOrientationMask)!=0,(pose.flags&spNode::InheritScaleMask)!=0);
    sky->SetBillboardAxisForAnalysis((pose.flags>>20)&3u);
    need(camera.AttachChildForAnalysis(sky)&&camera.UpdateWorldForAnalysis(0,&cameraOrientation),"SKY_WORLD: common world update failed");
    const auto world=sky->GetWorldMatrixForAnalysis();
    for(float x:world)need(std::isfinite(x),"SKY_WORLD: nonfinite output");return world;
}
}
