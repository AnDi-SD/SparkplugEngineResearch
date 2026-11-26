#pragma once

#include <array>
#include <cstdint>

namespace winx::reconstruction
{
    class wxPivotingDoor
    {
    public:
        using CapturedDoorPrefixForAnalysis = std::array<std::uint8_t, 0x1CC>;
        using BorrowedPlayerNodePrefixForAnalysis = std::array<std::uint8_t, 0x80>;

        // Computes the PC distance decision for a typed door with a zero interaction flag
        // and an already resolved node. Captured coordinates admit finite normals or
        // signed zeros with magnitudes at most 16384. A refusal preserves the output.
        [[nodiscard]] static bool TryEvaluatePcCachedDistanceForAnalysis(
            const CapturedDoorPrefixForAnalysis& door, const BorrowedPlayerNodePrefixForAnalysis& playerNode,
            bool& withinThreshold) noexcept;
    };
}
