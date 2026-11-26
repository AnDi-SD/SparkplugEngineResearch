#pragma once

#include <array>
#include <cstdint>

namespace sparkplug::reconstruction
{
    using AnimationVectorCubicPcWordsForAnalysis = std::array<std::uint32_t, 3>;

    struct AnimationVectorCubicPcRecordForAnalysis
    {
        AnimationVectorCubicPcWordsForAnalysis value;
        AnimationVectorCubicPcWordsForAnalysis incomingTangent;
        AnimationVectorCubicPcWordsForAnalysis outgoingTangent;
        AnimationVectorCubicPcWordsForAnalysis quadratic;
        AnimationVectorCubicPcWordsForAnalysis cubic;
    };

    // PC packed Vector3 cubic sampler at 0x00479110. The incoming tangent is unread.
    // Used words are binary32 signed zeros or normals of magnitude [2^-32, 4096].
    // Factor is signed zero or a positive binary32 normal in [2^-32, 1].
    // The complete destination may alias a group of the record. Refusal preserves it.
    [[nodiscard]] bool TrySampleAnimationVectorCubicPcForAnalysis(
        const AnimationVectorCubicPcRecordForAnalysis& record,
        std::uint32_t factorBits,
        AnimationVectorCubicPcWordsForAnalysis& output) noexcept;
}
