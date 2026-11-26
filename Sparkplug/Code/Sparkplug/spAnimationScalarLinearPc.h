#pragma once

#include <cstdint>

namespace sparkplug::reconstruction
{
    // PC scalar position blend at 0x00479360. The caller supplies both words
    // selected by its descriptor and a binary32 interval factor.
    // The admitted domain is signed zero and normal endpoint magnitudes
    // [2^-64, 4096], and signed zero or a factor in [2^-32, 1].
    [[nodiscard]] bool TrySampleAnimationScalarLinearPcForAnalysis(
        std::uint32_t leftBits, std::uint32_t rightBits, std::uint32_t factorBits,
        std::uint32_t& outputBits) noexcept;
}
