#pragma once
#include <array>
#include <cstdint>

namespace sparkplug::reconstruction
{
    // Computed branch of PC 00420470. Inputs hold raw binary32 x/y/z words.
    // The normal nearest-even x87 profile admits signed zero and finite normal
    // magnitudes from 2^-32 to 4096. A computed result holds the cross product,
    // then copies the raw up and forward rows. Refused inputs preserve output.
    bool BuildOrientationBasisPcNearestForAnalysis(const std::array<std::uint32_t, 3>& forward,
        const std::array<std::uint32_t, 3>& up, std::array<std::uint32_t, 9>& basis) noexcept;
}
