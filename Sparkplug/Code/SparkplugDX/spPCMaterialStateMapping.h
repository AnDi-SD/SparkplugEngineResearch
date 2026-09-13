#pragma once
// Shared recovered PC4B0A90/4B0AD0/4BDB10/4BDDB0/4BDE00/4BB890 mapping.
// CP30/32/33; mutable global black input was completed by the later skin-light
// composition. No renderer ownership, native globals or device API required.
// Lighting supplies diffuse/ambient/specular/emissive array<float,4>, power,
// packedColorC194, globalBlackARGB and diffuseSource/ambientSource. Overrides
// supplies sources/count/selectors as in spDXRenderer's existing public state.
// These templates keep those public class types/signatures unchanged and let
// an adapter use its own local storage and a recording/cache dispatch sink.
#include <array>
#include <cstdint>
#include "../../Analysis/PC/spColorMath.h"

namespace sparkplug::reconstruction
{
    using spPCMaterialStateSubmitForAnalysis=std::int32_t (*)(void*,
        std::uint32_t index,std::uint32_t value) noexcept;

    inline bool ApplyPCRenderStateCacheEntryForAnalysis(
        std::uint32_t& cachedEntry, const std::uint32_t index, const std::uint32_t value,
        const spPCMaterialStateSubmitForAnalysis submit, void* context) noexcept
    {
        if (cachedEntry == value)
            return true;
        if (!submit)
            return false; // host-only; native dereferences device unconditionally
        (void)submit(context, index, value);
        cachedEntry = value;
        return true;
    }

    inline bool ApplyPCMaterialColorSourceForAnalysis(std::uint32_t& rawSource,
        bool ambient,std::uint32_t value,spPCMaterialStateSubmitForAnalysis dispatch,void* context) noexcept
    {
        if(rawSource==value)return true;
        rawSource=value;
        if(value<10||value>12)return true;
        if(!dispatch)return false;
        (void)dispatch(context,ambient?148u:145u,value-10);return true;
    }

    template<class Lighting>
    inline bool ApplyPCMaterialLightingForAnalysis(Lighting& state,
        std::uint32_t mode,spPCMaterialStateSubmitForAnalysis dispatch,void* context) noexcept
    {
        if(!dispatch)return false;
        (void)dispatch(context,29,mode>=3&&mode<=5&&state.specularPower>0.F?1u:0u);
        if(mode>7)return true;
        const auto black=PCARGBToRGBAForAnalysis(state.globalBlackARGB);
        if(mode==0)state.emissive={1,1,1,1};
        else if(mode==1)
        {
            state.emissive=state.diffuse;const float alpha=state.diffuse[3];
            state.diffuse=state.ambient=state.specular=black;state.diffuse[3]=alpha;
        }
        else if(mode==6)
        {state.emissive=PCARGBToRGBAForAnalysis(state.packedColorC194);state.diffuse=state.ambient=state.specular=black;}
        (void)dispatch(context,137,mode==2?0u:1u);
        (void)ApplyPCMaterialColorSourceForAnalysis(state.diffuseSource,false,mode==2||mode==5?11u:10u,dispatch,context);
        (void)ApplyPCMaterialColorSourceForAnalysis(state.ambientSource,true,mode==4?11u:10u,dispatch,context);
        return true;
    }

    template<class Lighting>
    inline bool ApplyPCMaterialRenderStateForAnalysis(std::uint32_t& cached,
        std::uint32_t index,std::uint32_t value,spPCMaterialStateSubmitForAnalysis dispatch,void* context,
        Lighting* lighting) noexcept
    {
        cached=value; // actual store precedes dispatch/index rejection
        if(index<1||index>10)return false;
        if(index==8)return lighting&&ApplyPCMaterialLightingForAnalysis(*lighting,value,dispatch,context);
        if((index==2&&value>1)||(index==3&&value>2)||((index==6||index==10)&&value>7))return false;
        if(index==7&&(value==5||value>6))return true; // real no-op modes
        if(!dispatch)return false; // host guard
        auto emit=[&](std::uint32_t state,std::uint32_t mapped){(void)dispatch(context,state,mapped);};
        switch(index)
        {
        case 1:emit(8,value==1?2:3);break;
        case 2:emit(9,value+1);break;
        case 3:emit(22,value+1);break;
        case 4:emit(7,value==1?1:0);break;
        case 5:emit(14,value==1?1:0);break;
        case 6:emit(23,value+1);break;
        case 7:
        {
            constexpr std::array<std::array<std::uint32_t,2>,7> blend{{{2,1},{9,1},{5,6},{1,4},{2,2},{0,0},{5,2}}};
            emit(19,blend[value][0]);emit(20,blend[value][1]);break;
        }
        case 9:emit(24,value);break;
        case 10:emit(25,value+1);break;
        default:return false;
        }
        return true; // original ignores lower-level HRESULT/result
    }

    template<class Lighting,class Overrides>
    inline bool ApplyPCMaterialStateSetForAnalysis(std::array<std::uint32_t,11>& raw,
        const std::array<std::uint32_t,11>& material,const Overrides* overrides,
        Lighting& lighting,spPCMaterialStateSubmitForAnalysis dispatch,void* context) noexcept
    {
        raw[8]=255;
        for(std::uint32_t index=1;index<11;++index)
        {
            const auto* source=&material;
            if(overrides)
            {
                const auto slot=overrides->selectors[index-1];
                if(slot)
                {
                    if(!overrides->sources||slot>=overrides->count||!overrides->sources[slot])return false;
                    source=overrides->sources[slot];
                }
            }
            const auto value=(*source)[index];
            if(raw[index]!=value&&!ApplyPCMaterialRenderStateForAnalysis(raw[index],index,value,dispatch,context,&lighting))return false;
        }
        return true;
    }
}
