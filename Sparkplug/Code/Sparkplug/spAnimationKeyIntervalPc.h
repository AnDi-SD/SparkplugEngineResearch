#pragma once

#include <cstddef>
#include <cstdint>

namespace sparkplug::reconstruction
{
    struct AnimationKeyIntervalPcForAnalysis
    {
        std::int32_t index;
        std::uint32_t factorBits;
    };

    // PC 0x00478F90 for a borrowed, nondecreasing array of admitted F32 timestamps.
    // Returns the selected index and the raw binary32 factor.
    [[nodiscard]] bool TryFindAnimationKeyIntervalPcForAnalysis(
        const std::uint32_t* timestampBits, std::size_t keyCount, std::uint32_t timeBits,
        std::int32_t cachedIndex, AnimationKeyIntervalPcForAnalysis& result) noexcept;
}
