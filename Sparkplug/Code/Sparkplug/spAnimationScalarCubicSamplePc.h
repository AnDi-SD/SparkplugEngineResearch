#pragma once

#include "spAnimationScalarCubicPc.h"

namespace sparkplug::reconstruction
{
    // Scalar Horner branch of PC spAnimTrack sampling, starting at 0x0047933D.
    // Used record words are signed zeros or binary32 normals of magnitude [2^-64, 4096].
    // Factor is signed zero or a positive binary32 normal in [2^-32, 1].
    // The destination may alias one record word. Refusal preserves it.
    [[nodiscard]] bool TrySampleAnimationScalarCubicPcForAnalysis(
        const AnimationScalarCubicKeyPcForAnalysis& record,
        std::uint32_t factorBits,
        std::uint32_t& outputBits) noexcept;
}
