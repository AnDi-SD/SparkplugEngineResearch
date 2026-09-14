#pragma once
// Shared recovered PC shader source preparation. Native manager selection and
// effect compilation delegate here; no compiler, device or backend dependency.
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace sparkplug::reconstruction {
struct PCShaderSourceInputsForAnalysis {std::string header,insertion;};
inline PCShaderSourceInputsForAnalysis BuildPCShaderSourceInputsForAnalysis(const std::array<std::uint32_t,2>& key) {
    PCShaderSourceInputsForAnalysis input;const auto mask=key[0];
    const auto line=[](std::string_view name,std::uint32_t value){return std::string(name)+" = "+std::to_string(value)+";\n";};
    input.insertion=line("BlendWeightCount",mask&15)+line("ColorMode",(mask>>16)&15)+line("LightCount",(mask>>20)&15)+line("bUseSpecular",(mask>>24)&1);
    for(unsigned i=0;i<((mask>>20)&15);++i)input.insertion+=line("LightType["+std::to_string(i)+"]",(key[1]>>(2*i))&3);
    for(unsigned i=0;i<8;++i)input.insertion+=line("bHasUVTransform["+std::to_string(i)+"]",(mask>>(8+i))&1);
    for(unsigned i=0;i<std::min((mask>>4)&15u,8u);++i)input.header+="#define USE_TEXCOORD"+std::to_string(i)+"\n";
    return input;
}
inline std::string ComposePCShaderSourceForAnalysis(std::string_view code,std::string_view declaration,
    std::string_view insertion,std::string_view header) {
    std::string source(header);source+=declaration;source+=code;
    const auto position=source.find("// INSERTION POINT");
    if(position!=std::string::npos&&!insertion.empty())source.insert(position,insertion.data(),insertion.size());
    return source;
}
} // namespace sparkplug::reconstruction
