#pragma once
// Own PC random-expression stores through the existing nearest64 accumulator.
#include "Analysis/PC/spTransformConstMath.h"
#include "Analysis/PC/spOcclusionSceneMath.h"
#include <array>
#include <bit>
#include <cmath>

namespace winx::analysis::pc
{
    inline std::uint32_t PcMinotaurAttackRandomWordForAnalysis(std::uint32_t random) noexcept
    {
        namespace arithmetic=sparkplug::evidence::pc::transform_const_math;
        using sparkplug::evidence::pc::float80_rtz::FromFloat;
        const auto value=random?arithmetic::NormalizeNearest(arithmetic::Wide::Shifted(random,0),0,false):arithmetic::Number{};
        const auto scaled=arithmetic::MultiplyFloatNearest(value,std::bit_cast<float>(0x386A6000U));
        return std::bit_cast<std::uint32_t>(arithmetic::StoreNearest(arithmetic::AddNearest(scaled,FromFloat(250000.0f))));
    }
    inline bool PcMinotaurAttackOutsideRangeForAnalysis(const std::array<float,3>& origin,
        const std::array<float,3>& target,bool& outside) noexcept
    {
        namespace arithmetic=sparkplug::evidence::pc::transform_const_math;
        using sparkplug::evidence::pc::float80_rtz::FromFloat;
        using sparkplug::evidence::pc::occlusion_scene_math::SquareNearest;
        std::array<arithmetic::Number,3> delta{};
        for (unsigned i=0;i!=3;++i)
        {
            if (!std::isfinite(origin[i]) || !std::isfinite(target[i]) || std::abs(origin[i])>1024.0f || std::abs(target[i])>1024.0f) return false;
            auto negative=FromFloat(origin[i]);negative.negative=!negative.negative;
            delta[i]=arithmetic::AddNearest(FromFloat(target[i]),negative);
        }
        // Native retains each difference and(z²+y²)+x² before FCOMP.
        const auto squared=arithmetic::AddNearest(arithmetic::AddNearest(SquareNearest(delta[2]),SquareNearest(delta[1])),SquareNearest(delta[0]));
        outside=arithmetic::Compare(squared,FromFloat(237500.0f))>0;return true;
    }
}
