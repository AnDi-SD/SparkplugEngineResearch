#pragma once
// Modern shader boundary: parameter layout is HOST-owned. Products, view-space
// light vectors, packed-color conversion and cone cosines call the shared PC
// constant producers. Selection remains the RenderNode's actual light cache.
#include "ViewerBridge.h"
#include "Code/SparkplugDX/spDXRenderer.h"
#include "Code/SparkplugDX/spDXLight.h"
#include "Code/SparkplugDX/spDXMaterial.h"
#include "Code/SparkplugPC/spPCVertexShader.h"
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace spvhost {
inline SpvShaderLighting CaptureShaderLighting(
    const sparkplug::reconstruction::spLightManager::CacheForAnalysis& cache,
    const sparkplug::reconstruction::spDXMaterial& material,const float* view,std::uint32_t constantColor)
{
    using namespace sparkplug::reconstruction;
    const auto need=[](bool ok,const char* error){if(!ok)throw std::runtime_error(error);};
    need(view&&cache.GetCount()<=8,"Invalid shader view or selected light count");
    spDXShader::ConstantInputsForAnalysis inputs;
    for(unsigned i=0;i<16;++i)need(std::isfinite(view[i]),"Non-finite shader view");
    std::memcpy(inputs.viewMatrix.data(),view,64);
    inputs.material=&material;inputs.constantColor=constantColor;inputs.lightListPresent=true;
    if(cache.GetAmbient()) {
        auto* light=dynamic_cast<const spDXLight*>(cache.GetAmbient());
        need(light!=nullptr,"Unsupported selected ambient light");
        inputs.ambientLight=spDXRenderer::ReadShaderLightForAnalysis(*light);
    }
    // Only copy defined producer words. Power/cone producers intentionally do
    // not initialize the other three words of their original register rows.
    const auto query=[&](const char* name,float* output,unsigned words) {
        spPCVertexShader shader;spDXShader::ParameterForAnalysis parameter{};
        const auto length=std::strlen(name);need(length<parameter.name.size(),"Shader parameter name is too long");
        std::memcpy(parameter.name.data(),name,length);parameter.registerCount=1;
        need(shader.AppendParameterForAnalysis(parameter),"Invalid shader parameter name");
        std::vector<std::uint32_t> scratch(4,0x7fc01234);
        need(shader.BuildConstantsForAnalysis(scratch,inputs),"Shared shader constant producer refused input");
        std::memcpy(output,scratch.data(),words*4);
        for(unsigned i=0;i<words;++i)need(std::isfinite(output[i]),"Non-finite shader constant output");
    };
    SpvShaderLighting output{};
    output.colorMode=material.GetRenderStateForAnalysis(8);
    need(output.colorMode<=15,"Color mode overlaps the original shader light-count key bits");
    query("MatDiffuse",output.diffuse,4);output.known|=2;
    query("MatSpecularPwr",&output.power,1);output.known|=8;
    output.specular=output.power>0;
    query("ConstColor",output.constantColor,4);output.known|=4;
    query("AmbientCol",output.ambient,4);output.known|=1;
    output.count=static_cast<std::uint32_t>(cache.GetCount());
    for(unsigned i=0;i<output.count;++i) {
        auto* light=dynamic_cast<const spDXLight*>(cache.Get(i));
        need(light!=nullptr,"Unsupported selected shader light");
        // Shader key/constants retain selected disabled lights, as the PC draw
        // caller does. Do not substitute the fixed-function enabled-light list.
        inputs.lights={spDXRenderer::ReadShaderLightForAnalysis(*light)};
        auto& row=output.lights[i];row.type=inputs.lights[0].type;
        need(row.type<=2,"Unsupported Fixed.rfx light type");
        if(output.colorMode==0||output.colorMode==1||output.colorMode==2||output.colorMode==6)continue;
        query("LightMatDiff",row.diffuse,4);row.known|=1;
        if(output.specular){query("LightMatSpec",row.specular,4);row.known|=2;}
        if(row.type!=0){query("LightPos",row.position,4);row.known|=4;}
        if(row.type!=1){query("LightDir",row.direction,4);row.known|=8;}
        if(row.type==1&&!output.specular){query("LightAttenuation",row.attenuation,4);row.known|=16;}
        if(row.type==2){query("LightInner",&row.inner,1);query("LightOuter",&row.outer,1);row.known|=96;}
    }
    return output;
}
static_assert(sizeof(SpvShaderLight)==96&&sizeof(SpvShaderLighting)==836);
}
