#pragma once
#include <array>
#include <cstdint>

namespace sparkplug::reconstruction
{
    // Raw x87 ST0 fields, independent of the host long-double representation.
    struct spVector3Length80ForAnalysis final
    {
        std::uint64_t significand = 0;
        std::uint16_t signExponent = 0;
    };

    // PC 0041D2D0 accepts three binary32 words in x/y/z order. The verified
    // nearest-even x87 profile admits either signed zero or normal magnitudes
    // in [2^-48, 4096]. A refusal preserves both arguments. The returned bool
    // reports profile admission, and length represents the native ST0 value.
    bool NormalizeVector3PcNearestForAnalysis(std::array<std::uint32_t, 3>& vectorBits,
        spVector3Length80ForAnalysis& length) noexcept;
}
