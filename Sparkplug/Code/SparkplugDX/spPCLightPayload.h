#pragma once
// Shared recovered PC4B53C0 arithmetic (CP44/CP90), extracted without changing
// the spDXLight producer. This is an analysis boundary, not an original API.
// World values and globals are explicit inputs; this does not update a Node,
// clear dirty flags, register a light or infer whether its cache is current.
#include "../../Analysis/PC/spColorMath.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace sparkplug::reconstruction
{
    struct PCLightPayloadInputsForAnalysis
    {
        std::uint32_t kind=0;
        std::array<float,4> color{};
        std::array<float,3> position{},direction{},defaultVector{};
        float range=0,intensity=0,hotspot=0,falloff=0;
        std::uint32_t ambientARGB=0;
        bool attenuationEnabled=false;
    };

    // Payload may contain optional words (unknown native allocator bytes) or
    // owned raw words. Untouched fields stay untouched, including directional
    // theta/phi and all but range/falloff for ambient/unknown types.
    template<class Payload>
    [[nodiscard]] bool RefreshPCLightPayloadForAnalysis(
        const PCLightPayloadInputsForAnalysis& input,Payload& payload) noexcept
    {
        const auto kind=input.kind;const float range=input.range,intensity=input.intensity;
        if(!std::isfinite(range)||!std::isfinite(intensity)||!std::isfinite(input.hotspot)||!std::isfinite(input.falloff))return false;
        for(const auto* v:{&input.position,&input.direction,&input.defaultVector})for(float f:*v)if(!std::isfinite(f))return false;
        auto color=input.color;for(float f:color)if(!std::isfinite(f))return false;
        const auto write=[&payload](unsigned index,float value){std::uint32_t bits;std::memcpy(&bits,&value,4);payload[index]=bits;};
        write(19,range);write(20,1);
        if(kind>2)return true;
        payload[0]=kind==0?3:kind;
        const auto& pos=kind==0?input.defaultVector:input.position;
        const auto& dir=kind==1?input.defaultVector:input.direction;
        for(unsigned i=0;i<3;++i){write(13+i,pos[i]);write(16+i,dir[i]);}
        if(kind==0)
        {
            for(unsigned i=0;i<4;++i)color[i]=float(double(color[i])*double(intensity));
            for(unsigned i=0;i<3;++i)if(color[i]>1)color[i]=1;
        }
        const auto ambient=PCARGBToRGBAForAnalysis(input.ambientARGB);
        for(unsigned i=0;i<4;++i){write(1+i,color[i]);write(5+i,color[i]);write(9+i,ambient[i]);}
        const bool attenuation=input.attenuationEnabled&&range>0&&intensity>0;
        if(kind==0){write(21,1);write(22,0);write(23,0);return true;}
        write(21,kind==1||attenuation?float(1.0/double(intensity)):1.F);
        write(22,attenuation?float(double(0.7F)/((double(intensity)*double(range))*double(0.3F))):0.F);
        write(23,0);write(24,kind==2?input.hotspot:0.F);write(25,kind==2?input.falloff:0.F);
        return true;
    }
}
