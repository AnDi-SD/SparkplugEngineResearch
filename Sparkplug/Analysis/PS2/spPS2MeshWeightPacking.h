#pragma once

// Captured-value reconstruction of the raw record stores in EE15D6C0.
// This is neither a PS2 pointer layout nor an EE arithmetic provider.

#include <array>
#include <cstdint>
#include <optional>

namespace sparkplug::evidence::ps2
{
    using MeshWeightRecordForAnalysis = std::array<std::uint32_t, 8>;

    // Complete supported record: four raw weight words followed by four
    // integer bone-index lanes. Missing trailing weights/indices are zero.
    // Original bone-index bytes are multiplied by4; no float conversion.
    [[nodiscard]] inline std::optional<MeshWeightRecordForAnalysis> PackMeshWeightsForAnalysis(
        const std::array<std::uint32_t, 4>& rawWeightWords,
        std::uint32_t packedBoneBytes, unsigned weightCount) noexcept
    {
        if (weightCount == 0 || weightCount > 4) return std::nullopt;
        MeshWeightRecordForAnalysis result{};
        for (unsigned lane = 0; lane < weightCount; ++lane)
        {
            result[lane] = rawWeightWords[lane];
            result[4 + lane] = ((packedBoneBytes >> (lane * 8)) & 255U) << 2;
        }
        return result;
    }
}
