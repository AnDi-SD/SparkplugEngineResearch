#pragma once
#include <array>
#include <cstdint>

namespace sparkplug::reconstruction
{
    // PC 004203B0 row-vector product with three binary32 output stores.
    // Admits signed zeros and normal input words with magnitudes 2^-32 to
    // 4096. The input and output can be the same three-word object.
    // A refusal preserves every output word.
    bool TransformMovementVectorPcNearestForAnalysis(const std::array<std::uint32_t, 9>& matrixBits,
        const std::array<std::uint32_t, 3>& inputBits, std::array<std::uint32_t, 3>& outputBits) noexcept;
}
