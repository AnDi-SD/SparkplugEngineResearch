#pragma once
// Shared recovered PC4BB1F0 mapping (CP31), independent of renderer ownership.
// Cache supplies raw[9], coordinateIndex and transformFlags, as in the existing
// spDXRenderer::TextureStageCacheForAnalysis. The template keeps that public
// type/signature intact and lets a native adapter supply its own state storage.
#include <array>
#include <cstdint>

namespace sparkplug::reconstruction
{
    using spPCTextureStateSubmitForAnalysis=std::int32_t (*)(void*,bool sampler,
        std::uint32_t stage,std::uint32_t index,std::uint32_t value) noexcept;

    template<class Cache>
    inline bool ApplyPCTextureStateForAnalysis(Cache& cache,
        std::uint32_t stage,std::uint32_t index,std::uint32_t value,bool overrideCoordinates,
        spPCTextureStateSubmitForAnalysis submit,void* context) noexcept
    {
        if(stage>=8||index>=cache.raw.size())return false; // host bounds
        cache.raw[index]=value;
        if(index==0)return true;
        if((index==1||index==2)&&value>=16)return false; // unbounded native table guarded
        if(!submit)return false;
        auto emit=[&](bool sampler,std::uint32_t state,std::uint32_t mapped){(void)submit(context,sampler,stage,state,mapped);};
        switch(index)
        {
        case 1:case 2:
        {
            constexpr std::array<std::uint32_t,16> operation{1,2,3,4,5,6,7,10,13,12,15,16,18,19,20,21};
            emit(false,index==1?1:4,operation[value]);break;
        }
        case 3:case 4:emit(true,index-2,value==0?1:value==1?2:3);break;
        case 5:emit(true,4,value);break;
        case 6:
        {
            const auto filter=value<=3?value:0u;
            emit(true,7,filter==3?2:filter);emit(true,5,filter?filter:1);emit(true,6,filter?filter:1);break;
        }
        case 7:
        {
            constexpr std::array<std::uint32_t,13> generation{0,1,2,3,4,5,6,7,0x10000,0x30000,0x20000,0x30000,0x40000};
            auto mapped=value<generation.size()?generation[value]:0u;
            if(overrideCoordinates)cache.raw[index]=mapped=stage;
            if(cache.coordinateIndex!=mapped){emit(false,11,mapped);cache.coordinateIndex=mapped;}break;
        }
        case 8:
        {
            const auto low=value&7u;auto mapped=low<=4?low:stage;
            if(value&8u)mapped|=0x100u;
            if(overrideCoordinates)mapped=0;
            if(cache.transformFlags!=mapped){emit(false,24,mapped);cache.transformFlags=mapped;}break;
        }
        default:return false;
        }
        return true;
    }
}
