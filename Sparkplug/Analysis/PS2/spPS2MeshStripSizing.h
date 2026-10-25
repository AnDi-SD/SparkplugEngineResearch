#pragma once

// Captured-value reconstruction of original160750/1604B0. No constructor,
// caller160C00, allocator, stripifier or native list pointer traversal runs.
#include <array>
#include <cstdint>
#include <vector>

namespace sparkplug::evidence::ps2
{
    struct MeshStripSizingInputsForAnalysis final
    {
        std::uint32_t effectiveFlags = 0;       // mesh+1C
        std::uint32_t streamCount = 0;          // mesh+20
        std::array<std::uint32_t, 22> descriptors{}; // mesh+48
        std::uint32_t grouping = 0;             // mesh+F8
        std::uint32_t declaredStripCount = 0;   // stripObject+10
        std::uint32_t argumentA2 = 0;           // outer caller passes capacity
        // Counts from the actually traversed sentinel list's uint16 records.
        // Deliberately independent of declaredStripCount. Captured values
        // replace pointer access, not successful internal producer callbacks.
        std::vector<std::uint16_t> traversedCounts;
    };

    enum class MeshStripSizingStatusForAnalysis { Complete, RecordBudgetExceeded };

    struct MeshStripSizingResultForAnalysis final
    {
        bool completed = false;
        MeshStripSizingStatusForAnalysis status = MeshStripSizingStatusForAnalysis::RecordBudgetExceeded;
        std::uint32_t qwords = 0;
        // The original temporary array exists only in grouping==1. These
        // counters have no native temporary-memory interpretation otherwise.
        bool componentCountersProduced = false;
        std::array<std::uint32_t, 22> alignedComponentBytes{};
        std::uint32_t recordsVisited = 0;
    };

    // Exact low32 result, excluding the outer RET tag. This is OBSERVED
    // reservation arithmetic, not a safe allocation bound for arbitrary
    // captured fields. Interleaved ignores the list body and record budget.
    // Grouped uses actual captured counts and an explicit host record budget.
    // Native pointer faults, malformed/cyclic lists, multiply accumulator/HI
    // effects and hardware execution are outside this value API.
    [[nodiscard]] MeshStripSizingResultForAnalysis ComputeMeshStripReservationForAnalysis(
        const MeshStripSizingInputsForAnalysis& inputs,
        std::uint32_t maximumRecords = 4096) noexcept;
}
