#pragma once

#include <array>
#include <cstdint>

namespace sparkplug::reconstruction
{
    using AnimationVectorLinearPcWordsForAnalysis = std::array<std::uint32_t, 3>;

    // PC packed Vector3 position/scale helper at 0x00479060.
    // Coordinates are binary32 signed zeros or normals of magnitude [2^-32, 4096].
    // Factor is signed zero or a positive binary32 normal in [2^-32, 1].
    // The destination may be either complete input. Refusal preserves it.
    [[nodiscard]] bool TryInterpolateAnimationVectorLinearPcForAnalysis(
        const AnimationVectorLinearPcWordsForAnalysis& first,
        const AnimationVectorLinearPcWordsForAnalysis& second,
        std::uint32_t factorBits,
        AnimationVectorLinearPcWordsForAnalysis& output) noexcept;
}
