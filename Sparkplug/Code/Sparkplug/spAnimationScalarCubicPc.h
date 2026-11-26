#pragma once

#include <cstddef>
#include <cstdint>

namespace sparkplug::reconstruction
{
    struct AnimationScalarCubicKeyPcForAnalysis
    {
        std::uint32_t valueBits;
        std::uint32_t incomingBits;
        std::uint32_t outgoingBits;
        std::uint32_t quadraticBits;
        std::uint32_t cubicBits;
    };

    // PC 0x00493160 with round-to-nearest binary80 arithmetic. The caller
    // supplies a borrowed array of at least one 20-byte key. Only coefficients
    // of segments followed by another key are written.
    [[nodiscard]] bool TryPrepareAnimationScalarCubicPcForAnalysis(
        AnimationScalarCubicKeyPcForAnalysis* keys, std::size_t keyCount) noexcept;
}
