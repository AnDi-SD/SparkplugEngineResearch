#pragma once

#include "Code/Sparkplug/spIndexBuffer.h"
#include "Code/Sparkplug/spVertexBuffer.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace sparkplug::evidence::pc
{
// Analytical alias of the ORIGINAL separate object: ctor 4604F0, dtor
// 460500, one-slot vtable 6E76FC -> deleting dtor 460AE0. Its source name is
// unknown. This is neither spVertexBuffer nor an invented Occlusion class.
class GeometryHelper4604F0 final
{
public:
    virtual ~GeometryHelper4604F0() = default;

    // Original qsort receives UInt16 elements, count, width 2 and comparator
    // 4607F0. The explicit comparator context replaces global 75FF9C only at
    // the portable host boundary. Calls are synchronous; callbacks must not
    // mutate the input buffers or retain these pointers. There is NO default
    // sorter, no installed CRT dependency, and no assumed ordering of ties.
    using ComparatorForAnalysis = int (*)(const void*, const std::uint16_t*,
        const std::uint16_t*);
    struct SortDispatchForAnalysis
    {
        void* context = nullptr;
        bool (*invoke)(void*, std::uint16_t*, std::size_t, std::size_t,
            ComparatorForAnalysis, const void*) = nullptr;
    };
    struct ObservationForAnalysis
    {
        std::vector<std::uint16_t> sortedIds;
        bool duplicateFound = false;
        bool compactionCalled = false;
        // Actual 451040..451047 request: strideBytes * newCount * 4.
        // This is NOT the logical vertex extent exposed by spVertexBuffer.
        std::uint32_t compactionAllocationBytes = 0;
    };

    // Host limits, not original unchecked preconditions. Only the position-
    // only UInt16 branch used by Occlusion Init is reconstructed here. Empty
    // buffers remain unsupported; zero-count original lifetime is not proven.
    static constexpr std::uint32_t MaximumVerticesForAnalysis = 65535;
    static constexpr std::uint32_t MaximumIndicesForAnalysis = 65535;
    [[nodiscard]] bool WeldForAnalysis(reconstruction::spIndexBuffer& indices,
        reconstruction::spVertexBuffer& vertices, const SortDispatchForAnalysis& sort,
        ObservationForAnalysis& observation, std::string* error = nullptr) const;

    // 4607F0 -> 13D1850: unsigned lexicographic comparison of twelve raw
    // position bytes. No floating point epsilon or signed-zero normalization.
    [[nodiscard]] static std::optional<int> ComparePositionsForAnalysis(
        const reconstruction::spVertexBuffer& vertices,
        std::uint16_t left, std::uint16_t right) noexcept;

    // 4609A0 -> 450F50; independent original compactor, also used by 460C40.
    // Mutates IB before replacing VB, in original order. false is a host
    // unsupported/allocation result; the original routines return void.
    [[nodiscard]] bool CompactForAnalysis(reconstruction::spIndexBuffer& indices,
        reconstruction::spVertexBuffer& vertices,
        ObservationForAnalysis& observation, std::string* error = nullptr) const;
};
}
