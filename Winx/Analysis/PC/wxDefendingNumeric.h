#pragma once
#include <cmath>

namespace winx::evidence::pc
{
    // Only the PC515010 comparison of two binary32 operands against the
    // exact binary32 threshold0.17. This is not a general x87 replacement.
    inline bool DefendingSumBelowThresholdForAnalysis(float first, float second) noexcept
    {
        constexpr double threshold = double(0.17000000178813934326f);
        const double a = first, b = second, high = a + b;
        if (high != threshold || !std::isfinite(a) || !std::isfinite(b)) return high < threshold;
        const double split = high - a;
        const double low = (a - (high - split)) + (b - split);
        // Nearest64 spacing at exponent-3 is2^-66. The exact threshold has
        // even significand; its half-ULP tie rounds back to the threshold.
        return low < -0x1p-67;
    }
}
