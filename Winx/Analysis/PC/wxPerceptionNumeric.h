#pragma once

// PC4F1975..4F19AB, finite float32 inputs and x87 nearest/64 control037F.
// The common integer software arithmetic models each extended operation and
// the accepted float32 minimum store; no host long-double assumption.
#include "Analysis/PC/spOcclusionSceneMath.h"
#include <array>
#include <cmath>
#include <stdexcept>

namespace winx::analysis::pc
{
    inline bool UpdatePerceptionMinimumForAnalysis(
        const std::array<float, 3>& origin, const std::array<float, 3>& target,
        float& minimum)
    {
        using namespace sparkplug::evidence::pc::transform_const_math;
        using sparkplug::evidence::pc::float80_rtz::FromFloat;
        using sparkplug::evidence::pc::occlusion_scene_math::SquareNearest;
        if (!std::isfinite(minimum) || minimum < 0)
            throw std::invalid_argument("wxPerception PC minimum must be finite and nonnegative");
        std::array<Number, 3> delta;
        for (std::size_t axis = 0; axis != 3; ++axis)
        {
            if (!std::isfinite(origin[axis]) || !std::isfinite(target[axis]))
                throw std::domain_error("wxPerception PC nearest64 requires finite coordinates");
            auto negativeOrigin = FromFloat(origin[axis]);
            negativeOrigin.negative = !negativeOrigin.negative;
            delta[axis] = AddNearest(FromFloat(target[axis]), negativeOrigin);
        }
        // Original x87 stack forms (z*z+y*y)+x*x, rounding each operation
        // at64 significand bits. The comparison precedes the float32 store.
        const auto distance = AddNearest(AddNearest(SquareNearest(delta[2]),
            SquareNearest(delta[1])), SquareNearest(delta[0]));
        if (Compare(distance, FromFloat(minimum)) > 0) return false;
        // Accepted distance<=finite minimum guarantees that this store cannot
        // overflow. Very small distances may store zero, as in the original.
        minimum = StoreNearest(distance);
        return true;
    }
}
