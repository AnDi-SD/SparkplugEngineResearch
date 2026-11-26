#pragma once
#include <array>
#include <cstdint>

namespace sparkplug::reconstruction
{
    // PC 00471420 consumes three XYZ points as raw binary32 words. Its output
    // holds normal XYZ and distance d for dot(normal, point) - d. The input
    // profile admits signed zeros and normal magnitudes from 2^-40 to 16.
    // An intermediate cross outside the exact normalizer profile, or an
    // unadmitted point, returns false and preserves the four output words.
    bool BuildPlaneFromThreePcNearestForAnalysis(const std::array<std::array<std::uint32_t, 3>, 3>& points,
        std::array<std::uint32_t, 4>& plane) noexcept;
}
