#pragma once
#include <array>
#include <cstdint>

namespace sparkplug::reconstruction
{
    struct spBoundingVolumeSizePcForAnalysis final
    {
        std::array<std::uint32_t, 3> fullSizeBits;
        std::uint32_t radiusBits;
    };

    // The PC Box/OBB size-field body after a successful stream read.
    // Accepts finite binary32 words in separate input storage. Computes the
    // radius using the original nearest-even x87 operations. Refusal
    // preserves the supplied target fields.
    bool ApplyBoundingVolumeSizePcNearestForAnalysis(const std::array<std::uint32_t, 3>& fullSizeBits,
        spBoundingVolumeSizePcForAnalysis& target) noexcept;
}
