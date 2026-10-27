#pragma once

// Analytical name for the numeric portion of PC5AA070. Exact small integer
// local coordinates, original float distance store and x87 range-square order.
// Host sqrt is used only in this bounded domain, not as an EE/FPU emulator.
#include "Analysis/PC/spOcclusionSceneMath.h"
#include <array>
#include <cmath>

namespace winx::analysis::pc
{
    inline bool HurtRangeForAnalysis(const std::array<float, 3>& origin,
        const std::array<float, 3>& player, const float limit, bool& extend, float& range) noexcept
    {
        if (!std::isfinite(limit) || limit < 0) return false;
        double squared = 0;
        for (unsigned axis = 0; axis != 3; ++axis)
        {
            for (const float value : {origin[axis], player[axis]})
                if (!std::isfinite(value) || std::abs(value) > 1024.0f || std::trunc(value) != value)
                    return false;
            const double delta = double(player[axis]) - double(origin[axis]);
            squared += delta * delta; // <=3*2048^2, exact integer sum
        }
        const float distance = float(std::sqrt(squared));
        // Preserve the original dimensional mismatch: compare distance to3C,
        // but write (distance+100)^2. Do not compare distance squared instead.
        extend = distance > limit;
        if (extend)
        {
            using namespace sparkplug::evidence::pc::transform_const_math;
            using sparkplug::evidence::pc::float80_rtz::FromFloat;
            using sparkplug::evidence::pc::occlusion_scene_math::SquareNearest;
            range = StoreNearest(SquareNearest(AddNearest(FromFloat(distance), FromFloat(100.0f))));
        }
        return true;
    }
}
