#pragma once

// Analytical names for shared PC5970B0/597140/5971A0. Original ownership and
// source names are unknown. These finite-input operations use the existing
// PC41D2D0 adapter and host acos for MSVCR71!_CIacos; they do not qualify the
// imported CRT, arbitrary x87 rounding, nonfinite geometry or PS2 arithmetic.
#include "wxButterflyNumeric.h"
#include <array>
#include <cmath>

namespace winx::analysis::pc
{
    using PlanarVectorForAnalysis = std::array<float, 3>;

    inline bool IsBoundedPlanarVectorForAnalysis(const PlanarVectorForAnalysis& value) noexcept
    {
        // Y is overwritten/ignored by the original planar operations.
        return std::isfinite(value[0]) && std::isfinite(value[2]) &&
            std::abs(value[0]) <= 1024.0f && std::abs(value[2]) <= 1024.0f;
    }

    inline double PcPlanarVectorYawForAnalysis(PlanarVectorForAnalysis value) noexcept
    {
        const double cosine = value[2] > 1.0f ? 1.0 : value[2] < -1.0f ? -1.0 : value[2];
        const double angle = std::acos(cosine);
        // Negative zero compares equal to zero, as in the original FCOMP.
        return value[0] < 0.0f ? -angle : angle;
    }

    inline bool PcPlanarPositionYawForAnalysis(const PlanarVectorForAnalysis& origin,
        const PlanarVectorForAnalysis& target, float& yaw) noexcept
    {
        if (!IsBoundedPlanarVectorForAnalysis(origin) || !IsBoundedPlanarVectorForAnalysis(target))
            return false;
        PlanarVectorForAnalysis direction{
            float(double(target[0]) - double(origin[0])), 0.0f,
            float(double(target[2]) - double(origin[2]))};
        // The caller spills each difference before the shared normalization.
        winx::evidence::pc::ButterflyNormalizeForAnalysis(direction);
        yaw = float(PcPlanarVectorYawForAnalysis(direction));
        return true;
    }

    inline bool PcPlanarFacingForAnalysis(PlanarVectorForAnalysis forward,
        const float desiredYaw, const float tolerance, bool& facing) noexcept
    {
        if (!IsBoundedPlanarVectorForAnalysis(forward) || !std::isfinite(desiredYaw) ||
            std::abs(desiredYaw) > 0x1.921fb6p+1f || !std::isfinite(tolerance))
            return false;
        forward[1] = 0.0f;
        winx::evidence::pc::ButterflyNormalizeForAnalysis(forward);
        const float currentYaw = float(PcPlanarVectorYawForAnalysis(forward));
        // Both stored yaws are already within PC596F80's [-pi,pi] limits.
        // Their subtraction is exact in binary64; PC5971A0 keeps the result
        // wider than float until this comparison. Equality is NOT facing.
        double distance = std::abs(double(desiredYaw) - double(currentYaw));
        if (distance > double(0x1.921fb6p+1f))
            distance = double(0x1.921fb6p+2f) - distance;
        facing = distance < double(tolerance);
        return true;
    }
}
