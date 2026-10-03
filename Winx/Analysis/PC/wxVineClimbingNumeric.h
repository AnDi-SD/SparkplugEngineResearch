#pragma once

namespace winx::analysis::pc
{
    // Portable host arithmetic for the ONE native FSub/FComp50 boundary.
    // PC517446..51745D uses binary80 with64 significand bits, nearest-even.
    // This does not implement general x87 arithmetic, status or other modes.
    // At50 the binary80 half-ULP is2^-59 and50 has an even significand.
    inline bool HeightDifferenceAbove50ForAnalysis(float first, float second) noexcept
    {
        const double a = first, b = second;
        const double high = a - b;
        if (high != 50.0) return high > 50.0;
        // Error-free TwoDiff recovers the residual lost by binary64. Both
        // input floats are exact doubles and their difference cannot overflow.
        const double bVirtual = a - high;
        const double aVirtual = high + bVirtual;
        const double bRoundoff = bVirtual - b;
        const double aRoundoff = a - aVirtual;
        const double low = aRoundoff + bRoundoff;
        return low > 0x1p-59;
    }
}
