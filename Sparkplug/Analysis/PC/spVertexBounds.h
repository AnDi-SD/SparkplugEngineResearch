#pragma once
// Portable name for original PC468370 ->468000. Extracted without changing
// the already verified implementation formerly local to spDXMesh.cpp.
#include "Code/Sparkplug/spVertexBuffer.h"
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
namespace sparkplug::evidence::pc {
inline bool ComputeVertexSphere(const reconstruction::spVertexBuffer& vertices,
    std::array<float,4>& sphere) noexcept {
    const auto& bytes=vertices.GetDataForAnalysis();
    const std::size_t stride=vertices.GetVertexStrideForAnalysis();
    const std::size_t count=vertices.GetVertexCountForAnalysis();
    if(stride<12||count>bytes.size()/stride)return false;
    const auto position=[&](std::size_t index) {
        std::array<float,3> value{};
        std::memcpy(value.data(),bytes.data()+index*stride,12);return value;
    };
    const float high=std::numeric_limits<float>::max();
    std::array<float,3> low{high,high,high},upper{-high,-high,-high};
    for(std::size_t i=0;i<count;++i) {
        const auto v=position(i);
        for(std::size_t c=0;c<3;++c) {
            if(low[c]>v[c])low[c]=v[c];
            if(upper[c]<v[c])upper[c]=v[c];
        }
    }
    // Explicit float stores in the x87 producer are part of its result.
    for(std::size_t c=0;c<3;++c) {
        const float extent=static_cast<float>(double(upper[c])-low[c]);
        const double sum=double(upper[c])+low[c];
        const float center=static_cast<float>((c<2?double(static_cast<float>(sum)):sum)*.5);
        const double half=double(extent)*.5;
        const float first=static_cast<float>(double(center)-half);
        const double last=double(center)+half;
        const double joined=(c==0?double(static_cast<float>(last)):last)+first;
        sphere[c]=static_cast<float>((c<2?double(static_cast<float>(joined)):joined)*.5);
    }
    float radiusSquared=0;
    for(std::size_t i=0;i<count;++i) {
        const auto v=position(i);
        const double x=double(v[0])-sphere[0],y=double(v[1])-sphere[1],z=double(v[2])-sphere[2];
        const double distance=(z*z+y*y)+x*x;
        if(distance>radiusSquared)radiusSquared=static_cast<float>(distance);
    }
    sphere[3]=static_cast<float>(std::sqrt(double(radiusSquared)));return true;
}
}
