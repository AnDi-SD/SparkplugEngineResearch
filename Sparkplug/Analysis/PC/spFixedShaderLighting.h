#pragma once
// Semantic reconstruction of Fixed.rfx: view normal, directional/point/spot
// diffuse and ColorMode. Not an original CPU method. Specular is outside this
// interface; input colors are shader constants, not inferred material fields.
#include <algorithm>
#include "spFixedShaderSkinning.h"

namespace sparkplug::evidence::pc {
struct FixedDirectionalLightForAnalysis {
    FixedSkinVector direction{},diffuse{};
};
struct FixedDirectionalLightingForAnalysis {
    // Three uploaded float4 view registers. out[j] = sum(in[i]*row[i][j]).
    std::array<FixedSkinVector,3> view{{{1,0,0,0},{0,1,0,0},{0,0,1,0}}};
    FixedSkinVector ambient{},materialDiffuse{1,1,1,1},constantColor{};
    std::array<FixedDirectionalLightForAnalysis,8> lights{};
    uint32_t lightCount=0,colorMode=4;
};
struct FixedLightingOutputForAnalysis {
    std::array<float,3> viewNormal{};
    FixedSkinVector color{};
};
struct FixedDiffuseLightForAnalysis {
    uint32_t type=0; // Fixed shader literal: directional0, point1, spot2.
    FixedSkinVector direction{},diffuse{},position{},attenuation{};
    float inner=0,outer=0;
};
struct FixedDiffuseLightingForAnalysis {
    FixedSkinVector ambient{},materialDiffuse{1,1,1,1},constantColor{};
    std::array<FixedDiffuseLightForAnalysis,8> lights{};
    uint32_t lightCount=0,colorMode=4;
};
// PosView and the linear view-normal product are supplied explicitly. This
// function normalizes the latter just as Fixed does; it does not guess matrix
// orientation or derive shader light constants from engine light objects.
[[nodiscard]] inline bool ShadeFixedDiffuseForAnalysis(
    const FixedSkinVector& viewPosition,const std::array<float,3>& viewNormal,const FixedSkinVector& vertexColor,
    const FixedDiffuseLightingForAnalysis& parameters,
    FixedLightingOutputForAnalysis& output) noexcept {
    auto finite=[](const auto& vector){for(float value:vector)if(!std::isfinite(value))return false;return true;};
    if(parameters.lightCount>parameters.lights.size()||parameters.colorMode>7||
       !finite(viewNormal)||!finite(vertexColor)||!finite(parameters.ambient)||
       !finite(parameters.materialDiffuse)||!finite(parameters.constantColor))return false;
    FixedLightingOutputForAnalysis result;
    result.viewNormal=viewNormal;
    float length2=0;for(float value:result.viewNormal)length2+=value*value;
    if(!(length2>0)||!std::isfinite(length2))return false;
    const float inverseLength=1/std::sqrt(length2);
    for(float& value:result.viewNormal)value*=inverseLength;
    result.color=parameters.ambient;
    for(unsigned i=0;i<parameters.lightCount;++i){const auto& light=parameters.lights[i];
        if(light.type>2||!finite(light.diffuse))return false;
        if(light.type==0) {
            if(!finite(light.direction))return false;
            float intensity=0;
            for(unsigned j=0;j<3;++j)intensity-=result.viewNormal[j]*light.direction[j];
            if(!std::isfinite(intensity))return false;
            if(intensity>0)for(unsigned j=0;j<3;++j)result.color[j]+=light.diffuse[j]*intensity;
            continue;
        }
        if(!finite(viewPosition)||!finite(light.position))return false;
        FixedSkinVector difference{};float distance4Squared=0;
        for(unsigned j=0;j<4;++j){difference[j]=light.position[j]-viewPosition[j];distance4Squared+=difference[j]*difference[j];}
        if(!(distance4Squared>0)||!std::isfinite(distance4Squared))return false;
        // Fixed normalizes the float4 difference before assigning xyz to its
        // float3 light direction. The homogeneous component is not discarded.
        const float inverseDistance4=1/std::sqrt(distance4Squared);
        std::array<float,3> direction{};float intensity=0;
        for(unsigned j=0;j<3;++j){direction[j]=difference[j]*inverseDistance4;intensity+=result.viewNormal[j]*direction[j];}
        if(!std::isfinite(intensity))return false;
        if(light.type==1) {
            if(intensity>0) {
                float distance3Squared=0;for(unsigned j=0;j<3;++j)distance3Squared+=difference[j]*difference[j];
                const float attenuation=light.attenuation[0]+light.attenuation[1]*std::sqrt(distance3Squared);
                if(!std::isfinite(attenuation)||attenuation==0)return false;
                const float factor=1/attenuation;
                // This multiplies ALL accumulated RGB, including ambient and
                // preceding lights. Quadratic attenuation is not read here.
                for(unsigned j=0;j<3;++j){result.color[j]+=light.diffuse[j]*intensity;result.color[j]*=factor;}
            }
        }else {
            if(!finite(light.direction)||!std::isfinite(light.inner)||!std::isfinite(light.outer))return false;
            float cosine=0;for(unsigned j=0;j<3;++j)cosine-=direction[j]*light.direction[j];
            const float width=light.inner-light.outer;
            if(!std::isfinite(cosine)||!std::isfinite(width)||width==0)return false;
            float spot=(cosine-light.outer)/width;
            if(!std::isfinite(spot))return false;
            spot=(std::min)(1.f,(std::max)(0.f,spot));
            // Unlike point/directional diffuse, this branch does not clamp a
            // negative normal/light dot. There is no distance attenuation.
            for(unsigned j=0;j<3;++j)result.color[j]+=spot*(light.diffuse[j]*intensity);
        }
    }
    switch(parameters.colorMode){
    case 0:result.color={1,1,1,1};break;
    case 1:result.color=parameters.materialDiffuse;break;
    case 2:result.color=vertexColor;break;
    case 4:
        for(unsigned j=0;j<3;++j)result.color[j]+=vertexColor[j];
        result.color[3]=parameters.materialDiffuse[3];break;
    case 5:
        result.color[3]=parameters.materialDiffuse[3];
        for(unsigned j=0;j<4;++j)result.color[j]*=vertexColor[j];break;
    case 6:result.color=parameters.constantColor;break;
    default:result.color[3]=parameters.materialDiffuse[3];break; // 3 and 7.
    }
    // No saturation: the shader's color can be negative or exceed one. Packed
    // D3D9 COLOR output and framebuffer blending are separate boundaries.
    if(!finite(result.color)||!finite(result.viewNormal))return false;
    output=result;return true;
}
// Compatibility entry for the established three-register directional input.
// Its view-normal transform and arithmetic order remain unchanged; ColorMode
// and light accumulation share the same implementation as local lights.
[[nodiscard]] inline bool ShadeFixedDirectionalForAnalysis(
    const FixedSkinVector& deformedNormal,const FixedSkinVector& vertexColor,
    const FixedDirectionalLightingForAnalysis& parameters,
    FixedLightingOutputForAnalysis& output) noexcept {
    if(parameters.lightCount>parameters.lights.size())return false;
    for(float value:deformedNormal)if(!std::isfinite(value))return false;
    for(const auto& row:parameters.view)for(float value:row)if(!std::isfinite(value))return false;
    std::array<float,3> viewNormal{};
    for(unsigned j=0;j<3;++j)for(unsigned i=0;i<3;++i)viewNormal[j]+=deformedNormal[i]*parameters.view[i][j];
    FixedDiffuseLightingForAnalysis diffuse;
    diffuse.ambient=parameters.ambient;diffuse.materialDiffuse=parameters.materialDiffuse;diffuse.constantColor=parameters.constantColor;
    diffuse.lightCount=parameters.lightCount;diffuse.colorMode=parameters.colorMode;
    for(unsigned i=0;i<parameters.lightCount;++i){diffuse.lights[i].direction=parameters.lights[i].direction;diffuse.lights[i].diffuse=parameters.lights[i].diffuse;}
    return ShadeFixedDiffuseForAnalysis({0,0,0,1},viewNormal,vertexColor,diffuse,output);
}
} // namespace sparkplug::evidence::pc
