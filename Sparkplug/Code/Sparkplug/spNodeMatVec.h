#pragma once
#include <array>
#include <cstdint>

namespace sparkplug::reconstruction
{
    // PC 00420350 over a row-major matrix and separate three-word vectors.
    // Admits signed zeros and normal binary32 magnitudes 2^-32 to 2^27.
    // A refusal preserves every output word.
    bool TransformNodeVectorPcNearestForAnalysis(const std::array<std::uint32_t, 9>& matrixBits,
        const std::array<std::uint32_t, 3>& inputBits, std::array<std::uint32_t, 3>& outputBits) noexcept;
}
