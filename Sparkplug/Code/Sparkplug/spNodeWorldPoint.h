#pragma once
#include <array>
#include <cstdint>

namespace sparkplug::reconstruction
{
    struct spNodeWorldPointCacheForAnalysis final
    {
        std::array<std::uint32_t, 3> positionBits;
        std::array<std::uint32_t, 3> scaleBits;
        std::array<std::uint32_t, 9> orientationBits;
    };

    // Exact PC 00420660 calculation over an established world cache. The
    // nearest-even profile admits signed zero and normal magnitudes 2^-16 to
    // 4096. The point and destination can be the same three words. Cache
    // storage cannot receive the result. A refusal preserves all output words.
    bool TransformNodeWorldPointPcNearestForAnalysis(const spNodeWorldPointCacheForAnalysis& cache,
        const std::array<std::uint32_t, 3>& pointBits, std::array<std::uint32_t, 3>& outputBits) noexcept;
}
