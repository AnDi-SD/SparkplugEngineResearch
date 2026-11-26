#pragma once

#include <array>
#include <cstdint>

namespace sparkplug::reconstruction
{
    using AnimationQuaternionPcWordsForAnalysis = std::array<std::uint32_t, 4>;

    struct AnimationQuaternionSlerpPreparationPcForAnalysis
    {
        std::uint32_t dotBits;
        AnimationQuaternionPcWordsForAnalysis second;
    };

    // PC Slerp preparation at 0x004648C0 through the bounded stop at 0x00464973.
    // XYZ/W input words are signed zeros or binary32 normals of magnitude [2^-32, 4096].
    // Reports the adjusted second quaternion and binary32 dot before later interpolation.
    [[nodiscard]] bool TryPrepareAnimationQuaternionSlerpPcForAnalysis(
        const AnimationQuaternionPcWordsForAnalysis& first,
        const AnimationQuaternionPcWordsForAnalysis& second,
        AnimationQuaternionSlerpPreparationPcForAnalysis& output) noexcept;
}
