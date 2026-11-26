#pragma once

#include <array>
#include <cstdint>

namespace sparkplug::reconstruction
{
    // PC 00426B00 with FPCW=037F, row-major binary32 words.
    // The matrices must be pairwise disjoint. Each input word admits signed zeros
    // and normal magnitudes from 2^-64 to 2^32 inclusive. Refusal preserves destination.
    bool MultiplyMatrix4PcNearestForAnalysis(const std::array<std::uint32_t, 16>& leftBits,
        const std::array<std::uint32_t, 16>& rightBits,
        std::array<std::uint32_t, 16>& destinationBits) noexcept;
}
