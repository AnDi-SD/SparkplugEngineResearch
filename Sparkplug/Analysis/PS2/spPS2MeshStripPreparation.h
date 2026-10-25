#pragma once

// Captured-value reconstruction of the direct/prepopulated strip route in
// EE15F1F0/15FD30 and stable count sort1611E0. This is not the Type3 stripifier:
// native1C4FE0 returns false and does not create these input records.

#include <cstdint>
#include <vector>

namespace sparkplug::evidence::ps2
{
    struct PreparedMeshStripForAnalysis final
    {
        std::uint32_t sourceOrdinal = 0;
        std::vector<std::uint16_t> indices;
        std::vector<std::uint8_t> adc;
    };

    struct MeshStripJoinForAnalysis final
    {
        std::uint32_t anchorOrdinal = 0;
        std::uint32_t appendedOrdinal = 0;
        std::uint32_t firstAppendedVertex = 0;
        bool reversed = false;
    };

    enum class MeshStripPreparationStatusForAnalysis
    {
        Prepared, InvalidCapacity, InvalidStripCount, StripExceedsCapacity,
        ByteBudgetExceeded, AllocationFailed,
    };

    struct MeshStripPreparationResultForAnalysis final
    {
        bool completed = false;
        MeshStripPreparationStatusForAnalysis status = MeshStripPreparationStatusForAnalysis::InvalidCapacity;
        std::vector<PreparedMeshStripForAnalysis> strips;
        std::vector<MeshStripJoinForAnalysis> joins;
    };

    // Input corresponds to original uint16-count records, with indices captured
    // into independent vectors. Original input has no ADC; newly copied ADC is0.
    // Within each parity group, join whole subsequent fitting strips greedily
    // in original order; reverse appended indices iff accumulated count is odd;
    // first two appended ADC bytes are1. Then stable-sort survivors by descending
    // unsigned count, starting with even survivors followed by odd survivors.
    //
    // Host guards reject counts<3,counts>capacity,capacity outside3..65535 and
    // logical output(indices+ADC) over maximumBytes. Original invalid-pointer,
    // small-count,oversized-strip and allocation-fault behavior is not modeled.
    // Logical byte budget is not a claim about native allocator consumption.
    // No pointer aliasing, heap callbacks, DMA emission or successful internal
    // producer substitution is involved. Allocation failure returns no output.
    [[nodiscard]] MeshStripPreparationResultForAnalysis PreparePrepopulatedMeshStripsForAnalysis(
        const std::vector<std::vector<std::uint16_t>>& strips,
        std::uint32_t capacity,
        std::uint32_t maximumBytes = 64U * 1024U * 1024U);
}
