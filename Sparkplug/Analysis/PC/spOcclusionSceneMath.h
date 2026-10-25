#pragma once

// PC45ED4B..45EE06 view-space occluder ordering key. Original X/Y spill to
// float; Z remains x87 nearest64 through its square. Default rounding and
// finite input/output domain only. No host long-double/backend assumptions.
#include "spOcclusionCameraMath.h"

namespace sparkplug::evidence::pc::occlusion_scene_math
{
    namespace arithmetic=transform_const_math;
    using Number=arithmetic::Number;
    inline Number SquareNearest(Number value) noexcept
    {
        if(!value.mantissa)return {};
        // Exact64x64 product in the existing four-limb accumulator; round only
        // after the full product, retaining the original FMUL st(1) precision.
        auto low=arithmetic::Wide::Shifted(value.mantissa,0);
        auto high=low;low.Multiply(std::uint32_t(value.mantissa));
        high.Multiply(std::uint32_t(value.mantissa>>32));
        for(unsigned i=3;i;--i)high.words[i]=high.words[i-1];high.words[0]=0;
        low.Add(high);
        return arithmetic::NormalizeNearest(low,value.exponent*2,false);
    }
    inline Number ViewComponent(const std::array<float,3>& center,
        const std::array<float,16>& view,unsigned column) noexcept
    {
        using namespace occlusion_camera_math;
        return AddNearest(AddNearest(AddNearest(Multiply(FromFloat(center[1]),view[4+column]),
            Multiply(FromFloat(center[2]),view[8+column])),Multiply(FromFloat(center[0]),view[column])),
            FromFloat(view[12+column]));
    }
    inline bool OrderingKey(const std::array<float,3>& center,const std::array<float,16>& view,
        bool orthographic,float& output) noexcept
    {
        using namespace occlusion_camera_math;
        if(!Finite(center)||!Finite(view))return false;
        const float x=StoreNearest(ViewComponent(center,view,0));
        const float y=StoreNearest(ViewComponent(center,view,1));
        if(!std::isfinite(x)||!std::isfinite(y))return false;
        const auto z=ViewComponent(center,view,2);
        const auto squareZ=SquareNearest(z);
        const auto total=orthographic?squareZ:AddNearest(AddNearest(
            SquareNearest(FromFloat(x)),SquareNearest(FromFloat(y))),squareZ);
        const float result=StoreNearest(total);if(!std::isfinite(result))return false;
        output=result;return true;
    }
}
