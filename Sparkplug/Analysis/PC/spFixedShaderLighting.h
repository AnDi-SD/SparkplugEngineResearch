#pragma once
// Semantic reconstruction of Fixed.rfx: view normal, directional diffuse and
// ColorMode. Not an original CPU method. Point/spot/specular are outside this
// interface; input colors are shader constants, not inferred material fields.
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
[[nodiscard]] inline bool ShadeFixedDirectionalForAnalysis(
    const FixedSkinVector& deformedNormal,const FixedSkinVector& vertexColor,
    const FixedDirectionalLightingForAnalysis& parameters,
    FixedLightingOutputForAnalysis& output) noexcept {
    auto finite=[](const auto& vector){for(float value:vector)if(!std::isfinite(value))return false;return true;};
    if(parameters.lightCount>parameters.lights.size()||parameters.colorMode>7||
       !finite(deformedNormal)||!finite(vertexColor)||!finite(parameters.ambient)||
       !finite(parameters.materialDiffuse)||!finite(parameters.constantColor))return false;
    for(const auto& row:parameters.view)if(!finite(row))return false;
    FixedLightingOutputForAnalysis result;
    for(unsigned j=0;j<3;++j)for(unsigned i=0;i<3;++i)
        result.viewNormal[j]+=deformedNormal[i]*parameters.view[i][j];
    float length2=0;for(float value:result.viewNormal)length2+=value*value;
    if(!(length2>0)||!std::isfinite(length2))return false;
    const float inverseLength=1/std::sqrt(length2);
    for(float& value:result.viewNormal)value*=inverseLength;
    result.color=parameters.ambient;
    for(unsigned i=0;i<parameters.lightCount;++i){const auto& light=parameters.lights[i];
        if(!finite(light.direction)||!finite(light.diffuse))return false;
        float intensity=0;
        for(unsigned j=0;j<3;++j)intensity-=result.viewNormal[j]*light.direction[j];
        if(!std::isfinite(intensity))return false;
        if(intensity>0)for(unsigned j=0;j<3;++j)result.color[j]+=light.diffuse[j]*intensity;
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
} // namespace sparkplug::evidence::pc
