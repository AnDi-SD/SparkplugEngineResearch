#pragma once

// Own PC5AEA70/5AEEE0 numeric consumers, analytical names. Reuse the common
// nearest64 accumulator instead of depending on host long double precision.
// This admitted finite domain and default rounding do not qualify EE/FPU
// modes, nonfinite inputs or every engine numeric operation.
#include "Analysis/PC/spOcclusionSceneMath.h"
#include <array>
#include <cmath>

namespace winx::analysis::pc
{
    namespace ice_gargoyle_math
    {
        namespace arithmetic=sparkplug::evidence::pc::transform_const_math;
        using arithmetic::Number;
        using sparkplug::evidence::pc::float80_rtz::FromFloat;
        inline Number Negate(Number value) noexcept { value.negative=!value.negative;return value; }
        inline Number Difference(float first,float second) noexcept
        { return arithmetic::AddNearest(FromFloat(first),Negate(FromFloat(second))); }
        inline bool Bounded(float value) noexcept
        { return std::isfinite(value) && std::abs(value)<=1024.0f; }
    }
    inline bool PcIceGargoyleWithdrawlCloseForAnalysis(const std::array<float,3>& origin,
        const std::array<float,3>& target,bool& close) noexcept
    {
        using namespace ice_gargoyle_math;
        using sparkplug::evidence::pc::occlusion_scene_math::SquareNearest;
        for (unsigned axis=0;axis!=3;++axis)
            if (!Bounded(origin[axis]) || !Bounded(target[axis])) return false;
        const auto x=Difference(target[0],origin[0]);
        const auto y=Difference(target[1],origin[1]);
        const auto z=Difference(target[2],origin[2]);
        // Original FCOMP consumes (z*z+y*y)+x*x without a float spill.
        const auto square=arithmetic::AddNearest(arithmetic::AddNearest(
            SquareNearest(z),SquareNearest(y)),SquareNearest(x));
        close=arithmetic::Compare(square,FromFloat(22500.0f))<0;
        return true;
    }
    inline bool PcIceGargoyleClawHeightForAnalysis(float ownerY,float targetY,
        bool& withinBand,bool& ownerBelowTarget) noexcept
    {
        using namespace ice_gargoyle_math;
        if (!Bounded(ownerY) || !Bounded(targetY)) return false;
        const auto targetMinus150=arithmetic::AddNearest(FromFloat(targetY),Negate(FromFloat(150.0f)));
        auto gap=arithmetic::AddNearest(FromFloat(ownerY),Negate(targetMinus150));
        gap.negative=false;
        withinBand=arithmetic::Compare(gap,FromFloat(50.0f))<=0;
        ownerBelowTarget=arithmetic::Compare(Difference(ownerY,targetY),FromFloat(0.0f))<0;
        return true;
    }
}
